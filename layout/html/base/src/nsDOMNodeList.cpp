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

#include "nsDOMNodeList.h"
#include "nsIDOMNode.h"

static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);

nsDOMNodeList::nsDOMNodeList(nsIContent &aContent) : mContent(aContent)
{
  mRefCnt = 1;

  // keep the content alive so the array of children
  // does not go away without "this" to know
  mContent.AddRef();

  mScriptObject = nsnull;
}

nsDOMNodeList::~nsDOMNodeList()
{
  mContent.Release();
}

nsresult nsDOMNodeList::QueryInterface(REFNSIID aIID, void** aInstancePtr)
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

NS_IMPL_ADDREF(nsDOMNodeList)

NS_IMPL_RELEASE(nsDOMNodeList)


nsresult nsDOMNodeList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    res = NS_NewScriptNodeList(aContext, this, nsnull, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;
  return res;
}

nsresult nsDOMNodeList::ResetScriptObject()
{
  mScriptObject = nsnull;
  return NS_OK;
}

// nsIDOMNodeList interface
NS_IMETHODIMP    
nsDOMNodeList::GetLength(PRUint32* aLength)
{
  *aLength = mContent.ChildCount();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMNodeList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsIContent *content = nsnull;
  nsresult res = NS_OK;
  content = mContent.ChildAt(aIndex);
  if (nsnull != content) {
    res = content->QueryInterface(kIDOMNodeIID, (void**)aReturn);
    NS_RELEASE(content);
  }
  else {
    *aReturn = nsnull;
  }

  return res;
}



