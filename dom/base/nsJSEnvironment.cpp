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
#include "nsIJSRuntimeService.h"
#include "nsCOMPtr.h"
#include "nsISupportsPrimitives.h"
#include "nsReadableUtils.h"
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
#include "nsCxPusher.h"
#include "mozilla/EventDispatcher.h"
#include "nsIContent.h"
#include "nsCycleCollector.h"
#include "nsNetUtil.h"
#include "nsXPCOMCIDInternal.h"
#include "nsIXULRuntime.h"
#include "nsTextFormatter.h"

#include "xpcpublic.h"

#include "js/OldDebugAPI.h"
#include "jswrapper.h"
#include "nsIArray.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "prmem.h"
#include "WrapperFactory.h"
#include "nsGlobalWindow.h"
#include "nsScriptNameSpaceManager.h"
#include "StructuredCloneTags.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/dom/ErrorEvent.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/ImageDataBinding.h"
#include "nsAXPCNativeCallContext.h"
#include "mozilla/CycleCollectedJSRuntime.h"

#include "nsJSPrincipals.h"

#ifdef XP_MACOSX
// AssertMacros.h defines 'check' and conflicts with AccessCheck.h
#undef check
#endif
#include "AccessCheck.h"

#ifdef MOZ_JSDEBUGGER
#include "jsdIDebuggerService.h"
#endif
#ifdef MOZ_LOGGING
// Force PR_LOGGING so we can get JS strict warnings even in release builds
#define FORCE_PR_LOG 1
#endif
#include "prlog.h"
#include "prthread.h"

#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/asmjscache/AsmJSCache.h"
#include "mozilla/dom/CanvasRenderingContext2DBinding.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/ContentEvents.h"

#include "nsCycleCollectionNoteRootCallback.h"
#include "GeckoProfiler.h"

using namespace mozilla;
using namespace mozilla::dom;

const size_t gStackSize = 8192;

#ifdef PR_LOGGING
static PRLogModuleInfo* gJSDiagnostics;
#endif

// Thank you Microsoft!
#ifdef CompareString
#undef CompareString
#endif

#define NS_SHRINK_GC_BUFFERS_DELAY  4000 // ms

// The amount of time we wait from the first request to GC to actually
// doing the first GC.
#define NS_FIRST_GC_DELAY           10000 // ms

#define NS_FULL_GC_DELAY            60000 // ms

// Maximum amount of time that should elapse between incremental GC slices
#define NS_INTERSLICE_GC_DELAY      100 // ms

// If we haven't painted in 100ms, we allow for a longer GC budget
#define NS_INTERSLICE_GC_BUDGET     40 // ms

// The amount of time we wait between a request to CC (after GC ran)
// and doing the actual CC.
#define NS_CC_DELAY                 6000 // ms

#define NS_CC_SKIPPABLE_DELAY       400 // ms

// Maximum amount of time that should elapse between incremental CC slices
static const int64_t kICCIntersliceDelay = 32; // ms

// Time budget for an incremental CC slice
static const int64_t kICCSliceBudget = 10; // ms

// Maximum total duration for an ICC
static const uint32_t kMaxICCDuration = 2000; // ms

// Force a CC after this long if there's more than NS_CC_FORCED_PURPLE_LIMIT
// objects in the purple buffer.
#define NS_CC_FORCED                (2 * 60 * PR_USEC_PER_SEC) // 2 min
#define NS_CC_FORCED_PURPLE_LIMIT   10

// Don't allow an incremental GC to lock out the CC for too long.
#define NS_MAX_CC_LOCKEDOUT_TIME    (15 * PR_USEC_PER_SEC) // 15 seconds

// Trigger a CC if the purple buffer exceeds this size when we check it.
#define NS_CC_PURPLE_LIMIT          200

#define JAVASCRIPT nsIProgrammingLanguage::JAVASCRIPT

// Large value used to specify that a script should run essentially forever
#define NS_UNLIMITED_SCRIPT_RUNTIME (0x40000000LL << 32)

#define NS_MAJOR_FORGET_SKIPPABLE_CALLS 2

// if you add statics here, add them to the list in StartupJSEnvironment

static nsITimer *sGCTimer;
static nsITimer *sShrinkGCBuffersTimer;
static nsITimer *sCCTimer;
static nsITimer *sICCTimer;
static nsITimer *sFullGCTimer;
static nsITimer *sInterSliceGCTimer;

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
static uint32_t sLikelyShortLivingObjectsNeedingGC;
static bool sPostGCEventsToConsole;
static bool sPostGCEventsToObserver;
static int32_t sCCTimerFireCount = 0;
static uint32_t sMinForgetSkippableTime = UINT32_MAX;
static uint32_t sMaxForgetSkippableTime = 0;
static uint32_t sTotalForgetSkippableTime = 0;
static uint32_t sRemovedPurples = 0;
static uint32_t sForgetSkippableBeforeCC = 0;
static uint32_t sPreviousSuspectedCount = 0;
static uint32_t sCleanupsSinceLastGC = UINT32_MAX;
static bool sNeedsFullCC = false;
static bool sNeedsGCAfterCC = false;
static nsJSContext *sContextList = nullptr;
static bool sIncrementalCC = false;

static nsScriptNameSpaceManager *gNameSpaceManager;

static nsIJSRuntimeService *sRuntimeService;

static const char kJSRuntimeServiceContractID[] =
  "@mozilla.org/js/xpc/RuntimeService;1";

static PRTime sFirstCollectionTime;

static JSRuntime *sRuntime;

static bool sIsInitialized;
static bool sDidShutdown;
static bool sShuttingDown;
static int32_t sContextCount;

static nsIScriptSecurityManager *sSecurityManager;

// nsJSEnvironmentObserver observes the memory-pressure notifications
// and forces a garbage collection and cycle collection when it happens, if
// the appropriate pref is set.

static bool sGCOnMemoryPressure;

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
  nsJSContext::KillShrinkGCBuffersTimer();
  nsJSContext::KillCCTimer();
  nsJSContext::KillICCTimer();
  nsJSContext::KillFullGCTimer();
  nsJSContext::KillInterSliceGCTimer();
}

// If we collected a substantial amount of cycles, poke the GC since more objects
// might be unreachable now.
static bool
NeedsGCAfterCC()
{
  return sCCollectedWaitingForGC > 250 ||
    sLikelyShortLivingObjectsNeedingGC > 2500 ||
    sNeedsGCAfterCC;
}

class nsJSEnvironmentObserver MOZ_FINAL : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(nsJSEnvironmentObserver, nsIObserver)

