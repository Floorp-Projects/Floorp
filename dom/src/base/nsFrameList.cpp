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

#include "nsFrameList.h"
#include "nsIWebShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"

static NS_DEFINE_IID(kIDOMWindowCollectionIID, NS_IDOMWINDOWCOLLECTION_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);
static NS_DEFINE_IID(kIDOMWindowIID, NS_IDOMWINDOW_IID);

nsFrameList::nsFrameList(nsIWebShell *aWebShell)
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  //Not refcnted.  Ref is nulled out be nsGlobalWindow when its
  //WebShell nulls its ref out.
  mWebShell = aWebShell;
}

nsFrameList::~nsFrameList()
{
}

nsresult nsFrameList::QueryInterface(REFNSIID aIID, void** aInstancePtr)
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

NS_IMPL_ADDREF(nsFrameList)
NS_IMPL_RELEASE(nsFrameList)

NS_IMETHODIMP
nsFrameList::SetWebShell(nsIWebShell* aWebShell)
{
  mWebShell = aWebShell;

  return NS_OK;
}

NS_IMETHODIMP 
nsFrameList::GetLength(PRUint32* aLength)
{
  PRInt32 mLength;
  nsresult ret;

  ret = mWebShell->GetChildCount(mLength);

  *aLength = mLength;
  return ret;
}

NS_IMETHODIMP 
nsFrameList::Item(PRUint32 aIndex, nsIDOMWindow** aReturn)
{
  nsCOMPtr<nsIWebShell> item;

  mWebShell->ChildAt(aIndex, *getter_AddRefs(item));

  nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(item));
  if (NS_WARN_IF_FALSE(globalObject, "Couldn't get to the globalObject")) {
    *aReturn = nsnull;
  }
  else {
    CallQueryInterface(globalObject.get(), aReturn);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsFrameList::NamedItem(const nsAReadableString& aName, nsIDOMWindow** aReturn)
{
  nsCOMPtr<nsIWebShell> item;

  mWebShell->FindChildWithName(aName.GetUnicode(), *getter_AddRefs(item));

  nsCOMPtr<nsIScriptGlobalObject> globalObject(do_GetInterface(item));
  if (NS_WARN_IF_FALSE(globalObject, "Couldn't get to the globalObject")) {
    *aReturn = nsnull;
  }
  else {
    CallQueryInterface(globalObject.get(), aReturn);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsFrameList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
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
nsFrameList::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}
