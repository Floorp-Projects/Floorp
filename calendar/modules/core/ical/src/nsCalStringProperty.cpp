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

#include "nsCalStringProperty.h"
#include "nscalcoreicalCIID.h"
#include "jdefines.h"
#include "dprprty.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kICalICalendarParserObjectIID, NS_ICALICALENDAR_PARSER_OBJECT_IID);
static NS_DEFINE_IID(kICalPropertyIID, NS_ICALPROPERTY_IID);
static NS_DEFINE_IID(kICalStandardPropertyIID, NS_ICALSTANDARDPROPERTY_IID);
static NS_DEFINE_IID(kCCalStringPropertyCID, NS_CALSTRINGPROPERTY_CID);

nsCalStringProperty::nsCalStringProperty()
{
  NS_INIT_REFCNT();
  Init();
}

nsCalStringProperty::~nsCalStringProperty()
{
  if (mICalProperty) {
    delete ((StringProperty *) mICalProperty);
    mICalProperty = nsnull;
  }
}

// implement ISupports functions (addref, release, query-interface)
NS_IMPL_ADDREF(nsCalStringProperty)
NS_IMPL_RELEASE(nsCalStringProperty)

nsresult nsCalStringProperty::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kCCalStringPropertyCID);

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

nsresult nsCalStringProperty::Init()
{
  UnicodeString u;
  mICalProperty = (ICalProperty *) new StringProperty(u, 0);
  //mICalProperty = new nsStandardProperty();
  return NS_OK;
}

void * nsCalStringProperty::GetValue() const 
{
  return (void *) ((StringProperty *) mICalProperty)->getValue();
}

nsresult nsCalStringProperty::SetValue(void * value)
{
  ((StringProperty *) mICalProperty)->setValue(value);
  return NS_OK;
}

nsICalProperty * nsCalStringProperty::Clone()
{
  nsCalStringProperty * prop = nsnull;

  static NS_DEFINE_IID(kCalStringPropertyCID, NS_CALSTRINGPROPERTY_CID);

  nsresult res = nsRepository::CreateInstance(kCalStringPropertyCID, 
                                              nsnull, 
                                              kCalStringPropertyCID,
                                              (void **) &prop);

  if (NS_OK != res)
    return nsnull;
  
  prop->mICalProperty = (ICalProperty *) ((StringProperty *) mICalProperty)->clone(0);
 
  return prop;
}

PRBool nsCalStringProperty::IsValid()
{
  return (PRBool) ((StringProperty *) mICalProperty)->isValid();
}

nsString & nsCalStringProperty::ToString(nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  u = ((StringProperty *) mICalProperty)->toString(u);
  out = u.toCString("");
  */
  return out;
}

nsString & nsCalStringProperty::ToICALString(nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  u = ((StringProperty *) mICalProperty)->toICALString(u);
  out = u.toCString("");
  */
  return out;
}

nsString & nsCalStringProperty::ToICALString(nsString & sProp, nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  UnicodeString prop = sProp.toCString("");
  u = ((StringProperty *) mICalProperty)->toICALString(prop, u);
  out = u.toCString("");
  */
  return out;
}

nsString & nsCalStringProperty::toExportString(nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  u = ((StringProperty *) mICalProperty)->toExportString(u);
  out = u.toCString("");
  */
  return out;
}