NS_IMETHODIMP
nsJSEnvironmentObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                 const char16_t* aData)
{
  if (sGCOnMemoryPressure && !nsCRT::strcmp(aTopic, "memory-pressure")) {
    if(StringBeginsWith(nsDependentString(aData),
                        NS_LITERAL_STRING("low-memory-ongoing"))) {
      // Don't GC/CC if we are in an ongoing low-memory state since its very
      // slow and it likely won't help us anyway.
      return NS_OK;
    }
    nsJSContext::GarbageCollectNow(JS::gcreason::MEM_PRESSURE,
                                   nsJSContext::NonIncrementalGC,
                                   nsJSContext::NonCompartmentGC,
                                   nsJSContext::ShrinkingGC);
    nsJSContext::CycleCollectNow();
    if (NeedsGCAfterCC()) {
      nsJSContext::GarbageCollectNow(JS::gcreason::MEM_PRESSURE,
                                     nsJSContext::NonIncrementalGC,
                                     nsJSContext::NonCompartmentGC,
                                     nsJSContext::ShrinkingGC);
    }
  } else if (!nsCRT::strcmp(aTopic, "quit-application")) {
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
  AutoFree(void *aPtr) : mPtr(aPtr) {
  }
  ~AutoFree() {
    if (mPtr)
      nsMemory::Free(mPtr);
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
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aScriptGlobal));
  nsIDocShell *docShell = win ? win->GetDocShell() : nullptr;
  if (docShell) {
    nsRefPtr<nsPresContext> presContext;
    docShell->GetPresContext(getter_AddRefs(presContext));

    static int32_t errorDepth; // Recursion prevention
    ++errorDepth;

    if (errorDepth < 2) {
      // Dispatch() must be synchronous for the recursion block
      // (errorDepth) to work.
      nsRefPtr<ErrorEvent> event =
        ErrorEvent::Constructor(static_cast<nsGlobalWindow*>(win.get()),
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

namespace mozilla {
namespace dom {

AsyncErrorReporter::AsyncErrorReporter(JSRuntime* aRuntime,
                                       JSErrorReport* aErrorReport,
                                       const char* aFallbackMessage,
                                       bool aIsChromeError,
                                       nsPIDOMWindow* aWindow)
  : mSourceLine(static_cast<const char16_t*>(aErrorReport->uclinebuf))
  , mLineNumber(aErrorReport->lineno)
  , mColumn(aErrorReport->column)
  , mFlags(aErrorReport->flags)
{
  if (!aErrorReport->filename) {
    mFileName.SetIsVoid(true);
  } else {
    mFileName.AssignWithConversion(aErrorReport->filename);
  }

  const char16_t* m = static_cast<const char16_t*>(aErrorReport->ucmessage);
  if (m) {
    const char16_t* n = static_cast<const char16_t*>
      (js::GetErrorTypeName(aRuntime, aErrorReport->exnType));
    if (n) {
      mErrorMsg.Assign(n);
      mErrorMsg.AppendLiteral(": ");
    }
    mErrorMsg.Append(m);
  }

  if (mErrorMsg.IsEmpty() && aFallbackMessage) {
    mErrorMsg.AssignWithConversion(aFallbackMessage);
  }

  mCategory = aIsChromeError ? NS_LITERAL_CSTRING("chrome javascript") :
                               NS_LITERAL_CSTRING("content javascript");

  mInnerWindowID = 0;
  if (aWindow && aWindow->IsOuterWindow()) {
    aWindow = aWindow->GetCurrentInnerWindow();
  }
  if (aWindow) {
    mInnerWindowID = aWindow->WindowID();
  }
}

void
AsyncErrorReporter::ReportError()
{
  nsCOMPtr<nsIScriptError> errorObject =
    do_CreateInstance("@mozilla.org/scripterror;1");
  if (!errorObject) {
    return;
  }

  nsresult rv = errorObject->InitWithWindowID(mErrorMsg, mFileName,
                                              mSourceLine, mLineNumber,
                                              mColumn, mFlags, mCategory,
                                              mInnerWindowID);
  if (NS_FAILED(rv)) {
    return;
  }

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  if (!consoleService) {
    return;
  }

  consoleService->LogMessage(errorObject);
  return;
}

} // namespace dom
} // namespace mozilla

class ScriptErrorEvent : public AsyncErrorReporter
{
public:
  ScriptErrorEvent(nsIScriptGlobalObject* aScriptGlobal,
                   JSRuntime* aRuntime,
                   JSErrorReport* aErrorReport,
                   const char* aFallbackMessage,
                   nsIPrincipal* aScriptOriginPrincipal,
                   nsIPrincipal* aGlobalPrincipal,
                   nsPIDOMWindow* aWindow,
                   JS::Handle<JS::Value> aError,
                   bool aDispatchEvent)
    // Pass an empty category, then compute ours
    : AsyncErrorReporter(aRuntime, aErrorReport, aFallbackMessage,
                         nsContentUtils::IsSystemPrincipal(aGlobalPrincipal),
                         aWindow)
    , mScriptGlobal(aScriptGlobal)
    , mOriginPrincipal(aScriptOriginPrincipal)
    , mDispatchEvent(aDispatchEvent)
    , mError(aRuntime, aError)
  {
  }

  NS_IMETHOD Run()
  {
    nsEventStatus status = nsEventStatus_eIgnore;
    // First, notify the DOM that we have a script error.
    if (mDispatchEvent) {
      nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(mScriptGlobal));
      nsIDocShell* docShell = win ? win->GetDocShell() : nullptr;
      if (docShell &&
          !JSREPORT_IS_WARNING(mFlags) &&
          !sHandlingScriptError) {
        AutoRestore<bool> recursionGuard(sHandlingScriptError);
        sHandlingScriptError = true;

        nsRefPtr<nsPresContext> presContext;
        docShell->GetPresContext(getter_AddRefs(presContext));

        ThreadsafeAutoJSContext cx;
        RootedDictionary<ErrorEventInit> init(cx);
        init.mCancelable = true;
        init.mFilename = mFileName;
        init.mBubbles = true;

        nsCOMPtr<nsIScriptObjectPrincipal> sop(do_QueryInterface(win));
        NS_ENSURE_STATE(sop);
        nsIPrincipal* p = sop->GetPrincipal();
        NS_ENSURE_STATE(p);

        bool sameOrigin = !mOriginPrincipal;

        if (p && !sameOrigin) {
          if (NS_FAILED(p->Subsumes(mOriginPrincipal, &sameOrigin))) {
            sameOrigin = false;
          }
        }

        NS_NAMED_LITERAL_STRING(xoriginMsg, "Script error.");
        if (sameOrigin) {
          init.mMessage = mErrorMsg;
          init.mLineno = mLineNumber;
          init.mColumn = mColumn;
          init.mError = mError;
        } else {
          NS_WARNING("Not same origin error!");
          init.mMessage = xoriginMsg;
          init.mLineno = 0;
        }

        nsRefPtr<ErrorEvent> event =
          ErrorEvent::Constructor(static_cast<nsGlobalWindow*>(win.get()),
                                  NS_LITERAL_STRING("error"), init);
        event->SetTrusted(true);

        EventDispatcher::DispatchDOMEvent(win, nullptr, event, presContext,
                                          &status);
      }
    }

    if (status != nsEventStatus_eConsumeNoDefault) {
      AsyncErrorReporter::ReportError();
    }

    return NS_OK;
  }

private:
  nsCOMPtr<nsIScriptGlobalObject> mScriptGlobal;
  nsCOMPtr<nsIPrincipal>          mOriginPrincipal;
  bool                            mDispatchEvent;
  JS::PersistentRootedValue       mError;

  static bool sHandlingScriptError;
};

bool ScriptErrorEvent::sHandlingScriptError = false;

// NOTE: This function could be refactored to use the above.  The only reason
// it has not been done is that the code below only fills the error event
// after it has a good nsPresContext - whereas using the above function
// would involve always filling it.  Is that a concern?
void
NS_ScriptErrorReporter(JSContext *cx,
                       const char *message,
                       JSErrorReport *report)
{
  // We don't want to report exceptions too eagerly, but warnings in the
  // absence of werror are swallowed whole, so report those now.
  if (!JSREPORT_IS_WARNING(report->flags)) {
    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    if (JS::DescribeScriptedCaller(cx)) {
      xpc->MarkErrorUnreported(cx);
      return;
    }

    if (xpc) {
      nsAXPCNativeCallContext *cc = nullptr;
      xpc->GetCurrentNativeCallContext(&cc);
      if (cc) {
        nsAXPCNativeCallContext *prev = cc;
        while (NS_SUCCEEDED(prev->GetPreviousCallContext(&prev)) && prev) {
          uint16_t lang;
          if (NS_SUCCEEDED(prev->GetLanguage(&lang)) &&
            lang == nsAXPCNativeCallContext::LANG_JS) {
            xpc->MarkErrorUnreported(cx);
            return;
          }
        }
      }
    }
  }

  // XXX this means we are not going to get error reports on non DOM contexts
  nsIScriptContext *context = nsJSUtils::GetDynamicScriptContext(cx);

  JS::Rooted<JS::Value> exception(cx);
  ::JS_GetPendingException(cx, &exception);

  // Note: we must do this before running any more code on cx (if cx is the
  // dynamic script context).
  ::JS_ClearPendingException(cx);

  if (context) {
    nsIScriptGlobalObject *globalObject = context->GetGlobalObject();

    if (globalObject) {

      nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(globalObject);
      nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal =
        do_QueryInterface(globalObject);
      NS_ASSERTION(scriptPrincipal, "Global objects must implement "
                   "nsIScriptObjectPrincipal");
      nsContentUtils::AddScriptRunner(
        new ScriptErrorEvent(globalObject,
                             JS_GetRuntime(cx),
                             report,
                             message,
                             nsJSPrincipals::get(report->originPrincipals),
                             scriptPrincipal->GetPrincipal(),
                             win,
                             exception,
                             /* We do not try to report Out Of Memory via a dom
                              * event because the dom event handler would
                              * encounter an OOM exception trying to process the
                              * event, and then we'd need to generate a new OOM
                              * event for that new OOM instance -- this isn't
                              * pretty.
                              */
                             report->errorNumber != JSMSG_OUT_OF_MEMORY));
    }
  }

  if (nsContentUtils::DOMWindowDumpEnabled()) {
    // Print it to stderr as well, for the benefit of those invoking
    // mozilla with -console.
    nsAutoCString error;
    error.Assign("JavaScript ");
    if (JSREPORT_IS_STRICT(report->flags))
      error.Append("strict ");
    if (JSREPORT_IS_WARNING(report->flags))
      error.Append("warning: ");
    else
      error.Append("error: ");
    error.Append(report->filename);
    error.Append(", line ");
    error.AppendInt(report->lineno, 10);
    error.Append(": ");
    if (report->ucmessage) {
      AppendUTF16toUTF8(reinterpret_cast<const char16_t*>(report->ucmessage),
                        error);
    } else {
      error.Append(message);
    }

    fprintf(stderr, "%s\n", error.get());
    fflush(stderr);
  }

#ifdef PR_LOGGING
  if (!gJSDiagnostics)
    gJSDiagnostics = PR_NewLogModule("JSDiagnostics");

  if (gJSDiagnostics) {
    PR_LOG(gJSDiagnostics,
           JSREPORT_IS_WARNING(report->flags) ? PR_LOG_WARNING : PR_LOG_ERROR,
           ("file %s, line %u: %s\n%s%s",
            report->filename, report->lineno, message,
            report->linebuf ? report->linebuf : "",
            (report->linebuf &&
             report->linebuf[strlen(report->linebuf)-1] != '\n')
            ? "\n"
            : ""));
  }
#endif
}

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

  nsAutoCString spec;
  uri->GetSpec(spec);
  printf("%s\n", spec.get());
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

  nsAutoCString spec;
  uri->GetSpec(spec);
  printf("%s\n", spec.get());
}

void
DumpString(const nsAString &str)
{
  printf("%s\n", NS_ConvertUTF16toUTF8(str).get());
}
#endif

#define JS_OPTIONS_DOT_STR "javascript.options."

static const char js_options_dot_str[]   = JS_OPTIONS_DOT_STR;
static const char js_strict_option_str[] = JS_OPTIONS_DOT_STR "strict";
#ifdef DEBUG
static const char js_strict_debug_option_str[] = JS_OPTIONS_DOT_STR "strict.debug";
#endif
static const char js_werror_option_str[] = JS_OPTIONS_DOT_STR "werror";
#ifdef JS_GC_ZEAL
static const char js_zeal_option_str[]        = JS_OPTIONS_DOT_STR "gczeal";
static const char js_zeal_frequency_str[]     = JS_OPTIONS_DOT_STR "gczeal.frequency";
#endif
static const char js_memlog_option_str[]      = JS_OPTIONS_DOT_STR "mem.log";
static const char js_memnotify_option_str[]   = JS_OPTIONS_DOT_STR "mem.notify";

void
nsJSContext::JSOptionChangedCallback(const char *pref, void *data)
{
  nsJSContext *context = reinterpret_cast<nsJSContext *>(data);
  JSContext *cx = context->mContext;

  sPostGCEventsToConsole = Preferences::GetBool(js_memlog_option_str);
  sPostGCEventsToObserver = Preferences::GetBool(js_memnotify_option_str);

  JS::ContextOptionsRef(cx).setExtraWarnings(Preferences::GetBool(js_strict_option_str));

  // The vanilla GetGlobalObject returns null if a global isn't set up on
  // the context yet. We can sometimes be call midway through context init,
  // So ask for the member directly instead.
  nsIScriptGlobalObject *global = context->GetGlobalObjectRef();

  // XXX should we check for sysprin instead of a chrome window, to make
  // XXX components be covered by the chrome pref instead of the content one?
  nsCOMPtr<nsIDOMWindow> contentWindow(do_QueryInterface(global));
  nsCOMPtr<nsIDOMChromeWindow> chromeWindow(do_QueryInterface(global));

#ifdef DEBUG
  // In debug builds, warnings are enabled in chrome context if
  // javascript.options.strict.debug is true
  if (Preferences::GetBool(js_strict_debug_option_str) &&
      (chromeWindow || !contentWindow)) {
    JS::ContextOptionsRef(cx).setExtraWarnings(true);
  }
#endif

  JS::ContextOptionsRef(cx).setWerror(Preferences::GetBool(js_werror_option_str));

#ifdef JS_GC_ZEAL
  int32_t zeal = Preferences::GetInt(js_zeal_option_str, -1);
  int32_t frequency = Preferences::GetInt(js_zeal_frequency_str, JS_DEFAULT_ZEAL_FREQ);
  if (zeal >= 0)
    ::JS_SetGCZeal(context->mContext, (uint8_t)zeal, frequency);
#endif
}

nsJSContext::nsJSContext(bool aGCOnDestruction,
                         nsIScriptGlobalObject* aGlobalObject)
  : mWindowProxy(nullptr)
  , mGCOnDestruction(aGCOnDestruction)
  , mGlobalObjectRef(aGlobalObject)
{
  EnsureStatics();

  mNext = sContextList;
  mPrev = &sContextList;
  if (sContextList) {
    sContextList->mPrev = &mNext;
  }
  sContextList = this;

  ++sContextCount;

  mContext = ::JS_NewContext(sRuntime, gStackSize);
  if (mContext) {
    ::JS_SetContextPrivate(mContext, static_cast<nsIScriptContext *>(this));

    // Make sure the new context gets the default context options
    JS::ContextOptionsRef(mContext).setPrivateIsNSISupports(true)
                                   .setNoDefaultCompartmentObject(true);

    // Watch for the JS boolean options
    Preferences::RegisterCallback(JSOptionChangedCallback,
                                  js_options_dot_str, this);
  }
  mIsInitialized = false;
  mProcessingScriptTag = false;
  HoldJSObjects(this);
}

nsJSContext::~nsJSContext()
{
  *mPrev = mNext;
  if (mNext) {
    mNext->mPrev = mPrev;
  }

  mGlobalObjectRef = nullptr;

  DestroyJSContext();

  --sContextCount;

  if (!sContextCount && sDidShutdown) {
    // The last context is being deleted, and we're already in the
    // process of shutting down, release the JS runtime service, and
    // the security manager.

    NS_IF_RELEASE(sRuntimeService);
    NS_IF_RELEASE(sSecurityManager);
  }
}

// This function is called either by the destructor or unlink, which means that
// it can never be called when there is an outstanding ref to the
// nsIScriptContext on the stack. Our stack-scoped cx pushers hold such a ref,
// so we can assume here that mContext is not on the stack (and therefore not
// in use).
void
nsJSContext::DestroyJSContext()
{
  if (!mContext) {
    return;
  }

  // Clear our entry in the JSContext, bugzilla bug 66413
  ::JS_SetContextPrivate(mContext, nullptr);

  // Unregister our "javascript.options.*" pref-changed callback.
  Preferences::UnregisterCallback(JSOptionChangedCallback,
                                  js_options_dot_str, this);

  if (mGCOnDestruction) {
    PokeGC(JS::gcreason::NSJSCONTEXT_DESTROY);
  }

  JS_DestroyContextNoGC(mContext);
  mContext = nullptr;
  DropJSObjects(this);
}

// QueryInterface implementation for nsJSContext
NS_IMPL_CYCLE_COLLECTION_CLASS(nsJSContext)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSContext)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mWindowProxy)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSContext)
  NS_ASSERTION(!tmp->mContext || !js::ContextHasOutstandingRequests(tmp->mContext),
               "Trying to unlink a context with outstanding requests.");
  tmp->mIsInitialized = false;
  tmp->mGCOnDestruction = false;
  tmp->mWindowProxy = nullptr;
  tmp->DestroyJSContext();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobalObjectRef)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsJSContext)
  NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsJSContext, tmp->GetCCRefcnt())
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobalObjectRef)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsJSContext)
  NS_INTERFACE_MAP_ENTRY(nsIScriptContext)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF(nsJSContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsJSContext)

