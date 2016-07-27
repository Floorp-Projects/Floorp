/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistrationInfo.h"

BEGIN_WORKERS_NAMESPACE

namespace {

class ContinueActivateRunnable final : public LifeCycleEventCallback
{
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;
  bool mSuccess;

public:
  explicit ContinueActivateRunnable(const nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration)
    : mRegistration(aRegistration)
    , mSuccess(false)
  {
    AssertIsOnMainThread();
  }

  void
  SetResult(bool aResult) override
  {
    mSuccess = aResult;
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    mRegistration->FinishActivate(mSuccess);
    mRegistration = nullptr;
    return NS_OK;
  }
};

} // anonymous namespace

void
ServiceWorkerRegistrationInfo::Clear()
{
  if (mEvaluatingWorker) {
    mEvaluatingWorker = nullptr;
  }

  if (mInstallingWorker) {
    mInstallingWorker->UpdateState(ServiceWorkerState::Redundant);
    mInstallingWorker->WorkerPrivate()->NoteDeadServiceWorkerInfo();
    mInstallingWorker = nullptr;
    // FIXME(nsm): Abort any inflight requests from installing worker.
  }

  if (mWaitingWorker) {
    mWaitingWorker->UpdateState(ServiceWorkerState::Redundant);
    mWaitingWorker->WorkerPrivate()->NoteDeadServiceWorkerInfo();
    mWaitingWorker = nullptr;
  }

  if (mActiveWorker) {
    mActiveWorker->UpdateState(ServiceWorkerState::Redundant);
    mActiveWorker->WorkerPrivate()->NoteDeadServiceWorkerInfo();
    mActiveWorker = nullptr;
  }

  NotifyListenersOnChange(WhichServiceWorker::INSTALLING_WORKER |
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
  AssertIsOnMainThread();

  RefPtr<ServiceWorkerInfo> serviceWorker;
  if (mEvaluatingWorker && mEvaluatingWorker->ID() == aId) {
    serviceWorker = mEvaluatingWorker;
  } else if (mInstallingWorker && mInstallingWorker->ID() == aId) {
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
  MOZ_ALWAYS_SUCCEEDS(
    NS_DispatchToMainThread(NewRunnableMethod(this,
                                              &ServiceWorkerRegistrationInfo::TryToActivate)));
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
ServiceWorkerRegistrationInfo::Activate()
{
  if (!mWaitingWorker) {
    return;
  }

  TransitionWaitingToActive();

  // FIXME(nsm): Unlink appcache if there is one.

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  swm->CheckPendingReadyPromises();

  // "Queue a task to fire a simple event named controllerchange..."
  nsCOMPtr<nsIRunnable> controllerChangeRunnable =
    NewRunnableMethod<RefPtr<ServiceWorkerRegistrationInfo>>(
      swm, &ServiceWorkerManager::FireControllerChange, this);
  NS_DispatchToMainThread(controllerChangeRunnable);

  nsCOMPtr<nsIRunnable> failRunnable =
    NewRunnableMethod<bool>(this,
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

  if ((now - mLastUpdateCheckTime) > kSecondsPerDay) {
    return true;
  }
  return false;
}

void
ServiceWorkerRegistrationInfo::NotifyListenersOnChange(WhichServiceWorker aChangedWorkers)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aChangedWorkers & (WhichServiceWorker::INSTALLING_WORKER |
                                WhichServiceWorker::WAITING_WORKER |
                                WhichServiceWorker::ACTIVE_WORKER));

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  swm->InvalidateServiceWorkerRegistrationWorker(this, aChangedWorkers);

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
ServiceWorkerRegistrationInfo::GetEvaluating() const
{
  AssertIsOnMainThread();
  return mEvaluatingWorker;
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
ServiceWorkerRegistrationInfo::SetEvaluating(ServiceWorkerInfo* aServiceWorker)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aServiceWorker);
  MOZ_ASSERT(!mEvaluatingWorker);
  MOZ_ASSERT(!mInstallingWorker);
  MOZ_ASSERT(mWaitingWorker != aServiceWorker);
  MOZ_ASSERT(mActiveWorker != aServiceWorker);

  mEvaluatingWorker = aServiceWorker;
}

void
ServiceWorkerRegistrationInfo::ClearEvaluating()
{
  AssertIsOnMainThread();

  if (!mEvaluatingWorker) {
    return;
  }

  mEvaluatingWorker->UpdateState(ServiceWorkerState::Redundant);
  mEvaluatingWorker = nullptr;
}

void
ServiceWorkerRegistrationInfo::ClearInstalling()
{
  AssertIsOnMainThread();

  if (!mInstallingWorker) {
    return;
  }

  mInstallingWorker->UpdateState(ServiceWorkerState::Redundant);
  mInstallingWorker = nullptr;
  NotifyListenersOnChange(WhichServiceWorker::INSTALLING_WORKER);
}

void
ServiceWorkerRegistrationInfo::TransitionEvaluatingToInstalling()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mEvaluatingWorker);
  MOZ_ASSERT(!mInstallingWorker);

  mInstallingWorker = mEvaluatingWorker.forget();
  mInstallingWorker->UpdateState(ServiceWorkerState::Installing);
  NotifyListenersOnChange(WhichServiceWorker::INSTALLING_WORKER);
}

