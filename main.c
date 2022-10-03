/**
 * @file        main.c
 * @brief       GPIO Example
 * @details
 */

/*******************************************************************************
 * Copyright (C) 2016 Maxim Integrated Products, Inc., All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Maxim Integrated
 * Products, Inc. shall not be used except as stated in the Maxim Integrated
 * Products, Inc. Branding Policy.
 *
 * The mere transfer of this software does not imply any licenses
 * of trade secrets, proprietary technology, copyrights, patents,
 * trademarks, maskwork rights, or any other form of intellectual
 * property whatsoever. Maxim Integrated Products, Inc. retains all
 * ownership rights.
 *
 * $Date: 2018-08-09 18:45:02 -0500 (Thu, 09 Aug 2018) $
 * $Revision: 36818 $
 *
 ******************************************************************************/

/* **** Includes **** */
#include <stdio.h>
#include <string.h>
#include "mxc_config.h"
#include "board.h"
#include "gpio.h"
#include "tmr_utils.h"
#include "./elapsed_time/elapsed_time.h"
#include "mxc_delay.h"

/* **** Definitions **** */

#define ADC_SPI_CLK_PORT_OUT				PORT_0
#define ADC_SPI_CLK_PIN_OUT					PIN_2

#define ADC_SPI_CSB_PORT_OUT				PORT_0
#define ADC_SPI_CSB_PIN_OUT					PIN_3

#define ADC_CH_SEL_PORT_OUT					PORT_0
#define ADC_CH_SEL_PIN_OUT					PIN_7

#define ADC_SPI_CIPO_PORT_IN				PORT_0
#define ADC_SPI_CIPO_PIN_IN					PIN_0

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))

#define ADC_OUT_SLOPE_CALIBRATION 			0.993F
#define ADC_OUT_INTERCEPT_CALIBRATION 	-0.0005F


/* 
	For UART_TX:
	If CONSOLE_UART_DEBUG = 0 and console_uart_sys_cfg = {MAP_A, UART_FLOW_DISABLE};, => UART_TX = P0_4 
*/

/* **** Globals **** */

gpio_cfg_t adc_spi_cipo_in;
gpio_cfg_t adc_spi_clk_out;
gpio_cfg_t adc_spi_csb_out;
gpio_cfg_t adc_spi_ch_sel_out;

/* ########################################################################### */

/* **** Functions **** */

void GPIO_SerialWireDebugDisable(void)
{
    MXC_GCR->scon |= MXC_S_GCR_SCON_SWD_DIS_DISABLE;
}

#define SAMPLE_CYCLES 16
		// Creates a string output of the binary input
void print_binary(uint16_t data_in) {
	for (int16_t binary_idx = SAMPLE_CYCLES-1; binary_idx >= 0; binary_idx--){
		if(bitRead(data_in, binary_idx)){
			printf("  1");
		}
		else{
			printf("  0");
		}
//		printf(bitRead(data_in, binary_idx) ? '1' : '0');
	}
}

// Simple for loop that toggles the clock then inspects the CIPO pin
double bit_bang_spi(uint16_t* _rx_low, uint16_t* _rx_high)  // This function transmit the data via bitbanging
{
	double execution_time_us = 0;
	GPIO_OutSet(&adc_spi_clk_out);
	mxc_delay(MXC_DELAY_USEC(10)); 	// t2 > 5ns
	GPIO_OutClr(&adc_spi_csb_out); 	// CS_B low
	mxc_delay(MXC_DELAY_USEC(1)); 	// t2 > 5ns

	elapsed_time_start(0);
	for(int16_t idx = SAMPLE_CYCLES-1; idx >= 2; idx--)
	{
		GPIO_OutClr(&adc_spi_clk_out);        // SS low                   // SCK low
		mxc_delay(MXC_DELAY_USEC(10)); // t2 > 5ns
		GPIO_OutSet(&adc_spi_clk_out);
		mxc_delay(MXC_DELAY_USEC(10)); // t2 > 5ns
		bitWrite((*_rx_high), idx, GPIO_InGet(&adc_spi_cipo_in)); // Capture CIPO
	} 
	elapsed_time_stop(0);
	GPIO_OutSet(&adc_spi_csb_out); // CS_B High

	return execution_time_us;        // Return the received data
}