nsrefcnt
nsJSContext::GetCCRefcnt()
{
  nsrefcnt refcnt = mRefCnt.get();

  // In the (abnormal) case of synchronous cycle-collection, the context may be
  // actively running JS code in which case we must keep it alive by adding an
  // extra refcount.
  if (mContext && js::ContextHasOutstandingRequests(mContext)) {
    refcnt++;
  }

  return refcnt;
}

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

// Helper function to find the JSObject associated with a (presumably DOM)
// interface.
nsresult
nsJSContext::JSObjectFromInterface(nsISupports* aTarget,
                                   JS::Handle<JSObject*> aScope,
                                   JSObject** aRet)
{
  // It is legal to specify a null target.
  if (!aTarget) {
    *aRet = nullptr;
    return NS_OK;
  }

  AutoJSContext cx;

  // Get the jsobject associated with this target
  // We don't wrap here because we trust the JS engine to wrap the target
  // later.
  JS::Rooted<JS::Value> v(cx);
  nsresult rv = nsContentUtils::WrapNative(cx, aScope, aTarget, &v);
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject* obj = v.toObjectOrNull();
  if (obj) {
    JS::ExposeObjectToActiveJS(obj);
  }

#ifdef DEBUG
  JS::Rooted<JSObject*> rootedObj(cx, obj);
  nsCOMPtr<nsISupports> targetSupp = do_QueryInterface(aTarget);
  nsCOMPtr<nsISupports> native =
    nsContentUtils::XPConnect()->GetNativeOfWrapper(cx, rootedObj);
  NS_ASSERTION(native == targetSupp, "Native should be the target!");
  obj = rootedObj;
#endif

  *aRet = obj;
  return NS_OK;
}

nsresult
nsJSContext::BindCompiledEventHandler(nsISupports* aTarget,
                                      JS::Handle<JSObject*> aScope,
                                      JS::Handle<JSObject*> aHandler,
                                      JS::MutableHandle<JSObject*> aBoundHandler)
{
  NS_ENSURE_ARG(aHandler);
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);
  NS_PRECONDITION(!aBoundHandler, "Shouldn't already have a bound handler!");

  if (aScope) {
    JS::ExposeObjectToActiveJS(aScope);
  }
  JS::ExposeObjectToActiveJS(aHandler);
  AutoPushJSContext cx(mContext);

  // Get the jsobject associated with this target
  JS::Rooted<JSObject*> target(cx);
  JS::Rooted<JSObject*> scope(cx, aScope);
  nsresult rv = JSObjectFromInterface(aTarget, scope, target.address());
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  {
    JSAutoCompartment ac(cx, aHandler);
    JS::Rooted<JS::Value> val(cx, JS::ObjectValue(*aHandler));
    NS_ASSERTION(JS_TypeOfValue(cx, val) == JSTYPE_FUNCTION,
                 "Event handler object not a function");
  }
#endif

  JSAutoCompartment ac(cx, target);

  JSObject* funobj;
  // Make sure the handler function is parented by its event target object
  if (aHandler) {
    funobj = JS_CloneFunctionObject(cx, aHandler, target);
    if (!funobj) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    funobj = nullptr;
  }

  aBoundHandler.set(funobj);

  return rv;
}

nsIScriptGlobalObject *
nsJSContext::GetGlobalObject()
{
  AutoJSContext cx;
  JS::Rooted<JSObject*> global(mContext, GetWindowProxy());
  if (!global) {
    return nullptr;
  }

  if (mGlobalObjectRef)
    return mGlobalObjectRef;

#ifdef DEBUG
  {
    JSObject *inner = JS_ObjectToInnerObject(cx, global);

    // If this assertion hits then it means that we have a window object as
    // our global, but we never called CreateOuterObject.
    NS_ASSERTION(inner == global, "Shouldn't be able to innerize here");
  }
#endif

  const JSClass *c = JS_GetClass(global);

  nsCOMPtr<nsIScriptGlobalObject> sgo;
  if (IsDOMClass(c)) {
    sgo = do_QueryInterface(UnwrapDOMObjectToISupports(global));
  } else {
    if ((~c->flags) & (JSCLASS_HAS_PRIVATE |
                       JSCLASS_PRIVATE_IS_NSISUPPORTS)) {
      return nullptr;
    }

    nsISupports *priv = static_cast<nsISupports*>(js::GetObjectPrivate(global));

    nsCOMPtr<nsIXPConnectWrappedNative> wrapped_native =
      do_QueryInterface(priv);
    if (wrapped_native) {
      // The global object is a XPConnect wrapped native, the native in
      // the wrapper might be the nsIScriptGlobalObject

      sgo = do_QueryWrappedNative(wrapped_native);
    } else {
      sgo = do_QueryInterface(priv);
    }
  }

  // This'll return a pointer to something we're about to release, but
  // that's ok, the JS object will hold it alive long enough.
  return sgo;
}

JSContext*
nsJSContext::GetNativeContext()
{
  return mContext;
}

