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

#include "nsCalDateTimeProperty.h"
#include "nscalcoreicalCIID.h"
#include "jdefines.h"
#include "dprprty.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kICalICalendarParserObjectIID, NS_ICALICALENDAR_PARSER_OBJECT_IID);
static NS_DEFINE_IID(kICalPropertyIID, NS_ICALPROPERTY_IID);
static NS_DEFINE_IID(kCCalDateTimePropertyCID, NS_CALDATETIMEPROPERTY_CID);

nsCalDateTimeProperty::nsCalDateTimeProperty()
{
  NS_INIT_REFCNT();
  Init();
}

nsCalDateTimeProperty::~nsCalDateTimeProperty()
{
  if (mICalProperty) {
    delete ((DateTimeProperty *) mICalProperty);
    mICalProperty = nsnull;
  }
}

// implement ISupports functions (addref, release, query-interface)
NS_IMPL_ADDREF(nsCalDateTimeProperty)
NS_IMPL_RELEASE(nsCalDateTimeProperty)

nsresult nsCalDateTimeProperty::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kCCalDateTimePropertyCID);

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
  return (nsCalStandardProperty::QueryInterface(aIID, aInstancePtr));
}


nsresult nsCalDateTimeProperty::Init()
{
  DateTime d;
  mICalProperty = new DateTimeProperty(d, 0);
  //mStandardProperty = new nsStandardProperty();
  return NS_OK;
}

void * nsCalDateTimeProperty::GetValue() const 
{
  return (void *) ((DateTimeProperty *) mICalProperty)->getValue();
}

nsresult nsCalDateTimeProperty::SetValue(void * value)
{
  ((DateTimeProperty *) mICalProperty)->setValue(value);
  return NS_OK;
}

nsICalProperty * nsCalDateTimeProperty::Clone()
{
  nsCalDateTimeProperty * prop = nsnull;

  static NS_DEFINE_IID(kCalDateTimePropertyCID, NS_CALDATETIMEPROPERTY_CID);

  nsresult res = nsRepository::CreateInstance(kCalDateTimePropertyCID, 
                                              nsnull, 
                                              kCalDateTimePropertyCID,
                                              (void **) &prop);

  if (NS_OK != res)
    return nsnull;
  
  prop->mICalProperty = (ICalProperty *) ((DateTimeProperty* )mICalProperty)->clone(0);
 
  return prop;
}

PRBool nsCalDateTimeProperty::IsValid()
{
  return (PRBool) ((DateTimeProperty *)mICalProperty)->isValid();
}

nsString & nsCalDateTimeProperty::ToString(nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  u = ((DateTimeProperty *)mICalProperty)->toString(u);
  out = u.toCString("");
  */
  return out;
}

nsString & nsCalDateTimeProperty::ToICALString(nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  u = ((DateTimeProperty *)mICalProperty)->toICALString(u);
  out = u.toCString("");
  */
  return out;
}

nsString & nsCalDateTimeProperty::ToICALString(nsString & sProp, nsString & out)
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

nsString & nsCalDateTimeProperty::toExportString(nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  u = ((DateTimeProperty *)mICalProperty)->toExportString(u);
  out = u.toCString("");
  */
  return out;
}












