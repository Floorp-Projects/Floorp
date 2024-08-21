/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerRunnable.h"

#include "WorkerScope.h"
#include "js/RootingAPI.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/Assertions.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryHistogramEnums.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/Worker.h"
#include "mozilla/dom/WorkerCommon.h"
#include "nsDebug.h"
#include "nsGlobalWindowInner.h"
#include "nsID.h"
#include "nsIEventTarget.h"
#include "nsIGlobalObject.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "nsWrapperCacheInlines.h"

namespace mozilla::dom {

static mozilla::LazyLogModule sWorkerRunnableLog("WorkerRunnable");

#ifdef LOG
#  undef LOG
#endif
#define LOG(args) MOZ_LOG(sWorkerRunnableLog, LogLevel::Verbose, args);

namespace {

const nsIID kWorkerRunnableIID = {
    0x320cc0b5,
    0xef12,
    0x4084,
    {0x88, 0x6e, 0xca, 0x6a, 0x81, 0xe4, 0x1d, 0x68}};

}  // namespace

#ifdef DEBUG
WorkerRunnable::WorkerRunnable(const char* aName)
#  ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
    : mName(aName) {
  LOG(("WorkerRunnable::WorkerRunnable [%p] (%s)", this, mName));
}
#  else
{
  LOG(("WorkerRunnable::WorkerRunnable [%p]", this));
}
#  endif
#endif

// static
WorkerRunnable* WorkerRunnable::FromRunnable(nsIRunnable* aRunnable) {
  MOZ_ASSERT(aRunnable);

  WorkerRunnable* runnable;
  nsresult rv = aRunnable->QueryInterface(kWorkerRunnableIID,
                                          reinterpret_cast<void**>(&runnable));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  MOZ_ASSERT(runnable);
  return runnable;
}

bool WorkerRunnable::Dispatch(WorkerPrivate* aWorkerPrivate) {
  LOG(("WorkerRunnable::Dispatch [%p] aWorkerPrivate: %p", this,
       aWorkerPrivate));
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);
  bool ok = PreDispatch(aWorkerPrivate);
  if (ok) {
    ok = DispatchInternal(aWorkerPrivate);
  }
  PostDispatch(aWorkerPrivate, ok);
  return ok;
}

NS_IMETHODIMP WorkerRunnable::Run() { return NS_OK; }

NS_IMPL_ADDREF(WorkerRunnable)
NS_IMPL_RELEASE(WorkerRunnable)

#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
NS_IMETHODIMP
WorkerRunnable::GetName(nsACString& aName) {
  if (mName) {
    aName.AssignASCII(mName);
  } else {
    aName.Truncate();
  }
  return NS_OK;
}
#endif

NS_INTERFACE_MAP_BEGIN(WorkerRunnable)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
#ifdef MOZ_COLLECTING_RUNNABLE_TELEMETRY
  NS_INTERFACE_MAP_ENTRY(nsINamed)
#endif
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRunnable)
  // kWorkerRunnableIID is special in that it does not AddRef its result.
  if (aIID.Equals(kWorkerRunnableIID)) {
    *aInstancePtr = this;
    return NS_OK;
  } else
NS_INTERFACE_MAP_END

WorkerParentThreadRunnable::WorkerParentThreadRunnable(const char* aName)
    : WorkerRunnable(aName) {
  LOG(("WorkerParentThreadRunnable::WorkerParentThreadRunnable [%p]", this));
}

WorkerParentThreadRunnable::~WorkerParentThreadRunnable() = default;

bool WorkerParentThreadRunnable::PreDispatch(WorkerPrivate* aWorkerPrivate) {
#ifdef DEBUG
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
#endif
  return true;
}

bool WorkerParentThreadRunnable::DispatchInternal(
    WorkerPrivate* aWorkerPrivate) {
  LOG(("WorkerParentThreadRunnable::DispatchInternal [%p]", this));
  mWorkerParentRef = aWorkerPrivate->GetWorkerParentRef();
  RefPtr<WorkerParentThreadRunnable> runnable(this);
  return NS_SUCCEEDED(aWorkerPrivate->DispatchToParent(runnable.forget()));
}

void WorkerParentThreadRunnable::PostDispatch(WorkerPrivate* aWorkerPrivate,
                                              bool aDispatchResult) {
#ifdef DEBUG
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();
#endif
}

bool WorkerParentThreadRunnable::PreRun(WorkerPrivate* aWorkerPrivate) {
  return true;
}

