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

#include "nsEditProperty.h"

static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIEditPropertyIID, NS_IEDITPROPERTY_IID);


NS_IMPL_ADDREF(nsEditProperty)

NS_IMPL_RELEASE(nsEditProperty)

nsIAtom * nsIEditProperty::bold = NS_NewAtom("BOLD");
nsIAtom * nsIEditProperty::italic = NS_NewAtom("ITALIC");

NS_IMETHODIMP
nsEditProperty::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIEditPropertyIID)) {
    *aInstancePtr = (void*)(nsIEditProperty*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

nsresult NS_NewEditProperty(nsIEditProperty **aResult)
{
  if (aResult)
  {
    *aResult = new nsEditProperty();
    if (!*aResult) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(*aResult);
    return NS_OK;
  }
  return NS_ERROR_NULL_POINTER;
}