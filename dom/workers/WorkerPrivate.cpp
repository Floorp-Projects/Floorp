/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerPrivate.h"

#include <utility>

#include "js/CompilationAndEvaluation.h"
#include "js/ContextOptions.h"
#include "js/Exception.h"
#include "js/friend/ErrorMessages.h"  // JSMSG_OUT_OF_MEMORY
#include "js/LocaleSensitive.h"
#include "js/MemoryMetrics.h"
#include "js/SourceText.h"
#include "MessageEventRunnable.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/Result.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/BrowsingContextGroup.h"
#include "mozilla/dom/CallbackDebuggerNotification.h"
#include "mozilla/dom/ClientManager.h"
#include "mozilla/dom/ClientState.h"
#include "mozilla/dom/Console.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/IndexedDatabaseManager.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/nsCSPContext.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/PerformanceStorageWorker.h"
#include "mozilla/dom/PromiseDebugging.h"
#include "mozilla/dom/RemoteWorkerChild.h"
#include "mozilla/dom/RemoteWorkerService.h"
#include "mozilla/dom/TimeoutHandler.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/dom/JSExecutionManager.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/StorageAccess.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ThreadEventQueue.h"
#include "mozilla/ThrottledEventQueue.h"
#include "mozilla/TimelineConsumers.h"
#include "mozilla/WorkerTimelineMarker.h"
#include "nsCycleCollector.h"
#include "nsGlobalWindowInner.h"
#include "nsNetUtil.h"
#include "nsIFile.h"
#include "nsIMemoryReporter.h"
#include "nsIPermissionManager.h"
#include "nsIProtocolHandler.h"
#include "nsIScriptError.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIUUIDGenerator.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsRFPService.h"
#include "nsSandboxFlags.h"
#include "nsUTF8Utils.h"

#include "RuntimeService.h"
#include "ScriptLoader.h"
#include "mozilla/dom/ServiceWorkerEvents.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/net/CookieJarSettings.h"
#include "WorkerCSPEventListener.h"
#include "WorkerDebugger.h"
#include "WorkerDebuggerManager.h"
#include "WorkerError.h"
#include "WorkerEventTarget.h"
#include "WorkerNavigator.h"
#include "WorkerRef.h"
#include "WorkerRunnable.h"
#include "WorkerThread.h"

#include "nsThreadManager.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

// JS_MaybeGC will run once every second during normal execution.
#define PERIODIC_GC_TIMER_DELAY_SEC 1

// A shrinking GC will run five seconds after the last event is processed.
#define IDLE_GC_TIMER_DELAY_SEC 5

static mozilla::LazyLogModule sWorkerPrivateLog("WorkerPrivate");
static mozilla::LazyLogModule sWorkerTimeoutsLog("WorkerTimeouts");

mozilla::LogModule* WorkerLog() { return sWorkerPrivateLog; }

mozilla::LogModule* TimeoutsLog() { return sWorkerTimeoutsLog; }

#ifdef LOG
#  undef LOG
#endif
#define LOG(log, _args) MOZ_LOG(log, LogLevel::Debug, _args);

namespace mozilla {

using namespace ipc;

namespace dom {

using namespace workerinternals;

MOZ_DEFINE_MALLOC_SIZE_OF(JsWorkerMallocSizeOf)

namespace {

#ifdef DEBUG

const nsIID kDEBUGWorkerEventTargetIID = {
    0xccaba3fa,
    0x5be2,
    0x4de2,
    {0xba, 0x87, 0x3b, 0x3b, 0x5b, 0x1d, 0x5, 0xfb}};

#endif

// The number of nested timeouts before we start clamping. HTML says 5.
const uint32_t kClampTimeoutNestingLevel = 5u;

template <class T>
class UniquePtrComparator {
  typedef UniquePtr<T> A;
  typedef T* B;

 public:
  bool Equals(const A& a, const A& b) const {
    return a && b ? *a == *b : !a && !b ? true : false;
  }
  bool LessThan(const A& a, const A& b) const {
    return a && b ? *a < *b : b ? true : false;
  }
};

template <class T>
inline UniquePtrComparator<T> GetUniquePtrComparator(
    const nsTArray<UniquePtr<T>>&) {
  return UniquePtrComparator<T>();
}

// This class is used to wrap any runnables that the worker receives via the
// nsIEventTarget::Dispatch() method (either from NS_DispatchToCurrentThread or
// from the worker's EventTarget).
class ExternalRunnableWrapper final : public WorkerRunnable {
  nsCOMPtr<nsIRunnable> mWrappedRunnable;

 public:
  ExternalRunnableWrapper(WorkerPrivate* aWorkerPrivate,
                          nsIRunnable* aWrappedRunnable)
      : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mWrappedRunnable(aWrappedRunnable) {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWrappedRunnable);
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(ExternalRunnableWrapper, WorkerRunnable)

 private:
  ~ExternalRunnableWrapper() = default;

  virtual bool PreDispatch(WorkerPrivate* aWorkerPrivate) override {
    // Silence bad assertions.
    return true;
  }

  virtual void PostDispatch(WorkerPrivate* aWorkerPrivate,
                            bool aDispatchResult) override {
    // Silence bad assertions.
  }

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    nsresult rv = mWrappedRunnable->Run();
    if (NS_FAILED(rv)) {
      if (!JS_IsExceptionPending(aCx)) {
        Throw(aCx, rv);
      }
      return false;
    }
    return true;
  }

  nsresult Cancel() override {
    nsCOMPtr<nsIDiscardableRunnable> doomed =
        do_QueryInterface(mWrappedRunnable);
    MOZ_ASSERT(doomed);  // We checked this earlier!
    doomed->OnDiscard();
    return WorkerRunnable::Cancel();
  }
};

struct WindowAction {
  nsPIDOMWindowInner* mWindow;
  bool mDefaultAction;

  MOZ_IMPLICIT WindowAction(nsPIDOMWindowInner* aWindow)
      : mWindow(aWindow), mDefaultAction(true) {}

  bool operator==(const WindowAction& aOther) const {
    return mWindow == aOther.mWindow;
  }
};

class WorkerFinishedRunnable final : public WorkerControlRunnable {
  WorkerPrivate* mFinishedWorker;

 public:
  WorkerFinishedRunnable(WorkerPrivate* aWorkerPrivate,
                         WorkerPrivate* aFinishedWorker)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mFinishedWorker(aFinishedWorker) {}

 private:
  virtual bool PreDispatch(WorkerPrivate* aWorkerPrivate) override {
    // Silence bad assertions.
    return true;
  }

  virtual void PostDispatch(WorkerPrivate* aWorkerPrivate,
                            bool aDispatchResult) override {
    // Silence bad assertions.
  }

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    // This may block on the main thread.
    AutoYieldJSThreadExecution yield;

    if (!mFinishedWorker->ProxyReleaseMainThreadObjects()) {
      NS_WARNING("Failed to dispatch, going to leak!");
    }

    RuntimeService* runtime = RuntimeService::GetService();
    NS_ASSERTION(runtime, "This should never be null!");

    mFinishedWorker->DisableDebugger();

    runtime->UnregisterWorker(*mFinishedWorker);

    mFinishedWorker->ClearSelfAndParentEventTargetRef();
    return true;
  }
};

class TopLevelWorkerFinishedRunnable final : public Runnable {
  WorkerPrivate* mFinishedWorker;

 public:
  explicit TopLevelWorkerFinishedRunnable(WorkerPrivate* aFinishedWorker)
      : mozilla::Runnable("TopLevelWorkerFinishedRunnable"),
        mFinishedWorker(aFinishedWorker) {
    aFinishedWorker->AssertIsOnWorkerThread();
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(TopLevelWorkerFinishedRunnable, Runnable)

 private:
  ~TopLevelWorkerFinishedRunnable() = default;

  NS_IMETHOD
  Run() override {
    AssertIsOnMainThread();

    RuntimeService* runtime = RuntimeService::GetService();
    MOZ_ASSERT(runtime);

    mFinishedWorker->DisableDebugger();

    runtime->UnregisterWorker(*mFinishedWorker);

    if (!mFinishedWorker->ProxyReleaseMainThreadObjects()) {
      NS_WARNING("Failed to dispatch, going to leak!");
    }

    mFinishedWorker->ClearSelfAndParentEventTargetRef();
    return NS_OK;
  }
};

class ModifyBusyCountRunnable final : public WorkerControlRunnable {
  bool mIncrease;

 public:
  ModifyBusyCountRunnable(WorkerPrivate* aWorkerPrivate, bool aIncrease)
      : WorkerControlRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount),
        mIncrease(aIncrease) {}

 private:
  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    return aWorkerPrivate->ModifyBusyCount(mIncrease);
  }

  virtual void PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                       bool aRunResult) override {
    if (mIncrease) {
      WorkerControlRunnable::PostRun(aCx, aWorkerPrivate, aRunResult);
      return;
    }
    // Don't do anything here as it's possible that aWorkerPrivate has been
    // deleted.
  }
};

class CompileScriptRunnable final : public WorkerDebuggeeRunnable {
  nsString mScriptURL;
  UniquePtr<SerializedStackHolder> mOriginStack;

 public:
  explicit CompileScriptRunnable(WorkerPrivate* aWorkerPrivate,
                                 UniquePtr<SerializedStackHolder> aOriginStack,
                                 const nsAString& aScriptURL)
      : WorkerDebuggeeRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount),
        mScriptURL(aScriptURL),
        mOriginStack(aOriginStack.release()) {}

 private:
  // We can't implement PreRun effectively, because at the point when that would
  // run we have not yet done our load so don't know things like our final
  // principal and whatnot.

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->AssertIsOnWorkerThread();

    WorkerGlobalScope* globalScope =
        aWorkerPrivate->GetOrCreateGlobalScope(aCx);
    if (NS_WARN_IF(!globalScope)) {
      return false;
    }

    if (NS_WARN_IF(!aWorkerPrivate->EnsureCSPEventListener())) {
      return false;
    }

    ErrorResult rv;
    workerinternals::LoadMainScript(aWorkerPrivate, std::move(mOriginStack),
                                    mScriptURL, WorkerScript, rv);
    rv.WouldReportJSException();
    // Explicitly ignore NS_BINDING_ABORTED on rv.  Or more precisely, still
    // return false and don't SetWorkerScriptExecutedSuccessfully() in that
    // case, but don't throw anything on aCx.  The idea is to not dispatch error
    // events if our load is canceled with that error code.
    if (rv.ErrorCodeIs(NS_BINDING_ABORTED)) {
      rv.SuppressException();
      return false;
    }

    // Make sure to propagate exceptions from rv onto aCx, so that they will get
    // reported after we return.  We want to propagate just JS exceptions,
    // because all the other errors are handled when the script is loaded.
    // See: https://dom.spec.whatwg.org/#concept-event-fire
    if (rv.Failed() && !rv.IsJSException()) {
      WorkerErrorReport::CreateAndDispatchGenericErrorRunnableToParent(
          aWorkerPrivate);
      rv.SuppressException();
      return false;
    }

    // This is a little dumb, but aCx is in the null realm here because we
    // set it up that way in our Run(), since we had not created the global at
    // that point yet.  So we need to enter the realm of our global,
    // because setting a pending exception on aCx involves wrapping into its
    // current compartment.  Luckily we have a global now.
    JSAutoRealm ar(aCx, globalScope->GetGlobalJSObject());
    if (rv.MaybeSetPendingException(aCx)) {
      // In the event of an uncaught exception, the worker should still keep
      // running (return true) but should not be marked as having executed
      // successfully (which will cause ServiceWorker installation to fail).
      // In previous error handling cases in this method, we return false (to
      // trigger CloseInternal) because the global is not in an operable
      // state at all.
      //
      // For ServiceWorkers, this would correspond to the "Run Service Worker"
      // algorithm returning an "abrupt completion" and _not_ failure.
      //
      // For DedicatedWorkers and SharedWorkers, this would correspond to the
      // "run a worker" algorithm disregarding the return value of "run the
      // classic script"/"run the module script" in step 24:
      //
      // "If script is a classic script, then run the classic script script.
      // Otherwise, it is a module script; run the module script script."
      return true;
    }

    aWorkerPrivate->SetWorkerScriptExecutedSuccessfully();
    return true;
  }

  void PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aRunResult) override {
    if (!aRunResult) {
      aWorkerPrivate->CloseInternal();
    }
    WorkerRunnable::PostRun(aCx, aWorkerPrivate, aRunResult);
  }
};

class NotifyRunnable final : public WorkerControlRunnable {
  WorkerStatus mStatus;

 public:
  NotifyRunnable(WorkerPrivate* aWorkerPrivate, WorkerStatus aStatus)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mStatus(aStatus) {
    MOZ_ASSERT(aStatus == Closing || aStatus == Canceling ||
               aStatus == Killing);
  }

 private:
  virtual bool PreDispatch(WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->AssertIsOnParentThread();
    return aWorkerPrivate->ModifyBusyCount(true);
  }

  virtual void PostDispatch(WorkerPrivate* aWorkerPrivate,
                            bool aDispatchResult) override {
    aWorkerPrivate->AssertIsOnParentThread();
    if (!aDispatchResult) {
      // We couldn't dispatch to the worker, which means it's already dead.
      // Undo the busy count modification.
      aWorkerPrivate->ModifyBusyCount(false);
    }
  }

  virtual void PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                       bool aRunResult) override {
    aWorkerPrivate->ModifyBusyCountFromWorker(false);
  }

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    return aWorkerPrivate->NotifyInternal(mStatus);
  }
};

class FreezeRunnable final : public WorkerControlRunnable {
 public:
  explicit FreezeRunnable(WorkerPrivate* aWorkerPrivate)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount) {}

 private:
  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    return aWorkerPrivate->FreezeInternal();
  }
};

class ThawRunnable final : public WorkerControlRunnable {
 public:
  explicit ThawRunnable(WorkerPrivate* aWorkerPrivate)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount) {}

 private:
  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    return aWorkerPrivate->ThawInternal();
  }
};

class PropagateStorageAccessPermissionGrantedRunnable final
    : public WorkerControlRunnable {
 public:
  explicit PropagateStorageAccessPermissionGrantedRunnable(
      WorkerPrivate* aWorkerPrivate)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount) {}

 private:
  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->PropagateStorageAccessPermissionGrantedInternal();
    return true;
  }
};

class ReportErrorToConsoleRunnable final : public WorkerRunnable {
  const char* mMessage;
  const nsTArray<nsString> mParams;

 public:
  // aWorkerPrivate is the worker thread we're on (or the main thread, if null)
  static void Report(WorkerPrivate* aWorkerPrivate, const char* aMessage,
                     const nsTArray<nsString>& aParams) {
    if (aWorkerPrivate) {
      aWorkerPrivate->AssertIsOnWorkerThread();
    } else {
      AssertIsOnMainThread();
    }

    // Now fire a runnable to do the same on the parent's thread if we can.
    if (aWorkerPrivate) {
      RefPtr<ReportErrorToConsoleRunnable> runnable =
          new ReportErrorToConsoleRunnable(aWorkerPrivate, aMessage, aParams);
      runnable->Dispatch();
      return;
    }

    // Log a warning to the console.
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "DOM"_ns,
                                    nullptr, nsContentUtils::eDOM_PROPERTIES,
                                    aMessage, aParams);
  }

 private:
  ReportErrorToConsoleRunnable(WorkerPrivate* aWorkerPrivate,
                               const char* aMessage,
                               const nsTArray<nsString>& aParams)
      : WorkerRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount),
        mMessage(aMessage),
        mParams(aParams.Clone()) {}

  virtual void PostDispatch(WorkerPrivate* aWorkerPrivate,
                            bool aDispatchResult) override {
    aWorkerPrivate->AssertIsOnWorkerThread();

    // Dispatch may fail if the worker was canceled, no need to report that as
    // an error, so don't call base class PostDispatch.
  }

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    WorkerPrivate* parent = aWorkerPrivate->GetParent();
    MOZ_ASSERT_IF(!parent, NS_IsMainThread());
    Report(parent, mMessage, mParams);
    return true;
  }
};

class TimerRunnable final : public WorkerRunnable,
                            public nsITimerCallback,
                            public nsINamed {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  explicit TimerRunnable(WorkerPrivate* aWorkerPrivate)
      : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount) {}

 private:
  ~TimerRunnable() = default;

  virtual bool PreDispatch(WorkerPrivate* aWorkerPrivate) override {
    // Silence bad assertions.
    return true;
  }

  virtual void PostDispatch(WorkerPrivate* aWorkerPrivate,
                            bool aDispatchResult) override {
    // Silence bad assertions.
  }

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY until worker runnables are generally
  // MOZ_CAN_RUN_SCRIPT.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    return aWorkerPrivate->RunExpiredTimeouts(aCx);
  }

  NS_IMETHOD
  Notify(nsITimer* aTimer) override { return Run(); }

  NS_IMETHOD
  GetName(nsACString& aName) override {
    aName.AssignLiteral("TimerRunnable");
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS_INHERITED(TimerRunnable, WorkerRunnable, nsITimerCallback,
                            nsINamed)

class DebuggerImmediateRunnable : public WorkerRunnable {
  RefPtr<dom::Function> mHandler;

 public:
  explicit DebuggerImmediateRunnable(WorkerPrivate* aWorkerPrivate,
                                     dom::Function& aHandler)
      : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mHandler(&aHandler) {}

 private:
  virtual bool IsDebuggerRunnable() const override { return true; }

  virtual bool PreDispatch(WorkerPrivate* aWorkerPrivate) override {
    // Silence bad assertions.
    return true;
  }

  virtual void PostDispatch(WorkerPrivate* aWorkerPrivate,
                            bool aDispatchResult) override {
    // Silence bad assertions.
  }

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
    JS::Rooted<JS::Value> callable(
        aCx, JS::ObjectOrNullValue(mHandler->CallableOrNull()));
    JS::HandleValueArray args = JS::HandleValueArray::empty();
    JS::Rooted<JS::Value> rval(aCx);
    if (!JS_CallFunctionValue(aCx, global, callable, args, &rval)) {
      // Just return false; WorkerRunnable::Run will report the exception.
      return false;
    }

    return true;
  }
};

void PeriodicGCTimerCallback(nsITimer* aTimer, void* aClosure) {
  auto workerPrivate = static_cast<WorkerPrivate*>(aClosure);
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();
  workerPrivate->GarbageCollectInternal(workerPrivate->GetJSContext(),
                                        false /* shrinking */,
                                        false /* collect children */);
}

void IdleGCTimerCallback(nsITimer* aTimer, void* aClosure) {
  auto workerPrivate = static_cast<WorkerPrivate*>(aClosure);
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();
  workerPrivate->GarbageCollectInternal(workerPrivate->GetJSContext(),
                                        true /* shrinking */,
                                        false /* collect children */);
}

class UpdateContextOptionsRunnable final : public WorkerControlRunnable {
  JS::ContextOptions mContextOptions;

 public:
  UpdateContextOptionsRunnable(WorkerPrivate* aWorkerPrivate,
                               const JS::ContextOptions& aContextOptions)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mContextOptions(aContextOptions) {}

 private:
  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->UpdateContextOptionsInternal(aCx, mContextOptions);
    return true;
  }
};

class UpdateLanguagesRunnable final : public WorkerRunnable {
  nsTArray<nsString> mLanguages;

 public:
  UpdateLanguagesRunnable(WorkerPrivate* aWorkerPrivate,
                          const nsTArray<nsString>& aLanguages)
      : WorkerRunnable(aWorkerPrivate), mLanguages(aLanguages.Clone()) {}

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->UpdateLanguagesInternal(mLanguages);
    return true;
  }
};

class UpdateJSWorkerMemoryParameterRunnable final
    : public WorkerControlRunnable {
  Maybe<uint32_t> mValue;
  JSGCParamKey mKey;

 public:
  UpdateJSWorkerMemoryParameterRunnable(WorkerPrivate* aWorkerPrivate,
                                        JSGCParamKey aKey,
                                        Maybe<uint32_t> aValue)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mValue(aValue),
        mKey(aKey) {}

 private:
  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->UpdateJSWorkerMemoryParameterInternal(aCx, mKey, mValue);
    return true;
  }
};

#ifdef JS_GC_ZEAL
class UpdateGCZealRunnable final : public WorkerControlRunnable {
  uint8_t mGCZeal;
  uint32_t mFrequency;

 public:
  UpdateGCZealRunnable(WorkerPrivate* aWorkerPrivate, uint8_t aGCZeal,
                       uint32_t aFrequency)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mGCZeal(aGCZeal),
        mFrequency(aFrequency) {}

 private:
  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->UpdateGCZealInternal(aCx, mGCZeal, mFrequency);
    return true;
  }
};
#endif

class SetLowMemoryStateRunnable final : public WorkerControlRunnable {
  bool mState;

 public:
  SetLowMemoryStateRunnable(WorkerPrivate* aWorkerPrivate, bool aState)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mState(aState) {}

 private:
  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->SetLowMemoryStateInternal(aCx, mState);
    return true;
  }
};

class GarbageCollectRunnable final : public WorkerControlRunnable {
  bool mShrinking;
  bool mCollectChildren;

 public:
  GarbageCollectRunnable(WorkerPrivate* aWorkerPrivate, bool aShrinking,
                         bool aCollectChildren)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mShrinking(aShrinking),
        mCollectChildren(aCollectChildren) {}

 private:
  virtual bool PreDispatch(WorkerPrivate* aWorkerPrivate) override {
    // Silence bad assertions, this can be dispatched from either the main
    // thread or the timer thread..
    return true;
  }

  virtual void PostDispatch(WorkerPrivate* aWorkerPrivate,
                            bool aDispatchResult) override {
    // Silence bad assertions, this can be dispatched from either the main
    // thread or the timer thread..
  }

  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->GarbageCollectInternal(aCx, mShrinking, mCollectChildren);
    return true;
  }
};

class CycleCollectRunnable : public WorkerControlRunnable {
  bool mCollectChildren;

 public:
  CycleCollectRunnable(WorkerPrivate* aWorkerPrivate, bool aCollectChildren)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mCollectChildren(aCollectChildren) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->CycleCollectInternal(mCollectChildren);
    return true;
  }
};

class OfflineStatusChangeRunnable : public WorkerRunnable {
 public:
  OfflineStatusChangeRunnable(WorkerPrivate* aWorkerPrivate, bool aIsOffline)
      : WorkerRunnable(aWorkerPrivate), mIsOffline(aIsOffline) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->OfflineStatusChangeEventInternal(mIsOffline);
    return true;
  }

 private:
  bool mIsOffline;
};

class MemoryPressureRunnable : public WorkerControlRunnable {
 public:
  explicit MemoryPressureRunnable(WorkerPrivate* aWorkerPrivate)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->MemoryPressureInternal();
    return true;
  }
};

#ifdef DEBUG
static bool StartsWithExplicit(nsACString& s) {
  return StringBeginsWith(s, "explicit/"_ns);
}
#endif

PRThread* PRThreadFromThread(nsIThread* aThread) {
  MOZ_ASSERT(aThread);

  PRThread* result;
  MOZ_ALWAYS_SUCCEEDS(aThread->GetPRThread(&result));
  MOZ_ASSERT(result);

  return result;
}

