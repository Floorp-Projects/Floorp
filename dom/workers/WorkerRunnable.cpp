/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerRunnable.h"

#include "WorkerPrivate.h"
#include "WorkerScope.h"
#include "js/RootingAPI.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ErrorResult.h"
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

namespace mozilla {
namespace dom {

namespace {

const nsIID kWorkerRunnableIID = {
    0x320cc0b5,
    0xef12,
    0x4084,
    {0x88, 0x6e, 0xca, 0x6a, 0x81, 0xe4, 0x1d, 0x68}};

}  // namespace

#ifdef DEBUG
WorkerRunnable::WorkerRunnable(WorkerPrivate* aWorkerPrivate,
                               TargetAndBusyBehavior aBehavior)
    : mWorkerPrivate(aWorkerPrivate),
      mBehavior(aBehavior),
      mCanceled(0),
      mCallingCancelWithinRun(false) {
  MOZ_ASSERT(aWorkerPrivate);
}
#endif

bool WorkerRunnable::IsDebuggerRunnable() const { return false; }

nsIGlobalObject* WorkerRunnable::DefaultGlobalObject() const {
  if (IsDebuggerRunnable()) {
    return mWorkerPrivate->DebuggerGlobalScope();
  } else {
    return mWorkerPrivate->GlobalScope();
  }
}

bool WorkerRunnable::PreDispatch(WorkerPrivate* aWorkerPrivate) {
#ifdef DEBUG
  MOZ_ASSERT(aWorkerPrivate);

  switch (mBehavior) {
    case ParentThreadUnchangedBusyCount:
      aWorkerPrivate->AssertIsOnWorkerThread();
      break;

    case WorkerThreadModifyBusyCount:
    case WorkerThreadUnchangedBusyCount:
      aWorkerPrivate->AssertIsOnParentThread();
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Unknown behavior!");
  }
#endif

  if (mBehavior == WorkerThreadModifyBusyCount) {
    return aWorkerPrivate->ModifyBusyCount(true);
  }

  return true;
}

bool WorkerRunnable::Dispatch() {
  bool ok = PreDispatch(mWorkerPrivate);
  if (ok) {
    ok = DispatchInternal();
  }
  PostDispatch(mWorkerPrivate, ok);
  return ok;
}

bool WorkerRunnable::DispatchInternal() {
  RefPtr<WorkerRunnable> runnable(this);

  if (mBehavior == WorkerThreadModifyBusyCount ||
      mBehavior == WorkerThreadUnchangedBusyCount) {
    if (IsDebuggerRunnable()) {
      return NS_SUCCEEDED(
          mWorkerPrivate->DispatchDebuggerRunnable(runnable.forget()));
    } else {
      return NS_SUCCEEDED(mWorkerPrivate->Dispatch(runnable.forget()));
    }
  }

  MOZ_ASSERT(mBehavior == ParentThreadUnchangedBusyCount);

  if (WorkerPrivate* parent = mWorkerPrivate->GetParent()) {
    return NS_SUCCEEDED(parent->Dispatch(runnable.forget()));
  }

  if (IsDebuggeeRunnable()) {
    RefPtr<WorkerDebuggeeRunnable> debuggeeRunnable =
        runnable.forget().downcast<WorkerDebuggeeRunnable>();
    return NS_SUCCEEDED(mWorkerPrivate->DispatchDebuggeeToMainThread(
        debuggeeRunnable.forget(), NS_DISPATCH_NORMAL));
  }

  return NS_SUCCEEDED(mWorkerPrivate->DispatchToMainThread(runnable.forget()));
}

void WorkerRunnable::PostDispatch(WorkerPrivate* aWorkerPrivate,
                                  bool aDispatchResult) {
  MOZ_ASSERT(aWorkerPrivate);

#ifdef DEBUG
  switch (mBehavior) {
    case ParentThreadUnchangedBusyCount:
      aWorkerPrivate->AssertIsOnWorkerThread();
      break;

    case WorkerThreadModifyBusyCount:
      aWorkerPrivate->AssertIsOnParentThread();
      break;

    case WorkerThreadUnchangedBusyCount:
      aWorkerPrivate->AssertIsOnParentThread();
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Unknown behavior!");
  }
#endif

  if (!aDispatchResult) {
    if (mBehavior == WorkerThreadModifyBusyCount) {
      aWorkerPrivate->ModifyBusyCount(false);
    }
  }
}

bool WorkerRunnable::PreRun(WorkerPrivate* aWorkerPrivate) { return true; }

void WorkerRunnable::PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                             bool aRunResult) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aWorkerPrivate);

