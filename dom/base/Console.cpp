/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Console.h"
#include "mozilla/dom/ConsoleBinding.h"

#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/StructuredCloneHelper.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/Maybe.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDocument.h"
#include "nsDOMNavigationTiming.h"
#include "nsGlobalWindow.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "nsPerformance.h"
#include "ScriptSettings.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "xpcprivate.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsProxyRelease.h"
#include "mozilla/ConsoleTimelineMarker.h"
#include "mozilla/TimestampTimelineMarker.h"

#include "nsIConsoleAPIStorage.h"
#include "nsIDOMWindowUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
#include "nsIProgrammingLanguage.h"
#include "nsISensitiveInfoHiddenURI.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIWebNavigation.h"
#include "nsIXPConnect.h"

#ifdef MOZ_ENABLE_PROFILER_SPS
#include "nsIProfiler.h"
#endif

// The maximum allowed number of concurrent timers per page.
#define MAX_PAGE_TIMERS 10000

// The maximum allowed number of concurrent counters per page.
#define MAX_PAGE_COUNTERS 10000

// The maximum stacktrace depth when populating the stacktrace array used for
// console.trace().
#define DEFAULT_MAX_STACKTRACE_DEPTH 200

// This tags are used in the Structured Clone Algorithm to move js values from
// worker thread to main thread
#define CONSOLE_TAG_BLOB   JS_SCTAG_USER_MIN

using namespace mozilla::dom::exceptions;
using namespace mozilla::dom::workers;

namespace mozilla {
namespace dom {

struct
ConsoleStructuredCloneData
{
  nsCOMPtr<nsISupports> mParent;
  nsTArray<nsRefPtr<BlobImpl>> mBlobs;
};

/**
 * Console API in workers uses the Structured Clone Algorithm to move any value
 * from the worker thread to the main-thread. Some object cannot be moved and,
 * in these cases, we convert them to strings.
 * It's not the best, but at least we are able to show something.
 */

class ConsoleCallData final
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ConsoleCallData)

  ConsoleCallData()
    : mMethodName(Console::MethodLog)
    , mPrivate(false)
    , mTimeStamp(JS_Now() / PR_USEC_PER_MSEC)
    , mIDType(eUnknown)
    , mOuterIDNumber(0)
    , mInnerIDNumber(0)
  { }

  void
  Initialize(JSContext* aCx, Console::MethodName aName,
             const nsAString& aString, const Sequence<JS::Value>& aArguments)
  {
    mGlobal = JS::CurrentGlobalOrNull(aCx);
    mMethodName = aName;
    mMethodString = aString;

    for (uint32_t i = 0; i < aArguments.Length(); ++i) {
      if (!mArguments.AppendElement(aArguments[i])) {
        return;
      }
    }
  }

  void
  SetIDs(uint64_t aOuterID, uint64_t aInnerID)
  {
    MOZ_ASSERT(mIDType == eUnknown);

    mOuterIDNumber = aOuterID;
    mInnerIDNumber = aInnerID;
    mIDType = eNumber;
  }

  void
  SetIDs(const nsAString& aOuterID, const nsAString& aInnerID)
  {
    MOZ_ASSERT(mIDType == eUnknown);

    mOuterIDString = aOuterID;
    mInnerIDString = aInnerID;
    mIDType = eString;
  }

  void
  CleanupJSObjects()
  {
    mArguments.Clear();
    mGlobal = nullptr;
  }

  JS::Heap<JSObject*> mGlobal;

  Console::MethodName mMethodName;
  bool mPrivate;
  int64_t mTimeStamp;
  DOMHighResTimeStamp mMonotonicTimer;

  // The concept of outerID and innerID is misleading because when a
  // ConsoleCallData is created from a window, these are the window IDs, but
  // when the object is created from a SharedWorker, a ServiceWorker or a
  // subworker of a ChromeWorker these IDs are the type of worker and the
  // filename of the callee.
  // In Console.jsm the ID is 'jsm'.
  enum {
    eString,
    eNumber,
    eUnknown
  } mIDType;

