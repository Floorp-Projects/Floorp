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
#include "nsCalICalendarComponent.h"
#include "nsICalICalendarComponent.h"
#include "nscalcoreicalCIID.h"
#include "icalcomp.h"
#include "nsICalICalendarContainer.h"
#include "nsICalICalendarParserObject.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kICalICalendarParserObjectIID, NS_ICALICALENDAR_PARSER_OBJECT_IID);
static NS_DEFINE_IID(kICalICalendarContainerIID, NS_ICALICALENDARCONTAINER_IID);
static NS_DEFINE_IID(kICalICalendarComponentIID, NS_ICALICALENDARCOMPONENT_IID);

nsCalICalendarComponent::nsCalICalendarComponent()
{
  NS_INIT_REFCNT();
}

nsCalICalendarComponent::~nsCalICalendarComponent()
{
}

NS_IMPL_ADDREF(nsCalICalendarComponent)
NS_IMPL_RELEASE(nsCalICalendarComponent)

nsresult nsCalICalendarComponent::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) ((nsICalICalendarComponent*)(this));
  }
  else if (aIID.Equals(kICalICalendarParserObjectIID)) {
    *aInstancePtr = (void*)(this);
  }
  else if (aIID.Equals(kICalICalendarContainerIID)) {
    *aInstancePtr = (void*) ((nsICalICalendarContainer*)(this));
  }
  else if (aIID.Equals(kICalICalendarComponentIID)) {
    *aInstancePtr = (void*)(this);
  }
  else  {
    *aInstancePtr = 0;
    return NS_NOINTERFACE;
  }
  ((nsISupports*) *aInstancePtr)->AddRef();
  return NS_OK;
}

nsresult nsCalICalendarComponent::Init()
{
  return NS_OK;
}

PRBool nsCalICalendarComponent::StoreProperty(nsCalICalendarTag tag, nsICalProperty * prop,
                                      JulianPtrArray * vTimeZones)
{
  // handle x-tokens??
  return PR_TRUE;
}

nsresult nsCalICalendarComponent::SetParameter(nsString & aKey, nsString & aValue)
{
  return NS_OK;
}

