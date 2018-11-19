/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedWorkerService.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/SystemGroup.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

StaticMutex sSharedWorkerMutex;

// Raw pointer because SharedWorkerParent keeps this object alive.
SharedWorkerService* MOZ_NON_OWNING_REF sSharedWorkerService;

class GetOrCreateWorkerManagerRunnable final : public Runnable
{
public:
  GetOrCreateWorkerManagerRunnable(SharedWorkerParent* aActor,
                                   const SharedWorkerLoadInfo& aInfo)
    : Runnable("GetOrCreateWorkerManagerRunnable")
    , mBackgroundEventTarget(GetCurrentThreadEventTarget())
    , mActor(aActor)
    , mInfo(aInfo)
  {}

  NS_IMETHOD
  Run()
  {
    // The service is always available because it's kept alive by the actor.
    SharedWorkerService* service = SharedWorkerService::Get();
    MOZ_ASSERT(service);

    service->GetOrCreateWorkerManagerOnMainThread(mBackgroundEventTarget,
                                                  mActor, mInfo);

    return NS_OK;
  }

private:
  nsCOMPtr<nsIEventTarget> mBackgroundEventTarget;
  RefPtr<SharedWorkerParent> mActor;
  SharedWorkerLoadInfo mInfo;
};

class RemoveWorkerManagerRunnable final : public Runnable
{
public:
  RemoveWorkerManagerRunnable(SharedWorkerService* aService,
                              SharedWorkerManager* aManager)
    : Runnable("RemoveWorkerManagerRunnable")
    , mService(aService)
    , mManager(aManager)
  {
    MOZ_ASSERT(mService);
    MOZ_ASSERT(mManager);
  }

  NS_IMETHOD
  Run()
  {
    mService->RemoveWorkerManagerOnMainThread(mManager);
    return NS_OK;
  }

private:
  RefPtr<SharedWorkerService> mService;
  RefPtr<SharedWorkerManager> mManager;
};

class WorkerManagerCreatedRunnable final : public Runnable
{
public:
  WorkerManagerCreatedRunnable(SharedWorkerManager* aManager,
                               SharedWorkerParent* aActor)
    : Runnable("WorkerManagerCreatedRunnable")
    , mManager(aManager)
    , mActor(aActor)
  {}

  NS_IMETHOD
  Run()
  {
    AssertIsOnBackgroundThread();
    mManager->AddActor(mActor);
    mActor->ManagerCreated(mManager);
    return NS_OK;
  }

private:
  RefPtr<SharedWorkerManager> mManager;
  RefPtr<SharedWorkerParent> mActor;
};

class ErrorPropagationRunnable final : public Runnable
{
public:
  ErrorPropagationRunnable(SharedWorkerParent* aActor,
                           nsresult aError)
    : Runnable("ErrorPropagationRunnable")
    , mActor(aActor)
    , mError(aError)
  {}

  NS_IMETHOD
  Run()
  {
    AssertIsOnBackgroundThread();
    mActor->ErrorPropagation(mError);
    return NS_OK;
  }

private:
  RefPtr<SharedWorkerParent> mActor;
  nsresult mError;
};

}

/* static */ already_AddRefed<SharedWorkerService>
SharedWorkerService::GetOrCreate()
{
  AssertIsOnBackgroundThread();

  StaticMutexAutoLock lock(sSharedWorkerMutex);

  if (sSharedWorkerService) {
    RefPtr<SharedWorkerService> instance = sSharedWorkerService;
    return  instance.forget();
  }

  RefPtr<SharedWorkerService> instance = new SharedWorkerService();
  return instance.forget();
}

/* static */ SharedWorkerService*
SharedWorkerService::Get()
{
  StaticMutexAutoLock lock(sSharedWorkerMutex);

  MOZ_ASSERT(sSharedWorkerService);
  return sSharedWorkerService;
}

SharedWorkerService::SharedWorkerService()
{
  AssertIsOnBackgroundThread();

  MOZ_ASSERT(!sSharedWorkerService);
  sSharedWorkerService = this;
}

SharedWorkerService::~SharedWorkerService()
{
  StaticMutexAutoLock lock(sSharedWorkerMutex);

  MOZ_ASSERT(sSharedWorkerService == this);
  sSharedWorkerService = nullptr;
}

