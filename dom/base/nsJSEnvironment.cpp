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
#include "nsIJSContextStack.h"
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
#include "nsGUIEvent.h"
#include "nsThreadUtils.h"
#include "nsITimer.h"
#include "nsIAtom.h"
#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsIContent.h"
#include "nsCycleCollector.h"
#include "nsNetUtil.h"
#include "nsXPCOMCIDInternal.h"
#include "nsIXULRuntime.h"

#include "xpcpublic.h"

#include "jsdbgapi.h"           // for JS_ClearWatchPointsForObject
#include "jswrapper.h"
#include "nsIArray.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsDOMScriptObjectHolder.h"
#include "prmem.h"
#include "WrapperFactory.h"
#include "nsGlobalWindow.h"
#include "nsScriptNameSpaceManager.h"
#include "StructuredCloneTags.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/ImageDataBinding.h"

#include "nsJSPrincipals.h"
#include "jsdbgapi.h"

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
#include "mozilla/dom/CanvasRenderingContext2DBinding.h"

#include "sampler.h"

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

#define NS_MAX_COMPARTMENT_GC_COUNT 20

// Maximum amount of time that should elapse between incremental GC slices
#define NS_INTERSLICE_GC_DELAY      100 // ms

// If we haven't painted in 100ms, we allow for a longer GC budget
#define NS_INTERSLICE_GC_BUDGET     40 // ms

// The amount of time we wait between a request to CC (after GC ran)
// and doing the actual CC.
#define NS_CC_DELAY                 6000 // ms

#define NS_CC_SKIPPABLE_DELAY       400 // ms

// Force a CC after this long if there's more than NS_CC_FORCED_PURPLE_LIMIT
// objects in the purple buffer.
#define NS_CC_FORCED                (2 * 60 * PR_USEC_PER_SEC) // 2 min
#define NS_CC_FORCED_PURPLE_LIMIT   10

// Don't allow an incremental GC to lock out the CC for too long.
#define NS_MAX_CC_LOCKEDOUT_TIME    (15 * PR_USEC_PER_SEC) // 15 seconds

// Trigger a CC if the purple buffer exceeds this size when we check it.
#define NS_CC_PURPLE_LIMIT          200

#define JAVASCRIPT nsIProgrammingLanguage::JAVASCRIPT

// if you add statics here, add them to the list in nsJSRuntime::Startup

static nsITimer *sGCTimer;
static nsITimer *sShrinkGCBuffersTimer;
static nsITimer *sCCTimer;
static nsITimer *sFullGCTimer;
static nsITimer *sInterSliceGCTimer;

static PRTime sLastCCEndTime;

static bool sCCLockedOut;
static PRTime sCCLockedOutTime;

static js::GCSliceCallback sPrevGCSliceCallback;
static js::AnalysisPurgeCallback sPrevAnalysisPurgeCallback;

// The number of currently pending document loads. This count isn't
// guaranteed to always reflect reality and can't easily as we don't
// have an easy place to know when a load ends or is interrupted in
// all cases. This counter also gets reset if we end up GC'ing while
// we're waiting for a slow page to load. IOW, this count may be 0
// even when there are pending loads.
static uint32_t sPendingLoadCount;
static bool sLoadingInProgress;

static uint32_t sCCollectedWaitingForGC;
static bool sPostGCEventsToConsole;
static bool sPostGCEventsToObserver;
static bool sDisableExplicitCompartmentGC;
static uint32_t sCCTimerFireCount = 0;
static uint32_t sMinForgetSkippableTime = UINT32_MAX;
static uint32_t sMaxForgetSkippableTime = 0;
static uint32_t sTotalForgetSkippableTime = 0;
static uint32_t sRemovedPurples = 0;
static uint32_t sForgetSkippableBeforeCC = 0;
static uint32_t sPreviousSuspectedCount = 0;
static uint32_t sCompartmentGCCount = NS_MAX_COMPARTMENT_GC_COUNT;
static uint32_t sCleanupsSinceLastGC = UINT32_MAX;
static bool sNeedsFullCC = false;
static nsJSContext *sContextList = nullptr;

static nsScriptNameSpaceManager *gNameSpaceManager;

static nsIJSRuntimeService *sRuntimeService;
JSRuntime *nsJSRuntime::sRuntime;

static const char kJSRuntimeServiceContractID[] =
  "@mozilla.org/js/xpc/RuntimeService;1";

static PRTime sFirstCollectionTime;

static bool sIsInitialized;
static bool sDidShutdown;
static bool sShuttingDown;
static int32_t sContextCount;

static PRTime sMaxScriptRunTime;
static PRTime sMaxChromeScriptRunTime;

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
  nsJSContext::KillFullGCTimer();
  nsJSContext::KillInterSliceGCTimer();
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
                                 const PRUnichar* aData)
{
  if (sGCOnMemoryPressure && !nsCRT::strcmp(aTopic, "memory-pressure")) {
    nsJSContext::GarbageCollectNow(js::gcreason::MEM_PRESSURE,
                                   nsJSContext::NonIncrementalGC,
                                   nsJSContext::NonCompartmentGC,
                                   nsJSContext::ShrinkingGC);
    nsJSContext::CycleCollectNow();
  } else if (!nsCRT::strcmp(aTopic, "quit-application")) {
    sShuttingDown = true;
    KillTimers();
  }

  return NS_OK;
}

class nsRootedJSValueArray {
public:
  explicit nsRootedJSValueArray(JSContext *cx) : avr(cx, vals.Length(), vals.Elements()) {}

  bool SetCapacity(JSContext *cx, size_t capacity) {
    bool ok = vals.SetCapacity(capacity);
    if (!ok)
      return false;
    // Values must be safe for the GC to inspect (they must not contain garbage).
    memset(vals.Elements(), 0, vals.Capacity() * sizeof(jsval));
    resetRooter(cx);
    return true;
  }

  jsval *Elements() {
    return vals.Elements();
  }

private:
  void resetRooter(JSContext *cx) {
    avr.changeArray(vals.Elements(), vals.Length());
  }

  nsAutoTArray<jsval, 16> vals;
  JS::AutoArrayRooter avr;
};

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
                     nsScriptErrorEvent *aErrorEvent,
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

    if (presContext && errorDepth < 2) {
      // Dispatch() must be synchronous for the recursion block
      // (errorDepth) to work.
      nsEventDispatcher::Dispatch(win, presContext, aErrorEvent, nullptr,
                                  aStatus);
      called = true;
    }
    --errorDepth;
  }
  return called;
}

class ScriptErrorEvent : public nsRunnable
{
public:
  ScriptErrorEvent(nsIScriptGlobalObject* aScriptGlobal,
                   nsIPrincipal* aScriptOriginPrincipal,
                   uint32_t aLineNr, uint32_t aColumn, uint32_t aFlags,
                   const nsAString& aErrorMsg,
                   const nsAString& aFileName,
                   const nsAString& aSourceLine,
                   bool aDispatchEvent,
                   uint64_t aInnerWindowID)
    : mScriptGlobal(aScriptGlobal), mOriginPrincipal(aScriptOriginPrincipal),
      mLineNr(aLineNr), mColumn(aColumn),
    mFlags(aFlags), mErrorMsg(aErrorMsg), mFileName(aFileName),
    mSourceLine(aSourceLine), mDispatchEvent(aDispatchEvent),
    mInnerWindowID(aInnerWindowID)
  {}

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
        sHandlingScriptError = true; // Recursion prevention

        nsRefPtr<nsPresContext> presContext;
        docShell->GetPresContext(getter_AddRefs(presContext));

        if (presContext) {
          nsScriptErrorEvent errorevent(true, NS_LOAD_ERROR);

          errorevent.fileName = mFileName.get();

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
            errorevent.errorMsg = mErrorMsg.get();
            errorevent.lineNr = mLineNr;
          } else {
            NS_WARNING("Not same origin error!");
            errorevent.errorMsg = xoriginMsg.get();
            errorevent.lineNr = 0;
          }

          nsEventDispatcher::Dispatch(win, presContext, &errorevent, nullptr,
                                      &status);
        }

        sHandlingScriptError = false;
      }
    }

    if (status != nsEventStatus_eConsumeNoDefault) {
      // Make an nsIScriptError and populate it with information from
      // this error.
      nsCOMPtr<nsIScriptError> errorObject =
        do_CreateInstance("@mozilla.org/scripterror;1");

      if (errorObject != nullptr) {
        nsresult rv = NS_ERROR_NOT_AVAILABLE;

        // Set category to chrome or content
        nsCOMPtr<nsIScriptObjectPrincipal> scriptPrincipal =
          do_QueryInterface(mScriptGlobal);
        NS_ASSERTION(scriptPrincipal, "Global objects must implement "
                     "nsIScriptObjectPrincipal");
        nsCOMPtr<nsIPrincipal> systemPrincipal;
        sSecurityManager->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
        const char * category =
          scriptPrincipal->GetPrincipal() == systemPrincipal
          ? "chrome javascript"
          : "content javascript";

        rv = errorObject->InitWithWindowID(mErrorMsg, mFileName,
                                           mSourceLine,
                                           mLineNr, mColumn, mFlags,
                                           category, mInnerWindowID);

        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIConsoleService> consoleService =
            do_GetService(NS_CONSOLESERVICE_CONTRACTID, &rv);
          if (NS_SUCCEEDED(rv)) {
            consoleService->LogMessage(errorObject);
          }
        }
      }
    }
    return NS_OK;
  }


  nsCOMPtr<nsIScriptGlobalObject> mScriptGlobal;
  nsCOMPtr<nsIPrincipal>          mOriginPrincipal;
  uint32_t                        mLineNr;
  uint32_t                        mColumn;
  uint32_t                        mFlags;
  nsString                        mErrorMsg;
  nsString                        mFileName;
  nsString                        mSourceLine;
  bool                            mDispatchEvent;
  uint64_t                        mInnerWindowID;

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
    if (JS_DescribeScriptedCaller(cx, nullptr, nullptr)) {
      return;
    }

    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    if (xpc) {
      nsAXPCNativeCallContext *cc = nullptr;
      xpc->GetCurrentNativeCallContext(&cc);
      if (cc) {
        nsAXPCNativeCallContext *prev = cc;
        while (NS_SUCCEEDED(prev->GetPreviousCallContext(&prev)) && prev) {
          uint16_t lang;
          if (NS_SUCCEEDED(prev->GetLanguage(&lang)) &&
            lang == nsAXPCNativeCallContext::LANG_JS) {
            return;
          }
        }
      }
    }
  }

  // XXX this means we are not going to get error reports on non DOM contexts
  nsIScriptContext *context = nsJSUtils::GetDynamicScriptContext(cx);

  // Note: we must do this before running any more code on cx (if cx is the
  // dynamic script context).
  ::JS_ClearPendingException(cx);

  if (context) {
    nsIScriptGlobalObject *globalObject = context->GetGlobalObject();

    if (globalObject) {
      nsAutoString fileName, msg;
      if (!report->filename) {
        fileName.SetIsVoid(true);
      } else {
        fileName.AssignWithConversion(report->filename);
      }

      const PRUnichar* m = static_cast<const PRUnichar*>(report->ucmessage);
      if (m) {
        const PRUnichar* n = static_cast<const PRUnichar*>
            (js::GetErrorTypeName(cx, report->exnType));
        if (n) {
          msg.Assign(n);
          msg.AppendLiteral(": ");
        }
        msg.Append(m);
      }

      if (msg.IsEmpty() && message) {
        msg.AssignWithConversion(message);
      }


      /* We do not try to report Out Of Memory via a dom
       * event because the dom event handler would encounter
       * an OOM exception trying to process the event, and
       * then we'd need to generate a new OOM event for that
       * new OOM instance -- this isn't pretty.
       */
      nsAutoString sourceLine;
      sourceLine.Assign(reinterpret_cast<const PRUnichar*>(report->uclinebuf));
      nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(globalObject);
      uint64_t innerWindowID = 0;
      if (win) {
        nsCOMPtr<nsPIDOMWindow> innerWin = win->GetCurrentInnerWindow();
        if (innerWin) {
          innerWindowID = innerWin->WindowID();
        }
      }
      nsContentUtils::AddScriptRunner(
        new ScriptErrorEvent(globalObject,
                             nsJSPrincipals::get(report->originPrincipals),
                             report->lineno,
                             report->uctokenptr - report->uclinebuf,
                             report->flags, msg, fileName, sourceLine,
                             report->errorNumber != JSMSG_OUT_OF_MEMORY,
                             innerWindowID));
    }
  }

