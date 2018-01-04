/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Console.h"
#include "mozilla/dom/ConsoleInstance.h"
#include "mozilla/dom/ConsoleBinding.h"

#include "mozilla/dom/BlobBinding.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/WorkletGlobalScope.h"
#include "mozilla/Maybe.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDocument.h"
#include "nsDOMNavigationTiming.h"
#include "nsGlobalWindow.h"
#include "nsJSUtils.h"
#include "nsNetUtil.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "xpcpublic.h"
#include "nsContentUtils.h"
#include "nsDocShell.h"
#include "nsProxyRelease.h"
#include "mozilla/ConsoleTimelineMarker.h"
#include "mozilla/TimestampTimelineMarker.h"

#include "nsIConsoleAPIStorage.h"
#include "nsIDOMWindowUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsILoadContext.h"
#include "nsISensitiveInfoHiddenURI.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIWebNavigation.h"
#include "nsIXPConnect.h"

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

// This value is taken from ConsoleAPIStorage.js
#define STORAGE_MAX_EVENTS 1000

using namespace mozilla::dom::exceptions;
using namespace mozilla::dom::workers;

namespace mozilla {
namespace dom {

struct
ConsoleStructuredCloneData
{
  nsCOMPtr<nsISupports> mParent;
  nsTArray<RefPtr<BlobImpl>> mBlobs;
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
  NS_INLINE_DECL_REFCOUNTING(ConsoleCallData)

  ConsoleCallData()
    : mMethodName(Console::MethodLog)
    , mTimeStamp(JS_Now() / PR_USEC_PER_MSEC)
    , mStartTimerValue(0)
    , mStartTimerStatus(Console::eTimerUnknown)
    , mStopTimerDuration(0)
    , mStopTimerStatus(Console::eTimerUnknown)
    , mCountValue(MAX_PAGE_COUNTERS)
    , mIDType(eUnknown)
    , mOuterIDNumber(0)
    , mInnerIDNumber(0)
    , mStatus(eUnused)
  {}

  bool
  Initialize(JSContext* aCx, Console::MethodName aName,
             const nsAString& aString,
             const Sequence<JS::Value>& aArguments,
             Console* aConsole)
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(aConsole);

    // We must be registered before doing any JS operation otherwise it can
    // happen that mCopiedArguments are not correctly traced.
    aConsole->StoreCallData(this);

    mMethodName = aName;
    mMethodString = aString;

    mGlobal = JS::CurrentGlobalOrNull(aCx);

    for (uint32_t i = 0; i < aArguments.Length(); ++i) {
      if (NS_WARN_IF(!mCopiedArguments.AppendElement(aArguments[i]))) {
        aConsole->UnstoreCallData(this);
        return false;
      }
    }

    return true;
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
  SetOriginAttributes(const OriginAttributes& aOriginAttributes)
  {
    mOriginAttributes = aOriginAttributes;
  }

  void
  SetAddonId(nsIPrincipal* aPrincipal)
  {
    nsAutoString addonId;
    aPrincipal->GetAddonId(addonId);

    mAddonId = addonId;
  }

  bool
  PopulateArgumentsSequence(Sequence<JS::Value>& aSequence) const
  {
    AssertIsOnOwningThread();

    for (uint32_t i = 0; i < mCopiedArguments.Length(); ++i) {
      if (NS_WARN_IF(!aSequence.AppendElement(mCopiedArguments[i],
                                              fallible))) {
        return false;
      }
    }

    return true;
  }

  void
  Trace(const TraceCallbacks& aCallbacks, void* aClosure)
  {
    ConsoleCallData* tmp = this;
    for (uint32_t i = 0; i < mCopiedArguments.Length(); ++i) {
      NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCopiedArguments[i])
    }

    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mGlobal);
  }

  void
  AssertIsOnOwningThread() const
  {
    NS_ASSERT_OWNINGTHREAD(ConsoleCallData);
  }

  JS::Heap<JSObject*> mGlobal;

  // This is a copy of the arguments we received from the DOM bindings. Console
  // object traces them because this ConsoleCallData calls
  // RegisterConsoleCallData() in the Initialize().
  nsTArray<JS::Heap<JS::Value>> mCopiedArguments;

  Console::MethodName mMethodName;
  int64_t mTimeStamp;

  // These values are set in the owning thread and they contain the timestamp of
  // when the new timer has started, the name of it and the status of the
  // creation of it. If status is false, something went wrong. User
  // DOMHighResTimeStamp instead mozilla::TimeStamp because we use
  // monotonicTimer from Performance.now();
  // They will be set on the owning thread and never touched again on that
  // thread. They will be used in order to create a ConsoleTimerStart dictionary
  // when console.time() is used.
  DOMHighResTimeStamp mStartTimerValue;
  nsString mStartTimerLabel;
  Console::TimerStatus mStartTimerStatus;

  // These values are set in the owning thread and they contain the duration,
  // the name and the status of the StopTimer method. If status is false,
  // something went wrong. They will be set on the owning thread and never
  // touched again on that thread. They will be used in order to create a
  // ConsoleTimerEnd dictionary. This members are set when
  // console.timeEnd() is called.
  double mStopTimerDuration;
  nsString mStopTimerLabel;
  Console::TimerStatus mStopTimerStatus;

  // These 2 values are set by IncreaseCounter on the owning thread and they are
  // used CreateCounterValue. These members are set when console.count() is
  // called.
  nsString mCountLabel;
  uint32_t mCountValue;

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

  OriginAttributes mOriginAttributes;

  nsString mAddonId;

  nsString mMethodString;

  // Stack management is complicated, because we want to do it as
  // lazily as possible.  Therefore, we have the following behavior:
  // 1)  mTopStackFrame is initialized whenever we have any JS on the stack
  // 2)  mReifiedStack is initialized if we're created in a worker.
  // 3)  mStack is set (possibly to null if there is no JS on the stack) if
  //     we're created on main thread.
  Maybe<ConsoleStackEntry> mTopStackFrame;
  Maybe<nsTArray<ConsoleStackEntry>> mReifiedStack;
  nsCOMPtr<nsIStackFrame> mStack;

  // mStatus is about the lifetime of this object. Console must take care of
  // keep it alive or not following this enumeration.
  enum {
    // If the object is created but it is owned by some runnable, this is its
    // status. It can be deleted at any time.
    eUnused,

    // When a runnable takes ownership of a ConsoleCallData and send it to
    // different thread, this is its status. Console cannot delete it at this
    // time.
    eInUse,

