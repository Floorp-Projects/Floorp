/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "nsJSEnvironment.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsPIDOMWindow.h"
#include "nsDOMCID.h"
#include "nsIXPConnect.h"
#include "nsCOMPtr.h"
#include "nsISupportsPrimitives.h"
#include "nsReadableUtils.h"
#include "nsDOMJSUtils.h"
#include "nsJSUtils.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsPresContext.h"
#include "nsIConsoleService.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserverService.h"
#include "nsITimer.h"
#include "nsAtom.h"
#include "nsContentUtils.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/HoldDropJSObjects.h"
#include "nsIContent.h"
#include "nsCycleCollector.h"
#include "nsXPCOMCIDInternal.h"
#include "nsServiceManagerUtils.h"
#include "nsTextFormatter.h"
#ifdef XP_WIN
#  include <process.h>
#  define getpid _getpid
#else
#  include <unistd.h>  // for getpid()
#endif
#include "xpcpublic.h"

#include "jsapi.h"
#include "js/Array.h"  // JS::NewArrayObject
#include "js/PropertySpec.h"
#include "js/SliceBudget.h"
#include "js/Wrapper.h"
#include "nsIArray.h"
#include "CCGCScheduler.h"
#include "WrapperFactory.h"
#include "nsGlobalWindow.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/PresShell.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticPrefs_javascript.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/FetchUtil.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/SerializedStackHolder.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "nsRefreshDriver.h"
#include "nsJSPrincipals.h"
#include "AccessCheck.h"
#include "mozilla/Logging.h"
#include "prthread.h"

#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "nsCycleCollectionNoteRootCallback.h"
#include "mozilla/IdleTaskRunner.h"
#include "nsViewManager.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"

using namespace mozilla;
using namespace mozilla::dom;

// Thank you Microsoft!
#ifdef CompareString
#  undef CompareString
#endif

static nsITimer* sShrinkingGCTimer;
static StaticRefPtr<IdleTaskRunner> sCCRunner;
static nsITimer* sFullGCTimer;
static StaticRefPtr<IdleTaskRunner> sGCRunner;

static JS::GCSliceCallback sPrevGCSliceCallback;

static bool sIncrementalCC = false;

static bool sIsInitialized;
static bool sShuttingDown;

// nsJSEnvironmentObserver observes the user-interaction-inactive notifications
// and triggers a shrinking a garbage collection if the user is still inactive
// after NS_SHRINKING_GC_DELAY ms later, if the appropriate pref is set.

static bool sIsCompactingOnUserInactive = false;

static TimeDuration sGCUnnotifiedTotalTime;

static CCGCScheduler sScheduler;

inline TimeStamp mozilla::CCGCScheduler::Now() { return TimeStamp::Now(); }

inline uint32_t mozilla::CCGCScheduler::SuspectedCCObjects() {
  return nsCycleCollector_suspectedCount();
}

struct CycleCollectorStats {
  constexpr CycleCollectorStats() = default;
  void Init();
  void Clear();
  void AfterPrepareForCycleCollectionSlice(TimeStamp aDeadline,
                                           TimeStamp aBeginTime,
                                           TimeStamp aMaybeAfterGCTime);
  void AfterCycleCollectionSlice();
  void AfterSyncForgetSkippable(TimeStamp beginTime);
  void AfterForgetSkippable(TimeDuration duration, uint32_t aRemovedPurples);
  void AfterCycleCollection();

  void SendTelemetry(TimeDuration aCCNowDuration) const;
  void MaybeLogStats(const CycleCollectorResults& aResults,
                     uint32_t aCleanups) const;
  void MaybeNotifyStats(const CycleCollectorResults& aResults,
                        TimeDuration aCCNowDuration, uint32_t aCleanups) const;

  // Time the current slice began, including any GC finishing.
  TimeStamp mBeginSliceTime;

  // Time the previous slice of the current CC ended.
  TimeStamp mEndSliceTime;

  // Time the current cycle collection began.
  TimeStamp mBeginTime;

  // The longest GC finishing duration for any slice of the current CC.
  TimeDuration mMaxGCDuration;

  // True if we ran sync forget skippable in any slice of the current CC.
  bool mRanSyncForgetSkippable = false;

  // Number of suspected objects at the start of the current CC.
  uint32_t mSuspected = 0;

  // The longest duration spent on sync forget skippable in any slice of the
  // current CC.
  TimeDuration mMaxSkippableDuration;

  // The longest pause of any slice in the current CC.
  TimeDuration mMaxSliceTime;

  // The longest slice time since ClearMaxCCSliceTime() was called.
  TimeDuration mMaxSliceTimeSinceClear;

  // The total amount of time spent actually running the current CC.
  TimeDuration mTotalSliceTime;

  // True if we were locked out by the GC in any slice of the current CC.
  bool mAnyLockedOut = false;

  // A file to dump CC activity to; set by MOZ_CCTIMER environment variable.
  FILE* mFile = nullptr;

  // In case CC slice was triggered during idle time, set to the end of the idle
  // period.
  TimeStamp mIdleDeadline;

  TimeDuration mMinForgetSkippableTime;
  TimeDuration mMaxForgetSkippableTime;
  TimeDuration mTotalForgetSkippableTime;
  uint32_t mForgetSkippableBeforeCC = 0;

  uint32_t mRemovedPurples = 0;
};

static CycleCollectorStats sCCStats;

static const char* ProcessNameForCollectorLog() {
  return XRE_GetProcessType() == GeckoProcessType_Default ? "default"
                                                          : "content";
}

namespace xpc {

// This handles JS Exceptions (via ExceptionStackOrNull), DOM and XPC
// Exceptions, and arbitrary values that were associated with a stack by the
// JS engine when they were thrown, as specified by exceptionStack.
//
// Note that the returned stackObj and stackGlobal are _not_ wrapped into the
// compartment of exceptionValue.
void FindExceptionStackForConsoleReport(nsPIDOMWindowInner* win,
                                        JS::HandleValue exceptionValue,
                                        JS::HandleObject exceptionStack,
                                        JS::MutableHandleObject stackObj,
                                        JS::MutableHandleObject stackGlobal) {
  stackObj.set(nullptr);
  stackGlobal.set(nullptr);

  if (!exceptionValue.isObject()) {
    // Use the stack provided by the JS engine, if available. This will not be
    // a wrapper.
    if (exceptionStack) {
      stackObj.set(exceptionStack);
      stackGlobal.set(JS::GetNonCCWObjectGlobal(exceptionStack));
    }
    return;
  }

  if (win && win->AsGlobal()->IsDying()) {
    // Pretend like we have no stack, so we don't end up keeping the global
    // alive via the stack.
    return;
  }

  JS::RootingContext* rcx = RootingCx();
  JS::RootedObject exceptionObject(rcx, &exceptionValue.toObject());
  if (JSObject* excStack = JS::ExceptionStackOrNull(exceptionObject)) {
    // At this point we know exceptionObject is a possibly-wrapped
    // js::ErrorObject that has excStack as stack. excStack might also be a CCW,
    // but excStack must be same-compartment with the unwrapped ErrorObject.
    // Return the ErrorObject's global as stackGlobal. This matches what we do
    // in the ErrorObject's |.stack| getter and ensures stackObj and stackGlobal
    // are same-compartment.
    JSObject* unwrappedException = js::UncheckedUnwrap(exceptionObject);
    stackObj.set(excStack);
    stackGlobal.set(JS::GetNonCCWObjectGlobal(unwrappedException));
    return;
  }

  // It is not a JS Exception, try DOM Exception.
  RefPtr<Exception> exception;
  UNWRAP_OBJECT(DOMException, exceptionObject, exception);
  if (!exception) {
    // Not a DOM Exception, try XPC Exception.
    UNWRAP_OBJECT(Exception, exceptionObject, exception);
    if (!exception) {
      // As above, use the stack provided by the JS engine, if available.
      if (exceptionStack) {
        stackObj.set(exceptionStack);
        stackGlobal.set(JS::GetNonCCWObjectGlobal(exceptionStack));
      }
      return;
    }
  }

  nsCOMPtr<nsIStackFrame> stack = exception->GetLocation();
  if (!stack) {
    return;
  }
  JS::RootedValue value(rcx);
  stack->GetNativeSavedFrame(&value);
  if (value.isObject()) {
    stackObj.set(&value.toObject());
    MOZ_ASSERT(JS::IsUnwrappedSavedFrame(stackObj));
    stackGlobal.set(JS::GetNonCCWObjectGlobal(stackObj));
    return;
  }
}

} /* namespace xpc */

static TimeDuration GetCollectionTimeDelta() {
  static TimeStamp sFirstCollectionTime;
  TimeStamp now = TimeStamp::Now();
  if (sFirstCollectionTime) {
    return now - sFirstCollectionTime;
  }
  sFirstCollectionTime = now;
  return TimeDuration();
}

static void KillTimers() {
  nsJSContext::KillGCRunner();
  nsJSContext::KillShrinkingGCTimer();
  nsJSContext::KillCCRunner();
  nsJSContext::KillFullGCTimer();
}

class nsJSEnvironmentObserver final : public nsIObserver {
  ~nsJSEnvironmentObserver() = default;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS(nsJSEnvironmentObserver, nsIObserver)

NS_IMETHODIMP
nsJSEnvironmentObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                 const char16_t* aData) {
  if (!nsCRT::strcmp(aTopic, "memory-pressure")) {
    if (StaticPrefs::javascript_options_gc_on_memory_pressure()) {
      if (sShuttingDown) {
        // Don't GC/CC if we're already shutting down.
        return NS_OK;
      }
      nsDependentString data(aData);
      if (data.EqualsLiteral("low-memory-ongoing")) {
        // Don't GC/CC if we are in an ongoing low-memory state since its very
        // slow and it likely won't help us anyway.
        return NS_OK;
      }
      if (data.EqualsLiteral("low-memory")) {
        nsJSContext::SetLowMemoryState(true);
      }
      nsJSContext::GarbageCollectNow(JS::GCReason::MEM_PRESSURE,
                                     nsJSContext::NonIncrementalGC,
                                     nsJSContext::ShrinkingGC);
      nsJSContext::CycleCollectNow();
      if (sScheduler.NeedsGCAfterCC()) {
        nsJSContext::GarbageCollectNow(JS::GCReason::MEM_PRESSURE,
                                       nsJSContext::NonIncrementalGC,
                                       nsJSContext::ShrinkingGC);
      }
    }
  } else if (!nsCRT::strcmp(aTopic, "memory-pressure-stop")) {
    nsJSContext::SetLowMemoryState(false);
  } else if (!nsCRT::strcmp(aTopic, "user-interaction-inactive")) {
    if (StaticPrefs::javascript_options_compact_on_user_inactive()) {
      nsJSContext::PokeShrinkingGC();
    }
  } else if (!nsCRT::strcmp(aTopic, "user-interaction-active")) {
    nsJSContext::KillShrinkingGCTimer();
    if (sIsCompactingOnUserInactive) {
      AutoJSAPI jsapi;
      jsapi.Init();
      JS::AbortIncrementalGC(jsapi.cx());
    }
    MOZ_ASSERT(!sIsCompactingOnUserInactive);
  } else if (!nsCRT::strcmp(aTopic, "quit-application") ||
             !nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) ||
             !nsCRT::strcmp(aTopic, "content-child-will-shutdown")) {
    sShuttingDown = true;
    KillTimers();
  }

  return NS_OK;
}

