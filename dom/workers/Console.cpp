/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Console.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/OldDebugAPI.h"

#include "nsJSUtils.h"
#include "WorkerRunnable.h"
#include "nsComponentManagerUtils.h"
#include "nsIDOMGlobalPropertyInitializer.h"

#include "mozilla/dom/WorkerConsoleBinding.h"
#include "mozilla/dom/Exceptions.h"

#define CONSOLE_TAG JS_SCTAG_USER_MIN

// From dom/base/ConsoleAPI.js
#define DEFAULT_MAX_STACKTRACE_DEPTH 200

using namespace mozilla::dom::exceptions;

BEGIN_WORKERS_NAMESPACE

class ConsoleProxy
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(URLProxy)

  bool
  Init(JSContext* aCx, nsPIDOMWindow* aWindow)
  {
    AssertIsOnMainThread();

    // Console API:
    nsCOMPtr<nsISupports> cInstance =
      do_CreateInstance("@mozilla.org/console-api;1");

    nsCOMPtr<nsIDOMGlobalPropertyInitializer> gpi =
      do_QueryInterface(cInstance);
    NS_ENSURE_TRUE(gpi, false);

    // We don't do anything with the return value.
    JS::Rooted<JS::Value> prop_val(aCx);
    if (NS_FAILED(gpi->Init(aWindow, &prop_val))) {
      return false;
    }

    mXpcwrappedjs = do_QueryInterface(cInstance);
    NS_ENSURE_TRUE(mXpcwrappedjs, false);

    return true;
  }

  nsIXPConnectWrappedJS*
  GetWrappedJS() const
  {
    AssertIsOnMainThread();
    return mXpcwrappedjs;
  }

  void ReleaseWrappedJS()
  {
    AssertIsOnMainThread();
    mXpcwrappedjs = nullptr;
  }

private:
  nsCOMPtr<nsIXPConnectWrappedJS> mXpcwrappedjs;
};

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
  if (strings->Length() <= aData) {
    return nullptr;
  }

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

class ConsoleStackData
{
public:
  ConsoleStackData()
  : mLineNumber(0)
  {}

  nsCString mFilename;
  uint32_t mLineNumber;
  nsCString mFunctionName;
};

