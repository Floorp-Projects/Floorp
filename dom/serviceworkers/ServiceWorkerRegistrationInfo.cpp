/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegistrationInfo.h"

#include "ServiceWorkerManager.h"
#include "ServiceWorkerPrivate.h"
#include "ServiceWorkerRegistrationListener.h"

#include "mozilla/SchedulerGroup.h"

namespace mozilla {
namespace dom {

namespace {

class ContinueActivateRunnable final : public LifeCycleEventCallback {
  nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo> mRegistration;
  bool mSuccess;

 public:
  explicit ContinueActivateRunnable(
      const nsMainThreadPtrHandle<ServiceWorkerRegistrationInfo>& aRegistration)
      : mRegistration(aRegistration), mSuccess(false) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  void SetResult(bool aResult) override { mSuccess = aResult; }

  NS_IMETHOD
  Run() override {
    MOZ_ASSERT(NS_IsMainThread());
    mRegistration->FinishActivate(mSuccess);
    mRegistration = nullptr;
    return NS_OK;
  }
};

}  // anonymous namespace

void ServiceWorkerRegistrationInfo::ShutdownWorkers() {
  ForEachWorker([](RefPtr<ServiceWorkerInfo>& aWorker) {
    aWorker->WorkerPrivate()->NoteDeadServiceWorkerInfo();
    aWorker = nullptr;
  });
}

void ServiceWorkerRegistrationInfo::Clear() {
  ForEachWorker([](RefPtr<ServiceWorkerInfo>& aWorker) {
    aWorker->UpdateState(ServiceWorkerState::Redundant);
    aWorker->UpdateRedundantTime();
  });

  // FIXME: Abort any inflight requests from installing worker.

  ShutdownWorkers();
  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();
  NotifyCleared();
}

void ServiceWorkerRegistrationInfo::ClearAsCorrupt() {
  mCorrupt = true;
  Clear();
}

bool ServiceWorkerRegistrationInfo::IsCorrupt() const { return mCorrupt; }

ServiceWorkerRegistrationInfo::ServiceWorkerRegistrationInfo(
    const nsACString& aScope, nsIPrincipal* aPrincipal,
    ServiceWorkerUpdateViaCache aUpdateViaCache)
    : mPrincipal(aPrincipal),
      mDescriptor(GetNextId(), GetNextVersion(), aPrincipal, aScope,
                  aUpdateViaCache),
      mControlledClientsCounter(0),
      mDelayMultiplier(0),
      mUpdateState(NoUpdate),
      mCreationTime(PR_Now()),
      mCreationTimeStamp(TimeStamp::Now()),
      mLastUpdateTime(0),
      mUnregistered(false),
      mCorrupt(false) {
  MOZ_ASSERT_IF(ServiceWorkerParentInterceptEnabled(),
                XRE_GetProcessType() == GeckoProcessType_Default);
}

ServiceWorkerRegistrationInfo::~ServiceWorkerRegistrationInfo() {
  MOZ_DIAGNOSTIC_ASSERT(!IsControllingClients());
}

void ServiceWorkerRegistrationInfo::AddInstance(
    ServiceWorkerRegistrationListener* aInstance,
    const ServiceWorkerRegistrationDescriptor& aDescriptor) {
  MOZ_DIAGNOSTIC_ASSERT(aInstance);
  MOZ_ASSERT(!mInstanceList.Contains(aInstance));
  MOZ_DIAGNOSTIC_ASSERT(aDescriptor.Id() == mDescriptor.Id());
  MOZ_DIAGNOSTIC_ASSERT(aDescriptor.PrincipalInfo() ==
                        mDescriptor.PrincipalInfo());
  MOZ_DIAGNOSTIC_ASSERT(aDescriptor.Scope() == mDescriptor.Scope());
  MOZ_DIAGNOSTIC_ASSERT(aDescriptor.Version() <= mDescriptor.Version());
  uint64_t lastVersion = aDescriptor.Version();
  for (auto& entry : mVersionList) {
    if (lastVersion > entry->mDescriptor.Version()) {
      continue;
    }
    lastVersion = entry->mDescriptor.Version();
    aInstance->UpdateState(entry->mDescriptor);
  }
  // Note, the mDescriptor may be contained in the version list.  Since the
  // version list is aged out, though, it may also not be in the version list.
  // So always check for the mDescriptor update here.
  if (lastVersion < mDescriptor.Version()) {
    aInstance->UpdateState(mDescriptor);
  }
  mInstanceList.AppendElement(aInstance);
}

void ServiceWorkerRegistrationInfo::RemoveInstance(
    ServiceWorkerRegistrationListener* aInstance) {
  MOZ_DIAGNOSTIC_ASSERT(aInstance);
  DebugOnly<bool> removed = mInstanceList.RemoveElement(aInstance);
  MOZ_ASSERT(removed);
}

const nsCString& ServiceWorkerRegistrationInfo::Scope() const {
  return mDescriptor.Scope();
}

nsIPrincipal* ServiceWorkerRegistrationInfo::Principal() const {
  return mPrincipal;
}

bool ServiceWorkerRegistrationInfo::IsUnregistered() const {
  return mUnregistered;
}

void ServiceWorkerRegistrationInfo::SetUnregistered() {
#ifdef DEBUG
  MOZ_ASSERT(!mUnregistered);

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);