void WorkerParentThreadRunnable::PostRun(JSContext* aCx,
                                         WorkerPrivate* aWorkerPrivate,
                                         bool aRunResult) {
  MOZ_ASSERT(aCx);
#ifdef DEBUG
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnParentThread();
#endif
}

NS_IMETHODIMP
WorkerParentThreadRunnable::Run() {
  LOG(("WorkerParentThreadRunnable::Run [%p]", this));
  RefPtr<WorkerPrivate> workerPrivate;
  MOZ_ASSERT(mWorkerParentRef);
  workerPrivate = mWorkerParentRef->Private();
  if (!workerPrivate) {
    NS_WARNING("Worker has already shut down!!!");
    return NS_OK;
  }
#ifdef DEBUG
  workerPrivate->AssertIsOnParentThread();
#endif

  WorkerPrivate* parent = workerPrivate->GetParent();
  bool isOnMainThread = !parent;
  bool result = PreRun(workerPrivate);
  MOZ_ASSERT(result);

  LOG(("WorkerParentThreadRunnable::Run [%p] WorkerPrivate: %p, parent: %p",
       this, workerPrivate.get(), parent));

  // Track down the appropriate global, if any, to use for the AutoEntryScript.
  nsCOMPtr<nsIGlobalObject> globalObject;
  if (isOnMainThread) {
    MOZ_ASSERT(isOnMainThread == NS_IsMainThread());
    globalObject = nsGlobalWindowInner::Cast(workerPrivate->GetWindow());
  } else {
    MOZ_ASSERT(parent == GetCurrentThreadWorkerPrivate());
    globalObject = parent->GlobalScope();
    MOZ_DIAGNOSTIC_ASSERT(globalObject);
  }
  // We might run script as part of WorkerRun, so we need an AutoEntryScript.
  // This is part of the HTML spec for workers at:
  // http://www.whatwg.org/specs/web-apps/current-work/#run-a-worker
  // If we don't have a globalObject we have to use an AutoJSAPI instead, but
  // this is OK as we won't be running script in these circumstances.
  Maybe<mozilla::dom::AutoJSAPI> maybeJSAPI;
  Maybe<mozilla::dom::AutoEntryScript> aes;
  JSContext* cx;
  AutoJSAPI* jsapi;

  if (globalObject) {
    aes.emplace(globalObject, "Worker parent thread runnable", isOnMainThread);
    jsapi = aes.ptr();
    cx = aes->cx();
  } else {
    maybeJSAPI.emplace();
    maybeJSAPI->Init();
    jsapi = maybeJSAPI.ptr();
    cx = jsapi->cx();
  }

  // Note that we can't assert anything about
  // workerPrivate->ParentEventTargetRef()->GetWrapper()
  // existing, since it may in fact have been GCed (and we may be one of the
  // runnables cleaning up the worker as a result).

  // If we are on the parent thread and that thread is not the main thread,
  // then we must be a dedicated worker (because there are no
  // Shared/ServiceWorkers whose parent is itself a worker) and then we
  // definitely have a globalObject.  If it _is_ the main thread, globalObject
  // can be null for workers started from JSMs or other non-window contexts,
  // sadly.
  MOZ_ASSERT_IF(!isOnMainThread,
                workerPrivate->IsDedicatedWorker() && globalObject);

  // If we're on the parent thread we might be in a null realm in the
  // situation described above when globalObject is null.  Make sure to enter
  // the realm of the worker's reflector if there is one.  There might
  // not be one if we're just starting to compile the script for this worker.
  Maybe<JSAutoRealm> ar;
  if (workerPrivate->IsDedicatedWorker() &&
      workerPrivate->ParentEventTargetRef() &&
      workerPrivate->ParentEventTargetRef()->GetWrapper()) {
    JSObject* wrapper = workerPrivate->ParentEventTargetRef()->GetWrapper();

    // If we're on the parent thread and have a reflector and a globalObject,
    // then the realms of cx, globalObject, and the worker's reflector
    // should all match.
    MOZ_ASSERT_IF(globalObject,
                  js::GetNonCCWObjectRealm(wrapper) == js::GetContextRealm(cx));
    MOZ_ASSERT_IF(globalObject,
                  js::GetNonCCWObjectRealm(wrapper) ==
                      js::GetNonCCWObjectRealm(
                          globalObject->GetGlobalJSObjectPreserveColor()));

    // If we're on the parent thread and have a reflector, then our
    // JSContext had better be either in the null realm (and hence
    // have no globalObject) or in the realm of our reflector.
    MOZ_ASSERT(!js::GetContextRealm(cx) ||
                   js::GetNonCCWObjectRealm(wrapper) == js::GetContextRealm(cx),
               "Must either be in the null compartment or in our reflector "
               "compartment");

    ar.emplace(cx, wrapper);
  }

  MOZ_ASSERT(!jsapi->HasException());
  result = WorkerRun(cx, workerPrivate);
  jsapi->ReportException();

  // It would be nice to avoid passing a JSContext to PostRun, but in the case
  // of ScriptExecutorRunnable we need to know the current compartment on the
  // JSContext (the one we set up based on the global returned from PreRun) so
  // that we can sanely do exception reporting.  In particular, we want to make
  // sure that we do our JS_SetPendingException while still in that compartment,
  // because otherwise we might end up trying to create a cross-compartment
  // wrapper when we try to move the JS exception from our runnable's
  // ErrorResult to the JSContext, and that's not desirable in this case.
  //
  // We _could_ skip passing a JSContext here and then in
  // ScriptExecutorRunnable::PostRun end up grabbing it from the WorkerPrivate
  // and looking at its current compartment.  But that seems like slightly weird
  // action-at-a-distance...
  //
  // In any case, we do NOT try to change the compartment on the JSContext at
  // this point; in the one case in which we could do that
  // (CompileScriptRunnable) it actually doesn't matter which compartment we're
  // in for PostRun.
  PostRun(cx, workerPrivate, result);
  MOZ_ASSERT(!jsapi->HasException());

  return result ? NS_OK : NS_ERROR_FAILURE;
}

