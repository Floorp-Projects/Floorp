/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* *
 * 
 *
 * xp_thrmo.c --- Status message text for the thermometer.
 */

#include "xp.h"
#include "xp_thrmo.h"
#include "xpgetstr.h"
#include <ctype.h>

extern int XP_THERMO_BYTE_FORMAT;
extern int XP_THERMO_KBYTE_FORMAT;
extern int XP_THERMO_HOURS_FORMAT;
extern int XP_THERMO_MINUTES_FORMAT;
extern int XP_THERMO_SECONDS_FORMAT;
extern int XP_THERMO_SINGULAR_FORMAT;
extern int XP_THERMO_PLURAL_FORMAT;
extern int XP_THERMO_PERCENTAGE_FORMAT;
extern int XP_THERMO_UH;
extern int XP_THERMO_PERCENT_FORM;
extern int XP_THERMO_PERCENT_RATE_FORM;
extern int XP_THERMO_RAW_COUNT_FORM;
extern int XP_THERMO_BYTE_RATE_FORMAT;
extern int XP_THERMO_K_RATE_FORMAT;
extern int XP_THERMO_M_RATE_FORMAT;
extern int XP_THERMO_STALLED_FORMAT;
extern int XP_THERMO_RATE_REMAINING_FORM;
extern int XP_THERMO_RATE_FORM;


#define KILOBYTE		(1024L)
#define MEGABYTE		(KILOBYTE * KILOBYTE)
#define MINUTE			(60L)
#define HOUR			(MINUTE * MINUTE)

#define BYTE_FORMAT		 XP_GetString(XP_THERMO_BYTE_FORMAT)
#define KILOBYTE_FORMAT	 XP_GetString(XP_THERMO_KBYTE_FORMAT)
#define MEGABYTE_FORMAT  XP_GetString(XP_THERMO_MBYTE_FORMAT)

#define BYTE_RATE_FORMAT	XP_GetString(XP_THERMO_BYTE_RATE_FORMAT)
#define K_RATE_FORMAT		XP_GetString(XP_THERMO_K_RATE_FORMAT)
#define M_RATE_FORMAT		XP_GetString(XP_THERMO_M_RATE_FORMAT)
#define STALLED_FORMAT		XP_GetString(XP_THERMO_STALLED_FORMAT)

#define HOURS_FORMAT		   XP_GetString(XP_THERMO_HOURS_FORMAT)
#define MINUTES_FORMAT		XP_GetString(XP_THERMO_MINUTES_FORMAT)
#define SECONDS_FORMAT		XP_GetString(XP_THERMO_SECONDS_FORMAT)

#define PERCENTAGE_FORMAT	XP_GetString(XP_THERMO_PERCENTAGE_FORMAT)

#define UH					XP_GetString(XP_THERMO_UH)

#define IS_PLURAL(x)		(((x) == 1) ? "" : XP_GetString(XP_THERMO_PLURAL_FORMAT) )		/* L10N? */

#define	ENOUGH_TIME_TO_GUESS	5L /* #### customizable? */

#define RATE_REMAINING_FORM	XP_GetString(XP_THERMO_RATE_REMAINING_FORM)
#define RATE_FORM				   XP_GetString(XP_THERMO_RATE_FORM)
#define PERCENT_FORM			   XP_GetString(XP_THERMO_PERCENT_FORM)
#define PERCENT_RATE_FORM		XP_GetString(XP_THERMO_PERCENT_RATE_FORM)
#define RAW_COUNT_FORM			XP_GetString(XP_THERMO_RAW_COUNT_FORM)

#define STALL_TIME				4L