    // When a runnable owns this ConsoleCallData, we can't delete it directly.
    // instead, we mark it with this new status and we move it in
    // mCallDataStoragePending list in order to keep it alive an trace it
    // correctly. Once the runnable finishs its task, it will delete this object
    // calling ReleaseCallData().
    eToBeDeleted
  } mStatus;

private:
  ~ConsoleCallData()
  {
    AssertIsOnOwningThread();
    MOZ_ASSERT(mStatus != eInUse);
  }
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

class ConsoleRunnable : public WorkerProxyToMainThreadRunnable
                      , public StructuredCloneHolderBase
{
public:
  explicit ConsoleRunnable(Console* aConsole)
    : WorkerProxyToMainThreadRunnable(GetCurrentThreadWorkerPrivate())
    , mConsole(aConsole)
  {}

  virtual
  ~ConsoleRunnable()
  {
    // Clear the StructuredCloneHolderBase class.
    Clear();
  }

  bool
  Dispatch(JSContext* aCx)
  {
    mWorkerPrivate->AssertIsOnWorkerThread();

    if (NS_WARN_IF(!PreDispatch(aCx))) {
      RunBackOnWorkerThreadForCleanup();
      return false;
    }

    if (NS_WARN_IF(!WorkerProxyToMainThreadRunnable::Dispatch())) {
      // RunBackOnWorkerThreadForCleanup() will be called by
      // WorkerProxyToMainThreadRunnable::Dispatch().
      return false;
    }

    return true;
  }

protected:
  void
  RunOnMainThread() override
  {
    AssertIsOnMainThread();

    // Walk up to our containing page
    WorkerPrivate* wp = mWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    nsPIDOMWindowInner* window = wp->GetWindow();
    if (!window) {
      RunWindowless();
    } else {
      RunWithWindow(window);
    }
  }

  void
  RunWithWindow(nsPIDOMWindowInner* aWindow)
  {
    AssertIsOnMainThread();

    AutoJSAPI jsapi;
    MOZ_ASSERT(aWindow);

    RefPtr<nsGlobalWindowInner> win = nsGlobalWindowInner::Cast(aWindow);
    if (NS_WARN_IF(!jsapi.Init(win))) {
      return;
    }

    nsPIDOMWindowOuter* outerWindow = aWindow->GetOuterWindow();
    if (NS_WARN_IF(!outerWindow)) {
      return;
    }

    RunConsole(jsapi.cx(), outerWindow, aWindow);
  }

  void
  RunWindowless()
  {
    AssertIsOnMainThread();

    WorkerPrivate* wp = mWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    MOZ_ASSERT(!wp->GetWindow());

    AutoJSAPI jsapi;
    jsapi.Init();

    JSContext* cx = jsapi.cx();

    JS::Rooted<JSObject*> global(cx, mConsole->GetOrCreateSandbox(cx, wp->GetPrincipal()));
    if (NS_WARN_IF(!global)) {
      return;
    }

    // The GetOrCreateSandbox call returns a proxy to the actual sandbox object.
    // We don't need a proxy here.
    global = js::UncheckedUnwrap(global);

    JSAutoCompartment ac(cx, global);

    RunConsole(cx, nullptr, nullptr);
  }

  void
  RunBackOnWorkerThreadForCleanup() override
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    ReleaseData();
    mConsole = nullptr;
  }

  // This method is called in the owning thread of the Console object.
  virtual bool
  PreDispatch(JSContext* aCx) = 0;

  // This method is called in the main-thread.
  virtual void
  RunConsole(JSContext* aCx, nsPIDOMWindowOuter* aOuterWindow,
             nsPIDOMWindowInner* aInnerWindow) = 0;

  // This method is called in the owning thread of the Console object.
  virtual void
  ReleaseData() = 0;

  virtual JSObject* CustomReadHandler(JSContext* aCx,
                                      JSStructuredCloneReader* aReader,
                                      uint32_t aTag,
                                      uint32_t aIndex) override
  {
    AssertIsOnMainThread();

    if (aTag == CONSOLE_TAG_BLOB) {
      MOZ_ASSERT(mClonedData.mBlobs.Length() > aIndex);

      JS::Rooted<JS::Value> val(aCx);
      {
        RefPtr<Blob> blob =
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

  virtual bool CustomWriteHandler(JSContext* aCx,
                                  JSStructuredCloneWriter* aWriter,
                                  JS::Handle<JSObject*> aObj) override
  {
    RefPtr<Blob> blob;
    if (NS_SUCCEEDED(UNWRAP_OBJECT(Blob, aObj, blob)) &&
        blob->Impl()->MayBeClonedToOtherThreads()) {
      if (NS_WARN_IF(!JS_WriteUint32Pair(aWriter, CONSOLE_TAG_BLOB,
                                         mClonedData.mBlobs.Length()))) {
        return false;
      }

      mClonedData.mBlobs.AppendElement(blob->Impl());
      return true;
    }

    if (!JS_ObjectNotWritten(aWriter, aObj)) {
      return false;
    }

    JS::Rooted<JS::Value> value(aCx, JS::ObjectOrNullValue(aObj));
    JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, value));
    if (NS_WARN_IF(!jsString)) {
      return false;
    }

    if (NS_WARN_IF(!JS_WriteString(aWriter, jsString))) {
      return false;
    }

    return true;
  }

  // This must be released on the worker thread.
  RefPtr<Console> mConsole;

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
  {
    MOZ_ASSERT(aCallData);
    mWorkerPrivate->AssertIsOnWorkerThread();
    mCallData->AssertIsOnOwningThread();
  }

private:
  ~ConsoleCallDataRunnable()
  {
    MOZ_ASSERT(!mCallData);
  }

  bool
  PreDispatch(JSContext* aCx) override
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
    mCallData->AssertIsOnOwningThread();

    ClearException ce(aCx);

    JS::Rooted<JSObject*> arguments(aCx,
      JS_NewArrayObject(aCx, mCallData->mCopiedArguments.Length()));
    if (NS_WARN_IF(!arguments)) {
      return false;
    }

    JS::Rooted<JS::Value> arg(aCx);
    for (uint32_t i = 0; i < mCallData->mCopiedArguments.Length(); ++i) {
      arg = mCallData->mCopiedArguments[i];
      if (NS_WARN_IF(!JS_DefineElement(aCx, arguments, i, arg,
                                       JSPROP_ENUMERATE))) {
        return false;
      }
    }

    JS::Rooted<JS::Value> value(aCx, JS::ObjectValue(*arguments));

    if (NS_WARN_IF(!Write(aCx, value))) {
      return false;
    }

    mCallData->mStatus = ConsoleCallData::eInUse;
    return true;
  }

  void
  RunConsole(JSContext* aCx, nsPIDOMWindowOuter* aOuterWindow,
             nsPIDOMWindowInner* aInnerWindow) override
  {
    AssertIsOnMainThread();

    // The windows have to run in parallel.
    MOZ_ASSERT(!!aOuterWindow == !!aInnerWindow);

    if (aOuterWindow) {
      mCallData->SetIDs(aOuterWindow->WindowID(), aInnerWindow->WindowID());
    } else {
      ConsoleStackEntry frame;
      if (mCallData->mTopStackFrame) {
        frame = *mCallData->mTopStackFrame;
      }

      nsString id = frame.mFilename;
      nsString innerID;
      if (mWorkerPrivate->IsSharedWorker()) {
        innerID = NS_LITERAL_STRING("SharedWorker");
      } else if (mWorkerPrivate->IsServiceWorker()) {
        innerID = NS_LITERAL_STRING("ServiceWorker");
        // Use scope as ID so the webconsole can decide if the message should
        // show up per tab
        CopyASCIItoUTF16(mWorkerPrivate->ServiceWorkerScope(), id);
      } else {
        innerID = NS_LITERAL_STRING("Worker");
      }

      mCallData->SetIDs(id, innerID);
    }

    // Now we could have the correct window (if we are not window-less).
    mClonedData.mParent = aInnerWindow;

    ProcessCallData(aCx);

    mClonedData.mParent = nullptr;
  }

  virtual void
  ReleaseData() override
  {
    mConsole->AssertIsOnOwningThread();

    if (mCallData->mStatus == ConsoleCallData::eToBeDeleted) {
      mConsole->ReleaseCallData(mCallData);
    } else {
      MOZ_ASSERT(mCallData->mStatus == ConsoleCallData::eInUse);
      mCallData->mStatus = ConsoleCallData::eUnused;
    }

    mCallData = nullptr;
  }

  void
  ProcessCallData(JSContext* aCx)
  {
    AssertIsOnMainThread();

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

    Sequence<JS::Value> values;
    SequenceRooter<JS::Value> arguments(aCx, &values);

    for (uint32_t i = 0; i < length; ++i) {
      JS::Rooted<JS::Value> value(aCx);

      if (!JS_GetElement(aCx, argumentsObj, i, &value)) {
        return;
      }

      if (!values.AppendElement(value, fallible)) {
        return;
      }
    }

    MOZ_ASSERT(values.Length() == length);

    mConsole->ProcessCallData(aCx, mCallData, values);
  }

  RefPtr<ConsoleCallData> mCallData;
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

    JS::Rooted<JSObject*> arguments(aCx,
      JS_NewArrayObject(aCx, mArguments.Length()));
    if (NS_WARN_IF(!arguments)) {
      return false;
    }

    JS::Rooted<JS::Value> arg(aCx);
    for (uint32_t i = 0; i < mArguments.Length(); ++i) {
      arg = mArguments[i];
      if (NS_WARN_IF(!JS_DefineElement(aCx, arguments, i, arg,
                                       JSPROP_ENUMERATE))) {
        return false;
      }
    }

