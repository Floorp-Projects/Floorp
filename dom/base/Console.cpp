/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Console.h"
#include "mozilla/dom/ConsoleBinding.h"

#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/Maybe.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDocument.h"
#include "nsDOMNavigationTiming.h"
#include "nsGlobalWindow.h"
#include "nsJSUtils.h"
#include "nsPerformance.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "xpcprivate.h"
#include "nsContentUtils.h"

#include "nsIConsoleAPIStorage.h"
#include "nsIDOMWindowUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIWebNavigation.h"

// The maximum allowed number of concurrent timers per page.
#define MAX_PAGE_TIMERS 10000

// The maximum allowed number of concurrent counters per page.
#define MAX_PAGE_COUNTERS 10000

// The maximum stacktrace depth when populating the stacktrace array used for
// console.trace().
#define DEFAULT_MAX_STACKTRACE_DEPTH 200

// The console API methods are async and their action is executed later. This
// delay tells how much later.
#define CALL_DELAY 15 // milliseconds

// This constant tells how many messages to process in a single timer execution.
#define MESSAGES_IN_INTERVAL 1500

// This tag is used in the Structured Clone Algorithm to move js values from
// worker thread to main thread
#define CONSOLE_TAG JS_SCTAG_USER_MIN

using namespace mozilla::dom::exceptions;
using namespace mozilla::dom::workers;

namespace mozilla {
namespace dom {

/**
 * Console API in workers uses the Structured Clone Algorithm to move any value
 * from the worker thread to the main-thread. Some object cannot be moved and,
 * in these cases, we convert them to strings.
 * It's not the best, but at least we are able to show something.
 */

// This method is called by the Structured Clone Algorithm when some data has
// to be read.
static JSObject*
ConsoleStructuredCloneCallbacksRead(JSContext* aCx,
                                    JSStructuredCloneReader* /* unused */,
                                    uint32_t aTag, uint32_t aData,
                                    void* aClosure)
{
  AssertIsOnMainThread();

  if (aTag != CONSOLE_TAG) {
    return nullptr;
  }

  nsTArray<nsString>* strings = static_cast<nsTArray<nsString>*>(aClosure);
  MOZ_ASSERT(strings->Length() > aData);

  JS::Rooted<JS::Value> value(aCx);
  if (!xpc::StringToJsval(aCx, strings->ElementAt(aData), &value)) {
    return nullptr;
  }

  JS::Rooted<JSObject*> obj(aCx);
  if (!JS_ValueToObject(aCx, value, &obj)) {
    return nullptr;
  }

  return obj;
}

// This method is called by the Structured Clone Algorithm when some data has
// to be written.
static bool
ConsoleStructuredCloneCallbacksWrite(JSContext* aCx,
                                     JSStructuredCloneWriter* aWriter,
                                     JS::Handle<JSObject*> aObj,
                                     void* aClosure)
{
  JS::Rooted<JS::Value> value(aCx, JS::ObjectOrNullValue(aObj));
  JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, value));
  if (!jsString) {
    return false;
  }

  nsDependentJSString string;
  if (!string.init(aCx, jsString)) {
    return false;
  }

  nsTArray<nsString>* strings = static_cast<nsTArray<nsString>*>(aClosure);

  if (!JS_WriteUint32Pair(aWriter, CONSOLE_TAG, strings->Length())) {
    return false;
  }

  strings->AppendElement(string);

  return true;
}

static void
ConsoleStructuredCloneCallbacksError(JSContext* /* aCx */,
                                     uint32_t /* aErrorId */)
{
  NS_WARNING("Failed to clone data for the Console API in workers.");
}

JSStructuredCloneCallbacks gConsoleCallbacks = {
  ConsoleStructuredCloneCallbacksRead,
  ConsoleStructuredCloneCallbacksWrite,
  ConsoleStructuredCloneCallbacksError
};

class ConsoleCallData MOZ_FINAL : public LinkedListElement<ConsoleCallData>
{
public:
  ConsoleCallData()
    : mMethodName(Console::MethodLog)
    , mPrivate(false)
    , mTimeStamp(JS_Now() / PR_USEC_PER_MSEC)
    , mMonotonicTimer(0)
  {
    MOZ_COUNT_CTOR(ConsoleCallData);
  }

  ~ConsoleCallData()
  {
    MOZ_COUNT_DTOR(ConsoleCallData);
  }

  void
  Initialize(JSContext* aCx, Console::MethodName aName,
             const nsAString& aString, const Sequence<JS::Value>& aArguments)
  {
    mGlobal = JS::CurrentGlobalOrNull(aCx);
    mMethodName = aName;
    mMethodString = aString;

    for (uint32_t i = 0; i < aArguments.Length(); ++i) {
      mArguments.AppendElement(aArguments[i]);
    }
  }

  JS::Heap<JSObject*> mGlobal;

  Console::MethodName mMethodName;
  bool mPrivate;
  int64_t mTimeStamp;
  DOMHighResTimeStamp mMonotonicTimer;

  nsString mMethodString;
  nsTArray<JS::Heap<JS::Value>> mArguments;

  // Stack management is complicated, because we want to do it as
  // lazily as possible.  Therefore, we have the following behavior:
  // 1)  mTopStackFrame is initialized whenever we have any JS on the stack
  // 2)  mReifiedStack is initialized if we're created in a worker.
  // 3)  mStack is set (possibly to null if there is no JS on the stack) if
  //     we're created on main thread.
  Maybe<ConsoleStackEntry> mTopStackFrame;
  Maybe<nsTArray<ConsoleStackEntry>> mReifiedStack;
  nsCOMPtr<nsIStackFrame> mStack;
};

