/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistrationInfo.h"

#include "ServiceWorkerManager.h"
#include "ServiceWorkerPrivate.h"

namespace mozilla {
namespace dom {

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
    MOZ_ASSERT(NS_IsMainThread());
  }

  void
  SetResult(bool aResult) override
  {
    mSuccess = aResult;
  }

  NS_IMETHOD
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());
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

  RefPtr<ServiceWorkerInfo> installing = mInstallingWorker.forget();
  RefPtr<ServiceWorkerInfo> waiting = mWaitingWorker.forget();
  RefPtr<ServiceWorkerInfo> active = mActiveWorker.forget();

  if (installing) {
    installing->UpdateState(ServiceWorkerState::Redundant);
    installing->UpdateRedundantTime();
    installing->WorkerPrivate()->NoteDeadServiceWorkerInfo();
    // FIXME(nsm): Abort any inflight requests from installing worker.
  }

  if (waiting) {
    waiting->UpdateState(ServiceWorkerState::Redundant);
    waiting->UpdateRedundantTime();
    waiting->WorkerPrivate()->NoteDeadServiceWorkerInfo();
  }

  if (active) {
    active->UpdateState(ServiceWorkerState::Redundant);
    active->UpdateRedundantTime();
    active->WorkerPrivate()->NoteDeadServiceWorkerInfo();
  }

  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();
}

void
ServiceWorkerRegistrationInfo::ClearAsCorrupt()
{
  mCorrupt = true;
  Clear();
}

bool
ServiceWorkerRegistrationInfo::IsCorrupt() const
{
  return mCorrupt;
}

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo(
    const nsACString& aScope,
    nsIPrincipal* aPrincipal,
    ServiceWorkerUpdateViaCache aUpdateViaCache)
  : mPrincipal(aPrincipal)
  , mDescriptor(GetNextId(), aPrincipal, aScope, aUpdateViaCache)
  , mControlledClientsCounter(0)
  , mDelayMultiplier(0)
  , mUpdateState(NoUpdate)
  , mCreationTime(PR_Now())
  , mCreationTimeStamp(TimeStamp::Now())
  , mLastUpdateTime(0)
  , mPendingUninstall(false)
  , mCorrupt(false)
{}

ServiceWorkerRegistrationInfo::~ServiceWorkerRegistrationInfo()
{
  MOZ_DIAGNOSTIC_ASSERT(!IsControllingClients());
}

const nsCString&
ServiceWorkerRegistrationInfo::Scope() const
{
  return mDescriptor.Scope();
}

nsIPrincipal*
ServiceWorkerRegistrationInfo::Principal() const
{
  return mPrincipal;
}

bool
ServiceWorkerRegistrationInfo::IsPendingUninstall() const
{
  return mPendingUninstall;
}

void
ServiceWorkerRegistrationInfo::SetPendingUninstall()
{
  mPendingUninstall = true;
}

void
ServiceWorkerRegistrationInfo::ClearPendingUninstall()
{
  // If we are resurrecting an uninstalling registration, then persist
  // it to disk again.  We preemptively removed it earlier during
  // unregister so that closing the window by shutting down the browser
  // results in the registration being gone on restart.
  if (mPendingUninstall && mActiveWorker) {
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->StoreRegistration(mPrincipal, this);
    }
  }
  mPendingUninstall = false;
}

