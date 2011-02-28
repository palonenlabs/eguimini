// This file has been prepared for Doxygen automatic documentation generation.
/*! \file *********************************************************************
 *
 * \brief  Real time clock driver.
 *
 *      The real-time clock driver provides an interface to the asynchronous
 *      timer running off an external 32.768 kHz watch crystal. The driver keeps
 *      track of seconds, minutes, hours, and days. Also, one second is divided
 *      into a number of ticks that corresponds to the rate at which the
 *      asynchronous timer interrupt is called. The default setting is to
 *      configure the timer to overflow every 1/128th second, which is a
 *      suitable tick rate for the polling handler of the Joystick Driver.
 *
 *      In order to make use of the tick for other purposes than just keeping
 *      track of time, the user can register a tick handler, which is a function
 *      that is called after every tick. In the DB101 firmware, the main
 *      processing routine of the Timing Library is registered as the tick
 *      handler of the real-time clock driver. The timing library then takes
 *      care of called the polling routine of the joystick driver.
 *
 *      It is also possible to register a day handler with the driver. The day
 *      handler is called every time the day counter is incremented, and can be
 *      used to implement higher-level calendar functionality.
 *
 *      When migrating between devices, the user might need to reconfigure the RTC
 *      intialization. The timer prescaler settings might need to be changed in
 *      order to achieve the expected tick rate. The settings can be found inside
 *      RTC_Init().
 *
 * \par Application note:
 *      AVR482: DB101 Software
 *
 * \par Documentation
 *      For comprehensive code documentation, supported compilers, compiler
 *      settings and supported devices see readme.html
 *
 * \author
 *      Atmel Corporation: http://www.atmel.com \n
 *      Support email: avr@atmel.com
 *
 * $Revision: 2413 $
 * $URL: http://revisor.norway.atmel.com/AppsAVR8/avr482_db101_software_users_guide/tags/release_A_code_20070917/IAR/rtc_driver/rtc_driver.c $
 * $Date: 2007-09-17 01:49:21 -0600 (ma, 17 sep 2007) $  \n
 *
 * Copyright (c) 2006, Atmel Corporation All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. The name of ATMEL may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY AND
 * SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include <stdbool.h>
#include <cal.h>
#include "rtc_driver.h"


/*******************************
 * Private types and variables.
 *******************************/

volatile uint8_t    RTC_days;     //!< Holds day of week.
volatile uint8_t    RTC_hours;    //!< Holds hour of day.
volatile uint8_t    RTC_minutes;  //!< Holds minute of hour.
volatile uint8_t    RTC_seconds;  //!< Holds second of minute.
volatile RTC_tick_t RTC_ticks;    //!< Holds tick count. Tick granularity depends on prescaler setting.

volatile RTC_TickHandler_t RTC_TickHandler; //!< Tick handler callback function pointer.
volatile RTC_DayHandler_t RTC_DayHandler; //!< Day handler callback function pointer.



/************************************
 * Private function implementations.
 ************************************/

//! This interrupt handler is called every tick.
CAL_ISR( TIMER2_OVF_vect )
{
	// Call external tick handler if present.
	if (RTC_TickHandler != NULL) {
		RTC_TickHandler();
	}
	
	// Count ticks or wrap back to zero.
	if (RTC_ticks < (RTC_TICKS_PER_SECOND - 1)) {
		++RTC_ticks;
	} else {
		RTC_ticks = 0;
		
		// Count seconds or wrap back to zero.
		if (RTC_seconds < (RTC_SECONDS_PER_MINUTE - 1)) {
			++RTC_seconds;
		} else {
			RTC_seconds = 0;
			
			// Count minutes or wrap back to zero.
			if (RTC_minutes < (RTC_MINUTES_PER_HOUR - 1)) {
				++RTC_minutes;
			} else {
				RTC_minutes = 0;
				
				// Count hours or wrap back to zero.
				if (RTC_hours < (RTC_HOURS_PER_DAY - 1)) {
					++RTC_hours;
				} else {
					RTC_hours = 0;
	
					// Count days and overflow when RTC_day_t datatype overflows.
					++RTC_days;
					
					// Call external day handler if present.
					if (RTC_DayHandler != NULL) {
						RTC_DayHandler();
					}
				} // end: if (RTC_hours ...
			} // end: if (RTC_minutes ...
		} // end: if (RTC_seconds ...
	} // end: if (RTC_ticks ...
}



/****************************
 * Function implementations.
 ****************************/

/*! 
 *  This function must be called before doing anything with the library.
 */