class ConsoleRunnable MOZ_FINAL : public nsRunnable
{
public:
  explicit ConsoleRunnable(WorkerConsole* aConsole,
                           WorkerPrivate* aWorkerPrivate)
    : mConsole(aConsole)
    , mWorkerPrivate(aWorkerPrivate)
    , mMethod(nullptr)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool
  Dispatch(JSContext* aCx,
           const char* aMethod,
           JS::Handle<JS::Value> aArguments,
           nsTArray<ConsoleStackData>& aStackData)
  {
    mMethod = aMethod;
    mStackData.SwapElements(aStackData);

    if (!mArguments.write(aCx, aArguments, &gConsoleCallbacks, &mStrings)) {
      JS_ClearPendingException(aCx);
      return false;
    }

    mWorkerPrivate->AssertIsOnWorkerThread();

    AutoSyncLoopHolder syncLoop(mWorkerPrivate);

    mSyncLoopTarget = syncLoop.EventTarget();

    if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
      JS_ReportError(aCx,
                     "Failed to dispatch to main thread for the "
                     "Console API (method %s)!", mMethod);
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

  void
  RunConsole()
  {
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

    // Walk up to our containing page
    WorkerPrivate* wp = mWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    AutoPushJSContext cx(wp->ParentJSContext());
    JSAutoRequest ar(cx);
    ClearException ce(cx);

    nsRefPtr<ConsoleProxy> proxy = mConsole->GetProxy();
    if (!proxy) {
      nsPIDOMWindow* window = wp->GetWindow();
      NS_ENSURE_TRUE_VOID(window);

      proxy = new ConsoleProxy();
      if (!proxy->Init(cx, window)) {
        return;
      }

      mConsole->SetProxy(proxy);
    }

    JS::Rooted<JSObject*> consoleObj(cx, proxy->GetWrappedJS()->GetJSObject());
    NS_ENSURE_TRUE_VOID(consoleObj);

    JSAutoCompartment ac(cx, consoleObj);

    // 3 args for the queueCall.
    nsDependentCString method(mMethod);

    JS::Rooted<JS::Value> methodValue(cx);
    if (!ByteStringToJsval(cx, method, &methodValue)) {
      return;
    }

    JS::Rooted<JS::Value> argumentsValue(cx);
    if (!mArguments.read(cx, &argumentsValue, &gConsoleCallbacks, &mStrings)) {
      return;
    }

    JS::Rooted<JS::Value> stackValue(cx);
    {
      JS::Rooted<JSObject*> stackObj(cx,
        JS_NewArrayObject(cx, mStackData.Length(), nullptr));
      if (!stackObj) {
        return;
      }

      for (uint32_t i = 0; i < mStackData.Length(); ++i) {
        WorkerConsoleStack stack;

        CopyUTF8toUTF16(mStackData[i].mFilename, stack.mFilename);

        CopyUTF8toUTF16(mStackData[i].mFunctionName, stack.mFunctionName);

        stack.mLineNumber = mStackData[i].mLineNumber;

        stack.mLanguage = nsIProgrammingLanguage::JAVASCRIPT;

        JS::Rooted<JS::Value> value(cx);
        if (!stack.ToObject(cx, JS::NullPtr(), &value)) {
          return;
        }

        if (!JS_DefineElement(cx, stackObj, i, value, nullptr, nullptr, 0)) {
          return;
        }
      }

      stackValue = JS::ObjectValue(*stackObj);
    }

    JS::AutoValueArray<3> args(cx);
    args[0].set(methodValue);
    args[1].set(argumentsValue);
    args[2].set(stackValue);

    JS::Rooted<JS::Value> ret(cx);
    JS_CallFunctionName(cx, consoleObj, "queueCall", args, ret.address());
  }

  WorkerConsole* mConsole;
  WorkerPrivate* mWorkerPrivate;
  nsCOMPtr<nsIEventTarget> mSyncLoopTarget;

  const char* mMethod;
  JSAutoStructuredCloneBuffer mArguments;
  nsTArray<ConsoleStackData> mStackData;

  nsTArray<nsString> mStrings;
};

class TeardownRunnable : public nsRunnable
{
public:
  TeardownRunnable(ConsoleProxy* aProxy)
    : mProxy(aProxy)
  {
  }

  NS_IMETHOD Run()
  {
    AssertIsOnMainThread();

    mProxy->ReleaseWrappedJS();
    mProxy = nullptr;

    return NS_OK;
  }

private:
  nsRefPtr<ConsoleProxy> mProxy;
};

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WorkerConsole)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WorkerConsole, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WorkerConsole, Release)

/* static */ already_AddRefed<WorkerConsole>
WorkerConsole::Create()
{
  nsRefPtr<WorkerConsole> console = new WorkerConsole();
  return console.forget();
}

JSObject*
WorkerConsole::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return WorkerConsoleBinding_workers::Wrap(aCx, aScope, this);
}

WorkerConsole::WorkerConsole()
{
  MOZ_COUNT_CTOR(WorkerConsole);
  SetIsDOMBinding();
}

WorkerConsole::~WorkerConsole()
{
  MOZ_COUNT_DTOR(WorkerConsole);

  if (mProxy) {
    nsRefPtr<TeardownRunnable> runnable = new TeardownRunnable(mProxy);
    mProxy = nullptr;

    if (NS_FAILED(NS_DispatchToMainThread(runnable))) {
      NS_ERROR("Failed to dispatch teardown runnable!");
    }
  }
}

void
WorkerConsole::SetProxy(ConsoleProxy* aProxy)
{
  MOZ_ASSERT(!mProxy);
  mProxy = aProxy;
}

