/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerService.h"

#include "mozilla/dom/PRemoteWorkerParent.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundParent.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Services.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsXPCOMPrivate.h"
#include "RemoteWorkerController.h"
#include "RemoteWorkerServiceChild.h"
#include "RemoteWorkerServiceParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

namespace {

StaticMutex sRemoteWorkerServiceMutex;
StaticRefPtr<RemoteWorkerService> sRemoteWorkerService;

}  // namespace

/**
 * Block shutdown until the RemoteWorkers have shutdown so that we do not try
 * and shutdown the RemoteWorkerService "Worker Launcher" thread until they have
 * cleanly shutdown.
 *
 * Note that this shutdown blocker is not used to initiate shutdown of any of
 * the workers directly; their shutdown is initiated from PBackground in the
 * parent process.  The shutdown blocker just exists to avoid races around
 * shutting down the worker launcher thread after all of the workers have
 * shutdown and torn down their actors.
 *
 * Currently, it should be the case that the ContentParent should want to keep
 * the content processes alive until the RemoteWorkers have all reported their
 * shutdown over IPC (on the "Worker Launcher" thread).  So for an orderly
 * content process shutdown that is waiting for there to no longer be a reason
 * to keep the content process alive, this blocker should only hang around for
 * a brief period of time, helping smooth out lifecycle edge cases.
 *
 * In the event the content process is trying to shutdown while the
 * RemoteWorkers think they should still be alive, it's possible that this
 * blocker could expose the relevant logic error in the parent process if no
 * attempt is made to shutdown the RemoteWorker.
 */
class RemoteWorkerServiceShutdownBlocker final
    : public nsIAsyncShutdownBlocker {
  ~RemoteWorkerServiceShutdownBlocker() = default;

 public:
  RemoteWorkerServiceShutdownBlocker(RemoteWorkerService* aService,
                                     nsIAsyncShutdownClient* aShutdownClient)
      : mService(aService), mShutdownClient(aShutdownClient) {
    nsAutoString blockerName;
    MOZ_ALWAYS_SUCCEEDS(GetName(blockerName));

    MOZ_ALWAYS_SUCCEEDS(mShutdownClient->AddBlocker(
        this, NS_LITERAL_STRING_FROM_CSTRING(__FILE__), __LINE__, blockerName));
  }

  void RemoteWorkersAllGoneAllowShutdown() {
    mShutdownClient->RemoveBlocker(this);
    mShutdownClient = nullptr;

    mService->FinishShutdown();
    mService = nullptr;
  }

  NS_IMETHOD
  GetName(nsAString& aNameOut) override {
    aNameOut = nsLiteralString(
        u"RemoteWorkerService: waiting for RemoteWorkers to shutdown");
    return NS_OK;
  }

  NS_IMETHOD
  BlockShutdown(nsIAsyncShutdownClient* aClient) override {
    mService->BeginShutdown();
    return NS_OK;
  }

  NS_IMETHOD
  GetState(nsIPropertyBag**) override { return NS_OK; }

  NS_DECL_ISUPPORTS

  RefPtr<RemoteWorkerService> mService;
  nsCOMPtr<nsIAsyncShutdownClient> mShutdownClient;
};

NS_IMPL_ISUPPORTS(RemoteWorkerServiceShutdownBlocker, nsIAsyncShutdownBlocker)

RemoteWorkerServiceKeepAlive::RemoteWorkerServiceKeepAlive(
    RemoteWorkerServiceShutdownBlocker* aBlocker)
    : mBlocker(aBlocker) {
  MOZ_ASSERT(NS_IsMainThread());
}

RemoteWorkerServiceKeepAlive::~RemoteWorkerServiceKeepAlive() {
  // Dispatch a runnable to the main thread to tell the Shutdown Blocker to
  // remove itself and notify the RemoteWorkerService it can finish its
  // shutdown.  We dispatch this to the main thread even if we are already on
  // the main thread.
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction(__func__, [blocker = std::move(mBlocker)] {
        blocker->RemoteWorkersAllGoneAllowShutdown();
      });
  MOZ_ALWAYS_SUCCEEDS(
      SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));
}

