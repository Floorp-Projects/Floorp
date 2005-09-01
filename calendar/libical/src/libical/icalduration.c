/* -*- Mode: C -*-
  ======================================================================
  FILE: icaltime.c
  CREATOR: eric 02 June 2000
  
  $Id: icalduration.c,v 1.19 2005/02/15 17:38:58 acampi Exp $
  $Locker:  $
    
 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


 ======================================================================*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "icalduration.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "icalerror.h"
#include "icalmemory.h"
#include "icalvalue.h"

#ifdef WIN32
#define snprintf _snprintf
#endif


/* From Seth Alves,  <alves@hungry.com>   */
struct icaldurationtype icaldurationtype_from_int(int t)
{
    struct icaldurationtype dur;
    int used = 0;

    dur = icaldurationtype_null_duration();

    if(t < 0){
	dur.is_neg = 1;
	t = -t;
    }

    if (t % (60 * 60 * 24 * 7) == 0) {
	dur.weeks = t / (60 * 60 * 24 * 7);
    } else {
	used += dur.weeks * (60 * 60 * 24 * 7);
	dur.days = (t - used) / (60 * 60 * 24);
	used += dur.days * (60 * 60 * 24);
	dur.hours = (t - used) / (60 * 60);
	used += dur.hours * (60 * 60);
	dur.minutes = (t - used) / (60);
	used += dur.minutes * (60);
	dur.seconds = (t - used);
    }
 
    return dur;
}

struct icaldurationtype icaldurationtype_from_string(const char* str)
{

    int i;
    int begin_flag = 0;
    int time_flag = 0;
    int date_flag = 0;
    int week_flag = 0;
    int digits=-1;
    int scan_size = -1;
    int size = strlen(str);
    char p;
    struct icaldurationtype d;

    memset(&d, 0, sizeof(struct icaldurationtype));

    for(i=0;i != size;i++){
	p = str[i];
	
	switch(p) 
	    {
	    case '-': {
		if(i != 0 || begin_flag == 1) goto error;

		d.is_neg = 1;
		break;
	    }

	    case 'P': {
		if (i != 0 && i !=1 ) goto error;
		begin_flag = 1;
		break;
	    }

	    case 'T': {
		time_flag = 1;
		break;
	    }

	    case '0':
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
		{ 
		    
		    /* HACK. Skip any more digits if the l;ast one
                       read has not been assigned */
		    if(digits != -1){
			break;
		    }

		    if (begin_flag == 0) goto error;
		    /* Get all of the digits, not one at a time */
		    scan_size = sscanf(&str[i],"%d",&digits);
		    if(scan_size == 0) goto error;
		    break;
		}

	    case 'H': {	
		if (time_flag == 0||week_flag == 1||d.hours !=0||digits ==-1) 
		    goto error;
		d.hours = digits; digits = -1;
		break;
	    }
	    case 'M': {
		if (time_flag == 0||week_flag==1||d.minutes != 0||digits ==-1) 
		    goto error;
		d.minutes = digits; digits = -1;	    
		break;
	    }
	    case 'S': {
		if (time_flag == 0||week_flag==1||d.seconds!=0||digits ==-1) 
		    goto error;
		d.seconds = digits; digits = -1;	    
		break;
	    }
	    case 'W': {
		if (time_flag==1||date_flag==1||d.weeks!=0||digits ==-1) 
		    goto error;
		week_flag = 1;	
		d.weeks = digits; digits = -1;	    
		break;
	    }
	    case 'D': {
		if (time_flag==1||week_flag==1||d.days!=0||digits ==-1) 
		    goto error;
		date_flag = 1;
		d.days = digits; digits = -1;	    
		break;
	    }
	    default: {
		goto error;
	    }

	    }
    }

    return d;
	

 error:
    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
    return icaldurationtype_bad_duration();
}

static
void append_duration_segment(char** buf, char** buf_ptr, size_t* buf_size, 
			     char* sep, unsigned int value) {

    char temp[32];

    snprintf(temp,sizeof(temp),"%d",value);

    icalmemory_append_string(buf, buf_ptr, buf_size, temp);
    icalmemory_append_string(buf, buf_ptr, buf_size, sep);
    
}

