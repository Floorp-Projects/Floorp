/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsIDOMMsgAppCore.h"
#include "nsMsgAppCore.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMBaseAppCore.h"
#include "nsJSMsgAppCore.h"

/* rhp - for access to webshell */
#include "nsCOMPtr.h"
#include "nsIDOMWindow.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIScriptGlobalObject.h"

static NS_DEFINE_IID(kIMsgAppCoreIID, NS_IDOMMSGAPPCORE_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMBaseAppCoreIID, NS_IDOMBASEAPPCORE_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

class nsMsgAppCore : public nsIDOMMsgAppCore,
                     public nsIScriptObjectOwner
{
  
public:
  nsMsgAppCore();
  virtual ~nsMsgAppCore();

  NS_DECL_ISUPPORTS
  NS_DECL_IDOMBASEAPPCORE

  // nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  // nsIMsgAppCore
  NS_IMETHOD GetNewMail();
  NS_IMETHOD SetWindow(nsIDOMWindow* aWin);


private:
  
  nsString mId;
  void *mScriptObject;

  /* rhp - need this to drive message display */
  nsIDOMWindow       *mWindow;
  nsIWebShell        *mWebShell;
};

//
// nsMsgAppCore
//
nsMsgAppCore::nsMsgAppCore()
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  mWebShell = nsnull; 
}

nsMsgAppCore::~nsMsgAppCore()
{

}

//
// nsISupports
//
NS_IMPL_ADDREF(nsMsgAppCore);
NS_IMPL_RELEASE(nsMsgAppCore);

NS_IMETHODIMP
nsMsgAppCore::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
      return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIScriptObjectOwnerIID)) {
      *aInstancePtr = (void*) ((nsIScriptObjectOwner*)this);
      AddRef();
      return NS_OK;
  }
  if ( aIID.Equals(kIDOMBaseAppCoreIID)) {
      *aInstancePtr = (void*) ((nsIDOMBaseAppCore*)this);
      AddRef();
      return NS_OK;
  }
  else if ( aIID.Equals(kIMsgAppCoreIID) ) {
      *aInstancePtr = (void*)(nsISupports*)(nsIScriptObjectOwner*)this;
      AddRef();
      return NS_OK;
  }
  else if ( aIID.Equals(kISupportsIID) ) {
      *aInstancePtr = (void*)(nsISupports*)(nsIScriptObjectOwner*)this;
      AddRef();
      return NS_OK;
  }

  return NS_NOINTERFACE;
}

//
// nsIScriptObjectOwner
//
nsresult
nsMsgAppCore::GetScriptObject(nsIScriptContext *aContext, void **aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) 
  {
      res = NS_NewScriptMsgAppCore(aContext, 
                                   (nsISupports *)(nsIDOMMsgAppCore*)this, 
                                   nsnull, 
                                   &mScriptObject);
  }

  *aScriptObject = mScriptObject;
  return res;
}

nsresult
nsMsgAppCore::SetScriptObject(void* aScriptObject)
{
  
  return NS_OK;
} 

//
// nsIDOMBaseAppCore
//
nsresult
nsMsgAppCore::Init(const nsString& aId)
{
  mId = aId;
  return NS_OK;
}


nsresult
nsMsgAppCore::GetId(nsString& aId)
{
  aId = mId;
  return NS_OK;
}

//
// nsIMsgAppCore
//
nsresult
nsMsgAppCore::GetNewMail()
{
  printf("nsMsgAppCore::GetNewMail()\n");
  return NS_OK;
}
                              
extern "C"
nsresult
NS_NewMsgAppCore(nsIDOMMsgAppCore **aResult)
{
  if (!aResult) return NS_ERROR_NULL_POINTER;

  nsMsgAppCore *appcore = new nsMsgAppCore();
  if (appcore) {
    return appcore->QueryInterface(kIMsgAppCoreIID,
                                   (void **)aResult);

  }
  return NS_ERROR_NOT_INITIALIZED;
}


NS_IMETHODIMP    
nsMsgAppCore::SetWindow(nsIDOMWindow* aWin)
{
  mWindow = aWin;
  NS_ADDREF(aWin);

  /* rhp - Needed to access the webshell to drive message display */
  printf("nsMsgAppCore::SetWindow(): Getting the webShell of interest...\n");

  nsCOMPtr<nsIScriptGlobalObject> globalObj( aWin );
  if (!globalObj) 
  {
    return NS_ERROR_FAILURE;
  }

  nsIWebShell *webShell;
  globalObj->GetWebShell(&webShell);
  if (nsnull != webShell) 
  {
    nsIWebShellContainer *webShellContainer;
    webShell->GetContainer(webShellContainer);
    if (nsnull != webShellContainer) 
    {
      /* NOTE: Need to know what the name of the frame for the message window is
               named */
      webShellContainer->FindWebShellWithName(nsAutoString("browser.messagewindow"), 
                                              mWebShell);
      NS_RELEASE(webShellContainer);
      printf("nsMsgAppCore::SetWindow(): Got the webShell of interest...\n");
    }

    NS_RELEASE(webShell);
  }

	return NS_OK;
}
