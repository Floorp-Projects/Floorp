/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerDebugger.h"

#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/PerformanceUtils.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsThreadUtils.h"
#include "ScriptLoader.h"
#include "WorkerCommon.h"
#include "WorkerError.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#if defined(XP_WIN)
#  include <processthreadsapi.h>  // for GetCurrentProcessId()
#else
#  include <unistd.h>  // for getpid()
#endif                 // defined(XP_WIN)

namespace mozilla {
namespace dom {

namespace {

class DebuggerMessageEventRunnable : public WorkerDebuggerRunnable {
  nsString mMessage;

 public:
  DebuggerMessageEventRunnable(WorkerPrivate* aWorkerPrivate,
                               const nsAString& aMessage)
      : WorkerDebuggerRunnable(aWorkerPrivate), mMessage(aMessage) {}

 private:
  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    WorkerDebuggerGlobalScope* globalScope =
        aWorkerPrivate->DebuggerGlobalScope();
    MOZ_ASSERT(globalScope);

    JS::Rooted<JSString*> message(
        aCx, JS_NewUCStringCopyN(aCx, mMessage.get(), mMessage.Length()));
    if (!message) {
      return false;
    }
    JS::Rooted<JS::Value> data(aCx, JS::StringValue(message));

    RefPtr<MessageEvent> event =
        new MessageEvent(globalScope, nullptr, nullptr);
    event->InitMessageEvent(nullptr, u"message"_ns, CanBubble::eNo,
                            Cancelable::eYes, data, u""_ns, u""_ns, nullptr,
                            Sequence<OwningNonNull<MessagePort>>());
    event->SetTrusted(true);

    globalScope->DispatchEvent(*event);
    return true;
  }
};

class CompileDebuggerScriptRunnable final : public WorkerDebuggerRunnable {
  nsString mScriptURL;

 public:
  CompileDebuggerScriptRunnable(WorkerPrivate* aWorkerPrivate,
                                const nsAString& aScriptURL)
      : WorkerDebuggerRunnable(aWorkerPrivate), mScriptURL(aScriptURL) {}

 private:
  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    aWorkerPrivate->AssertIsOnWorkerThread();

    WorkerDebuggerGlobalScope* globalScope =
        aWorkerPrivate->CreateDebuggerGlobalScope(aCx);
    if (!globalScope) {
      NS_WARNING("Failed to make global!");
      return false;
    }

    if (NS_WARN_IF(!aWorkerPrivate->EnsureCSPEventListener())) {
      return false;
    }

    JS::Rooted<JSObject*> global(aCx, globalScope->GetWrapper());

    ErrorResult rv;
    JSAutoRealm ar(aCx, global);
    workerinternals::LoadMainScript(aWorkerPrivate, nullptr, mScriptURL,
                                    DebuggerScript, rv);
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
    // reported after we return.  We do this for all failures on rv, because now
    // we're using rv to track all the state we care about.
    if (rv.MaybeSetPendingException(aCx)) {
      return false;
    }

    return true;
  }
};

}  // namespace

class WorkerDebugger::PostDebuggerMessageRunnable final : public Runnable {
  WorkerDebugger* mDebugger;
  nsString mMessage;

 public:
  PostDebuggerMessageRunnable(WorkerDebugger* aDebugger,
                              const nsAString& aMessage)
      : mozilla::Runnable("PostDebuggerMessageRunnable"),
        mDebugger(aDebugger),
        mMessage(aMessage) {}

 private:
  ~PostDebuggerMessageRunnable() = default;

  NS_IMETHOD
  Run() override {
    mDebugger->PostMessageToDebuggerOnMainThread(mMessage);

    return NS_OK;
  }
};

class WorkerDebugger::ReportDebuggerErrorRunnable final : public Runnable {
  WorkerDebugger* mDebugger;
  nsString mFilename;
  uint32_t mLineno;
  nsString mMessage;

 public:
  ReportDebuggerErrorRunnable(WorkerDebugger* aDebugger,
                              const nsAString& aFilename, uint32_t aLineno,
                              const nsAString& aMessage)
      : Runnable("ReportDebuggerErrorRunnable"),
        mDebugger(aDebugger),
        mFilename(aFilename),
        mLineno(aLineno),
        mMessage(aMessage) {}

 private:
  ~ReportDebuggerErrorRunnable() = default;

  NS_IMETHOD
  Run() override {
    mDebugger->ReportErrorToDebuggerOnMainThread(mFilename, mLineno, mMessage);

    return NS_OK;
  }
};

WorkerDebugger::WorkerDebugger(WorkerPrivate* aWorkerPrivate)
    : mWorkerPrivate(aWorkerPrivate), mIsInitialized(false) {
  AssertIsOnMainThread();
}

