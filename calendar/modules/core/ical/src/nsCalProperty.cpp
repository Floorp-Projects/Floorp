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

#include "jdefines.h"
#include "nsCalProperty.h"
#include "nscalcoreicalCIID.h"
#include "sdprprty.h"
#include "icalprm.h"
#include "ptrarray.h"
#include "unistring.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kICalPropertyIID, NS_ICALPROPERTY_IID);
static NS_DEFINE_IID(kICalICalendarParserObjectIID, NS_ICALICALENDAR_PARSER_OBJECT_IID);

nsCalProperty::nsCalProperty()
{
  //NS_INIT_REFCNT();
  //Init();
}

nsCalProperty::~nsCalProperty()
{
}

nsresult nsCalProperty::Init()
{
  return NS_OK;
}

// implement ISupports functions (addref, release, query-interface)
NS_IMPL_ADDREF(nsCalProperty)
NS_IMPL_RELEASE(nsCalProperty)

nsresult nsCalProperty::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsICalProperty*)(this));
  }
  else if (aIID.Equals(kICalPropertyIID)) {
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

ICalProperty * nsCalProperty::GetICalProperty() const
{
  return mICalProperty;
}