  uint64_t mOuterIDNumber;
  nsString mOuterIDString;

  uint64_t mInnerIDNumber;
  nsString mInnerIDString;

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

private:
  ~ConsoleCallData()
  { }
};

// This class is used to clear any exception at the end of this method.
class ClearException
{
public:
  explicit ClearException(JSContext* aCx)
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
                      , public WorkerFeature
                      , public StructuredCloneHelperInternal
{
public:
  explicit ConsoleRunnable(Console* aConsole)
    : mWorkerPrivate(GetCurrentThreadWorkerPrivate())
    , mConsole(aConsole)
  {
    MOZ_ASSERT(mWorkerPrivate);
  }

  virtual
  ~ConsoleRunnable()
  {
    // Shutdown the StructuredCloneHelperInternal class.
    Shutdown();
  }

  bool
  Dispatch()
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    JSContext* cx = mWorkerPrivate->GetJSContext();

    if (!PreDispatch(cx)) {
      return false;
    }

    if (NS_WARN_IF(!mWorkerPrivate->AddFeature(cx, this))) {
      return false;
    }

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(this)));
    return true;
  }

  virtual bool Notify(JSContext* aCx, workers::Status aStatus) override
  {
    // We don't care about the notification. We just want to keep the
    // mWorkerPrivate alive.
    return true;
  }

private:
  NS_IMETHOD Run() override
  {
    AssertIsOnMainThread();

    // Walk up to our containing page
    WorkerPrivate* wp = mWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    nsPIDOMWindow* window = wp->GetWindow();
    if (!window) {
      RunWindowless();
    } else {
      RunWithWindow(window);
    }

    PostDispatch();
    return NS_OK;
  }

  void
  PostDispatch()
  {
    class ConsoleReleaseRunnable final : public MainThreadWorkerControlRunnable
    {
      nsRefPtr<ConsoleRunnable> mRunnable;

    public:
      ConsoleReleaseRunnable(WorkerPrivate* aWorkerPrivate,
                             ConsoleRunnable* aRunnable)
        : MainThreadWorkerControlRunnable(aWorkerPrivate)
        , mRunnable(aRunnable)
      {
        MOZ_ASSERT(aRunnable);
      }

      virtual bool
      WorkerRun(JSContext* aCx, workers::WorkerPrivate* aWorkerPrivate) override
      {
        MOZ_ASSERT(aWorkerPrivate);
        aWorkerPrivate->AssertIsOnWorkerThread();

        aWorkerPrivate->RemoveFeature(aCx, mRunnable);
        mRunnable->mConsole = nullptr;
        return true;
      }

    private:
      ~ConsoleReleaseRunnable()
      {}
    };

    nsRefPtr<WorkerControlRunnable> runnable =
      new ConsoleReleaseRunnable(mWorkerPrivate, this);
    runnable->Dispatch(nullptr);
  }

  void
  RunWithWindow(nsPIDOMWindow* aWindow)
  {
    AutoJSAPI jsapi;
    MOZ_ASSERT(aWindow);

    nsRefPtr<nsGlobalWindow> win = static_cast<nsGlobalWindow*>(aWindow);
    if (NS_WARN_IF(!jsapi.Init(win))) {
      return;
    }

    MOZ_ASSERT(aWindow->IsInnerWindow());
    nsPIDOMWindow* outerWindow = aWindow->GetOuterWindow();
    MOZ_ASSERT(outerWindow);

    RunConsole(jsapi.cx(), outerWindow, aWindow);
  }

  void
  RunWindowless()
  {
    WorkerPrivate* wp = mWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    MOZ_ASSERT(!wp->GetWindow());

    AutoSafeJSContext cx;

    JS::Rooted<JSObject*> global(cx, mConsole->GetOrCreateSandbox(cx, wp->GetPrincipal()));
    if (NS_WARN_IF(!global)) {
      return;
    }

    // The CreateSandbox call returns a proxy to the actual sandbox object. We
    // don't need a proxy here.
    global = js::UncheckedUnwrap(global);

    JSAutoCompartment ac(cx, global);

    RunConsole(cx, nullptr, nullptr);
  }

