/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalcsdb.h Calendar Server Database
 CREATOR: eric 23 December 1999


 $Id: icalcsdb.h,v 1.1.1.1 2001/01/02 07:33:03 ebusboom Exp $
 $Locker:  $

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/


======================================================================*/

#ifndef ICALCSDB_H
#define ICALCSDB_H

#include "ical.h"

typedef void icalcsdb;

icalcsdb* icalcsdb_new(char* path);

void icalcsdb_free(icalcsdb* csdb);

icalerrorenum icalcsdb_create(icalcsdb* db, char* calid);

icalerrorenum icalcsdb_delete(icalcsdb* db, char* calid);

icalerrorenum icalcsdb_move(icalcsdb* db, char* oldcalid, char* newcalid);

icalerrorenum icalcsdb_noop(icalcsdb* db);

char* icalcsdb_generateuid(icalcsdb* db);

icalcomponent* icalcsdb_expand_upn(icalcsdb* db, char* upn);
icalcomponent* icalcsdb_expand_calid(icalcsdb* db, char* calid);

icalerrorenum icalcsbd_senddata(icalcsdb* db, icalcomponent* comp);

icalset* icalcsdb_get_calendar(icalcsdb* db, char* calid, 
			       icalcomponent *gauge);

icalset* icalcsdb_get_vcars(icalcsdb* db);

icalset* icalcsdb_get_properties(icalcsdb* db);

icalset* icalcsdb_get_capabilities(icalcsdb* db);

icalset* icalcsdb_get_timezones(icalcsdb* db);


#endif /* !ICALCSDB_H */



