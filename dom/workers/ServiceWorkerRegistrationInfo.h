/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_serviceworkerregistrationinfo_h
#define mozilla_dom_workers_serviceworkerregistrationinfo_h

#include "mozilla/dom/workers/ServiceWorkerInfo.h"

namespace mozilla {
namespace dom {
namespace workers {

class ServiceWorkerRegistrationInfo final
  : public nsIServiceWorkerRegistrationInfo
{
  uint32_t mControlledDocumentsCounter;

  enum
  {
    NoUpdate,
    NeedTimeCheckAndUpdate,
    NeedUpdate
  } mUpdateState;

  uint64_t mLastUpdateCheckTime;

  RefPtr<ServiceWorkerInfo> mEvaluatingWorker;
  RefPtr<ServiceWorkerInfo> mActiveWorker;
  RefPtr<ServiceWorkerInfo> mWaitingWorker;
  RefPtr<ServiceWorkerInfo> mInstallingWorker;

  virtual ~ServiceWorkerRegistrationInfo();

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVICEWORKERREGISTRATIONINFO

  const nsCString mScope;

  nsCOMPtr<nsIPrincipal> mPrincipal;

  nsTArray<nsCOMPtr<nsIServiceWorkerRegistrationInfoListener>> mListeners;

  // When unregister() is called on a registration, it is not immediately
  // removed since documents may be controlled. It is marked as
  // pendingUninstall and when all controlling documents go away, removed.
  bool mPendingUninstall;

  ServiceWorkerRegistrationInfo(const nsACString& aScope,
                                nsIPrincipal* aPrincipal);

  already_AddRefed<ServiceWorkerInfo>
  Newest() const
  {
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

  already_AddRefed<ServiceWorkerInfo>
  GetServiceWorkerInfoById(uint64_t aId);

  void
  StartControllingADocument()
  {
    ++mControlledDocumentsCounter;
  }

  void
  StopControllingADocument()
  {
    MOZ_ASSERT(mControlledDocumentsCounter);
    --mControlledDocumentsCounter;
  }

  bool
  IsControllingDocuments() const
  {
    return mActiveWorker && mControlledDocumentsCounter;
  }

  void
  Clear();

  void
  TryToActivateAsync();

  void
  TryToActivate();

  void
  Activate();

  void
  FinishActivate(bool aSuccess);

  void
  RefreshLastUpdateCheckTime();

  bool
  IsLastUpdateCheckTimeOverOneDay() const;

  void
  NotifyListenersOnChange(WhichServiceWorker aChangedWorkers);

  void
  MaybeScheduleTimeCheckAndUpdate();

  void
  MaybeScheduleUpdate();

  bool
  CheckAndClearIfUpdateNeeded();

  ServiceWorkerInfo*
  GetEvaluating() const;

  ServiceWorkerInfo*
  GetInstalling() const;

  ServiceWorkerInfo*
  GetWaiting() const;

  ServiceWorkerInfo*
  GetActive() const;

  // Set the given worker as the evaluating service worker.  The worker
  // state is not changed.
  void
  SetEvaluating(ServiceWorkerInfo* aServiceWorker);

  // Remove an existing evaluating worker, if present.  The worker will
  // be transitioned to the Redundant state.
  void
  ClearEvaluating();

  // Remove an existing installing worker, if present.  The worker will
  // be transitioned to the Redundant state.
  void
  ClearInstalling();

  // Transition the current evaluating worker to be the installing worker.  The
  // worker's state is update to Installing.
  void
  TransitionEvaluatingToInstalling();

  // Transition the current installing worker to be the waiting worker.  The
  // worker's state is updated to Installed.
  void
  TransitionInstallingToWaiting();

  // Override the current active worker.  This is used during browser
  // initialization to load persisted workers.  Its also used to propagate
  // active workers across child processes in e10s.  This second use will
  // go away once the ServiceWorkerManager moves to the parent process.
  // The worker is transitioned to the Activated state.
  void
  SetActive(ServiceWorkerInfo* aServiceWorker);

  // Transition the current waiting worker to be the new active worker.  The
  // worker is updated to the Activating state.
  void
  TransitionWaitingToActive();
};

} // namespace workers
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_workers_serviceworkerregistrationinfo_h
