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

#ifndef nsCalTimeBasedEvent_h___
#define nsCalTimeBasedEvent_h___

#include "jdefines.h"
#include "nsCalICalendarComponent.h"
#include "nsICalTimeBasedEvent.h"
#include "nsICalProperty.h"
#include "tmbevent.h"
#include "nscalcoreicalexp.h"

class NS_CAL_CORE_ICAL nsCalTimeBasedEvent: public nsCalICalendarComponent,
                                            public nsICalTimeBasedEvent
{
protected:
  nsCalTimeBasedEvent();
  ~nsCalTimeBasedEvent();

public:
  NS_IMETHOD Init();

  NS_DECL_ISUPPORTS  

  // nsICalTimeBasedEvent
  /*
  NS_IMETHOD_(nsIDateTime *) GetDTStart() const;
  NS_IMETHOD SetDTStart(nsIDateTime *, JulianPtrArray * parameters = 0);
  NS_IMETHOD SetDTStartProperty(nsICalDateTimeProperty * property);
  */
  NS_IMETHOD_(PRInt32) GetSequence() const;
  NS_IMETHOD SetSequence(PRInt32 i, JulianPtrArray * parameters = 0);
  NS_IMETHOD SetSequenceProperty(nsICalProperty * property);

  NS_IMETHOD AddAttachProperty(nsICalProperty * property);
  NS_IMETHOD AddAttendeeProperty(nsICalProperty * property);
  NS_IMETHOD AddCategoriesProperty(nsICalProperty * property);
  NS_IMETHOD SetClassProperty(nsICalProperty * property);
  NS_IMETHOD AddCommentProperty(nsICalProperty * property);
  NS_IMETHOD AddContactProperty(nsICalProperty * property);
  NS_IMETHOD SetCreatedProperty(nsICalProperty * property);
  NS_IMETHOD SetDescriptionProperty(nsICalProperty * property);
  NS_IMETHOD SetDTStartProperty(nsICalProperty * property);
  NS_IMETHOD SetDTStampProperty(nsICalProperty * property);
  NS_IMETHOD AddExDateProperty(nsICalProperty * property);
  NS_IMETHOD AddExRuleProperty(nsICalProperty * property);
  NS_IMETHOD SetLastModifiedProperty(nsICalProperty * property);
  NS_IMETHOD SetOrganizerProperty(nsICalProperty * property);
  NS_IMETHOD AddRDateProperty(nsICalProperty * property);
  NS_IMETHOD AddRRuleProperty(nsICalProperty * property);
  NS_IMETHOD SetRecurrenceIDProperty(nsICalProperty * property);
  NS_IMETHOD AddRelatedToProperty(nsICalProperty * property);
  NS_IMETHOD AddRequestStatusProperty(nsICalProperty * property);
  NS_IMETHOD SetStatusProperty(nsICalProperty * property);
  NS_IMETHOD SetSummaryProperty(nsICalProperty * property);
  NS_IMETHOD SetUIDProperty(nsICalProperty * property);
  NS_IMETHOD SetURLProperty(nsICalProperty * property);

NS_IMETHOD_(PRBool) StoreProperty(nsCalICalendarTag tag, nsICalProperty * prop, 
                                JulianPtrArray * vTimeZones);

  NS_IMETHOD SelfCheck();


};

#endif




