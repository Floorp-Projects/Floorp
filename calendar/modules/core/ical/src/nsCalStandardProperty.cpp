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
#include "nsCalStandardProperty.h"
#include "nscalcoreicalCIID.h"
#include "sdprprty.h"
#include "icalprm.h"
#include "ptrarray.h"
#include "unistring.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kICalPropertyIID, NS_ICALPROPERTY_IID);
static NS_DEFINE_IID(kICalStandardPropertyIID, NS_ICALSTANDARDPROPERTY_IID);
static NS_DEFINE_IID(kICalICalendarParserObjectIID, NS_ICALICALENDAR_PARSER_OBJECT_IID);
nsCalStandardProperty::nsCalStandardProperty()
{
  //NS_INIT_REFCNT();
  //Init();
}

nsCalStandardProperty::~nsCalStandardProperty()
{
}

// implement ISupports functions (addref, release, query-interface)
NS_IMPL_ADDREF(nsCalStandardProperty)
NS_IMPL_RELEASE(nsCalStandardProperty)

nsresult nsCalStandardProperty::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsICalStandardProperty*)(this));
  }
  else if (aIID.Equals(kICalPropertyIID)) {
    *aInstancePtr = (void*)(this);
  }
  else if (aIID.Equals(kICalStandardPropertyIID)) {
    *aInstancePtr = (void*)(this);
  }
  else if (aIID.Equals(kICalICalendarParserObjectIID)) {
    *aInstancePtr = (void*)(this);
  }
  else  {
    *aInstancePtr = 0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;
}

nsresult nsCalStandardProperty::Init()
{
  return NS_OK;
}

nsresult nsCalStandardProperty::AddParameter(nsString & aKey, nsString & aValue) 
{
  ICalParameter * ipm = new ICalParameter((UnicodeString) aKey, (UnicodeString) aValue);
  return AddParameter(ipm);
}


nsresult nsCalStandardProperty::AddParameter(ICalParameter * ipm)
{
  if (mICalProperty)
    ((StandardProperty *) mICalProperty)->addParameter(ipm);
  return NS_OK;
}

nsresult nsCalStandardProperty::Parse(nsString & in)
{
  return NS_OK;
}

nsresult nsCalStandardProperty::SetParameters(JulianPtrArray * parameters)
{
  ((StandardProperty *) mICalProperty)->setParameters(parameters);
  return NS_OK;
}

JulianPtrArray * nsCalStandardProperty::GetParameters()
{
  return ((StandardProperty *) mICalProperty)->getParameters();
}

nsresult nsCalStandardProperty::SetParameter(nsString & aKey, nsString & aValue)
{  
  char * cKey = 0;
  char * cValue = 0;
  cKey = aKey.ToCString(cKey, aKey.Length());
  cValue = aValue.ToCString(cValue, aValue.Length());
  // don't do anything, just append to vector for now
  ICalParameter * ipm = new ICalParameter(cKey, cValue); 
  AddParameter(ipm);
  delete [] cKey;
  delete [] cValue;
  return NS_OK;
}