    JS::Rooted<JS::Value> value(aCx, JS::ObjectValue(*arguments));

    if (NS_WARN_IF(!Write(aCx, value))) {
      return false;
    }

    return true;
  }

  void
  RunConsole(JSContext* aCx, nsPIDOMWindowOuter* aOuterWindow,
             nsPIDOMWindowInner* aInnerWindow) override
  {
    AssertIsOnMainThread();

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
    if (NS_WARN_IF(!argumentsObj)) {
      return;
    }

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

    mConsole->ProfileMethodInternal(aCx, mAction, arguments);
  }

  virtual void
  ReleaseData() override
  {}

  nsString mAction;

  // This is a reference of the sequence of arguments we receive from the DOM
  // bindings and it's rooted by them. It's only used on the owning thread in
  // PreDispatch().
  const Sequence<JS::Value>& mArguments;
};

NS_IMPL_CYCLE_COLLECTION_CLASS(Console)

// We don't need to traverse/unlink mStorage and mSandbox because they are not
// CCed objects and they are only used on the main thread, even when this
// Console object is used on workers.

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(Console)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsoleEventNotifier)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDumpFunction)
  tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(Console)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsoleEventNotifier)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDumpFunction)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(Console)
  for (uint32_t i = 0; i < tmp->mCallDataStorage.Length(); ++i) {
    tmp->mCallDataStorage[i]->Trace(aCallbacks, aClosure);
  }

  for (uint32_t i = 0; i < tmp->mCallDataStoragePending.Length(); ++i) {
    tmp->mCallDataStoragePending[i]->Trace(aCallbacks, aClosure);
  }

NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Console)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Console)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Console)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

/* static */ already_AddRefed<Console>
Console::Create(nsPIDOMWindowInner* aWindow, ErrorResult& aRv)
{
  MOZ_ASSERT_IF(NS_IsMainThread(), aWindow);

  RefPtr<Console> console = new Console(aWindow);
  console->Initialize(aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return console.forget();
}

Console::Console(nsPIDOMWindowInner* aWindow)
  : mWindow(aWindow)
  , mOuterID(0)
  , mInnerID(0)
  , mDumpToStdout(false)
  , mStatus(eUnknown)
  , mCreationTimeStamp(TimeStamp::Now())
{
  if (mWindow) {
    mInnerID = mWindow->WindowID();

    // Without outerwindow any console message coming from this object will not
    // shown in the devtools webconsole. But this should be fine because
    // probably we are shutting down, or the window is CCed/GCed.
    nsPIDOMWindowOuter* outerWindow = mWindow->GetOuterWindow();
    if (outerWindow) {
      mOuterID = outerWindow->WindowID();
    }
  }

  mozilla::HoldJSObjects(this);
}

Console::~Console()
{
  AssertIsOnOwningThread();
  Shutdown();
  mozilla::DropJSObjects(this);
}

void
Console::Initialize(ErrorResult& aRv)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mStatus == eUnknown);

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (NS_WARN_IF(!obs)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    if (mWindow) {
      aRv = obs->AddObserver(this, "inner-window-destroyed", true);
      if (NS_WARN_IF(aRv.Failed())) {
        return;
      }
    }

    aRv = obs->AddObserver(this, "memory-pressure", true);
    if (NS_WARN_IF(aRv.Failed())) {
      return;
    }
  }

  mStatus = eInitialized;
}

void
Console::Shutdown()
{
  AssertIsOnOwningThread();

  if (mStatus == eUnknown || mStatus == eShuttingDown) {
    return;
  }

  if (NS_IsMainThread()) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "inner-window-destroyed");
      obs->RemoveObserver(this, "memory-pressure");
    }
  }

  NS_ReleaseOnMainThreadSystemGroup("Console::mStorage", mStorage.forget());
  NS_ReleaseOnMainThreadSystemGroup("Console::mSandbox", mSandbox.forget());

  mTimerRegistry.Clear();
  mCounterRegistry.Clear();

  mCallDataStorage.Clear();
  mCallDataStoragePending.Clear();

  mStatus = eShuttingDown;
}

NS_IMETHODIMP
Console::Observe(nsISupports* aSubject, const char* aTopic,
                 const char16_t* aData)
{
  AssertIsOnMainThread();

  if (!strcmp(aTopic, "inner-window-destroyed")) {
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

    uint64_t innerID;
    nsresult rv = wrapper->GetData(&innerID);
    NS_ENSURE_SUCCESS(rv, rv);

    if (innerID == mInnerID) {
      Shutdown();
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, "memory-pressure")) {
    ClearStorage();
    return NS_OK;
  }

  return NS_OK;
}

void
Console::ClearStorage()
{
  mCallDataStorage.Clear();
}