nsresult
nsJSContext::InitContext()
{
  // Make sure callers of this use
  // WillInitializeContext/DidInitializeContext around this call.
  NS_ENSURE_TRUE(!mIsInitialized, NS_ERROR_ALREADY_INITIALIZED);

  if (!mContext)
    return NS_ERROR_OUT_OF_MEMORY;

  ::JS_SetErrorReporter(mContext, NS_ScriptErrorReporter);

  JSOptionChangedCallback(js_options_dot_str, this);

  return NS_OK;
}

nsresult
nsJSContext::InitializeExternalClasses()
{
  nsScriptNameSpaceManager *nameSpaceManager = GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  return nameSpaceManager->InitForContext(this);
}

nsresult
nsJSContext::SetProperty(JS::Handle<JSObject*> aTarget, const char* aPropName, nsISupports* aArgs)
{
  nsCxPusher pusher;
  pusher.Push(mContext);

  JS::AutoValueVector args(mContext);

  JS::Rooted<JSObject*> global(mContext, GetWindowProxy());
  nsresult rv =
    ConvertSupportsTojsvals(aArgs, global, args);
  NS_ENSURE_SUCCESS(rv, rv);

  // got the arguments, now attach them.

  for (uint32_t i = 0; i < args.length(); ++i) {
    if (!JS_WrapValue(mContext, args.handleAt(i))) {
      return NS_ERROR_FAILURE;
    }
  }

  JSObject* array = ::JS_NewArrayObject(mContext, args);
  if (!array) {
    return NS_ERROR_FAILURE;
  }
  JS::Rooted<JS::Value> arrayVal(mContext, JS::ObjectValue(*array));

  return JS_DefineProperty(mContext, aTarget, aPropName, arrayVal,
                           nullptr, nullptr, 0) ? NS_OK : NS_ERROR_FAILURE;
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
      JS::MutableHandle<JS::Value> thisVal = aArgsOut.handleAt(argCtr);
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
          rv = nsContentUtils::WrapNative(cx, aScope, arg, thisVal);
        }
      }
    }
  } else {
    nsCOMPtr<nsIVariant> variant = do_QueryInterface(aArgs);
    if (variant) {
      rv = xpc->VariantToJS(cx, aScope, variant, aArgsOut.handleAt(0));
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

      *aArgv = STRING_TO_JSVAL(str);

      break;
    }
    case nsISupportsPrimitive::TYPE_STRING : {
      nsCOMPtr<nsISupportsString> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      nsAutoString data;

      p->GetData(data);

      // cast is probably safe since wchar_t and jschar are expected
      // to be equivalent; both unsigned 16-bit entities
      JSString *str =
        ::JS_NewUCStringCopyN(cx, data.get(), data.Length());
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      *aArgv = STRING_TO_JSVAL(str);
      break;
    }
    case nsISupportsPrimitive::TYPE_PRBOOL : {
      nsCOMPtr<nsISupportsPRBool> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      bool data;

      p->GetData(&data);

      *aArgv = BOOLEAN_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT8 : {
      nsCOMPtr<nsISupportsPRUint8> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      uint8_t data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT16 : {
      nsCOMPtr<nsISupportsPRUint16> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      uint16_t data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT32 : {
      nsCOMPtr<nsISupportsPRUint32> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      uint32_t data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_CHAR : {
      nsCOMPtr<nsISupportsChar> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      char data;

      p->GetData(&data);

      JSString *str = ::JS_NewStringCopyN(cx, &data, 1);
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      *aArgv = STRING_TO_JSVAL(str);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRINT16 : {
      nsCOMPtr<nsISupportsPRInt16> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      int16_t data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRINT32 : {
      nsCOMPtr<nsISupportsPRInt32> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      int32_t data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

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

      JS::Rooted<JSObject*> global(cx, GetWindowProxy());
      JS::Rooted<JS::Value> v(cx);
      nsresult rv = nsContentUtils::WrapNative(cx, global,
                                               data, iid, &v);
      NS_ENSURE_SUCCESS(rv, rv);

      *aArgv = v;

      break;
    }
    case nsISupportsPrimitive::TYPE_ID :
    case nsISupportsPrimitive::TYPE_PRUINT64 :
    case nsISupportsPrimitive::TYPE_PRINT64 :
    case nsISupportsPrimitive::TYPE_PRTIME :
    case nsISupportsPrimitive::TYPE_VOID : {
      NS_WARNING("Unsupported primitive type used");
      *aArgv = JSVAL_NULL;
      break;
    }
    default : {
      NS_WARNING("Unknown primitive type used");
      *aArgv = JSVAL_NULL;
      break;
    }
  }
  return NS_OK;
}

#ifdef NS_TRACE_MALLOC

#include <errno.h>              // XXX assume Linux if NS_TRACE_MALLOC
#include <fcntl.h>
#ifdef XP_UNIX
#include <unistd.h>
#endif
#ifdef XP_WIN32
#include <io.h>
#endif
#include "nsTraceMalloc.h"

static bool
CheckUniversalXPConnectForTraceMalloc(JSContext *cx)
{
    if (nsContentUtils::IsCallerChrome())
        return true;
    JS_ReportError(cx, "trace-malloc functions require UniversalXPConnect");
    return false;
}

static bool
TraceMallocDisable(JSContext *cx, unsigned argc, JS::Value *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return false;

    NS_TraceMallocDisable();
    args.rval().setUndefined();
    return true;
}

static bool
TraceMallocEnable(JSContext *cx, unsigned argc, JS::Value *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return false;

    NS_TraceMallocEnable();
    args.rval().setUndefined();
    return true;
}

static bool
TraceMallocOpenLogFile(JSContext *cx, unsigned argc, JS::Value *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return false;

    int fd;
    if (argc == 0) {
        fd = -1;
    } else {
        JSString *str = JS::ToString(cx, args[0]);
        if (!str)
            return false;
        JSAutoByteString filename(cx, str);
        if (!filename)
            return false;
        fd = open(filename.ptr(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            JS_ReportError(cx, "can't open %s: %s", filename.ptr(), strerror(errno));
            return false;
        }
    }
    args.rval().setInt32(fd);
    return true;
}

static bool
TraceMallocChangeLogFD(JSContext *cx, unsigned argc, JS::Value *vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return false;

    int32_t fd, oldfd;
    if (args.length() == 0) {
        oldfd = -1;
    } else {
        if (!JS::ToInt32(cx, args[0], &fd))
            return false;
        oldfd = NS_TraceMallocChangeLogFD(fd);
        if (oldfd == -2) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
    }
    args.rval().setInt32(oldfd);
    return true;
}

static bool
TraceMallocCloseLogFD(JSContext *cx, unsigned argc, JS::Value *vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return false;

    int32_t fd;
    if (args.length() == 0) {
        args.rval().setUndefined();
        return true;
    }
    if (!JS::ToInt32(cx, args[0], &fd))
        return false;
    NS_TraceMallocCloseLogFD((int) fd);
    args.rval().setInt32(fd);
    return true;
}

static bool
TraceMallocLogTimestamp(JSContext *cx, unsigned argc, JS::Value *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return false;

    JSString *str = JS::ToString(cx, args.get(0));
    if (!str)
        return false;
    JSAutoByteString caption(cx, str);
    if (!caption)
        return false;
    NS_TraceMallocLogTimestamp(caption.ptr());
    args.rval().setUndefined();
    return true;
}

static bool
TraceMallocDumpAllocations(JSContext *cx, unsigned argc, JS::Value *vp)
{
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return false;

    JSString *str = JS::ToString(cx, args.get(0));
    if (!str)
        return false;
    JSAutoByteString pathname(cx, str);
    if (!pathname)
        return false;
    if (NS_TraceMallocDumpAllocations(pathname.ptr()) < 0) {
        JS_ReportError(cx, "can't dump to %s: %s", pathname.ptr(), strerror(errno));
        return false;
    }
    args.rval().setUndefined();
    return true;
}

static const JSFunctionSpec TraceMallocFunctions[] = {
    JS_FS("TraceMallocDisable",         TraceMallocDisable,         0, 0),
    JS_FS("TraceMallocEnable",          TraceMallocEnable,          0, 0),
    JS_FS("TraceMallocOpenLogFile",     TraceMallocOpenLogFile,     1, 0),
    JS_FS("TraceMallocChangeLogFD",     TraceMallocChangeLogFD,     1, 0),
    JS_FS("TraceMallocCloseLogFD",      TraceMallocCloseLogFD,      1, 0),
    JS_FS("TraceMallocLogTimestamp",    TraceMallocLogTimestamp,    1, 0),
    JS_FS("TraceMallocDumpAllocations", TraceMallocDumpAllocations, 1, 0),
    JS_FS_END
};

#endif /* NS_TRACE_MALLOC */

#ifdef MOZ_DMD

#include <errno.h>

namespace mozilla {
namespace dmd {

// See https://wiki.mozilla.org/Performance/MemShrink/DMD for instructions on
// how to use DMD.

static bool
ReportAndDump(JSContext *cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JSString *str = JS::ToString(cx, args.get(0));
  if (!str)
    return false;
  JSAutoByteString pathname(cx, str);
  if (!pathname)
    return false;

  FILE* fp = fopen(pathname.ptr(), "w");
  if (!fp) {
    JS_ReportError(cx, "DMD can't open %s: %s",
                   pathname.ptr(), strerror(errno));
    return false;
  }

  dmd::ClearReports();
  fprintf(stderr, "DMD: running reporters...\n");
  dmd::RunReportersForThisProcess();
  dmd::Writer writer(FpWrite, fp);
  dmd::Dump(writer);

  fclose(fp);

  args.rval().setUndefined();
  return true;
}

} // namespace dmd
} // namespace mozilla

static const JSFunctionSpec DMDFunctions[] = {
    JS_FS("DMDReportAndDump", dmd::ReportAndDump, 1, 0),
    JS_FS_END
};

#endif  // defined(MOZ_DMD)

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
  nsresult rv = InitializeExternalClasses();
  NS_ENSURE_SUCCESS(rv, rv);

  JSOptionChangedCallback(js_options_dot_str, this);
  AutoPushJSContext cx(mContext);

  // Attempt to initialize profiling functions
  ::JS_DefineProfilingFunctions(cx, aGlobalObj);

#ifdef NS_TRACE_MALLOC
  if (nsContentUtils::IsCallerChrome()) {
    // Attempt to initialize TraceMalloc functions
    ::JS_DefineFunctions(cx, aGlobalObj, TraceMallocFunctions);
  }
#endif

#ifdef MOZ_DMD
  // Attempt to initialize DMD functions
  ::JS_DefineFunctions(cx, aGlobalObj, DMDFunctions);
#endif

#ifdef MOZ_JPROF
  // Attempt to initialize JProf functions
  ::JS_DefineFunctions(cx, aGlobalObj, JProfFunctions);
#endif

  return rv;
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
  NS_RELEASE(sFullGCTimer);

  uintptr_t reason = reinterpret_cast<uintptr_t>(aClosure);
  nsJSContext::GarbageCollectNow(static_cast<JS::gcreason::Reason>(reason),
                                 nsJSContext::IncrementalGC);
}

//static
void
nsJSContext::GarbageCollectNow(JS::gcreason::Reason aReason,
                               IsIncremental aIncremental,
                               IsCompartment aCompartment,
                               IsShrinking aShrinking,
                               int64_t aSliceMillis)
{
  PROFILER_LABEL("GC", "GarbageCollectNow");

  MOZ_ASSERT_IF(aSliceMillis, aIncremental == IncrementalGC);

  KillGCTimer();
  KillShrinkGCBuffersTimer();

  // Reset sPendingLoadCount in case the timer that fired was a
  // timer we scheduled due to a normal GC timer firing while
  // documents were loading. If this happens we're waiting for a
  // document that is taking a long time to load, and we effectively
  // ignore the fact that the currently loading documents are still
  // loading and move on as if they weren't.
  sPendingLoadCount = 0;
  sLoadingInProgress = false;

  if (!nsContentUtils::XPConnect() || !sRuntime) {
    return;
  }

  if (sCCLockedOut && aIncremental == IncrementalGC) {
    // We're in the middle of incremental GC. Do another slice.
    JS::PrepareForIncrementalGC(sRuntime);
    JS::IncrementalGC(sRuntime, aReason, aSliceMillis);
    return;
  }

  JS::PrepareForFullGC(sRuntime);
  if (aIncremental == IncrementalGC) {
    MOZ_ASSERT(aShrinking == NonShrinkingGC);
    JS::IncrementalGC(sRuntime, aReason, aSliceMillis);
  } else if (aShrinking == ShrinkingGC) {
    JS::ShrinkingGC(sRuntime, aReason);
  } else {
    JS::GCForReason(sRuntime, aReason);
  }
}

//static
void
nsJSContext::ShrinkGCBuffersNow()
{
  PROFILER_LABEL("GC", "ShrinkGCBuffersNow");

  KillShrinkGCBuffersTimer();

  JS::ShrinkGCBuffers(sRuntime);
}

static void
FinishAnyIncrementalGC()
{
  if (sCCLockedOut) {
    // We're in the middle of an incremental GC, so finish it.
    JS::PrepareForIncrementalGC(sRuntime);
    JS::FinishIncrementalGC(sRuntime, JS::gcreason::CC_FORCED);
  }
}

static void
FireForgetSkippable(uint32_t aSuspected, bool aRemoveChildless)
{
  PRTime startTime = PR_Now();
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
  void Clear()
  {
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

  void PrepareForCycleCollectionSlice(int32_t aExtraForgetSkippableCalls = 0);

  void FinishCycleCollectionSlice()
  {
    if (mBeginSliceTime.IsNull()) {
      // We already called this method from EndCycleCollectionCallback for this slice.
      return;
    }

    mEndSliceTime = TimeStamp::Now();
    uint32_t sliceTime = TimeBetween(mBeginSliceTime, mEndSliceTime);
    mMaxSliceTime = std::max(mMaxSliceTime, sliceTime);
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

  // The total amount of time spent actually running the current CC.
  uint32_t mTotalSliceTime;

  // True if we were locked out by the GC in any slice of the current CC.
  bool mAnyLockedOut;

  int32_t mExtraForgetSkippableCalls;
};

CycleCollectorStats gCCStats;

void
CycleCollectorStats::PrepareForCycleCollectionSlice(int32_t aExtraForgetSkippableCalls)
{
  mBeginSliceTime = TimeStamp::Now();

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
      FireForgetSkippable(nsCycleCollector_suspectedCount(), false);
      ranSyncForgetSkippable = true;
    }

    for (int32_t i = 0; i < mExtraForgetSkippableCalls; ++i) {
      FireForgetSkippable(nsCycleCollector_suspectedCount(), false);
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

  PROFILER_LABEL("CC", "CycleCollectNow");
  gCCStats.PrepareForCycleCollectionSlice(aExtraForgetSkippableCalls);
  nsCycleCollector_collect(aListener);
  gCCStats.FinishCycleCollectionSlice();
}

//static
void
nsJSContext::RunCycleCollectorSlice()
{
  if (!NS_IsMainThread()) {
    return;
  }

  PROFILER_LABEL("CC", "RunCycleCollectorSlice");

  gCCStats.PrepareForCycleCollectionSlice();

  // Decide how long we want to budget for this slice. By default,
  // use an unlimited budget.
  int64_t sliceBudget = -1;

  if (sIncrementalCC) {
    if (gCCStats.mBeginTime.IsNull()) {
      // If no CC is in progress, use the standard slice time.
      sliceBudget = kICCSliceBudget;
    } else {
      TimeStamp now = TimeStamp::Now();

      // Only run a limited slice if we're within the max running time.
      if (TimeBetween(gCCStats.mBeginTime, now) < kMaxICCDuration) {
        float sliceMultiplier = std::max(TimeBetween(gCCStats.mEndSliceTime, now) / (float)kICCIntersliceDelay, 1.0f);
        sliceBudget = kICCSliceBudget * sliceMultiplier;
      }
    }
  }

  nsCycleCollector_collectSlice(sliceBudget);

  gCCStats.FinishCycleCollectionSlice();
}

static void
ICCTimerFired(nsITimer* aTimer, void* aClosure)
{
  if (sDidShutdown) {
    return;
  }

  // Ignore ICC timer fires during IGC. Running ICC during an IGC will cause us
  // to synchronously finish the GC, which is bad.

  if (sCCLockedOut) {
    PRTime now = PR_Now();
    if (sCCLockedOutTime == 0) {
      sCCLockedOutTime = now;
      return;
    }
    if (now - sCCLockedOutTime < NS_MAX_CC_LOCKEDOUT_TIME) {
      return;
    }
  }

  nsJSContext::RunCycleCollectorSlice();
}

//static
void
nsJSContext::BeginCycleCollectionCallback()
{
  MOZ_ASSERT(NS_IsMainThread());

  gCCStats.mBeginTime = gCCStats.mBeginSliceTime.IsNull() ? TimeStamp::Now() : gCCStats.mBeginSliceTime;
  gCCStats.mSuspected = nsCycleCollector_suspectedCount();

  KillCCTimer();

  gCCStats.RunForgetSkippable();

  MOZ_ASSERT(!sICCTimer, "Tried to create a new ICC timer when one already existed.");

  if (!sIncrementalCC) {
    return;
  }

  CallCreateInstance("@mozilla.org/timer;1", &sICCTimer);
  if (sICCTimer) {
    sICCTimer->InitWithFuncCallback(ICCTimerFired,
                                    nullptr,
                                    kICCIntersliceDelay,
                                    nsITimer::TYPE_REPEATING_SLACK);
  }
}

//static
void
nsJSContext::EndCycleCollectionCallback(CycleCollectorResults &aResults)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsJSContext::KillICCTimer();

  // Update timing information for the current slice before we log it, if
  // we previously called PrepareForCycleCollectionSlice(). During shutdown
  // CCs, this won't happen.
  gCCStats.FinishCycleCollectionSlice();

  sCCollectedWaitingForGC += aResults.mFreedRefCounted + aResults.mFreedGCed;

  if (NeedsGCAfterCC()) {
    PokeGC(JS::gcreason::CC_WAITING);
  }

  TimeStamp endCCTimeStamp = TimeStamp::Now();

  PRTime endCCTime;
  if (sPostGCEventsToObserver) {
    endCCTime = PR_Now();
  }

  // Log information about the CC via telemetry, JSON and the console.
  uint32_t ccNowDuration = TimeBetween(gCCStats.mBeginTime, endCCTimeStamp);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_FINISH_IGC, gCCStats.mAnyLockedOut);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_SYNC_SKIPPABLE, gCCStats.mRanSyncForgetSkippable);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_FULL, ccNowDuration);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_MAX_PAUSE, gCCStats.mMaxSliceTime);

  if (!sLastCCEndTime.IsNull()) {
    uint32_t timeBetween = TimeBetween(sLastCCEndTime, gCCStats.mBeginTime);
    Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_TIME_BETWEEN, timeBetween);
  }
  sLastCCEndTime = endCCTimeStamp;

  Telemetry::Accumulate(Telemetry::FORGET_SKIPPABLE_MAX,
                        sMaxForgetSkippableTime / PR_USEC_PER_MSEC);

  PRTime delta = GetCollectionTimeDelta();

  uint32_t cleanups = sForgetSkippableBeforeCC ? sForgetSkippableBeforeCC : 1;
  uint32_t minForgetSkippableTime = (sMinForgetSkippableTime == UINT32_MAX)
    ? 0 : sMinForgetSkippableTime;

  if (sPostGCEventsToConsole) {
    nsCString mergeMsg;
    if (aResults.mMergedZones) {
      mergeMsg.AssignLiteral(" merged");
    }

    nsCString gcMsg;
    if (aResults.mForcedGC) {
      gcMsg.AssignLiteral(", forced a GC");
    }

    NS_NAMED_MULTILINE_LITERAL_STRING(kFmt,
      MOZ_UTF16("CC(T+%.1f) max pause: %lums, total time: %lums, suspected: %lu, visited: %lu RCed and %lu%s GCed, collected: %lu RCed and %lu GCed (%lu|%lu waiting for GC)%s\n")
      MOZ_UTF16("ForgetSkippable %lu times before CC, min: %lu ms, max: %lu ms, avg: %lu ms, total: %lu ms, max sync: %lu ms, removed: %lu"));
    nsString msg;
    msg.Adopt(nsTextFormatter::smprintf(kFmt.get(), double(delta) / PR_USEC_PER_SEC,
                                        gCCStats.mMaxSliceTime, gCCStats.mTotalSliceTime,
                                        gCCStats.mSuspected,
                                        aResults.mVisitedRefCounted, aResults.mVisitedGCed, mergeMsg.get(),
                                        aResults.mFreedRefCounted, aResults.mFreedGCed,
                                        sCCollectedWaitingForGC, sLikelyShortLivingObjectsNeedingGC,
                                        gcMsg.get(),
                                        sForgetSkippableBeforeCC,
                                        minForgetSkippableTime / PR_USEC_PER_MSEC,
                                        sMaxForgetSkippableTime / PR_USEC_PER_MSEC,
                                        (sTotalForgetSkippableTime / cleanups) /
                                          PR_USEC_PER_MSEC,
                                        sTotalForgetSkippableTime / PR_USEC_PER_MSEC,
                                        gCCStats.mMaxSkippableDuration, sRemovedPurples));
    nsCOMPtr<nsIConsoleService> cs =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (cs) {
      cs->LogStringMessage(msg.get());
    }
  }

  if (sPostGCEventsToObserver) {
    NS_NAMED_MULTILINE_LITERAL_STRING(kJSONFmt,
       MOZ_UTF16("{ \"timestamp\": %llu, ")
         MOZ_UTF16("\"duration\": %lu, ")
         MOZ_UTF16("\"max_slice_pause\": %lu, ")
         MOZ_UTF16("\"total_slice_pause\": %lu, ")
         MOZ_UTF16("\"max_finish_gc_duration\": %lu, ")
         MOZ_UTF16("\"max_sync_skippable_duration\": %lu, ")
         MOZ_UTF16("\"suspected\": %lu, ")
         MOZ_UTF16("\"visited\": { ")
             MOZ_UTF16("\"RCed\": %lu, ")
             MOZ_UTF16("\"GCed\": %lu }, ")
         MOZ_UTF16("\"collected\": { ")
             MOZ_UTF16("\"RCed\": %lu, ")
             MOZ_UTF16("\"GCed\": %lu }, ")
         MOZ_UTF16("\"waiting_for_gc\": %lu, ")
         MOZ_UTF16("\"short_living_objects_waiting_for_gc\": %lu, ")
         MOZ_UTF16("\"forced_gc\": %d, ")
         MOZ_UTF16("\"forget_skippable\": { ")
             MOZ_UTF16("\"times_before_cc\": %lu, ")
             MOZ_UTF16("\"min\": %lu, ")
             MOZ_UTF16("\"max\": %lu, ")
             MOZ_UTF16("\"avg\": %lu, ")
             MOZ_UTF16("\"total\": %lu, ")
             MOZ_UTF16("\"removed\": %lu } ")
       MOZ_UTF16("}"));
    nsString json;
    json.Adopt(nsTextFormatter::smprintf(kJSONFmt.get(), endCCTime, ccNowDuration,
                                         gCCStats.mMaxSliceTime,
                                         gCCStats.mTotalSliceTime,
                                         gCCStats.mMaxGCDuration,
                                         gCCStats.mMaxSkippableDuration,
                                         gCCStats.mSuspected,
                                         aResults.mVisitedRefCounted, aResults.mVisitedGCed,
                                         aResults.mFreedRefCounted, aResults.mFreedGCed,
                                         sCCollectedWaitingForGC,
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
void
InterSliceGCTimerFired(nsITimer *aTimer, void *aClosure)
{
  NS_RELEASE(sInterSliceGCTimer);
  nsJSContext::GarbageCollectNow(JS::gcreason::INTER_SLICE_GC,
                                 nsJSContext::IncrementalGC,
                                 nsJSContext::CompartmentGC,
                                 nsJSContext::NonShrinkingGC,
                                 NS_INTERSLICE_GC_BUDGET);
}

// static
void
GCTimerFired(nsITimer *aTimer, void *aClosure)
{
  NS_RELEASE(sGCTimer);

  uintptr_t reason = reinterpret_cast<uintptr_t>(aClosure);
  nsJSContext::GarbageCollectNow(static_cast<JS::gcreason::Reason>(reason),
                                 nsJSContext::IncrementalGC,
                                 nsJSContext::CompartmentGC);
}

void
ShrinkGCBuffersTimerFired(nsITimer *aTimer, void *aClosure)
{
  NS_RELEASE(sShrinkGCBuffersTimer);

  nsJSContext::ShrinkGCBuffersNow();
}

static bool
ShouldTriggerCC(uint32_t aSuspected)
{
  return sNeedsFullCC ||
         aSuspected > NS_CC_PURPLE_LIMIT ||
         (aSuspected > NS_CC_FORCED_PURPLE_LIMIT &&
          TimeUntilNow(sLastCCEndTime) > NS_CC_FORCED);
}

static uint32_t
TimeToNextCC()
{
  if (sIncrementalCC) {
    return NS_CC_DELAY - kMaxICCDuration;
  }
  return NS_CC_DELAY;
}

static_assert(NS_CC_DELAY > kMaxICCDuration, "ICC shouldn't reduce CC delay to 0");

static void
CCTimerFired(nsITimer *aTimer, void *aClosure)
{
  if (sDidShutdown) {
    return;
  }

  static uint32_t ccDelay = NS_CC_DELAY;
  if (sCCLockedOut) {
    ccDelay = TimeToNextCC() / 3;

    PRTime now = PR_Now();
    if (sCCLockedOutTime == 0) {
      // Reset sCCTimerFireCount so that we run forgetSkippable
      // often enough before CC. Because of reduced ccDelay
      // forgetSkippable will be called just a few times.
      // NS_MAX_CC_LOCKEDOUT_TIME limit guarantees that we end up calling
      // forgetSkippable and CycleCollectNow eventually.
      sCCTimerFireCount = 0;
      sCCLockedOutTime = now;
      return;
    }
    if (now - sCCLockedOutTime < NS_MAX_CC_LOCKEDOUT_TIME) {
      return;
    }
  }

  ++sCCTimerFireCount;

  // During early timer fires, we only run forgetSkippable. During the first
  // late timer fire, we decide if we are going to have a second and final
  // late timer fire, where we may begin to run the CC. Should run at least one
  // early timer fire to allow cleanup before the CC.
  int32_t numEarlyTimerFires = std::max((int32_t)ccDelay / NS_CC_SKIPPABLE_DELAY - 2, 1);
  bool isLateTimerFire = sCCTimerFireCount > numEarlyTimerFires;
  uint32_t suspected = nsCycleCollector_suspectedCount();
  if (isLateTimerFire && ShouldTriggerCC(suspected)) {
    if (sCCTimerFireCount == numEarlyTimerFires + 1) {
      FireForgetSkippable(suspected, true);
      if (ShouldTriggerCC(nsCycleCollector_suspectedCount())) {
        // Our efforts to avoid a CC have failed, so we return to let the
        // timer fire once more to trigger a CC.
        return;
      }
    } else {
      // We are in the final timer fire and still meet the conditions for
      // triggering a CC. Let RunCycleCollectorSlice finish the current IGC, if
      // any because that will allow us to include the GC time in the CC pause.
      nsJSContext::RunCycleCollectorSlice();
    }
  } else if ((sPreviousSuspectedCount + 100) <= suspected) {
      // Only do a forget skippable if there are more than a few new objects.
      FireForgetSkippable(suspected, false);
  }

  if (isLateTimerFire) {
    ccDelay = TimeToNextCC();

    // We have either just run the CC or decided we don't want to run the CC
    // next time, so kill the timer.
    sPreviousSuspectedCount = 0;
    nsJSContext::KillCCTimer();
  }
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

  // Its probably a good idea to GC soon since we have finished loading.
  sLoadingInProgress = false;
  PokeGC(JS::gcreason::LOAD_END);
}

// static
void
nsJSContext::PokeGC(JS::gcreason::Reason aReason, int aDelay)
{
  if (sGCTimer || sInterSliceGCTimer || sShuttingDown) {
    // There's already a timer for GC'ing, just return
    return;
  }

  if (sCCTimer) {
    // Make sure CC is called...
    sNeedsFullCC = true;
    // and GC after it.
    sNeedsGCAfterCC = true;
    return;
  }

  if (sICCTimer) {
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

  sGCTimer->InitWithFuncCallback(GCTimerFired, reinterpret_cast<void *>(aReason),
                                 aDelay
                                 ? aDelay
                                 : (first
                                    ? NS_FIRST_GC_DELAY
                                    : NS_GC_DELAY),
                                 nsITimer::TYPE_ONE_SHOT);

  first = false;
}

// static
void
nsJSContext::PokeShrinkGCBuffers()
{
  if (sShrinkGCBuffersTimer || sShuttingDown) {
    return;
  }

  CallCreateInstance("@mozilla.org/timer;1", &sShrinkGCBuffersTimer);

  if (!sShrinkGCBuffersTimer) {
    // Failed to create timer (probably because we're in XPCOM shutdown)
    return;
  }

  sShrinkGCBuffersTimer->InitWithFuncCallback(ShrinkGCBuffersTimerFired, nullptr,
                                              NS_SHRINK_GC_BUFFERS_DELAY,
                                              nsITimer::TYPE_ONE_SHOT);
}

// static
void
nsJSContext::MaybePokeCC()
{
  if (sCCTimer || sICCTimer || sShuttingDown || !sHasRunGC) {
    return;
  }

  if (ShouldTriggerCC(nsCycleCollector_suspectedCount())) {
    sCCTimerFireCount = 0;
    CallCreateInstance("@mozilla.org/timer;1", &sCCTimer);
    if (!sCCTimer) {
      return;
    }
    // We can kill some objects before running forgetSkippable.
    nsCycleCollector_dispatchDeferredDeletion();

    sCCTimer->InitWithFuncCallback(CCTimerFired, nullptr,
                                   NS_CC_SKIPPABLE_DELAY,
                                   nsITimer::TYPE_REPEATING_SLACK);
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
nsJSContext::KillInterSliceGCTimer()
{
  if (sInterSliceGCTimer) {
    sInterSliceGCTimer->Cancel();
    NS_RELEASE(sInterSliceGCTimer);
  }
}

//static
void
nsJSContext::KillShrinkGCBuffersTimer()
{
  if (sShrinkGCBuffersTimer) {
    sShrinkGCBuffersTimer->Cancel();

    NS_RELEASE(sShrinkGCBuffersTimer);
  }
}

//static
void
nsJSContext::KillCCTimer()
{
  sCCLockedOutTime = 0;

  if (sCCTimer) {
    sCCTimer->Cancel();

    NS_RELEASE(sCCTimer);
  }
}

//static
void
nsJSContext::KillICCTimer()
{
  sCCLockedOutTime = 0;

  if (sICCTimer) {
    sICCTimer->Cancel();

    NS_RELEASE(sICCTimer);
  }
}

void
nsJSContext::GC(JS::gcreason::Reason aReason)
{
  PokeGC(aReason);
}

class NotifyGCEndRunnable : public nsRunnable
{
  nsString mMessage;

public:
  NotifyGCEndRunnable(const nsString& aMessage) : mMessage(aMessage) {}

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

  const jschar oomMsg[3] = { '{', '}', 0 };
  const jschar *toSend = mMessage.get() ? mMessage.get() : oomMsg;
  observerService->NotifyObservers(nullptr, "garbage-collection-statistics", toSend);

  return NS_OK;
}

static void
DOMGCSliceCallback(JSRuntime *aRt, JS::GCProgress aProgress, const JS::GCDescription &aDesc)
{
  NS_ASSERTION(NS_IsMainThread(), "GCs must run on the main thread");

  if (aProgress == JS::GC_CYCLE_END) {
    PRTime delta = GetCollectionTimeDelta();

    if (sPostGCEventsToConsole) {
      NS_NAMED_LITERAL_STRING(kFmt, "GC(T+%.1f) ");
      nsString prefix, gcstats;
      gcstats.Adopt(aDesc.formatMessage(aRt));
      prefix.Adopt(nsTextFormatter::smprintf(kFmt.get(),
                                             double(delta) / PR_USEC_PER_SEC));
      nsString msg = prefix + gcstats;
      nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
      if (cs) {
        cs->LogStringMessage(msg.get());
      }
    }

    if (sPostGCEventsToObserver) {
      nsString json;
      json.Adopt(aDesc.formatJSON(aRt, PR_Now()));
      nsRefPtr<NotifyGCEndRunnable> notify = new NotifyGCEndRunnable(json);
      NS_DispatchToMainThread(notify);
    }
  }

  // Prevent cycle collections and shrinking during incremental GC.
  if (aProgress == JS::GC_CYCLE_BEGIN) {
    sCCLockedOut = true;
    nsJSContext::KillShrinkGCBuffersTimer();
  } else if (aProgress == JS::GC_CYCLE_END) {
    sCCLockedOut = false;
  }

  // The GC has more work to do, so schedule another GC slice.
  if (aProgress == JS::GC_SLICE_END) {
    nsJSContext::KillInterSliceGCTimer();
    if (!sShuttingDown) {
      CallCreateInstance("@mozilla.org/timer;1", &sInterSliceGCTimer);
      sInterSliceGCTimer->InitWithFuncCallback(InterSliceGCTimerFired,
                                               nullptr,
                                               NS_INTERSLICE_GC_DELAY,
                                               nsITimer::TYPE_ONE_SHOT);
    }
  }

  if (aProgress == JS::GC_CYCLE_END) {
    // May need to kill the inter-slice GC timer
    nsJSContext::KillInterSliceGCTimer();

    sCCollectedWaitingForGC = 0;
    sLikelyShortLivingObjectsNeedingGC = 0;
    sCleanupsSinceLastGC = 0;
    sNeedsFullCC = true;
    sHasRunGC = true;
    nsJSContext::MaybePokeCC();

    if (aDesc.isCompartment_) {
      if (!sFullGCTimer && !sShuttingDown) {
        CallCreateInstance("@mozilla.org/timer;1", &sFullGCTimer);
        JS::gcreason::Reason reason = JS::gcreason::FULL_GC_TIMER;
        sFullGCTimer->InitWithFuncCallback(FullGCTimerFired,
                                           reinterpret_cast<void *>(reason),
                                           NS_FULL_GC_DELAY,
                                           nsITimer::TYPE_ONE_SHOT);
      }
    } else {
      nsJSContext::KillFullGCTimer();

      // Avoid shrinking during heavy activity, which is suggested by
      // compartment GC.
      nsJSContext::PokeShrinkGCBuffers();
    }
  }

  if ((aProgress == JS::GC_SLICE_END || aProgress == JS::GC_CYCLE_END) &&
      ShouldTriggerCC(nsCycleCollector_suspectedCount())) {
    nsCycleCollector_dispatchDeferredDeletion();
  }

  if (sPrevGCSliceCallback)
    (*sPrevGCSliceCallback)(aRt, aProgress, aDesc);
}

void
nsJSContext::ReportPendingException()
{
  if (mIsInitialized) {
    nsJSUtils::ReportPendingException(mContext);
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
  JSObject* windowProxy = GetWindowProxyPreserveColor();
  if (windowProxy) {
    JS::ExposeObjectToActiveJS(windowProxy);
  }

  return windowProxy;
}

JSObject*
nsJSContext::GetWindowProxyPreserveColor()
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
  sGCTimer = sFullGCTimer = sCCTimer = sICCTimer = nullptr;
  sCCLockedOut = false;
  sCCLockedOutTime = 0;
  sLastCCEndTime = TimeStamp();
  sHasRunGC = false;
  sPendingLoadCount = 0;
  sLoadingInProgress = false;
  sCCollectedWaitingForGC = 0;
  sLikelyShortLivingObjectsNeedingGC = 0;
  sPostGCEventsToConsole = false;
  sNeedsFullCC = false;
  sNeedsGCAfterCC = false;
  gNameSpaceManager = nullptr;
  sRuntimeService = nullptr;
  sRuntime = nullptr;
  sIsInitialized = false;
  sDidShutdown = false;
  sShuttingDown = false;
  sContextCount = 0;
  sSecurityManager = nullptr;
  gCCStats.Clear();
}

static void
ReportAllJSExceptionsPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool reportAll = Preferences::GetBool(aPrefName, false);
  nsContentUtils::XPConnect()->SetReportAllJSExceptions(reportAll);
}

static void
SetMemoryHighWaterMarkPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  int32_t highwatermark = Preferences::GetInt(aPrefName, 128);

  JS_SetGCParameter(sRuntime, JSGC_MAX_MALLOC_BYTES,
                    highwatermark * 1024L * 1024L);
}

static void
SetMemoryMaxPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  int32_t pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  uint32_t max = (pref <= 0 || pref >= 0x1000) ? -1 : (uint32_t)pref * 1024 * 1024;
  JS_SetGCParameter(sRuntime, JSGC_MAX_BYTES, max);
}

static void
SetMemoryGCModePrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool enableCompartmentGC = Preferences::GetBool("javascript.options.mem.gc_per_compartment");
  bool enableIncrementalGC = Preferences::GetBool("javascript.options.mem.gc_incremental");
  JSGCMode mode;
  if (enableIncrementalGC) {
    mode = JSGC_MODE_INCREMENTAL;
  } else if (enableCompartmentGC) {
    mode = JSGC_MODE_COMPARTMENT;
  } else {
    mode = JSGC_MODE_GLOBAL;
  }
  JS_SetGCParameter(sRuntime, JSGC_MODE, mode);
}

static void
SetMemoryGCSliceTimePrefChangedCallback(const char* aPrefName, void* aClosure)
{
  int32_t pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  if (pref > 0 && pref < 100000)
    JS_SetGCParameter(sRuntime, JSGC_SLICE_TIME_BUDGET, pref);
}

static void
SetMemoryGCPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  int32_t pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  if (pref >= 0 && pref < 10000)
    JS_SetGCParameter(sRuntime, (JSGCParamKey)(intptr_t)aClosure, pref);
}

static void
SetMemoryGCDynamicHeapGrowthPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool pref = Preferences::GetBool(aPrefName);
  JS_SetGCParameter(sRuntime, JSGC_DYNAMIC_HEAP_GROWTH, pref);
}

static void
SetMemoryGCDynamicMarkSlicePrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool pref = Preferences::GetBool(aPrefName);
  JS_SetGCParameter(sRuntime, JSGC_DYNAMIC_MARK_SLICE, pref);
}

static void
SetIncrementalCCPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool pref = Preferences::GetBool(aPrefName);
  sIncrementalCC = pref;
}

