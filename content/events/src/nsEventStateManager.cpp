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
#include "nsIContent.h"
#include "nsIDocument.h"

static NS_DEFINE_IID(kIEventStateManagerIID, NS_IEVENTSTATEMANAGER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsEventStateManager::nsEventStateManager() {
  mEventTarget = nsnull;
  mLastMouseOverContent = nsnull;
  mActiveLink = nsnull;
  NS_INIT_REFCNT();
}

nsEventStateManager::~nsEventStateManager() {
  NS_IF_RELEASE(mEventTarget);
  NS_IF_RELEASE(mLastMouseOverContent);
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
  NS_IF_RELEASE(mEventTarget);
  
  mEventTarget = aSupports;
  NS_ADDREF(mEventTarget);
  return NS_OK;
}

NS_METHOD nsEventStateManager::GetLastMouseOverContent(nsIContent **aContent)
{
  NS_PRECONDITION(nsnull != aContent, "null ptr");
  if (nsnull == aContent) {
    return NS_ERROR_NULL_POINTER;
  }
  *aContent = mLastMouseOverContent;
  NS_IF_ADDREF(mLastMouseOverContent);
  return NS_OK;
}

NS_METHOD nsEventStateManager::SetLastMouseOverContent(nsIContent *aContent)
{
  NS_IF_RELEASE(mLastMouseOverContent);
  
  mLastMouseOverContent = aContent;
  NS_IF_ADDREF(mLastMouseOverContent);
  return NS_OK;
}

NS_METHOD nsEventStateManager::GetActiveLink(nsIContent **aLink)
{
  NS_PRECONDITION(nsnull != aLink, "null ptr");
  if (nsnull == aLink) {
    return NS_ERROR_NULL_POINTER;
  }
  *aLink = mActiveLink;
  NS_IF_ADDREF(mActiveLink);
  return NS_OK;
}

NS_METHOD nsEventStateManager::SetActiveLink(nsIContent *aLink)
{
  nsIDocument *mDocument;

  //XXX this should just be able to call ContentChanged for the link once
  //either nsFrame::ContentChanged does something or we have a separate
  //link class
  if (nsnull != mActiveLink) {
    if (NS_OK == mActiveLink->GetDocument(mDocument)) {
      nsIContent *mKid;
      for (int i = 0; i < mActiveLink->ChildCount(); i++) {
        mKid = mActiveLink->ChildAt(i);
        mDocument->ContentChanged(mKid, nsnull);
        NS_RELEASE(mKid);
      }
    }
    NS_RELEASE(mDocument);
  }

  NS_IF_RELEASE(mActiveLink);

  mActiveLink = aLink;
  NS_IF_ADDREF(mActiveLink);

  if (nsnull != mActiveLink) {
    if (NS_OK == mActiveLink->GetDocument(mDocument)) {
      nsIContent *mKid;
      for (int i = 0; i < mActiveLink->ChildCount(); i++) {
        mKid = mActiveLink->ChildAt(i);
        mDocument->ContentChanged(mKid, nsnull);
        NS_RELEASE(mKid);
      }
    }
    NS_RELEASE(mDocument);
  }

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