void
ServiceWorkerRegistrationInfo::TransitionInstallingToWaiting()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mInstallingWorker);

  if (mWaitingWorker) {
    MOZ_ASSERT(mInstallingWorker->CacheName() != mWaitingWorker->CacheName());
    mWaitingWorker->UpdateState(ServiceWorkerState::Redundant);
  }

  mWaitingWorker = mInstallingWorker.forget();
  mWaitingWorker->UpdateState(ServiceWorkerState::Installed);
  NotifyListenersOnChange(WhichServiceWorker::INSTALLING_WORKER |
                          WhichServiceWorker::WAITING_WORKER);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  swm->StoreRegistration(mPrincipal, this);
}

void
ServiceWorkerRegistrationInfo::SetActive(ServiceWorkerInfo* aServiceWorker)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aServiceWorker);

  // TODO: Assert installing, waiting, and active are nullptr once the SWM
  //       moves to the parent process.  After that happens this code will
  //       only run for browser initialization and not for cross-process
  //       overrides.
  MOZ_ASSERT(mInstallingWorker != aServiceWorker);
  MOZ_ASSERT(mWaitingWorker != aServiceWorker);
  MOZ_ASSERT(mActiveWorker != aServiceWorker);

  if (mActiveWorker) {
    MOZ_ASSERT(aServiceWorker->CacheName() != mActiveWorker->CacheName());
    mActiveWorker->UpdateState(ServiceWorkerState::Redundant);
  }

  // The active worker is being overriden due to initial load or
  // another process activating a worker.  Move straight to the
  // Activated state.
  mActiveWorker = aServiceWorker;
  mActiveWorker->SetActivateStateUncheckedWithoutEvent(ServiceWorkerState::Activated);
  NotifyListenersOnChange(WhichServiceWorker::ACTIVE_WORKER);
}

void
ServiceWorkerRegistrationInfo::TransitionWaitingToActive()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(mWaitingWorker);

  if (mActiveWorker) {
    MOZ_ASSERT(mWaitingWorker->CacheName() != mActiveWorker->CacheName());
    mActiveWorker->UpdateState(ServiceWorkerState::Redundant);
  }

  // We are transitioning from waiting to active normally, so go to
  // the activating state.
  mActiveWorker = mWaitingWorker.forget();
  mActiveWorker->UpdateState(ServiceWorkerState::Activating);
  NotifyListenersOnChange(WhichServiceWorker::WAITING_WORKER |
                          WhichServiceWorker::ACTIVE_WORKER);
}

END_WORKERS_NAMESPACE