JSObject*
NS_DOMReadStructuredClone(JSContext* cx,
                          JSStructuredCloneReader* reader,
                          uint32_t tag,
                          uint32_t data,
                          void* closure)
{
  if (tag == SCTAG_DOM_IMAGEDATA) {
    // Read the information out of the stream.
    uint32_t width, height;
    JS::Rooted<JS::Value> dataArray(cx);
    if (!JS_ReadUint32Pair(reader, &width, &height) ||
        !JS_ReadTypedArray(reader, &dataArray)) {
      return nullptr;
    }
    MOZ_ASSERT(dataArray.isObject());

    // Construct the ImageData.
    nsRefPtr<ImageData> imageData = new ImageData(width, height,
                                                  dataArray.toObject());
    // Wrap it in a JS::Value.
    JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
    if (!global) {
      return nullptr;
    }
    return imageData->WrapObject(cx, global);
  }

  // Don't know what this is. Bail.
  xpc::Throw(cx, NS_ERROR_DOM_DATA_CLONE_ERR);
  return nullptr;
}

bool
NS_DOMWriteStructuredClone(JSContext* cx,
                           JSStructuredCloneWriter* writer,
                           JS::Handle<JSObject*> obj,
                           void *closure)
{
  ImageData* imageData;
  nsresult rv = UNWRAP_OBJECT(ImageData, obj, imageData);
  if (NS_FAILED(rv)) {
    // Don't know what this is. Bail.
    xpc::Throw(cx, NS_ERROR_DOM_DATA_CLONE_ERR);
    return false;
  }

  // Prepare the ImageData internals.
  uint32_t width = imageData->Width();
  uint32_t height = imageData->Height();
  JS::Rooted<JSObject*> dataArray(cx, imageData->GetDataObject());

  // Write the internals to the stream.
  JSAutoCompartment ac(cx, dataArray);
  return JS_WriteUint32Pair(writer, SCTAG_DOM_IMAGEDATA, 0) &&
         JS_WriteUint32Pair(writer, width, height) &&
         JS_WriteTypedArray(writer, JS::ObjectValue(*dataArray));
}

