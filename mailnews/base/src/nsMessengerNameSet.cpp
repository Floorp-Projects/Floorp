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

#include "jsapi.h"
#include "nsIScriptContext.h"
#include "nsIScriptNameSpaceManager.h"
#include "nsIScriptExternalNameSet.h"


/* hack the AppCore stuff here */
#include "nsIDOMMsgAppCore.h"
#include "nsMsgAppCore.h"

nsMessengerNameSet::nsMessengerNameSet():
    mRefCnt(0)
{
  NS_INIT_REFCNT();
}

nsMessengerNameSet::~nsMessengerNameSet()
{
  
}

NS_IMPL_ISUPPORTS(nsMessengerNameSet, nsIScriptExternalNameSet::GetIID());

NS_IMETHODIMP
nsMessengerNameSet::InitializeClasses(nsIScriptContext* aScriptContext)
{
  nsresult rv = NS_OK;

  /* initialize the AppCore */
  NS_InitMsgAppCoreClass(aScriptContext, nsnull);

  return rv;
}

static NS_DEFINE_CID(kCMsgAppCoreCID, NS_MSGAPPCORE_CID);

NS_IMETHODIMP
nsMessengerNameSet::AddNameSet(nsIScriptContext *aScriptContext)
{
  nsresult rv = NS_OK;
  nsIScriptNameSpaceManager *manager;

  rv = aScriptContext->GetNameSpaceManager(&manager);
  
  /* register the appcore here too */
  if (NS_SUCCEEDED(rv))
      rv = manager->RegisterGlobalName("MsgAppCore",
                                       kCMsgAppCoreCID,
                                       PR_TRUE);

  // don't release the manager because that (Supposedly) removes
  // the registered names from the namespace
  
  return rv;

}


