/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalrestriction.h
  CREATOR: eric 24 April 1999
  
  $Id: icalrestriction.h,v 1.1.1.1 2001/01/02 07:33:02 ebusboom Exp $


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalrestriction.h

  Contributions from:
     Graham Davison (g.m.davison@computer.org)


======================================================================*/

#include "icalproperty.h"
#include "icalcomponent.h"

#ifndef ICALRESTRICTION_H
#define ICALRESTRICTION_H

/* These must stay in this order for icalrestriction_compare to work */
typedef enum icalrestriction_kind {
    ICAL_RESTRICTION_NONE=0,		/* 0 */
    ICAL_RESTRICTION_ZERO,		/* 1 */
    ICAL_RESTRICTION_ONE,		/* 2 */
    ICAL_RESTRICTION_ZEROPLUS,		/* 3 */
    ICAL_RESTRICTION_ONEPLUS,		/* 4 */
    ICAL_RESTRICTION_ZEROORONE,		/* 5 */
    ICAL_RESTRICTION_ONEEXCLUSIVE,	/* 6 */
    ICAL_RESTRICTION_ONEMUTUAL,		/* 7 */
    ICAL_RESTRICTION_UNKNOWN		/* 8 */
} icalrestriction_kind;

int 
icalrestriction_compare(icalrestriction_kind restr, int count);


int
icalrestriction_is_parameter_allowed(icalproperty_kind property,
                                       icalparameter_kind parameter);

int icalrestriction_check(icalcomponent* comp);


#endif /* !ICALRESTRICTION_H */