// A runnable to cancel the worker from the parent thread when self.close() is
// called. This runnable is executed on the parent process in order to cancel
// the current runnable. It uses a normal WorkerDebuggeeRunnable in order to be
// sure that all the pending WorkerDebuggeeRunnables are executed before this.
class CancelingOnParentRunnable final : public WorkerDebuggeeRunnable {
 public:
  explicit CancelingOnParentRunnable(WorkerPrivate* aWorkerPrivate)
      : WorkerDebuggeeRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount) {
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->Cancel();
    return true;
  }
};

// A runnable to cancel the worker from the parent process.
class CancelingWithTimeoutOnParentRunnable final
    : public WorkerControlRunnable {
 public:
  explicit CancelingWithTimeoutOnParentRunnable(WorkerPrivate* aWorkerPrivate)
      : WorkerControlRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->AssertIsOnParentThread();
    aWorkerPrivate->StartCancelingTimer();
    return true;
  }
};

class CancelingTimerCallback final : public nsITimerCallback {
 public:
  NS_DECL_ISUPPORTS

  explicit CancelingTimerCallback(WorkerPrivate* aWorkerPrivate)
      : mWorkerPrivate(aWorkerPrivate) {}

  NS_IMETHOD
  Notify(nsITimer* aTimer) override {
    mWorkerPrivate->AssertIsOnParentThread();
    mWorkerPrivate->Cancel();
    return NS_OK;
  }

 private:
  ~CancelingTimerCallback() = default;

  // Raw pointer here is OK because the timer is canceled during the shutdown
  // steps.
  WorkerPrivate* mWorkerPrivate;
};

NS_IMPL_ISUPPORTS(CancelingTimerCallback, nsITimerCallback)

// This runnable starts the canceling of a worker after a self.close().
class CancelingRunnable final : public Runnable {
 public:
  CancelingRunnable() : Runnable("CancelingRunnable") {}

  NS_IMETHOD
  Run() override {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    workerPrivate->AssertIsOnWorkerThread();

    // Now we can cancel the this worker from the parent process.
    RefPtr<CancelingOnParentRunnable> r =
        new CancelingOnParentRunnable(workerPrivate);
    r->Dispatch();

    return NS_OK;
  }
};

} /* anonymous namespace */

nsString ComputeWorkerPrivateId() {
  nsresult rv;
  nsCOMPtr<nsIUUIDGenerator> uuidGenerator =
      do_GetService("@mozilla.org/uuid-generator;1", &rv);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsID uuid;
  rv = uuidGenerator->GenerateUUIDInPlace(&uuid);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  char buffer[NSID_LENGTH];
  uuid.ToProvidedString(buffer);

  nsString id;
  // Remove {} and the null terminator
  id.AssignASCII(&buffer[1], NSID_LENGTH - 3);

  return id;
}

class WorkerPrivate::EventTarget final : public nsISerialEventTarget {
  // This mutex protects mWorkerPrivate and must be acquired *before* the
  // WorkerPrivate's mutex whenever they must both be held.
  mozilla::Mutex mMutex;
  WorkerPrivate* mWorkerPrivate;
  nsIEventTarget* mWeakNestedEventTarget;
  nsCOMPtr<nsIEventTarget> mNestedEventTarget;

 public:
  explicit EventTarget(WorkerPrivate* aWorkerPrivate)
      : mMutex("WorkerPrivate::EventTarget::mMutex"),
        mWorkerPrivate(aWorkerPrivate),
        mWeakNestedEventTarget(nullptr) {
    MOZ_ASSERT(aWorkerPrivate);
  }

  EventTarget(WorkerPrivate* aWorkerPrivate, nsIEventTarget* aNestedEventTarget)
      : mMutex("WorkerPrivate::EventTarget::mMutex"),
        mWorkerPrivate(aWorkerPrivate),
        mWeakNestedEventTarget(aNestedEventTarget),
        mNestedEventTarget(aNestedEventTarget) {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aNestedEventTarget);
  }

  void Disable() {
    nsCOMPtr<nsIEventTarget> nestedEventTarget;
    {
      MutexAutoLock lock(mMutex);

      // Note, Disable() can be called more than once safely.
      mWorkerPrivate = nullptr;
      mNestedEventTarget.swap(nestedEventTarget);
    }
  }

  nsIEventTarget* GetWeakNestedEventTarget() const {
    MOZ_ASSERT(mWeakNestedEventTarget);
    return mWeakNestedEventTarget;
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL

 private:
  ~EventTarget() = default;
};

struct WorkerPrivate::TimeoutInfo {
  TimeoutInfo()
      : mId(0),
        mNestingLevel(0),
        mIsInterval(false),
        mCanceled(false),
        mOnChromeWorker(false) {
    MOZ_COUNT_CTOR(mozilla::dom::WorkerPrivate::TimeoutInfo);
  }

  ~TimeoutInfo() { MOZ_COUNT_DTOR(mozilla::dom::WorkerPrivate::TimeoutInfo); }

  bool operator==(const TimeoutInfo& aOther) {
    return mTargetTime == aOther.mTargetTime;
  }

  bool operator<(const TimeoutInfo& aOther) {
    return mTargetTime < aOther.mTargetTime;
  }

  void AccumulateNestingLevel(const uint32_t& aBaseLevel) {
    if (aBaseLevel < kClampTimeoutNestingLevel) {
      mNestingLevel = aBaseLevel + 1;
      return;
    }
    mNestingLevel = kClampTimeoutNestingLevel;
  }

  void CalculateTargetTime() {
    auto target = mInterval;
    // Don't clamp timeout for chrome workers
    if (mNestingLevel >= kClampTimeoutNestingLevel && !mOnChromeWorker) {
      target = TimeDuration::Max(
          mInterval,
          TimeDuration::FromMilliseconds(StaticPrefs::dom_min_timeout_value()));
    }
    mTargetTime = TimeStamp::Now() + target;
  }

  RefPtr<TimeoutHandler> mHandler;
  mozilla::TimeStamp mTargetTime;
  mozilla::TimeDuration mInterval;
  int32_t mId;
  uint32_t mNestingLevel;
  bool mIsInterval;
  bool mCanceled;
  bool mOnChromeWorker;
};

class WorkerJSContextStats final : public JS::RuntimeStats {
  const nsCString mRtPath;

 public:
  explicit WorkerJSContextStats(const nsACString& aRtPath)
      : JS::RuntimeStats(JsWorkerMallocSizeOf), mRtPath(aRtPath) {}

  ~WorkerJSContextStats() {
    for (size_t i = 0; i != zoneStatsVector.length(); i++) {
      delete static_cast<xpc::ZoneStatsExtras*>(zoneStatsVector[i].extra);
    }

    for (size_t i = 0; i != realmStatsVector.length(); i++) {
      delete static_cast<xpc::RealmStatsExtras*>(realmStatsVector[i].extra);
    }
  }

  const nsCString& Path() const { return mRtPath; }

  virtual void initExtraZoneStats(JS::Zone* aZone, JS::ZoneStats* aZoneStats,
                                  const JS::AutoRequireNoGC& nogc) override {
    MOZ_ASSERT(!aZoneStats->extra);

    // ReportJSRuntimeExplicitTreeStats expects that
    // aZoneStats->extra is a xpc::ZoneStatsExtras pointer.
    xpc::ZoneStatsExtras* extras = new xpc::ZoneStatsExtras;
    extras->pathPrefix = mRtPath;
    extras->pathPrefix += nsPrintfCString("zone(0x%p)/", (void*)aZone);

    MOZ_ASSERT(StartsWithExplicit(extras->pathPrefix));

    aZoneStats->extra = extras;
  }

  virtual void initExtraRealmStats(JS::Realm* aRealm,
                                   JS::RealmStats* aRealmStats,
                                   const JS::AutoRequireNoGC& nogc) override {
    MOZ_ASSERT(!aRealmStats->extra);

    // ReportJSRuntimeExplicitTreeStats expects that
    // aRealmStats->extra is a xpc::RealmStatsExtras pointer.
    xpc::RealmStatsExtras* extras = new xpc::RealmStatsExtras;

    // This is the |jsPathPrefix|.  Each worker has exactly one realm.
    extras->jsPathPrefix.Assign(mRtPath);
    extras->jsPathPrefix +=
        nsPrintfCString("zone(0x%p)/", (void*)js::GetRealmZone(aRealm));
    extras->jsPathPrefix += "realm(web-worker)/"_ns;

    // This should never be used when reporting with workers (hence the "?!").
    extras->domPathPrefix.AssignLiteral("explicit/workers/?!/");

    MOZ_ASSERT(StartsWithExplicit(extras->jsPathPrefix));
    MOZ_ASSERT(StartsWithExplicit(extras->domPathPrefix));

    extras->location = nullptr;

    aRealmStats->extra = extras;
  }
};

class WorkerPrivate::MemoryReporter final : public nsIMemoryReporter {
  NS_DECL_THREADSAFE_ISUPPORTS

  friend class WorkerPrivate;

  SharedMutex mMutex;
  WorkerPrivate* mWorkerPrivate;

 public:
  explicit MemoryReporter(WorkerPrivate* aWorkerPrivate)
      : mMutex(aWorkerPrivate->mMutex), mWorkerPrivate(aWorkerPrivate) {
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport, nsISupports* aData,
                 bool aAnonymize) override;

 private:
  class FinishCollectRunnable;

  class CollectReportsRunnable final : public MainThreadWorkerControlRunnable {
    RefPtr<FinishCollectRunnable> mFinishCollectRunnable;
    const bool mAnonymize;

   public:
    CollectReportsRunnable(WorkerPrivate* aWorkerPrivate,
                           nsIHandleReportCallback* aHandleReport,
                           nsISupports* aHandlerData, bool aAnonymize,
                           const nsACString& aPath);

   private:
    bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;

    ~CollectReportsRunnable() {
      if (NS_IsMainThread()) {
        mFinishCollectRunnable->Run();
        return;
      }

      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(workerPrivate);
      MOZ_ALWAYS_SUCCEEDS(workerPrivate->DispatchToMainThreadForMessaging(
          mFinishCollectRunnable.forget()));
    }
  };

  class FinishCollectRunnable final : public Runnable {
    nsCOMPtr<nsIHandleReportCallback> mHandleReport;
    nsCOMPtr<nsISupports> mHandlerData;
    size_t mPerformanceUserEntries;
    size_t mPerformanceResourceEntries;
    const bool mAnonymize;
    bool mSuccess;

   public:
    WorkerJSContextStats mCxStats;

    explicit FinishCollectRunnable(nsIHandleReportCallback* aHandleReport,
                                   nsISupports* aHandlerData, bool aAnonymize,
                                   const nsACString& aPath);

    NS_IMETHOD Run() override;

    void SetPerformanceSizes(size_t userEntries, size_t resourceEntries) {
      mPerformanceUserEntries = userEntries;
      mPerformanceResourceEntries = resourceEntries;
    }

    void SetSuccess(bool success) { mSuccess = success; }

   private:
    ~FinishCollectRunnable() {
      // mHandleReport and mHandlerData are released on the main thread.
      AssertIsOnMainThread();
    }

    FinishCollectRunnable(const FinishCollectRunnable&) = delete;
    FinishCollectRunnable& operator=(const FinishCollectRunnable&) = delete;
    FinishCollectRunnable& operator=(const FinishCollectRunnable&&) = delete;
  };

  ~MemoryReporter() = default;

  void Disable() {
    // Called from WorkerPrivate::DisableMemoryReporter.
    mMutex.AssertCurrentThreadOwns();

    NS_ASSERTION(mWorkerPrivate, "Disabled more than once!");
    mWorkerPrivate = nullptr;
  }
};

NS_IMPL_ISUPPORTS(WorkerPrivate::MemoryReporter, nsIMemoryReporter)

NS_IMETHODIMP
WorkerPrivate::MemoryReporter::CollectReports(
    nsIHandleReportCallback* aHandleReport, nsISupports* aData,
    bool aAnonymize) {
  AssertIsOnMainThread();

  RefPtr<CollectReportsRunnable> runnable;

  {
    MutexAutoLock lock(mMutex);

    if (!mWorkerPrivate) {
      // This will effectively report 0 memory.
      nsCOMPtr<nsIMemoryReporterManager> manager =
          do_GetService("@mozilla.org/memory-reporter-manager;1");
      if (manager) {
        manager->EndReport();
      }
      return NS_OK;
    }

    nsAutoCString path;
    path.AppendLiteral("explicit/workers/workers(");
    if (aAnonymize && !mWorkerPrivate->Domain().IsEmpty()) {
      path.AppendLiteral("<anonymized-domain>)/worker(<anonymized-url>");
    } else {
      nsAutoCString escapedDomain(mWorkerPrivate->Domain());
      if (escapedDomain.IsEmpty()) {
        escapedDomain += "chrome";
      } else {
        escapedDomain.ReplaceChar('/', '\\');
      }
      path.Append(escapedDomain);
      path.AppendLiteral(")/worker(");
      NS_ConvertUTF16toUTF8 escapedURL(mWorkerPrivate->ScriptURL());
      escapedURL.ReplaceChar('/', '\\');
      path.Append(escapedURL);
    }
    path.AppendPrintf(", 0x%p)/", static_cast<void*>(mWorkerPrivate));

    runnable = new CollectReportsRunnable(mWorkerPrivate, aHandleReport, aData,
                                          aAnonymize, path);
  }

  if (!runnable->Dispatch()) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

WorkerPrivate::MemoryReporter::CollectReportsRunnable::CollectReportsRunnable(
    WorkerPrivate* aWorkerPrivate, nsIHandleReportCallback* aHandleReport,
    nsISupports* aHandlerData, bool aAnonymize, const nsACString& aPath)
    : MainThreadWorkerControlRunnable(aWorkerPrivate),
      mFinishCollectRunnable(new FinishCollectRunnable(
          aHandleReport, aHandlerData, aAnonymize, aPath)),
      mAnonymize(aAnonymize) {}

bool WorkerPrivate::MemoryReporter::CollectReportsRunnable::WorkerRun(
    JSContext* aCx, WorkerPrivate* aWorkerPrivate) {
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<WorkerGlobalScope> scope = aWorkerPrivate->GlobalScope();
  RefPtr<Performance> performance =
      scope ? scope->GetPerformanceIfExists() : nullptr;
  if (performance) {
    size_t userEntries = performance->SizeOfUserEntries(JsWorkerMallocSizeOf);
    size_t resourceEntries =
        performance->SizeOfResourceEntries(JsWorkerMallocSizeOf);
    mFinishCollectRunnable->SetPerformanceSizes(userEntries, resourceEntries);
  }

  mFinishCollectRunnable->SetSuccess(aWorkerPrivate->CollectRuntimeStats(
      &mFinishCollectRunnable->mCxStats, mAnonymize));

  return true;
}

WorkerPrivate::MemoryReporter::FinishCollectRunnable::FinishCollectRunnable(
    nsIHandleReportCallback* aHandleReport, nsISupports* aHandlerData,
    bool aAnonymize, const nsACString& aPath)
    : mozilla::Runnable(
          "dom::WorkerPrivate::MemoryReporter::FinishCollectRunnable"),
      mHandleReport(aHandleReport),
      mHandlerData(aHandlerData),
      mPerformanceUserEntries(0),
      mPerformanceResourceEntries(0),
      mAnonymize(aAnonymize),
      mSuccess(false),
      mCxStats(aPath) {}

NS_IMETHODIMP
WorkerPrivate::MemoryReporter::FinishCollectRunnable::Run() {
  AssertIsOnMainThread();

  nsCOMPtr<nsIMemoryReporterManager> manager =
      do_GetService("@mozilla.org/memory-reporter-manager;1");

  if (!manager) return NS_OK;

  if (mSuccess) {
    xpc::ReportJSRuntimeExplicitTreeStats(
        mCxStats, mCxStats.Path(), mHandleReport, mHandlerData, mAnonymize);

    if (mPerformanceUserEntries) {
      nsCString path = mCxStats.Path();
      path.AppendLiteral("dom/performance/user-entries");
      mHandleReport->Callback(
          ""_ns, path, nsIMemoryReporter::KIND_HEAP,
          nsIMemoryReporter::UNITS_BYTES, mPerformanceUserEntries,
          "Memory used for performance user entries."_ns, mHandlerData);
    }

    if (mPerformanceResourceEntries) {
      nsCString path = mCxStats.Path();
      path.AppendLiteral("dom/performance/resource-entries");
      mHandleReport->Callback(
          ""_ns, path, nsIMemoryReporter::KIND_HEAP,
          nsIMemoryReporter::UNITS_BYTES, mPerformanceResourceEntries,
          "Memory used for performance resource entries."_ns, mHandlerData);
    }
  }

  manager->EndReport();

  return NS_OK;
}

WorkerPrivate::SyncLoopInfo::SyncLoopInfo(EventTarget* aEventTarget)
    : mEventTarget(aEventTarget),
      mCompleted(false),
      mResult(false)
#ifdef DEBUG
      ,
      mHasRun(false)
#endif
{
}

Document* WorkerPrivate::GetDocument() const {
  AssertIsOnMainThread();
  if (mLoadInfo.mWindow) {
    return mLoadInfo.mWindow->GetExtantDoc();
  }
  // if we don't have a document, we should query the document
  // from the parent in case of a nested worker
  WorkerPrivate* parent = mParent;
  while (parent) {
    if (parent->mLoadInfo.mWindow) {
      return parent->mLoadInfo.mWindow->GetExtantDoc();
    }
    parent = parent->GetParent();
  }
  // couldn't query a document, give up and return nullptr
  return nullptr;
}

void WorkerPrivate::SetCSP(nsIContentSecurityPolicy* aCSP) {
  AssertIsOnMainThread();
  if (!aCSP) {
    return;
  }
  aCSP->EnsureEventTarget(mMainThreadEventTarget);

  mLoadInfo.mCSP = aCSP;
  mLoadInfo.mCSPInfo = MakeUnique<CSPInfo>();
  nsresult rv = CSPToCSPInfo(mLoadInfo.mCSP, mLoadInfo.mCSPInfo.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
}

nsresult WorkerPrivate::SetCSPFromHeaderValues(
    const nsACString& aCSPHeaderValue,
    const nsACString& aCSPReportOnlyHeaderValue) {
  AssertIsOnMainThread();
  MOZ_DIAGNOSTIC_ASSERT(!mLoadInfo.mCSP);

  NS_ConvertASCIItoUTF16 cspHeaderValue(aCSPHeaderValue);
  NS_ConvertASCIItoUTF16 cspROHeaderValue(aCSPReportOnlyHeaderValue);

  nsresult rv;
  nsCOMPtr<nsIContentSecurityPolicy> csp = new nsCSPContext();

  // First, we try to query the URI from the Principal, but
  // in case selfURI remains empty (e.g in case the Principal
  // is a SystemPrincipal) then we fall back and use the
  // base URI as selfURI for CSP.
  nsCOMPtr<nsIURI> selfURI;
  // Its not recommended to use the BasePrincipal to get the URI
  // but in this case we need to make an exception
  auto* basePrin = BasePrincipal::Cast(mLoadInfo.mPrincipal);
  if (basePrin) {
    basePrin->GetURI(getter_AddRefs(selfURI));
  }
  if (!selfURI) {
    selfURI = mLoadInfo.mBaseURI;
  }
  MOZ_ASSERT(selfURI, "need a self URI for CSP");

  rv = csp->SetRequestContextWithPrincipal(mLoadInfo.mPrincipal, selfURI,
                                           u""_ns, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  csp->EnsureEventTarget(mMainThreadEventTarget);

  // If there's a CSP header, apply it.
  if (!cspHeaderValue.IsEmpty()) {
    rv = CSP_AppendCSPFromHeader(csp, cspHeaderValue, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // If there's a report-only CSP header, apply it.
  if (!cspROHeaderValue.IsEmpty()) {
    rv = CSP_AppendCSPFromHeader(csp, cspROHeaderValue, true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Set evalAllowed, default value is set in GetAllowsEval
  bool evalAllowed = false;
  bool reportEvalViolations = false;
  rv = csp->GetAllowsEval(&reportEvalViolations, &evalAllowed);
  NS_ENSURE_SUCCESS(rv, rv);

  mLoadInfo.mCSP = csp;
  mLoadInfo.mEvalAllowed = evalAllowed;
  mLoadInfo.mReportCSPViolations = reportEvalViolations;

  mLoadInfo.mCSPInfo = MakeUnique<CSPInfo>();
  rv = CSPToCSPInfo(csp, mLoadInfo.mCSPInfo.get());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

void WorkerPrivate::StoreCSPOnClient() {
  auto data = mWorkerThreadAccessible.Access();
  MOZ_ASSERT(data->mScope);
  if (mLoadInfo.mCSPInfo) {
    data->mScope->MutableClientSourceRef().SetCspInfo(*mLoadInfo.mCSPInfo);
  }
}

void WorkerPrivate::UpdateReferrerInfoFromHeader(
    const nsACString& aReferrerPolicyHeaderValue) {
  NS_ConvertUTF8toUTF16 headerValue(aReferrerPolicyHeaderValue);

  if (headerValue.IsEmpty()) {
    return;
  }

  ReferrerPolicy policy =
      ReferrerInfo::ReferrerPolicyFromHeaderString(headerValue);
  if (policy == ReferrerPolicy::_empty) {
    return;
  }

  nsCOMPtr<nsIReferrerInfo> referrerInfo =
      static_cast<ReferrerInfo*>(GetReferrerInfo())->CloneWithNewPolicy(policy);
  SetReferrerInfo(referrerInfo);
}

void WorkerPrivate::Traverse(nsCycleCollectionTraversalCallback& aCb) {
  AssertIsOnParentThread();

  // The WorkerPrivate::mParentEventTargetRef has a reference to the exposed
  // Worker object, which is really held by the worker thread.  We traverse this
  // reference if and only if our busy count is zero and we have not released
  // the main thread reference.  We do not unlink it.  This allows the CC to
  // break cycles involving the Worker and begin shutting it down (which does
  // happen in unlink) but ensures that the WorkerPrivate won't be deleted
  // before we're done shutting down the thread.
  if (!mBusyCount && !mMainThreadObjectsForgotten) {
    nsCycleCollectionTraversalCallback& cb = aCb;
    WorkerPrivate* tmp = this;
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParentEventTargetRef);
  }
}

nsresult WorkerPrivate::Dispatch(already_AddRefed<WorkerRunnable> aRunnable,
                                 nsIEventTarget* aSyncLoopTarget) {
  // May be called on any thread!
  MutexAutoLock lock(mMutex);
  return DispatchLockHeld(std::move(aRunnable), aSyncLoopTarget, lock);
}

nsresult WorkerPrivate::DispatchLockHeld(
    already_AddRefed<WorkerRunnable> aRunnable, nsIEventTarget* aSyncLoopTarget,
    const MutexAutoLock& aProofOfLock) {
  // May be called on any thread!
  RefPtr<WorkerRunnable> runnable(aRunnable);

  MOZ_ASSERT_IF(aSyncLoopTarget, mThread);

  if (mStatus == Dead || (!aSyncLoopTarget && ParentStatus() > Running)) {
    NS_WARNING(
        "A runnable was posted to a worker that is already shutting "
        "down!");
    return NS_ERROR_UNEXPECTED;
  }

  if (runnable->IsDebuggeeRunnable() && !mDebuggerReady) {
    MOZ_RELEASE_ASSERT(!aSyncLoopTarget);
    mDelayedDebuggeeRunnables.AppendElement(runnable);
    return NS_OK;
  }

  if (!mThread) {
    if (ParentStatus() == Pending || mStatus == Pending) {
      mPreStartRunnables.AppendElement(runnable);
      return NS_OK;
    }

    NS_WARNING(
        "Using a worker event target after the thread has already"
        "been released!");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv;
  if (aSyncLoopTarget) {
    rv = aSyncLoopTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
  } else {
    // WorkerDebuggeeRunnables don't need any special treatment here. True,
    // they should not be delivered to a frozen worker. But frozen workers
    // aren't drawing from the thread's main event queue anyway, only from
    // mControlQueue.
    rv = mThread->DispatchAnyThread(WorkerThreadFriendKey(), runnable.forget());
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mCondVar.Notify();
  return NS_OK;
}

void WorkerPrivate::EnableDebugger() {
  AssertIsOnParentThread();

  if (NS_FAILED(RegisterWorkerDebugger(this))) {
    NS_WARNING("Failed to register worker debugger!");
    return;
  }
}

void WorkerPrivate::DisableDebugger() {
  AssertIsOnParentThread();

  // RegisterDebuggerMainThreadRunnable might be dispatched but not executed.
  // Wait for its execution before unregistraion.
  if (!NS_IsMainThread()) {
    WaitForIsDebuggerRegistered(true);
  }

  if (NS_FAILED(UnregisterWorkerDebugger(this))) {
    NS_WARNING("Failed to unregister worker debugger!");
  }
}

nsresult WorkerPrivate::DispatchControlRunnable(
    already_AddRefed<WorkerControlRunnable> aWorkerControlRunnable) {
  // May be called on any thread!
  RefPtr<WorkerControlRunnable> runnable(aWorkerControlRunnable);
  MOZ_ASSERT(runnable);

  {
    MutexAutoLock lock(mMutex);

    if (mStatus == Dead) {
      return NS_ERROR_UNEXPECTED;
    }

    // Transfer ownership to the control queue.
    mControlQueue.Push(runnable.forget().take());

    if (JSContext* cx = mJSContext) {
      MOZ_ASSERT(mThread);
      JS_RequestInterruptCallback(cx);
    }

    mCondVar.Notify();
  }

  return NS_OK;
}

nsresult WorkerPrivate::DispatchDebuggerRunnable(
    already_AddRefed<WorkerRunnable> aDebuggerRunnable) {
  // May be called on any thread!

  RefPtr<WorkerRunnable> runnable(aDebuggerRunnable);

  MOZ_ASSERT(runnable);

  {
    MutexAutoLock lock(mMutex);

    if (mStatus == Dead) {
      NS_WARNING(
          "A debugger runnable was posted to a worker that is already "
          "shutting down!");
      return NS_ERROR_UNEXPECTED;
    }

    // Transfer ownership to the debugger queue.
    mDebuggerQueue.Push(runnable.forget().take());

    mCondVar.Notify();
  }

  return NS_OK;
}

already_AddRefed<WorkerRunnable> WorkerPrivate::MaybeWrapAsWorkerRunnable(
    already_AddRefed<nsIRunnable> aRunnable) {
  // May be called on any thread!

  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  MOZ_ASSERT(runnable);

  RefPtr<WorkerRunnable> workerRunnable =
      WorkerRunnable::FromRunnable(runnable);
  if (workerRunnable) {
    return workerRunnable.forget();
  }

  nsCOMPtr<nsIDiscardableRunnable> maybe = do_QueryInterface(runnable);
  if (!maybe) {
    MOZ_CRASH(
        "All runnables destined for a worker thread must be "
        "nsIDiscardableRunnable!");
  }

  workerRunnable = new ExternalRunnableWrapper(this, runnable);
  return workerRunnable.forget();
}

bool WorkerPrivate::Start() {
  // May be called on any thread!
  {
    MutexAutoLock lock(mMutex);
    NS_ASSERTION(mParentStatus != Running, "How can this be?!");

    if (mParentStatus == Pending) {
      mParentStatus = Running;
      return true;
    }
  }

  return false;
}

// aCx is null when called from the finalizer
bool WorkerPrivate::Notify(WorkerStatus aStatus) {
  AssertIsOnParentThread();

  bool pending;
  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= aStatus) {
      return true;
    }

    pending = mParentStatus == Pending;
    mParentStatus = aStatus;
  }

  if (pending) {
#ifdef DEBUG
    {
      // Fake a thread here just so that our assertions don't go off for no
      // reason.
      nsIThread* currentThread = NS_GetCurrentThread();
      MOZ_ASSERT(currentThread);

      MOZ_ASSERT(!mPRThread);
      mPRThread = PRThreadFromThread(currentThread);
      MOZ_ASSERT(mPRThread);
    }
#endif

    // Worker never got a chance to run, go ahead and delete it.
    ScheduleDeletion(WorkerPrivate::WorkerNeverRan);
    return true;
  }

  // No Canceling timeout is needed.
  if (mCancelingTimer) {
    mCancelingTimer->Cancel();
    mCancelingTimer = nullptr;
  }

  RefPtr<NotifyRunnable> runnable = new NotifyRunnable(this, aStatus);
  return runnable->Dispatch();
}

bool WorkerPrivate::Freeze(const nsPIDOMWindowInner* aWindow) {
  AssertIsOnParentThread();

  mParentFrozen = true;

  // WorkerDebuggeeRunnables sent from a worker to content must not be delivered
  // while the worker is frozen.
  //
  // Since a top-level worker and all its children share the same
  // mMainThreadDebuggeeEventTarget, it's sufficient to do this only in the
  // top-level worker.
  if (aWindow) {
    // This is called from WorkerPrivate construction, and We may not have
    // allocated mMainThreadDebuggeeEventTarget yet.
    if (mMainThreadDebuggeeEventTarget) {
      // Pausing a ThrottledEventQueue is infallible.
      MOZ_ALWAYS_SUCCEEDS(mMainThreadDebuggeeEventTarget->SetIsPaused(true));
    }
  }

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Canceling) {
      return true;
    }
  }

  DisableDebugger();

  RefPtr<FreezeRunnable> runnable = new FreezeRunnable(this);
  if (!runnable->Dispatch()) {
    return false;
  }

  return true;
}

bool WorkerPrivate::Thaw(const nsPIDOMWindowInner* aWindow) {
  AssertIsOnParentThread();
  MOZ_ASSERT(mParentFrozen);

  mParentFrozen = false;

  // Delivery of WorkerDebuggeeRunnables to the window may resume.
  //
  // Since a top-level worker and all its children share the same
  // mMainThreadDebuggeeEventTarget, it's sufficient to do this only in the
  // top-level worker.
  if (aWindow) {
    // Since the worker is no longer frozen, only a paused parent window should
    // require the queue to remain paused.
    //
    // This can only fail if the ThrottledEventQueue cannot dispatch its
    // executor to the main thread, in which case the main thread was never
    // going to draw runnables from it anyway, so the failure doesn't matter.
    Unused << mMainThreadDebuggeeEventTarget->SetIsPaused(
        IsParentWindowPaused());
  }

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Canceling) {
      return true;
    }
  }

  EnableDebugger();

  RefPtr<ThawRunnable> runnable = new ThawRunnable(this);
  if (!runnable->Dispatch()) {
    return false;
  }

  return true;
}

void WorkerPrivate::ParentWindowPaused() {
  AssertIsOnMainThread();
  MOZ_ASSERT(!mParentWindowPaused);
  mParentWindowPaused = true;

  // This is called from WorkerPrivate construction, and we may not have
  // allocated mMainThreadDebuggeeEventTarget yet.
  if (mMainThreadDebuggeeEventTarget) {
    // Pausing a ThrottledEventQueue is infallible.
    MOZ_ALWAYS_SUCCEEDS(mMainThreadDebuggeeEventTarget->SetIsPaused(true));
  }
}

void WorkerPrivate::ParentWindowResumed() {
  AssertIsOnMainThread();

  MOZ_ASSERT(mParentWindowPaused);
  mParentWindowPaused = false;

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Canceling) {
      return;
    }
  }

  // Since the window is no longer paused, the queue should only remain paused
  // if the worker is frozen.
  //
  // This can only fail if the ThrottledEventQueue cannot dispatch its executor
  // to the main thread, in which case the main thread was never going to draw
  // runnables from it anyway, so the failure doesn't matter.
  Unused << mMainThreadDebuggeeEventTarget->SetIsPaused(IsFrozen());
}

void WorkerPrivate::PropagateStorageAccessPermissionGranted() {
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Canceling) {
      return;
    }
  }

  RefPtr<PropagateStorageAccessPermissionGrantedRunnable> runnable =
      new PropagateStorageAccessPermissionGrantedRunnable(this);
  Unused << NS_WARN_IF(!runnable->Dispatch());
}