#ifdef DEBUG
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
    AppendUTF16toUTF8(reinterpret_cast<const PRUnichar*>(report->ucmessage),
                      error);
  } else {
    error.Append(message);
  }

  fprintf(stderr, "%s\n", error.get());
  fflush(stderr);
#endif

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
JSObject2Win(JSContext *cx, JSObject *obj)
{
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  if (!xpc) {
    return nullptr;
  }

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  xpc->GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wrapper));
  if (wrapper) {
    nsCOMPtr<nsPIDOMWindow> win = do_QueryWrappedNative(wrapper);
    if (win) {
      return static_cast<nsGlobalWindow *>
                        (static_cast<nsPIDOMWindow *>(win));
    }
  }

  return nullptr;
}

void
PrintWinURI(nsGlobalWindow *win)
{
  if (!win) {
    printf("No window passed in.\n");
    return;
  }

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(win->GetExtantDocument());
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

static already_AddRefed<nsIPrompt>
GetPromptFromContext(nsJSContext* ctx)
{
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(ctx->GetGlobalObject()));
  NS_ENSURE_TRUE(win, nullptr);

  nsIDocShell *docShell = win->GetDocShell();
  NS_ENSURE_TRUE(docShell, nullptr);

  nsCOMPtr<nsIInterfaceRequestor> ireq(do_QueryInterface(docShell));
  NS_ENSURE_TRUE(ireq, nullptr);

  // Get the nsIPrompt interface from the docshell
  nsIPrompt* prompt;
  ireq->GetInterface(NS_GET_IID(nsIPrompt), (void**)&prompt);
  return prompt;
}

JSBool
nsJSContext::DOMOperationCallback(JSContext *cx)
{
  nsresult rv;

  // Get the native context
  nsJSContext *ctx = static_cast<nsJSContext *>(::JS_GetContextPrivate(cx));

  if (!ctx) {
    // Can happen; see bug 355811
    return JS_TRUE;
  }

  // XXX Save the operation callback time so we can restore it after the GC,
  // because GCing can cause JS to run on our context, causing our
  // ScriptEvaluated to be called, and clearing our operation callback time.
  // See bug 302333.
  PRTime callbackTime = ctx->mOperationCallbackTime;
  PRTime modalStateTime = ctx->mModalStateTime;

  // Now restore the callback time and count, in case they got reset.
  ctx->mOperationCallbackTime = callbackTime;
  ctx->mModalStateTime = modalStateTime;

  PRTime now = PR_Now();

  if (callbackTime == 0) {
    // Initialize mOperationCallbackTime to start timing how long the
    // script has run
    ctx->mOperationCallbackTime = now;
    return JS_TRUE;
  }

  if (ctx->mModalStateDepth) {
    // We're waiting on a modal dialog, nothing more to do here.
    return JS_TRUE;
  }

  PRTime duration = now - callbackTime;

  // Check the amount of time this script has been running, or if the
  // dialog is disabled.
  JSObject* global = ::JS_GetGlobalForScopeChain(cx);
  bool isTrackingChromeCodeTime =
    global && xpc::AccessCheck::isChrome(js::GetObjectCompartment(global));
  if (duration < (isTrackingChromeCodeTime ?
                  sMaxChromeScriptRunTime : sMaxScriptRunTime)) {
    return JS_TRUE;
  }

  if (!nsContentUtils::IsSafeToRunScript()) {
    // If it isn't safe to run script, then it isn't safe to bring up the
    // prompt (since that will cause the event loop to spin). In this case
    // (which is rare), we just stop the script... But report a warning so
    // that developers have some idea of what went wrong.

    JS_ReportWarning(cx, "A long running script was terminated");
    return JS_FALSE;
  }

  // If we get here we're most likely executing an infinite loop in JS,
  // we'll tell the user about this and we'll give the user the option
  // of stopping the execution of the script.
  nsCOMPtr<nsIPrompt> prompt = GetPromptFromContext(ctx);
  NS_ENSURE_TRUE(prompt, JS_FALSE);

  // Check if we should offer the option to debug
  JSScript *script;
  unsigned lineno;
  JSBool hasFrame = ::JS_DescribeScriptedCaller(cx, &script, &lineno);

  bool debugPossible = hasFrame && js::CanCallContextDebugHandler(cx);
#ifdef MOZ_JSDEBUGGER
  // Get the debugger service if necessary.
  if (debugPossible) {
    bool jsds_IsOn = false;
    const char jsdServiceCtrID[] = "@mozilla.org/js/jsd/debugger-service;1";
    nsCOMPtr<jsdIExecutionHook> jsdHook;
    nsCOMPtr<jsdIDebuggerService> jsds = do_GetService(jsdServiceCtrID, &rv);

    // Check if there's a user for the debugger service that's 'on' for us
    if (NS_SUCCEEDED(rv)) {
      jsds->GetDebuggerHook(getter_AddRefs(jsdHook));
      jsds->GetIsOn(&jsds_IsOn);
    }

    // If there is a debug handler registered for this runtime AND
    // ((jsd is on AND has a hook) OR (jsd isn't on (something else debugs)))
    // then something useful will be done with our request to debug.
    debugPossible = ((jsds_IsOn && (jsdHook != nullptr)) || !jsds_IsOn);
  }
#endif

  // Get localizable strings
  nsXPIDLString title, msg, stopButton, waitButton, debugButton, neverShowDlg;

  rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                          "KillScriptTitle",
                                          title);

  nsresult tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                           "StopScriptButton",
                                           stopButton);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }

  tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                           "WaitForScriptButton",
                                           waitButton);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }

  tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                           "DontAskAgain",
                                           neverShowDlg);
  if (NS_FAILED(tmp)) {
    rv = tmp;
  }


  if (debugPossible) {
    tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                             "DebugScriptButton",
                                             debugButton);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }

    tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                             "KillScriptWithDebugMessage",
                                             msg);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
  }
  else {
    tmp = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                             "KillScriptMessage",
                                             msg);
    if (NS_FAILED(tmp)) {
      rv = tmp;
    }
  }

  //GetStringFromName can return NS_OK and still give NULL string
  if (NS_FAILED(rv) || !title || !msg || !stopButton || !waitButton ||
      (!debugButton && debugPossible) || !neverShowDlg) {
    NS_ERROR("Failed to get localized strings.");
    return JS_TRUE;
  }

  // Append file and line number information, if available
  if (script) {
    const char *filename = ::JS_GetScriptFilename(cx, script);
    if (filename) {
      nsXPIDLString scriptLocation;
      NS_ConvertUTF8toUTF16 filenameUTF16(filename);
      const PRUnichar *formatParams[] = { filenameUTF16.get() };
      rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                 "KillScriptLocation",
                                                 formatParams,
                                                 scriptLocation);

      if (NS_SUCCEEDED(rv) && scriptLocation) {
        msg.AppendLiteral("\n\n");
        msg.Append(scriptLocation);
        msg.Append(':');
        msg.AppendInt(lineno);
      }
    }
  }

  int32_t buttonPressed = 0; //In case user exits dialog by clicking X
  bool neverShowDlgChk = false;
  uint32_t buttonFlags = nsIPrompt::BUTTON_POS_1_DEFAULT +
                         (nsIPrompt::BUTTON_TITLE_IS_STRING *
                          (nsIPrompt::BUTTON_POS_0 + nsIPrompt::BUTTON_POS_1));

  // Add a third button if necessary:
  if (debugPossible)
    buttonFlags += nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_2;

  // Null out the operation callback while we're re-entering JS here.
  ::JS_SetOperationCallback(cx, nullptr);

  // Open the dialog.
  rv = prompt->ConfirmEx(title, msg, buttonFlags, waitButton, stopButton,
                         debugButton, neverShowDlg, &neverShowDlgChk,
                         &buttonPressed);

  ::JS_SetOperationCallback(cx, DOMOperationCallback);

  if (NS_SUCCEEDED(rv) && (buttonPressed == 0)) {
    // Allow the script to continue running

    if (neverShowDlgChk) {
      Preferences::SetInt(isTrackingChromeCodeTime ?
        "dom.max_chrome_script_run_time" : "dom.max_script_run_time", 0);
    }

    ctx->mOperationCallbackTime = PR_Now();
    return JS_TRUE;
  }
  else if ((buttonPressed == 2) && debugPossible) {
    return js_CallContextDebugHandler(cx);
  }

  JS_ClearPendingException(cx);
  return JS_FALSE;
}

void
nsJSContext::EnterModalState()
{
  if (!mModalStateDepth) {
    mModalStateTime =  mOperationCallbackTime ? PR_Now() : 0;
  }
  ++mModalStateDepth;
}

void
nsJSContext::LeaveModalState()
{
  if (!mModalStateDepth) {
    NS_ERROR("Uh, mismatched LeaveModalState() call!");

    return;
  }

  --mModalStateDepth;

  // If we're still in a modal dialog, or mOperationCallbackTime is still
  // uninitialized, do nothing.
  if (mModalStateDepth || !mOperationCallbackTime) {
    return;
  }

  // If mOperationCallbackTime was set when we entered the first dialog
  // (and mModalStateTime is thus non-zero), adjust mOperationCallbackTime
  // to account for time spent in the dialog.
  // If mOperationCallbackTime got set while the modal dialog was open,
  // simply set mOperationCallbackTime to the closing time of the dialog so
  // that we never adjust mOperationCallbackTime to be in the future. 
  if (mModalStateTime) {
    mOperationCallbackTime += PR_Now() - mModalStateTime;
  }
  else {
    mOperationCallbackTime = PR_Now();
  }
}

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
static const char js_methodjit_content_str[]  = JS_OPTIONS_DOT_STR "methodjit.content";
static const char js_methodjit_chrome_str[]   = JS_OPTIONS_DOT_STR "methodjit.chrome";
static const char js_methodjit_always_str[]   = JS_OPTIONS_DOT_STR "methodjit_always";
static const char js_typeinfer_str[]          = JS_OPTIONS_DOT_STR "typeinference";
static const char js_pccounts_content_str[]   = JS_OPTIONS_DOT_STR "pccounts.content";
static const char js_pccounts_chrome_str[]    = JS_OPTIONS_DOT_STR "pccounts.chrome";
static const char js_jit_hardening_str[]      = JS_OPTIONS_DOT_STR "jit_hardening";
static const char js_memlog_option_str[]      = JS_OPTIONS_DOT_STR "mem.log";
static const char js_memnotify_option_str[]   = JS_OPTIONS_DOT_STR "mem.notify";
static const char js_disable_explicit_compartment_gc[] =
  JS_OPTIONS_DOT_STR "mem.disable_explicit_compartment_gc";
static const char js_ion_content_str[]        = JS_OPTIONS_DOT_STR "ion.content";
static const char js_ion_parallel_compilation_str[] = JS_OPTIONS_DOT_STR "ion.parallel_compilation";

