/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorkerService.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticMutex.h"
#include "nsProxyRelease.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

StaticMutex sSharedWorkerMutex;

// Raw pointer because SharedWorkerParent keeps this object alive, indirectly
// via SharedWorkerManagerHolder.
CheckedUnsafePtr<SharedWorkerService> sSharedWorkerService;

class GetOrCreateWorkerManagerRunnable final : public Runnable {
 public:
  GetOrCreateWorkerManagerRunnable(SharedWorkerService* aService,
                                   SharedWorkerParent* aActor,
                                   const RemoteWorkerData& aData,
                                   uint64_t aWindowID,
                                   const MessagePortIdentifier& aPortIdentifier)
      : Runnable("GetOrCreateWorkerManagerRunnable"),
        mBackgroundEventTarget(GetCurrentThreadEventTarget()),
        mService(aService),
        mActor(aActor),
        mData(aData),
        mWindowID(aWindowID),
        mPortIdentifier(aPortIdentifier) {}

  NS_IMETHOD
  Run() {
    mService->GetOrCreateWorkerManagerOnMainThread(
        mBackgroundEventTarget, mActor, mData, mWindowID, mPortIdentifier);

    return NS_OK;
  }

 private:
  nsCOMPtr<nsIEventTarget> mBackgroundEventTarget;
  RefPtr<SharedWorkerService> mService;
  RefPtr<SharedWorkerParent> mActor;
  RemoteWorkerData mData;
  uint64_t mWindowID;
  UniqueMessagePortId mPortIdentifier;
};

class WorkerManagerCreatedRunnable final : public Runnable {
 public:
  WorkerManagerCreatedRunnable(
      already_AddRefed<SharedWorkerManagerWrapper> aManagerWrapper,
      SharedWorkerParent* aActor, const RemoteWorkerData& aData,
      uint64_t aWindowID, UniqueMessagePortId& aPortIdentifier)
      : Runnable("WorkerManagerCreatedRunnable"),
        mManagerWrapper(aManagerWrapper),
        mActor(aActor),
        mData(aData),
        mWindowID(aWindowID),
        mPortIdentifier(std::move(aPortIdentifier)) {}

  NS_IMETHOD
  Run() {
    AssertIsOnBackgroundThread();

    if (NS_WARN_IF(!mManagerWrapper->Manager()->MaybeCreateRemoteWorker(
            mData, mWindowID, mPortIdentifier, mActor->OtherPid()))) {
      mActor->ErrorPropagation(NS_ERROR_FAILURE);
      return NS_OK;
    }

    mManagerWrapper->Manager()->AddActor(mActor);
    mActor->ManagerCreated(mManagerWrapper.forget());
    return NS_OK;
  }

 private:
  RefPtr<SharedWorkerManagerWrapper> mManagerWrapper;
  RefPtr<SharedWorkerParent> mActor;
  RemoteWorkerData mData;
  uint64_t mWindowID;
  UniqueMessagePortId mPortIdentifier;
};

class ErrorPropagationRunnable final : public Runnable {
 public:
  ErrorPropagationRunnable(SharedWorkerParent* aActor, nsresult aError)
      : Runnable("ErrorPropagationRunnable"), mActor(aActor), mError(aError) {}

  NS_IMETHOD
  Run() {
    AssertIsOnBackgroundThread();
    mActor->ErrorPropagation(mError);
    return NS_OK;
  }

 private:
  RefPtr<SharedWorkerParent> mActor;
  nsresult mError;
};

}  // namespace

/* static */
already_AddRefed<SharedWorkerService> SharedWorkerService::GetOrCreate() {
  AssertIsOnBackgroundThread();

  StaticMutexAutoLock lock(sSharedWorkerMutex);

  if (sSharedWorkerService) {
    RefPtr<SharedWorkerService> instance = sSharedWorkerService.get();
    return instance.forget();
  }

  RefPtr<SharedWorkerService> instance = new SharedWorkerService();
  return instance.forget();
}

/* static */
SharedWorkerService* SharedWorkerService::Get() {
  StaticMutexAutoLock lock(sSharedWorkerMutex);

  MOZ_ASSERT(sSharedWorkerService);
  return sSharedWorkerService;
}

