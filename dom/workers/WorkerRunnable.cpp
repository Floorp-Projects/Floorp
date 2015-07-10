/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerRunnable.h"

#include "nsGlobalWindow.h"
#include "nsIEventTarget.h"
#include "nsIGlobalObject.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/dom/ScriptSettings.h"

#include "js/RootingAPI.h"
#include "js/Value.h"

#include "WorkerPrivate.h"
#include "WorkerScope.h"

USING_WORKERS_NAMESPACE

namespace {

const nsIID kWorkerRunnableIID = {
  0x320cc0b5, 0xef12, 0x4084, { 0x88, 0x6e, 0xca, 0x6a, 0x81, 0xe4, 0x1d, 0x68 }
};

void
MaybeReportMainThreadException(JSContext* aCx, bool aResult)
{
  AssertIsOnMainThread();

  if (aCx && !aResult) {
    JS_ReportPendingException(aCx);
  }
}

} // anonymous namespace

#ifdef DEBUG
WorkerRunnable::WorkerRunnable(WorkerPrivate* aWorkerPrivate,
                               TargetAndBusyBehavior aBehavior)
: mWorkerPrivate(aWorkerPrivate), mBehavior(aBehavior), mCanceled(0),
  mCallingCancelWithinRun(false)
{
  MOZ_ASSERT(aWorkerPrivate);
}
#endif

bool
WorkerRunnable::IsDebuggerRunnable() const
{
  return false;
}

nsIGlobalObject*
WorkerRunnable::DefaultGlobalObject() const
{
  if (IsDebuggerRunnable()) {
    return mWorkerPrivate->DebuggerGlobalScope();
  } else {
    return mWorkerPrivate->GlobalScope();
  }
}

bool
WorkerRunnable::PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
#ifdef DEBUG
  MOZ_ASSERT(aWorkerPrivate);

  switch (mBehavior) {
    case ParentThreadUnchangedBusyCount:
      aWorkerPrivate->AssertIsOnWorkerThread();
      break;

    case WorkerThreadModifyBusyCount:
      aWorkerPrivate->AssertIsOnParentThread();
      MOZ_ASSERT(aCx);
      break;

    case WorkerThreadUnchangedBusyCount:
      aWorkerPrivate->AssertIsOnParentThread();
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("Unknown behavior!");
  }
#endif

  if (mBehavior == WorkerThreadModifyBusyCount) {
    return aWorkerPrivate->ModifyBusyCount(aCx, true);
  }

  return true;
}

bool
WorkerRunnable::Dispatch(JSContext* aCx)
{
  bool ok;

  if (!aCx) {
    ok = PreDispatch(nullptr, mWorkerPrivate);
    if (ok) {
      ok = DispatchInternal();
    }
    PostDispatch(nullptr, mWorkerPrivate, ok);
    return ok;
  }

  JSAutoRequest ar(aCx);

  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));

  Maybe<JSAutoCompartment> ac;
  if (global) {
    ac.emplace(aCx, global);
  }

  ok = PreDispatch(aCx, mWorkerPrivate);

  if (ok && !DispatchInternal()) {
    ok = false;
  }

  PostDispatch(aCx, mWorkerPrivate, ok);

  return ok;
}

bool
WorkerRunnable::DispatchInternal()
{
  nsRefPtr<WorkerRunnable> runnable(this);

  if (mBehavior == WorkerThreadModifyBusyCount ||
      mBehavior == WorkerThreadUnchangedBusyCount) {
    if (IsDebuggerRunnable()) {
      return NS_SUCCEEDED(mWorkerPrivate->DispatchDebuggerRunnable(runnable.forget()));
    } else {
      return NS_SUCCEEDED(mWorkerPrivate->Dispatch(runnable.forget()));
    }
  }

  MOZ_ASSERT(mBehavior == ParentThreadUnchangedBusyCount);

  if (WorkerPrivate* parent = mWorkerPrivate->GetParent()) {
    return NS_SUCCEEDED(parent->Dispatch(runnable.forget()));
  }

  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  MOZ_ASSERT(mainThread);

  return NS_SUCCEEDED(mainThread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));
}

