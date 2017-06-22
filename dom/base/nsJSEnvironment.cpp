/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "nsJSEnvironment.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIDOMChromeWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIScriptSecurityManager.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
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
#include "nsIScriptError.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPrompt.h"
#include "nsIObserverService.h"
#include "nsITimer.h"
#include "nsIAtom.h"
#include "nsContentUtils.h"
#include "mozilla/EventDispatcher.h"
#include "nsIContent.h"
#include "nsCycleCollector.h"
#include "nsXPCOMCIDInternal.h"
#include "nsIXULRuntime.h"
#include "nsTextFormatter.h"
#ifdef XP_WIN
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h> // for getpid()
#endif
#include "xpcpublic.h"

#include "jsapi.h"
#include "jswrapper.h"
#include "js/SliceBudget.h"
#include "nsIArray.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "prmem.h"
#include "WrapperFactory.h"
#include "nsGlobalWindow.h"
#include "nsScriptNameSpaceManager.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/MainThreadIdlePeriod.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsAXPCNativeCallContext.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/SystemGroup.h"
#include "nsRefreshDriver.h"
#include "nsJSPrincipals.h"

#ifdef XP_MACOSX
// AssertMacros.h defines 'check' and conflicts with AccessCheck.h
#undef check
#endif
#include "AccessCheck.h"

#include "mozilla/Logging.h"
#include "prthread.h"

#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/asmjscache/AsmJSCache.h"
#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/ContentEvents.h"

#include "nsCycleCollectionNoteRootCallback.h"
#include "GeckoProfiler.h"

using namespace mozilla;
using namespace mozilla::dom;

const size_t gStackSize = 8192;

// Thank you Microsoft!
#ifdef CompareString
#undef CompareString
#endif

#define NS_SHRINK_GC_BUFFERS_DELAY  4000 // ms

// The amount of time we wait from the first request to GC to actually
// doing the first GC.
#define NS_FIRST_GC_DELAY           10000 // ms

#define NS_FULL_GC_DELAY            60000 // ms

// The default amount of time to wait from the user being idle to starting a
// shrinking GC.
#define NS_DEAULT_INACTIVE_GC_DELAY 300000 // ms

// Maximum amount of time that should elapse between incremental GC slices
#define NS_INTERSLICE_GC_DELAY      100 // ms

// The amount of time we wait between a request to CC (after GC ran)
// and doing the actual CC.
#define NS_CC_DELAY                 6000 // ms

#define NS_CC_SKIPPABLE_DELAY       250 // ms

// ForgetSkippable is usually fast, so we can use small budgets.
// This isn't a real budget but a hint to CollectorRunner whether there
// is enough time to call ForgetSkippable.
static const int64_t kForgetSkippableSliceDuration = 2;

// Maximum amount of time that should elapse between incremental CC slices
static const int64_t kICCIntersliceDelay = 32; // ms

// Time budget for an incremental CC slice when using timer to run it.
static const int64_t kICCSliceBudget = 5; // ms
// Minimum budget for an incremental CC slice when using idle time to run it.
static const int64_t kIdleICCSliceBudget = 3; // ms

// Maximum total duration for an ICC
static const uint32_t kMaxICCDuration = 2000; // ms

// Force a CC after this long if there's more than NS_CC_FORCED_PURPLE_LIMIT
// objects in the purple buffer.
#define NS_CC_FORCED                (2 * 60 * PR_USEC_PER_SEC) // 2 min
#define NS_CC_FORCED_PURPLE_LIMIT   10

// Don't allow an incremental GC to lock out the CC for too long.
#define NS_MAX_CC_LOCKEDOUT_TIME    (30 * PR_USEC_PER_SEC) // 30 seconds

// Trigger a CC if the purple buffer exceeds this size when we check it.
#define NS_CC_PURPLE_LIMIT          200

// Large value used to specify that a script should run essentially forever
#define NS_UNLIMITED_SCRIPT_RUNTIME (0x40000000LL << 32)

class CollectorRunner;

// if you add statics here, add them to the list in StartupJSEnvironment

static nsITimer *sGCTimer;
static nsITimer *sShrinkingGCTimer;
static StaticRefPtr<CollectorRunner> sCCRunner;
static StaticRefPtr<CollectorRunner> sICCRunner;
static nsITimer *sFullGCTimer;
static StaticRefPtr<CollectorRunner> sInterSliceGCRunner;

static TimeStamp sLastCCEndTime;

static bool sCCLockedOut;
static PRTime sCCLockedOutTime;

static JS::GCSliceCallback sPrevGCSliceCallback;

static bool sHasRunGC;

// The number of currently pending document loads. This count isn't
// guaranteed to always reflect reality and can't easily as we don't
// have an easy place to know when a load ends or is interrupted in
// all cases. This counter also gets reset if we end up GC'ing while
// we're waiting for a slow page to load. IOW, this count may be 0
// even when there are pending loads.
static uint32_t sPendingLoadCount;
static bool sLoadingInProgress;

static uint32_t sCCollectedWaitingForGC;
static uint32_t sCCollectedZonesWaitingForGC;
static uint32_t sLikelyShortLivingObjectsNeedingGC;
static bool sPostGCEventsToConsole;
static bool sPostGCEventsToObserver;
static int32_t sCCRunnerFireCount = 0;
static uint32_t sMinForgetSkippableTime = UINT32_MAX;
static uint32_t sMaxForgetSkippableTime = 0;
static uint32_t sTotalForgetSkippableTime = 0;
static uint32_t sRemovedPurples = 0;
static uint32_t sForgetSkippableBeforeCC = 0;
static uint32_t sPreviousSuspectedCount = 0;
static uint32_t sCleanupsSinceLastGC = UINT32_MAX;
static bool sNeedsFullCC = false;
static bool sNeedsFullGC = false;
static bool sNeedsGCAfterCC = false;
static bool sIncrementalCC = false;
static int32_t sActiveIntersliceGCBudget = 5; // ms;
static nsScriptNameSpaceManager *gNameSpaceManager;

static PRTime sFirstCollectionTime;

static bool sIsInitialized;
static bool sDidShutdown;
static bool sShuttingDown;
static int32_t sContextCount;

static nsIScriptSecurityManager *sSecurityManager;

// nsJSEnvironmentObserver observes the memory-pressure notifications
// and forces a garbage collection and cycle collection when it happens, if
// the appropriate pref is set.

static bool sGCOnMemoryPressure;

// nsJSEnvironmentObserver observes the user-interaction-inactive notifications
// and triggers a shrinking a garbage collection if the user is still inactive
// after NS_SHRINKING_GC_DELAY ms later, if the appropriate pref is set.

static bool sCompactOnUserInactive;
static uint32_t sCompactOnUserInactiveDelay = NS_DEAULT_INACTIVE_GC_DELAY;
static bool sIsCompactingOnUserInactive = false;

// In testing, we call RunNextCollectorTimer() to ensure that the collectors are run more
// aggressively than they would be in regular browsing. sExpensiveCollectorPokes keeps
// us from triggering expensive full collections too frequently.
static int32_t sExpensiveCollectorPokes = 0;
static const int32_t kPokesBetweenExpensiveCollectorTriggers = 5;

static TimeDuration sGCUnnotifiedTotalTime;

// Return true if some meaningful work was done.
typedef bool (*CollectorRunnerCallback) (TimeStamp aDeadline, void* aData);

// Repeating callback runner for CC and GC.
class CollectorRunner final : public IdleRunnable
{
public:
  static already_AddRefed<CollectorRunner>
  Create(CollectorRunnerCallback aCallback, uint32_t aDelay,
         int64_t aBudget, bool aRepeating, void* aData = nullptr)
  {
    if (sShuttingDown) {
      return nullptr;
    }

    RefPtr<CollectorRunner> runner =
      new CollectorRunner(aCallback, aDelay, aBudget, aRepeating, aData);
    runner->Schedule(false); // Initial scheduling shouldn't use idle dispatch.
    return runner.forget();
  }

  NS_IMETHOD Run() override
  {
    if (!mCallback) {
      return NS_OK;
    }

    // Deadline is null when called from timer.
    bool deadLineWasNull = mDeadline.IsNull();
    bool didRun = false;
    if (deadLineWasNull || ((TimeStamp::Now() + mBudget) < mDeadline)) {
      CancelTimer();
      didRun = mCallback(mDeadline, mData);
    }

    if (mCallback && (mRepeating || !didRun)) {
      // If we didn't do meaningful work, don't schedule using immediate
      // idle dispatch, since that could lead to a loop until the idle
      // period ends.
      Schedule(didRun);
    }

    return NS_OK;
  }

  static void
  TimedOut(nsITimer* aTimer, void* aClosure)
  {
    RefPtr<CollectorRunner> runnable = static_cast<CollectorRunner*>(aClosure);
    runnable->Run();
  }

  void SetDeadline(mozilla::TimeStamp aDeadline) override
  {
    mDeadline = aDeadline;
  };

  void SetTimer(uint32_t aDelay, nsIEventTarget* aTarget) override
  {
    if (mTimerActive) {
      return;
    }

    mTarget = aTarget;
    if (!mTimer) {
      mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    } else {
      mTimer->Cancel();
    }

    if (mTimer) {
      mTimer->SetTarget(mTarget);
      mTimer->InitWithFuncCallback(TimedOut, this, aDelay,
                                   nsITimer::TYPE_ONE_SHOT);
      mTimerActive = true;
    }
  }

  nsresult Cancel() override
  {
    CancelTimer();
    mTimer = nullptr;
    mScheduleTimer = nullptr;
    mCallback = nullptr;
    return NS_OK;
  }

  static void
  ScheduleTimedOut(nsITimer* aTimer, void* aClosure)
  {
    RefPtr<CollectorRunner> runnable = static_cast<CollectorRunner*>(aClosure);
    runnable->Schedule(true);
  }

  void Schedule(bool aAllowIdleDispatch)
  {
    if (!mCallback) {
      return;
    }

    if (sShuttingDown) {
      Cancel();
      return;
    }

    mDeadline = TimeStamp();
    TimeStamp now = TimeStamp::Now();
    TimeStamp hint = nsRefreshDriver::GetIdleDeadlineHint(now);
    if (hint != now) {
      // RefreshDriver is ticking, let it schedule the idle dispatch.
      nsRefreshDriver::DispatchIdleRunnableAfterTick(this, mDelay);
      // Ensure we get called at some point, even if RefreshDriver is stopped.
      SetTimer(mDelay, mTarget);
    } else {
      // RefreshDriver doesn't seem to be running.
      if (aAllowIdleDispatch) {
        nsCOMPtr<nsIRunnable> runnable = this;
        NS_IdleDispatchToCurrentThread(runnable.forget(), mDelay);
        SetTimer(mDelay, mTarget);
      } else {
        if (!mScheduleTimer) {
          mScheduleTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
          if (!mScheduleTimer) {
            return;
          }
        } else {
          mScheduleTimer->Cancel();
        }

        // We weren't allowed to do idle dispatch immediately, do it after a
        // short timeout.
        mScheduleTimer->InitWithFuncCallback(ScheduleTimedOut, this, 16,
                                             nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY);
      }
    }
  }

private:
  explicit CollectorRunner(CollectorRunnerCallback aCallback,
                           uint32_t aDelay, int64_t aBudget,
                           bool aRepeating, void* aData)
    : mCallback(aCallback), mDelay(aDelay)
    , mBudget(TimeDuration::FromMilliseconds(aBudget))
    , mRepeating(aRepeating), mTimerActive(false), mData(aData)
  {
  }