/* ########################################################################### */
/* ########################################################################### */
#define VDD 3.3F
#define BITS_RESOLUTION 12U
int main(void)
{
		uint16_t rx_low = 0;
		uint16_t rx_high = 0;
		uint16_t rx_high_12b = 0;
		double execution_time_us = 0;
		unsigned long loop_cnt = 0;

		char a_in_ch[] = "AIN_CH1";

    printf("\n\n***** MAX32660 to MAX19777 ADC Example ******\n\n");

		GPIO_SerialWireDebugDisable();

    /* Setup input pin. */
    /* Switch on EV kit is open when non-pressed, and grounded when pressed.  Use an internal pull-up so pin
       reads high when button is not pressed. */

    adc_spi_cipo_in.port = ADC_SPI_CIPO_PORT_IN;
    adc_spi_cipo_in.mask = ADC_SPI_CIPO_PIN_IN;
    adc_spi_cipo_in.pad = GPIO_PAD_NONE;
    adc_spi_cipo_in.func = GPIO_FUNC_IN;
    GPIO_Config(&adc_spi_cipo_in);

    /* Setup output pins. */
    adc_spi_clk_out.port = ADC_SPI_CLK_PORT_OUT;
    adc_spi_clk_out.mask = ADC_SPI_CLK_PIN_OUT;
    adc_spi_clk_out.pad = GPIO_PAD_PULL_UP;
    adc_spi_clk_out.func = GPIO_FUNC_OUT;
    GPIO_Config(&adc_spi_clk_out);

    adc_spi_csb_out.port = ADC_SPI_CSB_PORT_OUT;
    adc_spi_csb_out.mask = ADC_SPI_CSB_PIN_OUT;
    adc_spi_csb_out.pad = GPIO_PAD_PULL_UP;
    adc_spi_csb_out.func = GPIO_FUNC_OUT;
    GPIO_Config(&adc_spi_csb_out);
		
    adc_spi_ch_sel_out.port = ADC_CH_SEL_PORT_OUT;
    adc_spi_ch_sel_out.mask = ADC_CH_SEL_PIN_OUT;
    adc_spi_ch_sel_out.pad = GPIO_PAD_PULL_DOWN;
    adc_spi_ch_sel_out.func = GPIO_FUNC_OUT;
    GPIO_Config(&adc_spi_ch_sel_out);

		elapsed_time_init();		

    while (1) {
		
			// Probing each of the Max19777's two channels in turn
			if(loop_cnt%2 == 0){
				strcpy(a_in_ch, "AIN_CH1");
				GPIO_OutClr(&adc_spi_ch_sel_out);
			}
			else{
				strcpy(a_in_ch, "AIN_CH2");
				GPIO_OutSet(&adc_spi_ch_sel_out);
			}

			execution_time_us = bit_bang_spi(&rx_low, &rx_high); // data transmission

			// Formatting the data and displaying via serial connection			
			printf("CNT = %02ld|  16 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01\n", loop_cnt);

			rx_high_12b = ((int)rx_high) >> 3U;
			printf("%s = ", a_in_ch);
			print_binary(rx_high_12b);
			printf("\n");
			
			float adc_meas = ADC_OUT_SLOPE_CALIBRATION*(rx_high_12b*VDD/((double)(2UL<<(BITS_RESOLUTION-1))))+ADC_OUT_INTERCEPT_CALIBRATION;
			printf("%s = %0d | %05.4fV\n", a_in_ch, rx_high_12b, adc_meas);
			printf("Execution Time (us) = %f\n", execution_time_us);
			loop_cnt++;

			mxc_delay(MXC_DELAY_MSEC(5000));
		}
}
