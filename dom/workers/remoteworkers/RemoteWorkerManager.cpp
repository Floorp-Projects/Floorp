/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerManager.h"

#include <utility>

#include "mozilla/SchedulerGroup.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/RemoteWorkerParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
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

// Respecting COOP and COEP requires processing headers in the parent
// process in order to choose an appropriate content process, but the
// workers' ScriptLoader processes headers in content processes. An
// intermediary step that provides security guarantees is to simply never
// allow SharedWorkers and ServiceWorkers to exist in a COOP+COEP process.
// The ultimate goal is to allow these worker types to be put in such
// processes based on their script response headers.
// https://bugzilla.mozilla.org/show_bug.cgi?id=1595206
bool IsServiceWorkerRemoteType(const nsAString& aRemoteType) {
  return IsWebRemoteType(aRemoteType) && !IsWebCoopCoepRemoteType(aRemoteType);
}

void TransmitPermissionsAndBlobURLsForPrincipalInfo(
    ContentParent* aContentParent, const PrincipalInfo& aPrincipalInfo) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aContentParent);

  nsCOMPtr<nsIPrincipal> principal = PrincipalInfoToPrincipal(aPrincipalInfo);

  aContentParent->TransmitBlobURLsForPrincipal(principal);

  MOZ_ALWAYS_SUCCEEDS(
      aContentParent->TransmitPermissionsForPrincipal(principal));
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

  nsTArray<Pending> unlaunched;

  RefPtr<ContentParent> contentParent =
      BackgroundParent::GetContentParent(aActor->Manager());
  auto scopeExit =
      MakeScopeExit([&] { NS_ReleaseOnMainThread(contentParent.forget()); });
  const auto& remoteType = contentParent->GetRemoteType();

  if (!mPendings.IsEmpty()) {
    // Flush pending launching.
    for (Pending& p : mPendings) {
      if (p.mController->IsTerminated()) {
        continue;
      }

      if (IsServiceWorker(p.mData) && !IsServiceWorkerRemoteType(remoteType)) {
        unlaunched.AppendElement(std::move(p));
        continue;
      }

      LaunchInternal(p.mController, aActor, p.mData);
    }

    std::swap(mPendings, unlaunched);

    // AddRef is called when the first Pending object is added to mPendings, so
    // the balancing Release is called when the last Pending object is removed.
    // RemoteWorkerServiceParents will hold strong references to
    // RemoteWorkerManager.
    if (mPendings.IsEmpty()) {
      Release();
    }
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

    LaunchNewContentProcess(aData);
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

template <typename Callback>
void RemoteWorkerManager::ForEachActor(Callback&& aCallback) const {
  AssertIsOnBackgroundThread();

  const auto length = mChildActors.Length();
  const auto end = static_cast<uint32_t>(rand()) % length;
  uint32_t i = end;

  nsTArray<RefPtr<ContentParent>> proxyReleaseArray;

  do {
    MOZ_ASSERT(i < mChildActors.Length());
    RemoteWorkerServiceParent* actor = mChildActors[i];

    RefPtr<ContentParent> contentParent =
        BackgroundParent::GetContentParent(actor->Manager());

    auto scopeExit = MakeScopeExit(
        [&]() { proxyReleaseArray.AppendElement(std::move(contentParent)); });

    if (!aCallback(actor, std::move(contentParent))) {
      break;
    }

    i = (i + 1) % length;
  } while (i != end);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [proxyReleaseArray = std::move(proxyReleaseArray)] {});

  MOZ_ALWAYS_SUCCEEDS(
      SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));
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
RemoteWorkerManager::SelectTargetActorForServiceWorker(
    const RemoteWorkerData& aData) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mChildActors.IsEmpty());
  MOZ_ASSERT(IsServiceWorker(aData));

  RemoteWorkerServiceParent* actor = nullptr;

  ForEachActor([&](RemoteWorkerServiceParent* aActor,
                   RefPtr<ContentParent>&& aContentParent) {
    const auto& remoteType = aContentParent->GetRemoteType();

    if (IsServiceWorkerRemoteType(remoteType)) {
      auto lock = aContentParent->mRemoteWorkerActorData.Lock();

      if (lock->mCount || !lock->mShutdownStarted) {
        ++lock->mCount;

        // This won't cause any race conditions because the content process
        // should wait for the permissions to be received before executing the
        // Service Worker.
        nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
            __func__, [contentParent = std::move(aContentParent),
                       principalInfo = aData.principalInfo()] {
              TransmitPermissionsAndBlobURLsForPrincipalInfo(contentParent,
                                                             principalInfo);
            });

        MOZ_ALWAYS_SUCCEEDS(
            SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));

        actor = aActor;
        return false;
      }
    }

    MOZ_ASSERT(!actor);
    return true;
  });

  return actor;
}