void
WorkerRunnable::PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                             bool aDispatchResult)
{
  MOZ_ASSERT(aWorkerPrivate);

#ifdef DEBUG
  switch (mBehavior) {
    case ParentThreadUnchangedBusyCount:
      aWorkerPrivate->AssertIsOnWorkerThread();
      break;

    case WorkerThreadModifyBusyCount:
      aWorkerPrivate->AssertIsOnParentThread();
      MOZ_ASSERT(aCx);
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
      aWorkerPrivate->ModifyBusyCount(aCx, false);
    }
    if (aCx) {
      JS_ReportPendingException(aCx);
    }
  }
}

void
WorkerRunnable::PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                        bool aRunResult)
{
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
    if (!aWorkerPrivate->ModifyBusyCountFromWorker(aCx, false)) {
      aRunResult = false;
    }
  }

  if (!aRunResult) {
    JS_ReportPendingException(aCx);
  }
}

// static
WorkerRunnable*
WorkerRunnable::FromRunnable(nsIRunnable* aRunnable)
{
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
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  // kWorkerRunnableIID is special in that it does not AddRef its result.
  if (aIID.Equals(kWorkerRunnableIID)) {
    *aInstancePtr = this;
    return NS_OK;
  }
  else
NS_INTERFACE_MAP_END

NS_IMETHODIMP
WorkerRunnable::Run()
{
  bool targetIsWorkerThread = mBehavior == WorkerThreadModifyBusyCount ||
                              mBehavior == WorkerThreadUnchangedBusyCount;

#ifdef DEBUG
  MOZ_ASSERT_IF(mCallingCancelWithinRun, targetIsWorkerThread);
  if (targetIsWorkerThread) {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }
  else {
    MOZ_ASSERT(mBehavior == ParentThreadUnchangedBusyCount);
    mWorkerPrivate->AssertIsOnParentThread();
  }
#endif

  if (IsCanceled() && !mCallingCancelWithinRun) {
    return NS_OK;
  }

  if (targetIsWorkerThread &&
      mWorkerPrivate->AllPendingRunnablesShouldBeCanceled() &&
      !IsCanceled() && !mCallingCancelWithinRun) {

    // Prevent recursion.
    mCallingCancelWithinRun = true;

    Cancel();

    MOZ_ASSERT(mCallingCancelWithinRun);
    mCallingCancelWithinRun = false;

    MOZ_ASSERT(IsCanceled(), "Subclass Cancel() didn't set IsCanceled()!");

    return NS_OK;
  }

  // Track down the appropriate global to use for the AutoJSAPI/AutoEntryScript.
  nsCOMPtr<nsIGlobalObject> globalObject;
  bool isMainThread = !targetIsWorkerThread && !mWorkerPrivate->GetParent();
  MOZ_ASSERT(isMainThread == NS_IsMainThread());
  nsRefPtr<WorkerPrivate> kungFuDeathGrip;
  if (targetIsWorkerThread) {
    JSContext* cx = GetCurrentThreadJSContext();
    if (NS_WARN_IF(!cx)) {
      return NS_ERROR_FAILURE;
    }

    JSObject* global = JS::CurrentGlobalOrNull(cx);
    if (global) {
      globalObject = GetGlobalObjectForGlobal(global);
    } else {
      globalObject = DefaultGlobalObject();
    }
  } else {
    kungFuDeathGrip = mWorkerPrivate;
    if (isMainThread) {
      globalObject = static_cast<nsGlobalWindow*>(mWorkerPrivate->GetWindow());
    } else {
      globalObject = mWorkerPrivate->GetParent()->GlobalScope();
    }
  }

  // We might run script as part of WorkerRun, so we need an AutoEntryScript.
  // This is part of the HTML spec for workers at:
  // http://www.whatwg.org/specs/web-apps/current-work/#run-a-worker
  // If we don't have a globalObject we have to use an AutoJSAPI instead, but
  // this is OK as we won't be running script in these circumstances.
  // It's important that aes is declared after jsapi, because if WorkerRun
  // creates a global then we construct aes before PostRun and we need them to
  // be destroyed in the correct order.
  mozilla::dom::AutoJSAPI jsapi;
  Maybe<mozilla::dom::AutoEntryScript> aes;
  JSContext* cx;
  if (globalObject) {
    aes.emplace(globalObject, "Worker runnable",
                isMainThread,
                isMainThread ? nullptr : GetCurrentThreadJSContext());
    cx = aes->cx();
  } else {
    jsapi.Init();
    cx = jsapi.cx();
  }

  // If we're not on the worker thread we'll either be in our parent's
  // compartment or the null compartment, so we need to enter our own.
  Maybe<JSAutoCompartment> ac;
  if (!targetIsWorkerThread && mWorkerPrivate->GetWrapper()) {
    ac.emplace(cx, mWorkerPrivate->GetWrapper());
  }

  bool result = WorkerRun(cx, mWorkerPrivate);

  // In the case of CompileScriptRunnnable, WorkerRun above can cause us to
  // lazily create a global, so we construct aes here before calling PostRun.
  if (targetIsWorkerThread && !aes && DefaultGlobalObject()) {
    aes.emplace(DefaultGlobalObject(), "worker runnable",
                false, GetCurrentThreadJSContext());
    cx = aes->cx();
  }

  PostRun(cx, mWorkerPrivate, result);

  return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
WorkerRunnable::Cancel()
{
  uint32_t canceledCount = ++mCanceled;

  MOZ_ASSERT(canceledCount, "Cancel() overflow!");

  // The docs say that Cancel() should not be called more than once and that we
  // should throw NS_ERROR_UNEXPECTED if it is.
  return (canceledCount == 1) ? NS_OK : NS_ERROR_UNEXPECTED;
}

void
WorkerDebuggerRunnable::PostDispatch(JSContext* aCx,
                                     WorkerPrivate* aWorkerPrivate,
                                     bool aDispatchResult)
{
  MaybeReportMainThreadException(aCx, aDispatchResult);
}

WorkerSyncRunnable::WorkerSyncRunnable(WorkerPrivate* aWorkerPrivate,
                                       nsIEventTarget* aSyncLoopTarget)
: WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
  mSyncLoopTarget(aSyncLoopTarget)
{
#ifdef DEBUG
  if (mSyncLoopTarget) {
    mWorkerPrivate->AssertValidSyncLoop(mSyncLoopTarget);
  }
#endif
}

WorkerSyncRunnable::WorkerSyncRunnable(
                               WorkerPrivate* aWorkerPrivate,
                               already_AddRefed<nsIEventTarget>&& aSyncLoopTarget)
: WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
  mSyncLoopTarget(aSyncLoopTarget)
{
#ifdef DEBUG
  if (mSyncLoopTarget) {
    mWorkerPrivate->AssertValidSyncLoop(mSyncLoopTarget);
  }
#endif
}

WorkerSyncRunnable::~WorkerSyncRunnable()
{
}

bool
WorkerSyncRunnable::DispatchInternal()
{
  if (mSyncLoopTarget) {
    nsRefPtr<WorkerSyncRunnable> runnable(this);
    return NS_SUCCEEDED(mSyncLoopTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));
  }

  return WorkerRunnable::DispatchInternal();
}

void
MainThreadWorkerSyncRunnable::PostDispatch(JSContext* aCx,
                                           WorkerPrivate* aWorkerPrivate,
                                           bool aDispatchResult)
{
  MaybeReportMainThreadException(aCx, aDispatchResult);
}

StopSyncLoopRunnable::StopSyncLoopRunnable(
                               WorkerPrivate* aWorkerPrivate,
                               already_AddRefed<nsIEventTarget>&& aSyncLoopTarget,
                               bool aResult)
: WorkerSyncRunnable(aWorkerPrivate, Move(aSyncLoopTarget)), mResult(aResult)
{
#ifdef DEBUG
  mWorkerPrivate->AssertValidSyncLoop(mSyncLoopTarget);
#endif
}

NS_IMETHODIMP
StopSyncLoopRunnable::Cancel()
{
  nsresult rv = Run();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv), "Run() failed");

  nsresult rv2 = WorkerSyncRunnable::Cancel();
  NS_WARN_IF_FALSE(NS_SUCCEEDED(rv2), "Cancel() failed");

  return NS_FAILED(rv) ? rv : rv2;
}

