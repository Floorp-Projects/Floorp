/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerPrivate.h"

#include "amIAddonManager.h"
#include "nsIClassInfo.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIConsoleService.h"
#include "nsIDOMDOMException.h"
#include "nsIDOMEvent.h"
#include "nsIDocument.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestor.h"
#include "nsIMemoryReporter.h"
#include "nsINetworkInterceptController.h"
#include "nsIPermissionManager.h"
#include "nsIScriptError.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptTimeoutHandler.h"
#include "nsITabChild.h"
#include "nsITextToSubURI.h"
#include "nsIThreadInternal.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIWorkerDebugger.h"
#include "nsIXPConnect.h"
#include "nsPIDOMWindow.h"
#include "nsGlobalWindow.h"

#include <algorithm>
#include "ImageContainer.h"
#include "jsfriendapi.h"
#include "js/MemoryMetrics.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Likely.h"
#include "mozilla/LoadContext.h"
#include "mozilla/Move.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Console.h"
#include "mozilla/dom/DocGroup.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/ExtendableMessageEventBinding.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/IndexedDatabaseManager.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/dom/nsCSPUtils.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/PMessagePort.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseDebugging.h"
#include "mozilla/dom/PromiseNativeHandler.h"
#include "mozilla/dom/SimpleGlobalObject.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/dom/WorkerDebuggerGlobalScopeBinding.h"
#include "mozilla/dom/WorkerGlobalScopeBinding.h"
#include "mozilla/Preferences.h"
#include "mozilla/SizePrintfMacros.h"
#include "mozilla/ThrottledEventQueue.h"
#include "mozilla/TimelineConsumers.h"
#include "mozilla/WorkerTimelineMarker.h"
#include "nsAlgorithm.h"
#include "nsContentUtils.h"
#include "nsCycleCollector.h"
#include "nsError.h"
#include "nsDOMJSUtils.h"
#include "nsHostObjectProtocolHandler.h"
#include "nsJSEnvironment.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "nsSandboxFlags.h"
#include "nsScriptError.h"
#include "nsUTF8Utils.h"
#include "prthread.h"
#include "xpcpublic.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#ifdef DEBUG
#include "nsThreadManager.h"
#endif

#include "Navigator.h"
#include "Principal.h"
#include "RuntimeService.h"
#include "ScriptLoader.h"
#include "ServiceWorkerEvents.h"
#include "ServiceWorkerManager.h"
#include "SharedWorker.h"
#include "WorkerDebuggerManager.h"
#include "WorkerHolder.h"
#include "WorkerNavigator.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "WorkerThread.h"

// JS_MaybeGC will run once every second during normal execution.
#define PERIODIC_GC_TIMER_DELAY_SEC 1

// A shrinking GC will run five seconds after the last event is processed.
#define IDLE_GC_TIMER_DELAY_SEC 5

#define PREF_WORKERS_ENABLED "dom.workers.enabled"

static mozilla::LazyLogModule sWorkerPrivateLog("WorkerPrivate");
static mozilla::LazyLogModule sWorkerTimeoutsLog("WorkerTimeouts");

mozilla::LogModule*
WorkerLog()
{
  return sWorkerPrivateLog;
}

mozilla::LogModule*
TimeoutsLog()
{
  return sWorkerTimeoutsLog;
}

#define LOG(log, _args) MOZ_LOG(log, LogLevel::Debug, _args);

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::ipc;

USING_WORKERS_NAMESPACE

MOZ_DEFINE_MALLOC_SIZE_OF(JsWorkerMallocSizeOf)

#ifdef DEBUG

BEGIN_WORKERS_NAMESPACE

void
AssertIsOnMainThread()
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");
}

END_WORKERS_NAMESPACE

#endif

namespace {

#ifdef DEBUG

const nsIID kDEBUGWorkerEventTargetIID = {
  0xccaba3fa, 0x5be2, 0x4de2, { 0xba, 0x87, 0x3b, 0x3b, 0x5b, 0x1d, 0x5, 0xfb }
};

#endif

template <class T>
class AutoPtrComparator
{
  typedef nsAutoPtr<T> A;
  typedef T* B;

public:
  bool Equals(const A& a, const B& b) const {
    return a && b ? *a == *b : !a && !b ? true : false;
  }
  bool LessThan(const A& a, const B& b) const {
    return a && b ? *a < *b : b ? true : false;
  }
};

template <class T>
inline AutoPtrComparator<T>
GetAutoPtrComparator(const nsTArray<nsAutoPtr<T> >&)
{
  return AutoPtrComparator<T>();
}

// Specialize this if there's some class that has multiple nsISupports bases.
template <class T>
struct ISupportsBaseInfo
{
  typedef T ISupportsBase;
};

template <template <class> class SmartPtr, class T>
inline void
SwapToISupportsArray(SmartPtr<T>& aSrc,
                     nsTArray<nsCOMPtr<nsISupports> >& aDest)
{
  nsCOMPtr<nsISupports>* dest = aDest.AppendElement();

  T* raw = nullptr;
  aSrc.swap(raw);

  nsISupports* rawSupports =
    static_cast<typename ISupportsBaseInfo<T>::ISupportsBase*>(raw);
  dest->swap(rawSupports);
}

// This class is used to wrap any runnables that the worker receives via the
// nsIEventTarget::Dispatch() method (either from NS_DispatchToCurrentThread or
// from the worker's EventTarget).
class ExternalRunnableWrapper final : public WorkerRunnable
{
  nsCOMPtr<nsIRunnable> mWrappedRunnable;

public:
  ExternalRunnableWrapper(WorkerPrivate* aWorkerPrivate,
                          nsIRunnable* aWrappedRunnable)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mWrappedRunnable(aWrappedRunnable)
  {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aWrappedRunnable);
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  ~ExternalRunnableWrapper()
  { }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    nsresult rv = mWrappedRunnable->Run();
    if (NS_FAILED(rv)) {
      if (!JS_IsExceptionPending(aCx)) {
        Throw(aCx, rv);
      }
      return false;
    }
    return true;
  }

  nsresult
  Cancel() override
  {
    nsresult rv;
    nsCOMPtr<nsICancelableRunnable> cancelable =
      do_QueryInterface(mWrappedRunnable);
    MOZ_ASSERT(cancelable); // We checked this earlier!
    rv = cancelable->Cancel();
    nsresult rv2 = WorkerRunnable::Cancel();
    return NS_FAILED(rv) ? rv : rv2;
  }
};

struct WindowAction
{
  nsPIDOMWindowInner* mWindow;
  bool mDefaultAction;

  MOZ_IMPLICIT WindowAction(nsPIDOMWindowInner* aWindow)
  : mWindow(aWindow), mDefaultAction(true)
  { }

  bool
  operator==(const WindowAction& aOther) const
  {
    return mWindow == aOther.mWindow;
  }
};

void
LogErrorToConsole(const WorkerErrorReport& aReport, uint64_t aInnerWindowId)
{
  AssertIsOnMainThread();

  RefPtr<nsScriptErrorBase> scriptError = new nsScriptError();
  NS_WARNING_ASSERTION(scriptError, "Failed to create script error!");

  if (scriptError) {
    nsAutoCString category("Web Worker");
    if (NS_FAILED(scriptError->InitWithWindowID(aReport.mMessage,
                                                aReport.mFilename,
                                                aReport.mLine,
                                                aReport.mLineNumber,
                                                aReport.mColumnNumber,
                                                aReport.mFlags,
                                                category,
                                                aInnerWindowId))) {
      NS_WARNING("Failed to init script error!");
      scriptError = nullptr;
    }

    for (size_t i = 0, len = aReport.mNotes.Length(); i < len; i++) {
      const WorkerErrorNote& note = aReport.mNotes.ElementAt(i);

      nsScriptErrorNote* noteObject = new nsScriptErrorNote();
      noteObject->Init(note.mMessage, note.mFilename,
                       note.mLineNumber, note.mColumnNumber);
      scriptError->AddNote(noteObject);
    }
  }

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  NS_WARNING_ASSERTION(consoleService, "Failed to get console service!");

  if (consoleService) {
    if (scriptError) {
      if (NS_SUCCEEDED(consoleService->LogMessage(scriptError))) {
        return;
      }
      NS_WARNING("LogMessage failed!");
    } else if (NS_SUCCEEDED(consoleService->LogStringMessage(
                              aReport.mMessage.BeginReading()))) {
      return;
    }
    NS_WARNING("LogStringMessage failed!");
  }

  NS_ConvertUTF16toUTF8 msg(aReport.mMessage);
  NS_ConvertUTF16toUTF8 filename(aReport.mFilename);

  static const char kErrorString[] = "JS error in Web Worker: %s [%s:%u]";

#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", kErrorString, msg.get(),
                      filename.get(), aReport.mLineNumber);
#endif

  fprintf(stderr, kErrorString, msg.get(), filename.get(), aReport.mLineNumber);
  fflush(stderr);
}

class MainThreadReleaseRunnable final : public Runnable
{
  nsTArray<nsCOMPtr<nsISupports>> mDoomed;
  nsCOMPtr<nsILoadGroup> mLoadGroupToCancel;

public:
  MainThreadReleaseRunnable(nsTArray<nsCOMPtr<nsISupports>>& aDoomed,
                            nsCOMPtr<nsILoadGroup>& aLoadGroupToCancel)
  {
    mDoomed.SwapElements(aDoomed);
    mLoadGroupToCancel.swap(aLoadGroupToCancel);
  }

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD
  Run() override
  {
    if (mLoadGroupToCancel) {
      mLoadGroupToCancel->Cancel(NS_BINDING_ABORTED);
      mLoadGroupToCancel = nullptr;
    }

    mDoomed.Clear();
    return NS_OK;
  }

private:
  ~MainThreadReleaseRunnable()
  { }
};

class WorkerFinishedRunnable final : public WorkerControlRunnable
{
  WorkerPrivate* mFinishedWorker;

public:
  WorkerFinishedRunnable(WorkerPrivate* aWorkerPrivate,
                         WorkerPrivate* aFinishedWorker)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mFinishedWorker(aFinishedWorker)
  { }

private:
  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    // Silence bad assertions.
    return true;
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    // Silence bad assertions.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    if (!mFinishedWorker->ProxyReleaseMainThreadObjects()) {
      NS_WARNING("Failed to dispatch, going to leak!");
    }

    RuntimeService* runtime = RuntimeService::GetService();
    NS_ASSERTION(runtime, "This should never be null!");

    mFinishedWorker->DisableDebugger();

    runtime->UnregisterWorker(mFinishedWorker);

    mFinishedWorker->ClearSelfRef();
    return true;
  }
};

class TopLevelWorkerFinishedRunnable final : public Runnable
{
  WorkerPrivate* mFinishedWorker;

public:
  explicit TopLevelWorkerFinishedRunnable(WorkerPrivate* aFinishedWorker)
  : mFinishedWorker(aFinishedWorker)
  {
    aFinishedWorker->AssertIsOnWorkerThread();
  }

  NS_DECL_ISUPPORTS_INHERITED

private:
  ~TopLevelWorkerFinishedRunnable() {}

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

    RuntimeService* runtime = RuntimeService::GetService();
    MOZ_ASSERT(runtime);

    mFinishedWorker->DisableDebugger();

    runtime->UnregisterWorker(mFinishedWorker);

    if (!mFinishedWorker->ProxyReleaseMainThreadObjects()) {
      NS_WARNING("Failed to dispatch, going to leak!");
    }

    mFinishedWorker->ClearSelfRef();
    return NS_OK;
  }
};

class ModifyBusyCountRunnable final : public WorkerControlRunnable
{
  bool mIncrease;

public:
  ModifyBusyCountRunnable(WorkerPrivate* aWorkerPrivate, bool aIncrease)
  : WorkerControlRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount),
    mIncrease(aIncrease)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return aWorkerPrivate->ModifyBusyCount(mIncrease);
  }

  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
          override
  {
    if (mIncrease) {
      WorkerControlRunnable::PostRun(aCx, aWorkerPrivate, aRunResult);
      return;
    }
    // Don't do anything here as it's possible that aWorkerPrivate has been
    // deleted.
  }
};

class ReportCompileErrorRunnable final : public WorkerRunnable
{
public:
  static void
  CreateAndDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    RefPtr<ReportCompileErrorRunnable> runnable =
      new ReportCompileErrorRunnable(aCx, aWorkerPrivate);
    runnable->Dispatch();
  }

private:
  ReportCompileErrorRunnable(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    : WorkerRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

    // Dispatch may fail if the worker was canceled, no need to report that as
    // an error, so don't call base class PostDispatch.
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    if (aWorkerPrivate->IsFrozen() ||
        aWorkerPrivate->IsParentWindowPaused()) {
      MOZ_ASSERT(!IsDebuggerRunnable());
      aWorkerPrivate->QueueRunnable(this);
      return true;
    }

    if (aWorkerPrivate->IsSharedWorker()) {
      aWorkerPrivate->BroadcastErrorToSharedWorkers(aCx, nullptr,
                                                    /* isErrorEvent */ false);
      return true;
    }

    if (aWorkerPrivate->IsServiceWorker()) {
      RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
      if (swm) {
        swm->HandleError(aCx, aWorkerPrivate->GetPrincipal(),
                         aWorkerPrivate->ServiceWorkerScope(),
                         aWorkerPrivate->ScriptURL(),
                         EmptyString(), EmptyString(), EmptyString(),
                         0, 0, JSREPORT_ERROR, JSEXN_ERR);
      }
      return true;
    }

    if (!aWorkerPrivate->IsAcceptingEvents()) {
      return true;
    }

    RefPtr<Event> event =
      Event::Constructor(aWorkerPrivate, NS_LITERAL_STRING("error"),
                         EventInit());
    event->SetTrusted(true);

    nsEventStatus status = nsEventStatus_eIgnore;
    aWorkerPrivate->DispatchDOMEvent(nullptr, event, nullptr, &status);
    return true;
  }
};

class CompileScriptRunnable final : public WorkerRunnable
{
  nsString mScriptURL;

public:
  explicit CompileScriptRunnable(WorkerPrivate* aWorkerPrivate,
                                 const nsAString& aScriptURL)
  : WorkerRunnable(aWorkerPrivate),
    mScriptURL(aScriptURL)
  { }

private:
  // We can't implement PreRun effectively, because at the point when that would
  // run we have not yet done our load so don't know things like our final
  // principal and whatnot.

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

    ErrorResult rv;
    scriptloader::LoadMainScript(aWorkerPrivate, mScriptURL, WorkerScript, rv);
    rv.WouldReportJSException();
    // Explicitly ignore NS_BINDING_ABORTED on rv.  Or more precisely, still
    // return false and don't SetWorkerScriptExecutedSuccessfully() in that
    // case, but don't throw anything on aCx.  The idea is to not dispatch error
    // events if our load is canceled with that error code.
    if (rv.ErrorCodeIs(NS_BINDING_ABORTED)) {
      rv.SuppressException();
      return false;
    }

    WorkerGlobalScope* globalScope = aWorkerPrivate->GlobalScope();
    if (NS_WARN_IF(!globalScope)) {
      // We never got as far as calling GetOrCreateGlobalScope, or it failed.
      // We have no way to enter a compartment, hence no sane way to report this
      // error.  :(
      rv.SuppressException();
      return false;
    }

    // Make sure to propagate exceptions from rv onto aCx, so that they will get
    // reported after we return.  We want to propagate just JS exceptions,
    // because all the other errors are handled when the script is loaded.
    // See: https://dom.spec.whatwg.org/#concept-event-fire
    if (rv.Failed() && !rv.IsJSException()) {
      ReportCompileErrorRunnable::CreateAndDispatch(aCx, aWorkerPrivate);
      rv.SuppressException();
      return false;
    }

    // This is a little dumb, but aCx is in the null compartment here because we
    // set it up that way in our Run(), since we had not created the global at
    // that point yet.  So we need to enter the compartment of our global,
    // because setting a pending exception on aCx involves wrapping into its
    // current compartment.  Luckily we have a global now.
    JSAutoCompartment ac(aCx, globalScope->GetGlobalJSObject());
    if (rv.MaybeSetPendingException(aCx)) {
      return false;
    }

    aWorkerPrivate->SetWorkerScriptExecutedSuccessfully();
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

    JS::Rooted<JSObject*> global(aCx, globalScope->GetWrapper());

    ErrorResult rv;
    JSAutoCompartment ac(aCx, global);
    scriptloader::LoadMainScript(aWorkerPrivate, mScriptURL,
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

class MessageEventRunnable final : public WorkerRunnable
                                 , public StructuredCloneHolder
{
public:
  MessageEventRunnable(WorkerPrivate* aWorkerPrivate,
                       TargetAndBusyBehavior aBehavior)
  : WorkerRunnable(aWorkerPrivate, aBehavior)
  , StructuredCloneHolder(CloningSupported, TransferringSupported,
                          StructuredCloneScope::SameProcessDifferentThread)
  {
  }

  bool
  DispatchDOMEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                   DOMEventTargetHelper* aTarget, bool aIsMainThread)
  {
    nsCOMPtr<nsIGlobalObject> parent = do_QueryInterface(aTarget->GetParentObject());

    // For some workers without window, parent is null and we try to find it
    // from the JS Context.
    if (!parent) {
      JS::Rooted<JSObject*> globalObject(aCx, JS::CurrentGlobalOrNull(aCx));
      if (NS_WARN_IF(!globalObject)) {
        return false;
      }

      parent = xpc::NativeGlobal(globalObject);
      if (NS_WARN_IF(!parent)) {
        return false;
      }
    }

    MOZ_ASSERT(parent);

    JS::Rooted<JS::Value> messageData(aCx);
    ErrorResult rv;

    UniquePtr<AbstractTimelineMarker> start;
    UniquePtr<AbstractTimelineMarker> end;
    RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
    bool isTimelineRecording = timelines && !timelines->IsEmpty();

    if (isTimelineRecording) {
      start = MakeUnique<WorkerTimelineMarker>(aIsMainThread
        ? ProfileTimelineWorkerOperationType::DeserializeDataOnMainThread
        : ProfileTimelineWorkerOperationType::DeserializeDataOffMainThread,
        MarkerTracingType::START);
    }

    Read(parent, aCx, &messageData, rv);

    if (isTimelineRecording) {
      end = MakeUnique<WorkerTimelineMarker>(aIsMainThread
        ? ProfileTimelineWorkerOperationType::DeserializeDataOnMainThread
        : ProfileTimelineWorkerOperationType::DeserializeDataOffMainThread,
        MarkerTracingType::END);
      timelines->AddMarkerForAllObservedDocShells(start);
      timelines->AddMarkerForAllObservedDocShells(end);
    }

    if (NS_WARN_IF(rv.Failed())) {
      xpc::Throw(aCx, rv.StealNSResult());
      return false;
    }

    Sequence<OwningNonNull<MessagePort>> ports;
    if (!TakeTransferredPortsAsSequence(ports)) {
      return false;
    }

    nsCOMPtr<nsIDOMEvent> domEvent;
    RefPtr<MessageEvent> event = new MessageEvent(aTarget, nullptr, nullptr);
    event->InitMessageEvent(nullptr,
                            NS_LITERAL_STRING("message"),
                            false /* non-bubbling */,
                            false /* cancelable */,
                            messageData,
                            EmptyString(),
                            EmptyString(),
                            nullptr,
                            ports);
    domEvent = do_QueryObject(event);

    domEvent->SetTrusted(true);

    nsEventStatus dummy = nsEventStatus_eIgnore;
    aTarget->DispatchDOMEvent(nullptr, domEvent, nullptr, &dummy);

    return true;
  }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    if (mBehavior == ParentThreadUnchangedBusyCount) {
      // Don't fire this event if the JS object has been disconnected from the
      // private object.
      if (!aWorkerPrivate->IsAcceptingEvents()) {
        return true;
      }

      if (aWorkerPrivate->IsFrozen() ||
          aWorkerPrivate->IsParentWindowPaused()) {
        MOZ_ASSERT(!IsDebuggerRunnable());
        aWorkerPrivate->QueueRunnable(this);
        return true;
      }

      aWorkerPrivate->AssertInnerWindowIsCorrect();

      return DispatchDOMEvent(aCx, aWorkerPrivate, aWorkerPrivate,
                              !aWorkerPrivate->GetParent());
    }

    MOZ_ASSERT(aWorkerPrivate == GetWorkerPrivateFromContext(aCx));

    return DispatchDOMEvent(aCx, aWorkerPrivate, aWorkerPrivate->GlobalScope(),
                            false);
  }
};

class DebuggerMessageEventRunnable : public WorkerDebuggerRunnable {
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
                            false, // canBubble
                            true, // cancelable
                            data,
                            EmptyString(),
                            EmptyString(),
                            nullptr,
                            Sequence<OwningNonNull<MessagePort>>());
    event->SetTrusted(true);

    nsCOMPtr<nsIDOMEvent> domEvent = do_QueryObject(event);
    nsEventStatus status = nsEventStatus_eIgnore;
    globalScope->DispatchDOMEvent(nullptr, domEvent, nullptr, &status);
    return true;
  }
};

class NotifyRunnable final : public WorkerControlRunnable
{
  Status mStatus;

public:
  NotifyRunnable(WorkerPrivate* aWorkerPrivate, Status aStatus)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mStatus(aStatus)
  {
    MOZ_ASSERT(aStatus == Closing || aStatus == Terminating ||
               aStatus == Canceling || aStatus == Killing);
  }

private:
  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->AssertIsOnParentThread();
    return aWorkerPrivate->ModifyBusyCount(true);
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    aWorkerPrivate->AssertIsOnParentThread();
    if (!aDispatchResult) {
      // We couldn't dispatch to the worker, which means it's already dead.
      // Undo the busy count modification.
      aWorkerPrivate->ModifyBusyCount(false);
    }
  }

  virtual void
  PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate, bool aRunResult)
          override
  {
    aWorkerPrivate->ModifyBusyCountFromWorker(false);
    return;
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    bool ok = aWorkerPrivate->NotifyInternal(aCx, mStatus);
    MOZ_ASSERT(!JS_IsExceptionPending(aCx));
    return ok;
  }
};

class FreezeRunnable final : public WorkerControlRunnable
{
public:
  explicit FreezeRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return aWorkerPrivate->FreezeInternal();
  }
};

class ThawRunnable final : public WorkerControlRunnable
{
public:
  explicit ThawRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return aWorkerPrivate->ThawInternal();
  }
};

