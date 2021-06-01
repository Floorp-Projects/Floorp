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
#include "mozilla/dom/RemoteWorkerController.h"
#include "mozilla/dom/RemoteWorkerParent.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundParent.h"
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
#  include "mozilla/dom/DOMException.h"
#  include "mozilla/CycleCollectedJSContext.h"
#  include "mozilla/Sprintf.h"  // SprintfLiteral
#  include "nsIXPConnect.h"     // nsIXPConnectWrappedJS
#endif
#include "mozilla/StaticPrefs_extensions.h"
#include "nsCOMPtr.h"
#include "nsIE10SUtils.h"
#include "nsImportModule.h"
#include "nsIXULRuntime.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"
#include "RemoteWorkerServiceParent.h"

mozilla::LazyLogModule gRemoteWorkerManagerLog("RemoteWorkerManager");

#define LOG(fmt) \
  MOZ_LOG(gRemoteWorkerManagerLog, mozilla::LogLevel::Verbose, fmt)

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
bool RemoteWorkerManager::MatchRemoteType(const nsACString& processRemoteType,
                                          const nsACString& workerRemoteType) {
  LOG(("MatchRemoteType [processRemoteType=%s, workerRemoteType=%s]",
       PromiseFlatCString(processRemoteType).get(),
       PromiseFlatCString(workerRemoteType).get()));

  // Respecting COOP and COEP requires processing headers in the parent
  // process in order to choose an appropriate content process, but the
  // workers' ScriptLoader processes headers in content processes. An
  // intermediary step that provides security guarantees is to simply never
  // allow SharedWorkers and ServiceWorkers to exist in a COOP+COEP process.
  // The ultimate goal is to allow these worker types to be put in such
  // processes based on their script response headers.
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1595206
  //
  // RemoteWorkerManager::GetRemoteType should not select this remoteType
  // and so workerRemoteType is not expected to be set to a coop+coep
  // remoteType and here we can just assert that it is not happening.
  MOZ_ASSERT(!IsWebCoopCoepRemoteType(workerRemoteType));

  // For similar reasons to the ones related to COOP+COEP processes,
  // we don't expect workerRemoteType to be set to a large allocation one.
  MOZ_ASSERT(workerRemoteType != LARGE_ALLOCATION_REMOTE_TYPE);

  return processRemoteType.Equals(workerRemoteType);
}