RemoteWorkerServiceParent*
RemoteWorkerManager::SelectTargetActorForSharedWorker(
    base::ProcessId aProcessId) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mChildActors.IsEmpty());

  RemoteWorkerServiceParent* actor = nullptr;

  ForEachActor([&](RemoteWorkerServiceParent* aActor,
                   RefPtr<ContentParent>&& aContentParent) {
    if (IsWebCoopCoepRemoteType(aContentParent->GetRemoteType())) {
      return true;
    }

    if (aActor->OtherPid() == aProcessId) {
      actor = aActor;
      return false;
    }

    if (!actor) {
      actor = aActor;
    }

    return true;
  });

  return actor;
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

  return IsServiceWorker(aData) ? SelectTargetActorForServiceWorker(aData)
                                : SelectTargetActorForSharedWorker(aProcessId);
}

void RemoteWorkerManager::LaunchNewContentProcess(
    const RemoteWorkerData& aData) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  nsCOMPtr<nsISerialEventTarget> bgEventTarget =
      GetCurrentThreadSerialEventTarget();

  using CallbackParamType = ContentParent::LaunchPromise::ResolveOrRejectValue;

  // A new content process must be requested on the main thread. On success,
  // the success callback will also run on the main thread. On failure, however,
  // the failure callback must be run on the background thread - it uses
  // RemoteWorkerManager, and RemoteWorkerManager isn't threadsafe, so the
  // promise callback will just dispatch the "real" failure callback to the
  // background thread.
  auto processLaunchCallback = [isServiceWorker = IsServiceWorker(aData),
                                principalInfo = aData.principalInfo(),
                                bgEventTarget = std::move(bgEventTarget),
                                self = RefPtr<RemoteWorkerManager>(this)](
                                   const CallbackParamType& aValue) mutable {
    if (aValue.IsResolve()) {
      if (isServiceWorker) {
        TransmitPermissionsAndBlobURLsForPrincipalInfo(aValue.ResolveValue(),
                                                       principalInfo);
      }

      // The failure callback won't run, and we're on the main thread, so
      // we need to properly release the thread-unsafe RemoteWorkerManager.
      NS_ProxyRelease(__func__, bgEventTarget, self.forget());
    } else {
      // The "real" failure callback.
      nsCOMPtr<nsIRunnable> r =
          NS_NewRunnableFunction(__func__, [self = std::move(self)] {
            auto pendings = std::move(self->mPendings);
            for (const auto& pending : pendings) {
              pending.mController->CreationFailed();
            }
          });

      bgEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
    }
  };

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [callback = std::move(processLaunchCallback)]() mutable {
        ContentParent::GetNewOrUsedBrowserProcessAsync(
            /* aFrameElement = */ nullptr,
            /* aRemoteType = */ NS_LITERAL_STRING(DEFAULT_REMOTE_TYPE))
            ->Then(GetCurrentThreadSerialEventTarget(), __func__,
                   [callback = std::move(callback)](
                       const CallbackParamType& aValue) mutable {
                     callback(aValue);
                   });
      });

  SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
}

}  // namespace dom
}  // namespace mozilla