bool WorkerPrivate::Close() {
  mMutex.AssertCurrentThreadOwns();
  if (mParentStatus < Closing) {
    mParentStatus = Closing;
  }

  return true;
}

bool WorkerPrivate::ModifyBusyCount(bool aIncrease) {
  AssertIsOnParentThread();

  MOZ_ASSERT(aIncrease || mBusyCount, "Mismatched busy count mods!");

  if (aIncrease) {
    mBusyCount++;
    return true;
  }

  if (--mBusyCount == 0) {
    bool shouldCancel;
    {
      MutexAutoLock lock(mMutex);
      shouldCancel = mParentStatus == Canceling;
    }

    if (shouldCancel && !Cancel()) {
      return false;
    }
  }

  return true;
}

bool WorkerPrivate::ProxyReleaseMainThreadObjects() {
  AssertIsOnParentThread();
  MOZ_ASSERT(!mMainThreadObjectsForgotten);

  nsCOMPtr<nsILoadGroup> loadGroupToCancel;
  // If we're not overriden, then do nothing here.  Let the load group get
  // handled in ForgetMainThreadObjects().
  if (mLoadInfo.mInterfaceRequestor) {
    mLoadInfo.mLoadGroup.swap(loadGroupToCancel);
  }

  bool result = mLoadInfo.ProxyReleaseMainThreadObjects(
      this, std::move(loadGroupToCancel));

  mMainThreadObjectsForgotten = true;

  return result;
}

void WorkerPrivate::UpdateContextOptions(
    const JS::ContextOptions& aContextOptions) {
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);
    mJSSettings.contextOptions = aContextOptions;
  }

  RefPtr<UpdateContextOptionsRunnable> runnable =
      new UpdateContextOptionsRunnable(this, aContextOptions);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to update worker context options!");
  }
}

void WorkerPrivate::UpdateLanguages(const nsTArray<nsString>& aLanguages) {
  AssertIsOnParentThread();

  RefPtr<UpdateLanguagesRunnable> runnable =
      new UpdateLanguagesRunnable(this, aLanguages);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to update worker languages!");
  }
}

void WorkerPrivate::UpdateJSWorkerMemoryParameter(JSGCParamKey aKey,
                                                  Maybe<uint32_t> aValue) {
  AssertIsOnParentThread();

  bool changed = false;

  {
    MutexAutoLock lock(mMutex);
    changed = mJSSettings.ApplyGCSetting(aKey, aValue);
  }

  if (changed) {
    RefPtr<UpdateJSWorkerMemoryParameterRunnable> runnable =
        new UpdateJSWorkerMemoryParameterRunnable(this, aKey, aValue);
    if (!runnable->Dispatch()) {
      NS_WARNING("Failed to update memory parameter!");
    }
  }
}

#ifdef JS_GC_ZEAL
void WorkerPrivate::UpdateGCZeal(uint8_t aGCZeal, uint32_t aFrequency) {
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);
    mJSSettings.gcZeal = aGCZeal;
    mJSSettings.gcZealFrequency = aFrequency;
  }

  RefPtr<UpdateGCZealRunnable> runnable =
      new UpdateGCZealRunnable(this, aGCZeal, aFrequency);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to update worker gczeal!");
  }
}
#endif

void WorkerPrivate::SetLowMemoryState(bool aState) {
  AssertIsOnParentThread();

  RefPtr<SetLowMemoryStateRunnable> runnable =
      new SetLowMemoryStateRunnable(this, aState);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to set low memory state!");
  }
}

void WorkerPrivate::GarbageCollect(bool aShrinking) {
  AssertIsOnParentThread();

  RefPtr<GarbageCollectRunnable> runnable = new GarbageCollectRunnable(
      this, aShrinking, /* collectChildren = */ true);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to GC worker!");
  }
}

void WorkerPrivate::CycleCollect() {
  AssertIsOnParentThread();

  RefPtr<CycleCollectRunnable> runnable =
      new CycleCollectRunnable(this, /* collectChildren = */ true);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to CC worker!");
  }
}

void WorkerPrivate::OfflineStatusChangeEvent(bool aIsOffline) {
  AssertIsOnParentThread();

  RefPtr<OfflineStatusChangeRunnable> runnable =
      new OfflineStatusChangeRunnable(this, aIsOffline);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to dispatch offline status change event!");
  }
}

void WorkerPrivate::OfflineStatusChangeEventInternal(bool aIsOffline) {
  auto data = mWorkerThreadAccessible.Access();

  // The worker is already in this state. No need to dispatch an event.
  if (data->mOnLine == !aIsOffline) {
    return;
  }

  for (uint32_t index = 0; index < data->mChildWorkers.Length(); ++index) {
    data->mChildWorkers[index]->OfflineStatusChangeEvent(aIsOffline);
  }

  data->mOnLine = !aIsOffline;
  WorkerGlobalScope* globalScope = GlobalScope();
  RefPtr<WorkerNavigator> nav = globalScope->GetExistingNavigator();
  if (nav) {
    nav->SetOnLine(data->mOnLine);
  }

  nsString eventType;
  if (aIsOffline) {
    eventType.AssignLiteral("offline");
  } else {
    eventType.AssignLiteral("online");
  }

  RefPtr<Event> event = NS_NewDOMEvent(globalScope, nullptr, nullptr);

  event->InitEvent(eventType, false, false);
  event->SetTrusted(true);

  globalScope->DispatchEvent(*event);
}

void WorkerPrivate::MemoryPressure() {
  AssertIsOnParentThread();

  RefPtr<MemoryPressureRunnable> runnable = new MemoryPressureRunnable(this);
  Unused << NS_WARN_IF(!runnable->Dispatch());
}

void WorkerPrivate::WorkerScriptLoaded() {
  AssertIsOnMainThread();

  if (IsSharedWorker() || IsServiceWorker()) {
    // No longer need to hold references to the window or document we came from.
    mLoadInfo.mWindow = nullptr;
    mLoadInfo.mScriptContext = nullptr;
  }
}

void WorkerPrivate::SetBaseURI(nsIURI* aBaseURI) {
  AssertIsOnMainThread();

  if (!mLoadInfo.mBaseURI) {
    NS_ASSERTION(GetParent(), "Shouldn't happen without a parent!");
    mLoadInfo.mResolvedScriptURI = aBaseURI;
  }

  mLoadInfo.mBaseURI = aBaseURI;

  if (NS_FAILED(aBaseURI->GetSpec(mLocationInfo.mHref))) {
    mLocationInfo.mHref.Truncate();
  }

  mLocationInfo.mHostname.Truncate();
  nsContentUtils::GetHostOrIPv6WithBrackets(aBaseURI, mLocationInfo.mHostname);

  nsCOMPtr<nsIURL> url(do_QueryInterface(aBaseURI));
  if (!url || NS_FAILED(url->GetFilePath(mLocationInfo.mPathname))) {
    mLocationInfo.mPathname.Truncate();
  }

  nsCString temp;

  if (url && NS_SUCCEEDED(url->GetQuery(temp)) && !temp.IsEmpty()) {
    mLocationInfo.mSearch.Assign('?');
    mLocationInfo.mSearch.Append(temp);
  }

  if (NS_SUCCEEDED(aBaseURI->GetRef(temp)) && !temp.IsEmpty()) {
    if (mLocationInfo.mHash.IsEmpty()) {
      mLocationInfo.mHash.Assign('#');
      mLocationInfo.mHash.Append(temp);
    }
  }

  if (NS_SUCCEEDED(aBaseURI->GetScheme(mLocationInfo.mProtocol))) {
    mLocationInfo.mProtocol.Append(':');
  } else {
    mLocationInfo.mProtocol.Truncate();
  }

  int32_t port;
  if (NS_SUCCEEDED(aBaseURI->GetPort(&port)) && port != -1) {
    mLocationInfo.mPort.AppendInt(port);

    nsAutoCString host(mLocationInfo.mHostname);
    host.Append(':');
    host.Append(mLocationInfo.mPort);

    mLocationInfo.mHost.Assign(host);
  } else {
    mLocationInfo.mHost.Assign(mLocationInfo.mHostname);
  }

  nsContentUtils::GetUTFOrigin(aBaseURI, mLocationInfo.mOrigin);
}

nsresult WorkerPrivate::SetPrincipalsAndCSPOnMainThread(
    nsIPrincipal* aPrincipal, nsIPrincipal* aPartitionedPrincipal,
    nsILoadGroup* aLoadGroup, nsIContentSecurityPolicy* aCsp) {
  return mLoadInfo.SetPrincipalsAndCSPOnMainThread(
      aPrincipal, aPartitionedPrincipal, aLoadGroup, aCsp);
}

nsresult WorkerPrivate::SetPrincipalsAndCSPFromChannel(nsIChannel* aChannel) {
  return mLoadInfo.SetPrincipalsAndCSPFromChannel(aChannel);
}

bool WorkerPrivate::FinalChannelPrincipalIsValid(nsIChannel* aChannel) {
  return mLoadInfo.FinalChannelPrincipalIsValid(aChannel);
}

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
bool WorkerPrivate::PrincipalURIMatchesScriptURL() {
  return mLoadInfo.PrincipalURIMatchesScriptURL();
}
#endif

void WorkerPrivate::UpdateOverridenLoadGroup(nsILoadGroup* aBaseLoadGroup) {
  AssertIsOnMainThread();

  // The load group should have been overriden at init time.
  mLoadInfo.mInterfaceRequestor->MaybeAddBrowserChild(aBaseLoadGroup);
}

#ifdef DEBUG

void WorkerPrivate::AssertIsOnParentThread() const {
  if (GetParent()) {
    GetParent()->AssertIsOnWorkerThread();
  } else {
    AssertIsOnMainThread();
  }
}

void WorkerPrivate::AssertInnerWindowIsCorrect() const {
  AssertIsOnParentThread();

  // Only care about top level workers from windows.
  if (mParent || !mLoadInfo.mWindow) {
    return;
  }

  AssertIsOnMainThread();

  nsPIDOMWindowOuter* outer = mLoadInfo.mWindow->GetOuterWindow();
  NS_ASSERTION(outer && outer->GetCurrentInnerWindow() == mLoadInfo.mWindow,
               "Inner window no longer correct!");
}

#endif

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
bool WorkerPrivate::PrincipalIsValid() const {
  return mLoadInfo.PrincipalIsValid();
}
#endif

WorkerPrivate::WorkerThreadAccessible::WorkerThreadAccessible(
    WorkerPrivate* const aParent)
    : mNumWorkerRefsPreventingShutdownStart(0),
      mDebuggerEventLoopLevel(0),
      mErrorHandlerRecursionCount(0),
      mNextTimeoutId(1),
      mCurrentTimerNestingLevel(0),
      mFrozen(false),
      mTimerRunning(false),
      mRunningExpiredTimeouts(false),
      mPeriodicGCTimerRunning(false),
      mIdleGCTimerRunning(false),
      mOnLine(aParent ? aParent->OnLine() : !NS_IsOffline()),
      mJSThreadExecutionGranted(false),
      mCCCollectedAnything(false) {}

namespace {

bool IsNewWorkerSecureContext(const WorkerPrivate* const aParent,
                              const WorkerType aWorkerType,
                              const WorkerLoadInfo& aLoadInfo) {
  if (aParent) {
    return aParent->IsSecureContext();
  }

  // Our secure context state depends on the kind of worker we have.

  if (aLoadInfo.mPrincipalIsSystem) {
    return true;
  }

  if (aWorkerType == WorkerTypeService) {
    return true;
  }

  if (aLoadInfo.mSecureContext != WorkerLoadInfo::eNotSet) {
    return aLoadInfo.mSecureContext == WorkerLoadInfo::eSecureContext;
  }

  MOZ_ASSERT_UNREACHABLE(
      "non-chrome worker that is not a service worker "
      "that has no parent and no associated window");

  return false;
}

}  // namespace

