/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalperiod.h
 CREATOR: eric 26 Jan 2001


 $Id: icalperiod.h,v 1.4 2002/06/28 11:11:24 acampi Exp $
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

#ifndef ICALPERIOD_H
#define ICALPERIOD_H

#include "icaltime.h"
#include "icalduration.h"

struct icalperiodtype 
{
	struct icaltimetype start; 
	struct icaltimetype end; 
	struct icaldurationtype duration;
};

struct icalperiodtype icalperiodtype_from_string (const char* str);

const char* icalperiodtype_as_ical_string(struct icalperiodtype p);

struct icalperiodtype icalperiodtype_null_period(void);

int icalperiodtype_is_null_period(struct icalperiodtype p);

int icalperiodtype_is_valid_period(struct icalperiodtype p);

#endif /* !ICALTIME_H */



