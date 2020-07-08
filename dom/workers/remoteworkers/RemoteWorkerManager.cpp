/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerManager.h"

#include <utility>

#include "mozilla/SchedulerGroup.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/ContentChild.h"  // ContentChild::GetSingleton
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

void TransmitPermissionsAndBlobURLsForPrincipalInfo(
    ContentParent* aContentParent, const PrincipalInfo& aPrincipalInfo) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aContentParent);

  auto principalOrErr = PrincipalInfoToPrincipal(aPrincipalInfo);

  if (NS_WARN_IF(principalOrErr.isErr())) {
    return;
  }

  nsCOMPtr<nsIPrincipal> principal = principalOrErr.unwrap();

  aContentParent->TransmitBlobURLsForPrincipal(principal);

  MOZ_ALWAYS_SUCCEEDS(
      aContentParent->TransmitPermissionsForPrincipal(principal));
}

}  // namespace

// static
bool RemoteWorkerManager::MatchRemoteType(const nsAString& processRemoteType,
                                          const nsAString& workerRemoteType) {
  if (processRemoteType.Equals(workerRemoteType)) {
    return true;
  }

  // Respecting COOP and COEP requires processing headers in the parent
  // process in order to choose an appropriate content process, but the
  // workers' ScriptLoader processes headers in content processes. An
  // intermediary step that provides security guarantees is to simply never
  // allow SharedWorkers and ServiceWorkers to exist in a COOP+COEP process.
  // The ultimate goal is to allow these worker types to be put in such
  // processes based on their script response headers.
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1595206
  if (IsWebCoopCoepRemoteType(processRemoteType)) {
    return false;
  }

  // A worker for a non privileged child process can be launched in
  // any web child process that is not COOP and COEP.
  if ((workerRemoteType.IsEmpty() || IsWebRemoteType(workerRemoteType)) &&
      IsWebRemoteType(processRemoteType)) {
    return true;
  }

  return false;
}

// static
Result<nsString, nsresult> RemoteWorkerManager::GetRemoteType(
    const nsCOMPtr<nsIPrincipal>& aPrincipal, WorkerType aWorkerType) {
  AssertIsOnMainThread();

  if (aWorkerType != WorkerType::WorkerTypeService &&
      aWorkerType != WorkerType::WorkerTypeShared) {
    // This methods isn't expected to be called for worker type that
    // aren't remote workers (currently Service and Shared workers).
    return Err(NS_ERROR_UNEXPECTED);
  }

  nsString remoteType;

  // If Gecko is running in single process mode, there is no child process
  // to select, return without assigning any remoteType.
  if (!BrowserTabsRemoteAutostart()) {
    return remoteType;
  }

  auto* contentChild = ContentChild::GetSingleton();

  bool isSystem = !!BasePrincipal::Cast(aPrincipal)->IsSystemPrincipal();
  bool isMozExtension =
      !isSystem && !!BasePrincipal::Cast(aPrincipal)->AddonPolicy();

  if (aWorkerType == WorkerType::WorkerTypeShared && !contentChild &&
      !isSystem) {
    // Prevent content principals SharedWorkers to be launched in the main
    // process while running in multiprocess mode.
    //
    // NOTE this also prevents moz-extension SharedWorker to be created
    // while the extension process is disabled by prefs, allowing it would
    // also trigger an assertion failure in
    // RemoteWorkerManager::SelectorTargetActor, due to an unexpected
    // content-principal parent-process workers while e10s is on).
    return Err(NS_ERROR_ABORT);
  }

  bool separatePrivilegedMozilla = Preferences::GetBool(
      "browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", false);

  if (isMozExtension) {
    remoteType.Assign(NS_LITERAL_STRING_FROM_CSTRING(EXTENSION_REMOTE_TYPE));
  } else if (separatePrivilegedMozilla) {
    bool isPrivilegedMozilla = false;
    aPrincipal->IsURIInPrefList("browser.tabs.remote.separatedMozillaDomains",
                                &isPrivilegedMozilla);

    if (isPrivilegedMozilla) {
      remoteType.Assign(
          NS_LITERAL_STRING_FROM_CSTRING(PRIVILEGEDMOZILLA_REMOTE_TYPE));
    } else {
      remoteType.Assign(
          aWorkerType == WorkerType::WorkerTypeShared && contentChild
              ? contentChild->GetRemoteType()
              : NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE));
    }
  } else {
    remoteType.Assign(NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE));
  }

  return remoteType;
}