int
nsJSContext::JSOptionChangedCallback(const char *pref, void *data)
{
  nsJSContext *context = reinterpret_cast<nsJSContext *>(data);
  uint32_t oldDefaultJSOptions = context->mDefaultJSOptions;
  uint32_t newDefaultJSOptions = oldDefaultJSOptions;

  sPostGCEventsToConsole = Preferences::GetBool(js_memlog_option_str);
  sPostGCEventsToObserver = Preferences::GetBool(js_memnotify_option_str);
  sDisableExplicitCompartmentGC =
    Preferences::GetBool(js_disable_explicit_compartment_gc);

  bool strict = Preferences::GetBool(js_strict_option_str);
  if (strict)
    newDefaultJSOptions |= JSOPTION_STRICT;
  else
    newDefaultJSOptions &= ~JSOPTION_STRICT;

  // The vanilla GetGlobalObject returns null if a global isn't set up on
  // the context yet. We can sometimes be call midway through context init,
  // So ask for the member directly instead.
  nsIScriptGlobalObject *global = context->GetGlobalObjectRef();

  // XXX should we check for sysprin instead of a chrome window, to make
  // XXX components be covered by the chrome pref instead of the content one?
  nsCOMPtr<nsIDOMWindow> contentWindow(do_QueryInterface(global));
  nsCOMPtr<nsIDOMChromeWindow> chromeWindow(do_QueryInterface(global));

  bool useMethodJIT = Preferences::GetBool(chromeWindow || !contentWindow ?
                                               js_methodjit_chrome_str :
                                               js_methodjit_content_str);
  bool usePCCounts = Preferences::GetBool(chromeWindow || !contentWindow ?
                                            js_pccounts_chrome_str :
                                            js_pccounts_content_str);
  bool useMethodJITAlways = Preferences::GetBool(js_methodjit_always_str);
  bool useTypeInference = !chromeWindow && contentWindow && Preferences::GetBool(js_typeinfer_str);
  bool useXML = Preferences::GetBool(chromeWindow || !contentWindow ?
                                     "javascript.options.xml.chrome" :
                                     "javascript.options.xml.content");
  bool useHardening = Preferences::GetBool(js_jit_hardening_str);
  bool useIon = Preferences::GetBool(js_ion_content_str);
  bool parallelIonCompilation = Preferences::GetBool(js_ion_parallel_compilation_str);
  nsCOMPtr<nsIXULRuntime> xr = do_GetService(XULRUNTIME_SERVICE_CONTRACTID);
  if (xr) {
    bool safeMode = false;
    xr->GetInSafeMode(&safeMode);
    if (safeMode) {
      useMethodJIT = false;
      usePCCounts = false;
      useTypeInference = false;
      useMethodJITAlways = true;
      useXML = false;
      useHardening = false;
      useIon = false;
    }
  }

  if (useMethodJIT)
    newDefaultJSOptions |= JSOPTION_METHODJIT;
  else
    newDefaultJSOptions &= ~JSOPTION_METHODJIT;

  if (usePCCounts)
    newDefaultJSOptions |= JSOPTION_PCCOUNT;
  else
    newDefaultJSOptions &= ~JSOPTION_PCCOUNT;

  if (useMethodJITAlways)
    newDefaultJSOptions |= JSOPTION_METHODJIT_ALWAYS;
  else
    newDefaultJSOptions &= ~JSOPTION_METHODJIT_ALWAYS;

  if (useTypeInference)
    newDefaultJSOptions |= JSOPTION_TYPE_INFERENCE;
  else
    newDefaultJSOptions &= ~JSOPTION_TYPE_INFERENCE;

  if (useIon)
    newDefaultJSOptions |= JSOPTION_ION;
  else
    newDefaultJSOptions &= ~JSOPTION_ION;

  if (useXML)
    newDefaultJSOptions |= JSOPTION_ALLOW_XML;
  else
    newDefaultJSOptions &= ~JSOPTION_ALLOW_XML;

#ifdef DEBUG
  // In debug builds, warnings are enabled in chrome context if
  // javascript.options.strict.debug is true
  bool strictDebug = Preferences::GetBool(js_strict_debug_option_str);
  if (strictDebug && (newDefaultJSOptions & JSOPTION_STRICT) == 0) {
    if (chromeWindow || !contentWindow)
      newDefaultJSOptions |= JSOPTION_STRICT;
  }
#endif

  bool werror = Preferences::GetBool(js_werror_option_str);
  if (werror)
    newDefaultJSOptions |= JSOPTION_WERROR;
  else
    newDefaultJSOptions &= ~JSOPTION_WERROR;

  ::JS_SetOptions(context->mContext,
                  newDefaultJSOptions & (JSRUNOPTION_MASK | JSOPTION_ALLOW_XML));

  ::JS_SetParallelCompilationEnabled(context->mContext, parallelIonCompilation);

  // Save the new defaults for the next page load (InitContext).
  context->mDefaultJSOptions = newDefaultJSOptions;

  JSRuntime *rt = JS_GetRuntime(context->mContext);
  JS_SetJitHardening(rt, useHardening);

#ifdef JS_GC_ZEAL
  int32_t zeal = Preferences::GetInt(js_zeal_option_str, -1);
  int32_t frequency = Preferences::GetInt(js_zeal_frequency_str, JS_DEFAULT_ZEAL_FREQ);
  if (zeal >= 0)
    ::JS_SetGCZeal(context->mContext, (uint8_t)zeal, frequency);
#endif

  return 0;
}

nsJSContext::nsJSContext(JSRuntime *aRuntime, bool aGCOnDestruction,
                         nsIScriptGlobalObject* aGlobalObject)
  : mActive(false)
  , mGCOnDestruction(aGCOnDestruction)
  , mExecuteDepth(0)
  , mGlobalObjectRef(aGlobalObject)
{
  mNext = sContextList;
  mPrev = &sContextList;
  if (sContextList) {
    sContextList->mPrev = &mNext;
  }
  sContextList = this;

  ++sContextCount;

  mDefaultJSOptions = JSOPTION_PRIVATE_IS_NSISUPPORTS | JSOPTION_ALLOW_XML;

  mContext = ::JS_NewContext(aRuntime, gStackSize);
  if (mContext) {
    ::JS_SetContextPrivate(mContext, static_cast<nsIScriptContext *>(this));

    // Preserve any flags the context callback might have set.
    mDefaultJSOptions |= ::JS_GetOptions(mContext);

    // Make sure the new context gets the default context options
    ::JS_SetOptions(mContext, mDefaultJSOptions);

    // Watch for the JS boolean options
    Preferences::RegisterCallback(JSOptionChangedCallback,
                                  js_options_dot_str, this);

    ::JS_SetOperationCallback(mContext, DOMOperationCallback);

    xpc_LocalizeContext(mContext);
  }
  mIsInitialized = false;
  mTerminations = nullptr;
  mScriptsEnabled = true;
  mOperationCallbackTime = 0;
  mModalStateTime = 0;
  mModalStateDepth = 0;
  mProcessingScriptTag = false;
}

nsJSContext::~nsJSContext()
{
#ifdef DEBUG
  nsCycleCollector_DEBUG_wasFreed(static_cast<nsIScriptContext*>(this));
#endif

  *mPrev = mNext;
  if (mNext) {
    mNext->mPrev = mPrev;
  }

  // We may still have pending termination functions if the context is destroyed
  // before they could be executed. In this case, free the references to their
  // parameters, but don't execute the functions (see bug 622326).
  delete mTerminations;

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
    PokeGC(js::gcreason::NSJSCONTEXT_DESTROY);
  }
        
  // Let xpconnect destroy the JSContext when it thinks the time is right.
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  if (xpc) {
    xpc->ReleaseJSContext(mContext, true);
  } else {
    ::JS_DestroyContextNoGC(mContext);
  }
  mContext = nullptr;
}

// QueryInterface implementation for nsJSContext
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSContext)
NS_IMPL_CYCLE_COLLECTION_TRACE_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSContext)
  NS_ASSERTION(!tmp->mContext || !js::ContextHasOutstandingRequests(tmp->mContext),
               "Trying to unlink a context with outstanding requests.");
  tmp->mIsInitialized = false;
  tmp->mGCOnDestruction = false;
  if (tmp->mContext) {
    JSAutoRequest ar(tmp->mContext);
    JS_SetGlobalObject(tmp->mContext, nullptr);
  }
  tmp->DestroyJSContext();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobalObjectRef)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsJSContext)
  NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsJSContext, tmp->GetCCRefcnt())
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobalObjectRef)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mContext");
  nsContentUtils::XPConnect()->NoteJSContext(tmp->mContext, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsJSContext)
  NS_INTERFACE_MAP_ENTRY(nsIScriptContext)
  NS_INTERFACE_MAP_ENTRY(nsIScriptContextPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptNotify)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptContext)
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

nsresult
nsJSContext::EvaluateString(const nsAString& aScript,
                            JSObject& aScopeObject,
                            JS::CompileOptions& aOptions,
                            bool aCoerceToString,
                            JS::Value* aRetValue)
{
  SAMPLE_LABEL("JS", "EvaluateString");
  MOZ_ASSERT_IF(aOptions.versionSet, aOptions.version != JSVERSION_UNKNOWN);
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);
  *aRetValue = JSVAL_VOID;

  if (!mScriptsEnabled) {
    return NS_OK;
  }

  nsCxPusher pusher;
  if (!pusher.Push(mContext))
    return NS_ERROR_FAILURE;

  xpc_UnmarkGrayObject(&aScopeObject);
  nsAutoMicroTask mt;

  JSPrincipals* p = JS_GetCompartmentPrincipals(js::GetObjectCompartment(&aScopeObject));
  aOptions.setPrincipals(p);

  bool ok = false;
  nsresult rv = sSecurityManager->CanExecuteScripts(mContext, nsJSPrincipals::get(p), &ok);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(ok, NS_OK);

  nsJSContext::TerminationFuncHolder holder(this);

  // Scope the JSAutoCompartment so that it gets destroyed before we pop the
  // cx and potentially call JS_RestoreFrameChain.
  XPCAutoRequest ar(mContext);
  {
    JSAutoCompartment ac(mContext, &aScopeObject);

    ++mExecuteDepth;

    js::RootedObject rootedScope(mContext, &aScopeObject);
    ok = JS::Evaluate(mContext, rootedScope, aOptions,
                      PromiseFlatString(aScript).get(),
                      aScript.Length(), aRetValue);
    if (ok && !aRetValue->isUndefined() && aCoerceToString) {
      JSString* str = JS_ValueToString(mContext, *aRetValue);
      ok = !!str;
      *aRetValue = ok ? JS::StringValue(str) : JS::UndefinedValue();
    }
    --mExecuteDepth;
  }

  if (!ok) {
    // Tell XPConnect about any pending exceptions. This is needed
    // to avoid dropping JS exceptions in case we got here through
    // nested calls through XPConnect.

    ReportPendingException();
  }

  // ScriptEvaluated needs to come after we pop the stack
  pusher.Pop();
  ScriptEvaluated(true);

  // Wrap the return value into whatever compartment mContext was in.
  if (!JS_WrapValue(mContext, aRetValue))
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

nsIScriptObjectPrincipal*
nsJSContext::GetObjectPrincipal()
{
  nsCOMPtr<nsIScriptObjectPrincipal> prin = do_QueryInterface(GetGlobalObject());
  return prin;
}