// static
Result<nsCString, nsresult> RemoteWorkerManager::GetRemoteType(
    const nsCOMPtr<nsIPrincipal>& aPrincipal, WorkerKind aWorkerKind) {
  AssertIsOnMainThread();

  MOZ_ASSERT_IF(aWorkerKind == WorkerKind::WorkerKindService,
                aPrincipal->GetIsContentPrincipal());

  nsCOMPtr<nsIE10SUtils> e10sUtils =
      do_ImportModule("resource://gre/modules/E10SUtils.jsm", "E10SUtils");
  if (NS_WARN_IF(!e10sUtils)) {
    LOG(("GetRemoteType Abort: could not import E10SUtils"));
    return Err(NS_ERROR_DOM_ABORT_ERR);
  }

  nsCString preferredRemoteType = DEFAULT_REMOTE_TYPE;
  if (aWorkerKind == WorkerKind::WorkerKindShared) {
    if (auto* contentChild = ContentChild::GetSingleton()) {
      // For a shared worker set the preferred remote type to the content
      // child process remote type.
      preferredRemoteType = contentChild->GetRemoteType();
    } else if (aPrincipal->IsSystemPrincipal()) {
      preferredRemoteType = NOT_REMOTE_TYPE;
    }
  }

  nsIE10SUtils::RemoteWorkerType workerType;

  switch (aWorkerKind) {
    case WorkerKind::WorkerKindService:
      workerType = nsIE10SUtils::REMOTE_WORKER_TYPE_SERVICE;
      break;
    case WorkerKind::WorkerKindShared:
      workerType = nsIE10SUtils::REMOTE_WORKER_TYPE_SHARED;
      break;
    default:
      // This method isn't expected to be called for worker types that
      // aren't remote workers (currently Service and Shared workers).
      LOG(("GetRemoteType Error on unexpected worker type"));
      MOZ_DIAGNOSTIC_ASSERT(false, "Unexpected worker type");
      return Err(NS_ERROR_DOM_ABORT_ERR);
  }

  // Here we do not have access to the window and so we can't use its
  // useRemoteTabs and useRemoteSubframes flags (for the service
  // worker there may not even be a window associated to the worker
  // yet), and so we have to use the prefs instead.
  bool isMultiprocess = BrowserTabsRemoteAutostart();
  bool isFission = FissionAutostart();

  nsCString remoteType = NOT_REMOTE_TYPE;

  nsresult rv = e10sUtils->GetRemoteTypeForWorkerPrincipal(
      aPrincipal, workerType, isMultiprocess, isFission, preferredRemoteType,
      remoteType);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    LOG(
        ("GetRemoteType Abort: E10SUtils.getRemoteTypeForWorkerPrincipal "
         "exception"));
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    nsCString principalTypeOrScheme;
    if (aPrincipal->IsSystemPrincipal()) {
      principalTypeOrScheme = "system"_ns;
    } else if (aPrincipal->GetIsExpandedPrincipal()) {
      principalTypeOrScheme = "expanded"_ns;
    } else if (aPrincipal->GetIsNullPrincipal()) {
      principalTypeOrScheme = "null"_ns;
    } else {
      nsCOMPtr<nsIURI> uri = aPrincipal->GetURI();
      nsresult rv2 = uri->GetScheme(principalTypeOrScheme);
      if (NS_FAILED(rv2)) {
        principalTypeOrScheme = "content"_ns;
      }
    }

    nsCString processRemoteType = "parent"_ns;
    if (auto* contentChild = ContentChild::GetSingleton()) {
      // RemoteTypePrefix make sure that we are not going to include
      // the full origin that may be part of the current remote type.
      processRemoteType = RemoteTypePrefix(contentChild->GetRemoteType());
    }

    // Convert the error code into an error name.
    nsAutoCString errorName;
    GetErrorName(rv, errorName);

    // Try to retrieve the line number from the exception.
    nsAutoCString errorFilename("(unknown)"_ns);
    uint32_t jsmErrorLineNumber = 0;

    if (auto* context = CycleCollectedJSContext::Get()) {
      if (RefPtr<Exception> exn = context->GetPendingException()) {
        nsAutoString filename(u"(unknown)"_ns);

        if (rv == NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS) {
          // When the failure is a Javascript Error, the line number retrieved
          // from the Exception instance isn't going to be the E10SUtils.jsm
          // line that originated the failure, and so we fallback to retrieve it
          // from the nsIScriptError.
          nsCOMPtr<nsIScriptError> scriptError =
              do_QueryInterface(exn->GetData());
          if (scriptError) {
            scriptError->GetLineNumber(&jsmErrorLineNumber);
            scriptError->GetSourceName(filename);
          }
        } else {
          nsCOMPtr<nsIXPConnectWrappedJS> wrapped =
              do_QueryInterface(e10sUtils);
          dom::AutoJSAPI jsapi;
          if (jsapi.Init(wrapped->GetJSObjectGlobal())) {
            auto* cx = jsapi.cx();
            jsmErrorLineNumber = exn->LineNumber(cx);
            exn->GetFilename(cx, filename);
          }
        }

        errorFilename = NS_ConvertUTF16toUTF8(filename);
      }
    }

    char buf[1024];
    SprintfLiteral(
        buf,
        "workerType=%s, principal=%s, preferredRemoteType=%s, "
        "processRemoteType=%s, errorName=%s, errorLocation=%s:%d",
        aWorkerKind == WorkerKind::WorkerKindService ? "service" : "shared",
        principalTypeOrScheme.get(),
        PromiseFlatCString(RemoteTypePrefix(preferredRemoteType)).get(),
        processRemoteType.get(), errorName.get(), errorFilename.get(),
        jsmErrorLineNumber);
    MOZ_CRASH_UNSAFE_PRINTF(
        "E10SUtils.getRemoteTypeForWorkerPrincipal did throw: %s", buf);
#endif
    return Err(NS_ERROR_DOM_ABORT_ERR);
  }

  if (MOZ_LOG_TEST(gRemoteWorkerManagerLog, LogLevel::Verbose)) {
    nsCString principalOrigin;
    aPrincipal->GetOrigin(principalOrigin);

    LOG(
        ("GetRemoteType workerType=%s, principal=%s, "
         "preferredRemoteType=%s, selectedRemoteType=%s",
         aWorkerKind == WorkerKind::WorkerKindService ? "service" : "shared",
         principalOrigin.get(), preferredRemoteType.get(), remoteType.get()));
  }

  return remoteType;
}