  RefPtr<ServiceWorkerRegistrationInfo> registration =
      swm->GetRegistration(Principal(), Scope());
  MOZ_ASSERT(registration != this);
#endif

  mUnregistered = true;
}

NS_IMPL_ISUPPORTS(ServiceWorkerRegistrationInfo,
                  nsIServiceWorkerRegistrationInfo)

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetPrincipal(nsIPrincipal** aPrincipal) {
  MOZ_ASSERT(NS_IsMainThread());
  NS_ADDREF(*aPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetScope(nsAString& aScope) {
  MOZ_ASSERT(NS_IsMainThread());
  CopyUTF8toUTF16(Scope(), aScope);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetScriptSpec(nsAString& aScriptSpec) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ServiceWorkerInfo> newest = NewestIncludingEvaluating();
  if (newest) {
    CopyUTF8toUTF16(newest->ScriptSpec(), aScriptSpec);
  }
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetUpdateViaCache(uint16_t* aUpdateViaCache) {
  *aUpdateViaCache = static_cast<uint16_t>(GetUpdateViaCache());
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetLastUpdateTime(PRTime* _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(_retval);
  *_retval = mLastUpdateTime;
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetEvaluatingWorker(
    nsIServiceWorkerInfo** aResult) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ServiceWorkerInfo> info = mEvaluatingWorker;
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetInstallingWorker(
    nsIServiceWorkerInfo** aResult) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ServiceWorkerInfo> info = mInstallingWorker;
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetWaitingWorker(
    nsIServiceWorkerInfo** aResult) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ServiceWorkerInfo> info = mWaitingWorker;
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetActiveWorker(nsIServiceWorkerInfo** aResult) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<ServiceWorkerInfo> info = mActiveWorker;
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::GetWorkerByID(uint64_t aID,
                                             nsIServiceWorkerInfo** aResult) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aResult);

  RefPtr<ServiceWorkerInfo> info = GetServiceWorkerInfoById(aID);
  // It is ok to return null for a missing service worker info.
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::AddListener(
    nsIServiceWorkerRegistrationInfoListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aListener || mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.AppendElement(aListener);

  return NS_OK;
}

NS_IMETHODIMP
ServiceWorkerRegistrationInfo::RemoveListener(
    nsIServiceWorkerRegistrationInfoListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!aListener || !mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.RemoveElement(aListener);

  return NS_OK;
}

already_AddRefed<ServiceWorkerInfo>
ServiceWorkerRegistrationInfo::GetServiceWorkerInfoById(uint64_t aId) {
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

void ServiceWorkerRegistrationInfo::TryToActivateAsync(
    TryToActivateCallback&& aCallback) {
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(
      NewRunnableMethod<StoreCopyPassByRRef<TryToActivateCallback>>(
          "ServiceWorkerRegistrationInfo::TryToActivate", this,
          &ServiceWorkerRegistrationInfo::TryToActivate,
          std::move(aCallback))));
}

/*
 * TryToActivate should not be called directly, use TryToActivateAsync instead.
 */
void ServiceWorkerRegistrationInfo::TryToActivate(
    TryToActivateCallback&& aCallback) {
  MOZ_ASSERT(NS_IsMainThread());
  bool controlling = IsControllingClients();
  bool skipWaiting = mWaitingWorker && mWaitingWorker->SkipWaitingFlag();
  bool idle = IsIdle();
  if (idle && (!controlling || skipWaiting)) {
    Activate();
  }

  if (aCallback) {
    aCallback();
  }
}

void ServiceWorkerRegistrationInfo::Activate() {
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
  RefPtr<LifeCycleEventCallback> callback =
      new ContinueActivateRunnable(handle);

  ServiceWorkerPrivate* workerPrivate = mActiveWorker->WorkerPrivate();
  MOZ_ASSERT(workerPrivate);
  nsresult rv = workerPrivate->SendLifeCycleEvent(NS_LITERAL_STRING("activate"),
                                                  callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    nsCOMPtr<nsIRunnable> failRunnable = NewRunnableMethod<bool>(
        "dom::ServiceWorkerRegistrationInfo::FinishActivate", this,
        &ServiceWorkerRegistrationInfo::FinishActivate, false /* success */);
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(failRunnable.forget()));
    return;
  }
}

void ServiceWorkerRegistrationInfo::FinishActivate(bool aSuccess) {
  if (mUnregistered || !mActiveWorker ||
      mActiveWorker->State() != ServiceWorkerState::Activating) {
    return;
  }

  // Activation never fails, so aSuccess is ignored.
  mActiveWorker->UpdateState(ServiceWorkerState::Activated);
  mActiveWorker->UpdateActivatedTime();

  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // browser shutdown started during async activation completion step
    return;
  }
  swm->StoreRegistration(mPrincipal, this);
}

void ServiceWorkerRegistrationInfo::RefreshLastUpdateCheckTime() {
  MOZ_ASSERT(NS_IsMainThread());

  mLastUpdateTime =
      mCreationTime +
      static_cast<PRTime>(
          (TimeStamp::Now() - mCreationTimeStamp).ToMicroseconds());
  NotifyChromeRegistrationListeners();
}

bool ServiceWorkerRegistrationInfo::IsLastUpdateCheckTimeOverOneDay() const {
  MOZ_ASSERT(NS_IsMainThread());

  // For testing.
  if (Preferences::GetBool("dom.serviceWorkers.testUpdateOverOneDay")) {
    return true;
  }

  const int64_t kSecondsPerDay = 86400;
  const int64_t nowMicros =
      mCreationTime +
      static_cast<PRTime>(
          (TimeStamp::Now() - mCreationTimeStamp).ToMicroseconds());

  // now < mLastUpdateTime if the system time is reset between storing
  // and loading mLastUpdateTime from ServiceWorkerRegistrar.
  if (nowMicros < mLastUpdateTime ||
      (nowMicros - mLastUpdateTime) / PR_USEC_PER_SEC > kSecondsPerDay) {
    return true;
  }
  return false;
}

void ServiceWorkerRegistrationInfo::UpdateRegistrationState() {
  UpdateRegistrationState(mDescriptor.UpdateViaCache());
}

void ServiceWorkerRegistrationInfo::UpdateRegistrationState(
    ServiceWorkerUpdateViaCache aUpdateViaCache) {
  MOZ_ASSERT(NS_IsMainThread());

  TimeStamp oldest = TimeStamp::Now() - TimeDuration::FromSeconds(30);
  if (!mVersionList.IsEmpty() && mVersionList[0]->mTimeStamp < oldest) {
    nsTArray<UniquePtr<VersionEntry>> list;
    mVersionList.SwapElements(list);
    for (auto& entry : list) {
      if (entry->mTimeStamp >= oldest) {
        mVersionList.AppendElement(std::move(entry));
      }
    }
  }
  mVersionList.AppendElement(MakeUnique<VersionEntry>(mDescriptor));

  // We are going to modify the descriptor, so increase its version number.
  mDescriptor.SetVersion(GetNextVersion());

  // Note, this also sets the new version number on the ServiceWorkerInfo
  // objects before we copy over their updated descriptors.
  mDescriptor.SetWorkers(mInstallingWorker, mWaitingWorker, mActiveWorker);

  mDescriptor.SetUpdateViaCache(aUpdateViaCache);

  nsTObserverArray<ServiceWorkerRegistrationListener*>::ForwardIterator it(
      mInstanceList);
  while (it.HasMore()) {
    RefPtr<ServiceWorkerRegistrationListener> target = it.GetNext();
    target->UpdateState(mDescriptor);
  }
}

void ServiceWorkerRegistrationInfo::NotifyChromeRegistrationListeners() {
  nsTArray<nsCOMPtr<nsIServiceWorkerRegistrationInfoListener>> listeners(
      mListeners.Clone());
  for (size_t index = 0; index < listeners.Length(); ++index) {
    listeners[index]->OnChange();
  }
}

void ServiceWorkerRegistrationInfo::MaybeScheduleTimeCheckAndUpdate() {
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

void ServiceWorkerRegistrationInfo::MaybeScheduleUpdate() {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (!swm) {
    // shutting down, do nothing
    return;
  }

  mUpdateState = NeedUpdate;

  swm->ScheduleUpdateTimer(mPrincipal, Scope());
}

bool ServiceWorkerRegistrationInfo::CheckAndClearIfUpdateNeeded() {
  MOZ_ASSERT(NS_IsMainThread());

  bool result =
      mUpdateState == NeedUpdate || (mUpdateState == NeedTimeCheckAndUpdate &&
                                     IsLastUpdateCheckTimeOverOneDay());

  mUpdateState = NoUpdate;

  return result;
}

ServiceWorkerInfo* ServiceWorkerRegistrationInfo::GetEvaluating() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mEvaluatingWorker;
}

ServiceWorkerInfo* ServiceWorkerRegistrationInfo::GetInstalling() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mInstallingWorker;
}

ServiceWorkerInfo* ServiceWorkerRegistrationInfo::GetWaiting() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mWaitingWorker;
}

ServiceWorkerInfo* ServiceWorkerRegistrationInfo::GetActive() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mActiveWorker;
}

