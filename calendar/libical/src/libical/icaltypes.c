/* -*- Mode: C -*-
  ======================================================================
  FILE: icaltypes.c
  CREATOR: eric 16 May 1999
  
  $Id: icaltypes.c,v 1.15 2002/10/30 23:59:57 acampi Exp $
  $Locker:  $
    

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icaltypes.c

 ======================================================================*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "icaltypes.h"
#include "icalerror.h"
#include "icalmemory.h"
#include <stdlib.h> /* for malloc and abs() */
#include <errno.h> /* for errno */
#include <string.h> /* for icalmemory_strdup */
#include <assert.h>

#ifdef WIN32
#define snprintf      _snprintf
#define strcasecmp    stricmp
#endif

#define TEMP_MAX 1024


int icaltriggertype_is_null_trigger(struct icaltriggertype tr)
{
    if(icaltime_is_null_time(tr.time) && 
       icaldurationtype_is_null_duration(tr.duration)){
        return 1;
    }

    return 0;
}
    
int icaltriggertype_is_bad_trigger(struct icaltriggertype tr)
{
    if(icaldurationtype_is_bad_duration(tr.duration)){
        return 1;
    }

    return 0;
}

struct icaltriggertype icaltriggertype_from_int(const int reltime)
{
    struct icaltriggertype tr;

    tr.time	= icaltime_null_time();
    tr.duration = icaldurationtype_from_int(reltime);

    return tr;
}

struct icaltriggertype icaltriggertype_from_string(const char* str)
{

    
    struct icaltriggertype tr, null_tr;
    icalerrorstate es;
    icalerrorenum e;

    tr.time= icaltime_null_time();
    tr.duration = icaldurationtype_from_int(0);

    null_tr = tr;

    if(str == 0) goto error;

    /* Suppress errors so a failure in icaltime_from_string() does not cause an abort */
    es = icalerror_get_error_state(ICAL_MALFORMEDDATA_ERROR);
    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,ICAL_ERROR_NONFATAL);
    e = icalerrno;
    icalerror_set_errno(ICAL_NO_ERROR);

    tr.time = icaltime_from_string(str);

    if (icaltime_is_null_time(tr.time)){

	tr.duration = icaldurationtype_from_string(str);

        if (icaldurationtype_is_bad_duration(tr.duration)) goto error;
    } 

    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,es);
    icalerror_set_errno(e);
    return tr;

 error:
    icalerror_set_error_state(ICAL_MALFORMEDDATA_ERROR,es);
    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
    return tr;

}


struct icalreqstattype icalreqstattype_from_string(const char* str)
{
  const char *p1,*p2;
  struct icalreqstattype stat;
  short major=0, minor=0;

  icalerror_check_arg((str != 0),"str");

  stat.code = ICAL_UNKNOWN_STATUS;
  stat.debug = 0; 
   stat.desc = 0;

  /* Get the status numbers */

  sscanf(str, "%hd.%hd",&major, &minor);

  if (major <= 0 || minor < 0){
    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
    return stat;
  }

  stat.code = icalenum_num_to_reqstat(major, minor);

  if (stat.code == ICAL_UNKNOWN_STATUS){
    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
    return stat;
  }
  

  p1 = strchr(str,';');

  if (p1 == 0){
/*    icalerror_set_errno(ICAL_BADARG_ERROR);*/
    return stat;
  }

  /* Just ignore the second clause; it will be taken from inside the library 
   */



  p2 = strchr(p1+1,';');
  if (p2 != 0 && *p2 != 0){
    stat.debug = p2+1;
  } 

  return stat;
  
}

const char* icalreqstattype_as_string(struct icalreqstattype stat)
{
  char *temp;

  temp = (char*)icalmemory_tmp_buffer(TEMP_MAX);

  icalerror_check_arg_rz((stat.code != ICAL_UNKNOWN_STATUS),"Status");
  
  if (stat.desc == 0){
    stat.desc = icalenum_reqstat_desc(stat.code);
  }
  
  if(stat.debug != 0){
    snprintf(temp,TEMP_MAX,"%d.%d;%s;%s", icalenum_reqstat_major(stat.code),
             icalenum_reqstat_minor(stat.code),
             stat.desc, stat.debug);
    
  } else {
    snprintf(temp,TEMP_MAX,"%d.%d;%s", icalenum_reqstat_major(stat.code),
             icalenum_reqstat_minor(stat.code),
             stat.desc);
  }

  return temp;
}