#define METHOD(name, string)                                                   \
  /* static */ void                                                            \
  Console::name(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData) \
  {                                                                            \
    Method(aGlobal, Method##name, NS_LITERAL_STRING(string), aData);           \
  }

METHOD(Log, "log")
METHOD(Info, "info")
METHOD(Warn, "warn")
METHOD(Error, "error")
METHOD(Exception, "exception")
METHOD(Debug, "debug")
METHOD(Table, "table")
METHOD(Trace, "trace")

// Displays an interactive listing of all the properties of an object.
METHOD(Dir, "dir");
METHOD(Dirxml, "dirxml");

METHOD(Group, "group")
METHOD(GroupCollapsed, "groupCollapsed")

#undef METHOD

/* static */ void
Console::Clear(const GlobalObject& aGlobal)
{
  const Sequence<JS::Value> data;
  Method(aGlobal, MethodClear, NS_LITERAL_STRING("clear"), data);
}

/* static */ void
Console::GroupEnd(const GlobalObject& aGlobal)
{
  const Sequence<JS::Value> data;
  Method(aGlobal, MethodGroupEnd, NS_LITERAL_STRING("groupEnd"), data);
}

/* static */ void
Console::Time(const GlobalObject& aGlobal, const nsAString& aLabel)
{
  StringMethod(aGlobal, aLabel, MethodTime, NS_LITERAL_STRING("time"));
}

/* static */ void
Console::TimeEnd(const GlobalObject& aGlobal, const nsAString& aLabel)
{
  StringMethod(aGlobal, aLabel, MethodTimeEnd, NS_LITERAL_STRING("timeEnd"));
}

/* static */ void
Console::StringMethod(const GlobalObject& aGlobal, const nsAString& aLabel,
                      MethodName aMethodName, const nsAString& aMethodString)
{
  RefPtr<Console> console = GetConsole(aGlobal);
  if (!console) {
    return;
  }

  console->StringMethodInternal(aGlobal.Context(), aLabel, aMethodName,
                                aMethodString);
}

void
Console::StringMethodInternal(JSContext* aCx, const nsAString& aLabel,
                              MethodName aMethodName,
                              const nsAString& aMethodString)
{
  ClearException ce(aCx);

  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(aCx, &data);

  JS::Rooted<JS::Value> value(aCx);
  if (!dom::ToJSValue(aCx, aLabel, &value)) {
    return;
  }

  if (!data.AppendElement(value, fallible)) {
    return;
  }

  MethodInternal(aCx, aMethodName, aMethodString, data);
}

/* static */ void
Console::TimeStamp(const GlobalObject& aGlobal,
                   const JS::Handle<JS::Value> aData)
{
  JSContext* cx = aGlobal.Context();

  ClearException ce(cx);

  Sequence<JS::Value> data;
  SequenceRooter<JS::Value> rooter(cx, &data);

  if (aData.isString() && !data.AppendElement(aData, fallible)) {
    return;
  }

  Method(aGlobal, MethodTimeStamp, NS_LITERAL_STRING("timeStamp"), data);
}

/* static */ void
Console::Profile(const GlobalObject& aGlobal, const Sequence<JS::Value>& aData)
{
  ProfileMethod(aGlobal, NS_LITERAL_STRING("profile"), aData);
}

/* static */ void
Console::ProfileEnd(const GlobalObject& aGlobal,
                    const Sequence<JS::Value>& aData)
{
  ProfileMethod(aGlobal, NS_LITERAL_STRING("profileEnd"), aData);
}

/* static */ void
Console::ProfileMethod(const GlobalObject& aGlobal, const nsAString& aAction,
                       const Sequence<JS::Value>& aData)
{
  RefPtr<Console> console = GetConsole(aGlobal);
  if (!console) {
    return;
  }

  JSContext* cx = aGlobal.Context();
  console->ProfileMethodInternal(cx, aAction, aData);
}

void
Console::ProfileMethodInternal(JSContext* aCx, const nsAString& aAction,
                               const Sequence<JS::Value>& aData)
{
  // Make all Console API no-op if DevTools aren't enabled.
  if (!nsContentUtils::DevToolsEnabled(aCx)) {
    return;
  }

  MaybeExecuteDumpFunction(aCx, aAction, aData);

  if (!NS_IsMainThread()) {
    // Here we are in a worker thread.
    RefPtr<ConsoleProfileRunnable> runnable =
      new ConsoleProfileRunnable(this, aAction, aData);

    runnable->Dispatch(aCx);
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

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  nsCOMPtr<nsISupports> wrapper;
  const nsIID& iid = NS_GET_IID(nsISupports);

  if (NS_FAILED(xpc->WrapJS(aCx, eventObj, iid, getter_AddRefs(wrapper)))) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(wrapper, "console-api-profiler", nullptr);
  }
}

/* static */ void
Console::Assert(const GlobalObject& aGlobal, bool aCondition,
                const Sequence<JS::Value>& aData)
{
  if (!aCondition) {
    Method(aGlobal, MethodAssert, NS_LITERAL_STRING("assert"), aData);
  }
}

/* static */ void
Console::Count(const GlobalObject& aGlobal, const nsAString& aLabel)
{
  StringMethod(aGlobal, aLabel, MethodCount, NS_LITERAL_STRING("count"));
}

namespace {

nsresult
StackFrameToStackEntry(JSContext* aCx, nsIStackFrame* aStackFrame,
                       ConsoleStackEntry& aStackEntry)
{
  MOZ_ASSERT(aStackFrame);

  nsresult rv = aStackFrame->GetFilename(aCx, aStackEntry.mFilename);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t lineNumber;
  rv = aStackFrame->GetLineNumber(aCx, &lineNumber);
  NS_ENSURE_SUCCESS(rv, rv);

  aStackEntry.mLineNumber = lineNumber;

  int32_t columnNumber;
  rv = aStackFrame->GetColumnNumber(aCx, &columnNumber);
  NS_ENSURE_SUCCESS(rv, rv);

  aStackEntry.mColumnNumber = columnNumber;

  rv = aStackFrame->GetName(aCx, aStackEntry.mFunctionName);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString cause;
  rv = aStackFrame->GetAsyncCause(aCx, cause);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!cause.IsEmpty()) {
    aStackEntry.mAsyncCause.Construct(cause);
  }

  return NS_OK;
}

nsresult
ReifyStack(JSContext* aCx, nsIStackFrame* aStack,
           nsTArray<ConsoleStackEntry>& aRefiedStack)
{
  nsCOMPtr<nsIStackFrame> stack(aStack);

  while (stack) {
    ConsoleStackEntry& data = *aRefiedStack.AppendElement();
    nsresult rv = StackFrameToStackEntry(aCx, stack, data);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIStackFrame> caller;
    rv = stack->GetCaller(aCx, getter_AddRefs(caller));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!caller) {
      rv = stack->GetAsyncCaller(aCx, getter_AddRefs(caller));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    stack.swap(caller);
  }

  return NS_OK;
}

} // anonymous namespace

// Queue a call to a console method. See the CALL_DELAY constant.
/* static */ void
Console::Method(const GlobalObject& aGlobal, MethodName aMethodName,
                const nsAString& aMethodString,
                const Sequence<JS::Value>& aData)
{
  RefPtr<Console> console = GetConsole(aGlobal);
  if (!console) {
    return;
  }

  console->MethodInternal(aGlobal.Context(), aMethodName, aMethodString,
                          aData);
}

void
Console::MethodInternal(JSContext* aCx, MethodName aMethodName,
                        const nsAString& aMethodString,
                        const Sequence<JS::Value>& aData)
{
  // Make all Console API no-op if DevTools aren't enabled.
  if (!nsContentUtils::DevToolsEnabled(aCx)) {
    return;
  }

  AssertIsOnOwningThread();

  RefPtr<ConsoleCallData> callData(new ConsoleCallData());

  ClearException ce(aCx);

  if (NS_WARN_IF(!callData->Initialize(aCx, aMethodName, aMethodString,
                                       aData, this))) {
    return;
  }

  OriginAttributes oa;

  if (NS_IsMainThread()) {
    if (mWindow) {
      // Save the principal's OriginAttributes in the console event data
      // so that we will be able to filter messages by origin attributes.
      nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(mWindow);
      if (NS_WARN_IF(!sop)) {
        return;
      }

      nsCOMPtr<nsIPrincipal> principal = sop->GetPrincipal();
      if (NS_WARN_IF(!principal)) {
        return;
      }

      oa = principal->OriginAttributesRef();
      callData->SetAddonId(principal);

#ifdef DEBUG
      if (!nsContentUtils::IsSystemPrincipal(principal)) {
        nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(mWindow);
        if (webNav) {
          nsCOMPtr<nsILoadContext> loadContext = do_QueryInterface(webNav);
          MOZ_ASSERT(loadContext);

          bool pb;
          if (NS_SUCCEEDED(loadContext->GetUsePrivateBrowsing(&pb))) {
            MOZ_ASSERT(pb == !!oa.mPrivateBrowsingId);
          }
        }
      }
#endif
    }
  } else {
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(workerPrivate);
    oa = workerPrivate->GetOriginAttributes();
  }

  callData->SetOriginAttributes(oa);

  JS::StackCapture captureMode = ShouldIncludeStackTrace(aMethodName) ?
    JS::StackCapture(JS::MaxFrames(DEFAULT_MAX_STACKTRACE_DEPTH)) :
    JS::StackCapture(JS::FirstSubsumedFrame(aCx));
  nsCOMPtr<nsIStackFrame> stack = CreateStack(aCx, mozilla::Move(captureMode));

  if (stack) {
    callData->mTopStackFrame.emplace();
    nsresult rv = StackFrameToStackEntry(aCx, stack,
                                         *callData->mTopStackFrame);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  if (NS_IsMainThread()) {
    callData->mStack = stack;
  } else {
    // nsIStackFrame is not threadsafe, so we need to snapshot it now,
    // before we post our runnable to the main thread.
    callData->mReifiedStack.emplace();
    nsresult rv = ReifyStack(aCx, stack, *callData->mReifiedStack);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  DOMHighResTimeStamp monotonicTimer;

  // Monotonic timer for 'time' and 'timeEnd'
  if ((aMethodName == MethodTime ||
       aMethodName == MethodTimeEnd ||
       aMethodName == MethodTimeStamp) &&
      !MonotonicTimer(aCx, aMethodName, aData, &monotonicTimer)) {
    return;
  }

  if (aMethodName == MethodTime && !aData.IsEmpty()) {
    callData->mStartTimerStatus = StartTimer(aCx, aData[0],
                                             monotonicTimer,
                                             callData->mStartTimerLabel,
                                             &callData->mStartTimerValue);
  }

  else if (aMethodName == MethodTimeEnd && !aData.IsEmpty()) {
    callData->mStopTimerStatus = StopTimer(aCx, aData[0],
                                           monotonicTimer,
                                           callData->mStopTimerLabel,
                                           &callData->mStopTimerDuration);
  }

  else if (aMethodName == MethodCount) {
    callData->mCountValue = IncreaseCounter(aCx, aData, callData->mCountLabel);
    if (!callData->mCountValue) {
      return;
    }
  }

  // Before processing this CallData differently, it's time to call the dump
  // function.
  if (aMethodName == MethodTrace) {
    MaybeExecuteDumpFunctionForTrace(aCx, stack);
  } else {
    MaybeExecuteDumpFunction(aCx, aMethodString, aData);
  }

  if (NS_IsMainThread()) {
    if (mWindow) {
      callData->SetIDs(mOuterID, mInnerID);
    } else if (!mPassedInnerID.IsEmpty()) {
      callData->SetIDs(NS_LITERAL_STRING("jsm"), mPassedInnerID);
    } else {
      nsAutoString filename;
      if (callData->mTopStackFrame.isSome()) {
        filename = callData->mTopStackFrame->mFilename;
      }

      callData->SetIDs(NS_LITERAL_STRING("jsm"), filename);
    }

    ProcessCallData(aCx, callData, aData);

    // Just because we don't want to expose
    // retrieveConsoleEvents/setConsoleEventHandler to main-thread, we can
    // cleanup the mCallDataStorage:
    UnstoreCallData(callData);
    return;
  }

  // We do this only in workers for now.
  NotifyHandler(aCx, aData, callData);

  RefPtr<ConsoleCallDataRunnable> runnable =
    new ConsoleCallDataRunnable(this, callData);
  Unused << NS_WARN_IF(!runnable->Dispatch(aCx));
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
  nsresult rv = ReifyStack(aCx, stack, reifiedStack);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    Throw(aCx, rv);
    return false;
  }

  JS::Rooted<JS::Value> stackVal(aCx);
  if (NS_WARN_IF(!ToJSValue(aCx, reifiedStack, &stackVal))) {
    return false;
  }

  MOZ_ASSERT(stackVal.isObject());

  js::SetFunctionNativeReserved(callee, SLOT_STACKOBJ, stackVal);
  js::SetFunctionNativeReserved(callee, SLOT_RAW_STACK, JS::UndefinedValue());

  args.rval().set(stackVal);
  return true;
}

void
Console::ProcessCallData(JSContext* aCx, ConsoleCallData* aData,
                         const Sequence<JS::Value>& aArguments)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aData);

  JS::Rooted<JS::Value> eventValue(aCx);

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

  // aCx and aArguments are in the same compartment.
  if (NS_WARN_IF(!PopulateConsoleNotificationInTheTargetScope(aCx, aArguments,
                                                              xpc::PrivilegedJunkScope(),
                                                              &eventValue, aData))) {
    return;
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

  if (aData->mMethodName == MethodClear) {
    DebugOnly<nsresult> rv = mStorage->ClearEvents(innerID);
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "ClearEvents failed");
  }

  if (NS_FAILED(mStorage->RecordEvent(innerID, outerID, eventValue))) {
    NS_WARNING("Failed to record a console event.");
  }
}