// This class is used to clear any exception at the end of this method.
class ClearException
{
public:
  ClearException(JSContext* aCx)
    : mCx(aCx)
  {
  }

  ~ClearException()
  {
    JS_ClearPendingException(mCx);
  }

private:
  JSContext* mCx;
};

class ConsoleRunnable : public nsRunnable
{
public:
  ConsoleRunnable()
    : mWorkerPrivate(GetCurrentThreadWorkerPrivate())
  {
    MOZ_ASSERT(mWorkerPrivate);
  }

  virtual
  ~ConsoleRunnable()
  {
  }

  bool
  Dispatch()
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    JSContext* cx = mWorkerPrivate->GetJSContext();

    if (!PreDispatch(cx)) {
      return false;
    }

    AutoSyncLoopHolder syncLoop(mWorkerPrivate);
    mSyncLoopTarget = syncLoop.EventTarget();

    if (NS_FAILED(NS_DispatchToMainThread(this))) {
      JS_ReportError(cx,
                     "Failed to dispatch to main thread for the Console API!");
      return false;
    }

    return syncLoop.Run();
  }

private:
  NS_IMETHOD Run()
  {
    AssertIsOnMainThread();

    RunConsole();

    nsRefPtr<MainThreadStopSyncLoopRunnable> response =
      new MainThreadStopSyncLoopRunnable(mWorkerPrivate,
                                         mSyncLoopTarget.forget(),
                                         true);
    if (!response->Dispatch(nullptr)) {
      NS_WARNING("Failed to dispatch response!");
    }

    return NS_OK;
  }

protected:
  virtual bool
  PreDispatch(JSContext* aCx) = 0;

  virtual void
  RunConsole() = 0;

  WorkerPrivate* mWorkerPrivate;

private:
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;
};

// This runnable appends a CallData object into the Console queue running on
// the main-thread.
class ConsoleCallDataRunnable MOZ_FINAL : public ConsoleRunnable
{
public:
  ConsoleCallDataRunnable(ConsoleCallData* aCallData)
    : mCallData(aCallData)
  {
  }

private:
  bool
  PreDispatch(JSContext* aCx) MOZ_OVERRIDE
  {
    ClearException ce(aCx);
    JSAutoCompartment ac(aCx, mCallData->mGlobal);

    JS::Rooted<JSObject*> arguments(aCx,
      JS_NewArrayObject(aCx, mCallData->mArguments.Length()));
    if (!arguments) {
      return false;
    }

    JS::Rooted<JS::Value> arg(aCx);
    for (uint32_t i = 0; i < mCallData->mArguments.Length(); ++i) {
      arg = mCallData->mArguments[i];
      if (!JS_DefineElement(aCx, arguments, i, arg, JSPROP_ENUMERATE)) {
        return false;
      }
    }

    JS::Rooted<JS::Value> value(aCx, JS::ObjectValue(*arguments));

    if (!mArguments.write(aCx, value, &gConsoleCallbacks, &mStrings)) {
      return false;
    }

    mCallData->mArguments.Clear();
    mCallData->mGlobal = nullptr;
    return true;
  }

  void
  RunConsole() MOZ_OVERRIDE
  {
    // Walk up to our containing page
    WorkerPrivate* wp = mWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    AutoPushJSContext cx(wp->ParentJSContext());
    ClearException ce(cx);

    nsPIDOMWindow* window = wp->GetWindow();
    NS_ENSURE_TRUE_VOID(window);

    nsRefPtr<nsGlobalWindow> win = static_cast<nsGlobalWindow*>(window);
    NS_ENSURE_TRUE_VOID(win);

    ErrorResult error;
    nsRefPtr<Console> console = win->GetConsole(error);
    if (error.Failed()) {
      NS_WARNING("Failed to get console from the window.");
      return;
    }

    JS::Rooted<JS::Value> argumentsValue(cx);
    if (!mArguments.read(cx, &argumentsValue, &gConsoleCallbacks, &mStrings)) {
      return;
    }

    MOZ_ASSERT(argumentsValue.isObject());
    JS::Rooted<JSObject*> argumentsObj(cx, &argumentsValue.toObject());
    MOZ_ASSERT(JS_IsArrayObject(cx, argumentsObj));

    uint32_t length;
    if (!JS_GetArrayLength(cx, argumentsObj, &length)) {
      return;
    }

    for (uint32_t i = 0; i < length; ++i) {
      JS::Rooted<JS::Value> value(cx);

      if (!JS_GetElement(cx, argumentsObj, i, &value)) {
        return;
      }

      mCallData->mArguments.AppendElement(value);
    }

    MOZ_ASSERT(mCallData->mArguments.Length() == length);

    mCallData->mGlobal = JS::CurrentGlobalOrNull(cx);
    console->AppendCallData(mCallData.forget());
  }

private:
  nsAutoPtr<ConsoleCallData> mCallData;

  JSAutoStructuredCloneBuffer mArguments;
  nsTArray<nsString> mStrings;
};

// This runnable calls ProfileMethod() on the console on the main-thread.
class ConsoleProfileRunnable MOZ_FINAL : public ConsoleRunnable
{
public:
  ConsoleProfileRunnable(const nsAString& aAction,
                         const Sequence<JS::Value>& aArguments)
    : mAction(aAction)
    , mArguments(aArguments)
  {
  }

private:
  bool
  PreDispatch(JSContext* aCx) MOZ_OVERRIDE
  {
    ClearException ce(aCx);

    JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
    if (!global) {
      return false;
    }

    JSAutoCompartment ac(aCx, global);

    JS::Rooted<JSObject*> arguments(aCx,
      JS_NewArrayObject(aCx, mArguments.Length()));
    if (!arguments) {
      return false;
    }

    JS::Rooted<JS::Value> arg(aCx);
    for (uint32_t i = 0; i < mArguments.Length(); ++i) {
      arg = mArguments[i];
      if (!JS_DefineElement(aCx, arguments, i, arg, JSPROP_ENUMERATE)) {
        return false;
      }
    }

    JS::Rooted<JS::Value> value(aCx, JS::ObjectValue(*arguments));

    if (!mBuffer.write(aCx, value, &gConsoleCallbacks, &mStrings)) {
      return false;
    }

    return true;
  }

