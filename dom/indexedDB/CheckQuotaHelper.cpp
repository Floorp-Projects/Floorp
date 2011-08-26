/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "CheckQuotaHelper.h"

#include "nsIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "nsXULAppAPI.h"

#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"

#include "IndexedDatabaseManager.h"

#define PERMISSION_INDEXEDDB_UNLIMITED "indexedDB-unlimited"

#define TOPIC_QUOTA_PROMPT "indexedDB-quota-prompt"
#define TOPIC_QUOTA_RESPONSE "indexedDB-quota-response"
#define TOPIC_QUOTA_CANCEL "indexedDB-quota-cancel"

USING_INDEXEDDB_NAMESPACE
using namespace mozilla::services;
using mozilla::MutexAutoLock;

namespace {

inline
PRUint32
GetQuotaPermissions(const nsACString& aASCIIOrigin,
                    nsIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIScriptObjectPrincipal> sop(do_QueryInterface(aWindow));
  NS_ENSURE_TRUE(sop, nsIPermissionManager::DENY_ACTION);

  if (nsContentUtils::IsSystemPrincipal(sop->GetPrincipal())) {
    return nsIPermissionManager::ALLOW_ACTION;
  }

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aASCIIOrigin);
  NS_ENSURE_SUCCESS(rv, nsIPermissionManager::DENY_ACTION);

  nsCOMPtr<nsIPermissionManager> permissionManager =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(permissionManager, nsIPermissionManager::DENY_ACTION);

  PRUint32 permission;
  rv = permissionManager->TestPermission(uri, PERMISSION_INDEXEDDB_UNLIMITED,
                                         &permission);
  NS_ENSURE_SUCCESS(rv, nsIPermissionManager::DENY_ACTION);

  return permission;
}

} // anonymous namespace

CheckQuotaHelper::CheckQuotaHelper(IDBDatabase* aDatabase,
                                   mozilla::Mutex& aMutex)
: mWindow(aDatabase->Owner()),
  mWindowSerial(mWindow->GetSerial()),
  mOrigin(aDatabase->Origin()),
  mMutex(aMutex),
  mCondVar(mMutex, "CheckQuotaHelper::mCondVar"),
  mPromptResult(0),
  mWaiting(true),
  mHasPrompted(false)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  mMutex.AssertCurrentThreadOwns();
}

bool
CheckQuotaHelper::PromptAndReturnQuotaIsDisabled()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  mMutex.AssertCurrentThreadOwns();

  while (mWaiting) {
    mCondVar.Wait();
  }

  NS_ASSERTION(!mWindow, "This should always be null here!");

  NS_ASSERTION(mPromptResult == nsIPermissionManager::ALLOW_ACTION ||
               mPromptResult == nsIPermissionManager::DENY_ACTION ||
               mPromptResult == nsIPermissionManager::UNKNOWN_ACTION,
               "Unknown permission!");

  return mPromptResult == nsIPermissionManager::ALLOW_ACTION;
}