/* static */
void RemoteWorkerService::Initialize() {
  MOZ_ASSERT(NS_IsMainThread());

  StaticMutexAutoLock lock(sRemoteWorkerServiceMutex);
  MOZ_ASSERT(!sRemoteWorkerService);

  RefPtr<RemoteWorkerService> service = new RemoteWorkerService();

  // ## Content Process Initialization Case
  //
  // We are being told to initialize now that we know what our remote type is.
  // But if we want to register with the JS-based AsyncShutdown mechanism, it's
  // too early to do this, so we must delay using a runnable.
  //
  // ### Additional Details
  //
  // In content processes, we will be invoked by ContentChild::RecvRemoteType
  // after specializing the content process, which was approximately the
  // appropriate time to register when this was written.  A new complication is
  // that the async shutdown service is implemented in JS and currently
  // ContentParent::InitInternal calls SendRemoteType immediately before calling
  // ScriptPreloader::InitContentChild which will call
  // SendPScriptCacheConstructor.  If we try and load JS before
  // RecvPScriptCacheConstructor happens, we will fail to load the AsyncShutdown
  // service.
  //
  // There are notifications generated when RecvPScriptCacheConstructor is
  // called; it will call NS_CreateServicesFromCategory with
  // "content-process-ready-for-script" as both the category to enumerate to
  // call do_GetService on and then to generate an observer notification with
  // that same name on the service as long as it directly implements
  // nsIObserver.  (It does not generate a general observer notification because
  // the code reasonably assumes that the code would be using the category
  // manager to start itself up.)
  //
  // TODO: Convert this into an XPCOM service that could leverage the category
  // service.  For now, we just schedule a runnable to initialize the
  // service once we get to the event loop.  This should largely be okay because
  // we need the main thread event loop to be spinning for this to work.  This
  // could move latencies attribution around slightly so that metrics see a
  // longer time to have a content process spawn a worker in and less time for
  // the first spawn in that process.  In the event PBackground is janky, this
  // could introduce new latencies that aren't just changes in attribution,
  // however.
  if (!XRE_IsParentProcess()) {
    sRemoteWorkerService = service;

    nsCOMPtr<nsIRunnable> r =
        NS_NewRunnableFunction(__func__, [initService = std::move(service)] {
          nsresult rv = initService->InitializeOnMainThread();
          Unused << NS_WARN_IF(NS_FAILED(rv));
        });
    MOZ_ALWAYS_SUCCEEDS(
        SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));
    return;
  }
  // ## Parent Process Initialization Case
  //
  // Otherwise we are in the parent process and were invoked by
  // nsLayoutStatics::Initialize.  We wait until profile-after-change to kick
  // off the Worker Launcher thread and have it connect to PBackground.  This is
  // an appropriate time for remote worker APIs to come online, especially
  // because the PRemoteWorkerService mechanism needs processes to eagerly
  // register themselves with PBackground since the design explicitly intends to
  // avoid blocking on the main threads.  (Disclaimer: Currently, things block
  // on the main thread.)

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (NS_WARN_IF(!obs)) {
    return;
  }

  nsresult rv = obs->AddObserver(service, "profile-after-change", false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  sRemoteWorkerService = service;
}

/* static */
nsIThread* RemoteWorkerService::Thread() {
  StaticMutexAutoLock lock(sRemoteWorkerServiceMutex);
  MOZ_ASSERT(sRemoteWorkerService);
  MOZ_ASSERT(sRemoteWorkerService->mThread);
  return sRemoteWorkerService->mThread;
}

/* static */
already_AddRefed<RemoteWorkerServiceKeepAlive>
RemoteWorkerService::MaybeGetKeepAlive() {
  StaticMutexAutoLock lock(sRemoteWorkerServiceMutex);
  // In normal operation no one should be calling this without a service
  // existing, so assert, but we'll also handle this being null as it is a
  // plausible shutdown race.
  MOZ_ASSERT(sRemoteWorkerService);
  if (!sRemoteWorkerService) {
    return nullptr;
  }

  // Note that this value can be null, but this all handles that.
  auto lockedKeepAlive = sRemoteWorkerService->mKeepAlive.Lock();
  RefPtr<RemoteWorkerServiceKeepAlive> extraRef = *lockedKeepAlive;
  return extraRef.forget();
}