WorkerDebugger::~WorkerDebugger() {
  MOZ_ASSERT(!mWorkerPrivate);

  if (!NS_IsMainThread()) {
    for (auto& listener : mListeners) {
      NS_ReleaseOnMainThread("WorkerDebugger::mListeners", listener.forget());
    }
  }
}

NS_IMPL_ISUPPORTS(WorkerDebugger, nsIWorkerDebugger)

NS_IMETHODIMP
WorkerDebugger::GetIsClosed(bool* aResult) {
  AssertIsOnMainThread();

  *aResult = !mWorkerPrivate;
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetIsChrome(bool* aResult) {
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = mWorkerPrivate->IsChromeWorker();
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetIsInitialized(bool* aResult) {
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = mIsInitialized;
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetParent(nsIWorkerDebugger** aResult) {
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  WorkerPrivate* parent = mWorkerPrivate->GetParent();
  if (!parent) {
    *aResult = nullptr;
    return NS_OK;
  }

  MOZ_ASSERT(mWorkerPrivate->IsDedicatedWorker());

  nsCOMPtr<nsIWorkerDebugger> debugger = parent->Debugger();
  debugger.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetType(uint32_t* aResult) {
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = mWorkerPrivate->Type();
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetUrl(nsAString& aResult) {
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  aResult = mWorkerPrivate->ScriptURL();
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetWindow(mozIDOMWindow** aResult) {
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = DedicatedWorkerWindow();
  window.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetWindowIDs(nsTArray<uint64_t>& aResult) {
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mWorkerPrivate->IsDedicatedWorker()) {
    if (const auto window = DedicatedWorkerWindow()) {
      aResult.AppendElement(window->WindowID());
    }
  } else if (mWorkerPrivate->IsSharedWorker()) {
    const RemoteWorkerChild* const controller =
        mWorkerPrivate->GetRemoteWorkerController();
    MOZ_ASSERT(controller);
    aResult = controller->WindowIDs().Clone();
  }

  return NS_OK;
}

nsCOMPtr<nsPIDOMWindowInner> WorkerDebugger::DedicatedWorkerWindow() {
  MOZ_ASSERT(mWorkerPrivate);

  WorkerPrivate* worker = mWorkerPrivate;
  while (worker->GetParent()) {
    worker = worker->GetParent();
  }

  if (!worker->IsDedicatedWorker()) {
    return nullptr;
  }

  return worker->GetWindow();
}

NS_IMETHODIMP
WorkerDebugger::GetPrincipal(nsIPrincipal** aResult) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aResult);

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  nsCOMPtr<nsIPrincipal> prin = mWorkerPrivate->GetPrincipal();
  prin.forget(aResult);

  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetServiceWorkerID(uint32_t* aResult) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aResult);

  if (!mWorkerPrivate || !mWorkerPrivate->IsServiceWorker()) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = mWorkerPrivate->ServiceWorkerID();
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetId(nsAString& aResult) {
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  aResult = mWorkerPrivate->Id();
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::Initialize(const nsAString& aURL) {
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mIsInitialized) {
    RefPtr<CompileDebuggerScriptRunnable> runnable =
        new CompileDebuggerScriptRunnable(mWorkerPrivate, aURL);
    if (!runnable->Dispatch()) {
      return NS_ERROR_FAILURE;
    }

    mIsInitialized = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::PostMessageMoz(const nsAString& aMessage) {
  AssertIsOnMainThread();

  if (!mWorkerPrivate || !mIsInitialized) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<DebuggerMessageEventRunnable> runnable =
      new DebuggerMessageEventRunnable(mWorkerPrivate, aMessage);
  if (!runnable->Dispatch()) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::AddListener(nsIWorkerDebuggerListener* aListener) {
  AssertIsOnMainThread();

  if (mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.AppendElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::RemoveListener(nsIWorkerDebuggerListener* aListener) {
  AssertIsOnMainThread();

  if (!mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.RemoveElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::SetDebuggerReady(bool aReady) {
  return mWorkerPrivate->SetIsDebuggerReady(aReady);
}

void WorkerDebugger::Close() {
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate = nullptr;

  for (const auto& listener : mListeners.Clone()) {
    listener->OnClose();
  }
}

void WorkerDebugger::PostMessageToDebugger(const nsAString& aMessage) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<PostDebuggerMessageRunnable> runnable =
      new PostDebuggerMessageRunnable(this, aMessage);
  if (NS_FAILED(mWorkerPrivate->DispatchToMainThreadForMessaging(
          runnable.forget()))) {
    NS_WARNING("Failed to post message to debugger on main thread!");
  }
}

void WorkerDebugger::PostMessageToDebuggerOnMainThread(
    const nsAString& aMessage) {
  AssertIsOnMainThread();

  for (const auto& listener : mListeners.Clone()) {
    listener->OnMessage(aMessage);
  }
}

void WorkerDebugger::ReportErrorToDebugger(const nsAString& aFilename,
                                           uint32_t aLineno,
                                           const nsAString& aMessage) {
  mWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<ReportDebuggerErrorRunnable> runnable =
      new ReportDebuggerErrorRunnable(this, aFilename, aLineno, aMessage);
  if (NS_FAILED(mWorkerPrivate->DispatchToMainThreadForMessaging(
          runnable.forget()))) {
    NS_WARNING("Failed to report error to debugger on main thread!");
  }
}

void WorkerDebugger::ReportErrorToDebuggerOnMainThread(
    const nsAString& aFilename, uint32_t aLineno, const nsAString& aMessage) {
  AssertIsOnMainThread();

  for (const auto& listener : mListeners.Clone()) {
    listener->OnError(aFilename, aLineno, aMessage);
  }

  AutoJSAPI jsapi;
  // We're only using this context to deserialize a stack to report to the
  // console, so the scope we use doesn't matter. Stack frame filtering happens
  // based on the principal encoded into the frame and the caller compartment,
  // not the compartment of the frame object, and the console reporting code
  // will not be using our context, and therefore will not care what compartment
  // it has entered.
  DebugOnly<bool> ok = jsapi.Init(xpc::PrivilegedJunkScope());
  MOZ_ASSERT(ok, "PrivilegedJunkScope should exist");

  WorkerErrorReport report;
  report.mMessage = aMessage;
  report.mFilename = aFilename;
  WorkerErrorReport::LogErrorToConsole(jsapi.cx(), report, 0);
}

RefPtr<PerformanceInfoPromise> WorkerDebugger::ReportPerformanceInfo() {
  AssertIsOnMainThread();
  RefPtr<BrowsingContext> top;
  RefPtr<WorkerDebugger> self = this;

#if defined(XP_WIN)
  uint32_t pid = GetCurrentProcessId();
#else
  uint32_t pid = getpid();
#endif
  bool isTopLevel = false;
  uint64_t windowID = mWorkerPrivate->WindowID();
  PerformanceMemoryInfo memoryInfo;

  // Walk up to our containing page and its window
  WorkerPrivate* wp = mWorkerPrivate;
  while (wp->GetParent()) {
    wp = wp->GetParent();
  }
  nsPIDOMWindowInner* win = wp->GetWindow();
  if (win) {
    BrowsingContext* context = win->GetBrowsingContext();
    if (context) {
      top = context->Top();
      if (top && top->GetCurrentWindowContext()) {
        windowID = top->GetCurrentWindowContext()->OuterWindowId();
        isTopLevel = context->IsTop();
      }
    }
  }

  // getting the worker URL
  RefPtr<nsIURI> scriptURI = mWorkerPrivate->GetResolvedScriptURI();
  if (NS_WARN_IF(!scriptURI)) {
    // This can happen at shutdown, let's stop here.
    return PerformanceInfoPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }
  nsCString url = scriptURI->GetSpecOrDefault();

  const auto& perf = mWorkerPrivate->PerformanceCounterRef();
  uint64_t perfId = perf.GetID();
  uint16_t count = perf.GetTotalDispatchCount();
  uint64_t duration = perf.GetExecutionDuration();

  // Workers only produce metrics for a single category -
  // DispatchCategory::Worker. We still return an array of CategoryDispatch so
  // the PerformanceInfo struct is common to all performance counters throughout
  // Firefox.
  FallibleTArray<CategoryDispatch> items;

  CategoryDispatch item =
      CategoryDispatch(DispatchCategory::Worker.GetValue(), count);
  if (!items.AppendElement(item, fallible)) {
    NS_ERROR("Could not complete the operation");
  }

  if (!isTopLevel) {
    return PerformanceInfoPromise::CreateAndResolve(
        PerformanceInfo(url, pid, windowID, duration, perfId, true, isTopLevel,
                        memoryInfo, items),
        __func__);
  }

  // We need to keep a ref on workerPrivate, passed to the promise,
  // to make sure it's still aloive when collecting the info.
  RefPtr<WorkerPrivate> workerRef = mWorkerPrivate;
  RefPtr<AbstractThread> mainThread = AbstractThread::MainThread();

  return CollectMemoryInfo(top, mainThread)
      ->Then(
          mainThread, __func__,
          [workerRef, url, pid, perfId, windowID, duration, isTopLevel,
           items = std::move(items)](const PerformanceMemoryInfo& aMemoryInfo) {
            return PerformanceInfoPromise::CreateAndResolve(
                PerformanceInfo(url, pid, windowID, duration, perfId, true,
                                isTopLevel, aMemoryInfo, items),
                __func__);
          },
          [workerRef]() {
            return PerformanceInfoPromise::CreateAndReject(NS_ERROR_FAILURE,
                                                           __func__);
          });
}

}  // namespace dom
}  // namespace mozilla
