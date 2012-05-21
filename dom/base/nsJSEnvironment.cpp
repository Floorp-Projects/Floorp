/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#include "nsDOMClassInfo.h"
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

#include "mozilla/FunctionTimer.h"
#include "mozilla/Preferences.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/BindingUtils.h"

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

// The amount of time we wait between a request to CC (after GC ran)
// and doing the actual CC.
#define NS_CC_DELAY                 6000 // ms

#define NS_CC_SKIPPABLE_DELAY       400 // ms

// Force a CC after this long if there's anything in the purple buffer.
#define NS_CC_FORCED                (2 * 60 * PR_USEC_PER_SEC) // 2 min

// Don't allow an incremental GC to lock out the CC for too long.
#define NS_MAX_CC_LOCKEDOUT_TIME    (5 * PR_USEC_PER_SEC) // 5 seconds

// Trigger a CC if the purple buffer exceeds this size when we check it.
#define NS_CC_PURPLE_LIMIT          250

#define JAVASCRIPT nsIProgrammingLanguage::JAVASCRIPT

// if you add statics here, add them to the list in nsJSRuntime::Startup

static nsITimer *sGCTimer;
static nsITimer *sShrinkGCBuffersTimer;
static nsITimer *sCCTimer;
static nsITimer *sFullGCTimer;

static PRTime sLastCCEndTime;

static bool sCCLockedOut;
static PRTime sCCLockedOutTime;

static js::GCSliceCallback sPrevGCSliceCallback;

// The number of currently pending document loads. This count isn't
// guaranteed to always reflect reality and can't easily as we don't
// have an easy place to know when a load ends or is interrupted in
// all cases. This counter also gets reset if we end up GC'ing while
// we're waiting for a slow page to load. IOW, this count may be 0
// even when there are pending loads.
static PRUint32 sPendingLoadCount;
static bool sLoadingInProgress;

static PRUint32 sCCollectedWaitingForGC;
static bool sPostGCEventsToConsole;
static bool sDisableExplicitCompartmentGC;
static PRUint32 sCCTimerFireCount = 0;
static PRUint32 sMinForgetSkippableTime = PR_UINT32_MAX;
static PRUint32 sMaxForgetSkippableTime = 0;
static PRUint32 sTotalForgetSkippableTime = 0;
static PRUint32 sRemovedPurples = 0;
static PRUint32 sForgetSkippableBeforeCC = 0;
static PRUint32 sPreviousSuspectedCount = 0;
static PRUint32 sCompartmentGCCount = NS_MAX_COMPARTMENT_GC_COUNT;
static PRUint32 sCleanupsSinceLastGC = PR_UINT32_MAX;
static bool sNeedsFullCC = false;
static nsJSContext *sContextList = nsnull;

nsScriptNameSpaceManager *gNameSpaceManager;

static nsIJSRuntimeService *sRuntimeService;
JSRuntime *nsJSRuntime::sRuntime;

static const char kJSRuntimeServiceContractID[] =
  "@mozilla.org/js/xpc/RuntimeService;1";

static PRTime sFirstCollectionTime;

static bool sIsInitialized;
static bool sDidShutdown;

static PRInt32 sContextCount;

static PRTime sMaxScriptRunTime;
static PRTime sMaxChromeScriptRunTime;

static nsIScriptSecurityManager *sSecurityManager;

// nsMemoryPressureObserver observes the memory-pressure notifications
// and forces a garbage collection and cycle collection when it happens, if
// the appropriate pref is set.

static bool sGCOnMemoryPressure;

class nsMemoryPressureObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(nsMemoryPressureObserver, nsIObserver)

NS_IMETHODIMP
nsMemoryPressureObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                  const PRUnichar* aData)
{
  if (sGCOnMemoryPressure) {
    nsJSContext::GarbageCollectNow(js::gcreason::MEM_PRESSURE, nsGCShrinking,
                                   true);
    nsJSContext::CycleCollectNow();
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
  nsIDocShell *docShell = win ? win->GetDocShell() : nsnull;
  if (docShell) {
    nsRefPtr<nsPresContext> presContext;
    docShell->GetPresContext(getter_AddRefs(presContext));

    static PRInt32 errorDepth; // Recursion prevention
    ++errorDepth;

    if (presContext && errorDepth < 2) {
      // Dispatch() must be synchronous for the recursion block
      // (errorDepth) to work.
      nsEventDispatcher::Dispatch(win, presContext, aErrorEvent, nsnull,
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
                   PRUint32 aLineNr, PRUint32 aColumn, PRUint32 aFlags,
                   const nsAString& aErrorMsg,
                   const nsAString& aFileName,
                   const nsAString& aSourceLine,
                   bool aDispatchEvent,
                   PRUint64 aInnerWindowID)
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
      nsIDocShell* docShell = win ? win->GetDocShell() : nsnull;
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

          nsEventDispatcher::Dispatch(win, presContext, &errorevent, nsnull,
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

      if (errorObject != nsnull) {
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

        rv = errorObject->InitWithWindowID(mErrorMsg.get(), mFileName.get(),
                                           mSourceLine.get(),
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
  PRUint32                        mLineNr;
  PRUint32                        mColumn;
  PRUint32                        mFlags;
  nsString                        mErrorMsg;
  nsString                        mFileName;
  nsString                        mSourceLine;
  bool                            mDispatchEvent;
  PRUint64                        mInnerWindowID;

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
    if (JS_DescribeScriptedCaller(cx, nsnull, nsnull)) {
      return;
    }

    nsIXPConnect* xpc = nsContentUtils::XPConnect();
    if (xpc) {
      nsAXPCNativeCallContext *cc = nsnull;
      xpc->GetCurrentNativeCallContext(&cc);
      if (cc) {
        nsAXPCNativeCallContext *prev = cc;
        while (NS_SUCCEEDED(prev->GetPreviousCallContext(&prev)) && prev) {
          PRUint16 lang;
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
            (js::GetErrorTypeNameFromNumber(cx, report->errorNumber));
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
      PRUint64 innerWindowID = 0;
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
  nsCAutoString error;
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
    return nsnull;
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

  return nsnull;
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

  nsCAutoString spec;
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

  nsCAutoString spec;
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
  NS_ENSURE_TRUE(win, nsnull);

  nsIDocShell *docShell = win->GetDocShell();
  NS_ENSURE_TRUE(docShell, nsnull);

  nsCOMPtr<nsIInterfaceRequestor> ireq(do_QueryInterface(docShell));
  NS_ENSURE_TRUE(ireq, nsnull);

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
    debugPossible = ((jsds_IsOn && (jsdHook != nsnull)) || !jsds_IsOn);
  }
#endif

  // Get localizable strings
  nsXPIDLString title, msg, stopButton, waitButton, debugButton, neverShowDlg;

  rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                          "KillScriptTitle",
                                          title);

  rv |= nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                           "StopScriptButton",
                                           stopButton);

  rv |= nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                           "WaitForScriptButton",
                                           waitButton);

  rv |= nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                           "DontAskAgain",
                                           neverShowDlg);


  if (debugPossible) {
    rv |= nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                             "DebugScriptButton",
                                             debugButton);

    rv |= nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                             "KillScriptWithDebugMessage",
                                             msg);
  }
  else {
    rv |= nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                             "KillScriptMessage",
                                             msg);
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

  PRInt32 buttonPressed = 0; //In case user exits dialog by clicking X
  bool neverShowDlgChk = false;
  PRUint32 buttonFlags = nsIPrompt::BUTTON_POS_1_DEFAULT +
                         (nsIPrompt::BUTTON_TITLE_IS_STRING *
                          (nsIPrompt::BUTTON_POS_0 + nsIPrompt::BUTTON_POS_1));

  // Add a third button if necessary:
  if (debugPossible)
    buttonFlags += nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_2;

  // Null out the operation callback while we're re-entering JS here.
  ::JS_SetOperationCallback(cx, nsnull);

  // Open the dialog.
  rv = prompt->ConfirmEx(title, msg, buttonFlags, waitButton, stopButton,
                         debugButton, neverShowDlg, &neverShowDlgChk,
                         &buttonPressed);

  ::JS_SetOperationCallback(cx, DOMOperationCallback);

  if (NS_FAILED(rv) || (buttonPressed == 0)) {
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
static const char js_relimit_option_str[]= JS_OPTIONS_DOT_STR "relimit";
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
static const char js_memlog_option_str[] = JS_OPTIONS_DOT_STR "mem.log";
static const char js_disable_explicit_compartment_gc[] =
  JS_OPTIONS_DOT_STR "disable_explicit_compartment_gc";

int
nsJSContext::JSOptionChangedCallback(const char *pref, void *data)
{
  nsJSContext *context = reinterpret_cast<nsJSContext *>(data);
  PRUint32 oldDefaultJSOptions = context->mDefaultJSOptions;
  PRUint32 newDefaultJSOptions = oldDefaultJSOptions;

  sPostGCEventsToConsole = Preferences::GetBool(js_memlog_option_str);
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
  bool useHardening = Preferences::GetBool(js_jit_hardening_str);
  nsCOMPtr<nsIXULRuntime> xr = do_GetService(XULRUNTIME_SERVICE_CONTRACTID);
  if (xr) {
    bool safeMode = false;
    xr->GetInSafeMode(&safeMode);
    if (safeMode) {
      useMethodJIT = false;
      usePCCounts = false;
      useTypeInference = false;
      useMethodJITAlways = true;
      useHardening = false;
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

  bool relimit = Preferences::GetBool(js_relimit_option_str);
  if (relimit)
    newDefaultJSOptions |= JSOPTION_RELIMIT;
  else
    newDefaultJSOptions &= ~JSOPTION_RELIMIT;

  ::JS_SetOptions(context->mContext, newDefaultJSOptions & JSRUNOPTION_MASK);

  // Save the new defaults for the next page load (InitContext).
  context->mDefaultJSOptions = newDefaultJSOptions;

  JSRuntime *rt = JS_GetRuntime(context->mContext);
  JS_SetJitHardening(rt, useHardening);

#ifdef JS_GC_ZEAL
  PRInt32 zeal = Preferences::GetInt(js_zeal_option_str, -1);
  PRInt32 frequency = Preferences::GetInt(js_zeal_frequency_str, JS_DEFAULT_ZEAL_FREQ);
  if (zeal >= 0)
    ::JS_SetGCZeal(context->mContext, (PRUint8)zeal, frequency);
#endif

  return 0;
}

nsJSContext::nsJSContext(JSRuntime *aRuntime)
  : mActive(false),
    mGCOnDestruction(true),
    mExecuteDepth(0)
{
  mNext = sContextList;
  mPrev = &sContextList;
  if (sContextList) {
    sContextList->mPrev = &mNext;
  }
  sContextList = this;

  ++sContextCount;

  mDefaultJSOptions = JSOPTION_PRIVATE_IS_NSISUPPORTS;

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
  mTerminations = nsnull;
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

  mGlobalObjectRef = nsnull;

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
  ::JS_SetContextPrivate(mContext, nsnull);

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
  mContext = nsnull;
}

// QueryInterface implementation for nsJSContext
NS_IMPL_CYCLE_COLLECTION_CLASS(nsJSContext)
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSContext)
NS_IMPL_CYCLE_COLLECTION_TRACE_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSContext)
  NS_ASSERTION(!tmp->mContext || js::GetContextOutstandingRequests(tmp->mContext) == 0,
               "Trying to unlink a context with outstanding requests.");
  tmp->mIsInitialized = false;
  tmp->mGCOnDestruction = false;
  tmp->DestroyJSContext();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mGlobalObjectRef)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsJSContext)
  NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsJSContext, tmp->GetCCRefcnt())
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mGlobalObjectRef)
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
  if (NS_LIKELY(mContext))
    refcnt += js::GetContextOutstandingRequests(mContext);
  return refcnt;
}