// static
bool RemoteWorkerManager::HasExtensionPrincipal(const RemoteWorkerData& aData) {
  auto principalInfo = aData.principalInfo();
  return principalInfo.type() == PrincipalInfo::TContentPrincipalInfo &&
         // This helper method is also called from the background thread and so
         // we can't check if the principal does have an addonPolicy object
         // associated and we have to resort to check the url scheme instead.
         StringBeginsWith(principalInfo.get_ContentPrincipalInfo().spec(),
                          "moz-extension://"_ns);
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
    // should be allowed to run in the parent process, and extension principals
    // if extensions.webextensions.remote is false.
    return principalInfo.type() == PrincipalInfo::TSystemPrincipalInfo ||
           (!StaticPrefs::extensions_webextensions_remote() &&
            aData.remoteType().Equals(NOT_REMOTE_TYPE) &&
            HasExtensionPrincipal(aData));
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
      principal, isServiceWorker ? WorkerKindService : WorkerKindShared);
  if (NS_WARN_IF(remoteType.isErr())) {
    LOG(("IsRemoteTypeAllowed: Error to retrieve remote type"));
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
    const auto& processRemoteType = aActor->GetRemoteType();
    nsTArray<Pending> unlaunched;

    // Flush pending launching.
    for (Pending& p : mPendings) {
      if (p.mController->IsTerminated()) {
        continue;
      }

      const auto& workerRemoteType = p.mData.remoteType();

      if (MatchRemoteType(processRemoteType, workerRemoteType)) {
        LOG(("RegisterActor - Launch Pending, workerRemoteType=%s",
             workerRemoteType.get()));
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

    LOG(("RegisterActor - mPendings length: %zu", mPendings.Length()));
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
   * If a target actor for the remote worker has been selected, the actor has
   * already been registered with the corresponding `ContentParent` and we
   * should not increment the `mRemoteWorkerActorData`'s `mCount` again (see
   * `SelectTargetActorForServiceWorker()` /
   * `SelectTargetActorForSharedWorker()`).
   */
  LaunchInternal(aController, targetActor, aData, true);
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

  // We need to send permissions to content processes, but not if we're spawning
  // the worker here in the parent process.
  if (aTargetActor != mParentActor) {
    RefPtr<ContentParent> contentParent =
        BackgroundParent::GetContentParent(aTargetActor->Manager());

    // This won't cause any race conditions because the content process
    // should wait for the permissions to be received before executing the
    // Service Worker.
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        __func__, [contentParent = std::move(contentParent),
                   principalInfo = aData.principalInfo()] {
          TransmitPermissionsAndBlobURLsForPrincipalInfo(contentParent,
                                                         principalInfo);
        });

    MOZ_ALWAYS_SUCCEEDS(
        SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));
  }

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
void RemoteWorkerManager::ForEachActor(
    Callback&& aCallback, const nsACString& aRemoteType,
    Maybe<base::ProcessId> aProcessId) const {
  AssertIsOnBackgroundThread();

  const auto length = mChildActors.Length();

  auto end = static_cast<uint32_t>(rand()) % length;
  if (aProcessId) {
    // Start from the actor with the given processId instead of starting from
    // a random index.
    for (auto j = length - 1; j > 0; j--) {
      if (mChildActors[j]->OtherPid() == *aProcessId) {
        end = j;
        break;
      }
    }
  }

  uint32_t i = end;

  nsTArray<RefPtr<ContentParent>> proxyReleaseArray;

  do {
    MOZ_ASSERT(i < mChildActors.Length());
    RemoteWorkerServiceParent* actor = mChildActors[i];

    if (MatchRemoteType(actor->GetRemoteType(), aRemoteType)) {
      RefPtr<ContentParent> contentParent =
          BackgroundParent::GetContentParent(actor->Manager());

      auto scopeExit = MakeScopeExit(
          [&]() { proxyReleaseArray.AppendElement(std::move(contentParent)); });

      if (!aCallback(actor, std::move(contentParent))) {
        break;
      }
    }

    i = (i + 1) % length;
  } while (i != end);

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [proxyReleaseArray = std::move(proxyReleaseArray)] {});

  MOZ_ALWAYS_SUCCEEDS(
      SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));
}