// static
bool RemoteWorkerManager::IsRemoteTypeAllowed(const RemoteWorkerData& aData) {
  AssertIsOnMainThread();

  // If Gecko is running in single process mode, there is no child process
  // to select and we have to just consider it valid (if it should haven't
  // been launched it should have been already prevented before reaching
  // a RemoteWorkerChild instance).
  if (!BrowserTabsRemoteAutostart()) {
    return true;
  }

  const auto& principalInfo = aData.principalInfo();

  auto* contentChild = ContentChild::GetSingleton();
  if (!contentChild) {
    // If e10s isn't disabled, only workers related to the system principal
    // should be allowed to run in the parent process.
    return principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo;
  }

  auto principalOrErr = PrincipalInfoToPrincipal(principalInfo);
  if (NS_WARN_IF(principalOrErr.isErr())) {
    return false;
  }
  nsCOMPtr<nsIPrincipal> principal = principalOrErr.unwrap();

  // Recompute the remoteType based on the principal, to double-check that it
  // has not been tempered to select a different child process than the one
  // expected.
  bool isServiceWorker = aData.serviceWorkerData().type() ==
                         OptionalServiceWorkerData::TServiceWorkerData;
  auto remoteType = GetRemoteType(
      principal, isServiceWorker ? WorkerTypeService : WorkerTypeShared);
  if (NS_WARN_IF(remoteType.isErr())) {
    return false;
  }

  return MatchRemoteType(remoteType.unwrap(), contentChild->GetRemoteType());
}

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
    const auto& remoteType = GetRemoteTypeForActor(aActor);
    nsTArray<Pending> unlaunched;

    // Flush pending launching.
    for (Pending& p : mPendings) {
      if (p.mController->IsTerminated()) {
        continue;
      }

      const auto& workerRemoteType = p.mData.remoteType();

      if (MatchRemoteType(remoteType, workerRemoteType)) {
        LaunchInternal(p.mController, aActor, p.mData);
      } else {
        unlaunched.AppendElement(std::move(p));
        continue;
      }
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

/* static */
nsString RemoteWorkerManager::GetRemoteTypeForActor(
    const RemoteWorkerServiceParent* aActor) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  MOZ_ASSERT(aActor);

  RefPtr<ContentParent> contentParent =
      BackgroundParent::GetContentParent(aActor->Manager());
  auto scopeExit =
      MakeScopeExit([&] { NS_ReleaseOnMainThread(contentParent.forget()); });

  if (NS_WARN_IF(!contentParent)) {
    return EmptyString();
  }

  nsString aRemoteType(contentParent->GetRemoteType());

  return aRemoteType;
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

  const auto& workerRemoteType = aData.remoteType();

  ForEachActor([&](RemoteWorkerServiceParent* aActor,
                   RefPtr<ContentParent>&& aContentParent) {
    const auto& remoteType = aContentParent->GetRemoteType();

    if (MatchRemoteType(remoteType, workerRemoteType)) {
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
    base::ProcessId aProcessId, const RemoteWorkerData& aData) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mChildActors.IsEmpty());

  RemoteWorkerServiceParent* actor = nullptr;

  ForEachActor([&](RemoteWorkerServiceParent* aActor,
                   RefPtr<ContentParent>&& aContentParent) {
    bool matchRemoteType =
        MatchRemoteType(aContentParent->GetRemoteType(), aData.remoteType());

    if (!matchRemoteType) {
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

  return IsServiceWorker(aData)
             ? SelectTargetActorForServiceWorker(aData)
             : SelectTargetActorForSharedWorker(aProcessId, aData);
}

void RemoteWorkerManager::LaunchNewContentProcess(
    const RemoteWorkerData& aData) {
  AssertIsInMainProcess();
  AssertIsOnBackgroundThread();

  nsCOMPtr<nsISerialEventTarget> bgEventTarget = GetCurrentSerialEventTarget();

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
                                   const CallbackParamType& aValue,
                                   const nsString& remoteType) mutable {
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
      nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
          __func__, [self = std::move(self), remoteType] {
            nsTArray<Pending> uncancelled;
            auto pendings = std::move(self->mPendings);

            for (const auto& pending : pendings) {
              const auto& workerRemoteType = pending.mData.remoteType();
              if (self->MatchRemoteType(remoteType, workerRemoteType)) {
                pending.mController->CreationFailed();
              } else {
                uncancelled.AppendElement(pending);
              }
            }

            std::swap(self->mPendings, uncancelled);
          });

      bgEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
    }
  };

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [callback = std::move(processLaunchCallback),
                 workerRemoteType = aData.remoteType()]() mutable {
        auto remoteType =
            workerRemoteType.IsEmpty()
                ? NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE)
                : workerRemoteType;

        ContentParent::GetNewOrUsedBrowserProcessAsync(
            /* aFrameElement = */ nullptr,
            /* aRemoteType = */ remoteType)
            ->Then(GetCurrentSerialEventTarget(), __func__,
                   [callback = std::move(callback),
                    remoteType](const CallbackParamType& aValue) mutable {
                     callback(aValue, remoteType);
                   });
      });

  SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
}

}  // namespace dom
}  // namespace mozilla