ServiceWorkerInfo* ServiceWorkerRegistrationInfo::GetByDescriptor(
    const ServiceWorkerDescriptor& aDescriptor) const {
  if (mActiveWorker && mActiveWorker->Descriptor().Matches(aDescriptor)) {
    return mActiveWorker;
  }
  if (mWaitingWorker && mWaitingWorker->Descriptor().Matches(aDescriptor)) {
    return mWaitingWorker;
  }
  if (mInstallingWorker &&
      mInstallingWorker->Descriptor().Matches(aDescriptor)) {
    return mInstallingWorker;
  }
  if (mEvaluatingWorker &&
      mEvaluatingWorker->Descriptor().Matches(aDescriptor)) {
    return mEvaluatingWorker;
  }
  return nullptr;
}

void ServiceWorkerRegistrationInfo::SetEvaluating(
    ServiceWorkerInfo* aServiceWorker) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aServiceWorker);
  MOZ_ASSERT(!mEvaluatingWorker);
  MOZ_ASSERT(!mInstallingWorker);
  MOZ_ASSERT(mWaitingWorker != aServiceWorker);
  MOZ_ASSERT(mActiveWorker != aServiceWorker);

  mEvaluatingWorker = aServiceWorker;

  // We don't call UpdateRegistrationState() here because the evaluating worker
  // is currently not exposed to content on the registration, so calling it here
  // would produce redundant IPC traffic.
  NotifyChromeRegistrationListeners();
}

