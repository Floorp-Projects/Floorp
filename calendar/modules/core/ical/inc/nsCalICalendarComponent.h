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

#ifndef nsCalICalendarComponent_h___
#define nsCalICalendarComponent_h___

#include "jdefines.h"
#include "nsICalICalendarComponent.h"
#include "nsICalICalendarContainer.h"
#include "nsICalProperty.h"
#include "nsICalICalendarParserObject.h"
#include "icalcomp.h"

class NS_CAL_CORE_ICAL nsCalICalendarComponent: public nsICalICalendarComponent,
                                                public nsICalICalendarContainer,
                                                public nsICalICalendarParserObject
{

protected:
  nsCalICalendarComponent();
  ~nsCalICalendarComponent();

public:

  NS_IMETHOD Init();

  NS_DECL_ISUPPORTS

  /* nsICalICalendarParserObject */
  NS_IMETHOD SetParameter(nsString & aKey, nsString & aValue);

  NS_IMETHOD_(PRBool) StoreProperty(nsCalICalendarTag tag, nsICalProperty * prop, 
                                    JulianPtrArray * vTimeZones);  


protected:
  ICalComponent * mICalComponent;

};

#endif