nsresult
nsJSContext::EvaluateStringWithValue(const nsAString& aScript,
                                     JSObject* aScopeObject,
                                     nsIPrincipal *aPrincipal,
                                     const char *aURL,
                                     PRUint32 aLineNo,
                                     PRUint32 aVersion,
                                     JS::Value* aRetValue,
                                     bool* aIsUndefined)
{
  NS_TIME_FUNCTION_MIN_FMT(1.0, "%s (line %d) (url: %s, line: %d)", MOZ_FUNCTION_NAME,
                           __LINE__, aURL, aLineNo);

  SAMPLE_LABEL("JS", "EvaluateStringWithValue");
  NS_ABORT_IF_FALSE(aScopeObject,
    "Shouldn't call EvaluateStringWithValue with null scope object.");

  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    if (aIsUndefined) {
      *aIsUndefined = true;
    }

    return NS_OK;
  }

  xpc_UnmarkGrayObject(aScopeObject);
  nsAutoMicroTask mt;

  // Safety first: get an object representing the script's principals, i.e.,
  // the entities who signed this script, or the fully-qualified-domain-name
  // or "codebase" from which it was loaded.
  nsCOMPtr<nsIPrincipal> principal = aPrincipal;
  nsresult rv;
  if (!aPrincipal) {
    nsIScriptGlobalObject *global = GetGlobalObject();
    if (!global)
      return NS_ERROR_FAILURE;
    nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal =
      do_QueryInterface(global, &rv);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    principal = objPrincipal->GetPrincipal();
    if (!principal)
      return NS_ERROR_FAILURE;
  }

  bool ok = false;

  rv = sSecurityManager->CanExecuteScripts(mContext, principal, &ok);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  // Push our JSContext on the current thread's context stack so JS called
  // from native code via XPConnect uses the right context.  Do this whether
  // or not the SecurityManager said "ok", in order to simplify control flow
  // below where we pop before returning.
  nsCOMPtr<nsIJSContextStack> stack =
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext))) {
    return NS_ERROR_FAILURE;
  }

  jsval val;

  rv = sSecurityManager->PushContextPrincipal(mContext, nsnull, principal);
  NS_ENSURE_SUCCESS(rv, rv);

  nsJSContext::TerminationFuncHolder holder(this);

  // SecurityManager said "ok", but don't compile if aVersion is unknown.
  // Since the caller is responsible for parsing the version strings, we just
  // check it isn't JSVERSION_UNKNOWN.
  if (ok && ((JSVersion)aVersion) != JSVERSION_UNKNOWN) {

    XPCAutoRequest ar(mContext);

    JSAutoEnterCompartment ac;
    if (!ac.enter(mContext, aScopeObject)) {
      stack->Pop(nsnull);
      return NS_ERROR_FAILURE;
    }

    ++mExecuteDepth;

    ok = ::JS_EvaluateUCScriptForPrincipalsVersion(mContext,
                                                   aScopeObject,
                                                   nsJSPrincipals::get(principal),
                                                   static_cast<const jschar*>(PromiseFlatString(aScript).get()),
                                                   aScript.Length(),
                                                   aURL,
                                                   aLineNo,
                                                   &val,
                                                   JSVersion(aVersion));

    --mExecuteDepth;

    if (!ok) {
      // Tell XPConnect about any pending exceptions. This is needed
      // to avoid dropping JS exceptions in case we got here through
      // nested calls through XPConnect.

      ReportPendingException();
    }
  }

  // If all went well, convert val to a string (XXXbe unless undefined?).
  if (ok) {
    if (aIsUndefined) {
      *aIsUndefined = JSVAL_IS_VOID(val);
    }

    *aRetValue = val;
    // XXX - nsScriptObjectHolder should be used once this method moves to
    // the new world order. However, use of 'jsval' appears to make this
    // tricky...
  }
  else {
    if (aIsUndefined) {
      *aIsUndefined = true;
    }
  }

  sSecurityManager->PopContextPrincipal(mContext);

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(true);

  return rv;

}

