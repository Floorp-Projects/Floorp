/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CheckQuotaHelper.h"

#include "nsIDOMWindow.h"
#include "nsIObserverService.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIURI.h"
#include "nsPIDOMWindow.h"

#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/Services.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#define PERMISSION_INDEXEDDB_UNLIMITED "indexedDB-unlimited"

#define TOPIC_QUOTA_PROMPT "indexedDB-quota-prompt"
#define TOPIC_QUOTA_RESPONSE "indexedDB-quota-response"
#define TOPIC_QUOTA_CANCEL "indexedDB-quota-cancel"

USING_QUOTA_NAMESPACE
using namespace mozilla::services;
using mozilla::MutexAutoLock;

namespace {

inline
uint32_t
GetQuotaPermissionFromWindow(nsIDOMWindow* aWindow)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIScriptObjectPrincipal> sop(do_QueryInterface(aWindow));
  NS_ENSURE_TRUE(sop, nsIPermissionManager::DENY_ACTION);

  return CheckQuotaHelper::GetQuotaPermission(sop->GetPrincipal());
}

} // anonymous namespace

CheckQuotaHelper::CheckQuotaHelper(nsPIDOMWindow* aWindow,
                                   mozilla::Mutex& aMutex)
: mWindow(aWindow),
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
                                              TOPIC_QUOTA_CANCEL, nullptr))) {
      NS_WARNING("Failed to notify observers!");
    }

    // If that didn't trigger an Observe callback (maybe the window had already
    // died?) then go ahead and do it manually.
    if (!mHasPrompted) {
      nsAutoString response;
      response.AppendInt(nsIPermissionManager::UNKNOWN_ACTION);

      if (NS_SUCCEEDED(Observe(nullptr, TOPIC_QUOTA_RESPONSE, response.get()))) {
        NS_ASSERTION(mHasPrompted, "Should have set this in Observe!");
      }
      else {
        NS_WARNING("Failed to notify!");
      }
    }
  }
}

// static
uint32_t
CheckQuotaHelper::GetQuotaPermission(nsIPrincipal* aPrincipal)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aPrincipal, "Null principal!");

  NS_ASSERTION(!nsContentUtils::IsSystemPrincipal(aPrincipal),
               "Chrome windows shouldn't track quota!");

  nsCOMPtr<nsIPermissionManager> pm =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  NS_ENSURE_TRUE(pm, nsIPermissionManager::DENY_ACTION);

  uint32_t permission;
  nsresult rv = pm->TestPermissionFromPrincipal(aPrincipal,
                                                PERMISSION_INDEXEDDB_UNLIMITED,
                                                &permission);
  NS_ENSURE_SUCCESS(rv, nsIPermissionManager::DENY_ACTION);

  return permission;
}

NS_IMPL_ISUPPORTS3(CheckQuotaHelper, nsIRunnable,
                   nsIInterfaceRequestor,
                   nsIObserver)

NS_IMETHODIMP
CheckQuotaHelper::Run()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsresult rv = NS_OK;

  if (NS_SUCCEEDED(rv)) {
    if (!mHasPrompted) {
      mPromptResult = GetQuotaPermissionFromWindow(mWindow);
    }

    if (mHasPrompted) {
      // Add permissions to the database, but only if we are in the parent
      // process (if we are in the child process, we have already
      // set the permission when the prompt was shown in the parent, as
      // we cannot set the permission from the child).
      if (mPromptResult != nsIPermissionManager::UNKNOWN_ACTION &&
          XRE_GetProcessType() == GeckoProcessType_Default) {
        nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(mWindow);
        NS_ENSURE_TRUE(sop, NS_ERROR_FAILURE);

        nsCOMPtr<nsIPermissionManager> permissionManager =
          do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
        NS_ENSURE_STATE(permissionManager);

        rv = permissionManager->AddFromPrincipal(sop->GetPrincipal(),
                                                 PERMISSION_INDEXEDDB_UNLIMITED,
                                                 mPromptResult,
                                                 nsIPermissionManager::EXPIRE_NEVER, 0);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    else if (mPromptResult == nsIPermissionManager::UNKNOWN_ACTION) {
      uint32_t quota = QuotaManager::GetStorageQuotaMB();

      nsString quotaString;
      quotaString.AppendInt(quota);

      nsCOMPtr<nsIObserverService> obs = GetObserverService();
      NS_ENSURE_STATE(obs);

      // We have to watch to make sure that the window doesn't go away without
      // responding to us. Otherwise our database threads will hang.
      rv = obs->AddObserver(this, DOM_WINDOW_DESTROYED_TOPIC, false);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = obs->NotifyObservers(static_cast<nsIRunnable*>(this),
                                TOPIC_QUOTA_PROMPT, quotaString.get());
      NS_ENSURE_SUCCESS(rv, rv);

      return NS_OK;
    }
  }

  MutexAutoLock lock(mMutex);

  NS_ASSERTION(mWaiting, "Huh?!");

  // This should never be used again.
  mWindow = nullptr;

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

  *aResult = nullptr;
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
