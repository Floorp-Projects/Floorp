/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalgaugeimpl.h
 CREATOR: eric 09 Aug 2000


 $Id: icalgaugeimpl.h,v 1.6 2002/07/21 17:18:15 lindner Exp $
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

#include "ical.h"

typedef enum icalgaugecompare {
    ICALGAUGECOMPARE_EQUAL=ICAL_XLICCOMPARETYPE_EQUAL,
    ICALGAUGECOMPARE_LESS=ICAL_XLICCOMPARETYPE_LESS,
    ICALGAUGECOMPARE_LESSEQUAL=ICAL_XLICCOMPARETYPE_LESSEQUAL,
    ICALGAUGECOMPARE_GREATER=ICAL_XLICCOMPARETYPE_GREATER,
    ICALGAUGECOMPARE_GREATEREQUAL=ICAL_XLICCOMPARETYPE_GREATEREQUAL,
    ICALGAUGECOMPARE_NOTEQUAL=ICAL_XLICCOMPARETYPE_NOTEQUAL,
    ICALGAUGECOMPARE_REGEX=ICAL_XLICCOMPARETYPE_REGEX,
    ICALGAUGECOMPARE_ISNULL=ICAL_XLICCOMPARETYPE_ISNULL,
    ICALGAUGECOMPARE_ISNOTNULL=ICAL_XLICCOMPARETYPE_ISNOTNULL,
    ICALGAUGECOMPARE_NONE=0
} icalgaugecompare;

typedef enum icalgaugelogic {
    ICALGAUGELOGIC_NONE,
    ICALGAUGELOGIC_AND,
    ICALGAUGELOGIC_OR
} icalgaugelogic;
    

struct icalgauge_where {
	icalgaugelogic logic;
	icalcomponent_kind comp;
	icalproperty_kind prop;
	icalgaugecompare compare;
	char* value;
};

struct icalgauge_impl
{
	pvl_list select; /**< Of icalgaugecompare, using only prop and comp fields*/
	pvl_list from;   /**< List of component_kinds, as integers */
	pvl_list where;  /**< List of icalgaugecompare */
        int      expand;
};


