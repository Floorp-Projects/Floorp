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

#include "jdefines.h"
#include "nsCalAttendeeProperty.h"
#include "nscalcoreicalCIID.h"
#include "icalprm.h"
#include "ptrarray.h"
#include "unistring.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kICalPropertyIID, NS_ICALPROPERTY_IID);
static NS_DEFINE_IID(kCCalAttendeePropertyIID, NS_CALATTENDEEPROPERTY_CID);

nsCalAttendeeProperty::nsCalAttendeeProperty()
{
  NS_INIT_REFCNT();
  Init();
}

nsCalAttendeeProperty::~nsCalAttendeeProperty()
{
  if (mICalProperty) {
    delete ((Attendee *) mICalProperty);
    mICalProperty = nsnull;
  }
}

NS_IMPL_ADDREF(nsCalAttendeeProperty)
NS_IMPL_RELEASE(nsCalAttendeeProperty)

nsresult nsCalAttendeeProperty::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, NS_CALATTENDEEPROPERTY_CID);

  if (aIID.Equals(kClassIID)) {
    *aInstancePtr = (void*) ((nsICalProperty*)(this));
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) (this);
    AddRef();
    return NS_OK;
  }
  return (nsCalProperty::QueryInterface(aIID, aInstancePtr));
}

nsresult nsCalAttendeeProperty::Init()
{
  DateTime d;
  // todo: make it work for other things beside VEVENT. for now!
  mICalProperty = new Attendee(ICalComponent::ICAL_COMPONENT_VEVENT, 0);
  return NS_OK;
}

void * nsCalAttendeeProperty::GetValue() const
{
  return (void *) &(((Attendee *) mICalProperty)->getName());
}

nsresult nsCalAttendeeProperty::SetValue(void * value)
{
  UnicodeString u = *((UnicodeString *) value);
  ((Attendee *) mICalProperty)->setName(u);
  return NS_OK;
}

nsresult nsCalAttendeeProperty::SetParameters(JulianPtrArray * parameters)
{
  ((Attendee *) mICalProperty)->setParameters(parameters);
  return NS_OK;
}

JulianPtrArray * nsCalAttendeeProperty::GetParameters()
{
  // todo: XXX: fix.
  return 0;
}

nsICalProperty * nsCalAttendeeProperty::Clone()
{
  nsCalAttendeeProperty * prop = nsnull;

  static NS_DEFINE_IID(kCalAttendeePropertyCID, NS_CALATTENDEEPROPERTY_CID);

  nsresult res = nsRepository::CreateInstance(kCalAttendeePropertyCID, 
                                              nsnull, 
                                              kCalAttendeePropertyCID,
                                              (void **) &prop);

  if (NS_OK != res)
    return nsnull;
  
  prop->mICalProperty = (ICalProperty *) ((Attendee* )mICalProperty)->clone(0);
 
  return prop;
}

PRBool nsCalAttendeeProperty::IsValid()
{
  return (PRBool) ((Attendee *)mICalProperty)->isValid();
}

nsString & nsCalAttendeeProperty::ToString(nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  u = ((DateTimeProperty *)mICalProperty)->toString(u);
  out = u.toCString("");
  */
  return out;
}

nsString & nsCalAttendeeProperty::ToICALString(nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  u = ((DateTimeProperty *)mICalProperty)->toICALString(u);
  out = u.toCString("");
  */
  return out;
}

nsString & nsCalAttendeeProperty::ToICALString(nsString & sProp, nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  UnicodeString prop = sProp.toCString("");
  u = ((DateTimeProperty *)mICalProperty)->toICALString(prop, u);
  out = u.toCString("");
  */
  return out;
}

nsresult nsCalAttendeeProperty::SetParameter(nsString & aKey, nsString & aValue)
{  
  // todo: XXX: fix.
  return NS_OK;
}