bool
Console::PopulateConsoleNotificationInTheTargetScope(JSContext* aCx,
                                                     const Sequence<JS::Value>& aArguments,
                                                     JSObject* aTargetScope,
                                                     JS::MutableHandle<JS::Value> aEventValue,
                                                     ConsoleCallData* aData)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aData);
  MOZ_ASSERT(aTargetScope);

  JS::Rooted<JSObject*> targetScope(aCx, aTargetScope);

  ConsoleStackEntry frame;
  if (aData->mTopStackFrame) {
    frame = *aData->mTopStackFrame;
  }

  ClearException ce(aCx);
  RootedDictionary<ConsoleEvent> event(aCx);

  event.mAddonId = aData->mAddonId;

  event.mID.Construct();
  event.mInnerID.Construct();

  if (aData->mIDType == ConsoleCallData::eString) {
    event.mID.Value().SetAsString() = aData->mOuterIDString;
    event.mInnerID.Value().SetAsString() = aData->mInnerIDString;
  } else if (aData->mIDType == ConsoleCallData::eNumber) {
    event.mID.Value().SetAsUnsignedLongLong() = aData->mOuterIDNumber;
    event.mInnerID.Value().SetAsUnsignedLongLong() = aData->mInnerIDNumber;
  } else {
    // aData->mIDType can be eUnknown when we dispatch notifications via
    // mConsoleEventNotifier.
    event.mID.Value().SetAsUnsignedLongLong() = 0;
    event.mInnerID.Value().SetAsUnsignedLongLong() = 0;
  }

  event.mConsoleID = mConsoleID;
  event.mLevel = aData->mMethodString;
  event.mFilename = frame.mFilename;

  nsCOMPtr<nsIURI> filenameURI;
  nsAutoCString pass;
  if (NS_IsMainThread() &&
      NS_SUCCEEDED(NS_NewURI(getter_AddRefs(filenameURI), frame.mFilename)) &&
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
  event.mPrivate = !!aData->mOriginAttributes.mPrivateBrowsingId;

  switch (aData->mMethodName) {
    case MethodLog:
    case MethodInfo:
    case MethodWarn:
    case MethodError:
    case MethodException:
    case MethodDebug:
    case MethodAssert:
    case MethodGroup:
    case MethodGroupCollapsed:
      event.mArguments.Construct();
      event.mStyles.Construct();
      if (NS_WARN_IF(!ProcessArguments(aCx, aArguments,
                                       event.mArguments.Value(),
                                       event.mStyles.Value()))) {
        return false;
      }

      break;

    default:
      event.mArguments.Construct();
      if (NS_WARN_IF(!ArgumentsToValueList(aArguments,
                                           event.mArguments.Value()))) {
        return false;
      }
  }

  if (aData->mMethodName == MethodGroup ||
      aData->mMethodName == MethodGroupCollapsed) {
    ComposeAndStoreGroupName(aCx, event.mArguments.Value(), event.mGroupName);
  }

  else if (aData->mMethodName == MethodGroupEnd) {
    if (!UnstoreGroupName(event.mGroupName)) {
      return false;
    }
  }

  else if (aData->mMethodName == MethodTime && !aArguments.IsEmpty()) {
    event.mTimer = CreateStartTimerValue(aCx, aData->mStartTimerLabel,
                                         aData->mStartTimerStatus);
  }

  else if (aData->mMethodName == MethodTimeEnd && !aArguments.IsEmpty()) {
    event.mTimer = CreateStopTimerValue(aCx, aData->mStopTimerLabel,
                                        aData->mStopTimerDuration,
                                        aData->mStopTimerStatus);
  }

  else if (aData->mMethodName == MethodCount) {
    event.mCounter = CreateCounterValue(aCx, aData->mCountLabel,
                                        aData->mCountValue);
  }

  JSAutoCompartment ac2(aCx, targetScope);

  if (NS_WARN_IF(!ToJSValue(aCx, event, aEventValue))) {
    return false;
  }

  JS::Rooted<JSObject*> eventObj(aCx, &aEventValue.toObject());
  if (NS_WARN_IF(!JS_DefineProperty(aCx, eventObj, "wrappedJSObject", eventObj,
                                    JSPROP_ENUMERATE))) {
    return false;
  }

  if (ShouldIncludeStackTrace(aData->mMethodName)) {
    // Now define the "stacktrace" property on eventObj.  There are two cases
    // here.  Either we came from a worker and have a reified stack, or we want
    // to define a getter that will lazily reify the stack.
    if (aData->mReifiedStack) {
      JS::Rooted<JS::Value> stacktrace(aCx);
      if (NS_WARN_IF(!ToJSValue(aCx, *aData->mReifiedStack, &stacktrace)) ||
          NS_WARN_IF(!JS_DefineProperty(aCx, eventObj, "stacktrace", stacktrace,
                                        JSPROP_ENUMERATE))) {
        return false;
      }
    } else {
      JSFunction* fun = js::NewFunctionWithReserved(aCx, LazyStackGetter, 0, 0,
                                                    "stacktrace");
      if (NS_WARN_IF(!fun)) {
        return false;
      }

      JS::Rooted<JSObject*> funObj(aCx, JS_GetFunctionObject(fun));

      // We want to store our stack in the function and have it stay alive.  But
      // we also need sane access to the C++ nsIStackFrame.  So store both a JS
      // wrapper and the raw pointer: the former will keep the latter alive.
      JS::Rooted<JS::Value> stackVal(aCx);
      nsresult rv = nsContentUtils::WrapNative(aCx, aData->mStack,
                                               &stackVal);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return false;
      }

      js::SetFunctionNativeReserved(funObj, SLOT_STACKOBJ, stackVal);
      js::SetFunctionNativeReserved(funObj, SLOT_RAW_STACK,
                                    JS::PrivateValue(aData->mStack.get()));

      if (NS_WARN_IF(!JS_DefineProperty(aCx, eventObj, "stacktrace",
                                        JS_DATA_TO_FUNC_PTR(JSNative, funObj.get()),
                                        nullptr,
                                        JSPROP_ENUMERATE |
                                        JSPROP_GETTER | JSPROP_SETTER))) {
        return false;
      }
    }
  }

  return true;
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
    if (NS_WARN_IF(!str)) {
      return false;
    }

    if (NS_WARN_IF(!aSequence.AppendElement(JS::StringValue(str), fallible))) {
      return false;
    }

    aOutput.Truncate();
  }

  return true;
}

} // namespace

