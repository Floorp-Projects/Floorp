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

#include "nsCalVEvent.h"
#include "nscalcoreicalCIID.h"
#include "tmbevent.h"
#include "nsICalICalendarContainer.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kICalICalendarParserObjectIID, NS_ICALICALENDAR_PARSER_OBJECT_IID);
static NS_DEFINE_IID(kICalICalendarContainerIID, NS_ICALICALENDARCONTAINER_IID);
static NS_DEFINE_IID(kICalVEventIID, NS_ICALVEVENT_IID);
static NS_DEFINE_IID(kICalTimeBasedEventIID, NS_ICALTIMEBASEDEVENT_IID);
static NS_DEFINE_IID(kCCalVEventCID, NS_CALICALENDARVEVENT_CID);

nsCalVEvent::nsCalVEvent()
{
  NS_INIT_REFCNT();
  nsCalVEvent::Init();
}

nsCalVEvent::~nsCalVEvent()
{
  if (mICalComponent)
  {
    delete ((VEvent *) mICalComponent);
    mICalComponent = 0;
  }
}

NS_IMPL_ADDREF(nsCalVEvent)
NS_IMPL_RELEASE(nsCalVEvent)

nsresult nsCalVEvent::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kCCalVEventCID);
  if (aIID.Equals(kClassIID)) {
    *aInstancePtr = (void*) ((nsICalVEvent*)(this));
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(this);
    AddRef();
    return NS_OK;
  }
  return (nsCalTimeBasedEvent::QueryInterface(aIID, aInstancePtr));
}

nsresult nsCalVEvent::Init()
{
  mICalComponent = (ICalComponent *) new VEvent(0);
  return NS_OK;
}

nsresult nsCalVEvent::SetParameter(nsString & aKey, nsString & aValue)
{
  return NS_OK;
}

DateTime nsCalVEvent::GetDTEnd() const 
{
  if (((VEvent *) mICalComponent))
    return (DateTime) ((VEvent *) mICalComponent)->getDTEnd();
  else
    return -1;
}

nsresult nsCalVEvent::SetDTEnd(DateTime d, JulianPtrArray * parameters)
{
  if (((VEvent *) mICalComponent))
    ((VEvent *) mICalComponent)->setDTEnd(d, parameters);
  return NS_OK;    
}

nsresult nsCalVEvent::SetDTEndProperty(nsICalProperty * property)
{
  // todo: assert property is of datetime
  if (((VEvent *) mICalComponent)) {
    DateTime d = *((DateTime *) property->GetValue());
    JulianPtrArray * params = property->GetParameters();
    ((VEvent *) mICalComponent)->setDTEnd(d, params);
  }
  return NS_OK;
}

nsresult nsCalVEvent::SetLocationProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((VEvent *) mICalComponent)->setLocation(s, params);
  return NS_OK;
}
nsresult nsCalVEvent::SetTranspProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((VEvent *) mICalComponent)->setTransp(s, params);
  return NS_OK;
}
nsresult nsCalVEvent::SetGEOProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((VEvent *) mICalComponent)->setGEO(s, params);
  return NS_OK;
}

nsresult nsCalVEvent::SetPriorityProperty(nsICalProperty * property)
{
  // todo: assert property is of integer type
  if (mICalComponent) {
    t_int32 i = *((t_int32 *) (property->GetValue()));
    JulianPtrArray * params = property->GetParameters();
    ((VEvent *) mICalComponent)->setPriority(i, params);
  }
  return NS_OK;
}

nsresult nsCalVEvent::AddResourcesProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  ((VEvent *) mICalComponent)->addResources(s, params);
  return NS_OK;
}

PRBool nsCalVEvent::StoreProperty(nsCalICalendarTag tag, nsICalProperty * prop,
                              JulianPtrArray * vTimeZones)
{
  if (nsnull == mICalComponent)
    return NS_OK; // todo: change to custom error.
  if (nsnull == prop)
    return NS_OK; // todo: change to custom error.

  switch(tag)
  {
    case eCalICalendarTag_dtend:     SetDTEndProperty(prop); break;
      //case eCalICalendarTag_duration: SetDurationProperty(prop); break;
    case eCalICalendarTag_geo:        SetGEOProperty(prop); break;
    case eCalICalendarTag_location:   SetLocationProperty(prop); break;
    case eCalICalendarTag_priority:   SetPriorityProperty(prop); break;
    case eCalICalendarTag_transp:     SetTranspProperty(prop); break;
      //case eCalICalendarTag_resources:  AddResourcesProperty(prop); break;
    default:
      // todo: handle errors && x-tokens
      break;
  }
  return (nsCalTimeBasedEvent::StoreProperty(tag, prop, vTimeZones));
}

/*
nsresult nsCalVEvent::SelfCheck() 
{
  if (((VEvent *) mICalComponent)) {
    ((VEvent *) mICalComponent)->selfCheck();
  }
  return NS_OK;
}
*/

VEvent * nsCalVEvent::GetICalEvent()
{
  return (VEvent *) mICalComponent;
}

PRBool nsCalVEvent::IsValid()
{
  return (((VEvent *)mICalComponent)->isValid());
}