/****************************************************************
 ************************** AutoFree ****************************
 ****************************************************************/

class AutoFree {
 public:
  explicit AutoFree(void* aPtr) : mPtr(aPtr) {}
  ~AutoFree() {
    if (mPtr) free(mPtr);
  }
  void Invalidate() { mPtr = 0; }

 private:
  void* mPtr;
};

// A utility function for script languages to call.  Although it looks small,
// the use of nsIDocShell and nsPresContext triggers a huge number of
// dependencies that most languages would not otherwise need.
// XXXmarkh - This function is mis-placed!
bool NS_HandleScriptError(nsIScriptGlobalObject* aScriptGlobal,
                          const ErrorEventInit& aErrorEventInit,
                          nsEventStatus* aStatus) {
  bool called = false;
  nsCOMPtr<nsPIDOMWindowInner> win(do_QueryInterface(aScriptGlobal));
  nsIDocShell* docShell = win ? win->GetDocShell() : nullptr;
  if (docShell) {
    RefPtr<nsPresContext> presContext = docShell->GetPresContext();

    static int32_t errorDepth;  // Recursion prevention
    ++errorDepth;

    if (errorDepth < 2) {
      // Dispatch() must be synchronous for the recursion block
      // (errorDepth) to work.
      RefPtr<ErrorEvent> event = ErrorEvent::Constructor(
          nsGlobalWindowInner::Cast(win), u"error"_ns, aErrorEventInit);
      event->SetTrusted(true);

      EventDispatcher::DispatchDOMEvent(win, nullptr, event, presContext,
                                        aStatus);
      called = true;
    }
    --errorDepth;
  }
  return called;
}

class ScriptErrorEvent : public Runnable {
 public:
  ScriptErrorEvent(nsPIDOMWindowInner* aWindow, JS::RootingContext* aRootingCx,
                   xpc::ErrorReport* aReport, JS::Handle<JS::Value> aError,
                   JS::Handle<JSObject*> aErrorStack)
      : mozilla::Runnable("ScriptErrorEvent"),
        mWindow(aWindow),
        mReport(aReport),
        mError(aRootingCx, aError),
        mErrorStack(aRootingCx, aErrorStack) {}

  NS_IMETHOD Run() override {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsPIDOMWindowInner* win = mWindow;
    MOZ_ASSERT(win);
    MOZ_ASSERT(NS_IsMainThread());
    // First, notify the DOM that we have a script error, but only if
    // our window is still the current inner.
    JS::RootingContext* rootingCx = RootingCx();
    if (win->IsCurrentInnerWindow() && win->GetDocShell() &&
        !sHandlingScriptError) {
      AutoRestore<bool> recursionGuard(sHandlingScriptError);
      sHandlingScriptError = true;

      RefPtr<nsPresContext> presContext = win->GetDocShell()->GetPresContext();

      RootedDictionary<ErrorEventInit> init(rootingCx);
      init.mCancelable = true;
      init.mFilename = mReport->mFileName;
      init.mBubbles = true;

      constexpr auto xoriginMsg = u"Script error."_ns;
      if (!mReport->mIsMuted) {
        init.mMessage = mReport->mErrorMsg;
        init.mLineno = mReport->mLineNumber;
        init.mColno = mReport->mColumn;
        init.mError = mError;
      } else {
        NS_WARNING("Not same origin error!");
        init.mMessage = xoriginMsg;
        init.mLineno = 0;
      }

      RefPtr<ErrorEvent> event = ErrorEvent::Constructor(
          nsGlobalWindowInner::Cast(win), u"error"_ns, init);
      event->SetTrusted(true);

      EventDispatcher::DispatchDOMEvent(win, nullptr, event, presContext,
                                        &status);
    }

    if (status != nsEventStatus_eConsumeNoDefault) {
      JS::Rooted<JSObject*> stack(rootingCx);
      JS::Rooted<JSObject*> stackGlobal(rootingCx);
      xpc::FindExceptionStackForConsoleReport(win, mError, mErrorStack, &stack,
                                              &stackGlobal);
      JS::Rooted<Maybe<JS::Value>> exception(rootingCx, Some(mError));
      nsGlobalWindowInner* inner = nsGlobalWindowInner::Cast(win);
      mReport->LogToConsoleWithStack(inner, exception, stack, stackGlobal);
    }

    return NS_OK;
  }

 private:
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<xpc::ErrorReport> mReport;
  JS::PersistentRootedValue mError;
  JS::PersistentRootedObject mErrorStack;

  static bool sHandlingScriptError;
};

bool ScriptErrorEvent::sHandlingScriptError = false;

// This temporarily lives here to avoid code churn. It will go away entirely
// soon.
namespace xpc {

void DispatchScriptErrorEvent(nsPIDOMWindowInner* win,
                              JS::RootingContext* rootingCx,
                              xpc::ErrorReport* xpcReport,
                              JS::Handle<JS::Value> exception,
                              JS::Handle<JSObject*> exceptionStack) {
  nsContentUtils::AddScriptRunner(new ScriptErrorEvent(
      win, rootingCx, xpcReport, exception, exceptionStack));
}

} /* namespace xpc */

#ifdef DEBUG
// A couple of useful functions to call when you're debugging.
nsGlobalWindowInner* JSObject2Win(JSObject* obj) {
  return xpc::WindowOrNull(obj);
}

template <typename T>
void PrintWinURI(T* win) {
  if (!win) {
    printf("No window passed in.\n");
    return;
  }

  nsCOMPtr<Document> doc = win->GetExtantDoc();
  if (!doc) {
    printf("No document in the window.\n");
    return;
  }

  nsIURI* uri = doc->GetDocumentURI();
  if (!uri) {
    printf("Document doesn't have a URI.\n");
    return;
  }

  printf("%s\n", uri->GetSpecOrDefault().get());
}

void PrintWinURIInner(nsGlobalWindowInner* aWin) { return PrintWinURI(aWin); }

void PrintWinURIOuter(nsGlobalWindowOuter* aWin) { return PrintWinURI(aWin); }

template <typename T>
void PrintWinCodebase(T* win) {
  if (!win) {
    printf("No window passed in.\n");
    return;
  }

  nsIPrincipal* prin = win->GetPrincipal();
  if (!prin) {
    printf("Window doesn't have principals.\n");
    return;
  }
  if (prin->IsSystemPrincipal()) {
    printf("No URI, it's the system principal.\n");
    return;
  }
  nsCString spec;
  prin->GetAsciiSpec(spec);
  printf("%s\n", spec.get());
}

void PrintWinCodebaseInner(nsGlobalWindowInner* aWin) {
  return PrintWinCodebase(aWin);
}

void PrintWinCodebaseOuter(nsGlobalWindowOuter* aWin) {
  return PrintWinCodebase(aWin);
}

void DumpString(const nsAString& str) {
  printf("%s\n", NS_ConvertUTF16toUTF8(str).get());
}
#endif

nsJSContext::nsJSContext(bool aGCOnDestruction,
                         nsIScriptGlobalObject* aGlobalObject)
    : mWindowProxy(nullptr),
      mGCOnDestruction(aGCOnDestruction),
      mGlobalObjectRef(aGlobalObject) {
  EnsureStatics();

  mProcessingScriptTag = false;
  HoldJSObjects(this);
}

nsJSContext::~nsJSContext() {
  mGlobalObjectRef = nullptr;

  Destroy();
}

void nsJSContext::Destroy() {
  if (mGCOnDestruction) {
    PokeGC(JS::GCReason::NSJSCONTEXT_DESTROY, mWindowProxy);
  }

  DropJSObjects(this);
}

// QueryInterface implementation for nsJSContext
NS_IMPL_CYCLE_COLLECTION_CLASS(nsJSContext)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSContext)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mWindowProxy)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSContext)
  tmp->mGCOnDestruction = false;
  tmp->mWindowProxy = nullptr;
  tmp->Destroy();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobalObjectRef)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsJSContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobalObjectRef)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsJSContext)
  NS_INTERFACE_MAP_ENTRY(nsIScriptContext)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsJSContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsJSContext)

#ifdef DEBUG
bool AtomIsEventHandlerName(nsAtom* aName) {
  const char16_t* name = aName->GetUTF16String();

  const char16_t* cp;
  char16_t c;
  for (cp = name; *cp != '\0'; ++cp) {
    c = *cp;
    if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z')) return false;
  }

  return true;
}
#endif

nsIScriptGlobalObject* nsJSContext::GetGlobalObject() {
  // Note: this could probably be simplified somewhat more; see bug 974327
  // comments 1 and 3.
  if (!mWindowProxy) {
    return nullptr;
  }

  MOZ_ASSERT(mGlobalObjectRef);
  return mGlobalObjectRef;
}

nsresult nsJSContext::SetProperty(JS::Handle<JSObject*> aTarget,
                                  const char* aPropName, nsISupports* aArgs) {
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(GetGlobalObject()))) {
    return NS_ERROR_FAILURE;
  }
  JSContext* cx = jsapi.cx();

  JS::RootedVector<JS::Value> args(cx);

  JS::Rooted<JSObject*> global(cx, GetWindowProxy());
  nsresult rv = ConvertSupportsTojsvals(cx, aArgs, global, &args);
  NS_ENSURE_SUCCESS(rv, rv);

  // got the arguments, now attach them.

  for (uint32_t i = 0; i < args.length(); ++i) {
    if (!JS_WrapValue(cx, args[i])) {
      return NS_ERROR_FAILURE;
    }
  }

  JS::Rooted<JSObject*> array(cx, JS::NewArrayObject(cx, args));
  if (!array) {
    return NS_ERROR_FAILURE;
  }

  return JS_DefineProperty(cx, aTarget, aPropName, array, 0) ? NS_OK
                                                             : NS_ERROR_FAILURE;
}