WorkerPrivate::WorkerPrivate(
    WorkerPrivate* aParent, const nsAString& aScriptURL, bool aIsChromeWorker,
    WorkerType aWorkerType, const nsAString& aWorkerName,
    const nsACString& aServiceWorkerScope, WorkerLoadInfo& aLoadInfo,
    nsString&& aId, const nsID& aAgentClusterId,
    const nsILoadInfo::CrossOriginOpenerPolicy aAgentClusterOpenerPolicy)
    : mMutex("WorkerPrivate Mutex"),
      mCondVar(mMutex, "WorkerPrivate CondVar"),
      mParent(aParent),
      mScriptURL(aScriptURL),
      mWorkerName(aWorkerName),
      mWorkerType(aWorkerType),
      mLoadInfo(std::move(aLoadInfo)),
      mDebugger(nullptr),
      mJSContext(nullptr),
      mPRThread(nullptr),
      mWorkerControlEventTarget(new WorkerEventTarget(
          this, WorkerEventTarget::Behavior::ControlOnly)),
      mWorkerHybridEventTarget(
          new WorkerEventTarget(this, WorkerEventTarget::Behavior::Hybrid)),
      mParentStatus(Pending),
      mStatus(Pending),
      mBusyCount(0),
      mLoadingWorkerScript(false),
      mCreationTimeStamp(TimeStamp::Now()),
      mCreationTimeHighRes((double)PR_Now() / PR_USEC_PER_MSEC),
      mReportedUseCounters(false),
      mAgentClusterId(aAgentClusterId),
      mWorkerThreadAccessible(aParent),
      mPostSyncLoopOperations(0),
      mParentWindowPaused(false),
      mCancelAllPendingRunnables(false),
      mWorkerScriptExecutedSuccessfully(false),
      mFetchHandlerWasAdded(false),
      mMainThreadObjectsForgotten(false),
      mIsChromeWorker(aIsChromeWorker),
      mParentFrozen(false),
      mIsSecureContext(
          IsNewWorkerSecureContext(mParent, mWorkerType, mLoadInfo)),
      mDebuggerRegistered(false),
      mDebuggerReady(true),
      mIsInAutomation(false),
      mId(std::move(aId)),
      mAgentClusterOpenerPolicy(aAgentClusterOpenerPolicy) {
  MOZ_ASSERT_IF(!IsDedicatedWorker(), NS_IsMainThread());

  if (aParent) {
    aParent->AssertIsOnWorkerThread();

    // Note that this copies our parent's secure context state into mJSSettings.
    aParent->CopyJSSettings(mJSSettings);

    MOZ_ASSERT_IF(mIsChromeWorker, mIsSecureContext);

    mIsInAutomation = aParent->IsInAutomation();

    MOZ_ASSERT(IsDedicatedWorker());

    if (aParent->mParentFrozen) {
      Freeze(nullptr);
    }
  } else {
    AssertIsOnMainThread();

    RuntimeService::GetDefaultJSSettings(mJSSettings);

    {
      JS::RealmOptions& chromeRealmOptions = mJSSettings.chromeRealmOptions;
      JS::RealmOptions& contentRealmOptions = mJSSettings.contentRealmOptions;

      JS::RealmBehaviors& chromeRealmBehaviors = chromeRealmOptions.behaviors();
      JS::RealmBehaviors& contentRealmBehaviors =
          contentRealmOptions.behaviors();

      bool usesSystemPrincipal = UsesSystemPrincipal();

      // Make timing imprecise in unprivileged code to blunt Spectre timing
      // attacks.
      bool clampAndJitterTime = !usesSystemPrincipal;
      chromeRealmBehaviors.setClampAndJitterTime(clampAndJitterTime);
      contentRealmBehaviors.setClampAndJitterTime(clampAndJitterTime);

      JS::RealmCreationOptions& chromeCreationOptions =
          chromeRealmOptions.creationOptions();
      JS::RealmCreationOptions& contentCreationOptions =
          contentRealmOptions.creationOptions();

      // Expose uneval and toSource functions only if this is privileged code.
      bool toSourceEnabled = usesSystemPrincipal;
      chromeCreationOptions.setToSourceEnabled(toSourceEnabled);
      contentCreationOptions.setToSourceEnabled(toSourceEnabled);

      if (mIsSecureContext) {
        chromeCreationOptions.setSecureContext(true);
        contentCreationOptions.setSecureContext(true);
      }

      // The SharedArrayBuffer global constructor property should not be present
      // in a fresh global object when shared memory objects aren't allowed
      // (because COOP/COEP support isn't enabled, or because COOP/COEP don't
      // act to isolate this worker to a separate process).
      const bool defineSharedArrayBufferConstructor = IsSharedMemoryAllowed();
      chromeCreationOptions.setDefineSharedArrayBufferConstructor(
          defineSharedArrayBufferConstructor);
      contentCreationOptions.setDefineSharedArrayBufferConstructor(
          defineSharedArrayBufferConstructor);
    }

    mIsInAutomation = xpc::IsInAutomation();

    // Our parent can get suspended after it initiates the async creation
    // of a new worker thread.  In this case suspend the new worker as well.
    if (mLoadInfo.mWindow && mLoadInfo.mWindow->IsSuspended()) {
      ParentWindowPaused();
    }

    if (mLoadInfo.mWindow && mLoadInfo.mWindow->IsFrozen()) {
      Freeze(mLoadInfo.mWindow);
    }
  }

  nsCOMPtr<nsISerialEventTarget> target;

  // A child worker just inherits the parent workers ThrottledEventQueue
  // and main thread target for now.  This is mainly due to the restriction
  // that ThrottledEventQueue can only be created on the main thread at the
  // moment.
  if (aParent) {
    mMainThreadEventTargetForMessaging =
        aParent->mMainThreadEventTargetForMessaging;
    mMainThreadEventTarget = aParent->mMainThreadEventTarget;
    mMainThreadDebuggeeEventTarget = aParent->mMainThreadDebuggeeEventTarget;
    return;
  }

  MOZ_ASSERT(NS_IsMainThread());
  target = GetWindow()
               ? GetWindow()->GetBrowsingContextGroup()->GetWorkerEventQueue()
               : nullptr;

  if (!target) {
    target = GetMainThreadSerialEventTarget();
    MOZ_DIAGNOSTIC_ASSERT(target);
  }

  // Throttle events to the main thread using a ThrottledEventQueue specific to
  // this tree of worker threads.
  mMainThreadEventTargetForMessaging =
      ThrottledEventQueue::Create(target, "Worker queue for messaging");
  if (StaticPrefs::dom_worker_use_medium_high_event_queue()) {
    mMainThreadEventTarget = ThrottledEventQueue::Create(
        GetMainThreadSerialEventTarget(), "Worker queue",
        nsIRunnablePriority::PRIORITY_MEDIUMHIGH);
  } else {
    mMainThreadEventTarget = mMainThreadEventTargetForMessaging;
  }
  mMainThreadDebuggeeEventTarget =
      ThrottledEventQueue::Create(target, "Worker debuggee queue");
  if (IsParentWindowPaused() || IsFrozen()) {
    MOZ_ALWAYS_SUCCEEDS(mMainThreadDebuggeeEventTarget->SetIsPaused(true));
  }
}

WorkerPrivate::~WorkerPrivate() {
  DropJSObjects(this);

  mWorkerControlEventTarget->ForgetWorkerPrivate(this);

  // We force the hybrid event target to forget the thread when we
  // enter the Killing state, but we do it again here to be safe.
  // Its possible that we may be created and destroyed without progressing
  // to Killing via some obscure code path.
  mWorkerHybridEventTarget->ForgetWorkerPrivate(this);
}