nsresult
nsJSContext::CompileScript(const PRUnichar* aText,
                           int32_t aTextLength,
                           nsIPrincipal *aPrincipal,
                           const char *aURL,
                           uint32_t aLineNo,
                           uint32_t aVersion,
                           nsScriptObjectHolder<JSScript>& aScriptObject,
                           bool aSaveSource /* = false */)
{
  SAMPLE_LABEL_PRINTF("JS", "Compile Script", "%s", aURL ? aURL : "");
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aPrincipal);

  JSObject* scopeObject = ::JS_GetGlobalObject(mContext);
  xpc_UnmarkGrayObject(scopeObject);

  bool ok = false;

  nsresult rv = sSecurityManager->CanExecuteScripts(mContext, aPrincipal, &ok);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  aScriptObject.drop(); // ensure old object not used on failure...

  // Don't compile if SecurityManager said "not ok" or aVersion is unknown.
  // Since the caller is responsible for parsing the version strings, we just
  // check it isn't JSVERSION_UNKNOWN.
  if (!ok || JSVersion(aVersion) == JSVERSION_UNKNOWN)
    return NS_OK;
    
  XPCAutoRequest ar(mContext);


  JS::CompileOptions options(mContext);
  JS::CompileOptions::SourcePolicy sp = aSaveSource ?
    JS::CompileOptions::SAVE_SOURCE :
    JS::CompileOptions::LAZY_SOURCE;
  options.setPrincipals(nsJSPrincipals::get(aPrincipal))
         .setFileAndLine(aURL, aLineNo)
         .setVersion(JSVersion(aVersion))
         .setSourcePolicy(sp);
  js::RootedObject rootedScope(mContext, scopeObject);
  JSScript* script = JS::Compile(mContext,
                                 rootedScope,
                                 options,
                                 static_cast<const jschar*>(aText),
                                 aTextLength);
  if (!script) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return aScriptObject.set(script);
}

nsresult
nsJSContext::ExecuteScript(JSScript* aScriptObject,
                           JSObject* aScopeObject)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    return NS_OK;
  }

  nsAutoMicroTask mt;

  if (!aScopeObject) {
    aScopeObject = JS_GetGlobalObject(mContext);
  }

  xpc_UnmarkGrayScript(aScriptObject);
  xpc_UnmarkGrayObject(aScopeObject);

  // Push our JSContext on our thread's context stack, in case native code
  // called from JS calls back into JS via XPConnect.
  nsresult rv;
  nsCOMPtr<nsIJSContextStack> stack =
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext))) {
    return NS_ERROR_FAILURE;
  }

  nsJSContext::TerminationFuncHolder holder(this);
  XPCAutoRequest ar(mContext);
  JSAutoCompartment ac(mContext, aScopeObject);
  ++mExecuteDepth;

  // The result of evaluation, used only if there were no errors. This need
  // not be a GC root currently, provided we run the GC only from the
  // operation callback or from ScriptEvaluated.
  jsval val;
  if (!JS_ExecuteScript(mContext, aScopeObject, aScriptObject, &val)) {
    ReportPendingException();
  }
  --mExecuteDepth;

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nullptr))) {
    rv = NS_ERROR_FAILURE;
  }

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(true);

  return rv;
}


#ifdef DEBUG
bool
AtomIsEventHandlerName(nsIAtom *aName)
{
  const PRUnichar *name = aName->GetUTF16String();

  const PRUnichar *cp;
  PRUnichar c;
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
nsJSContext::JSObjectFromInterface(nsISupports* aTarget, JSObject* aScope, JSObject** aRet)
{
  // It is legal to specify a null target.
  if (!aTarget) {
    *aRet = nullptr;
    return NS_OK;
  }

  // Get the jsobject associated with this target
  // We don't wrap here because we trust the JS engine to wrap the target
  // later.
  jsval v;
  nsresult rv = nsContentUtils::WrapNative(mContext, aScope, aTarget, &v);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  nsCOMPtr<nsISupports> targetSupp = do_QueryInterface(aTarget);
  nsCOMPtr<nsISupports> native =
    nsContentUtils::XPConnect()->GetNativeOfWrapper(mContext,
                                                    JSVAL_TO_OBJECT(v));
  NS_ASSERTION(native == targetSupp, "Native should be the target!");
#endif

  *aRet = xpc_UnmarkGrayObject(JSVAL_TO_OBJECT(v));

  return NS_OK;
}


nsresult
nsJSContext::CallEventHandler(nsISupports* aTarget, JSObject* aScope,
                              JSObject* aHandler, nsIArray* aargv,
                              nsIVariant** arv)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    return NS_OK;
  }

  SAMPLE_LABEL("JS", "CallEventHandler");

  nsAutoMicroTask mt;
  xpc_UnmarkGrayObject(aScope);
  xpc_UnmarkGrayObject(aHandler);

  XPCAutoRequest ar(mContext);
  JSObject* target = nullptr;
  nsresult rv = JSObjectFromInterface(aTarget, aScope, &target);
  NS_ENSURE_SUCCESS(rv, rv);

  JS::AutoObjectRooter targetVal(mContext, target);
  jsval rval = JSVAL_VOID;

  // This one's a lot easier than EvaluateString because we don't have to
  // hassle with principals: they're already compiled into the JS function.
  // xxxmarkh - this comment is no longer true - principals are not used at
  // all now, and never were in some cases.

  nsCxPusher pusher;
  if (!pusher.Push(mContext, true))
    return NS_ERROR_FAILURE;

  // check if the event handler can be run on the object in question
  rv = sSecurityManager->CheckFunctionAccess(mContext, aHandler, target);

  nsJSContext::TerminationFuncHolder holder(this);

  if (NS_SUCCEEDED(rv)) {
    // Convert args to jsvals.
    uint32_t argc = 0;
    jsval *argv = nullptr;

    JSObject *funobj = aHandler;
    jsval funval = OBJECT_TO_JSVAL(funobj);
    JSAutoCompartment ac(mContext, funobj);
    if (!JS_WrapObject(mContext, &target)) {
      ReportPendingException();
      return NS_ERROR_FAILURE;
    }

    Maybe<nsRootedJSValueArray> tempStorage;

    // Use |target| as the scope for wrapping the arguments, since aScope is
    // the safe scope in many cases, which isn't very useful.  Wrapping aTarget
    // was OK because those typically have PreCreate methods that give them the
    // right scope anyway, and we want to make sure that the arguments end up
    // in the same scope as aTarget.
    rv = ConvertSupportsTojsvals(aargv, target, &argc, &argv, tempStorage);
    NS_ENSURE_SUCCESS(rv, rv);
    for (uint32_t i = 0; i < argc; i++) {
      if (!JSVAL_IS_PRIMITIVE(argv[i])) {
        xpc_UnmarkGrayObject(JSVAL_TO_OBJECT(argv[i]));
      }
    }

    ++mExecuteDepth;
    bool ok = ::JS_CallFunctionValue(mContext, target,
                                       funval, argc, argv, &rval);
    --mExecuteDepth;

    if (!ok) {
      // Don't pass back results from failed calls.
      rval = JSVAL_VOID;

      // Tell the caller that the handler threw an error.
      rv = NS_ERROR_FAILURE;
    } else if (rval == JSVAL_NULL) {
      *arv = nullptr;
    } else if (!JS_WrapValue(mContext, &rval)) {
      rv = NS_ERROR_FAILURE;
    } else {
      rv = nsContentUtils::XPConnect()->JSToVariant(mContext, rval, arv);
    }

    // Tell XPConnect about any pending exceptions. This is needed
    // to avoid dropping JS exceptions in case we got here through
    // nested calls through XPConnect.
    if (NS_FAILED(rv))
      ReportPendingException();
  }

  pusher.Pop();

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(true);

  return rv;
}

nsresult
nsJSContext::BindCompiledEventHandler(nsISupports* aTarget, JSObject* aScope,
                                      JSObject* aHandler,
                                      nsScriptObjectHolder<JSObject>& aBoundHandler)
{
  NS_ENSURE_ARG(aHandler);
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);
  NS_PRECONDITION(!aBoundHandler, "Shouldn't already have a bound handler!");

  xpc_UnmarkGrayObject(aScope);
  xpc_UnmarkGrayObject(aHandler);

  XPCAutoRequest ar(mContext);

  // Get the jsobject associated with this target
  JSObject *target = nullptr;
  nsresult rv = JSObjectFromInterface(aTarget, aScope, &target);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  {
    JSAutoCompartment ac(mContext, aHandler);
    NS_ASSERTION(JS_TypeOfValue(mContext,
                                OBJECT_TO_JSVAL(aHandler)) == JSTYPE_FUNCTION,
                 "Event handler object not a function");
  }
#endif

  JSAutoCompartment ac(mContext, target);

  JSObject* funobj;
  // Make sure the handler function is parented by its event target object
  if (aHandler) {
    funobj = JS_CloneFunctionObject(mContext, aHandler, target);
    if (!funobj) {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  } else {
    funobj = NULL;
  }

  aBoundHandler.set(funobj);

  return rv;
}

// serialization
nsresult
nsJSContext::Serialize(nsIObjectOutputStream* aStream, JSScript* aScriptObject)
{
  if (!aScriptObject)
    return NS_ERROR_FAILURE;

  return nsContentUtils::XPConnect()->WriteScript(aStream, mContext,
                                                  xpc_UnmarkGrayScript(aScriptObject));
}

nsresult
nsJSContext::Deserialize(nsIObjectInputStream* aStream,
                         nsScriptObjectHolder<JSScript>& aResult)
{
  JSScript *script;
  nsresult rv = nsContentUtils::XPConnect()->ReadScript(aStream, mContext, &script);
  if (NS_FAILED(rv)) return rv;
    
  return aResult.set(script);
}

nsIScriptGlobalObject *
nsJSContext::GetGlobalObject()
{
  JSObject *global = ::JS_GetGlobalObject(mContext);

  if (!global) {
    return nullptr;
  }

  if (mGlobalObjectRef)
    return mGlobalObjectRef;

#ifdef DEBUG
  {
    JSObject *inner = JS_ObjectToInnerObject(mContext, global);

    // If this assertion hits then it means that we have a window object as
    // our global, but we never called CreateOuterObject.
    NS_ASSERTION(inner == global, "Shouldn't be able to innerize here");
  }
#endif

  JSClass *c = JS_GetClass(global);

  // Whenever we end up with globals that are JSCLASS_IS_DOMJSCLASS
  // and have an nsISupports DOM object, we will need to modify this
  // check here.
  MOZ_ASSERT(!(c->flags & JSCLASS_IS_DOMJSCLASS));
  if ((~c->flags) & (JSCLASS_HAS_PRIVATE |
                     JSCLASS_PRIVATE_IS_NSISUPPORTS)) {
    return nullptr;
  }
  
  nsISupports *priv = static_cast<nsISupports*>(js::GetObjectPrivate(global));

  nsCOMPtr<nsIXPConnectWrappedNative> wrapped_native =
    do_QueryInterface(priv);

  nsCOMPtr<nsIScriptGlobalObject> sgo;
  if (wrapped_native) {
    // The global object is a XPConnect wrapped native, the native in
    // the wrapper might be the nsIScriptGlobalObject

    sgo = do_QueryWrappedNative(wrapped_native);
  } else {
    sgo = do_QueryInterface(priv);
  }

  // This'll return a pointer to something we're about to release, but
  // that's ok, the JS object will hold it alive long enough.
  return sgo;
}

JSObject*
nsJSContext::GetNativeGlobal()
{
    return JS_GetGlobalObject(mContext);
}