#ifdef DEBUG
  switch (mBehavior) {
    case ParentThreadUnchangedBusyCount:
      aWorkerPrivate->AssertIsOnParentThread();
      break;

    case WorkerThreadModifyBusyCount:
      aWorkerPrivate->AssertIsOnWorkerThread();
      break;

    case WorkerThreadUnchangedBusyCount:
      aWorkerPrivate->AssertIsOnWorkerThread();
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Unknown behavior!");
  }
#endif

  if (mBehavior == WorkerThreadModifyBusyCount) {
    aWorkerPrivate->ModifyBusyCountFromWorker(false);
  }
}

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

NS_IMPL_ADDREF(WorkerRunnable)
NS_IMPL_RELEASE(WorkerRunnable)

NS_INTERFACE_MAP_BEGIN(WorkerRunnable)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY(nsICancelableRunnable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRunnable)
  // kWorkerRunnableIID is special in that it does not AddRef its result.
  if (aIID.Equals(kWorkerRunnableIID)) {
    *aInstancePtr = this;
    return NS_OK;
  } else
NS_INTERFACE_MAP_END

NS_IMETHODIMP
WorkerRunnable::Run() {
  bool targetIsWorkerThread = mBehavior == WorkerThreadModifyBusyCount ||
                              mBehavior == WorkerThreadUnchangedBusyCount;

#ifdef DEBUG
  MOZ_ASSERT_IF(mCallingCancelWithinRun, targetIsWorkerThread);
  if (targetIsWorkerThread) {
    mWorkerPrivate->AssertIsOnWorkerThread();
  } else {
    MOZ_ASSERT(mBehavior == ParentThreadUnchangedBusyCount);
    mWorkerPrivate->AssertIsOnParentThread();
  }
#endif

  if (IsCanceled() && !mCallingCancelWithinRun) {
    return NS_OK;
  }

  if (targetIsWorkerThread &&
      mWorkerPrivate->AllPendingRunnablesShouldBeCanceled() && !IsCanceled() &&
      !mCallingCancelWithinRun) {
    // Prevent recursion.
    mCallingCancelWithinRun = true;

    Cancel();

    MOZ_ASSERT(mCallingCancelWithinRun);
    mCallingCancelWithinRun = false;

    MOZ_ASSERT(IsCanceled(), "Subclass Cancel() didn't set IsCanceled()!");

    if (mBehavior == WorkerThreadModifyBusyCount) {
      mWorkerPrivate->ModifyBusyCountFromWorker(false);
    }

    return NS_OK;
  }

  bool result = PreRun(mWorkerPrivate);
  if (!result) {
    MOZ_ASSERT(targetIsWorkerThread,
               "The only PreRun implementation that can fail is "
               "ScriptExecutorRunnable");
    mWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(!JS_IsExceptionPending(mWorkerPrivate->GetJSContext()));
    // We can't enter a useful realm on the JSContext here; just pass it
    // in as-is.
    PostRun(mWorkerPrivate->GetJSContext(), mWorkerPrivate, false);
    return NS_ERROR_FAILURE;
  }

  // Track down the appropriate global, if any, to use for the AutoEntryScript.
  nsCOMPtr<nsIGlobalObject> globalObject;
  bool isMainThread = !targetIsWorkerThread && !mWorkerPrivate->GetParent();
  MOZ_ASSERT(isMainThread == NS_IsMainThread());
  RefPtr<WorkerPrivate> kungFuDeathGrip;
  if (targetIsWorkerThread) {
    globalObject = mWorkerPrivate->GetCurrentEventLoopGlobal();
    if (!globalObject) {
      globalObject = DefaultGlobalObject();
      // Our worker thread may not be in a good state here if there is no
      // JSContext avaliable.  The way this manifests itself is that
      // globalObject ends up null (though it's not clear to me how we can be
      // running runnables at all when DefaultGlobalObject() is returning
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

    // We may still not have a globalObject here: in the case of
    // CompileScriptRunnable, we don't actually create the global object until
    // we have the script data, which happens in a syncloop under
    // CompileScriptRunnable::WorkerRun, so we can't assert that it got created
    // in the PreRun call above.
  } else {
    kungFuDeathGrip = mWorkerPrivate;
    if (isMainThread) {
      globalObject = nsGlobalWindowInner::Cast(mWorkerPrivate->GetWindow());
    } else {
      globalObject = mWorkerPrivate->GetParent()->GlobalScope();
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
    aes.emplace(globalObject, "Worker runnable", isMainThread);
    jsapi = aes.ptr();
    cx = aes->cx();
  } else {
    maybeJSAPI.emplace();
    maybeJSAPI->Init();
    jsapi = maybeJSAPI.ptr();
    cx = jsapi->cx();
  }

  // Note that we can't assert anything about
  // mWorkerPrivate->ParentEventTargetRef()->GetWrapper()
  // existing, since it may in fact have been GCed (and we may be one of the
  // runnables cleaning up the worker as a result).

  // If we are on the parent thread and that thread is not the main thread,
  // then we must be a dedicated worker (because there are no
  // Shared/ServiceWorkers whose parent is itself a worker) and then we
  // definitely have a globalObject.  If it _is_ the main thread, globalObject
  // can be null for workers started from JSMs or other non-window contexts,
  // sadly.
  MOZ_ASSERT_IF(!targetIsWorkerThread && !isMainThread,
                mWorkerPrivate->IsDedicatedWorker() && globalObject);

  // If we're on the parent thread we might be in a null realm in the
  // situation described above when globalObject is null.  Make sure to enter
  // the realm of the worker's reflector if there is one.  There might
  // not be one if we're just starting to compile the script for this worker.
  Maybe<JSAutoRealm> ar;
  if (!targetIsWorkerThread && mWorkerPrivate->IsDedicatedWorker() &&
      mWorkerPrivate->ParentEventTargetRef()->GetWrapper()) {
    JSObject* wrapper = mWorkerPrivate->ParentEventTargetRef()->GetWrapper();

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
  result = WorkerRun(cx, mWorkerPrivate);
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
  PostRun(cx, mWorkerPrivate, result);
  MOZ_ASSERT(!jsapi->HasException());

  return result ? NS_OK : NS_ERROR_FAILURE;
}

nsresult WorkerRunnable::Cancel() {
  uint32_t canceledCount = ++mCanceled;

  MOZ_ASSERT(canceledCount, "Cancel() overflow!");

  // The docs say that Cancel() should not be called more than once and that we
  // should throw NS_ERROR_UNEXPECTED if it is.
  return (canceledCount == 1) ? NS_OK : NS_ERROR_UNEXPECTED;
}

void WorkerDebuggerRunnable::PostDispatch(WorkerPrivate* aWorkerPrivate,
                                          bool aDispatchResult) {}

WorkerSyncRunnable::WorkerSyncRunnable(WorkerPrivate* aWorkerPrivate,
                                       nsIEventTarget* aSyncLoopTarget)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
      mSyncLoopTarget(aSyncLoopTarget) {
#ifdef DEBUG
  if (mSyncLoopTarget) {
    mWorkerPrivate->AssertValidSyncLoop(mSyncLoopTarget);
  }
#endif
}

WorkerSyncRunnable::WorkerSyncRunnable(
    WorkerPrivate* aWorkerPrivate, nsCOMPtr<nsIEventTarget>&& aSyncLoopTarget)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
      mSyncLoopTarget(std::move(aSyncLoopTarget)) {
#ifdef DEBUG
  if (mSyncLoopTarget) {
    mWorkerPrivate->AssertValidSyncLoop(mSyncLoopTarget);
  }
#endif
}

WorkerSyncRunnable::~WorkerSyncRunnable() = default;

bool WorkerSyncRunnable::DispatchInternal() {
  if (mSyncLoopTarget) {
    RefPtr<WorkerSyncRunnable> runnable(this);
    return NS_SUCCEEDED(
        mSyncLoopTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));
  }

  return WorkerRunnable::DispatchInternal();
}

void MainThreadWorkerSyncRunnable::PostDispatch(WorkerPrivate* aWorkerPrivate,
                                                bool aDispatchResult) {}

MainThreadStopSyncLoopRunnable::MainThreadStopSyncLoopRunnable(
    WorkerPrivate* aWorkerPrivate, nsCOMPtr<nsIEventTarget>&& aSyncLoopTarget,
    bool aResult)
    : WorkerSyncRunnable(aWorkerPrivate, std::move(aSyncLoopTarget)),
      mResult(aResult) {
  AssertIsOnMainThread();
#ifdef DEBUG
  mWorkerPrivate->AssertValidSyncLoop(mSyncLoopTarget);
#endif
}

nsresult MainThreadStopSyncLoopRunnable::Cancel() {
  nsresult rv = Run();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Run() failed");

  nsresult rv2 = WorkerSyncRunnable::Cancel();
  NS_WARNING_ASSERTION(NS_SUCCEEDED(rv2), "Cancel() failed");

  return NS_FAILED(rv) ? rv : rv2;
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

bool MainThreadStopSyncLoopRunnable::DispatchInternal() {
  MOZ_ASSERT(mSyncLoopTarget);

  RefPtr<MainThreadStopSyncLoopRunnable> runnable(this);
  return NS_SUCCEEDED(
      mSyncLoopTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));
}

void MainThreadStopSyncLoopRunnable::PostDispatch(WorkerPrivate* aWorkerPrivate,
                                                  bool aDispatchResult) {}

#ifdef DEBUG
WorkerControlRunnable::WorkerControlRunnable(WorkerPrivate* aWorkerPrivate,
                                             TargetAndBusyBehavior aBehavior)
    : WorkerRunnable(aWorkerPrivate, aBehavior) {
  MOZ_ASSERT(aWorkerPrivate);
  MOZ_ASSERT(aBehavior == ParentThreadUnchangedBusyCount ||
                 aBehavior == WorkerThreadUnchangedBusyCount,
             "WorkerControlRunnables should not modify the busy count");
}
#endif

nsresult WorkerControlRunnable::Cancel() {
  if (NS_FAILED(Run())) {
    NS_WARNING("WorkerControlRunnable::Run() failed.");
  }

  return WorkerRunnable::Cancel();
}

bool WorkerControlRunnable::DispatchInternal() {
  RefPtr<WorkerControlRunnable> runnable(this);

  if (mBehavior == WorkerThreadUnchangedBusyCount) {
    return NS_SUCCEEDED(
        mWorkerPrivate->DispatchControlRunnable(runnable.forget()));
  }

  if (WorkerPrivate* parent = mWorkerPrivate->GetParent()) {
    return NS_SUCCEEDED(parent->DispatchControlRunnable(runnable.forget()));
  }

  return NS_SUCCEEDED(mWorkerPrivate->DispatchToMainThread(runnable.forget()));
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

  mSyncLoopTarget = syncLoop.GetEventTarget();
  if (!mSyncLoopTarget) {
    // SyncLoop creation can fail if the worker is shutting down.
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  DebugOnly<nsresult> rv = mWorkerPrivate->DispatchToMainThread(this);
  MOZ_ASSERT(
      NS_SUCCEEDED(rv),
      "Should only fail after xpcom-shutdown-threads and we're gone by then");

  bool success = syncLoop.Run();

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

  bool runResult = MainThreadRun();

  RefPtr<MainThreadStopSyncLoopRunnable> response =
      new MainThreadStopSyncLoopRunnable(mWorkerPrivate,
                                         std::move(mSyncLoopTarget), runResult);

  MOZ_ALWAYS_TRUE(response->Dispatch());

  return NS_OK;
}

bool WorkerSameThreadRunnable::PreDispatch(WorkerPrivate* aWorkerPrivate) {
  // We don't call WorkerRunnable::PreDispatch, because we're using
  // WorkerThreadModifyBusyCount for mBehavior, and WorkerRunnable will assert
  // that PreDispatch is on the parent thread in that case.
  aWorkerPrivate->AssertIsOnWorkerThread();
  return true;
}

void WorkerSameThreadRunnable::PostDispatch(WorkerPrivate* aWorkerPrivate,
                                            bool aDispatchResult) {
  // We don't call WorkerRunnable::PostDispatch, because we're using
  // WorkerThreadModifyBusyCount for mBehavior, and WorkerRunnable will assert
  // that PostDispatch is on the parent thread in that case.
  aWorkerPrivate->AssertIsOnWorkerThread();
  if (aDispatchResult) {
    DebugOnly<bool> willIncrement =
        aWorkerPrivate->ModifyBusyCountFromWorker(true);
    // Should never fail since if this thread is still running, so should the
    // parent and it should be able to process a control runnable.
    MOZ_ASSERT(willIncrement);
  }
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
    ReleaseRunnable(WorkerPrivate* aWorkerPrivate,
                    WorkerProxyToMainThreadRunnable* aRunnable)
        : MainThreadWorkerControlRunnable(aWorkerPrivate),
          mRunnable(aRunnable) {
      MOZ_ASSERT(aRunnable);
    }

    // We must call RunBackOnWorkerThreadForCleanup() also if the runnable is
    // canceled.
    nsresult Cancel() override {
      WorkerRun(nullptr, mWorkerPrivate);
      return MainThreadWorkerControlRunnable::Cancel();
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

  RefPtr<WorkerControlRunnable> runnable =
      new ReleaseRunnable(mWorkerRef->Private(), this);
  Unused << NS_WARN_IF(!runnable->Dispatch());
}

void WorkerProxyToMainThreadRunnable::ReleaseWorker() { mWorkerRef = nullptr; }

bool WorkerDebuggeeRunnable::PreDispatch(WorkerPrivate* aWorkerPrivate) {
  if (mBehavior == ParentThreadUnchangedBusyCount) {
    RefPtr<StrongWorkerRef> strongRef = StrongWorkerRef::Create(
        aWorkerPrivate, "WorkerDebuggeeRunnable::mSender");
    if (!strongRef) {
      return false;
    }

    mSender = new ThreadSafeWorkerRef(strongRef);
  }

  return WorkerRunnable::PreDispatch(aWorkerPrivate);
}

}  // namespace dom
}  // namespace mozilla