nsresult RemoteWorkerService::InitializeOnMainThread() {
  // I would like to call this thread "DOM Remote Worker Launcher", but the max
  // length is 16 chars.
  nsresult rv = NS_NewNamedThread("Worker Launcher", getter_AddRefs(mThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIAsyncShutdownService> shutdownSvc =
      services::GetAsyncShutdownService();
  if (NS_WARN_IF(!shutdownSvc)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIAsyncShutdownClient> phase;
  // Register a blocker that will ensure that the Worker Launcher thread does
  // not go away until all of its RemoteWorkerChild instances have let their
  // workers shutdown.  This is necessary for sanity and should add a minimal
  // shutdown delay.
  //
  // (Previously we shutdown at "xpcom-shutdown", but the service currently only
  // provides "xpcom-will-shutdown".  This is arguably beneficial since many
  // things on "xpcom-shutdown" may spin event loops whereas we and the other
  // blockers can operate in parallel and avoid being delayed by those serial
  // blockages.)
  MOZ_ALWAYS_SUCCEEDS(shutdownSvc->GetXpcomWillShutdown(getter_AddRefs(phase)));
  if (NS_WARN_IF(!phase)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<RemoteWorkerServiceShutdownBlocker> blocker =
      new RemoteWorkerServiceShutdownBlocker(this, phase);

  {
    RefPtr<RemoteWorkerServiceKeepAlive> keepAlive =
        new RemoteWorkerServiceKeepAlive(blocker);

    auto lockedKeepAlive = mKeepAlive.Lock();
    *lockedKeepAlive = std::move(keepAlive);
  }

  RefPtr<RemoteWorkerService> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "InitializeThread", [self]() { self->InitializeOnTargetThread(); });

  rv = mThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

RemoteWorkerService::RemoteWorkerService()
    : mKeepAlive(nullptr, "RemoteWorkerService::mKeepAlive") {
  MOZ_ASSERT(NS_IsMainThread());
}

RemoteWorkerService::~RemoteWorkerService() = default;

void RemoteWorkerService::InitializeOnTargetThread() {
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(mThread->IsOnCurrentThread());

  PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return;
  }

  RefPtr<RemoteWorkerServiceChild> serviceActor =
      MakeAndAddRef<RemoteWorkerServiceChild>();
  if (NS_WARN_IF(!backgroundActor->SendPRemoteWorkerServiceConstructor(
          serviceActor))) {
    return;
  }

  // Now we are ready!
  mActor = serviceActor;
}

void RemoteWorkerService::CloseActorOnTargetThread() {
  MOZ_ASSERT(mThread);
  MOZ_ASSERT(mThread->IsOnCurrentThread());

  // If mActor is nullptr it means that initialization failed.
  if (mActor) {
    // Here we need to shutdown the IPC protocol.
    mActor->Send__delete__(mActor);
    mActor = nullptr;
  }
}

NS_IMETHODIMP
RemoteWorkerService::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData) {
  MOZ_ASSERT(!strcmp(aTopic, "profile-after-change"));
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "profile-after-change");
  }

  return InitializeOnMainThread();
}

void RemoteWorkerService::BeginShutdown() {
  // Drop our keepalive reference which may allow near-immediate removal of the
  // blocker.
  auto lockedKeepAlive = mKeepAlive.Lock();
  *lockedKeepAlive = nullptr;
}

void RemoteWorkerService::FinishShutdown() {
  // Clear the singleton before spinning the event loop when shutting down the
  // thread so that MaybeGetKeepAlive() can assert if there are any late calls
  // and to better reflect the actual state.
  //
  // Our caller, the RemoteWorkerServiceShutdownBlocker, will continue to hold a
  // strong reference to us until we return from this call, so there are no
  // lifecycle implications to dropping this reference.
  {
    StaticMutexAutoLock lock(sRemoteWorkerServiceMutex);
    sRemoteWorkerService = nullptr;
  }

  RefPtr<RemoteWorkerService> self = this;
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("RemoteWorkerService::CloseActorOnTargetThread",
                             [self]() { self->CloseActorOnTargetThread(); });

  mThread->Dispatch(r.forget(), NS_DISPATCH_NORMAL);

  // We've posted a shutdown message; now shutdown the thread.  This will spin
  // a nested event loop waiting for the thread to process all pending events
  // (including the just dispatched CloseActorOnTargetThread which will close
  // the actor), ensuring to block main thread shutdown long enough to avoid
  // races.
  mThread->Shutdown();
  mThread = nullptr;
}

NS_IMPL_ISUPPORTS(RemoteWorkerService, nsIObserver)

}  // namespace dom
}  // namespace mozilla