protected:
  virtual bool
  PreDispatch(JSContext* aCx) = 0;

  virtual void
  RunConsole(JSContext* aCx, nsPIDOMWindow* aOuterWindow,
             nsPIDOMWindow* aInnerWindow) = 0;

  virtual JSObject* ReadCallback(JSContext* aCx,
                                 JSStructuredCloneReader* aReader,
                                 uint32_t aTag,
                                 uint32_t aIndex) override
  {
    AssertIsOnMainThread();

    if (aTag == CONSOLE_TAG_BLOB) {
      MOZ_ASSERT(mClonedData.mBlobs.Length() > aIndex);

      JS::Rooted<JS::Value> val(aCx);
      {
        nsRefPtr<Blob> blob =
          Blob::Create(mClonedData.mParent, mClonedData.mBlobs.ElementAt(aIndex));
        if (!ToJSValue(aCx, blob, &val)) {
          return nullptr;
        }
      }

      return &val.toObject();
    }

    MOZ_CRASH("No other tags are supported.");
    return nullptr;
  }

  virtual bool WriteCallback(JSContext* aCx,
                             JSStructuredCloneWriter* aWriter,
                             JS::Handle<JSObject*> aObj) override
  {
    nsRefPtr<Blob> blob;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, aObj, blob)) &&
        blob->Impl()->MayBeClonedToOtherThreads()) {
      if (!JS_WriteUint32Pair(aWriter, CONSOLE_TAG_BLOB,
                              mClonedData.mBlobs.Length())) {
        return false;
      }

      mClonedData.mBlobs.AppendElement(blob->Impl());
      return true;
    }

    JS::Rooted<JS::Value> value(aCx, JS::ObjectOrNullValue(aObj));
    JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, value));
    if (!jsString) {
      return false;
    }

    if (!JS_WriteString(aWriter, jsString)) {
      return false;
    }

    return true;
  }

  WorkerPrivate* mWorkerPrivate;

  // This must be released on the worker thread.
  nsRefPtr<Console> mConsole;

  ConsoleStructuredCloneData mClonedData;
};

// This runnable appends a CallData object into the Console queue running on
// the main-thread.
class ConsoleCallDataRunnable final : public ConsoleRunnable
{
public:
  ConsoleCallDataRunnable(Console* aConsole,
                          ConsoleCallData* aCallData)
    : ConsoleRunnable(aConsole)
    , mCallData(aCallData)
  { }

private:
  ~ConsoleCallDataRunnable()
  {
    class ReleaseCallData final : public nsRunnable
    {
    public:
      explicit ReleaseCallData(nsRefPtr<ConsoleCallData>& aCallData)
      {
        mCallData.swap(aCallData);
      }

      NS_IMETHOD Run() override
      {
        mCallData = nullptr;
        return NS_OK;
      }

    private:
      nsRefPtr<ConsoleCallData> mCallData;
    };

    nsRefPtr<ReleaseCallData> runnable = new ReleaseCallData(mCallData);
    if(NS_FAILED(NS_DispatchToMainThread(runnable))) {
      NS_WARNING("Failed to dispatch a ReleaseCallData runnable. Leaking.");
    }
  }

  bool
  PreDispatch(JSContext* aCx) override
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

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

    if (!Write(aCx, value)) {
      return false;
    }