// static
already_AddRefed<WorkerPrivate> WorkerPrivate::Constructor(
    JSContext* aCx, const nsAString& aScriptURL, bool aIsChromeWorker,
    WorkerType aWorkerType, const nsAString& aWorkerName,
    const nsACString& aServiceWorkerScope, WorkerLoadInfo* aLoadInfo,
    ErrorResult& aRv, nsString aId) {
  WorkerPrivate* parent =
      NS_IsMainThread() ? nullptr : GetCurrentThreadWorkerPrivate();

  // If this is a sub-worker, we need to keep the parent worker alive until this
  // one is registered.
  RefPtr<StrongWorkerRef> workerRef;
  if (parent) {
    parent->AssertIsOnWorkerThread();

    workerRef = StrongWorkerRef::Create(parent, "WorkerPrivate::Constructor");
    if (NS_WARN_IF(!workerRef)) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return nullptr;
    }
  } else {
    AssertIsOnMainThread();
  }

  Maybe<WorkerLoadInfo> stackLoadInfo;
  if (!aLoadInfo) {
    stackLoadInfo.emplace();

    nsresult rv =
        GetLoadInfo(aCx, nullptr, parent, aScriptURL, aIsChromeWorker,
                    InheritLoadGroup, aWorkerType, stackLoadInfo.ptr());
    aRv.MightThrowJSException();
    if (NS_FAILED(rv)) {
      workerinternals::ReportLoadError(aRv, rv, aScriptURL);
      return nullptr;
    }

    aLoadInfo = stackLoadInfo.ptr();
  }

  // NB: This has to be done before creating the WorkerPrivate, because it will
  // attempt to use static variables that are initialized in the RuntimeService
  // constructor.
  RuntimeService* runtimeService;

  if (!parent) {
    runtimeService = RuntimeService::GetOrCreateService();
    if (!runtimeService) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
  } else {
    runtimeService = RuntimeService::GetService();
  }

  MOZ_ASSERT(runtimeService);

  // Don't create a worker with the shutting down RuntimeService.
  if (runtimeService->IsShuttingDown()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsILoadInfo::CrossOriginOpenerPolicy agentClusterCoop =
      nsILoadInfo::OPENER_POLICY_UNSAFE_NONE;
  nsID agentClusterId;
  if (parent) {
    MOZ_ASSERT(aWorkerType == WorkerType::WorkerTypeDedicated);

    agentClusterId = parent->AgentClusterId();
    agentClusterCoop = parent->mAgentClusterOpenerPolicy;
  } else {
    AssertIsOnMainThread();

    if (aWorkerType == WorkerType::WorkerTypeService ||
        aWorkerType == WorkerType::WorkerTypeShared) {
      agentClusterId = aLoadInfo->mAgentClusterId;
    } else if (aLoadInfo->mWindow) {
      Document* doc = aLoadInfo->mWindow->GetExtantDoc();
      MOZ_DIAGNOSTIC_ASSERT(doc);
      RefPtr<DocGroup> docGroup = doc->GetDocGroup();

      agentClusterId = docGroup ? docGroup->AgentClusterId()
                                : nsContentUtils::GenerateUUID();

      BrowsingContext* bc = aLoadInfo->mWindow->GetBrowsingContext();
      MOZ_DIAGNOSTIC_ASSERT(bc);
      agentClusterCoop = bc->Top()->GetOpenerPolicy();
    } else {
      // If the window object was failed to be set into the WorkerLoadInfo, we
      // make the worker into another agent cluster group instead of failures.
      agentClusterId = nsContentUtils::GenerateUUID();
    }
  }

  RefPtr<WorkerPrivate> worker =
      new WorkerPrivate(parent, aScriptURL, aIsChromeWorker, aWorkerType,
                        aWorkerName, aServiceWorkerScope, *aLoadInfo,
                        std::move(aId), agentClusterId, agentClusterCoop);

  // Gecko contexts always have an explicitly-set default locale (set by
  // XPJSRuntime::Initialize for the main thread, set by
  // WorkerThreadPrimaryRunnable::Run for workers just before running worker
  // code), so this is never SpiderMonkey's builtin default locale.
  JS::UniqueChars defaultLocale = JS_GetDefaultLocale(aCx);
  if (NS_WARN_IF(!defaultLocale)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  worker->mDefaultLocale = std::move(defaultLocale);

  if (!runtimeService->RegisterWorker(*worker)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  worker->EnableDebugger();

  MOZ_DIAGNOSTIC_ASSERT(worker->PrincipalIsValid());

  UniquePtr<SerializedStackHolder> stack;
  if (worker->IsWatchedByDevTools()) {
    stack = GetCurrentStackForNetMonitor(aCx);
  }

  RefPtr<CompileScriptRunnable> compiler =
      new CompileScriptRunnable(worker, std::move(stack), aScriptURL);
  if (!compiler->Dispatch()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  worker->mSelfRef = worker;

  return worker.forget();
}

nsresult WorkerPrivate::SetIsDebuggerReady(bool aReady) {
  AssertIsOnMainThread();
  MutexAutoLock lock(mMutex);

  if (mDebuggerReady == aReady) {
    return NS_OK;
  }

  if (!aReady && mDebuggerRegistered) {
    // The debugger can only be marked as not ready during registration.
    return NS_ERROR_FAILURE;
  }

  mDebuggerReady = aReady;

  if (aReady && mDebuggerRegistered) {
    // Dispatch all the delayed runnables without releasing the lock, to ensure
    // that the order in which debuggee runnables execute is the same as the
    // order in which they were originally dispatched.
    auto pending = std::move(mDelayedDebuggeeRunnables);
    for (uint32_t i = 0; i < pending.Length(); i++) {
      RefPtr<WorkerRunnable> runnable = std::move(pending[i]);
      nsresult rv = DispatchLockHeld(runnable.forget(), nullptr, lock);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    MOZ_RELEASE_ASSERT(mDelayedDebuggeeRunnables.IsEmpty());
  }

  return NS_OK;
}

// static
nsresult WorkerPrivate::GetLoadInfo(JSContext* aCx, nsPIDOMWindowInner* aWindow,
                                    WorkerPrivate* aParent,
                                    const nsAString& aScriptURL,
                                    bool aIsChromeWorker,
                                    LoadGroupBehavior aLoadGroupBehavior,
                                    WorkerType aWorkerType,
                                    WorkerLoadInfo* aLoadInfo) {
  using namespace mozilla::dom::workerinternals;

  MOZ_ASSERT(aCx);
  MOZ_ASSERT_IF(NS_IsMainThread(),
                aCx == nsContentUtils::GetCurrentJSContext());

  if (aWindow) {
    AssertIsOnMainThread();
  }

  WorkerLoadInfo loadInfo;
  nsresult rv;

  if (aParent) {
    aParent->AssertIsOnWorkerThread();

    // If the parent is going away give up now.
    WorkerStatus parentStatus;
    {
      MutexAutoLock lock(aParent->mMutex);
      parentStatus = aParent->mStatus;
    }

    if (parentStatus > Running) {
      return NS_ERROR_FAILURE;
    }

    // Passing a pointer to our stack loadInfo is safe here because this
    // method uses a sync runnable to get the channel from the main thread.
    rv = ChannelFromScriptURLWorkerThread(aCx, aParent, aScriptURL, loadInfo);
    if (NS_FAILED(rv)) {
      MOZ_ALWAYS_TRUE(loadInfo.ProxyReleaseMainThreadObjects(aParent));
      return rv;
    }

    // Now that we've spun the loop there's no guarantee that our parent is
    // still alive.  We may have received control messages initiating shutdown.
    {
      MutexAutoLock lock(aParent->mMutex);
      parentStatus = aParent->mStatus;
    }

    if (parentStatus > Running) {
      MOZ_ALWAYS_TRUE(loadInfo.ProxyReleaseMainThreadObjects(aParent));
      return NS_ERROR_FAILURE;
    }

    loadInfo.mDomain = aParent->Domain();
    loadInfo.mFromWindow = aParent->IsFromWindow();
    loadInfo.mWindowID = aParent->WindowID();
    loadInfo.mStorageAccess = aParent->StorageAccess();
    loadInfo.mUseRegularPrincipal = aParent->UseRegularPrincipal();
    loadInfo.mHasStorageAccessPermissionGranted =
        aParent->HasStorageAccessPermissionGranted();
    loadInfo.mCookieJarSettings = aParent->CookieJarSettings();
    loadInfo.mOriginAttributes = aParent->GetOriginAttributes();
    loadInfo.mServiceWorkersTestingInWindow =
        aParent->ServiceWorkersTestingInWindow();
    loadInfo.mParentController = aParent->GlobalScope()->GetController();
    loadInfo.mWatchedByDevTools = aParent->IsWatchedByDevTools();
  } else {
    AssertIsOnMainThread();

    // Make sure that the IndexedDatabaseManager is set up
    Unused << NS_WARN_IF(!IndexedDatabaseManager::GetOrCreate());

    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    MOZ_ASSERT(ssm);

    bool isChrome = nsContentUtils::IsSystemCaller(aCx);

    // First check to make sure the caller has permission to make a privileged
    // worker if they called the ChromeWorker/ChromeSharedWorker constructor.
    if (aIsChromeWorker && !isChrome) {
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    // Chrome callers (whether creating a ChromeWorker or Worker) always get the
    // system principal here as they're allowed to load anything. The script
    // loader will refuse to run any script that does not also have the system
    // principal.
    if (isChrome) {
      rv = ssm->GetSystemPrincipal(getter_AddRefs(loadInfo.mLoadingPrincipal));
      NS_ENSURE_SUCCESS(rv, rv);

      loadInfo.mPrincipalIsSystem = true;
    }

    // See if we're being called from a window.
    nsCOMPtr<nsPIDOMWindowInner> globalWindow = aWindow;
    if (!globalWindow) {
      globalWindow = xpc::CurrentWindowOrNull(aCx);
    }

    nsCOMPtr<Document> document;
    Maybe<ClientInfo> clientInfo;

    if (globalWindow) {
      // Only use the current inner window, and only use it if the caller can
      // access it.
      if (nsPIDOMWindowOuter* outerWindow = globalWindow->GetOuterWindow()) {
        loadInfo.mWindow = outerWindow->GetCurrentInnerWindow();
      }

      BrowsingContext* browsingContext = globalWindow->GetBrowsingContext();

      // TODO: fix this for SharedWorkers with multiple documents (bug
      // 1177935)
      loadInfo.mServiceWorkersTestingInWindow =
          browsingContext &&
          browsingContext->Top()->ServiceWorkersTestingEnabled();

      if (!loadInfo.mWindow ||
          (globalWindow != loadInfo.mWindow &&
           !nsContentUtils::CanCallerAccess(loadInfo.mWindow))) {
        return NS_ERROR_DOM_SECURITY_ERR;
      }

      nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(loadInfo.mWindow);
      MOZ_ASSERT(sgo);

      loadInfo.mScriptContext = sgo->GetContext();
      NS_ENSURE_TRUE(loadInfo.mScriptContext, NS_ERROR_FAILURE);

      // If we're called from a window then we can dig out the principal and URI
      // from the document.
      document = loadInfo.mWindow->GetExtantDoc();
      NS_ENSURE_TRUE(document, NS_ERROR_FAILURE);

      loadInfo.mBaseURI = document->GetDocBaseURI();
      loadInfo.mLoadGroup = document->GetDocumentLoadGroup();
      NS_ENSURE_TRUE(loadInfo.mLoadGroup, NS_ERROR_FAILURE);

      clientInfo = globalWindow->GetClientInfo();

      // Use the document's NodePrincipal as loading principal if we're not
      // being called from chrome.
      if (!loadInfo.mLoadingPrincipal) {
        loadInfo.mLoadingPrincipal = document->NodePrincipal();
        NS_ENSURE_TRUE(loadInfo.mLoadingPrincipal, NS_ERROR_FAILURE);

        // We use the document's base domain to limit the number of workers
        // each domain can create. For sandboxed documents, we use the domain
        // of their first non-sandboxed document, walking up until we find
        // one. If we can't find one, we fall back to using the GUID of the
        // null principal as the base domain.
        if (document->GetSandboxFlags() & SANDBOXED_ORIGIN) {
          nsCOMPtr<Document> tmpDoc = document;
          do {
            tmpDoc = tmpDoc->GetInProcessParentDocument();
          } while (tmpDoc && tmpDoc->GetSandboxFlags() & SANDBOXED_ORIGIN);

          if (tmpDoc) {
            // There was an unsandboxed ancestor, yay!
            nsCOMPtr<nsIPrincipal> tmpPrincipal = tmpDoc->NodePrincipal();
            rv = tmpPrincipal->GetBaseDomain(loadInfo.mDomain);
            NS_ENSURE_SUCCESS(rv, rv);
          } else {
            // No unsandboxed ancestor, use our GUID.
            rv = loadInfo.mLoadingPrincipal->GetBaseDomain(loadInfo.mDomain);
            NS_ENSURE_SUCCESS(rv, rv);
          }
        } else {
          // Document creating the worker is not sandboxed.
          rv = loadInfo.mLoadingPrincipal->GetBaseDomain(loadInfo.mDomain);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }

      NS_ENSURE_TRUE(NS_LoadGroupMatchesPrincipal(loadInfo.mLoadGroup,
                                                  loadInfo.mLoadingPrincipal),
                     NS_ERROR_FAILURE);

      nsCOMPtr<nsIPermissionManager> permMgr =
          do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      uint32_t perm;
      rv = permMgr->TestPermissionFromPrincipal(loadInfo.mLoadingPrincipal,
                                                "systemXHR"_ns, &perm);
      NS_ENSURE_SUCCESS(rv, rv);

      loadInfo.mXHRParamsAllowed = perm == nsIPermissionManager::ALLOW_ACTION;

      loadInfo.mWatchedByDevTools =
          browsingContext && browsingContext->WatchedByDevTools();

      loadInfo.mReferrerInfo =
          ReferrerInfo::CreateForFetch(loadInfo.mLoadingPrincipal, document);
      loadInfo.mFromWindow = true;
      loadInfo.mWindowID = globalWindow->WindowID();
      loadInfo.mStorageAccess = StorageAllowedForWindow(globalWindow);
      loadInfo.mUseRegularPrincipal = document->UseRegularPrincipal();
      loadInfo.mHasStorageAccessPermissionGranted =
          document->HasStorageAccessPermissionGranted();

      // This is an hack to deny the storage-access-permission for workers of
      // sub-iframes.
      if (loadInfo.mHasStorageAccessPermissionGranted &&
          StorageAllowedForDocument(document) != StorageAccess::eAllow) {
        loadInfo.mHasStorageAccessPermissionGranted = false;
      }
      loadInfo.mCookieJarSettings = document->CookieJarSettings();
      StoragePrincipalHelper::GetRegularPrincipalOriginAttributes(
          document, loadInfo.mOriginAttributes);
      loadInfo.mParentController = globalWindow->GetController();
      loadInfo.mSecureContext = loadInfo.mWindow->IsSecureContext()
                                    ? WorkerLoadInfo::eSecureContext
                                    : WorkerLoadInfo::eInsecureContext;
    } else {
      // Not a window
      MOZ_ASSERT(isChrome);

      // We're being created outside of a window. Need to figure out the script
      // that is creating us in order for us to use relative URIs later on.
      JS::AutoFilename fileName;
      if (JS::DescribeScriptedCaller(aCx, &fileName)) {
        // In most cases, fileName is URI. In a few other cases
        // (e.g. xpcshell), fileName is a file path. Ideally, we would
        // prefer testing whether fileName parses as an URI and fallback
        // to file path in case of error, but Windows file paths have
        // the interesting property that they can be parsed as bogus
        // URIs (e.g. C:/Windows/Tmp is interpreted as scheme "C",
        // hostname "Windows", path "Tmp"), which defeats this algorithm.
        // Therefore, we adopt the opposite convention.
        nsCOMPtr<nsIFile> scriptFile =
            do_CreateInstance("@mozilla.org/file/local;1", &rv);
        if (NS_FAILED(rv)) {
          return rv;
        }

        rv = scriptFile->InitWithPath(NS_ConvertUTF8toUTF16(fileName.get()));
        if (NS_SUCCEEDED(rv)) {
          rv = NS_NewFileURI(getter_AddRefs(loadInfo.mBaseURI), scriptFile);
        }
        if (NS_FAILED(rv)) {
          // As expected, fileName is not a path, so proceed with
          // a uri.
          rv = NS_NewURI(getter_AddRefs(loadInfo.mBaseURI), fileName.get());
        }
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
      loadInfo.mXHRParamsAllowed = true;
      loadInfo.mFromWindow = false;
      loadInfo.mWindowID = UINT64_MAX;
      loadInfo.mStorageAccess = StorageAccess::eAllow;
      loadInfo.mUseRegularPrincipal = true;
      loadInfo.mHasStorageAccessPermissionGranted = false;
      loadInfo.mCookieJarSettings = mozilla::net::CookieJarSettings::Create();
      MOZ_ASSERT(loadInfo.mCookieJarSettings);

      loadInfo.mOriginAttributes = OriginAttributes();
    }

    MOZ_ASSERT(loadInfo.mLoadingPrincipal);
    MOZ_ASSERT(isChrome || !loadInfo.mDomain.IsEmpty());

    if (!loadInfo.mLoadGroup || aLoadGroupBehavior == OverrideLoadGroup) {
      OverrideLoadInfoLoadGroup(loadInfo, loadInfo.mLoadingPrincipal);
    }
    MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(loadInfo.mLoadGroup,
                                            loadInfo.mLoadingPrincipal));

    // Top level workers' main script use the document charset for the script
    // uri encoding.
    nsCOMPtr<nsIURI> url;
    rv = nsContentUtils::NewURIWithDocumentCharset(
        getter_AddRefs(url), aScriptURL, document, loadInfo.mBaseURI);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SYNTAX_ERR);

    rv = ChannelFromScriptURLMainThread(
        loadInfo.mLoadingPrincipal, document, loadInfo.mLoadGroup, url,
        clientInfo, ContentPolicyType(aWorkerType), loadInfo.mCookieJarSettings,
        loadInfo.mReferrerInfo, getter_AddRefs(loadInfo.mChannel));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_GetFinalChannelURI(loadInfo.mChannel,
                               getter_AddRefs(loadInfo.mResolvedScriptURI));
    NS_ENSURE_SUCCESS(rv, rv);

    // We need the correct hasStoragePermission flag for the channel here since
    // we will do a content blocking check later when we set the storage
    // principal for the worker. The channel here won't be opened when we do the
    // check later, so the hasStoragePermission flag is incorrect. To address
    // this, We copy the hasStoragePermission flag from the document if there is
    // a window. The worker is created as the same origin of the window. So, the
    // worker is supposed to have the same storage permission as the window as
    // well as the hasStoragePermission flag.
    nsCOMPtr<nsILoadInfo> channelLoadInfo = loadInfo.mChannel->LoadInfo();
    rv = channelLoadInfo->SetHasStoragePermission(
        loadInfo.mHasStorageAccessPermissionGranted);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = loadInfo.SetPrincipalsAndCSPFromChannel(loadInfo.mChannel);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  MOZ_DIAGNOSTIC_ASSERT(loadInfo.mLoadingPrincipal);
  MOZ_DIAGNOSTIC_ASSERT(loadInfo.PrincipalIsValid());

  *aLoadInfo = std::move(loadInfo);
  return NS_OK;
}

// static
void WorkerPrivate::OverrideLoadInfoLoadGroup(WorkerLoadInfo& aLoadInfo,
                                              nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(!aLoadInfo.mInterfaceRequestor);
  MOZ_ASSERT(aLoadInfo.mLoadingPrincipal == aPrincipal);

  aLoadInfo.mInterfaceRequestor =
      new WorkerLoadInfo::InterfaceRequestor(aPrincipal, aLoadInfo.mLoadGroup);
  aLoadInfo.mInterfaceRequestor->MaybeAddBrowserChild(aLoadInfo.mLoadGroup);

  // NOTE: this defaults the load context to:
  //  - private browsing = false
  //  - content = true
  //  - use remote tabs = false
  nsCOMPtr<nsILoadGroup> loadGroup = do_CreateInstance(NS_LOADGROUP_CONTRACTID);

  nsresult rv =
      loadGroup->SetNotificationCallbacks(aLoadInfo.mInterfaceRequestor);
  MOZ_ALWAYS_SUCCEEDS(rv);

  aLoadInfo.mLoadGroup = std::move(loadGroup);

  MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(aLoadInfo.mLoadGroup, aPrincipal));
}

void WorkerPrivate::RunLoopNeverRan() {
  {
    MutexAutoLock lock(mMutex);

    mStatus = Dead;
  }

  // After mStatus is set to Dead there can be no more
  // WorkerControlRunnables so no need to lock here.
  if (!mControlQueue.IsEmpty()) {
    WorkerControlRunnable* runnable = nullptr;
    while (mControlQueue.Pop(runnable)) {
      runnable->Cancel();
      runnable->Release();
    }
  }

  ScheduleDeletion(WorkerPrivate::WorkerRan);
}

void WorkerPrivate::DoRunLoop(JSContext* aCx) {
  auto data = mWorkerThreadAccessible.Access();
  MOZ_ASSERT(mThread);

  MOZ_RELEASE_ASSERT(!GetExecutionManager());

  {
    MutexAutoLock lock(mMutex);
    mJSContext = aCx;

    MOZ_ASSERT(mStatus == Pending);
    mStatus = Running;
  }

  // Now that we've done that, we can go ahead and set up our AutoJSAPI.  We
  // can't before this point, because it can't find the right JSContext before
  // then, since it gets it from our mJSContext.
  AutoJSAPI jsapi;
  jsapi.Init();
  MOZ_ASSERT(jsapi.cx() == aCx);

  EnableMemoryReporter();

  InitializeGCTimers();

  for (;;) {
    WorkerStatus currentStatus;
    bool debuggerRunnablesPending = false;
    bool normalRunnablesPending = false;

    {
      MutexAutoLock lock(mMutex);

      // Wait for a runnable to arrive that we can execute, or for it to be okay
      // to shutdown this worker once all holders have been removed.
      // Holders may be removed from inside normal runnables, but we don't check
      // for that after processing normal runnables, so we need to let control
      // flow to the shutdown logic without blocking.
      while (mControlQueue.IsEmpty() &&
             !(debuggerRunnablesPending = !mDebuggerQueue.IsEmpty()) &&
             !(normalRunnablesPending = NS_HasPendingEvents(mThread)) &&
             !(mStatus != Running && !HasActiveWorkerRefs())) {
        // We pop out to this loop when there are no pending events.
        // If we don't reset these, we may not re-enter ProcessNextEvent()
        // until we have events to process, and it may seem like we have
        // an event running for a very long time.
        mThread->SetRunningEventDelay(TimeDuration(), TimeStamp());

        WaitForWorkerEvents();
      }

      auto result = ProcessAllControlRunnablesLocked();
      if (result != ProcessAllControlRunnablesResult::Nothing) {
        // NB: There's no JS on the stack here, so Abort vs MayContinue is
        // irrelevant

        // The state of the world may have changed, recheck it.
        normalRunnablesPending = NS_HasPendingEvents(mThread);
        // The debugger queue doesn't get cleared, so we can ignore that.
      }

      currentStatus = mStatus;
    }

    // if all holders are done then we can kill this thread.
    if (currentStatus != Running && !HasActiveWorkerRefs()) {
      // Now we are ready to kill the worker thread.
      if (currentStatus == Canceling) {
        NotifyInternal(Killing);

#ifdef DEBUG
        {
          MutexAutoLock lock(mMutex);
          currentStatus = mStatus;
        }
        MOZ_ASSERT(currentStatus == Killing);
#else
        currentStatus = Killing;
#endif
      }

      // If we're supposed to die then we should exit the loop.
      if (currentStatus == Killing) {
        // We are about to destroy worker, report all use counters.
        ReportUseCounters();

        // Flush uncaught rejections immediately, without
        // waiting for a next tick.
        PromiseDebugging::FlushUncaughtRejections();

        ShutdownGCTimers();

        DisableMemoryReporter();

        {
          MutexAutoLock lock(mMutex);

          mStatus = Dead;
          mJSContext = nullptr;
        }

        // After mStatus is set to Dead there can be no more
        // WorkerControlRunnables so no need to lock here.
        if (!mControlQueue.IsEmpty()) {
          WorkerControlRunnable* runnable = nullptr;
          while (mControlQueue.Pop(runnable)) {
            runnable->Cancel();
            runnable->Release();
          }
        }

        // Unroot the globals
        data->mScope = nullptr;
        data->mDebuggerScope = nullptr;

        return;
      }
    }

    if (debuggerRunnablesPending || normalRunnablesPending) {
      // Start the periodic GC timer if it is not already running.
      SetGCTimerMode(PeriodicTimer);
    }

    if (debuggerRunnablesPending) {
      WorkerRunnable* runnable = nullptr;

      {
        MutexAutoLock lock(mMutex);

        mDebuggerQueue.Pop(runnable);
        debuggerRunnablesPending = !mDebuggerQueue.IsEmpty();
      }

      MOZ_ASSERT(runnable);
      static_cast<nsIRunnable*>(runnable)->Run();
      runnable->Release();

      CycleCollectedJSContext* ccjs = CycleCollectedJSContext::Get();
      ccjs->PerformDebuggerMicroTaskCheckpoint();

      if (debuggerRunnablesPending) {
        WorkerDebuggerGlobalScope* globalScope = DebuggerGlobalScope();
        MOZ_ASSERT(globalScope);

        // Now *might* be a good time to GC. Let the JS engine make the
        // decision.
        JSAutoRealm ar(aCx, globalScope->GetGlobalJSObject());
        JS_MaybeGC(aCx);
      }
    } else if (normalRunnablesPending) {
      // Process a single runnable from the main queue.
      NS_ProcessNextEvent(mThread, false);

      normalRunnablesPending = NS_HasPendingEvents(mThread);
      if (normalRunnablesPending && GlobalScope()) {
        // Now *might* be a good time to GC. Let the JS engine make the
        // decision.
        JSAutoRealm ar(aCx, GlobalScope()->GetGlobalJSObject());
        JS_MaybeGC(aCx);
      }
    }

    if (!debuggerRunnablesPending && !normalRunnablesPending) {
      // Both the debugger event queue and the normal event queue has been
      // exhausted, cancel the periodic GC timer and schedule the idle GC timer.
      SetGCTimerMode(IdleTimer);
    }

    // If the worker thread is spamming the main thread faster than it can
    // process the work, then pause the worker thread until the main thread
    // catches up.
    size_t queuedEvents = mMainThreadEventTargetForMessaging->Length() +
                          mMainThreadDebuggeeEventTarget->Length();
    if (queuedEvents > 5000) {
      // Note, postMessage uses mMainThreadDebuggeeEventTarget!
      mMainThreadDebuggeeEventTarget->AwaitIdle();
    }
  }

  MOZ_CRASH("Shouldn't get here!");
}

namespace {
/**
 * If there is a current CycleCollectedJSContext, return its recursion depth,
 * otherwise return 1.
 *
 * In the edge case where a worker is starting up so late that PBackground is
 * already shutting down, the cycle collected context will never be created,
 * but we will need to drain the event loop in ClearMainEventQueue.  This will
 * result in a normal NS_ProcessPendingEvents invocation which will call
 * WorkerPrivate::OnProcessNextEvent and WorkerPrivate::AfterProcessNextEvent
 * which want to handle the need to process control runnables and perform a
 * sanity check assertion, respectively.
 *
 * We claim a depth of 1 when there's no CCJS because this most corresponds to
 * reality, but this doesn't meant that other code might want to drain various
 * runnable queues as part of this cleanup.
 */
uint32_t GetEffectiveEventLoopRecursionDepth() {
  auto ccjs = CycleCollectedJSContext::Get();
  if (ccjs) {
    return ccjs->RecursionDepth();
  }

  return 1;
}

}  // namespace

void WorkerPrivate::OnProcessNextEvent() {
  AssertIsOnWorkerThread();

  uint32_t recursionDepth = GetEffectiveEventLoopRecursionDepth();
  MOZ_ASSERT(recursionDepth);

  // Normally we process control runnables in DoRunLoop or RunCurrentSyncLoop.
  // However, it's possible that non-worker C++ could spin its own nested event
  // loop, and in that case we must ensure that we continue to process control
  // runnables here.
  if (recursionDepth > 1 && mSyncLoopStack.Length() < recursionDepth - 1) {
    Unused << ProcessAllControlRunnables();
    // There's no running JS, and no state to revalidate, so we can ignore the
    // return value.
  }
}

void WorkerPrivate::AfterProcessNextEvent() {
  AssertIsOnWorkerThread();
  MOZ_ASSERT(GetEffectiveEventLoopRecursionDepth());
}

nsIEventTarget* WorkerPrivate::MainThreadEventTargetForMessaging() {
  return mMainThreadEventTargetForMessaging;
}

nsresult WorkerPrivate::DispatchToMainThreadForMessaging(nsIRunnable* aRunnable,
                                                         uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> r = aRunnable;
  return DispatchToMainThreadForMessaging(r.forget(), aFlags);
}

nsresult WorkerPrivate::DispatchToMainThreadForMessaging(
    already_AddRefed<nsIRunnable> aRunnable, uint32_t aFlags) {
  return mMainThreadEventTargetForMessaging->Dispatch(std::move(aRunnable),
                                                      aFlags);
}

nsIEventTarget* WorkerPrivate::MainThreadEventTarget() {
  return mMainThreadEventTarget;
}

nsresult WorkerPrivate::DispatchToMainThread(nsIRunnable* aRunnable,
                                             uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> r = aRunnable;
  return DispatchToMainThread(r.forget(), aFlags);
}

nsresult WorkerPrivate::DispatchToMainThread(
    already_AddRefed<nsIRunnable> aRunnable, uint32_t aFlags) {
  return mMainThreadEventTarget->Dispatch(std::move(aRunnable), aFlags);
}

nsresult WorkerPrivate::DispatchDebuggeeToMainThread(
    already_AddRefed<WorkerDebuggeeRunnable> aRunnable, uint32_t aFlags) {
  return mMainThreadDebuggeeEventTarget->Dispatch(std::move(aRunnable), aFlags);
}

nsISerialEventTarget* WorkerPrivate::ControlEventTarget() {
  return mWorkerControlEventTarget;
}

nsISerialEventTarget* WorkerPrivate::HybridEventTarget() {
  return mWorkerHybridEventTarget;
}

ClientType WorkerPrivate::GetClientType() const {
  switch (Type()) {
    case WorkerTypeDedicated:
      return ClientType::Worker;
    case WorkerTypeShared:
      return ClientType::Sharedworker;
    case WorkerTypeService:
      return ClientType::Serviceworker;
    default:
      MOZ_CRASH("unknown worker type!");
  }
}

UniquePtr<ClientSource> WorkerPrivate::CreateClientSource() {
  auto data = mWorkerThreadAccessible.Access();
  MOZ_ASSERT(!data->mScope, "Client should be created before the global");

  auto clientSource = ClientManager::CreateSource(
      GetClientType(), mWorkerHybridEventTarget, GetPrincipalInfo());
  MOZ_DIAGNOSTIC_ASSERT(clientSource);

  clientSource->SetAgentClusterId(mAgentClusterId);

  if (data->mFrozen) {
    clientSource->Freeze();
  }

  // Shortly after the client is reserved we will try loading the main script
  // for the worker.  This may get intercepted by the ServiceWorkerManager
  // which will then try to create a ClientHandle.  Its actually possible for
  // the main thread to create this ClientHandle before our IPC message creating
  // the ClientSource completes.  To avoid this race we synchronously ping our
  // parent Client actor here.  This ensure the worker ClientSource is created
  // in the parent before the main thread might try reaching it with a
  // ClientHandle.
  //
  // An alternative solution would have been to handle the out-of-order
  // operations on the parent side.  We could have created a small window where
  // we allow ClientHandle objects to exist without a ClientSource.  We would
  // then time out these handles if they stayed orphaned for too long.  This
  // approach would be much more complex, but also avoid this extra bit of
  // latency when starting workers.
  //
  // Note, we only have to do this for workers that can be controlled by a
  // service worker.  So avoid the sync overhead here if we are starting a
  // service worker or a chrome worker.
  if (Type() != WorkerTypeService && !IsChromeWorker()) {
    clientSource->WorkerSyncPing(this);
  }

  return clientSource;
}

bool WorkerPrivate::EnsureCSPEventListener() {
  if (!mCSPEventListener) {
    mCSPEventListener = WorkerCSPEventListener::Create(this);
    if (NS_WARN_IF(!mCSPEventListener)) {
      return false;
    }
  }
  return true;
}

nsICSPEventListener* WorkerPrivate::CSPEventListener() const {
  MOZ_ASSERT(mCSPEventListener);
  return mCSPEventListener;
}

void WorkerPrivate::EnsurePerformanceStorage() {
  AssertIsOnWorkerThread();

  if (!mPerformanceStorage) {
    mPerformanceStorage = PerformanceStorageWorker::Create(this);
  }
}

bool WorkerPrivate::GetExecutionGranted() const {
  auto data = mWorkerThreadAccessible.Access();
  return data->mJSThreadExecutionGranted;
}

void WorkerPrivate::SetExecutionGranted(bool aGranted) {
  auto data = mWorkerThreadAccessible.Access();
  data->mJSThreadExecutionGranted = aGranted;
}

void WorkerPrivate::ScheduleTimeSliceExpiration(uint32_t aDelay) {
  auto data = mWorkerThreadAccessible.Access();

  if (!data->mTSTimer) {
    data->mTSTimer = NS_NewTimer();
    MOZ_ALWAYS_SUCCEEDS(data->mTSTimer->SetTarget(mWorkerControlEventTarget));
  }

  // Whenever an event is scheduled on the WorkerControlEventTarget an
  // interrupt is automatically requested which causes us to yield JS execution
  // and the next JS execution in the queue to execute.
  // This allows for simple code reuse of the existing interrupt callback code
  // used for control events.
  MOZ_ALWAYS_SUCCEEDS(data->mTSTimer->InitWithNamedFuncCallback(
      [](nsITimer* Timer, void* aClosure) { return; }, nullptr, aDelay,
      nsITimer::TYPE_ONE_SHOT, "TimeSliceExpirationTimer"));
}

void WorkerPrivate::CancelTimeSliceExpiration() {
  auto data = mWorkerThreadAccessible.Access();
  MOZ_ALWAYS_SUCCEEDS(data->mTSTimer->Cancel());
}

JSExecutionManager* WorkerPrivate::GetExecutionManager() const {
  auto data = mWorkerThreadAccessible.Access();
  return data->mExecutionManager.get();
}

void WorkerPrivate::SetExecutionManager(JSExecutionManager* aManager) {
  auto data = mWorkerThreadAccessible.Access();
  data->mExecutionManager = aManager;
}

void WorkerPrivate::ExecutionReady() {
  auto data = mWorkerThreadAccessible.Access();
  {
    MutexAutoLock lock(mMutex);
    if (mStatus >= Canceling) {
      return;
    }
  }

  data->mScope->MutableClientSourceRef().WorkerExecutionReady(this);
}

void WorkerPrivate::InitializeGCTimers() {
  auto data = mWorkerThreadAccessible.Access();

  // We need a timer for GC. The basic plan is to run a non-shrinking GC
  // periodically (PERIODIC_GC_TIMER_DELAY_SEC) while the worker is running.
  // Once the worker goes idle we set a short (IDLE_GC_TIMER_DELAY_SEC) timer to
  // run a shrinking GC. If the worker receives more messages then the short
  // timer is canceled and the periodic timer resumes.
  data->mGCTimer = NS_NewTimer();
  MOZ_ASSERT(data->mGCTimer);

  data->mPeriodicGCTimerRunning = false;
  data->mIdleGCTimerRunning = false;
}

void WorkerPrivate::SetGCTimerMode(GCTimerMode aMode) {
  auto data = mWorkerThreadAccessible.Access();
  MOZ_ASSERT(data->mGCTimer);

  if ((aMode == PeriodicTimer && data->mPeriodicGCTimerRunning) ||
      (aMode == IdleTimer && data->mIdleGCTimerRunning)) {
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(data->mGCTimer->Cancel());

  data->mPeriodicGCTimerRunning = false;
  data->mIdleGCTimerRunning = false;
  LOG(WorkerLog(), ("Worker %p canceled GC timer because %s\n", this,
                    aMode == PeriodicTimer ? "periodic"
                    : aMode == IdleTimer   ? "idle"
                                           : "none"));

  if (aMode == NoTimer) {
    return;
  }

  MOZ_ASSERT(aMode == PeriodicTimer || aMode == IdleTimer);

  uint32_t delay = 0;
  int16_t type = nsITimer::TYPE_ONE_SHOT;
  nsTimerCallbackFunc callback = nullptr;
  const char* name = nullptr;

  if (aMode == PeriodicTimer) {
    delay = PERIODIC_GC_TIMER_DELAY_SEC * 1000;
    type = nsITimer::TYPE_REPEATING_SLACK;
    callback = PeriodicGCTimerCallback;
    name = "dom::PeriodicGCTimerCallback";
  } else {
    delay = IDLE_GC_TIMER_DELAY_SEC * 1000;
    type = nsITimer::TYPE_ONE_SHOT;
    callback = IdleGCTimerCallback;
    name = "dom::IdleGCTimerCallback";
  }

  MOZ_ALWAYS_SUCCEEDS(data->mGCTimer->SetTarget(mWorkerControlEventTarget));
  MOZ_ALWAYS_SUCCEEDS(data->mGCTimer->InitWithNamedFuncCallback(
      callback, this, delay, type, name));

  if (aMode == PeriodicTimer) {
    LOG(WorkerLog(), ("Worker %p scheduled periodic GC timer\n", this));
    data->mPeriodicGCTimerRunning = true;
  } else {
    LOG(WorkerLog(), ("Worker %p scheduled idle GC timer\n", this));
    data->mIdleGCTimerRunning = true;
  }
}

void WorkerPrivate::ShutdownGCTimers() {
  auto data = mWorkerThreadAccessible.Access();

  MOZ_ASSERT(data->mGCTimer);

  // Always make sure the timer is canceled.
  MOZ_ALWAYS_SUCCEEDS(data->mGCTimer->Cancel());

  LOG(WorkerLog(), ("Worker %p killed the GC timer\n", this));

  data->mGCTimer = nullptr;
  data->mPeriodicGCTimerRunning = false;
  data->mIdleGCTimerRunning = false;
}

bool WorkerPrivate::InterruptCallback(JSContext* aCx) {
  auto data = mWorkerThreadAccessible.Access();

  AutoYieldJSThreadExecution yield;

  // If we are here it's because a WorkerControlRunnable has been dispatched.
  // The runnable could be processed here or it could have already been
  // processed by a sync event loop.
  // The most important thing this method must do, is to decide if the JS
  // execution should continue or not. If the runnable returns an error or if
  // the worker status is >= Canceling, we should stop the JS execution.

  MOZ_ASSERT(!JS_IsExceptionPending(aCx));

  bool mayContinue = true;
  bool scheduledIdleGC = false;

  for (;;) {
    // Run all control events now.
    auto result = ProcessAllControlRunnables();
    if (result == ProcessAllControlRunnablesResult::Abort) {
      mayContinue = false;
    }

    bool mayFreeze = data->mFrozen;

    {
      MutexAutoLock lock(mMutex);

      if (mayFreeze) {
        mayFreeze = mStatus <= Running;
      }

      if (mStatus >= Canceling) {
        mayContinue = false;
      }
    }

    if (!mayContinue || !mayFreeze) {
      break;
    }

    // Cancel the periodic GC timer here before freezing. The idle GC timer
    // will clean everything up once it runs.
    if (!scheduledIdleGC) {
      SetGCTimerMode(IdleTimer);
      scheduledIdleGC = true;
    }

    while ((mayContinue = MayContinueRunning())) {
      MutexAutoLock lock(mMutex);
      if (!mControlQueue.IsEmpty()) {
        break;
      }

      WaitForWorkerEvents();
    }
  }

  if (!mayContinue) {
    // We want only uncatchable exceptions here.
    NS_ASSERTION(!JS_IsExceptionPending(aCx),
                 "Should not have an exception set here!");
    return false;
  }

  // Make sure the periodic timer gets turned back on here.
  SetGCTimerMode(PeriodicTimer);

  return true;
}

void WorkerPrivate::CloseInternal() {
  AssertIsOnWorkerThread();
  NotifyInternal(Closing);
}

bool WorkerPrivate::IsOnCurrentThread() {
  // May be called on any thread!

  MOZ_ASSERT(mPRThread);
  return PR_GetCurrentThread() == mPRThread;
}

void WorkerPrivate::ScheduleDeletion(WorkerRanOrNot aRanOrNot) {
  AssertIsOnWorkerThread();
  {
    // mWorkerThreadAccessible's accessor must be destructed before
    // the scheduled Runnable gets to run.
    auto data = mWorkerThreadAccessible.Access();
    MOZ_ASSERT(data->mChildWorkers.IsEmpty());

    MOZ_RELEASE_ASSERT(!data->mDeletionScheduled);
    data->mDeletionScheduled.Flip();
  }
  MOZ_ASSERT(mSyncLoopStack.IsEmpty());
  MOZ_ASSERT(mPostSyncLoopOperations == 0);

  ClearMainEventQueue(aRanOrNot);
#ifdef DEBUG
  if (WorkerRan == aRanOrNot) {
    nsIThread* currentThread = NS_GetCurrentThread();
    MOZ_ASSERT(currentThread);
    MOZ_ASSERT(!NS_HasPendingEvents(currentThread));
  }
#endif

  if (WorkerPrivate* parent = GetParent()) {
    RefPtr<WorkerFinishedRunnable> runnable =
        new WorkerFinishedRunnable(parent, this);
    if (!runnable->Dispatch()) {
      NS_WARNING("Failed to dispatch runnable!");
    }
  } else {
    // Note, this uses the lower priority DispatchToMainThreadForMessaging for
    // dispatching TopLevelWorkerFinishedRunnable to the main thread so that
    // other relevant runnables are guaranteed to run before it.
    RefPtr<TopLevelWorkerFinishedRunnable> runnable =
        new TopLevelWorkerFinishedRunnable(this);
    if (NS_FAILED(DispatchToMainThreadForMessaging(runnable.forget()))) {
      NS_WARNING("Failed to dispatch runnable!");
    }
  }
}

bool WorkerPrivate::CollectRuntimeStats(JS::RuntimeStats* aRtStats,
                                        bool aAnonymize) {
  AssertIsOnWorkerThread();
  NS_ASSERTION(aRtStats, "Null RuntimeStats!");
  NS_ASSERTION(mJSContext, "This must never be null!");

  return JS::CollectRuntimeStats(mJSContext, aRtStats, nullptr, aAnonymize);
}

void WorkerPrivate::EnableMemoryReporter() {
  auto data = mWorkerThreadAccessible.Access();
  MOZ_ASSERT(!data->mMemoryReporter);

  // No need to lock here since the main thread can't race until we've
  // successfully registered the reporter.
  data->mMemoryReporter = new MemoryReporter(this);

  if (NS_FAILED(RegisterWeakAsyncMemoryReporter(data->mMemoryReporter))) {
    NS_WARNING("Failed to register memory reporter!");
    // No need to lock here since a failed registration means our memory
    // reporter can't start running. Just clean up.
    data->mMemoryReporter = nullptr;
  }
}

void WorkerPrivate::DisableMemoryReporter() {
  auto data = mWorkerThreadAccessible.Access();

  RefPtr<MemoryReporter> memoryReporter;
  {
    // Mutex protectes MemoryReporter::mWorkerPrivate which is cleared by
    // MemoryReporter::Disable() below.
    MutexAutoLock lock(mMutex);

    // There is nothing to do here if the memory reporter was never successfully
    // registered.
    if (!data->mMemoryReporter) {
      return;
    }

    // We don't need this set any longer. Swap it out so that we can unregister
    // below.
    data->mMemoryReporter.swap(memoryReporter);

    // Next disable the memory reporter so that the main thread stops trying to
    // signal us.
    memoryReporter->Disable();
  }

  // Finally unregister the memory reporter.
  if (NS_FAILED(UnregisterWeakMemoryReporter(memoryReporter))) {
    NS_WARNING("Failed to unregister memory reporter!");
  }
}

void WorkerPrivate::WaitForWorkerEvents() {
  AUTO_PROFILER_LABEL("WorkerPrivate::WaitForWorkerEvents", IDLE);

  AssertIsOnWorkerThread();
  mMutex.AssertCurrentThreadOwns();

  // Wait for a worker event.
  mCondVar.Wait();
}

WorkerPrivate::ProcessAllControlRunnablesResult
WorkerPrivate::ProcessAllControlRunnablesLocked() {
  AssertIsOnWorkerThread();
  mMutex.AssertCurrentThreadOwns();

  AutoYieldJSThreadExecution yield;

  auto result = ProcessAllControlRunnablesResult::Nothing;

  for (;;) {
    WorkerControlRunnable* event;
    if (!mControlQueue.Pop(event)) {
      break;
    }

    MutexAutoUnlock unlock(mMutex);

    MOZ_ASSERT(event);
    if (NS_FAILED(static_cast<nsIRunnable*>(event)->Run())) {
      result = ProcessAllControlRunnablesResult::Abort;
    }

    if (result == ProcessAllControlRunnablesResult::Nothing) {
      // We ran at least one thing.
      result = ProcessAllControlRunnablesResult::MayContinue;
    }
    event->Release();
  }

  return result;
}

void WorkerPrivate::ClearMainEventQueue(WorkerRanOrNot aRanOrNot) {
  AssertIsOnWorkerThread();

  MOZ_ASSERT((mPostSyncLoopOperations & ePendingEventQueueClearing)
                 ? (mSyncLoopStack.Length() == 1)
                 : mSyncLoopStack.IsEmpty());
  MOZ_ASSERT(!mCancelAllPendingRunnables);

  mCancelAllPendingRunnables = true;
  WorkerGlobalScope* globalScope = GlobalScope();
  if (globalScope) {
    // It's appropriate to disconnect event targets at the point that it's no
    // longer possible for new tasks to be dispatched at the global, and this is
    // that point.
    globalScope->DisconnectEventTargetObjects();

    globalScope->WorkerPrivateSaysForbidScript();
  }

  if (WorkerNeverRan == aRanOrNot) {
    for (uint32_t count = mPreStartRunnables.Length(), index = 0; index < count;
         index++) {
      RefPtr<WorkerRunnable> runnable = std::move(mPreStartRunnables[index]);
      static_cast<nsIRunnable*>(runnable.get())->Run();
    }
  } else {
    nsIThread* currentThread = NS_GetCurrentThread();
    MOZ_ASSERT(currentThread);

    NS_ProcessPendingEvents(currentThread);
  }

  if (globalScope) {
    globalScope->WorkerPrivateSaysAllowScript();
  }
  MOZ_ASSERT(mCancelAllPendingRunnables);
  mCancelAllPendingRunnables = false;
}

void WorkerPrivate::ClearDebuggerEventQueue() {
  while (!mDebuggerQueue.IsEmpty()) {
    WorkerRunnable* runnable = nullptr;
    mDebuggerQueue.Pop(runnable);
    // It should be ok to simply release the runnable, without running it.
    runnable->Release();
  }
}

bool WorkerPrivate::FreezeInternal() {
  auto data = mWorkerThreadAccessible.Access();
  NS_ASSERTION(!data->mFrozen, "Already frozen!");

  AutoYieldJSThreadExecution yield;

  // The worker can freeze even if it failed to run (and doesn't have a global).
  if (data->mScope) {
    data->mScope->MutableClientSourceRef().Freeze();
  }

  data->mFrozen = true;

  for (uint32_t index = 0; index < data->mChildWorkers.Length(); index++) {
    data->mChildWorkers[index]->Freeze(nullptr);
  }

  return true;
}

bool WorkerPrivate::ThawInternal() {
  auto data = mWorkerThreadAccessible.Access();
  NS_ASSERTION(data->mFrozen, "Not yet frozen!");

  for (uint32_t index = 0; index < data->mChildWorkers.Length(); index++) {
    data->mChildWorkers[index]->Thaw(nullptr);
  }

  data->mFrozen = false;

  // The worker can thaw even if it failed to run (and doesn't have a global).
  if (data->mScope) {
    data->mScope->MutableClientSourceRef().Thaw();
  }

  return true;
}

void WorkerPrivate::PropagateStorageAccessPermissionGrantedInternal() {
  auto data = mWorkerThreadAccessible.Access();

  mLoadInfo.mUseRegularPrincipal = true;
  mLoadInfo.mHasStorageAccessPermissionGranted = true;

  WorkerGlobalScope* globalScope = GlobalScope();
  if (globalScope) {
    globalScope->StorageAccessPermissionGranted();
  }

  for (uint32_t index = 0; index < data->mChildWorkers.Length(); index++) {
    data->mChildWorkers[index]->PropagateStorageAccessPermissionGranted();
  }
}

void WorkerPrivate::TraverseTimeouts(nsCycleCollectionTraversalCallback& cb) {
  auto data = mWorkerThreadAccessible.Access();
  for (uint32_t i = 0; i < data->mTimeouts.Length(); ++i) {
    // TODO(erahm): No idea what's going on here.
    TimeoutInfo* tmp = data->mTimeouts[i].get();
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHandler)
  }
}

void WorkerPrivate::UnlinkTimeouts() {
  auto data = mWorkerThreadAccessible.Access();
  data->mTimeouts.Clear();
}

bool WorkerPrivate::ModifyBusyCountFromWorker(bool aIncrease) {
  AssertIsOnWorkerThread();

  {
    MutexAutoLock lock(mMutex);

    // If we're in shutdown then the busy count is no longer being considered so
    // just return now.
    if (mStatus >= Killing) {
      return true;
    }
  }

  RefPtr<ModifyBusyCountRunnable> runnable =
      new ModifyBusyCountRunnable(this, aIncrease);
  return runnable->Dispatch();
}

bool WorkerPrivate::AddChildWorker(WorkerPrivate* aChildWorker) {
  auto data = mWorkerThreadAccessible.Access();

#ifdef DEBUG
  {
    WorkerStatus currentStatus;
    {
      MutexAutoLock lock(mMutex);
      currentStatus = mStatus;
    }

    MOZ_ASSERT(currentStatus == Running);
  }
#endif

  NS_ASSERTION(!data->mChildWorkers.Contains(aChildWorker),
               "Already know about this one!");
  data->mChildWorkers.AppendElement(aChildWorker);

  return data->mChildWorkers.Length() == 1 ? ModifyBusyCountFromWorker(true)
                                           : true;
}

void WorkerPrivate::RemoveChildWorker(WorkerPrivate* aChildWorker) {
  auto data = mWorkerThreadAccessible.Access();

  NS_ASSERTION(data->mChildWorkers.Contains(aChildWorker),
               "Didn't know about this one!");
  data->mChildWorkers.RemoveElement(aChildWorker);

  if (data->mChildWorkers.IsEmpty() && !ModifyBusyCountFromWorker(false)) {
    NS_WARNING("Failed to modify busy count!");
  }
}

bool WorkerPrivate::AddWorkerRef(WorkerRef* aWorkerRef,
                                 WorkerStatus aFailStatus) {
  MOZ_ASSERT(aWorkerRef);
  auto data = mWorkerThreadAccessible.Access();

  {
    MutexAutoLock lock(mMutex);

    if (mStatus >= aFailStatus) {
      return false;
    }

    // We shouldn't create strong references to workers before their main loop
    // begins running. Strong references must be disposed of on the worker
    // thread, so strong references from other threads use a control runnable
    // for that purpose. If the worker fails to reach the main loop stage then
    // no control runnables get run and it would be impossible to get rid of the
    // reference properly.
    MOZ_DIAGNOSTIC_ASSERT_IF(aWorkerRef->IsPreventingShutdown(),
                             mStatus >= WorkerStatus::Running);
  }

  MOZ_ASSERT(!data->mWorkerRefs.Contains(aWorkerRef),
             "Already know about this one!");

  if (aWorkerRef->IsPreventingShutdown()) {
    if (!data->mNumWorkerRefsPreventingShutdownStart &&
        !ModifyBusyCountFromWorker(true)) {
      return false;
    }
    data->mNumWorkerRefsPreventingShutdownStart += 1;
  }

  data->mWorkerRefs.AppendElement(aWorkerRef);
  return true;
}

void WorkerPrivate::RemoveWorkerRef(WorkerRef* aWorkerRef) {
  MOZ_ASSERT(aWorkerRef);
  auto data = mWorkerThreadAccessible.Access();

  MOZ_ASSERT(data->mWorkerRefs.Contains(aWorkerRef),
             "Didn't know about this one!");
  data->mWorkerRefs.RemoveElement(aWorkerRef);

  if (aWorkerRef->IsPreventingShutdown()) {
    data->mNumWorkerRefsPreventingShutdownStart -= 1;
    if (!data->mNumWorkerRefsPreventingShutdownStart &&
        !ModifyBusyCountFromWorker(false)) {
      NS_WARNING("Failed to modify busy count!");
    }
  }
}

void WorkerPrivate::NotifyWorkerRefs(WorkerStatus aStatus) {
  auto data = mWorkerThreadAccessible.Access();

  NS_ASSERTION(aStatus > Closing, "Bad status!");

  for (auto* workerRef : data->mWorkerRefs.ForwardRange()) {
    workerRef->Notify();
  }

  AutoTArray<WorkerPrivate*, 10> children;
  children.AppendElements(data->mChildWorkers);

  for (uint32_t index = 0; index < children.Length(); index++) {
    if (!children[index]->Notify(aStatus)) {
      NS_WARNING("Failed to notify child worker!");
    }
  }
}

void WorkerPrivate::CancelAllTimeouts() {
  auto data = mWorkerThreadAccessible.Access();

  LOG(TimeoutsLog(), ("Worker %p CancelAllTimeouts.\n", this));

  if (data->mTimerRunning) {
    NS_ASSERTION(data->mTimer && data->mTimerRunnable, "Huh?!");
    NS_ASSERTION(!data->mTimeouts.IsEmpty(), "Huh?!");

    if (NS_FAILED(data->mTimer->Cancel())) {
      NS_WARNING("Failed to cancel timer!");
    }

    for (uint32_t index = 0; index < data->mTimeouts.Length(); index++) {
      data->mTimeouts[index]->mCanceled = true;
    }

    // If mRunningExpiredTimeouts, then the fact that they are all canceled now
    // means that the currently executing RunExpiredTimeouts will deal with
    // them.  Otherwise, we need to clean them up ourselves.
    if (!data->mRunningExpiredTimeouts) {
      data->mTimeouts.Clear();
      ModifyBusyCountFromWorker(false);
    }

    // Set mTimerRunning false even if mRunningExpiredTimeouts is true, so that
    // if we get reentered under this same RunExpiredTimeouts call we don't
    // assert above that !mTimeouts().IsEmpty(), because that's clearly false
    // now.
    data->mTimerRunning = false;
  }
#ifdef DEBUG
  else if (!data->mRunningExpiredTimeouts) {
    NS_ASSERTION(data->mTimeouts.IsEmpty(), "Huh?!");
  }
#endif

  data->mTimer = nullptr;
  data->mTimerRunnable = nullptr;
}

already_AddRefed<nsIEventTarget> WorkerPrivate::CreateNewSyncLoop(
    WorkerStatus aFailStatus) {
  AssertIsOnWorkerThread();
  MOZ_ASSERT(
      aFailStatus >= Canceling,
      "Sync loops can be created when the worker is in Running/Closing state!");

  {
    MutexAutoLock lock(mMutex);

    if (mStatus >= aFailStatus) {
      return nullptr;
    }
  }

  auto queue = static_cast<ThreadEventQueue*>(mThread->EventQueue());
  nsCOMPtr<nsISerialEventTarget> realEventTarget = queue->PushEventQueue();
  MOZ_ASSERT(realEventTarget);

  RefPtr<EventTarget> workerEventTarget =
      new EventTarget(this, realEventTarget);

  {
    // Modifications must be protected by mMutex in DEBUG builds, see comment
    // about mSyncLoopStack in WorkerPrivate.h.
#ifdef DEBUG
    MutexAutoLock lock(mMutex);
#endif

    mSyncLoopStack.AppendElement(new SyncLoopInfo(workerEventTarget));
  }

  return workerEventTarget.forget();
}

bool WorkerPrivate::RunCurrentSyncLoop() {
  AssertIsOnWorkerThread();

  JSContext* cx = GetJSContext();
  MOZ_ASSERT(cx);

  AutoPushEventLoopGlobal eventLoopGlobal(this, cx);

  // This should not change between now and the time we finish running this sync
  // loop.
  uint32_t currentLoopIndex = mSyncLoopStack.Length() - 1;

  SyncLoopInfo* loopInfo = mSyncLoopStack[currentLoopIndex].get();

  AutoYieldJSThreadExecution yield;

  MOZ_ASSERT(loopInfo);
  MOZ_ASSERT(!loopInfo->mHasRun);
  MOZ_ASSERT(!loopInfo->mCompleted);

#ifdef DEBUG
  loopInfo->mHasRun = true;
#endif

  while (!loopInfo->mCompleted) {
    bool normalRunnablesPending = false;

    // Don't block with the periodic GC timer running.
    if (!NS_HasPendingEvents(mThread)) {
      SetGCTimerMode(IdleTimer);
    }

    // Wait for something to do.
    {
      MutexAutoLock lock(mMutex);

      for (;;) {
        while (mControlQueue.IsEmpty() && !normalRunnablesPending &&
               !(normalRunnablesPending = NS_HasPendingEvents(mThread))) {
          WaitForWorkerEvents();
        }

        auto result = ProcessAllControlRunnablesLocked();
        if (result != ProcessAllControlRunnablesResult::Nothing) {
          // The state of the world may have changed. Recheck it if we need to
          // continue.
          normalRunnablesPending =
              result == ProcessAllControlRunnablesResult::MayContinue &&
              NS_HasPendingEvents(mThread);

          // NB: If we processed a NotifyRunnable, we might have run
          // non-control runnables, one of which may have shut down the
          // sync loop.
          if (loopInfo->mCompleted) {
            break;
          }
        }

        // If we *didn't* run any control runnables, this should be unchanged.
        MOZ_ASSERT(!loopInfo->mCompleted);

        if (normalRunnablesPending) {
          break;
        }
      }
    }

    if (normalRunnablesPending) {
      // Make sure the periodic timer is running before we continue.
      SetGCTimerMode(PeriodicTimer);

      MOZ_ALWAYS_TRUE(NS_ProcessNextEvent(mThread, false));

      // Now *might* be a good time to GC. Let the JS engine make the decision.
      if (GetCurrentEventLoopGlobal()) {
        // If GetCurrentEventLoopGlobal() is non-null, our JSContext is in a
        // Realm, so it's safe to try to GC.
        MOZ_ASSERT(JS::CurrentGlobalOrNull(cx));
        JS_MaybeGC(cx);
      }
    }
  }

  // Make sure that the stack didn't change underneath us.
  MOZ_ASSERT(mSyncLoopStack[currentLoopIndex].get() == loopInfo);

  return DestroySyncLoop(currentLoopIndex);
}

bool WorkerPrivate::DestroySyncLoop(uint32_t aLoopIndex) {
  MOZ_ASSERT(!mSyncLoopStack.IsEmpty());
  MOZ_ASSERT(mSyncLoopStack.Length() - 1 == aLoopIndex);

  AutoYieldJSThreadExecution yield;

  // We're about to delete the loop, stash its event target and result.
  const auto& loopInfo = mSyncLoopStack[aLoopIndex];
  nsIEventTarget* nestedEventTarget =
      loopInfo->mEventTarget->GetWeakNestedEventTarget();
  MOZ_ASSERT(nestedEventTarget);

  bool result = loopInfo->mResult;

  auto queue = static_cast<ThreadEventQueue*>(mThread->EventQueue());
  queue->PopEventQueue(nestedEventTarget);

  // Are we making a 1 -> 0 transition here?
  if (mSyncLoopStack.Length() == 1) {
    if ((mPostSyncLoopOperations & ePendingEventQueueClearing)) {
      ClearMainEventQueue(WorkerRan);
    }

    if ((mPostSyncLoopOperations & eDispatchCancelingRunnable)) {
      DispatchCancelingRunnable();
    }

    mPostSyncLoopOperations = 0;
  }

  {
    // Modifications must be protected by mMutex in DEBUG builds, see comment
    // about mSyncLoopStack in WorkerPrivate.h.
#ifdef DEBUG
    MutexAutoLock lock(mMutex);
#endif

    // This will delete |loopInfo|!
    mSyncLoopStack.RemoveElementAt(aLoopIndex);
  }

  return result;
}

void WorkerPrivate::DispatchCancelingRunnable() {
  // Here we use a normal runnable to know when the current JS chunk of code
  // is finished. We cannot use a WorkerRunnable because they are not
  // accepted any more by the worker, and we do not want to use a
  // WorkerControlRunnable because they are immediately executed.
  RefPtr<CancelingRunnable> r = new CancelingRunnable();
  mThread->nsThread::Dispatch(r.forget(), NS_DISPATCH_NORMAL);

  // At the same time, we want to be sure that we interrupt infinite loops.
  // The following runnable starts a timer that cancel the worker, from the
  // parent thread, after CANCELING_TIMEOUT millseconds.
  RefPtr<CancelingWithTimeoutOnParentRunnable> rr =
      new CancelingWithTimeoutOnParentRunnable(this);
  rr->Dispatch();
}

void WorkerPrivate::ReportUseCounters() {
  AssertIsOnWorkerThread();

  static const bool kDebugUseCounters = false;

  if (mReportedUseCounters) {
    return;
  }
  mReportedUseCounters = true;

  if (Telemetry::HistogramUseCounterWorkerCount <= 0 || IsChromeWorker()) {
    return;
  }

  const size_t type = Type();
  switch (type) {
    case WorkerTypeDedicated:
      Telemetry::Accumulate(Telemetry::DEDICATED_WORKER_DESTROYED, 1);
      break;
    case WorkerTypeShared:
      Telemetry::Accumulate(Telemetry::SHARED_WORKER_DESTROYED, 1);
      break;
    case WorkerTypeService:
      Telemetry::Accumulate(Telemetry::SERVICE_WORKER_DESTROYED, 1);
      break;
    default:
      MOZ_ASSERT(false, "Unknown worker type");
      return;
  }

  if (kDebugUseCounters) {
    nsAutoCString path(Domain());
    path.AppendLiteral("(");
    NS_ConvertUTF16toUTF8 script(ScriptURL());
    path.Append(script);
    path.AppendPrintf(", 0x%p)", static_cast<void*>(this));
    printf("-- Worker use counters for %s --\n", path.get());
  }

  static_assert(
      static_cast<size_t>(UseCounterWorker::Count) * 3 ==
          static_cast<size_t>(Telemetry::HistogramUseCounterWorkerCount),
      "There should be three histograms (dedicated and shared and "
      "servie) for each worker use counter");
  const size_t count = static_cast<size_t>(UseCounterWorker::Count);
  const size_t factor =
      static_cast<size_t>(Telemetry::HistogramUseCounterWorkerCount) / count;
  MOZ_ASSERT(factor > type);

  for (size_t c = 0; c < count; ++c) {
    // Histograms for worker use counters use the same order as the worker types
    // , so we can use the worker type to index to corresponding histogram.
    Telemetry::HistogramID id = static_cast<Telemetry::HistogramID>(
        Telemetry::HistogramFirstUseCounterWorker + c * factor + type);
    MOZ_ASSERT(id <= Telemetry::HistogramLastUseCounterWorker);

    if (bool value = GetUseCounter(static_cast<UseCounterWorker>(c))) {
      Telemetry::Accumulate(id, 1);

      if (kDebugUseCounters) {
        const char* name = Telemetry::GetHistogramName(id);
        printf("  %s  #%d: %d\n", name, id, value);
      }
    }
  }
}

void WorkerPrivate::StopSyncLoop(nsIEventTarget* aSyncLoopTarget,
                                 bool aResult) {
  AssertIsOnWorkerThread();
  AssertValidSyncLoop(aSyncLoopTarget);

  MOZ_ASSERT(!mSyncLoopStack.IsEmpty());

  for (uint32_t index = mSyncLoopStack.Length(); index > 0; index--) {
    const auto& loopInfo = mSyncLoopStack[index - 1];
    MOZ_ASSERT(loopInfo);
    MOZ_ASSERT(loopInfo->mEventTarget);

    if (loopInfo->mEventTarget == aSyncLoopTarget) {
      // Can't assert |loop->mHasRun| here because dispatch failures can cause
      // us to bail out early.
      MOZ_ASSERT(!loopInfo->mCompleted);

      loopInfo->mResult = aResult;
      loopInfo->mCompleted = true;

      loopInfo->mEventTarget->Disable();

      return;
    }

    MOZ_ASSERT(!SameCOMIdentity(loopInfo->mEventTarget, aSyncLoopTarget));
  }

  MOZ_CRASH("Unknown sync loop!");
}

#ifdef DEBUG
void WorkerPrivate::AssertValidSyncLoop(nsIEventTarget* aSyncLoopTarget) {
  MOZ_ASSERT(aSyncLoopTarget);

  EventTarget* workerTarget;
  nsresult rv = aSyncLoopTarget->QueryInterface(
      kDEBUGWorkerEventTargetIID, reinterpret_cast<void**>(&workerTarget));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(workerTarget);

  bool valid = false;

  {
    MutexAutoLock lock(mMutex);

    for (uint32_t index = 0; index < mSyncLoopStack.Length(); index++) {
      const auto& loopInfo = mSyncLoopStack[index];
      MOZ_ASSERT(loopInfo);
      MOZ_ASSERT(loopInfo->mEventTarget);

      if (loopInfo->mEventTarget == aSyncLoopTarget) {
        valid = true;
        break;
      }

      MOZ_ASSERT(!SameCOMIdentity(loopInfo->mEventTarget, aSyncLoopTarget));
    }
  }

  MOZ_ASSERT(valid);
}
#endif

void WorkerPrivate::PostMessageToParent(
    JSContext* aCx, JS::Handle<JS::Value> aMessage,
    const Sequence<JSObject*>& aTransferable, ErrorResult& aRv) {
  AssertIsOnWorkerThread();
  MOZ_DIAGNOSTIC_ASSERT(IsDedicatedWorker());

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());

  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransferable,
                                                          &transferable);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  RefPtr<MessageEventRunnable> runnable = new MessageEventRunnable(
      this, WorkerRunnable::ParentThreadUnchangedBusyCount);

  UniquePtr<AbstractTimelineMarker> start;
  UniquePtr<AbstractTimelineMarker> end;
  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  bool isTimelineRecording = timelines && !timelines->IsEmpty();

  if (isTimelineRecording) {
    start = MakeUnique<WorkerTimelineMarker>(
        NS_IsMainThread()
            ? ProfileTimelineWorkerOperationType::SerializeDataOnMainThread
            : ProfileTimelineWorkerOperationType::SerializeDataOffMainThread,
        MarkerTracingType::START);
  }

  JS::CloneDataPolicy clonePolicy;

  // Parent and dedicated workers are always part of the same cluster.
  clonePolicy.allowIntraClusterClonableSharedObjects();

  if (IsSharedMemoryAllowed()) {
    clonePolicy.allowSharedMemoryObjects();
  }

  runnable->Write(aCx, aMessage, transferable, clonePolicy, aRv);

  if (isTimelineRecording) {
    end = MakeUnique<WorkerTimelineMarker>(
        NS_IsMainThread()
            ? ProfileTimelineWorkerOperationType::SerializeDataOnMainThread
            : ProfileTimelineWorkerOperationType::SerializeDataOffMainThread,
        MarkerTracingType::END);
    timelines->AddMarkerForAllObservedDocShells(start);
    timelines->AddMarkerForAllObservedDocShells(end);
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (!runnable->Dispatch()) {
    aRv = NS_ERROR_FAILURE;
  }
}

void WorkerPrivate::EnterDebuggerEventLoop() {
  auto data = mWorkerThreadAccessible.Access();

  JSContext* cx = GetJSContext();
  MOZ_ASSERT(cx);

  AutoPushEventLoopGlobal eventLoopGlobal(this, cx);
  AutoYieldJSThreadExecution yield;

  CycleCollectedJSContext* ccjscx = CycleCollectedJSContext::Get();

  uint32_t currentEventLoopLevel = ++data->mDebuggerEventLoopLevel;

  while (currentEventLoopLevel <= data->mDebuggerEventLoopLevel) {
    bool debuggerRunnablesPending = false;

    {
      MutexAutoLock lock(mMutex);

      debuggerRunnablesPending = !mDebuggerQueue.IsEmpty();
    }

    // Don't block with the periodic GC timer running.
    if (!debuggerRunnablesPending) {
      SetGCTimerMode(IdleTimer);
    }

    // Wait for something to do
    {
      MutexAutoLock lock(mMutex);

      std::queue<RefPtr<MicroTaskRunnable>>& debuggerMtQueue =
          ccjscx->GetDebuggerMicroTaskQueue();
      while (mControlQueue.IsEmpty() &&
             !(debuggerRunnablesPending = !mDebuggerQueue.IsEmpty()) &&
             debuggerMtQueue.empty()) {
        WaitForWorkerEvents();
      }

      ProcessAllControlRunnablesLocked();

      // XXXkhuey should we abort JS on the stack here if we got Abort above?
    }
    ccjscx->PerformDebuggerMicroTaskCheckpoint();
    if (debuggerRunnablesPending) {
      // Start the periodic GC timer if it is not already running.
      SetGCTimerMode(PeriodicTimer);

      WorkerRunnable* runnable = nullptr;

      {
        MutexAutoLock lock(mMutex);

        mDebuggerQueue.Pop(runnable);
      }

      MOZ_ASSERT(runnable);
      static_cast<nsIRunnable*>(runnable)->Run();
      runnable->Release();

      ccjscx->PerformDebuggerMicroTaskCheckpoint();

      // Now *might* be a good time to GC. Let the JS engine make the decision.
      if (GetCurrentEventLoopGlobal()) {
        // If GetCurrentEventLoopGlobal() is non-null, our JSContext is in a
        // Realm, so it's safe to try to GC.
        MOZ_ASSERT(JS::CurrentGlobalOrNull(cx));
        JS_MaybeGC(cx);
      }
    }
  }
}

void WorkerPrivate::LeaveDebuggerEventLoop() {
  auto data = mWorkerThreadAccessible.Access();

  // TODO: Why lock the mutex if we're accessing data accessible to one thread
  // only?
  MutexAutoLock lock(mMutex);

  if (data->mDebuggerEventLoopLevel > 0) {
    --data->mDebuggerEventLoopLevel;
  }
}

void WorkerPrivate::PostMessageToDebugger(const nsAString& aMessage) {
  mDebugger->PostMessageToDebugger(aMessage);
}

void WorkerPrivate::SetDebuggerImmediate(dom::Function& aHandler,
                                         ErrorResult& aRv) {
  AssertIsOnWorkerThread();

  RefPtr<DebuggerImmediateRunnable> runnable =
      new DebuggerImmediateRunnable(this, aHandler);
  if (!runnable->Dispatch()) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

void WorkerPrivate::ReportErrorToDebugger(const nsAString& aFilename,
                                          uint32_t aLineno,
                                          const nsAString& aMessage) {
  mDebugger->ReportErrorToDebugger(aFilename, aLineno, aMessage);
}

bool WorkerPrivate::NotifyInternal(WorkerStatus aStatus) {
  auto data = mWorkerThreadAccessible.Access();

  // Yield execution while notifying out-of-module WorkerRefs and cancelling
  // runnables.
  AutoYieldJSThreadExecution yield;

  NS_ASSERTION(aStatus > Running && aStatus < Dead, "Bad status!");

  RefPtr<EventTarget> eventTarget;

  // Save the old status and set the new status.
  WorkerStatus previousStatus;
  {
    MutexAutoLock lock(mMutex);

    if (mStatus >= aStatus) {
      return true;
    }

    MOZ_ASSERT_IF(aStatus == Killing, mStatus == Canceling);

    if (aStatus >= Canceling) {
      MutexAutoUnlock unlock(mMutex);
      if (data->mScope) {
        data->mScope->NoteTerminating();
      }
    }

    // Make sure the hybrid event target stops dispatching runnables
    // once we reaching the killing state.
    if (aStatus == Killing) {
      // To avoid deadlock we always acquire the event target mutex before the
      // worker private mutex.  (We do it in this order because this is what
      // workers best for event dispatching.)  To enforce that order here we
      // need to unlock the worker private mutex before we lock the event target
      // mutex in ForgetWorkerPrivate.
      {
        MutexAutoUnlock unlock(mMutex);
        mWorkerHybridEventTarget->ForgetWorkerPrivate(this);
      }

      // Check the status code again in case another NotifyInternal came in
      // while we were unlocked above.
      if (mStatus >= aStatus) {
        return true;
      }
    }

    previousStatus = mStatus;
    mStatus = aStatus;

    // Mark parent status as closing immediately to avoid new events being
    // dispatched after we clear the queue below.
    if (aStatus == Closing) {
      Close();
    }
  }

  MOZ_ASSERT(previousStatus != Pending);

  if (aStatus >= Closing) {
    CancelAllTimeouts();
  }

  // Let all our holders know the new status.
  if (aStatus > Closing) {
    NotifyWorkerRefs(aStatus);
  }

  // If this is the first time our status has changed then we need to clear the
  // main event queue.
  if (previousStatus == Running) {
    // NB: If we're in a sync loop, we can't clear the queue immediately,
    // because this is the wrong queue. So we have to defer it until later.
    if (!mSyncLoopStack.IsEmpty()) {
      mPostSyncLoopOperations |= ePendingEventQueueClearing;
    } else {
      ClearMainEventQueue(WorkerRan);
    }
  }

  // If the worker script never ran, or failed to compile, we don't need to do
  // anything else.
  if (!GlobalScope()) {
    return true;
  }

  // Don't abort the script now, but we dispatch a runnable to do it when the
  // current JS frame is executed.
  if (aStatus == Closing) {
    if (!mSyncLoopStack.IsEmpty()) {
      mPostSyncLoopOperations |= eDispatchCancelingRunnable;
    } else {
      DispatchCancelingRunnable();
    }
    return true;
  }

  MOZ_ASSERT(aStatus == Canceling || aStatus == Killing);

  // Always abort the script.
  return false;
}

void WorkerPrivate::ReportError(JSContext* aCx,
                                JS::ConstUTF8CharsZ aToStringResult,
                                JSErrorReport* aReport) {
  auto data = mWorkerThreadAccessible.Access();

  if (!MayContinueRunning() || data->mErrorHandlerRecursionCount == 2) {
    return;
  }

  NS_ASSERTION(data->mErrorHandlerRecursionCount == 0 ||
                   data->mErrorHandlerRecursionCount == 1,
               "Bad recursion logic!");

  UniquePtr<WorkerErrorReport> report = MakeUnique<WorkerErrorReport>();
  if (aReport) {
    report->AssignErrorReport(aReport);
  }

  JS::ExceptionStack exnStack(aCx);
  if (JS_IsExceptionPending(aCx)) {
    if (!JS::StealPendingExceptionStack(aCx, &exnStack)) {
      JS_ClearPendingException(aCx);
      return;
    }

    JS::RootedObject stack(aCx), stackGlobal(aCx);
    xpc::FindExceptionStackForConsoleReport(
        nullptr, exnStack.exception(), exnStack.stack(), &stack, &stackGlobal);

    if (stack) {
      JSAutoRealm ar(aCx, stackGlobal);
      report->SerializeWorkerStack(aCx, this, stack);
    }
  } else {
    // ReportError is also used for reporting warnings,
    // so there won't be a pending exception.
    MOZ_ASSERT(aReport->isWarning());
  }

  if (report->mMessage.IsEmpty() && aToStringResult) {
    nsDependentCString toStringResult(aToStringResult.c_str());
    if (!AppendUTF8toUTF16(toStringResult, report->mMessage,
                           mozilla::fallible)) {
      // Try again, with only a 1 KB string. Do this infallibly this time.
      // If the user doesn't have 1 KB to spare we're done anyways.
      uint32_t index = std::min(uint32_t(1024), toStringResult.Length());

      // Drop the last code point that may be cropped.
      index = RewindToPriorUTF8Codepoint(toStringResult.BeginReading(), index);

      nsDependentCString truncatedToStringResult(aToStringResult.c_str(),
                                                 index);
      AppendUTF8toUTF16(truncatedToStringResult, report->mMessage);
    }
  }

  data->mErrorHandlerRecursionCount++;

  // Don't want to run the scope's error handler if this is a recursive error or
  // if we ran out of memory.
  bool fireAtScope = data->mErrorHandlerRecursionCount == 1 &&
                     report->mErrorNumber != JSMSG_OUT_OF_MEMORY &&
                     JS::CurrentGlobalOrNull(aCx);

  WorkerErrorReport::ReportError(aCx, this, fireAtScope, nullptr,
                                 std::move(report), 0, exnStack.exception());

  data->mErrorHandlerRecursionCount--;
}

// static
void WorkerPrivate::ReportErrorToConsole(const char* aMessage) {
  nsTArray<nsString> emptyParams;
  WorkerPrivate::ReportErrorToConsole(aMessage, emptyParams);
}

// static
void WorkerPrivate::ReportErrorToConsole(const char* aMessage,
                                         const nsTArray<nsString>& aParams) {
  WorkerPrivate* wp = nullptr;
  if (!NS_IsMainThread()) {
    wp = GetCurrentThreadWorkerPrivate();
  }

  ReportErrorToConsoleRunnable::Report(wp, aMessage, aParams);
}

int32_t WorkerPrivate::SetTimeout(JSContext* aCx, TimeoutHandler* aHandler,
                                  int32_t aTimeout, bool aIsInterval,
                                  ErrorResult& aRv) {
  auto data = mWorkerThreadAccessible.Access();
  MOZ_ASSERT(aHandler);

  const int32_t timerId = data->mNextTimeoutId++;

  WorkerStatus currentStatus;
  {
    MutexAutoLock lock(mMutex);
    currentStatus = mStatus;
  }

  // If the worker is trying to call setTimeout/setInterval and the parent
  // thread has initiated the close process then just silently fail.
  if (currentStatus >= Closing) {
    return timerId;
  }

  auto newInfo = MakeUnique<TimeoutInfo>();
  newInfo->mOnChromeWorker = mIsChromeWorker;
  newInfo->mIsInterval = aIsInterval;
  newInfo->mId = timerId;
  newInfo->AccumulateNestingLevel(data->mCurrentTimerNestingLevel);

  if (MOZ_UNLIKELY(timerId == INT32_MAX)) {
    NS_WARNING("Timeout ids overflowed!");
    data->mNextTimeoutId = 1;
  }

  newInfo->mHandler = aHandler;

  // See if any of the optional arguments were passed.
  aTimeout = std::max(0, aTimeout);
  newInfo->mInterval = TimeDuration::FromMilliseconds(aTimeout);
  newInfo->CalculateTargetTime();

  const auto& insertedInfo = data->mTimeouts.InsertElementSorted(
      std::move(newInfo), GetUniquePtrComparator(data->mTimeouts));

  LOG(TimeoutsLog(), ("Worker %p has new timeout: delay=%d interval=%s\n", this,
                      aTimeout, aIsInterval ? "yes" : "no"));

  // If the timeout we just made is set to fire next then we need to update the
  // timer, unless we're currently running timeouts.
  if (insertedInfo == data->mTimeouts.Elements() &&
      !data->mRunningExpiredTimeouts) {
    if (!data->mTimer) {
      data->mTimer = NS_NewTimer();
      if (!data->mTimer) {
        aRv.Throw(NS_ERROR_UNEXPECTED);
        return 0;
      }

      data->mTimerRunnable = new TimerRunnable(this);
    }

    if (!data->mTimerRunning) {
      if (!ModifyBusyCountFromWorker(true)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return 0;
      }
      data->mTimerRunning = true;
    }

    if (!RescheduleTimeoutTimer(aCx)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return 0;
    }
  }

  return timerId;
}

void WorkerPrivate::ClearTimeout(int32_t aId) {
  auto data = mWorkerThreadAccessible.Access();

  if (!data->mTimeouts.IsEmpty()) {
    NS_ASSERTION(data->mTimerRunning, "Huh?!");

    for (uint32_t index = 0; index < data->mTimeouts.Length(); index++) {
      const auto& info = data->mTimeouts[index];
      if (info->mId == aId) {
        info->mCanceled = true;
        break;
      }
    }
  }
}

bool WorkerPrivate::RunExpiredTimeouts(JSContext* aCx) {
  auto data = mWorkerThreadAccessible.Access();

  // We may be called recursively (e.g. close() inside a timeout) or we could
  // have been canceled while this event was pending, bail out if there is
  // nothing to do.
  if (data->mRunningExpiredTimeouts || !data->mTimerRunning) {
    return true;
  }

  NS_ASSERTION(data->mTimer && data->mTimerRunnable, "Must have a timer!");
  NS_ASSERTION(!data->mTimeouts.IsEmpty(), "Should have some work to do!");

  bool retval = true;

  auto comparator = GetUniquePtrComparator(data->mTimeouts);
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));

  // We want to make sure to run *something*, even if the timer fired a little
  // early. Fudge the value of now to at least include the first timeout.
  const TimeStamp actual_now = TimeStamp::Now();
  const TimeStamp now = std::max(actual_now, data->mTimeouts[0]->mTargetTime);

  if (now != actual_now) {
    LOG(TimeoutsLog(), ("Worker %p fudged timeout by %f ms.\n", this,
                        (now - actual_now).ToMilliseconds()));
  }

  AutoTArray<TimeoutInfo*, 10> expiredTimeouts;
  for (uint32_t index = 0; index < data->mTimeouts.Length(); index++) {
    TimeoutInfo* info = data->mTimeouts[index].get();
    if (info->mTargetTime > now) {
      break;
    }
    expiredTimeouts.AppendElement(info);
  }

  // Guard against recursion.
  data->mRunningExpiredTimeouts = true;

  MOZ_DIAGNOSTIC_ASSERT(data->mCurrentTimerNestingLevel == 0);

  // Run expired timeouts.
  for (uint32_t index = 0; index < expiredTimeouts.Length(); index++) {
    TimeoutInfo*& info = expiredTimeouts[index];
    AutoRestore<uint32_t> nestingLevel(data->mCurrentTimerNestingLevel);

    if (info->mCanceled) {
      continue;
    }

    // Set current timer nesting level to current running timer handler's
    // nesting level
    data->mCurrentTimerNestingLevel = info->mNestingLevel;

    LOG(TimeoutsLog(),
        ("Worker %p executing timeout with original delay %f ms.\n", this,
         info->mInterval.ToMilliseconds()));

    // Always check JS_IsExceptionPending if something fails, and if
    // JS_IsExceptionPending returns false (i.e. uncatchable exception) then
    // break out of the loop.
    const char* reason;
    if (info->mIsInterval) {
      reason = "setInterval handler";
    } else {
      reason = "setTimeout handler";
    }

    RefPtr<TimeoutHandler> handler(info->mHandler);

    RefPtr<WorkerGlobalScope> scope(this->GlobalScope());
    CallbackDebuggerNotificationGuard guard(
        scope, info->mIsInterval
                   ? DebuggerNotificationType::SetIntervalCallback
                   : DebuggerNotificationType::SetTimeoutCallback);
    if (!handler->Call(reason)) {
      retval = false;
      break;
    }

    NS_ASSERTION(data->mRunningExpiredTimeouts, "Someone changed this!");
  }

  // No longer possible to be called recursively.
  data->mRunningExpiredTimeouts = false;

  // Now remove canceled and expired timeouts from the main list.
  // NB: The timeouts present in expiredTimeouts must have the same order
  // with respect to each other in mTimeouts.  That is, mTimeouts is just
  // expiredTimeouts with extra elements inserted.  There may be unexpired
  // timeouts that have been inserted between the expired timeouts if the
  // timeout event handler called setTimeout/setInterval.
  for (uint32_t index = 0, expiredTimeoutIndex = 0,
                expiredTimeoutLength = expiredTimeouts.Length();
       index < data->mTimeouts.Length();) {
    const auto& info = data->mTimeouts[index];
    if ((expiredTimeoutIndex < expiredTimeoutLength &&
         info == expiredTimeouts[expiredTimeoutIndex] &&
         ++expiredTimeoutIndex) ||
        info->mCanceled) {
      if (info->mIsInterval && !info->mCanceled) {
        // Reschedule intervals.
        // Reschedule a timeout, if needed, increase the nesting level.
        info->AccumulateNestingLevel(info->mNestingLevel);
        info->CalculateTargetTime();
        // Don't resort the list here, we'll do that at the end.
        ++index;
      } else {
        data->mTimeouts.RemoveElement(info);
      }
    } else {
      // If info did not match the current entry in expiredTimeouts, it
      // shouldn't be there at all.
      NS_ASSERTION(!expiredTimeouts.Contains(info),
                   "Our timeouts are out of order!");
      ++index;
    }
  }

  data->mTimeouts.Sort(comparator);

  // Either signal the parent that we're no longer using timeouts or reschedule
  // the timer.
  if (data->mTimeouts.IsEmpty()) {
    if (!ModifyBusyCountFromWorker(false)) {
      retval = false;
    }
    data->mTimerRunning = false;
  } else if (retval && !RescheduleTimeoutTimer(aCx)) {
    retval = false;
  }

  return retval;
}

bool WorkerPrivate::RescheduleTimeoutTimer(JSContext* aCx) {
  auto data = mWorkerThreadAccessible.Access();
  MOZ_ASSERT(!data->mRunningExpiredTimeouts);
  NS_ASSERTION(!data->mTimeouts.IsEmpty(), "Should have some timeouts!");
  NS_ASSERTION(data->mTimer && data->mTimerRunnable, "Should have a timer!");

  // NB: This is important! The timer may have already fired, e.g. if a timeout
  // callback itself calls setTimeout for a short duration and then takes longer
  // than that to finish executing. If that has happened, it's very important
  // that we don't execute the event that is now pending in our event queue, or
  // our code in RunExpiredTimeouts to "fudge" the timeout value will unleash an
  // early timeout when we execute the event we're about to queue.
  data->mTimer->Cancel();

  double delta =
      (data->mTimeouts[0]->mTargetTime - TimeStamp::Now()).ToMilliseconds();
  uint32_t delay = delta > 0 ? std::min(delta, double(UINT32_MAX)) : 0;

  LOG(TimeoutsLog(),
      ("Worker %p scheduled timer for %d ms, %zu pending timeouts\n", this,
       delay, data->mTimeouts.Length()));

  nsresult rv = data->mTimer->InitWithCallback(data->mTimerRunnable, delay,
                                               nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    JS_ReportErrorASCII(aCx, "Failed to start timer!");
    return false;
  }

  return true;
}

void WorkerPrivate::StartCancelingTimer() {
  AssertIsOnParentThread();

  auto errorCleanup = MakeScopeExit([&] { mCancelingTimer = nullptr; });

  MOZ_ASSERT(!mCancelingTimer);

  if (WorkerPrivate* parent = GetParent()) {
    mCancelingTimer = NS_NewTimer(parent->ControlEventTarget());
  } else {
    mCancelingTimer = NS_NewTimer();
  }

  if (NS_WARN_IF(!mCancelingTimer)) {
    return;
  }

  // This is not needed if we are already in an advanced shutdown state.
  {
    MutexAutoLock lock(mMutex);
    if (ParentStatus() >= Canceling) {
      return;
    }
  }

  uint32_t cancelingTimeoutMillis =
      StaticPrefs::dom_worker_canceling_timeoutMilliseconds();

  RefPtr<CancelingTimerCallback> callback = new CancelingTimerCallback(this);
  nsresult rv = mCancelingTimer->InitWithCallback(
      callback, cancelingTimeoutMillis, nsITimer::TYPE_ONE_SHOT);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  errorCleanup.release();
}

void WorkerPrivate::UpdateContextOptionsInternal(
    JSContext* aCx, const JS::ContextOptions& aContextOptions) {
  auto data = mWorkerThreadAccessible.Access();

  JS::ContextOptionsRef(aCx) = aContextOptions;

  for (uint32_t index = 0; index < data->mChildWorkers.Length(); index++) {
    data->mChildWorkers[index]->UpdateContextOptions(aContextOptions);
  }
}

void WorkerPrivate::UpdateLanguagesInternal(
    const nsTArray<nsString>& aLanguages) {
  WorkerGlobalScope* globalScope = GlobalScope();
  RefPtr<WorkerNavigator> nav = globalScope->GetExistingNavigator();
  if (nav) {
    nav->SetLanguages(aLanguages);
  }

  auto data = mWorkerThreadAccessible.Access();
  for (uint32_t index = 0; index < data->mChildWorkers.Length(); index++) {
    data->mChildWorkers[index]->UpdateLanguages(aLanguages);
  }

  RefPtr<Event> event = NS_NewDOMEvent(globalScope, nullptr, nullptr);

  event->InitEvent(u"languagechange"_ns, false, false);
  event->SetTrusted(true);

  globalScope->DispatchEvent(*event);
}

void WorkerPrivate::UpdateJSWorkerMemoryParameterInternal(
    JSContext* aCx, JSGCParamKey aKey, Maybe<uint32_t> aValue) {
  auto data = mWorkerThreadAccessible.Access();

  if (aValue) {
    JS_SetGCParameter(aCx, aKey, *aValue);
  } else {
    JS_ResetGCParameter(aCx, aKey);
  }

  for (uint32_t index = 0; index < data->mChildWorkers.Length(); index++) {
    data->mChildWorkers[index]->UpdateJSWorkerMemoryParameter(aKey, aValue);
  }
}

#ifdef JS_GC_ZEAL
void WorkerPrivate::UpdateGCZealInternal(JSContext* aCx, uint8_t aGCZeal,
                                         uint32_t aFrequency) {
  auto data = mWorkerThreadAccessible.Access();

  JS_SetGCZeal(aCx, aGCZeal, aFrequency);

  for (uint32_t index = 0; index < data->mChildWorkers.Length(); index++) {
    data->mChildWorkers[index]->UpdateGCZeal(aGCZeal, aFrequency);
  }
}
#endif

void WorkerPrivate::SetLowMemoryStateInternal(JSContext* aCx, bool aState) {
  auto data = mWorkerThreadAccessible.Access();

  JS::SetLowMemoryState(aCx, aState);

  for (uint32_t index = 0; index < data->mChildWorkers.Length(); index++) {
    data->mChildWorkers[index]->SetLowMemoryState(aState);
  }
}

void WorkerPrivate::SetCCCollectedAnything(bool collectedAnything) {
  mWorkerThreadAccessible.Access()->mCCCollectedAnything = collectedAnything;
}

void WorkerPrivate::GarbageCollectInternal(JSContext* aCx, bool aShrinking,
                                           bool aCollectChildren) {
  // Perform GC followed by CC (the CC is triggered by
  // WorkerJSRuntime::CustomGCCallback at the end of the collection).

  auto data = mWorkerThreadAccessible.Access();

  if (!GlobalScope()) {
    // We haven't compiled anything yet. Just bail out.
    return;
  }

  if (aShrinking || aCollectChildren) {
    JS::PrepareForFullGC(aCx);

    if (aShrinking && mSyncLoopStack.IsEmpty()) {
      JS::NonIncrementalGC(aCx, GC_SHRINK, JS::GCReason::DOM_WORKER);

      // Check whether the CC collected anything and if so GC again. This is
      // necessary to collect all garbage.
      if (data->mCCCollectedAnything) {
        JS::NonIncrementalGC(aCx, GC_NORMAL, JS::GCReason::DOM_WORKER);
      }

      if (!aCollectChildren) {
        LOG(WorkerLog(), ("Worker %p collected idle garbage\n", this));
      }
    } else {
      JS::NonIncrementalGC(aCx, GC_NORMAL, JS::GCReason::DOM_WORKER);
      LOG(WorkerLog(), ("Worker %p collected garbage\n", this));
    }
  } else {
    JS_MaybeGC(aCx);
    LOG(WorkerLog(), ("Worker %p collected periodic garbage\n", this));
  }

  if (aCollectChildren) {
    for (uint32_t index = 0; index < data->mChildWorkers.Length(); index++) {
      data->mChildWorkers[index]->GarbageCollect(aShrinking);
    }
  }
}

void WorkerPrivate::CycleCollectInternal(bool aCollectChildren) {
  auto data = mWorkerThreadAccessible.Access();

  nsCycleCollector_collect(nullptr);

  if (aCollectChildren) {
    for (uint32_t index = 0; index < data->mChildWorkers.Length(); index++) {
      data->mChildWorkers[index]->CycleCollect();
    }
  }
}

void WorkerPrivate::MemoryPressureInternal() {
  auto data = mWorkerThreadAccessible.Access();

  if (data->mScope) {
    RefPtr<Console> console = data->mScope->GetConsoleIfExists();
    if (console) {
      console->ClearStorage();
    }

    RefPtr<Performance> performance = data->mScope->GetPerformanceIfExists();
    if (performance) {
      performance->MemoryPressure();
    }

    data->mScope->RemoveReportRecords();
  }

  if (data->mDebuggerScope) {
    RefPtr<Console> console = data->mDebuggerScope->GetConsoleIfExists();
    if (console) {
      console->ClearStorage();
    }
  }

  for (uint32_t index = 0; index < data->mChildWorkers.Length(); index++) {
    data->mChildWorkers[index]->MemoryPressure();
  }
}

void WorkerPrivate::SetThread(WorkerThread* aThread) {
  if (aThread) {
#ifdef DEBUG
    {
      bool isOnCurrentThread;
      MOZ_ASSERT(NS_SUCCEEDED(aThread->IsOnCurrentThread(&isOnCurrentThread)));
      MOZ_ASSERT(!isOnCurrentThread);
    }
#endif

    MOZ_ASSERT(!mPRThread);
    mPRThread = PRThreadFromThread(aThread);
    MOZ_ASSERT(mPRThread);

    mWorkerThreadAccessible.Transfer(mPRThread);
  } else {
    MOZ_ASSERT(mPRThread);
  }
}

void WorkerPrivate::SetWorkerPrivateInWorkerThread(
    WorkerThread* const aThread) {
  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(!mThread);
  MOZ_ASSERT(mStatus == Pending);

  mThread = aThread;
  mThread->SetWorker(WorkerThreadFriendKey{}, this);

  if (!mPreStartRunnables.IsEmpty()) {
    for (uint32_t index = 0; index < mPreStartRunnables.Length(); index++) {
      MOZ_ALWAYS_SUCCEEDS(mThread->DispatchAnyThread(
          WorkerThreadFriendKey{}, mPreStartRunnables[index].forget()));
    }
    mPreStartRunnables.Clear();
  }
}

void WorkerPrivate::ResetWorkerPrivateInWorkerThread() {
  RefPtr<WorkerThread> doomedThread;

  // Release the mutex before doomedThread.
  MutexAutoLock lock(mMutex);

  MOZ_ASSERT(mThread);

  mThread->SetWorker(WorkerThreadFriendKey{}, nullptr);
  mThread.swap(doomedThread);
}

void WorkerPrivate::BeginCTypesCall() {
  AssertIsOnWorkerThread();
  auto data = mWorkerThreadAccessible.Access();

  // Don't try to GC while we're blocked in a ctypes call.
  SetGCTimerMode(NoTimer);

  data->mYieldJSThreadExecution.EmplaceBack();
}

void WorkerPrivate::EndCTypesCall() {
  AssertIsOnWorkerThread();
  auto data = mWorkerThreadAccessible.Access();

  data->mYieldJSThreadExecution.RemoveLastElement();

  // Make sure the periodic timer is running before we start running JS again.
  SetGCTimerMode(PeriodicTimer);
}

void WorkerPrivate::BeginCTypesCallback() {
  AssertIsOnWorkerThread();

  // Make sure the periodic timer is running before we start running JS again.
  SetGCTimerMode(PeriodicTimer);

  // Re-requesting execution is not needed since the JSRuntime code calling
  // this will do an AutoEntryScript.
}

void WorkerPrivate::EndCTypesCallback() {
  AssertIsOnWorkerThread();

  // Don't try to GC while we're blocked in a ctypes call.
  SetGCTimerMode(NoTimer);
}

bool WorkerPrivate::ConnectMessagePort(JSContext* aCx,
                                       UniqueMessagePortId& aIdentifier) {
  AssertIsOnWorkerThread();

  WorkerGlobalScope* globalScope = GlobalScope();

  JS::Rooted<JSObject*> jsGlobal(aCx, globalScope->GetWrapper());
  MOZ_ASSERT(jsGlobal);

  // This UniqueMessagePortId is used to create a new port, still connected
  // with the other one, but in the worker thread.
  ErrorResult rv;
  RefPtr<MessagePort> port = MessagePort::Create(globalScope, aIdentifier, rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return false;
  }

  GlobalObject globalObject(aCx, jsGlobal);
  if (globalObject.Failed()) {
    return false;
  }

  RootedDictionary<MessageEventInit> init(aCx);
  init.mData = JS_GetEmptyStringValue(aCx);
  init.mBubbles = false;
  init.mCancelable = false;
  init.mSource.SetValue().SetAsMessagePort() = port;
  if (!init.mPorts.AppendElement(port.forget(), fallible)) {
    return false;
  }

  RefPtr<MessageEvent> event =
      MessageEvent::Constructor(globalObject, u"connect"_ns, init);

  event->SetTrusted(true);

  globalScope->DispatchEvent(*event);

  return true;
}

WorkerGlobalScope* WorkerPrivate::GetOrCreateGlobalScope(JSContext* aCx) {
  auto data = mWorkerThreadAccessible.Access();

  if (data->mScope) {
    return data->mScope;
  }

  if (IsSharedWorker()) {
    data->mScope = new SharedWorkerGlobalScope(
        WrapNotNull(this), CreateClientSource(), WorkerName());
  } else if (IsServiceWorker()) {
    data->mScope =
        new ServiceWorkerGlobalScope(WrapNotNull(this), CreateClientSource(),
                                     GetServiceWorkerRegistrationDescriptor());
  } else {
    data->mScope = new DedicatedWorkerGlobalScope(
        WrapNotNull(this), CreateClientSource(), WorkerName());
  }

  JS::Rooted<JSObject*> global(aCx);
  NS_ENSURE_TRUE(data->mScope->WrapGlobalObject(aCx, &global), nullptr);

  JSAutoRealm ar(aCx, global);

  if (!RegisterBindings(aCx, global)) {
    data->mScope = nullptr;
    return nullptr;
  }

  JS_FireOnNewGlobalObject(aCx, global);

  return data->mScope;
}

WorkerDebuggerGlobalScope* WorkerPrivate::CreateDebuggerGlobalScope(
    JSContext* aCx) {
  auto data = mWorkerThreadAccessible.Access();
  MOZ_ASSERT(!data->mDebuggerScope);

  // The debugger global gets a dummy client, not the "real" client used by the
  // debugee worker.
  auto clientSource = ClientManager::CreateSource(
      GetClientType(), HybridEventTarget(), NullPrincipalInfo());

  data->mDebuggerScope =
      new WorkerDebuggerGlobalScope(WrapNotNull(this), std::move(clientSource));

  JS::Rooted<JSObject*> global(aCx);
  NS_ENSURE_TRUE(data->mDebuggerScope->WrapGlobalObject(aCx, &global), nullptr);

  JSAutoRealm ar(aCx, global);

  if (!RegisterDebuggerBindings(aCx, global)) {
    data->mDebuggerScope = nullptr;
    return nullptr;
  }

  JS_FireOnNewGlobalObject(aCx, global);

  return data->mDebuggerScope;
}

bool WorkerPrivate::IsOnWorkerThread() const {
  // We can't use mThread because it must be protected by mMutex and sometimes
  // this method is called when mMutex is already locked. This method should
  // always work.
  MOZ_ASSERT(mPRThread,
             "AssertIsOnWorkerThread() called before a thread was assigned!");

  return mPRThread == PR_GetCurrentThread();
}

#ifdef DEBUG
void WorkerPrivate::AssertIsOnWorkerThread() const {
  MOZ_ASSERT(IsOnWorkerThread());
}
#endif  // DEBUG

void WorkerPrivate::DumpCrashInformation(nsACString& aString) {
  auto data = mWorkerThreadAccessible.Access();

  aString.Append("IsChromeWorker(");
  if (IsChromeWorker()) {
    aString.Append(NS_ConvertUTF16toUTF8(ScriptURL()));
  } else {
    aString.Append("false");
  }
  aString.Append(")");
  for (const auto* workerRef : data->mWorkerRefs.NonObservingRange()) {
    if (workerRef->IsPreventingShutdown()) {
      aString.Append("|");
      aString.Append(workerRef->Name());
    }
  }
}

PerformanceStorage* WorkerPrivate::GetPerformanceStorage() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mPerformanceStorage);
  return mPerformanceStorage;
}

void WorkerPrivate::SetRemoteWorkerController(RemoteWorkerChild* aController) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aController);
  MOZ_ASSERT(!mRemoteWorkerController);

  mRemoteWorkerController = aController;
}

