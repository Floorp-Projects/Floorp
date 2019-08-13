/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerManager.h"

#include <utility>

#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/RemoteWorkerParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "nsCOMPtr.h"
#include "nsIXULRuntime.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "RemoteWorkerServiceParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

// Raw pointer because this object is kept alive by RemoteWorkerServiceParent
// actors.
RemoteWorkerManager* sRemoteWorkerManager;

bool IsServiceWorker(const RemoteWorkerData& aData) {
  return aData.serviceWorkerData().type() ==
         OptionalServiceWorkerData::TServiceWorkerData;
}

}  // namespace

/* static */
already_AddRefed<RemoteWorkerManager> RemoteWorkerManager::GetOrCreate() {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  if (!sRemoteWorkerManager) {
    sRemoteWorkerManager = new RemoteWorkerManager();
  }

  RefPtr<RemoteWorkerManager> rwm = sRemoteWorkerManager;
  return rwm.forget();
}

RemoteWorkerManager::RemoteWorkerManager() : mParentActor(nullptr) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!sRemoteWorkerManager);
}

RemoteWorkerManager::~RemoteWorkerManager() {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(sRemoteWorkerManager == this);
  sRemoteWorkerManager = nullptr;
}

void RemoteWorkerManager::RegisterActor(RemoteWorkerServiceParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (!BackgroundParent::IsOtherProcessActor(aActor->Manager())) {
    MOZ_ASSERT(!mParentActor);
    mParentActor = aActor;
    MOZ_ASSERT(mPendings.IsEmpty());
    return;
  }

  MOZ_ASSERT(!mChildActors.Contains(aActor));
  mChildActors.AppendElement(aActor);

  if (!mPendings.IsEmpty()) {
    // Flush pending launching.
    for (const Pending& p : mPendings) {
      LaunchInternal(p.mController, aActor, p.mData);
    }

    mPendings.Clear();

    // We don't need to keep this manager alive manually. The Actor is doing it
    // for us.
    Release();
  }
}

void RemoteWorkerManager::UnregisterActor(RemoteWorkerServiceParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aActor);

  if (aActor == mParentActor) {
    mParentActor = nullptr;
  } else {
    MOZ_ASSERT(mChildActors.Contains(aActor));
    mChildActors.RemoveElement(aActor);
  }
}

void RemoteWorkerManager::Launch(RemoteWorkerController* aController,
                                 const RemoteWorkerData& aData,
                                 base::ProcessId aProcessId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  RemoteWorkerServiceParent* targetActor = SelectTargetActor(aData, aProcessId);

  // If there is not an available actor, let's store the data, and let's spawn a
  // new process.
  if (!targetActor) {
    // If this is the first time we have a pending launching, we must keep alive
    // the manager.
    if (mPendings.IsEmpty()) {
      AddRef();
    }

    Pending* pending = mPendings.AppendElement();
    pending->mController = aController;
    pending->mData = aData;

    LaunchNewContentProcess();
    return;
  }

  /**
   * If a target actor for a Service Worker has been selected, the actor has
   * already been registered with the corresponding ContentParent (see
   * `SelectTargetActorForServiceWorker()`).
   */
  LaunchInternal(aController, targetActor, aData, IsServiceWorker(aData));
}

void RemoteWorkerManager::LaunchInternal(
    RemoteWorkerController* aController,
    RemoteWorkerServiceParent* aTargetActor, const RemoteWorkerData& aData,
    bool aRemoteWorkerAlreadyRegistered) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(aController);
  MOZ_ASSERT(aTargetActor);
  MOZ_ASSERT(aTargetActor == mParentActor ||
             mChildActors.Contains(aTargetActor));

  RemoteWorkerParent* workerActor = static_cast<RemoteWorkerParent*>(
      aTargetActor->Manager()->SendPRemoteWorkerConstructor(aData));
  if (NS_WARN_IF(!workerActor)) {
    AsyncCreationFailed(aController);
    return;
  }

  workerActor->Initialize(aRemoteWorkerAlreadyRegistered);

  // This makes the link better the 2 actors.
  aController->SetWorkerActor(workerActor);
  workerActor->SetController(aController);
}

