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

#include "nsCalTimeBasedEvent.h"
#include "nscalcoreicalCIID.h"
#include "tmbevent.h"
#include "nsICalICalendarContainer.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kICalTimeBasedEventIID, NS_ICALTIMEBASEDEVENT_IID);

nsCalTimeBasedEvent::nsCalTimeBasedEvent()
{
  NS_INIT_REFCNT();
}

nsCalTimeBasedEvent::~nsCalTimeBasedEvent()
{
}

NS_IMPL_ADDREF(nsCalTimeBasedEvent)
NS_IMPL_RELEASE(nsCalTimeBasedEvent)

nsresult nsCalTimeBasedEvent::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kICalTimeBasedEventIID);
  if (aIID.Equals(kClassIID)) {
    *aInstancePtr = (void*) ((nsICalTimeBasedEvent*)(this));
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(this);
    AddRef();
    return NS_OK;
  }
  return (nsCalICalendarComponent::QueryInterface(aIID, aInstancePtr));
}

nsresult nsCalTimeBasedEvent::Init()
{
  return NS_OK;
}

PRInt32 nsCalTimeBasedEvent::GetSequence() const 
{
  if (mICalComponent)
    return (PRInt32) ((TimeBasedEvent *) mICalComponent)->getSequence();
  else
    return -1;
}

nsresult nsCalTimeBasedEvent::SetSequence(PRInt32 i, JulianPtrArray * parameters)
{
  if (mICalComponent)
    ((TimeBasedEvent *) mICalComponent)->setSequence(i, parameters);
  return NS_OK;    
}

nsresult nsCalTimeBasedEvent::SetSequenceProperty(nsICalProperty * property)
{
  if (mICalComponent) {
    t_int32 i = *((t_int32 *) (property->GetValue()));
    JulianPtrArray * params = property->GetParameters();
    ((TimeBasedEvent *) mICalComponent)->setSequence(i, params);
  }
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::AddAttachProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->addAttach(s, params);
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::AddAttendeeProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  // todo: finish
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::AddCategoriesProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->addCategories(s, params);
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::SetClassProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->setClass(s, params);
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::AddCommentProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->addComment(s, params);
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::AddContactProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->addContact(s, params);
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::SetCreatedProperty(nsICalProperty * property)
{
  // todo: assert property is of datetime
  if (((TimeBasedEvent *) mICalComponent)) {
    DateTime d = *((DateTime *) property->GetValue());
    JulianPtrArray * params = property->GetParameters();
    ((TimeBasedEvent *) mICalComponent)->setCreated(d, params);
  }
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::SetDescriptionProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->setDescription(s, params);
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::SetDTStartProperty(nsICalProperty * property)
{
  // todo: assert property is of datetime
  if (((TimeBasedEvent *) mICalComponent)) {
    // todo: parse value = data stuff.
    DateTime d = *((DateTime *) property->GetValue());
    JulianPtrArray * params = property->GetParameters();
    ((TimeBasedEvent *) mICalComponent)->setDTStart(d, params);
  }
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::SetDTStampProperty(nsICalProperty * property)
{
  // todo: assert property is of datetime
  if (((TimeBasedEvent *) mICalComponent)) {
    DateTime d = *((DateTime *) property->GetValue());
    JulianPtrArray * params = property->GetParameters();
    ((TimeBasedEvent *) mICalComponent)->setDTStamp(d, params);
  }
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::AddExDateProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->addExDate(s, params);
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::AddExRuleProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  // todo: finish
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::SetLastModifiedProperty(nsICalProperty * property)
{
  // todo: assert property is of datetime
  if (((TimeBasedEvent *) mICalComponent)) {
    DateTime d = *((DateTime *) property->GetValue());
    JulianPtrArray * params = property->GetParameters();
    ((TimeBasedEvent *) mICalComponent)->setDTStamp(d, params);
  }
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::SetOrganizerProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  // todo: finish
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::AddRDateProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  // todo: parse value = DATE, PERIOD
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->addRDate(s, params);
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::AddRRuleProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  // todo: finish
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::SetRecurrenceIDProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  // todo: finish
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::AddRelatedToProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->addRelatedTo(s, params);
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::AddRequestStatusProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->addRequestStatus(s, params);
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::SetStatusProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->setStatus(s, params);
  return NS_OK;
}
nsresult nsCalTimeBasedEvent::SetSummaryProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->setSummary(s, params);
  return NS_OK;
}
nsresult nsCalTimeBasedEvent::SetUIDProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->setUID(s, params);
  return NS_OK;
}
nsresult nsCalTimeBasedEvent::SetURLProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((TimeBasedEvent *) mICalComponent)->setURL(s, params);
  return NS_OK;
}

PRBool nsCalTimeBasedEvent::StoreProperty(nsCalICalendarTag tag, nsICalProperty * prop,
                                      JulianPtrArray * vTimeZones)
{
  if (nsnull == mICalComponent)
    return NS_OK; // todo: change to custom error.
  if (nsnull == prop)
    return NS_OK; // todo: change to custom error.

  switch(tag)
  {
    case eCalICalendarTag_sequence:   SetSequenceProperty(prop);  break;
    default:
      // todo: handle errors && x-tokens
      break;
  }
  return NS_OK;
}

nsresult nsCalTimeBasedEvent::SelfCheck() 
{
  /*
  if (mICalComponent) {
    ((TimeBasedEvent *) mICalComponent)->selfCheck();
  }
  */
  return NS_OK;
}