nsresult nsJSContext::ConvertSupportsTojsvals(
    JSContext* aCx, nsISupports* aArgs, JS::Handle<JSObject*> aScope,
    JS::MutableHandleVector<JS::Value> aArgsOut) {
  nsresult rv = NS_OK;

  // If the array implements nsIJSArgArray, copy the contents and return.
  nsCOMPtr<nsIJSArgArray> fastArray = do_QueryInterface(aArgs);
  if (fastArray) {
    uint32_t argc;
    JS::Value* argv;
    rv = fastArray->GetArgs(&argc, reinterpret_cast<void**>(&argv));
    if (NS_SUCCEEDED(rv) && !aArgsOut.append(argv, argc)) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
    return rv;
  }

  // Take the slower path converting each item.
  // Handle only nsIArray and nsIVariant.  nsIArray is only needed for
  // SetProperty('arguments', ...);

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  NS_ENSURE_TRUE(xpc, NS_ERROR_UNEXPECTED);

  if (!aArgs) return NS_OK;
  uint32_t argCount;
  // This general purpose function may need to convert an arg array
  // (window.arguments, event-handler args) and a generic property.
  nsCOMPtr<nsIArray> argsArray(do_QueryInterface(aArgs));

  if (argsArray) {
    rv = argsArray->GetLength(&argCount);
    NS_ENSURE_SUCCESS(rv, rv);
    if (argCount == 0) return NS_OK;
  } else {
    argCount = 1;  // the nsISupports which is not an array
  }

  // Use the caller's auto guards to release and unroot.
  if (!aArgsOut.resize(argCount)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (argsArray) {
    for (uint32_t argCtr = 0; argCtr < argCount && NS_SUCCEEDED(rv); argCtr++) {
      nsCOMPtr<nsISupports> arg;
      JS::MutableHandle<JS::Value> thisVal = aArgsOut[argCtr];
      argsArray->QueryElementAt(argCtr, NS_GET_IID(nsISupports),
                                getter_AddRefs(arg));
      if (!arg) {
        thisVal.setNull();
        continue;
      }
      nsCOMPtr<nsIVariant> variant(do_QueryInterface(arg));
      if (variant != nullptr) {
        rv = xpc->VariantToJS(aCx, aScope, variant, thisVal);
      } else {
        // And finally, support the nsISupportsPrimitives supplied
        // by the AppShell.  It generally will pass only strings, but
        // as we have code for handling all, we may as well use it.
        rv = AddSupportsPrimitiveTojsvals(aCx, arg, thisVal.address());
        if (rv == NS_ERROR_NO_INTERFACE) {
          // something else - probably an event object or similar -
          // just wrap it.
#ifdef DEBUG
          // but first, check its not another nsISupportsPrimitive, as
          // these are now deprecated for use with script contexts.
          nsCOMPtr<nsISupportsPrimitive> prim(do_QueryInterface(arg));
          NS_ASSERTION(prim == nullptr,
                       "Don't pass nsISupportsPrimitives - use nsIVariant!");
#endif
          JSAutoRealm ar(aCx, aScope);
          rv = nsContentUtils::WrapNative(aCx, arg, thisVal);
        }
      }
    }
  } else {
    nsCOMPtr<nsIVariant> variant = do_QueryInterface(aArgs);
    if (variant) {
      rv = xpc->VariantToJS(aCx, aScope, variant, aArgsOut[0]);
    } else {
      NS_ERROR("Not an array, not an interface?");
      rv = NS_ERROR_UNEXPECTED;
    }
  }
  return rv;
}

// This really should go into xpconnect somewhere...
nsresult nsJSContext::AddSupportsPrimitiveTojsvals(JSContext* aCx,
                                                   nsISupports* aArg,
                                                   JS::Value* aArgv) {
  MOZ_ASSERT(aArg, "Empty arg");

  nsCOMPtr<nsISupportsPrimitive> argPrimitive(do_QueryInterface(aArg));
  if (!argPrimitive) return NS_ERROR_NO_INTERFACE;

  uint16_t type;
  argPrimitive->GetType(&type);

  switch (type) {
    case nsISupportsPrimitive::TYPE_CSTRING: {
      nsCOMPtr<nsISupportsCString> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      nsAutoCString data;

      p->GetData(data);

      JSString* str = ::JS_NewStringCopyN(aCx, data.get(), data.Length());
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      aArgv->setString(str);

      break;
    }
    case nsISupportsPrimitive::TYPE_STRING: {
      nsCOMPtr<nsISupportsString> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      nsAutoString data;

      p->GetData(data);

      // cast is probably safe since wchar_t and char16_t are expected
      // to be equivalent; both unsigned 16-bit entities
      JSString* str = ::JS_NewUCStringCopyN(aCx, data.get(), data.Length());
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      aArgv->setString(str);
      break;
    }
    case nsISupportsPrimitive::TYPE_PRBOOL: {
      nsCOMPtr<nsISupportsPRBool> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      bool data;

      p->GetData(&data);

      aArgv->setBoolean(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT8: {
      nsCOMPtr<nsISupportsPRUint8> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      uint8_t data;

      p->GetData(&data);

      aArgv->setInt32(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT16: {
      nsCOMPtr<nsISupportsPRUint16> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      uint16_t data;

      p->GetData(&data);

      aArgv->setInt32(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT32: {
      nsCOMPtr<nsISupportsPRUint32> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      uint32_t data;

      p->GetData(&data);

      aArgv->setInt32(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_CHAR: {
      nsCOMPtr<nsISupportsChar> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      char data;

      p->GetData(&data);

      JSString* str = ::JS_NewStringCopyN(aCx, &data, 1);
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      aArgv->setString(str);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRINT16: {
      nsCOMPtr<nsISupportsPRInt16> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      int16_t data;

      p->GetData(&data);

      aArgv->setInt32(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRINT32: {
      nsCOMPtr<nsISupportsPRInt32> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      int32_t data;

      p->GetData(&data);

      aArgv->setInt32(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_FLOAT: {
      nsCOMPtr<nsISupportsFloat> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      float data;

      p->GetData(&data);

      *aArgv = ::JS_NumberValue(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_DOUBLE: {
      nsCOMPtr<nsISupportsDouble> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      double data;

      p->GetData(&data);

      *aArgv = ::JS_NumberValue(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_INTERFACE_POINTER: {
      nsCOMPtr<nsISupportsInterfacePointer> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      nsCOMPtr<nsISupports> data;
      nsIID* iid = nullptr;

      p->GetData(getter_AddRefs(data));
      p->GetDataIID(&iid);
      NS_ENSURE_TRUE(iid, NS_ERROR_UNEXPECTED);

      AutoFree iidGuard(iid);  // Free iid upon destruction.

      JS::Rooted<JSObject*> scope(aCx, GetWindowProxy());
      JS::Rooted<JS::Value> v(aCx);
      JSAutoRealm ar(aCx, scope);
      nsresult rv = nsContentUtils::WrapNative(aCx, data, iid, &v);
      NS_ENSURE_SUCCESS(rv, rv);

      *aArgv = v;

      break;
    }
    case nsISupportsPrimitive::TYPE_ID:
    case nsISupportsPrimitive::TYPE_PRUINT64:
    case nsISupportsPrimitive::TYPE_PRINT64:
    case nsISupportsPrimitive::TYPE_PRTIME: {
      NS_WARNING("Unsupported primitive type used");
      aArgv->setNull();
      break;
    }
    default: {
      NS_WARNING("Unknown primitive type used");
      aArgv->setNull();
      break;
    }
  }
  return NS_OK;
}

#ifdef MOZ_JPROF

#  include <signal.h>

inline bool IsJProfAction(struct sigaction* action) {
  return (action->sa_sigaction &&
          (action->sa_flags & (SA_RESTART | SA_SIGINFO)) ==
              (SA_RESTART | SA_SIGINFO));
}

void NS_JProfStartProfiling();
void NS_JProfStopProfiling();
void NS_JProfClearCircular();

static bool JProfStartProfilingJS(JSContext* cx, unsigned argc, JS::Value* vp) {
  NS_JProfStartProfiling();
  return true;
}

void NS_JProfStartProfiling() {
  // Figure out whether we're dealing with SIGPROF, SIGALRM, or
  // SIGPOLL profiling (SIGALRM for JP_REALTIME, SIGPOLL for
  // JP_RTC_HZ)
  struct sigaction action;

  // Must check ALRM before PROF since both are enabled for real-time
  sigaction(SIGALRM, nullptr, &action);
  // printf("SIGALRM: %p, flags = %x\n",action.sa_sigaction,action.sa_flags);
  if (IsJProfAction(&action)) {
    // printf("Beginning real-time jprof profiling.\n");
    raise(SIGALRM);
    return;
  }

  sigaction(SIGPROF, nullptr, &action);
  // printf("SIGPROF: %p, flags = %x\n",action.sa_sigaction,action.sa_flags);
  if (IsJProfAction(&action)) {
    // printf("Beginning process-time jprof profiling.\n");
    raise(SIGPROF);
    return;
  }

  sigaction(SIGPOLL, nullptr, &action);
  // printf("SIGPOLL: %p, flags = %x\n",action.sa_sigaction,action.sa_flags);
  if (IsJProfAction(&action)) {
    // printf("Beginning rtc-based jprof profiling.\n");
    raise(SIGPOLL);
    return;
  }

  printf("Could not start jprof-profiling since JPROF_FLAGS was not set.\n");
}

static bool JProfStopProfilingJS(JSContext* cx, unsigned argc, JS::Value* vp) {
  NS_JProfStopProfiling();
  return true;
}

void NS_JProfStopProfiling() {
  raise(SIGUSR1);
  // printf("Stopped jprof profiling.\n");
}

static bool JProfClearCircularJS(JSContext* cx, unsigned argc, JS::Value* vp) {
  NS_JProfClearCircular();
  return true;
}

void NS_JProfClearCircular() {
  raise(SIGUSR2);
  // printf("cleared jprof buffer\n");
}

static bool JProfSaveCircularJS(JSContext* cx, unsigned argc, JS::Value* vp) {
  // Not ideal...
  NS_JProfStopProfiling();
  NS_JProfStartProfiling();
  return true;
}

static const JSFunctionSpec JProfFunctions[] = {
    JS_FN("JProfStartProfiling", JProfStartProfilingJS, 0, 0),
    JS_FN("JProfStopProfiling", JProfStopProfilingJS, 0, 0),
    JS_FN("JProfClearCircular", JProfClearCircularJS, 0, 0),
    JS_FN("JProfSaveCircular", JProfSaveCircularJS, 0, 0), JS_FS_END};

#endif /* defined(MOZ_JPROF) */

nsresult nsJSContext::InitClasses(JS::Handle<JSObject*> aGlobalObj) {
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JSAutoRealm ar(cx, aGlobalObj);

#ifdef MOZ_JPROF
  // Attempt to initialize JProf functions
  ::JS_DefineFunctions(cx, aGlobalObj, JProfFunctions);
#endif

  return NS_OK;
}

bool nsJSContext::GetProcessingScriptTag() { return mProcessingScriptTag; }

void nsJSContext::SetProcessingScriptTag(bool aFlag) {
  mProcessingScriptTag = aFlag;
}

void FullGCTimerFired(nsITimer* aTimer, void* aClosure) {
  nsJSContext::KillFullGCTimer();
  MOZ_ASSERT(!aClosure, "Don't pass a closure to FullGCTimerFired");
  nsJSContext::GarbageCollectNow(JS::GCReason::FULL_GC_TIMER,
                                 nsJSContext::IncrementalGC);
}

// static
void nsJSContext::SetLowMemoryState(bool aState) {
  JSContext* cx = danger::GetJSContext();
  JS::SetLowMemoryState(cx, aState);
}

// static
void nsJSContext::GarbageCollectNow(JS::GCReason aReason,
                                    IsIncremental aIncremental,
                                    IsShrinking aShrinking,
                                    int64_t aSliceMillis) {
  AUTO_PROFILER_LABEL_DYNAMIC_CSTR_NONSENSITIVE(
      "nsJSContext::GarbageCollectNow", GCCC, JS::ExplainGCReason(aReason));

  MOZ_ASSERT_IF(aSliceMillis, aIncremental == IncrementalGC);

  // We use danger::GetJSContext() since AutoJSAPI will assert if the current
  // thread's context is null (such as during shutdown).
  JSContext* cx = danger::GetJSContext();

  if (!nsContentUtils::XPConnect() || !cx) {
    return;
  }

  if (sScheduler.InIncrementalGC() && aIncremental == IncrementalGC) {
    // We're in the middle of incremental GC. Do another slice.
    JS::PrepareForIncrementalGC(cx);
    JS::IncrementalGCSlice(cx, aReason, aSliceMillis);
    return;
  }

  JSGCInvocationKind gckind = aShrinking == ShrinkingGC ? GC_SHRINK : GC_NORMAL;

  if (aIncremental == NonIncrementalGC ||
      aReason == JS::GCReason::FULL_GC_TIMER) {
    sScheduler.SetNeedsFullGC();
  }

  if (sScheduler.NeedsFullGC()) {
    JS::PrepareForFullGC(cx);
  }

  if (aIncremental == IncrementalGC) {
    // Incremental GC slices will be triggered by the sGCRunner. If one doesn't
    // already exist, create it in the GC_SLICE_END callback for the first
    // slice being executed here.
    JS::StartIncrementalGC(cx, gckind, aReason, aSliceMillis);
  } else {
    JS::NonIncrementalGC(cx, gckind, aReason);
  }
}

static void FinishAnyIncrementalGC() {
  AUTO_PROFILER_LABEL("FinishAnyIncrementalGC", GCCC);

  if (sScheduler.InIncrementalGC()) {
    AutoJSAPI jsapi;
    jsapi.Init();

    // We're in the middle of an incremental GC, so finish it.
    JS::PrepareForIncrementalGC(jsapi.cx());
    JS::FinishIncrementalGC(jsapi.cx(), JS::GCReason::CC_FORCED);
  }
}

static void FireForgetSkippable(bool aRemoveChildless, TimeStamp aDeadline) {
  AUTO_PROFILER_TRACING_MARKER(
      "CC", aDeadline.IsNull() ? "ForgetSkippable" : "IdleForgetSkippable",
      GCCC);
  TimeStamp startTimeStamp = TimeStamp::Now();
  FinishAnyIncrementalGC();

  uint32_t suspectedBefore = nsCycleCollector_suspectedCount();
  js::SliceBudget budget =
      sScheduler.ComputeForgetSkippableBudget(startTimeStamp, aDeadline);
  bool earlyForgetSkippable = sScheduler.IsEarlyForgetSkippable();
  nsCycleCollector_forgetSkippable(budget, aRemoveChildless,
                                   earlyForgetSkippable);
  TimeStamp now = TimeStamp::Now();
  uint32_t removedPurples =
      sScheduler.NoteForgetSkippableComplete(now, suspectedBefore);

  TimeDuration duration = now - startTimeStamp;

  sCCStats.AfterForgetSkippable(duration, removedPurples);

  if (duration.ToSeconds()) {
    TimeDuration idleDuration;
    if (!aDeadline.IsNull()) {
      if (aDeadline < now) {
        // This slice overflowed the idle period.
        if (aDeadline > startTimeStamp) {
          idleDuration = aDeadline - startTimeStamp;
        }
      } else {
        idleDuration = duration;
      }
    }

    uint32_t percent =
        uint32_t(idleDuration.ToSeconds() / duration.ToSeconds() * 100);
    Telemetry::Accumulate(Telemetry::FORGET_SKIPPABLE_DURING_IDLE, percent);
  }
}

MOZ_ALWAYS_INLINE
static TimeDuration TimeBetween(TimeStamp aStart, TimeStamp aEnd) {
  MOZ_ASSERT(aEnd >= aStart);
  return aEnd - aStart;
}

static TimeDuration TimeUntilNow(TimeStamp start) {
  if (start.IsNull()) {
    return TimeDuration();
  }
  return TimeBetween(start, TimeStamp::Now());
}

void CycleCollectorStats::Init() {
  Clear();

  char* env = getenv("MOZ_CCTIMER");
  if (!env) {
    return;
  }
  if (strcmp(env, "none") == 0) {
    mFile = nullptr;
  } else if (strcmp(env, "stdout") == 0) {
    mFile = stdout;
  } else if (strcmp(env, "stderr") == 0) {
    mFile = stderr;
  } else {
    mFile = fopen(env, "a");
    if (!mFile) {
      MOZ_CRASH("Failed to open MOZ_CCTIMER log file.");
    }
  }
}

void CycleCollectorStats::Clear() {
  if (mFile && mFile != stdout && mFile != stderr) {
    fclose(mFile);
  }
  *this = CycleCollectorStats();
}

void CycleCollectorStats::AfterCycleCollectionSlice() {
  if (mBeginSliceTime.IsNull()) {
    // We already called this method from EndCycleCollectionCallback for this
    // slice.
    return;
  }

  mEndSliceTime = TimeStamp::Now();
  TimeDuration duration = mEndSliceTime - mBeginSliceTime;

  if (duration.ToSeconds()) {
    TimeDuration idleDuration;
    if (!mIdleDeadline.IsNull()) {
      if (mIdleDeadline < mEndSliceTime) {
        // This slice overflowed the idle period.
        idleDuration = mIdleDeadline - mBeginSliceTime;
      } else {
        idleDuration = duration;
      }
    }

    uint32_t percent =
        uint32_t(idleDuration.ToSeconds() / duration.ToSeconds() * 100);
    Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_SLICE_DURING_IDLE,
                          percent);
  }

  TimeDuration sliceTime = TimeBetween(mBeginSliceTime, mEndSliceTime);
  mMaxSliceTime = std::max(mMaxSliceTime, sliceTime);
  mMaxSliceTimeSinceClear = std::max(mMaxSliceTimeSinceClear, sliceTime);
  mTotalSliceTime += sliceTime;
  mBeginSliceTime = TimeStamp();
}

void CycleCollectorStats::AfterPrepareForCycleCollectionSlice(
    TimeStamp aDeadline, TimeStamp aBeginTime, TimeStamp aMaybeAfterGCTime) {
  mBeginSliceTime = aBeginTime;
  mIdleDeadline = aDeadline;

  if (!aMaybeAfterGCTime.IsNull()) {
    mAnyLockedOut = true;
    mMaxGCDuration = std::max(mMaxGCDuration, aMaybeAfterGCTime - aBeginTime);
  }
}

void CycleCollectorStats::AfterSyncForgetSkippable(TimeStamp beginTime) {
  mMaxSkippableDuration =
      std::max(mMaxSkippableDuration, TimeUntilNow(beginTime));
  mRanSyncForgetSkippable = true;
}

void CycleCollectorStats::AfterForgetSkippable(TimeDuration duration,
                                               uint32_t aRemovedPurples) {
  if (!mMinForgetSkippableTime || mMinForgetSkippableTime > duration) {
    mMinForgetSkippableTime = duration;
  }
  if (!mMaxForgetSkippableTime || mMaxForgetSkippableTime < duration) {
    mMaxForgetSkippableTime = duration;
  }
  mTotalForgetSkippableTime += duration;
  ++mForgetSkippableBeforeCC;

  mRemovedPurples += aRemovedPurples;
}

void CycleCollectorStats::SendTelemetry(TimeDuration aCCNowDuration) const {
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_FINISH_IGC, mAnyLockedOut);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_SYNC_SKIPPABLE,
                        mRanSyncForgetSkippable);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_FULL,
                        aCCNowDuration.ToMilliseconds());
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_MAX_PAUSE,
                        mMaxSliceTime.ToMilliseconds());

  TimeStamp lastCCEndTime = sScheduler.GetLastCCEndTime();
  if (!lastCCEndTime.IsNull()) {
    TimeDuration timeBetween = TimeBetween(lastCCEndTime, mBeginTime);
    Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_TIME_BETWEEN,
                          timeBetween.ToSeconds());
  }

  Telemetry::Accumulate(Telemetry::FORGET_SKIPPABLE_MAX,
                        mMaxForgetSkippableTime.ToMilliseconds());
}

void CycleCollectorStats::MaybeLogStats(const CycleCollectorResults& aResults,
                                        uint32_t aCleanups) const {
  if (!StaticPrefs::javascript_options_mem_log() && !sCCStats.mFile) {
    return;
  }

  TimeDuration delta = GetCollectionTimeDelta();

  nsCString mergeMsg;
  if (aResults.mMergedZones) {
    mergeMsg.AssignLiteral(" merged");
  }

  nsCString gcMsg;
  if (aResults.mForcedGC) {
    gcMsg.AssignLiteral(", forced a GC");
  }

  const char16_t* kFmt =
      u"CC(T+%.1f)[%s-%i] max pause: %.fms, total time: %.fms, slices: %lu, "
      u"suspected: %lu, visited: %lu RCed and %lu%s GCed, collected: %lu "
      u"RCed and %lu GCed (%lu|%lu|%lu waiting for GC)%s\n"
      u"ForgetSkippable %lu times before CC, min: %.f ms, max: %.f ms, avg: "
      u"%.f ms, total: %.f ms, max sync: %.f ms, removed: %lu";
  nsString msg;
  nsTextFormatter::ssprintf(
      msg, kFmt, delta.ToMicroseconds() / PR_USEC_PER_SEC,
      ProcessNameForCollectorLog(), getpid(), mMaxSliceTime.ToMilliseconds(),
      mTotalSliceTime.ToMilliseconds(), aResults.mNumSlices, mSuspected,
      aResults.mVisitedRefCounted, aResults.mVisitedGCed, mergeMsg.get(),
      aResults.mFreedRefCounted, aResults.mFreedGCed,
      sScheduler.mCCollectedWaitingForGC,
      sScheduler.mCCollectedZonesWaitingForGC,
      sScheduler.mLikelyShortLivingObjectsNeedingGC, gcMsg.get(),
      mForgetSkippableBeforeCC, mMinForgetSkippableTime.ToMilliseconds(),
      mMaxForgetSkippableTime.ToMilliseconds(),
      mTotalForgetSkippableTime.ToMilliseconds() / aCleanups,
      mTotalForgetSkippableTime.ToMilliseconds(),
      mMaxSkippableDuration.ToMilliseconds(), mRemovedPurples);
  if (StaticPrefs::javascript_options_mem_log()) {
    nsCOMPtr<nsIConsoleService> cs =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (cs) {
      cs->LogStringMessage(msg.get());
    }
  }
  if (mFile) {
    fprintf(mFile, "%s\n", NS_ConvertUTF16toUTF8(msg).get());
  }
}

void CycleCollectorStats::MaybeNotifyStats(
    const CycleCollectorResults& aResults, TimeDuration aCCNowDuration,
    uint32_t aCleanups) const {
  if (!StaticPrefs::javascript_options_mem_notify()) {
    return;
  }

  const char16_t* kJSONFmt =
      u"{ \"timestamp\": %llu, "
      u"\"duration\": %.f, "
      u"\"max_slice_pause\": %.f, "
      u"\"total_slice_pause\": %.f, "
      u"\"max_finish_gc_duration\": %.f, "
      u"\"max_sync_skippable_duration\": %.f, "
      u"\"suspected\": %lu, "
      u"\"visited\": { "
      u"\"RCed\": %lu, "
      u"\"GCed\": %lu }, "
      u"\"collected\": { "
      u"\"RCed\": %lu, "
      u"\"GCed\": %lu }, "
      u"\"waiting_for_gc\": %lu, "
      u"\"zones_waiting_for_gc\": %lu, "
      u"\"short_living_objects_waiting_for_gc\": %lu, "
      u"\"forced_gc\": %d, "
      u"\"forget_skippable\": { "
      u"\"times_before_cc\": %lu, "
      u"\"min\": %.f, "
      u"\"max\": %.f, "
      u"\"avg\": %.f, "
      u"\"total\": %.f, "
      u"\"removed\": %lu } "
      u"}";

  nsString json;
  nsTextFormatter::ssprintf(
      json, kJSONFmt, PR_Now(), aCCNowDuration.ToMilliseconds(),
      mMaxSliceTime.ToMilliseconds(), mTotalSliceTime.ToMilliseconds(),
      mMaxGCDuration.ToMilliseconds(), mMaxSkippableDuration.ToMilliseconds(),
      mSuspected, aResults.mVisitedRefCounted, aResults.mVisitedGCed,
      aResults.mFreedRefCounted, aResults.mFreedGCed,
      sScheduler.mCCollectedWaitingForGC,
      sScheduler.mCCollectedZonesWaitingForGC,
      sScheduler.mLikelyShortLivingObjectsNeedingGC, aResults.mForcedGC,
      mForgetSkippableBeforeCC, mMinForgetSkippableTime.ToMilliseconds(),
      mMaxForgetSkippableTime.ToMilliseconds(),
      mTotalForgetSkippableTime.ToMilliseconds() / aCleanups,
      mTotalForgetSkippableTime.ToMilliseconds(), mRemovedPurples);
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(nullptr, "cycle-collection-statistics",
                                     json.get());
  }
}

// static
void nsJSContext::CycleCollectNow(nsICycleCollectorListener* aListener) {
  if (!NS_IsMainThread()) {
    return;
  }

  AUTO_PROFILER_LABEL("nsJSContext::CycleCollectNow", GCCC);

  PrepareForCycleCollectionSlice(TimeStamp());
  nsCycleCollector_collect(aListener);
  sCCStats.AfterCycleCollectionSlice();
}

// static
void nsJSContext::PrepareForCycleCollectionSlice(TimeStamp aDeadline) {
  TimeStamp beginTime = TimeStamp::Now();

  // Before we begin the cycle collection, make sure there is no active GC.
  TimeStamp afterGCTime;
  if (sScheduler.InIncrementalGC()) {
    FinishAnyIncrementalGC();
    afterGCTime = TimeStamp::Now();
  }
  sCCStats.AfterPrepareForCycleCollectionSlice(aDeadline, beginTime,
                                               afterGCTime);
}

// static
void nsJSContext::RunCycleCollectorSlice(TimeStamp aDeadline) {
  if (!NS_IsMainThread()) {
    return;
  }

  AUTO_PROFILER_TRACING_MARKER(
      "CC", aDeadline.IsNull() ? "CCSlice" : "IdleCCSlice", GCCC);

  AUTO_PROFILER_LABEL("nsJSContext::RunCycleCollectorSlice", GCCC);

  PrepareForCycleCollectionSlice(aDeadline);

  // Decide how long we want to budget for this slice.
  if (sIncrementalCC) {
    bool preferShorterSlices;
    js::SliceBudget budget = sScheduler.ComputeCCSliceBudget(
        aDeadline, sCCStats.mBeginTime, sCCStats.mEndSliceTime,
        &preferShorterSlices);
    nsCycleCollector_collectSlice(budget, preferShorterSlices);
  } else {
    js::SliceBudget budget = js::SliceBudget::unlimited();
    nsCycleCollector_collectSlice(budget, false);
  }

  sCCStats.AfterCycleCollectionSlice();
}

// static
void nsJSContext::RunCycleCollectorWorkSlice(int64_t aWorkBudget) {
  if (!NS_IsMainThread()) {
    return;
  }

  AUTO_PROFILER_LABEL("nsJSContext::RunCycleCollectorWorkSlice", GCCC);

  PrepareForCycleCollectionSlice(TimeStamp());

  js::SliceBudget budget = js::SliceBudget(js::WorkBudget(aWorkBudget));
  nsCycleCollector_collectSlice(budget);

  sCCStats.AfterCycleCollectionSlice();
}

void nsJSContext::ClearMaxCCSliceTime() {
  sCCStats.mMaxSliceTimeSinceClear = TimeDuration();
}

uint32_t nsJSContext::GetMaxCCSliceTimeSinceClear() {
  return sCCStats.mMaxSliceTimeSinceClear.ToMilliseconds();
}

// static
void nsJSContext::BeginCycleCollectionCallback() {
  MOZ_ASSERT(NS_IsMainThread());

  TimeStamp startTime = TimeStamp::Now();
  sCCStats.mBeginTime =
      sCCStats.mBeginSliceTime.IsNull() ? startTime : sCCStats.mBeginSliceTime;
  sCCStats.mSuspected = nsCycleCollector_suspectedCount();

  // Run forgetSkippable synchronously to reduce the size of the CC graph. This
  // is particularly useful if we recently finished a GC.
  if (sScheduler.IsEarlyForgetSkippable()) {
    while (sScheduler.IsEarlyForgetSkippable()) {
      FireForgetSkippable(false, TimeStamp());
    }
    sCCStats.AfterSyncForgetSkippable(startTime);
  }

  if (sShuttingDown) {
    return;
  }

  if (!sCCRunner) {
    sScheduler.InitCCRunnerStateMachine(
        mozilla::CCGCScheduler::CCRunnerState::CycleCollecting);
  }
  EnsureCCRunner(kICCIntersliceDelay, kIdleICCSliceBudget);
}

// static
void nsJSContext::EndCycleCollectionCallback(CycleCollectorResults& aResults) {
  MOZ_ASSERT(NS_IsMainThread());

  nsJSContext::KillCCRunner();

  // Update timing information for the current slice before we log it, if
  // we previously called PrepareForCycleCollectionSlice(). During shutdown
  // CCs, this won't happen.
  sCCStats.AfterCycleCollectionSlice();
  sScheduler.NoteCycleCollected(aResults);

  TimeStamp endCCTimeStamp = TimeStamp::Now();
  TimeDuration ccNowDuration = TimeBetween(sCCStats.mBeginTime, endCCTimeStamp);

  if (sScheduler.NeedsGCAfterCC()) {
    MOZ_ASSERT(StaticPrefs::javascript_options_gc_delay() >
                   kMaxICCDuration.ToMilliseconds(),
               "A max duration ICC shouldn't reduce GC delay to 0");

    PokeGC(JS::GCReason::CC_FINISHED, nullptr,
           StaticPrefs::javascript_options_gc_delay() -
               std::min(ccNowDuration, kMaxICCDuration).ToMilliseconds());
  }

  // Log information about the CC via telemetry, JSON and the console.

  sCCStats.SendTelemetry(ccNowDuration);

  uint32_t cleanups = std::max(sCCStats.mForgetSkippableBeforeCC, 1u);

  sCCStats.MaybeLogStats(aResults, cleanups);

  sCCStats.MaybeNotifyStats(aResults, ccNowDuration, cleanups);

  // Update global state to indicate we have just run a cycle collection.
  sScheduler.NoteCCEnd(endCCTimeStamp);
  sCCStats.Clear();
}

// static
bool GCRunnerFired(TimeStamp aDeadline, void* /* aClosure */) {
  MOZ_ASSERT(!sShuttingDown, "GCRunner still alive during shutdown");

  GCRunnerStep step = sScheduler.GetNextGCRunnerAction(aDeadline);
  switch (step.mAction) {
    case GCRunnerAction::None:
      nsJSContext::KillGCRunner();
      return false;

    case GCRunnerAction::MajorGC: {
      // Reconfigure the GCRunner for GC slices.
      nsIEventTarget* target = mozilla::GetCurrentEventTarget();
      if (target) {
        sGCRunner->SetTimer(
            StaticPrefs::javascript_options_gc_delay_interslice(), target);
      }
      sGCRunner->SetMinimumUsefulBudget(
          sScheduler.mActiveIntersliceGCBudget.ToMilliseconds());
      break;
    }

    case GCRunnerAction::GCSlice:
      break;
  }

  // Run a GC slice, possibly the first one of a major GC.

  MOZ_ASSERT(sScheduler.mActiveIntersliceGCBudget);
  TimeStamp startTimeStamp = TimeStamp::Now();
  TimeDuration budget =
      sScheduler.ComputeInterSliceGCBudget(aDeadline, startTimeStamp);
  TimeDuration duration = sGCUnnotifiedTotalTime;
  nsJSContext::GarbageCollectNow(step.mReason, nsJSContext::IncrementalGC,
                                 nsJSContext::NonShrinkingGC,
                                 budget.ToMilliseconds());

  sGCUnnotifiedTotalTime = TimeDuration();
  TimeStamp now = TimeStamp::Now();
  TimeDuration sliceDuration = now - startTimeStamp;
  duration += sliceDuration;
  if (duration.ToSeconds()) {
    TimeDuration idleDuration;
    if (!aDeadline.IsNull()) {
      if (aDeadline < now) {
        // This slice overflowed the idle period.
        idleDuration = aDeadline - startTimeStamp;
      } else {
        // Note, we don't want to use duration here, since it may contain
        // data also from JS engine triggered GC slices.
        idleDuration = sliceDuration;
      }
    }

    uint32_t percent =
        uint32_t(idleDuration.ToSeconds() / duration.ToSeconds() * 100);
    Telemetry::Accumulate(Telemetry::GC_SLICE_DURING_IDLE, percent);
  }

  // If the GC doesn't have any more work to do on the foreground thread (and
  // e.g. is waiting for background sweeping to finish) then return false to
  // make IdleTaskRunner postpone the next call a bit.
  JSContext* cx = danger::GetJSContext();
  return JS::IncrementalGCHasForegroundWork(cx);
}

// static
void ShrinkingGCTimerFired(nsITimer* aTimer, void* aClosure) {
  nsJSContext::KillShrinkingGCTimer();
  sIsCompactingOnUserInactive = true;
  nsJSContext::GarbageCollectNow(JS::GCReason::USER_INACTIVE,
                                 nsJSContext::IncrementalGC,
                                 nsJSContext::ShrinkingGC);
}

static bool CCRunnerFired(TimeStamp aDeadline) {
  bool didDoWork = false;

  // The CC/GC scheduler (sScheduler) decides what action(s) to take during
  // this invocation of the CC runner.
  //
  // This may be zero, one, or multiple actions. (Zero is when CC is blocked by
  // incremental GC, or when the scheduler determined that a CC is no longer
  // needed.) Loop until the scheduler finishes this invocation by returning
  // `Yield` in step.mYield.
  CCRunnerStep step;
  do {
    step = sScheduler.GetNextCCRunnerAction(aDeadline);
    switch (step.mAction) {
      case CCRunnerAction::None:
        break;

      case CCRunnerAction::ForgetSkippable:
        // 'Forget skippable' only, then end this invocation.
        FireForgetSkippable(bool(step.mRemoveChildless), aDeadline);
        break;

      case CCRunnerAction::CleanupContentUnbinder:
        // Clear content unbinder before the first actual CC slice.
        Element::ClearContentUnbinder();
        break;

      case CCRunnerAction::CleanupDeferred:
        // and if time still permits, perform deferred deletions.
        nsCycleCollector_doDeferredDeletion();
        break;

      case CCRunnerAction::CycleCollect:
        // Cycle collection slice.
        nsJSContext::RunCycleCollectorSlice(aDeadline);
        break;

      case CCRunnerAction::StopRunning:
        // End this CC, either because we have run a cycle collection slice, or
        // because a CC is no longer needed.
        nsJSContext::KillCCRunner();
        break;
    }

    if (step.mAction != CCRunnerAction::None) {
      didDoWork = true;
    }
  } while (step.mYield == CCRunnerYield::Continue);

  return didDoWork;
}

// static
void nsJSContext::EnsureCCRunner(TimeDuration aDelay, TimeDuration aBudget) {
  MOZ_ASSERT(!sShuttingDown);

  if (!sCCRunner) {
    sCCRunner = IdleTaskRunner::Create(
        CCRunnerFired, "EnsureCCRunner::CCRunnerFired", 0,
        aDelay.ToMilliseconds(), aBudget.ToMilliseconds(), true,
        [] { return sShuttingDown; });
  } else {
    sCCRunner->SetMinimumUsefulBudget(aBudget.ToMilliseconds());
    nsIEventTarget* target = mozilla::GetCurrentEventTarget();
    if (target) {
      sCCRunner->SetTimer(aDelay.ToMilliseconds(), target);
    }
  }
}

// static
bool nsJSContext::HasHadCleanupSinceLastGC() {
  return sScheduler.IsEarlyForgetSkippable(1);
}

// Check all of the various collector timers/runners and see if they are waiting
// to fire. This does not check sFullGCTimer, as that's a more expensive
// collection we run on a long timer.

// static
void nsJSContext::RunNextCollectorTimer(JS::GCReason aReason,
                                        mozilla::TimeStamp aDeadline) {
  if (sShuttingDown) {
    return;
  }

  nsCOMPtr<nsIRunnable> runnable;
  if (sGCRunner) {
    // If the GC has already started, we will just run the next slice here and
    // this call will have no effect (the reason will be ignored, and the
    // trigger will be cleared at the end of the GC.)
    sScheduler.SetWantMajorGC(aReason);
    sGCRunner->SetDeadline(aDeadline);
    runnable = sGCRunner;
  } else {
    // Check the CC timers after the GC timers, because the CC timers won't do
    // anything if a GC is in progress.
    MOZ_ASSERT(
        !sScheduler.InIncrementalGC(),
        "Don't check the CC timers if the CC is locked out during an iGC.");

    if (sCCRunner) {
      sCCRunner->SetDeadline(aDeadline);
      runnable = sCCRunner;
    }
  }

  if (runnable) {
    runnable->Run();
  }
}

// static
void nsJSContext::MaybeRunNextCollectorSlice(nsIDocShell* aDocShell,
                                             JS::GCReason aReason) {
  if (!aDocShell || !XRE_IsContentProcess()) {
    return;
  }

  BrowsingContext* bc = aDocShell->GetBrowsingContext();
  if (!bc) {
    return;
  }

  BrowsingContext* root = bc->Top();
  if (bc == root) {
    // We don't want to run collectors when loading the top level page.
    return;
  }

  nsIDocShell* rootDocShell = root->GetDocShell();
  if (!rootDocShell) {
    return;
  }

  Document* rootDocument = rootDocShell->GetDocument();
  if (!rootDocument ||
      rootDocument->GetReadyStateEnum() != Document::READYSTATE_COMPLETE ||
      rootDocument->IsInBackgroundWindow()) {
    return;
  }

  PresShell* presShell = rootDocument->GetPresShell();
  if (!presShell) {
    return;
  }

  nsViewManager* vm = presShell->GetViewManager();
  if (!vm) {
    return;
  }

  // GetLastUserEventTime returns microseconds.
  uint32_t lastEventTime = 0;
  vm->GetLastUserEventTime(lastEventTime);
  uint32_t currentTime = PR_IntervalToMicroseconds(PR_IntervalNow());
  // Only try to trigger collectors more often if user hasn't interacted with
  // the page for awhile.
  if ((currentTime - lastEventTime) >
      (StaticPrefs::dom_events_user_interaction_interval() *
       PR_USEC_PER_MSEC)) {
    Maybe<TimeStamp> next = nsRefreshDriver::GetNextTickHint();
    // Try to not delay the next RefreshDriver tick, so give a reasonable
    // deadline for collectors.
    if (next.isSome()) {
      nsJSContext::RunNextCollectorTimer(aReason, next.value());
    }
  }
}

// static
void nsJSContext::PokeGC(JS::GCReason aReason, JSObject* aObj,
                         uint32_t aDelay) {
  if (sShuttingDown) {
    return;
  }

  if (aObj) {
    JS::Zone* zone = JS::GetObjectZone(aObj);
    CycleCollectedJSRuntime::Get()->AddZoneWaitingForGC(zone);
  } else if (aReason != JS::GCReason::CC_FINISHED) {
    sScheduler.SetNeedsFullGC();
  }

  if (sGCRunner) {
    // There's already a timer for GC'ing, just return
    return;
  }

  if (sCCRunner) {
    // Make sure CC is called regardless of the size of the purple buffer, and
    // GC after it.
    sScheduler.EnsureCCThenGC();
    return;
  }

  static bool first = true;

  uint32_t delay =
      aDelay ? aDelay
             : (first ? StaticPrefs::javascript_options_gc_delay_first()
                      : StaticPrefs::javascript_options_gc_delay());

  sGCRunner = IdleTaskRunner::Create(
      [](TimeStamp aDeadline) { return GCRunnerFired(aDeadline, nullptr); },
      "GCRunnerFired",
      // Wait for javascript.options.gc_delay, then start looking for idle time
      // to run the initial GC slice. Wait at most the interslice GC delay
      // before forcing a run.
      delay, StaticPrefs::javascript_options_gc_delay_interslice(),
      sScheduler.mActiveIntersliceGCBudget.ToMilliseconds(), true,
      [] { return sShuttingDown; });

  first = false;
}

// static
void nsJSContext::PokeShrinkingGC() {
  if (sShrinkingGCTimer || sShuttingDown) {
    return;
  }

  NS_NewTimerWithFuncCallback(
      &sShrinkingGCTimer, ShrinkingGCTimerFired, nullptr,
      StaticPrefs::javascript_options_compact_on_user_inactive_delay(),
      nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY, "ShrinkingGCTimerFired");
}

// static
void nsJSContext::MaybePokeCC() {
  if (sCCRunner || sShuttingDown) {
    return;
  }

  if (sScheduler.ShouldScheduleCC()) {
    // We can kill some objects before running forgetSkippable.
    nsCycleCollector_dispatchDeferredDeletion();

    if (!sCCRunner) {
      sScheduler.InitCCRunnerStateMachine(
          mozilla::CCGCScheduler::CCRunnerState::ReducePurple);
    }
    EnsureCCRunner(kCCSkippableDelay, kForgetSkippableSliceDuration);
  }
}

void nsJSContext::KillFullGCTimer() {
  if (sFullGCTimer) {
    sFullGCTimer->Cancel();
    NS_RELEASE(sFullGCTimer);
  }
}

void nsJSContext::KillGCRunner() {
  if (sGCRunner) {
    sGCRunner->Cancel();
    sGCRunner = nullptr;
  }
}

// static
void nsJSContext::KillShrinkingGCTimer() {
  if (sShrinkingGCTimer) {
    sShrinkingGCTimer->Cancel();
    NS_RELEASE(sShrinkingGCTimer);
  }
}

// static
void nsJSContext::KillCCRunner() {
  sScheduler.UnblockCC();
  sScheduler.DeactivateCCRunner();
  if (sCCRunner) {
    sCCRunner->Cancel();
    sCCRunner = nullptr;
  }
}

static void DOMGCSliceCallback(JSContext* aCx, JS::GCProgress aProgress,
                               const JS::GCDescription& aDesc) {
  NS_ASSERTION(NS_IsMainThread(), "GCs must run on the main thread");

  static TimeStamp sCurrentGCStartTime;

  switch (aProgress) {
    case JS::GC_CYCLE_BEGIN: {
      // Prevent cycle collections and shrinking during incremental GC.
      sScheduler.NoteGCBegin();
      sCurrentGCStartTime = TimeStamp::Now();
      break;
    }

    case JS::GC_CYCLE_END: {
      TimeDuration delta = GetCollectionTimeDelta();

      if (StaticPrefs::javascript_options_mem_log()) {
        nsString gcstats;
        gcstats.Adopt(aDesc.formatSummaryMessage(aCx));
        nsAutoString prefix;
        nsTextFormatter::ssprintf(prefix, u"GC(T+%.1f)[%s-%i] ",
                                  delta.ToSeconds(),
                                  ProcessNameForCollectorLog(), getpid());
        nsString msg = prefix + gcstats;
        nsCOMPtr<nsIConsoleService> cs =
            do_GetService(NS_CONSOLESERVICE_CONTRACTID);
        if (cs) {
          cs->LogStringMessage(msg.get());
        }
      }

      sScheduler.NoteGCEnd();
      sIsCompactingOnUserInactive = false;

      // May need to kill the GC runner
      nsJSContext::KillGCRunner();

      nsJSContext::MaybePokeCC();

      if (aDesc.isZone_) {
        if (!sFullGCTimer && !sShuttingDown) {
          NS_NewTimerWithFuncCallback(
              &sFullGCTimer, FullGCTimerFired, nullptr,
              StaticPrefs::javascript_options_gc_delay_full(),
              nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY, "FullGCTimerFired");
        }
      } else {
        nsJSContext::KillFullGCTimer();
        sScheduler.SetNeedsFullGC(false);
      }

      if (sScheduler.IsCCNeeded()) {
        nsCycleCollector_dispatchDeferredDeletion();
      }

      Telemetry::Accumulate(Telemetry::GC_IN_PROGRESS_MS,
                            TimeUntilNow(sCurrentGCStartTime).ToMilliseconds());
      break;
    }

    case JS::GC_SLICE_BEGIN:
      break;

    case JS::GC_SLICE_END:
      sGCUnnotifiedTotalTime +=
          aDesc.lastSliceEnd(aCx) - aDesc.lastSliceStart(aCx);

      if (sShuttingDown || aDesc.isComplete_) {
        nsJSContext::KillGCRunner();
      } else if (!sGCRunner) {
        // If incremental GC wasn't triggered by GCTimerFired, we may not
        // have a runner to ensure all the slices are handled. So, create
        // the runner here.
        sGCRunner = IdleTaskRunner::Create(
            [](TimeStamp aDeadline) {
              return GCRunnerFired(aDeadline, nullptr);
            },
            "GCRunnerFired",
            // Start immediately looking for idle time, waiting at most the
            // interslice GC delay before forcing a run.
            0, StaticPrefs::javascript_options_gc_delay_interslice(),
            sScheduler.mActiveIntersliceGCBudget.ToMilliseconds(), true,
            [] { return sShuttingDown; });
      }

      if (sScheduler.IsCCNeeded()) {
        nsCycleCollector_dispatchDeferredDeletion();
      }

      if (StaticPrefs::javascript_options_mem_log()) {
        nsString gcstats;
        gcstats.Adopt(aDesc.formatSliceMessage(aCx));
        nsAutoString prefix;
        nsTextFormatter::ssprintf(prefix, u"[%s-%i] ",
                                  ProcessNameForCollectorLog(), getpid());
        nsString msg = prefix + gcstats;
        nsCOMPtr<nsIConsoleService> cs =
            do_GetService(NS_CONSOLESERVICE_CONTRACTID);
        if (cs) {
          cs->LogStringMessage(msg.get());
        }
      }

      break;

    default:
      MOZ_CRASH("Unexpected GCProgress value");
  }

  if (sPrevGCSliceCallback) {
    (*sPrevGCSliceCallback)(aCx, aProgress, aDesc);
  }
}

void nsJSContext::SetWindowProxy(JS::Handle<JSObject*> aWindowProxy) {
  mWindowProxy = aWindowProxy;
}

JSObject* nsJSContext::GetWindowProxy() { return mWindowProxy; }

void nsJSContext::LikelyShortLivingObjectCreated() {
  ++sScheduler.mLikelyShortLivingObjectsNeedingGC;
}

void mozilla::dom::StartupJSEnvironment() {
  // initialize all our statics, so that we can restart XPCOM
  sShrinkingGCTimer = sFullGCTimer = nullptr;
  sIsInitialized = false;
  sShuttingDown = false;
  new (&sScheduler) CCGCScheduler();  // Reset the scheduler state.
  sCCStats.Init();
}

static void SetGCParameter(JSGCParamKey aParam, uint32_t aValue) {
  AutoJSAPI jsapi;
  jsapi.Init();
  JS_SetGCParameter(jsapi.cx(), aParam, aValue);
}

static void ResetGCParameter(JSGCParamKey aParam) {
  AutoJSAPI jsapi;
  jsapi.Init();
  JS_ResetGCParameter(jsapi.cx(), aParam);
}

static void SetMemoryPrefChangedCallbackMB(const char* aPrefName,
                                           void* aClosure) {
  int32_t prefMB = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  CheckedInt<int32_t> prefB = CheckedInt<int32_t>(prefMB) * 1024 * 1024;
  if (prefB.isValid() && prefB.value() >= 0) {
    SetGCParameter((JSGCParamKey)(uintptr_t)aClosure, prefB.value());
  } else {
    ResetGCParameter((JSGCParamKey)(uintptr_t)aClosure);
  }
}

static void SetMemoryNurseryPrefChangedCallback(const char* aPrefName,
                                                void* aClosure) {
  int32_t prefKB = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  CheckedInt<int32_t> prefB = CheckedInt<int32_t>(prefKB) * 1024;
  if (prefB.isValid() && prefB.value() >= 0) {
    SetGCParameter((JSGCParamKey)(uintptr_t)aClosure, prefB.value());
  } else {
    ResetGCParameter((JSGCParamKey)(uintptr_t)aClosure);
  }
}

static void SetMemoryPrefChangedCallbackInt(const char* aPrefName,
                                            void* aClosure) {
  int32_t pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  if (pref >= 0 && pref < 10000) {
    SetGCParameter((JSGCParamKey)(uintptr_t)aClosure, pref);
  } else {
    ResetGCParameter((JSGCParamKey)(uintptr_t)aClosure);
  }
}

static void SetMemoryPrefChangedCallbackBool(const char* aPrefName,
                                             void* aClosure) {
  bool pref = Preferences::GetBool(aPrefName);
  SetGCParameter((JSGCParamKey)(uintptr_t)aClosure, pref);
}

static void SetMemoryGCSliceTimePrefChangedCallback(const char* aPrefName,
                                                    void* aClosure) {
  int32_t pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  if (pref > 0 && pref < 100000) {
    sScheduler.SetActiveIntersliceGCBudget(
        TimeDuration::FromMilliseconds(pref));
    SetGCParameter(JSGC_SLICE_TIME_BUDGET_MS, pref);
  } else {
    ResetGCParameter(JSGC_SLICE_TIME_BUDGET_MS);
  }
}

static void SetIncrementalCCPrefChangedCallback(const char* aPrefName,
                                                void* aClosure) {
  bool pref = Preferences::GetBool(aPrefName);
  sIncrementalCC = pref;
}

class JSDispatchableRunnable final : public Runnable {
  ~JSDispatchableRunnable() { MOZ_ASSERT(!mDispatchable); }

 public:
  explicit JSDispatchableRunnable(JS::Dispatchable* aDispatchable)
      : mozilla::Runnable("JSDispatchableRunnable"),
        mDispatchable(aDispatchable) {
    MOZ_ASSERT(mDispatchable);
  }

 protected:
  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    AutoJSAPI jsapi;
    jsapi.Init();

    JS::Dispatchable::MaybeShuttingDown maybeShuttingDown =
        sShuttingDown ? JS::Dispatchable::ShuttingDown
                      : JS::Dispatchable::NotShuttingDown;

    mDispatchable->run(jsapi.cx(), maybeShuttingDown);
    mDispatchable = nullptr;  // mDispatchable may delete itself

    return NS_OK;
  }

 private:
  JS::Dispatchable* mDispatchable;
};

static bool DispatchToEventLoop(void* closure,
                                JS::Dispatchable* aDispatchable) {
  MOZ_ASSERT(!closure);

  // This callback may execute either on the main thread or a random JS-internal
  // helper thread. This callback can be called during shutdown so we cannot
  // simply NS_DispatchToMainThread. Failure during shutdown is expected and
  // properly handled by the JS engine.

  nsCOMPtr<nsIEventTarget> mainTarget = GetMainThreadEventTarget();
  if (!mainTarget) {
    return false;
  }

  RefPtr<JSDispatchableRunnable> r = new JSDispatchableRunnable(aDispatchable);
  MOZ_ALWAYS_SUCCEEDS(mainTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL));
  return true;
}

static bool ConsumeStream(JSContext* aCx, JS::HandleObject aObj,
                          JS::MimeType aMimeType,
                          JS::StreamConsumer* aConsumer) {
  return FetchUtil::StreamResponseToJS(aCx, aObj, aMimeType, aConsumer,
                                       nullptr);
}

void nsJSContext::EnsureStatics() {
  if (sIsInitialized) {
    if (!nsContentUtils::XPConnect()) {
      MOZ_CRASH();
    }
    return;
  }

  // Let's make sure that our main thread is the same as the xpcom main thread.
  MOZ_ASSERT(NS_IsMainThread());

  AutoJSAPI jsapi;
  jsapi.Init();

  sPrevGCSliceCallback = JS::SetGCSliceCallback(jsapi.cx(), DOMGCSliceCallback);

  JS::InitDispatchToEventLoop(jsapi.cx(), DispatchToEventLoop, nullptr);
  JS::InitConsumeStreamCallback(jsapi.cx(), ConsumeStream,
                                FetchUtil::ReportJSStreamError);

  // Set these global xpconnect options...
  Preferences::RegisterCallbackAndCall(SetMemoryPrefChangedCallbackMB,
                                       "javascript.options.mem.max",
                                       (void*)JSGC_MAX_BYTES);
  Preferences::RegisterCallbackAndCall(SetMemoryNurseryPrefChangedCallback,
                                       "javascript.options.mem.nursery.min_kb",
                                       (void*)JSGC_MIN_NURSERY_BYTES);
  Preferences::RegisterCallbackAndCall(SetMemoryNurseryPrefChangedCallback,
                                       "javascript.options.mem.nursery.max_kb",
                                       (void*)JSGC_MAX_NURSERY_BYTES);

  Preferences::RegisterCallbackAndCall(SetMemoryPrefChangedCallbackBool,
                                       "javascript.options.mem.gc_per_zone",
                                       (void*)JSGC_PER_ZONE_GC_ENABLED);

  Preferences::RegisterCallbackAndCall(SetMemoryPrefChangedCallbackBool,
                                       "javascript.options.mem.gc_incremental",
                                       (void*)JSGC_INCREMENTAL_GC_ENABLED);

  Preferences::RegisterCallbackAndCall(
      SetMemoryGCSliceTimePrefChangedCallback,
      "javascript.options.mem.gc_incremental_slice_ms");

  Preferences::RegisterCallbackAndCall(SetMemoryPrefChangedCallbackBool,
                                       "javascript.options.mem.gc_compacting",
                                       (void*)JSGC_COMPACTING_ENABLED);

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackBool,
      "javascript.options.mem.incremental_weakmap",
      (void*)JSGC_INCREMENTAL_WEAKMAP_ENABLED);

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_high_frequency_time_limit_ms",
      (void*)JSGC_HIGH_FREQUENCY_TIME_LIMIT);

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_low_frequency_heap_growth",
      (void*)JSGC_LOW_FREQUENCY_HEAP_GROWTH);

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_high_frequency_large_heap_growth",
      (void*)JSGC_HIGH_FREQUENCY_LARGE_HEAP_GROWTH);

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_high_frequency_small_heap_growth",
      (void*)JSGC_HIGH_FREQUENCY_SMALL_HEAP_GROWTH);

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_small_heap_size_max_mb",
      (void*)JSGC_SMALL_HEAP_SIZE_MAX);

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_large_heap_size_min_mb",
      (void*)JSGC_LARGE_HEAP_SIZE_MIN);

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_allocation_threshold_mb",
      (void*)JSGC_ALLOCATION_THRESHOLD);

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_small_heap_incremental_limit",
      (void*)JSGC_SMALL_HEAP_INCREMENTAL_LIMIT);
  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_large_heap_incremental_limit",
      (void*)JSGC_LARGE_HEAP_INCREMENTAL_LIMIT);

  Preferences::RegisterCallbackAndCall(SetIncrementalCCPrefChangedCallback,
                                       "dom.cycle_collector.incremental");

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_min_empty_chunk_count",
      (void*)JSGC_MIN_EMPTY_CHUNK_COUNT);

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_max_empty_chunk_count",
      (void*)JSGC_MAX_EMPTY_CHUNK_COUNT);

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_helper_thread_ratio",
      (void*)JSGC_HELPER_THREAD_RATIO);

  Preferences::RegisterCallbackAndCall(
      SetMemoryPrefChangedCallbackInt,
      "javascript.options.mem.gc_max_helper_threads",
      (void*)JSGC_MAX_HELPER_THREADS);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    MOZ_CRASH();
  }

  nsIObserver* observer = new nsJSEnvironmentObserver();
  obs->AddObserver(observer, "memory-pressure", false);
  obs->AddObserver(observer, "user-interaction-inactive", false);
  obs->AddObserver(observer, "user-interaction-active", false);
  obs->AddObserver(observer, "quit-application", false);
  obs->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  obs->AddObserver(observer, "content-child-will-shutdown", false);

  sIsInitialized = true;
}