  ~CollectorRunner()
  {
    CancelTimer();
  }

  void CancelTimer()
  {
    nsRefreshDriver::CancelIdleRunnable(this);
    if (mTimer) {
      mTimer->Cancel();
    }
    if (mScheduleTimer) {
      mScheduleTimer->Cancel();
    }
    mTimerActive = false;
  }

  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsITimer> mScheduleTimer;
  nsCOMPtr<nsIEventTarget> mTarget;
  CollectorRunnerCallback mCallback;
  uint32_t mDelay;
  TimeStamp mDeadline;
  TimeDuration mBudget;
  bool mRepeating;
  bool mTimerActive;
  void* mData;
};

static const char*
ProcessNameForCollectorLog()
{
  return XRE_GetProcessType() == GeckoProcessType_Default ?
    "default" : "content";
}

namespace xpc {

// This handles JS Exceptions (via ExceptionStackOrNull), as well as DOM and XPC
// Exceptions.
//
// Note that the returned object is _not_ wrapped into the compartment of
// exceptionValue.
JSObject*
FindExceptionStackForConsoleReport(nsPIDOMWindowInner* win,
                                   JS::HandleValue exceptionValue)
{
  if (!exceptionValue.isObject()) {
    return nullptr;
  }

  if (win && win->InnerObjectsFreed()) {
    // Pretend like we have no stack, so we don't end up keeping the global
    // alive via the stack.
    return nullptr;
  }

  JS::RootingContext* rcx = RootingCx();
  JS::RootedObject exceptionObject(rcx, &exceptionValue.toObject());
  JSObject* stackObject = ExceptionStackOrNull(exceptionObject);
  if (stackObject) {
    return stackObject;
  }

  // It is not a JS Exception, try DOM Exception.
  RefPtr<Exception> exception;
  UNWRAP_OBJECT(DOMException, exceptionObject, exception);
  if (!exception) {
    // Not a DOM Exception, try XPC Exception.
    UNWRAP_OBJECT(Exception, exceptionObject, exception);
    if (!exception) {
      return nullptr;
    }
  }

  nsCOMPtr<nsIStackFrame> stack = exception->GetLocation();
  if (!stack) {
    return nullptr;
  }
  JS::RootedValue value(rcx);
  stack->GetNativeSavedFrame(&value);
  if (value.isObject()) {
    return &value.toObject();
  }
  return nullptr;
}

} /* namespace xpc */

static PRTime
GetCollectionTimeDelta()
{
  PRTime now = PR_Now();
  if (sFirstCollectionTime) {
    return now - sFirstCollectionTime;
  }
  sFirstCollectionTime = now;
  return 0;
}

static void
KillTimers()
{
  nsJSContext::KillGCTimer();
  nsJSContext::KillShrinkingGCTimer();
  nsJSContext::KillCCRunner();
  nsJSContext::KillICCRunner();
  nsJSContext::KillFullGCTimer();
  nsJSContext::KillInterSliceGCRunner();
}

// If we collected a substantial amount of cycles, poke the GC since more objects
// might be unreachable now.
static bool
NeedsGCAfterCC()
{
  return sCCollectedWaitingForGC > 250 ||
    sCCollectedZonesWaitingForGC > 0 ||
    sLikelyShortLivingObjectsNeedingGC > 2500 ||
    sNeedsGCAfterCC;
}

class nsJSEnvironmentObserver final : public nsIObserver
{
  ~nsJSEnvironmentObserver() {}
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS(nsJSEnvironmentObserver, nsIObserver)

NS_IMETHODIMP
nsJSEnvironmentObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                 const char16_t* aData)
{
  if (!nsCRT::strcmp(aTopic, "memory-pressure")) {
    if (sGCOnMemoryPressure) {
      if(StringBeginsWith(nsDependentString(aData),
                          NS_LITERAL_STRING("low-memory-ongoing"))) {
        // Don't GC/CC if we are in an ongoing low-memory state since its very
        // slow and it likely won't help us anyway.
        return NS_OK;
      }
      nsJSContext::GarbageCollectNow(JS::gcreason::MEM_PRESSURE,
                                     nsJSContext::NonIncrementalGC,
                                     nsJSContext::ShrinkingGC);
      nsJSContext::CycleCollectNow();
      if (NeedsGCAfterCC()) {
        nsJSContext::GarbageCollectNow(JS::gcreason::MEM_PRESSURE,
                                       nsJSContext::NonIncrementalGC,
                                       nsJSContext::ShrinkingGC);
      }
    }
  } else if (!nsCRT::strcmp(aTopic, "user-interaction-inactive")) {
    if (sCompactOnUserInactive) {
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
             !nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
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
  explicit AutoFree(void* aPtr) : mPtr(aPtr) {
  }
  ~AutoFree() {
    if (mPtr)
      free(mPtr);
  }
  void Invalidate() {
    mPtr = 0;
  }
private:
  void *mPtr;
};

// A utility function for script languages to call.  Although it looks small,
// the use of nsIDocShell and nsPresContext triggers a huge number of
// dependencies that most languages would not otherwise need.
// XXXmarkh - This function is mis-placed!
bool
NS_HandleScriptError(nsIScriptGlobalObject *aScriptGlobal,
                     const ErrorEventInit &aErrorEventInit,
                     nsEventStatus *aStatus)
{
  bool called = false;
  nsCOMPtr<nsPIDOMWindowInner> win(do_QueryInterface(aScriptGlobal));
  nsIDocShell *docShell = win ? win->GetDocShell() : nullptr;
  if (docShell) {
    RefPtr<nsPresContext> presContext;
    docShell->GetPresContext(getter_AddRefs(presContext));

    static int32_t errorDepth; // Recursion prevention
    ++errorDepth;

    if (errorDepth < 2) {
      // Dispatch() must be synchronous for the recursion block
      // (errorDepth) to work.
      RefPtr<ErrorEvent> event =
        ErrorEvent::Constructor(nsGlobalWindow::Cast(win),
                                NS_LITERAL_STRING("error"),
                                aErrorEventInit);
      event->SetTrusted(true);

      EventDispatcher::DispatchDOMEvent(win, nullptr, event, presContext,
                                        aStatus);
      called = true;
    }
    --errorDepth;
  }
  return called;
}

class ScriptErrorEvent : public Runnable
{
public:
  ScriptErrorEvent(nsPIDOMWindowInner* aWindow,
                   JS::RootingContext* aRootingCx,
                   xpc::ErrorReport* aReport,
                   JS::Handle<JS::Value> aError)
    : mWindow(aWindow)
    , mReport(aReport)
    , mError(aRootingCx, aError)
  {}

  NS_IMETHOD Run() override
  {
    nsEventStatus status = nsEventStatus_eIgnore;
    nsPIDOMWindowInner* win = mWindow;
    MOZ_ASSERT(win);
    MOZ_ASSERT(NS_IsMainThread());
    // First, notify the DOM that we have a script error, but only if
    // our window is still the current inner.
    JS::RootingContext* rootingCx = RootingCx();
    if (win->IsCurrentInnerWindow() && win->GetDocShell() && !sHandlingScriptError) {
      AutoRestore<bool> recursionGuard(sHandlingScriptError);
      sHandlingScriptError = true;

      RefPtr<nsPresContext> presContext;
      win->GetDocShell()->GetPresContext(getter_AddRefs(presContext));

      RootedDictionary<ErrorEventInit> init(rootingCx);
      init.mCancelable = true;
      init.mFilename = mReport->mFileName;
      init.mBubbles = true;

      NS_NAMED_LITERAL_STRING(xoriginMsg, "Script error.");
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

      RefPtr<ErrorEvent> event =
        ErrorEvent::Constructor(nsGlobalWindow::Cast(win),
                                NS_LITERAL_STRING("error"), init);
      event->SetTrusted(true);

      EventDispatcher::DispatchDOMEvent(win, nullptr, event, presContext,
                                        &status);
    }

    if (status != nsEventStatus_eConsumeNoDefault) {
      JS::Rooted<JSObject*> stack(rootingCx,
        xpc::FindExceptionStackForConsoleReport(win, mError));
      mReport->LogToConsoleWithStack(stack);
    }

    return NS_OK;
  }

private:
  nsCOMPtr<nsPIDOMWindowInner>  mWindow;
  RefPtr<xpc::ErrorReport>      mReport;
  JS::PersistentRootedValue       mError;

  static bool sHandlingScriptError;
};

bool ScriptErrorEvent::sHandlingScriptError = false;

// This temporarily lives here to avoid code churn. It will go away entirely
// soon.
namespace xpc {

void
DispatchScriptErrorEvent(nsPIDOMWindowInner *win, JS::RootingContext* rootingCx,
                         xpc::ErrorReport *xpcReport, JS::Handle<JS::Value> exception)
{
  nsContentUtils::AddScriptRunner(new ScriptErrorEvent(win, rootingCx, xpcReport, exception));
}

} /* namespace xpc */

#ifdef DEBUG
// A couple of useful functions to call when you're debugging.
nsGlobalWindow *
JSObject2Win(JSObject *obj)
{
  return xpc::WindowOrNull(obj);
}

void
PrintWinURI(nsGlobalWindow *win)
{
  if (!win) {
    printf("No window passed in.\n");
    return;
  }

  nsCOMPtr<nsIDocument> doc = win->GetExtantDoc();
  if (!doc) {
    printf("No document in the window.\n");
    return;
  }

  nsIURI *uri = doc->GetDocumentURI();
  if (!uri) {
    printf("Document doesn't have a URI.\n");
    return;
  }

  printf("%s\n", uri->GetSpecOrDefault().get());
}

void
PrintWinCodebase(nsGlobalWindow *win)
{
  if (!win) {
    printf("No window passed in.\n");
    return;
  }

  nsIPrincipal *prin = win->GetPrincipal();
  if (!prin) {
    printf("Window doesn't have principals.\n");
    return;
  }

  nsCOMPtr<nsIURI> uri;
  prin->GetURI(getter_AddRefs(uri));
  if (!uri) {
    printf("No URI, maybe the system principal.\n");
    return;
  }

  printf("%s\n", uri->GetSpecOrDefault().get());
}

void
DumpString(const nsAString &str)
{
  printf("%s\n", NS_ConvertUTF16toUTF8(str).get());
}
#endif

#define JS_OPTIONS_DOT_STR "javascript.options."

static const char js_options_dot_str[]   = JS_OPTIONS_DOT_STR;

nsJSContext::nsJSContext(bool aGCOnDestruction,
                         nsIScriptGlobalObject* aGlobalObject)
  : mWindowProxy(nullptr)
  , mGCOnDestruction(aGCOnDestruction)
  , mGlobalObjectRef(aGlobalObject)
{
  EnsureStatics();

  ++sContextCount;

  mIsInitialized = false;
  mProcessingScriptTag = false;
  HoldJSObjects(this);
}

nsJSContext::~nsJSContext()
{
  mGlobalObjectRef = nullptr;

  Destroy();

  --sContextCount;

  if (!sContextCount && sDidShutdown) {
    // The last context is being deleted, and we're already in the
    // process of shutting down, release the security manager.

    NS_IF_RELEASE(sSecurityManager);
  }
}

void
nsJSContext::Destroy()
{
  if (mGCOnDestruction) {
    PokeGC(JS::gcreason::NSJSCONTEXT_DESTROY, mWindowProxy);
  }

  DropJSObjects(this);
}

// QueryInterface implementation for nsJSContext
NS_IMPL_CYCLE_COLLECTION_CLASS(nsJSContext)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSContext)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mWindowProxy)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSContext)
  tmp->mIsInitialized = false;
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
bool
AtomIsEventHandlerName(nsIAtom *aName)
{
  const char16_t *name = aName->GetUTF16String();

  const char16_t *cp;
  char16_t c;
  for (cp = name; *cp != '\0'; ++cp)
  {
    c = *cp;
    if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z'))
      return false;
  }

  return true;
}
#endif