/* Returns a text string to describe the current progress of some/all
   transfers in progress.

   total_bytes		How many bytes we are waiting for, of all documents
					in progress.  If this is unknown, it should be 0.
					Note that if four documents are in progress, and
					the sizes of three are known and the size of one is
					unknown, then the total_bytes should be 0, since
					a single unknown makes the total unknown.

    bytes_received	How many bytes have been read so far.  If total_bytes
					is non-0, and this is larger than total_bytes, then
					there is a bug.

    start_time_secs	The time at which the transfer started, in seconds.

    now_secs		The current time, in seconds.  The 0-point of these
					values is uninteresting, only the relationship between
					The two.  It would be fine for start_time_secs to 
					always be 0, and now_secs to be the number of seconds
					since the transfer began.

  Returns: a statically allocated string, or NULL. NULL is returned in
  out-of-memory conditions, or when there is nothing to say (as would
  happen early in the transfer, if all values were 0.) DO NOT FREE THE
  RETURNED STRING.

  The caller is responsible for freeing the returned string. */

const char*
XP_ProgressText( unsigned long total_bytes,
				 unsigned long bytes_received,
				 unsigned long start_time_secs,
				 unsigned long now_secs )
{
	/* This is all fairly hairy, but generating sensible human-readable text
	 always is.   Also, this doesn't internationalize very well... */
	static char*	output = NULL;
	static char*	scratch = NULL;
	static unsigned long	last_secs = 0;
	static unsigned long	last_bytes_received = 0;
	static unsigned long	last_secs_left = -1;
	static unsigned long	last_secs_received = 0;
	static unsigned long	last_total = -1;
	
	XP_Bool			size_known = total_bytes > 0;
	XP_Bool			stalled = FALSE;
	
	unsigned long	bytes_remaining = total_bytes - bytes_received;
#if 0
	float			fmegs_received = (float)bytes_received / (float)MEGABYTE;
	unsigned long	delta_received = bytes_received - last_bytes_received;
	long			delta_time = now_secs - last_secs;
#endif
	unsigned long	elapsed_time = now_secs - start_time_secs;

	float			bytes_per_sec = 0;
	long			secs_left = -1;
	long 			how_long_since = 0;
	
	char*			brief_length;
	char*			percent;			/* bytes_received / total_bytes */
	char*			rate;				/* bytes_received / elapsed_time */		
	char*			tleft;				/* bytes_remaining / rate */
	
	/* allocate our static buffers */
	if ( !output )
	{
		output = (char*) XP_ALLOC( 300 ); /* what if this ain't enough? */
		if ( !output )
			return NULL;
	}
	if ( !scratch )
	{
		scratch = (char*) XP_ALLOC( 300 );
		if ( !scratch )
		{
			XP_FREE( output );
			output = NULL;
			return NULL;
		}
	}	
	/* scratch is just a buffer that we divide up for several different
	purposes, instead of mallocing a few small chunks.  output is
	the buffer that we eventually write into when combining these
	various chunks together. */
	brief_length = scratch;
	percent = brief_length + 20;
	rate = percent + 20;
	tleft = rate + 100;

	/* key off of total_bytes and update statics when it changes */	
	if ( total_bytes != last_total && total_bytes > 0 )
	{
		last_secs = 0;
		last_bytes_received = 0;
		last_secs_left = -1;
		last_secs_received = 0;
		last_total = total_bytes;
	}
	
	if ( total_bytes < KILOBYTE )
		sprintf( brief_length, BYTE_FORMAT, total_bytes );
	else
		sprintf( brief_length, KILOBYTE_FORMAT, total_bytes / KILOBYTE );
	
	/* boundary conditions */
	if ( bytes_received <= 0 || start_time_secs <= 0 ||
		 start_time_secs >= now_secs ) 
	{
		*rate = 0;
		*tleft = 0;
	}
	else
	{
		/* build rate and time left strings */

		if ( bytes_received < last_bytes_received )
			last_bytes_received = bytes_received;
			
		if ( elapsed_time > 0 )
			bytes_per_sec = ( (float)bytes_received / (float)elapsed_time );
		else
			bytes_per_sec = 0;
			
		if ( bytes_received == last_bytes_received && bytes_received != total_bytes )
		{
			how_long_since = now_secs - last_secs_received;
			if ( how_long_since > STALL_TIME )
				stalled = TRUE;
		}
		else
			last_secs_received = now_secs;
			
        /* someone is mixing the streams --- just bail.
           this sucks
           chouck 18-Sep-95
         */
        if ( bytes_per_sec < 0 )
        	return NULL;

		if ( elapsed_time < ENOUGH_TIME_TO_GUESS )
			;
		else if (bytes_per_sec != 0)
		{
			secs_left = (long)( size_known ? ( (float)bytes_remaining / bytes_per_sec ) : 0);
	
			if ( secs_left > last_secs_left )
				secs_left = last_secs_left;		
			else
				last_secs_left = secs_left;
		}
		
		/* build rate string */
		if ( elapsed_time < ENOUGH_TIME_TO_GUESS )
			*rate = 0;
		else if ( stalled )
			sprintf( rate, STALLED_FORMAT );
		else if ( bytes_per_sec < KILOBYTE )
			sprintf( rate, BYTE_RATE_FORMAT, (long) bytes_per_sec );
		else
		{
			double		tmp;
			tmp = (double)bytes_per_sec / (double)KILOBYTE;	
			sprintf( rate, K_RATE_FORMAT, tmp );
		}

		/* build time left string */
		if (	secs_left <= 0 ||
				elapsed_time < ENOUGH_TIME_TO_GUESS ||
				bytes_per_sec < KILOBYTE )
			*tleft = 0;
		else if ( secs_left >= HOUR )
			sprintf( tleft, HOURS_FORMAT,
					 secs_left / HOUR,
					 (secs_left / MINUTE) % MINUTE,
					 secs_left % MINUTE );
		else if ( secs_left >= MINUTE )
			sprintf( tleft, MINUTES_FORMAT, secs_left / MINUTE, secs_left % MINUTE );
		else
			sprintf( tleft, SECONDS_FORMAT, secs_left, IS_PLURAL( secs_left ) );
	}
	
	/* build percentage string */
	if ( size_known && total_bytes > 0 )
	{
		int p = ( bytes_received * 100 ) / total_bytes;
		/* Allow it to get >100 so that we notice when netlib is lying to
		 us, but don't treat 99.8% as 100% because people look at the
		 100% and read "done" instead of "almost done" and wonder what
		 it's doing. */
		if ( p >= 100 && ( bytes_received != total_bytes ) )
			p = 99;
		sprintf( percent, PERCENTAGE_FORMAT, p );
	}
	else
	{
		if ( bytes_received < KILOBYTE )
			sprintf( percent, UH, bytes_received, IS_PLURAL( bytes_received ) );
		else
			sprintf( percent, KILOBYTE_FORMAT, bytes_received / KILOBYTE );
	}
	
	
	/* build output string */
	if ( size_known )
	{
		if ( total_bytes )
		{
			if ( *tleft )
			{
				/* "%s of %s (at %s, %s remaining)" */
				sprintf( output, RATE_REMAINING_FORM, percent, brief_length, rate, tleft );
			}
			else if ( *rate )
			{
				/* "%s of %s (at %s)" */
				sprintf( output, RATE_FORM, percent, brief_length, rate );
			}
			else
			{
				/* "%s of %s" */
				sprintf( output, PERCENT_FORM, percent, brief_length );
			}
		}
	}
	else if ( bytes_received > 0 )
	{
		if ( *rate )
		{
			/* "%s read (at %s)" */
			sprintf( output, PERCENT_RATE_FORM, percent, rate );
		}
		else
			sprintf( output, RAW_COUNT_FORM, percent );
	}
	else
		*output = 0;
	
#if 0
	XP_TRACE(( "\n" ));
	XP_TRACE(( "know size:%d\n", size_known ));
	XP_TRACE(( "total:%d rec:%d last_rec:%d\n", total_bytes, bytes_received, last_bytes_received));
	XP_TRACE(( "now:%d last:%d lsr:%d\n", now_secs, last_secs, last_secs_received));
	XP_TRACE(( "left:%d last_left:%d\n", secs_left, last_secs_left));
	XP_TRACE(( "d:%d e:%d bps:%f\n", delta_time, elapsed_time, bytes_per_sec ));
	XP_TRACE(( "ti:%d\n", how_long_since ));
#endif

	last_secs = now_secs;
	last_bytes_received = bytes_received;
	
	if ( *output )
	{
		return output;
	}
	else
	{
		return NULL;
	}
}