void mozilla::dom::ShutdownJSEnvironment() {
  KillTimers();

  sShuttingDown = true;
  sScheduler.Shutdown();
}

AsyncErrorReporter::AsyncErrorReporter(xpc::ErrorReport* aReport)
    : Runnable("dom::AsyncErrorReporter"), mReport(aReport) {}

void AsyncErrorReporter::SerializeStack(JSContext* aCx,
                                        JS::Handle<JSObject*> aStack) {
  mStackHolder = MakeUnique<SerializedStackHolder>();
  mStackHolder->SerializeMainThreadOrWorkletStack(aCx, aStack);
}

void AsyncErrorReporter::SetException(JSContext* aCx,
                                      JS::Handle<JS::Value> aException) {
  MOZ_ASSERT(NS_IsMainThread());
  mException.init(aCx, aException);
  mHasException = true;
}

NS_IMETHODIMP AsyncErrorReporter::Run() {
  AutoJSAPI jsapi;
  // We're only using this context to deserialize a stack to report to the
  // console, so the scope we use doesn't matter. Stack frame filtering happens
  // based on the principal encoded into the frame and the caller compartment,
  // not the compartment of the frame object, and the console reporting code
  // will not be using our context, and therefore will not care what compartment
  // it has entered.
  DebugOnly<bool> ok = jsapi.Init(xpc::PrivilegedJunkScope());
  MOZ_ASSERT(ok, "Problem with system global?");
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> stack(cx);
  JS::Rooted<JSObject*> stackGlobal(cx);
  if (mStackHolder) {
    stack = mStackHolder->ReadStack(cx);
    if (stack) {
      stackGlobal = JS::CurrentGlobalOrNull(cx);
    }
  }

  JS::Rooted<Maybe<JS::Value>> exception(cx, Nothing());
  if (mHasException) {
    MOZ_ASSERT(NS_IsMainThread());
    exception = Some(mException);
    // Remove our reference to the exception.
    mException.setUndefined();
    mHasException = false;
  }

  mReport->LogToConsoleWithStack(nullptr, exception, stack, stackGlobal);
  return NS_OK;
}

