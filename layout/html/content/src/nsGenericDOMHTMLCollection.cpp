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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nslayout.h"
#include "nsGenericDOMHTMLCollection.h"


nsGenericDOMHTMLCollection::nsGenericDOMHTMLCollection() 
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
}

nsGenericDOMHTMLCollection::~nsGenericDOMHTMLCollection()
{
}

nsresult 
nsGenericDOMHTMLCollection::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMHTMLCollection))) {
    *aInstancePtr = (void*)(nsIDOMHTMLCollection*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIScriptObjectOwner))) {
    *aInstancePtr = (void*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMHTMLCollection*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsGenericDOMHTMLCollection)
NS_IMPL_RELEASE(nsGenericDOMHTMLCollection)

NS_IMETHODIMP
nsGenericDOMHTMLCollection::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptHTMLCollection(aContext, (nsISupports *)(nsIDOMHTMLCollection *)this, nsnull, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP
nsGenericDOMHTMLCollection::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}
