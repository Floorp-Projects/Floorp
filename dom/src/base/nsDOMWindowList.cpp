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

#include "nsDOMWindowList.h"
#include "nsIWebShell.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"

static NS_DEFINE_IID(kIDOMWindowCollectionIID, NS_IDOMWINDOWCOLLECTION_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);
static NS_DEFINE_IID(kIDOMWindowIID, NS_IDOMWINDOW_IID);

nsDOMWindowList::nsDOMWindowList(nsIWebShell *aWebShell)
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  //Not refcnted.  Ref is nulled out be nsGlobalWindow when its
  //WebShell nulls its ref out.
  mWebShell = aWebShell;
}

nsDOMWindowList::~nsDOMWindowList()
{
}

nsresult nsDOMWindowList::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMWindowCollectionIID)) {
    *aInstancePtr = (void*)(nsIDOMWindowCollection*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtr = (void*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMWindowCollection*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsDOMWindowList)
NS_IMPL_RELEASE(nsDOMWindowList)

NS_IMETHODIMP
nsDOMWindowList::SetWebShell(nsIWebShell* aWebShell)
{
  mWebShell = aWebShell;

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMWindowList::GetLength(PRUint32* aLength)
{
  PRInt32 mLength;
  nsresult ret;

  ret = mWebShell->GetChildCount(mLength);

  *aLength = mLength;
  return ret;
}

NS_IMETHODIMP 
nsDOMWindowList::Item(PRUint32 aIndex, nsIDOMWindow** aReturn)
{
  nsIWebShell *mItem;
  nsresult ret;

  mWebShell->ChildAt(aIndex, mItem);

  if (nsnull != mItem) {
    nsIScriptContextOwner *mItemContextOwner;
    if (NS_OK == mItem->QueryInterface(kIScriptContextOwnerIID, (void**)&mItemContextOwner)) {
      nsIScriptGlobalObject *mItemGlobalObject;
      if (NS_OK == mItemContextOwner->GetScriptGlobalObject(&mItemGlobalObject)) {
        ret = mItemGlobalObject->QueryInterface(kIDOMWindowIID, (void**)aReturn);
        NS_RELEASE(mItemGlobalObject);
      }
      NS_RELEASE(mItemContextOwner);
    }
    NS_RELEASE(mItem);
  }
  else {
    *aReturn = nsnull;
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsDOMWindowList::NamedItem(const nsString& aName, nsIDOMWindow** aReturn)
{
  nsIWebShell *mItem;
  nsresult ret;
  
  mWebShell->FindChildWithName(aName.GetUnicode(), mItem);

  if (nsnull != mItem) {
    nsIScriptContextOwner *mItemContextOwner;
    if (NS_OK == mItem->QueryInterface(kIScriptContextOwnerIID, (void**)&mItemContextOwner)) {
      nsIScriptGlobalObject *mItemGlobalObject;
      if (NS_OK == mItemContextOwner->GetScriptGlobalObject(&mItemGlobalObject)) {
        ret = mItemGlobalObject->QueryInterface(kIDOMWindowIID, (void**)aReturn);
        NS_RELEASE(mItemGlobalObject);
      }
      NS_RELEASE(mItemContextOwner);
    }
    NS_RELEASE(mItem);
  }
  else {
    *aReturn = nsnull;
  }
  
  return NS_OK;
}

NS_IMETHODIMP 
nsDOMWindowList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  nsIScriptGlobalObject *global = aContext->GetGlobalObject();

  if (nsnull == mScriptObject) {
    res = NS_NewScriptWindowCollection(aContext, (nsISupports *)(nsIDOMWindowCollection *)this, global, (void**)&mScriptObject);
  }
  *aScriptObject = mScriptObject;

  NS_RELEASE(global);
  return res;
}

NS_IMETHODIMP 
nsDOMWindowList::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}