SharedWorkerService::SharedWorkerService() {
  AssertIsOnBackgroundThread();

  MOZ_ASSERT(!sSharedWorkerService);
  sSharedWorkerService = this;
}

SharedWorkerService::~SharedWorkerService() {
  StaticMutexAutoLock lock(sSharedWorkerMutex);

  MOZ_ASSERT(sSharedWorkerService == this);
  sSharedWorkerService = nullptr;
}

void SharedWorkerService::GetOrCreateWorkerManager(
    SharedWorkerParent* aActor, const RemoteWorkerData& aData,
    uint64_t aWindowID, const MessagePortIdentifier& aPortIdentifier) {
  AssertIsOnBackgroundThread();

  // The real check happens on main-thread.
  RefPtr<GetOrCreateWorkerManagerRunnable> r =
      new GetOrCreateWorkerManagerRunnable(this, aActor, aData, aWindowID,
                                           aPortIdentifier);

  nsresult rv = SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void SharedWorkerService::GetOrCreateWorkerManagerOnMainThread(
    nsIEventTarget* aBackgroundEventTarget, SharedWorkerParent* aActor,
    const RemoteWorkerData& aData, uint64_t aWindowID,
    UniqueMessagePortId& aPortIdentifier) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBackgroundEventTarget);
  MOZ_ASSERT(aActor);

  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrincipal> storagePrincipal =
      PrincipalInfoToPrincipal(aData.storagePrincipalInfo(), &rv);
  if (NS_WARN_IF(!storagePrincipal)) {
    ErrorPropagationOnMainThread(aBackgroundEventTarget, aActor, rv);
    return;
  }

  nsCOMPtr<nsIPrincipal> loadingPrincipal =
      PrincipalInfoToPrincipal(aData.loadingPrincipalInfo(), &rv);
  if (NS_WARN_IF(!loadingPrincipal)) {
    ErrorPropagationOnMainThread(aBackgroundEventTarget, aActor, rv);
    return;
  }

  RefPtr<SharedWorkerManagerHolder> managerHolder;

  // Let's see if there is already a SharedWorker to share.
  nsCOMPtr<nsIURI> resolvedScriptURL =
      DeserializeURI(aData.resolvedScriptURL());
  for (SharedWorkerManager* workerManager : mWorkerManagers) {
    managerHolder = workerManager->MatchOnMainThread(
        this, aData.domain(), resolvedScriptURL, aData.name(), loadingPrincipal,
        BasePrincipal::Cast(storagePrincipal)->OriginAttributesRef());
    if (managerHolder) {
      break;
    }
  }

  // Let's create a new one.
  if (!managerHolder) {
    managerHolder = SharedWorkerManager::Create(
        this, aBackgroundEventTarget, aData, loadingPrincipal,
        BasePrincipal::Cast(storagePrincipal)->OriginAttributesRef());

    mWorkerManagers.AppendElement(managerHolder->Manager());
  } else {
    // We are attaching the actor to an existing one.
    if (managerHolder->Manager()->IsSecureContext() !=
        aData.isSecureContext()) {
      ErrorPropagationOnMainThread(aBackgroundEventTarget, aActor,
                                   NS_ERROR_DOM_SECURITY_ERR);
      return;
    }
  }

  RefPtr<SharedWorkerManagerWrapper> wrapper =
      new SharedWorkerManagerWrapper(managerHolder.forget());

  RefPtr<WorkerManagerCreatedRunnable> r = new WorkerManagerCreatedRunnable(
      wrapper.forget(), aActor, aData, aWindowID, aPortIdentifier);
  aBackgroundEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void SharedWorkerService::ErrorPropagationOnMainThread(
    nsIEventTarget* aBackgroundEventTarget, SharedWorkerParent* aActor,
    nsresult aError) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBackgroundEventTarget);
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(NS_FAILED(aError));

  RefPtr<ErrorPropagationRunnable> r =
      new ErrorPropagationRunnable(aActor, aError);
  aBackgroundEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void SharedWorkerService::RemoveWorkerManagerOnMainThread(
    SharedWorkerManager* aManager) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mWorkerManagers.Contains(aManager));

  mWorkerManagers.RemoveElement(aManager);
}

}  // namespace dom
}  // namespace mozilla