void
NS_DOMStructuredCloneError(JSContext* cx,
                           uint32_t errorid)
{
  // We don't currently support any extensions to structured cloning.
  xpc::Throw(cx, NS_ERROR_DOM_DATA_CLONE_ERR);
}

static bool
AsmJSCacheOpenEntryForRead(JS::Handle<JSObject*> aGlobal,
                           const jschar* aBegin,
                           const jschar* aLimit,
                           size_t* aSize,
                           const uint8_t** aMemory,
                           intptr_t *aHandle)
{
  nsIPrincipal* principal = nsContentUtils::GetObjectPrincipal(aGlobal);
  return asmjscache::OpenEntryForRead(principal, aBegin, aLimit, aSize, aMemory,
                                      aHandle);
}

static bool
AsmJSCacheOpenEntryForWrite(JS::Handle<JSObject*> aGlobal,
                            bool aInstalled,
                            const jschar* aBegin,
                            const jschar* aEnd,
                            size_t aSize,
                            uint8_t** aMemory,
                            intptr_t* aHandle)
{
  nsIPrincipal* principal = nsContentUtils::GetObjectPrincipal(aGlobal);
  return asmjscache::OpenEntryForWrite(principal, aInstalled, aBegin, aEnd,
                                       aSize, aMemory, aHandle);
}

