/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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