void
WorkerConsole::Method(JSContext* aCx, const char* aMethodName,
                      const Sequence<JS::Value>& aData,
                      uint32_t aStackLevel)
{
  nsCOMPtr<nsIStackFrame> stack = CreateStack(aCx, aStackLevel);
  if (!stack) {
    return;
  }

  // nsIStackFrame is not thread-safe so we take what we need and we store in
  // an array of ConsoleStackData objects.
  nsTArray<ConsoleStackData> stackData;
  while (stack) {
    ConsoleStackData& data = *stackData.AppendElement();

    if (NS_FAILED(stack->GetFilename(data.mFilename))) {
      return;
    }

    int32_t lineNumber;
    if (NS_FAILED(stack->GetLineNumber(&lineNumber))) {
      return;
    }

    data.mLineNumber = lineNumber;

    if (NS_FAILED(stack->GetName(data.mFunctionName))) {
      return;
    }

    nsCOMPtr<nsIStackFrame> caller;
    if (NS_FAILED(stack->GetCaller(getter_AddRefs(caller)))) {
      return;
    }

    stack.swap(caller);
  }

  JS::Rooted<JSObject*> arguments(aCx,
    JS_NewArrayObject(aCx, aData.Length(), nullptr));
  if (!arguments) {
    return;
  }

  for (uint32_t i = 0; i < aData.Length(); ++i) {
    if (!JS_DefineElement(aCx, arguments, i, aData[i], nullptr, nullptr,
                          JSPROP_ENUMERATE)) {
      return;
    }
  }
  JS::Rooted<JS::Value> argumentsValue(aCx, JS::ObjectValue(*arguments));

  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(worker);

  nsRefPtr<ConsoleRunnable> runnable = new ConsoleRunnable(this, worker);
  runnable->Dispatch(aCx, aMethodName, argumentsValue, stackData);
}

#define METHOD(name, jsName)                                \
  void                                                      \
  WorkerConsole::name(JSContext* aCx,                       \
                      const Sequence<JS::Value>& aData)     \
  {                                                         \
    Method(aCx, jsName, aData, 1);                          \
  }

METHOD(Log, "log")
METHOD(Info, "info")
METHOD(Warn, "warn")
METHOD(Error, "error")
METHOD(Exception, "exception")
METHOD(Debug, "debug")

void
WorkerConsole::Trace(JSContext* aCx)
{
  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(aCx, &data);
  Method(aCx, "trace", data, DEFAULT_MAX_STACKTRACE_DEPTH);
}

void
WorkerConsole::Dir(JSContext* aCx,
                   const Optional<JS::Handle<JS::Value>>& aValue)
{
  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(aCx, &data);

  if (aValue.WasPassed()) {
    data.AppendElement(aValue.Value());
  }

  Method(aCx, "dir", data, 1);
}

METHOD(Group, "group")
METHOD(GroupCollapsed, "groupCollapsed")
METHOD(GroupEnd, "groupEnd")

void
WorkerConsole::Time(JSContext* aCx,
                    const Optional<JS::Handle<JS::Value>>& aTimer)
{
  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(aCx, &data);

  if (aTimer.WasPassed()) {
    data.AppendElement(aTimer.Value());
  }

  Method(aCx, "time", data, 1);
}

void
WorkerConsole::TimeEnd(JSContext* aCx,
                       const Optional<JS::Handle<JS::Value>>& aTimer)
{
  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(aCx, &data);

  if (aTimer.WasPassed()) {
    data.AppendElement(aTimer.Value());
  }

  Method(aCx, "timeEnd", data, 1);
}

METHOD(Profile, "profile")
METHOD(ProfileEnd, "profileEnd")

void
WorkerConsole::Assert(JSContext* aCx, bool aCondition,
                      const Sequence<JS::Value>& aData)
{
  if (!aCondition) {
    Method(aCx, "assert", aData, 1);
  }
}

void
WorkerConsole::__noSuchMethod__()
{
  // Nothing to do.
}

#undef METHOD

END_WORKERS_NAMESPACE
