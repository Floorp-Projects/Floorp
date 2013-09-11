/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_checkpermissionshelper_h__
#define mozilla_dom_indexeddb_checkpermissionshelper_h__

// Only meant to be included in IndexedDB source files, not exported.
#include "OpenDatabaseHelper.h"

#include "nsIInterfaceRequestor.h"
#include "nsIObserver.h"
#include "nsIRunnable.h"

class nsIDOMWindow;
class nsIThread;

BEGIN_INDEXEDDB_NAMESPACE

class CheckPermissionsHelper MOZ_FINAL : public nsIRunnable,
                                         public nsIInterfaceRequestor,
                                         public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIOBSERVER

  CheckPermissionsHelper(OpenDatabaseHelper* aHelper,
                         nsIDOMWindow* aWindow)
  : mHelper(aHelper),
    mWindow(aWindow),
    // If we're trying to delete the database, we should never prompt the user.
    // Anything that would prompt is translated to denied.
    mPromptAllowed(!aHelper->mForDeletion),
    mHasPrompted(false),
    mPromptResult(0)
  {
    NS_ASSERTION(aHelper, "Null pointer!");
    NS_ASSERTION(aHelper->mPersistenceType == quota::PERSISTENCE_TYPE_PERSISTENT,
                 "Checking permission for non persistent databases?!");
  }

private:
  nsRefPtr<OpenDatabaseHelper> mHelper;
  nsCOMPtr<nsIDOMWindow> mWindow;
  bool mPromptAllowed;
  bool mHasPrompted;
  uint32_t mPromptResult;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_checkpermissionshelper_h__