class ReportErrorToConsoleRunnable final : public WorkerRunnable
{
  const char* mMessage;

public:
  // aWorkerPrivate is the worker thread we're on (or the main thread, if null)
  static void
  Report(WorkerPrivate* aWorkerPrivate, const char* aMessage)
  {
    if (aWorkerPrivate) {
      aWorkerPrivate->AssertIsOnWorkerThread();
    } else {
      AssertIsOnMainThread();
    }

    // Now fire a runnable to do the same on the parent's thread if we can.
    if (aWorkerPrivate) {
      RefPtr<ReportErrorToConsoleRunnable> runnable =
        new ReportErrorToConsoleRunnable(aWorkerPrivate, aMessage);
      runnable->Dispatch();
      return;
    }

    // Log a warning to the console.
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag,
                                    NS_LITERAL_CSTRING("DOM"),
                                    nullptr,
                                    nsContentUtils::eDOM_PROPERTIES,
                                    aMessage);
  }

private:
  ReportErrorToConsoleRunnable(WorkerPrivate* aWorkerPrivate, const char* aMessage)
  : WorkerRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount),
    mMessage(aMessage)
  { }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

    // Dispatch may fail if the worker was canceled, no need to report that as
    // an error, so don't call base class PostDispatch.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    WorkerPrivate* parent = aWorkerPrivate->GetParent();
    MOZ_ASSERT_IF(!parent, NS_IsMainThread());
    Report(parent, mMessage);
    return true;
  }
};

class ReportErrorRunnable final : public WorkerRunnable
{
  WorkerErrorReport mReport;

public:
  // aWorkerPrivate is the worker thread we're on (or the main thread, if null)
  // aTarget is the worker object that we are going to fire an error at
  // (if any).
  static void
  ReportError(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
              bool aFireAtScope, WorkerPrivate* aTarget,
              const WorkerErrorReport& aReport, uint64_t aInnerWindowId,
              JS::Handle<JS::Value> aException = JS::NullHandleValue)
  {
    if (aWorkerPrivate) {
      aWorkerPrivate->AssertIsOnWorkerThread();
    } else {
      AssertIsOnMainThread();
    }

    // We should not fire error events for warnings but instead make sure that
    // they show up in the error console.
    if (!JSREPORT_IS_WARNING(aReport.mFlags)) {
      // First fire an ErrorEvent at the worker.
      RootedDictionary<ErrorEventInit> init(aCx);

      if (aReport.mMutedError) {
        init.mMessage.AssignLiteral("Script error.");
      } else {
        init.mMessage = aReport.mMessage;
        init.mFilename = aReport.mFilename;
        init.mLineno = aReport.mLineNumber;
        init.mError = aException;
      }

      init.mCancelable = true;
      init.mBubbles = false;

      if (aTarget) {
        RefPtr<ErrorEvent> event =
          ErrorEvent::Constructor(aTarget, NS_LITERAL_STRING("error"), init);
        event->SetTrusted(true);

        nsEventStatus status = nsEventStatus_eIgnore;
        aTarget->DispatchDOMEvent(nullptr, event, nullptr, &status);

        if (status == nsEventStatus_eConsumeNoDefault) {
          return;
        }
      }

      // Now fire an event at the global object, but don't do that if the error
      // code is too much recursion and this is the same script threw the error.
      // XXXbz the interaction of this with worker errors seems kinda broken.
      // An overrecursion in the debugger or debugger sandbox will get turned
      // into an error event on our parent worker!
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1271441 tracks making this
      // better.
      if (aFireAtScope &&
          (aTarget || aReport.mErrorNumber != JSMSG_OVER_RECURSED)) {
        JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
        NS_ASSERTION(global, "This should never be null!");

        nsEventStatus status = nsEventStatus_eIgnore;
        nsIScriptGlobalObject* sgo;

        if (aWorkerPrivate) {
          WorkerGlobalScope* globalScope = nullptr;
          UNWRAP_OBJECT(WorkerGlobalScope, global, globalScope);

          if (!globalScope) {
            WorkerDebuggerGlobalScope* globalScope = nullptr;
            UNWRAP_OBJECT(WorkerDebuggerGlobalScope, global, globalScope);

            MOZ_ASSERT_IF(globalScope, globalScope->GetWrapperPreserveColor() == global);
            if (globalScope || IsDebuggerSandbox(global)) {
              aWorkerPrivate->ReportErrorToDebugger(aReport.mFilename, aReport.mLineNumber,
                                                    aReport.mMessage);
              return;
            }

            MOZ_ASSERT(SimpleGlobalObject::SimpleGlobalType(global) ==
                         SimpleGlobalObject::GlobalType::BindingDetail);
            // XXXbz We should really log this to console, but unwinding out of
            // this stuff without ending up firing any events is ... hard.  Just
            // return for now.
            // https://bugzilla.mozilla.org/show_bug.cgi?id=1271441 tracks
            // making this better.
            return;
          }

          MOZ_ASSERT(globalScope->GetWrapperPreserveColor() == global);
          nsIDOMEventTarget* target = static_cast<nsIDOMEventTarget*>(globalScope);

          RefPtr<ErrorEvent> event =
            ErrorEvent::Constructor(aTarget, NS_LITERAL_STRING("error"), init);
          event->SetTrusted(true);

          if (NS_FAILED(EventDispatcher::DispatchDOMEvent(target, nullptr,
                                                          event, nullptr,
                                                          &status))) {
            NS_WARNING("Failed to dispatch worker thread error event!");
            status = nsEventStatus_eIgnore;
          }
        }
        else if ((sgo = nsJSUtils::GetStaticScriptGlobal(global))) {
          MOZ_ASSERT(NS_IsMainThread());

          if (NS_FAILED(sgo->HandleScriptError(init, &status))) {
            NS_WARNING("Failed to dispatch main thread error event!");
            status = nsEventStatus_eIgnore;
          }
        }

        // Was preventDefault() called?
        if (status == nsEventStatus_eConsumeNoDefault) {
          return;
        }
      }
    }

    // Now fire a runnable to do the same on the parent's thread if we can.
    if (aWorkerPrivate) {
      RefPtr<ReportErrorRunnable> runnable =
        new ReportErrorRunnable(aWorkerPrivate, aReport);
      runnable->Dispatch();
      return;
    }

    // Otherwise log an error to the error console.
    LogErrorToConsole(aReport, aInnerWindowId);
  }

private:
  ReportErrorRunnable(WorkerPrivate* aWorkerPrivate,
                      const WorkerErrorReport& aReport)
  : WorkerRunnable(aWorkerPrivate, ParentThreadUnchangedBusyCount),
    mReport(aReport)
  { }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    aWorkerPrivate->AssertIsOnWorkerThread();

    // Dispatch may fail if the worker was canceled, no need to report that as
    // an error, so don't call base class PostDispatch.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    JS::Rooted<JSObject*> target(aCx, aWorkerPrivate->GetWrapper());

    uint64_t innerWindowId;
    bool fireAtScope = true;

    bool workerIsAcceptingEvents = aWorkerPrivate->IsAcceptingEvents();

    WorkerPrivate* parent = aWorkerPrivate->GetParent();
    if (parent) {
      innerWindowId = 0;
    }
    else {
      AssertIsOnMainThread();

      if (aWorkerPrivate->IsFrozen() ||
          aWorkerPrivate->IsParentWindowPaused()) {
        MOZ_ASSERT(!IsDebuggerRunnable());
        aWorkerPrivate->QueueRunnable(this);
        return true;
      }

      if (aWorkerPrivate->IsSharedWorker()) {
        aWorkerPrivate->BroadcastErrorToSharedWorkers(aCx, &mReport,
                                                      /* isErrorEvent */ true);
        return true;
      }

      // Service workers do not have a main thread parent global, so normal
      // worker error reporting will crash.  Instead, pass the error to
      // the ServiceWorkerManager to report on any controlled documents.
      if (aWorkerPrivate->IsServiceWorker()) {
        RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
        if (swm) {
          swm->HandleError(aCx, aWorkerPrivate->GetPrincipal(),
                           aWorkerPrivate->ServiceWorkerScope(),
                           aWorkerPrivate->ScriptURL(),
                           mReport.mMessage,
                           mReport.mFilename, mReport.mLine, mReport.mLineNumber,
                           mReport.mColumnNumber, mReport.mFlags,
                           mReport.mExnType);
        }
        return true;
      }

      // The innerWindowId is only required if we are going to ReportError
      // below, which is gated on this condition. The inner window correctness
      // check is only going to succeed when the worker is accepting events.
      if (workerIsAcceptingEvents) {
        aWorkerPrivate->AssertInnerWindowIsCorrect();
        innerWindowId = aWorkerPrivate->WindowID();
      }
    }

    // Don't fire this event if the JS object has been disconnected from the
    // private object.
    if (!workerIsAcceptingEvents) {
      return true;
    }

    ReportError(aCx, parent, fireAtScope, aWorkerPrivate, mReport,
                innerWindowId);
    return true;
  }
};

class TimerRunnable final : public WorkerRunnable,
                            public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  explicit TimerRunnable(WorkerPrivate* aWorkerPrivate)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  { }

private:
  ~TimerRunnable() {}

  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    // Silence bad assertions.
    return true;
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    // Silence bad assertions.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return aWorkerPrivate->RunExpiredTimeouts(aCx);
  }

  NS_IMETHOD
  Notify(nsITimer* aTimer) override
  {
    return Run();
  }
};

NS_IMPL_ISUPPORTS_INHERITED(TimerRunnable, WorkerRunnable, nsITimerCallback)

class DebuggerImmediateRunnable : public WorkerRunnable
{
  RefPtr<dom::Function> mHandler;

public:
  explicit DebuggerImmediateRunnable(WorkerPrivate* aWorkerPrivate,
                                     dom::Function& aHandler)
  : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mHandler(&aHandler)
  { }

private:
  virtual bool
  IsDebuggerRunnable() const override
  {
    return true;
  }

  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    // Silence bad assertions.
    return true;
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    // Silence bad assertions.
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
    JS::Rooted<JS::Value> callable(aCx, JS::ObjectOrNullValue(mHandler->CallableOrNull()));
    JS::HandleValueArray args = JS::HandleValueArray::empty();
    JS::Rooted<JS::Value> rval(aCx);
    if (!JS_CallFunctionValue(aCx, global, callable, args, &rval)) {
      // Just return false; WorkerRunnable::Run will report the exception.
      return false;
    }

    return true;
  }
};

void
PeriodicGCTimerCallback(nsITimer* aTimer, void* aClosure)
{
  auto workerPrivate = static_cast<WorkerPrivate*>(aClosure);
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();
  workerPrivate->GarbageCollectInternal(workerPrivate->GetJSContext(),
                                        false /* shrinking */,
                                        false /* collect children */);
}

void
IdleGCTimerCallback(nsITimer* aTimer, void* aClosure)
{
  auto workerPrivate = static_cast<WorkerPrivate*>(aClosure);
  MOZ_DIAGNOSTIC_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();
  workerPrivate->GarbageCollectInternal(workerPrivate->GetJSContext(),
                                        true /* shrinking */,
                                        false /* collect children */);
}

class UpdateContextOptionsRunnable final : public WorkerControlRunnable
{
  JS::ContextOptions mContextOptions;

public:
  UpdateContextOptionsRunnable(WorkerPrivate* aWorkerPrivate,
                               const JS::ContextOptions& aContextOptions)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mContextOptions(aContextOptions)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->UpdateContextOptionsInternal(aCx, mContextOptions);
    return true;
  }
};

class UpdatePreferenceRunnable final : public WorkerControlRunnable
{
  WorkerPreference mPref;
  bool mValue;

public:
  UpdatePreferenceRunnable(WorkerPrivate* aWorkerPrivate,
                           WorkerPreference aPref,
                           bool aValue)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
      mPref(aPref),
      mValue(aValue)
  { }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->UpdatePreferenceInternal(mPref, mValue);
    return true;
  }
};

class UpdateLanguagesRunnable final : public WorkerRunnable
{
  nsTArray<nsString> mLanguages;

public:
  UpdateLanguagesRunnable(WorkerPrivate* aWorkerPrivate,
                          const nsTArray<nsString>& aLanguages)
    : WorkerRunnable(aWorkerPrivate),
      mLanguages(aLanguages)
  { }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->UpdateLanguagesInternal(mLanguages);
    return true;
  }
};

class UpdateJSWorkerMemoryParameterRunnable final :
  public WorkerControlRunnable
{
  uint32_t mValue;
  JSGCParamKey mKey;

public:
  UpdateJSWorkerMemoryParameterRunnable(WorkerPrivate* aWorkerPrivate,
                                        JSGCParamKey aKey,
                                        uint32_t aValue)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mValue(aValue), mKey(aKey)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->UpdateJSWorkerMemoryParameterInternal(aCx, mKey, mValue);
    return true;
  }
};

#ifdef JS_GC_ZEAL
class UpdateGCZealRunnable final : public WorkerControlRunnable
{
  uint8_t mGCZeal;
  uint32_t mFrequency;

public:
  UpdateGCZealRunnable(WorkerPrivate* aWorkerPrivate,
                       uint8_t aGCZeal,
                       uint32_t aFrequency)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mGCZeal(aGCZeal), mFrequency(aFrequency)
  { }

private:
  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->UpdateGCZealInternal(aCx, mGCZeal, mFrequency);
    return true;
  }
};
#endif

class GarbageCollectRunnable final : public WorkerControlRunnable
{
  bool mShrinking;
  bool mCollectChildren;

public:
  GarbageCollectRunnable(WorkerPrivate* aWorkerPrivate, bool aShrinking,
                         bool aCollectChildren)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mShrinking(aShrinking), mCollectChildren(aCollectChildren)
  { }

private:
  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    // Silence bad assertions, this can be dispatched from either the main
    // thread or the timer thread..
    return true;
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    // Silence bad assertions, this can be dispatched from either the main
    // thread or the timer thread..
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->GarbageCollectInternal(aCx, mShrinking, mCollectChildren);
    return true;
  }
};

class CycleCollectRunnable : public WorkerControlRunnable
{
  bool mCollectChildren;

public:
  CycleCollectRunnable(WorkerPrivate* aWorkerPrivate, bool aCollectChildren)
  : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
    mCollectChildren(aCollectChildren)
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    aWorkerPrivate->CycleCollectInternal(mCollectChildren);
    return true;
  }
};

class OfflineStatusChangeRunnable : public WorkerRunnable
{
public:
  OfflineStatusChangeRunnable(WorkerPrivate* aWorkerPrivate, bool aIsOffline)
    : WorkerRunnable(aWorkerPrivate),
      mIsOffline(aIsOffline)
  {
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    aWorkerPrivate->OfflineStatusChangeEventInternal(mIsOffline);
    return true;
  }

private:
  bool mIsOffline;
};

class MemoryPressureRunnable : public WorkerControlRunnable
{
public:
  explicit MemoryPressureRunnable(WorkerPrivate* aWorkerPrivate)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  {}

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
  {
    aWorkerPrivate->MemoryPressureInternal();
    return true;
  }
};

#ifdef DEBUG
static bool
StartsWithExplicit(nsACString& s)
{
    return StringBeginsWith(s, NS_LITERAL_CSTRING("explicit/"));
}
#endif

class MessagePortRunnable final : public WorkerRunnable
{
  MessagePortIdentifier mPortIdentifier;

public:
  MessagePortRunnable(WorkerPrivate* aWorkerPrivate, MessagePort* aPort)
  : WorkerRunnable(aWorkerPrivate)
  {
    MOZ_ASSERT(aPort);
    // In order to move the port from one thread to another one, we have to
    // close and disentangle it. The output will be a MessagePortIdentifier that
    // will be used to recreate a new MessagePort on the other thread.
    aPort->CloneAndDisentangle(mPortIdentifier);
  }

private:
  ~MessagePortRunnable()
  { }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    return aWorkerPrivate->ConnectMessagePort(aCx, mPortIdentifier);
  }

  nsresult
  Cancel() override
  {
    MessagePort::ForceClose(mPortIdentifier);
    return WorkerRunnable::Cancel();
  }
};

class DummyRunnable final
  : public WorkerRunnable
{
public:
  explicit
  DummyRunnable(WorkerPrivate* aWorkerPrivate)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

private:
  ~DummyRunnable()
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT_UNREACHABLE("Should never call Dispatch on this!");
    return true;
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    MOZ_ASSERT_UNREACHABLE("Should never call Dispatch on this!");
  }

  virtual bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    // Do nothing.
    return true;
  }
};

PRThread*
PRThreadFromThread(nsIThread* aThread)
{
  MOZ_ASSERT(aThread);

  PRThread* result;
  MOZ_ALWAYS_SUCCEEDS(aThread->GetPRThread(&result));
  MOZ_ASSERT(result);

  return result;
}

class SimpleWorkerHolder final : public WorkerHolder
{
public:
  virtual bool Notify(Status aStatus) { return true; }
};

} /* anonymous namespace */

NS_IMPL_ISUPPORTS_INHERITED0(MainThreadReleaseRunnable, Runnable)

NS_IMPL_ISUPPORTS_INHERITED0(TopLevelWorkerFinishedRunnable, Runnable)

namespace {

class WrappedControlRunnable final : public WorkerControlRunnable
{
  nsCOMPtr<nsIRunnable> mInner;

  ~WrappedControlRunnable()
  {
  }

public:
  WrappedControlRunnable(WorkerPrivate* aWorkerPrivate,
                         already_AddRefed<nsIRunnable>&& aInner)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    , mInner(aInner)
  {
  }

  virtual bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    // Silence bad assertions, this can be dispatched from any thread.
    return true;
  }

  virtual void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    // Silence bad assertions, this can be dispatched from any thread.
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    mInner->Run();
    return true;
  }

  nsresult
  Cancel() override
  {
    // First run the default cancelation code
    WorkerControlRunnable::Cancel();

    // Attempt to cancel the inner runnable as well
    nsCOMPtr<nsICancelableRunnable> cr = do_QueryInterface(mInner);
    if (cr) {
      return cr->Cancel();
    }
    return NS_OK;
  }
};

} // anonymous namespace

BEGIN_WORKERS_NAMESPACE

class WorkerControlEventTarget final : public nsIEventTarget
{
  mozilla::Mutex mMutex;
  WorkerPrivate* mWorkerPrivate;

  ~WorkerControlEventTarget() = default;

public:
  explicit WorkerControlEventTarget(WorkerPrivate* aWorkerPrivate)
    : mMutex("WorkerControlEventTarget")
    , mWorkerPrivate(aWorkerPrivate)
  {
    MOZ_DIAGNOSTIC_ASSERT(mWorkerPrivate);
  }

  void
  ForgetWorkerPrivate(WorkerPrivate* aWorkerPrivate)
  {
    MutexAutoLock lock(mMutex);
    MOZ_DIAGNOSTIC_ASSERT(mWorkerPrivate == aWorkerPrivate);
    mWorkerPrivate = nullptr;
  }

  NS_IMETHOD
  DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags) override
  {
    nsCOMPtr<nsIRunnable> runnable(aRunnable);
    return Dispatch(runnable.forget(), aFlags);
  }

  NS_IMETHOD
  Dispatch(already_AddRefed<nsIRunnable> aRunnable, uint32_t aFlags = NS_DISPATCH_NORMAL) override
  {
    MutexAutoLock lock(mMutex);

    if (!mWorkerPrivate) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<WorkerControlRunnable> r = new WrappedControlRunnable(mWorkerPrivate,
                                                                 Move(aRunnable));
    if (!r->Dispatch()) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  NS_IMETHOD
  DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t) override
  {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD
  IsOnCurrentThread(bool* aIsOnCurrentThread) override
  {
    MOZ_ASSERT(aIsOnCurrentThread);
    MutexAutoLock lock(mMutex);

    if (!mWorkerPrivate) {
      *aIsOnCurrentThread = false;
      return NS_OK;
    }

    return mWorkerPrivate->IsOnCurrentThread(aIsOnCurrentThread);
  }

  NS_DECL_THREADSAFE_ISUPPORTS
};

NS_IMPL_ISUPPORTS(WorkerControlEventTarget, nsIEventTarget)

END_WORKERS_NAMESPACE

WorkerLoadInfo::WorkerLoadInfo()
  : mLoadFlags(nsIRequest::LOAD_NORMAL)
  , mWindowID(UINT64_MAX)
  , mServiceWorkerID(0)
  , mReferrerPolicy(net::RP_Unset)
  , mFromWindow(false)
  , mEvalAllowed(false)
  , mReportCSPViolations(false)
  , mXHRParamsAllowed(false)
  , mPrincipalIsSystem(false)
  , mStorageAllowed(false)
  , mServiceWorkersTestingInWindow(false)
{
  MOZ_COUNT_CTOR(WorkerLoadInfo);
}

WorkerLoadInfo::~WorkerLoadInfo()
{
  MOZ_COUNT_DTOR(WorkerLoadInfo);
}

void
WorkerLoadInfo::StealFrom(WorkerLoadInfo& aOther)
{
  MOZ_ASSERT(!mBaseURI);
  aOther.mBaseURI.swap(mBaseURI);

  MOZ_ASSERT(!mResolvedScriptURI);
  aOther.mResolvedScriptURI.swap(mResolvedScriptURI);

  MOZ_ASSERT(!mPrincipal);
  aOther.mPrincipal.swap(mPrincipal);

  MOZ_ASSERT(!mScriptContext);
  aOther.mScriptContext.swap(mScriptContext);

  MOZ_ASSERT(!mWindow);
  aOther.mWindow.swap(mWindow);

  MOZ_ASSERT(!mCSP);
  aOther.mCSP.swap(mCSP);

  MOZ_ASSERT(!mChannel);
  aOther.mChannel.swap(mChannel);

  MOZ_ASSERT(!mLoadGroup);
  aOther.mLoadGroup.swap(mLoadGroup);

  MOZ_ASSERT(!mLoadFailedAsyncRunnable);
  aOther.mLoadFailedAsyncRunnable.swap(mLoadFailedAsyncRunnable);

  MOZ_ASSERT(!mInterfaceRequestor);
  aOther.mInterfaceRequestor.swap(mInterfaceRequestor);

  MOZ_ASSERT(!mPrincipalInfo);
  mPrincipalInfo = aOther.mPrincipalInfo.forget();

  mDomain = aOther.mDomain;
  mOrigin = aOther.mOrigin;
  mServiceWorkerCacheName = aOther.mServiceWorkerCacheName;
  mLoadFlags = aOther.mLoadFlags;
  mWindowID = aOther.mWindowID;
  mServiceWorkerID = aOther.mServiceWorkerID;
  mReferrerPolicy = aOther.mReferrerPolicy;
  mFromWindow = aOther.mFromWindow;
  mEvalAllowed = aOther.mEvalAllowed;
  mReportCSPViolations = aOther.mReportCSPViolations;
  mXHRParamsAllowed = aOther.mXHRParamsAllowed;
  mPrincipalIsSystem = aOther.mPrincipalIsSystem;
  mStorageAllowed = aOther.mStorageAllowed;
  mServiceWorkersTestingInWindow = aOther.mServiceWorkersTestingInWindow;
  mOriginAttributes = aOther.mOriginAttributes;
}