nsresult WorkerParentThreadRunnable::Cancel() {
  LOG(("WorkerParentThreadRunnable::Cancel [%p]", this));
  return NS_OK;
}

WorkerParentControlRunnable::WorkerParentControlRunnable(const char* aName)
    : WorkerParentThreadRunnable(aName) {}

WorkerParentControlRunnable::~WorkerParentControlRunnable() = default;

nsresult WorkerParentControlRunnable::Cancel() {
  LOG(("WorkerParentControlRunnable::Cancel [%p]", this));
  if (NS_FAILED(Run())) {
    NS_WARNING("WorkerParentControlRunnable::Run() failed.");
  }
  return NS_OK;
}

WorkerThreadRunnable::WorkerThreadRunnable(const char* aName)
    : WorkerRunnable(aName), mCallingCancelWithinRun(false) {
  LOG(("WorkerThreadRunnable::WorkerThreadRunnable [%p]", this));
}

nsIGlobalObject* WorkerThreadRunnable::DefaultGlobalObject(
    WorkerPrivate* aWorkerPrivate) const {
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);
  if (IsDebuggerRunnable()) {
    return aWorkerPrivate->DebuggerGlobalScope();
  }
  return aWorkerPrivate->GlobalScope();
}

bool WorkerThreadRunnable::PreDispatch(WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);
#ifdef DEBUG
  aWorkerPrivate->AssertIsOnParentThread();
#endif
  return true;
}

bool WorkerThreadRunnable::DispatchInternal(WorkerPrivate* aWorkerPrivate) {
  LOG(("WorkerThreadRunnable::DispatchInternal [%p]", this));
  RefPtr<WorkerThreadRunnable> runnable(this);
  return NS_SUCCEEDED(aWorkerPrivate->Dispatch(runnable.forget()));
}

void WorkerThreadRunnable::PostDispatch(WorkerPrivate* aWorkerPrivate,
                                        bool aDispatchResult) {
  MOZ_ASSERT(aWorkerPrivate);
#ifdef DEBUG
  aWorkerPrivate->AssertIsOnParentThread();
#endif
}

bool WorkerThreadRunnable::PreRun(WorkerPrivate* aWorkerPrivate) {
  return true;
}

void WorkerThreadRunnable::PostRun(JSContext* aCx,
                                   WorkerPrivate* aWorkerPrivate,
                                   bool aRunResult) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aWorkerPrivate);

#ifdef DEBUG
  aWorkerPrivate->AssertIsOnWorkerThread();
#endif
}

