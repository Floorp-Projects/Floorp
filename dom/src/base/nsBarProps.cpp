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
#include "nsBarProps.h"
#include "nsIBrowserWindow.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"

static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMBarPropIID, NS_IDOMBARPROP_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

//
//  Basic (virtual) BarProp class implementation
//
BarPropImpl::BarPropImpl() {
  NS_INIT_REFCNT();
  mBrowser = nsnull;
  mScriptObject = nsnull;
}

BarPropImpl::~BarPropImpl() {
}

NS_IMPL_ADDREF(BarPropImpl)
NS_IMPL_RELEASE(BarPropImpl)

nsresult 
BarPropImpl::QueryInterface(const nsIID& aIID,
                              void** aInstancePtrResult) {

  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMBarPropIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMBarProp*)this);
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
BarPropImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject) {
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();
    res = NS_NewScriptBarProp(aContext, (nsIDOMBarProp *) this, (nsIDOMWindow *) global, &mScriptObject);
    NS_IF_RELEASE(global);
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP
BarPropImpl::SetScriptObject(void *aScriptObject) {
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP_(void)       
BarPropImpl::SetBrowserWindow(nsIBrowserWindow *aBrowser) {
  mBrowser = aBrowser;
}

NS_IMETHODIMP
BarPropImpl::GetVisibleByFlag(PRBool *aVisible, PRUint32 aChromeFlag) {
  PRUint32 chromeFlags;
  *aVisible = PR_FALSE;
  if (mBrowser && NS_SUCCEEDED(mBrowser->GetChrome(chromeFlags))) {
    if (chromeFlags & aChromeFlag)
      *aVisible = PR_TRUE;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
BarPropImpl::SetVisibleByFlag(PRBool aVisible, PRUint32 aChromeFlag) {
  PRUint32 chromeFlags;
  if (mBrowser && NS_SUCCEEDED(mBrowser->GetChrome(chromeFlags))) {
    if (aVisible)
      chromeFlags |= aChromeFlag;
    else
      chromeFlags &= ~aChromeFlag;
    return mBrowser->SetChrome(chromeFlags);
  }
  return NS_ERROR_FAILURE;
}

//
// MenubarProp class implementation
//

MenubarPropImpl::MenubarPropImpl() {
}

MenubarPropImpl::~MenubarPropImpl() {
}

NS_IMETHODIMP
MenubarPropImpl::GetVisible(PRBool *aVisible) {
  return BarPropImpl::GetVisibleByFlag(aVisible, NS_CHROME_MENU_BAR_ON);
}

NS_IMETHODIMP
MenubarPropImpl::SetVisible(PRBool aVisible) {
  return BarPropImpl::SetVisibleByFlag(aVisible, NS_CHROME_MENU_BAR_ON);
}

//
// ToolbarProp class implementation
//

ToolbarPropImpl::ToolbarPropImpl() {
}

ToolbarPropImpl::~ToolbarPropImpl() {
}

NS_IMETHODIMP
ToolbarPropImpl::GetVisible(PRBool *aVisible) {
  return BarPropImpl::GetVisibleByFlag(aVisible, NS_CHROME_TOOL_BAR_ON);
}

NS_IMETHODIMP
ToolbarPropImpl::SetVisible(PRBool aVisible) {
  return BarPropImpl::SetVisibleByFlag(aVisible, NS_CHROME_TOOL_BAR_ON);
}

//
// LocationbarProp class implementation
//

LocationbarPropImpl::LocationbarPropImpl() {
}

LocationbarPropImpl::~LocationbarPropImpl() {
}

NS_IMETHODIMP
LocationbarPropImpl::GetVisible(PRBool *aVisible) {
  return BarPropImpl::GetVisibleByFlag(aVisible, NS_CHROME_LOCATION_BAR_ON);
}

NS_IMETHODIMP
LocationbarPropImpl::SetVisible(PRBool aVisible) {
  return BarPropImpl::SetVisibleByFlag(aVisible, NS_CHROME_LOCATION_BAR_ON);
}

//
// PersonalbarProp class implementation
//

PersonalbarPropImpl::PersonalbarPropImpl() {
}

PersonalbarPropImpl::~PersonalbarPropImpl() {
}

NS_IMETHODIMP
PersonalbarPropImpl::GetVisible(PRBool *aVisible) {
  return BarPropImpl::GetVisibleByFlag(aVisible, NS_CHROME_PERSONAL_TOOLBAR_ON);
}

NS_IMETHODIMP
PersonalbarPropImpl::SetVisible(PRBool aVisible) {
  return BarPropImpl::SetVisibleByFlag(aVisible, NS_CHROME_PERSONAL_TOOLBAR_ON);
}

//
// StatusbarProp class implementation
//

StatusbarPropImpl::StatusbarPropImpl() {
}

StatusbarPropImpl::~StatusbarPropImpl() {
}

NS_IMETHODIMP
StatusbarPropImpl::GetVisible(PRBool *aVisible) {
  return BarPropImpl::GetVisibleByFlag(aVisible, NS_CHROME_STATUS_BAR_ON);
}

NS_IMETHODIMP
StatusbarPropImpl::SetVisible(PRBool aVisible) {
  return BarPropImpl::SetVisibleByFlag(aVisible, NS_CHROME_STATUS_BAR_ON);
}

//
// ScrollbarsProp class implementation
//

ScrollbarsPropImpl::ScrollbarsPropImpl() {
}

ScrollbarsPropImpl::~ScrollbarsPropImpl() {
}

NS_IMETHODIMP
ScrollbarsPropImpl::GetVisible(PRBool *aVisible) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
ScrollbarsPropImpl::SetVisible(PRBool aVisible) {
  return NS_ERROR_FAILURE;
}