JSContext*
nsJSContext::GetNativeContext()
{
  return xpc_UnmarkGrayContext(mContext);
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
  nsScriptNameSpaceManager *nameSpaceManager = nsJSRuntime::GetNameSpaceManager();
  NS_ENSURE_TRUE(nameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  return nameSpaceManager->InitForContext(this);
}

nsresult
nsJSContext::SetProperty(JSObject* aTarget, const char* aPropName, nsISupports* aArgs)
{
  uint32_t  argc;
  jsval    *argv = nullptr;

  XPCAutoRequest ar(mContext);

  Maybe<nsRootedJSValueArray> tempStorage;

  nsresult rv =
    ConvertSupportsTojsvals(aArgs, GetNativeGlobal(), &argc, &argv, tempStorage);
  NS_ENSURE_SUCCESS(rv, rv);

  jsval vargs;

  // got the arguments, now attach them.

  // window.dialogArguments is supposed to be an array if a JS array
  // was passed to showModalDialog(), deal with that here.
  if (strcmp(aPropName, "dialogArguments") == 0 && argc <= 1) {
    vargs = argc ? argv[0] : JSVAL_VOID;
  } else {
    for (uint32_t i = 0; i < argc; ++i) {
      if (!JS_WrapValue(mContext, &argv[i])) {
        return NS_ERROR_FAILURE;
      }
    }

    JSObject *args = ::JS_NewArrayObject(mContext, argc, argv);
    vargs = OBJECT_TO_JSVAL(args);
  }

  // Make sure to use JS_DefineProperty here so that we can override
  // readonly XPConnect properties here as well (read dialogArguments).
  return JS_DefineProperty(mContext, aTarget, aPropName, vargs, NULL, NULL, 0)
    ? NS_OK
    : NS_ERROR_FAILURE;
}

nsresult
nsJSContext::ConvertSupportsTojsvals(nsISupports *aArgs,
                                     JSObject *aScope,
                                     uint32_t *aArgc,
                                     jsval **aArgv,
                                     Maybe<nsRootedJSValueArray> &aTempStorage)
{
  nsresult rv = NS_OK;

  // If the array implements nsIJSArgArray, just grab the values directly.
  nsCOMPtr<nsIJSArgArray> fastArray = do_QueryInterface(aArgs);
  if (fastArray != nullptr)
    return fastArray->GetArgs(aArgc, reinterpret_cast<void **>(aArgv));

  // Take the slower path converting each item.
  // Handle only nsIArray and nsIVariant.  nsIArray is only needed for
  // SetProperty('arguments', ...);

  *aArgv = nullptr;
  *aArgc = 0;

  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  NS_ENSURE_TRUE(xpc, NS_ERROR_UNEXPECTED);

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
  aTempStorage.construct(mContext);
  bool ok = aTempStorage.ref().SetCapacity(mContext, argCount);
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  jsval *argv = aTempStorage.ref().Elements();

  if (argsArray) {
    for (uint32_t argCtr = 0; argCtr < argCount && NS_SUCCEEDED(rv); argCtr++) {
      nsCOMPtr<nsISupports> arg;
      jsval *thisval = argv + argCtr;
      argsArray->QueryElementAt(argCtr, NS_GET_IID(nsISupports),
                                getter_AddRefs(arg));
      if (!arg) {
        *thisval = JSVAL_NULL;
        continue;
      }
      nsCOMPtr<nsIVariant> variant(do_QueryInterface(arg));
      if (variant != nullptr) {
        rv = xpc->VariantToJS(mContext, aScope, variant, thisval);
      } else {
        // And finally, support the nsISupportsPrimitives supplied
        // by the AppShell.  It generally will pass only strings, but
        // as we have code for handling all, we may as well use it.
        rv = AddSupportsPrimitiveTojsvals(arg, thisval);
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
          nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
          jsval v;
          rv = nsContentUtils::WrapNative(mContext, aScope, arg, &v,
                                          getter_AddRefs(wrapper));
          if (NS_SUCCEEDED(rv)) {
            *thisval = v;
          }
        }
      }
    }
  } else {
    nsCOMPtr<nsIVariant> variant = do_QueryInterface(aArgs);
    if (variant) {
      rv = xpc->VariantToJS(mContext, aScope, variant, argv);
    } else {
      NS_ERROR("Not an array, not an interface?");
      rv = NS_ERROR_UNEXPECTED;
    }
  }
  if (NS_FAILED(rv))
    return rv;
  *aArgv = argv;
  *aArgc = argCount;
  return NS_OK;
}