static void
OnLargeAllocationFailure()
{
  nsCOMPtr<nsIObserverService> os =
    mozilla::services::GetObserverService();
  if (os) {
    os->NotifyObservers(nullptr, "memory-pressure", MOZ_UTF16("heap-minimize"));
  }
}

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

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

  rv = CallGetService(kJSRuntimeServiceContractID, &sRuntimeService);
  if (NS_FAILED(rv)) {
    MOZ_CRASH();
  }

  rv = sRuntimeService->GetRuntime(&sRuntime);
  if (NS_FAILED(rv)) {
    MOZ_CRASH();
  }

  // Let's make sure that our main thread is the same as the xpcom main thread.
  MOZ_ASSERT(NS_IsMainThread());

  sPrevGCSliceCallback = JS::SetGCSliceCallback(sRuntime, DOMGCSliceCallback);

  // Set up the structured clone callbacks.
  static JSStructuredCloneCallbacks cloneCallbacks = {
    NS_DOMReadStructuredClone,
    NS_DOMWriteStructuredClone,
    NS_DOMStructuredCloneError
  };
  JS_SetStructuredCloneCallbacks(sRuntime, &cloneCallbacks);

  static js::DOMCallbacks DOMcallbacks = {
    InstanceClassHasProtoAtDepth
  };
  SetDOMCallbacks(sRuntime, &DOMcallbacks);

  // Set up the asm.js cache callbacks
  static JS::AsmJSCacheOps asmJSCacheOps = {
    AsmJSCacheOpenEntryForRead,
    asmjscache::CloseEntryForRead,
    AsmJSCacheOpenEntryForWrite,
    asmjscache::CloseEntryForWrite,
    asmjscache::GetBuildId
  };
  JS::SetAsmJSCacheOps(sRuntime, &asmJSCacheOps);

  JS::SetLargeAllocationFailureCallback(sRuntime, OnLargeAllocationFailure);

  // Set these global xpconnect options...
  Preferences::RegisterCallbackAndCall(ReportAllJSExceptionsPrefChangedCallback,
                                       "dom.report_all_js_exceptions");

  Preferences::RegisterCallbackAndCall(SetMemoryHighWaterMarkPrefChangedCallback,
                                       "javascript.options.mem.high_water_mark");

  Preferences::RegisterCallbackAndCall(SetMemoryMaxPrefChangedCallback,
                                       "javascript.options.mem.max");

  Preferences::RegisterCallbackAndCall(SetMemoryGCModePrefChangedCallback,
                                       "javascript.options.mem.gc_per_compartment");

  Preferences::RegisterCallbackAndCall(SetMemoryGCModePrefChangedCallback,
                                       "javascript.options.mem.gc_incremental");

  Preferences::RegisterCallbackAndCall(SetMemoryGCSliceTimePrefChangedCallback,
                                       "javascript.options.mem.gc_incremental_slice_ms");

  Preferences::RegisterCallbackAndCall(SetMemoryGCPrefChangedCallback,
                                       "javascript.options.mem.gc_high_frequency_time_limit_ms",
                                       (void *)JSGC_HIGH_FREQUENCY_TIME_LIMIT);

  Preferences::RegisterCallbackAndCall(SetMemoryGCDynamicMarkSlicePrefChangedCallback,
                                       "javascript.options.mem.gc_dynamic_mark_slice");

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

  Preferences::RegisterCallbackAndCall(SetMemoryGCPrefChangedCallback,
                                       "javascript.options.mem.gc_decommit_threshold_mb",
                                       (void *)JSGC_DECOMMIT_THRESHOLD);

  Preferences::RegisterCallbackAndCall(SetIncrementalCCPrefChangedCallback,
                                       "dom.cycle_collector.incremental");

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    MOZ_CRASH();
  }

  Preferences::AddBoolVarCache(&sGCOnMemoryPressure,
                               "javascript.options.gc_on_memory_pressure",
                               true);

  nsIObserver* observer = new nsJSEnvironmentObserver();
  obs->AddObserver(observer, "memory-pressure", false);
  obs->AddObserver(observer, "quit-application", false);

  // Bug 907848 - We need to explicitly get the nsIDOMScriptObjectFactory
  // service in order to force its constructor to run, which registers a
  // shutdown observer. It would be nice to make this more explicit and less
  // side-effect-y.
  nsCOMPtr<nsIDOMScriptObjectFactory> factory = do_GetService(kDOMScriptObjectFactoryCID);
  if (!factory) {
    MOZ_CRASH();
  }

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

