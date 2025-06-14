#include "main.h"
#include "LCD.h"
#include "eeprom.h"
#include "frequency.h"
#include "gps.h"
#include "menu.h"
#include "int.h"
#include "tim.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// All times in ms
#define PPS_PULSE_WIDTH         100
#define VCO_ADJUSTMENT_DELAY    3000

void warmup()
{
    char     buf[9];
    uint32_t warmup_time = 300;
    LCD_Clear();
    while (warmup_time != 0) {
        LCD_Puts(0, 0, " WARMUP");
        sprintf(buf, "  %ld  ", warmup_time);
        LCD_Puts(0, 1, buf);
        warmup_time--;

        for (int i = 0; i < 200; i++) {
            if (rotary_get_click()) {
                warmup_time = 0;
                break;
            }
            HAL_Delay(5);
        }
    }
}

void gpsdo(void)
{
    HAL_TIM_Base_Start_IT(&htim2);

    EE_Init(&ee_storage, sizeof(ee_storage_t));
    EE_Read();
    if (ee_storage.pwm == 0xffff) {
        ee_storage.pwm = 38000;
    }
    TIM1->CCR2 = ee_storage.pwm;
    if (ee_storage.contrast == 0xff) {
        ee_storage.contrast = 80;
    }
    contrast = ee_storage.contrast;
    update_contrast();
    if (ee_storage.pps_sync_on == 0xff) {
        ee_storage.pps_sync_on = true;
    }
    pps_sync_on = ee_storage.pps_sync_on;
    if (ee_storage.pps_sync_delay == 0xffffffff) {
        ee_storage.pps_sync_delay = 10;
    }
    pps_sync_delay = ee_storage.pps_sync_delay;
    if (ee_storage.pps_sync_threshold == 0xffffffff) {
        ee_storage.pps_sync_threshold = 30000;
    }
    pps_sync_threshold = ee_storage.pps_sync_threshold;
    if (ee_storage.pps_ppm_auto_sync == 0xff) {
        ee_storage.pps_ppm_auto_sync = true;
    }
    pps_ppm_auto_sync = ee_storage.pps_ppm_auto_sync;
    if (ee_storage.pwm_auto_save == 0xff) {
        ee_storage.pwm_auto_save = true;
    }
    pwm_auto_save = ee_storage.pwm_auto_save;

    if (ee_storage.trend_auto_v == 0xff) {
        ee_storage.trend_auto_v = true;
    }
    trend_auto_v = ee_storage.trend_auto_v;
    if (ee_storage.trend_auto_h == 0xff) {
        ee_storage.trend_auto_h = true;
    }
    trend_auto_h = ee_storage.trend_auto_h;
    if (ee_storage.trend_v_scale == 0xffffffff) {
        ee_storage.trend_v_scale = 70;
    }
    trend_v_scale = ee_storage.trend_v_scale;
    if (ee_storage.trend_h_scale == 0xffffffff) {
        ee_storage.trend_h_scale = 1;
    }
    trend_h_scale = ee_storage.trend_h_scale;
    // Boot menu
    if (ee_storage.boot_menu == 0xff) {
        ee_storage.boot_menu = 0; // Default to main screen
    }
    menu_set_current_menu(ee_storage.boot_menu);
    // Check for custom gps baudrate
    if (ee_storage.gps_baudrate == 0xffffffff) {
        ee_storage.gps_baudrate = GPS_DEFAULT_BAUDRATE;
    }
    if (ee_storage.gps_time_offset == 0xffffffff) {
        ee_storage.gps_time_offset = -MIN_TIME_OFFSET;
    }
    gps_time_offset = ee_storage.gps_time_offset+MIN_TIME_OFFSET;
    if (ee_storage.gps_date_format == 0xff) {
        ee_storage.gps_date_format = DATE_FORMAT_UTC;
    }
    gps_date_format = ee_storage.gps_date_format;
    if (ee_storage.gps_model == 0xff) {
        ee_storage.gps_model = GPS_MODEL_UNKNOWN;
    }
    gps_model = ee_storage.gps_model;
    // PPB lock threshold (*100)
    if (ee_storage.ppb_lock_threshold == 0xffffffff) {
        ee_storage.ppb_lock_threshold = DEFAULT_PPB_LOCK_THRESHOLD;
    }
    ppb_lock_threshold = ee_storage.ppb_lock_threshold;


    gps_start_it();

    menu_set_gps_baudrate(ee_storage.gps_baudrate);

    LCD_Init();

    lcd_create_chars();
    init_trend_values();

    // warmup();

    LCD_Clear();

    HAL_Delay(100);
    frequency_start();

    HAL_TIM_Base_Start(&htim3);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);

    bool vco_adjust_allowed = false;

    while (1) {
        uint32_t now = HAL_GetTick();
        if(pps_out_up && now-last_pps_out >= PPS_PULSE_WIDTH)
        {
            HAL_GPIO_WritePin(PPS_OUTPUT_GPIO_Port, PPS_OUTPUT_Pin, 0);
            pps_out_up = false;
        }
        if (!vco_adjust_allowed && now >= VCO_ADJUSTMENT_DELAY) 
        {   // Start adjusting the VCO after some time
            frequency_allow_adjustment(true);
            vco_adjust_allowed = true;
        }
        
        gps_read();
        menu_run();
    }
}