void ServiceWorkerRegistrationInfo::ClearEvaluating() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mEvaluatingWorker) {
    return;
  }

  mEvaluatingWorker->UpdateState(ServiceWorkerState::Redundant);
  // We don't update the redundant time for the sw here, since we've not expose
  // evalutingWorker yet.
  mEvaluatingWorker = nullptr;

  // As for SetEvaluating, UpdateRegistrationState() does not need to be called.
  NotifyChromeRegistrationListeners();
}

void ServiceWorkerRegistrationInfo::ClearInstalling() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mInstallingWorker) {
    return;
  }

  RefPtr<ServiceWorkerInfo> installing = std::move(mInstallingWorker);
  installing->UpdateState(ServiceWorkerState::Redundant);
  installing->UpdateRedundantTime();

  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();
}

void ServiceWorkerRegistrationInfo::TransitionEvaluatingToInstalling() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mEvaluatingWorker);
  MOZ_ASSERT(!mInstallingWorker);

  mInstallingWorker = std::move(mEvaluatingWorker);
  mInstallingWorker->UpdateState(ServiceWorkerState::Installing);

  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();
}

void ServiceWorkerRegistrationInfo::TransitionInstallingToWaiting() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInstallingWorker);

  if (mWaitingWorker) {
    MOZ_ASSERT(mInstallingWorker->CacheName() != mWaitingWorker->CacheName());
    mWaitingWorker->UpdateState(ServiceWorkerState::Redundant);
    mWaitingWorker->UpdateRedundantTime();
  }

  mWaitingWorker = std::move(mInstallingWorker);
  mWaitingWorker->UpdateState(ServiceWorkerState::Installed);
  mWaitingWorker->UpdateInstalledTime();

  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();

  // TODO: When bug 1426401 is implemented we will need to call
  //       StoreRegistration() here to persist the waiting worker.
}

