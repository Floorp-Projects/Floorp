/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalduration.h
 CREATOR: eric 26 Jan 2001


 $Id: icalduration.h,v 1.3 2002/09/26 22:04:56 lindner Exp $
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

#ifndef ICALDURATION_H
#define ICALDURATION_H

#include "icaltime.h"

struct icaldurationtype
{
	int is_neg;
	unsigned int days;
	unsigned int weeks;
	unsigned int hours;
	unsigned int minutes;
	unsigned int seconds;
};

struct icaldurationtype icaldurationtype_from_int(int t);
struct icaldurationtype icaldurationtype_from_string(const char*);
int icaldurationtype_as_int(struct icaldurationtype duration);
char* icaldurationtype_as_ical_string(struct icaldurationtype d);
struct icaldurationtype icaldurationtype_null_duration(void);
struct icaldurationtype icaldurationtype_bad_duration(void);
int icaldurationtype_is_null_duration(struct icaldurationtype d);
int icaldurationtype_is_bad_duration(struct icaldurationtype d);

struct icaltimetype  icaltime_add(struct icaltimetype t,
				  struct icaldurationtype  d);

struct icaldurationtype  icaltime_subtract(struct icaltimetype t1,
					   struct icaltimetype t2);

#endif /* !ICALDURATION_H */



