/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalcalendar.h
 CREATOR: eric 23 December 1999


 $Id: icalcalendar.h,v 1.2 2002/06/27 02:30:58 acampi Exp $
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

#ifndef ICALCALENDAR_H
#define ICALCALENDAR_H

#include "ical.h"
#include "icalset.h"

/* icalcalendar
 * Routines for storing calendar data in a file system. The calendar 
 * has two icaldirsets, one for incoming components and one for booked
 * components. It also has interfaces to access the free/busy list
 * and a list of calendar properties */

typedef struct icalcalendar_impl icalcalendar;

icalcalendar* icalcalendar_new(char* dir);

void icalcalendar_free(icalcalendar* calendar);

int icalcalendar_lock(icalcalendar* calendar);

int icalcalendar_unlock(icalcalendar* calendar);

int icalcalendar_islocked(icalcalendar* calendar);

int icalcalendar_ownlock(icalcalendar* calendar);

icalset* icalcalendar_get_booked(icalcalendar* calendar);

icalset* icalcalendar_get_incoming(icalcalendar* calendar);

icalset* icalcalendar_get_properties(icalcalendar* calendar);

icalset* icalcalendar_get_freebusy(icalcalendar* calendar);


#endif /* !ICALCALENDAR_H */