bool
Console::ProcessArguments(JSContext* aCx,
                          const Sequence<JS::Value>& aData,
                          Sequence<JS::Value>& aSequence,
                          Sequence<nsString>& aStyles) const
{
  if (aData.IsEmpty()) {
    return true;
  }

  if (aData.Length() == 1 || !aData[0].isString()) {
    return ArgumentsToValueList(aData, aSequence);
  }

  JS::Rooted<JS::Value> format(aCx, aData[0]);
  JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, format));
  if (NS_WARN_IF(!jsString)) {
    return false;
  }

  nsAutoJSString string;
  if (NS_WARN_IF(!string.init(aCx, jsString))) {
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
        if (NS_WARN_IF(!FlushOutput(aCx, aSequence, output))) {
          return false;
        }

        JS::Rooted<JS::Value> v(aCx);
        if (index < aData.Length()) {
          v = aData[index++];
        }

        if (NS_WARN_IF(!aSequence.AppendElement(v, fallible))) {
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

        if (NS_WARN_IF(!FlushOutput(aCx, aSequence, output))) {
          return false;
        }

        if (index < aData.Length()) {
          JS::Rooted<JS::Value> v(aCx, aData[index++]);
          JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, v));
          if (NS_WARN_IF(!jsString)) {
            return false;
          }

          int32_t diff = aSequence.Length() - aStyles.Length();
          if (diff > 0) {
            for (int32_t i = 0; i < diff; i++) {
              if (NS_WARN_IF(!aStyles.AppendElement(VoidString(), fallible))) {
                return false;
              }
            }
          }

          nsAutoJSString string;
          if (NS_WARN_IF(!string.init(aCx, jsString))) {
            return false;
          }

          if (NS_WARN_IF(!aStyles.AppendElement(string, fallible))) {
            return false;
          }
        }
        break;
      }

      case 's':
        if (index < aData.Length()) {
          JS::Rooted<JS::Value> value(aCx, aData[index++]);
          JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, value));
          if (NS_WARN_IF(!jsString)) {
            return false;
          }

          nsAutoJSString v;
          if (NS_WARN_IF(!v.init(aCx, jsString))) {
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
          if (NS_WARN_IF(!JS::ToInt32(aCx, value, &v))) {
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
          if (NS_WARN_IF(!JS::ToNumber(aCx, value, &v))) {
            return false;
          }

          // nspr returns "nan", but we want to expose it as "NaN"
          if (std::isnan(v)) {
            output.AppendFloat(v);
          } else {
            nsCString format;
            MakeFormatString(format, integer, mantissa, 'f');
            output.AppendPrintf(format.get(), v);
          }
        }
        break;

      default:
        output.Append(tmp);
        break;
    }
  }

  if (NS_WARN_IF(!FlushOutput(aCx, aSequence, output))) {
    return false;
  }

  // Discard trailing style element if there is no output to apply it to.
  if (aStyles.Length() > aSequence.Length()) {
    aStyles.TruncateLength(aSequence.Length());
  }

  // The rest of the array, if unused by the format string.
  for (; index < aData.Length(); ++index) {
    if (NS_WARN_IF(!aSequence.AppendElement(aData[index], fallible))) {
      return false;
    }
  }

  return true;
}

void
Console::MakeFormatString(nsCString& aFormat, int32_t aInteger,
                          int32_t aMantissa, char aCh) const
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
Console::ComposeAndStoreGroupName(JSContext* aCx,
                                  const Sequence<JS::Value>& aData,
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

  mGroupStack.AppendElement(aName);
}

bool
Console::UnstoreGroupName(nsAString& aName)
{
  if (mGroupStack.IsEmpty()) {
    return false;
  }

  uint32_t pos = mGroupStack.Length() - 1;
  aName = mGroupStack[pos];
  mGroupStack.RemoveElementAt(pos);
  return true;
}

Console::TimerStatus
Console::StartTimer(JSContext* aCx, const JS::Value& aName,
                    DOMHighResTimeStamp aTimestamp,
                    nsAString& aTimerLabel,
                    DOMHighResTimeStamp* aTimerValue)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aTimerValue);

  *aTimerValue = 0;

  if (NS_WARN_IF(mTimerRegistry.Count() >= MAX_PAGE_TIMERS)) {
    return eTimerMaxReached;
  }

  JS::Rooted<JS::Value> name(aCx, aName);
  JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, name));
  if (NS_WARN_IF(!jsString)) {
    return eTimerJSException;
  }

  nsAutoJSString label;
  if (NS_WARN_IF(!label.init(aCx, jsString))) {
    return eTimerJSException;
  }

  aTimerLabel = label;

  auto entry = mTimerRegistry.LookupForAdd(label);
  if (entry) {
    return eTimerAlreadyExists;
  }
  entry.OrInsert([&aTimestamp](){ return aTimestamp; });

  *aTimerValue = aTimestamp;
  return eTimerDone;
}

JS::Value
Console::CreateStartTimerValue(JSContext* aCx, const nsAString& aTimerLabel,
                               TimerStatus aTimerStatus) const
{
  MOZ_ASSERT(aTimerStatus != eTimerUnknown);

  if (aTimerStatus != eTimerDone) {
    return CreateTimerError(aCx, aTimerLabel, aTimerStatus);
  }

  RootedDictionary<ConsoleTimerStart> timer(aCx);

  timer.mName = aTimerLabel;

  JS::Rooted<JS::Value> value(aCx);
  if (!ToJSValue(aCx, timer, &value)) {
    return JS::UndefinedValue();
  }

  return value;
}

Console::TimerStatus
Console::StopTimer(JSContext* aCx, const JS::Value& aName,
                   DOMHighResTimeStamp aTimestamp,
                   nsAString& aTimerLabel,
                   double* aTimerDuration)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aTimerDuration);

  *aTimerDuration = 0;

  JS::Rooted<JS::Value> name(aCx, aName);
  JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, name));
  if (NS_WARN_IF(!jsString)) {
    return eTimerJSException;
  }

  nsAutoJSString key;
  if (NS_WARN_IF(!key.init(aCx, jsString))) {
    return eTimerJSException;
  }

  aTimerLabel = key;

  DOMHighResTimeStamp value = 0;
  if (!mTimerRegistry.Remove(key, &value)) {
    NS_WARNING("mTimerRegistry entry not found");
    return eTimerDoesntExist;
  }

  *aTimerDuration = aTimestamp - value;
  return eTimerDone;
}

JS::Value
Console::CreateStopTimerValue(JSContext* aCx, const nsAString& aLabel,
                              double aDuration, TimerStatus aStatus) const
{
  if (aStatus != eTimerDone) {
    return CreateTimerError(aCx, aLabel, aStatus);
  }

  RootedDictionary<ConsoleTimerEnd> timer(aCx);
  timer.mName = aLabel;
  timer.mDuration = aDuration;

  JS::Rooted<JS::Value> value(aCx);
  if (!ToJSValue(aCx, timer, &value)) {
    return JS::UndefinedValue();
  }

  return value;
}