RemoteWorkerChild* WorkerPrivate::GetRemoteWorkerController() {
  AssertIsOnMainThread();
  MOZ_ASSERT(mRemoteWorkerController);
  return mRemoteWorkerController;
}

void WorkerPrivate::SetRemoteWorkerControllerWeakRef(
    ThreadSafeWeakPtr<RemoteWorkerChild> aWeakRef) {
  MOZ_ASSERT(!aWeakRef.IsNull());
  MOZ_ASSERT(mRemoteWorkerControllerWeakRef.IsNull());
  MOZ_ASSERT(IsServiceWorker());

  mRemoteWorkerControllerWeakRef = std::move(aWeakRef);
}

ThreadSafeWeakPtr<RemoteWorkerChild>
WorkerPrivate::GetRemoteWorkerControllerWeakRef() {
  MOZ_ASSERT(IsServiceWorker());
  return mRemoteWorkerControllerWeakRef;
}

RefPtr<GenericPromise> WorkerPrivate::SetServiceWorkerSkipWaitingFlag() {
  AssertIsOnWorkerThread();
  MOZ_ASSERT(IsServiceWorker());

  RefPtr<RemoteWorkerChild> rwc(mRemoteWorkerControllerWeakRef);

  if (!rwc) {
    return GenericPromise::CreateAndReject(NS_ERROR_DOM_ABORT_ERR, __func__);
  }

  RefPtr<GenericPromise> promise =
      rwc->MaybeSendSetServiceWorkerSkipWaitingFlag();

  NS_ProxyRelease("WorkerPrivate::mRemoteWorkerControllerWeakRef",
                  RemoteWorkerService::Thread(), rwc.forget());

  return promise;
}