bool
StopSyncLoopRunnable::WorkerRun(JSContext* aCx,
                                WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_ASSERT(mSyncLoopTarget);

  nsCOMPtr<nsIEventTarget> syncLoopTarget;
  mSyncLoopTarget.swap(syncLoopTarget);

  if (!mResult) {
    MaybeSetException(aCx);
  }

  aWorkerPrivate->StopSyncLoop(syncLoopTarget, mResult);
  return true;
}

bool
StopSyncLoopRunnable::DispatchInternal()
{
  MOZ_ASSERT(mSyncLoopTarget);

  nsRefPtr<StopSyncLoopRunnable> runnable(this);
  return NS_SUCCEEDED(mSyncLoopTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));
}

void
MainThreadStopSyncLoopRunnable::PostDispatch(JSContext* aCx,
                                             WorkerPrivate* aWorkerPrivate,
                                             bool aDispatchResult)
{
  MaybeReportMainThreadException(aCx, aDispatchResult);
}

#ifdef DEBUG
WorkerControlRunnable::WorkerControlRunnable(WorkerPrivate* aWorkerPrivate,
                                             TargetAndBusyBehavior aBehavior)
: WorkerRunnable(aWorkerPrivate, aBehavior)
{
  MOZ_ASSERT(aWorkerPrivate);
  MOZ_ASSERT(aBehavior == ParentThreadUnchangedBusyCount ||
             aBehavior == WorkerThreadUnchangedBusyCount,
             "WorkerControlRunnables should not modify the busy count");
}
#endif

