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

#include "nsJSSecurityManager.h"

static NS_DEFINE_IID(kIScriptSecurityManagerIID, NS_ISCRIPTSECURITYMANAGER_IID);

nsJSSecurityManager::nsJSSecurityManager()
{
  NS_INIT_REFCNT();
}

nsJSSecurityManager::~nsJSSecurityManager()
{
}

NS_IMPL_ISUPPORTS(nsJSSecurityManager, kIScriptSecurityManagerIID)

NS_IMETHODIMP
nsJSSecurityManager::CheckScriptAccess(nsIScriptContext* aContext, PRBool* aResult)
{
  *aResult = PR_TRUE;
  return NS_OK;
}

extern "C" NS_DOM nsresult NS_NewScriptSecurityManager(nsIScriptSecurityManager ** aInstancePtrResult)
{
  nsIScriptSecurityManager* it = new nsJSSecurityManager();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIScriptSecurityManagerIID, (void **) aInstancePtrResult);
}

