/*
 *	Copyright (c) 1986-2000, Hiram Clawson 
 *	All rights reserved.
 *
 *	Redistribution and use in source and binary forms, with or
 *	without modification, are permitted provided that the following
 *	conditions are met:
 *
 *		Redistributions of source code must retain the above
 *		copyright notice, this list of conditions and the
 *		following disclaimer.
 *
 *		Redistributions in binary form must reproduce the
 *		above copyright notice, this list of conditions and
 *		the following disclaimer in the documentation and/or
 *		other materials provided with the distribution.
 *
 *		Neither name of The Museum of Hiram nor the names of
 *		its contributors may be used to endorse or promote products
 *		derived from this software without specific prior
 *		written permission. 
 *
 *	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *	CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *	IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *	INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 *	OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *	HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 *	STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 *	IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *	THE POSSIBILITY OF SUCH DAMAGE. 
 */

/**
 *	@file astime.h
 *      @brief contains definitions of structures used for time calculations.
 */

#ifndef	_astime_h_
#define _astime_h_

typedef struct ut_instant {
	double	j_date; /**< julian decimal date, 0 = 01 Jan 4713 BC 12 HR UT */
	long year;      /**< year, valid range [-4,713, +2,147,483,647] */
	int	month;	/**<	[1-12]	*/
	int	day;	/**<	[1-31]	*/
	int	i_hour;	/**<	[0-23]	*/
	int	i_minute;	/**<	[0-59]	*/
	int	i_second;	/**<	[0-59]	*/
	double	d_hour;		/**< [0.0-23.9999] includes minute and second */
	double	d_minute;	/**<	[0.0-59.9999] includes second	*/
	double	d_second;	/**<	[0.0-59.9999]	*/
	int	weekday;	/**<	[0-6]	*/
	int	day_of_year;	/**<	[1-366]	*/
} UTinstant, * UTinstantPtr;

/*	Functions in caldate.c	*/

long caldat( UTinstantPtr );	/** converts julian date to year,mo,da */
double juldat( UTinstantPtr );	/** returns julian day from year,mo,da */

#endif	/*	_astime_h_	*/