JS::Value
Console::CreateTimerError(JSContext* aCx, const nsAString& aLabel,
                          TimerStatus aStatus) const
{
  MOZ_ASSERT(aStatus != eTimerUnknown && aStatus != eTimerDone);

  RootedDictionary<ConsoleTimerError> error(aCx);

  error.mName = aLabel;

  switch (aStatus) {
  case eTimerAlreadyExists:
    error.mError.AssignLiteral("timerAlreadyExists");
    break;

  case eTimerDoesntExist:
    error.mError.AssignLiteral("timerDoesntExist");
    break;

  case eTimerJSException:
    error.mError.AssignLiteral("timerJSError");
    break;

  case eTimerMaxReached:
    error.mError.AssignLiteral("maxTimersExceeded");
    break;

  default:
    MOZ_CRASH("Unsupported status");
    break;
  }

  JS::Rooted<JS::Value> value(aCx);
  if (!ToJSValue(aCx, error, &value)) {
    return JS::UndefinedValue();
  }

  return value;
}

bool
Console::ArgumentsToValueList(const Sequence<JS::Value>& aData,
                              Sequence<JS::Value>& aSequence) const
{
  for (uint32_t i = 0; i < aData.Length(); ++i) {
    if (NS_WARN_IF(!aSequence.AppendElement(aData[i], fallible))) {
      return false;
    }
  }

  return true;
}

uint32_t
Console::IncreaseCounter(JSContext* aCx, const Sequence<JS::Value>& aArguments,
                         nsAString& aCountLabel)
{
  AssertIsOnOwningThread();

  ClearException ce(aCx);

  MOZ_ASSERT(!aArguments.IsEmpty());

  JS::Rooted<JS::Value> labelValue(aCx, aArguments[0]);
  JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, labelValue));
  if (!jsString) {
    return 0; // We cannot continue.
  }

  nsAutoJSString string;
  if (!string.init(aCx, jsString)) {
    return 0; // We cannot continue.
  }

  aCountLabel = string;

  const bool maxCountersReached = mCounterRegistry.Count() >= MAX_PAGE_COUNTERS;
  auto entry = mCounterRegistry.LookupForAdd(aCountLabel);
  if (entry) {
    ++entry.Data();
  } else {
    entry.OrInsert([](){ return 1; });
    if (maxCountersReached) {
      // oops, we speculatively added an entry even though we shouldn't
      mCounterRegistry.Remove(aCountLabel);
      return MAX_PAGE_COUNTERS;
    }
  }
  return entry.Data();
}

JS::Value
Console::CreateCounterValue(JSContext* aCx, const nsAString& aCountLabel,
                            uint32_t aCountValue) const
{
  ClearException ce(aCx);

  if (aCountValue == MAX_PAGE_COUNTERS) {
    RootedDictionary<ConsoleCounterError> error(aCx);

    JS::Rooted<JS::Value> value(aCx);
    if (!ToJSValue(aCx, error, &value)) {
      return JS::UndefinedValue();
    }

    return value;
  }

  RootedDictionary<ConsoleCounter> data(aCx);
  data.mLabel = aCountLabel;
  data.mCount = aCountValue;

  JS::Rooted<JS::Value> value(aCx);
  if (!ToJSValue(aCx, data, &value)) {
    return JS::UndefinedValue();
  }

  return value;
}

bool
Console::ShouldIncludeStackTrace(MethodName aMethodName) const
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
  AssertIsOnMainThread();

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

void
Console::StoreCallData(ConsoleCallData* aCallData)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(aCallData);
  MOZ_ASSERT(!mCallDataStorage.Contains(aCallData));
  MOZ_ASSERT(!mCallDataStoragePending.Contains(aCallData));

  mCallDataStorage.AppendElement(aCallData);

  if (mCallDataStorage.Length() > STORAGE_MAX_EVENTS) {
    RefPtr<ConsoleCallData> callData = mCallDataStorage[0];
    mCallDataStorage.RemoveElementAt(0);

    MOZ_ASSERT(callData->mStatus != ConsoleCallData::eToBeDeleted);

    // We cannot delete this object now because we have to trace its JSValues
    // until the pending operation (ConsoleCallDataRunnable) is completed.
    if (callData->mStatus == ConsoleCallData::eInUse) {
      callData->mStatus = ConsoleCallData::eToBeDeleted;
      mCallDataStoragePending.AppendElement(callData);
    }
  }
}

void
Console::UnstoreCallData(ConsoleCallData* aCallData)
{
  AssertIsOnOwningThread();

  MOZ_ASSERT(aCallData);

  MOZ_ASSERT(!mCallDataStoragePending.Contains(aCallData));

  // It can be that mCallDataStorage has been already cleaned in case the
  // processing of the argument of some Console methods triggers the
  // window.close().

  mCallDataStorage.RemoveElement(aCallData);
}

void
Console::ReleaseCallData(ConsoleCallData* aCallData)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallData);
  MOZ_ASSERT(aCallData->mStatus == ConsoleCallData::eToBeDeleted);
  MOZ_ASSERT(mCallDataStoragePending.Contains(aCallData));

  mCallDataStoragePending.RemoveElement(aCallData);
}

void
Console::NotifyHandler(JSContext* aCx, const Sequence<JS::Value>& aArguments,
                       ConsoleCallData* aCallData)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aCallData);

  if (!mConsoleEventNotifier) {
    return;
  }

  JS::Rooted<JS::Value> value(aCx);

  JS::Rooted<JSObject*> callable(aCx, mConsoleEventNotifier->CallableOrNull());
  if (NS_WARN_IF(!callable)) {
    return;
  }

  // aCx and aArguments are in the same compartment because this method is
  // called directly when a Console.something() runs.
  // mConsoleEventNotifier->Callable() is the scope where value will be sent to.
  if (NS_WARN_IF(!PopulateConsoleNotificationInTheTargetScope(aCx, aArguments,
                                                              callable,
                                                              &value,
                                                              aCallData))) {
    return;
  }

  JS::Rooted<JS::Value> ignored(aCx);
  mConsoleEventNotifier->Call(value, &ignored);
}

void
Console::RetrieveConsoleEvents(JSContext* aCx, nsTArray<JS::Value>& aEvents,
                               ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  // We don't want to expose this functionality to main-thread yet.
  MOZ_ASSERT(!NS_IsMainThread());

  JS::Rooted<JSObject*> targetScope(aCx, JS::CurrentGlobalOrNull(aCx));

  for (uint32_t i = 0; i < mCallDataStorage.Length(); ++i) {
    JS::Rooted<JS::Value> value(aCx);

    JS::Rooted<JSObject*> sequenceScope(aCx, mCallDataStorage[i]->mGlobal);
    JSAutoCompartment ac(aCx, sequenceScope);

    Sequence<JS::Value> sequence;
    SequenceRooter<JS::Value> arguments(aCx, &sequence);

    if (!mCallDataStorage[i]->PopulateArgumentsSequence(sequence)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    // Here we have aCx and sequence in the same compartment.
    // targetScope is the destination scope and value will be populated in its
    // compartment.
    if (NS_WARN_IF(!PopulateConsoleNotificationInTheTargetScope(aCx, sequence,
                                                                targetScope,
                                                                &value,
                                                                mCallDataStorage[i]))) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    aEvents.AppendElement(value);
  }
}

void
Console::SetConsoleEventHandler(AnyCallback* aHandler)
{
  AssertIsOnOwningThread();

  // We don't want to expose this functionality to main-thread yet.
  MOZ_ASSERT(!NS_IsMainThread());

  mConsoleEventNotifier = aHandler;
}

void
Console::AssertIsOnOwningThread() const
{
  NS_ASSERT_OWNINGTHREAD(Console);
}

bool
Console::IsShuttingDown() const
{
  MOZ_ASSERT(mStatus != eUnknown);
  return mStatus == eShuttingDown;
}

/* static */ already_AddRefed<Console>
Console::GetConsole(const GlobalObject& aGlobal)
{
  ErrorResult rv;
  RefPtr<Console> console = GetConsoleInternal(aGlobal, rv);
  if (NS_WARN_IF(rv.Failed()) || !console) {
    rv.SuppressException();
    return nullptr;
  }

  console->AssertIsOnOwningThread();

  if (console->IsShuttingDown()) {
    return nullptr;
  }

  return console.forget();
}

/* static */ already_AddRefed<Console>
Console::GetConsoleInternal(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  // Worklet
  if (NS_IsMainThread()) {
    nsCOMPtr<WorkletGlobalScope> workletScope =
      do_QueryInterface(aGlobal.GetAsSupports());
    if (workletScope) {
      return workletScope->GetConsole(aRv);
    }
  }

  // Window
  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> innerWindow =
      do_QueryInterface(aGlobal.GetAsSupports());

    // we are probably running a chrome script.
    if (!innerWindow) {
      RefPtr<Console> console = new Console(nullptr);
      console->Initialize(aRv);
      if (NS_WARN_IF(aRv.Failed())) {
        return nullptr;
      }

      return console.forget();
    }

    nsGlobalWindowInner* window = nsGlobalWindowInner::Cast(innerWindow);
    return window->GetConsole(aRv);
  }

  // Workers
  MOZ_ASSERT(!NS_IsMainThread());

  JSContext* cx = aGlobal.Context();
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(cx);
  MOZ_ASSERT(workerPrivate);

  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(aGlobal.GetAsSupports());
  if (NS_WARN_IF(!global)) {
    return nullptr;
  }

  WorkerGlobalScope* scope = workerPrivate->GlobalScope();
  MOZ_ASSERT(scope);

  // Normal worker scope.
  if (scope == global) {
    return scope->GetConsole(aRv);
  }

  // Debugger worker scope
  else {
    WorkerDebuggerGlobalScope* debuggerScope =
      workerPrivate->DebuggerGlobalScope();
    MOZ_ASSERT(debuggerScope);
    MOZ_ASSERT(debuggerScope == global, "Which kind of global do we have?");

    return debuggerScope->GetConsole(aRv);
  }
}

