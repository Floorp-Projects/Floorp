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

#include "nsISupports.h"
#include "nsIEventStateManager.h"
#include "nsEventStateManager.h"

static NS_DEFINE_IID(kIEventStateManagerIID, NS_IEVENTSTATEMANAGER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsEventStateManager::nsEventStateManager() {
  mEventTarget = nsnull;
}

nsEventStateManager::~nsEventStateManager() {
}

NS_IMPL_ADDREF(nsEventStateManager)
NS_IMPL_RELEASE(nsEventStateManager)
NS_IMPL_QUERY_INTERFACE(nsEventStateManager, kIEventStateManagerIID);

NS_METHOD nsEventStateManager::GetEventTarget(nsISupports **aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mEventTarget;
  NS_IF_ADDREF(mEventTarget);
  return NS_OK;
}

NS_METHOD nsEventStateManager::SetEventTarget(nsISupports *aSupports)
{
  mEventTarget = aSupports;
  return NS_OK;
}

nsresult NS_NewEventStateManager(nsIEventStateManager** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIEventStateManager* manager = new nsEventStateManager();
  if (nsnull == manager) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return manager->QueryInterface(kIEventStateManagerIID, (void **) aInstancePtrResult);
}