// This really should go into xpconnect somewhere...
nsresult
nsJSContext::AddSupportsPrimitiveTojsvals(nsISupports *aArg, jsval *aArgv)
{
  NS_PRECONDITION(aArg, "Empty arg");

  nsCOMPtr<nsISupportsPrimitive> argPrimitive(do_QueryInterface(aArg));
  if (!argPrimitive)
    return NS_ERROR_NO_INTERFACE;

  JSContext *cx = mContext;
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
        ::JS_NewUCStringCopyN(cx,
                              reinterpret_cast<const jschar *>(data.get()),
                              data.Length());
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

      nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
      JSObject *global = xpc_UnmarkGrayObject(::JS_GetGlobalObject(cx));
      jsval v;
      nsresult rv = nsContentUtils::WrapNative(cx, global,
                                               data, iid, &v,
                                               getter_AddRefs(wrapper));
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

static JSBool
CheckUniversalXPConnectForTraceMalloc(JSContext *cx)
{
    if (nsContentUtils::IsCallerChrome())
        return JS_TRUE;
    JS_ReportError(cx, "trace-malloc functions require UniversalXPConnect");
    return JS_FALSE;
}

static JSBool
TraceMallocDisable(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return JS_FALSE;

    NS_TraceMallocDisable();
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
TraceMallocEnable(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return JS_FALSE;

    NS_TraceMallocEnable();
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
TraceMallocOpenLogFile(JSContext *cx, unsigned argc, jsval *vp)
{
    int fd;
    JSString *str;

    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return JS_FALSE;

    if (argc == 0) {
        fd = -1;
    } else {
        str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
        if (!str)
            return JS_FALSE;
        JSAutoByteString filename(cx, str);
        if (!filename)
            return JS_FALSE;
        fd = open(filename.ptr(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            JS_ReportError(cx, "can't open %s: %s", filename.ptr(), strerror(errno));
            return JS_FALSE;
        }
    }
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(fd));
    return JS_TRUE;
}

static JSBool
TraceMallocChangeLogFD(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return JS_FALSE;

    int32_t fd, oldfd;
    if (argc == 0) {
        oldfd = -1;
    } else {
        if (!JS_ValueToECMAInt32(cx, JS_ARGV(cx, vp)[0], &fd))
            return JS_FALSE;
        oldfd = NS_TraceMallocChangeLogFD(fd);
        if (oldfd == -2) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }
    }
    JS_SET_RVAL(cx, vp, INT_TO_JSVAL(oldfd));
    return JS_TRUE;
}

static JSBool
TraceMallocCloseLogFD(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return JS_FALSE;

    int32_t fd;
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    if (argc == 0)
        return JS_TRUE;
    if (!JS_ValueToECMAInt32(cx, JS_ARGV(cx, vp)[0], &fd))
        return JS_FALSE;
    NS_TraceMallocCloseLogFD((int) fd);
    return JS_TRUE;
}

static JSBool
TraceMallocLogTimestamp(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return JS_FALSE;

    JSString *str = JS_ValueToString(cx, argc ? JS_ARGV(cx, vp)[0] : JSVAL_VOID);
    if (!str)
        return JS_FALSE;
    JSAutoByteString caption(cx, str);
    if (!caption)
        return JS_FALSE;
    NS_TraceMallocLogTimestamp(caption.ptr());
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
TraceMallocDumpAllocations(JSContext *cx, unsigned argc, jsval *vp)
{
    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return JS_FALSE;

    JSString *str = JS_ValueToString(cx, argc ? JS_ARGV(cx, vp)[0] : JSVAL_VOID);
    if (!str)
        return JS_FALSE;
    JSAutoByteString pathname(cx, str);
    if (!pathname)
        return JS_FALSE;
    if (NS_TraceMallocDumpAllocations(pathname.ptr()) < 0) {
        JS_ReportError(cx, "can't dump to %s: %s", pathname.ptr(), strerror(errno));
        return JS_FALSE;
    }
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSFunctionSpec TraceMallocFunctions[] = {
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

static JSBool
ReportAndDump(JSContext *cx, unsigned argc, jsval *vp)
{
  JSString *str = JS_ValueToString(cx, argc ? JS_ARGV(cx, vp)[0] : JSVAL_VOID);
  if (!str)
    return JS_FALSE;
  JSAutoByteString pathname(cx, str);
  if (!pathname)
    return JS_FALSE;

  FILE* fp = fopen(pathname.ptr(), "w");
  if (!fp) {
    JS_ReportError(cx, "DMD can't open %s: %s",
                   pathname.ptr(), strerror(errno));
    return JS_FALSE;
  }

  dmd::ClearReports();
  fprintf(stderr, "DMD: running reporters...\n");
  dmd::RunReporters();
  dmd::Writer writer(FpWrite, fp);
  dmd::Dump(writer);

  fclose(fp);

  JS_SET_RVAL(cx, vp, JSVAL_VOID);
  return JS_TRUE;
}

} // namespace dmd
} // namespace mozilla

static JSFunctionSpec DMDFunctions[] = {
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

static JSBool
JProfStartProfilingJS(JSContext *cx, unsigned argc, jsval *vp)
{
  NS_JProfStartProfiling();
  return JS_TRUE;
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

static JSBool
JProfStopProfilingJS(JSContext *cx, unsigned argc, jsval *vp)
{
  NS_JProfStopProfiling();
  return JS_TRUE;
}

void
NS_JProfStopProfiling()
{
    raise(SIGUSR1);
    //printf("Stopped jprof profiling.\n");
}

static JSBool
JProfClearCircularJS(JSContext *cx, unsigned argc, jsval *vp)
{
  NS_JProfClearCircular();
  return JS_TRUE;
}

void
NS_JProfClearCircular()
{
    raise(SIGUSR2);
    //printf("cleared jprof buffer\n");
}

static JSBool
JProfSaveCircularJS(JSContext *cx, unsigned argc, jsval *vp)
{
  // Not ideal...
  NS_JProfStopProfiling();
  NS_JProfStartProfiling();
  return JS_TRUE;
}

static JSFunctionSpec JProfFunctions[] = {
    JS_FS("JProfStartProfiling",        JProfStartProfilingJS,      0, 0),
    JS_FS("JProfStopProfiling",         JProfStopProfilingJS,       0, 0),
    JS_FS("JProfClearCircular",         JProfClearCircularJS,       0, 0),
    JS_FS("JProfSaveCircular",          JProfSaveCircularJS,        0, 0),
    JS_FS_END
};

#endif /* defined(MOZ_JPROF) */

nsresult
nsJSContext::InitClasses(JSObject* aGlobalObj)
{
  nsresult rv = InitializeExternalClasses();
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(mContext);

  JSOptionChangedCallback(js_options_dot_str, this);

  // Attempt to initialize profiling functions
  ::JS_DefineProfilingFunctions(mContext, aGlobalObj);

#ifdef NS_TRACE_MALLOC
  // Attempt to initialize TraceMalloc functions
  ::JS_DefineFunctions(mContext, aGlobalObj, TraceMallocFunctions);
#endif

#ifdef MOZ_DMD
  // Attempt to initialize DMD functions
  ::JS_DefineFunctions(mContext, aGlobalObj, DMDFunctions);
#endif

#ifdef MOZ_JPROF
  // Attempt to initialize JProf functions
  ::JS_DefineFunctions(mContext, aGlobalObj, JProfFunctions);
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

void
nsJSContext::ScriptEvaluated(bool aTerminated)
{
  if (aTerminated && mTerminations) {
    // Make sure to null out mTerminations before doing anything that
    // might cause new termination funcs to be added!
    nsJSContext::TerminationFuncClosure* start = mTerminations;
    mTerminations = nullptr;

    for (nsJSContext::TerminationFuncClosure* cur = start;
         cur;
         cur = cur->mNext) {
      (*(cur->mTerminationFunc))(cur->mTerminationFuncArg);
    }
    delete start;
  }

  JS_MaybeGC(mContext);

  if (aTerminated) {
    mOperationCallbackTime = 0;
    mModalStateTime = 0;
    mActive = true;
  }
}

void
nsJSContext::SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                    nsIDOMWindow* aRef)
{
  NS_PRECONDITION(GetExecutingScript(), "should be executing script");

  nsJSContext::TerminationFuncClosure* newClosure =
    new nsJSContext::TerminationFuncClosure(aFunc, aRef, mTerminations);
  mTerminations = newClosure;
}

bool
nsJSContext::GetScriptsEnabled()
{
  return mScriptsEnabled;
}

void
nsJSContext::SetScriptsEnabled(bool aEnabled, bool aFireTimeouts)
{
  // eeek - this seems the wrong way around - the global should callback
  // into each context, so every language is disabled.
  mScriptsEnabled = aEnabled;

  nsIScriptGlobalObject *global = GetGlobalObject();

  if (global) {
    global->SetScriptsEnabled(aEnabled, aFireTimeouts);
  }
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

bool
nsJSContext::GetExecutingScript()
{
  return JS_IsRunning(mContext) || mExecuteDepth > 0;
}

NS_IMETHODIMP
nsJSContext::ScriptExecuted()
{
  ScriptEvaluated(!::JS_IsRunning(mContext));

  return NS_OK;
}

void
FullGCTimerFired(nsITimer* aTimer, void* aClosure)
{
  NS_RELEASE(sFullGCTimer);

  uintptr_t reason = reinterpret_cast<uintptr_t>(aClosure);
  nsJSContext::GarbageCollectNow(static_cast<js::gcreason::Reason>(reason),
                                 nsJSContext::IncrementalGC);
}

//static
void
nsJSContext::GarbageCollectNow(js::gcreason::Reason aReason,
                               IsIncremental aIncremental,
                               IsCompartment aCompartment,
                               IsShrinking aShrinking,
                               int64_t aSliceMillis)
{
  SAMPLE_LABEL("GC", "GarbageCollectNow");

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

  if (!nsContentUtils::XPConnect() || !nsJSRuntime::sRuntime) {
    return;
  }

  if (sCCLockedOut && aIncremental == IncrementalGC) {
    // We're in the middle of incremental GC. Do another slice.
    js::PrepareForIncrementalGC(nsJSRuntime::sRuntime);
    js::IncrementalGC(nsJSRuntime::sRuntime, aReason, aSliceMillis);
    return;
  }

  // Use compartment GC when we're not asked to do a shrinking GC nor
  // global GC and compartment GC has been called less than
  // NS_MAX_COMPARTMENT_GC_COUNT times after the previous global GC.
  if (!sDisableExplicitCompartmentGC &&
      aShrinking != ShrinkingGC && aCompartment != NonCompartmentGC &&
      sCompartmentGCCount < NS_MAX_COMPARTMENT_GC_COUNT) {
    js::PrepareForFullGC(nsJSRuntime::sRuntime);
    for (nsJSContext* cx = sContextList; cx; cx = cx->mNext) {
      if (!cx->mActive && cx->mContext) {
        if (JSObject* global = cx->GetNativeGlobal()) {
          js::SkipCompartmentForGC(js::GetObjectCompartment(global));
        }
      }
      cx->mActive = false;
    }
    if (js::IsGCScheduled(nsJSRuntime::sRuntime)) {
      if (aIncremental == IncrementalGC) {
        js::IncrementalGC(nsJSRuntime::sRuntime, aReason, aSliceMillis);
      } else {
        js::GCForReason(nsJSRuntime::sRuntime, aReason);
      }
    }
    return;
  }

  for (nsJSContext* cx = sContextList; cx; cx = cx->mNext) {
    cx->mActive = false;
  }
  js::PrepareForFullGC(nsJSRuntime::sRuntime);
  if (aIncremental == IncrementalGC) {
    js::IncrementalGC(nsJSRuntime::sRuntime, aReason, aSliceMillis);
  } else {
    js::GCForReason(nsJSRuntime::sRuntime, aReason);
  }
}

//static
void
nsJSContext::ShrinkGCBuffersNow()
{
  SAMPLE_LABEL("GC", "ShrinkGCBuffersNow");

  KillShrinkGCBuffersTimer();

  JS_ShrinkGCBuffers(nsJSRuntime::sRuntime);
}

// Return true if any JSContext has a "global object" with a gray
// parent. The intent is to look for JS Object windows. We don't merge
// system compartments, so we don't use them to trigger merging CCs.
static bool
AnyGrayGlobalParent()
{
  if (!nsJSRuntime::sRuntime) {
    return false;
  }
  JSContext *iter = nullptr;
  JSContext *cx;
  while ((cx = JS_ContextIterator(nsJSRuntime::sRuntime, &iter))) {
    if (JSObject *global = JS_GetGlobalObject(cx)) {
      if (JSObject *parent = js::GetObjectParent(global)) {
        if (js::GCThingIsMarkedGray(parent) &&
            !js::IsSystemCompartment(js::GetGCThingCompartment(parent))) {
          return true;
        }
      }
    }
  }
  return false;
}

static bool
DoMergingCC(bool aForced)
{
  // Don't merge too many times in a row, and do at least a minimum
  // number of unmerged CCs in a row.
  static const int32_t kMinConsecutiveUnmerged = 3;
  static const int32_t kMaxConsecutiveMerged = 3;

  static int32_t sUnmergedNeeded = 0;
  static int32_t sMergedInARow = 0;

  MOZ_ASSERT(0 <= sUnmergedNeeded && sUnmergedNeeded <= kMinConsecutiveUnmerged);
  MOZ_ASSERT(0 <= sMergedInARow && sMergedInARow <= kMaxConsecutiveMerged);

  if (sMergedInARow == kMaxConsecutiveMerged) {
    MOZ_ASSERT(sUnmergedNeeded == 0);
    sUnmergedNeeded = kMinConsecutiveUnmerged;
  }

  if (sUnmergedNeeded > 0) {
    sUnmergedNeeded--;
    sMergedInARow = 0;
    return false;
  }

  if (!aForced && AnyGrayGlobalParent()) {
    sMergedInARow++;
    return true;
  } else {
    sMergedInARow = 0;
    return false;
  }

}

static void
FinishAnyIncrementalGC()
{
  if (sCCLockedOut) {
    // We're in the middle of an incremental GC, so finish it.
    js::PrepareForIncrementalGC(nsJSRuntime::sRuntime);
    js::FinishIncrementalGC(nsJSRuntime::sRuntime, js::gcreason::CC_FORCED);
  }
}

static void
FireForgetSkippable(uint32_t aSuspected, bool aRemoveChildless)
{
  PRTime startTime = PR_Now();
  FinishAnyIncrementalGC();
  nsCycleCollector_forgetSkippable(aRemoveChildless);
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
TimeBetween(PRTime start, PRTime end)
{
  MOZ_ASSERT(end >= start);
  return (uint32_t)(end - start) / PR_USEC_PER_MSEC;
}

//static
void
nsJSContext::CycleCollectNow(nsICycleCollectorListener *aListener,
                             int32_t aExtraForgetSkippableCalls,
                             bool aForced)
{
  if (!NS_IsMainThread()) {
    return;
  }

  SAMPLE_LABEL("CC", "CycleCollectNow");

  PRTime start = PR_Now();

  // Before we begin the cycle collection, make sure there is no active GC.
  bool finishedIGC = sCCLockedOut;
  FinishAnyIncrementalGC();
  PRTime endGCTime = PR_Now();
  uint32_t gcDuration = TimeBetween(start, endGCTime);

  KillCCTimer();

  uint32_t suspected = nsCycleCollector_suspectedCount();
  bool ranSyncForgetSkippable = false;

  // Run forgetSkippable synchronously to reduce the size of the CC graph. This
  // is particularly useful if we recently finished a GC.
  if (sCleanupsSinceLastGC < 2 && aExtraForgetSkippableCalls >= 0) {
    while (sCleanupsSinceLastGC < 2) {
      FireForgetSkippable(nsCycleCollector_suspectedCount(), false);
      ranSyncForgetSkippable = true;
    }
  }

  for (int32_t i = 0; i < aExtraForgetSkippableCalls; ++i) {
    FireForgetSkippable(nsCycleCollector_suspectedCount(), false);
    ranSyncForgetSkippable = true;
  }

  PRTime endSkippableTime = PR_Now();
  uint32_t skippableDuration = TimeBetween(endGCTime, endSkippableTime);

  // Prepare to actually run the CC.
  bool mergingCC = DoMergingCC(aForced);
  nsCycleCollectorResults ccResults;
  nsCycleCollector_collect(mergingCC, &ccResults, aListener);
  sCCollectedWaitingForGC += ccResults.mFreedRefCounted + ccResults.mFreedGCed;

  // If we collected a substantial amount of cycles, poke the GC since more objects
  // might be unreachable now.
  if (sCCollectedWaitingForGC > 250) {
    PokeGC(js::gcreason::CC_WAITING);
  }

  PRTime endCCTime = PR_Now();

  // Log information about the CC via telemetry, JSON and the console.
  uint32_t ccNowDuration = TimeBetween(start, endCCTime);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_FINISH_IGC, finishedIGC);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_SYNC_SKIPPABLE, ranSyncForgetSkippable);
  Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_FULL, ccNowDuration);

  if (sLastCCEndTime) {
    uint32_t timeBetween = (uint32_t)(start - sLastCCEndTime) / PR_USEC_PER_SEC;
    Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_TIME_BETWEEN, timeBetween);
  }
  sLastCCEndTime = endCCTime;

  Telemetry::Accumulate(Telemetry::FORGET_SKIPPABLE_MAX,
                        sMaxForgetSkippableTime / PR_USEC_PER_MSEC);

  PRTime delta = GetCollectionTimeDelta();

  uint32_t cleanups = sForgetSkippableBeforeCC ? sForgetSkippableBeforeCC : 1;
  uint32_t minForgetSkippableTime = (sMinForgetSkippableTime == UINT32_MAX)
    ? 0 : sMinForgetSkippableTime;

  if (sPostGCEventsToConsole) {
    nsCString mergeMsg;
    if (mergingCC) {
      mergeMsg.AssignLiteral(" merged");
    }

    nsCString gcMsg;
    if (ccResults.mForcedGC) {
      gcMsg.AssignLiteral(", forced a GC");
    }

    NS_NAMED_MULTILINE_LITERAL_STRING(kFmt,
      NS_LL("CC(T+%.1f) duration: %lums, suspected: %lu, visited: %lu RCed and %lu%s GCed, collected: %lu RCed and %lu GCed (%lu waiting for GC)%s\n")
      NS_LL("ForgetSkippable %lu times before CC, min: %lu ms, max: %lu ms, avg: %lu ms, total: %lu ms, sync: %lu ms, removed: %lu"));
    nsString msg;
    msg.Adopt(nsTextFormatter::smprintf(kFmt.get(), double(delta) / PR_USEC_PER_SEC,
                                        ccNowDuration, suspected,
                                        ccResults.mVisitedRefCounted, ccResults.mVisitedGCed, mergeMsg.get(),
                                        ccResults.mFreedRefCounted, ccResults.mFreedGCed,
                                        sCCollectedWaitingForGC, gcMsg.get(),
                                        sForgetSkippableBeforeCC,
                                        minForgetSkippableTime / PR_USEC_PER_MSEC,
                                        sMaxForgetSkippableTime / PR_USEC_PER_MSEC,
                                        (sTotalForgetSkippableTime / cleanups) /
                                          PR_USEC_PER_MSEC,
                                        sTotalForgetSkippableTime / PR_USEC_PER_MSEC,
                                        skippableDuration, sRemovedPurples));
    nsCOMPtr<nsIConsoleService> cs =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (cs) {
      cs->LogStringMessage(msg.get());
    }
  }

  if (sPostGCEventsToObserver) {
    NS_NAMED_MULTILINE_LITERAL_STRING(kJSONFmt,
       NS_LL("{ \"timestamp\": %llu, ")
         NS_LL("\"duration\": %llu, ")
         NS_LL("\"finish_gc_duration\": %llu, ")
         NS_LL("\"sync_skippable_duration\": %llu, ")
         NS_LL("\"suspected\": %lu, ")
         NS_LL("\"visited\": { ")
             NS_LL("\"RCed\": %lu, ")
             NS_LL("\"GCed\": %lu }, ")
         NS_LL("\"collected\": { ")
             NS_LL("\"RCed\": %lu, ")
             NS_LL("\"GCed\": %lu }, ")
         NS_LL("\"waiting_for_gc\": %lu, ")
         NS_LL("\"forced_gc\": %d, ")
         NS_LL("\"forget_skippable\": { ")
             NS_LL("\"times_before_cc\": %lu, ")
             NS_LL("\"min\": %lu, ")
             NS_LL("\"max\": %lu, ")
             NS_LL("\"avg\": %lu, ")
             NS_LL("\"total\": %lu, ")
             NS_LL("\"removed\": %lu } ")
       NS_LL("}"));
    nsString json;
    json.Adopt(nsTextFormatter::smprintf(kJSONFmt.get(), endCCTime,
                                         ccNowDuration, gcDuration, skippableDuration,
                                         suspected,
                                         ccResults.mVisitedRefCounted, ccResults.mVisitedGCed,
                                         ccResults.mFreedRefCounted, ccResults.mFreedGCed,
                                         sCCollectedWaitingForGC,
                                         ccResults.mForcedGC,
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
}

// static
void
InterSliceGCTimerFired(nsITimer *aTimer, void *aClosure)
{
  NS_RELEASE(sInterSliceGCTimer);
  nsJSContext::GarbageCollectNow(js::gcreason::INTER_SLICE_GC,
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
  nsJSContext::GarbageCollectNow(static_cast<js::gcreason::Reason>(reason),
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
          sLastCCEndTime + NS_CC_FORCED < PR_Now());
}

static void
CCTimerFired(nsITimer *aTimer, void *aClosure)
{
  if (sDidShutdown) {
    return;
  }

  static uint32_t ccDelay = NS_CC_DELAY;
  if (sCCLockedOut) {
    ccDelay = NS_CC_DELAY / 3;

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
  // late timer fire, where we may run the CC.
  const uint32_t numEarlyTimerFires = ccDelay / NS_CC_SKIPPABLE_DELAY - 2;
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
      // triggering a CC. Let CycleCollectNow finish the current IGC, if any,
      // because that will allow us to include the GC time in the CC pause.
      nsJSContext::CycleCollectNow(nullptr, 0, false);
    }
  } else if ((sPreviousSuspectedCount + 100) <= suspected) {
      // Only do a forget skippable if there are more than a few new objects.
      FireForgetSkippable(suspected, false);
  }

  if (isLateTimerFire) {
    ccDelay = NS_CC_DELAY;

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
  PokeGC(js::gcreason::LOAD_END);
}

// static
void
nsJSContext::PokeGC(js::gcreason::Reason aReason, int aDelay)
{
  if (sGCTimer || sShuttingDown) {
    // There's already a timer for GC'ing, just return
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
  if (sCCTimer || sShuttingDown) {
    return;
  }

  if (ShouldTriggerCC(nsCycleCollector_suspectedCount())) {
    sCCTimerFireCount = 0;
    CallCreateInstance("@mozilla.org/timer;1", &sCCTimer);
    if (!sCCTimer) {
      return;
    }
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

void
nsJSContext::GC(js::gcreason::Reason aReason)
{
  mActive = true;
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
DOMGCSliceCallback(JSRuntime *aRt, js::GCProgress aProgress, const js::GCDescription &aDesc)
{
  NS_ASSERTION(NS_IsMainThread(), "GCs must run on the main thread");

  if (aProgress == js::GC_CYCLE_END) {
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
  if (aProgress == js::GC_CYCLE_BEGIN) {
    sCCLockedOut = true;
    nsJSContext::KillShrinkGCBuffersTimer();
  } else if (aProgress == js::GC_CYCLE_END) {
    sCCLockedOut = false;
  }

  // The GC has more work to do, so schedule another GC slice.
  if (aProgress == js::GC_SLICE_END) {
    nsJSContext::KillInterSliceGCTimer();
    if (!sShuttingDown) {
      CallCreateInstance("@mozilla.org/timer;1", &sInterSliceGCTimer);
      sInterSliceGCTimer->InitWithFuncCallback(InterSliceGCTimerFired,
                                               NULL,
                                               NS_INTERSLICE_GC_DELAY,
                                               nsITimer::TYPE_ONE_SHOT);
    }
  }

  if (aProgress == js::GC_CYCLE_END) {
    // May need to kill the inter-slice GC timer
    nsJSContext::KillInterSliceGCTimer();

    sCCollectedWaitingForGC = 0;
    sCleanupsSinceLastGC = 0;
    sNeedsFullCC = true;
    nsJSContext::MaybePokeCC();

    if (aDesc.isCompartment) {
      ++sCompartmentGCCount;
      if (!sFullGCTimer && !sShuttingDown) {
        CallCreateInstance("@mozilla.org/timer;1", &sFullGCTimer);
        js::gcreason::Reason reason = js::gcreason::FULL_GC_TIMER;
        sFullGCTimer->InitWithFuncCallback(FullGCTimerFired,
                                           reinterpret_cast<void *>(reason),
                                           NS_FULL_GC_DELAY,
                                           nsITimer::TYPE_ONE_SHOT);
      }
    } else {
      sCompartmentGCCount = 0;
      nsJSContext::KillFullGCTimer();

      // Avoid shrinking during heavy activity, which is suggested by
      // compartment GC.
      nsJSContext::PokeShrinkGCBuffers();
    }
  }

  if (sPrevGCSliceCallback)
    (*sPrevGCSliceCallback)(aRt, aProgress, aDesc);
}

static void
DOMAnalysisPurgeCallback(JSRuntime *aRt, JSFlatString *aDesc)
{
  NS_ASSERTION(NS_IsMainThread(), "GCs must run on the main thread");

  PRTime delta = GetCollectionTimeDelta();

  if (sPostGCEventsToConsole) {
    NS_NAMED_LITERAL_STRING(kFmt, "Analysis Purge (T+%.1f) ");
    nsString prefix;
    prefix.Adopt(nsTextFormatter::smprintf(kFmt.get(),
                                           double(delta) / PR_USEC_PER_SEC));

    nsDependentJSString stats(aDesc);
    nsString msg = prefix + stats;

    nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (cs) {
      cs->LogStringMessage(msg.get());
    }
  }

  if (sPrevAnalysisPurgeCallback)
    (*sPrevAnalysisPurgeCallback)(aRt, aDesc);
}

// Script object mananagement - note duplicate implementation
// in nsJSRuntime below...
nsresult
nsJSContext::HoldScriptObject(void* aScriptObject)
{
    NS_ASSERTION(sIsInitialized, "runtime not initialized");
    if (! nsJSRuntime::sRuntime) {
        NS_NOTREACHED("couldn't add GC root - no runtime");
        return NS_ERROR_FAILURE;
    }

    ::JS_LockGCThingRT(nsJSRuntime::sRuntime, aScriptObject);
    return NS_OK;
}

nsresult
nsJSContext::DropScriptObject(void* aScriptObject)
{
  NS_ASSERTION(sIsInitialized, "runtime not initialized");
  if (! nsJSRuntime::sRuntime) {
    NS_NOTREACHED("couldn't remove GC root");
    return NS_ERROR_FAILURE;
  }

  ::JS_UnlockGCThingRT(nsJSRuntime::sRuntime, aScriptObject);
  return NS_OK;
}

void
nsJSContext::ReportPendingException()
{
  if (mIsInitialized) {
    nsJSUtils::ReportPendingException(mContext);
  }
}

/**********************************************************************
 * nsJSRuntime implementation
 *********************************************************************/

// QueryInterface implementation for nsJSRuntime
NS_INTERFACE_MAP_BEGIN(nsJSRuntime)
  NS_INTERFACE_MAP_ENTRY(nsIScriptRuntime)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsJSRuntime)
NS_IMPL_RELEASE(nsJSRuntime)

already_AddRefed<nsIScriptContext>
nsJSRuntime::CreateContext(bool aGCOnDestruction,
                           nsIScriptGlobalObject* aGlobalObject)
{
  nsCOMPtr<nsIScriptContext> scriptContext =
    new nsJSContext(sRuntime, aGCOnDestruction, aGlobalObject);
  return scriptContext.forget();
}

//static
void
nsJSRuntime::Startup()
{
  // initialize all our statics, so that we can restart XPCOM
  sGCTimer = sFullGCTimer = sCCTimer = nullptr;
  sCCLockedOut = false;
  sCCLockedOutTime = 0;
  sLastCCEndTime = 0;
  sPendingLoadCount = 0;
  sLoadingInProgress = false;
  sCCollectedWaitingForGC = 0;
  sPostGCEventsToConsole = false;
  sDisableExplicitCompartmentGC = false;
  sNeedsFullCC = false;
  gNameSpaceManager = nullptr;
  sRuntimeService = nullptr;
  sRuntime = nullptr;
  sIsInitialized = false;
  sDidShutdown = false;
  sShuttingDown = false;
  sContextCount = 0;
  sSecurityManager = nullptr;
}

static int
MaxScriptRunTimePrefChangedCallback(const char *aPrefName, void *aClosure)
{
  // Default limit on script run time to 10 seconds. 0 means let
  // scripts run forever.
  bool isChromePref =
    strcmp(aPrefName, "dom.max_chrome_script_run_time") == 0;
  int32_t time = Preferences::GetInt(aPrefName, isChromePref ? 20 : 10);

  PRTime t;
  if (time <= 0) {
    // Let scripts run for a really, really long time.
    t = 0x40000000LL << 32;
  } else {
    t = time * PR_USEC_PER_SEC;
  }

  if (isChromePref) {
    sMaxChromeScriptRunTime = t;
  } else {
    sMaxScriptRunTime = t;
  }

  return 0;
}

static int
ReportAllJSExceptionsPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool reportAll = Preferences::GetBool(aPrefName, false);
  nsContentUtils::XPConnect()->SetReportAllJSExceptions(reportAll);
  return 0;
}

static int
SetMemoryHighWaterMarkPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  int32_t highwatermark = Preferences::GetInt(aPrefName, 128);

  JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_MAX_MALLOC_BYTES,
                    highwatermark * 1024L * 1024L);
  return 0;
}

static int
SetMemoryMaxPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  int32_t pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  uint32_t max = (pref <= 0 || pref >= 0x1000) ? -1 : (uint32_t)pref * 1024 * 1024;
  JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_MAX_BYTES, max);
  return 0;
}

static int
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
  JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_MODE, mode);
  return 0;
}

static int
SetMemoryGCSliceTimePrefChangedCallback(const char* aPrefName, void* aClosure)
{
  int32_t pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  if (pref > 0 && pref < 100000)
    JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_SLICE_TIME_BUDGET, pref);
  return 0;
}

static int
SetMemoryGCPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  int32_t pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  if (pref > 0 && pref < 10000)
    JS_SetGCParameter(nsJSRuntime::sRuntime, (JSGCParamKey)(intptr_t)aClosure, pref);
  return 0;
}

static int
SetMemoryGCDynamicHeapGrowthPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool pref = Preferences::GetBool(aPrefName);
  JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_DYNAMIC_HEAP_GROWTH, pref);
  return 0;
}

