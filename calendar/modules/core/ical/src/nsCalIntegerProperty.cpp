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

#include "nsCalIntegerProperty.h"
#include "nsCalStandardProperty.h"
#include "nscalcoreicalCIID.h"
#include "jdefines.h"
#include "iprprty.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kICalICalendarParserObjectIID, NS_ICALICALENDAR_PARSER_OBJECT_IID);
static NS_DEFINE_IID(kICalPropertyIID, NS_ICALPROPERTY_IID);
static NS_DEFINE_IID(kCCalIntegerPropertyCID, NS_CALINTEGERPROPERTY_CID);

nsCalIntegerProperty::nsCalIntegerProperty()
{
  NS_INIT_REFCNT();
  Init();
}

nsCalIntegerProperty::~nsCalIntegerProperty()
{
  if (mICalProperty) {
    delete ((IntegerProperty *) mICalProperty);
    mICalProperty = nsnull;
  }
}

// implement ISupports functions (addref, release, query-interface)
NS_IMPL_ADDREF(nsCalIntegerProperty)
NS_IMPL_RELEASE(nsCalIntegerProperty)

nsresult nsCalIntegerProperty::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kClassIID, kCCalIntegerPropertyCID);

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

nsresult nsCalIntegerProperty::Init()
{
  t_int32 i;
  mICalProperty = (ICalProperty *) new IntegerProperty(i, 0);
  //mICalProperty = new nsStandardProperty();
  return NS_OK;
}

void * nsCalIntegerProperty::GetValue() const 
{
  return (void *) ((IntegerProperty *) mICalProperty)->getValue();
}

nsresult nsCalIntegerProperty::SetValue(void * value)
{
  ((IntegerProperty *) mICalProperty)->setValue(value);
  return NS_OK;
}

nsICalProperty * nsCalIntegerProperty::Clone()
{
  nsCalIntegerProperty * prop = nsnull;

  static NS_DEFINE_IID(kCalIntegerPropertyCID, NS_CALINTEGERPROPERTY_CID);

  nsresult res = nsRepository::CreateInstance(kCalIntegerPropertyCID, 
                                              nsnull, 
                                              kCalIntegerPropertyCID,
                                              (void **) &prop);

  if (NS_OK != res)
    return nsnull;
  
  prop->mICalProperty = (ICalProperty *) ((IntegerProperty *) mICalProperty)->clone(0);
 
  return prop;
}

PRBool nsCalIntegerProperty::IsValid()
{
  return (PRBool) ((IntegerProperty *) mICalProperty)->isValid();
}

nsString & nsCalIntegerProperty::ToString(nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  u = ((StringProperty *) mICalProperty)->toString(u);
  out = u.toCString("");
  */
  return out;
}

nsString & nsCalIntegerProperty::ToICALString(nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  u = ((StringProperty *) mICalProperty)->toICALString(u);
  out = u.toCString("");
  */
  return out;
}

nsString & nsCalIntegerProperty::ToICALString(nsString & sProp, nsString & out)
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

nsString & nsCalIntegerProperty::toExportString(nsString & out)
{
  /*
    todo: XXX: finish
  UnicodeString u = out.toCString("");
  u = ((StringProperty *) mICalProperty)->toExportString(u);
  out = u.toCString("");
  */
  return out;
}