  void
  RunConsole() MOZ_OVERRIDE
  {
    // Walk up to our containing page
    WorkerPrivate* wp = mWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    AutoPushJSContext cx(wp->ParentJSContext());
    ClearException ce(cx);

    JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
    NS_ENSURE_TRUE_VOID(global);
    JSAutoCompartment ac(cx, global);

    nsPIDOMWindow* window = wp->GetWindow();
    NS_ENSURE_TRUE_VOID(window);

    nsRefPtr<nsGlobalWindow> win = static_cast<nsGlobalWindow*>(window);
    NS_ENSURE_TRUE_VOID(win);

    ErrorResult error;
    nsRefPtr<Console> console = win->GetConsole(error);
    if (error.Failed()) {
      NS_WARNING("Failed to get console from the window.");
      return;
    }

    JS::Rooted<JS::Value> argumentsValue(cx);
    if (!mBuffer.read(cx, &argumentsValue, &gConsoleCallbacks, &mStrings)) {
      return;
    }

    MOZ_ASSERT(argumentsValue.isObject());
    JS::Rooted<JSObject*> argumentsObj(cx, &argumentsValue.toObject());
    MOZ_ASSERT(JS_IsArrayObject(cx, argumentsObj));

    uint32_t length;
    if (!JS_GetArrayLength(cx, argumentsObj, &length)) {
      return;
    }

    Sequence<JS::Value> arguments;

    for (uint32_t i = 0; i < length; ++i) {
      JS::Rooted<JS::Value> value(cx);

      if (!JS_GetElement(cx, argumentsObj, i, &value)) {
        return;
      }

      arguments.AppendElement(value);
    }

    console->ProfileMethod(cx, mAction, arguments);
  }

private:
  nsString mAction;
  Sequence<JS::Value> mArguments;

  JSAutoStructuredCloneBuffer mBuffer;
  nsTArray<nsString> mStrings;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(Console)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Console)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTimer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mStorage)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER

  tmp->ClearConsoleData();

NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Console)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTimer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mStorage)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Console)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER

  for (ConsoleCallData* data = tmp->mQueuedCalls.getFirst(); data != nullptr;
       data = data->getNext()) {
    if (data->mGlobal) {
      aCallbacks.Trace(&data->mGlobal, "data->mGlobal", aClosure);
    }

    for (uint32_t i = 0; i < data->mArguments.Length(); ++i) {
      aCallbacks.Trace(&data->mArguments[i], "data->mArguments[i]", aClosure);
    }
  }

NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Console)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Console)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Console)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsITimerCallback)
NS_INTERFACE_MAP_END

Console::Console(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
  , mOuterID(0)
  , mInnerID(0)
{
  if (mWindow) {
    MOZ_ASSERT(mWindow->IsInnerWindow());
    mInnerID = mWindow->WindowID();

    nsPIDOMWindow* outerWindow = mWindow->GetOuterWindow();
    MOZ_ASSERT(outerWindow);
    mOuterID = outerWindow->WindowID();
  }

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->AddObserver(this, "inner-window-destroyed", false);
    }
  }

  SetIsDOMBinding();
  mozilla::HoldJSObjects(this);
}

Console::~Console()
{
  mozilla::DropJSObjects(this);
}

NS_IMETHODIMP
Console::Observe(nsISupports* aSubject, const char* aTopic,
                 const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (strcmp(aTopic, "inner-window-destroyed")) {
    return NS_OK;
  }

  nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
  NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

  uint64_t innerID;
  nsresult rv = wrapper->GetData(&innerID);
  NS_ENSURE_SUCCESS(rv, rv);

  if (innerID == mInnerID) {
    nsCOMPtr<nsIObserverService> obs =
      do_GetService("@mozilla.org/observer-service;1");
    if (obs) {
      obs->RemoveObserver(this, "inner-window-destroyed");
    }

    ClearConsoleData();
    mTimerRegistry.Clear();

    if (mTimer) {
      mTimer->Cancel();
      mTimer = nullptr;
    }
  }

  return NS_OK;
}

JSObject*
Console::WrapObject(JSContext* aCx)
{
  return ConsoleBinding::Wrap(aCx, this);
}