NS_IMPL_ISUPPORTS(ServiceWorkerRegistrationInfo, nsIServiceWorkerRegistrationInfo)

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetPrincipal(nsIPrincipal** aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ADDREF(*aPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetScope(nsAString& aScope)
{
  MOZ_ASSERT(NS_IsMainThread());
  CopyUTF8toUTF16(Scope(), aScope);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetScriptSpec(nsAString& aScriptSpec)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ServiceWorkerInfo> newest = Newest();
  if (newest) {
    CopyUTF8toUTF16(newest->ScriptSpec(), aScriptSpec);
  }
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetUpdateViaCache(uint16_t* aUpdateViaCache)
{
    *aUpdateViaCache = static_cast<uint16_t>(GetUpdateViaCache());
    return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetLastUpdateTime(PRTime* _retval)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(_retval);
  *_retval = mLastUpdateTime;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetInstallingWorker(nsIServiceWorkerInfo **aResult)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIServiceWorkerInfo> info = do_QueryInterface(mInstallingWorker);
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetWaitingWorker(nsIServiceWorkerInfo **aResult)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIServiceWorkerInfo> info = do_QueryInterface(mWaitingWorker);
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetActiveWorker(nsIServiceWorkerInfo **aResult)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIServiceWorkerInfo> info = do_QueryInterface(mActiveWorker);
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetWorkerByID(uint64_t aID, nsIServiceWorkerInfo **aResult)
{
  MOZ_ASSERT(NS_IsMainThread());
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
  MOZ_ASSERT(NS_IsMainThread());

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
  MOZ_ASSERT(NS_IsMainThread());

  if (!aListener || !mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.RemoveElement(aListener);

  return NS_OK;
}

already_AddRefed<ServiceWorkerInfo>
ServiceWorkerRegistrationInfo::GetServiceWorkerInfoById(uint64_t aId)
{
  MOZ_ASSERT(NS_IsMainThread());

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
    NS_DispatchToMainThread(NewRunnableMethod("ServiceWorkerRegistrationInfo::TryToActivate",
                                              this,
                                              &ServiceWorkerRegistrationInfo::TryToActivate)));
}

/*
 * TryToActivate should not be called directly, use TryToActivateAsync instead.
 */
void
ServiceWorkerRegistrationInfo::TryToActivate()
{
  MOZ_ASSERT(NS_IsMainThread());
  bool controlling = IsControllingClients();
  bool skipWaiting = mWaitingWorker && mWaitingWorker->SkipWaitingFlag();
  bool idle = IsIdle();
  if (idle && (!controlling || skipWaiting)) {
    Activate();
  }
}

void
ServiceWorkerRegistrationInfo::Activate()
{
  if (!mWaitingWorker) {
    return;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // browser shutdown began during async activation step
    return;
  }

  TransitionWaitingToActive();

  // FIXME(nsm): Unlink appcache if there is one.

  // "Queue a task to fire a simple event named controllerchange..."
  MOZ_DIAGNOSTIC_ASSERT(mActiveWorker);
  swm->UpdateClientControllers(this);

  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> handle(
    new nsMainThreadPtrHolder<ServiceWorkerRegistrationInfo>(
      "ServiceWorkerRegistrationInfoProxy", this));
  RefPtr<LifeCycleEventCallback> callback = new ContinueActivateRunnable(handle);

  ServiceWorkerPrivate* workerPrivate = mActiveWorker->WorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  nsresult rv = workerPrivate->SendLifeCycleEvent(NS_LITERAL_STRING("activate"),
                                                  callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    nsCOMPtr<nsIRunnable> failRunnable = NewRunnableMethod<bool>(
      "dom::ServiceWorkerRegistrationInfo::FinishActivate",
      this,
      &ServiceWorkerRegistrationInfo::FinishActivate,
      false /* success */);
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(failRunnable.forget()));
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
  mActiveWorker->UpdateActivatedTime();

  mDescriptor.SetWorkers(mInstallingWorker, mWaitingWorker, mActiveWorker);

  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // browser shutdown started during async activation completion step
    return;
  }
  swm->StoreRegistration(mPrincipal, this);
}

void
ServiceWorkerRegistrationInfo::RefreshLastUpdateCheckTime()
{
  MOZ_ASSERT(NS_IsMainThread());

  mLastUpdateTime =
    mCreationTime + static_cast<PRTime>((TimeStamp::Now() -
                                         mCreationTimeStamp).ToMicroseconds());
  NotifyChromeRegistrationListeners();
}

bool
ServiceWorkerRegistrationInfo::IsLastUpdateCheckTimeOverOneDay() const
{
  MOZ_ASSERT(NS_IsMainThread());

  // For testing.
  if (Preferences::GetBool("dom.serviceWorkers.testUpdateOverOneDay")) {
    return true;
  }

  const int64_t kSecondsPerDay = 86400;
  const int64_t nowMicros =
    mCreationTime + static_cast<PRTime>((TimeStamp::Now() -
                                         mCreationTimeStamp).ToMicroseconds());

  // now < mLastUpdateTime if the system time is reset between storing
  // and loading mLastUpdateTime from ServiceWorkerRegistrar.
  if (nowMicros < mLastUpdateTime ||
      (nowMicros - mLastUpdateTime) / PR_USEC_PER_SEC > kSecondsPerDay) {
    return true;
  }
  return false;
}

void
ServiceWorkerRegistrationInfo::UpdateRegistrationState()
{
  MOZ_ASSERT(NS_IsMainThread());

  mDescriptor.SetWorkers(mInstallingWorker, mWaitingWorker, mActiveWorker);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  NS_ENSURE_TRUE_VOID(swm);

  swm->UpdateRegistrationListeners(this);
}

void
ServiceWorkerRegistrationInfo::NotifyChromeRegistrationListeners()
{
  nsTArray<nsCOMPtr<nsIServiceWorkerRegistrationInfoListener>> listeners(mListeners);
  for (size_t index = 0; index < listeners.Length(); ++index) {
    listeners[index]->OnChange();
  }
}

void
ServiceWorkerRegistrationInfo::MaybeScheduleTimeCheckAndUpdate()
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // shutting down, do nothing
    return;
  }

  if (mUpdateState == NoUpdate) {
    mUpdateState = NeedTimeCheckAndUpdate;
  }

  swm->ScheduleUpdateTimer(mPrincipal, Scope());
}

void
ServiceWorkerRegistrationInfo::MaybeScheduleUpdate()
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // shutting down, do nothing
    return;
  }

  mUpdateState = NeedUpdate;

  swm->ScheduleUpdateTimer(mPrincipal, Scope());
}