bool
Console::MonotonicTimer(JSContext* aCx, MethodName aMethodName,
                        const Sequence<JS::Value>& aData,
                        DOMHighResTimeStamp* aTimeStamp)
{
  if (mWindow) {
    nsGlobalWindowInner *win = nsGlobalWindowInner::Cast(mWindow);
    MOZ_ASSERT(win);

    RefPtr<Performance> performance = win->GetPerformance();
    if (!performance) {
      return false;
    }

    *aTimeStamp = performance->Now();

    nsDocShell* docShell = static_cast<nsDocShell*>(mWindow->GetDocShell());
    RefPtr<TimelineConsumers> timelines = TimelineConsumers::Get();
    bool isTimelineRecording = timelines && timelines->HasConsumer(docShell);

    // The 'timeStamp' recordings do not need an argument; use empty string
    // if no arguments passed in.
    if (isTimelineRecording && aMethodName == MethodTimeStamp) {
      JS::Rooted<JS::Value> value(aCx, aData.Length() == 0
        ? JS_GetEmptyStringValue(aCx)
        : aData[0]);
      JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, value));
      if (!jsString) {
        return false;
      }

      nsAutoJSString key;
      if (!key.init(aCx, jsString)) {
        return false;
      }

      timelines->AddMarkerForDocShell(docShell, Move(
        MakeUnique<TimestampTimelineMarker>(key)));
    }
    // For `console.time(foo)` and `console.timeEnd(foo)`.
    else if (isTimelineRecording && aData.Length() == 1) {
      JS::Rooted<JS::Value> value(aCx, aData[0]);
      JS::Rooted<JSString*> jsString(aCx, JS::ToString(aCx, value));
      if (!jsString) {
        return false;
      }

      nsAutoJSString key;
      if (!key.init(aCx, jsString)) {
        return false;
      }

      timelines->AddMarkerForDocShell(docShell, Move(
        MakeUnique<ConsoleTimelineMarker>(
          key, aMethodName == MethodTime ? MarkerTracingType::START
                                         : MarkerTracingType::END)));
    }

    return true;
  }

  if (NS_IsMainThread()) {
    double duration = (TimeStamp::Now() - mCreationTimeStamp).ToMilliseconds();

    // Round down to the nearest 5us, because if the timer is too accurate
    // people can do nasty timing attacks with it.  See similar code in the
    // worker Performance implementation.
    const double maxResolutionMs = 0.005;
    return nsRFPService::ReduceTimePrecisionAsMSecs(
      floor(duration / maxResolutionMs) * maxResolutionMs);
    return true;
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  *aTimeStamp = workerPrivate->TimeStampToDOMHighRes(TimeStamp::Now());
  return true;
}

/* static */ already_AddRefed<ConsoleInstance>
Console::CreateInstance(const GlobalObject& aGlobal,
                        const ConsoleInstanceOptions& aOptions)
{
  RefPtr<ConsoleInstance> console = new ConsoleInstance(aOptions);
  return console.forget();
}

void
Console::MaybeExecuteDumpFunction(JSContext* aCx,
                                  const nsAString& aMethodName,
                                  const Sequence<JS::Value>& aData)
{
  if (!mDumpFunction && !mDumpToStdout) {
    return;
  }

  nsAutoString message;
  message.AssignLiteral("console.");
  message.Append(aMethodName);
  message.AppendLiteral(": ");

  if (!mDumpPrefix.IsEmpty()) {
    message.Append(mDumpPrefix);
    message.AppendLiteral(": ");
  }

  for (uint32_t i = 0; i < aData.Length(); ++i) {
    JS::Rooted<JS::Value> v(aCx, aData[i]);
    JS::Rooted<JSString*> jsString(aCx, JS_ValueToSource(aCx, v));
    if (!jsString) {
      continue;
    }

    nsAutoJSString string;
    if (NS_WARN_IF(!string.init(aCx, jsString))) {
      return;
    }

    if (i != 0) {
      message.AppendLiteral(" ");
    }

    message.Append(string);
  }

  message.AppendLiteral("\n");
  ExecuteDumpFunction(message);
}

void
Console::MaybeExecuteDumpFunctionForTrace(JSContext* aCx, nsIStackFrame* aStack)
{
  if (!aStack || (!mDumpFunction && !mDumpToStdout)) {
    return;
  }

  nsAutoString message;
  message.AssignLiteral("console.trace:\n");

  if (!mDumpPrefix.IsEmpty()) {
    message.Append(mDumpPrefix);
    message.AppendLiteral(": ");
  }

  nsCOMPtr<nsIStackFrame> stack(aStack);

  while (stack) {
    nsAutoString filename;
    nsresult rv = stack->GetFilename(aCx, filename);
    NS_ENSURE_SUCCESS_VOID(rv);

    message.Append(filename);
    message.AppendLiteral(" ");

    int32_t lineNumber;
    rv = stack->GetLineNumber(aCx, &lineNumber);
    NS_ENSURE_SUCCESS_VOID(rv);

    message.AppendInt(lineNumber);
    message.AppendLiteral(" ");

    nsAutoString functionName;
    rv = stack->GetName(aCx, functionName);
    NS_ENSURE_SUCCESS_VOID(rv);

    message.Append(filename);
    message.AppendLiteral("\n");

    nsCOMPtr<nsIStackFrame> caller;
    rv = stack->GetCaller(aCx, getter_AddRefs(caller));
    NS_ENSURE_SUCCESS_VOID(rv);

    if (!caller) {
      rv = stack->GetAsyncCaller(aCx, getter_AddRefs(caller));
      NS_ENSURE_SUCCESS_VOID(rv);
    }

    stack.swap(caller);
  }

  message.AppendLiteral("\n");
  ExecuteDumpFunction(message);
}

void
Console::ExecuteDumpFunction(const nsAString& aMessage)
{
  if (mDumpFunction) {
    IgnoredErrorResult rv;
    mDumpFunction->Call(aMessage, rv);
    return;
  }

  NS_ConvertUTF16toUTF8 str(aMessage);
  MOZ_LOG(nsContentUtils::DOMDumpLog(), LogLevel::Debug, ("%s", str.get()));
#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s", str.get());
#endif
  fputs(str.get(), stdout);
  fflush(stdout);
}

} // namespace dom
} // namespace mozilla