/**
 * When selecting a target actor for a given remote worker, we have to consider
 * that:
 *
 * - Service Workers can spawn even when their registering page/script isn't
 *   active (e.g. push notifications), so we don't attempt to spawn the worker
 *   in its registering script's process. We search linearly and choose the
 *   search's starting position randomly.
 *
 * - When Fission is enabled, Shared Workers may have to be spawned into
 * different child process from the one where it has been registered from, and
 * that child process may be going to be marked as dead and shutdown.
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
RemoteWorkerServiceParent* RemoteWorkerManager::SelectTargetActorInternal(
    const RemoteWorkerData& aData, base::ProcessId aProcessId) const {
  AssertIsOnBackgroundThread();
  MOZ_ASSERT(!mChildActors.IsEmpty());

  RemoteWorkerServiceParent* actor = nullptr;

  const auto& workerRemoteType = aData.remoteType();

  ForEachActor(
      [&](RemoteWorkerServiceParent* aActor,
          RefPtr<ContentParent>&& aContentParent) {
        // Make sure to choose an actor related to a child process that is not
        // going to shutdown while we are still in the process of launching the
        // remote worker.
        //
        // ForEachActor will start from the child actor coming from the child
        // process with a pid equal to aProcessId if any, otherwise it would
        // start from a random actor in the mChildActors array, this guarantees
        // that we will choose that actor if it does also match the remote type.
        auto lock = aContentParent->mRemoteWorkerActorData.Lock();

        if ((lock->mCount || !lock->mShutdownStarted) &&
            (aActor->OtherPid() == aProcessId || !actor)) {
          ++lock->mCount;

          actor = aActor;
          return false;
        }

        MOZ_ASSERT(!actor);
        return true;
      },
      workerRemoteType, IsServiceWorker(aData) ? Nothing() : Some(aProcessId));

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

  // Extension principal workers are allowed to run on the parent process
  // when "extension.webextensions.remote" pref is false.
  if (aProcessId == base::GetCurrentProcId() &&
      aData.remoteType().Equals(NOT_REMOTE_TYPE) &&
      !StaticPrefs::extensions_webextensions_remote() &&
      HasExtensionPrincipal(aData)) {
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

  return SelectTargetActorInternal(aData, aProcessId);
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
  auto processLaunchCallback = [principalInfo = aData.principalInfo(),
                                bgEventTarget = std::move(bgEventTarget),
                                self = RefPtr<RemoteWorkerManager>(this)](
                                   const CallbackParamType& aValue,
                                   const nsCString& remoteType) mutable {
    if (aValue.IsResolve()) {
      LOG(("LaunchNewContentProcess: successfully got child process"));

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
                LOG(
                    ("LaunchNewContentProcess: Cancel pending with "
                     "workerRemoteType=%s",
                     workerRemoteType.get()));
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

  LOG(("LaunchNewContentProcess: remoteType=%s", aData.remoteType().get()));

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [callback = std::move(processLaunchCallback),
                 workerRemoteType = aData.remoteType()]() mutable {
        auto remoteType =
            workerRemoteType.IsEmpty() ? DEFAULT_REMOTE_TYPE : workerRemoteType;

        // Request a process making sure to specify aPreferUsed=true.  For a
        // given remoteType there's a pool size limit.  If we pass aPreferUsed
        // here, then if there's any process in the pool already, we will use
        // that.  If we pass false (which is the default if omitted), then this
        // call will spawn a new process if the pool isn't at its limit yet.
        //
        // (Our intent is never to grow the pool size here.  Our logic gets here
        // because our current logic on PBackground is only aware of
        // RemoteWorkerServiceParent actors that have registered themselves,
        // which is fundamentally unaware of processes that will match in the
        // future when they register.  So we absolutely are fine with and want
        // any existing processes.)
        ContentParent::GetNewOrUsedBrowserProcessAsync(
            /* aRemoteType = */ remoteType,
            /* aGroup */ nullptr,
            hal::ProcessPriority::PROCESS_PRIORITY_FOREGROUND,
            /* aPreferUsed */ true)
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
