/* -*- Mode: C -*-
  ======================================================================
  FILE: icalperiod.c
  CREATOR: eric 02 June 2000
  
  $Id: icalperiod.c,v 1.11 2002/10/30 21:22:03 acampi Exp $
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

#include "icalperiod.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "icalerror.h"
#include "icalmemory.h"




struct icalperiodtype icalperiodtype_from_string (const char* str)
{
    
    struct icalperiodtype p, null_p;
    char *s = icalmemory_strdup(str);
    char *start, *end = s;
    icalerrorstate es;

    /* Errors are normally generated in the following code, so save
       the error state for resoration later */

    icalerrorenum e = icalerrno;

    p.start = p.end = icaltime_null_time();
    p.duration = icaldurationtype_from_int(0);

    null_p = p;

    if(s == 0) goto error;

    start = s;
    end = strchr(s, '/');

    if(end == 0) goto error;

    *end = 0;
    end++;

    p.start = icaltime_from_string(start);

    if (icaltime_is_null_time(p.start)) goto error;

    es = icalerror_get_error_state(ICAL_MALFORMEDDATA_ERROR);
    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,ICAL_ERROR_NONFATAL);

    p.end = icaltime_from_string(end);

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,es);
    

    if (icaltime_is_null_time(p.end)){

	p.duration = icaldurationtype_from_string(end);

	if(icaldurationtype_as_int(p.duration) == 0) goto error;
    } 

    icalerrno = e;

    icalmemory_free_buffer(s);

    return p;

 error:
    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);

    if (s)
	icalmemory_free_buffer (s);
    return null_p;
}


const char* icalperiodtype_as_ical_string(struct icalperiodtype p)
{

    const char* start;
    const char* end;

    char *buf;
    size_t buf_size = 40;
    char* buf_ptr = 0;

    buf = (char*)icalmemory_new_buffer(buf_size);
    buf_ptr = buf;
    

    start = icaltime_as_ical_string(p.start);

    icalmemory_append_string(&buf, &buf_ptr, &buf_size, start); 

    if(!icaltime_is_null_time(p.end)){
	end = icaltime_as_ical_string(p.end);
    } else {
	end = icaldurationtype_as_ical_string(p.duration);
    }

    icalmemory_append_char(&buf, &buf_ptr, &buf_size, '/'); 

    icalmemory_append_string(&buf, &buf_ptr, &buf_size, end); 
    
	icalmemory_add_tmp_buffer(buf);

    return buf;
}



struct icalperiodtype icalperiodtype_null_period(void) {
    struct icalperiodtype p;
    p.start = icaltime_null_time();
    p.end = icaltime_null_time();
    p.duration = icaldurationtype_null_duration();

    return p;
}
int icalperiodtype_is_null_period(struct icalperiodtype p){
    
    if(icaltime_is_null_time(p.start) && 
       icaltime_is_null_time(p.end) && 
       icaldurationtype_is_null_duration(p.duration)){
	return 1;
    } else {
	return 0;
    }
}

int icalperiodtype_is_valid_period(struct icalperiodtype p){
    if(icaltime_is_valid_time(p.start) && 
       (icaltime_is_valid_time(p.end) || icaltime_is_null_time(p.end)) )
	{
	    return 1;
	}

    return 0;
}

