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



#include "nsComposerNameSet.h"

#include "nsIComposer.h"

#include "jsapi.h"
#include "nsIScriptContext.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"
#include "nsComposer.h"


/* hack the AppCore stuff here */
#include "nsIDOMComposeAppCore.h"
#include "nsComposeAppCore.h"

static NS_DEFINE_IID(kIScriptExternalNameSetIID, NS_ISCRIPTEXTERNALNAMESET_IID);





nsComposerNameSet::nsComposerNameSet()
{
  NS_INIT_REFCNT();
}

nsComposerNameSet::~nsComposerNameSet()
{
  
}

NS_IMPL_ISUPPORTS(nsComposerNameSet, nsIScriptExternalNameSet::GetIID());

NS_IMETHODIMP
nsComposerNameSet::InitializeClasses(nsIScriptContext* aScriptContext)
{
  nsresult rv = NS_OK;
  JSContext *cx = (JSContext*)aScriptContext->GetNativeContext();
  
  /* initialize the AppCore */
  NS_InitComposeAppCoreClass(aScriptContext, nsnull);

#ifdef XPIDL_JS_STUBS
  nsIComposer::InitJSClass(cx);
#endif

  return rv;
}

static NS_DEFINE_CID(kCComposerCID, NS_COMPOSER_CID);
static NS_DEFINE_CID(kCComposeAppCoreCID, NS_COMPOSEAPPCORE_CID);

NS_IMETHODIMP
nsComposerNameSet::AddNameSet(nsIScriptContext *aScriptContext)
{
  nsresult rv = NS_OK;
  nsIScriptNameSpaceManager *manager;

  rv = aScriptContext->GetNameSpaceManager(&manager);
  
  if (NS_SUCCEEDED(rv))
    rv = manager->RegisterGlobalName("Composer",
                                     kCComposerCID,
                                     PR_TRUE);

  /* register the appcore here too */
  if (NS_SUCCEEDED(rv))
      rv = manager->RegisterGlobalName("ComposeAppCore",
                                       kCComposeAppCoreCID,
                                       PR_TRUE);
                                       
  return rv;

}