#define METHOD(name, string)                                          \
  void                                                                \
  Console::name(JSContext* aCx, const Sequence<JS::Value>& aData)     \
  {                                                                   \
    Method(aCx, Method##name, NS_LITERAL_STRING(string), aData);      \
  }

METHOD(Log, "log")
METHOD(Info, "info")
METHOD(Warn, "warn")
METHOD(Error, "error")
METHOD(Exception, "exception")
METHOD(Debug, "debug")

void
Console::Trace(JSContext* aCx)
{
  const Sequence<JS::Value> data;
  Method(aCx, MethodTrace, NS_LITERAL_STRING("trace"), data);
}

// Displays an interactive listing of all the properties of an object.
METHOD(Dir, "dir");

METHOD(Group, "group")
METHOD(GroupCollapsed, "groupCollapsed")
METHOD(GroupEnd, "groupEnd")

void
Console::Time(JSContext* aCx, const JS::Handle<JS::Value> aTime)
{
  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(aCx, &data);

  if (!aTime.isUndefined()) {
    data.AppendElement(aTime);
  }

  Method(aCx, MethodTime, NS_LITERAL_STRING("time"), data);
}

void
Console::TimeEnd(JSContext* aCx, const JS::Handle<JS::Value> aTime)
{
  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(aCx, &data);

  if (!aTime.isUndefined()) {
    data.AppendElement(aTime);
  }

  Method(aCx, MethodTimeEnd, NS_LITERAL_STRING("timeEnd"), data);
}

void
Console::Profile(JSContext* aCx, const Sequence<JS::Value>& aData)
{
  ProfileMethod(aCx, NS_LITERAL_STRING("profile"), aData);
}

void
Console::ProfileEnd(JSContext* aCx, const Sequence<JS::Value>& aData)
{
  ProfileMethod(aCx, NS_LITERAL_STRING("profileEnd"), aData);
}

void
Console::ProfileMethod(JSContext* aCx, const nsAString& aAction,
                       const Sequence<JS::Value>& aData)
{
  if (!NS_IsMainThread()) {
    // Here we are in a worker thread.
    nsRefPtr<ConsoleProfileRunnable> runnable =
      new ConsoleProfileRunnable(aAction, aData);
    runnable->Dispatch();
    return;
  }

  ClearException ce(aCx);

  RootedDictionary<ConsoleProfileEvent> event(aCx);
  event.mAction = aAction;

  event.mArguments.Construct();
  Sequence<JS::Value>& sequence = event.mArguments.Value();

  for (uint32_t i = 0; i < aData.Length(); ++i) {
    sequence.AppendElement(aData[i]);
  }

  JS::Rooted<JS::Value> eventValue(aCx);
  if (!event.ToObject(aCx, &eventValue)) {
    return;
  }

  JS::Rooted<JSObject*> eventObj(aCx, &eventValue.toObject());
  MOZ_ASSERT(eventObj);

  if (!JS_DefineProperty(aCx, eventObj, "wrappedJSObject", eventValue,
      JSPROP_ENUMERATE)) {
    return;
  }

  nsXPConnect*  xpc = nsXPConnect::XPConnect();
  nsCOMPtr<nsISupports> wrapper;
  const nsIID& iid = NS_GET_IID(nsISupports);

  if (NS_FAILED(xpc->WrapJS(aCx, eventObj, iid, getter_AddRefs(wrapper)))) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs =
    do_GetService("@mozilla.org/observer-service;1");
  if (obs) {
    obs->NotifyObservers(wrapper, "console-api-profiler", nullptr);
  }
}

void
Console::Assert(JSContext* aCx, bool aCondition,
                const Sequence<JS::Value>& aData)
{
  if (!aCondition) {
    Method(aCx, MethodAssert, NS_LITERAL_STRING("assert"), aData);
  }
}

METHOD(Count, "count")

void
Console::__noSuchMethod__()
{
  // Nothing to do.
}

static
nsresult
StackFrameToStackEntry(nsIStackFrame* aStackFrame,
                       ConsoleStackEntry& aStackEntry,
                       uint32_t aLanguage)
{
  MOZ_ASSERT(aStackFrame);

  nsresult rv = aStackFrame->GetFilename(aStackEntry.mFilename);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t lineNumber;
  rv = aStackFrame->GetLineNumber(&lineNumber);
  NS_ENSURE_SUCCESS(rv, rv);

  aStackEntry.mLineNumber = lineNumber;

  rv = aStackFrame->GetName(aStackEntry.mFunctionName);
  NS_ENSURE_SUCCESS(rv, rv);

  aStackEntry.mLanguage = aLanguage;
  return NS_OK;
}

static
nsresult
ReifyStack(nsIStackFrame* aStack, nsTArray<ConsoleStackEntry>& aRefiedStack)
{
  nsCOMPtr<nsIStackFrame> stack(aStack);

  while (stack) {
    uint32_t language;
    nsresult rv = stack->GetLanguage(&language);
    NS_ENSURE_SUCCESS(rv, rv);

    if (language == nsIProgrammingLanguage::JAVASCRIPT ||
        language == nsIProgrammingLanguage::JAVASCRIPT2) {
      ConsoleStackEntry& data = *aRefiedStack.AppendElement();
      rv = StackFrameToStackEntry(stack, data, language);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    nsCOMPtr<nsIStackFrame> caller;
    rv = stack->GetCaller(getter_AddRefs(caller));
    NS_ENSURE_SUCCESS(rv, rv);

    stack.swap(caller);
  }

  return NS_OK;
}

// Queue a call to a console method. See the CALL_DELAY constant.
void
Console::Method(JSContext* aCx, MethodName aMethodName,
                const nsAString& aMethodString,
                const Sequence<JS::Value>& aData)
{
  // This RAII class removes the last element of the mQueuedCalls if something
  // goes wrong.
  class RAII {
  public:
    RAII(LinkedList<ConsoleCallData>& aList)
      : mList(aList)
      , mUnfinished(true)
    {
    }

    ~RAII()
    {
      if (mUnfinished) {
        ConsoleCallData* data = mList.popLast();
        MOZ_ASSERT(data);
        delete data;
      }
    }

    void
    Finished()
    {
      mUnfinished = false;
    }

  private:
    LinkedList<ConsoleCallData>& mList;
    bool mUnfinished;
  };

  ConsoleCallData* callData = new ConsoleCallData();
  mQueuedCalls.insertBack(callData);

  ClearException ce(aCx);

  callData->Initialize(aCx, aMethodName, aMethodString, aData);
  RAII raii(mQueuedCalls);

  if (mWindow) {
    nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(mWindow);
    if (!webNav) {
      return;
    }

    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(webNav);
    MOZ_ASSERT(loadContext);

    loadContext->GetUsePrivateBrowsing(&callData->mPrivate);
  }

  uint32_t maxDepth = ShouldIncludeStackrace(aMethodName) ?
                      DEFAULT_MAX_STACKTRACE_DEPTH : 1;
  nsCOMPtr<nsIStackFrame> stack = CreateStack(aCx, maxDepth);

  if (!stack) {
    return;
  }

  // Walk up to the first JS stack frame and save it if we find it.
  do {
    uint32_t language;
    nsresult rv = stack->GetLanguage(&language);
    if (NS_FAILED(rv)) {
      return;
    }

    if (language == nsIProgrammingLanguage::JAVASCRIPT ||
        language == nsIProgrammingLanguage::JAVASCRIPT2) {
      callData->mTopStackFrame.construct();
      nsresult rv = StackFrameToStackEntry(stack,
                                           callData->mTopStackFrame.ref(),
                                           language);
      if (NS_FAILED(rv)) {
        return;
      }

      break;
    }

    nsCOMPtr<nsIStackFrame> caller;
    rv = stack->GetCaller(getter_AddRefs(caller));
    if (NS_FAILED(rv)) {
      return;
    }

    stack.swap(caller);
  } while (stack);

  if (NS_IsMainThread()) {
    callData->mStack = stack;
  } else {
    // nsIStackFrame is not threadsafe, so we need to snapshot it now,
    // before we post our runnable to the main thread.
    callData->mReifiedStack.construct();
    nsresult rv = ReifyStack(stack, callData->mReifiedStack.ref());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  // Monotonic timer for 'time' and 'timeEnd'
  if ((aMethodName == MethodTime || aMethodName == MethodTimeEnd)) {
    if (mWindow) {
      nsGlobalWindow *win = static_cast<nsGlobalWindow*>(mWindow.get());
      MOZ_ASSERT(win);

      ErrorResult rv;
      nsRefPtr<nsPerformance> performance = win->GetPerformance(rv);
      if (rv.Failed()) {
        return;
      }

      callData->mMonotonicTimer = performance->Now();
    } else {
      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(workerPrivate);

      TimeDuration duration =
        mozilla::TimeStamp::Now() - workerPrivate->CreationTimeStamp();

      callData->mMonotonicTimer = duration.ToMilliseconds();
    }
  }

  // The operation is completed. RAII class has to be disabled.
  raii.Finished();

  if (!NS_IsMainThread()) {
    // Here we are in a worker thread. The ConsoleCallData has to been removed
    // from the list and it will be deleted by the ConsoleCallDataRunnable or
    // by the Main-Thread Console object.
    mQueuedCalls.popLast();

    nsRefPtr<ConsoleCallDataRunnable> runnable =
      new ConsoleCallDataRunnable(callData);
    runnable->Dispatch();
    return;
  }

  if (!mTimer) {
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    mTimer->InitWithCallback(this, CALL_DELAY,
                             nsITimer::TYPE_REPEATING_SLACK);
  }
}

void
Console::AppendCallData(ConsoleCallData* aCallData)
{
  mQueuedCalls.insertBack(aCallData);

  if (!mTimer) {
    mTimer = do_CreateInstance("@mozilla.org/timer;1");
    mTimer->InitWithCallback(this, CALL_DELAY,
                             nsITimer::TYPE_REPEATING_SLACK);
  }
}

// Timer callback used to process each of the queued calls.
NS_IMETHODIMP
Console::Notify(nsITimer *timer)
{
  MOZ_ASSERT(!mQueuedCalls.isEmpty());

  for (uint32_t i = 0; i < MESSAGES_IN_INTERVAL; ++i) {
    ConsoleCallData* data = mQueuedCalls.popFirst();
    if (!data) {
      break;
    }

    ProcessCallData(data);
    delete data;
  }

  if (mQueuedCalls.isEmpty() && mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  return NS_OK;
}

// We store information to lazily compute the stack in the reserved slots of
// LazyStackGetter.  The first slot always stores a JS object: it's either the
// JS wrapper of the nsIStackFrame or the actual reified stack representation.
// The second slot is a PrivateValue() holding an nsIStackFrame* when we haven't
// reified the stack yet, or an UndefinedValue() otherwise.
enum {
  SLOT_STACKOBJ,
  SLOT_RAW_STACK
};

bool
LazyStackGetter(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);
  JS::Rooted<JSObject*> callee(aCx, &args.callee());

  JS::Value v = js::GetFunctionNativeReserved(&args.callee(), SLOT_RAW_STACK);
  if (v.isUndefined()) {
    // Already reified.
    args.rval().set(js::GetFunctionNativeReserved(callee, SLOT_STACKOBJ));
    return true;
  }

  nsIStackFrame* stack = reinterpret_cast<nsIStackFrame*>(v.toPrivate());
  nsTArray<ConsoleStackEntry> reifiedStack;
  nsresult rv = ReifyStack(stack, reifiedStack);
  if (NS_FAILED(rv)) {
    Throw(aCx, rv);
    return false;
  }

  JS::Rooted<JS::Value> stackVal(aCx);
  if (!ToJSValue(aCx, reifiedStack, &stackVal)) {
    return false;
  }

  MOZ_ASSERT(stackVal.isObject());

  js::SetFunctionNativeReserved(callee, SLOT_STACKOBJ, stackVal);
  js::SetFunctionNativeReserved(callee, SLOT_RAW_STACK, JS::UndefinedValue());

  args.rval().set(stackVal);
  return true;
}

void
Console::ProcessCallData(ConsoleCallData* aData)
{
  MOZ_ASSERT(aData);
  MOZ_ASSERT(NS_IsMainThread());

  ConsoleStackEntry frame;
  if (!aData->mTopStackFrame.empty()) {
    frame = aData->mTopStackFrame.ref();
  }

  AutoSafeJSContext cx;
  ClearException ce(cx);
  RootedDictionary<ConsoleEvent> event(cx);

  JSAutoCompartment ac(cx, aData->mGlobal);

  event.mID.Construct();
  event.mInnerID.Construct();
  if (mWindow) {
    event.mID.Value().SetAsUnsignedLong() = mOuterID;
    event.mInnerID.Value().SetAsUnsignedLong() = mInnerID;
  } else {
    // If we are in a JSM, the window doesn't exist.
    event.mID.Value().SetAsString() = NS_LITERAL_STRING("jsm");
    event.mInnerID.Value().SetAsString() = frame.mFilename;
  }

  event.mLevel = aData->mMethodString;
  event.mFilename = frame.mFilename;
  event.mLineNumber = frame.mLineNumber;
  event.mFunctionName = frame.mFunctionName;
  event.mTimeStamp = aData->mTimeStamp;
  event.mPrivate = aData->mPrivate;

  switch (aData->mMethodName) {
    case MethodLog:
    case MethodInfo:
    case MethodWarn:
    case MethodError:
    case MethodException:
    case MethodDebug:
    case MethodAssert:
      event.mArguments.Construct();
      event.mStyles.Construct();
      ProcessArguments(cx, aData->mArguments, event.mArguments.Value(),
                       event.mStyles.Value());
      break;

    default:
      event.mArguments.Construct();
      ArgumentsToValueList(aData->mArguments, event.mArguments.Value());
  }

  if (aData->mMethodName == MethodGroup ||
      aData->mMethodName == MethodGroupCollapsed ||
      aData->mMethodName == MethodGroupEnd) {
    ComposeGroupName(cx, aData->mArguments, event.mGroupName);
  }

  else if (aData->mMethodName == MethodTime && !aData->mArguments.IsEmpty()) {
    event.mTimer = StartTimer(cx, aData->mArguments[0], aData->mMonotonicTimer);
  }

  else if (aData->mMethodName == MethodTimeEnd && !aData->mArguments.IsEmpty()) {
    event.mTimer = StopTimer(cx, aData->mArguments[0], aData->mMonotonicTimer);
  }

  else if (aData->mMethodName == MethodCount) {
    event.mCounter = IncreaseCounter(cx, frame, aData->mArguments);
  }

  // We want to create a console event object and pass it to our
  // nsIConsoleAPIStorage implementation.  We want to define some accessor
  // properties on this object, and those will need to keep an nsIStackFrame
  // alive.  But nsIStackFrame cannot be wrapped in an untrusted scope.  And
  // further, passing untrusted objects to system code is likely to run afoul of
  // Object Xrays.  So we want to wrap in a system-principal scope here.  But
  // which one?  We could cheat and try to get the underlying JSObject* of
  // mStorage, but that's a bit fragile.  Instead, we just use the junk scope,
  // with explicit permission from the XPConnect module owner.  If you're
  // tempted to do that anywhere else, talk to said module owner first.
  JSAutoCompartment ac2(cx, xpc::GetJunkScope());

  JS::Rooted<JS::Value> eventValue(cx);
  if (!event.ToObject(cx, &eventValue)) {
    return;
  }

  JS::Rooted<JSObject*> eventObj(cx, &eventValue.toObject());
  MOZ_ASSERT(eventObj);

  if (!JS_DefineProperty(cx, eventObj, "wrappedJSObject", eventValue, JSPROP_ENUMERATE)) {
    return;
  }

  if (ShouldIncludeStackrace(aData->mMethodName)) {
    // Now define the "stacktrace" property on eventObj.  There are two cases
    // here.  Either we came from a worker and have a reified stack, or we want
    // to define a getter that will lazily reify the stack.
    if (!aData->mReifiedStack.empty()) {
      JS::Rooted<JS::Value> stacktrace(cx);
      if (!ToJSValue(cx, aData->mReifiedStack.ref(), &stacktrace) ||
          !JS_DefineProperty(cx, eventObj, "stacktrace", stacktrace,
                             JSPROP_ENUMERATE)) {
        return;
      }
    } else {
      JSFunction* fun = js::NewFunctionWithReserved(cx, LazyStackGetter, 0, 0,
                                                    eventObj, "stacktrace");
      if (!fun) {
        return;
      }

      JS::Rooted<JSObject*> funObj(cx, JS_GetFunctionObject(fun));

      // We want to store our stack in the function and have it stay alive.  But
      // we also need sane access to the C++ nsIStackFrame.  So store both a JS
      // wrapper and the raw pointer: the former will keep the latter alive.
      JS::Rooted<JS::Value> stackVal(cx);
      nsresult rv = nsContentUtils::WrapNative(cx, aData->mStack,
                                               &stackVal);
      if (NS_FAILED(rv)) {
        return;
      }

      js::SetFunctionNativeReserved(funObj, SLOT_STACKOBJ, stackVal);
      js::SetFunctionNativeReserved(funObj, SLOT_RAW_STACK,
                                    JS::PrivateValue(aData->mStack.get()));

      if (!JS_DefineProperty(cx, eventObj, "stacktrace",
                             JS::UndefinedHandleValue,
                             JSPROP_ENUMERATE | JSPROP_SHARED | JSPROP_GETTER |
                             JSPROP_SETTER,
                             JS_DATA_TO_FUNC_PTR(JSPropertyOp, funObj.get()),
                             nullptr)) {
        return;
      }
    }
  }

  if (!mStorage) {
    mStorage = do_GetService("@mozilla.org/consoleAPI-storage;1");
  }

  if (!mStorage) {
    NS_WARNING("Failed to get the ConsoleAPIStorage service.");
    return;
  }

  nsAutoString innerID;
  innerID.AppendInt(mInnerID);

  if (NS_FAILED(mStorage->RecordEvent(innerID, eventValue))) {
    NS_WARNING("Failed to record a console event.");
  }

  nsXPConnect*  xpc = nsXPConnect::XPConnect();
  nsCOMPtr<nsISupports> wrapper;
  const nsIID& iid = NS_GET_IID(nsISupports);

  if (NS_FAILED(xpc->WrapJS(cx, eventObj, iid, getter_AddRefs(wrapper)))) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs =
    do_GetService("@mozilla.org/observer-service;1");
  if (obs) {
    nsAutoString outerID;
    outerID.AppendInt(mOuterID);

    obs->NotifyObservers(wrapper, "console-api-log-event", outerID.get());
  }
}

void
Console::ProcessArguments(JSContext* aCx,
                          const nsTArray<JS::Heap<JS::Value>>& aData,
                          Sequence<JS::Value>& aSequence,
                          Sequence<JS::Value>& aStyles)
{
  if (aData.IsEmpty()) {
    return;
  }

  if (aData.Length() == 1 || !aData[0].isString()) {
    ArgumentsToValueList(aData, aSequence);
    return;
  }

  JS::Rooted<JS::Value> format(aCx, aData[0]);
  JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, format));
  if (!jsString) {
    return;
  }

  nsDependentJSString string;
  if (!string.init(aCx, jsString)) {
    return;
  }

  nsString::const_iterator start, end;
  string.BeginReading(start);
  string.EndReading(end);

  nsString output;
  uint32_t index = 1;

  while (start != end) {
    if (*start != '%') {
      output.Append(*start);
      ++start;
      continue;
    }

    ++start;
    if (start == end) {
      output.Append('%');
      break;
    }

    if (*start == '%') {
      output.Append(*start);
      ++start;
      continue;
    }

    nsAutoString tmp;
    tmp.Append('%');

    int32_t integer = -1;
    int32_t mantissa = -1;

    // Let's parse %<number>.<number> for %d and %f
    if (*start >= '0' && *start <= '9') {
      integer = 0;

      do {
        integer = integer * 10 + *start - '0';
        tmp.Append(*start);
        ++start;
      } while (*start >= '0' && *start <= '9' && start != end);
    }

    if (start == end) {
      output.Append(tmp);
      break;
    }

    if (*start == '.') {
      tmp.Append(*start);
      ++start;

      if (start == end) {
        output.Append(tmp);
        break;
      }

      // '.' must be followed by a number.
      if (*start < '0' || *start > '9') {
        output.Append(tmp);
        continue;
      }

      mantissa = 0;

      do {
        mantissa = mantissa * 10 + *start - '0';
        tmp.Append(*start);
        ++start;
      } while (*start >= '0' && *start <= '9' && start != end);

      if (start == end) {
        output.Append(tmp);
        break;
      }
    }

    char ch = *start;
    tmp.Append(ch);
    ++start;

    switch (ch) {
      case 'o':
      case 'O':
      {
        if (!output.IsEmpty()) {
          JS::Rooted<JSString*> str(aCx, JS_NewUCStringCopyN(aCx,
                                                             output.get(),
                                                             output.Length()));
          if (!str) {
            return;
          }

          aSequence.AppendElement(JS::StringValue(str));
          output.Truncate();
        }

        JS::Rooted<JS::Value> v(aCx);
        if (index < aData.Length()) {
          v = aData[index++];
        }

        aSequence.AppendElement(v);
        break;
      }

      case 'c':
      {
        if (!output.IsEmpty()) {
          JS::Rooted<JSString*> str(aCx, JS_NewUCStringCopyN(aCx,
                                                             output.get(),
                                                             output.Length()));
          if (!str) {
            return;
          }

          aSequence.AppendElement(JS::StringValue(str));
          output.Truncate();
        }

        if (index < aData.Length()) {
          JS::Rooted<JS::Value> v(aCx, aData[index++]);
          JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, v));
          if (!jsString) {
            return;
          }

          int32_t diff = aSequence.Length() - aStyles.Length();
          if (diff > 0) {
            for (int32_t i = 0; i < diff; i++) {
              aStyles.AppendElement(JS::NullValue());
            }
          }
          aStyles.AppendElement(JS::StringValue(jsString));
        }
        break;
      }

      case 's':
        if (index < aData.Length()) {
          JS::Rooted<JS::Value> value(aCx, aData[index++]);
          JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, value));
          if (!jsString) {
            return;
          }

          nsDependentJSString v;
          if (!v.init(aCx, jsString)) {
            return;
          }

          output.Append(v);
        }
        break;

      case 'd':
      case 'i':
        if (index < aData.Length()) {
          JS::Rooted<JS::Value> value(aCx, aData[index++]);

          int32_t v;
          if (!JS::ToInt32(aCx, value, &v)) {
            return;
          }

          nsCString format;
          MakeFormatString(format, integer, mantissa, 'd');
          output.AppendPrintf(format.get(), v);
        }
        break;

      case 'f':
        if (index < aData.Length()) {
          JS::Rooted<JS::Value> value(aCx, aData[index++]);

          double v;
          if (!JS::ToNumber(aCx, value, &v)) {
            return;
          }

          nsCString format;
          MakeFormatString(format, integer, mantissa, 'f');
          output.AppendPrintf(format.get(), v);
        }
        break;

      default:
        output.Append(tmp);
        break;
    }
  }

  if (!output.IsEmpty()) {
    JS::Rooted<JSString*> str(aCx, JS_NewUCStringCopyN(aCx, output.get(),
                                                       output.Length()));
    if (!str) {
      return;
    }

    aSequence.AppendElement(JS::StringValue(str));
  }

  // The rest of the array, if unused by the format string.
  for (; index < aData.Length(); ++index) {
    aSequence.AppendElement(aData[index]);
  }
}

