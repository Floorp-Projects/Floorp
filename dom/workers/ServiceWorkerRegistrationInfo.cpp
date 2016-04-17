/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistrationInfo.h"

BEGIN_WORKERS_NAMESPACE

void
ServiceWorkerRegistrationInfo::Clear()
{
  if (mInstallingWorker) {
    mInstallingWorker->UpdateState(ServiceWorkerState::Redundant);
    mInstallingWorker->WorkerPrivate()->NoteDeadServiceWorkerInfo();
    mInstallingWorker = nullptr;
    // FIXME(nsm): Abort any inflight requests from installing worker.
  }

  if (mWaitingWorker) {
    mWaitingWorker->UpdateState(ServiceWorkerState::Redundant);

    nsresult rv = serviceWorkerScriptCache::PurgeCache(mPrincipal,
                                                       mWaitingWorker->CacheName());
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to purge the waiting cache.");
    }

    mWaitingWorker->WorkerPrivate()->NoteDeadServiceWorkerInfo();
    mWaitingWorker = nullptr;
  }

  if (mActiveWorker) {
    mActiveWorker->UpdateState(ServiceWorkerState::Redundant);

    nsresult rv = serviceWorkerScriptCache::PurgeCache(mPrincipal,
                                                       mActiveWorker->CacheName());
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to purge the active cache.");
    }

    mActiveWorker->WorkerPrivate()->NoteDeadServiceWorkerInfo();
    mActiveWorker = nullptr;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);
  swm->InvalidateServiceWorkerRegistrationWorker(this,
                                                 WhichServiceWorker::INSTALLING_WORKER |
                                                 WhichServiceWorker::WAITING_WORKER |
                                                 WhichServiceWorker::ACTIVE_WORKER);
}

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo(const nsACString& aScope,
                                                             nsIPrincipal* aPrincipal)
  : mControlledDocumentsCounter(0)
  , mUpdateState(NoUpdate)
  , mLastUpdateCheckTime(0)
  , mScope(aScope)
  , mPrincipal(aPrincipal)
  , mPendingUninstall(false)
{}

ServiceWorkerRegistrationInfo::~ServiceWorkerRegistrationInfo()
{
  if (IsControllingDocuments()) {
    NS_WARNING("ServiceWorkerRegistrationInfo is still controlling documents. This can be a bug or a leak in ServiceWorker API or in any other API that takes the document alive.");
  }
}

