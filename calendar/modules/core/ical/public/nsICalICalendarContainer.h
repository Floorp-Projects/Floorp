/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsICalICalendarContainer_h___
#define nsICalICalendarContainer_h___

#include "nsISupports.h"
#include "nsCalICalendarTags.h"
#include "nsICalProperty.h"
#include "ptrarray.h"

// 1ed5e9f0-6912-11d2-943d-006008268c31
#define NS_ICALICALENDARCONTAINER_IID      \
 { 0x1ed5e9f0, 0x6912, 0x11d2, \
   {0x94, 0x3d, 0x00, 0x60, 0x08, 0x26, 0x8c, 0x31} }

/* VCALENDAR, VEVENT, VTODO, VJOURNAL, VTIMEZONE, VFREEBUSY, VALARM should implement this */
class nsICalICalendarContainer : public nsISupports 
{

public:

  /**
   * Initialize the CALICALENDARContainer
   * @result The result of the initialization, NS_Ok if no errors
   */
  NS_IMETHOD Init() = 0;

  // for now
  NS_IMETHOD_(PRBool) StoreProperty(nsCalICalendarTag tag, nsICalProperty * prop, 
                                    JulianPtrArray * vTimeZones) = 0;

};

#endif /* nsICALICalendarContainer_h___ */