    mCallData->CleanupJSObjects();
    return true;
  }

  void
  RunConsole(JSContext* aCx, nsPIDOMWindow* aOuterWindow,
             nsPIDOMWindow* aInnerWindow) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // The windows have to run in parallel.
    MOZ_ASSERT(!!aOuterWindow == !!aInnerWindow);

    if (aOuterWindow) {
      mCallData->SetIDs(aOuterWindow->WindowID(), aInnerWindow->WindowID());
    } else {
      ConsoleStackEntry frame;
      if (mCallData->mTopStackFrame) {
        frame = *mCallData->mTopStackFrame;
      }

      nsString id;
      if (mWorkerPrivate->IsSharedWorker()) {
        id = NS_LITERAL_STRING("SharedWorker");
      } else if (mWorkerPrivate->IsServiceWorker()) {
        id = NS_LITERAL_STRING("ServiceWorker");
      } else {
        id = NS_LITERAL_STRING("Worker");
      }

      mCallData->SetIDs(frame.mFilename, id);
    }

    // Now we could have the correct window (if we are not window-less).
    mClonedData.mParent = aInnerWindow;

    ProcessCallData(aCx);
    mCallData->CleanupJSObjects();

    mClonedData.mParent = nullptr;
  }

private:
  void
  ProcessCallData(JSContext* aCx)
  {
    ClearException ce(aCx);

    JS::Rooted<JS::Value> argumentsValue(aCx);
    if (!Read(aCx, &argumentsValue)) {
      return;
    }

    MOZ_ASSERT(argumentsValue.isObject());
    JS::Rooted<JSObject*> argumentsObj(aCx, &argumentsValue.toObject());

    uint32_t length;
    if (!JS_GetArrayLength(aCx, argumentsObj, &length)) {
      return;
    }

    for (uint32_t i = 0; i < length; ++i) {
      JS::Rooted<JS::Value> value(aCx);

      if (!JS_GetElement(aCx, argumentsObj, i, &value)) {
        return;
      }

      mCallData->mArguments.AppendElement(value);
    }

    MOZ_ASSERT(mCallData->mArguments.Length() == length);

    mCallData->mGlobal = JS::CurrentGlobalOrNull(aCx);
    mConsole->ProcessCallData(mCallData);
  }

  nsRefPtr<ConsoleCallData> mCallData;
};

// This runnable calls ProfileMethod() on the console on the main-thread.
class ConsoleProfileRunnable final : public ConsoleRunnable
{
public:
  ConsoleProfileRunnable(Console* aConsole, const nsAString& aAction,
                         const Sequence<JS::Value>& aArguments)
    : ConsoleRunnable(aConsole)
    , mAction(aAction)
    , mArguments(aArguments)
  {
    MOZ_ASSERT(aConsole);
  }

private:
  bool
  PreDispatch(JSContext* aCx) override
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

    if (!Write(aCx, value)) {
      return false;
    }

    mArguments.Clear();
    return true;
  }

  void
  RunConsole(JSContext* aCx, nsPIDOMWindow* aOuterWindow,
             nsPIDOMWindow* aInnerWindow) override
  {
    ClearException ce(aCx);

    // Now we could have the correct window (if we are not window-less).
    mClonedData.mParent = aInnerWindow;

    JS::Rooted<JS::Value> argumentsValue(aCx);
    bool ok = Read(aCx, &argumentsValue);
    mClonedData.mParent = nullptr;

    if (!ok) {
      return;
    }

    MOZ_ASSERT(argumentsValue.isObject());
    JS::Rooted<JSObject*> argumentsObj(aCx, &argumentsValue.toObject());

    uint32_t length;
    if (!JS_GetArrayLength(aCx, argumentsObj, &length)) {
      return;
    }

    Sequence<JS::Value> arguments;

    for (uint32_t i = 0; i < length; ++i) {
      JS::Rooted<JS::Value> value(aCx);

      if (!JS_GetElement(aCx, argumentsObj, i, &value)) {
        return;
      }

      if (!arguments.AppendElement(value, fallible)) {
        return;
      }
    }

    mConsole->ProfileMethod(aCx, mAction, arguments);
  }

  nsString mAction;
  Sequence<JS::Value> mArguments;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(Console)

// We don't need to traverse/unlink mStorage and mSandbox because they are not
// CCed objects and they are only used on the main thread, even when this
// Console object is used on workers.

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Console)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Console)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Console)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Console)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Console)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Console)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
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

  mozilla::HoldJSObjects(this);
}

