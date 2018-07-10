/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerDebugger.h"

#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
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
#include <processthreadsapi.h>  // for GetCurrentProcessId()
#else
#include <unistd.h> // for getpid()
#endif // defined(XP_WIN)

namespace mozilla {
namespace dom {

namespace {

class DebuggerMessageEventRunnable : public WorkerDebuggerRunnable
{
  nsString mMessage;

public:
  DebuggerMessageEventRunnable(WorkerPrivate* aWorkerPrivate,
                               const nsAString& aMessage)
  : WorkerDebuggerRunnable(aWorkerPrivate),
    mMessage(aMessage)
  {
  }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    WorkerDebuggerGlobalScope* globalScope = aWorkerPrivate->DebuggerGlobalScope();
    MOZ_ASSERT(globalScope);

    JS::Rooted<JSString*> message(aCx, JS_NewUCStringCopyN(aCx, mMessage.get(),
                                                           mMessage.Length()));
    if (!message) {
      return false;
    }
    JS::Rooted<JS::Value> data(aCx, JS::StringValue(message));

    RefPtr<MessageEvent> event = new MessageEvent(globalScope, nullptr,
                                                  nullptr);
    event->InitMessageEvent(nullptr,
                            NS_LITERAL_STRING("message"),
                            CanBubble::eNo,
                            Cancelable::eYes,
                            data,
                            EmptyString(),
                            EmptyString(),
                            nullptr,
                            Sequence<OwningNonNull<MessagePort>>());
    event->SetTrusted(true);

    globalScope->DispatchEvent(*event);
    return true;
  }
};

class CompileDebuggerScriptRunnable final : public WorkerDebuggerRunnable
{
  nsString mScriptURL;

public:
  CompileDebuggerScriptRunnable(WorkerPrivate* aWorkerPrivate,
                                const nsAString& aScriptURL)
  : WorkerDebuggerRunnable(aWorkerPrivate),
    mScriptURL(aScriptURL)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

    WorkerDebuggerGlobalScope* globalScope =
      aWorkerPrivate->CreateDebuggerGlobalScope(aCx);
    if (!globalScope) {
      NS_WARNING("Failed to make global!");
      return false;
    }

    if (NS_WARN_IF(!aWorkerPrivate->EnsureClientSource())) {
      return false;
    }

    JS::Rooted<JSObject*> global(aCx, globalScope->GetWrapper());

    ErrorResult rv;
    JSAutoRealm ar(aCx, global);
    workerinternals::LoadMainScript(aWorkerPrivate, mScriptURL,
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

} // anonymous

class WorkerDebugger::PostDebuggerMessageRunnable final : public Runnable
{
  WorkerDebugger *mDebugger;
  nsString mMessage;

public:
  PostDebuggerMessageRunnable(WorkerDebugger* aDebugger,
                              const nsAString& aMessage)
    : mozilla::Runnable("PostDebuggerMessageRunnable")
    , mDebugger(aDebugger)
    , mMessage(aMessage)
  {
  }

private:
  ~PostDebuggerMessageRunnable()
  { }

  NS_IMETHOD
  Run() override
  {
    mDebugger->PostMessageToDebuggerOnMainThread(mMessage);

    return NS_OK;
  }
};

class WorkerDebugger::ReportDebuggerErrorRunnable final : public Runnable
{
  WorkerDebugger *mDebugger;
  nsString mFilename;
  uint32_t mLineno;
  nsString mMessage;

public:
  ReportDebuggerErrorRunnable(WorkerDebugger* aDebugger,
                              const nsAString& aFilename,
                              uint32_t aLineno,
                              const nsAString& aMessage)
    : Runnable("ReportDebuggerErrorRunnable")
    , mDebugger(aDebugger)
    , mFilename(aFilename)
    , mLineno(aLineno)
    , mMessage(aMessage)
  {
  }

private:
  ~ReportDebuggerErrorRunnable()
  { }