NS_IMPL_ISUPPORTS(ServiceWorkerRegistrationInfo, nsIServiceWorkerRegistrationInfo)

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetPrincipal(nsIPrincipal** aPrincipal)
{
  AssertIsOnMainThread();
  NS_ADDREF(*aPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetScope(nsAString& aScope)
{
  AssertIsOnMainThread();
  CopyUTF8toUTF16(mScope, aScope);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetScriptSpec(nsAString& aScriptSpec)
{
  AssertIsOnMainThread();
  RefPtr<ServiceWorkerInfo> newest = Newest();
  if (newest) {
    CopyUTF8toUTF16(newest->ScriptSpec(), aScriptSpec);
  }
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetInstallingWorker(nsIServiceWorkerInfo **aResult)
{
  AssertIsOnMainThread();
  nsCOMPtr<nsIServiceWorkerInfo> info = do_QueryInterface(mInstallingWorker);
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetWaitingWorker(nsIServiceWorkerInfo **aResult)
{
  AssertIsOnMainThread();
  nsCOMPtr<nsIServiceWorkerInfo> info = do_QueryInterface(mWaitingWorker);
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetActiveWorker(nsIServiceWorkerInfo **aResult)
{
  AssertIsOnMainThread();
  nsCOMPtr<nsIServiceWorkerInfo> info = do_QueryInterface(mActiveWorker);
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetWorkerByID(uint64_t aID, nsIServiceWorkerInfo **aResult)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aResult);

  RefPtr<ServiceWorkerInfo> info = GetServiceWorkerInfoById(aID);
  // It is ok to return null for a missing service worker info.
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::AddListener(
                            nsIServiceWorkerRegistrationInfoListener *aListener)
{
  AssertIsOnMainThread();

  if (!aListener || mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.AppendElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::RemoveListener(
                            nsIServiceWorkerRegistrationInfoListener *aListener)
{
  AssertIsOnMainThread();

  if (!aListener || !mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.RemoveElement(aListener);

  return NS_OK;
}

already_AddRefed<ServiceWorkerInfo>
ServiceWorkerRegistrationInfo::GetServiceWorkerInfoById(uint64_t aId)
{
  RefPtr<ServiceWorkerInfo> serviceWorker;
  if (mInstallingWorker && mInstallingWorker->ID() == aId) {
    serviceWorker = mInstallingWorker;
  } else if (mWaitingWorker && mWaitingWorker->ID() == aId) {
    serviceWorker = mWaitingWorker;
  } else if (mActiveWorker && mActiveWorker->ID() == aId) {
    serviceWorker = mActiveWorker;
  }

  return serviceWorker.forget();
}

void
ServiceWorkerRegistrationInfo::TryToActivateAsync()
{
  nsCOMPtr<nsIRunnable> r =
  NS_NewRunnableMethod(this,
                       &ServiceWorkerRegistrationInfo::TryToActivate);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(r));
}

/*
 * TryToActivate should not be called directly, use TryToACtivateAsync instead.
 */
void
ServiceWorkerRegistrationInfo::TryToActivate()
{
  if (!IsControllingDocuments() ||
      // Waiting worker will be removed if the registration is removed
      (mWaitingWorker && mWaitingWorker->SkipWaitingFlag())) {
    Activate();
  }
}

void
ServiceWorkerRegistrationInfo::PurgeActiveWorker()
{
  RefPtr<ServiceWorkerInfo> exitingWorker = mActiveWorker.forget();
  if (!exitingWorker)
    return;

  // FIXME(jaoo): Bug 1170543 - Wait for exitingWorker to finish and terminate it.
  exitingWorker->UpdateState(ServiceWorkerState::Redundant);
  nsresult rv = serviceWorkerScriptCache::PurgeCache(mPrincipal,
                                                     exitingWorker->CacheName());
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to purge the activating cache.");
  }
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  swm->InvalidateServiceWorkerRegistrationWorker(this, WhichServiceWorker::ACTIVE_WORKER);
}

void
ServiceWorkerRegistrationInfo::Activate()
{
  RefPtr<ServiceWorkerInfo> activatingWorker = mWaitingWorker;
  if (!activatingWorker) {
    return;
  }

  PurgeActiveWorker();

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  swm->InvalidateServiceWorkerRegistrationWorker(this, WhichServiceWorker::WAITING_WORKER);

  mActiveWorker = activatingWorker.forget();
  mWaitingWorker = nullptr;
  mActiveWorker->UpdateState(ServiceWorkerState::Activating);
  NotifyListenersOnChange();

  // FIXME(nsm): Unlink appcache if there is one.

  swm->CheckPendingReadyPromises();

  // "Queue a task to fire a simple event named controllerchange..."
  nsCOMPtr<nsIRunnable> controllerChangeRunnable =
    NS_NewRunnableMethodWithArg<RefPtr<ServiceWorkerRegistrationInfo>>(
      swm, &ServiceWorkerManager::FireControllerChange, this);
  NS_DispatchToMainThread(controllerChangeRunnable);

  nsCOMPtr<nsIRunnable> failRunnable =
    NS_NewRunnableMethodWithArg<bool>(this,
                                      &ServiceWorkerRegistrationInfo::FinishActivate,
                                      false /* success */);

  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> handle(
    new nsMainThreadPtrHolder<ServiceWorkerRegistrationInfo>(this));
  RefPtr<LifeCycleEventCallback> callback = new ContinueActivateRunnable(handle);

  ServiceWorkerPrivate* workerPrivate = mActiveWorker->WorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  nsresult rv = workerPrivate->SendLifeCycleEvent(NS_LITERAL_STRING("activate"),
                                                  callback, failRunnable);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(failRunnable));
    return;
  }
}

void
ServiceWorkerRegistrationInfo::FinishActivate(bool aSuccess)
{
  if (mPendingUninstall || !mActiveWorker ||
      mActiveWorker->State() != ServiceWorkerState::Activating) {
    return;
  }

  // Activation never fails, so aSuccess is ignored.
  mActiveWorker->UpdateState(ServiceWorkerState::Activated);
  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  swm->StoreRegistration(mPrincipal, this);
}

void
ServiceWorkerRegistrationInfo::RefreshLastUpdateCheckTime()
{
  AssertIsOnMainThread();
  mLastUpdateCheckTime = PR_IntervalNow() / PR_MSEC_PER_SEC;
}

bool
ServiceWorkerRegistrationInfo::IsLastUpdateCheckTimeOverOneDay() const
{
  AssertIsOnMainThread();

  // For testing.
  if (Preferences::GetBool("dom.serviceWorkers.testUpdateOverOneDay")) {
    return true;
  }

  const uint64_t kSecondsPerDay = 86400;
  const uint64_t now = PR_IntervalNow() / PR_MSEC_PER_SEC;

  if ((mLastUpdateCheckTime != 0) &&
      (now - mLastUpdateCheckTime > kSecondsPerDay)) {
    return true;
  }
  return false;
}

void
ServiceWorkerRegistrationInfo::NotifyListenersOnChange()
{
  nsTArray<nsCOMPtr<nsIServiceWorkerRegistrationInfoListener>> listeners(mListeners);
  for (size_t index = 0; index < listeners.Length(); ++index) {
    listeners[index]->OnChange();
  }
}

void
ServiceWorkerRegistrationInfo::MaybeScheduleTimeCheckAndUpdate()
{
  AssertIsOnMainThread();

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // shutting down, do nothing
    return;
  }

  if (mUpdateState == NoUpdate) {
    mUpdateState = NeedTimeCheckAndUpdate;
  }

  swm->ScheduleUpdateTimer(mPrincipal, mScope);
}

void
ServiceWorkerRegistrationInfo::MaybeScheduleUpdate()
{
  AssertIsOnMainThread();

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // shutting down, do nothing
    return;
  }

  mUpdateState = NeedUpdate;

  swm->ScheduleUpdateTimer(mPrincipal, mScope);
}

bool
ServiceWorkerRegistrationInfo::CheckAndClearIfUpdateNeeded()
{
  AssertIsOnMainThread();

  bool result = mUpdateState == NeedUpdate ||
               (mUpdateState == NeedTimeCheckAndUpdate &&
                IsLastUpdateCheckTimeOverOneDay());

  mUpdateState = NoUpdate;

  return result;
}

ServiceWorkerInfo*
ServiceWorkerRegistrationInfo::GetInstalling() const
{
  AssertIsOnMainThread();
  return mInstallingWorker;
}

ServiceWorkerInfo*
ServiceWorkerRegistrationInfo::GetWaiting() const
{
  AssertIsOnMainThread();
  return mWaitingWorker;
}

ServiceWorkerInfo*
ServiceWorkerRegistrationInfo::GetActive() const
{
  AssertIsOnMainThread();
  return mActiveWorker;
}

void
ServiceWorkerRegistrationInfo::SetInstalling(ServiceWorkerInfo* aServiceWorker)
{
  AssertIsOnMainThread();
  mInstallingWorker = aServiceWorker;
}

void
ServiceWorkerRegistrationInfo::SetWaiting(ServiceWorkerInfo* aServiceWorker)
{
  AssertIsOnMainThread();
  mWaitingWorker = aServiceWorker;
}

void
ServiceWorkerRegistrationInfo::SetActive(ServiceWorkerInfo* aServiceWorker)
{
  AssertIsOnMainThread();
  mActiveWorker = aServiceWorker;
}

END_WORKERS_NAMESPACE