static int
SetMemoryGCDynamicMarkSlicePrefChangedCallback(const char* aPrefName, void* aClosure)
{
  bool pref = Preferences::GetBool(aPrefName);
  JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_DYNAMIC_MARK_SLICE, pref);
  return 0;
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
    JS::Value dataArray;
    if (!JS_ReadUint32Pair(reader, &width, &height) ||
        !JS_ReadTypedArray(reader, &dataArray)) {
      return nullptr;
    }
    MOZ_ASSERT(dataArray.isObject());

    // Construct the ImageData.
    nsRefPtr<ImageData> imageData = new ImageData(width, height,
                                                  dataArray.toObject());
    // Wrap it in a jsval.
    JSObject* global = JS_GetGlobalForScopeChain(cx);
    if (!global) {
      return nullptr;
    }
    return imageData->WrapObject(cx, global);
  }

  // Don't know what this is. Bail.
  xpc::Throw(cx, NS_ERROR_DOM_DATA_CLONE_ERR);
  return nullptr;
}

JSBool
NS_DOMWriteStructuredClone(JSContext* cx,
                           JSStructuredCloneWriter* writer,
                           JSObject* obj,
                           void *closure)
{
  ImageData* imageData;
  nsresult rv = UnwrapObject<ImageData>(cx, obj, imageData);
  if (NS_FAILED(rv)) {
    // Don't know what this is. Bail.
    xpc::Throw(cx, NS_ERROR_DOM_DATA_CLONE_ERR);
    return JS_FALSE;
  }

  // Prepare the ImageData internals.
  uint32_t width = imageData->Width();
  uint32_t height = imageData->Height();
  JS::Value dataArray = JS::ObjectValue(*imageData->GetDataObject());

  // Write the internals to the stream.
  return JS_WriteUint32Pair(writer, SCTAG_DOM_IMAGEDATA, 0) &&
         JS_WriteUint32Pair(writer, width, height) &&
         JS_WriteTypedArray(writer, dataArray);
}