void ServiceWorkerRegistrationInfo::SetActive(
    ServiceWorkerInfo* aServiceWorker) {
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
  mActiveWorker->SetActivateStateUncheckedWithoutEvent(
      ServiceWorkerState::Activated);

  // We don't need to update activated time when we load registration from
  // registrar.
  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();
}

void ServiceWorkerRegistrationInfo::TransitionWaitingToActive() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mWaitingWorker);

  if (mActiveWorker) {
    MOZ_ASSERT(mWaitingWorker->CacheName() != mActiveWorker->CacheName());
    mActiveWorker->UpdateState(ServiceWorkerState::Redundant);
    mActiveWorker->UpdateRedundantTime();
  }

  // We are transitioning from waiting to active normally, so go to
  // the activating state.
  mActiveWorker = std::move(mWaitingWorker);
  mActiveWorker->UpdateState(ServiceWorkerState::Activating);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "ServiceWorkerRegistrationInfo::TransitionWaitingToActive", [] {
        RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
        if (swm) {
          swm->CheckPendingReadyPromises();
        }
      });
  MOZ_ALWAYS_SUCCEEDS(
      SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));

  UpdateRegistrationState();
  NotifyChromeRegistrationListeners();
}

bool ServiceWorkerRegistrationInfo::IsIdle() const {
  return !mActiveWorker || mActiveWorker->WorkerPrivate()->IsIdle();
}

ServiceWorkerUpdateViaCache ServiceWorkerRegistrationInfo::GetUpdateViaCache()
    const {
  return mDescriptor.UpdateViaCache();
}

void ServiceWorkerRegistrationInfo::SetUpdateViaCache(
    ServiceWorkerUpdateViaCache aUpdateViaCache) {
  UpdateRegistrationState(aUpdateViaCache);
}

int64_t ServiceWorkerRegistrationInfo::GetLastUpdateTime() const {
  return mLastUpdateTime;
}

void ServiceWorkerRegistrationInfo::SetLastUpdateTime(const int64_t aTime) {
  if (aTime == 0) {
    return;
  }

  mLastUpdateTime = aTime;
}

const ServiceWorkerRegistrationDescriptor&
ServiceWorkerRegistrationInfo::Descriptor() const {
  return mDescriptor;
}