const nsAString& WorkerPrivate::Id() {
  AssertIsOnMainThread();

  if (mId.IsEmpty()) {
    mId = ComputeWorkerPrivateId();
  }

  MOZ_ASSERT(!mId.IsEmpty());

  return mId;
}

bool WorkerPrivate::IsSharedMemoryAllowed() const {
  if (StaticPrefs::
          dom_postMessage_sharedArrayBuffer_bypassCOOP_COEP_insecure_enabled()) {
    return true;
  }

  return CrossOriginIsolated();
}

bool WorkerPrivate::CrossOriginIsolated() const {
  if (!StaticPrefs::
          dom_postMessage_sharedArrayBuffer_withCOOP_COEP_AtStartup()) {
    return false;
  }

  return mAgentClusterOpenerPolicy ==
         nsILoadInfo::OPENER_POLICY_SAME_ORIGIN_EMBEDDER_POLICY_REQUIRE_CORP;
}

nsILoadInfo::CrossOriginEmbedderPolicy WorkerPrivate::GetEmbedderPolicy()
    const {
  if (!StaticPrefs::browser_tabs_remote_useCrossOriginEmbedderPolicy()) {
    return nsILoadInfo::EMBEDDER_POLICY_NULL;
  }

  return mEmbedderPolicy.valueOr(nsILoadInfo::EMBEDDER_POLICY_NULL);
}