void
Console::MakeFormatString(nsCString& aFormat, int32_t aInteger,
                          int32_t aMantissa, char aCh)
{
  aFormat.Append('%');
  if (aInteger >= 0) {
    aFormat.AppendInt(aInteger);
  }

  if (aMantissa >= 0) {
    aFormat.Append('.');
    aFormat.AppendInt(aMantissa);
  }

  aFormat.Append(aCh);
}

void
Console::ComposeGroupName(JSContext* aCx,
                          const nsTArray<JS::Heap<JS::Value>>& aData,
                          nsAString& aName)
{
  for (uint32_t i = 0; i < aData.Length(); ++i) {
    if (i != 0) {
      aName.AppendASCII(" ");
    }

    JS::Rooted<JS::Value> value(aCx, aData[i]);
    JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, value));
    if (!jsString) {
      return;
    }

    nsDependentJSString string;
    if (!string.init(aCx, jsString)) {
      return;
    }

    aName.Append(string);
  }
}

JS::Value
Console::StartTimer(JSContext* aCx, const JS::Value& aName,
                    DOMHighResTimeStamp aTimestamp)
{
  if (mTimerRegistry.Count() >= MAX_PAGE_TIMERS) {
    RootedDictionary<ConsoleTimerError> error(aCx);

    JS::Rooted<JS::Value> value(aCx);
    if (!error.ToObject(aCx, &value)) {
      return JS::UndefinedValue();
    }

    return value;
  }

  RootedDictionary<ConsoleTimerStart> timer(aCx);

  JS::Rooted<JS::Value> name(aCx, aName);
  JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, name));
  if (!jsString) {
    return JS::UndefinedValue();
  }

  nsDependentJSString key;
  if (!key.init(aCx, jsString)) {
    return JS::UndefinedValue();
  }

  timer.mName = key;

  DOMHighResTimeStamp entry;
  if (!mTimerRegistry.Get(key, &entry)) {
    mTimerRegistry.Put(key, aTimestamp);
  } else {
    aTimestamp = entry;
  }

  timer.mStarted = aTimestamp;

  JS::Rooted<JS::Value> value(aCx);
  if (!timer.ToObject(aCx, &value)) {
    return JS::UndefinedValue();
  }

  return value;
}

