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
#include "nsScreen.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"

static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMScreenIID, NS_IDOMSCREEN_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

//
//  Screen class implementation 
//
ScreenImpl::ScreenImpl()
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
}

ScreenImpl::~ScreenImpl()
{
}

NS_IMPL_ADDREF(ScreenImpl)
NS_IMPL_RELEASE(ScreenImpl)

nsresult 
ScreenImpl::QueryInterface(const nsIID& aIID,
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
  if (aIID.Equals(kIDOMScreenIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMScreen*)this);
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
ScreenImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP
ScreenImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();
    res = NS_NewScriptScreen(aContext, (nsIDOMScreen*)this, (nsIDOMWindow*)global, &mScriptObject);
    NS_IF_RELEASE(global);
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP
ScreenImpl::GetWidth(PRInt32* aWidth)
{
  //XXX not implmented
  *aWidth = -1;
  return NS_OK;
}

NS_IMETHODIMP
ScreenImpl::GetHeight(PRInt32* aHeight)
{
  //XXX not implmented
  *aHeight = -1;
  return NS_OK;
}

NS_IMETHODIMP
ScreenImpl::GetPixelDepth(PRInt32* aPixelDepth)
{
  //XXX not implmented
  *aPixelDepth = -1;
  return NS_OK;
}

NS_IMETHODIMP
ScreenImpl::GetColorDepth(PRInt32* aColorDepth)
{
  //XXX not implmented
  *aColorDepth = -1;
  return NS_OK;
}

NS_IMETHODIMP
ScreenImpl::GetAvailWidth(PRInt32* aAvailWidth)
{
  //XXX not implmented
  *aAvailWidth = -1;
  return NS_OK;
}

NS_IMETHODIMP
ScreenImpl::GetAvailHeight(PRInt32* aAvailHeight)
{
  //XXX not implmented
  *aAvailHeight = -1;
  return NS_OK;
}

NS_IMETHODIMP
ScreenImpl::GetAvailLeft(PRInt32* aAvailLeft)
{
  //XXX not implmented
  *aAvailLeft = -1;
  return NS_OK;
}

NS_IMETHODIMP
ScreenImpl::GetAvailTop(PRInt32* aAvailTop)
{
  //XXX not implmented
  *aAvailTop = -1;
  return NS_OK;
}