// A fast-array class for JS.  This class supports both nsIJSScriptArray and
// nsIArray.  If it is JS itself providing and consuming this class, all work
// can be done via nsIJSScriptArray, and avoid the conversion of elements
// to/from nsISupports.
// When consumed by non-JS (eg, another script language), conversion is done
// on-the-fly.
class nsJSArgArray final : public nsIJSArgArray {
 public:
  nsJSArgArray(JSContext* aContext, uint32_t argc, const JS::Value* argv,
               nsresult* prv);

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsJSArgArray,
                                                         nsIJSArgArray)

  // nsIArray
  NS_DECL_NSIARRAY

  // nsIJSArgArray
  nsresult GetArgs(uint32_t* argc, void** argv) override;

  void ReleaseJSObjects();

 protected:
  ~nsJSArgArray();
  JSContext* mContext;
  JS::Heap<JS::Value>* mArgv;
  uint32_t mArgc;
};

nsJSArgArray::nsJSArgArray(JSContext* aContext, uint32_t argc,
                           const JS::Value* argv, nsresult* prv)
    : mContext(aContext), mArgv(nullptr), mArgc(argc) {
  // copy the array - we don't know its lifetime, and ours is tied to xpcom
  // refcounting.
  if (argc) {
    mArgv = new (fallible) JS::Heap<JS::Value>[argc];
    if (!mArgv) {
      *prv = NS_ERROR_OUT_OF_MEMORY;
      return;
    }
  }

  // Callers are allowed to pass in a null argv even for argc > 0. They can
  // then use GetArgs to initialize the values.
  if (argv) {
    for (uint32_t i = 0; i < argc; ++i) mArgv[i] = argv[i];
  }

  if (argc > 0) {
    mozilla::HoldJSObjects(this);
  }

  *prv = NS_OK;
}