JS::Value
Console::StopTimer(JSContext* aCx, const JS::Value& aName,
                   DOMHighResTimeStamp aTimestamp)
{
  JS::Rooted<JS::Value> name(aCx, aName);
  JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, name));
  if (!jsString) {
    return JS::UndefinedValue();
  }

  nsDependentJSString key;
  if (!key.init(aCx, jsString)) {
    return JS::UndefinedValue();
  }

  DOMHighResTimeStamp entry;
  if (!mTimerRegistry.Get(key, &entry)) {
    return JS::UndefinedValue();
  }

  mTimerRegistry.Remove(key);

  RootedDictionary<ConsoleTimerEnd> timer(aCx);
  timer.mName = key;
  timer.mDuration = aTimestamp - entry;

  JS::Rooted<JS::Value> value(aCx);
  if (!timer.ToObject(aCx, &value)) {
    return JS::UndefinedValue();
  }

  return value;
}

void
Console::ArgumentsToValueList(const nsTArray<JS::Heap<JS::Value>>& aData,
                              Sequence<JS::Value>& aSequence)
{
  for (uint32_t i = 0; i < aData.Length(); ++i) {
    aSequence.AppendElement(aData[i]);
  }
}

JS::Value
Console::IncreaseCounter(JSContext* aCx, const ConsoleStackEntry& aFrame,
                          const nsTArray<JS::Heap<JS::Value>>& aArguments)
{
  ClearException ce(aCx);

  nsAutoString key;
  nsAutoString label;

  if (!aArguments.IsEmpty()) {
    JS::Rooted<JS::Value> labelValue(aCx, aArguments[0]);
    JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, labelValue));

    nsDependentJSString string;
    if (jsString && string.init(aCx, jsString)) {
      label = string;
      key = string;
    }
  }

  if (key.IsEmpty()) {
    key.Append(aFrame.mFilename);
    key.Append(':');
    key.AppendInt(aFrame.mLineNumber);
  }

  uint32_t count = 0;
  if (!mCounterRegistry.Get(key, &count)) {
    if (mCounterRegistry.Count() >= MAX_PAGE_COUNTERS) {
      RootedDictionary<ConsoleCounterError> error(aCx);

      JS::Rooted<JS::Value> value(aCx);
      if (!error.ToObject(aCx, &value)) {
        return JS::UndefinedValue();
      }

      return value;
    }
  }

  ++count;
  mCounterRegistry.Put(key, count);

  RootedDictionary<ConsoleCounter> data(aCx);
  data.mLabel = label;
  data.mCount = count;

  JS::Rooted<JS::Value> value(aCx);
  if (!data.ToObject(aCx, &value)) {
    return JS::UndefinedValue();
  }

  return value;
}

void
Console::ClearConsoleData()
{
  while (ConsoleCallData* data = mQueuedCalls.popFirst()) {
    delete data;
  }
}

bool
Console::ShouldIncludeStackrace(MethodName aMethodName)
{
  switch (aMethodName) {
    case MethodError:
    case MethodException:
    case MethodAssert:
    case MethodTrace:
      return true;
    default:
      return false;
  }
}

} // namespace dom
} // namespace mozilla