void
CheckQuotaHelper::Cancel()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  mMutex.AssertCurrentThreadOwns();

  if (mWaiting && !mHasPrompted) {
    MutexAutoUnlock unlock(mMutex);

    // First close any prompts that are open for this window.
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    NS_WARN_IF_FALSE(obs, "Failed to get observer service!");
    if (obs && NS_FAILED(obs->NotifyObservers(static_cast<nsIRunnable*>(this),
                                              TOPIC_QUOTA_CANCEL, nsnull))) {
      NS_WARNING("Failed to notify observers!");
    }

    // If that didn't trigger an Observe callback (maybe the window had already
    // died?) then go ahead and do it manually.
    if (!mHasPrompted) {
      nsAutoString response;
      response.AppendInt(nsIPermissionManager::UNKNOWN_ACTION);

      if (NS_SUCCEEDED(Observe(nsnull, TOPIC_QUOTA_RESPONSE, response.get()))) {
        NS_ASSERTION(mHasPrompted, "Should have set this in Observe!");
      }
      else {
        NS_WARNING("Failed to notify!");
      }
    }
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS3(CheckQuotaHelper, nsIRunnable,
                                                nsIInterfaceRequestor,
                                                nsIObserver)

NS_IMETHODIMP
CheckQuotaHelper::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mHasPrompted) {
    mPromptResult = GetQuotaPermissions(mOrigin, mWindow);
  }

  nsresult rv;
  if (mHasPrompted) {
    // Add permissions to the database, but only if we are in the parent
    // process (if we are in the child process, we have already
    // set the permission when the prompt was shown in the parent, as
    // we cannot set the permission from the child).
    if (mPromptResult != nsIPermissionManager::UNKNOWN_ACTION &&
        XRE_GetProcessType() == GeckoProcessType_Default) {
      nsCOMPtr<nsIURI> uri;
      rv = NS_NewURI(getter_AddRefs(uri), mOrigin);
      NS_ENSURE_SUCCESS(rv, rv);
  
      nsCOMPtr<nsIPermissionManager> permissionManager =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
      NS_ENSURE_STATE(permissionManager);
  
      rv = permissionManager->Add(uri, PERMISSION_INDEXEDDB_UNLIMITED,
                                  mPromptResult,
                                  nsIPermissionManager::EXPIRE_NEVER, 0);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  else if (mPromptResult == nsIPermissionManager::UNKNOWN_ACTION) {
    PRUint32 quota = IndexedDatabaseManager::GetIndexedDBQuotaMB();

    nsString quotaString;
    quotaString.AppendInt(quota);

    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    NS_ENSURE_STATE(obs);

    // We have to watch to make sure that the window doesn't go away without
    // responding to us. Otherwise our database threads will hang.
    rv = obs->AddObserver(this, DOM_WINDOW_DESTROYED_TOPIC, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = obs->NotifyObservers(static_cast<nsIRunnable*>(this),
                              TOPIC_QUOTA_PROMPT, quotaString.get());
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  MutexAutoLock lock(mMutex);

  NS_ASSERTION(mWaiting, "Huh?!");

    // This should never be used again.
  mWindow = nsnull;

  mWaiting = false;
  mCondVar.NotifyAll();

  return NS_OK;
}

NS_IMETHODIMP
CheckQuotaHelper::GetInterface(const nsIID& aIID,
                               void** aResult)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aIID.Equals(NS_GET_IID(nsIObserver))) {
    return QueryInterface(aIID, aResult);
  }

  if (aIID.Equals(NS_GET_IID(nsIDOMWindow))) {
    return mWindow->QueryInterface(aIID, aResult);
  }

  *aResult = nsnull;
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
CheckQuotaHelper::Observe(nsISupports* aSubject,
                          const char* aTopic,
                          const PRUnichar* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsresult rv;

  if (!strcmp(aTopic, TOPIC_QUOTA_RESPONSE)) {
    if (!mHasPrompted) {
      mHasPrompted = true;

      mPromptResult = nsDependentString(aData).ToInteger(&rv);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = NS_DispatchToCurrentThread(this);
      NS_ENSURE_SUCCESS(rv, rv);

      // We no longer care about the window here.
      nsCOMPtr<nsIObserverService> obs = GetObserverService();
      NS_ENSURE_STATE(obs);

      rv = obs->RemoveObserver(this, DOM_WINDOW_DESTROYED_TOPIC);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
  }

  if (!strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC)) {
    NS_ASSERTION(!mHasPrompted, "Should have removed observer before now!");
    NS_ASSERTION(mWindow, "This should never be null!");

    nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aSubject));
    NS_ENSURE_STATE(window);

    if (mWindow->GetSerial() == window->GetSerial()) {
      // This is our window, dying, without responding to our prompt! Fake one.
      mHasPrompted = true;
      mPromptResult = nsIPermissionManager::UNKNOWN_ACTION;

      rv = NS_DispatchToCurrentThread(this);
      NS_ENSURE_SUCCESS(rv, rv);

      // We no longer care about the window here.
      nsCOMPtr<nsIObserverService> obs = GetObserverService();
      NS_ENSURE_STATE(obs);

      rv = obs->RemoveObserver(this, DOM_WINDOW_DESTROYED_TOPIC);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
  }

  NS_NOTREACHED("Unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}
