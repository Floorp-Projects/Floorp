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

#ifndef nsCalVEvent_h___
#define nsCalVEvent_h___

#include "nsCalTimeBasedEvent.h"
#include "nsICalVEvent.h"
#include "vevent.h"
#include "datetime.h"

class NS_CAL_CORE_ICAL nsCalVEvent: public nsCalTimeBasedEvent,
                                    public nsICalVEvent
{
public:
  nsCalVEvent();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init();
  NS_IMETHOD SetParameter(nsString & aKey, nsString & aValue);

  // nsICalVEvent
  NS_IMETHOD_(DateTime) GetDTEnd() const;
  NS_IMETHOD SetDTEnd(DateTime d, JulianPtrArray * parameters);
  NS_IMETHOD SetDTEndProperty(nsICalProperty * property);

  NS_IMETHOD SetGEOProperty(nsICalProperty * property);
  NS_IMETHOD SetLocationProperty(nsICalProperty * property);
  NS_IMETHOD SetPriorityProperty(nsICalProperty * property);
  NS_IMETHOD SetTranspProperty(nsICalProperty * property);
  //NS_IMETHOD SetDurationProperty(nsICalProperty * property);
  NS_IMETHOD AddResourcesProperty(nsICalProperty * property);

  NS_IMETHOD_(PRBool) StoreProperty(nsCalICalendarTag tag, nsICalProperty * prop, 
                                JulianPtrArray * vTimeZones);

  NS_IMETHOD_(VEvent *) GetICalEvent();

  NS_IMETHOD_(PRBool) IsValid();

protected:
  ~nsCalVEvent();
};

#endif





