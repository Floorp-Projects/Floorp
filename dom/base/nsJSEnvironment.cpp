/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Hammond <mhammond@skippinet.com.au>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jscntxt.h"
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
#include "jsxdrapi.h"
#include "nsIArray.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsDOMScriptObjectHolder.h"
#include "prmem.h"
#include "WrapperFactory.h"
#include "nsGlobalWindow.h"
#include "nsScriptNameSpaceManager.h"

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

using namespace mozilla;

const size_t gStackSize = 8192;

#ifdef PR_LOGGING
static PRLogModuleInfo* gJSDiagnostics;
#endif

// Thank you Microsoft!
#ifdef CompareString
#undef CompareString
#endif

// The amount of time we wait between a request to GC (due to leaving
// a page) and doing the actual GC.
#define NS_GC_DELAY                 4000 // ms

// The amount of time we wait from the first request to GC to actually
// doing the first GC.
#define NS_FIRST_GC_DELAY           10000 // ms

// The amount of time we wait between a request to CC (after GC ran)
// and doing the actual CC.
#define NS_CC_DELAY                 5000 // ms

#define JAVASCRIPT nsIProgrammingLanguage::JAVASCRIPT

// if you add statics here, add them to the list in nsJSRuntime::Startup

static nsITimer *sGCTimer;
static nsITimer *sCCTimer;