// Helper function to convert a jsval to an nsAString, and set
// exception flags if the conversion fails.
static nsresult
JSValueToAString(JSContext *cx, jsval val, nsAString *result,
                 bool *isUndefined)
{
  if (isUndefined) {
    *isUndefined = JSVAL_IS_VOID(val);
  }

  if (!result) {
    return NS_OK;
  }

  JSString* jsstring = ::JS_ValueToString(cx, val);
  if (!jsstring) {
    goto error;
  }

  size_t length;
  const jschar *chars;
  chars = ::JS_GetStringCharsAndLength(cx, jsstring, &length);
  if (!chars) {
    goto error;
  }

  result->Assign(chars, length);
  return NS_OK;

error:
  // We failed to convert val to a string. We're either OOM, or the
  // security manager denied access to .toString(), or somesuch, on
  // an object. Treat this case as if the result were undefined.

  result->Truncate();

  if (isUndefined) {
    *isUndefined = true;
  }

  if (!::JS_IsExceptionPending(cx)) {
    // JS_ValueToString()/JS_GetStringCharsAndLength returned null w/o an
    // exception pending. That means we're OOM.

    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsIScriptObjectPrincipal*
nsJSContext::GetObjectPrincipal()
{
  nsCOMPtr<nsIScriptObjectPrincipal> prin = do_QueryInterface(GetGlobalObject());
  return prin;
}

nsresult
nsJSContext::EvaluateString(const nsAString& aScript,
                            JSObject* aScopeObject,
                            nsIPrincipal *aPrincipal,
                            nsIPrincipal *aOriginPrincipal,
                            const char *aURL,
                            PRUint32 aLineNo,
                            JSVersion aVersion,
                            nsAString *aRetValue,
                            bool* aIsUndefined)
{
  NS_TIME_FUNCTION_MIN_FMT(1.0, "%s (line %d) (url: %s, line: %d)", MOZ_FUNCTION_NAME,
                           __LINE__, aURL, aLineNo);

  SAMPLE_LABEL("JS", "EvaluateString");
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    if (aIsUndefined) {
      *aIsUndefined = true;
    }

    if (aRetValue) {
      aRetValue->Truncate();
    }

    return NS_OK;
  }

  nsAutoMicroTask mt;

  if (!aScopeObject) {
    aScopeObject = JS_GetGlobalObject(mContext);
  }

  xpc_UnmarkGrayObject(aScopeObject);

  // Safety first: get an object representing the script's principals, i.e.,
  // the entities who signed this script, or the fully-qualified-domain-name
  // or "codebase" from which it was loaded.
  nsCOMPtr<nsIPrincipal> principal = aPrincipal;
  if (!aPrincipal) {
    nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal =
      do_QueryInterface(GetGlobalObject());
    if (!objPrincipal)
      return NS_ERROR_FAILURE;
    principal = objPrincipal->GetPrincipal();
    if (!principal)
      return NS_ERROR_FAILURE;
  }

  bool ok = false;

  nsresult rv = sSecurityManager->CanExecuteScripts(mContext, principal, &ok);
  if (NS_FAILED(rv)) {
    return NS_ERROR_FAILURE;
  }

  // Push our JSContext on the current thread's context stack so JS called
  // from native code via XPConnect uses the right context.  Do this whether
  // or not the SecurityManager said "ok", in order to simplify control flow
  // below where we pop before returning.
  nsCOMPtr<nsIJSContextStack> stack =
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext))) {
    return NS_ERROR_FAILURE;
  }

  // The result of evaluation, used only if there were no errors.  This need
  // not be a GC root currently, provided we run the GC only from the
  // operation callback or from ScriptEvaluated.
  jsval val = JSVAL_VOID;
  jsval* vp = aRetValue ? &val : NULL;

  rv = sSecurityManager->PushContextPrincipal(mContext, nsnull, principal);
  NS_ENSURE_SUCCESS(rv, rv);

  nsJSContext::TerminationFuncHolder holder(this);

  ++mExecuteDepth;

  // SecurityManager said "ok", but don't compile if aVersion is unknown.
  // Since the caller is responsible for parsing the version strings, we just
  // check it isn't JSVERSION_UNKNOWN.
  if (ok && JSVersion(aVersion) != JSVERSION_UNKNOWN) {
    XPCAutoRequest ar(mContext);
    JSAutoEnterCompartment ac;
    if (!ac.enter(mContext, aScopeObject)) {
      stack->Pop(nsnull);
      return NS_ERROR_FAILURE;
    }

    ok = JS_EvaluateUCScriptForPrincipalsVersionOrigin(
      mContext, aScopeObject,
      nsJSPrincipals::get(principal), nsJSPrincipals::get(aOriginPrincipal),
      static_cast<const jschar*>(PromiseFlatString(aScript).get()),
      aScript.Length(), aURL, aLineNo, vp, JSVersion(aVersion));

    if (!ok) {
      // Tell XPConnect about any pending exceptions. This is needed
      // to avoid dropping JS exceptions in case we got here through
      // nested calls through XPConnect.

      ReportPendingException();
    }
  }

  // If all went well, convert val to a string if one is wanted.
  if (ok) {
    XPCAutoRequest ar(mContext);
    JSAutoEnterCompartment ac;
    if (!ac.enter(mContext, aScopeObject)) {
      stack->Pop(nsnull);
    }
    rv = JSValueToAString(mContext, val, aRetValue, aIsUndefined);
  }
  else {
    if (aIsUndefined) {
      *aIsUndefined = true;
    }

    if (aRetValue) {
      aRetValue->Truncate();
    }
  }

  --mExecuteDepth;

  sSecurityManager->PopContextPrincipal(mContext);

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(true);

  return rv;
}

nsresult
nsJSContext::CompileScript(const PRUnichar* aText,
                           PRInt32 aTextLength,
                           nsIPrincipal *aPrincipal,
                           const char *aURL,
                           PRUint32 aLineNo,
                           PRUint32 aVersion,
                           nsScriptObjectHolder<JSScript>& aScriptObject)
{
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


  JSScript* script =
    ::JS_CompileUCScriptForPrincipalsVersion(mContext,
                                             scopeObject,
                                             nsJSPrincipals::get(aPrincipal),
                                             static_cast<const jschar*>(aText),
                                             aTextLength,
                                             aURL,
                                             aLineNo,
                                             JSVersion(aVersion));
  if (!script) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return aScriptObject.set(script);
}

nsresult
nsJSContext::ExecuteScript(JSScript* aScriptObject,
                           JSObject* aScopeObject,
                           nsAString* aRetValue,
                           bool* aIsUndefined)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    if (aIsUndefined) {
      *aIsUndefined = true;
    }

    if (aRetValue) {
      aRetValue->Truncate();
    }

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

  nsCOMPtr<nsIPrincipal> principal;
  rv = sSecurityManager->GetObjectPrincipal(mContext,
                                            JS_GetGlobalFromScript(aScriptObject),
                                            getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = sSecurityManager->PushContextPrincipal(mContext, nsnull, principal);
  NS_ENSURE_SUCCESS(rv, rv);

  nsJSContext::TerminationFuncHolder holder(this);
  XPCAutoRequest ar(mContext);
  ++mExecuteDepth;

  // The result of evaluation, used only if there were no errors. This need
  // not be a GC root currently, provided we run the GC only from the
  // operation callback or from ScriptEvaluated.
  jsval val;
  bool ok = JS_ExecuteScript(mContext, aScopeObject, aScriptObject, &val);
  if (ok) {
    // If all went well, convert val to a string (XXXbe unless undefined?).
    rv = JSValueToAString(mContext, val, aRetValue, aIsUndefined);
  } else {
    ReportPendingException();

    if (aIsUndefined) {
      *aIsUndefined = true;
    }

    if (aRetValue) {
      aRetValue->Truncate();
    }
  }

  --mExecuteDepth;

  sSecurityManager->PopContextPrincipal(mContext);

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

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
    *aRet = nsnull;
    return NS_OK;
  }

  // Get the jsobject associated with this target
  // We don't wrap here because we trust the JS engine to wrap the target
  // later.
  jsval v;
  nsresult rv = nsContentUtils::WrapNative(mContext, aScope, aTarget, &v);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef NS_DEBUG
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
nsJSContext::CompileEventHandler(nsIAtom *aName,
                                 PRUint32 aArgCount,
                                 const char** aArgNames,
                                 const nsAString& aBody,
                                 const char *aURL, PRUint32 aLineNo,
                                 PRUint32 aVersion,
                                 nsScriptObjectHolder<JSObject>& aHandler)
{
  NS_TIME_FUNCTION_MIN_FMT(1.0, "%s (line %d) (url: %s, line: %d)", MOZ_FUNCTION_NAME,
                           __LINE__, aURL, aLineNo);

  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  NS_PRECONDITION(AtomIsEventHandlerName(aName), "Bad event name");
  NS_PRECONDITION(!::JS_IsExceptionPending(mContext),
                  "Why are we being called with a pending exception?");

  if (!sSecurityManager) {
    NS_ERROR("Huh, we need a script security manager to compile "
             "an event handler!");

    return NS_ERROR_UNEXPECTED;
  }

  // Don't compile if aVersion is unknown.  Since the caller is responsible for
  // parsing the version strings, we just check it isn't JSVERSION_UNKNOWN.
  if ((JSVersion)aVersion == JSVERSION_UNKNOWN) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

#ifdef DEBUG
  JSContext* top = nsContentUtils::GetCurrentJSContext();
  NS_ASSERTION(mContext == top, "Context not properly pushed!");
#endif

  // Event handlers are always shared, and must be bound before use.
  // Therefore we never bother compiling with principals.
  // (that probably means we should avoid JS_CompileUCFunctionForPrincipals!)
  XPCAutoRequest ar(mContext);

  JSFunction* fun =
      ::JS_CompileUCFunctionForPrincipalsVersion(mContext,
                                                 nsnull, nsnull,
                                                 nsAtomCString(aName).get(), aArgCount, aArgNames,
                                                 (jschar*)PromiseFlatString(aBody).get(),
                                                 aBody.Length(),
                                                 aURL, aLineNo, JSVersion(aVersion));

  if (!fun) {
    ReportPendingException();
    return NS_ERROR_ILLEGAL_VALUE;
  }

  JSObject *handler = ::JS_GetFunctionObject(fun);
  return aHandler.set(handler);
}

