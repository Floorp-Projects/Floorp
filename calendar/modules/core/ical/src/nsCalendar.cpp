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

#include "nsCalendar.h"
//#include "nsICalICalendarParserObject.h"
#include "nscalcoreicalCIID.h"
#include "nscal.h"
#include "nsICalICalendarContainer.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kICalICalendarParserObjectIID, NS_ICALICALENDAR_PARSER_OBJECT_IID);
static NS_DEFINE_IID(kICalICalendarContainerIID, NS_ICALICALENDARCONTAINER_IID);
static NS_DEFINE_IID(kICalendarIID, NS_ICALENDAR_IID);

nsCalendar::nsCalendar()
{
  NS_INIT_REFCNT();
  Init();
}

nsCalendar::~nsCalendar()
{
  if (mCalendar) {
    delete mCalendar;
    mCalendar = nsnull;
  }
}

// implement ISupports functions (addref, release, query-interface)
NS_IMPL_ADDREF(nsCalendar)
NS_IMPL_RELEASE(nsCalendar)

nsresult nsCalendar::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsICalendar*)(this));
  }
  else if (aIID.Equals(kICalICalendarParserObjectIID)) {
    *aInstancePtr = (void*)(this);
  }
  else if (aIID.Equals(kICalICalendarContainerIID)) {
    *aInstancePtr = (void*) ((nsICalICalendarContainer*)(this));
  }
  else if (aIID.Equals(kICalendarIID)) {
    *aInstancePtr = (void*)(this);
  }
  else  {
    *aInstancePtr = 0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;
}

nsresult nsCalendar::Init()
{
  mCalendar = new NSCalendar();
  return NS_OK;
}

nsresult nsCalendar::SetParameter(nsString & aKey, nsString & aValue)
{
  return NS_OK;
}

UnicodeString nsCalendar::GetCalScale() const
{
  return mCalendar->getCalScale();
}

nsresult nsCalendar::SetCalScale(UnicodeString s, JulianPtrArray * parameters)
{
  mCalendar->setCalScale(s, parameters);
  return NS_OK;
}

/*
nsICalProperty * nsCalendar::GetCalScaleProperty() const
{
  return mCalendar->getCalScaleProperty();
}
*/

UnicodeString nsCalendar::GetVersion() const
{
  return mCalendar->getVersion();
}

nsresult nsCalendar::SetVersion(UnicodeString s, JulianPtrArray * parameters)
{
  mCalendar->setVersion(s, parameters);
  return NS_OK;
}

nsresult nsCalendar::SetVersionProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  mCalendar->setVersion(s, params);
  return NS_OK;
}

nsresult nsCalendar::SetProdidProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  mCalendar->setProdid(s, params);
  return NS_OK;
}

nsresult nsCalendar::SetCalScaleProperty(nsICalProperty * property)
{
  // todo: assert property is of string type
  UnicodeString s = *((UnicodeString* ) property->GetValue());
  JulianPtrArray * params = property->GetParameters();
  mCalendar->setCalScale(s, params);
  return NS_OK;
}

nsresult nsCalendar::SetMethodProperty(nsICalProperty * property)
{
  // todo: assert property is of integer type
  UnicodeString s = *((UnicodeString *) property->GetValue());
  NSCalendar::METHOD i = NSCalendar::stringToMethod(s);
  mCalendar->setMethod(i);
  return NS_OK;
}

/*
nsICalProperty * nsCalendar::GetVersionProperty() const
{
  return mCalendar->getVersionProperty();
}
*/

UnicodeString nsCalendar::GetProdid() const
{
  return mCalendar->getProdid();
}

nsresult nsCalendar::SetProdid(UnicodeString s, JulianPtrArray * parameters)
{
  mCalendar->setProdid(s, parameters);
  return NS_OK;
}

/*
nsICalProperty * nsCalendar::GetProdidProperty() const
{
  return mCalendar->getProdidProperty();
}
*/

nsresult nsCalendar::SetMethod(PRInt32 i)
{
  mCalendar->setMethod((NSCalendar::METHOD) i);
  return NS_OK;
}
 
PRInt32 nsCalendar::GetMethod() const
{
  return (PRInt32) mCalendar->getMethod();
}

nsresult nsCalendar::AddEvent(nsICalVEvent * event) 
{
  if (mCalendar)
  {
    mCalendar->addEvent(event->GetICalEvent());
  }
  return NS_OK;
}

PRBool nsCalendar::StoreProperty(nsCalICalendarTag tag, nsICalProperty * prop,
                                 JulianPtrArray * vTimeZones)
{
  if (nsnull == mCalendar)
    return NS_OK; // todo: change to custom error.
  if (nsnull == prop)
    return NS_OK; // todo: change to custom error.

  switch(tag)
  {
    case eCalICalendarTag_method:   SetMethodProperty(prop);  break;
    case eCalICalendarTag_version:  SetVersionProperty(prop);  break;
    case eCalICalendarTag_calscale: SetCalScaleProperty(prop);  break;
    case eCalICalendarTag_prodid:   SetProdidProperty(prop);  break;
    default:
      // todo: handle errors && x-tokens
      break;
  }
  return NS_OK;
}

NSCalendar * nsCalendar::GetNSCalendar() const 
{
  return mCalendar;
}



