static bool sGCHasRun;

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
    nsJSContext::GarbageCollectNow();
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
  js::AutoArrayRooter avr;
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
                   PRUint32 aLineNr, PRUint32 aColumn, PRUint32 aFlags,
                   const nsAString& aErrorMsg,
                   const nsAString& aFileName,
                   const nsAString& aSourceLine,
                   bool aDispatchEvent,
                   PRUint64 aInnerWindowID)
  : mScriptGlobal(aScriptGlobal), mLineNr(aLineNr), mColumn(aColumn),
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

          bool sameOrigin = mFileName.IsVoid();

          if (p && !sameOrigin) {
            nsCOMPtr<nsIURI> errorURI;
            NS_NewURI(getter_AddRefs(errorURI), mFileName);
            if (errorURI) {
              // FIXME: Once error reports contain the origin of the
              // error (principals) we should change this to do the
              // security check based on the principals and not
              // URIs. See bug 387476.
              sameOrigin = NS_SUCCEEDED(p->CheckMayLoad(errorURI, false));
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
            // FIXME: once the principal of the script is not tied to
            // the filename, we can stop using the post-redirect
            // filename if we want and remove this line.  Note that
            // apparently we can't handle null filenames in the error
            // event dispatching code.
            static PRUnichar nullFilename[] = { PRUnichar(0) };
            errorevent.fileName = nullFilename;
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

        nsCOMPtr<nsIScriptError2> error2(do_QueryInterface(errorObject));
        if (error2) {
          rv = error2->InitWithWindowID(mErrorMsg.get(), mFileName.get(),
                                        mSourceLine.get(),
                                        mLineNr, mColumn, mFlags,
                                        category, mInnerWindowID);
        } else {
          rv = errorObject->Init(mErrorMsg.get(), mFileName.get(),
                                 mSourceLine.get(),
                                 mLineNr, mColumn, mFlags,
                                 category);
        }

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
    JSStackFrame * fp = nsnull;
    while ((fp = JS_FrameIterator(cx, &fp))) {
      if (JS_IsScriptFrame(cx, fp)) {
        return;
      }
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

      const PRUnichar *m = reinterpret_cast<const PRUnichar*>
                                             (report->ucmessage);
      if (m) {
        msg.Assign(m);
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
        new ScriptErrorEvent(globalObject, report->lineno,
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
  NS_ENSURE_TRUE(prompt, JS_TRUE);

  // Check if we should offer the option to debug
  JSStackFrame* fp = ::JS_GetScriptedCaller(cx, NULL);
  bool debugPossible = (fp != nsnull && cx->debugHooks &&
                          cx->debugHooks->debuggerHandler != nsnull);
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
  JSScript *script = fp ? ::JS_GetFrameScript(cx, fp) : nsnull;
  if (script) {
    const char *filename = ::JS_GetScriptFilename(cx, script);
    if (filename) {
      nsXPIDLString scriptLocation;
      NS_ConvertUTF8toUTF16 filenameUTF16(filename);
      const PRUnichar *formatParams[] = { filenameUTF16.get() };
      rv = nsContentUtils::FormatLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                                 "KillScriptLocation",
                                                 formatParams, 1,
                                                 scriptLocation);

      if (NS_SUCCEEDED(rv) && scriptLocation) {
        msg.AppendLiteral("\n\n");
        msg.Append(scriptLocation);

        JSStackFrame *fp, *iterator = nsnull;
        fp = ::JS_FrameIterator(cx, &iterator);
        if (fp) {
          jsbytecode *pc = ::JS_GetFramePC(cx, fp);
          if (pc) {
            PRUint32 lineno = ::JS_PCToLineNumber(cx, script, pc);
            msg.Append(':');
            msg.AppendInt(lineno);
          }
        }
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
    // Debug the script
    jsval rval;
    switch(cx->debugHooks->debuggerHandler(cx, script, ::JS_GetFramePC(cx, fp),
                                           &rval,
                                           cx->debugHooks->
                                           debuggerHandlerData)) {
      case JSTRAP_RETURN:
        JS_SetFrameReturnValue(cx, fp, rval);
        return JS_TRUE;
      case JSTRAP_ERROR:
        JS_ClearPendingException(cx);
        return JS_FALSE;
      case JSTRAP_THROW:
        JS_SetPendingException(cx, rval);
        return JS_FALSE;
      case JSTRAP_CONTINUE:
      default:
        return JS_TRUE;
    }
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
static const char js_zeal_compartment_str[]   = JS_OPTIONS_DOT_STR "gczeal.compartment_gc";
#endif
static const char js_tracejit_content_str[]   = JS_OPTIONS_DOT_STR "tracejit.content";
static const char js_tracejit_chrome_str[]    = JS_OPTIONS_DOT_STR "tracejit.chrome";
static const char js_methodjit_content_str[]  = JS_OPTIONS_DOT_STR "methodjit.content";
static const char js_methodjit_chrome_str[]   = JS_OPTIONS_DOT_STR "methodjit.chrome";
static const char js_profiling_content_str[]  = JS_OPTIONS_DOT_STR "jitprofiling.content";
static const char js_profiling_chrome_str[]   = JS_OPTIONS_DOT_STR "jitprofiling.chrome";
static const char js_methodjit_always_str[]   = JS_OPTIONS_DOT_STR "methodjit_always";
static const char js_typeinfer_str[]          = JS_OPTIONS_DOT_STR "typeinference";
static const char js_pccounts_content_str[]   = JS_OPTIONS_DOT_STR "pccounts.content";
static const char js_pccounts_chrome_str[]    = JS_OPTIONS_DOT_STR "pccounts.chrome";
static const char js_jit_hardening_str[]      = JS_OPTIONS_DOT_STR "jit_hardening";
static const char js_memlog_option_str[] = JS_OPTIONS_DOT_STR "mem.log";

int
nsJSContext::JSOptionChangedCallback(const char *pref, void *data)
{
  nsJSContext *context = reinterpret_cast<nsJSContext *>(data);
  PRUint32 oldDefaultJSOptions = context->mDefaultJSOptions;
  PRUint32 newDefaultJSOptions = oldDefaultJSOptions;

  sPostGCEventsToConsole = Preferences::GetBool(js_memlog_option_str);

  bool strict = Preferences::GetBool(js_strict_option_str);
  if (strict)
    newDefaultJSOptions |= JSOPTION_STRICT;
  else
    newDefaultJSOptions &= ~JSOPTION_STRICT;

  nsIScriptGlobalObject *global = context->GetGlobalObject();
  // XXX should we check for sysprin instead of a chrome window, to make
  // XXX components be covered by the chrome pref instead of the content one?
  nsCOMPtr<nsIDOMChromeWindow> chromeWindow(do_QueryInterface(global));

  bool useTraceJIT = Preferences::GetBool(chromeWindow ?
                                              js_tracejit_chrome_str :
                                              js_tracejit_content_str);
  bool useMethodJIT = Preferences::GetBool(chromeWindow ?
                                               js_methodjit_chrome_str :
                                               js_methodjit_content_str);
  bool useProfiling = Preferences::GetBool(chromeWindow ?
                                               js_profiling_chrome_str :
                                               js_profiling_content_str);
  bool usePCCounts = Preferences::GetBool(chromeWindow ?
                                            js_pccounts_chrome_str :
                                            js_pccounts_content_str);
  bool useMethodJITAlways = Preferences::GetBool(js_methodjit_always_str);
  bool useTypeInference = !chromeWindow && Preferences::GetBool(js_typeinfer_str);
  bool useHardening = Preferences::GetBool(js_jit_hardening_str);
  nsCOMPtr<nsIXULRuntime> xr = do_GetService(XULRUNTIME_SERVICE_CONTRACTID);
  if (xr) {
    bool safeMode = false;
    xr->GetInSafeMode(&safeMode);
    if (safeMode) {
      useTraceJIT = false;
      useMethodJIT = false;
      useProfiling = false;
      usePCCounts = false;
      useTypeInference = false;
      useMethodJITAlways = true;
      useHardening = false;
    }
  }    

  if (useTraceJIT)
    newDefaultJSOptions |= JSOPTION_JIT;
  else
    newDefaultJSOptions &= ~JSOPTION_JIT;

  if (useMethodJIT)
    newDefaultJSOptions |= JSOPTION_METHODJIT;
  else
    newDefaultJSOptions &= ~JSOPTION_METHODJIT;

  if (useProfiling)
    newDefaultJSOptions |= JSOPTION_PROFILING;
  else
    newDefaultJSOptions &= ~JSOPTION_PROFILING;

  if (usePCCounts)
    newDefaultJSOptions |= JSOPTION_PCCOUNT;
  else
    newDefaultJSOptions &= ~JSOPTION_PCCOUNT;

  if (useMethodJITAlways)
    newDefaultJSOptions |= JSOPTION_METHODJIT_ALWAYS;
  else
    newDefaultJSOptions &= ~JSOPTION_METHODJIT_ALWAYS;

  if (useHardening)
    newDefaultJSOptions &= ~JSOPTION_SOFTEN;
  else
    newDefaultJSOptions |= JSOPTION_SOFTEN;

  if (useTypeInference)
    newDefaultJSOptions |= JSOPTION_TYPE_INFERENCE;
  else
    newDefaultJSOptions &= ~JSOPTION_TYPE_INFERENCE;

#ifdef DEBUG
  // In debug builds, warnings are enabled in chrome context if
  // javascript.options.strict.debug is true
  bool strictDebug = Preferences::GetBool(js_strict_debug_option_str);
  // Note this callback is also called from context's InitClasses thus we don't
  // need to enable this directly from InitContext
  if (strictDebug && (newDefaultJSOptions & JSOPTION_STRICT) == 0) {
    if (chromeWindow)
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

#ifdef JS_GC_ZEAL
  PRInt32 zeal = Preferences::GetInt(js_zeal_option_str, -1);
  PRInt32 frequency = Preferences::GetInt(js_zeal_frequency_str, JS_DEFAULT_ZEAL_FREQ);
  bool compartment = Preferences::GetBool(js_zeal_compartment_str, false);
  if (zeal >= 0)
    ::JS_SetGCZeal(context->mContext, (PRUint8)zeal, frequency, compartment);
#endif

  return 0;
}

nsJSContext::nsJSContext(JSRuntime *aRuntime)
  : mGCOnDestruction(true),
    mExecuteDepth(0)
{

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
  if (!mContext)
    return;

  // Clear our entry in the JSContext, bugzilla bug 66413
  ::JS_SetContextPrivate(mContext, nsnull);

  // Unregister our "javascript.options.*" pref-changed callback.
  Preferences::UnregisterCallback(JSOptionChangedCallback,
                                  js_options_dot_str, this);

  bool do_gc = mGCOnDestruction && !sGCTimer;

  // Let xpconnect destroy the JSContext when it thinks the time is right.
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  if (xpc) {
    xpc->ReleaseJSContext(mContext, !do_gc);
  } else if (do_gc) {
    ::JS_DestroyContext(mContext);
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
  NS_ASSERTION(!tmp->mContext || tmp->mContext->outstandingRequests == 0,
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
    refcnt += mContext->outstandingRequests;
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

  NS_ABORT_IF_FALSE(aScopeObject,
    "Shouldn't call EvaluateStringWithValue with null scope object.");

  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    if (aIsUndefined) {
      *aIsUndefined = true;
    }

    return NS_OK;
  }

  // Safety first: get an object representing the script's principals, i.e.,
  // the entities who signed this script, or the fully-qualified-domain-name
  // or "codebase" from which it was loaded.
  JSPrincipals *jsprin;
  nsIPrincipal *principal = aPrincipal;
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

  principal->GetJSPrincipals(mContext, &jsprin);

  // From here on, we must JSPRINCIPALS_DROP(jsprin) before returning...

  bool ok = false;

  rv = sSecurityManager->CanExecuteScripts(mContext, principal, &ok);
  if (NS_FAILED(rv)) {
    JSPRINCIPALS_DROP(mContext, jsprin);
    return NS_ERROR_FAILURE;
  }

  // Push our JSContext on the current thread's context stack so JS called
  // from native code via XPConnect uses the right context.  Do this whether
  // or not the SecurityManager said "ok", in order to simplify control flow
  // below where we pop before returning.
  nsCOMPtr<nsIJSContextStack> stack =
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext))) {
    JSPRINCIPALS_DROP(mContext, jsprin);
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

    JSAutoRequest ar(mContext);

    JSAutoEnterCompartment ac;
    if (!ac.enter(mContext, aScopeObject)) {
      JSPRINCIPALS_DROP(mContext, jsprin);
      stack->Pop(nsnull);
      return NS_ERROR_FAILURE;
    }

    ++mExecuteDepth;

    ok = ::JS_EvaluateUCScriptForPrincipalsVersion(mContext,
                                                   aScopeObject,
                                                   jsprin,
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

  // Whew!  Finally done with these manually ref-counted things.
  JSPRINCIPALS_DROP(mContext, jsprin);

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
                            const char *aURL,
                            PRUint32 aLineNo,
                            PRUint32 aVersion,
                            nsAString *aRetValue,
                            bool* aIsUndefined)
{
  NS_TIME_FUNCTION_MIN_FMT(1.0, "%s (line %d) (url: %s, line: %d)", MOZ_FUNCTION_NAME,
                           __LINE__, aURL, aLineNo);

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

  if (!aScopeObject) {
    aScopeObject = JS_GetGlobalObject(mContext);
  }

  // Safety first: get an object representing the script's principals, i.e.,
  // the entities who signed this script, or the fully-qualified-domain-name
  // or "codebase" from which it was loaded.
  JSPrincipals *jsprin;
  nsIPrincipal *principal = aPrincipal;
  if (aPrincipal) {
    aPrincipal->GetJSPrincipals(mContext, &jsprin);
  }
  else {
    nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal =
      do_QueryInterface(GetGlobalObject());
    if (!objPrincipal)
      return NS_ERROR_FAILURE;
    principal = objPrincipal->GetPrincipal();
    if (!principal)
      return NS_ERROR_FAILURE;
    principal->GetJSPrincipals(mContext, &jsprin);
  }

  // From here on, we must JSPRINCIPALS_DROP(jsprin) before returning...

  bool ok = false;

  nsresult rv = sSecurityManager->CanExecuteScripts(mContext, principal, &ok);
  if (NS_FAILED(rv)) {
    JSPRINCIPALS_DROP(mContext, jsprin);
    return NS_ERROR_FAILURE;
  }

  // Push our JSContext on the current thread's context stack so JS called
  // from native code via XPConnect uses the right context.  Do this whether
  // or not the SecurityManager said "ok", in order to simplify control flow
  // below where we pop before returning.
  nsCOMPtr<nsIJSContextStack> stack =
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext))) {
    JSPRINCIPALS_DROP(mContext, jsprin);
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
    JSAutoRequest ar(mContext);
    JSAutoEnterCompartment ac;
    if (!ac.enter(mContext, aScopeObject)) {
      stack->Pop(nsnull);
      JSPRINCIPALS_DROP(mContext, jsprin);
      return NS_ERROR_FAILURE;
    }

    ok = JS_EvaluateUCScriptForPrincipalsVersion(
      mContext, aScopeObject, jsprin,
      static_cast<const jschar*>(PromiseFlatString(aScript).get()),
      aScript.Length(), aURL, aLineNo, vp, JSVersion(aVersion));

    if (!ok) {
      // Tell XPConnect about any pending exceptions. This is needed
      // to avoid dropping JS exceptions in case we got here through
      // nested calls through XPConnect.

      ReportPendingException();
    }
  }

  // Whew!  Finally done with these manually ref-counted things.
  JSPRINCIPALS_DROP(mContext, jsprin);

  // If all went well, convert val to a string if one is wanted.
  if (ok) {
    JSAutoRequest ar(mContext);
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
                           nsScriptObjectHolder &aScriptObject)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  NS_ENSURE_ARG_POINTER(aPrincipal);

  JSObject* scopeObject = ::JS_GetGlobalObject(mContext);

  JSPrincipals *jsprin;
  aPrincipal->GetJSPrincipals(mContext, &jsprin);
  // From here on, we must JSPRINCIPALS_DROP(jsprin) before returning...

  bool ok = false;

  nsresult rv = sSecurityManager->CanExecuteScripts(mContext, aPrincipal, &ok);
  if (NS_FAILED(rv)) {
    JSPRINCIPALS_DROP(mContext, jsprin);
    return NS_ERROR_FAILURE;
  }

  aScriptObject.drop(); // ensure old object not used on failure...

  // SecurityManager said "ok", but don't compile if aVersion is unknown.
  // Since the caller is responsible for parsing the version strings, we just
  // check it isn't JSVERSION_UNKNOWN.
  if (ok && ((JSVersion)aVersion) != JSVERSION_UNKNOWN) {
    JSAutoRequest ar(mContext);

    JSScript* script =
        ::JS_CompileUCScriptForPrincipalsVersion(mContext,
                                                 scopeObject,
                                                 jsprin,
                                                 static_cast<const jschar*>(aText),
                                                 aTextLength,
                                                 aURL,
                                                 aLineNo,
                                                 JSVersion(aVersion));
    if (script) {
      NS_ASSERTION(aScriptObject.getScriptTypeID()==JAVASCRIPT,
                   "Expecting JS script object holder");
      rv = aScriptObject.set(script);
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // Whew!  Finally done.
  JSPRINCIPALS_DROP(mContext, jsprin);
  return rv;
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

  if (!aScopeObject) {
    aScopeObject = JS_GetGlobalObject(mContext);
  }

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
  JSAutoRequest ar(mContext);
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

  *aRet = JSVAL_TO_OBJECT(v);

  return NS_OK;
}


nsresult
nsJSContext::CompileEventHandler(nsIAtom *aName,
                                 PRUint32 aArgCount,
                                 const char** aArgNames,
                                 const nsAString& aBody,
                                 const char *aURL, PRUint32 aLineNo,
                                 PRUint32 aVersion,
                                 nsScriptObjectHolder &aHandler)
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
  JSAutoRequest ar(mContext);

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
  NS_ASSERTION(aHandler.getScriptTypeID()==JAVASCRIPT,
               "Expecting JS script object holder");
  return aHandler.set((void *)handler);
}

// XXX - note that CompileFunction doesn't yet play the nsScriptObjectHolder
// game - caller must still ensure JS GC root.
nsresult
nsJSContext::CompileFunction(void* aTarget,
                             const nsACString& aName,
                             PRUint32 aArgCount,
                             const char** aArgArray,
                             const nsAString& aBody,
                             const char* aURL,
                             PRUint32 aLineNo,
                             PRUint32 aVersion,
                             bool aShared,
                             void** aFunctionObject)
{
  NS_TIME_FUNCTION_FMT(1.0, "%s (line %d) (function: %s, url: %s, line: %d)", MOZ_FUNCTION_NAME,
                       __LINE__, aName.BeginReading(), aURL, aLineNo);

  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  // Don't compile if aVersion is unknown.  Since the caller is responsible for
  // parsing the version strings, we just check it isn't JSVERSION_UNKNOWN.
  if ((JSVersion)aVersion == JSVERSION_UNKNOWN) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  JSPrincipals *jsprin = nsnull;

  nsIScriptGlobalObject *global = GetGlobalObject();
  if (global) {
    // XXXbe why the two-step QI? speed up via a new GetGlobalObjectData func?
    nsCOMPtr<nsIScriptObjectPrincipal> globalData = do_QueryInterface(global);
    if (globalData) {
      nsIPrincipal *prin = globalData->GetPrincipal();
      if (!prin)
        return NS_ERROR_FAILURE;
      prin->GetJSPrincipals(mContext, &jsprin);
    }
  }

  JSObject *target = (JSObject*)aTarget;

  JSAutoRequest ar(mContext);

  JSFunction* fun =
      ::JS_CompileUCFunctionForPrincipalsVersion(mContext,
                                                 aShared ? nsnull : target, jsprin,
                                                 PromiseFlatCString(aName).get(),
                                                 aArgCount, aArgArray,
                                                 (jschar*)PromiseFlatString(aBody).get(),
                                                 aBody.Length(),
                                                 aURL, aLineNo,
                                                 JSVersion(aVersion));

  if (jsprin)
    JSPRINCIPALS_DROP(mContext, jsprin);
  if (!fun)
    return NS_ERROR_FAILURE;

  JSObject *handler = ::JS_GetFunctionObject(fun);
  if (aFunctionObject)
    *aFunctionObject = (void*) handler;
  return NS_OK;
}

nsresult
nsJSContext::CallEventHandler(nsISupports* aTarget, JSObject* aScope,
                              void *aHandler, nsIArray *aargv, nsIVariant **arv)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    return NS_OK;
  }

#ifdef NS_FUNCTION_TIMER
  {
    JSObject *obj = static_cast<JSObject *>(aHandler);
    if (js::IsFunctionProxy(obj))
      obj = js::UnwrapObject(obj);
    JSString *id = JS_GetFunctionId(static_cast<JSFunction *>(JS_GetPrivate(mContext, obj)));
    JSAutoByteString bytes;
    const char *name = !id ? "anonymous" : bytes.encode(mContext, id) ? bytes.ptr() : "<error>";
    NS_TIME_FUNCTION_FMT(1.0, "%s (line %d) (function: %s)", MOZ_FUNCTION_NAME, __LINE__, name);
  }
#endif

  JSAutoRequest ar(mContext);
  JSObject* target = nsnull;
  nsresult rv = JSObjectFromInterface(aTarget, aScope, &target);
  NS_ENSURE_SUCCESS(rv, rv);

  js::AutoObjectRooter targetVal(mContext, target);
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

    JSObject *funobj = static_cast<JSObject *>(aHandler);
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
                                      void *aHandler,
                                      nsScriptObjectHolder& aBoundHandler)
{
  NS_ENSURE_ARG(aHandler);
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);
  NS_PRECONDITION(!aBoundHandler, "Shouldn't already have a bound handler!");

  JSAutoRequest ar(mContext);

  // Get the jsobject associated with this target
  JSObject *target = nsnull;
  nsresult rv = JSObjectFromInterface(aTarget, aScope, &target);
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject *funobj = (JSObject*) aHandler;

#ifdef DEBUG
  {
    JSAutoEnterCompartment ac;
    if (!ac.enter(mContext, funobj)) {
      return NS_ERROR_FAILURE;
    }

    NS_ASSERTION(JS_TypeOfValue(mContext,
                                OBJECT_TO_JSVAL(funobj)) == JSTYPE_FUNCTION,
                 "Event handler object not a function");
  }
#endif

  JSAutoEnterCompartment ac;
  if (!ac.enter(mContext, target)) {
    return NS_ERROR_FAILURE;
  }

  // Make sure the handler function is parented by its event target object
  if (funobj) { // && ::JS_GetParent(mContext, funobj) != target) {
    funobj = ::JS_CloneFunctionObject(mContext, funobj, target);
    if (!funobj)
      rv = NS_ERROR_OUT_OF_MEMORY;
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

    nsresult rv;

    JSContext* cx = mContext;
    JSXDRState *xdr = ::JS_XDRNewMem(cx, JSXDR_ENCODE);
    if (! xdr)
        return NS_ERROR_OUT_OF_MEMORY;
    xdr->userdata = (void*) aStream;

    JSAutoRequest ar(cx);
    if (! ::JS_XDRScript(xdr, &aScriptObject)) {
        rv = NS_ERROR_FAILURE;  // likely to be a principals serialization error
    } else {
        // Get the encoded JSXDRState data and write it.  The JSXDRState owns
        // this buffer memory and will free it beneath ::JS_XDRDestroy.
        //
        // If an XPCOM object needs to be written in the midst of the JS XDR
        // encoding process, the C++ code called back from the JS engine (e.g.,
        // nsEncodeJSPrincipals in caps/src/nsJSPrincipals.cpp) will flush data
        // from the JSXDRState to aStream, then write the object, then return
        // to JS XDR code with xdr reset so new JS data is encoded at the front
        // of the xdr's data buffer.
        //
        // However many XPCOM objects are interleaved with JS XDR data in the
        // stream, when control returns here from ::JS_XDRScript, we'll have
        // one last buffer of data to write to aStream.

        uint32 size;
        const char* data = reinterpret_cast<const char*>
                                           (::JS_XDRMemGetData(xdr, &size));
        NS_ASSERTION(data, "no decoded JSXDRState data!");

        rv = aStream->Write32(size);
        if (NS_SUCCEEDED(rv))
            rv = aStream->WriteBytes(data, size);
    }

    ::JS_XDRDestroy(xdr);
    if (NS_FAILED(rv)) return rv;

    return rv;
}

nsresult
nsJSContext::Deserialize(nsIObjectInputStream* aStream,
                         nsScriptObjectHolder &aResult)
{
    JSScript *result = nsnull;
    nsresult rv;

    NS_TIME_FUNCTION_MIN(1.0);

    PRUint32 size;
    rv = aStream->Read32(&size);
    if (NS_FAILED(rv)) return rv;

    char* data;
    rv = aStream->ReadBytes(size, &data);
    if (NS_FAILED(rv)) return rv;

    JSContext* cx = mContext;

    JSXDRState *xdr = ::JS_XDRNewMem(cx, JSXDR_DECODE);
    if (! xdr) {
        rv = NS_ERROR_OUT_OF_MEMORY;
    } else {
        xdr->userdata = (void*) aStream;
        JSAutoRequest ar(cx);
        ::JS_XDRMemSetData(xdr, data, size);

        if (! ::JS_XDRScript(xdr, &result)) {
            rv = NS_ERROR_FAILURE;  // principals deserialization error?
        }

        // Update data in case ::JS_XDRScript called back into C++ code to
        // read an XPCOM object.
        //
        // In that case, the serialization process must have flushed a run
        // of counted bytes containing JS data at the point where the XPCOM
        // object starts, after which an encoding C++ callback from the JS
        // XDR code must have written the XPCOM object directly into the
        // nsIObjectOutputStream.
        //
        // The deserialization process will XDR-decode counted bytes up to
        // but not including the XPCOM object, then call back into C++ to
        // read the object, then read more counted bytes and hand them off
        // to the JSXDRState, so more JS data can be decoded.
        //
        // This interleaving of JS XDR data and XPCOM object data may occur
        // several times beneath the call to ::JS_XDRScript, above.  At the
        // end of the day, we need to free (via nsMemory) the data owned by
        // the JSXDRState.  So we steal it back, nulling xdr's buffer so it
        // doesn't get passed to ::JS_free by ::JS_XDRDestroy.

        uint32 junk;
        data = (char*) ::JS_XDRMemGetData(xdr, &junk);
        if (data)
            ::JS_XDRMemSetData(xdr, NULL, 0);
        ::JS_XDRDestroy(xdr);
    }

    // If data is null now, it must have been freed while deserializing an
    // XPCOM object (e.g., a principal) beneath ::JS_XDRScript.
    if (data)
        nsMemory::Free(data);
    NS_ASSERTION(aResult.getScriptTypeID()==JAVASCRIPT,
                 "Expecting JS script object holder");

    // Now that we've cleaned up, handle the case when rv is a failure
    // code, which could happen for all sorts of reasons above.
    NS_ENSURE_SUCCESS(rv, rv);

    return aResult.set(result);
}

void
nsJSContext::SetDefaultLanguageVersion(PRUint32 aVersion)
{
  ::JS_SetVersion(mContext, (JSVersion)aVersion);
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

  JSClass *c = JS_GET_CLASS(mContext, global);

  if (!c || ((~c->flags) & (JSCLASS_HAS_PRIVATE |
                            JSCLASS_PRIVATE_IS_NSISUPPORTS))) {
    return nsnull;
  }

  nsISupports *priv = (nsISupports *)js::GetObjectPrivate(global);

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
                                void **aNativeGlobal, nsISupports **aHolder)
{
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  PRUint32 flags = aIsChrome? nsIXPConnect::FLAG_SYSTEM_GLOBAL_OBJECT : 0;
  nsCOMPtr<nsIXPConnectJSObjectHolder> jsholder;

  nsCOMPtr<nsIPrincipal> systemPrincipal;
  if (aIsChrome) {
    nsIScriptSecurityManager *ssm = nsContentUtils::GetSecurityManager();
    ssm->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
  }

  nsresult rv = xpc->
          InitClassesWithNewWrappedGlobal(mContext,
                                          aNewInner, NS_GET_IID(nsISupports),
                                          aIsChrome ? systemPrincipal.get() : aPrincipal,
                                          nsnull, flags,
                                          getter_AddRefs(jsholder));
  if (NS_FAILED(rv))
    return rv;
  jsholder->GetJSObject(reinterpret_cast<JSObject **>(aNativeGlobal));
  *aHolder = jsholder.get();
  NS_ADDREF(*aHolder);
  return NS_OK;
}

nsresult
nsJSContext::ConnectToInner(nsIScriptGlobalObject *aNewInner, void *aOuterGlobal)
{
  NS_ENSURE_ARG(aNewInner);
#ifdef DEBUG
  JSObject *newInnerJSObject = aNewInner->GetGlobalJSObject();
#endif
  JSObject *outerGlobal = (JSObject *)aOuterGlobal;

  // Now that we're connecting the outer global to the inner one,
  // we must have transplanted it. The JS engine tries to maintain
  // the global object's compartment as its default compartment,
  // so update that now since it might have changed.
  JS_SetGlobalObject(mContext, outerGlobal);
  NS_ASSERTION(JS_GetPrototype(mContext, outerGlobal) ==
               JS_GetPrototype(mContext, newInnerJSObject),
               "outer and inner globals should have the same prototype");

  return NS_OK;
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

  return NS_OK;
}

nsresult
nsJSContext::CreateOuterObject(nsIScriptGlobalObject *aGlobalObject,
                               nsIScriptGlobalObject *aCurrentInner)
{
  mGlobalObjectRef = aGlobalObject;

  nsCOMPtr<nsIDOMChromeWindow> chromeWindow(do_QueryInterface(aGlobalObject));

  if (chromeWindow) {
    // Always enable E4X for XUL and other chrome content -- there is no
    // need to preserve the <!-- script hiding hack from JS-in-HTML daze
    // (introduced in 1995 for graceful script degradation in Netscape 1,
    // Mosaic, and other pre-JS browsers).
    JS_SetOptions(mContext, JS_GetOptions(mContext) | JSOPTION_XML);
  }

  JSObject *outer =
    NS_NewOuterWindowProxy(mContext, aCurrentInner->GetGlobalJSObject());
  if (!outer) {
    return NS_ERROR_FAILURE;
  }

  js::SetProxyExtra(outer, 0, js::PrivateValue(aGlobalObject));

  return SetOuterObject(outer);
}

nsresult
nsJSContext::SetOuterObject(void *aOuterObject)
{
  JSObject *outer = static_cast<JSObject *>(aOuterObject);

  // Force our context's global object to be the outer.
  JS_SetGlobalObject(mContext, outer);

  // NB: JS_SetGlobalObject sets mContext->compartment.
  JSObject *inner = JS_GetParent(mContext, outer);

  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  nsresult rv = xpc->GetWrappedNativeOfJSObject(mContext, inner,
                                                getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ABORT_IF_FALSE(wrapper, "bad wrapper");

  wrapper->RefreshPrototype();
  JS_SetPrototype(mContext, outer, JS_GetPrototype(mContext, inner));

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
nsJSContext::SetProperty(void *aTarget, const char *aPropName, nsISupports *aArgs)
{
  PRUint32  argc;
  jsval    *argv = nsnull;

  JSAutoRequest ar(mContext);

  Maybe<nsRootedJSValueArray> tempStorage;

  nsresult rv;
  rv = ConvertSupportsTojsvals(aArgs, GetNativeGlobal(), &argc, &argv, tempStorage);
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
  rv = ::JS_DefineProperty(mContext, reinterpret_cast<JSObject *>(aTarget),
                           aPropName, vargs, nsnull, nsnull, 0) ?
       NS_OK : NS_ERROR_FAILURE;

  return rv;
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
  PRUint32 argCtr, argCount;
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
    for (argCtr = 0; argCtr < argCount && NS_SUCCEEDED(rv); argCtr++) {
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
      jsval v;
      nsresult rv = nsContentUtils::WrapNative(cx, ::JS_GetGlobalObject(cx),
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
TraceMallocDisable(JSContext *cx, uintN argc, jsval *vp)
{
    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return JS_FALSE;

    NS_TraceMallocDisable();
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
TraceMallocEnable(JSContext *cx, uintN argc, jsval *vp)
{
    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return JS_FALSE;

    NS_TraceMallocEnable();
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
TraceMallocOpenLogFile(JSContext *cx, uintN argc, jsval *vp)
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
TraceMallocChangeLogFD(JSContext *cx, uintN argc, jsval *vp)
{
    int32 fd, oldfd;

    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return JS_FALSE;

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
TraceMallocCloseLogFD(JSContext *cx, uintN argc, jsval *vp)
{
    int32 fd;

    if (!CheckUniversalXPConnectForTraceMalloc(cx))
        return JS_FALSE;

    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    if (argc == 0)
        return JS_TRUE;
    if (!JS_ValueToECMAInt32(cx, JS_ARGV(cx, vp)[0], &fd))
        return JS_FALSE;
    NS_TraceMallocCloseLogFD((int) fd);
    return JS_TRUE;
}

static JSBool
TraceMallocLogTimestamp(JSContext *cx, uintN argc, jsval *vp)
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
TraceMallocDumpAllocations(JSContext *cx, uintN argc, jsval *vp)
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
JProfStartProfilingJS(JSContext *cx, uintN argc, jsval *vp)
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
JProfStopProfilingJS(JSContext *cx, uintN argc, jsval *vp)
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
JProfClearCircularJS(JSContext *cx, uintN argc, jsval *vp)
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
JProfSaveCircularJS(JSContext *cx, uintN argc, jsval *vp)
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

#ifdef MOZ_TRACEVIS
static JSFunctionSpec EthogramFunctions[] = {
    {"initEthogram",               js_InitEthogram,            0, 0},
    {"shutdownEthogram",           js_ShutdownEthogram,        0, 0},
    {nsnull,                       nsnull,                     0, 0}
};
#endif

nsresult
nsJSContext::InitClasses(void *aGlobalObj)
{
  nsresult rv = NS_OK;

  JSObject *globalObj = static_cast<JSObject *>(aGlobalObj);

  rv = InitializeExternalClasses();
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(mContext);

  ::JS_SetOptions(mContext, mDefaultJSOptions);

  // Attempt to initialize profiling functions
  ::JS_DefineProfilingFunctions(mContext, globalObj);

#ifdef NS_TRACE_MALLOC
  // Attempt to initialize TraceMalloc functions
  ::JS_DefineFunctions(mContext, globalObj, TraceMallocFunctions);
#endif

#ifdef MOZ_JPROF
  // Attempt to initialize JProf functions
  ::JS_DefineFunctions(mContext, globalObj, JProfFunctions);
#endif

#ifdef MOZ_TRACEVIS
  // Attempt to initialize Ethogram functions
  ::JS_DefineFunctions(mContext, globalObj, EthogramFunctions);
#endif

  JSOptionChangedCallback(js_options_dot_str, this);

  return rv;
}

void
nsJSContext::ClearScope(void *aGlobalObj, bool aClearFromProtoChain)
{
  // Push our JSContext on our thread's context stack.
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  if (stack && NS_FAILED(stack->Push(mContext))) {
    stack = nsnull;
  }

  if (aGlobalObj) {
    JSObject *obj = (JSObject *)aGlobalObj;
    JSAutoRequest ar(mContext);

    JSAutoEnterCompartment ac;
    ac.enterAndIgnoreErrors(mContext, obj);

    // Grab a reference to the window property, which is the outer
    // window, so that we can re-define it once we've cleared
    // scope. This is what keeps the outer window alive in cases where
    // nothing else does.
    jsval window;
    if (!JS_GetProperty(mContext, obj, "window", &window)) {
      window = JSVAL_VOID;

      JS_ClearPendingException(mContext);
    }

    JS_ClearScope(mContext, obj);

    NS_ABORT_IF_FALSE(!xpc::WrapperFactory::IsXrayWrapper(obj), "unexpected wrapper");

    if (window != JSVAL_VOID) {
      if (!JS_DefineProperty(mContext, obj, "window", window,
                             JS_PropertyStub, JS_StrictPropertyStub,
                             JSPROP_ENUMERATE | JSPROP_READONLY |
                             JSPROP_PERMANENT)) {
        JS_ClearPendingException(mContext);
      }
    }

    if (!js::GetObjectParent(obj)) {
      JS_ClearRegExpStatics(mContext, obj);
    }

    // Always clear watchpoints, to deal with two cases:
    // 1.  The first document for this window is loading, and a miscreant has
    //     preset watchpoints on the window object in order to attack the new
    //     document's privileged information.
    // 2.  A document loaded and used watchpoints on its own window, leaving
    //     them set until the next document loads. We must clean up window
    //     watchpoints here.
    // Watchpoints set on document and subordinate objects are all cleared
    // when those sub-window objects are finalized, after JS_ClearScope and
    // a GC run that finds them to be garbage.
    ::JS_ClearWatchPointsForObject(mContext, obj);

    // Since the prototype chain is shared between inner and outer (and
    // stays with the inner), we don't clear things from the prototype
    // chain when we're clearing an outer window whose current inner we
    // still want.
    if (aClearFromProtoChain) {
      nsWindowSH::InvalidateGlobalScopePolluter(mContext, obj);

      // Clear up obj's prototype chain, but not Object.prototype.
      for (JSObject *o = ::JS_GetPrototype(mContext, obj), *next;
           o && (next = ::JS_GetPrototype(mContext, o)); o = next)
        ::JS_ClearScope(mContext, o);
    }
  }

  if (stack) {
    stack->Pop(nsnull);
  }
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
nsJSContext::FinalizeContext()
{
  ;
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
  }
}

nsresult
nsJSContext::SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                    nsISupports* aRef)
{
  NS_PRECONDITION(GetExecutingScript(), "should be executing script");

  nsJSContext::TerminationFuncClosure* newClosure =
    new nsJSContext::TerminationFuncClosure(aFunc, aRef, mTerminations);
  if (!newClosure) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mTerminations = newClosure;
  return NS_OK;
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

//static
void
nsJSContext::GarbageCollectNow()
{
  NS_TIME_FUNCTION_MIN(1.0);

  KillGCTimer();

  // Reset sPendingLoadCount in case the timer that fired was a
  // timer we scheduled due to a normal GC timer firing while
  // documents were loading. If this happens we're waiting for a
  // document that is taking a long time to load, and we effectively
  // ignore the fact that the currently loading documents are still
  // loading and move on as if they weren't.
  sPendingLoadCount = 0;
  sLoadingInProgress = false;

  if (nsContentUtils::XPConnect()) {
    nsContentUtils::XPConnect()->GarbageCollect();
  }
}

//Static
void
nsJSContext::CycleCollectNow(nsICycleCollectorListener *aListener)
{
  if (!NS_IsMainThread()) {
    return;
  }

  NS_TIME_FUNCTION_MIN(1.0);

  KillCCTimer();

  PRTime start = PR_Now();

  PRUint32 suspected = nsCycleCollector_suspectedCount();
  PRUint32 collected = nsCycleCollector_collect(aListener);
  sCCollectedWaitingForGC += collected;

  // If we collected a substantial amount of cycles, poke the GC since more objects
  // might be unreachable now.
  if (sCCollectedWaitingForGC > 250) {
    PokeGC();
  }

  if (sPostGCEventsToConsole) {
    PRTime now = PR_Now();
    PRTime delta = 0;
    if (sFirstCollectionTime) {
      delta = now - sFirstCollectionTime;
    } else {
      sFirstCollectionTime = now;
    }

    NS_NAMED_LITERAL_STRING(kFmt,
                            "CC(T+%.1f) collected: %lu (%lu waiting for GC), suspected: %lu, duration: %llu ms.");
    nsString msg;
    msg.Adopt(nsTextFormatter::smprintf(kFmt.get(), double(delta) / PR_USEC_PER_SEC,
                                        collected, sCCollectedWaitingForGC, suspected,
                                        (now - start) / PR_USEC_PER_MSEC));
    nsCOMPtr<nsIConsoleService> cs =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (cs) {
      cs->LogStringMessage(msg.get());
    }
  }
}

// static
void
GCTimerFired(nsITimer *aTimer, void *aClosure)
{
  NS_RELEASE(sGCTimer);

  nsJSContext::GarbageCollectNow();
}

// static
void
CCTimerFired(nsITimer *aTimer, void *aClosure)
{
  NS_RELEASE(sCCTimer);

  nsJSContext::CycleCollectNow();
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
  PokeGC();
}

// static
void
nsJSContext::PokeGC()
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

  sGCTimer->InitWithFuncCallback(GCTimerFired, nsnull,
                                 first
                                 ? NS_FIRST_GC_DELAY
                                 : NS_GC_DELAY,
                                 nsITimer::TYPE_ONE_SHOT);

  first = false;
}

// static
void
nsJSContext::MaybePokeCC()
{
  if (nsCycleCollector_suspectedCount() > 1000) {
    PokeCC();
  }
}

// static
void
nsJSContext::PokeCC()
{
  if (sCCTimer || !sGCHasRun) {
    // There's already a timer for GC'ing, or GC hasn't run yet, just return.
    return;
  }

  CallCreateInstance("@mozilla.org/timer;1", &sCCTimer);

  if (!sCCTimer) {
    // Failed to create timer (probably because we're in XPCOM shutdown)
    return;
  }

  sCCTimer->InitWithFuncCallback(CCTimerFired, nsnull,
                                 NS_CC_DELAY,
                                 nsITimer::TYPE_ONE_SHOT);
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

//static
void
nsJSContext::KillCCTimer()
{
  if (sCCTimer) {
    sCCTimer->Cancel();

    NS_RELEASE(sCCTimer);
  }
}

void
nsJSContext::GC()
{
  PokeGC();
}

static void
DOMGCFinishedCallback(JSRuntime *rt, JSCompartment *comp, const char *status)
{
  NS_ASSERTION(NS_IsMainThread(), "GCs must run on the main thread");

  if (sPostGCEventsToConsole) {
    PRTime now = PR_Now();
    PRTime delta = 0;
    if (sFirstCollectionTime) {
      delta = now - sFirstCollectionTime;
    } else {
      sFirstCollectionTime = now;
    }

    NS_NAMED_LITERAL_STRING(kFmt, "GC(T+%.1f) %s");
    nsString msg;
    msg.Adopt(nsTextFormatter::smprintf(kFmt.get(),
                                        double(delta) / PR_USEC_PER_SEC, status));
    nsCOMPtr<nsIConsoleService> cs = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    if (cs) {
      cs->LogStringMessage(msg.get());
    }
  }

  sCCollectedWaitingForGC = 0;
  if (sGCTimer) {
    // If we were waiting for a GC to happen, kill the timer.
    nsJSContext::KillGCTimer();

    // If this is a compartment GC, restart it. We still want
    // a full GC to happen. Compartment GCs usually happen as a
    // result of last-ditch or MaybeGC. In both cases its
    // probably a time of heavy activity and we want to delay
    // the full GC, but we do want it to happen eventually.
    if (comp) {
      nsJSContext::PokeGC();

      // We poked the GC, so we can kill any pending CC here.
      nsJSContext::KillCCTimer();
    }
  } else {
    // If this was a full GC, poke the CC to run soon.
    if (!comp) {
      sGCHasRun = true;
      nsJSContext::PokeCC();
    }
  }

  // If we didn't end up scheduling a GC, and there are unused
  // chunks waiting to expire, make sure we will GC again soon.
  if (!sGCTimer && JS_GetGCParameter(rt, JSGC_UNUSED_CHUNKS) > 0) {
    nsJSContext::PokeGC();
  }
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
  sGCTimer = sCCTimer = nsnull;
  sGCHasRun = false;
  sPendingLoadCount = 0;
  sLoadingInProgress = false;
  sCCollectedWaitingForGC = 0;
  sPostGCEventsToConsole = false;
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
  bool enableCompartmentGC = Preferences::GetBool(aPrefName);
  JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_MODE, enableCompartmentGC
                                                      ? JSGC_MODE_COMPARTMENT
                                                      : JSGC_MODE_GLOBAL);
  return 0;
}

static JSPrincipals *
ObjectPrincipalFinder(JSContext *cx, JSObject *obj)
{
  if (!sSecurityManager)
    return nsnull;

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv =
    sSecurityManager->GetObjectPrincipal(cx, obj,
                                         getter_AddRefs(principal));

  if (NS_FAILED(rv) || !principal) {
    return nsnull;
  }

  JSPrincipals *jsPrincipals = nsnull;
  principal->GetJSPrincipals(cx, &jsPrincipals);

  // nsIPrincipal::GetJSPrincipals() returns a strong reference to the
  // JS principals, but the caller of this function expects a weak
  // reference. So we need to release here.

  JSPRINCIPALS_DROP(cx, jsPrincipals);

  return jsPrincipals;
}

JSObject*
NS_DOMReadStructuredClone(JSContext* cx,
                          JSStructuredCloneReader* reader,
                          uint32 tag,
                          uint32 data,
                          void* closure)
{
  // We don't currently support any extensions to structured cloning.
  nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_DOM_DATA_CLONE_ERR);
  return nsnull;
}

JSBool
NS_DOMWriteStructuredClone(JSContext* cx,
                           JSStructuredCloneWriter* writer,
                           JSObject* obj,
                           void *closure)
{
  // We don't currently support any extensions to structured cloning.
  nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_DOM_DATA_CLONE_ERR);
  return JS_FALSE;
}

void
NS_DOMStructuredCloneError(JSContext* cx,
                           uint32 errorid)
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

  ::JS_SetGCFinishedCallback(sRuntime, DOMGCFinishedCallback);

  JSSecurityCallbacks *callbacks = JS_GetRuntimeSecurityCallbacks(sRuntime);
  NS_ASSERTION(callbacks, "SecMan should have set security callbacks!");

  callbacks->findObjectPrincipals = ObjectPrincipalFinder;

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
  nsJSContext::KillCCTimer();

  NS_IF_RELEASE(gNameSpaceManager);

  if (!sContextCount) {
    // We're being shutdown, and there are no more contexts
    // alive, release the JS runtime service and the security manager.

    if (sRuntimeService && sSecurityManager) {
      JSSecurityCallbacks *callbacks = JS_GetRuntimeSecurityCallbacks(sRuntime);
      if (callbacks) {
        NS_ASSERTION(callbacks->findObjectPrincipals == ObjectPrincipalFinder,
                     "Fighting over the findObjectPrincipals callback!");
        callbacks->findObjectPrincipals = NULL;
      }
    }
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
class nsJSArgArray : public nsIJSArgArray, public nsIArray {
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
        NS_IMPL_CYCLE_COLLECTION_TRACE_CALLBACK(JAVASCRIPT,
                                                JSVAL_TO_GCTHING(*argv),
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
  if (!mArgv) {
    NS_WARNING("nsJSArgArray has no argv!");
    return NS_ERROR_UNEXPECTED;
  }
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
                         nsIArray **aArray)
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