char* icaldurationtype_as_ical_string(struct icaldurationtype d) 
{

    char *buf, *output_line;
    size_t buf_size = 256;
    char* buf_ptr = 0;
    int seconds;

    buf = (char*)icalmemory_new_buffer(buf_size);
    buf_ptr = buf;
    

    seconds = icaldurationtype_as_int(d);

    if(seconds !=0){
	
	if(d.is_neg == 1){
	    icalmemory_append_char(&buf, &buf_ptr, &buf_size, '-'); 
	}

	icalmemory_append_char(&buf, &buf_ptr, &buf_size, 'P');
    
	if (d.weeks != 0 ) {
	    append_duration_segment(&buf, &buf_ptr, &buf_size, "W", d.weeks);
	}
	
	if (d.days != 0 ) {
	    append_duration_segment(&buf, &buf_ptr, &buf_size, "D", d.days);
	}
	
	if (d.hours != 0 || d.minutes != 0 || d.seconds != 0) {
	    
	    icalmemory_append_string(&buf, &buf_ptr, &buf_size, "T");
	    
	    if (d.hours != 0 ) {
		append_duration_segment(&buf, &buf_ptr, &buf_size, "H", d.hours);
	    }
	    if (d.minutes != 0 ) {
		append_duration_segment(&buf, &buf_ptr, &buf_size, "M", 
					d.minutes);
	    }
	    if (d.seconds != 0 ) {
		append_duration_segment(&buf, &buf_ptr, &buf_size, "S", 
					d.seconds);
	    }
	    
	}
    } else {
	icalmemory_append_string(&buf, &buf_ptr, &buf_size, "PT0S");
    }
 
    output_line = icalmemory_tmp_copy(buf);
    icalmemory_free_buffer(buf);

    return output_line;
    
}


/* From Russel Steinthal */
int icaldurationtype_as_int(struct icaldurationtype dur)
{
    return (int)( (dur.seconds +
		   (60 * dur.minutes) +
		   (60 * 60 * dur.hours) +
		   (60 * 60 * 24 * dur.days) +
		   (60 * 60 * 24 * 7 * dur.weeks))
		  * (dur.is_neg==1? -1 : 1) ) ;
} 

struct icaldurationtype icaldurationtype_null_duration(void)
{
    struct icaldurationtype d;
    
    memset(&d,0,sizeof(struct icaldurationtype));
    
    return d;
}

int icaldurationtype_is_null_duration(struct icaldurationtype d)
{
    if(icaldurationtype_as_int(d) == 0){
	return 1;
    } else {
	return 0;
    }
}

/* in icalvalue_new_from_string_with_error, we should not call
   icaldurationtype_is_null_duration() to see if there is an error
   condition. Null duration is perfectly valid for an alarm.
   We cannot depend on the caller to check icalerrno either,
   following the philosophy of unix errno. we set the is_neg
   to -1 to indicate that this is a bad duration.
*/
struct icaldurationtype icaldurationtype_bad_duration()
{
    struct icaldurationtype d;
    memset(&d,0,sizeof(struct icaldurationtype));
    d.is_neg = -1;
    return d;
}

int icaldurationtype_is_bad_duration(struct icaldurationtype d)
{
    return (d.is_neg == -1);
}


struct icaltimetype  icaltime_add(struct icaltimetype t,
				  struct icaldurationtype  d)
{
    t.second += d.seconds;
    t.minute += d.minutes;
    t.hour += d.hours;
    t.day += d.days;
    t.day += d.weeks * 7;
    
    t = icaltime_normalize(t);
    
    return t;
}

struct icaldurationtype  icaltime_subtract(struct icaltimetype t1,
					   struct icaltimetype t2)
{

    time_t t1t = icaltime_as_timet(t1);
    time_t t2t = icaltime_as_timet(t2);

    return icaldurationtype_from_int((int)(t1t-t2t));


}

