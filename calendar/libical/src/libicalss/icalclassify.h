/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalclassify.h
 CREATOR: eric 21 Aug 2000


 $Id: icalclassify.h,v 1.4 2001/04/12 18:33:36 ebusboom Exp $
 $Locker:  $

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/


 =========================================================================*/

#ifndef ICALCLASSIFY_H
#define ICALCLASSIFY_H

#include "ical.h"
#include "icalset.h"

icalproperty_xlicclass icalclassify(icalcomponent* c,icalcomponent* match, 
			      const char* user);

icalcomponent* icalclassify_find_overlaps(icalset* set, icalcomponent* comp);

char* icalclassify_class_to_string(icalproperty_xlicclass c);


#endif /* ICALCLASSIFY_H*/


				    


