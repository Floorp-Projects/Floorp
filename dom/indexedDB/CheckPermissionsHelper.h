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
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIOBSERVER

  CheckPermissionsHelper(OpenDatabaseHelper* aHelper,
                         nsIDOMWindow* aWindow,
                         const nsACString& aASCIIOrigin,
                         bool aForDeletion)
  : mHelper(aHelper),
    mWindow(aWindow),
    mASCIIOrigin(aASCIIOrigin),
    // If we're trying to delete the database, we should never prompt the user.
    // Anything that would prompt is translated to denied.
    mPromptAllowed(!aForDeletion),
    mHasPrompted(false),
    mPromptResult(0)
  {
    NS_ASSERTION(aHelper, "Null pointer!");
    NS_ASSERTION(!aASCIIOrigin.IsEmpty(), "Empty origin!");
  }

private:
  nsRefPtr<OpenDatabaseHelper> mHelper;
  nsCOMPtr<nsIDOMWindow> mWindow;
  nsCString mASCIIOrigin;
  bool mPromptAllowed;
  bool mHasPrompted;
  PRUint32 mPromptResult;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_checkpermissionshelper_h__