nsresult
WorkerLoadInfo::SetPrincipalOnMainThread(nsIPrincipal* aPrincipal,
                                         nsILoadGroup* aLoadGroup)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(aLoadGroup, aPrincipal));

  mPrincipal = aPrincipal;
  mPrincipalIsSystem = nsContentUtils::IsSystemPrincipal(aPrincipal);

  nsresult rv = aPrincipal->GetCsp(getter_AddRefs(mCSP));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mCSP) {
    mCSP->GetAllowsEval(&mReportCSPViolations, &mEvalAllowed);
    // Set ReferrerPolicy
    bool hasReferrerPolicy = false;
    uint32_t rp = mozilla::net::RP_Unset;

    rv = mCSP->GetReferrerPolicy(&rp, &hasReferrerPolicy);
    NS_ENSURE_SUCCESS(rv, rv);

    if (hasReferrerPolicy) {
      mReferrerPolicy = static_cast<net::ReferrerPolicy>(rp);
    }
  } else {
    mEvalAllowed = true;
    mReportCSPViolations = false;
  }

  mLoadGroup = aLoadGroup;

  mPrincipalInfo = new PrincipalInfo();
  mOriginAttributes = nsContentUtils::GetOriginAttributes(aLoadGroup);

  rv = PrincipalToPrincipalInfo(aPrincipal, mPrincipalInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsContentUtils::GetUTFOrigin(aPrincipal, mOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
WorkerLoadInfo::GetPrincipalAndLoadGroupFromChannel(nsIChannel* aChannel,
                                                    nsIPrincipal** aPrincipalOut,
                                                    nsILoadGroup** aLoadGroupOut)
{
  AssertIsOnMainThread();
  MOZ_DIAGNOSTIC_ASSERT(aChannel);
  MOZ_DIAGNOSTIC_ASSERT(aPrincipalOut);
  MOZ_DIAGNOSTIC_ASSERT(aLoadGroupOut);

  // Initial triggering principal should be set
  MOZ_DIAGNOSTIC_ASSERT(mPrincipal);

  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  MOZ_DIAGNOSTIC_ASSERT(ssm);

  nsCOMPtr<nsIPrincipal> channelPrincipal;
  nsresult rv = ssm->GetChannelResultPrincipal(aChannel, getter_AddRefs(channelPrincipal));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoadGroup> channelLoadGroup;
  rv = aChannel->GetLoadGroup(getter_AddRefs(channelLoadGroup));
  NS_ENSURE_SUCCESS(rv, rv);
  MOZ_ASSERT(channelLoadGroup);

  // If the load principal is the system principal then the channel
  // principal must also be the system principal (we do not allow chrome
  // code to create workers with non-chrome scripts, and if we ever decide
  // to change this we need to make sure we don't always set
  // mPrincipalIsSystem to true in WorkerPrivate::GetLoadInfo()). Otherwise
  // this channel principal must be same origin with the load principal (we
  // check again here in case redirects changed the location of the script).
  if (nsContentUtils::IsSystemPrincipal(mPrincipal)) {
    if (!nsContentUtils::IsSystemPrincipal(channelPrincipal)) {
      nsCOMPtr<nsIURI> finalURI;
      rv = NS_GetFinalChannelURI(aChannel, getter_AddRefs(finalURI));
      NS_ENSURE_SUCCESS(rv, rv);

      // See if this is a resource URI. Since JSMs usually come from
      // resource:// URIs we're currently considering all URIs with the
      // URI_IS_UI_RESOURCE flag as valid for creating privileged workers.
      bool isResource;
      rv = NS_URIChainHasFlags(finalURI,
                               nsIProtocolHandler::URI_IS_UI_RESOURCE,
                               &isResource);
      NS_ENSURE_SUCCESS(rv, rv);

      if (isResource) {
        // Assign the system principal to the resource:// worker only if it
        // was loaded from code using the system principal.
        channelPrincipal = mPrincipal;
      } else {
        return NS_ERROR_DOM_BAD_URI;
      }
    }
  }

  // The principal can change, but it should still match the original
  // load group's appId and browser element flag.
  MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(channelLoadGroup, channelPrincipal));

  channelPrincipal.forget(aPrincipalOut);
  channelLoadGroup.forget(aLoadGroupOut);

  return NS_OK;
}

nsresult
WorkerLoadInfo::SetPrincipalFromChannel(nsIChannel* aChannel)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = GetPrincipalAndLoadGroupFromChannel(aChannel,
                                                    getter_AddRefs(principal),
                                                    getter_AddRefs(loadGroup));
  NS_ENSURE_SUCCESS(rv, rv);

  return SetPrincipalOnMainThread(principal, loadGroup);
}

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
bool
WorkerLoadInfo::FinalChannelPrincipalIsValid(nsIChannel* aChannel)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIPrincipal> principal;
  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = GetPrincipalAndLoadGroupFromChannel(aChannel,
                                                    getter_AddRefs(principal),
                                                    getter_AddRefs(loadGroup));
  NS_ENSURE_SUCCESS(rv, false);


  // Verify that the channel is still a null principal.  We don't care
  // if these are the exact same null principal object, though.  From
  // the worker's perspective its the same effect.
  if (principal->GetIsNullPrincipal() && mPrincipal->GetIsNullPrincipal()) {
    return true;
  }

  // Otherwise we require exact equality.  Redirects can happen, but they
  // are not allowed to change our principal.
  if (principal->Equals(mPrincipal)) {
    return true;
  }

  return false;
}

bool
WorkerLoadInfo::PrincipalIsValid() const
{
  return mPrincipal && mPrincipalInfo &&
         mPrincipalInfo->type() != PrincipalInfo::T__None &&
         mPrincipalInfo->type() <= PrincipalInfo::T__Last;
}

bool
WorkerLoadInfo::PrincipalURIMatchesScriptURL()
{
  AssertIsOnMainThread();

  nsAutoCString scheme;
  nsresult rv = mBaseURI->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, false);

  // A system principal must either be a blob URL or a resource JSM.
  if (mPrincipal->GetIsSystemPrincipal()) {
    if (scheme == NS_LITERAL_CSTRING("blob")) {
      return true;
    }

    bool isResource = false;
    nsresult rv = NS_URIChainHasFlags(mBaseURI,
                                      nsIProtocolHandler::URI_IS_UI_RESOURCE,
                                      &isResource);
    NS_ENSURE_SUCCESS(rv, false);

    return isResource;
  }

  // A null principal can occur for a data URL worker script or a blob URL
  // worker script from a sandboxed iframe.
  if (mPrincipal->GetIsNullPrincipal()) {
    return scheme == NS_LITERAL_CSTRING("data") ||
           scheme == NS_LITERAL_CSTRING("blob");
  }

  // The principal for a blob: URL worker script does not have a matching URL.
  // This is likely a bug in our referer setting logic, but exempt it for now.
  // This is another reason we should fix bug 1340694 so that referer does not
  // depend on the principal URI.
  if (scheme == NS_LITERAL_CSTRING("blob")) {
    return true;
  }

  nsCOMPtr<nsIURI> principalURI;
  rv = mPrincipal->GetURI(getter_AddRefs(principalURI));
  NS_ENSURE_SUCCESS(rv, false);
  NS_ENSURE_TRUE(principalURI, false);

  bool equal = false;
  rv = principalURI->Equals(mBaseURI, &equal);
  NS_ENSURE_SUCCESS(rv, false);

  return equal;
}
#endif // MOZ_DIAGNOSTIC_ASSERT_ENABLED

bool
WorkerLoadInfo::ProxyReleaseMainThreadObjects(WorkerPrivate* aWorkerPrivate)
{
  nsCOMPtr<nsILoadGroup> nullLoadGroup;
  return ProxyReleaseMainThreadObjects(aWorkerPrivate, nullLoadGroup);
}

bool
WorkerLoadInfo::ProxyReleaseMainThreadObjects(WorkerPrivate* aWorkerPrivate,
                                              nsCOMPtr<nsILoadGroup>& aLoadGroupToCancel)
{

  static const uint32_t kDoomedCount = 10;
  nsTArray<nsCOMPtr<nsISupports>> doomed(kDoomedCount);

  SwapToISupportsArray(mWindow, doomed);
  SwapToISupportsArray(mScriptContext, doomed);
  SwapToISupportsArray(mBaseURI, doomed);
  SwapToISupportsArray(mResolvedScriptURI, doomed);
  SwapToISupportsArray(mPrincipal, doomed);
  SwapToISupportsArray(mChannel, doomed);
  SwapToISupportsArray(mCSP, doomed);
  SwapToISupportsArray(mLoadGroup, doomed);
  SwapToISupportsArray(mLoadFailedAsyncRunnable, doomed);
  SwapToISupportsArray(mInterfaceRequestor, doomed);
  // Before adding anything here update kDoomedCount above!

  MOZ_ASSERT(doomed.Length() == kDoomedCount);

  RefPtr<MainThreadReleaseRunnable> runnable =
    new MainThreadReleaseRunnable(doomed, aLoadGroupToCancel);
  return NS_SUCCEEDED(aWorkerPrivate->DispatchToMainThread(runnable.forget()));
}

template <class Derived>
class WorkerPrivateParent<Derived>::EventTarget final
  : public nsIEventTarget
{
  // This mutex protects mWorkerPrivate and must be acquired *before* the
  // WorkerPrivate's mutex whenever they must both be held.
  mozilla::Mutex mMutex;
  WorkerPrivate* mWorkerPrivate;
  nsIEventTarget* mWeakNestedEventTarget;
  nsCOMPtr<nsIEventTarget> mNestedEventTarget;

public:
  explicit EventTarget(WorkerPrivate* aWorkerPrivate)
  : mMutex("WorkerPrivateParent::EventTarget::mMutex"),
    mWorkerPrivate(aWorkerPrivate), mWeakNestedEventTarget(nullptr)
  {
    MOZ_ASSERT(aWorkerPrivate);
  }

  EventTarget(WorkerPrivate* aWorkerPrivate, nsIEventTarget* aNestedEventTarget)
  : mMutex("WorkerPrivateParent::EventTarget::mMutex"),
    mWorkerPrivate(aWorkerPrivate), mWeakNestedEventTarget(aNestedEventTarget),
    mNestedEventTarget(aNestedEventTarget)
  {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aNestedEventTarget);
  }

  void
  Disable()
  {
    nsCOMPtr<nsIEventTarget> nestedEventTarget;
    {
      MutexAutoLock lock(mMutex);

      MOZ_ASSERT(mWorkerPrivate);
      mWorkerPrivate = nullptr;
      mNestedEventTarget.swap(nestedEventTarget);
    }
  }

  nsIEventTarget*
  GetWeakNestedEventTarget() const
  {
    MOZ_ASSERT(mWeakNestedEventTarget);
    return mWeakNestedEventTarget;
  }

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIEVENTTARGET_FULL

private:
  ~EventTarget()
  { }
};

WorkerLoadInfo::
InterfaceRequestor::InterfaceRequestor(nsIPrincipal* aPrincipal,
                                       nsILoadGroup* aLoadGroup)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  // Look for an existing LoadContext.  This is optional and it's ok if
  // we don't find one.
  nsCOMPtr<nsILoadContext> baseContext;
  if (aLoadGroup) {
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    aLoadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
    if (callbacks) {
      callbacks->GetInterface(NS_GET_IID(nsILoadContext),
                              getter_AddRefs(baseContext));
    }
    mOuterRequestor = callbacks;
  }

  mLoadContext = new LoadContext(aPrincipal, baseContext);
}

void
WorkerLoadInfo::
InterfaceRequestor::MaybeAddTabChild(nsILoadGroup* aLoadGroup)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!aLoadGroup) {
    return;
  }

  nsCOMPtr<nsIInterfaceRequestor> callbacks;
  aLoadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
  if (!callbacks) {
    return;
  }

  nsCOMPtr<nsITabChild> tabChild;
  callbacks->GetInterface(NS_GET_IID(nsITabChild), getter_AddRefs(tabChild));
  if (!tabChild) {
    return;
  }

  // Use weak references to the tab child.  Holding a strong reference will
  // not prevent an ActorDestroy() from being called on the TabChild.
  // Therefore, we should let the TabChild destroy itself as soon as possible.
  mTabChildList.AppendElement(do_GetWeakReference(tabChild));
}

NS_IMETHODIMP
WorkerLoadInfo::
InterfaceRequestor::GetInterface(const nsIID& aIID, void** aSink)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mLoadContext);

  if (aIID.Equals(NS_GET_IID(nsILoadContext))) {
    nsCOMPtr<nsILoadContext> ref = mLoadContext;
    ref.forget(aSink);
    return NS_OK;
  }

  // If we still have an active nsITabChild, then return it.  Its possible,
  // though, that all of the TabChild objects have been destroyed.  In that
  // case we return NS_NOINTERFACE.
  if (aIID.Equals(NS_GET_IID(nsITabChild))) {
    nsCOMPtr<nsITabChild> tabChild = GetAnyLiveTabChild();
    if (!tabChild) {
      return NS_NOINTERFACE;
    }
    tabChild.forget(aSink);
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsINetworkInterceptController)) &&
      mOuterRequestor) {
    // If asked for the network intercept controller, ask the outer requestor,
    // which could be the docshell.
    return mOuterRequestor->GetInterface(aIID, aSink);
  }

  return NS_NOINTERFACE;
}

already_AddRefed<nsITabChild>
WorkerLoadInfo::
InterfaceRequestor::GetAnyLiveTabChild()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Search our list of known TabChild objects for one that still exists.
  while (!mTabChildList.IsEmpty()) {
    nsCOMPtr<nsITabChild> tabChild =
      do_QueryReferent(mTabChildList.LastElement());

    // Does this tab child still exist?  If so, return it.  We are done.  If the
    // PBrowser actor is no longer useful, don't bother returning this tab.
    if (tabChild && !static_cast<TabChild*>(tabChild.get())->IsDestroyed()) {
      return tabChild.forget();
    }

    // Otherwise remove the stale weak reference and check the next one
    mTabChildList.RemoveElementAt(mTabChildList.Length() - 1);
  }

  return nullptr;
}

NS_IMPL_ADDREF(WorkerLoadInfo::InterfaceRequestor)
NS_IMPL_RELEASE(WorkerLoadInfo::InterfaceRequestor)
NS_IMPL_QUERY_INTERFACE(WorkerLoadInfo::InterfaceRequestor, nsIInterfaceRequestor)

struct WorkerPrivate::TimeoutInfo
{
  TimeoutInfo()
  : mId(0), mIsInterval(false), mCanceled(false)
  {
    MOZ_COUNT_CTOR(mozilla::dom::workers::WorkerPrivate::TimeoutInfo);
  }

  ~TimeoutInfo()
  {
    MOZ_COUNT_DTOR(mozilla::dom::workers::WorkerPrivate::TimeoutInfo);
  }

  bool operator==(const TimeoutInfo& aOther)
  {
    return mTargetTime == aOther.mTargetTime;
  }

  bool operator<(const TimeoutInfo& aOther)
  {
    return mTargetTime < aOther.mTargetTime;
  }

  nsCOMPtr<nsIScriptTimeoutHandler> mHandler;
  mozilla::TimeStamp mTargetTime;
  mozilla::TimeDuration mInterval;
  int32_t mId;
  bool mIsInterval;
  bool mCanceled;
};

class WorkerJSContextStats final : public JS::RuntimeStats
{
  const nsCString mRtPath;

public:
  explicit WorkerJSContextStats(const nsACString& aRtPath)
  : JS::RuntimeStats(JsWorkerMallocSizeOf), mRtPath(aRtPath)
  { }

  ~WorkerJSContextStats()
  {
    for (size_t i = 0; i != zoneStatsVector.length(); i++) {
      delete static_cast<xpc::ZoneStatsExtras*>(zoneStatsVector[i].extra);
    }

    for (size_t i = 0; i != compartmentStatsVector.length(); i++) {
      delete static_cast<xpc::CompartmentStatsExtras*>(compartmentStatsVector[i].extra);
    }
  }

  const nsCString& Path() const
  {
    return mRtPath;
  }

  virtual void
  initExtraZoneStats(JS::Zone* aZone,
                     JS::ZoneStats* aZoneStats)
                     override
  {
    MOZ_ASSERT(!aZoneStats->extra);

    // ReportJSRuntimeExplicitTreeStats expects that
    // aZoneStats->extra is a xpc::ZoneStatsExtras pointer.
    xpc::ZoneStatsExtras* extras = new xpc::ZoneStatsExtras;
    extras->pathPrefix = mRtPath;
    extras->pathPrefix += nsPrintfCString("zone(0x%p)/", (void *)aZone);

    MOZ_ASSERT(StartsWithExplicit(extras->pathPrefix));

    aZoneStats->extra = extras;
  }

  virtual void
  initExtraCompartmentStats(JSCompartment* aCompartment,
                            JS::CompartmentStats* aCompartmentStats)
                            override
  {
    MOZ_ASSERT(!aCompartmentStats->extra);

    // ReportJSRuntimeExplicitTreeStats expects that
    // aCompartmentStats->extra is a xpc::CompartmentStatsExtras pointer.
    xpc::CompartmentStatsExtras* extras = new xpc::CompartmentStatsExtras;

    // This is the |jsPathPrefix|.  Each worker has exactly two compartments:
    // one for atoms, and one for everything else.
    extras->jsPathPrefix.Assign(mRtPath);
    extras->jsPathPrefix += nsPrintfCString("zone(0x%p)/",
                                            (void *)js::GetCompartmentZone(aCompartment));
    extras->jsPathPrefix += js::IsAtomsCompartment(aCompartment)
                            ? NS_LITERAL_CSTRING("compartment(web-worker-atoms)/")
                            : NS_LITERAL_CSTRING("compartment(web-worker)/");

    // This should never be used when reporting with workers (hence the "?!").
    extras->domPathPrefix.AssignLiteral("explicit/workers/?!/");

    MOZ_ASSERT(StartsWithExplicit(extras->jsPathPrefix));
    MOZ_ASSERT(StartsWithExplicit(extras->domPathPrefix));

    extras->location = nullptr;

    aCompartmentStats->extra = extras;
  }
};

class WorkerPrivate::MemoryReporter final : public nsIMemoryReporter
{
  NS_DECL_THREADSAFE_ISUPPORTS

  friend class WorkerPrivate;

  SharedMutex mMutex;
  WorkerPrivate* mWorkerPrivate;
  bool mAlreadyMappedToAddon;

public:
  explicit MemoryReporter(WorkerPrivate* aWorkerPrivate)
  : mMutex(aWorkerPrivate->mMutex), mWorkerPrivate(aWorkerPrivate),
    mAlreadyMappedToAddon(false)
  {
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  NS_IMETHOD
  CollectReports(nsIHandleReportCallback* aHandleReport,
                 nsISupports* aData, bool aAnonymize) override;

private:
  class FinishCollectRunnable;

  class CollectReportsRunnable final : public MainThreadWorkerControlRunnable
  {
    RefPtr<FinishCollectRunnable> mFinishCollectRunnable;
    const bool mAnonymize;

  public:
    CollectReportsRunnable(
      WorkerPrivate* aWorkerPrivate,
      nsIHandleReportCallback* aHandleReport,
      nsISupports* aHandlerData,
      bool aAnonymize,
      const nsACString& aPath);

  private:
    bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;

    ~CollectReportsRunnable()
    {
      if (NS_IsMainThread()) {
        mFinishCollectRunnable->Run();
        return;
      }

      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(workerPrivate);
      MOZ_ALWAYS_SUCCEEDS(
        workerPrivate->DispatchToMainThread(mFinishCollectRunnable.forget()));
    }
  };

  class FinishCollectRunnable final : public Runnable
  {
    nsCOMPtr<nsIHandleReportCallback> mHandleReport;
    nsCOMPtr<nsISupports> mHandlerData;
    const bool mAnonymize;
    bool mSuccess;

  public:
    WorkerJSContextStats mCxStats;

    explicit FinishCollectRunnable(
      nsIHandleReportCallback* aHandleReport,
      nsISupports* aHandlerData,
      bool aAnonymize,
      const nsACString& aPath);

    NS_IMETHOD Run() override;

    void SetSuccess(bool success)
    {
      mSuccess = success;
    }

  private:
    ~FinishCollectRunnable()
    {
      // mHandleReport and mHandlerData are released on the main thread.
      AssertIsOnMainThread();
    }

    FinishCollectRunnable(const FinishCollectRunnable&) = delete;
    FinishCollectRunnable& operator=(const FinishCollectRunnable&) = delete;
    FinishCollectRunnable& operator=(const FinishCollectRunnable&&) = delete;
  };

  ~MemoryReporter()
  {
  }

  void
  Disable()
  {
    // Called from WorkerPrivate::DisableMemoryReporter.
    mMutex.AssertCurrentThreadOwns();

    NS_ASSERTION(mWorkerPrivate, "Disabled more than once!");
    mWorkerPrivate = nullptr;
  }

  // Only call this from the main thread and under mMutex lock.
  void
  TryToMapAddon(nsACString &path);
};

NS_IMPL_ISUPPORTS(WorkerPrivate::MemoryReporter, nsIMemoryReporter)

NS_IMETHODIMP
WorkerPrivate::MemoryReporter::CollectReports(nsIHandleReportCallback* aHandleReport,
                                              nsISupports* aData,
                                              bool aAnonymize)
{
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

    TryToMapAddon(path);

    runnable =
      new CollectReportsRunnable(mWorkerPrivate, aHandleReport, aData, aAnonymize, path);
  }

  if (!runnable->Dispatch()) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

void
WorkerPrivate::MemoryReporter::TryToMapAddon(nsACString &path)
{
  AssertIsOnMainThread();
  mMutex.AssertCurrentThreadOwns();

  if (mAlreadyMappedToAddon || !mWorkerPrivate) {
    return;
  }

  nsCOMPtr<nsIURI> scriptURI;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(scriptURI),
                          mWorkerPrivate->ScriptURL()))) {
    return;
  }

  mAlreadyMappedToAddon = true;

  if (!XRE_IsParentProcess()) {
    // Only try to access the service from the main process.
    return;
  }

  nsAutoCString addonId;
  bool ok;
  nsCOMPtr<amIAddonManager> addonManager =
    do_GetService("@mozilla.org/addons/integration;1");

  if (!addonManager ||
      NS_FAILED(addonManager->MapURIToAddonID(scriptURI, addonId, &ok)) ||
      !ok) {
    return;
  }

  static const size_t explicitLength = strlen("explicit/");
  addonId.Insert(NS_LITERAL_CSTRING("add-ons/"), 0);
  addonId += "/";
  path.Insert(addonId, explicitLength);
}