// XXX - note that CompileFunction doesn't yet play the nsScriptObjectHolder
// game - caller must still ensure JS GC root.
nsresult
nsJSContext::CompileFunction(JSObject* aTarget,
                             const nsACString& aName,
                             PRUint32 aArgCount,
                             const char** aArgArray,
                             const nsAString& aBody,
                             const char* aURL,
                             PRUint32 aLineNo,
                             PRUint32 aVersion,
                             bool aShared,
                             JSObject** aFunctionObject)
{
  NS_TIME_FUNCTION_FMT(1.0, "%s (line %d) (function: %s, url: %s, line: %d)", MOZ_FUNCTION_NAME,
                       __LINE__, aName.BeginReading(), aURL, aLineNo);

  NS_ABORT_IF_FALSE(aFunctionObject,
    "Shouldn't call CompileFunction with null return value.");

  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  // Don't compile if aVersion is unknown.  Since the caller is responsible for
  // parsing the version strings, we just check it isn't JSVERSION_UNKNOWN.
  if ((JSVersion)aVersion == JSVERSION_UNKNOWN) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  xpc_UnmarkGrayObject(aTarget);

  nsIScriptGlobalObject *global = GetGlobalObject();
  nsCOMPtr<nsIPrincipal> principal;
  if (global) {
    // XXXbe why the two-step QI? speed up via a new GetGlobalObjectData func?
    nsCOMPtr<nsIScriptObjectPrincipal> globalData = do_QueryInterface(global);
    if (globalData) {
      principal = globalData->GetPrincipal();
      if (!principal)
        return NS_ERROR_FAILURE;
    }
  }

  JSObject *target = aTarget;

  XPCAutoRequest ar(mContext);

  JSFunction* fun =
      ::JS_CompileUCFunctionForPrincipalsVersion(mContext,
                                                 aShared ? nsnull : target,
                                                 nsJSPrincipals::get(principal),
                                                 PromiseFlatCString(aName).get(),
                                                 aArgCount, aArgArray,
                                                 static_cast<const jschar*>(PromiseFlatString(aBody).get()),
                                                 aBody.Length(),
                                                 aURL, aLineNo,
                                                 JSVersion(aVersion));

  if (!fun)
    return NS_ERROR_FAILURE;

  *aFunctionObject = JS_GetFunctionObject(fun);
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

#ifdef NS_FUNCTION_TIMER
  {
    JSObject *obj = aHandler;
    if (js::IsFunctionProxy(obj))
      obj = js::UnwrapObject(obj);
    JSString *id = JS_GetFunctionId(static_cast<JSFunction *>(JS_GetPrivate(obj)));
    JSAutoByteString bytes;
    const char *name = !id ? "anonymous" : bytes.encode(mContext, id) ? bytes.ptr() : "<error>";
    NS_TIME_FUNCTION_FMT(1.0, "%s (line %d) (function: %s)", MOZ_FUNCTION_NAME, __LINE__, name);
  }
#endif
  SAMPLE_LABEL("JS", "CallEventHandler");

  nsAutoMicroTask mt;
  xpc_UnmarkGrayObject(aScope);
  xpc_UnmarkGrayObject(aHandler);

  XPCAutoRequest ar(mContext);
  JSObject* target = nsnull;
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
    PRUint32 argc = 0;
    jsval *argv = nsnull;

    JSObject *funobj = aHandler;
    nsCOMPtr<nsIPrincipal> principal;
    rv = sSecurityManager->GetObjectPrincipal(mContext, funobj,
                                              getter_AddRefs(principal));
    NS_ENSURE_SUCCESS(rv, rv);

    JSStackFrame *currentfp = nsnull;
    rv = sSecurityManager->PushContextPrincipal(mContext,
                                                JS_FrameIterator(mContext, &currentfp),
                                                principal);
    NS_ENSURE_SUCCESS(rv, rv);

    jsval funval = OBJECT_TO_JSVAL(funobj);
    JSAutoEnterCompartment ac;
    js::ForceFrame ff(mContext, funobj);
    if (!ac.enter(mContext, funobj) || !ff.enter() ||
        !JS_WrapObject(mContext, &target)) {
      ReportPendingException();
      sSecurityManager->PopContextPrincipal(mContext);
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
      *arv = nsnull;
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

    sSecurityManager->PopContextPrincipal(mContext);
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
  JSObject *target = nsnull;
  nsresult rv = JSObjectFromInterface(aTarget, aScope, &target);
  NS_ENSURE_SUCCESS(rv, rv);

#ifdef DEBUG
  {
    JSAutoEnterCompartment ac;
    if (!ac.enter(mContext, aHandler)) {
      return NS_ERROR_FAILURE;
    }

    NS_ASSERTION(JS_TypeOfValue(mContext,
                                OBJECT_TO_JSVAL(aHandler)) == JSTYPE_FUNCTION,
                 "Event handler object not a function");
  }
#endif

  JSAutoEnterCompartment ac;
  if (!ac.enter(mContext, target)) {
    return NS_ERROR_FAILURE;
  }

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

  return nsContentUtils::XPConnect()->WriteScript(aStream, mContext, aScriptObject);
    xpc_UnmarkGrayScript(aScriptObject);

}

nsresult
nsJSContext::Deserialize(nsIObjectInputStream* aStream,
                         nsScriptObjectHolder<JSScript>& aResult)
{
  NS_TIME_FUNCTION_MIN(1.0);
  
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
    return nsnull;
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
    return nsnull;
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

nsresult
nsJSContext::CreateNativeGlobalForInner(
                                nsIScriptGlobalObject *aNewInner,
                                bool aIsChrome,
                                nsIPrincipal *aPrincipal,
                                JSObject** aNativeGlobal, nsISupports **aHolder)
{
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  PRUint32 flags = aIsChrome? nsIXPConnect::FLAG_SYSTEM_GLOBAL_OBJECT : 0;

  nsCOMPtr<nsIPrincipal> systemPrincipal;
  if (aIsChrome) {
    nsIScriptSecurityManager *ssm = nsContentUtils::GetSecurityManager();
    ssm->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
  }

  nsRefPtr<nsIXPConnectJSObjectHolder> jsholder;
  nsresult rv = xpc->
          InitClassesWithNewWrappedGlobal(mContext, aNewInner,
                                          aIsChrome ? systemPrincipal.get() : aPrincipal,
                                          flags, getter_AddRefs(jsholder));
  if (NS_FAILED(rv)) {
    return rv;
  }
  jsholder->GetJSObject(aNativeGlobal);
  jsholder.forget(aHolder);
  return NS_OK;
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
nsJSContext::InitOuterWindow()
{
  JSObject *global = JS_ObjectToInnerObject(mContext, JS_GetGlobalObject(mContext));

  nsresult rv = InitClasses(global); // this will complete global object initialization
  NS_ENSURE_SUCCESS(rv, rv);

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
  PRUint32  argc;
  jsval    *argv = nsnull;

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
    for (PRUint32 i = 0; i < argc; ++i) {
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
                                     PRUint32 *aArgc,
                                     jsval **aArgv,
                                     Maybe<nsRootedJSValueArray> &aTempStorage)
{
  nsresult rv = NS_OK;

  // If the array implements nsIJSArgArray, just grab the values directly.
  nsCOMPtr<nsIJSArgArray> fastArray = do_QueryInterface(aArgs);
  if (fastArray != nsnull)
    return fastArray->GetArgs(aArgc, reinterpret_cast<void **>(aArgv));

  // Take the slower path converting each item.
  // Handle only nsIArray and nsIVariant.  nsIArray is only needed for
  // SetProperty('arguments', ...);

  *aArgv = nsnull;
  *aArgc = 0;

  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  NS_ENSURE_TRUE(xpc, NS_ERROR_UNEXPECTED);

  if (!aArgs)
    return NS_OK;
  PRUint32 argCount;
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
    for (PRUint32 argCtr = 0; argCtr < argCount && NS_SUCCEEDED(rv); argCtr++) {
      nsCOMPtr<nsISupports> arg;
      jsval *thisval = argv + argCtr;
      argsArray->QueryElementAt(argCtr, NS_GET_IID(nsISupports),
                                getter_AddRefs(arg));
      if (!arg) {
        *thisval = JSVAL_NULL;
        continue;
      }
      nsCOMPtr<nsIVariant> variant(do_QueryInterface(arg));
      if (variant != nsnull) {
        rv = xpc->VariantToJS(mContext, aScope, variant, thisval);
      } else {
        // And finally, support the nsISupportsPrimitives supplied
        // by the AppShell.  It generally will pass only strings, but
        // as we have code for handling all, we may as well use it.
        rv = AddSupportsPrimitiveTojsvals(arg, thisval);
        if (rv == NS_ERROR_NO_INTERFACE) {
          // something else - probably an event object or similar -
          // just wrap it.
#ifdef NS_DEBUG
          // but first, check its not another nsISupportsPrimitive, as
          // these are now deprecated for use with script contexts.
          nsCOMPtr<nsISupportsPrimitive> prim(do_QueryInterface(arg));
          NS_ASSERTION(prim == nsnull,
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
  PRUint16 type;
  argPrimitive->GetType(&type);

  switch(type) {
    case nsISupportsPrimitive::TYPE_CSTRING : {
      nsCOMPtr<nsISupportsCString> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      nsCAutoString data;

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

      PRUint8 data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT16 : {
      nsCOMPtr<nsISupportsPRUint16> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      PRUint16 data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRUINT32 : {
      nsCOMPtr<nsISupportsPRUint32> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      PRUint32 data;

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

      PRInt16 data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_PRINT32 : {
      nsCOMPtr<nsISupportsPRInt32> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      PRInt32 data;

      p->GetData(&data);

      *aArgv = INT_TO_JSVAL(data);

      break;
    }
    case nsISupportsPrimitive::TYPE_FLOAT : {
      nsCOMPtr<nsISupportsFloat> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      float data;

      p->GetData(&data);

      JSBool ok = ::JS_NewNumberValue(cx, data, aArgv);
      NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);

      break;
    }
    case nsISupportsPrimitive::TYPE_DOUBLE : {
      nsCOMPtr<nsISupportsDouble> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      double data;

      p->GetData(&data);

      JSBool ok = ::JS_NewNumberValue(cx, data, aArgv);
      NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);

      break;
    }
    case nsISupportsPrimitive::TYPE_INTERFACE_POINTER : {
      nsCOMPtr<nsISupportsInterfacePointer> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      nsCOMPtr<nsISupports> data;
      nsIID *iid = nsnull;

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
    bool hasCap = false;
    nsresult rv = nsContentUtils::GetSecurityManager()->
                    IsCapabilityEnabled("UniversalXPConnect", &hasCap);
    if (NS_SUCCEEDED(rv) && hasCap)
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
    {"TraceMallocDisable",         TraceMallocDisable,         0, 0},
    {"TraceMallocEnable",          TraceMallocEnable,          0, 0},
    {"TraceMallocOpenLogFile",     TraceMallocOpenLogFile,     1, 0},
    {"TraceMallocChangeLogFD",     TraceMallocChangeLogFD,     1, 0},
    {"TraceMallocCloseLogFD",      TraceMallocCloseLogFD,      1, 0},
    {"TraceMallocLogTimestamp",    TraceMallocLogTimestamp,    1, 0},
    {"TraceMallocDumpAllocations", TraceMallocDumpAllocations, 1, 0},
    {nsnull,                       nsnull,                     0, 0}
};

#endif /* NS_TRACE_MALLOC */

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
    sigaction(SIGALRM, nsnull, &action);
    //printf("SIGALRM: %p, flags = %x\n",action.sa_sigaction,action.sa_flags);
    if (IsJProfAction(&action)) {
        //printf("Beginning real-time jprof profiling.\n");
        raise(SIGALRM);
        return;
    }

    sigaction(SIGPROF, nsnull, &action);
    //printf("SIGPROF: %p, flags = %x\n",action.sa_sigaction,action.sa_flags);
    if (IsJProfAction(&action)) {
        //printf("Beginning process-time jprof profiling.\n");
        raise(SIGPROF);
        return;
    }

    sigaction(SIGPOLL, nsnull, &action);
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
    {"JProfStartProfiling",        JProfStartProfilingJS,      0, 0},
    {"JProfStopProfiling",         JProfStopProfilingJS,       0, 0},
    {"JProfClearCircular",         JProfClearCircularJS,       0, 0},
    {"JProfSaveCircular",          JProfSaveCircularJS,        0, 0},
    {nsnull,                       nsnull,                     0, 0}
};

#endif /* defined(MOZ_JPROF) */

#ifdef MOZ_DMD

// See https://wiki.mozilla.org/Performance/MemShrink/DMD for instructions on
// how to use DMD.

static JSBool
DMDCheckJS(JSContext *cx, unsigned argc, jsval *vp)
{
  mozilla::DMDCheckAndDump();
  return JS_TRUE;
}

static JSFunctionSpec DMDFunctions[] = {
    {"DMD",                        DMDCheckJS,                 0, 0},
    {nsnull,                       nsnull,                     0, 0}
};

#endif /* defined(MOZ_DMD) */

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

#ifdef MOZ_JPROF
  // Attempt to initialize JProf functions
  ::JS_DefineFunctions(mContext, aGlobalObj, JProfFunctions);
#endif

#ifdef MOZ_DMD
  // Attempt to initialize DMD functions
  ::JS_DefineFunctions(mContext, aGlobalObj, DMDFunctions);
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
    mTerminations = nsnull;

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

void
nsJSContext::SetGCOnDestruction(bool aGCOnDestruction)
{
  mGCOnDestruction = aGCOnDestruction;
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
                                 nsGCNormal, true);
}

//static
void
nsJSContext::GarbageCollectNow(js::gcreason::Reason aReason, PRUint32 aGckind,
                               bool aGlobal)
{
  NS_TIME_FUNCTION_MIN(1.0);
  SAMPLE_LABEL("GC", "GarbageCollectNow");

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

  if (!nsContentUtils::XPConnect()) {
    return;
  }
  
  // Use compartment GC when we're not asked to do a shrinking GC nor
  // global GC and compartment GC has been called less than
  // NS_MAX_COMPARTMENT_GC_COUNT times after the previous global GC.
  if (!sDisableExplicitCompartmentGC &&
      aGckind != nsGCShrinking && !aGlobal &&
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
      js::IncrementalGC(nsJSRuntime::sRuntime, aReason);
    }
    return;
  }

  for (nsJSContext* cx = sContextList; cx; cx = cx->mNext) {
    cx->mActive = false;
  }
  nsContentUtils::XPConnect()->GarbageCollect(aReason, aGckind);
}

//static
void
nsJSContext::ShrinkGCBuffersNow()
{
  NS_TIME_FUNCTION_MIN(1.0);
  SAMPLE_LABEL("GC", "ShrinkGCBuffersNow");

  KillShrinkGCBuffersTimer();

  JS_ShrinkGCBuffers(nsJSRuntime::sRuntime);
}

//Static
void
nsJSContext::CycleCollectNow(nsICycleCollectorListener *aListener,
                             PRInt32 aExtraForgetSkippableCalls)
{
  if (!NS_IsMainThread()) {
    return;
  }

  if (sCCLockedOut) {
    // We're in the middle of an incremental GC; finish it first
    nsJSContext::GarbageCollectNow(js::gcreason::CC_FORCED, nsGCNormal, true);
  }

  SAMPLE_LABEL("GC", "CycleCollectNow");
  NS_TIME_FUNCTION_MIN(1.0);

  KillCCTimer();

  PRTime start = PR_Now();

  PRUint32 suspected = nsCycleCollector_suspectedCount();

  // nsCycleCollector_forgetSkippable may mark some gray js to black.
  if (sCleanupsSinceLastGC < 2 && aExtraForgetSkippableCalls >= 0) {
    for (;sCleanupsSinceLastGC < 2; ++sCleanupsSinceLastGC) {
      nsCycleCollector_forgetSkippable();
    }
  }

  for (PRInt32 i = 0; i < aExtraForgetSkippableCalls; ++i) {
    nsCycleCollector_forgetSkippable();
    ++sCleanupsSinceLastGC;
  }

  nsCycleCollectorResults ccResults;
  nsCycleCollector_collect(&ccResults, aListener);
  sCCollectedWaitingForGC += ccResults.mFreedRefCounted + ccResults.mFreedGCed;

  // If we collected a substantial amount of cycles, poke the GC since more objects
  // might be unreachable now.
  if (sCCollectedWaitingForGC > 250) {
    PokeGC(js::gcreason::CC_WAITING);
  }

  PRTime now = PR_Now();

  if (sLastCCEndTime) {
    PRUint32 timeBetween = (PRUint32)(start - sLastCCEndTime) / PR_USEC_PER_SEC;
    Telemetry::Accumulate(Telemetry::CYCLE_COLLECTOR_TIME_BETWEEN, timeBetween);
  }
  sLastCCEndTime = now;

  Telemetry::Accumulate(Telemetry::FORGET_SKIPPABLE_MAX,
                        sMaxForgetSkippableTime / PR_USEC_PER_MSEC);

  if (sPostGCEventsToConsole) {
    PRTime delta = 0;
    if (sFirstCollectionTime) {
      delta = now - sFirstCollectionTime;
    } else {
      sFirstCollectionTime = now;
    }

    nsString gcmsg;
    if (ccResults.mForcedGC) {
      gcmsg.AssignLiteral(", forced a GC");
    }

    NS_NAMED_MULTILINE_LITERAL_STRING(kFmt,
      NS_LL("CC(T+%.1f) duration: %llums, suspected: %lu, visited: %lu RCed and %lu GCed, collected: %lu RCed and %lu GCed (%lu waiting for GC)%s\n")
      NS_LL("ForgetSkippable %lu times before CC, min: %lu ms, max: %lu ms, avg: %lu ms, total: %lu ms, removed: %lu"));
    nsString msg;
    PRUint32 cleanups = sForgetSkippableBeforeCC ? sForgetSkippableBeforeCC : 1;
    sMinForgetSkippableTime = (sMinForgetSkippableTime == PR_UINT32_MAX)
      ? 0 : sMinForgetSkippableTime;
    msg.Adopt(nsTextFormatter::smprintf(kFmt.get(), double(delta) / PR_USEC_PER_SEC,
                                        (now - start) / PR_USEC_PER_MSEC, suspected,
                                        ccResults.mVisitedRefCounted, ccResults.mVisitedGCed,
                                        ccResults.mFreedRefCounted, ccResults.mFreedGCed,
                                        sCCollectedWaitingForGC, gcmsg.get(),
                                        sForgetSkippableBeforeCC,
                                        sMinForgetSkippableTime / PR_USEC_PER_MSEC,
                                        sMaxForgetSkippableTime / PR_USEC_PER_MSEC,
                                        (sTotalForgetSkippableTime / cleanups) /
                                          PR_USEC_PER_MSEC,
                                        sTotalForgetSkippableTime / PR_USEC_PER_MSEC,
                                        sRemovedPurples));
    nsCOMPtr<nsIConsoleService> cs =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (cs) {
      cs->LogStringMessage(msg.get());
    }

    NS_NAMED_MULTILINE_LITERAL_STRING(kJSONFmt,
       NS_LL("{ \"timestamp\": %llu, ")
         NS_LL("\"duration\": %llu, ")
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
    json.Adopt(nsTextFormatter::smprintf(kJSONFmt.get(),
                                         now, (now - start) / PR_USEC_PER_MSEC, suspected,
                                         ccResults.mVisitedRefCounted, ccResults.mVisitedGCed,
                                         ccResults.mFreedRefCounted, ccResults.mFreedGCed,
                                         sCCollectedWaitingForGC,
                                         ccResults.mForcedGC,
                                         sForgetSkippableBeforeCC,
                                         sMinForgetSkippableTime / PR_USEC_PER_MSEC,
                                         sMaxForgetSkippableTime / PR_USEC_PER_MSEC,
                                         (sTotalForgetSkippableTime / cleanups) /
                                           PR_USEC_PER_MSEC,
                                         sTotalForgetSkippableTime / PR_USEC_PER_MSEC,
                                         sRemovedPurples));
    nsCOMPtr<nsIObserverService> observerService = mozilla::services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(nsnull, "cycle-collection-statistics", json.get());
    }
  }
  sMinForgetSkippableTime = PR_UINT32_MAX;
  sMaxForgetSkippableTime = 0;
  sTotalForgetSkippableTime = 0;
  sRemovedPurples = 0;
  sForgetSkippableBeforeCC = 0;
  sNeedsFullCC = false;
}

// static
void
GCTimerFired(nsITimer *aTimer, void *aClosure)
{
  NS_RELEASE(sGCTimer);

  uintptr_t reason = reinterpret_cast<uintptr_t>(aClosure);
  nsJSContext::GarbageCollectNow(static_cast<js::gcreason::Reason>(reason),
                                 nsGCNormal, false);
}

void
ShrinkGCBuffersTimerFired(nsITimer *aTimer, void *aClosure)
{
  NS_RELEASE(sShrinkGCBuffersTimer);

  nsJSContext::ShrinkGCBuffersNow();
}

static bool
ShouldTriggerCC(PRUint32 aSuspected)
{
  return sNeedsFullCC ||
         aSuspected > NS_CC_PURPLE_LIMIT ||
         sLastCCEndTime + NS_CC_FORCED < PR_Now();
}

static void
TimerFireForgetSkippable(PRUint32 aSuspected, bool aRemoveChildless)
{
  PRTime startTime = PR_Now();
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

static void
CCTimerFired(nsITimer *aTimer, void *aClosure)
{
  if (sDidShutdown) {
    return;
  }

  if (sCCLockedOut) {
    PRTime now = PR_Now();
    if (sCCLockedOutTime == 0) {
      sCCLockedOutTime = now;
      return;
    }
    if (now - sCCLockedOutTime < NS_MAX_CC_LOCKEDOUT_TIME) {
      return;
    }

    // Finish the current incremental GC
    nsJSContext::GarbageCollectNow(js::gcreason::CC_FORCED, nsGCNormal, true);
  }

  ++sCCTimerFireCount;

  // During early timer fires, we only run forgetSkippable. During the first
  // late timer fire, we decide if we are going to have a second and final
  // late timer fire, where we may run the CC.
  const PRUint32 numEarlyTimerFires = NS_CC_DELAY / NS_CC_SKIPPABLE_DELAY - 2;
  bool isLateTimerFire = sCCTimerFireCount > numEarlyTimerFires;
  PRUint32 suspected = nsCycleCollector_suspectedCount();
  if (isLateTimerFire && ShouldTriggerCC(suspected)) {
    if (sCCTimerFireCount == numEarlyTimerFires + 1) {
      TimerFireForgetSkippable(suspected, true);
      if (ShouldTriggerCC(nsCycleCollector_suspectedCount())) {
        // Our efforts to avoid a CC have failed, so we return to let the
        // timer fire once more to trigger a CC.
        return;
      }
    } else {
      // We are in the final timer fire and still meet the conditions for
      // triggering a CC.
      nsJSContext::CycleCollectNow();
    }
  } else if ((sPreviousSuspectedCount + 100) <= suspected) {
    // Only do a forget skippable if there are more than a few new objects.
    TimerFireForgetSkippable(suspected, false);
  }

  if (isLateTimerFire) {
    // We have either just run the CC or decided we don't want to run the CC
    // next time, so kill the timer.
    sPreviousSuspectedCount = 0;
    nsJSContext::KillCCTimer();
  }
}

// static
PRUint32
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
  if (sGCTimer) {
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
  if (sShrinkGCBuffersTimer) {
    return;
  }

  CallCreateInstance("@mozilla.org/timer;1", &sShrinkGCBuffersTimer);

  if (!sShrinkGCBuffersTimer) {
    // Failed to create timer (probably because we're in XPCOM shutdown)
    return;
  }

  sShrinkGCBuffersTimer->InitWithFuncCallback(ShrinkGCBuffersTimerFired, nsnull,
                                              NS_SHRINK_GC_BUFFERS_DELAY,
                                              nsITimer::TYPE_ONE_SHOT);
}

// static
void
nsJSContext::MaybePokeCC()
{
  if (sCCTimer || sDidShutdown) {
    return;
  }

  if (sNeedsFullCC ||
      nsCycleCollector_suspectedCount() > 100 ||
      sLastCCEndTime + NS_CC_FORCED < PR_Now()) {
    sCCTimerFireCount = 0;
    CallCreateInstance("@mozilla.org/timer;1", &sCCTimer);
    if (!sCCTimer) {
      return;
    }
    sCCTimer->InitWithFuncCallback(CCTimerFired, nsnull,
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
  observerService->NotifyObservers(nsnull, "garbage-collection-statistics", toSend);

  return NS_OK;
}

static void
DOMGCSliceCallback(JSRuntime *aRt, js::GCProgress aProgress, const js::GCDescription &aDesc)
{
  NS_ASSERTION(NS_IsMainThread(), "GCs must run on the main thread");

  if (aProgress == js::GC_CYCLE_END && sPostGCEventsToConsole) {
    PRTime now = PR_Now();
    PRTime delta = 0;
    if (sFirstCollectionTime) {
      delta = now - sFirstCollectionTime;
    } else {
      sFirstCollectionTime = now;
    }

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

    nsString json;
    json.Adopt(aDesc.formatJSON(aRt, now));
    nsRefPtr<NotifyGCEndRunnable> notify = new NotifyGCEndRunnable(json);
    NS_DispatchToMainThread(notify);
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
    nsJSContext::KillGCTimer();
    nsJSContext::PokeGC(js::gcreason::INTER_SLICE_GC, NS_INTERSLICE_GC_DELAY);
  }

  if (aProgress == js::GC_CYCLE_END) {
    // May need to kill the inter-slice GC timer
    nsJSContext::KillGCTimer();

    sCCollectedWaitingForGC = 0;
    sCleanupsSinceLastGC = 0;
    sNeedsFullCC = true;
    nsJSContext::MaybePokeCC();

    if (aDesc.isCompartment) {
      ++sCompartmentGCCount;
      if (!sFullGCTimer) {
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
  // set aside the frame chain, since it has nothing to do with the
  // exception we're reporting.
  if (mIsInitialized && ::JS_IsExceptionPending(mContext)) {
    bool saved = ::JS_SaveFrameChain(mContext);
    ::JS_ReportPendingException(mContext);
    if (saved)
        ::JS_RestoreFrameChain(mContext);
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
nsJSRuntime::CreateContext()
{
  nsCOMPtr<nsIScriptContext> scriptContext = new nsJSContext(sRuntime);
  return scriptContext.forget();
}

nsresult
nsJSRuntime::ParseVersion(const nsString &aVersionStr, PRUint32 *flags)
{
    NS_PRECONDITION(flags, "Null flags param?");
    JSVersion jsVersion = JSVERSION_UNKNOWN;
    if (aVersionStr.Length() != 3 || aVersionStr[0] != '1' || aVersionStr[1] != '.')
        jsVersion = JSVERSION_UNKNOWN;
    else switch (aVersionStr[2]) {
        case '0': jsVersion = JSVERSION_1_0; break;
        case '1': jsVersion = JSVERSION_1_1; break;
        case '2': jsVersion = JSVERSION_1_2; break;
        case '3': jsVersion = JSVERSION_1_3; break;
        case '4': jsVersion = JSVERSION_1_4; break;
        case '5': jsVersion = JSVERSION_1_5; break;
        case '6': jsVersion = JSVERSION_1_6; break;
        case '7': jsVersion = JSVERSION_1_7; break;
        case '8': jsVersion = JSVERSION_1_8; break;
        default:  jsVersion = JSVERSION_UNKNOWN;
    }
    *flags = (PRUint32)jsVersion;
    return NS_OK;
}

//static
void
nsJSRuntime::Startup()
{
  // initialize all our statics, so that we can restart XPCOM
  sGCTimer = sFullGCTimer = sCCTimer = nsnull;
  sCCLockedOut = false;
  sCCLockedOutTime = 0;
  sLastCCEndTime = 0;
  sPendingLoadCount = 0;
  sLoadingInProgress = false;
  sCCollectedWaitingForGC = 0;
  sPostGCEventsToConsole = false;
  sDisableExplicitCompartmentGC = false;
  sNeedsFullCC = false;
  gNameSpaceManager = nsnull;
  sRuntimeService = nsnull;
  sRuntime = nsnull;
  sIsInitialized = false;
  sDidShutdown = false;
  sContextCount = 0;
  sSecurityManager = nsnull;
}

static int
MaxScriptRunTimePrefChangedCallback(const char *aPrefName, void *aClosure)
{
  // Default limit on script run time to 10 seconds. 0 means let
  // scripts run forever.
  bool isChromePref =
    strcmp(aPrefName, "dom.max_chrome_script_run_time") == 0;
  PRInt32 time = Preferences::GetInt(aPrefName, isChromePref ? 20 : 10);

  PRTime t;
  if (time <= 0) {
    // Let scripts run for a really, really long time.
    t = LL_INIT(0x40000000, 0);
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
  PRInt32 highwatermark = Preferences::GetInt(aPrefName, 128);

  JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_MAX_MALLOC_BYTES,
                    highwatermark * 1024L * 1024L);
  return 0;
}

static int
SetMemoryMaxPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  PRInt32 pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  PRUint32 max = (pref <= 0 || pref >= 0x1000) ? -1 : (PRUint32)pref * 1024 * 1024;
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
  PRInt32 pref = Preferences::GetInt(aPrefName, -1);
  // handle overflow and negative pref values
  if (pref > 0 && pref < 100000)
    JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_SLICE_TIME_BUDGET, pref);
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
      return nsnull;
    }
    MOZ_ASSERT(dataArray.isObject());

    // Construct the ImageData.
    nsCOMPtr<nsIDOMImageData> imageData = new ImageData(width, height,
                                                        dataArray.toObject());
    // Wrap it in a jsval.
    JSObject* global = JS_GetGlobalForScopeChain(cx);
    if (!global) {
      return nsnull;
    }
    nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
    JS::Value val;
    nsresult rv =
      nsContentUtils::WrapNative(cx, global, imageData, &val,
                                 getter_AddRefs(wrapper));
    if (NS_FAILED(rv)) {
      return nsnull;
    }
    return val.toObjectOrNull();
  }

  // Don't know what this is. Bail.
  nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_DOM_DATA_CLONE_ERR);
  return nsnull;
}

JSBool
NS_DOMWriteStructuredClone(JSContext* cx,
                           JSStructuredCloneWriter* writer,
                           JSObject* obj,
                           void *closure)
{
  nsCOMPtr<nsIXPConnectWrappedNative> wrappedNative;
  nsContentUtils::XPConnect()->
    GetWrappedNativeOfJSObject(cx, obj, getter_AddRefs(wrappedNative));
  nsISupports *native = wrappedNative ? wrappedNative->Native() : nsnull;

  nsCOMPtr<nsIDOMImageData> imageData = do_QueryInterface(native);
  if (imageData) {
    // Prepare the ImageData internals.
    PRUint32 width, height;
    JS::Value dataArray;
    if (NS_FAILED(imageData->GetWidth(&width)) ||
        NS_FAILED(imageData->GetHeight(&height)) ||
        NS_FAILED(imageData->GetData(cx, &dataArray)))
    {
      return false;
    }

    // Write the internals to the stream.
    return JS_WriteUint32Pair(writer, SCTAG_DOM_IMAGEDATA, 0) &&
           JS_WriteUint32Pair(writer, width, height) &&
           JS_WriteTypedArray(writer, dataArray);
  }

  // Don't know what this is. Bail.
  nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_DOM_DATA_CLONE_ERR);
  return JS_FALSE;
}

void
NS_DOMStructuredCloneError(JSContext* cx,
                           uint32_t errorid)
{
  // We don't currently support any extensions to structured cloning.
  nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_DOM_DATA_CLONE_ERR);
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

  // Set up the structured clone callbacks.
  static JSStructuredCloneCallbacks cloneCallbacks = {
    NS_DOMReadStructuredClone,
    NS_DOMWriteStructuredClone,
    NS_DOMStructuredCloneError
  };
  JS_SetStructuredCloneCallbacks(sRuntime, &cloneCallbacks);

  // Set these global xpconnect options...
  Preferences::RegisterCallback(MaxScriptRunTimePrefChangedCallback,
                                "dom.max_script_run_time");
  MaxScriptRunTimePrefChangedCallback("dom.max_script_run_time", nsnull);

  Preferences::RegisterCallback(MaxScriptRunTimePrefChangedCallback,
                                "dom.max_chrome_script_run_time");
  MaxScriptRunTimePrefChangedCallback("dom.max_chrome_script_run_time",
                                      nsnull);

  Preferences::RegisterCallback(ReportAllJSExceptionsPrefChangedCallback,
                                "dom.report_all_js_exceptions");
  ReportAllJSExceptionsPrefChangedCallback("dom.report_all_js_exceptions",
                                           nsnull);

  Preferences::RegisterCallback(SetMemoryHighWaterMarkPrefChangedCallback,
                                "javascript.options.mem.high_water_mark");
  SetMemoryHighWaterMarkPrefChangedCallback("javascript.options.mem.high_water_mark",
                                            nsnull);

  Preferences::RegisterCallback(SetMemoryMaxPrefChangedCallback,
                                "javascript.options.mem.max");
  SetMemoryMaxPrefChangedCallback("javascript.options.mem.max",
                                  nsnull);

  Preferences::RegisterCallback(SetMemoryGCModePrefChangedCallback,
                                "javascript.options.mem.gc_per_compartment");
  SetMemoryGCModePrefChangedCallback("javascript.options.mem.gc_per_compartment",
                                     nsnull);

  Preferences::RegisterCallback(SetMemoryGCModePrefChangedCallback,
                                "javascript.options.mem.gc_incremental");
  SetMemoryGCModePrefChangedCallback("javascript.options.mem.gc_incremental",
                                     nsnull);

  Preferences::RegisterCallback(SetMemoryGCSliceTimePrefChangedCallback,
                                "javascript.options.mem.gc_incremental_slice_ms");
  SetMemoryGCSliceTimePrefChangedCallback("javascript.options.mem.gc_incremental_slice_ms",
                                          nsnull);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs)
    return NS_ERROR_FAILURE;

  Preferences::AddBoolVarCache(&sGCOnMemoryPressure,
                               "javascript.options.gc_on_memory_pressure",
                               true);

  nsIObserver* memPressureObserver = new nsMemoryPressureObserver();
  NS_ENSURE_TRUE(memPressureObserver, NS_ERROR_OUT_OF_MEMORY);
  obs->AddObserver(memPressureObserver, "memory-pressure", false);

  sIsInitialized = true;

  return NS_OK;
}

//static
nsScriptNameSpaceManager*
nsJSRuntime::GetNameSpaceManager()
{
  if (sDidShutdown)
    return nsnull;

  if (!gNameSpaceManager) {
    gNameSpaceManager = new nsScriptNameSpaceManager;
    NS_ADDREF(gNameSpaceManager);

    nsresult rv = gNameSpaceManager->Init();
    NS_ENSURE_SUCCESS(rv, nsnull);
  }

  return gNameSpaceManager;
}

/* static */
void
nsJSRuntime::Shutdown()
{
  nsJSContext::KillGCTimer();
  nsJSContext::KillShrinkGCBuffersTimer();
  nsJSContext::KillCCTimer();
  nsJSContext::KillFullGCTimer();

  NS_IF_RELEASE(gNameSpaceManager);

  if (!sContextCount) {
    // We're being shutdown, and there are no more contexts
    // alive, release the JS runtime service and the security manager.

    NS_IF_RELEASE(sRuntimeService);
    NS_IF_RELEASE(sSecurityManager);
  }

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
  if (*aRuntime == nsnull)
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
class nsJSArgArray : public nsIJSArgArray {
public:
  nsJSArgArray(JSContext *aContext, PRUint32 argc, jsval *argv, nsresult *prv);
  ~nsJSArgArray();
  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsJSArgArray,
                                                         nsIJSArgArray)

  // nsIArray
  NS_DECL_NSIARRAY

  // nsIJSArgArray
  nsresult GetArgs(PRUint32 *argc, void **argv);

  void ReleaseJSObjects();

protected:
  JSContext *mContext;
  jsval *mArgv;
  PRUint32 mArgc;
};

nsJSArgArray::nsJSArgArray(JSContext *aContext, PRUint32 argc, jsval *argv,
                           nsresult *prv) :
    mContext(aContext),
    mArgv(nsnull),
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
    for (PRUint32 i = 0; i < argc; ++i)
      mArgv[i] = argv[i];
  }

  *prv = argc > 0 ? NS_HOLD_JS_OBJECTS(this, nsJSArgArray) : NS_OK;
}

nsJSArgArray::~nsJSArgArray()
{
  ReleaseJSObjects();
}

void
nsJSArgArray::ReleaseJSObjects()
{
  if (mArgc > 0)
    NS_DROP_JS_OBJECTS(this, nsJSArgArray);
  if (mArgv) {
    PR_DELETE(mArgv);
  }
  mArgc = 0;
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
nsJSArgArray::GetArgs(PRUint32 *argc, void **argv)
{
  *argv = (void *)mArgv;
  *argc = mArgc;
  return NS_OK;
}

// nsIArray impl
NS_IMETHODIMP nsJSArgArray::GetLength(PRUint32 *aLength)
{
  *aLength = mArgc;
  return NS_OK;
}

/* void queryElementAt (in unsigned long index, in nsIIDRef uuid, [iid_is (uuid), retval] out nsQIResult result); */
NS_IMETHODIMP nsJSArgArray::QueryElementAt(PRUint32 index, const nsIID & uuid, void * *result)
{
  *result = nsnull;
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
NS_IMETHODIMP nsJSArgArray::IndexOf(PRUint32 startIndex, nsISupports *element, PRUint32 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* nsISimpleEnumerator enumerate (); */
NS_IMETHODIMP nsJSArgArray::Enumerate(nsISimpleEnumerator **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// The factory function
nsresult NS_CreateJSArgv(JSContext *aContext, PRUint32 argc, void *argv,
                         nsIJSArgArray **aArray)
{
  nsresult rv;
  nsJSArgArray *ret = new nsJSArgArray(aContext, argc,
                                       static_cast<jsval *>(argv), &rv);
  if (ret == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  if (NS_FAILED(rv)) {
    delete ret;
    return rv;
  }
  return ret->QueryInterface(NS_GET_IID(nsIArray), (void **)aArray);
}