void RemoteWorkerManager::AsyncCreationFailed(
    RemoteWorkerController* aController) {
  RefPtr<RemoteWorkerController> controller = aController;
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("RemoteWorkerManager::AsyncCreationFailed",
                             [controller]() { controller->CreationFailed(); });

  NS_DispatchToCurrentThread(r.forget());
}

/**
 * Service Workers can spawn even when their registering page/script isn't
 * active (e.g. push notifications), so we don't attempt to spawn the worker
 * in its registering script's process. We search linearly and choose the
 * search's starting position randomly.
 *
 * Spawning the workers in a random process makes the process selection criteria
 * a little tricky, as a candidate process may imminently shutdown due to a
 * remove worker actor unregistering
 * (see `ContentParent::UnregisterRemoveWorkerActor`).
 *
 * In `ContentParent::MaybeAsyncSendShutDownMessage` we only dispatch a runnable
 * to call `ContentParent::ShutDownProcess` if there are no registered remote
 * worker actors, and we ensure that the check for the number of registered
 * actors and the dispatching of the runnable are atomic. That happens on the
 * main thread, so here on the background thread,  while
 * `ContentParent::mRemoteWorkerActorData` is locked, if `mCount` > 0, we can
 * register a remote worker actor "early" and guarantee that the corresponding
 * content process will not shutdown.
 */
RemoteWorkerServiceParent*
RemoteWorkerManager::SelectTargetActorForServiceWorker() const {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mChildActors.IsEmpty());

  nsTArray<RefPtr<ContentParent>> contentParents;

  auto scopeExit = MakeScopeExit([&] {
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        __func__,
        [doomed = std::move(contentParents)]() mutable { doomed.Clear(); });

    SystemGroup::Dispatch(TaskCategory::Other, r.forget());
  });

  uint32_t random = uint32_t(rand() % mChildActors.Length());
  uint32_t i = random;

  do {
    auto actor = mChildActors[i];
    PBackgroundParent* bgParent = actor->Manager();
    MOZ_ASSERT(bgParent);

    RefPtr<ContentParent> contentParent =
        BackgroundParent::GetContentParent(bgParent);

    auto scopeExit = MakeScopeExit(
        [&] { contentParents.AppendElement(std::move(contentParent)); });

    if (contentParent->GetRemoteType().EqualsLiteral(DEFAULT_REMOTE_TYPE)) {
      auto lock = contentParent->mRemoteWorkerActorData.Lock();

      if (lock->mCount || !lock->mShutdownStarted) {
        ++lock->mCount;
        return actor;
      }
    }

    i = (++i % mChildActors.Length());
  } while (i != random);

  return nullptr;
}

RemoteWorkerServiceParent* RemoteWorkerManager::SelectTargetActor(
    const RemoteWorkerData& aData, base::ProcessId aProcessId) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  // System principal workers should run on the parent process.
  if (aData.principalInfo().type() == PrincipalInfo::TSystemPrincipalInfo) {
    MOZ_ASSERT(mParentActor);
    return mParentActor;
  }

  // If e10s is off, use the parent process.
  if (!BrowserTabsRemoteAutostart()) {
    MOZ_ASSERT(mParentActor);
    return mParentActor;
  }

  // We shouldn't have to worry about content-principal parent-process workers.
  MOZ_ASSERT(aProcessId != base::GetCurrentProcId());

  if (mChildActors.IsEmpty()) {
    return nullptr;
  }

  if (IsServiceWorker(aData)) {
    return SelectTargetActorForServiceWorker();
  }

  for (RemoteWorkerServiceParent* actor : mChildActors) {
    // Let's execute the RemoteWorker on the same process.
    if (actor->OtherPid() == aProcessId) {
      return actor;
    }
  }

  // Let's choose an actor, randomly.
  uint32_t id = uint32_t(rand()) % mChildActors.Length();
  return mChildActors[id];
}

void RemoteWorkerManager::LaunchNewContentProcess() {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  // This runnable will spawn a new process if it doesn't exist yet.
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("LaunchNewContentProcess", []() {
        RefPtr<ContentParent> unused =
            ContentParent::GetNewOrUsedBrowserProcess(
                nullptr, NS_LITERAL_STRING(DEFAULT_REMOTE_TYPE));
      });

  nsCOMPtr<nsIEventTarget> target =
      SystemGroup::EventTargetFor(TaskCategory::Other);
  target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

}  // namespace dom
}  // namespace mozilla