WorkerPrivate::MemoryReporter::CollectReportsRunnable::CollectReportsRunnable(
  WorkerPrivate* aWorkerPrivate,
  nsIHandleReportCallback* aHandleReport,
  nsISupports* aHandlerData,
  bool aAnonymize,
  const nsACString& aPath)
  : MainThreadWorkerControlRunnable(aWorkerPrivate),
    mFinishCollectRunnable(
      new FinishCollectRunnable(aHandleReport, aHandlerData, aAnonymize, aPath)),
    mAnonymize(aAnonymize)
{ }

bool
WorkerPrivate::MemoryReporter::CollectReportsRunnable::WorkerRun(JSContext* aCx,
                                                                 WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnWorkerThread();

  mFinishCollectRunnable->SetSuccess(
    aWorkerPrivate->CollectRuntimeStats(&mFinishCollectRunnable->mCxStats, mAnonymize));

  return true;
}

WorkerPrivate::MemoryReporter::FinishCollectRunnable::FinishCollectRunnable(
  nsIHandleReportCallback* aHandleReport,
  nsISupports* aHandlerData,
  bool aAnonymize,
  const nsACString& aPath)
  : mHandleReport(aHandleReport),
    mHandlerData(aHandlerData),
    mAnonymize(aAnonymize),
    mSuccess(false),
    mCxStats(aPath)
{ }

NS_IMETHODIMP
WorkerPrivate::MemoryReporter::FinishCollectRunnable::Run()
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIMemoryReporterManager> manager =
    do_GetService("@mozilla.org/memory-reporter-manager;1");

  if (!manager)
    return NS_OK;

  if (mSuccess) {
    xpc::ReportJSRuntimeExplicitTreeStats(mCxStats, mCxStats.Path(),
                                          mHandleReport, mHandlerData,
                                          mAnonymize);
  }

  manager->EndReport();

  return NS_OK;
}

WorkerPrivate::SyncLoopInfo::SyncLoopInfo(EventTarget* aEventTarget)
: mEventTarget(aEventTarget), mCompleted(false), mResult(false)
#ifdef DEBUG
  , mHasRun(false)
#endif
{
}