void
NS_DOMStructuredCloneError(JSContext* cx,
                           uint32_t errorid)
{
  // We don't currently support any extensions to structured cloning.
  xpc::Throw(cx, NS_ERROR_DOM_DATA_CLONE_ERR);
}

//static
nsresult
nsJSRuntime::Init()
{
  if (sIsInitialized) {
    if (!nsContentUtils::XPConnect())
      return NS_ERROR_NOT_AVAILABLE;

    return NS_OK;
  }

  nsresult rv = CallGetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID,
                               &sSecurityManager);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = CallGetService(kJSRuntimeServiceContractID, &sRuntimeService);
  // get the JSRuntime from the runtime svc, if possible
  NS_ENSURE_SUCCESS(rv, rv);

  rv = sRuntimeService->GetRuntime(&sRuntime);
  NS_ENSURE_SUCCESS(rv, rv);

  // Let's make sure that our main thread is the same as the xpcom main thread.
  NS_ASSERTION(NS_IsMainThread(), "bad");

  sPrevGCSliceCallback = js::SetGCSliceCallback(sRuntime, DOMGCSliceCallback);
  sPrevAnalysisPurgeCallback = js::SetAnalysisPurgeCallback(sRuntime, DOMAnalysisPurgeCallback);

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

  // Set these global xpconnect options...
  Preferences::RegisterCallback(MaxScriptRunTimePrefChangedCallback,
                                "dom.max_script_run_time");
  MaxScriptRunTimePrefChangedCallback("dom.max_script_run_time", nullptr);

  Preferences::RegisterCallback(MaxScriptRunTimePrefChangedCallback,
                                "dom.max_chrome_script_run_time");
  MaxScriptRunTimePrefChangedCallback("dom.max_chrome_script_run_time",
                                      nullptr);

  Preferences::RegisterCallback(ReportAllJSExceptionsPrefChangedCallback,
                                "dom.report_all_js_exceptions");
  ReportAllJSExceptionsPrefChangedCallback("dom.report_all_js_exceptions",
                                           nullptr);

  Preferences::RegisterCallback(SetMemoryHighWaterMarkPrefChangedCallback,
                                "javascript.options.mem.high_water_mark");
  SetMemoryHighWaterMarkPrefChangedCallback("javascript.options.mem.high_water_mark",
                                            nullptr);

  Preferences::RegisterCallback(SetMemoryMaxPrefChangedCallback,
                                "javascript.options.mem.max");
  SetMemoryMaxPrefChangedCallback("javascript.options.mem.max",
                                  nullptr);

  Preferences::RegisterCallback(SetMemoryGCModePrefChangedCallback,
                                "javascript.options.mem.gc_per_compartment");
  SetMemoryGCModePrefChangedCallback("javascript.options.mem.gc_per_compartment",
                                     nullptr);

  Preferences::RegisterCallback(SetMemoryGCModePrefChangedCallback,
                                "javascript.options.mem.gc_incremental");
  SetMemoryGCModePrefChangedCallback("javascript.options.mem.gc_incremental",
                                     nullptr);

  Preferences::RegisterCallback(SetMemoryGCSliceTimePrefChangedCallback,
                                "javascript.options.mem.gc_incremental_slice_ms");
  SetMemoryGCSliceTimePrefChangedCallback("javascript.options.mem.gc_incremental_slice_ms",
                                          nullptr);

  Preferences::RegisterCallback(SetMemoryGCPrefChangedCallback,
                                "javascript.options.mem.gc_high_frequency_time_limit_ms");
  SetMemoryGCPrefChangedCallback("javascript.options.mem.gc_high_frequency_time_limit_ms",
                                 (void *)JSGC_HIGH_FREQUENCY_TIME_LIMIT);

  Preferences::RegisterCallback(SetMemoryGCDynamicMarkSlicePrefChangedCallback,
                                "javascript.options.mem.gc_dynamic_mark_slice");
  SetMemoryGCDynamicMarkSlicePrefChangedCallback("javascript.options.mem.gc_dynamic_mark_slice",
                                                 nullptr);

  Preferences::RegisterCallback(SetMemoryGCDynamicHeapGrowthPrefChangedCallback,
                                "javascript.options.mem.gc_dynamic_heap_growth");
  SetMemoryGCDynamicHeapGrowthPrefChangedCallback("javascript.options.mem.gc_dynamic_heap_growth",
                                                  nullptr);

  Preferences::RegisterCallback(SetMemoryGCPrefChangedCallback,
                                "javascript.options.mem.gc_low_frequency_heap_growth");
  SetMemoryGCPrefChangedCallback("javascript.options.mem.gc_low_frequency_heap_growth",
                                 (void *)JSGC_LOW_FREQUENCY_HEAP_GROWTH);

  Preferences::RegisterCallback(SetMemoryGCPrefChangedCallback,
                                "javascript.options.mem.gc_high_frequency_heap_growth_min");
  SetMemoryGCPrefChangedCallback("javascript.options.mem.gc_high_frequency_heap_growth_min",
                                 (void *)JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN);

  Preferences::RegisterCallback(SetMemoryGCPrefChangedCallback,
                                "javascript.options.mem.gc_high_frequency_heap_growth_max");
  SetMemoryGCPrefChangedCallback("javascript.options.mem.gc_high_frequency_heap_growth_max",
                                 (void *)JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX);

  Preferences::RegisterCallback(SetMemoryGCPrefChangedCallback,
                                "javascript.options.mem.gc_high_frequency_low_limit_mb");
  SetMemoryGCPrefChangedCallback("javascript.options.mem.gc_high_frequency_low_limit_mb",
                                 (void *)JSGC_HIGH_FREQUENCY_LOW_LIMIT);

  Preferences::RegisterCallback(SetMemoryGCPrefChangedCallback,
                                "javascript.options.mem.gc_high_frequency_high_limit_mb");
  SetMemoryGCPrefChangedCallback("javascript.options.mem.gc_high_frequency_high_limit_mb",
                                 (void *)JSGC_HIGH_FREQUENCY_HIGH_LIMIT);

  Preferences::RegisterCallback(SetMemoryGCPrefChangedCallback,
                                "javascript.options.mem.analysis_purge_mb",
                                (void *)JSGC_ANALYSIS_PURGE_TRIGGER);
  SetMemoryGCPrefChangedCallback("javascript.options.mem.analysis_purge_mb",
                                 (void *)JSGC_ANALYSIS_PURGE_TRIGGER);

  Preferences::RegisterCallback(SetMemoryGCPrefChangedCallback,
                               "javascript.options.mem.gc_allocation_threshold_mb");
  SetMemoryGCPrefChangedCallback("javascript.options.mem.gc_allocation_threshold_mb",
                                (void *)JSGC_ALLOCATION_THRESHOLD);
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs)
    return NS_ERROR_FAILURE;

  Preferences::AddBoolVarCache(&sGCOnMemoryPressure,
                               "javascript.options.gc_on_memory_pressure",
                               true);

  nsIObserver* observer = new nsJSEnvironmentObserver();
  obs->AddObserver(observer, "memory-pressure", false);
  obs->AddObserver(observer, "quit-application", false);

  sIsInitialized = true;

  return NS_OK;
}

//static
nsScriptNameSpaceManager*
nsJSRuntime::GetNameSpaceManager()
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

/* static */
void
nsJSRuntime::Shutdown()
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

// Script object mananagement - note duplicate implementation
// in nsJSContext above...
nsresult
nsJSRuntime::HoldScriptObject(void* aScriptObject)
{
    NS_ASSERTION(sIsInitialized, "runtime not initialized");
    if (! sRuntime) {
        NS_NOTREACHED("couldn't remove GC root - no runtime");
        return NS_ERROR_FAILURE;
    }

    ::JS_LockGCThingRT(sRuntime, aScriptObject);
    return NS_OK;
}

nsresult
nsJSRuntime::DropScriptObject(void* aScriptObject)
{
  NS_ASSERTION(sIsInitialized, "runtime not initialized");
  if (! sRuntime) {
    NS_NOTREACHED("couldn't remove GC root");
    return NS_ERROR_FAILURE;
  }

  ::JS_UnlockGCThingRT(sRuntime, aScriptObject);
  return NS_OK;
}

// A factory for the runtime.
nsresult NS_CreateJSRuntime(nsIScriptRuntime **aRuntime)
{
  nsresult rv = nsJSRuntime::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  *aRuntime = new nsJSRuntime();
  if (*aRuntime == nullptr)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_IF_ADDREF(*aRuntime);
  return NS_OK;
}

// A fast-array class for JS.  This class supports both nsIJSScriptArray and
// nsIArray.  If it is JS itself providing and consuming this class, all work
// can be done via nsIJSScriptArray, and avoid the conversion of elements
// to/from nsISupports.
// When consumed by non-JS (eg, another script language), conversion is done
// on-the-fly.
class nsJSArgArray MOZ_FINAL : public nsIJSArgArray {
public:
  nsJSArgArray(JSContext *aContext, uint32_t argc, jsval *argv, nsresult *prv);
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
  jsval *mArgv;
  uint32_t mArgc;
};

nsJSArgArray::nsJSArgArray(JSContext *aContext, uint32_t argc, jsval *argv,
                           nsresult *prv) :
    mContext(aContext),
    mArgv(nullptr),
    mArgc(argc)
{
  // copy the array - we don't know its lifetime, and ours is tied to xpcom
  // refcounting.  Alloc zero'd array so cleanup etc is safe.
  if (argc) {
    mArgv = (jsval *) PR_CALLOC(argc * sizeof(jsval));
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
    NS_HOLD_JS_OBJECTS(this, nsJSArgArray);
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
    PR_DELETE(mArgv);
  }
  if (mArgc > 0) {
    mArgc = 0;
    NS_DROP_JS_OBJECTS(this, nsJSArgArray);
  }
}

// QueryInterface implementation for nsJSArgArray
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSArgArray)
  tmp->ReleaseJSObjects();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsJSArgArray)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSArgArray)
  jsval *argv = tmp->mArgv;
  if (argv) {
    jsval *end;
    for (end = argv + tmp->mArgc; argv < end; ++argv) {
      if (JSVAL_IS_GCTHING(*argv))
        NS_IMPL_CYCLE_COLLECTION_TRACE_JS_CALLBACK(JSVAL_TO_GCTHING(*argv),
                                                   "mArgv[i]")
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
    return nsContentUtils::XPConnect()->JSToVariant(mContext, mArgv[index],
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
                                       static_cast<jsval *>(argv), &rv);
  if (ret == nullptr)
    return NS_ERROR_OUT_OF_MEMORY;
  if (NS_FAILED(rv)) {
    delete ret;
    return rv;
  }
  return ret->QueryInterface(NS_GET_IID(nsIArray), (void **)aArray);
}