void RTC_Init( void )
{
	// Reset handler and time values.
	RTC_TickHandler = NULL;
	RTC_DayHandler = NULL;
	RTC_ticks = 0;
	RTC_seconds = 0;
	RTC_minutes = 0;
	RTC_hours = 0;
	RTC_days = 0;
	
	// Setup asynchronous oscillator.
	PRR0 &= ~(1 << PRTIM2); // Turn on Timer/Counter 2 module.
	TIMSK2 = 0x00; // Disable Timer/Counter 2 interrupts.
	ASSR = (1 << AS2); // Switch to asynchronous clock source.

	TCCR2A = (0 << WGM21) | (0 << WGM20); // Normal counter mode.
	TCCR2B = (0 << WGM22) | (0 << CS22) | (0 << CS21) | (1 << CS20); // No prescaling.
	TCNT2 = 0; // Make sure we start from zero.

	do {} while ((ASSR & ((1 << TCN2UB)  | // Wait for asynchronous clock domain
	                      (1 << OCR2AUB) | // to stabilize and synchronize.
	                      (1 << OCR2BUB) |
	                      (1 << TCR2AUB) |
	                      (1 << TCR2BUB))) != 0x00);

	TIFR2 = (1 << TOV2); // Clear any pending Overflow interrupts.
	TIMSK2 = (1 << TOIE2); // Enable Overflow interrupts.
}


/*! 
 *  The tick handler callback function is called every tick. The tick
 *  granularity is configured in the library configuration. If any
 *  handler is registered before, the link will be replaced.
 *
 * \param  TickHandler  Callback function to call on every tick
 */
void RTC_SetTickHandler( RTC_TickHandler_t TickHandler )
{
	// Make sure we operate without being disturbed by interrupts.
	uint8_t const savedSREG = SREG;
	CAL_disable_interrupt();
	
	// Set handler and notify driver that it's there.
	RTC_TickHandler = TickHandler;
	
	// Restore interrupt state (and rest of status flags, which don't care).	
	SREG = savedSREG;
}

/*! 
 * \return  Current callback function
*/
RTC_TickHandler_t RTC_GetTickHandler( void )
{
	return RTC_TickHandler;
}


void RTC_RemoveTickHandler( void )
{
	// Now, the handler will not be called anymore.
	RTC_TickHandler = NULL;
}

/*! 
 *  The tick handler callback function is called every day change.
 *  If any handler is registered before, the link will be replaced.
 *
 * \param  DayHandler  Callback function to be called every day
 */
void RTC_SetDayHandler( RTC_DayHandler_t DayHandler )
{
	// Make sure we operate without being disturbed by interrupts.
	uint8_t const savedSREG = SREG;
	CAL_disable_interrupt();
	
	// Set handler and notify driver that it's there.
	RTC_DayHandler = DayHandler;
	
	// Restore interrupt state (and rest of status flags, which don't care).	
	SREG = savedSREG;
}


void RTC_RemoveDayHandler( void )
{
	// Now, the handler will not be called anymore.
	RTC_DayHandler = NULL;
}


/*
 * Sets time of day without changing days and weeks. Does no checking of values,
 * so make sure they are correct time values before calling this function.
 *
 * \param  hours  Set to this 24-hours value
 * \param  minutes  Set to this minutes value
 * \param  seconds  Set to this seconds value
 */
void RTC_SetTimeOfDay( uint8_t hours, uint8_t minutes, uint8_t seconds )
{
	// Make sure we operate without being disturbed by interrupts.
	uint8_t const savedSREG = SREG;
	CAL_disable_interrupt();
	
	// Set time values.
	RTC_hours = hours;
	RTC_minutes = minutes;
	RTC_seconds = seconds;
	
	// Restore interrupt state (and rest of status flags, which don't care).
	SREG = savedSREG;
}

/*
 * \param  hours  Pointer to variable that will be set to current hours
 * \param  minutes  Pointer to variable that will be set to current minutes
 * \param  seconds  Pointer to variable that will be set to current seconds
 */
void RTC_GetTimeOfDay( uint8_t * hours, uint8_t * minutes, uint8_t * seconds )
{
	// Make sure we operate without being disturbed by interrupts.
	uint8_t const savedSREG = SREG;
	CAL_disable_interrupt();

	// Copy time values to variables pointed to by provided pointers.	
	*hours = RTC_hours;
	*minutes = RTC_minutes;
	*seconds = RTC_seconds;
	
	// Restore interrupt state (and rest of status flags, which don't care).
	SREG = savedSREG;
}

/*
 * \param  day  Day of week
 */
void RTC_SetDay( RTC_day_t day )
{
	// Make sure we operate without being disturbed by interrupts.
	uint8_t const savedSREG = SREG;
	CAL_disable_interrupt();

	// Update day counter.
	RTC_days = day;
	
	// Restore interrupt state (and rest of status flags, which don't care).
	SREG = savedSREG;
}

/*
 * \return  Current day of the week
 */
RTC_day_t RTC_GetDay( void )
{
	// Make sure we operate without being disturbed by interrupts.
	uint8_t const savedSREG = SREG;
	CAL_disable_interrupt();

	// Copy day counter.
	RTC_day_t day = RTC_days;
	
	// Restore interrupt state (and rest of status flags, which don't care).
	SREG = savedSREG;
	
	return day;
}

/*
 * \param  second  Set to this seconds value
 */
void RTC_SetSecond( uint8_t second )
{
	// Make sure we operate without being disturbed by interrupts.
	uint8_t const savedSREG = SREG;
	CAL_disable_interrupt();

	// Update values.
	RTC_seconds = second;
	RTC_ticks = 0;
	
	// Restore interrupt state (and rest of status flags, which don't care).
	SREG = savedSREG;
}