NS_IMETHODIMP
WorkerThreadRunnable::Run() {
  LOG(("WorkerThreadRunnable::Run [%p]", this));

  // The Worker initialization fails, there is no valid WorkerPrivate and
  // WorkerJSContext to run this WorkerThreadRunnable.
  if (mCleanPreStartDispatching) {
    LOG(("Clean the pre-start dispatched WorkerThreadRunnable [%p]", this));
    return NS_OK;
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT_DEBUG_OR_FUZZING(workerPrivate);
#ifdef DEBUG
  workerPrivate->AssertIsOnWorkerThread();
#endif

  if (!mCallingCancelWithinRun &&
      workerPrivate->CancelBeforeWorkerScopeConstructed()) {
    mCallingCancelWithinRun = true;
    Cancel();
    mCallingCancelWithinRun = false;
    return NS_OK;
  }

  bool result = PreRun(workerPrivate);
  if (!result) {
    workerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(!JS_IsExceptionPending(workerPrivate->GetJSContext()));
    // We can't enter a useful realm on the JSContext here; just pass it
    // in as-is.
    PostRun(workerPrivate->GetJSContext(), workerPrivate, false);
    return NS_ERROR_FAILURE;
  }

  // Track down the appropriate global, if any, to use for the AutoEntryScript.
  nsCOMPtr<nsIGlobalObject> globalObject =
      workerPrivate->GetCurrentEventLoopGlobal();
  if (!globalObject) {
    globalObject = DefaultGlobalObject(workerPrivate);
    // Our worker thread may not be in a good state here if there is no
    // JSContext avaliable.  The way this manifests itself is that
    // globalObject ends up null (though it's not clear to me how we can be
    // running runnables at all when default globalObject(DebuggerGlobalScope
    // for debugger runnable, and GlobalScope for normal runnables) is returning
    // false!) and then when we try to init the AutoJSAPI either
    // CycleCollectedJSContext::Get() returns null or it has a null JSContext.
    // In any case, we used to have a check for
    // GetCurrentWorkerThreadJSContext() being non-null here and that seems to
    // avoid the problem, so let's keep doing that check even if we don't need
    // the JSContext here at all.
    if (NS_WARN_IF(!globalObject && !GetCurrentWorkerThreadJSContext())) {
      return NS_ERROR_FAILURE;
    }
  }

  // We might run script as part of WorkerRun, so we need an AutoEntryScript.
  // This is part of the HTML spec for workers at:
  // http://www.whatwg.org/specs/web-apps/current-work/#run-a-worker
  // If we don't have a globalObject we have to use an AutoJSAPI instead, but
  // this is OK as we won't be running script in these circumstances.
  Maybe<mozilla::dom::AutoJSAPI> maybeJSAPI;
  Maybe<mozilla::dom::AutoEntryScript> aes;
  JSContext* cx;
  AutoJSAPI* jsapi;
  if (globalObject) {
    aes.emplace(globalObject, "Worker runnable", false);
    jsapi = aes.ptr();
    cx = aes->cx();
  } else {
    maybeJSAPI.emplace();
    maybeJSAPI->Init();
    jsapi = maybeJSAPI.ptr();
    cx = jsapi->cx();
  }

  MOZ_ASSERT(!jsapi->HasException());
  result = WorkerRun(cx, workerPrivate);
  jsapi->ReportException();

  // We can't even assert that this didn't create our global, since in the case
  // of CompileScriptRunnable it _does_.

  // It would be nice to avoid passing a JSContext to PostRun, but in the case
  // of ScriptExecutorRunnable we need to know the current compartment on the
  // JSContext (the one we set up based on the global returned from PreRun) so
  // that we can sanely do exception reporting.  In particular, we want to make
  // sure that we do our JS_SetPendingException while still in that compartment,
  // because otherwise we might end up trying to create a cross-compartment
  // wrapper when we try to move the JS exception from our runnable's
  // ErrorResult to the JSContext, and that's not desirable in this case.
  //
  // We _could_ skip passing a JSContext here and then in
  // ScriptExecutorRunnable::PostRun end up grabbing it from the WorkerPrivate
  // and looking at its current compartment.  But that seems like slightly weird
  // action-at-a-distance...
  //
  // In any case, we do NOT try to change the compartment on the JSContext at
  // this point; in the one case in which we could do that
  // (CompileScriptRunnable) it actually doesn't matter which compartment we're
  // in for PostRun.
  PostRun(cx, workerPrivate, result);
  MOZ_ASSERT(!jsapi->HasException());

  return result ? NS_OK : NS_ERROR_FAILURE;
}

nsresult WorkerThreadRunnable::Cancel() {
  LOG(("WorkerThreadRunnable::Cancel [%p]", this));
  return NS_OK;
}

void WorkerDebuggerRunnable::PostDispatch(WorkerPrivate* aWorkerPrivate,
                                          bool aDispatchResult) {}

WorkerSyncRunnable::WorkerSyncRunnable(nsIEventTarget* aSyncLoopTarget,
                                       const char* aName)
    : WorkerThreadRunnable(aName), mSyncLoopTarget(aSyncLoopTarget) {}

WorkerSyncRunnable::WorkerSyncRunnable(
    nsCOMPtr<nsIEventTarget>&& aSyncLoopTarget, const char* aName)
    : WorkerThreadRunnable(aName),
      mSyncLoopTarget(std::move(aSyncLoopTarget)) {}

WorkerSyncRunnable::~WorkerSyncRunnable() = default;

bool WorkerSyncRunnable::DispatchInternal(WorkerPrivate* aWorkerPrivate) {
  if (mSyncLoopTarget) {
#ifdef DEBUG
    aWorkerPrivate->AssertValidSyncLoop(mSyncLoopTarget);
#endif
    RefPtr<WorkerSyncRunnable> runnable(this);
    return NS_SUCCEEDED(
        mSyncLoopTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));
  }

  return WorkerThreadRunnable::DispatchInternal(aWorkerPrivate);
}

