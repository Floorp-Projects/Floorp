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

#include "nscore.h"
#include "nsHistory.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"

static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMHistoryIID, NS_IDOMHISTORY_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

//
//  History class implementation 
//
HistoryImpl::HistoryImpl()
{
  NS_INIT_REFCNT();
  mWebShell = nsnull;
  mScriptObject = nsnull;
}

HistoryImpl::~HistoryImpl()
{
}

NS_IMPL_ADDREF(HistoryImpl)
NS_IMPL_RELEASE(HistoryImpl)

nsresult 
HistoryImpl::QueryInterface(const nsIID& aIID,
                              void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMHistoryIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMHistory*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
HistoryImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();
    res = NS_NewScriptHistory(aContext, (nsIDOMHistory*)this, (nsIDOMWindow*)global, &mScriptObject);
    NS_IF_RELEASE(global);
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP_(void)       
HistoryImpl::SetWebShell(nsIWebShell *aWebShell)
{
  //mWebShell isn't refcnt'd here.  GlobalWindow calls SetWebShell(nsnull) 
  //when it's told that the WebShell is going to be deleted.
  mWebShell = aWebShell;
}

NS_IMETHODIMP
HistoryImpl::GetLength(PRInt32* aLength)
{
  if (nsnull != mWebShell) {
    mWebShell->GetHistoryLength(*aLength);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
HistoryImpl::GetCurrent(nsString& aCurrent)
{
  PRInt32 curIndex;
  const PRUnichar* curURL = nsnull;

  if (nsnull != mWebShell && NS_OK == mWebShell->GetHistoryIndex(curIndex)) {
    mWebShell->GetURL(curIndex, &curURL);
  }
  aCurrent.SetString(curURL);

  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::GetPrevious(nsString& aPrevious)
{
  PRInt32 curIndex;
  const PRUnichar* prevURL = nsnull;

  if (nsnull != mWebShell && NS_OK == mWebShell->GetHistoryIndex(curIndex)) {
    mWebShell->GetURL(curIndex-1, &prevURL);
  }
  aPrevious.SetString(prevURL);

  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::GetNext(nsString& aNext)
{
  PRInt32 curIndex;
  const PRUnichar* nextURL = nsnull;

  if (nsnull != mWebShell && NS_OK == mWebShell->GetHistoryIndex(curIndex)) {
    mWebShell->GetURL(curIndex+1, &nextURL);
  }
  aNext.SetString(nextURL);

  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::Back()
{
  if (nsnull != mWebShell && NS_OK == mWebShell->CanBack()) {
    mWebShell->Back();
  }
  
  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::Forward()
{
  if (nsnull != mWebShell && NS_OK == mWebShell->CanForward()) {
    mWebShell->Forward();
  }

  return NS_OK;
}

NS_IMETHODIMP
HistoryImpl::Go(PRInt32 aIndex)
{
  if (nsnull != mWebShell) {
    mWebShell->GoTo(aIndex);
  }

  return NS_OK;
}