bool
ServiceWorkerRegistrationInfo::CheckAndClearIfUpdateNeeded()
{
  MOZ_ASSERT(NS_IsMainThread());

  bool result = mUpdateState == NeedUpdate ||
               (mUpdateState == NeedTimeCheckAndUpdate &&
                IsLastUpdateCheckTimeOverOneDay());

  mUpdateState = NoUpdate;

  return result;
}

ServiceWorkerInfo*
ServiceWorkerRegistrationInfo::GetEvaluating() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mEvaluatingWorker;
}

ServiceWorkerInfo*
ServiceWorkerRegistrationInfo::GetInstalling() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mInstallingWorker;
}

ServiceWorkerInfo*
ServiceWorkerRegistrationInfo::GetWaiting() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mWaitingWorker;
}

ServiceWorkerInfo*
ServiceWorkerRegistrationInfo::GetActive() const
{
  MOZ_ASSERT(NS_IsMainThread());
  return mActiveWorker;
}

ServiceWorkerInfo*
ServiceWorkerRegistrationInfo::GetByDescriptor(const ServiceWorkerDescriptor& aDescriptor) const
{
  if (mActiveWorker && mActiveWorker->Descriptor().Matches(aDescriptor)) {
    return mActiveWorker;
  }
  if (mWaitingWorker && mWaitingWorker->Descriptor().Matches(aDescriptor)) {
    return mWaitingWorker;
  }
  if (mInstallingWorker && mInstallingWorker->Descriptor().Matches(aDescriptor)) {
    return mInstallingWorker;
  }
  if (mEvaluatingWorker && mEvaluatingWorker->Descriptor().Matches(aDescriptor)) {
    return mEvaluatingWorker;
  }
  return nullptr;
}

void
ServiceWorkerRegistrationInfo::SetEvaluating(ServiceWorkerInfo* aServiceWorker)
{
  MOZ_ASSERT(NS_IsMainThread());
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
  MOZ_ASSERT(NS_IsMainThread());

  if (!mEvaluatingWorker) {
    return;
  }

  mEvaluatingWorker->UpdateState(ServiceWorkerState::Redundant);
  // We don't update the redundant time for the sw here, since we've not expose
  // evalutingWorker yet.
  mEvaluatingWorker = nullptr;
}

void
ServiceWorkerRegistrationInfo::ClearInstalling()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mInstallingWorker) {
    return;
  }

  RefPtr<ServiceWorkerInfo> installing = mInstallingWorker.forget();
  installing->UpdateState(ServiceWorkerState::Redundant);
  installing->UpdateRedundantTime();

  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();
}

void
ServiceWorkerRegistrationInfo::TransitionEvaluatingToInstalling()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mEvaluatingWorker);
  MOZ_ASSERT(!mInstallingWorker);

  mInstallingWorker = mEvaluatingWorker.forget();
  mInstallingWorker->UpdateState(ServiceWorkerState::Installing);

  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();
}

