/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_serviceworkerregistrationinfo_h
#define mozilla_dom_serviceworkerregistrationinfo_h

#include <functional>

#include "mozilla/dom/IPCNavigationPreloadState.h"
#include "mozilla/dom/ServiceWorkerInfo.h"
#include "mozilla/dom/ServiceWorkerRegistrationBinding.h"
#include "mozilla/dom/ServiceWorkerRegistrationDescriptor.h"
#include "nsContentUtils.h"
#include "nsProxyRelease.h"
#include "nsTObserverArray.h"

namespace mozilla {
namespace dom {

class ServiceWorkerRegistrationListener;

class ServiceWorkerRegistrationInfo final
    : public nsIServiceWorkerRegistrationInfo {
  nsCOMPtr<nsIPrincipal> mPrincipal;
  ServiceWorkerRegistrationDescriptor mDescriptor;
  nsTArray<nsCOMPtr<nsIServiceWorkerRegistrationInfoListener>> mListeners;
  nsTObserverArray<ServiceWorkerRegistrationListener*> mInstanceList;

  struct VersionEntry {
    const ServiceWorkerRegistrationDescriptor mDescriptor;
    TimeStamp mTimeStamp;

    explicit VersionEntry(
        const ServiceWorkerRegistrationDescriptor& aDescriptor)
        : mDescriptor(aDescriptor), mTimeStamp(TimeStamp::Now()) {}
  };
  nsTArray<UniquePtr<VersionEntry>> mVersionList;

  const nsID mAgentClusterId = nsContentUtils::GenerateUUID();

  uint32_t mControlledClientsCounter;
  uint32_t mDelayMultiplier;

  enum { NoUpdate, NeedTimeCheckAndUpdate, NeedUpdate } mUpdateState;

  // Timestamp to track SWR's last update time
  PRTime mCreationTime;
  TimeStamp mCreationTimeStamp;
  // The time of update is 0, if SWR've never been updated yet.
  PRTime mLastUpdateTime;

  RefPtr<ServiceWorkerInfo> mEvaluatingWorker;
  RefPtr<ServiceWorkerInfo> mActiveWorker;
  RefPtr<ServiceWorkerInfo> mWaitingWorker;
  RefPtr<ServiceWorkerInfo> mInstallingWorker;

  virtual ~ServiceWorkerRegistrationInfo();

  // When unregister() is called on a registration, it is removed from the
  // "scope to registration map" but not immediately "cleared" (i.e. its workers
  // terminated, updated to the redundant state, etc.) because it may still be
  // controlling clients. It is marked as unregistered and when all controlled
  // clients go away, cleared. This way we can tell if a registration
  // is unregistered by querying the object itself rather than incurring a table
  // lookup (in the case when the registrations are passed around as pointers).
  bool mUnregistered;

  bool mCorrupt;

  IPCNavigationPreloadState mNavigationPreloadState;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEWORKERREGISTRATIONINFO

  using TryToActivateCallback = std::function<void()>;

  ServiceWorkerRegistrationInfo(
      const nsACString& aScope, nsIPrincipal* aPrincipal,
      ServiceWorkerUpdateViaCache aUpdateViaCache,
      IPCNavigationPreloadState&& aNavigationPreloadState);

  void AddInstance(ServiceWorkerRegistrationListener* aInstance,
                   const ServiceWorkerRegistrationDescriptor& aDescriptor);

  void RemoveInstance(ServiceWorkerRegistrationListener* aInstance);

  const nsCString& Scope() const;

  nsIPrincipal* Principal() const;

  bool IsUnregistered() const;

  void SetUnregistered();

  already_AddRefed<ServiceWorkerInfo> Newest() const {
    RefPtr<ServiceWorkerInfo> newest;
    if (mInstallingWorker) {
      newest = mInstallingWorker;
    } else if (mWaitingWorker) {
      newest = mWaitingWorker;
    } else {
      newest = mActiveWorker;
    }

    return newest.forget();
  }

  already_AddRefed<ServiceWorkerInfo> NewestIncludingEvaluating() const {
    if (mEvaluatingWorker) {
      RefPtr<ServiceWorkerInfo> newest = mEvaluatingWorker;
      return newest.forget();
    }
    return Newest();
  }

  already_AddRefed<ServiceWorkerInfo> GetServiceWorkerInfoById(uint64_t aId);

  void StartControllingClient() {
    ++mControlledClientsCounter;
    mDelayMultiplier = 0;
  }

  void StopControllingClient() {
    MOZ_ASSERT(mControlledClientsCounter);
    --mControlledClientsCounter;
  }

  bool IsControllingClients() const {
    return mActiveWorker && mControlledClientsCounter;
  }

  // As a side effect, this nullifies
  // `m{Evaluating,Installing,Waiting,Active}Worker`s.
  void ShutdownWorkers();

  void Clear();

  void ClearAsCorrupt();

  bool IsCorrupt() const;

  void TryToActivateAsync(TryToActivateCallback&& aCallback = nullptr);

  void TryToActivate(TryToActivateCallback&& aCallback);

  void Activate();

  void FinishActivate(bool aSuccess);

  void RefreshLastUpdateCheckTime();

  bool IsLastUpdateCheckTimeOverOneDay() const;

  void MaybeScheduleTimeCheckAndUpdate();

  void MaybeScheduleUpdate();

  bool CheckAndClearIfUpdateNeeded();

  ServiceWorkerInfo* GetEvaluating() const;

  ServiceWorkerInfo* GetInstalling() const;

  ServiceWorkerInfo* GetWaiting() const;

  ServiceWorkerInfo* GetActive() const;

  ServiceWorkerInfo* GetByDescriptor(
      const ServiceWorkerDescriptor& aDescriptor) const;

  // Set the given worker as the evaluating service worker.  The worker
  // state is not changed.
  void SetEvaluating(ServiceWorkerInfo* aServiceWorker);

  // Remove an existing evaluating worker, if present.  The worker will
  // be transitioned to the Redundant state.
  void ClearEvaluating();

  // Remove an existing installing worker, if present.  The worker will
  // be transitioned to the Redundant state.
  void ClearInstalling();

  // Transition the current evaluating worker to be the installing worker.  The
  // worker's state is update to Installing.
  void TransitionEvaluatingToInstalling();

  // Transition the current installing worker to be the waiting worker.  The
  // worker's state is updated to Installed.
  void TransitionInstallingToWaiting();

  // Override the current active worker.  This is used during browser
  // initialization to load persisted workers.  Its also used to propagate
  // active workers across child processes in e10s.  This second use will
  // go away once the ServiceWorkerManager moves to the parent process.
  // The worker is transitioned to the Activated state.
  void SetActive(ServiceWorkerInfo* aServiceWorker);

  // Transition the current waiting worker to be the new active worker.  The
  // worker is updated to the Activating state.
  void TransitionWaitingToActive();

  // Determine if the registration is actively performing work.
  bool IsIdle() const;

  ServiceWorkerUpdateViaCache GetUpdateViaCache() const;

  void SetUpdateViaCache(ServiceWorkerUpdateViaCache aUpdateViaCache);

  int64_t GetLastUpdateTime() const;

  void SetLastUpdateTime(const int64_t aTime);

  const ServiceWorkerRegistrationDescriptor& Descriptor() const;

  uint64_t Id() const;

  uint64_t Version() const;

  uint32_t GetUpdateDelay(const bool aWithMultiplier = true);

  void FireUpdateFound();

  void NotifyCleared();

  void ClearWhenIdle();

  const nsID& AgentClusterId() const;

  void SetNavigationPreloadEnabled(const bool& aEnabled);

  void SetNavigationPreloadHeader(const nsCString& aHeader);

  IPCNavigationPreloadState GetNavigationPreloadState() const;

 private:
  // Roughly equivalent to [[Update Registration State algorithm]]. Make sure
  // this is called *before* updating SW instances' state, otherwise they
  // may get CC-ed.
  void UpdateRegistrationState();

  void UpdateRegistrationState(ServiceWorkerUpdateViaCache aUpdateViaCache);

  // Used by devtools to track changes to the properties of
  // *nsIServiceWorkerRegistrationInfo*. Note, this doesn't necessarily need to
  // be in sync with the DOM registration objects, but it does need to be called
  // in the same task that changed |mInstallingWorker|, |mWaitingWorker| or
  // |mActiveWorker|.
  void NotifyChromeRegistrationListeners();

  static uint64_t GetNextId();

  static uint64_t GetNextVersion();

  // `aFunc`'s argument will be a reference to
  // `m{Evaluating,Installing,Waiting,Active}Worker` (not to copy of them).
  // Additionally, a null check will be performed for each worker before each
  // call to `aFunc`, so `aFunc` will always get a reference to a non-null
  // pointer.
  void ForEachWorker(void (*aFunc)(RefPtr<ServiceWorkerInfo>&));
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_serviceworkerregistrationinfo_h