nsJSArgArray::~nsJSArgArray() { ReleaseJSObjects(); }

void nsJSArgArray::ReleaseJSObjects() {
  if (mArgv) {
    delete[] mArgv;
  }
  if (mArgc > 0) {
    mArgc = 0;
    mozilla::DropJSObjects(this);
  }
}

// QueryInterface implementation for nsJSArgArray
NS_IMPL_CYCLE_COLLECTION_MULTI_ZONE_JSHOLDER_CLASS(nsJSArgArray)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSArgArray)
  tmp->ReleaseJSObjects();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsJSArgArray)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSArgArray)
  if (tmp->mArgv) {
    for (uint32_t i = 0; i < tmp->mArgc; ++i) {
      NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mArgv[i])
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsJSArgArray)
  NS_INTERFACE_MAP_ENTRY(nsIArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSArgArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIJSArgArray)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsJSArgArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsJSArgArray)

nsresult nsJSArgArray::GetArgs(uint32_t* argc, void** argv) {
  *argv = (void*)mArgv;
  *argc = mArgc;
  return NS_OK;
}

// nsIArray impl
NS_IMETHODIMP nsJSArgArray::GetLength(uint32_t* aLength) {
  *aLength = mArgc;
  return NS_OK;
}

NS_IMETHODIMP nsJSArgArray::QueryElementAt(uint32_t index, const nsIID& uuid,
                                           void** result) {
  *result = nullptr;
  if (index >= mArgc) return NS_ERROR_INVALID_ARG;

  if (uuid.Equals(NS_GET_IID(nsIVariant)) ||
      uuid.Equals(NS_GET_IID(nsISupports))) {
    // Have to copy a Heap into a Rooted to work with it.
    JS::Rooted<JS::Value> val(mContext, mArgv[index]);
    return nsContentUtils::XPConnect()->JSToVariant(mContext, val,
                                                    (nsIVariant**)result);
  }
  NS_WARNING("nsJSArgArray only handles nsIVariant");
  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP nsJSArgArray::IndexOf(uint32_t startIndex, nsISupports* element,
                                    uint32_t* _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsJSArgArray::ScriptedEnumerate(const nsIID& aElemIID,
                                              uint8_t aArgc,
                                              nsISimpleEnumerator** aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsJSArgArray::EnumerateImpl(const nsID& aEntryIID,
                                          nsISimpleEnumerator** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

// The factory function
nsresult NS_CreateJSArgv(JSContext* aContext, uint32_t argc,
                         const JS::Value* argv, nsIJSArgArray** aArray) {
  nsresult rv;
  nsCOMPtr<nsIJSArgArray> ret = new nsJSArgArray(aContext, argc, argv, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  ret.forget(aArray);
  return NS_OK;
}