NS_IMETHODIMP
WorkerControlRunnable::Cancel()
{
  if (NS_FAILED(Run())) {
    NS_WARNING("WorkerControlRunnable::Run() failed.");
  }

  return WorkerRunnable::Cancel();
}

bool
WorkerControlRunnable::DispatchInternal()
{
  nsRefPtr<WorkerControlRunnable> runnable(this);

  if (mBehavior == WorkerThreadUnchangedBusyCount) {
    return NS_SUCCEEDED(mWorkerPrivate->DispatchControlRunnable(runnable.forget()));
  }

  if (WorkerPrivate* parent = mWorkerPrivate->GetParent()) {
    return NS_SUCCEEDED(parent->DispatchControlRunnable(runnable.forget()));
  }

  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  MOZ_ASSERT(mainThread);

  return NS_SUCCEEDED(mainThread->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));
}

void
MainThreadWorkerControlRunnable::PostDispatch(JSContext* aCx,
                                              WorkerPrivate* aWorkerPrivate,
                                              bool aDispatchResult)
{
  AssertIsOnMainThread();

  if (aCx && !aDispatchResult) {
    JS_ReportPendingException(aCx);
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(WorkerControlRunnable, WorkerRunnable)

WorkerMainThreadRunnable::WorkerMainThreadRunnable(WorkerPrivate* aWorkerPrivate)
: mWorkerPrivate(aWorkerPrivate)
{
  mWorkerPrivate->AssertIsOnWorkerThread();
}

bool
WorkerMainThreadRunnable::Dispatch(JSContext* aCx)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  AutoSyncLoopHolder syncLoop(mWorkerPrivate);

  mSyncLoopTarget = syncLoop.EventTarget();
  nsRefPtr<WorkerMainThreadRunnable> runnable(this);

  if (NS_FAILED(NS_DispatchToMainThread(runnable.forget(), NS_DISPATCH_NORMAL))) {
    JS_ReportError(aCx, "Failed to dispatch to main thread!");
    return false;
  }

  return syncLoop.Run();
}

NS_IMETHODIMP
WorkerMainThreadRunnable::Run()
{
  AssertIsOnMainThread();

  bool runResult = MainThreadRun();

  nsRefPtr<MainThreadStopSyncLoopRunnable> response =
    new MainThreadStopSyncLoopRunnable(mWorkerPrivate,
                                       mSyncLoopTarget.forget(),
                                       runResult);

  MOZ_ALWAYS_TRUE(response->Dispatch(nullptr));

  return NS_OK;
}

bool
WorkerSameThreadRunnable::PreDispatch(JSContext* aCx,
                                      WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  return true;
}

void
WorkerSameThreadRunnable::PostDispatch(JSContext* aCx,
                                       WorkerPrivate* aWorkerPrivate,
                                       bool aDispatchResult)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  if (aDispatchResult) {
    DebugOnly<bool> willIncrement = aWorkerPrivate->ModifyBusyCountFromWorker(aCx, true);
    // Should never fail since if this thread is still running, so should the
    // parent and it should be able to process a control runnable.
    MOZ_ASSERT(willIncrement);
  }
}

void
WorkerSameThreadRunnable::PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                                  bool aRunResult)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aWorkerPrivate);

  aWorkerPrivate->AssertIsOnWorkerThread();

  DebugOnly<bool> willDecrement = aWorkerPrivate->ModifyBusyCountFromWorker(aCx, false);
  MOZ_ASSERT(willDecrement);

  if (!aRunResult) {
    JS_ReportPendingException(aCx);
  }
}