Console::~Console()
{
  if (!NS_IsMainThread()) {
    if (mStorage) {
      NS_ReleaseOnMainThread(mStorage);
    }

    if (mSandbox) {
      NS_ReleaseOnMainThread(mSandbox);
    }
  }

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

    mTimerRegistry.Clear();
  }

  return NS_OK;
}

JSObject*
Console::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return ConsoleBinding::Wrap(aCx, this, aGivenProto);
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
METHOD(Table, "table")

void
Console::Trace(JSContext* aCx)
{
  const Sequence<JS::Value> data;
  Method(aCx, MethodTrace, NS_LITERAL_STRING("trace"), data);
}

// Displays an interactive listing of all the properties of an object.
METHOD(Dir, "dir");
METHOD(Dirxml, "dirxml");

METHOD(Group, "group")
METHOD(GroupCollapsed, "groupCollapsed")
METHOD(GroupEnd, "groupEnd")

void
Console::Time(JSContext* aCx, const JS::Handle<JS::Value> aTime)
{
  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(aCx, &data);

  if (!aTime.isUndefined() && !data.AppendElement(aTime, fallible)) {
    return;
  }

  Method(aCx, MethodTime, NS_LITERAL_STRING("time"), data);
}

void
Console::TimeEnd(JSContext* aCx, const JS::Handle<JS::Value> aTime)
{
  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(aCx, &data);

  if (!aTime.isUndefined() && !data.AppendElement(aTime, fallible)) {
    return;
  }

  Method(aCx, MethodTimeEnd, NS_LITERAL_STRING("timeEnd"), data);
}