void
ServiceWorkerRegistrationInfo::TransitionInstallingToWaiting()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInstallingWorker);

  if (mWaitingWorker) {
    MOZ_ASSERT(mInstallingWorker->CacheName() != mWaitingWorker->CacheName());
    mWaitingWorker->UpdateState(ServiceWorkerState::Redundant);
    mWaitingWorker->UpdateRedundantTime();
  }

  mWaitingWorker = mInstallingWorker.forget();
  mWaitingWorker->UpdateState(ServiceWorkerState::Installed);
  mWaitingWorker->UpdateInstalledTime();

  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();

  // TODO: When bug 1426401 is implemented we will need to call
  //       StoreRegistration() here to persist the waiting worker.
}

void
ServiceWorkerRegistrationInfo::SetActive(ServiceWorkerInfo* aServiceWorker)
{
  MOZ_ASSERT(NS_IsMainThread());
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
    mActiveWorker->UpdateRedundantTime();
  }

  // The active worker is being overriden due to initial load or
  // another process activating a worker.  Move straight to the
  // Activated state.
  mActiveWorker = aServiceWorker;
  mActiveWorker->SetActivateStateUncheckedWithoutEvent(ServiceWorkerState::Activated);

  // We don't need to update activated time when we load registration from
  // registrar.
  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();
}

void
ServiceWorkerRegistrationInfo::TransitionWaitingToActive()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mWaitingWorker);

  if (mActiveWorker) {
    MOZ_ASSERT(mWaitingWorker->CacheName() != mActiveWorker->CacheName());
    mActiveWorker->UpdateState(ServiceWorkerState::Redundant);
    mActiveWorker->UpdateRedundantTime();
  }

  // We are transitioning from waiting to active normally, so go to
  // the activating state.
  mActiveWorker = mWaitingWorker.forget();
  mActiveWorker->UpdateState(ServiceWorkerState::Activating);

  nsCOMPtr<nsIRunnable> r =
    NS_NewRunnableFunction("ServiceWorkerRegistrationInfo::TransitionWaitingToActive",
    [] {
      RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
      if (swm) {
        swm->CheckPendingReadyPromises();
      }
    });
  MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));

  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();
}

bool
ServiceWorkerRegistrationInfo::IsIdle() const
{
  return !mActiveWorker || mActiveWorker->WorkerPrivate()->IsIdle();
}

ServiceWorkerUpdateViaCache
ServiceWorkerRegistrationInfo::GetUpdateViaCache() const
{
  return mDescriptor.UpdateViaCache();
}

void
ServiceWorkerRegistrationInfo::SetUpdateViaCache(
    ServiceWorkerUpdateViaCache aUpdateViaCache)
{
  mDescriptor.SetUpdateViaCache(aUpdateViaCache);
  UpdateRegistrationState();
}

int64_t
ServiceWorkerRegistrationInfo::GetLastUpdateTime() const
{
  return mLastUpdateTime;
}

void
ServiceWorkerRegistrationInfo::SetLastUpdateTime(const int64_t aTime)
{
  if (aTime == 0) {
    return;
  }

  mLastUpdateTime = aTime;
}

const ServiceWorkerRegistrationDescriptor&
ServiceWorkerRegistrationInfo::Descriptor() const
{
  return mDescriptor;
}

uint64_t
ServiceWorkerRegistrationInfo::Id() const
{
  return mDescriptor.Id();
}

uint32_t
ServiceWorkerRegistrationInfo::GetUpdateDelay()
{
  uint32_t delay = Preferences::GetInt("dom.serviceWorkers.update_delay",
                                       1000);
  // This can potentially happen if you spam registration->Update(). We don't
  // want to wrap to a lower value.
  if (mDelayMultiplier >= INT_MAX / (delay ? delay : 1)) {
    return INT_MAX;
  }

  delay *= mDelayMultiplier;

  if (!mControlledClientsCounter && mDelayMultiplier < (INT_MAX / 30)) {
    mDelayMultiplier = (mDelayMultiplier ? mDelayMultiplier : 1) * 30;
  }

  return delay;
}

// static
uint64_t
ServiceWorkerRegistrationInfo::GetNextId()
{
  MOZ_ASSERT(NS_IsMainThread());
  static uint64_t sNextId = 0;
  return ++sNextId;
}

} // namespace dom
} // namespace mozilla