Result<Ok, nsresult> WorkerPrivate::SetEmbedderPolicy(
    nsILoadInfo::CrossOriginEmbedderPolicy aPolicy) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mEmbedderPolicy.isNothing());

  if (!StaticPrefs::browser_tabs_remote_useCrossOriginEmbedderPolicy()) {
    return Ok();
  }

  // If owner's emebedder policy is corp_reqired, aPolicy must also be
  // corp_reqired. But if owner's embedder policy is null, aPolicy needs not
  // match owner's value.
  // https://wicg.github.io/cross-origin-embedder-policy/#cascade-vs-require
  EnsureOwnerEmbedderPolicy();
  if (mOwnerEmbedderPolicy.valueOr(nsILoadInfo::EMBEDDER_POLICY_NULL) !=
      nsILoadInfo::EMBEDDER_POLICY_NULL) {
    if (mOwnerEmbedderPolicy.valueOr(aPolicy) != aPolicy) {
      return Err(NS_ERROR_BLOCKED_BY_POLICY);
    }
  }

  mEmbedderPolicy.emplace(aPolicy);

  return Ok();
}

void WorkerPrivate::InheritOwnerEmbedderPolicyOrNull(nsIRequest* aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);

  EnsureOwnerEmbedderPolicy();

  if (mOwnerEmbedderPolicy.isSome()) {
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
    MOZ_ASSERT(channel);

    nsCOMPtr<nsIURI> scriptURI;
    MOZ_ALWAYS_SUCCEEDS(channel->GetURI(getter_AddRefs(scriptURI)));

    bool isLocalScriptURI = false;
    MOZ_ALWAYS_SUCCEEDS(NS_URIChainHasFlags(
        scriptURI, nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
        &isLocalScriptURI));

    MOZ_RELEASE_ASSERT(isLocalScriptURI);
  }

  mEmbedderPolicy.emplace(
      mOwnerEmbedderPolicy.valueOr(nsILoadInfo::EMBEDDER_POLICY_NULL));
}

bool WorkerPrivate::MatchEmbedderPolicy(
    nsILoadInfo::CrossOriginEmbedderPolicy aPolicy) const {
  MOZ_ASSERT(NS_IsMainThread());

  if (!StaticPrefs::browser_tabs_remote_useCrossOriginEmbedderPolicy()) {
    return true;
  }

  return mEmbedderPolicy.value() == aPolicy;
}

nsILoadInfo::CrossOriginEmbedderPolicy WorkerPrivate::GetOwnerEmbedderPolicy()
    const {
  if (!StaticPrefs::browser_tabs_remote_useCrossOriginEmbedderPolicy()) {
    return nsILoadInfo::EMBEDDER_POLICY_NULL;
  }

  return mOwnerEmbedderPolicy.valueOr(nsILoadInfo::EMBEDDER_POLICY_NULL);
}

void WorkerPrivate::EnsureOwnerEmbedderPolicy() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwnerEmbedderPolicy.isNothing());

  if (GetParent()) {
    mOwnerEmbedderPolicy.emplace(GetParent()->GetEmbedderPolicy());
  } else if (GetWindow() && GetWindow()->GetWindowContext()) {
    mOwnerEmbedderPolicy.emplace(
        GetWindow()->GetWindowContext()->GetEmbedderPolicy());
  }
}

const mozilla::ipc::PrincipalInfo&
WorkerPrivate::GetEffectiveStoragePrincipalInfo() const {
  AssertIsOnWorkerThread();

  if (mLoadInfo.mUseRegularPrincipal) {
    return *mLoadInfo.mPrincipalInfo;
  }

  return *mLoadInfo.mPartitionedPrincipalInfo;
}

const nsACString& WorkerPrivate::EffectiveStoragePrincipalOrigin() const {
  AssertIsOnWorkerThread();

  if (mLoadInfo.mUseRegularPrincipal) {
    return mLoadInfo.mOrigin;
  }

  return mLoadInfo.mPartitionedOrigin;
}

NS_IMPL_ADDREF(WorkerPrivate::EventTarget)
NS_IMPL_RELEASE(WorkerPrivate::EventTarget)

NS_INTERFACE_MAP_BEGIN(WorkerPrivate::EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsISerialEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
#ifdef DEBUG
  // kDEBUGWorkerEventTargetIID is special in that it does not AddRef its
  // result.
  if (aIID.Equals(kDEBUGWorkerEventTargetIID)) {
    *aInstancePtr = this;
    return NS_OK;
  } else
#endif
NS_INTERFACE_MAP_END

NS_IMETHODIMP
WorkerPrivate::EventTarget::DispatchFromScript(nsIRunnable* aRunnable,
                                               uint32_t aFlags) {
  nsCOMPtr<nsIRunnable> event(aRunnable);
  return Dispatch(event.forget(), aFlags);
}

NS_IMETHODIMP
WorkerPrivate::EventTarget::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                                     uint32_t aFlags) {
  // May be called on any thread!
  nsCOMPtr<nsIRunnable> event(aRunnable);

  // Workers only support asynchronous dispatch for now.
  if (NS_WARN_IF(aFlags != NS_DISPATCH_NORMAL)) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<WorkerRunnable> workerRunnable;

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    NS_WARNING(
        "A runnable was posted to a worker that is already shutting "
        "down!");
    return NS_ERROR_UNEXPECTED;
  }

  if (event) {
    workerRunnable = mWorkerPrivate->MaybeWrapAsWorkerRunnable(event.forget());
  }

  nsresult rv =
      mWorkerPrivate->Dispatch(workerRunnable.forget(), mNestedEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
WorkerPrivate::EventTarget::DelayedDispatch(already_AddRefed<nsIRunnable>,
                                            uint32_t)

{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WorkerPrivate::EventTarget::IsOnCurrentThread(bool* aIsOnCurrentThread) {
  // May be called on any thread!

  MOZ_ASSERT(aIsOnCurrentThread);

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    NS_WARNING("A worker's event target was used after the worker has !");
    return NS_ERROR_UNEXPECTED;
  }

  *aIsOnCurrentThread = mWorkerPrivate->IsOnCurrentThread();
  return NS_OK;
}

NS_IMETHODIMP_(bool)
WorkerPrivate::EventTarget::IsOnCurrentThreadInfallible() {
  // May be called on any thread!

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    NS_WARNING("A worker's event target was used after the worker has !");
    return false;
  }

  return mWorkerPrivate->IsOnCurrentThread();
}

WorkerPrivate::AutoPushEventLoopGlobal::AutoPushEventLoopGlobal(
    WorkerPrivate* aWorkerPrivate, JSContext* aCx)
    : mWorkerPrivate(aWorkerPrivate) {
  auto data = mWorkerPrivate->mWorkerThreadAccessible.Access();
  mOldEventLoopGlobal = std::move(data->mCurrentEventLoopGlobal);
  if (JSObject* global = JS::CurrentGlobalOrNull(aCx)) {
    data->mCurrentEventLoopGlobal = xpc::NativeGlobal(global);
  }
}

WorkerPrivate::AutoPushEventLoopGlobal::~AutoPushEventLoopGlobal() {
  auto data = mWorkerPrivate->mWorkerThreadAccessible.Access();
  data->mCurrentEventLoopGlobal = std::move(mOldEventLoopGlobal);
}

}  // namespace dom
}  // namespace mozilla