nsIScriptGlobalObject *
nsJSContext::GetGlobalObject()
{
  // Note: this could probably be simplified somewhat more; see bug 974327
  // comments 1 and 3.
  if (!mWindowProxy) {
    return nullptr;
  }

  MOZ_ASSERT(mGlobalObjectRef);
  return mGlobalObjectRef;
}

nsresult
nsJSContext::InitContext()
{
  // Make sure callers of this use
  // WillInitializeContext/DidInitializeContext around this call.
  NS_ENSURE_TRUE(!mIsInitialized, NS_ERROR_ALREADY_INITIALIZED);

  // XXXbz Is there still a point to this function?
  return NS_OK;
}

nsresult
nsJSContext::SetProperty(JS::Handle<JSObject*> aTarget, const char* aPropName, nsISupports* aArgs)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(GetGlobalObject()))) {
    return NS_ERROR_FAILURE;
  }
  JSContext* cx = jsapi.cx();

  JS::AutoValueVector args(cx);

  JS::Rooted<JSObject*> global(cx, GetWindowProxy());
  nsresult rv =
    ConvertSupportsTojsvals(aArgs, global, args);
  NS_ENSURE_SUCCESS(rv, rv);

  // got the arguments, now attach them.

  for (uint32_t i = 0; i < args.length(); ++i) {
    if (!JS_WrapValue(cx, args[i])) {
      return NS_ERROR_FAILURE;
    }
  }

  JS::Rooted<JSObject*> array(cx, ::JS_NewArrayObject(cx, args));
  if (!array) {
    return NS_ERROR_FAILURE;
  }

  return JS_DefineProperty(cx, aTarget, aPropName, array, 0) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsJSContext::ConvertSupportsTojsvals(nsISupports* aArgs,
                                     JS::Handle<JSObject*> aScope,
                                     JS::AutoValueVector& aArgsOut)
{
  nsresult rv = NS_OK;

  // If the array implements nsIJSArgArray, copy the contents and return.
  nsCOMPtr<nsIJSArgArray> fastArray = do_QueryInterface(aArgs);
  if (fastArray) {
    uint32_t argc;
    JS::Value* argv;
    rv = fastArray->GetArgs(&argc, reinterpret_cast<void **>(&argv));
    if (NS_SUCCEEDED(rv) && !aArgsOut.append(argv, argc)) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
    return rv;
  }

  // Take the slower path converting each item.
  // Handle only nsIArray and nsIVariant.  nsIArray is only needed for
  // SetProperty('arguments', ...);

  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  NS_ENSURE_TRUE(xpc, NS_ERROR_UNEXPECTED);
  AutoJSContext cx;

  if (!aArgs)
    return NS_OK;
  uint32_t argCount;
  // This general purpose function may need to convert an arg array
  // (window.arguments, event-handler args) and a generic property.
  nsCOMPtr<nsIArray> argsArray(do_QueryInterface(aArgs));

  if (argsArray) {
    rv = argsArray->GetLength(&argCount);
    NS_ENSURE_SUCCESS(rv, rv);
    if (argCount == 0)
      return NS_OK;
  } else {
    argCount = 1; // the nsISupports which is not an array
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
        rv = xpc->VariantToJS(cx, aScope, variant, thisVal);
      } else {
        // And finally, support the nsISupportsPrimitives supplied
        // by the AppShell.  It generally will pass only strings, but
        // as we have code for handling all, we may as well use it.
        rv = AddSupportsPrimitiveTojsvals(arg, thisVal.address());
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
          JSAutoCompartment ac(cx, aScope);
          rv = nsContentUtils::WrapNative(cx, arg, thisVal);
        }
      }
    }
  } else {
    nsCOMPtr<nsIVariant> variant = do_QueryInterface(aArgs);
    if (variant) {
      rv = xpc->VariantToJS(cx, aScope, variant, aArgsOut[0]);
    } else {
      NS_ERROR("Not an array, not an interface?");
      rv = NS_ERROR_UNEXPECTED;
    }
  }
  return rv;
}