void
mozilla::dom::ShutdownJSEnvironment()
{
  KillTimers();

  NS_IF_RELEASE(gNameSpaceManager);

  if (!sContextCount) {
    // We're being shutdown, and there are no more contexts
    // alive, release the JS runtime service and the security manager.

    NS_IF_RELEASE(sRuntimeService);
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
class nsJSArgArray MOZ_FINAL : public nsIJSArgArray {
public:
  nsJSArgArray(JSContext *aContext, uint32_t argc, JS::Value *argv,
               nsresult *prv);
  ~nsJSArgArray();
  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsJSArgArray,
                                                         nsIJSArgArray)

  // nsIArray
  NS_DECL_NSIARRAY

  // nsIJSArgArray
  nsresult GetArgs(uint32_t *argc, void **argv);

  void ReleaseJSObjects();

protected:
  JSContext *mContext;
  JS::Heap<JS::Value> *mArgv;
  uint32_t mArgc;
};

nsJSArgArray::nsJSArgArray(JSContext *aContext, uint32_t argc, JS::Value *argv,
                           nsresult *prv) :
    mContext(aContext),
    mArgv(nullptr),
    mArgc(argc)
{
  // copy the array - we don't know its lifetime, and ours is tied to xpcom
  // refcounting.
  if (argc) {
    static const fallible_t fallible = fallible_t();
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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSArgArray)
  if (tmp->mArgv) {
    for (uint32_t i = 0; i < tmp->mArgc; ++i) {
      NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mArgv[i])
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

/* void queryElementAt (in unsigned long index, in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
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

/* unsigned long indexOf (in unsigned long startIndex, in nsISupports element); */
NS_IMETHODIMP nsJSArgArray::IndexOf(uint32_t startIndex, nsISupports *element, uint32_t *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator enumerate (); */
NS_IMETHODIMP nsJSArgArray::Enumerate(nsISimpleEnumerator **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// The factory function
nsresult NS_CreateJSArgv(JSContext *aContext, uint32_t argc, void *argv,
                         nsIJSArgArray **aArray)
{
  nsresult rv;
  nsJSArgArray *ret = new nsJSArgArray(aContext, argc,
                                       static_cast<JS::Value *>(argv), &rv);
  if (ret == nullptr)
    return NS_ERROR_OUT_OF_MEMORY;
  if (NS_FAILED(rv)) {
    delete ret;
    return rv;
  }
  return ret->QueryInterface(NS_GET_IID(nsIArray), (void **)aArray);
}
