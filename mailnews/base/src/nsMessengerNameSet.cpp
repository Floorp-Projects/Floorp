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



#include "nsMessengerNameSet.h"

#include "nsIMessenger.h"

#include "jsapi.h"
#include "nsIScriptContext.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"

static NS_DEFINE_IID(kIScriptExternalNameSetIID, NS_ISCRIPTEXTERNALNAMESET_IID);





nsMessengerNameSet::nsMessengerNameSet()
{
  NS_INIT_REFCNT();
}

nsMessengerNameSet::~nsMessengerNameSet()
{
  
}

NS_IMPL_ISUPPORTS(nsMessengerNameSet, nsIScriptExternalNameSet::IID());

NS_IMETHODIMP
nsMessengerNameSet::InitializeClasses(nsIScriptContext* aScriptContext)
{
  nsresult rv = NS_OK;
  JSContext *cx = (JSContext*)aScriptContext->GetNativeContext();
  
  printf("nsMessengerNameSet::InitializeClasses() Initializing base classes\n");
#ifdef XPIDL_JS_STUBS
  nsIMessenger::InitJSClass(cx);
#endif

  return rv;
}


NS_IMETHODIMP
nsMessengerNameSet::AddNameSet(nsIScriptContext *aScriptContext)
{
  nsresult rv = NS_OK;
  nsIScriptNameSpaceManager *manager;

  printf("nsMessengerNameSet::AddNameSet() Registering Messenger in the JS namespace\n");
  rv = aScriptContext->GetNameSpaceManager(&manager);
  if (NS_SUCCEEDED(rv))
    rv = manager->RegisterGlobalName("Messenger",
                                     // put the CID here, not IID
                                     nsIMessenger::IID(),
                                     PR_TRUE);

  return rv;

}