uint64_t ServiceWorkerRegistrationInfo::Id() const { return mDescriptor.Id(); }

uint64_t ServiceWorkerRegistrationInfo::Version() const {
  return mDescriptor.Version();
}

uint32_t ServiceWorkerRegistrationInfo::GetUpdateDelay(
    const bool aWithMultiplier) {
  uint32_t delay = Preferences::GetInt("dom.serviceWorkers.update_delay", 1000);

  if (!aWithMultiplier) {
    return delay;
  }

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

void ServiceWorkerRegistrationInfo::FireUpdateFound() {
  nsTObserverArray<ServiceWorkerRegistrationListener*>::ForwardIterator it(
      mInstanceList);
  while (it.HasMore()) {
    RefPtr<ServiceWorkerRegistrationListener> target = it.GetNext();
    target->FireUpdateFound();
  }
}

void ServiceWorkerRegistrationInfo::NotifyCleared() {
  nsTObserverArray<ServiceWorkerRegistrationListener*>::ForwardIterator it(
      mInstanceList);
  while (it.HasMore()) {
    RefPtr<ServiceWorkerRegistrationListener> target = it.GetNext();
    target->RegistrationCleared();
  }
}

void ServiceWorkerRegistrationInfo::ClearWhenIdle() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsUnregistered());
  MOZ_ASSERT(!IsControllingClients());
  MOZ_ASSERT(!IsIdle(), "Already idle!");

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  MOZ_ASSERT(swm);

  swm->AddOrphanedRegistration(this);

  /**
   * Although a Service Worker will transition to idle many times during its
   * lifetime, the promise is only resolved once `GetIdlePromise` has been
   * called, populating the `MozPromiseHolder`. Additionally, this is the only
   * time this method will be called for the given ServiceWorker. This means we
   * will be notified to the transition we are interested in, and there are no
   * other callers to get confused.
   *
   * Note that because we are using `MozPromise`, our callback will be invoked
   * as a separate task, so there is a small potential for races in the event
   * code if things are still holding onto the ServiceWorker binding and using
   * `postMessage()` or other mechanisms to schedule new events on it, which
   * would make it non-idle. However, this is a race inherent in the spec which
   * does not deal with the reality of multiple threads in "Try Clear
   * Registration".
   */
  GetActive()->WorkerPrivate()->GetIdlePromise()->Then(
      GetCurrentThreadSerialEventTarget(), __func__,
      [self = RefPtr<ServiceWorkerRegistrationInfo>(this)](
          const GenericPromise::ResolveOrRejectValue& aResult) {
        MOZ_ASSERT(aResult.IsResolve());
        // This registration was already unregistered and not controlling
        // clients when `ClearWhenIdle` was called, so there should be no way
        // that more clients were acquired.
        MOZ_ASSERT(!self->IsControllingClients());
        MOZ_ASSERT(self->IsIdle());
        self->Clear();

        RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
        if (swm) {
          swm->RemoveOrphanedRegistration(self);
        }
      });
}

const nsID& ServiceWorkerRegistrationInfo::AgentClusterId() const {
  return mAgentClusterId;
}

// static
uint64_t ServiceWorkerRegistrationInfo::GetNextId() {
  MOZ_ASSERT(NS_IsMainThread());
  static uint64_t sNextId = 0;
  return ++sNextId;
}

// static
uint64_t ServiceWorkerRegistrationInfo::GetNextVersion() {
  MOZ_ASSERT(NS_IsMainThread());
  static uint64_t sNextVersion = 0;
  return ++sNextVersion;
}

void ServiceWorkerRegistrationInfo::ForEachWorker(
    void (*aFunc)(RefPtr<ServiceWorkerInfo>&)) {
  if (mEvaluatingWorker) {
    aFunc(mEvaluatingWorker);
  }

  if (mInstallingWorker) {
    aFunc(mInstallingWorker);
  }

  if (mWaitingWorker) {
    aFunc(mWaitingWorker);
  }

  if (mActiveWorker) {
    aFunc(mActiveWorker);
  }
}

}  // namespace dom
}  // namespace mozilla
