/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsGenericDOMNodeList.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsGenericElement.h"

nsGenericDOMNodeList::nsGenericDOMNodeList() 
{
  mRefCnt = 0;
  mScriptObject = nsnull;
}

nsGenericDOMNodeList::~nsGenericDOMNodeList()
{
}

nsresult 
nsGenericDOMNodeList::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
  static NS_DEFINE_IID(kIDOMNodeListIID, NS_IDOMNODELIST_IID);
  static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
  if (aIID.Equals(kIDOMNodeListIID)) {
    *aInstancePtr = (void*)(nsIDOMNodeList*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtr = (void*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMNodeList*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsGenericDOMNodeList)
NS_IMPL_RELEASE(nsGenericDOMNodeList)

NS_IMETHODIMP
nsGenericDOMNodeList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIDOMScriptObjectFactory *factory;
    
    res = nsGenericElement::GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }

    res = factory->NewScriptNodeList(aContext, (nsISupports *)(nsIDOMNodeList *)this, nsnull, (void**)&mScriptObject);
    NS_RELEASE(factory);
  }
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP
nsGenericDOMNodeList::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}