  NS_IMETHOD
  Run() override
  {
    mDebugger->ReportErrorToDebuggerOnMainThread(mFilename, mLineno, mMessage);

    return NS_OK;
  }
};

WorkerDebugger::WorkerDebugger(WorkerPrivate* aWorkerPrivate)
: mWorkerPrivate(aWorkerPrivate),
  mIsInitialized(false)
{
  AssertIsOnMainThread();
}

WorkerDebugger::~WorkerDebugger()
{
  MOZ_ASSERT(!mWorkerPrivate);

  if (!NS_IsMainThread()) {
    for (size_t index = 0; index < mListeners.Length(); ++index) {
      NS_ReleaseOnMainThreadSystemGroup(
        "WorkerDebugger::mListeners", mListeners[index].forget());
    }
  }
}

NS_IMPL_ISUPPORTS(WorkerDebugger, nsIWorkerDebugger)

NS_IMETHODIMP
WorkerDebugger::GetIsClosed(bool* aResult)
{
  AssertIsOnMainThread();

  *aResult = !mWorkerPrivate;
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetIsChrome(bool* aResult)
{
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = mWorkerPrivate->IsChromeWorker();
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetIsInitialized(bool* aResult)
{
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = mIsInitialized;
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetParent(nsIWorkerDebugger** aResult)
{
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
WorkerDebugger::GetType(uint32_t* aResult)
{
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = mWorkerPrivate->Type();
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetUrl(nsAString& aResult)
{
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  aResult = mWorkerPrivate->ScriptURL();
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetWindow(mozIDOMWindow** aResult)
{
  AssertIsOnMainThread();

  if (!mWorkerPrivate) {
    return NS_ERROR_UNEXPECTED;
  }

  if (mWorkerPrivate->GetParent() || !mWorkerPrivate->IsDedicatedWorker()) {
    *aResult = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowInner> window = mWorkerPrivate->GetWindow();
  window.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::GetPrincipal(nsIPrincipal** aResult)
{
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
WorkerDebugger::GetServiceWorkerID(uint32_t* aResult)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aResult);

  if (!mWorkerPrivate || !mWorkerPrivate->IsServiceWorker()) {
    return NS_ERROR_UNEXPECTED;
  }

  *aResult = mWorkerPrivate->ServiceWorkerID();
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::Initialize(const nsAString& aURL)
{
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
WorkerDebugger::PostMessageMoz(const nsAString& aMessage)
{
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
WorkerDebugger::AddListener(nsIWorkerDebuggerListener* aListener)
{
  AssertIsOnMainThread();

  if (mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.AppendElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebugger::RemoveListener(nsIWorkerDebuggerListener* aListener)
{
  AssertIsOnMainThread();

  if (!mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.RemoveElement(aListener);
  return NS_OK;
}

void
WorkerDebugger::Close()
{
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate = nullptr;

  nsTArray<nsCOMPtr<nsIWorkerDebuggerListener>> listeners(mListeners);
  for (size_t index = 0; index < listeners.Length(); ++index) {
      listeners[index]->OnClose();
  }
}

void
WorkerDebugger::PostMessageToDebugger(const nsAString& aMessage)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<PostDebuggerMessageRunnable> runnable =
    new PostDebuggerMessageRunnable(this, aMessage);
  if (NS_FAILED(mWorkerPrivate->DispatchToMainThread(runnable.forget()))) {
    NS_WARNING("Failed to post message to debugger on main thread!");
  }
}

void
WorkerDebugger::PostMessageToDebuggerOnMainThread(const nsAString& aMessage)
{
  AssertIsOnMainThread();

  nsTArray<nsCOMPtr<nsIWorkerDebuggerListener>> listeners(mListeners);
  for (size_t index = 0; index < listeners.Length(); ++index) {
    listeners[index]->OnMessage(aMessage);
  }
}

void
WorkerDebugger::ReportErrorToDebugger(const nsAString& aFilename,
                                      uint32_t aLineno,
                                      const nsAString& aMessage)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<ReportDebuggerErrorRunnable> runnable =
    new ReportDebuggerErrorRunnable(this, aFilename, aLineno, aMessage);
  if (NS_FAILED(mWorkerPrivate->DispatchToMainThread(runnable.forget()))) {
    NS_WARNING("Failed to report error to debugger on main thread!");
  }
}

void
WorkerDebugger::ReportErrorToDebuggerOnMainThread(const nsAString& aFilename,
                                                  uint32_t aLineno,
                                                  const nsAString& aMessage)
{
  AssertIsOnMainThread();

  nsTArray<nsCOMPtr<nsIWorkerDebuggerListener>> listeners(mListeners);
  for (size_t index = 0; index < listeners.Length(); ++index) {
    listeners[index]->OnError(aFilename, aLineno, aMessage);
  }

  WorkerErrorReport report;
  report.mMessage = aMessage;
  report.mFilename = aFilename;
  WorkerErrorReport::LogErrorToConsole(report, 0);
}

PerformanceInfo
WorkerDebugger::ReportPerformanceInfo()
{
  AssertIsOnMainThread();

#if defined(XP_WIN)
  uint32_t pid = GetCurrentProcessId();
#else
  uint32_t pid = getpid();
#endif
  bool isTopLevel= false;
  uint64_t pwid = 0;
  nsPIDOMWindowInner* win = mWorkerPrivate->GetWindow();
  if (win) {
    nsPIDOMWindowOuter* outer = win->GetOuterWindow();
    if (outer) {
      nsCOMPtr<nsPIDOMWindowOuter> top = outer->GetTop();
      if (top) {
        pwid = top->WindowID();
        isTopLevel = pwid == mWorkerPrivate->WindowID();
      }
    }
  }


  // Workers only produce metrics for a single category - DispatchCategory::Worker.
  // We still return an array of CategoryDispatch so the PerformanceInfo
  // struct is common to all performance counters throughout Firefox.
  FallibleTArray<CategoryDispatch> items;
  uint64_t duration = 0;
  uint16_t count = 0;
  RefPtr<nsIURI> uri = mWorkerPrivate->GetResolvedScriptURI();

  RefPtr<PerformanceCounter> perf = mWorkerPrivate->GetPerformanceCounter();
  if (perf) {
    count =  perf->GetTotalDispatchCount();
    duration = perf->GetExecutionDuration();
    CategoryDispatch item = CategoryDispatch(DispatchCategory::Worker.GetValue(), count);
    if (!items.AppendElement(item, fallible)) {
      NS_ERROR("Could not complete the operation");
      return PerformanceInfo(uri->GetSpecOrDefault(), pid, pwid, duration,
                            true, isTopLevel, items);
    }
    perf->ResetPerformanceCounters();
  }

  return PerformanceInfo(uri->GetSpecOrDefault(), pid, pwid, duration,
                         true, isTopLevel, items);
}

} // dom namespace
} // mozilla namespace