void
Console::TimeStamp(JSContext* aCx, const JS::Handle<JS::Value> aData)
{
  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(aCx, &data);

  if (aData.isString() && !data.AppendElement(aData, fallible)) {
    return;
  }

#ifdef MOZ_ENABLE_PROFILER_SPS
  if (aData.isString() && NS_IsMainThread()) {
    if (!mProfiler) {
      mProfiler = do_GetService("@mozilla.org/tools/profiler;1");
    }
    if (mProfiler) {
      bool active = false;
      if (NS_SUCCEEDED(mProfiler->IsActive(&active)) && active) {
        nsAutoJSString stringValue;
        if (stringValue.init(aCx, aData)) {
          mProfiler->AddMarker(NS_ConvertUTF16toUTF8(stringValue).get());
        }
      }
    }
  }
#endif

  Method(aCx, MethodTimeStamp, NS_LITERAL_STRING("timeStamp"), data);
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
      new ConsoleProfileRunnable(this, aAction, aData);
    runnable->Dispatch();
    return;
  }

  ClearException ce(aCx);

  RootedDictionary<ConsoleProfileEvent> event(aCx);
  event.mAction = aAction;

  event.mArguments.Construct();
  Sequence<JS::Value>& sequence = event.mArguments.Value();

  for (uint32_t i = 0; i < aData.Length(); ++i) {
    if (!sequence.AppendElement(aData[i], fallible)) {
      return;
    }
  }

  JS::Rooted<JS::Value> eventValue(aCx);
  if (!ToJSValue(aCx, event, &eventValue)) {
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
Console::NoopMethod()
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

  int32_t columnNumber;
  rv = aStackFrame->GetColumnNumber(&columnNumber);
  NS_ENSURE_SUCCESS(rv, rv);

  aStackEntry.mColumnNumber = columnNumber;

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

    if (language == nsIProgrammingLanguage::JAVASCRIPT) {
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
  nsRefPtr<ConsoleCallData> callData(new ConsoleCallData());

  ClearException ce(aCx);

  callData->Initialize(aCx, aMethodName, aMethodString, aData);

  if (mWindow) {
    nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(mWindow);
    if (!webNav) {
      return;
    }

    nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(webNav);
    MOZ_ASSERT(loadContext);

    loadContext->GetUsePrivateBrowsing(&callData->mPrivate);
  }

  uint32_t maxDepth = ShouldIncludeStackTrace(aMethodName) ?
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

    if (language == nsIProgrammingLanguage::JAVASCRIPT) {
      callData->mTopStackFrame.emplace();
      nsresult rv = StackFrameToStackEntry(stack,
                                           *callData->mTopStackFrame,
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
    callData->mReifiedStack.emplace();
    nsresult rv = ReifyStack(stack, *callData->mReifiedStack);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  // Monotonic timer for 'time' and 'timeEnd'
  if (aMethodName == MethodTime ||
      aMethodName == MethodTimeEnd ||
      aMethodName == MethodTimeStamp) {
    if (mWindow) {
      nsGlobalWindow *win = static_cast<nsGlobalWindow*>(mWindow.get());
      MOZ_ASSERT(win);

      nsRefPtr<nsPerformance> performance = win->GetPerformance();
      if (!performance) {
        return;
      }

      callData->mMonotonicTimer = performance->Now();

      // 'time' and 'timeEnd' are displayed in the devtools timeline if active.
      bool isTimelineRecording = false;
      nsDocShell* docShell = static_cast<nsDocShell*>(mWindow->GetDocShell());
      if (docShell) {
        docShell->GetRecordProfileTimelineMarkers(&isTimelineRecording);
      }

      // 'timeStamp' recordings do not need an argument; use empty string
      // if no arguments passed in
      if (isTimelineRecording && aMethodName == MethodTimeStamp) {
        JS::Rooted<JS::Value> value(aCx, aData.Length() == 0 ?
                                    JS_GetEmptyStringValue(aCx) : aData[0]);
        JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, value));
        nsAutoJSString key;
        if (jsString) {
          key.init(aCx, jsString);
        }

        UniquePtr<TimelineMarker> marker = MakeUnique<TimestampTimelineMarker>(key);
        TimelineConsumers::AddMarkerForDocShell(docShell, Move(marker));
      }
      // For `console.time(foo)` and `console.timeEnd(foo)`
      else if (isTimelineRecording && aData.Length() == 1) {
        JS::Rooted<JS::Value> value(aCx, aData[0]);
        JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, value));
        if (jsString) {
          nsAutoJSString key;
          if (key.init(aCx, jsString)) {
            UniquePtr<TimelineMarker> marker = MakeUnique<ConsoleTimelineMarker>(
              key, aMethodName == MethodTime ? MarkerTracingType::START
                                             : MarkerTracingType::END);
            TimelineConsumers::AddMarkerForDocShell(docShell, Move(marker));
          }
        }
      }

    } else {
      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(workerPrivate);

      TimeDuration duration =
        mozilla::TimeStamp::Now() - workerPrivate->CreationTimeStamp();

      callData->mMonotonicTimer = duration.ToMilliseconds();
    }
  }

  if (NS_IsMainThread()) {
    callData->SetIDs(mOuterID, mInnerID);
    ProcessCallData(callData);
    return;
  }

  nsRefPtr<ConsoleCallDataRunnable> runnable =
    new ConsoleCallDataRunnable(this, callData);
  runnable->Dispatch();
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
  if (aData->mTopStackFrame) {
    frame = *aData->mTopStackFrame;
  }

  AutoSafeJSContext cx;
  ClearException ce(cx);
  RootedDictionary<ConsoleEvent> event(cx);

  JSAutoCompartment ac(cx, aData->mGlobal);

  event.mID.Construct();
  event.mInnerID.Construct();

  MOZ_ASSERT(aData->mIDType != ConsoleCallData::eUnknown);
  if (aData->mIDType == ConsoleCallData::eString) {
    event.mID.Value().SetAsString() = aData->mOuterIDString;
    event.mInnerID.Value().SetAsString() = aData->mInnerIDString;
  } else {
    MOZ_ASSERT(aData->mIDType == ConsoleCallData::eNumber);
    event.mID.Value().SetAsUnsignedLongLong() = aData->mOuterIDNumber;
    event.mInnerID.Value().SetAsUnsignedLongLong() = aData->mInnerIDNumber;
  }

  event.mLevel = aData->mMethodString;
  event.mFilename = frame.mFilename;

  nsCOMPtr<nsIURI> filenameURI;
  nsAutoCString pass;
  if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(filenameURI), frame.mFilename)) &&
      NS_SUCCEEDED(filenameURI->GetPassword(pass)) && !pass.IsEmpty()) {
    nsCOMPtr<nsISensitiveInfoHiddenURI> safeURI = do_QueryInterface(filenameURI);
    nsAutoCString spec;
    if (safeURI &&
        NS_SUCCEEDED(safeURI->GetSensitiveInfoHiddenSpec(spec))) {
      CopyUTF8toUTF16(spec, event.mFilename);
    }
  }

  event.mLineNumber = frame.mLineNumber;
  event.mColumnNumber = frame.mColumnNumber;
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
      if (!ProcessArguments(cx, aData->mArguments, event.mArguments.Value(),
                            event.mStyles.Value())) {
        return;
      }

      break;

    default:
      event.mArguments.Construct();
      if (!ArgumentsToValueList(aData->mArguments, event.mArguments.Value())) {
        return;
      }
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
  JSAutoCompartment ac2(cx, xpc::PrivilegedJunkScope());

  JS::Rooted<JS::Value> eventValue(cx);
  if (!ToJSValue(cx, event, &eventValue)) {
    return;
  }

  JS::Rooted<JSObject*> eventObj(cx, &eventValue.toObject());
  MOZ_ASSERT(eventObj);

  if (!JS_DefineProperty(cx, eventObj, "wrappedJSObject", eventValue, JSPROP_ENUMERATE)) {
    return;
  }

  if (ShouldIncludeStackTrace(aData->mMethodName)) {
    // Now define the "stacktrace" property on eventObj.  There are two cases
    // here.  Either we came from a worker and have a reified stack, or we want
    // to define a getter that will lazily reify the stack.
    if (aData->mReifiedStack) {
      JS::Rooted<JS::Value> stacktrace(cx);
      if (!ToJSValue(cx, *aData->mReifiedStack, &stacktrace) ||
          !JS_DefineProperty(cx, eventObj, "stacktrace", stacktrace,
                             JSPROP_ENUMERATE)) {
        return;
      }
    } else {
      JSFunction* fun = js::NewFunctionWithReserved(cx, LazyStackGetter, 0, 0,
                                                    "stacktrace");
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
                             JS_DATA_TO_FUNC_PTR(JSNative, funObj.get()),
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

  nsAutoString innerID, outerID;

  MOZ_ASSERT(aData->mIDType != ConsoleCallData::eUnknown);
  if (aData->mIDType == ConsoleCallData::eString) {
    outerID = aData->mOuterIDString;
    innerID = aData->mInnerIDString;
  } else {
    MOZ_ASSERT(aData->mIDType == ConsoleCallData::eNumber);
    outerID.AppendInt(aData->mOuterIDNumber);
    innerID.AppendInt(aData->mInnerIDNumber);
  }

  if (NS_FAILED(mStorage->RecordEvent(innerID, outerID, eventValue))) {
    NS_WARNING("Failed to record a console event.");
  }
}

namespace {

// Helper method for ProcessArguments. Flushes output, if non-empty, to aSequence.
bool
FlushOutput(JSContext* aCx, Sequence<JS::Value>& aSequence, nsString &aOutput)
{
  if (!aOutput.IsEmpty()) {
    JS::Rooted<JSString*> str(aCx, JS_NewUCStringCopyN(aCx,
                                                       aOutput.get(),
                                                       aOutput.Length()));
    if (!str) {
      return false;
    }

    if (!aSequence.AppendElement(JS::StringValue(str), fallible)) {
      return false;
    }

    aOutput.Truncate();
  }

  return true;
}

} // namespace

bool
Console::ProcessArguments(JSContext* aCx,
                          const nsTArray<JS::Heap<JS::Value>>& aData,
                          Sequence<JS::Value>& aSequence,
                          Sequence<JS::Value>& aStyles)
{
  if (aData.IsEmpty()) {
    return true;
  }

  if (aData.Length() == 1 || !aData[0].isString()) {
    return ArgumentsToValueList(aData, aSequence);
  }

  JS::Rooted<JS::Value> format(aCx, aData[0]);
  JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, format));
  if (!jsString) {
    return false;
  }

  nsAutoJSString string;
  if (!string.init(aCx, jsString)) {
    return false;
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
        if (!FlushOutput(aCx, aSequence, output)) {
          return false;
        }

        JS::Rooted<JS::Value> v(aCx);
        if (index < aData.Length()) {
          v = aData[index++];
        }

        if (!aSequence.AppendElement(v, fallible)) {
          return false;
        }

        break;
      }

      case 'c':
      {
        // If there isn't any output but there's already a style, then
        // discard the previous style and use the next one instead.
        if (output.IsEmpty() && !aStyles.IsEmpty()) {
          aStyles.TruncateLength(aStyles.Length() - 1);
        }

        if (!FlushOutput(aCx, aSequence, output)) {
          return false;
        }

        if (index < aData.Length()) {
          JS::Rooted<JS::Value> v(aCx, aData[index++]);
          JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, v));
          if (!jsString) {
            return false;
          }

          int32_t diff = aSequence.Length() - aStyles.Length();
          if (diff > 0) {
            for (int32_t i = 0; i < diff; i++) {
              if (!aStyles.AppendElement(JS::NullValue(), fallible)) {
                return false;
              }
            }
          }

          if (!aStyles.AppendElement(JS::StringValue(jsString), fallible)) {
            return false;
          }
        }
        break;
      }

      case 's':
        if (index < aData.Length()) {
          JS::Rooted<JS::Value> value(aCx, aData[index++]);
          JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, value));
          if (!jsString) {
            return false;
          }

          nsAutoJSString v;
          if (!v.init(aCx, jsString)) {
            return false;
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
            return false;
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
            return false;
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

  if (!FlushOutput(aCx, aSequence, output)) {
    return false;
  }

  // Discard trailing style element if there is no output to apply it to.
  if (aStyles.Length() > aSequence.Length()) {
    aStyles.TruncateLength(aSequence.Length());
  }

  // The rest of the array, if unused by the format string.
  for (; index < aData.Length(); ++index) {
    if (!aSequence.AppendElement(aData[index], fallible)) {
      return false;
    }
  }

  return true;
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

    nsAutoJSString string;
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
    if (!ToJSValue(aCx, error, &value)) {
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

  nsAutoJSString key;
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
  if (!ToJSValue(aCx, timer, &value)) {
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

  nsAutoJSString key;
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
  if (!ToJSValue(aCx, timer, &value)) {
    return JS::UndefinedValue();
  }

  return value;
}

bool
Console::ArgumentsToValueList(const nsTArray<JS::Heap<JS::Value>>& aData,
                              Sequence<JS::Value>& aSequence)
{
  for (uint32_t i = 0; i < aData.Length(); ++i) {
    if (!aSequence.AppendElement(aData[i], fallible)) {
      return false;
    }
  }

  return true;
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

    nsAutoJSString string;
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
      if (!ToJSValue(aCx, error, &value)) {
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
  if (!ToJSValue(aCx, data, &value)) {
    return JS::UndefinedValue();
  }

  return value;
}

bool
Console::ShouldIncludeStackTrace(MethodName aMethodName)
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

JSObject*
Console::GetOrCreateSandbox(JSContext* aCx, nsIPrincipal* aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mSandbox) {
    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    MOZ_ASSERT(xpc, "This should never be null!");

    JS::Rooted<JSObject*> sandbox(aCx);
    nsresult rv = xpc->CreateSandbox(aCx, aPrincipal, sandbox.address());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    mSandbox = new JSObjectHolder(aCx, sandbox);
  }

  return mSandbox->GetJSObject();
}

} // namespace dom
} // namespace mozilla