void MainThreadWorkerSyncRunnable::PostDispatch(WorkerPrivate* aWorkerPrivate,
                                                bool aDispatchResult) {}

MainThreadStopSyncLoopRunnable::MainThreadStopSyncLoopRunnable(
    nsCOMPtr<nsIEventTarget>&& aSyncLoopTarget, nsresult aResult)
    : WorkerSyncRunnable(std::move(aSyncLoopTarget)), mResult(aResult) {
  LOG(("MainThreadStopSyncLoopRunnable::MainThreadStopSyncLoopRunnable [%p]",
       this));

  AssertIsOnMainThread();
}

nsresult MainThreadStopSyncLoopRunnable::Cancel() {
  LOG(("MainThreadStopSyncLoopRunnable::Cancel [%p]", this));
  nsresult rv = Run();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Run() failed");

  return rv;
}

bool MainThreadStopSyncLoopRunnable::WorkerRun(JSContext* aCx,
                                               WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mSyncLoopTarget);

  nsCOMPtr<nsIEventTarget> syncLoopTarget;
  mSyncLoopTarget.swap(syncLoopTarget);

  aWorkerPrivate->StopSyncLoop(syncLoopTarget, mResult);
  return true;
}

bool MainThreadStopSyncLoopRunnable::DispatchInternal(
    WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(mSyncLoopTarget);
#ifdef DEBUG
  aWorkerPrivate->AssertValidSyncLoop(mSyncLoopTarget);
#endif
  RefPtr<MainThreadStopSyncLoopRunnable> runnable(this);
  return NS_SUCCEEDED(
      mSyncLoopTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));
}

void MainThreadStopSyncLoopRunnable::PostDispatch(WorkerPrivate* aWorkerPrivate,
                                                  bool aDispatchResult) {}

WorkerControlRunnable::WorkerControlRunnable(const char* aName)
    : WorkerThreadRunnable(aName) {}

nsresult WorkerControlRunnable::Cancel() {
  LOG(("WorkerControlRunnable::Cancel [%p]", this));
  if (NS_FAILED(Run())) {
    NS_WARNING("WorkerControlRunnable::Run() failed.");
  }

  return NS_OK;
}

WorkerMainThreadRunnable::WorkerMainThreadRunnable(
    WorkerPrivate* aWorkerPrivate, const nsACString& aTelemetryKey)
    : mozilla::Runnable("dom::WorkerMainThreadRunnable"),
      mWorkerPrivate(aWorkerPrivate),
      mTelemetryKey(aTelemetryKey) {
  mWorkerPrivate->AssertIsOnWorkerThread();
}

WorkerMainThreadRunnable::~WorkerMainThreadRunnable() = default;

void WorkerMainThreadRunnable::Dispatch(WorkerStatus aFailStatus,
                                        mozilla::ErrorResult& aRv) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  TimeStamp startTime = TimeStamp::NowLoRes();

  AutoSyncLoopHolder syncLoop(mWorkerPrivate, aFailStatus);

  mSyncLoopTarget = syncLoop.GetSerialEventTarget();
  if (!mSyncLoopTarget) {
    // SyncLoop creation can fail if the worker is shutting down.
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  DebugOnly<nsresult> rv = mWorkerPrivate->DispatchToMainThread(this);
  MOZ_ASSERT(
      NS_SUCCEEDED(rv),
      "Should only fail after xpcom-shutdown-threads and we're gone by then");

  bool success = NS_SUCCEEDED(syncLoop.Run());

  Telemetry::Accumulate(
      Telemetry::SYNC_WORKER_OPERATION, mTelemetryKey,
      static_cast<uint32_t>(
          (TimeStamp::NowLoRes() - startTime).ToMilliseconds()));

  Unused << startTime;  // Shut the compiler up.

  if (!success) {
    aRv.ThrowUncatchableException();
  }
}