// This really should go into xpconnect somewhere...
nsresult
nsJSContext::AddSupportsPrimitiveTojsvals(nsISupports *aArg, JS::Value *aArgv)
{
  NS_PRECONDITION(aArg, "Empty arg");

  nsCOMPtr<nsISupportsPrimitive> argPrimitive(do_QueryInterface(aArg));
  if (!argPrimitive)
    return NS_ERROR_NO_INTERFACE;

  AutoJSContext cx;
  uint16_t type;
  argPrimitive->GetType(&type);

  switch(type) {
    case nsISupportsPrimitive::TYPE_CSTRING : {
      nsCOMPtr<nsISupportsCString> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      nsAutoCString data;

      p->GetData(data);


      JSString *str = ::JS_NewStringCopyN(cx, data.get(), data.Length());
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      aArgv->setString(str);

      break;
    }
    case nsISupportsPrimitive::TYPE_STRING : {
      nsCOMPtr<nsISupportsString> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      nsAutoString data;

      p->GetData(data);

      // cast is probably safe since wchar_t and char16_t are expected
      // to be equivalent; both unsigned 16-bit entities
      JSString *str =
        ::JS_NewUCStringCopyN(cx, data.get(), data.Length());
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      aArgv->setString(str);
      break;
    }
    case nsISupportsPrimitive::TYPE_PRBOOL : {
      nsCOMPtr<nsISupportsPRBool> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      bool data;

      p->GetData(&data);

      aArgv->setBoolean(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT8 : {
      nsCOMPtr<nsISupportsPRUint8> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      uint8_t data;

      p->GetData(&data);

      aArgv->setInt32(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT16 : {
      nsCOMPtr<nsISupportsPRUint16> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      uint16_t data;

      p->GetData(&data);

      aArgv->setInt32(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT32 : {
      nsCOMPtr<nsISupportsPRUint32> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      uint32_t data;

      p->GetData(&data);

      aArgv->setInt32(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_CHAR : {
      nsCOMPtr<nsISupportsChar> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      char data;

      p->GetData(&data);

      JSString *str = ::JS_NewStringCopyN(cx, &data, 1);
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      aArgv->setString(str);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRINT16 : {
      nsCOMPtr<nsISupportsPRInt16> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      int16_t data;

      p->GetData(&data);

      aArgv->setInt32(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRINT32 : {
      nsCOMPtr<nsISupportsPRInt32> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      int32_t data;

      p->GetData(&data);

      aArgv->setInt32(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_FLOAT : {
      nsCOMPtr<nsISupportsFloat> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      float data;

      p->GetData(&data);

      *aArgv = ::JS_NumberValue(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_DOUBLE : {
      nsCOMPtr<nsISupportsDouble> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      double data;

      p->GetData(&data);

      *aArgv = ::JS_NumberValue(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_INTERFACE_POINTER : {
      nsCOMPtr<nsISupportsInterfacePointer> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      nsCOMPtr<nsISupports> data;
      nsIID *iid = nullptr;

      p->GetData(getter_AddRefs(data));
      p->GetDataIID(&iid);
      NS_ENSURE_TRUE(iid, NS_ERROR_UNEXPECTED);

      AutoFree iidGuard(iid); // Free iid upon destruction.

      JS::Rooted<JSObject*> scope(cx, GetWindowProxy());
      JS::Rooted<JS::Value> v(cx);
      JSAutoCompartment ac(cx, scope);
      nsresult rv = nsContentUtils::WrapNative(cx, data, iid, &v);
      NS_ENSURE_SUCCESS(rv, rv);

      *aArgv = v;

      break;
    }
    case nsISupportsPrimitive::TYPE_ID :
    case nsISupportsPrimitive::TYPE_PRUINT64 :
    case nsISupportsPrimitive::TYPE_PRINT64 :
    case nsISupportsPrimitive::TYPE_PRTIME : {
      NS_WARNING("Unsupported primitive type used");
      aArgv->setNull();
      break;
    }
    default : {
      NS_WARNING("Unknown primitive type used");
      aArgv->setNull();
      break;
    }
  }
  return NS_OK;
}

#ifdef MOZ_JPROF

#include <signal.h>

inline bool
IsJProfAction(struct sigaction *action)
{
    return (action->sa_sigaction &&
            (action->sa_flags & (SA_RESTART | SA_SIGINFO)) == (SA_RESTART | SA_SIGINFO));
}

void NS_JProfStartProfiling();
void NS_JProfStopProfiling();
void NS_JProfClearCircular();

static bool
JProfStartProfilingJS(JSContext *cx, unsigned argc, JS::Value *vp)
{
  NS_JProfStartProfiling();
  return true;
}

void NS_JProfStartProfiling()
{
    // Figure out whether we're dealing with SIGPROF, SIGALRM, or
    // SIGPOLL profiling (SIGALRM for JP_REALTIME, SIGPOLL for
    // JP_RTC_HZ)
    struct sigaction action;

    // Must check ALRM before PROF since both are enabled for real-time
    sigaction(SIGALRM, nullptr, &action);
    //printf("SIGALRM: %p, flags = %x\n",action.sa_sigaction,action.sa_flags);
    if (IsJProfAction(&action)) {
        //printf("Beginning real-time jprof profiling.\n");
        raise(SIGALRM);
        return;
    }

    sigaction(SIGPROF, nullptr, &action);
    //printf("SIGPROF: %p, flags = %x\n",action.sa_sigaction,action.sa_flags);
    if (IsJProfAction(&action)) {
        //printf("Beginning process-time jprof profiling.\n");
        raise(SIGPROF);
        return;
    }

    sigaction(SIGPOLL, nullptr, &action);
    //printf("SIGPOLL: %p, flags = %x\n",action.sa_sigaction,action.sa_flags);
    if (IsJProfAction(&action)) {
        //printf("Beginning rtc-based jprof profiling.\n");
        raise(SIGPOLL);
        return;
    }

    printf("Could not start jprof-profiling since JPROF_FLAGS was not set.\n");
}

static bool
JProfStopProfilingJS(JSContext *cx, unsigned argc, JS::Value *vp)
{
  NS_JProfStopProfiling();
  return true;
}

void
NS_JProfStopProfiling()
{
    raise(SIGUSR1);
    //printf("Stopped jprof profiling.\n");
}

static bool
JProfClearCircularJS(JSContext *cx, unsigned argc, JS::Value *vp)
{
  NS_JProfClearCircular();
  return true;
}

void
NS_JProfClearCircular()
{
    raise(SIGUSR2);
    //printf("cleared jprof buffer\n");
}

static bool
JProfSaveCircularJS(JSContext *cx, unsigned argc, JS::Value *vp)
{
  // Not ideal...
  NS_JProfStopProfiling();
  NS_JProfStartProfiling();
  return true;
}

static const JSFunctionSpec JProfFunctions[] = {
    JS_FS("JProfStartProfiling",        JProfStartProfilingJS,      0, 0),
    JS_FS("JProfStopProfiling",         JProfStopProfilingJS,       0, 0),
    JS_FS("JProfClearCircular",         JProfClearCircularJS,       0, 0),
    JS_FS("JProfSaveCircular",          JProfSaveCircularJS,        0, 0),
    JS_FS_END
};

#endif /* defined(MOZ_JPROF) */

nsresult
nsJSContext::InitClasses(JS::Handle<JSObject*> aGlobalObj)
{
  AutoJSAPI jsapi;
  jsapi.Init();
  JSContext* cx = jsapi.cx();
  JSAutoCompartment ac(cx, aGlobalObj);

  // Attempt to initialize profiling functions
  ::JS_DefineProfilingFunctions(cx, aGlobalObj);

#ifdef MOZ_JPROF
  // Attempt to initialize JProf functions
  ::JS_DefineFunctions(cx, aGlobalObj, JProfFunctions);
#endif

  return NS_OK;
}

void
nsJSContext::WillInitializeContext()
{
  mIsInitialized = false;
}

void
nsJSContext::DidInitializeContext()
{
  mIsInitialized = true;
}

bool
nsJSContext::IsContextInitialized()
{
  return mIsInitialized;
}

bool
nsJSContext::GetProcessingScriptTag()
{
  return mProcessingScriptTag;
}

void
nsJSContext::SetProcessingScriptTag(bool aFlag)
{
  mProcessingScriptTag = aFlag;
}

void
FullGCTimerFired(nsITimer* aTimer, void* aClosure)
{
  nsJSContext::KillFullGCTimer();
  MOZ_ASSERT(!aClosure, "Don't pass a closure to FullGCTimerFired");
  nsJSContext::GarbageCollectNow(JS::gcreason::FULL_GC_TIMER,
                                 nsJSContext::IncrementalGC);
}

//static
void
nsJSContext::GarbageCollectNow(JS::gcreason::Reason aReason,
                               IsIncremental aIncremental,
                               IsShrinking aShrinking,
                               int64_t aSliceMillis)
{
  PROFILER_LABEL_DYNAMIC("nsJSContext", "GarbageCollectNow",
                         js::ProfileEntry::Category::GC,
                         JS::gcreason::ExplainReason(aReason));

  MOZ_ASSERT_IF(aSliceMillis, aIncremental == IncrementalGC);

  KillGCTimer();

  // Reset sPendingLoadCount in case the timer that fired was a
  // timer we scheduled due to a normal GC timer firing while
  // documents were loading. If this happens we're waiting for a
  // document that is taking a long time to load, and we effectively
  // ignore the fact that the currently loading documents are still
  // loading and move on as if they weren't.
  sPendingLoadCount = 0;
  sLoadingInProgress = false;

  // We use danger::GetJSContext() since AutoJSAPI will assert if the current
  // thread's context is null (such as during shutdown).
  JSContext* cx = danger::GetJSContext();

  if (!nsContentUtils::XPConnect() || !cx) {
    return;
  }

  if (sCCLockedOut && aIncremental == IncrementalGC) {
    // We're in the middle of incremental GC. Do another slice.
    JS::PrepareForIncrementalGC(cx);
    JS::IncrementalGCSlice(cx, aReason, aSliceMillis);
    return;
  }

  JSGCInvocationKind gckind = aShrinking == ShrinkingGC ? GC_SHRINK : GC_NORMAL;

  if (aIncremental == NonIncrementalGC || aReason == JS::gcreason::FULL_GC_TIMER) {
    sNeedsFullGC = true;
  }

  if (sNeedsFullGC) {
    JS::PrepareForFullGC(cx);
  } else {
    CycleCollectedJSRuntime::Get()->PrepareWaitingZonesForGC();
  }

  if (aIncremental == IncrementalGC) {
    JS::StartIncrementalGC(cx, gckind, aReason, aSliceMillis);
  } else {
    JS::GCForReason(cx, gckind, aReason);
  }
}

static void
FinishAnyIncrementalGC()
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::GC);

  if (sCCLockedOut) {
    AutoJSAPI jsapi;
    jsapi.Init();

    // We're in the middle of an incremental GC, so finish it.
    JS::PrepareForIncrementalGC(jsapi.cx());
    JS::FinishIncrementalGC(jsapi.cx(), JS::gcreason::CC_FORCED);
  }
}

static void
FireForgetSkippable(uint32_t aSuspected, bool aRemoveChildless,
                    TimeStamp aDeadline)
{
  AutoProfilerTracing
    tracing("CC", aDeadline.IsNull() ? "ForgetSkippable" : "IdleForgetSkippable");
  PRTime startTime = PR_Now();
  TimeStamp startTimeStamp = TimeStamp::Now();
  FinishAnyIncrementalGC();
  bool earlyForgetSkippable =
    sCleanupsSinceLastGC < NS_MAJOR_FORGET_SKIPPABLE_CALLS;
  nsCycleCollector_forgetSkippable(aRemoveChildless, earlyForgetSkippable);
  sPreviousSuspectedCount = nsCycleCollector_suspectedCount();
  ++sCleanupsSinceLastGC;
  PRTime delta = PR_Now() - startTime;
  if (sMinForgetSkippableTime > delta) {
    sMinForgetSkippableTime = delta;
  }
  if (sMaxForgetSkippableTime < delta) {
    sMaxForgetSkippableTime = delta;
  }
  sTotalForgetSkippableTime += delta;
  sRemovedPurples += (aSuspected - sPreviousSuspectedCount);
  ++sForgetSkippableBeforeCC;

  TimeStamp now = TimeStamp::Now();
  TimeDuration duration = now - startTimeStamp;
  if (duration.ToSeconds()) {
    TimeDuration idleDuration;
    if (!aDeadline.IsNull()) {
      if (aDeadline < now) {
        // This slice overflowed the idle period.
        idleDuration = aDeadline - startTimeStamp;
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
static uint32_t
TimeBetween(TimeStamp start, TimeStamp end)
{
  MOZ_ASSERT(end >= start);
  return (uint32_t) ((end - start).ToMilliseconds());
}

static uint32_t
TimeUntilNow(TimeStamp start)
{
  if (start.IsNull()) {
    return 0;
  }
  return TimeBetween(start, TimeStamp::Now());
}

struct CycleCollectorStats
{
  constexpr CycleCollectorStats() :
    mMaxGCDuration(0), mRanSyncForgetSkippable(false), mSuspected(0),
    mMaxSkippableDuration(0), mMaxSliceTime(0), mMaxSliceTimeSinceClear(0),
    mTotalSliceTime(0), mAnyLockedOut(false), mExtraForgetSkippableCalls(0),
    mFile(nullptr) {}

  void Init()
  {
    Clear();
    mMaxSliceTimeSinceClear = 0;

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

  void Clear()
  {
    if (mFile && mFile != stdout && mFile != stderr) {
      fclose(mFile);
    }
    mBeginSliceTime = TimeStamp();
    mEndSliceTime = TimeStamp();
    mBeginTime = TimeStamp();
    mMaxGCDuration = 0;
    mRanSyncForgetSkippable = false;
    mSuspected = 0;
    mMaxSkippableDuration = 0;
    mMaxSliceTime = 0;
    mTotalSliceTime = 0;
    mAnyLockedOut = false;
    mExtraForgetSkippableCalls = 0;
  }

  void PrepareForCycleCollectionSlice(TimeStamp aDeadline = TimeStamp(),
                                      int32_t aExtraForgetSkippableCalls = 0);

  void FinishCycleCollectionSlice()
  {
    if (mBeginSliceTime.IsNull()) {
      // We already called this method from EndCycleCollectionCallback for this slice.
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

    uint32_t sliceTime = TimeBetween(mBeginSliceTime, mEndSliceTime);
    mMaxSliceTime = std::max(mMaxSliceTime, sliceTime);
    mMaxSliceTimeSinceClear = std::max(mMaxSliceTimeSinceClear, sliceTime);
    mTotalSliceTime += sliceTime;
    mBeginSliceTime = TimeStamp();
    MOZ_ASSERT(mExtraForgetSkippableCalls == 0, "Forget to reset extra forget skippable calls?");
  }

  void RunForgetSkippable();

  // Time the current slice began, including any GC finishing.
  TimeStamp mBeginSliceTime;

  // Time the previous slice of the current CC ended.
  TimeStamp mEndSliceTime;

  // Time the current cycle collection began.
  TimeStamp mBeginTime;

  // The longest GC finishing duration for any slice of the current CC.
  uint32_t mMaxGCDuration;

  // True if we ran sync forget skippable in any slice of the current CC.
  bool mRanSyncForgetSkippable;

  // Number of suspected objects at the start of the current CC.
  uint32_t mSuspected;

  // The longest duration spent on sync forget skippable in any slice of the
  // current CC.
  uint32_t mMaxSkippableDuration;

  // The longest pause of any slice in the current CC.
  uint32_t mMaxSliceTime;

  // The longest slice time since ClearMaxCCSliceTime() was called.
  uint32_t mMaxSliceTimeSinceClear;

  // The total amount of time spent actually running the current CC.
  uint32_t mTotalSliceTime;

  // True if we were locked out by the GC in any slice of the current CC.
  bool mAnyLockedOut;

  int32_t mExtraForgetSkippableCalls;

  // A file to dump CC activity to; set by MOZ_CCTIMER environment variable.
  FILE* mFile;

  // In case CC slice was triggered during idle time, set to the end of the idle
  // period.
  TimeStamp mIdleDeadline;
};

CycleCollectorStats gCCStats;

void
CycleCollectorStats::PrepareForCycleCollectionSlice(TimeStamp aDeadline,
                                                    int32_t aExtraForgetSkippableCalls)
{
  mBeginSliceTime = TimeStamp::Now();
  mIdleDeadline = aDeadline;

  // Before we begin the cycle collection, make sure there is no active GC.
  if (sCCLockedOut) {
    mAnyLockedOut = true;
    FinishAnyIncrementalGC();
    uint32_t gcTime = TimeBetween(mBeginSliceTime, TimeStamp::Now());
    mMaxGCDuration = std::max(mMaxGCDuration, gcTime);
  }

  mExtraForgetSkippableCalls = aExtraForgetSkippableCalls;
}

void
CycleCollectorStats::RunForgetSkippable()
{
  // Run forgetSkippable synchronously to reduce the size of the CC graph. This
  // is particularly useful if we recently finished a GC.
  if (mExtraForgetSkippableCalls >= 0) {
    TimeStamp beginForgetSkippable = TimeStamp::Now();
    bool ranSyncForgetSkippable = false;
    while (sCleanupsSinceLastGC < NS_MAJOR_FORGET_SKIPPABLE_CALLS) {
      FireForgetSkippable(nsCycleCollector_suspectedCount(), false, TimeStamp());
      ranSyncForgetSkippable = true;
    }

    for (int32_t i = 0; i < mExtraForgetSkippableCalls; ++i) {
      FireForgetSkippable(nsCycleCollector_suspectedCount(), false, TimeStamp());
      ranSyncForgetSkippable = true;
    }

    if (ranSyncForgetSkippable) {
      mMaxSkippableDuration =
        std::max(mMaxSkippableDuration, TimeUntilNow(beginForgetSkippable));
      mRanSyncForgetSkippable = true;
    }

  }
  mExtraForgetSkippableCalls = 0;
}

//static
void
nsJSContext::CycleCollectNow(nsICycleCollectorListener *aListener,
                             int32_t aExtraForgetSkippableCalls)
{
  if (!NS_IsMainThread()) {
    return;
  }

  PROFILER_LABEL("nsJSContext", "CycleCollectNow",
    js::ProfileEntry::Category::CC);

  gCCStats.PrepareForCycleCollectionSlice(TimeStamp(),
                                          aExtraForgetSkippableCalls);
  nsCycleCollector_collect(aListener);
  gCCStats.FinishCycleCollectionSlice();
}

//static
void
nsJSContext::RunCycleCollectorSlice(TimeStamp aDeadline)
{
  if (!NS_IsMainThread()) {
    return;
  }

  AutoProfilerTracing
    tracing("CC", aDeadline.IsNull() ? "CCSlice" : "IdleCCSlice");

  PROFILER_LABEL("nsJSContext", "RunCycleCollectorSlice",
    js::ProfileEntry::Category::CC);

  gCCStats.PrepareForCycleCollectionSlice(aDeadline);

  // Decide how long we want to budget for this slice. By default,
  // use an unlimited budget.
  js::SliceBudget budget = js::SliceBudget::unlimited();

  if (sIncrementalCC) {
    int64_t baseBudget = kICCSliceBudget;
    if (!aDeadline.IsNull()) {
      baseBudget = int64_t((aDeadline - TimeStamp::Now()).ToMilliseconds());
    }

    if (gCCStats.mBeginTime.IsNull()) {
      // If no CC is in progress, use the standard slice time.
      budget = js::SliceBudget(js::TimeBudget(baseBudget));
    } else {
      TimeStamp now = TimeStamp::Now();

      // Only run a limited slice if we're within the max running time.
      uint32_t runningTime = TimeBetween(gCCStats.mBeginTime, now);
      if (runningTime < kMaxICCDuration) {
        const float maxSlice = MainThreadIdlePeriod::GetLongIdlePeriod();

        // Try to make up for a delay in running this slice.
        float sliceDelayMultiplier =
          TimeBetween(gCCStats.mEndSliceTime, now) / (float)kICCIntersliceDelay;
        float delaySliceBudget =
          std::min(baseBudget * sliceDelayMultiplier, maxSlice);

        // Increase slice budgets up to |maxSlice| as we approach
        // half way through the ICC, to avoid large sync CCs.
        float percentToHalfDone = std::min(2.0f * runningTime / kMaxICCDuration, 1.0f);
        float laterSliceBudget = maxSlice * percentToHalfDone;

        budget = js::SliceBudget(js::TimeBudget(std::max({delaySliceBudget,
                  laterSliceBudget, (float)baseBudget})));
      }
    }
  }

  nsCycleCollector_collectSlice(budget);

  gCCStats.FinishCycleCollectionSlice();
}

//static
void
nsJSContext::RunCycleCollectorWorkSlice(int64_t aWorkBudget)
{
  if (!NS_IsMainThread()) {
    return;
  }

  PROFILER_LABEL("nsJSContext", "RunCycleCollectorWorkSlice",
    js::ProfileEntry::Category::CC);

  gCCStats.PrepareForCycleCollectionSlice();

  js::SliceBudget budget = js::SliceBudget(js::WorkBudget(aWorkBudget));
  nsCycleCollector_collectSlice(budget);

  gCCStats.FinishCycleCollectionSlice();
}

void
nsJSContext::ClearMaxCCSliceTime()
{
  gCCStats.mMaxSliceTimeSinceClear = 0;
}

uint32_t
nsJSContext::GetMaxCCSliceTimeSinceClear()
{
  return gCCStats.mMaxSliceTimeSinceClear;
}

static bool
ICCRunnerFired(TimeStamp aDeadline, void* aData)
{
  if (sDidShutdown) {
    return false;
  }

  // Ignore ICC timer fires during IGC. Running ICC during an IGC will cause us
  // to synchronously finish the GC, which is bad.

  if (sCCLockedOut) {
    PRTime now = PR_Now();
    if (sCCLockedOutTime == 0) {
      sCCLockedOutTime = now;
      return false;
    }
    if (now - sCCLockedOutTime < NS_MAX_CC_LOCKEDOUT_TIME) {
      return false;
    }
  }

  nsJSContext::RunCycleCollectorSlice(aDeadline);
  return true;
}

//static
void
nsJSContext::BeginCycleCollectionCallback()
{
  MOZ_ASSERT(NS_IsMainThread());

  gCCStats.mBeginTime = gCCStats.mBeginSliceTime.IsNull() ? TimeStamp::Now() : gCCStats.mBeginSliceTime;
  gCCStats.mSuspected = nsCycleCollector_suspectedCount();

  KillCCRunner();

  gCCStats.RunForgetSkippable();

  MOZ_ASSERT(!sICCRunner, "Tried to create a new ICC timer when one already existed.");

  // Create an ICC timer even if ICC is globally disabled, because we could be manually triggering
  // an incremental collection, and we want to be sure to finish it.
  sICCRunner = CollectorRunner::Create(ICCRunnerFired, kICCIntersliceDelay,
                                       kIdleICCSliceBudget, true);
}

static_assert(NS_GC_DELAY > kMaxICCDuration, "A max duration ICC shouldn't reduce GC delay to 0");

//static
void
nsJSContext::EndCycleCollectionCallback(CycleCollectorResults &aResults)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsJSContext::KillICCRunner();

  // Update timing information for the current slice before we log it, if
  // we previously called PrepareForCycleCollectionSlice(). During shutdown
  // CCs, this won't happen.
  gCCStats.FinishCycleCollectionSlice();

  sCCollectedWaitingForGC += aResults.mFreedGCed;
  sCCollectedZonesWaitingForGC += aResults.mFreedJSZones;

  TimeStamp endCCTimeStamp = TimeStamp::Now();
  uint32_t ccNowDuration = TimeBetween(gCCStats.mBeginTime, endCCTimeStamp);

  if (NeedsGCAfterCC()) {
    PokeGC(JS::gcreason::CC_WAITING, nullptr,
           NS_GC_DELAY - std::min(ccNowDuration, kMaxICCDuration));
  }

  // Log information about the CC via telemetry, JSON and the console.
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_FINISH_IGC, gCCStats.mAnyLockedOut);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_SYNC_SKIPPABLE, gCCStats.mRanSyncForgetSkippable);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_FULL, ccNowDuration);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_MAX_PAUSE, gCCStats.mMaxSliceTime);

  if (!sLastCCEndTime.IsNull()) {
    // TimeBetween returns milliseconds, but we want to report seconds.
    uint32_t timeBetween = TimeBetween(sLastCCEndTime, gCCStats.mBeginTime) / 1000;
    Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_TIME_BETWEEN, timeBetween);
  }
  sLastCCEndTime = endCCTimeStamp;

  Telemetry::Accumulate(Telemetry::FORGET_SKIPPABLE_MAX,
                        sMaxForgetSkippableTime / PR_USEC_PER_MSEC);

  PRTime delta = GetCollectionTimeDelta();

  uint32_t cleanups = sForgetSkippableBeforeCC ? sForgetSkippableBeforeCC : 1;
  uint32_t minForgetSkippableTime = (sMinForgetSkippableTime == UINT32_MAX)
    ? 0 : sMinForgetSkippableTime;

  if (sPostGCEventsToConsole || gCCStats.mFile) {
    nsCString mergeMsg;
    if (aResults.mMergedZones) {
      mergeMsg.AssignLiteral(" merged");
    }

    nsCString gcMsg;
    if (aResults.mForcedGC) {
      gcMsg.AssignLiteral(", forced a GC");
    }

    const char16_t *kFmt =
      u"CC(T+%.1f)[%s-%i] max pause: %lums, total time: %lums, slices: %lu, suspected: %lu, visited: %lu RCed and %lu%s GCed, collected: %lu RCed and %lu GCed (%lu|%lu|%lu waiting for GC)%s\n"
      u"ForgetSkippable %lu times before CC, min: %lu ms, max: %lu ms, avg: %lu ms, total: %lu ms, max sync: %lu ms, removed: %lu";
    nsString msg;
    msg.Adopt(nsTextFormatter::smprintf(kFmt, double(delta) / PR_USEC_PER_SEC,
                                        ProcessNameForCollectorLog(), getpid(),
                                        gCCStats.mMaxSliceTime, gCCStats.mTotalSliceTime,
                                        aResults.mNumSlices, gCCStats.mSuspected,
                                        aResults.mVisitedRefCounted, aResults.mVisitedGCed, mergeMsg.get(),
                                        aResults.mFreedRefCounted, aResults.mFreedGCed,
                                        sCCollectedWaitingForGC, sCCollectedZonesWaitingForGC, sLikelyShortLivingObjectsNeedingGC,
                                        gcMsg.get(),
                                        sForgetSkippableBeforeCC,
                                        minForgetSkippableTime / PR_USEC_PER_MSEC,
                                        sMaxForgetSkippableTime / PR_USEC_PER_MSEC,
                                        (sTotalForgetSkippableTime / cleanups) /
                                          PR_USEC_PER_MSEC,
                                        sTotalForgetSkippableTime / PR_USEC_PER_MSEC,
                                        gCCStats.mMaxSkippableDuration, sRemovedPurples));
    if (sPostGCEventsToConsole) {
      nsCOMPtr<nsIConsoleService> cs =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);
      if (cs) {
        cs->LogStringMessage(msg.get());
      }
    }
    if (gCCStats.mFile) {
      fprintf(gCCStats.mFile, "%s\n", NS_ConvertUTF16toUTF8(msg).get());
    }
  }

  if (sPostGCEventsToObserver) {
    const char16_t* kJSONFmt =
       u"{ \"timestamp\": %llu, "
         u"\"duration\": %lu, "
         u"\"max_slice_pause\": %lu, "
         u"\"total_slice_pause\": %lu, "
         u"\"max_finish_gc_duration\": %lu, "
         u"\"max_sync_skippable_duration\": %lu, "
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
             u"\"min\": %lu, "
             u"\"max\": %lu, "
             u"\"avg\": %lu, "
             u"\"total\": %lu, "
             u"\"removed\": %lu } "
       u"}";
    nsString json;

    json.Adopt(nsTextFormatter::smprintf(kJSONFmt, PR_Now(), ccNowDuration,
                                         gCCStats.mMaxSliceTime,
                                         gCCStats.mTotalSliceTime,
                                         gCCStats.mMaxGCDuration,
                                         gCCStats.mMaxSkippableDuration,
                                         gCCStats.mSuspected,
                                         aResults.mVisitedRefCounted, aResults.mVisitedGCed,
                                         aResults.mFreedRefCounted, aResults.mFreedGCed,
                                         sCCollectedWaitingForGC,
                                         sCCollectedZonesWaitingForGC,
                                         sLikelyShortLivingObjectsNeedingGC,
                                         aResults.mForcedGC,
                                         sForgetSkippableBeforeCC,
                                         minForgetSkippableTime / PR_USEC_PER_MSEC,
                                         sMaxForgetSkippableTime / PR_USEC_PER_MSEC,
                                         (sTotalForgetSkippableTime / cleanups) /
                                           PR_USEC_PER_MSEC,
                                         sTotalForgetSkippableTime / PR_USEC_PER_MSEC,
                                         sRemovedPurples));
    nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(nullptr, "cycle-collection-statistics", json.get());
    }
  }

  // Update global state to indicate we have just run a cycle collection.
  sMinForgetSkippableTime = UINT32_MAX;
  sMaxForgetSkippableTime = 0;
  sTotalForgetSkippableTime = 0;
  sRemovedPurples = 0;
  sForgetSkippableBeforeCC = 0;
  sNeedsFullCC = false;
  sNeedsGCAfterCC = false;
  gCCStats.Clear();
}

// static
bool
InterSliceGCRunnerFired(TimeStamp aDeadline, void* aData)
{
  nsJSContext::KillInterSliceGCRunner();
  MOZ_ASSERT(sActiveIntersliceGCBudget > 0);
  // We use longer budgets when timer runs since that means
  // there hasn't been idle time recently and we may have significant amount
  // garbage to collect.
  int64_t budget = sActiveIntersliceGCBudget * 2;
  if (!aDeadline.IsNull()) {
    budget = int64_t((aDeadline - TimeStamp::Now()).ToMilliseconds());
  }

  TimeStamp startTimeStamp = TimeStamp::Now();
  TimeDuration duration = sGCUnnotifiedTotalTime;
  uintptr_t reason = reinterpret_cast<uintptr_t>(aData);
  nsJSContext::GarbageCollectNow(aData ?
                                   static_cast<JS::gcreason::Reason>(reason) :
                                   JS::gcreason::INTER_SLICE_GC,
                                 nsJSContext::IncrementalGC,
                                 nsJSContext::NonShrinkingGC,
                                 budget);

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
  return true;
}

// static
void
GCTimerFired(nsITimer *aTimer, void *aClosure)
{
  nsJSContext::KillGCTimer();
  // Now start the actual GC after initial timer has fired.
  sInterSliceGCRunner = CollectorRunner::Create(InterSliceGCRunnerFired,
                                                NS_INTERSLICE_GC_DELAY,
                                                sActiveIntersliceGCBudget,
                                                false,
                                                aClosure);
}

// static
void
ShrinkingGCTimerFired(nsITimer* aTimer, void* aClosure)
{
  nsJSContext::KillShrinkingGCTimer();
  sIsCompactingOnUserInactive = true;
  nsJSContext::GarbageCollectNow(JS::gcreason::USER_INACTIVE,
                                 nsJSContext::IncrementalGC,
                                 nsJSContext::ShrinkingGC);
}

static bool
ShouldTriggerCC(uint32_t aSuspected)
{
  return sNeedsFullCC ||
         aSuspected > NS_CC_PURPLE_LIMIT ||
         (aSuspected > NS_CC_FORCED_PURPLE_LIMIT &&
          TimeUntilNow(sLastCCEndTime) > NS_CC_FORCED);
}

static bool
CCRunnerFired(TimeStamp aDeadline, void* aData)
{
  if (sDidShutdown) {
    return false;
  }

  static uint32_t ccDelay = NS_CC_DELAY;
  if (sCCLockedOut) {
    ccDelay = NS_CC_DELAY / 3;

    PRTime now = PR_Now();
    if (sCCLockedOutTime == 0) {
      // Reset sCCRunnerFireCount so that we run forgetSkippable
      // often enough before CC. Because of reduced ccDelay
      // forgetSkippable will be called just a few times.
      // NS_MAX_CC_LOCKEDOUT_TIME limit guarantees that we end up calling
      // forgetSkippable and CycleCollectNow eventually.
      sCCRunnerFireCount = 0;
      sCCLockedOutTime = now;
      return false;
    }
    if (now - sCCLockedOutTime < NS_MAX_CC_LOCKEDOUT_TIME) {
      return false;
    }
  }

  ++sCCRunnerFireCount;

  bool didDoWork = false;

  // During early timer fires, we only run forgetSkippable. During the first
  // late timer fire, we decide if we are going to have a second and final
  // late timer fire, where we may begin to run the CC. Should run at least one
  // early timer fire to allow cleanup before the CC.
  int32_t numEarlyTimerFires = std::max((int32_t)ccDelay / NS_CC_SKIPPABLE_DELAY - 2, 1);
  bool isLateTimerFire = sCCRunnerFireCount > numEarlyTimerFires;
  uint32_t suspected = nsCycleCollector_suspectedCount();
  if (isLateTimerFire && ShouldTriggerCC(suspected)) {
    if (sCCRunnerFireCount == numEarlyTimerFires + 1) {
      FireForgetSkippable(suspected, true, aDeadline);
      didDoWork = true;
      if (ShouldTriggerCC(nsCycleCollector_suspectedCount())) {
        // Our efforts to avoid a CC have failed, so we return to let the
        // timer fire once more to trigger a CC.
        return didDoWork;
      }
    } else {
      // We are in the final timer fire and still meet the conditions for
      // triggering a CC. Let RunCycleCollectorSlice finish the current IGC, if
      // any because that will allow us to include the GC time in the CC pause.
      nsJSContext::RunCycleCollectorSlice(aDeadline);
      didDoWork = true;
    }
  } else if (((sPreviousSuspectedCount + 100) <= suspected) ||
             (sCleanupsSinceLastGC < NS_MAJOR_FORGET_SKIPPABLE_CALLS)) {
      // Only do a forget skippable if there are more than a few new objects
      // or we're doing the initial forget skippables.
      FireForgetSkippable(suspected, false, aDeadline);
      didDoWork = true;
  }

  if (isLateTimerFire) {
    ccDelay = NS_CC_DELAY;

    // We have either just run the CC or decided we don't want to run the CC
    // next time, so kill the timer.
    sPreviousSuspectedCount = 0;
    nsJSContext::KillCCRunner();
  }

  return didDoWork;
}

// static
uint32_t
nsJSContext::CleanupsSinceLastGC()
{
  return sCleanupsSinceLastGC;
}

// static
void
nsJSContext::LoadStart()
{
  sLoadingInProgress = true;
  ++sPendingLoadCount;
}

// static
void
nsJSContext::LoadEnd()
{
  if (!sLoadingInProgress)
    return;

  // sPendingLoadCount is not a well managed load counter (and doesn't
  // need to be), so make sure we don't make it wrap backwards here.
  if (sPendingLoadCount > 0) {
    --sPendingLoadCount;
    return;
  }

  sLoadingInProgress = false;
}

// Only trigger expensive timers when they have been checked a number of times.
static bool
ReadyToTriggerExpensiveCollectorTimer()
{
  bool ready = kPokesBetweenExpensiveCollectorTriggers < ++sExpensiveCollectorPokes;
  if (ready) {
    sExpensiveCollectorPokes = 0;
  }
  return ready;
}


// Check all of the various collector timers/runners and see if they are waiting to fire.
// For the synchronous collector timers/runners, sGCTimer and sCCRunner, we only want to
// trigger the collection occasionally, because they are expensive.  The incremental collector
// timers, sInterSliceGCRunner and sICCRunner, are fast and need to be run many times, so
// always run their corresponding timer.

// This does not check sFullGCTimer, as that's a more expensive collection we run
// on a long timer.

// static
void
nsJSContext::RunNextCollectorTimer()
{
  if (sShuttingDown) {
    return;
  }

  if (sGCTimer) {
    if (ReadyToTriggerExpensiveCollectorTimer()) {
      GCTimerFired(nullptr, reinterpret_cast<void *>(JS::gcreason::DOM_WINDOW_UTILS));
    }
    return;
  }

  if (sInterSliceGCRunner) {
    InterSliceGCRunnerFired(TimeStamp(), nullptr);
    return;
  }

  // Check the CC timers after the GC timers, because the CC timers won't do
  // anything if a GC is in progress.
  MOZ_ASSERT(!sCCLockedOut, "Don't check the CC timers if the CC is locked out.");

  if (sCCRunner) {
    if (ReadyToTriggerExpensiveCollectorTimer()) {
      CCRunnerFired(TimeStamp(), nullptr);
    }
    return;
  }

  if (sICCRunner) {
    ICCRunnerFired(TimeStamp(), nullptr);
    return;
  }
}

// static
void
nsJSContext::PokeGC(JS::gcreason::Reason aReason,
                    JSObject* aObj,
                    int aDelay)
{
  if (sShuttingDown) {
    return;
  }

  if (aObj) {
    JS::Zone* zone = JS::GetObjectZone(aObj);
    CycleCollectedJSRuntime::Get()->AddZoneWaitingForGC(zone);
  } else if (aReason != JS::gcreason::CC_WAITING) {
    sNeedsFullGC = true;
  }

  if (sGCTimer || sInterSliceGCRunner) {
    // There's already a timer for GC'ing, just return
    return;
  }

  if (sCCRunner) {
    // Make sure CC is called...
    sNeedsFullCC = true;
    // and GC after it.
    sNeedsGCAfterCC = true;
    return;
  }

  if (sICCRunner) {
    // Make sure GC is called after the current CC completes.
    // No need to set sNeedsFullCC because we are currently running a CC.
    sNeedsGCAfterCC = true;
    return;
  }

  CallCreateInstance("@mozilla.org/timer;1", &sGCTimer);

  if (!sGCTimer) {
    // Failed to create timer (probably because we're in XPCOM shutdown)
    return;
  }

  static bool first = true;

  sGCTimer->SetTarget(SystemGroup::EventTargetFor(TaskCategory::GarbageCollection));
  sGCTimer->InitWithNamedFuncCallback(GCTimerFired,
                                      reinterpret_cast<void *>(aReason),
                                      aDelay
                                      ? aDelay
                                      : (first
                                         ? NS_FIRST_GC_DELAY
                                         : NS_GC_DELAY),
                                      nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
                                      "GCTimerFired");

  first = false;
}

// static
void
nsJSContext::PokeShrinkingGC()
{
  if (sShrinkingGCTimer || sShuttingDown) {
    return;
  }

  CallCreateInstance("@mozilla.org/timer;1", &sShrinkingGCTimer);

  if (!sShrinkingGCTimer) {
    // Failed to create timer (probably because we're in XPCOM shutdown)
    return;
  }

  sShrinkingGCTimer->SetTarget(SystemGroup::EventTargetFor(TaskCategory::GarbageCollection));
  sShrinkingGCTimer->InitWithNamedFuncCallback(ShrinkingGCTimerFired, nullptr,
                                               sCompactOnUserInactiveDelay,
                                               nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
                                               "ShrinkingGCTimerFired");
}

// static
void
nsJSContext::MaybePokeCC()
{
  if (sCCRunner || sICCRunner || sShuttingDown || !sHasRunGC) {
    return;
  }

  uint32_t sinceLastCCEnd = TimeUntilNow(sLastCCEndTime);
  if (sinceLastCCEnd && sinceLastCCEnd < NS_CC_DELAY) {
    return;
  }

  if (ShouldTriggerCC(nsCycleCollector_suspectedCount())) {
    sCCRunnerFireCount = 0;

    // We can kill some objects before running forgetSkippable.
    nsCycleCollector_dispatchDeferredDeletion();

    sCCRunner =
      CollectorRunner::Create(CCRunnerFired, NS_CC_SKIPPABLE_DELAY,
                              kForgetSkippableSliceDuration, true);
  }
}

//static
void
nsJSContext::KillGCTimer()
{
  if (sGCTimer) {
    sGCTimer->Cancel();
    NS_RELEASE(sGCTimer);
  }
}

void
nsJSContext::KillFullGCTimer()
{
  if (sFullGCTimer) {
    sFullGCTimer->Cancel();
    NS_RELEASE(sFullGCTimer);
  }
}

void
nsJSContext::KillInterSliceGCRunner()
{
  if (sInterSliceGCRunner) {
    sInterSliceGCRunner->Cancel();
    sInterSliceGCRunner = nullptr;
  }
}

//static
void
nsJSContext::KillShrinkingGCTimer()
{
  if (sShrinkingGCTimer) {
    sShrinkingGCTimer->Cancel();
    NS_RELEASE(sShrinkingGCTimer);
  }
}

//static
void
nsJSContext::KillCCRunner()
{
  sCCLockedOutTime = 0;
  if (sCCRunner) {
    sCCRunner->Cancel();
    sCCRunner = nullptr;
  }
}

//static
void
nsJSContext::KillICCRunner()
{
  sCCLockedOutTime = 0;

  if (sICCRunner) {
    sICCRunner->Cancel();
    sICCRunner = nullptr;
  }
}

class NotifyGCEndRunnable : public Runnable
{
  nsString mMessage;

public:
  explicit NotifyGCEndRunnable(const nsString& aMessage) : mMessage(aMessage) {}

  NS_DECL_NSIRUNNABLE
};

NS_IMETHODIMP
NotifyGCEndRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
  if (!observerService) {
    return NS_OK;
  }

  const char16_t oomMsg[3] = { '{', '}', 0 };
  const char16_t *toSend = mMessage.get() ? mMessage.get() : oomMsg;
  observerService->NotifyObservers(nullptr, "garbage-collection-statistics", toSend);

  return NS_OK;
}

static void
DOMGCSliceCallback(JSContext* aCx, JS::GCProgress aProgress, const JS::GCDescription &aDesc)
{
  NS_ASSERTION(NS_IsMainThread(), "GCs must run on the main thread");

  switch (aProgress) {
    case JS::GC_CYCLE_BEGIN: {
      // Prevent cycle collections and shrinking during incremental GC.
      sCCLockedOut = true;
      break;
    }

    case JS::GC_CYCLE_END: {
      PRTime delta = GetCollectionTimeDelta();

      if (sPostGCEventsToConsole) {
        nsString prefix, gcstats;
        gcstats.Adopt(aDesc.formatSummaryMessage(aCx));
        prefix.Adopt(nsTextFormatter::smprintf(u"GC(T+%.1f)[%s-%i] ",
                                               double(delta) / PR_USEC_PER_SEC,
                                               ProcessNameForCollectorLog(),
                                               getpid()));
        nsString msg = prefix + gcstats;
        nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
        if (cs) {
          cs->LogStringMessage(msg.get());
        }
      }

      if (!sShuttingDown) {
        if (sPostGCEventsToObserver || Telemetry::CanRecordExtended()) {
          nsString json;
          json.Adopt(aDesc.formatJSON(aCx, PR_Now()));
          RefPtr<NotifyGCEndRunnable> notify = new NotifyGCEndRunnable(json);
          NS_DispatchToMainThread(notify);
        }
      }

      sCCLockedOut = false;
      sIsCompactingOnUserInactive = false;

      // May need to kill the inter-slice GC runner
      nsJSContext::KillInterSliceGCRunner();

      sCCollectedWaitingForGC = 0;
      sCCollectedZonesWaitingForGC = 0;
      sLikelyShortLivingObjectsNeedingGC = 0;
      sCleanupsSinceLastGC = 0;
      sNeedsFullCC = true;
      sHasRunGC = true;
      nsJSContext::MaybePokeCC();

      if (aDesc.isZone_) {
        if (!sFullGCTimer && !sShuttingDown) {
          CallCreateInstance("@mozilla.org/timer;1", &sFullGCTimer);
          sFullGCTimer->SetTarget(SystemGroup::EventTargetFor(TaskCategory::GarbageCollection));
          sFullGCTimer->InitWithNamedFuncCallback(FullGCTimerFired,
                                                  nullptr,
                                                  NS_FULL_GC_DELAY,
                                                  nsITimer::TYPE_ONE_SHOT_LOW_PRIORITY,
                                                  "FullGCTimerFired");
        }
      } else {
        nsJSContext::KillFullGCTimer();
      }

      if (ShouldTriggerCC(nsCycleCollector_suspectedCount())) {
        nsCycleCollector_dispatchDeferredDeletion();
      }

      if (!aDesc.isZone_) {
        sNeedsFullGC = false;
      }

      break;
    }

    case JS::GC_SLICE_BEGIN:
      break;

    case JS::GC_SLICE_END:
      sGCUnnotifiedTotalTime +=
        aDesc.lastSliceEnd(aCx) - aDesc.lastSliceStart(aCx);

      // Schedule another GC slice if the GC has more work to do.
      nsJSContext::KillInterSliceGCRunner();
      if (!sShuttingDown && !aDesc.isComplete_) {
        sInterSliceGCRunner =
          CollectorRunner::Create(InterSliceGCRunnerFired, NS_INTERSLICE_GC_DELAY,
                                  sActiveIntersliceGCBudget, false);
      }

      if (ShouldTriggerCC(nsCycleCollector_suspectedCount())) {
        nsCycleCollector_dispatchDeferredDeletion();
      }

      if (sPostGCEventsToConsole) {
        nsString prefix, gcstats;
        gcstats.Adopt(aDesc.formatSliceMessage(aCx));
        prefix.Adopt(nsTextFormatter::smprintf(u"[%s-%i] ",
                                               ProcessNameForCollectorLog(),
                                               getpid()));
        nsString msg = prefix + gcstats;
        nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
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

void
nsJSContext::SetWindowProxy(JS::Handle<JSObject*> aWindowProxy)
{
  mWindowProxy = aWindowProxy;
}

JSObject*
nsJSContext::GetWindowProxy()
{
  return mWindowProxy;
}

void
nsJSContext::LikelyShortLivingObjectCreated()
{
  ++sLikelyShortLivingObjectsNeedingGC;
}

void
mozilla::dom::StartupJSEnvironment()
{
  // initialize all our statics, so that we can restart XPCOM
  sGCTimer = sShrinkingGCTimer = sFullGCTimer = nullptr;
  sCCLockedOut = false;
  sCCLockedOutTime = 0;
  sLastCCEndTime = TimeStamp();
  sHasRunGC = false;
  sPendingLoadCount = 0;
  sLoadingInProgress = false;
  sCCollectedWaitingForGC = 0;
  sCCollectedZonesWaitingForGC = 0;
  sLikelyShortLivingObjectsNeedingGC = 0;
  sPostGCEventsToConsole = false;
  sNeedsFullCC = false;
  sNeedsFullGC = true;
  sNeedsGCAfterCC = false;
  gNameSpaceManager = nullptr;
  sIsInitialized = false;
  sDidShutdown = false;
  sShuttingDown = false;
  sContextCount = 0;
  sSecurityManager = nullptr;
  gCCStats.Init();
  sExpensiveCollectorPokes = 0;
}

static void
SetGCParameter(JSGCParamKey aParam, uint32_t aValue)
{
  AutoJSAPI jsapi;
  jsapi.Init();
  JS_SetGCParameter(jsapi.cx(), aParam, aValue);
}

static void
SetMemoryHighWaterMarkPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  int32_t highwatermark = Preferences::GetInt(aPrefName, 128);
  SetGCParameter(JSGC_MAX_MALLOC_BYTES,
                 highwatermark * 1024L * 1024L);
}

static void
SetMemoryMaxPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  int32_t pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  uint32_t max = (pref <= 0 || pref >= 0x1000) ? -1 : (uint32_t)pref * 1024 * 1024;
  SetGCParameter(JSGC_MAX_BYTES, max);
}

static void
SetMemoryGCModePrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool enableZoneGC = Preferences::GetBool("javascript.options.mem.gc_per_zone");
  bool enableIncrementalGC = Preferences::GetBool("javascript.options.mem.gc_incremental");
  JSGCMode mode;
  if (enableIncrementalGC) {
    mode = JSGC_MODE_INCREMENTAL;
  } else if (enableZoneGC) {
    mode = JSGC_MODE_ZONE;
  } else {
    mode = JSGC_MODE_GLOBAL;
  }

  SetGCParameter(JSGC_MODE, mode);
}

static void
SetMemoryGCSliceTimePrefChangedCallback(const char* aPrefName, void* aClosure)
{
  int32_t pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  if (pref > 0 && pref < 100000) {
    sActiveIntersliceGCBudget = pref;
    SetGCParameter(JSGC_SLICE_TIME_BUDGET, pref);
  }
}

static void
SetMemoryGCCompactingPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool pref = Preferences::GetBool(aPrefName);
  SetGCParameter(JSGC_COMPACTING_ENABLED, pref);
}

static void
SetMemoryGCPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  int32_t pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  if (pref >= 0 && pref < 10000) {
    SetGCParameter((JSGCParamKey)(intptr_t)aClosure, pref);
  }
}

static void
SetMemoryGCDynamicHeapGrowthPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool pref = Preferences::GetBool(aPrefName);
  SetGCParameter(JSGC_DYNAMIC_HEAP_GROWTH, pref);
}

static void
SetMemoryGCDynamicMarkSlicePrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool pref = Preferences::GetBool(aPrefName);
  SetGCParameter(JSGC_DYNAMIC_MARK_SLICE, pref);
}

static void
SetMemoryGCRefreshFrameSlicesEnabledPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool pref = Preferences::GetBool(aPrefName);
  SetGCParameter(JSGC_REFRESH_FRAME_SLICES_ENABLED, pref);
}


static void
SetIncrementalCCPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool pref = Preferences::GetBool(aPrefName);
  sIncrementalCC = pref;
}

static bool
AsmJSCacheOpenEntryForRead(JS::Handle<JSObject*> aGlobal,
                           const char16_t* aBegin,
                           const char16_t* aLimit,
                           size_t* aSize,
                           const uint8_t** aMemory,
                           intptr_t *aHandle)
{
  nsIPrincipal* principal =
    nsJSPrincipals::get(JS_GetCompartmentPrincipals(js::GetObjectCompartment(aGlobal)));
  return asmjscache::OpenEntryForRead(principal, aBegin, aLimit, aSize, aMemory,
                                      aHandle);
}

static JS::AsmJSCacheResult
AsmJSCacheOpenEntryForWrite(JS::Handle<JSObject*> aGlobal,
                            const char16_t* aBegin,
                            const char16_t* aEnd,
                            size_t aSize,
                            uint8_t** aMemory,
                            intptr_t* aHandle)
{
  nsIPrincipal* principal =
    nsJSPrincipals::get(JS_GetCompartmentPrincipals(js::GetObjectCompartment(aGlobal)));
  return asmjscache::OpenEntryForWrite(principal, aBegin, aEnd, aSize, aMemory,
                                       aHandle);
}

class AsyncTaskRunnable final : public Runnable
{
  ~AsyncTaskRunnable()
  {
    MOZ_ASSERT(!mTask);
  }

public:
  explicit AsyncTaskRunnable(JS::AsyncTask* aTask)
    : mTask(aTask)
  {
    MOZ_ASSERT(mTask);
  }

protected:
  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    AutoJSAPI jsapi;
    jsapi.Init();
    mTask->finish(jsapi.cx());
    mTask = nullptr;  // mTask may delete itself

    return NS_OK;
  }

private:
  JS::AsyncTask* mTask;
};

static bool
StartAsyncTaskCallback(JSContext* aCx, JS::AsyncTask* aTask)
{
  return true;
}

static bool
FinishAsyncTaskCallback(JS::AsyncTask* aTask)
{
  // AsyncTasks can finish during shutdown so cannot simply
  // NS_DispatchToMainThread.
  nsCOMPtr<nsIEventTarget> mainTarget = GetMainThreadEventTarget();
  if (!mainTarget) {
    return false;
  }

  RefPtr<AsyncTaskRunnable> r = new AsyncTaskRunnable(aTask);
  MOZ_ALWAYS_SUCCEEDS(mainTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL));
  return true;
}

void
nsJSContext::EnsureStatics()
{
  if (sIsInitialized) {
    if (!nsContentUtils::XPConnect()) {
      MOZ_CRASH();
    }
    return;
  }

  nsresult rv = CallGetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID,
                               &sSecurityManager);
  if (NS_FAILED(rv)) {
    MOZ_CRASH();
  }

  // Let's make sure that our main thread is the same as the xpcom main thread.
  MOZ_ASSERT(NS_IsMainThread());

  AutoJSAPI jsapi;
  jsapi.Init();

  sPrevGCSliceCallback = JS::SetGCSliceCallback(jsapi.cx(), DOMGCSliceCallback);

  // Set up the asm.js cache callbacks
  static const JS::AsmJSCacheOps asmJSCacheOps = {
    AsmJSCacheOpenEntryForRead,
    asmjscache::CloseEntryForRead,
    AsmJSCacheOpenEntryForWrite,
    asmjscache::CloseEntryForWrite
  };
  JS::SetAsmJSCacheOps(jsapi.cx(), &asmJSCacheOps);

  JS::SetAsyncTaskCallbacks(jsapi.cx(), StartAsyncTaskCallback, FinishAsyncTaskCallback);

  // Set these global xpconnect options...
  Preferences::RegisterCallbackAndCall(SetMemoryHighWaterMarkPrefChangedCallback,
                                       "javascript.options.mem.high_water_mark");

  Preferences::RegisterCallbackAndCall(SetMemoryMaxPrefChangedCallback,
                                       "javascript.options.mem.max");

  Preferences::RegisterCallbackAndCall(SetMemoryGCModePrefChangedCallback,
                                       "javascript.options.mem.gc_per_zone");

  Preferences::RegisterCallbackAndCall(SetMemoryGCModePrefChangedCallback,
                                       "javascript.options.mem.gc_incremental");

  Preferences::RegisterCallbackAndCall(SetMemoryGCSliceTimePrefChangedCallback,
                                       "javascript.options.mem.gc_incremental_slice_ms");

  Preferences::RegisterCallbackAndCall(SetMemoryGCCompactingPrefChangedCallback,
                                       "javascript.options.mem.gc_compacting");

  Preferences::RegisterCallbackAndCall(SetMemoryGCPrefChangedCallback,
                                       "javascript.options.mem.gc_high_frequency_time_limit_ms",
                                       (void *)JSGC_HIGH_FREQUENCY_TIME_LIMIT);

  Preferences::RegisterCallbackAndCall(SetMemoryGCDynamicMarkSlicePrefChangedCallback,
                                       "javascript.options.mem.gc_dynamic_mark_slice");

  Preferences::RegisterCallbackAndCall(SetMemoryGCRefreshFrameSlicesEnabledPrefChangedCallback,
                                       "javascript.options.mem.gc_refresh_frame_slices_enabled");

  Preferences::RegisterCallbackAndCall(SetMemoryGCDynamicHeapGrowthPrefChangedCallback,
                                       "javascript.options.mem.gc_dynamic_heap_growth");

  Preferences::RegisterCallbackAndCall(SetMemoryGCPrefChangedCallback,
                                       "javascript.options.mem.gc_low_frequency_heap_growth",
                                       (void *)JSGC_LOW_FREQUENCY_HEAP_GROWTH);

  Preferences::RegisterCallbackAndCall(SetMemoryGCPrefChangedCallback,
                                       "javascript.options.mem.gc_high_frequency_heap_growth_min",
                                       (void *)JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN);

  Preferences::RegisterCallbackAndCall(SetMemoryGCPrefChangedCallback,
                                       "javascript.options.mem.gc_high_frequency_heap_growth_max",
                                       (void *)JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX);

  Preferences::RegisterCallbackAndCall(SetMemoryGCPrefChangedCallback,
                                       "javascript.options.mem.gc_high_frequency_low_limit_mb",
                                       (void *)JSGC_HIGH_FREQUENCY_LOW_LIMIT);

  Preferences::RegisterCallbackAndCall(SetMemoryGCPrefChangedCallback,
                                       "javascript.options.mem.gc_high_frequency_high_limit_mb",
                                       (void *)JSGC_HIGH_FREQUENCY_HIGH_LIMIT);

  Preferences::RegisterCallbackAndCall(SetMemoryGCPrefChangedCallback,
                                       "javascript.options.mem.gc_allocation_threshold_mb",
                                       (void *)JSGC_ALLOCATION_THRESHOLD);

  Preferences::RegisterCallbackAndCall(SetIncrementalCCPrefChangedCallback,
                                       "dom.cycle_collector.incremental");

  Preferences::RegisterCallbackAndCall(SetMemoryGCPrefChangedCallback,
                                       "javascript.options.mem.gc_min_empty_chunk_count",
                                       (void *)JSGC_MIN_EMPTY_CHUNK_COUNT);

  Preferences::RegisterCallbackAndCall(SetMemoryGCPrefChangedCallback,
                                       "javascript.options.mem.gc_max_empty_chunk_count",
                                       (void *)JSGC_MAX_EMPTY_CHUNK_COUNT);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    MOZ_CRASH();
  }

  Preferences::AddBoolVarCache(&sGCOnMemoryPressure,
                               "javascript.options.gc_on_memory_pressure",
                               true);

  Preferences::AddBoolVarCache(&sCompactOnUserInactive,
                               "javascript.options.compact_on_user_inactive",
                               true);

  Preferences::AddUintVarCache(&sCompactOnUserInactiveDelay,
                               "javascript.options.compact_on_user_inactive_delay",
                               NS_DEAULT_INACTIVE_GC_DELAY);

  Preferences::AddBoolVarCache(&sPostGCEventsToConsole,
                               JS_OPTIONS_DOT_STR "mem.log");
  Preferences::AddBoolVarCache(&sPostGCEventsToObserver,
                               JS_OPTIONS_DOT_STR "mem.notify");

  nsIObserver* observer = new nsJSEnvironmentObserver();
  obs->AddObserver(observer, "memory-pressure", false);
  obs->AddObserver(observer, "user-interaction-inactive", false);
  obs->AddObserver(observer, "user-interaction-active", false);
  obs->AddObserver(observer, "quit-application", false);
  obs->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);

  sIsInitialized = true;
}

nsScriptNameSpaceManager*
mozilla::dom::GetNameSpaceManager()
{
  if (sDidShutdown)
    return nullptr;

  if (!gNameSpaceManager) {
    gNameSpaceManager = new nsScriptNameSpaceManager;
    NS_ADDREF(gNameSpaceManager);

    nsresult rv = gNameSpaceManager->Init();
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  return gNameSpaceManager;
}

nsScriptNameSpaceManager*
mozilla::dom::PeekNameSpaceManager()
{
  return gNameSpaceManager;
}

void
mozilla::dom::ShutdownJSEnvironment()
{
  KillTimers();

  NS_IF_RELEASE(gNameSpaceManager);

  if (!sContextCount) {
    // We're being shutdown, and there are no more contexts
    // alive, release the security manager.
    NS_IF_RELEASE(sSecurityManager);
  }

  sShuttingDown = true;
  sDidShutdown = true;
}

// A fast-array class for JS.  This class supports both nsIJSScriptArray and
// nsIArray.  If it is JS itself providing and consuming this class, all work
// can be done via nsIJSScriptArray, and avoid the conversion of elements
// to/from nsISupports.
// When consumed by non-JS (eg, another script language), conversion is done
// on-the-fly.
class nsJSArgArray final : public nsIJSArgArray {
public:
  nsJSArgArray(JSContext *aContext, uint32_t argc, const JS::Value* argv,
               nsresult *prv);

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
  JSContext *mContext;
  JS::Heap<JS::Value> *mArgv;
  uint32_t mArgc;
};

nsJSArgArray::nsJSArgArray(JSContext *aContext, uint32_t argc,
                           const JS::Value* argv, nsresult *prv)
  : mContext(aContext)
  , mArgv(nullptr)
  , mArgc(argc)
{
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
    for (uint32_t i = 0; i < argc; ++i)
      mArgv[i] = argv[i];
  }

  if (argc > 0) {
    mozilla::HoldJSObjects(this);
  }

  *prv = NS_OK;
}

nsJSArgArray::~nsJSArgArray()
{
  ReleaseJSObjects();
}

void
nsJSArgArray::ReleaseJSObjects()
{
  if (mArgv) {
    delete [] mArgv;
  }
  if (mArgc > 0) {
    mArgc = 0;
    mozilla::DropJSObjects(this);
  }
}

// QueryInterface implementation for nsJSArgArray
NS_IMPL_CYCLE_COLLECTION_CLASS(nsJSArgArray)

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

nsresult
nsJSArgArray::GetArgs(uint32_t *argc, void **argv)
{
  *argv = (void *)mArgv;
  *argc = mArgc;
  return NS_OK;
}

// nsIArray impl
NS_IMETHODIMP nsJSArgArray::GetLength(uint32_t *aLength)
{
  *aLength = mArgc;
  return NS_OK;
}

NS_IMETHODIMP nsJSArgArray::QueryElementAt(uint32_t index, const nsIID & uuid, void * *result)
{
  *result = nullptr;
  if (index >= mArgc)
    return NS_ERROR_INVALID_ARG;

  if (uuid.Equals(NS_GET_IID(nsIVariant)) || uuid.Equals(NS_GET_IID(nsISupports))) {
    // Have to copy a Heap into a Rooted to work with it.
    JS::Rooted<JS::Value> val(mContext, mArgv[index]);
    return nsContentUtils::XPConnect()->JSToVariant(mContext, val,
                                                    (nsIVariant **)result);
  }
  NS_WARNING("nsJSArgArray only handles nsIVariant");
  return NS_ERROR_NO_INTERFACE;
}

NS_IMETHODIMP nsJSArgArray::IndexOf(uint32_t startIndex, nsISupports *element, uint32_t *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsJSArgArray::Enumerate(nsISimpleEnumerator **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// The factory function
nsresult NS_CreateJSArgv(JSContext *aContext, uint32_t argc,
                         const JS::Value* argv, nsIJSArgArray **aArray)
{
  nsresult rv;
  nsCOMPtr<nsIJSArgArray> ret = new nsJSArgArray(aContext, argc, argv, &rv);
  if (NS_FAILED(rv)) {
    return rv;
  }
  ret.forget(aArray);
  return NS_OK;
}