template <class Derived>
nsIDocument*
WorkerPrivateParent<Derived>::GetDocument() const
{
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

template <class Derived>
void
WorkerPrivateParent<Derived>::SetCSP(nsIContentSecurityPolicy* aCSP)
{
  AssertIsOnMainThread();
  if (!aCSP) {
    return;
  }
  WorkerPrivate* self = ParentAsWorkerPrivate();
  aCSP->EnsureEventTarget(self->mMainThreadEventTarget);
  mLoadInfo.mCSP = aCSP;
}

template <class Derived>
nsresult
WorkerPrivateParent<Derived>::SetCSPFromHeaderValues(const nsACString& aCSPHeaderValue,
                                                     const nsACString& aCSPReportOnlyHeaderValue)
{
  AssertIsOnMainThread();
  MOZ_DIAGNOSTIC_ASSERT(!mLoadInfo.mCSP);

  NS_ConvertASCIItoUTF16 cspHeaderValue(aCSPHeaderValue);
  NS_ConvertASCIItoUTF16 cspROHeaderValue(aCSPReportOnlyHeaderValue);

  nsCOMPtr<nsIContentSecurityPolicy> csp;
  nsresult rv = mLoadInfo.mPrincipal->EnsureCSP(nullptr, getter_AddRefs(csp));
  if (!csp) {
    return NS_OK;
  }

  WorkerPrivate* self = ParentAsWorkerPrivate();
  csp->EnsureEventTarget(self->mMainThreadEventTarget);

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

  // Set ReferrerPolicy, default value is set in GetReferrerPolicy
  bool hasReferrerPolicy = false;
  uint32_t rp = mozilla::net::RP_Unset;
  rv = csp->GetReferrerPolicy(&rp, &hasReferrerPolicy);
  NS_ENSURE_SUCCESS(rv, rv);

  mLoadInfo.mCSP = csp;
  mLoadInfo.mEvalAllowed = evalAllowed;
  mLoadInfo.mReportCSPViolations = reportEvalViolations;

  if (hasReferrerPolicy) {
    mLoadInfo.mReferrerPolicy = static_cast<net::ReferrerPolicy>(rp);
  }

  return NS_OK;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::SetReferrerPolicyFromHeaderValue(
                                  const nsACString& aReferrerPolicyHeaderValue)
{
  NS_ConvertUTF8toUTF16 headerValue(aReferrerPolicyHeaderValue);

  if (headerValue.IsEmpty()) {
    return;
  }

  net::ReferrerPolicy policy =
    nsContentUtils::GetReferrerPolicyFromHeader(headerValue);
  if (policy == net::RP_Unset) {
    return;
  }

  SetReferrerPolicy(policy);
}


// Can't use NS_IMPL_CYCLE_COLLECTION_CLASS(WorkerPrivateParent) because of the
// templates.
template <class Derived>
typename WorkerPrivateParent<Derived>::cycleCollection
  WorkerPrivateParent<Derived>::_cycleCollectorGlobal =
    WorkerPrivateParent<Derived>::cycleCollection();

template <class Derived>
WorkerPrivateParent<Derived>::WorkerPrivateParent(
                                           WorkerPrivate* aParent,
                                           const nsAString& aScriptURL,
                                           bool aIsChromeWorker,
                                           WorkerType aWorkerType,
                                           const nsAString& aWorkerName,
                                           const nsACString& aServiceWorkerScope,
                                           WorkerLoadInfo& aLoadInfo)
: mMutex("WorkerPrivateParent Mutex"),
  mCondVar(mMutex, "WorkerPrivateParent CondVar"),
  mParent(aParent), mScriptURL(aScriptURL),
  mWorkerName(aWorkerName), mServiceWorkerScope(aServiceWorkerScope),
  mLoadingWorkerScript(false), mBusyCount(0), mParentWindowPausedDepth(0),
  mParentStatus(Pending), mParentFrozen(false),
  mIsChromeWorker(aIsChromeWorker), mMainThreadObjectsForgotten(false),
  mIsSecureContext(false), mWorkerType(aWorkerType),
  mCreationTimeStamp(TimeStamp::Now()),
  mCreationTimeHighRes((double)PR_Now() / PR_USEC_PER_MSEC)
{
  MOZ_ASSERT_IF(!IsDedicatedWorker(), NS_IsMainThread());

  if (aLoadInfo.mWindow) {
    AssertIsOnMainThread();
    MOZ_ASSERT(aLoadInfo.mWindow->IsInnerWindow(),
               "Should have inner window here!");
    BindToOwner(aLoadInfo.mWindow);
  }

  mLoadInfo.StealFrom(aLoadInfo);

  if (aParent) {
    aParent->AssertIsOnWorkerThread();

    // Note that this copies our parent's secure context state into mJSSettings.
    aParent->CopyJSSettings(mJSSettings);

    // And manually set our mIsSecureContext, though it's not really relevant to
    // dedicated workers...
    mIsSecureContext = aParent->IsSecureContext();
    MOZ_ASSERT_IF(mIsChromeWorker, mIsSecureContext);

    MOZ_ASSERT(IsDedicatedWorker());

    if (aParent->mParentFrozen) {
      Freeze(nullptr);
    }
  }
  else {
    AssertIsOnMainThread();

    RuntimeService::GetDefaultJSSettings(mJSSettings);

    // Our secure context state depends on the kind of worker we have.
    if (UsesSystemPrincipal() || IsServiceWorker()) {
      mIsSecureContext = true;
    } else if (mLoadInfo.mWindow) {
      // Shared and dedicated workers both inherit the loading window's secure
      // context state.  Shared workers then prevent windows with a different
      // secure context state from attaching to them.
      mIsSecureContext = mLoadInfo.mWindow->IsSecureContext();
    } else {
      MOZ_ASSERT_UNREACHABLE("non-chrome worker that is not a service worker "
                             "that has no parent and no associated window");
    }

    if (mIsSecureContext) {
      mJSSettings.chrome.compartmentOptions
                 .creationOptions().setSecureContext(true);
      mJSSettings.content.compartmentOptions
                 .creationOptions().setSecureContext(true);
    }

    // Our parent can get suspended after it initiates the async creation
    // of a new worker thread.  In this case suspend the new worker as well.
    if (mLoadInfo.mWindow && mLoadInfo.mWindow->IsSuspended()) {
      ParentWindowPaused();
    }

    if (mLoadInfo.mWindow && mLoadInfo.mWindow->IsFrozen()) {
      Freeze(mLoadInfo.mWindow);
    }
  }
}

template <class Derived>
WorkerPrivateParent<Derived>::~WorkerPrivateParent()
{
  DropJSObjects(this);
}

template <class Derived>
JSObject*
WorkerPrivateParent<Derived>::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  MOZ_ASSERT(!IsSharedWorker(),
             "We should never wrap a WorkerPrivate for a SharedWorker");

  AssertIsOnParentThread();

  // XXXkhuey this should not need to be rooted, the analysis is dumb.
  // See bug 980181.
  JS::Rooted<JSObject*> wrapper(aCx,
    WorkerBinding::Wrap(aCx, ParentAsWorkerPrivate(), aGivenProto));
  if (wrapper) {
    MOZ_ALWAYS_TRUE(TryPreserveWrapper(wrapper));
  }

  return wrapper;
}

template <class Derived>
nsresult
WorkerPrivateParent<Derived>::DispatchPrivate(already_AddRefed<WorkerRunnable> aRunnable,
                                              nsIEventTarget* aSyncLoopTarget)
{
  // May be called on any thread!
  RefPtr<WorkerRunnable> runnable(aRunnable);

  WorkerPrivate* self = ParentAsWorkerPrivate();

  {
    MutexAutoLock lock(mMutex);

    MOZ_ASSERT_IF(aSyncLoopTarget, self->mThread);

    if (!self->mThread) {
      if (ParentStatus() == Pending || self->mStatus == Pending) {
        mPreStartRunnables.AppendElement(runnable);
        return NS_OK;
      }

      NS_WARNING("Using a worker event target after the thread has already"
                 "been released!");
      return NS_ERROR_UNEXPECTED;
    }

    if (self->mStatus == Dead ||
        (!aSyncLoopTarget && ParentStatus() > Running)) {
      NS_WARNING("A runnable was posted to a worker that is already shutting "
                 "down!");
      return NS_ERROR_UNEXPECTED;
    }

    nsresult rv;
    if (aSyncLoopTarget) {
      rv = aSyncLoopTarget->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
    } else {
      rv = self->mThread->DispatchAnyThread(WorkerThreadFriendKey(), runnable.forget());
    }

    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mCondVar.Notify();
  }

  return NS_OK;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::EnableDebugger()
{
  AssertIsOnParentThread();

  WorkerPrivate* self = ParentAsWorkerPrivate();

  if (NS_FAILED(RegisterWorkerDebugger(self))) {
    NS_WARNING("Failed to register worker debugger!");
    return;
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::DisableDebugger()
{
  AssertIsOnParentThread();

  WorkerPrivate* self = ParentAsWorkerPrivate();

  if (NS_FAILED(UnregisterWorkerDebugger(self))) {
    NS_WARNING("Failed to unregister worker debugger!");
  }
}

template <class Derived>
nsresult
WorkerPrivateParent<Derived>::DispatchControlRunnable(
  already_AddRefed<WorkerControlRunnable> aWorkerControlRunnable)
{
  // May be called on any thread!
  RefPtr<WorkerControlRunnable> runnable(aWorkerControlRunnable);
  MOZ_ASSERT(runnable);

  WorkerPrivate* self = ParentAsWorkerPrivate();

  {
    MutexAutoLock lock(mMutex);

    if (self->mStatus == Dead) {
      return NS_ERROR_UNEXPECTED;
    }

    // Transfer ownership to the control queue.
    self->mControlQueue.Push(runnable.forget().take());

    if (JSContext* cx = self->mJSContext) {
      MOZ_ASSERT(self->mThread);
      JS_RequestInterruptCallback(cx);
    }

    mCondVar.Notify();
  }

  return NS_OK;
}

template <class Derived>
nsresult
WorkerPrivateParent<Derived>::DispatchDebuggerRunnable(
  already_AddRefed<WorkerRunnable> aDebuggerRunnable)
{
  // May be called on any thread!

  RefPtr<WorkerRunnable> runnable(aDebuggerRunnable);

  MOZ_ASSERT(runnable);

  WorkerPrivate* self = ParentAsWorkerPrivate();

  {
    MutexAutoLock lock(mMutex);

    if (self->mStatus == Dead) {
      NS_WARNING("A debugger runnable was posted to a worker that is already "
                 "shutting down!");
      return NS_ERROR_UNEXPECTED;
    }

    // Transfer ownership to the debugger queue.
    self->mDebuggerQueue.Push(runnable.forget().take());

    mCondVar.Notify();
  }

  return NS_OK;
}

template <class Derived>
already_AddRefed<WorkerRunnable>
WorkerPrivateParent<Derived>::MaybeWrapAsWorkerRunnable(already_AddRefed<nsIRunnable> aRunnable)
{
  // May be called on any thread!

  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  MOZ_ASSERT(runnable);

  RefPtr<WorkerRunnable> workerRunnable =
    WorkerRunnable::FromRunnable(runnable);
  if (workerRunnable) {
    return workerRunnable.forget();
  }

  nsCOMPtr<nsICancelableRunnable> cancelable = do_QueryInterface(runnable);
  if (!cancelable) {
    MOZ_CRASH("All runnables destined for a worker thread must be cancelable!");
  }

  workerRunnable =
    new ExternalRunnableWrapper(ParentAsWorkerPrivate(), runnable);
  return workerRunnable.forget();
}

template <class Derived>
already_AddRefed<nsIEventTarget>
WorkerPrivateParent<Derived>::GetEventTarget()
{
  WorkerPrivate* self = ParentAsWorkerPrivate();

  nsCOMPtr<nsIEventTarget> target;

  {
    MutexAutoLock lock(mMutex);

    if (!mEventTarget &&
        ParentStatus() <= Running &&
        self->mStatus <= Running) {
      mEventTarget = new EventTarget(self);
    }

    target = mEventTarget;
  }

  NS_WARNING_ASSERTION(
    target,
    "Requested event target for a worker that is already shutting down!");

  return target.forget();
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::Start()
{
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
template <class Derived>
bool
WorkerPrivateParent<Derived>::NotifyPrivate(Status aStatus)
{
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

  if (IsSharedWorker()) {
    RuntimeService* runtime = RuntimeService::GetService();
    MOZ_ASSERT(runtime);

    runtime->ForgetSharedWorker(ParentAsWorkerPrivate());
  }

  if (pending) {
    WorkerPrivate* self = ParentAsWorkerPrivate();

#ifdef DEBUG
    {
      // Fake a thread here just so that our assertions don't go off for no
      // reason.
      nsIThread* currentThread = NS_GetCurrentThread();
      MOZ_ASSERT(currentThread);

      MOZ_ASSERT(!self->mPRThread);
      self->mPRThread = PRThreadFromThread(currentThread);
      MOZ_ASSERT(self->mPRThread);
    }
#endif

    // Worker never got a chance to run, go ahead and delete it.
    self->ScheduleDeletion(WorkerPrivate::WorkerNeverRan);
    return true;
  }

  NS_ASSERTION(aStatus != Terminating || mQueuedRunnables.IsEmpty(),
               "Shouldn't have anything queued!");

  // Anything queued will be discarded.
  mQueuedRunnables.Clear();

  RefPtr<NotifyRunnable> runnable =
    new NotifyRunnable(ParentAsWorkerPrivate(), aStatus);
  return runnable->Dispatch();
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::Freeze(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnParentThread();

  // Shared workers are only frozen if all of their owning documents are
  // frozen. It can happen that mSharedWorkers is empty but this thread has
  // not been unregistered yet.
  if ((IsSharedWorker() || IsServiceWorker()) && !mSharedWorkers.IsEmpty()) {
    AssertIsOnMainThread();

    bool allFrozen = true;

    for (uint32_t i = 0; i < mSharedWorkers.Length(); ++i) {
      if (aWindow && mSharedWorkers[i]->GetOwner() == aWindow) {
        // Calling Freeze() may change the refcount, ensure that the worker
        // outlives this call.
        RefPtr<SharedWorker> kungFuDeathGrip = mSharedWorkers[i];

        kungFuDeathGrip->Freeze();
      } else {
        MOZ_ASSERT_IF(mSharedWorkers[i]->GetOwner() && aWindow,
                      !SameCOMIdentity(mSharedWorkers[i]->GetOwner(),
                                       aWindow));
        if (!mSharedWorkers[i]->IsFrozen()) {
          allFrozen = false;
        }
      }
    }

    if (!allFrozen || mParentFrozen) {
      return true;
    }
  }

  mParentFrozen = true;

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Terminating) {
      return true;
    }
  }

  DisableDebugger();

  RefPtr<FreezeRunnable> runnable =
    new FreezeRunnable(ParentAsWorkerPrivate());
  if (!runnable->Dispatch()) {
    return false;
  }

  return true;
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::Thaw(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnParentThread();

  MOZ_ASSERT(mParentFrozen);

  // Shared workers are resumed if any of their owning documents are thawed.
  // It can happen that mSharedWorkers is empty but this thread has not been
  // unregistered yet.
  if ((IsSharedWorker() || IsServiceWorker()) && !mSharedWorkers.IsEmpty()) {
    AssertIsOnMainThread();

    bool anyRunning = false;

    for (uint32_t i = 0; i < mSharedWorkers.Length(); ++i) {
      if (aWindow && mSharedWorkers[i]->GetOwner() == aWindow) {
        // Calling Thaw() may change the refcount, ensure that the worker
        // outlives this call.
        RefPtr<SharedWorker> kungFuDeathGrip = mSharedWorkers[i];

        kungFuDeathGrip->Thaw();
        anyRunning = true;
      } else {
        MOZ_ASSERT_IF(mSharedWorkers[i]->GetOwner() && aWindow,
                      !SameCOMIdentity(mSharedWorkers[i]->GetOwner(),
                                       aWindow));
        if (!mSharedWorkers[i]->IsFrozen()) {
          anyRunning = true;
        }
      }
    }

    if (!anyRunning || !mParentFrozen) {
      return true;
    }
  }

  MOZ_ASSERT(mParentFrozen);

  mParentFrozen = false;

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Terminating) {
      return true;
    }
  }

  EnableDebugger();

  // Execute queued runnables before waking up the worker, otherwise the worker
  // could post new messages before we run those that have been queued.
  if (!IsParentWindowPaused() && !mQueuedRunnables.IsEmpty()) {
    MOZ_ASSERT(IsDedicatedWorker());

    nsTArray<nsCOMPtr<nsIRunnable>> runnables;
    mQueuedRunnables.SwapElements(runnables);

    for (uint32_t index = 0; index < runnables.Length(); index++) {
      runnables[index]->Run();
    }
  }

  RefPtr<ThawRunnable> runnable =
    new ThawRunnable(ParentAsWorkerPrivate());
  if (!runnable->Dispatch()) {
    return false;
  }

  return true;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::ParentWindowPaused()
{
  AssertIsOnMainThread();
  MOZ_ASSERT_IF(IsDedicatedWorker(), mParentWindowPausedDepth == 0);
  mParentWindowPausedDepth += 1;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::ParentWindowResumed()
{
  AssertIsOnMainThread();

  MOZ_ASSERT(mParentWindowPausedDepth > 0);
  MOZ_ASSERT_IF(IsDedicatedWorker(), mParentWindowPausedDepth == 1);
  mParentWindowPausedDepth -= 1;
  if (mParentWindowPausedDepth > 0) {
    return;
  }

  {
    MutexAutoLock lock(mMutex);

    if (mParentStatus >= Terminating) {
      return;
    }
  }

  // Execute queued runnables before waking up, otherwise the worker could post
  // new messages before we run those that have been queued.
  if (!IsFrozen() && !mQueuedRunnables.IsEmpty()) {
    MOZ_ASSERT(IsDedicatedWorker());

    nsTArray<nsCOMPtr<nsIRunnable>> runnables;
    mQueuedRunnables.SwapElements(runnables);

    for (uint32_t index = 0; index < runnables.Length(); index++) {
      runnables[index]->Run();
    }
  }
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::Close()
{
  mMutex.AssertCurrentThreadOwns();

  if (mParentStatus < Closing) {
    mParentStatus = Closing;
  }

  return true;
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::ModifyBusyCount(bool aIncrease)
{
  AssertIsOnParentThread();

  NS_ASSERTION(aIncrease || mBusyCount, "Mismatched busy count mods!");

  if (aIncrease) {
    mBusyCount++;
    return true;
  }

  if (--mBusyCount == 0) {

    bool shouldCancel;
    {
      MutexAutoLock lock(mMutex);
      shouldCancel = mParentStatus == Terminating;
    }

    if (shouldCancel && !Cancel()) {
      return false;
    }
  }

  return true;
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::ProxyReleaseMainThreadObjects()
{
  AssertIsOnParentThread();
  MOZ_ASSERT(!mMainThreadObjectsForgotten);

  nsCOMPtr<nsILoadGroup> loadGroupToCancel;
  // If we're not overriden, then do nothing here.  Let the load group get
  // handled in ForgetMainThreadObjects().
  if (mLoadInfo.mInterfaceRequestor) {
    mLoadInfo.mLoadGroup.swap(loadGroupToCancel);
  }

  bool result = mLoadInfo.ProxyReleaseMainThreadObjects(ParentAsWorkerPrivate(),
                                                        loadGroupToCancel);

  mMainThreadObjectsForgotten = true;

  return result;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::PostMessageInternal(JSContext* aCx,
                                                  JS::Handle<JS::Value> aMessage,
                                                  const Sequence<JSObject*>& aTransferable,
                                                  ErrorResult& aRv)
{
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);
    if (mParentStatus > Running) {
      return;
    }
  }

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());

  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransferable,
                                                          &transferable);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  RefPtr<MessageEventRunnable> runnable =
    new MessageEventRunnable(ParentAsWorkerPrivate(),
                             WorkerRunnable::WorkerThreadModifyBusyCount);

  UniquePtr<AbstractTimelineMarker> start;
  UniquePtr<AbstractTimelineMarker> end;
  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  bool isTimelineRecording = timelines && !timelines->IsEmpty();

  if (isTimelineRecording) {
    start = MakeUnique<WorkerTimelineMarker>(NS_IsMainThread()
      ? ProfileTimelineWorkerOperationType::SerializeDataOnMainThread
      : ProfileTimelineWorkerOperationType::SerializeDataOffMainThread,
      MarkerTracingType::START);
  }

  runnable->Write(aCx, aMessage, transferable, JS::CloneDataPolicy(), aRv);

  if (isTimelineRecording) {
    end = MakeUnique<WorkerTimelineMarker>(NS_IsMainThread()
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
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::PostMessage(
                             JSContext* aCx, JS::Handle<JS::Value> aMessage,
                             const Sequence<JSObject*>& aTransferable,
                             ErrorResult& aRv)
{
  PostMessageInternal(aCx, aMessage, aTransferable, aRv);
}

template <class Derived>
void
WorkerPrivateParent<Derived>::UpdateContextOptions(
                                    const JS::ContextOptions& aContextOptions)
{
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);
    mJSSettings.contextOptions = aContextOptions;
  }

  RefPtr<UpdateContextOptionsRunnable> runnable =
    new UpdateContextOptionsRunnable(ParentAsWorkerPrivate(), aContextOptions);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to update worker context options!");
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::UpdatePreference(WorkerPreference aPref, bool aValue)
{
  AssertIsOnParentThread();
  MOZ_ASSERT(aPref >= 0 && aPref < WORKERPREF_COUNT);

  RefPtr<UpdatePreferenceRunnable> runnable =
    new UpdatePreferenceRunnable(ParentAsWorkerPrivate(), aPref, aValue);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to update worker preferences!");
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::UpdateLanguages(const nsTArray<nsString>& aLanguages)
{
  AssertIsOnParentThread();

  RefPtr<UpdateLanguagesRunnable> runnable =
    new UpdateLanguagesRunnable(ParentAsWorkerPrivate(), aLanguages);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to update worker languages!");
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::UpdateJSWorkerMemoryParameter(JSGCParamKey aKey,
                                                            uint32_t aValue)
{
  AssertIsOnParentThread();

  bool found = false;

  {
    MutexAutoLock lock(mMutex);
    found = mJSSettings.ApplyGCSetting(aKey, aValue);
  }

  if (found) {
    RefPtr<UpdateJSWorkerMemoryParameterRunnable> runnable =
      new UpdateJSWorkerMemoryParameterRunnable(ParentAsWorkerPrivate(), aKey,
                                                aValue);
    if (!runnable->Dispatch()) {
      NS_WARNING("Failed to update memory parameter!");
    }
  }
}

#ifdef JS_GC_ZEAL
template <class Derived>
void
WorkerPrivateParent<Derived>::UpdateGCZeal(uint8_t aGCZeal, uint32_t aFrequency)
{
  AssertIsOnParentThread();

  {
    MutexAutoLock lock(mMutex);
    mJSSettings.gcZeal = aGCZeal;
    mJSSettings.gcZealFrequency = aFrequency;
  }

  RefPtr<UpdateGCZealRunnable> runnable =
    new UpdateGCZealRunnable(ParentAsWorkerPrivate(), aGCZeal, aFrequency);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to update worker gczeal!");
  }
}
#endif

template <class Derived>
void
WorkerPrivateParent<Derived>::GarbageCollect(bool aShrinking)
{
  AssertIsOnParentThread();

  RefPtr<GarbageCollectRunnable> runnable =
    new GarbageCollectRunnable(ParentAsWorkerPrivate(), aShrinking,
                               /* collectChildren = */ true);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to GC worker!");
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::CycleCollect(bool aDummy)
{
  AssertIsOnParentThread();

  RefPtr<CycleCollectRunnable> runnable =
    new CycleCollectRunnable(ParentAsWorkerPrivate(),
                             /* collectChildren = */ true);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to CC worker!");
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::OfflineStatusChangeEvent(bool aIsOffline)
{
  AssertIsOnParentThread();

  RefPtr<OfflineStatusChangeRunnable> runnable =
    new OfflineStatusChangeRunnable(ParentAsWorkerPrivate(), aIsOffline);
  if (!runnable->Dispatch()) {
    NS_WARNING("Failed to dispatch offline status change event!");
  }
}

void
WorkerPrivate::OfflineStatusChangeEventInternal(bool aIsOffline)
{
  AssertIsOnWorkerThread();

  // The worker is already in this state. No need to dispatch an event.
  if (mOnLine == !aIsOffline) {
    return;
  }

  for (uint32_t index = 0; index < mChildWorkers.Length(); ++index) {
    mChildWorkers[index]->OfflineStatusChangeEvent(aIsOffline);
  }

  mOnLine = !aIsOffline;
  WorkerGlobalScope* globalScope = GlobalScope();
  RefPtr<WorkerNavigator> nav = globalScope->GetExistingNavigator();
  if (nav) {
    nav->SetOnLine(mOnLine);
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

  globalScope->DispatchDOMEvent(nullptr, event, nullptr, nullptr);
}

template <class Derived>
void
WorkerPrivateParent<Derived>::MemoryPressure(bool aDummy)
{
  AssertIsOnParentThread();

  RefPtr<MemoryPressureRunnable> runnable =
    new MemoryPressureRunnable(ParentAsWorkerPrivate());
  Unused << NS_WARN_IF(!runnable->Dispatch());
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::RegisterSharedWorker(SharedWorker* aSharedWorker,
                                                   MessagePort* aPort)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aSharedWorker);
  MOZ_ASSERT(IsSharedWorker());
  MOZ_ASSERT(!mSharedWorkers.Contains(aSharedWorker));

  if (IsSharedWorker()) {
    RefPtr<MessagePortRunnable> runnable =
      new MessagePortRunnable(ParentAsWorkerPrivate(), aPort);
    if (!runnable->Dispatch()) {
      return false;
    }
  }

  mSharedWorkers.AppendElement(aSharedWorker);

  // If there were other SharedWorker objects attached to this worker then they
  // may all have been frozen and this worker would need to be thawed.
  if (mSharedWorkers.Length() > 1 && IsFrozen() && !Thaw(nullptr)) {
    return false;
  }

  return true;
}

template <class Derived>
void
WorkerPrivateParent<Derived>::BroadcastErrorToSharedWorkers(
                                                    JSContext* aCx,
                                                    const WorkerErrorReport* aReport,
                                                    bool aIsErrorEvent)
{
  AssertIsOnMainThread();

  if (aIsErrorEvent && JSREPORT_IS_WARNING(aReport->mFlags)) {
    // Don't fire any events anywhere.  Just log to console.
    // XXXbz should we log to all the consoles of all the relevant windows?
    MOZ_ASSERT(aReport);
    LogErrorToConsole(*aReport, 0);
    return;
  }

  AutoTArray<RefPtr<SharedWorker>, 10> sharedWorkers;
  GetAllSharedWorkers(sharedWorkers);

  if (sharedWorkers.IsEmpty()) {
    return;
  }

  AutoTArray<WindowAction, 10> windowActions;
  nsresult rv;

  // First fire the error event at all SharedWorker objects. This may include
  // multiple objects in a single window as well as objects in different
  // windows.
  for (size_t index = 0; index < sharedWorkers.Length(); index++) {
    RefPtr<SharedWorker>& sharedWorker = sharedWorkers[index];

    // May be null.
    nsPIDOMWindowInner* window = sharedWorker->GetOwner();

    RefPtr<Event> event;

    if (aIsErrorEvent) {
      RootedDictionary<ErrorEventInit> errorInit(aCx);
      errorInit.mBubbles = false;
      errorInit.mCancelable = true;
      errorInit.mMessage = aReport->mMessage;
      errorInit.mFilename = aReport->mFilename;
      errorInit.mLineno = aReport->mLineNumber;
      errorInit.mColno = aReport->mColumnNumber;

      event = ErrorEvent::Constructor(sharedWorker, NS_LITERAL_STRING("error"),
                                      errorInit);
    } else {
      event = Event::Constructor(sharedWorker, NS_LITERAL_STRING("error"),
                                 EventInit());
    }

    if (!event) {
      ThrowAndReport(window, NS_ERROR_UNEXPECTED);
      continue;
    }

    event->SetTrusted(true);

    bool defaultActionEnabled;
    nsresult rv = sharedWorker->DispatchEvent(event, &defaultActionEnabled);
    if (NS_FAILED(rv)) {
      ThrowAndReport(window, rv);
      continue;
    }

    if (!aIsErrorEvent) {
      continue;
    }

    if (defaultActionEnabled) {
      // Add the owning window to our list so that we will fire an error event
      // at it later.
      if (!windowActions.Contains(window)) {
        windowActions.AppendElement(WindowAction(window));
      }
    } else {
      size_t actionsIndex = windowActions.LastIndexOf(WindowAction(window));
      if (actionsIndex != windowActions.NoIndex) {
        // Any listener that calls preventDefault() will prevent the window from
        // receiving the error event.
        windowActions[actionsIndex].mDefaultAction = false;
      }
    }
  }

  // If there are no windows to consider further then we're done.
  if (windowActions.IsEmpty()) {
    return;
  }

  bool shouldLogErrorToConsole = true;

  // Now fire error events at all the windows remaining.
  for (uint32_t index = 0; index < windowActions.Length(); index++) {
    WindowAction& windowAction = windowActions[index];

    // If there is no window or the script already called preventDefault then
    // skip this window.
    if (!windowAction.mWindow || !windowAction.mDefaultAction) {
      continue;
    }

    nsCOMPtr<nsIScriptGlobalObject> sgo =
      do_QueryInterface(windowAction.mWindow);
    MOZ_ASSERT(sgo);

    MOZ_ASSERT(NS_IsMainThread());
    RootedDictionary<ErrorEventInit> init(aCx);
    init.mLineno = aReport->mLineNumber;
    init.mFilename = aReport->mFilename;
    init.mMessage = aReport->mMessage;
    init.mCancelable = true;
    init.mBubbles = true;

    nsEventStatus status = nsEventStatus_eIgnore;
    rv = sgo->HandleScriptError(init, &status);
    if (NS_FAILED(rv)) {
      ThrowAndReport(windowAction.mWindow, rv);
      continue;
    }

    if (status == nsEventStatus_eConsumeNoDefault) {
      shouldLogErrorToConsole = false;
    }
  }

  // Finally log a warning in the console if no window tried to prevent it.
  if (shouldLogErrorToConsole) {
    MOZ_ASSERT(aReport);
    LogErrorToConsole(*aReport, 0);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::GetAllSharedWorkers(
                               nsTArray<RefPtr<SharedWorker>>& aSharedWorkers)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(IsSharedWorker() || IsServiceWorker());

  if (!aSharedWorkers.IsEmpty()) {
    aSharedWorkers.Clear();
  }

  for (uint32_t i = 0; i < mSharedWorkers.Length(); ++i) {
    aSharedWorkers.AppendElement(mSharedWorkers[i]);
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::CloseSharedWorkersForWindow(
                                                    nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(IsSharedWorker() || IsServiceWorker());
  MOZ_ASSERT(aWindow);

  bool someRemoved = false;

  for (uint32_t i = 0; i < mSharedWorkers.Length();) {
    if (mSharedWorkers[i]->GetOwner() == aWindow) {
      mSharedWorkers[i]->Close();
      mSharedWorkers.RemoveElementAt(i);
      someRemoved = true;
    } else {
      MOZ_ASSERT(!SameCOMIdentity(mSharedWorkers[i]->GetOwner(),
                                  aWindow));
      ++i;
    }
  }

  if (!someRemoved) {
    return;
  }

  // If there are still SharedWorker objects attached to this worker then they
  // may all be frozen and this worker would need to be frozen. Otherwise,
  // if that was the last SharedWorker then it's time to cancel this worker.

  if (!mSharedWorkers.IsEmpty()) {
    Freeze(nullptr);
  } else {
    Cancel();
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::CloseAllSharedWorkers()
{
  AssertIsOnMainThread();
  MOZ_ASSERT(IsSharedWorker() || IsServiceWorker());

  for (uint32_t i = 0; i < mSharedWorkers.Length(); ++i) {
    mSharedWorkers[i]->Close();
  }

  mSharedWorkers.Clear();

  Cancel();
}

template <class Derived>
void
WorkerPrivateParent<Derived>::WorkerScriptLoaded()
{
  AssertIsOnMainThread();

  if (IsSharedWorker() || IsServiceWorker()) {
    // No longer need to hold references to the window or document we came from.
    mLoadInfo.mWindow = nullptr;
    mLoadInfo.mScriptContext = nullptr;
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::SetBaseURI(nsIURI* aBaseURI)
{
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
  }
  else {
    mLocationInfo.mProtocol.Truncate();
  }

  int32_t port;
  if (NS_SUCCEEDED(aBaseURI->GetPort(&port)) && port != -1) {
    mLocationInfo.mPort.AppendInt(port);

    nsAutoCString host(mLocationInfo.mHostname);
    host.Append(':');
    host.Append(mLocationInfo.mPort);

    mLocationInfo.mHost.Assign(host);
  }
  else {
    mLocationInfo.mHost.Assign(mLocationInfo.mHostname);
  }

  nsContentUtils::GetUTFOrigin(aBaseURI, mLocationInfo.mOrigin);
}

template <class Derived>
nsresult
WorkerPrivateParent<Derived>::SetPrincipalOnMainThread(nsIPrincipal* aPrincipal,
                                                       nsILoadGroup* aLoadGroup)
{
  return mLoadInfo.SetPrincipalOnMainThread(aPrincipal, aLoadGroup);
}

template <class Derived>
nsresult
WorkerPrivateParent<Derived>::SetPrincipalFromChannel(nsIChannel* aChannel)
{
  return mLoadInfo.SetPrincipalFromChannel(aChannel);
}

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
template <class Derived>
bool
WorkerPrivateParent<Derived>::FinalChannelPrincipalIsValid(nsIChannel* aChannel)
{
  return mLoadInfo.FinalChannelPrincipalIsValid(aChannel);
}

template <class Derived>
bool
WorkerPrivateParent<Derived>::PrincipalURIMatchesScriptURL()
{
  return mLoadInfo.PrincipalURIMatchesScriptURL();
}
#endif

template <class Derived>
void
WorkerPrivateParent<Derived>::UpdateOverridenLoadGroup(nsILoadGroup* aBaseLoadGroup)
{
  AssertIsOnMainThread();

  // The load group should have been overriden at init time.
  mLoadInfo.mInterfaceRequestor->MaybeAddTabChild(aBaseLoadGroup);
}

template <class Derived>
void
WorkerPrivateParent<Derived>::FlushReportsToSharedWorkers(
                                           nsIConsoleReportCollector* aReporter)
{
  AssertIsOnMainThread();

  AutoTArray<RefPtr<SharedWorker>, 10> sharedWorkers;
  AutoTArray<WindowAction, 10> windowActions;
  GetAllSharedWorkers(sharedWorkers);

  // First find out all the shared workers' window.
  for (size_t index = 0; index < sharedWorkers.Length(); index++) {
    RefPtr<SharedWorker>& sharedWorker = sharedWorkers[index];

    // May be null.
    nsPIDOMWindowInner* window = sharedWorker->GetOwner();

    // Add the owning window to our list so that we will flush the reports later.
    if (window && !windowActions.Contains(window)) {
      windowActions.AppendElement(WindowAction(window));
    }
  }

  bool reportErrorToBrowserConsole = true;

  // Flush the reports.
  for (uint32_t index = 0; index < windowActions.Length(); index++) {
    WindowAction& windowAction = windowActions[index];

    aReporter->FlushReportsToConsole(
      windowAction.mWindow->WindowID(),
      nsIConsoleReportCollector::ReportAction::Save);
    reportErrorToBrowserConsole = false;
  }

  // Finally report to browser console if there is no any window or shared
  // worker.
  if (reportErrorToBrowserConsole) {
    aReporter->FlushReportsToConsole(0);
    return;
  }

  aReporter->ClearConsoleReports();
}

template <class Derived>
NS_IMPL_ADDREF_INHERITED(WorkerPrivateParent<Derived>, DOMEventTargetHelper)

template <class Derived>
NS_IMPL_RELEASE_INHERITED(WorkerPrivateParent<Derived>, DOMEventTargetHelper)

template <class Derived>
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(WorkerPrivateParent<Derived>)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

template <class Derived>
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WorkerPrivateParent<Derived>,
                                                  DOMEventTargetHelper)
  tmp->AssertIsOnParentThread();

  // The WorkerPrivate::mSelfRef has a reference to itself, which is really
  // held by the worker thread.  We traverse this reference if and only if our
  // busy count is zero and we have not released the main thread reference.
  // We do not unlink it.  This allows the CC to break cycles involving the
  // WorkerPrivate and begin shutting it down (which does happen in unlink) but
  // ensures that the WorkerPrivate won't be deleted before we're done shutting
  // down the thread.

  if (!tmp->mBusyCount && !tmp->mMainThreadObjectsForgotten) {
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSelfRef)
  }

  // The various strong references in LoadInfo are managed manually and cannot
  // be cycle collected.
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

template <class Derived>
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WorkerPrivateParent<Derived>,
                                                DOMEventTargetHelper)
  tmp->Terminate();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

template <class Derived>
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(WorkerPrivateParent<Derived>,
                                               DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

#ifdef DEBUG

template <class Derived>
void
WorkerPrivateParent<Derived>::AssertIsOnParentThread() const
{
  if (GetParent()) {
    GetParent()->AssertIsOnWorkerThread();
  }
  else {
    AssertIsOnMainThread();
  }
}

template <class Derived>
void
WorkerPrivateParent<Derived>::AssertInnerWindowIsCorrect() const
{
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
template <class Derived>
bool
WorkerPrivateParent<Derived>::PrincipalIsValid() const
{
  return mLoadInfo.PrincipalIsValid();
}
#endif

class PostDebuggerMessageRunnable final : public Runnable
{
  WorkerDebugger *mDebugger;
  nsString mMessage;

public:
  PostDebuggerMessageRunnable(WorkerDebugger* aDebugger,
                              const nsAString& aMessage)
  : mDebugger(aDebugger),
    mMessage(aMessage)
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

class ReportDebuggerErrorRunnable final : public Runnable
{
  WorkerDebugger *mDebugger;
  nsString mFilename;
  uint32_t mLineno;
  nsString mMessage;

public:
  ReportDebuggerErrorRunnable(WorkerDebugger* aDebugger,
                              const nsAString& aFilename, uint32_t aLineno,
                              const nsAString& aMessage)
  : mDebugger(aDebugger),
    mFilename(aFilename),
    mLineno(aLineno),
    mMessage(aMessage)
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
      NS_ReleaseOnMainThread(mListeners[index].forget());
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
  LogErrorToConsole(report, 0);
}

WorkerPrivate::WorkerPrivate(WorkerPrivate* aParent,
                             const nsAString& aScriptURL,
                             bool aIsChromeWorker, WorkerType aWorkerType,
                             const nsAString& aWorkerName,
                             const nsACString& aServiceWorkerScope,
                             WorkerLoadInfo& aLoadInfo)
  : WorkerPrivateParent<WorkerPrivate>(aParent, aScriptURL,
                                       aIsChromeWorker, aWorkerType,
                                       aWorkerName, aServiceWorkerScope,
                                       aLoadInfo)
  , mDebuggerRegistered(false)
  , mDebugger(nullptr)
  , mJSContext(nullptr)
  , mPRThread(nullptr)
  , mNumHoldersPreventingShutdownStart(0)
  , mDebuggerEventLoopLevel(0)
  , mMainThreadEventTarget(do_GetMainThread())
  , mWorkerControlEventTarget(new WorkerControlEventTarget(this))
  , mErrorHandlerRecursionCount(0)
  , mNextTimeoutId(1)
  , mStatus(Pending)
  , mFrozen(false)
  , mTimerRunning(false)
  , mRunningExpiredTimeouts(false)
  , mPendingEventQueueClearing(false)
  , mCancelAllPendingRunnables(false)
  , mPeriodicGCTimerRunning(false)
  , mIdleGCTimerRunning(false)
  , mWorkerScriptExecutedSuccessfully(false)
  , mFetchHandlerWasAdded(false)
  , mOnLine(false)
{
  if (aParent) {
    aParent->AssertIsOnWorkerThread();
    aParent->GetAllPreferences(mPreferences);
    mOnLine = aParent->OnLine();
  }
  else {
    AssertIsOnMainThread();
    RuntimeService::GetDefaultPreferences(mPreferences);
    mOnLine = !NS_IsOffline();
  }

  nsCOMPtr<nsIEventTarget> target;

  // A child worker just inherits the parent workers ThrottledEventQueue
  // and main thread target for now.  This is mainly due to the restriction
  // that ThrottledEventQueue can only be created on the main thread at the
  // moment.
  if (aParent) {
    mMainThreadThrottledEventQueue = aParent->mMainThreadThrottledEventQueue;
    mMainThreadEventTarget = aParent->mMainThreadEventTarget;
    return;
  }

  MOZ_ASSERT(NS_IsMainThread());
  target = GetWindow() ? GetWindow()->EventTargetFor(TaskCategory::Worker) : nullptr;

  if (!target) {
    nsCOMPtr<nsIThread> mainThread;
    NS_GetMainThread(getter_AddRefs(mainThread));
    MOZ_DIAGNOSTIC_ASSERT(mainThread);
    target = mainThread;
  }

  // Throttle events to the main thread using a ThrottledEventQueue specific to
  // this worker thread.  This may return nullptr during shutdown.
  mMainThreadThrottledEventQueue = ThrottledEventQueue::Create(target);

  // If we were able to creat the throttled event queue, then use it for
  // dispatching our main thread runnables.  Otherwise use our underlying
  // base target.
  if (mMainThreadThrottledEventQueue) {
    mMainThreadEventTarget = mMainThreadThrottledEventQueue;
  } else {
    mMainThreadEventTarget = target.forget();
  }
}

WorkerPrivate::~WorkerPrivate()
{
  mWorkerControlEventTarget->ForgetWorkerPrivate(this);
}

// static
already_AddRefed<WorkerPrivate>
WorkerPrivate::Constructor(const GlobalObject& aGlobal,
                           const nsAString& aScriptURL,
                           const WorkerOptions& aOptions,
                           ErrorResult& aRv)
{
  return WorkerPrivate::Constructor(aGlobal, aScriptURL, false,
                                    WorkerTypeDedicated,
                                    aOptions.mName, nullptr, aRv);
}

// static
bool
WorkerPrivate::WorkerAvailable(JSContext* aCx, JSObject* /* unused */)
{
  // If we're already on a worker workers are clearly enabled.
  if (!NS_IsMainThread()) {
    return true;
  }

  // If our caller is chrome, workers are always available.
  if (nsContentUtils::IsSystemCaller(aCx)) {
    return true;
  }

  // Else check the pref.
  return Preferences::GetBool(PREF_WORKERS_ENABLED);
}

// static
already_AddRefed<ChromeWorkerPrivate>
ChromeWorkerPrivate::Constructor(const GlobalObject& aGlobal,
                                 const nsAString& aScriptURL,
                                 ErrorResult& aRv)
{
  return WorkerPrivate::Constructor(aGlobal, aScriptURL, true,
                                    WorkerTypeDedicated, EmptyString(),
                                    nullptr, aRv)
                                    .downcast<ChromeWorkerPrivate>();
}

// static
bool
ChromeWorkerPrivate::WorkerAvailable(JSContext* aCx, JSObject* /* unused */)
{
  // Chrome is always allowed to use workers, and content is never
  // allowed to use ChromeWorker, so all we have to check is the
  // caller.  However, chrome workers apparently might not have a
  // system principal, so we have to check for them manually.
  if (NS_IsMainThread()) {
    return nsContentUtils::IsSystemCaller(aCx);
  }

  return GetWorkerPrivateFromContext(aCx)->IsChromeWorker();
}

// static
already_AddRefed<WorkerPrivate>
WorkerPrivate::Constructor(const GlobalObject& aGlobal,
                           const nsAString& aScriptURL,
                           bool aIsChromeWorker, WorkerType aWorkerType,
                           const nsAString& aWorkerName,
                           WorkerLoadInfo* aLoadInfo, ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();
  return Constructor(cx, aScriptURL, aIsChromeWorker, aWorkerType,
                     aWorkerName, NullCString(), aLoadInfo, aRv);
}

// static
already_AddRefed<WorkerPrivate>
WorkerPrivate::Constructor(JSContext* aCx,
                           const nsAString& aScriptURL,
                           bool aIsChromeWorker, WorkerType aWorkerType,
                           const nsAString& aWorkerName,
                           const nsACString& aServiceWorkerScope,
                           WorkerLoadInfo* aLoadInfo, ErrorResult& aRv)
{
  // If this is a sub-worker, we need to keep the parent worker alive until this
  // one is registered.
  UniquePtr<SimpleWorkerHolder> holder;

  WorkerPrivate* parent = NS_IsMainThread() ?
                          nullptr :
                          GetCurrentThreadWorkerPrivate();
  if (parent) {
    parent->AssertIsOnWorkerThread();

    holder.reset(new SimpleWorkerHolder());
    if (!holder->HoldWorker(parent, Canceling)) {
      aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
      return nullptr;
    }
  } else {
    AssertIsOnMainThread();
  }

  Maybe<WorkerLoadInfo> stackLoadInfo;
  if (!aLoadInfo) {
    stackLoadInfo.emplace();

    nsresult rv = GetLoadInfo(aCx, nullptr, parent, aScriptURL,
                              aIsChromeWorker, InheritLoadGroup,
                              aWorkerType, stackLoadInfo.ptr());
    aRv.MightThrowJSException();
    if (NS_FAILED(rv)) {
      scriptloader::ReportLoadError(aRv, rv, aScriptURL);
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
  }
  else {
    runtimeService = RuntimeService::GetService();
  }

  MOZ_ASSERT(runtimeService);

  RefPtr<WorkerPrivate> worker =
    new WorkerPrivate(parent, aScriptURL, aIsChromeWorker,
                      aWorkerType, aWorkerName, aServiceWorkerScope,
                      *aLoadInfo);

  // Gecko contexts always have an explicitly-set default locale (set by
  // XPJSRuntime::Initialize for the main thread, set by
  // WorkerThreadPrimaryRunnable::Run for workers just before running worker
  // code), so this is never SpiderMonkey's builtin default locale.
  JS::UniqueChars defaultLocale = JS_GetDefaultLocale(aCx);
  if (NS_WARN_IF(!defaultLocale)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  worker->mDefaultLocale = Move(defaultLocale);

  if (!runtimeService->RegisterWorker(worker)) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  worker->EnableDebugger();

  MOZ_DIAGNOSTIC_ASSERT(worker->PrincipalIsValid());

  RefPtr<CompileScriptRunnable> compiler =
    new CompileScriptRunnable(worker, aScriptURL);
  if (!compiler->Dispatch()) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  worker->mSelfRef = worker;

  return worker.forget();
}

// static
nsresult
WorkerPrivate::GetLoadInfo(JSContext* aCx, nsPIDOMWindowInner* aWindow,
                           WorkerPrivate* aParent, const nsAString& aScriptURL,
                           bool aIsChromeWorker,
                           LoadGroupBehavior aLoadGroupBehavior,
                           WorkerType aWorkerType,
                           WorkerLoadInfo* aLoadInfo)
{
  using namespace mozilla::dom::workers::scriptloader;

  MOZ_ASSERT(aCx);
  MOZ_ASSERT_IF(NS_IsMainThread(), aCx == nsContentUtils::GetCurrentJSContext());

  if (aWindow) {
    AssertIsOnMainThread();
  }

  WorkerLoadInfo loadInfo;
  nsresult rv;

  if (aParent) {
    aParent->AssertIsOnWorkerThread();

    // If the parent is going away give up now.
    Status parentStatus;
    {
      MutexAutoLock lock(aParent->mMutex);
      parentStatus = aParent->mStatus;
    }

    if (parentStatus > Running) {
      return NS_ERROR_FAILURE;
    }

    // Passing a pointer to our stack loadInfo is safe here because this
    // method uses a sync runnable to get the channel from the main thread.
    rv = ChannelFromScriptURLWorkerThread(aCx, aParent, aScriptURL,
                                          loadInfo);
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
    loadInfo.mStorageAllowed = aParent->IsStorageAllowed();
    loadInfo.mOriginAttributes = aParent->GetOriginAttributes();
    loadInfo.mServiceWorkersTestingInWindow =
      aParent->ServiceWorkersTestingInWindow();
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
      rv = ssm->GetSystemPrincipal(getter_AddRefs(loadInfo.mPrincipal));
      NS_ENSURE_SUCCESS(rv, rv);

      loadInfo.mPrincipalIsSystem = true;
    }

    // See if we're being called from a window.
    nsCOMPtr<nsPIDOMWindowInner> globalWindow = aWindow;
    if (!globalWindow) {
      nsCOMPtr<nsIScriptGlobalObject> scriptGlobal =
        nsJSUtils::GetStaticScriptGlobal(JS::CurrentGlobalOrNull(aCx));
      if (scriptGlobal) {
        globalWindow = do_QueryInterface(scriptGlobal);
        MOZ_ASSERT(globalWindow);
      }
    }

    nsCOMPtr<nsIDocument> document;

    if (globalWindow) {
      // Only use the current inner window, and only use it if the caller can
      // access it.
      if (nsPIDOMWindowOuter* outerWindow = globalWindow->GetOuterWindow()) {
        loadInfo.mWindow = outerWindow->GetCurrentInnerWindow();
        // TODO: fix this for SharedWorkers with multiple documents (bug 1177935)
        loadInfo.mServiceWorkersTestingInWindow =
          outerWindow->GetServiceWorkersTestingEnabled();
      }

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

      // Use the document's NodePrincipal as our principal if we're not being
      // called from chrome.
      if (!loadInfo.mPrincipal) {
        loadInfo.mPrincipal = document->NodePrincipal();
        NS_ENSURE_TRUE(loadInfo.mPrincipal, NS_ERROR_FAILURE);

        // We use the document's base domain to limit the number of workers
        // each domain can create. For sandboxed documents, we use the domain
        // of their first non-sandboxed document, walking up until we find
        // one. If we can't find one, we fall back to using the GUID of the
        // null principal as the base domain.
        if (document->GetSandboxFlags() & SANDBOXED_ORIGIN) {
          nsCOMPtr<nsIDocument> tmpDoc = document;
          do {
            tmpDoc = tmpDoc->GetParentDocument();
          } while (tmpDoc && tmpDoc->GetSandboxFlags() & SANDBOXED_ORIGIN);

          if (tmpDoc) {
            // There was an unsandboxed ancestor, yay!
            nsCOMPtr<nsIPrincipal> tmpPrincipal = tmpDoc->NodePrincipal();
            rv = tmpPrincipal->GetBaseDomain(loadInfo.mDomain);
            NS_ENSURE_SUCCESS(rv, rv);
          } else {
            // No unsandboxed ancestor, use our GUID.
            rv = loadInfo.mPrincipal->GetBaseDomain(loadInfo.mDomain);
            NS_ENSURE_SUCCESS(rv, rv);
          }
        } else {
          // Document creating the worker is not sandboxed.
          rv = loadInfo.mPrincipal->GetBaseDomain(loadInfo.mDomain);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }

      NS_ENSURE_TRUE(NS_LoadGroupMatchesPrincipal(loadInfo.mLoadGroup,
                                                  loadInfo.mPrincipal),
                     NS_ERROR_FAILURE);

      nsCOMPtr<nsIPermissionManager> permMgr =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      uint32_t perm;
      rv = permMgr->TestPermissionFromPrincipal(loadInfo.mPrincipal, "systemXHR",
                                                &perm);
      NS_ENSURE_SUCCESS(rv, rv);

      loadInfo.mXHRParamsAllowed = perm == nsIPermissionManager::ALLOW_ACTION;

      loadInfo.mFromWindow = true;
      loadInfo.mWindowID = globalWindow->WindowID();
      nsContentUtils::StorageAccess access =
        nsContentUtils::StorageAllowedForWindow(globalWindow);
      loadInfo.mStorageAllowed = access > nsContentUtils::StorageAccess::eDeny;
      loadInfo.mOriginAttributes = nsContentUtils::GetOriginAttributes(document);
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
          rv = NS_NewFileURI(getter_AddRefs(loadInfo.mBaseURI),
                             scriptFile);
        }
        if (NS_FAILED(rv)) {
          // As expected, fileName is not a path, so proceed with
          // a uri.
          rv = NS_NewURI(getter_AddRefs(loadInfo.mBaseURI),
                         fileName.get());
        }
        if (NS_FAILED(rv)) {
          return rv;
        }
      }
      loadInfo.mXHRParamsAllowed = true;
      loadInfo.mFromWindow = false;
      loadInfo.mWindowID = UINT64_MAX;
      loadInfo.mStorageAllowed = true;
      loadInfo.mOriginAttributes = OriginAttributes();
    }

    MOZ_ASSERT(loadInfo.mPrincipal);
    MOZ_ASSERT(isChrome || !loadInfo.mDomain.IsEmpty());

    if (!loadInfo.mLoadGroup || aLoadGroupBehavior == OverrideLoadGroup) {
      OverrideLoadInfoLoadGroup(loadInfo);
    }
    MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(loadInfo.mLoadGroup,
                                            loadInfo.mPrincipal));

    // Top level workers' main script use the document charset for the script
    // uri encoding.
    bool useDefaultEncoding = false;
    rv = ChannelFromScriptURLMainThread(loadInfo.mPrincipal, loadInfo.mBaseURI,
                                        document, loadInfo.mLoadGroup,
                                        aScriptURL,
                                        ContentPolicyType(aWorkerType),
                                        useDefaultEncoding,
                                        getter_AddRefs(loadInfo.mChannel));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NS_GetFinalChannelURI(loadInfo.mChannel,
                               getter_AddRefs(loadInfo.mResolvedScriptURI));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = loadInfo.SetPrincipalFromChannel(loadInfo.mChannel);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  MOZ_DIAGNOSTIC_ASSERT(loadInfo.PrincipalIsValid());

  aLoadInfo->StealFrom(loadInfo);
  return NS_OK;
}

// static
void
WorkerPrivate::OverrideLoadInfoLoadGroup(WorkerLoadInfo& aLoadInfo)
{
  MOZ_ASSERT(!aLoadInfo.mInterfaceRequestor);

  aLoadInfo.mInterfaceRequestor =
    new WorkerLoadInfo::InterfaceRequestor(aLoadInfo.mPrincipal,
                                           aLoadInfo.mLoadGroup);
  aLoadInfo.mInterfaceRequestor->MaybeAddTabChild(aLoadInfo.mLoadGroup);

  // NOTE: this defaults the load context to:
  //  - private browsing = false
  //  - content = true
  //  - use remote tabs = false
  nsCOMPtr<nsILoadGroup> loadGroup =
    do_CreateInstance(NS_LOADGROUP_CONTRACTID);

  nsresult rv =
    loadGroup->SetNotificationCallbacks(aLoadInfo.mInterfaceRequestor);
  MOZ_ALWAYS_SUCCEEDS(rv);

  aLoadInfo.mLoadGroup = loadGroup.forget();

  MOZ_ASSERT(NS_LoadGroupMatchesPrincipal(aLoadInfo.mLoadGroup,
                                          aLoadInfo.mPrincipal));
}

void
WorkerPrivate::DoRunLoop(JSContext* aCx)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(mThread);

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

  Maybe<JSAutoCompartment> workerCompartment;

  for (;;) {
    Status currentStatus, previousStatus;
    bool debuggerRunnablesPending = false;
    bool normalRunnablesPending = false;

    {
      MutexAutoLock lock(mMutex);
      previousStatus = mStatus;

      while (mControlQueue.IsEmpty() &&
             !(debuggerRunnablesPending = !mDebuggerQueue.IsEmpty()) &&
             !(normalRunnablesPending = NS_HasPendingEvents(mThread))) {
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
    if (currentStatus != Running && !HasActiveHolders()) {

      // If we just changed status, we must schedule the current runnables.
      if (previousStatus != Running && currentStatus != Killing) {
        NotifyInternal(aCx, Killing);
        MOZ_ASSERT(!JS_IsExceptionPending(aCx));

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
          WorkerControlRunnable* runnable;
          while (mControlQueue.Pop(runnable)) {
            runnable->Cancel();
            runnable->Release();
          }
        }

        // Unroot the globals
        mScope = nullptr;
        mDebuggerScope = nullptr;

        return;
      }
    }

    if (debuggerRunnablesPending || normalRunnablesPending) {
      // Start the periodic GC timer if it is not already running.
      SetGCTimerMode(PeriodicTimer);
    }

    if (debuggerRunnablesPending) {
      WorkerRunnable* runnable;

      {
        MutexAutoLock lock(mMutex);

        mDebuggerQueue.Pop(runnable);
        debuggerRunnablesPending = !mDebuggerQueue.IsEmpty();
      }

      MOZ_ASSERT(runnable);
      static_cast<nsIRunnable*>(runnable)->Run();
      runnable->Release();

      // Flush the promise queue.
      Promise::PerformWorkerDebuggerMicroTaskCheckpoint();

      if (debuggerRunnablesPending) {
        WorkerDebuggerGlobalScope* globalScope = DebuggerGlobalScope();
        MOZ_ASSERT(globalScope);

        // Now *might* be a good time to GC. Let the JS engine make the decision.
        JSAutoCompartment ac(aCx, globalScope->GetGlobalJSObject());
        JS_MaybeGC(aCx);
      }
    } else if (normalRunnablesPending) {
      // Process a single runnable from the main queue.
      NS_ProcessNextEvent(mThread, false);

      normalRunnablesPending = NS_HasPendingEvents(mThread);
      if (normalRunnablesPending && GlobalScope()) {
        // Now *might* be a good time to GC. Let the JS engine make the decision.
        JSAutoCompartment ac(aCx, GlobalScope()->GetGlobalJSObject());
        JS_MaybeGC(aCx);
      }
    }

    if (!debuggerRunnablesPending && !normalRunnablesPending) {
      // Both the debugger event queue and the normal event queue has been
      // exhausted, cancel the periodic GC timer and schedule the idle GC timer.
      SetGCTimerMode(IdleTimer);
    }

    // If the worker thread is spamming the main thread faster than it can
    // process the work, then pause the worker thread until the MT catches
    // up.
    if (mMainThreadThrottledEventQueue &&
        mMainThreadThrottledEventQueue->Length() > 5000) {
      mMainThreadThrottledEventQueue->AwaitIdle();
    }
  }

  MOZ_CRASH("Shouldn't get here!");
}

void
WorkerPrivate::OnProcessNextEvent()
{
  AssertIsOnWorkerThread();

  uint32_t recursionDepth = CycleCollectedJSContext::Get()->RecursionDepth();
  MOZ_ASSERT(recursionDepth);

  // Normally we process control runnables in DoRunLoop or RunCurrentSyncLoop.
  // However, it's possible that non-worker C++ could spin its own nested event
  // loop, and in that case we must ensure that we continue to process control
  // runnables here.
  if (recursionDepth > 1 &&
      mSyncLoopStack.Length() < recursionDepth - 1) {
    Unused << ProcessAllControlRunnables();
    // There's no running JS, and no state to revalidate, so we can ignore the
    // return value.
  }
}

void
WorkerPrivate::AfterProcessNextEvent()
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(CycleCollectedJSContext::Get()->RecursionDepth());
}

void
WorkerPrivate::MaybeDispatchLoadFailedRunnable()
{
  AssertIsOnWorkerThread();

  nsCOMPtr<nsIRunnable> runnable = StealLoadFailedAsyncRunnable();
  if (!runnable) {
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(DispatchToMainThread(runnable.forget()));
}

nsIEventTarget*
WorkerPrivate::MainThreadEventTarget()
{
  return mMainThreadEventTarget;
}

nsresult
WorkerPrivate::DispatchToMainThread(nsIRunnable* aRunnable, uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> r = aRunnable;
  return DispatchToMainThread(r.forget(), aFlags);
}

nsresult
WorkerPrivate::DispatchToMainThread(already_AddRefed<nsIRunnable> aRunnable,
                                    uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> runnable = aRunnable;
  if (nsCOMPtr<nsINamed> named = do_QueryInterface(runnable)) {
    named->SetName("WorkerRunnable");
  }
  return mMainThreadEventTarget->Dispatch(runnable.forget(), aFlags);
}

nsIEventTarget*
WorkerPrivate::ControlEventTarget()
{
  return mWorkerControlEventTarget;
}

void
WorkerPrivate::InitializeGCTimers()
{
  AssertIsOnWorkerThread();

  // We need a timer for GC. The basic plan is to run a non-shrinking GC
  // periodically (PERIODIC_GC_TIMER_DELAY_SEC) while the worker is running.
  // Once the worker goes idle we set a short (IDLE_GC_TIMER_DELAY_SEC) timer to
  // run a shrinking GC. If the worker receives more messages then the short
  // timer is canceled and the periodic timer resumes.
  mGCTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  MOZ_ASSERT(mGCTimer);

  mPeriodicGCTimerRunning = false;
  mIdleGCTimerRunning = false;
}

void
WorkerPrivate::SetGCTimerMode(GCTimerMode aMode)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(mGCTimer);

  if ((aMode == PeriodicTimer && mPeriodicGCTimerRunning) ||
      (aMode == IdleTimer && mIdleGCTimerRunning)) {
    return;
  }

  MOZ_ALWAYS_SUCCEEDS(mGCTimer->Cancel());

  mPeriodicGCTimerRunning = false;
  mIdleGCTimerRunning = false;
  LOG(WorkerLog(),
      ("Worker %p canceled GC timer because %s\n", this,
       aMode == PeriodicTimer ?
       "periodic" :
       aMode == IdleTimer ? "idle" : "none"));

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
    name = "dom::workers::PeriodicGCTimerCallback";
  }
  else {
    delay = IDLE_GC_TIMER_DELAY_SEC * 1000;
    type = nsITimer::TYPE_ONE_SHOT;
    callback = IdleGCTimerCallback;
    name = "dom::workers::IdleGCTimerCallback";
  }

  MOZ_ALWAYS_SUCCEEDS(mGCTimer->SetTarget(mWorkerControlEventTarget));
  MOZ_ALWAYS_SUCCEEDS(
    mGCTimer->InitWithNamedFuncCallback(callback, this, delay, type, name));

  if (aMode == PeriodicTimer) {
    LOG(WorkerLog(), ("Worker %p scheduled periodic GC timer\n", this));
    mPeriodicGCTimerRunning = true;
  }
  else {
    LOG(WorkerLog(), ("Worker %p scheduled idle GC timer\n", this));
    mIdleGCTimerRunning = true;
  }
}

void
WorkerPrivate::ShutdownGCTimers()
{
  AssertIsOnWorkerThread();

  MOZ_ASSERT(mGCTimer);

  // Always make sure the timer is canceled.
  MOZ_ALWAYS_SUCCEEDS(mGCTimer->Cancel());

  LOG(WorkerLog(), ("Worker %p killed the GC timer\n", this));

  mGCTimer = nullptr;
  mPeriodicGCTimerRunning = false;
  mIdleGCTimerRunning = false;
}

bool
WorkerPrivate::InterruptCallback(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  MOZ_ASSERT(!JS_IsExceptionPending(aCx));

  bool mayContinue = true;
  bool scheduledIdleGC = false;

  for (;;) {
    // Run all control events now.
    auto result = ProcessAllControlRunnables();
    if (result == ProcessAllControlRunnablesResult::Abort) {
      mayContinue = false;
    }

    bool mayFreeze = mFrozen;
    if (mayFreeze) {
      MutexAutoLock lock(mMutex);
      mayFreeze = mStatus <= Running;
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

      WaitForWorkerEvents(PR_MillisecondsToInterval(UINT32_MAX));
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

nsresult
WorkerPrivate::IsOnCurrentThread(bool* aIsOnCurrentThread)
{
  // May be called on any thread!

  MOZ_ASSERT(aIsOnCurrentThread);
  MOZ_ASSERT(mPRThread);

  *aIsOnCurrentThread = PR_GetCurrentThread() == mPRThread;
  return NS_OK;
}

void
WorkerPrivate::ScheduleDeletion(WorkerRanOrNot aRanOrNot)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(mChildWorkers.IsEmpty());
  MOZ_ASSERT(mSyncLoopStack.IsEmpty());
  MOZ_ASSERT(!mPendingEventQueueClearing);

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
  }
  else {
    RefPtr<TopLevelWorkerFinishedRunnable> runnable =
      new TopLevelWorkerFinishedRunnable(this);
    if (NS_FAILED(DispatchToMainThread(runnable.forget()))) {
      NS_WARNING("Failed to dispatch runnable!");
    }
  }
}

bool
WorkerPrivate::CollectRuntimeStats(JS::RuntimeStats* aRtStats,
                                   bool aAnonymize)
{
  AssertIsOnWorkerThread();
  NS_ASSERTION(aRtStats, "Null RuntimeStats!");
  NS_ASSERTION(mJSContext, "This must never be null!");

  return JS::CollectRuntimeStats(mJSContext, aRtStats, nullptr, aAnonymize);
}

void
WorkerPrivate::EnableMemoryReporter()
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(!mMemoryReporter);

  // No need to lock here since the main thread can't race until we've
  // successfully registered the reporter.
  mMemoryReporter = new MemoryReporter(this);

  if (NS_FAILED(RegisterWeakAsyncMemoryReporter(mMemoryReporter))) {
    NS_WARNING("Failed to register memory reporter!");
    // No need to lock here since a failed registration means our memory
    // reporter can't start running. Just clean up.
    mMemoryReporter = nullptr;
  }
}

void
WorkerPrivate::DisableMemoryReporter()
{
  AssertIsOnWorkerThread();

  RefPtr<MemoryReporter> memoryReporter;
  {
    // Mutex protectes MemoryReporter::mWorkerPrivate which is cleared by
    // MemoryReporter::Disable() below.
    MutexAutoLock lock(mMutex);

    // There is nothing to do here if the memory reporter was never successfully
    // registered.
    if (!mMemoryReporter) {
      return;
    }

    // We don't need this set any longer. Swap it out so that we can unregister
    // below.
    mMemoryReporter.swap(memoryReporter);

    // Next disable the memory reporter so that the main thread stops trying to
    // signal us.
    memoryReporter->Disable();
  }

  // Finally unregister the memory reporter.
  if (NS_FAILED(UnregisterWeakMemoryReporter(memoryReporter))) {
    NS_WARNING("Failed to unregister memory reporter!");
  }
}

void
WorkerPrivate::WaitForWorkerEvents(PRIntervalTime aInterval)
{
  AssertIsOnWorkerThread();
  mMutex.AssertCurrentThreadOwns();

  // Wait for a worker event.
  mCondVar.Wait(aInterval);
}

WorkerPrivate::ProcessAllControlRunnablesResult
WorkerPrivate::ProcessAllControlRunnablesLocked()
{
  AssertIsOnWorkerThread();
  mMutex.AssertCurrentThreadOwns();

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

void
WorkerPrivate::ClearMainEventQueue(WorkerRanOrNot aRanOrNot)
{
  AssertIsOnWorkerThread();

  MOZ_ASSERT(mSyncLoopStack.IsEmpty());
  MOZ_ASSERT(!mCancelAllPendingRunnables);
  mCancelAllPendingRunnables = true;

  if (WorkerNeverRan == aRanOrNot) {
    for (uint32_t count = mPreStartRunnables.Length(), index = 0;
         index < count;
         index++) {
      RefPtr<WorkerRunnable> runnable = mPreStartRunnables[index].forget();
      static_cast<nsIRunnable*>(runnable.get())->Run();
    }
  } else {
    nsIThread* currentThread = NS_GetCurrentThread();
    MOZ_ASSERT(currentThread);

    NS_ProcessPendingEvents(currentThread);
  }

  MOZ_ASSERT(mCancelAllPendingRunnables);
  mCancelAllPendingRunnables = false;
}

void
WorkerPrivate::ClearDebuggerEventQueue()
{
  while (!mDebuggerQueue.IsEmpty()) {
    WorkerRunnable* runnable;
    mDebuggerQueue.Pop(runnable);
    // It should be ok to simply release the runnable, without running it.
    runnable->Release();
  }
}

bool
WorkerPrivate::FreezeInternal()
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(!mFrozen, "Already frozen!");

  mFrozen = true;

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->Freeze(nullptr);
  }

  return true;
}

bool
WorkerPrivate::ThawInternal()
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(mFrozen, "Not yet frozen!");

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->Thaw(nullptr);
  }

  mFrozen = false;
  return true;
}

void
WorkerPrivate::TraverseTimeouts(nsCycleCollectionTraversalCallback& cb)
{
  for (uint32_t i = 0; i < mTimeouts.Length(); ++i) {
    TimeoutInfo* tmp = mTimeouts[i];
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHandler)
  }
}

void
WorkerPrivate::UnlinkTimeouts()
{
  mTimeouts.Clear();
}

bool
WorkerPrivate::ModifyBusyCountFromWorker(bool aIncrease)
{
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

bool
WorkerPrivate::AddChildWorker(ParentType* aChildWorker)
{
  AssertIsOnWorkerThread();

#ifdef DEBUG
  {
    Status currentStatus;
    {
      MutexAutoLock lock(mMutex);
      currentStatus = mStatus;
    }

    MOZ_ASSERT(currentStatus == Running);
  }
#endif

  NS_ASSERTION(!mChildWorkers.Contains(aChildWorker),
               "Already know about this one!");
  mChildWorkers.AppendElement(aChildWorker);

  return mChildWorkers.Length() == 1 ?
         ModifyBusyCountFromWorker(true) :
         true;
}

void
WorkerPrivate::RemoveChildWorker(ParentType* aChildWorker)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(mChildWorkers.Contains(aChildWorker),
               "Didn't know about this one!");
  mChildWorkers.RemoveElement(aChildWorker);

  if (mChildWorkers.IsEmpty() && !ModifyBusyCountFromWorker(false)) {
    NS_WARNING("Failed to modify busy count!");
  }
}

bool
WorkerPrivate::AddHolder(WorkerHolder* aHolder, Status aFailStatus)
{
  AssertIsOnWorkerThread();

  {
    MutexAutoLock lock(mMutex);

    if (mStatus >= aFailStatus) {
      return false;
    }
  }

  MOZ_ASSERT(!mHolders.Contains(aHolder), "Already know about this one!");

  if (aHolder->GetBehavior() == WorkerHolder::PreventIdleShutdownStart) {
    if (!mNumHoldersPreventingShutdownStart && !ModifyBusyCountFromWorker(true)) {
      return false;
    }
    mNumHoldersPreventingShutdownStart += 1;
  }

  mHolders.AppendElement(aHolder);
  return true;
}

void
WorkerPrivate::RemoveHolder(WorkerHolder* aHolder)
{
  AssertIsOnWorkerThread();

  MOZ_ASSERT(mHolders.Contains(aHolder), "Didn't know about this one!");
  mHolders.RemoveElement(aHolder);

  if (aHolder->GetBehavior() == WorkerHolder::PreventIdleShutdownStart) {
    mNumHoldersPreventingShutdownStart -= 1;
    if (!mNumHoldersPreventingShutdownStart && !ModifyBusyCountFromWorker(false)) {
      NS_WARNING("Failed to modify busy count!");
    }
  }
}

void
WorkerPrivate::NotifyHolders(JSContext* aCx, Status aStatus)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(!JS_IsExceptionPending(aCx));

  NS_ASSERTION(aStatus > Running, "Bad status!");

  if (aStatus >= Closing) {
    CancelAllTimeouts();
  }

  nsTObserverArray<WorkerHolder*>::ForwardIterator iter(mHolders);
  while (iter.HasMore()) {
    WorkerHolder* holder = iter.GetNext();
    if (!holder->Notify(aStatus)) {
      NS_WARNING("Failed to notify holder!");
    }
    MOZ_ASSERT(!JS_IsExceptionPending(aCx));
  }

  AutoTArray<ParentType*, 10> children;
  children.AppendElements(mChildWorkers);

  for (uint32_t index = 0; index < children.Length(); index++) {
    if (!children[index]->Notify(aStatus)) {
      NS_WARNING("Failed to notify child worker!");
    }
  }
}

void
WorkerPrivate::CancelAllTimeouts()
{
  AssertIsOnWorkerThread();

  LOG(TimeoutsLog(), ("Worker %p CancelAllTimeouts.\n", this));

  if (mTimerRunning) {
    NS_ASSERTION(mTimer && mTimerRunnable, "Huh?!");
    NS_ASSERTION(!mTimeouts.IsEmpty(), "Huh?!");

    if (NS_FAILED(mTimer->Cancel())) {
      NS_WARNING("Failed to cancel timer!");
    }

    for (uint32_t index = 0; index < mTimeouts.Length(); index++) {
      mTimeouts[index]->mCanceled = true;
    }

    // If mRunningExpiredTimeouts, then the fact that they are all canceled now
    // means that the currently executing RunExpiredTimeouts will deal with
    // them.  Otherwise, we need to clean them up ourselves.
    if (!mRunningExpiredTimeouts) {
      mTimeouts.Clear();
      ModifyBusyCountFromWorker(false);
    }

    // Set mTimerRunning false even if mRunningExpiredTimeouts is true, so that
    // if we get reentered under this same RunExpiredTimeouts call we don't
    // assert above that !mTimeouts().IsEmpty(), because that's clearly false
    // now.
    mTimerRunning = false;
  }
#ifdef DEBUG
  else if (!mRunningExpiredTimeouts) {
    NS_ASSERTION(mTimeouts.IsEmpty(), "Huh?!");
  }
#endif

  mTimer = nullptr;
  mTimerRunnable = nullptr;
}

already_AddRefed<nsIEventTarget>
WorkerPrivate::CreateNewSyncLoop(Status aFailStatus)
{
  AssertIsOnWorkerThread();

  {
    MutexAutoLock lock(mMutex);

    if (mStatus >= aFailStatus) {
      return nullptr;
    }
  }

  nsCOMPtr<nsIThreadInternal> thread = do_QueryInterface(NS_GetCurrentThread());
  MOZ_ASSERT(thread);

  nsCOMPtr<nsIEventTarget> realEventTarget;
  MOZ_ALWAYS_SUCCEEDS(thread->PushEventQueue(getter_AddRefs(realEventTarget)));

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

bool
WorkerPrivate::RunCurrentSyncLoop()
{
  AssertIsOnWorkerThread();

  JSContext* cx = GetJSContext();
  MOZ_ASSERT(cx);

  // This should not change between now and the time we finish running this sync
  // loop.
  uint32_t currentLoopIndex = mSyncLoopStack.Length() - 1;

  SyncLoopInfo* loopInfo = mSyncLoopStack[currentLoopIndex];

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
        while (mControlQueue.IsEmpty() &&
               !normalRunnablesPending &&
               !(normalRunnablesPending = NS_HasPendingEvents(mThread))) {
          WaitForWorkerEvents();
        }

        auto result = ProcessAllControlRunnablesLocked();
        if (result != ProcessAllControlRunnablesResult::Nothing) {
          // XXXkhuey how should we handle Abort here? See Bug 1003730.

          // The state of the world may have changed. Recheck it.
          normalRunnablesPending = NS_HasPendingEvents(mThread);

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
      if (JS::CurrentGlobalOrNull(cx)) {
        JS_MaybeGC(cx);
      }
    }
  }

  // Make sure that the stack didn't change underneath us.
  MOZ_ASSERT(mSyncLoopStack[currentLoopIndex] == loopInfo);

  return DestroySyncLoop(currentLoopIndex);
}

bool
WorkerPrivate::DestroySyncLoop(uint32_t aLoopIndex, nsIThreadInternal* aThread)
{
  MOZ_ASSERT(!mSyncLoopStack.IsEmpty());
  MOZ_ASSERT(mSyncLoopStack.Length() - 1 == aLoopIndex);

  if (!aThread) {
    aThread = mThread;
  }

  // We're about to delete the loop, stash its event target and result.
  SyncLoopInfo* loopInfo = mSyncLoopStack[aLoopIndex];
  nsIEventTarget* nestedEventTarget =
    loopInfo->mEventTarget->GetWeakNestedEventTarget();
  MOZ_ASSERT(nestedEventTarget);

  bool result = loopInfo->mResult;

  {
    // Modifications must be protected by mMutex in DEBUG builds, see comment
    // about mSyncLoopStack in WorkerPrivate.h.
#ifdef DEBUG
    MutexAutoLock lock(mMutex);
#endif

    // This will delete |loopInfo|!
    mSyncLoopStack.RemoveElementAt(aLoopIndex);
  }

  MOZ_ALWAYS_SUCCEEDS(aThread->PopEventQueue(nestedEventTarget));

  if (mSyncLoopStack.IsEmpty() && mPendingEventQueueClearing) {
    mPendingEventQueueClearing = false;
    ClearMainEventQueue(WorkerRan);
  }

  return result;
}

void
WorkerPrivate::StopSyncLoop(nsIEventTarget* aSyncLoopTarget, bool aResult)
{
  AssertIsOnWorkerThread();
  AssertValidSyncLoop(aSyncLoopTarget);

  MOZ_ASSERT(!mSyncLoopStack.IsEmpty());

  for (uint32_t index = mSyncLoopStack.Length(); index > 0; index--) {
    nsAutoPtr<SyncLoopInfo>& loopInfo = mSyncLoopStack[index - 1];
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
void
WorkerPrivate::AssertValidSyncLoop(nsIEventTarget* aSyncLoopTarget)
{
  MOZ_ASSERT(aSyncLoopTarget);

  EventTarget* workerTarget;
  nsresult rv =
    aSyncLoopTarget->QueryInterface(kDEBUGWorkerEventTargetIID,
                                    reinterpret_cast<void**>(&workerTarget));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(workerTarget);

  bool valid = false;

  {
    MutexAutoLock lock(mMutex);

    for (uint32_t index = 0; index < mSyncLoopStack.Length(); index++) {
      nsAutoPtr<SyncLoopInfo>& loopInfo = mSyncLoopStack[index];
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

void
WorkerPrivate::PostMessageToParentInternal(
                            JSContext* aCx,
                            JS::Handle<JS::Value> aMessage,
                            const Sequence<JSObject*>& aTransferable,
                            ErrorResult& aRv)
{
  AssertIsOnWorkerThread();

  JS::Rooted<JS::Value> transferable(aCx, JS::UndefinedValue());

  aRv = nsContentUtils::CreateJSValueFromSequenceOfObject(aCx, aTransferable,
                                                          &transferable);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  RefPtr<MessageEventRunnable> runnable =
    new MessageEventRunnable(this,
                             WorkerRunnable::ParentThreadUnchangedBusyCount);

  UniquePtr<AbstractTimelineMarker> start;
  UniquePtr<AbstractTimelineMarker> end;
  RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
  bool isTimelineRecording = timelines && !timelines->IsEmpty();

  if (isTimelineRecording) {
    start = MakeUnique<WorkerTimelineMarker>(NS_IsMainThread()
      ? ProfileTimelineWorkerOperationType::SerializeDataOnMainThread
      : ProfileTimelineWorkerOperationType::SerializeDataOffMainThread,
      MarkerTracingType::START);
  }

  runnable->Write(aCx, aMessage, transferable, JS::CloneDataPolicy(), aRv);

  if (isTimelineRecording) {
    end = MakeUnique<WorkerTimelineMarker>(NS_IsMainThread()
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

void
WorkerPrivate::EnterDebuggerEventLoop()
{
  AssertIsOnWorkerThread();

  JSContext* cx = GetJSContext();
  MOZ_ASSERT(cx);

  uint32_t currentEventLoopLevel = ++mDebuggerEventLoopLevel;

  while (currentEventLoopLevel <= mDebuggerEventLoopLevel) {
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

      while (mControlQueue.IsEmpty() &&
             !(debuggerRunnablesPending = !mDebuggerQueue.IsEmpty())) {
        WaitForWorkerEvents();
      }

      ProcessAllControlRunnablesLocked();

      // XXXkhuey should we abort JS on the stack here if we got Abort above?
    }

    if (debuggerRunnablesPending) {
      // Start the periodic GC timer if it is not already running.
      SetGCTimerMode(PeriodicTimer);

      WorkerRunnable* runnable;

      {
        MutexAutoLock lock(mMutex);

        mDebuggerQueue.Pop(runnable);
      }

      MOZ_ASSERT(runnable);
      static_cast<nsIRunnable*>(runnable)->Run();
      runnable->Release();

      // Flush the promise queue.
      Promise::PerformWorkerDebuggerMicroTaskCheckpoint();

      // Now *might* be a good time to GC. Let the JS engine make the decision.
      if (JS::CurrentGlobalOrNull(cx)) {
        JS_MaybeGC(cx);
      }
    }
  }
}

void
WorkerPrivate::LeaveDebuggerEventLoop()
{
  AssertIsOnWorkerThread();

  MutexAutoLock lock(mMutex);

  if (mDebuggerEventLoopLevel > 0) {
    --mDebuggerEventLoopLevel;
  }
}

void
WorkerPrivate::PostMessageToDebugger(const nsAString& aMessage)
{
  mDebugger->PostMessageToDebugger(aMessage);
}

void
WorkerPrivate::SetDebuggerImmediate(dom::Function& aHandler, ErrorResult& aRv)
{
  AssertIsOnWorkerThread();

  RefPtr<DebuggerImmediateRunnable> runnable =
    new DebuggerImmediateRunnable(this, aHandler);
  if (!runnable->Dispatch()) {
    aRv.Throw(NS_ERROR_FAILURE);
  }
}

void
WorkerPrivate::ReportErrorToDebugger(const nsAString& aFilename,
                                     uint32_t aLineno,
                                     const nsAString& aMessage)
{
  mDebugger->ReportErrorToDebugger(aFilename, aLineno, aMessage);
}

bool
WorkerPrivate::NotifyInternal(JSContext* aCx, Status aStatus)
{
  AssertIsOnWorkerThread();

  NS_ASSERTION(aStatus > Running && aStatus < Dead, "Bad status!");

  RefPtr<EventTarget> eventTarget;

  // Save the old status and set the new status.
  Status previousStatus;
  {
    MutexAutoLock lock(mMutex);

    if (mStatus >= aStatus) {
      MOZ_ASSERT(!mEventTarget);
      return true;
    }

    previousStatus = mStatus;
    mStatus = aStatus;

    // Mark parent status as closing immediately to avoid new events being
    // dispatched after we clear the queue below.
    if (aStatus == Closing) {
      Close();
    }

    mEventTarget.swap(eventTarget);
  }

  // Now that mStatus > Running, no-one can create a new WorkerEventTarget or
  // WorkerCrossThreadDispatcher if we don't already have one.
  if (eventTarget) {
    // Since we'll no longer process events, make sure we no longer allow anyone
    // to post them. We have to do this without mMutex held, since our mutex
    // must be acquired *after* the WorkerEventTarget's mutex when they're both
    // held.
    eventTarget->Disable();
    eventTarget = nullptr;
  }

  if (mCrossThreadDispatcher) {
    // Since we'll no longer process events, make sure we no longer allow
    // anyone to post them. We have to do this without mMutex held, since our
    // mutex must be acquired *after* mCrossThreadDispatcher's mutex when
    // they're both held.
    mCrossThreadDispatcher->Forget();
    mCrossThreadDispatcher = nullptr;
  }

  MOZ_ASSERT(previousStatus != Pending);

  // Let all our holders know the new status.
  NotifyHolders(aCx, aStatus);
  MOZ_ASSERT(!JS_IsExceptionPending(aCx));

  // If this is the first time our status has changed then we need to clear the
  // main event queue.
  if (previousStatus == Running) {
    // NB: If we're in a sync loop, we can't clear the queue immediately,
    // because this is the wrong queue. So we have to defer it until later.
    if (!mSyncLoopStack.IsEmpty()) {
      mPendingEventQueueClearing = true;
    } else {
      ClearMainEventQueue(WorkerRan);
    }
  }

  // If the worker script never ran, or failed to compile, we don't need to do
  // anything else.
  if (!GlobalScope()) {
    return true;
  }

  // Don't abort the script.
  if (aStatus == Closing) {
    return true;
  }

  MOZ_ASSERT(aStatus == Terminating ||
             aStatus == Canceling ||
             aStatus == Killing);

  // Always abort the script.
  return false;
}

void
WorkerErrorBase::AssignErrorBase(JSErrorBase* aReport)
{
  mFilename = NS_ConvertUTF8toUTF16(aReport->filename);
  mLineNumber = aReport->lineno;
  mColumnNumber = aReport->column;
  mErrorNumber = aReport->errorNumber;
}

void
WorkerErrorNote::AssignErrorNote(JSErrorNotes::Note* aNote)
{
  WorkerErrorBase::AssignErrorBase(aNote);
  xpc::ErrorNote::ErrorNoteToMessageString(aNote, mMessage);
}

void
WorkerErrorReport::AssignErrorReport(JSErrorReport* aReport)
{
  WorkerErrorBase::AssignErrorBase(aReport);
  xpc::ErrorReport::ErrorReportToMessageString(aReport, mMessage);

  mLine.Assign(aReport->linebuf(), aReport->linebufLength());
  mFlags = aReport->flags;
  MOZ_ASSERT(aReport->exnType >= JSEXN_FIRST && aReport->exnType < JSEXN_LIMIT);
  mExnType = JSExnType(aReport->exnType);
  mMutedError = aReport->isMuted;

  if (aReport->notes) {
    if (!mNotes.SetLength(aReport->notes->length(), fallible)) {
      return;
    }

    size_t i = 0;
    for (auto&& note : *aReport->notes) {
      mNotes.ElementAt(i).AssignErrorNote(note.get());
      i++;
    }
  }
}

void
WorkerPrivate::ReportError(JSContext* aCx, JS::ConstUTF8CharsZ aToStringResult,
                           JSErrorReport* aReport)
{
  AssertIsOnWorkerThread();

  if (!MayContinueRunning() || mErrorHandlerRecursionCount == 2) {
    return;
  }

  NS_ASSERTION(mErrorHandlerRecursionCount == 0 ||
               mErrorHandlerRecursionCount == 1,
               "Bad recursion logic!");

  JS::Rooted<JS::Value> exn(aCx);
  if (!JS_GetPendingException(aCx, &exn)) {
    // Probably shouldn't actually happen?  But let's go ahead and just use null
    // for lack of anything better.
    exn.setNull();
  }
  JS_ClearPendingException(aCx);

  WorkerErrorReport report;
  if (aReport) {
    report.AssignErrorReport(aReport);
  }
  else {
    report.mFlags = nsIScriptError::errorFlag | nsIScriptError::exceptionFlag;
  }

  if (report.mMessage.IsEmpty() && aToStringResult) {
    nsDependentCString toStringResult(aToStringResult.c_str());
    if (!AppendUTF8toUTF16(toStringResult, report.mMessage, mozilla::fallible)) {
      // Try again, with only a 1 KB string. Do this infallibly this time.
      // If the user doesn't have 1 KB to spare we're done anyways.
      uint32_t index = std::min(uint32_t(1024), toStringResult.Length());

      // Drop the last code point that may be cropped.
      index = RewindToPriorUTF8Codepoint(toStringResult.BeginReading(), index);

      nsDependentCString truncatedToStringResult(aToStringResult.c_str(),
                                                 index);
      AppendUTF8toUTF16(truncatedToStringResult, report.mMessage);
    }
  }

  mErrorHandlerRecursionCount++;

  // Don't want to run the scope's error handler if this is a recursive error or
  // if we ran out of memory.
  bool fireAtScope = mErrorHandlerRecursionCount == 1 &&
                     report.mErrorNumber != JSMSG_OUT_OF_MEMORY &&
                     JS::CurrentGlobalOrNull(aCx);

  ReportErrorRunnable::ReportError(aCx, this, fireAtScope, nullptr, report, 0,
                                   exn);

  mErrorHandlerRecursionCount--;
}

// static
void
WorkerPrivate::ReportErrorToConsole(const char* aMessage)
{
  WorkerPrivate* wp = nullptr;
  if (!NS_IsMainThread()) {
    wp = GetCurrentThreadWorkerPrivate();
  }

  ReportErrorToConsoleRunnable::Report(wp, aMessage);
}

int32_t
WorkerPrivate::SetTimeout(JSContext* aCx,
                          nsIScriptTimeoutHandler* aHandler,
                          int32_t aTimeout, bool aIsInterval,
                          ErrorResult& aRv)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(aHandler);

  const int32_t timerId = mNextTimeoutId++;

  Status currentStatus;
  {
    MutexAutoLock lock(mMutex);
    currentStatus = mStatus;
  }

  // If the worker is trying to call setTimeout/setInterval and the parent
  // thread has initiated the close process then just silently fail.
  if (currentStatus >= Closing) {
    aRv.Throw(NS_ERROR_FAILURE);
    return 0;
  }

  nsAutoPtr<TimeoutInfo> newInfo(new TimeoutInfo());
  newInfo->mIsInterval = aIsInterval;
  newInfo->mId = timerId;

  if (MOZ_UNLIKELY(timerId == INT32_MAX)) {
    NS_WARNING("Timeout ids overflowed!");
    mNextTimeoutId = 1;
  }

  newInfo->mHandler = aHandler;

  // See if any of the optional arguments were passed.
  aTimeout = std::max(0, aTimeout);
  newInfo->mInterval = TimeDuration::FromMilliseconds(aTimeout);

  newInfo->mTargetTime = TimeStamp::Now() + newInfo->mInterval;

  nsAutoPtr<TimeoutInfo>* insertedInfo =
    mTimeouts.InsertElementSorted(newInfo.forget(), GetAutoPtrComparator(mTimeouts));

  LOG(TimeoutsLog(), ("Worker %p has new timeout: delay=%d interval=%s\n",
                      this, aTimeout, aIsInterval ? "yes" : "no"));

  // If the timeout we just made is set to fire next then we need to update the
  // timer, unless we're currently running timeouts.
  if (insertedInfo == mTimeouts.Elements() && !mRunningExpiredTimeouts) {
    nsresult rv;

    if (!mTimer) {
      mTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return 0;
      }

      mTimerRunnable = new TimerRunnable(this);
    }

    if (!mTimerRunning) {
      if (!ModifyBusyCountFromWorker(true)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return 0;
      }
      mTimerRunning = true;
    }

    if (!RescheduleTimeoutTimer(aCx)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return 0;
    }
  }

  return timerId;
}

void
WorkerPrivate::ClearTimeout(int32_t aId)
{
  AssertIsOnWorkerThread();

  if (!mTimeouts.IsEmpty()) {
    NS_ASSERTION(mTimerRunning, "Huh?!");

    for (uint32_t index = 0; index < mTimeouts.Length(); index++) {
      nsAutoPtr<TimeoutInfo>& info = mTimeouts[index];
      if (info->mId == aId) {
        info->mCanceled = true;
        break;
      }
    }
  }
}

bool
WorkerPrivate::RunExpiredTimeouts(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  // We may be called recursively (e.g. close() inside a timeout) or we could
  // have been canceled while this event was pending, bail out if there is
  // nothing to do.
  if (mRunningExpiredTimeouts || !mTimerRunning) {
    return true;
  }

  NS_ASSERTION(mTimer && mTimerRunnable, "Must have a timer!");
  NS_ASSERTION(!mTimeouts.IsEmpty(), "Should have some work to do!");

  bool retval = true;

  AutoPtrComparator<TimeoutInfo> comparator = GetAutoPtrComparator(mTimeouts);
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));

  // We want to make sure to run *something*, even if the timer fired a little
  // early. Fudge the value of now to at least include the first timeout.
  const TimeStamp actual_now = TimeStamp::Now();
  const TimeStamp now = std::max(actual_now, mTimeouts[0]->mTargetTime);

  if (now != actual_now) {
    LOG(TimeoutsLog(), ("Worker %p fudged timeout by %f ms.\n", this,
                        (now - actual_now).ToMilliseconds()));
  }

  AutoTArray<TimeoutInfo*, 10> expiredTimeouts;
  for (uint32_t index = 0; index < mTimeouts.Length(); index++) {
    nsAutoPtr<TimeoutInfo>& info = mTimeouts[index];
    if (info->mTargetTime > now) {
      break;
    }
    expiredTimeouts.AppendElement(info);
  }

  // Guard against recursion.
  mRunningExpiredTimeouts = true;

  // Run expired timeouts.
  for (uint32_t index = 0; index < expiredTimeouts.Length(); index++) {
    TimeoutInfo*& info = expiredTimeouts[index];

    if (info->mCanceled) {
      continue;
    }

    LOG(TimeoutsLog(), ("Worker %p executing timeout with original delay %f ms.\n",
                        this, info->mInterval.ToMilliseconds()));

    // Always check JS_IsExceptionPending if something fails, and if
    // JS_IsExceptionPending returns false (i.e. uncatchable exception) then
    // break out of the loop.
    const char *reason;
    if (info->mIsInterval) {
      reason = "setInterval handler";
    } else {
      reason = "setTimeout handler";
    }

    RefPtr<Function> callback = info->mHandler->GetCallback();
    if (!callback) {
      // scope for the AutoEntryScript, so it comes off the stack before we do
      // Promise::PerformMicroTaskCheckpoint.
      AutoEntryScript aes(global, reason, false);

      // Evaluate the timeout expression.
      const nsAString& script = info->mHandler->GetHandlerText();

      const char* filename = nullptr;
      uint32_t lineNo = 0, dummyColumn = 0;
      info->mHandler->GetLocation(&filename, &lineNo, &dummyColumn);

      JS::CompileOptions options(aes.cx());
      options.setFileAndLine(filename, lineNo).setNoScriptRval(true);

      JS::Rooted<JS::Value> unused(aes.cx());

      if (!JS::Evaluate(aes.cx(), options, script.BeginReading(),
                        script.Length(), &unused) &&
          !JS_IsExceptionPending(aCx)) {
        retval = false;
        break;
      }
    } else {
      ErrorResult rv;
      JS::Rooted<JS::Value> ignoredVal(aCx);
      callback->Call(GlobalScope(), info->mHandler->GetArgs(), &ignoredVal, rv,
                     reason);
      if (rv.IsUncatchableException()) {
        rv.SuppressException();
        retval = false;
        break;
      }

      rv.SuppressException();
    }

    // Since we might be processing more timeouts, go ahead and flush
    // the promise queue now before we do that.
    Promise::PerformWorkerMicroTaskCheckpoint();

    NS_ASSERTION(mRunningExpiredTimeouts, "Someone changed this!");
  }

  // No longer possible to be called recursively.
  mRunningExpiredTimeouts = false;

  // Now remove canceled and expired timeouts from the main list.
  // NB: The timeouts present in expiredTimeouts must have the same order
  // with respect to each other in mTimeouts.  That is, mTimeouts is just
  // expiredTimeouts with extra elements inserted.  There may be unexpired
  // timeouts that have been inserted between the expired timeouts if the
  // timeout event handler called setTimeout/setInterval.
  for (uint32_t index = 0, expiredTimeoutIndex = 0,
       expiredTimeoutLength = expiredTimeouts.Length();
       index < mTimeouts.Length(); ) {
    nsAutoPtr<TimeoutInfo>& info = mTimeouts[index];
    if ((expiredTimeoutIndex < expiredTimeoutLength &&
         info == expiredTimeouts[expiredTimeoutIndex] &&
         ++expiredTimeoutIndex) ||
        info->mCanceled) {
      if (info->mIsInterval && !info->mCanceled) {
        // Reschedule intervals.
        info->mTargetTime = info->mTargetTime + info->mInterval;
        // Don't resort the list here, we'll do that at the end.
        ++index;
      }
      else {
        mTimeouts.RemoveElement(info);
      }
    }
    else {
      // If info did not match the current entry in expiredTimeouts, it
      // shouldn't be there at all.
      NS_ASSERTION(!expiredTimeouts.Contains(info),
                   "Our timeouts are out of order!");
      ++index;
    }
  }

  mTimeouts.Sort(comparator);

  // Either signal the parent that we're no longer using timeouts or reschedule
  // the timer.
  if (mTimeouts.IsEmpty()) {
    if (!ModifyBusyCountFromWorker(false)) {
      retval = false;
    }
    mTimerRunning = false;
  }
  else if (retval && !RescheduleTimeoutTimer(aCx)) {
    retval = false;
  }

  return retval;
}

bool
WorkerPrivate::RescheduleTimeoutTimer(JSContext* aCx)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(!mRunningExpiredTimeouts);
  NS_ASSERTION(!mTimeouts.IsEmpty(), "Should have some timeouts!");
  NS_ASSERTION(mTimer && mTimerRunnable, "Should have a timer!");

  // NB: This is important! The timer may have already fired, e.g. if a timeout
  // callback itself calls setTimeout for a short duration and then takes longer
  // than that to finish executing. If that has happened, it's very important
  // that we don't execute the event that is now pending in our event queue, or
  // our code in RunExpiredTimeouts to "fudge" the timeout value will unleash an
  // early timeout when we execute the event we're about to queue.
  mTimer->Cancel();

  double delta =
    (mTimeouts[0]->mTargetTime - TimeStamp::Now()).ToMilliseconds();
  uint32_t delay = delta > 0 ? std::min(delta, double(UINT32_MAX)) : 0;

  LOG(TimeoutsLog(), ("Worker %p scheduled timer for %d ms, %" PRIuSIZE " pending timeouts\n",
                      this, delay, mTimeouts.Length()));

  nsresult rv = mTimer->InitWithCallback(mTimerRunnable, delay, nsITimer::TYPE_ONE_SHOT);
  if (NS_FAILED(rv)) {
    JS_ReportErrorASCII(aCx, "Failed to start timer!");
    return false;
  }

  return true;
}

void
WorkerPrivate::UpdateContextOptionsInternal(
                                    JSContext* aCx,
                                    const JS::ContextOptions& aContextOptions)
{
  AssertIsOnWorkerThread();

  JS::ContextOptionsRef(aCx) = aContextOptions;

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateContextOptions(aContextOptions);
  }
}

void
WorkerPrivate::UpdateLanguagesInternal(const nsTArray<nsString>& aLanguages)
{
  WorkerGlobalScope* globalScope = GlobalScope();
  if (globalScope) {
    RefPtr<WorkerNavigator> nav = globalScope->GetExistingNavigator();
    if (nav) {
      nav->SetLanguages(aLanguages);
    }
  }

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateLanguages(aLanguages);
  }
}

void
WorkerPrivate::UpdatePreferenceInternal(WorkerPreference aPref, bool aValue)
{
  AssertIsOnWorkerThread();
  MOZ_ASSERT(aPref >= 0 && aPref < WORKERPREF_COUNT);

  mPreferences[aPref] = aValue;

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdatePreference(aPref, aValue);
  }
}

void
WorkerPrivate::UpdateJSWorkerMemoryParameterInternal(JSContext* aCx,
                                                     JSGCParamKey aKey,
                                                     uint32_t aValue)
{
  AssertIsOnWorkerThread();

  // XXX aValue might be 0 here (telling us to unset a previous value for child
  // workers). Calling JS_SetGCParameter with a value of 0 isn't actually
  // supported though. We really need some way to revert to a default value
  // here.
  if (aValue) {
    JS_SetGCParameter(aCx, aKey, aValue);
  }

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateJSWorkerMemoryParameter(aKey, aValue);
  }
}

#ifdef JS_GC_ZEAL
void
WorkerPrivate::UpdateGCZealInternal(JSContext* aCx, uint8_t aGCZeal,
                                    uint32_t aFrequency)
{
  AssertIsOnWorkerThread();

  JS_SetGCZeal(aCx, aGCZeal, aFrequency);

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->UpdateGCZeal(aGCZeal, aFrequency);
  }
}
#endif

void
WorkerPrivate::GarbageCollectInternal(JSContext* aCx, bool aShrinking,
                                      bool aCollectChildren)
{
  AssertIsOnWorkerThread();

  if (!GlobalScope()) {
    // We haven't compiled anything yet. Just bail out.
    return;
  }

  if (aShrinking || aCollectChildren) {
    JS::PrepareForFullGC(aCx);

    if (aShrinking) {
      JS::GCForReason(aCx, GC_SHRINK, JS::gcreason::DOM_WORKER);

      if (!aCollectChildren) {
        LOG(WorkerLog(), ("Worker %p collected idle garbage\n", this));
      }
    }
    else {
      JS::GCForReason(aCx, GC_NORMAL, JS::gcreason::DOM_WORKER);
      LOG(WorkerLog(), ("Worker %p collected garbage\n", this));
    }
  }
  else {
    JS_MaybeGC(aCx);
    LOG(WorkerLog(), ("Worker %p collected periodic garbage\n", this));
  }

  if (aCollectChildren) {
    for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
      mChildWorkers[index]->GarbageCollect(aShrinking);
    }
  }
}

void
WorkerPrivate::CycleCollectInternal(bool aCollectChildren)
{
  AssertIsOnWorkerThread();

  nsCycleCollector_collect(nullptr);

  if (aCollectChildren) {
    for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
      mChildWorkers[index]->CycleCollect(/* dummy = */ false);
    }
  }
}

void
WorkerPrivate::MemoryPressureInternal()
{
  AssertIsOnWorkerThread();

  RefPtr<Console> console = mScope ? mScope->GetConsoleIfExists() : nullptr;
  if (console) {
    console->ClearStorage();
  }

  console = mDebuggerScope ? mDebuggerScope->GetConsoleIfExists() : nullptr;
  if (console) {
    console->ClearStorage();
  }

  for (uint32_t index = 0; index < mChildWorkers.Length(); index++) {
    mChildWorkers[index]->MemoryPressure(false);
  }
}

void
WorkerPrivate::SetThread(WorkerThread* aThread)
{
  if (aThread) {
#ifdef DEBUG
    {
      bool isOnCurrentThread;
      MOZ_ASSERT(NS_SUCCEEDED(aThread->IsOnCurrentThread(&isOnCurrentThread)));
      MOZ_ASSERT(isOnCurrentThread);
    }
#endif

    MOZ_ASSERT(!mPRThread);
    mPRThread = PRThreadFromThread(aThread);
    MOZ_ASSERT(mPRThread);
  }
  else {
    MOZ_ASSERT(mPRThread);
  }

  const WorkerThreadFriendKey friendKey;

  RefPtr<WorkerThread> doomedThread;

  { // Scope so that |doomedThread| is released without holding the lock.
    MutexAutoLock lock(mMutex);

    if (aThread) {
      MOZ_ASSERT(!mThread);
      MOZ_ASSERT(mStatus == Pending);

      mThread = aThread;
      mThread->SetWorker(friendKey, this);

      if (!mPreStartRunnables.IsEmpty()) {
        for (uint32_t index = 0; index < mPreStartRunnables.Length(); index++) {
          MOZ_ALWAYS_SUCCEEDS(
            mThread->DispatchAnyThread(friendKey, mPreStartRunnables[index].forget()));
        }
        mPreStartRunnables.Clear();
      }
    }
    else {
      MOZ_ASSERT(mThread);

      mThread->SetWorker(friendKey, nullptr);

      mThread.swap(doomedThread);
    }
  }
}

WorkerCrossThreadDispatcher*
WorkerPrivate::GetCrossThreadDispatcher()
{
  MutexAutoLock lock(mMutex);

  if (!mCrossThreadDispatcher && mStatus <= Running) {
    mCrossThreadDispatcher = new WorkerCrossThreadDispatcher(this);
  }

  return mCrossThreadDispatcher;
}

void
WorkerPrivate::BeginCTypesCall()
{
  AssertIsOnWorkerThread();

  // Don't try to GC while we're blocked in a ctypes call.
  SetGCTimerMode(NoTimer);
}

void
WorkerPrivate::EndCTypesCall()
{
  AssertIsOnWorkerThread();

  // Make sure the periodic timer is running before we start running JS again.
  SetGCTimerMode(PeriodicTimer);
}

bool
WorkerPrivate::ConnectMessagePort(JSContext* aCx,
                                  MessagePortIdentifier& aIdentifier)
{
  AssertIsOnWorkerThread();

  WorkerGlobalScope* globalScope = GlobalScope();

  JS::Rooted<JSObject*> jsGlobal(aCx, globalScope->GetWrapper());
  MOZ_ASSERT(jsGlobal);

  // This MessagePortIdentifier is used to create a new port, still connected
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
  init.mBubbles = false;
  init.mCancelable = false;
  init.mSource.SetValue().SetAsMessagePort() = port;
  if (!init.mPorts.AppendElement(port.forget(), fallible)) {
    return false;
  }

  RefPtr<MessageEvent> event =
    MessageEvent::Constructor(globalObject,
                              NS_LITERAL_STRING("connect"), init, rv);

  event->SetTrusted(true);

  nsCOMPtr<nsIDOMEvent> domEvent = do_QueryObject(event);

  nsEventStatus dummy = nsEventStatus_eIgnore;
  globalScope->DispatchDOMEvent(nullptr, domEvent, nullptr, &dummy);

  return true;
}

WorkerGlobalScope*
WorkerPrivate::GetOrCreateGlobalScope(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  if (!mScope) {
    RefPtr<WorkerGlobalScope> globalScope;
    if (IsSharedWorker()) {
      globalScope = new SharedWorkerGlobalScope(this, WorkerName());
    } else if (IsServiceWorker()) {
      globalScope = new ServiceWorkerGlobalScope(this, ServiceWorkerScope());
    } else {
      globalScope = new DedicatedWorkerGlobalScope(this, WorkerName());
    }

    JS::Rooted<JSObject*> global(aCx);
    NS_ENSURE_TRUE(globalScope->WrapGlobalObject(aCx, &global), nullptr);

    JSAutoCompartment ac(aCx, global);

    // RegisterBindings() can spin a nested event loop so we have to set mScope
    // before calling it, and we have to make sure to unset mScope if it fails.
    mScope = Move(globalScope);

    if (!RegisterBindings(aCx, global)) {
      mScope = nullptr;
      return nullptr;
    }

    JS_FireOnNewGlobalObject(aCx, global);
  }

  return mScope;
}

WorkerDebuggerGlobalScope*
WorkerPrivate::CreateDebuggerGlobalScope(JSContext* aCx)
{
  AssertIsOnWorkerThread();

  MOZ_ASSERT(!mDebuggerScope);

  RefPtr<WorkerDebuggerGlobalScope> globalScope =
    new WorkerDebuggerGlobalScope(this);

  JS::Rooted<JSObject*> global(aCx);
  NS_ENSURE_TRUE(globalScope->WrapGlobalObject(aCx, &global), nullptr);

  JSAutoCompartment ac(aCx, global);

  // RegisterDebuggerBindings() can spin a nested event loop so we have to set
  // mDebuggerScope before calling it, and we have to make sure to unset
  // mDebuggerScope if it fails.
  mDebuggerScope = Move(globalScope);

  if (!RegisterDebuggerBindings(aCx, global)) {
    mDebuggerScope = nullptr;
    return nullptr;
  }

  JS_FireOnNewGlobalObject(aCx, global);

  return mDebuggerScope;
}

#ifdef DEBUG

void
WorkerPrivate::AssertIsOnWorkerThread() const
{
  // This is much more complicated than it needs to be but we can't use mThread
  // because it must be protected by mMutex and sometimes this method is called
  // when mMutex is already locked. This method should always work.
  MOZ_ASSERT(mPRThread,
             "AssertIsOnWorkerThread() called before a thread was assigned!");

  nsCOMPtr<nsIThread> thread;
  nsresult rv =
    nsThreadManager::get().GetThreadFromPRThread(mPRThread,
                                                 getter_AddRefs(thread));
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(thread);

  bool current;
  rv = thread->IsOnCurrentThread(&current);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(current, "Wrong thread!");
}

#endif // DEBUG

NS_IMPL_ISUPPORTS_INHERITED0(ExternalRunnableWrapper, WorkerRunnable)

template <class Derived>
NS_IMPL_ADDREF(WorkerPrivateParent<Derived>::EventTarget)

template <class Derived>
NS_IMPL_RELEASE(WorkerPrivateParent<Derived>::EventTarget)

template <class Derived>
NS_INTERFACE_MAP_BEGIN(WorkerPrivateParent<Derived>::EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsIEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
#ifdef DEBUG
  // kDEBUGWorkerEventTargetIID is special in that it does not AddRef its
  // result.
  if (aIID.Equals(kDEBUGWorkerEventTargetIID)) {
    *aInstancePtr = this;
    return NS_OK;
  }
  else
#endif
NS_INTERFACE_MAP_END

template <class Derived>
NS_IMETHODIMP
WorkerPrivateParent<Derived>::
EventTarget::DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> event(aRunnable);
  return Dispatch(event.forget(), aFlags);
}

template <class Derived>
NS_IMETHODIMP
WorkerPrivateParent<Derived>::
EventTarget::Dispatch(already_AddRefed<nsIRunnable> aRunnable, uint32_t aFlags)
{
  // May be called on any thread!
  nsCOMPtr<nsIRunnable> event(aRunnable);

  // Workers only support asynchronous dispatch for now.
  if (NS_WARN_IF(aFlags != NS_DISPATCH_NORMAL)) {
    return NS_ERROR_UNEXPECTED;
  }

  RefPtr<WorkerRunnable> workerRunnable;

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    NS_WARNING("A runnable was posted to a worker that is already shutting "
               "down!");
    return NS_ERROR_UNEXPECTED;
  }

  if (event) {
    workerRunnable = mWorkerPrivate->MaybeWrapAsWorkerRunnable(event.forget());
  }

  nsresult rv =
    mWorkerPrivate->DispatchPrivate(workerRunnable.forget(), mNestedEventTarget);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

template <class Derived>
NS_IMETHODIMP
WorkerPrivateParent<Derived>::
EventTarget::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

template <class Derived>
NS_IMETHODIMP
WorkerPrivateParent<Derived>::
EventTarget::IsOnCurrentThread(bool* aIsOnCurrentThread)
{
  // May be called on any thread!

  MOZ_ASSERT(aIsOnCurrentThread);

  MutexAutoLock lock(mMutex);

  if (!mWorkerPrivate) {
    NS_WARNING("A worker's event target was used after the worker has !");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = mWorkerPrivate->IsOnCurrentThread(aIsOnCurrentThread);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

BEGIN_WORKERS_NAMESPACE

WorkerCrossThreadDispatcher*
GetWorkerCrossThreadDispatcher(JSContext* aCx, const JS::Value& aWorker)
{
  if (!aWorker.isObject()) {
    return nullptr;
  }

  WorkerPrivate* w = nullptr;
  UNWRAP_OBJECT(Worker, &aWorker.toObject(), w);
  MOZ_ASSERT(w);
  return w->GetCrossThreadDispatcher();
}

// Force instantiation.
template class WorkerPrivateParent<WorkerPrivate>;

END_WORKERS_NAMESPACE