NS_IMETHODIMP
WorkerMainThreadRunnable::Run() {
  AssertIsOnMainThread();

  // This shouldn't be necessary once we're better about making sure no workers
  // are created during shutdown in earlier phases.
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMShutdownThreads)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  bool runResult = MainThreadRun();

  RefPtr<MainThreadStopSyncLoopRunnable> response =
      new MainThreadStopSyncLoopRunnable(std::move(mSyncLoopTarget),
                                         runResult ? NS_OK : NS_ERROR_FAILURE);

  MOZ_ALWAYS_TRUE(response->Dispatch(mWorkerPrivate));

  return NS_OK;
}

bool WorkerSameThreadRunnable::PreDispatch(WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();
  return true;
}

void WorkerSameThreadRunnable::PostDispatch(WorkerPrivate* aWorkerPrivate,
                                            bool aDispatchResult) {
  aWorkerPrivate->AssertIsOnWorkerThread();
}

WorkerProxyToMainThreadRunnable::WorkerProxyToMainThreadRunnable()
    : mozilla::Runnable("dom::WorkerProxyToMainThreadRunnable") {}

WorkerProxyToMainThreadRunnable::~WorkerProxyToMainThreadRunnable() = default;

bool WorkerProxyToMainThreadRunnable::Dispatch(WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<StrongWorkerRef> workerRef = StrongWorkerRef::Create(
      aWorkerPrivate, "WorkerProxyToMainThreadRunnable");
  if (NS_WARN_IF(!workerRef)) {
    RunBackOnWorkerThreadForCleanup(aWorkerPrivate);
    return false;
  }

  MOZ_ASSERT(!mWorkerRef);
  mWorkerRef = new ThreadSafeWorkerRef(workerRef);

  if (ForMessaging()
          ? NS_WARN_IF(NS_FAILED(
                aWorkerPrivate->DispatchToMainThreadForMessaging(this)))
          : NS_WARN_IF(NS_FAILED(aWorkerPrivate->DispatchToMainThread(this)))) {
    ReleaseWorker();
    RunBackOnWorkerThreadForCleanup(aWorkerPrivate);
    return false;
  }

  return true;
}

NS_IMETHODIMP
WorkerProxyToMainThreadRunnable::Run() {
  AssertIsOnMainThread();
  RunOnMainThread(mWorkerRef->Private());
  PostDispatchOnMainThread();
  return NS_OK;
}

void WorkerProxyToMainThreadRunnable::PostDispatchOnMainThread() {
  class ReleaseRunnable final : public MainThreadWorkerControlRunnable {
    RefPtr<WorkerProxyToMainThreadRunnable> mRunnable;

   public:
    explicit ReleaseRunnable(WorkerProxyToMainThreadRunnable* aRunnable)
        : MainThreadWorkerControlRunnable("ReleaseRunnable"),
          mRunnable(aRunnable) {
      MOZ_ASSERT(aRunnable);
    }

    virtual nsresult Cancel() override {
      MOZ_ASSERT(GetCurrentThreadWorkerPrivate());
      Unused << WorkerRun(nullptr, GetCurrentThreadWorkerPrivate());
      return NS_OK;
    }

    virtual bool WorkerRun(JSContext* aCx,
                           WorkerPrivate* aWorkerPrivate) override {
      MOZ_ASSERT(aWorkerPrivate);
      aWorkerPrivate->AssertIsOnWorkerThread();

      if (mRunnable) {
        mRunnable->RunBackOnWorkerThreadForCleanup(aWorkerPrivate);

        // Let's release the worker thread.
        mRunnable->ReleaseWorker();
        mRunnable = nullptr;
      }

      return true;
    }

   private:
    ~ReleaseRunnable() = default;
  };

  RefPtr<WorkerControlRunnable> runnable = new ReleaseRunnable(this);
  Unused << NS_WARN_IF(!runnable->Dispatch(mWorkerRef->Private()));
}

void WorkerProxyToMainThreadRunnable::ReleaseWorker() { mWorkerRef = nullptr; }

}  // namespace mozilla::dom