void
SharedWorkerService::GetOrCreateWorkerManager(SharedWorkerParent* aActor,
                                              const SharedWorkerLoadInfo& aInfo)
{
  AssertIsOnBackgroundThread();

  // The real check happens on main-thread.
  RefPtr<GetOrCreateWorkerManagerRunnable> r =
    new GetOrCreateWorkerManagerRunnable(aActor, aInfo);

  nsCOMPtr<nsIEventTarget> target = SystemGroup::EventTargetFor(TaskCategory::Other);
  nsresult rv = target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void
SharedWorkerService::GetOrCreateWorkerManagerOnMainThread(nsIEventTarget* aBackgroundEventTarget,
                                                          SharedWorkerParent* aActor,
                                                          const SharedWorkerLoadInfo& aInfo)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBackgroundEventTarget);
  MOZ_ASSERT(aActor);

  RefPtr<SharedWorkerManager> manager;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(aInfo.principalInfo(), &rv);
  if (NS_WARN_IF(!principal)) {
    ErrorPropagationOnMainThread(aBackgroundEventTarget, aActor, rv);
    return;
  }

  nsCOMPtr<nsIPrincipal> loadingPrincipal =
    PrincipalInfoToPrincipal(aInfo.loadingPrincipalInfo(), &rv);
  if (NS_WARN_IF(!principal)) {
    ErrorPropagationOnMainThread(aBackgroundEventTarget, aActor, rv);
    return;
  }

  // Let's see if there is already a SharedWorker to share.
  for (SharedWorkerManager* workerManager : mWorkerManagers) {
    if (workerManager->MatchOnMainThread(aInfo.domain(),
                                         aInfo.resolvedScriptURL(),
                                         aInfo.name(), loadingPrincipal)) {
      manager = workerManager;
      break;
    }
  }

  // Let's create a new one.
  if (!manager) {
    manager = new SharedWorkerManager(aInfo, principal, loadingPrincipal);

    rv = manager->CreateWorkerOnMainThread();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      ErrorPropagationOnMainThread(aBackgroundEventTarget, aActor, rv);
      return;
    }

    mWorkerManagers.AppendElement(manager);
  } else {
    // We are attaching the actor to an existing one.
    if (manager->IsSecureContext() != aInfo.isSecureContext()) {
      ErrorPropagationOnMainThread(aBackgroundEventTarget, aActor,
                                   NS_ERROR_DOM_SECURITY_ERR);
      return;
    }
  }

  // If the SharedWorker(Manager) already existed and was frozen, the existence
  // of a new, un-frozen actor should trigger the thawing of the SharedWorker.
  manager->ThawOnMainThread();

  manager->ConnectPortOnMainThread(aInfo.portIdentifier());

  RefPtr<WorkerManagerCreatedRunnable> r =
    new WorkerManagerCreatedRunnable(manager, aActor);
  aBackgroundEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void
SharedWorkerService::ErrorPropagationOnMainThread(nsIEventTarget* aBackgroundEventTarget,
                                                  SharedWorkerParent* aActor,
                                                  nsresult aError)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBackgroundEventTarget);
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(NS_FAILED(aError));

  RefPtr<ErrorPropagationRunnable> r =
    new ErrorPropagationRunnable(aActor, aError);
  aBackgroundEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void
SharedWorkerService::RemoveWorkerManager(SharedWorkerManager* aManager)
{
  AssertIsOnBackgroundThread();

  // We pass 'this' in order to be kept alive.
  RefPtr<RemoveWorkerManagerRunnable> r =
    new RemoveWorkerManagerRunnable(this, aManager);

  nsCOMPtr<nsIEventTarget> target = SystemGroup::EventTargetFor(TaskCategory::Other);
  nsresult rv = target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  Unused << NS_WARN_IF(NS_FAILED(rv));
}

void
SharedWorkerService::RemoveWorkerManagerOnMainThread(SharedWorkerManager* aManager)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mWorkerManagers.Contains(aManager));

  mWorkerManagers.RemoveElement(aManager);
}

} // dom namespace
} // mozilla namespace
