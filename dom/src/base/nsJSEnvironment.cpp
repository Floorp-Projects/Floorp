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

#include "nsJSEnvironment.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIDOMChromeWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIDOMText.h"
#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMKeyEvent.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMChromeWindow.h"
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
#include "nsScriptNameSpaceManager.h"
#include "nsThreadUtils.h"
#include "nsITimer.h"
#include "nsDOMClassInfo.h"
#include "nsIAtom.h"
#include "nsContentUtils.h"
#include "jscntxt.h"
#include "nsEventDispatcher.h"
#include "nsIContent.h"
#include "nsCycleCollector.h"

// For locale aware string methods
#include "plstr.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsUnicharUtils.h"
#include "nsILocaleService.h"
#include "nsICollation.h"
#include "nsCollationCID.h"
#include "nsDOMClassInfo.h"

#include "jsdbgapi.h"           // for JS_ClearWatchPointsForObject
#include "jsxdrapi.h"
#include "nsIArray.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsITimelineService.h"
#include "nsDOMScriptObjectHolder.h"
#include "prmem.h"

#ifdef NS_DEBUG
#include "jsgc.h"       // for WAY_TOO_MUCH_GC, if defined for GC debugging
#include "nsGlobalWindow.h"
#endif

#ifdef MOZ_JSDEBUGGER
#include "jsdIDebuggerService.h"
#endif

#include "nsIStringBundle.h"

#ifdef MOZ_LOGGING
// Force PR_LOGGING so we can get JS strict warnings even in release builds
#define FORCE_PR_LOG 1
#endif
#include "prlog.h"
#include "prthread.h"

#ifdef OJI
#include "nsIJVMManager.h"
#include "nsILiveConnectManager.h"
#endif

const size_t gStackSize = 8192;

#ifdef PR_LOGGING
static PRLogModuleInfo* gJSDiagnostics;
#endif

// Thank you Microsoft!
#ifndef WINCE
#ifdef CompareString
#undef CompareString
#endif
#endif // WINCE

// The amount of time we wait between a request to GC (due to leaving
// a page) and doing the actual GC.
#define NS_GC_DELAY                 2000 // ms

// The amount of time we wait until we force a GC in case the previous
// GC timer happened to fire while we were in the middle of loading a
// page (we'll GC once the page is loaded if that happens before this
// amount of time has passed).
#define NS_LOAD_IN_PROCESS_GC_DELAY 4000 // ms

// The amount of time we wait from the first request to GC to actually
// doing the first GC.
#define NS_FIRST_GC_DELAY           10000 // ms

#define JAVASCRIPT nsIProgrammingLanguage::JAVASCRIPT

// if you add statics here, add them to the list in nsJSRuntime::Startup

static nsITimer *sGCTimer;
static PRBool sReadyForGC;

// The number of currently pending document loads. This count isn't
// guaranteed to always reflect reality and can't easily as we don't
// have an easy place to know when a load ends or is interrupted in
// all cases. This counter also gets reset if we end up GC'ing while
// we're waiting for a slow page to load. IOW, this count may be 0
// even when there are pending loads.
static PRUint32 sPendingLoadCount;

// Boolean that tells us whether or not the current GC timer
// (sGCTimer) was scheduled due to a GC timer firing while we were in
// the middle of loading a page.
static PRBool sLoadInProgressGCTimer;

nsScriptNameSpaceManager *gNameSpaceManager;

static nsIJSRuntimeService *sRuntimeService;
JSRuntime *nsJSRuntime::sRuntime;

static const char kJSRuntimeServiceContractID[] =
  "@mozilla.org/js/xpc/RuntimeService;1";

static const char kDOMStringBundleURL[] =
  "chrome://global/locale/dom/dom.properties";

static JSGCCallback gOldJSGCCallback;

static PRBool sIsInitialized;
static PRBool sDidShutdown;

static PRInt32 sContextCount;

static PRTime sMaxScriptRunTime;
static PRTime sMaxChromeScriptRunTime;

static nsIScriptSecurityManager *sSecurityManager;

static nsICollation *gCollation;

static nsIUnicodeDecoder *gDecoder;

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

class AutoFreeJSStack {
public:
  AutoFreeJSStack(JSContext *ctx, void *aPtr) : mContext(ctx), mStack(aPtr) {
  }
  ~AutoFreeJSStack() {
    if (mContext && mStack)
      js_FreeStack(mContext, mStack);
  }
private:
  JSContext *mContext;
  void *mStack;
};

// A utility function for script languages to call.  Although it looks small,
// the use of nsIDocShell and nsPresContext triggers a huge number of
// dependencies that most languages would not otherwise need.
// XXXmarkh - This function is mis-placed!
PRBool
NS_HandleScriptError(nsIScriptGlobalObject *aScriptGlobal,
                     nsScriptErrorEvent *aErrorEvent,
                     nsEventStatus *aStatus)
{
  PRBool called = PR_FALSE;
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aScriptGlobal));
  nsIDocShell *docShell = win ? win->GetDocShell() : nsnull;
  if (docShell) {
    nsCOMPtr<nsPresContext> presContext;
    docShell->GetPresContext(getter_AddRefs(presContext));

    static PRInt32 errorDepth; // Recursion prevention
    ++errorDepth;
    
    if (presContext && errorDepth < 2) {
      // Dispatch() must be synchronous for the recursion block
      // (errorDepth) to work.
      nsEventDispatcher::Dispatch(win, presContext, aErrorEvent, nsnull,
                                  aStatus);
      called = PR_TRUE;
    }
    --errorDepth;
  }
  return called;
}

// NOTE: This function could be refactored to use the above.  The only reason
// it has not been done is that the code below only fills the error event
// after it has a good nsPresContext - whereas using the above function
// would involve always filling it.  Is that a concern?
void JS_DLL_CALLBACK
NS_ScriptErrorReporter(JSContext *cx,
                       const char *message,
                       JSErrorReport *report)
{
  NS_ASSERTION(message || report,
               "Must have a message or a report; otherwise what are we "
               "reporting?");
  
  // XXX this means we are not going to get error reports on non DOM contexts
  nsIScriptContext *context = nsJSUtils::GetDynamicScriptContext(cx);

  nsEventStatus status = nsEventStatus_eIgnore;

  // Note: we must do this before running any more code on cx (if cx is the
  // dynamic script context).
  ::JS_ClearPendingException(cx);

  if (context) {
    nsIScriptGlobalObject *globalObject = context->GetGlobalObject();

    if (globalObject) {
      nsAutoString fileName, msg;

      if (report) {
        fileName.AssignWithConversion(report->filename);

        const PRUnichar *m = NS_REINTERPRET_CAST(const PRUnichar*,
                                                 report->ucmessage);

        if (m) {
          msg.Assign(m);
        }
      }

      if (msg.IsEmpty() && message) {
        msg.AssignWithConversion(message);
      }

      // First, notify the DOM that we have a script error.
      /* We do not try to report Out Of Memory via a dom
       * event because the dom event handler would encounter
       * an OOM exception trying to process the event, and
       * then we'd need to generate a new OOM event for that
       * new OOM instance -- this isn't pretty.
       */
      {
        // Scope to make sure we're not using |win| in the rest of
        // this function when we should be using |globalObject|.  We
        // only need |win| for the event dispatch.
        nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(globalObject));
        nsIDocShell *docShell = win ? win->GetDocShell() : nsnull;
        if (docShell &&
            (!report ||
             (report->errorNumber != JSMSG_OUT_OF_MEMORY &&
              !JSREPORT_IS_WARNING(report->flags)))) {
          static PRInt32 errorDepth; // Recursion prevention
          ++errorDepth;

          nsCOMPtr<nsPresContext> presContext;
          docShell->GetPresContext(getter_AddRefs(presContext));

          if (presContext && errorDepth < 2) {
            nsScriptErrorEvent errorevent(PR_TRUE, NS_LOAD_ERROR);

            errorevent.fileName = fileName.get();
            errorevent.errorMsg = msg.get();
            errorevent.lineNr = report ? report->lineno : 0;

            // Dispatch() must be synchronous for the recursion block
            // (errorDepth) to work.
            nsEventDispatcher::Dispatch(win, presContext, &errorevent, nsnull,
                                        &status);
          }

          --errorDepth;
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
            do_QueryInterface(globalObject);
          NS_ASSERTION(scriptPrincipal, "Global objects must implement "
                       "nsIScriptObjectPrincipal");
          nsCOMPtr<nsIPrincipal> systemPrincipal;
          sSecurityManager->GetSystemPrincipal(getter_AddRefs(systemPrincipal));
          const char * category =
            scriptPrincipal->GetPrincipal() == systemPrincipal
            ? "chrome javascript"
            : "content javascript";

          if (report) {
            PRUint32 column = report->uctokenptr - report->uclinebuf;

            rv = errorObject->Init(msg.get(), fileName.get(),
                                   NS_REINTERPRET_CAST(const PRUnichar*,
                                                       report->uclinebuf),
                                   report->lineno, column, report->flags,
                                   category);
          } else if (message) {
            rv = errorObject->Init(msg.get(), nsnull, nsnull, 0, 0, 0,
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
    }
  }

#ifdef DEBUG
  // Print it to stderr as well, for the benefit of those invoking
  // mozilla with -console.
  nsCAutoString error;
  error.Assign("JavaScript ");
  if (!report) {
    error.Append("[no report]: ");
    error.Append(message);
  } else {
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
      AppendUTF16toUTF8(NS_REINTERPRET_CAST(const PRUnichar*, report->ucmessage),
                        error);
    } else {
      error.Append(message);
    }
    if (status != nsEventStatus_eIgnore && !JSREPORT_IS_WARNING(report->flags))
      error.Append(" Error was suppressed by event handler\n");
  }
  fprintf(stderr, "%s\n", error.get());
  fflush(stderr);
#endif

#ifdef PR_LOGGING
  if (report) {
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
  }
#endif
}

JS_STATIC_DLL_CALLBACK(JSBool)
LocaleToUnicode(JSContext *cx, char *src, jsval *rval)
{
  nsresult rv;

  if (!gDecoder) {
    // use app default locale
    nsCOMPtr<nsILocaleService> localeService = 
      do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsILocale> appLocale;
      rv = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
      if (NS_SUCCEEDED(rv)) {
        nsAutoString localeStr;
        rv = appLocale->
          GetCategory(NS_LITERAL_STRING(NSILOCALE_TIME), localeStr);
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get app locale info");

        nsCOMPtr<nsIPlatformCharset> platformCharset =
          do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);

        if (NS_SUCCEEDED(rv)) {
          nsCAutoString charset;
          rv = platformCharset->GetDefaultCharsetForLocale(localeStr, charset);
          if (NS_SUCCEEDED(rv)) {
            // get/create unicode decoder for charset
            nsCOMPtr<nsICharsetConverterManager> ccm =
              do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
            if (NS_SUCCEEDED(rv))
              ccm->GetUnicodeDecoder(charset.get(), &gDecoder);
          }
        }
      }
    }
  }

  JSString *str = nsnull;
  PRInt32 srcLength = PL_strlen(src);

  if (gDecoder) {
    PRInt32 unicharLength = srcLength;
    PRUnichar *unichars =
      (PRUnichar *)JS_malloc(cx, (srcLength + 1) * sizeof(PRUnichar));
    if (unichars) {
      rv = gDecoder->Convert(src, &srcLength, unichars, &unicharLength);
      if (NS_SUCCEEDED(rv)) {
        // terminate the returned string
        unichars[unicharLength] = 0;

        // nsIUnicodeDecoder::Convert may use fewer than srcLength PRUnichars
        if (unicharLength + 1 < srcLength + 1) {
          PRUnichar *shrunkUnichars =
            (PRUnichar *)JS_realloc(cx, unichars,
                                    (unicharLength + 1) * sizeof(PRUnichar));
          if (shrunkUnichars)
            unichars = shrunkUnichars;
        }
        str = JS_NewUCString(cx,
                             NS_REINTERPRET_CAST(jschar*, unichars),
                             unicharLength);
      }
      if (!str)
        JS_free(cx, unichars);
    }
  }

  if (!str) {
    nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_OUT_OF_MEMORY);
    return JS_FALSE;
  }

  *rval = STRING_TO_JSVAL(str);
  return JS_TRUE;
}


static JSBool
ChangeCase(JSContext *cx, JSString *src, jsval *rval,
           void(* changeCaseFnc)(const nsAString&, nsAString&))
{
  nsAutoString result;
  changeCaseFnc(nsDependentJSString(src), result);

  JSString *ucstr = JS_NewUCStringCopyN(cx, (jschar*)result.get(), result.Length());
  if (!ucstr) {
    return JS_FALSE;
  }

  *rval = STRING_TO_JSVAL(ucstr);

  return JS_TRUE;
}

static JSBool JS_DLL_CALLBACK
LocaleToUpperCase(JSContext *cx, JSString *src, jsval *rval)
{
  return ChangeCase(cx, src, rval, ToUpperCase);
}

static JSBool JS_DLL_CALLBACK
LocaleToLowerCase(JSContext *cx, JSString *src, jsval *rval)
{
  return ChangeCase(cx, src, rval, ToLowerCase);
}

static JSBool JS_DLL_CALLBACK
LocaleCompare(JSContext *cx, JSString *src1, JSString *src2, jsval *rval)
{
  nsresult rv;

  if (!gCollation) {
    nsCOMPtr<nsILocaleService> localeService =
      do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);

    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsILocale> locale;
      rv = localeService->GetApplicationLocale(getter_AddRefs(locale));

      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsICollationFactory> colFactory =
          do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID, &rv);

        if (NS_SUCCEEDED(rv)) {
          rv = colFactory->CreateCollation(locale, &gCollation);
        }
      }
    }

    if (NS_FAILED(rv)) {
      nsDOMClassInfo::ThrowJSException(cx, rv);

      return JS_FALSE;
    }
  }

  PRInt32 result;
  rv = gCollation->CompareString(nsICollation::kCollationStrengthDefault,
                                 nsDependentJSString(src1),
                                 nsDependentJSString(src2),
                                 &result);

  if (NS_FAILED(rv)) {
    nsDOMClassInfo::ThrowJSException(cx, rv);

    return JS_FALSE;
  }

  *rval = INT_TO_JSVAL(result);

  return JS_TRUE;
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
      return NS_STATIC_CAST(nsGlobalWindow *,
                            NS_STATIC_CAST(nsPIDOMWindow *, win));
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
#endif

// The number of branch callbacks between calls to JS_MaybeGC
#define MAYBE_GC_BRANCH_COUNT_MASK 0x00000fff // 4095

// The number of branch callbacks before we even check if our start
// timestamp is initialized. This is a fairly low number as we want to
// initialize the timestamp early enough to not waste much time before
// we get there, but we don't want to bother doing this too early as
// it's not generally necessary.
#define INITIALIZE_TIME_BRANCH_COUNT_MASK 0x000000ff // 255

// This function is called after each JS branch execution
JSBool JS_DLL_CALLBACK
nsJSContext::DOMBranchCallback(JSContext *cx, JSScript *script)
{
  // Get the native context
  nsJSContext *ctx = NS_STATIC_CAST(nsJSContext *, ::JS_GetContextPrivate(cx));

  if (!ctx) {
    // Can happen; see bug 355811
    return JS_TRUE;
  }

  PRUint32 callbackCount = ++ctx->mBranchCallbackCount;

  if (callbackCount & INITIALIZE_TIME_BRANCH_COUNT_MASK) {
    return JS_TRUE;
  }

  if (callbackCount == INITIALIZE_TIME_BRANCH_COUNT_MASK + 1 &&
      LL_IS_ZERO(ctx->mBranchCallbackTime)) {
    // Initialize mBranchCallbackTime to start timing how long the
    // script has run
    ctx->mBranchCallbackTime = PR_Now();

    ctx->mIsTrackingChromeCodeTime =
      ::JS_IsSystemObject(cx, ::JS_GetGlobalObject(cx));

    return JS_TRUE;
  }

  if (callbackCount & MAYBE_GC_BRANCH_COUNT_MASK) {
    return JS_TRUE;
  }

  // XXX Save the branch callback time so we can restore it after the GC,
  // because GCing can cause JS to run on our context, causing our
  // ScriptEvaluated to be called, and clearing our branch callback time and
  // count. See bug 302333.
  PRTime callbackTime = ctx->mBranchCallbackTime;

  // Run the GC if we get this far.
  JS_MaybeGC(cx);

  // Now restore the callback time and count, in case they got reset.
  ctx->mBranchCallbackTime = callbackTime;
  ctx->mBranchCallbackCount = callbackCount;

  PRTime now = PR_Now();

  PRTime duration;
  LL_SUB(duration, now, callbackTime);

  // Check the amount of time this script has been running, or if the
  // dialog is disabled.
  if (duration < (ctx->mIsTrackingChromeCodeTime ?
                  sMaxChromeScriptRunTime : sMaxScriptRunTime)) {
    return JS_TRUE;
  }

  // If we get here we're most likely executing an infinite loop in JS,
  // we'll tell the user about this and we'll give the user the option
  // of stopping the execution of the script.
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(ctx->GetGlobalObject()));
  NS_ENSURE_TRUE(win, JS_TRUE);

  nsIDocShell *docShell = win->GetDocShell();
  NS_ENSURE_TRUE(docShell, JS_TRUE);

  nsCOMPtr<nsIInterfaceRequestor> ireq(do_QueryInterface(docShell));
  NS_ENSURE_TRUE(ireq, JS_TRUE);

  // Get the nsIPrompt interface from the docshell
  nsCOMPtr<nsIPrompt> prompt;
  ireq->GetInterface(NS_GET_IID(nsIPrompt), getter_AddRefs(prompt));
  NS_ENSURE_TRUE(prompt, JS_TRUE);

  nsresult rv;

  // Check if we should offer the option to debug
  PRBool debugPossible = (cx->runtime && cx->runtime->debuggerHandler);
#ifdef MOZ_JSDEBUGGER
  // Get the debugger service if necessary.
  if (debugPossible) {
    PRBool jsds_IsOn = PR_FALSE;
    const char jsdServiceCtrID[] = "@mozilla.org/js/jsd/debugger-service;1";
    nsCOMPtr<jsdIExecutionHook> jsdHook;  
    nsCOMPtr<jsdIDebuggerService> jsds = do_GetService(jsdServiceCtrID, &rv);
  
    // Check if there's a user for the debugger service that's 'on' for us
    if (NS_SUCCEEDED(rv)) {
      jsds->GetDebuggerHook(getter_AddRefs(jsdHook));
      jsds->GetIsOn(&jsds_IsOn);
      if (jsds_IsOn) { // If this is not true, the next call would start jsd...
        rv = jsds->OnForRuntime(cx->runtime);
        jsds_IsOn = NS_SUCCEEDED(rv);
      }
    }

    // If there is a debug handler registered for this runtime AND
    // ((jsd is on AND has a hook) OR (jsd isn't on (something else debugs)))
    // then something useful will be done with our request to debug.
    debugPossible = ((jsds_IsOn && (jsdHook != nsnull)) || !jsds_IsOn);
  }
#endif

  // Get localizable strings
  nsCOMPtr<nsIStringBundleService>
    stringService(do_GetService(NS_STRINGBUNDLE_CONTRACTID));
  if (!stringService)
    return JS_TRUE;

  nsCOMPtr<nsIStringBundle> bundle;
  stringService->CreateBundle(kDOMStringBundleURL, getter_AddRefs(bundle));
  if (!bundle)
    return JS_TRUE;
  
  nsXPIDLString title, msg, stopButton, waitButton, debugButton, neverShowDlg;

  rv = bundle->GetStringFromName(NS_LITERAL_STRING("KillScriptTitle").get(),
                                  getter_Copies(title));
  rv |= bundle->GetStringFromName(NS_LITERAL_STRING("StopScriptButton").get(),
                                  getter_Copies(stopButton));
  rv |= bundle->GetStringFromName(NS_LITERAL_STRING("WaitForScriptButton").get(),
                                  getter_Copies(waitButton));
  rv |= bundle->GetStringFromName(NS_LITERAL_STRING("DontAskAgain").get(),
                                  getter_Copies(neverShowDlg));


  if (debugPossible) {
    rv |= bundle->GetStringFromName(NS_LITERAL_STRING("DebugScriptButton").get(),
                                    getter_Copies(debugButton));
    rv |= bundle->GetStringFromName(NS_LITERAL_STRING("KillScriptWithDebugMessage").get(),
                                   getter_Copies(msg));
  }
  else {
    rv |= bundle->GetStringFromName(NS_LITERAL_STRING("KillScriptMessage").get(),
                                   getter_Copies(msg));
  }

  //GetStringFromName can return NS_OK and still give NULL string
  if (NS_FAILED(rv) || !title || !msg || !stopButton || !waitButton ||
      (!debugButton && debugPossible) || !neverShowDlg) {
    NS_ERROR("Failed to get localized strings.");
    return JS_TRUE;
  }

  PRInt32 buttonPressed = 1; //In case user exits dialog by clicking X
  PRBool neverShowDlgChk = PR_FALSE;
  PRUint32 buttonFlags = (nsIPrompt::BUTTON_TITLE_IS_STRING *
                          (nsIPrompt::BUTTON_POS_0 + nsIPrompt::BUTTON_POS_1));

  // Add a third button if necessary:
  if (debugPossible)
    buttonFlags += nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_2;

  // Open the dialog.
  rv = prompt->ConfirmEx(title, msg, buttonFlags, stopButton, waitButton,
                         debugButton, neverShowDlg, &neverShowDlgChk,
                         &buttonPressed);

  if (NS_FAILED(rv) || (buttonPressed == 1)) {
    // Allow the script to continue running

    if (neverShowDlgChk) {
      nsIPrefBranch *prefBranch = nsContentUtils::GetPrefBranch();

      if (prefBranch) {
        prefBranch->SetIntPref(ctx->mIsTrackingChromeCodeTime ?
                               "dom.max_chrome_script_run_time" :
                               "dom.max_script_run_time", 0);
      }
    }

    ctx->mBranchCallbackTime = PR_Now();
    return JS_TRUE;
  }
  else if ((buttonPressed == 2) && debugPossible) {
    // Debug the script
    jsval rval;
    switch(cx->runtime->debuggerHandler(cx, script, cx->fp->pc, &rval, 
                                        cx->runtime->debuggerHandlerData)) {
      case JSTRAP_RETURN:
        cx->fp->rval = rval;
        return JS_TRUE;
      case JSTRAP_ERROR:
        cx->throwing = JS_FALSE;
        return JS_FALSE;
      case JSTRAP_THROW:
        JS_SetPendingException(cx, rval);
        return JS_FALSE;
      case JSTRAP_CONTINUE:
      default:
        return JS_TRUE;
    }
  }

  return JS_FALSE;
}

#define JS_OPTIONS_DOT_STR "javascript.options."

static const char js_options_dot_str[]   = JS_OPTIONS_DOT_STR;
static const char js_strict_option_str[] = JS_OPTIONS_DOT_STR "strict";
static const char js_werror_option_str[] = JS_OPTIONS_DOT_STR "werror";
static const char js_relimit_option_str[] = JS_OPTIONS_DOT_STR "relimit";

int PR_CALLBACK
nsJSContext::JSOptionChangedCallback(const char *pref, void *data)
{
  nsJSContext *context = NS_REINTERPRET_CAST(nsJSContext *, data);
  PRUint32 oldDefaultJSOptions = context->mDefaultJSOptions;
  PRUint32 newDefaultJSOptions = oldDefaultJSOptions;

  PRBool strict = nsContentUtils::GetBoolPref(js_strict_option_str);
  if (strict)
    newDefaultJSOptions |= JSOPTION_STRICT;
  else
    newDefaultJSOptions &= ~JSOPTION_STRICT;

  PRBool werror = nsContentUtils::GetBoolPref(js_werror_option_str);
  if (werror)
    newDefaultJSOptions |= JSOPTION_WERROR;
  else
    newDefaultJSOptions &= ~JSOPTION_WERROR;

  PRBool relimit = nsContentUtils::GetBoolPref(js_relimit_option_str);
  if (relimit)
    newDefaultJSOptions |= JSOPTION_RELIMIT;
  else
    newDefaultJSOptions &= ~JSOPTION_RELIMIT;

  if (newDefaultJSOptions != oldDefaultJSOptions) {
    // Set options only if we used the old defaults; otherwise the page has
    // customized some via the options object and we defer to its wisdom.
    if (::JS_GetOptions(context->mContext) == oldDefaultJSOptions)
      ::JS_SetOptions(context->mContext, newDefaultJSOptions);

    // Save the new defaults for the next page load (InitContext).
    context->mDefaultJSOptions = newDefaultJSOptions;
  }
  return 0;
}

nsJSContext::nsJSContext(JSRuntime *aRuntime) : mGCOnDestruction(PR_TRUE)
{

  ++sContextCount;

  mDefaultJSOptions = JSOPTION_PRIVATE_IS_NSISUPPORTS
                    | JSOPTION_NATIVE_BRANCH_CALLBACK
                    | JSOPTION_ANONFUNFIX
#ifdef DEBUG
                    | JSOPTION_STRICT   // lint catching for development
#endif
    ;

  // Let xpconnect resync its JSContext tracker. We do this before creating
  // a new JSContext just in case the heap manager recycles the JSContext
  // struct.
  nsContentUtils::XPConnect()->SyncJSContexts();

  mContext = ::JS_NewContext(aRuntime, gStackSize);
  if (mContext) {
    ::JS_SetContextPrivate(mContext, NS_STATIC_CAST(nsIScriptContext *, this));

    // Make sure the new context gets the default context options
    ::JS_SetOptions(mContext, mDefaultJSOptions);

    // Watch for the JS boolean options
    nsContentUtils::RegisterPrefCallback(js_options_dot_str,
                                         JSOptionChangedCallback,
                                         this);
    JSOptionChangedCallback(js_options_dot_str, this);

    ::JS_SetBranchCallback(mContext, DOMBranchCallback);

    static JSLocaleCallbacks localeCallbacks =
      {
        LocaleToUpperCase,
        LocaleToLowerCase,
        LocaleCompare,
        LocaleToUnicode
      };

    ::JS_SetLocaleCallbacks(mContext, &localeCallbacks);
  }
  mIsInitialized = PR_FALSE;
  mNumEvaluations = 0;
  mTerminations = nsnull;
  mScriptsEnabled = PR_TRUE;
  mBranchCallbackCount = 0;
  mBranchCallbackTime = LL_ZERO;
  mProcessingScriptTag = PR_FALSE;
  mIsTrackingChromeCodeTime = PR_FALSE;

  InvalidateContextAndWrapperCache();
}

nsJSContext::~nsJSContext()
{
  NS_PRECONDITION(!mTerminations, "Shouldn't have termination funcs by now");
                  
  // Cope with JS_NewContext failure in ctor (XXXbe move NewContext to Init?)
  if (!mContext)
    return;

  // Clear our entry in the JSContext, bugzilla bug 66413
  ::JS_SetContextPrivate(mContext, nsnull);

  // Clear the branch callback, bugzilla bug 238218
  ::JS_SetBranchCallback(mContext, nsnull);

  // Unregister our "javascript.options.*" pref-changed callback.
  nsContentUtils::UnregisterPrefCallback(js_options_dot_str,
                                         JSOptionChangedCallback,
                                         this);

  // Release mGlobalWrapperRef before the context is destroyed
  mGlobalWrapperRef = nsnull;

  // Let xpconnect destroy the JSContext when it thinks the time is right.
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  if (xpc) {
    PRBool do_gc = mGCOnDestruction && !sGCTimer && sReadyForGC;

    xpc->ReleaseJSContext(mContext, !do_gc);
  } else {
    ::JS_DestroyContext(mContext);
  }

  --sContextCount;

  if (!sContextCount && sDidShutdown) {
    // The last context is being deleted, and we're already in the
    // process of shutting down, release the JS runtime service, and
    // the security manager.

    NS_IF_RELEASE(sRuntimeService);
    NS_IF_RELEASE(sSecurityManager);
    NS_IF_RELEASE(gCollation);
    NS_IF_RELEASE(gDecoder);
  }
}

// QueryInterface implementation for nsJSContext
NS_IMPL_CYCLE_COLLECTION_CLASS(nsJSContext)
// XXX Should we call ClearScope here?
NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsJSContext)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsJSContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mGlobalWrapperRef)
  cb.NoteScriptChild(JAVASCRIPT, ::JS_GetGlobalObject(tmp->mContext));
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN(nsJSContext)
  NS_INTERFACE_MAP_ENTRY(nsIScriptContext)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptNotify)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptContext)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsJSContext)
NS_INTERFACE_MAP_END


NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsJSContext, nsIScriptContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsJSContext, nsIScriptContext)

nsresult
nsJSContext::EvaluateStringWithValue(const nsAString& aScript,
                                     void *aScopeObject,
                                     nsIPrincipal *aPrincipal,
                                     const char *aURL,
                                     PRUint32 aLineNo,
                                     PRUint32 aVersion,
                                     void* aRetValue,
                                     PRBool* aIsUndefined)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    if (aIsUndefined) {
      *aIsUndefined = PR_TRUE;
    }

    return NS_OK;
  }

  nsresult rv;
  if (!aScopeObject)
    aScopeObject = ::JS_GetGlobalObject(mContext);

  // Safety first: get an object representing the script's principals, i.e.,
  // the entities who signed this script, or the fully-qualified-domain-name
  // or "codebase" from which it was loaded.
  JSPrincipals *jsprin;
  nsIPrincipal *principal = aPrincipal;
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

  PRBool ok = PR_FALSE;

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

  nsJSContext::TerminationFuncHolder holder(this);

  // SecurityManager said "ok", but don't compile if aVersion is unknown.
  // Do compile with the default version (and avoid thrashing the context's
  // version) if aVersion is the default.
  // As the caller is responsible for parsing the version strings, we just
  // check it isn't JSVERSION_UNKNOWN.
  if (ok && ((JSVersion)aVersion) != JSVERSION_UNKNOWN) {
    // JSVERSION_HAS_XML may be set in our version mask - however, we can't
    // simply pass this directly to JS_SetOptions as it masks out that bit -
    // the only way to make this happen is via JS_SetOptions.
    JSBool hasxml = (aVersion & JSVERSION_HAS_XML) != 0;
    uint32 jsoptions = ::JS_GetOptions(mContext);
    JSBool optionsChanged = ((hasxml) ^ !!(jsoptions & JSOPTION_XML));

    if (optionsChanged) {
      ::JS_SetOptions(mContext,
                      hasxml
                      ? jsoptions | JSOPTION_XML
                      : jsoptions & ~JSOPTION_XML);
    }
    // Change the version - this is cheap when the versions match, so no need
    // to optimize here...
    JSVersion newVer = (JSVersion)(aVersion & JSVERSION_MASK);
    JSVersion oldVer = ::JS_SetVersion(mContext, newVer);
    JSAutoRequest ar(mContext);

    ok = ::JS_EvaluateUCScriptForPrincipals(mContext,
                                            (JSObject *)aScopeObject,
                                            jsprin,
                                            (jschar*)PromiseFlatString(aScript).get(),
                                            aScript.Length(),
                                            aURL,
                                            aLineNo,
                                            &val);

    ::JS_SetVersion(mContext, oldVer);

    if (optionsChanged) {
      ::JS_SetOptions(mContext, jsoptions);
    }

    if (!ok) {
        // Tell XPConnect about any pending exceptions. This is needed
        // to avoid dropping JS exceptions in case we got here through
        // nested calls through XPConnect.

        nsContentUtils::NotifyXPCIfExceptionPending(mContext);
    }
  }

  // Whew!  Finally done with these manually ref-counted things.
  JSPRINCIPALS_DROP(mContext, jsprin);

  // If all went well, convert val to a string (XXXbe unless undefined?).
  if (ok) {
    if (aIsUndefined) {
      *aIsUndefined = JSVAL_IS_VOID(val);
    }

    *NS_STATIC_CAST(jsval*, aRetValue) = val;
    // XXX - nsScriptObjectHolder should be used once this method moves to
    // the new world order. However, use of 'jsval' appears to make this
    // tricky...
  }
  else {
    if (aIsUndefined) {
      *aIsUndefined = PR_TRUE;
    }
  }

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(PR_TRUE);

  return rv;

}

// Helper function to convert a jsval to an nsAString, and set
// exception flags if the conversion fails.
static nsresult
JSValueToAString(JSContext *cx, jsval val, nsAString *result,
                 PRBool *isUndefined)
{
  if (isUndefined) {
    *isUndefined = JSVAL_IS_VOID(val);
  }

  if (!result) {
    return NS_OK;
  }

  JSString* jsstring = ::JS_ValueToString(cx, val);
  if (jsstring) {
    result->Assign(NS_REINTERPRET_CAST(const PRUnichar*,
                                       ::JS_GetStringChars(jsstring)),
                   ::JS_GetStringLength(jsstring));
  } else {
    result->Truncate();

    // We failed to convert val to a string. We're either OOM, or the
    // security manager denied access to .toString(), or somesuch, on
    // an object. Treat this case as if the result were undefined.

    if (isUndefined) {
      *isUndefined = PR_TRUE;
    }

    if (!::JS_IsExceptionPending(cx)) {
      // JS_ValueToString() returned null w/o an exception
      // pending. That means we're OOM.

      return NS_ERROR_OUT_OF_MEMORY;
    }

    // Tell XPConnect about any pending exceptions. This is needed to
    // avoid dropping JS exceptions in case we got here through nested
    // calls through XPConnect.

    nsContentUtils::NotifyXPCIfExceptionPending(cx);
  }

  return NS_OK;
}

nsresult
nsJSContext::EvaluateString(const nsAString& aScript,
                            void *aScopeObject,
                            nsIPrincipal *aPrincipal,
                            const char *aURL,
                            PRUint32 aLineNo,
                            PRUint32 aVersion,
                            nsAString *aRetValue,
                            PRBool* aIsUndefined)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    *aIsUndefined = PR_TRUE;

    if (aRetValue) {
      aRetValue->Truncate();
    }

    return NS_OK;
  }

  nsresult rv;
  if (!aScopeObject)
    aScopeObject = ::JS_GetGlobalObject(mContext);

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
      do_QueryInterface(GetGlobalObject(), &rv);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    principal = objPrincipal->GetPrincipal();
    if (!principal)
      return NS_ERROR_FAILURE;
    principal->GetJSPrincipals(mContext, &jsprin);
  }

  // From here on, we must JSPRINCIPALS_DROP(jsprin) before returning...

  PRBool ok = PR_FALSE;

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

  // The result of evaluation, used only if there were no errors.  This need
  // not be a GC root currently, provided we run the GC only from the branch
  // callback or from ScriptEvaluated.  TODO: use JS_Begin/EndRequest to keep
  // the GC from racing with JS execution on any thread.
  jsval val;

  nsJSContext::TerminationFuncHolder holder(this);

  // SecurityManager said "ok", but don't compile if aVersion is unknown.
  // Do compile with the default version (and avoid thrashing the context's
  // version) if aVersion is the default.
  // As the caller is responsible for parsing the version strings, we just
  // check it isn't JSVERSION_UNKNOWN.
  if (ok && ((JSVersion)aVersion) != JSVERSION_UNKNOWN) {
    JSAutoRequest ar(mContext);
    // JSVERSION_HAS_XML may be set in our version mask - however, we can't
    // simply pass this directly to JS_SetOptions as it masks out that bit -
    // the only way to make this happen is via JS_SetOptions.
    JSBool hasxml = (aVersion & JSVERSION_HAS_XML) != 0;
    uint32 jsoptions = ::JS_GetOptions(mContext);
    JSBool optionsChanged = ((hasxml) ^ !!(jsoptions & JSOPTION_XML));

    if (optionsChanged) {
      ::JS_SetOptions(mContext,
                      hasxml
                      ? jsoptions | JSOPTION_XML
                      : jsoptions & ~JSOPTION_XML);
    }
    // Change the version - this is cheap when the versions match, so no need
    // to optimize here...
    JSVersion newVer = (JSVersion)(aVersion & JSVERSION_MASK);
    JSVersion oldVer = ::JS_SetVersion(mContext, newVer);

    ok = ::JS_EvaluateUCScriptForPrincipals(mContext,
                                              (JSObject *)aScopeObject,
                                              jsprin,
                                              (jschar*)PromiseFlatString(aScript).get(),
                                              aScript.Length(),
                                              aURL,
                                              aLineNo,
                                              &val);

    ::JS_SetVersion(mContext, oldVer);

    if (optionsChanged) {
      ::JS_SetOptions(mContext, jsoptions);
    }

    if (!ok) {
        // Tell XPConnect about any pending exceptions. This is needed
        // to avoid dropping JS exceptions in case we got here through
        // nested calls through XPConnect.

        nsContentUtils::NotifyXPCIfExceptionPending(mContext);
    }
  }

  // Whew!  Finally done with these manually ref-counted things.
  JSPRINCIPALS_DROP(mContext, jsprin);

  // If all went well, convert val to a string (XXXbe unless undefined?).
  if (ok) {
    JSAutoRequest ar(mContext);
    rv = JSValueToAString(mContext, val, aRetValue, aIsUndefined);
  }
  else {
    if (aIsUndefined) {
      *aIsUndefined = PR_TRUE;
    }

    if (aRetValue) {
      aRetValue->Truncate();
    }
  }

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(PR_TRUE);

  return rv;
}

nsresult
nsJSContext::CompileScript(const PRUnichar* aText,
                           PRInt32 aTextLength,
                           void *aScopeObject,
                           nsIPrincipal *aPrincipal,
                           const char *aURL,
                           PRUint32 aLineNo,
                           PRUint32 aVersion,
                           nsScriptObjectHolder &aScriptObject)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  nsresult rv;
  NS_ENSURE_ARG_POINTER(aPrincipal);

  if (!aScopeObject)
    aScopeObject = ::JS_GetGlobalObject(mContext);

  JSPrincipals *jsprin;
  aPrincipal->GetJSPrincipals(mContext, &jsprin);
  // From here on, we must JSPRINCIPALS_DROP(jsprin) before returning...

  PRBool ok = PR_FALSE;

  rv = sSecurityManager->CanExecuteScripts(mContext, aPrincipal, &ok);
  if (NS_FAILED(rv)) {
    JSPRINCIPALS_DROP(mContext, jsprin);
    return NS_ERROR_FAILURE;
  }

  aScriptObject.drop(); // ensure old object not used on failure...

  // SecurityManager said "ok", but don't compile if aVersion is unknown.
  // Do compile with the default version (and avoid thrashing the context's
  // version) if aVersion is the default.
  // As the caller is responsible for parsing the version strings, we just
  // check it isn't JSVERSION_UNKNOWN.
  if (ok && ((JSVersion)aVersion) != JSVERSION_UNKNOWN) {
    // JSVERSION_HAS_XML may be set in our version mask - however, we can't
    // simply pass this directly to JS_SetOptions as it masks out that bit -
    // the only way to make this happen is via JS_SetOptions.
    JSAutoRequest ar(mContext);
    JSBool hasxml = (aVersion & JSVERSION_HAS_XML) != 0;
    uint32 jsoptions = ::JS_GetOptions(mContext);
    JSBool optionsChanged = ((hasxml) ^ !!(jsoptions & JSOPTION_XML));

    if (optionsChanged) {
      ::JS_SetOptions(mContext,
                      hasxml
                      ? jsoptions | JSOPTION_XML
                      : jsoptions & ~JSOPTION_XML);
    }
    // Change the version - this is cheap when the versions match, so no need
    // to optimize here...
    JSVersion newVer = (JSVersion)(aVersion & JSVERSION_MASK);
    JSVersion oldVer = ::JS_SetVersion(mContext, newVer);

    JSScript* script =
        ::JS_CompileUCScriptForPrincipals(mContext,
                                          (JSObject *)aScopeObject,
                                          jsprin,
                                          (jschar*) aText,
                                          aTextLength,
                                          aURL,
                                          aLineNo);
    if (script) {
      JSObject *scriptObject = ::JS_NewScriptObject(mContext, script);
      if (scriptObject) {
        NS_ASSERTION(aScriptObject.getScriptTypeID()==JAVASCRIPT,
                     "Expecting JS script object holder");
        rv = aScriptObject.set(scriptObject);
      } else {
        ::JS_DestroyScript(mContext, script);
        script = nsnull;
      }
    } else
      rv = NS_ERROR_OUT_OF_MEMORY;

    ::JS_SetVersion(mContext, oldVer);

    if (optionsChanged) {
      ::JS_SetOptions(mContext, jsoptions);
    }
  }

  // Whew!  Finally done.
  JSPRINCIPALS_DROP(mContext, jsprin);
  return rv;
}

nsresult
nsJSContext::ExecuteScript(void *aScriptObject,
                           void *aScopeObject,
                           nsAString* aRetValue,
                           PRBool* aIsUndefined)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    if (aIsUndefined) {
      *aIsUndefined = PR_TRUE;
    }

    if (aRetValue) {
      aRetValue->Truncate();
    }

    return NS_OK;
  }

  nsresult rv;

  if (!aScopeObject)
    aScopeObject = ::JS_GetGlobalObject(mContext);

  // Push our JSContext on our thread's context stack, in case native code
  // called from JS calls back into JS via XPConnect.
  nsCOMPtr<nsIJSContextStack> stack =
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext))) {
    return NS_ERROR_FAILURE;
  }

  // The result of evaluation, used only if there were no errors.  This need
  // not be a GC root currently, provided we run the GC only from the branch
  // callback or from ScriptEvaluated.  TODO: use JS_Begin/EndRequest to keep
  // the GC from racing with JS execution on any thread.
  jsval val;
  JSBool ok;

  nsJSContext::TerminationFuncHolder holder(this);
  JSAutoRequest ar(mContext);
  ok = ::JS_ExecuteScript(mContext,
                          (JSObject *)aScopeObject,
                          (JSScript*) ::JS_GetPrivate(mContext,
                          (JSObject*)aScriptObject),
                          &val);

  if (ok) {
    // If all went well, convert val to a string (XXXbe unless undefined?).
    rv = JSValueToAString(mContext, val, aRetValue, aIsUndefined);
  } else {
    if (aIsUndefined) {
      *aIsUndefined = PR_TRUE;
    }

    if (aRetValue) {
      aRetValue->Truncate();
    }

    // Tell XPConnect about any pending exceptions. This is needed to
    // avoid dropping JS exceptions in case we got here through nested
    // calls through XPConnect.

    nsContentUtils::NotifyXPCIfExceptionPending(mContext);
  }

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(PR_TRUE);

  return rv;
}


static inline const char *
AtomToEventHandlerName(nsIAtom *aName)
{
  const char *name;

  aName->GetUTF8String(&name);

#ifdef DEBUG
  const char *cp;
  char c;
  for (cp = name; *cp != '\0'; ++cp)
  {
    c = *cp;
    NS_ASSERTION (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z'),
                  "non-ASCII non-alphabetic event handler name");
  }
#endif

  return name;
}

// Helper function to find the JSObject associated with a (presumably DOM)
// interface.
nsresult
nsJSContext::JSObjectFromInterface(nsISupports* aTarget, void *aScope, JSObject **aRet)
{
  if (!aTarget) { // no target specified is ok
      *aRet = nsnull;
      return NS_OK;
  }
  // Get the jsobject associated with this target
  nsresult rv;
  nsCOMPtr<nsIXPConnectJSObjectHolder> jsholder;
  rv = nsContentUtils::XPConnect()->WrapNative(mContext, (JSObject *)aScope,
                                               aTarget,
                                               NS_GET_IID(nsISupports),
                                               getter_AddRefs(jsholder));
  NS_ENSURE_SUCCESS(rv, rv);
#ifdef NS_DEBUG
  nsCOMPtr<nsIXPConnectWrappedNative> wrapper = do_QueryInterface(jsholder);
  NS_ASSERTION(wrapper, "wrapper must impl nsIXPConnectWrappedNative");
  nsCOMPtr<nsISupports> targetSupp = do_QueryInterface(aTarget);
  NS_ASSERTION(wrapper->Native() == targetSupp, "Native should be the target!");
#endif
  return jsholder->GetJSObject(aRet);
}


nsresult
nsJSContext::CompileEventHandler(nsIAtom *aName,
                                 PRUint32 aArgCount,
                                 const char** aArgNames,
                                 const nsAString& aBody,
                                 const char *aURL, PRUint32 aLineNo,
                                 nsScriptObjectHolder &aHandler)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!sSecurityManager) {
    NS_ERROR("Huh, we need a script security manager to compile "
             "an event handler!");

    return NS_ERROR_UNEXPECTED;
  }

  const char *charName = AtomToEventHandlerName(aName);

  // Event handlers are always shared, and must be bound before use.
  // Therefore we never bother compiling with principals.
  // (that probably means we should avoid JS_CompileUCFunctionForPrincipals!)
  JSAutoRequest ar(mContext);
  JSFunction* fun =
      ::JS_CompileUCFunctionForPrincipals(mContext,
                                          nsnull, nsnull,
                                          charName, aArgCount, aArgNames,
                                          (jschar*)PromiseFlatString(aBody).get(),
                                          aBody.Length(),
                                          aURL, aLineNo);

  if (!fun) {
    return NS_ERROR_FAILURE;
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
                             PRBool aShared,
                             void** aFunctionObject)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

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
      ::JS_CompileUCFunctionForPrincipals(mContext,
                                          aShared ? nsnull : target, jsprin,
                                          PromiseFlatCString(aName).get(),
                                          aArgCount, aArgArray,
                                          (jschar*)PromiseFlatString(aBody).get(),
                                          aBody.Length(),
                                          aURL, aLineNo);

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
nsJSContext::CallEventHandler(nsISupports* aTarget, void *aScope, void *aHandler,
                              nsIArray *aargv, nsIVariant **arv)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    return NS_OK;
  }
  nsresult rv;
  JSObject* target = nsnull;
  nsAutoGCRoot root(&target, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = JSObjectFromInterface(aTarget, aScope, &target);
  NS_ENSURE_SUCCESS(rv, rv);

  jsval rval = JSVAL_VOID;

  // This one's a lot easier than EvaluateString because we don't have to
  // hassle with principals: they're already compiled into the JS function.
  // xxxmarkh - this comment is no longer true - principals are not used at
  // all now, and never were in some cases.

  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext)))
    return NS_ERROR_FAILURE;

  // check if the event handler can be run on the object in question
  rv = sSecurityManager->CheckFunctionAccess(mContext, aHandler, target);
  if (NS_SUCCEEDED(rv)) {
    // We're not done yet!  Some event listeners are confused about their
    // script context, so check whether we might actually be the wrong script
    // context.  To be safe, do CheckFunctionAccess checks for both.
    nsCOMPtr<nsIContent> content = do_QueryInterface(aTarget);
    if (content) {
      // XXXbz XBL2/sXBL issue
      nsIDocument* ownerDoc = content->GetOwnerDoc();
      if (ownerDoc) {
        nsIScriptGlobalObject* global = ownerDoc->GetScriptGlobalObject();
        if (global) {
          nsIScriptContext* context =
            global->GetScriptContext(JAVASCRIPT);
          if (context && context != this) {
            JSContext* cx =
              NS_STATIC_CAST(JSContext*, context->GetNativeContext());
            rv = stack->Push(cx);
            if (NS_SUCCEEDED(rv)) {
              rv = sSecurityManager->CheckFunctionAccess(cx, aHandler,
                                                         target);
              // Here we lose no matter what; we don't want to leave the wrong
              // cx on the stack.  I guess default to leaving mContext, to
              // cover those cases when we really do have a different context
              // for the handler and the node.  That's probably safer.
              if (NS_FAILED(stack->Pop(nsnull))) {
                return NS_ERROR_FAILURE;
              }
            }
          }
        }
      }
    }
  }

  nsJSContext::TerminationFuncHolder holder(this);

  if (NS_SUCCEEDED(rv)) {
    // Convert args to jsvals.
    void *mark;
    PRUint32 argc = 0;
    jsval *argv = nsnull;

    // Use |target| as the scope for wrapping the arguments, since aScope is
    // the safe scope in many cases, which isn't very useful.  Wrapping aTarget
    // was OK because those typically have PreCreate methods that give them the
    // right scope anyway, and we want to make sure that the arguments end up
    // in the same scope as aTarget.
    rv = ConvertSupportsTojsvals(aargv, target, &argc,
                                 NS_REINTERPRET_CAST(void **, &argv), &mark);
    if (NS_FAILED(rv)) {
      stack->Pop(nsnull);
      return rv;
    }
  
    AutoFreeJSStack stackGuard(mContext, mark); // ensure always freed.

    jsval funval = OBJECT_TO_JSVAL(aHandler);
    JSAutoRequest ar(mContext);
    PRBool ok = ::JS_CallFunctionValue(mContext, target,
                                       funval, argc, argv, &rval);

    if (!ok) {
      // Tell XPConnect about any pending exceptions. This is needed
      // to avoid dropping JS exceptions in case we got here through
      // nested calls through XPConnect.

      nsContentUtils::NotifyXPCIfExceptionPending(mContext);

      // Don't pass back results from failed calls.
      rval = JSVAL_VOID;

      // Tell the caller that the handler threw an error.
      rv = NS_ERROR_FAILURE;
    }
  }

  if (NS_FAILED(stack->Pop(nsnull)))
    return NS_ERROR_FAILURE;

  // Convert to variant before calling ScriptEvaluated, as it may GC, meaning
  // we would need to root rval.
  JSAutoRequest ar(mContext);
  if (NS_SUCCEEDED(rv)) {
    rv = nsContentUtils::XPConnect()->JSToVariant(mContext, rval, arv);
  }

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(PR_TRUE);

  return rv;
}

nsresult
nsJSContext::BindCompiledEventHandler(nsISupports* aTarget, void *aScope,
                                      nsIAtom *aName,
                                      void *aHandler)
{
  NS_ENSURE_ARG(aHandler);
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  const char *charName = AtomToEventHandlerName(aName);
  nsresult rv;

  // Get the jsobject associated with this target
  JSObject *target = nsnull;
  nsAutoGCRoot root(&target, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = JSObjectFromInterface(aTarget, aScope, &target);
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject *funobj = (JSObject*) aHandler;

  NS_ASSERTION(JS_TypeOfValue(mContext, OBJECT_TO_JSVAL(funobj)) == JSTYPE_FUNCTION,
               "Event handler object not a function");

  // Push our JSContext on our thread's context stack, in case native code
  // called from JS calls back into JS via XPConnect.
  nsCOMPtr<nsIJSContextStack> stack =
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext))) {
    return NS_ERROR_FAILURE;
  }

  JSAutoRequest ar(mContext);

  // Make sure the handler function is parented by its event target object
  if (funobj) { // && ::JS_GetParent(mContext, funobj) != target) {
    funobj = ::JS_CloneFunctionObject(mContext, funobj, target);
    if (!funobj)
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_SUCCEEDED(rv) &&
      // Make sure the flags here match those in nsEventReceiverSH::NewResolve
      !::JS_DefineProperty(mContext, target, charName,
                           OBJECT_TO_JSVAL(funobj), nsnull, nsnull,
                           JSPROP_ENUMERATE | JSPROP_PERMANENT)) {
    rv = NS_ERROR_FAILURE;
  }

  // XXXmarkh - ideally we should assert that the wrapped native is now
  // "long lived" - how to do that?

  if (NS_FAILED(stack->Pop(nsnull)) && NS_SUCCEEDED(rv)) {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

nsresult
nsJSContext::GetBoundEventHandler(nsISupports* aTarget, void *aScope,
                                  nsIAtom* aName,
                                  nsScriptObjectHolder &aHandler)
{
    nsresult rv;
    JSObject *obj = nsnull;
    nsAutoGCRoot root(&obj, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    JSAutoRequest ar(mContext);
    rv = JSObjectFromInterface(aTarget, aScope, &obj);
    NS_ENSURE_SUCCESS(rv, rv);

    const char *charName = AtomToEventHandlerName(aName);

    jsval funval;
    if (!JS_LookupProperty(mContext, obj, 
                             charName, &funval))
        return NS_ERROR_FAILURE;

    if (JS_TypeOfValue(mContext, funval) != JSTYPE_FUNCTION) {
        NS_WARNING("Event handler object not a function");
        aHandler.drop();
        return NS_OK;
    }
    NS_ASSERTION(aHandler.getScriptTypeID()==JAVASCRIPT,
                 "Expecting JS script object holder");
    return aHandler.set(JSVAL_TO_OBJECT(funval));
}

// serialization
nsresult
nsJSContext::Serialize(nsIObjectOutputStream* aStream, void *aScriptObject)
{
    JSObject *mJSObject = (JSObject *)aScriptObject;
    if (!mJSObject)
        return NS_ERROR_FAILURE;

    nsresult rv;

    JSContext* cx = mContext;
    JSXDRState *xdr = ::JS_XDRNewMem(cx, JSXDR_ENCODE);
    if (! xdr)
        return NS_ERROR_OUT_OF_MEMORY;
    xdr->userdata = (void*) aStream;

    JSAutoRequest ar(cx);
    JSScript *script = NS_REINTERPRET_CAST(JSScript*,
                                           ::JS_GetPrivate(cx, mJSObject));
    if (! ::JS_XDRScript(xdr, &script)) {
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
        const char* data = NS_REINTERPRET_CAST(const char*,
                                               ::JS_XDRMemGetData(xdr, &size));
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
    JSObject *result = nsnull;
    nsresult rv;

    NS_TIMELINE_MARK_FUNCTION("js script deserialize");

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

        JSScript *script = nsnull;
        if (! ::JS_XDRScript(xdr, &script)) {
            rv = NS_ERROR_FAILURE;  // principals deserialization error?
        } else {
            result = ::JS_NewScriptObject(cx, script);
            if (! result) {
                rv = NS_ERROR_OUT_OF_MEMORY;    // certain error
                ::JS_DestroyScript(cx, script);
            }
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
    NS_WARNING("Context has no global.");
    return nsnull;
  }

  JSClass *c = JS_GET_CLASS(mContext, global);

  if (!c || ((~c->flags) & (JSCLASS_HAS_PRIVATE |
                            JSCLASS_PRIVATE_IS_NSISUPPORTS))) {
    NS_WARNING("Global is not an nsISupports.");
    return nsnull;
  }

  nsCOMPtr<nsIScriptGlobalObject> sgo;
  nsISupports *priv =
    (nsISupports *)::JS_GetPrivate(mContext, global);

  nsCOMPtr<nsIXPConnectWrappedNative> wrapped_native =
    do_QueryInterface(priv);

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

void *
nsJSContext::GetNativeGlobal()
{
    return ::JS_GetGlobalObject(mContext);
}

nsresult
nsJSContext::CreateNativeGlobalForInner(
                                nsIScriptGlobalObject *aNewInner,
                                PRBool aIsChrome,
                                void **aNativeGlobal, nsISupports **aHolder)
{
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  PRUint32 flags = aIsChrome? nsIXPConnect::FLAG_SYSTEM_GLOBAL_OBJECT : 0;
  nsCOMPtr<nsIXPConnectJSObjectHolder> jsholder;
  nsresult rv = xpc->
          InitClassesWithNewWrappedGlobal(mContext,
                                          aNewInner, NS_GET_IID(nsISupports),
                                          flags,
                                          getter_AddRefs(jsholder));
  if (NS_FAILED(rv))
    return rv;
  jsholder->GetJSObject(NS_REINTERPRET_CAST(JSObject **, aNativeGlobal));
  *aHolder = jsholder.get();
  NS_ADDREF(*aHolder);
  return NS_OK;
}

nsresult
nsJSContext::ConnectToInner(nsIScriptGlobalObject *aNewInner, void *aOuterGlobal)
{
  NS_ENSURE_ARG(aNewInner);
  JSObject *newInnerJSObject = (JSObject *)aNewInner->GetScriptGlobal(JAVASCRIPT);
  JSObject *myobject = (JSObject *)aOuterGlobal;

  // *Don't* call JS_ClearScope here since it's unnecessary
  // and it confuses the JS engine as to which Function is
  // on which window. See bug 343966.

  // Make the inner and outer window both share the same
  // prototype. The prototype we share is the outer window's
  // prototype, this way XPConnect can still find the wrapper to
  // use when making a call like alert() (w/o qualifying it with
  // "window."). XPConnect looks up the wrapper based on the
  // function object's parent, which is the object the function
  // was called on, and when calling alert() we'll be calling the
  // alert() function from the outer window's prototype off of the
  // inner window. In this case XPConnect is able to find the
  // outer (through the JSExtendedClass hook outerObject), so this
  // prototype sharing works.

  // We do *not* want to use anything else out of the outer
  // object's prototype chain than the first prototype, which is
  // the XPConnect prototype. The rest we want from the inner
  // window's prototype, i.e. the global scope polluter and
  // Object.prototype. This way the outer also gets the benefits
  // of the global scope polluter, and the inner window's
  // Object.prototype.
  JSObject *proto = ::JS_GetPrototype(mContext, myobject);
  JSObject *innerProto = ::JS_GetPrototype(mContext, newInnerJSObject);
  JSObject *innerProtoProto = ::JS_GetPrototype(mContext, innerProto);

  ::JS_SetPrototype(mContext, newInnerJSObject, proto);
  ::JS_SetPrototype(mContext, proto, innerProtoProto);
  return NS_OK;
}

void *
nsJSContext::GetNativeContext()
{
  return mContext;
}

const JSClass* NS_DOMClassInfo_GetXPCNativeWrapperClass();
void NS_DOMClassInfo_SetXPCNativeWrapperClass(JSClass* aClass);

nsresult
nsJSContext::InitContext(nsIScriptGlobalObject *aGlobalObject)
{
  // Make sure callers of this use
  // WillInitializeContext/DidInitializeContext around this call.
  NS_ENSURE_TRUE(!mIsInitialized, NS_ERROR_ALREADY_INITIALIZED);

  if (!mContext)
    return NS_ERROR_OUT_OF_MEMORY;

  InvalidateContextAndWrapperCache();

  nsresult rv;

  if (!gNameSpaceManager) {
    gNameSpaceManager = new nsScriptNameSpaceManager;
    NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_OUT_OF_MEMORY);

    rv = gNameSpaceManager->Init();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  ::JS_SetErrorReporter(mContext, NS_ScriptErrorReporter);

  if (!aGlobalObject) {
    // If we don't get a global object then there's nothing more to do here.

    return NS_OK;
  }

  nsIXPConnect *xpc = nsContentUtils::XPConnect();

  JSObject *global = ::JS_GetGlobalObject(mContext);

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

  // If there's already a global object in mContext we won't tell
  // XPConnect to wrap aGlobalObject since it's already wrapped.

  if (!global) {
    nsCOMPtr<nsIDOMChromeWindow> chromeWindow(do_QueryInterface(aGlobalObject));
    PRUint32 flags = 0;
    
    if (chromeWindow) {
      // Flag this object and scripts compiled against it as "system", for
      // optional automated XPCNativeWrapper construction when chrome views
      // a content DOM.
      flags = nsIXPConnect::FLAG_SYSTEM_GLOBAL_OBJECT;

      // Always enable E4X for XUL and other chrome content -- there is no
      // need to preserve the <!-- script hiding hack from JS-in-HTML daze
      // (introduced in 1995 for graceful script degradation in Netscape 1,
      // Mosaic, and other pre-JS browsers).
      ::JS_SetOptions(mContext, ::JS_GetOptions(mContext) | JSOPTION_XML);
    }

    rv = xpc->InitClassesWithNewWrappedGlobal(mContext, aGlobalObject,
                                              NS_GET_IID(nsISupports),
                                              flags,
                                              getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);

    // Now check whether we need to grab a pointer to the
    // XPCNativeWrapper class
    if (!NS_DOMClassInfo_GetXPCNativeWrapperClass()) {
      JSAutoRequest ar(mContext);
      rv = FindXPCNativeWrapperClass(holder);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else {
    // If there's already a global object in mContext we're called
    // after ::JS_ClearScope() was called. We'll have to tell
    // XPConnect to re-initialize the global object to do things like
    // define the Components object on the global again and forget all
    // old prototypes in this scope.
    rv = xpc->InitClasses(mContext, global);
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIClassInfo> ci(do_QueryInterface(aGlobalObject));

    if (ci) {
      rv = xpc->WrapNative(mContext, global, aGlobalObject,
                           NS_GET_IID(nsISupports),
                           getter_AddRefs(holder));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIXPConnectWrappedNative> wrapper(do_QueryInterface(holder));
      NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

      rv = wrapper->RefreshPrototype();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Hold a strong reference to the wrapper for the global to avoid
  // rooting and unrooting the global object every time its AddRef()
  // or Release() methods are called
  mGlobalWrapperRef = holder;

  holder->GetJSObject(&global);

  rv = InitClasses(global); // this will complete global object initialization
  NS_ENSURE_SUCCESS(rv, rv);

  return rv;
}

nsresult
nsJSContext::InitializeExternalClasses()
{
  NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  return gNameSpaceManager->InitForContext(this);
}

nsresult
nsJSContext::InitializeLiveConnectClasses(JSObject *aGlobalObj)
{
  nsresult rv = NS_OK;

#ifdef OJI
  nsCOMPtr<nsIJVMManager> jvmManager =
    do_GetService(nsIJVMManager::GetCID(), &rv);

  if (NS_SUCCEEDED(rv) && jvmManager) {
    PRBool javaEnabled = PR_FALSE;

    rv = jvmManager->GetJavaEnabled(&javaEnabled);

    if (NS_SUCCEEDED(rv) && javaEnabled) {
      nsCOMPtr<nsILiveConnectManager> liveConnectManager =
        do_QueryInterface(jvmManager);

      if (liveConnectManager) {
        JSAutoRequest ar(mContext);
        rv = liveConnectManager->InitLiveConnectClasses(mContext, aGlobalObj);
      }
    }
  }
#endif /* OJI */

  // return all is well until things are stable.
  return NS_OK;
}

nsresult
nsJSContext::SetProperty(void *aTarget, const char *aPropName, nsISupports *aArgs)
{
  PRUint32  argc;
  jsval    *argv = nsnull;
  void *mark;

  JSAutoRequest ar(mContext);
  
  nsresult rv;
  rv = ConvertSupportsTojsvals(aArgs, GetNativeGlobal(), &argc,
                               NS_REINTERPRET_CAST(void **, &argv), &mark);
  NS_ENSURE_SUCCESS(rv, rv);
  AutoFreeJSStack stackGuard(mContext, mark); // ensure always freed.

  // got the array, now attach it.
  JSObject *args = ::JS_NewArrayObject(mContext, argc, argv);
  jsval vargs = OBJECT_TO_JSVAL(args);

  rv = ::JS_SetProperty(mContext, NS_REINTERPRET_CAST(JSObject *, aTarget), aPropName, &vargs) ?
       NS_OK : NS_ERROR_FAILURE;
  // free 'args'???

  return rv;
}

nsresult
nsJSContext::ConvertSupportsTojsvals(nsISupports *aArgs,
                                     void *aScope,
                                     PRUint32 *aArgc, void **aArgv,
                                     void **aMarkp)
{
  nsresult rv = NS_OK;

  // If the array implements nsIJSArgArray, just grab the values directly.
  nsCOMPtr<nsIJSArgArray> fastArray = do_QueryInterface(aArgs);
  if (fastArray != nsnull) {
    *aMarkp = nsnull;
    return fastArray->GetArgs(aArgc, aArgv);
  }
  // Take the slower path converting each item.
  // Handle only nsIArray and nsIVariant.  nsIArray is only needed for
  // SetProperty('arguments', ...);

  *aArgv = nsnull;
  *aArgc = 0;
  *aMarkp = nsnull;

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

  jsval *argv = js_AllocStack(mContext, argCount, aMarkp);
  NS_ENSURE_TRUE(argv, NS_ERROR_OUT_OF_MEMORY);

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
        rv = xpc->VariantToJS(mContext, (JSObject *)aScope, variant, 
                              thisval);
      } else {
        // And finally, support the nsISupportsPrimitives supplied
        // by the AppShell.  It generally will pass only strings, but
        // as we have code for handling all, we may as well use it.
        rv = AddSupportsPrimitiveTojsvals(arg, thisval);
        if (rv == NS_ERROR_NO_INTERFACE) {
          // something else - probably an event object or similar - just
          // just wrap it.
#ifdef NS_DEBUG
          // but first, check its not another nsISupportsPrimitive, as
          // these are now deprecated for use with script contexts.
          nsCOMPtr<nsISupportsPrimitive> prim(do_QueryInterface(arg));
          NS_ASSERTION(prim == nsnull,
                       "Don't pass nsISupportsPrimitives - use nsIVariant!");
#endif
          nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
          rv = xpc->WrapNative(mContext, (JSObject *)aScope, arg,
                               NS_GET_IID(nsISupports),
                               getter_AddRefs(wrapper));
          if (NS_SUCCEEDED(rv)) {
            JSObject *obj;
            rv = wrapper->GetJSObject(&obj);
            if (NS_SUCCEEDED(rv)) {
              *thisval = OBJECT_TO_JSVAL(obj);
            }
          }
        }
      }
    }
  } else {
    nsCOMPtr<nsIVariant> variant(do_QueryInterface(aArgs));
    if (variant)
      rv = xpc->VariantToJS(mContext, (JSObject *)aScope, variant, argv);
    else {
      NS_ERROR("Not an array, not an interface?");
      rv = NS_ERROR_UNEXPECTED;
    }
  }
  if (NS_FAILED(rv)) {
    js_FreeStack(mContext, *aMarkp);
    return rv;
  }
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
                              NS_REINTERPRET_CAST(const jschar *,data.get()),
                              data.Length());
      NS_ENSURE_TRUE(str, NS_ERROR_OUT_OF_MEMORY);

      *aArgv = STRING_TO_JSVAL(str);
      break;
    }
    case nsISupportsPrimitive::TYPE_PRBOOL : {
      nsCOMPtr<nsISupportsPRBool> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      PRBool data;

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

      jsdouble *d = ::JS_NewDouble(cx, data);

      *aArgv = DOUBLE_TO_JSVAL(d);

      break;
    }
    case nsISupportsPrimitive::TYPE_DOUBLE : {
      nsCOMPtr<nsISupportsDouble> p(do_QueryInterface(argPrimitive));
      NS_ENSURE_TRUE(p, NS_ERROR_UNEXPECTED);

      double data;

      p->GetData(&data);

      jsdouble *d = ::JS_NewDouble(cx, data);

      *aArgv = DOUBLE_TO_JSVAL(d);

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

      nsresult rv;
      nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIXPConnectJSObjectHolder> wrapper;
      rv = xpc->WrapNative(cx, ::JS_GetGlobalObject(cx), data,
                           *iid, getter_AddRefs(wrapper));
      NS_ENSURE_SUCCESS(rv, rv);

      JSObject *obj;
      rv = wrapper->GetJSObject(&obj);
      NS_ENSURE_SUCCESS(rv, rv);

      *aArgv = OBJECT_TO_JSVAL(obj);

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

nsresult
nsJSContext::FindXPCNativeWrapperClass(nsIXPConnectJSObjectHolder *aHolder)
{
  NS_ASSERTION(!NS_DOMClassInfo_GetXPCNativeWrapperClass(),
               "Why was this called?");

  JSObject *globalObj;
  aHolder->GetJSObject(&globalObj);
  NS_ASSERTION(globalObj, "Must have global by now!");
      
  const char* arg = "arg";
  NS_NAMED_LITERAL_STRING(body, "return new XPCNativeWrapper(arg);");

  // Can't use CompileFunction() here because our principal isn't
  // inited yet and a null principal makes it fail.
  JSFunction *fun =
    ::JS_CompileUCFunction(mContext,
                           globalObj,
                           "_XPCNativeWrapperCtor",
                           1, &arg,
                           (jschar*)body.get(),
                           body.Length(),
                           "javascript:return new XPCNativeWrapper(arg);",
                           1 // lineno
                           );
  NS_ENSURE_TRUE(fun, NS_ERROR_FAILURE);

  jsval globalVal = OBJECT_TO_JSVAL(globalObj);
  jsval wrapper;
      
  JSBool ok = ::JS_CallFunction(mContext, globalObj, fun,
                                1, &globalVal, &wrapper);
  if (!ok) {
    // No need to notify about pending exceptions here; we don't
    // expect any other than out of memory, really.
    return NS_ERROR_FAILURE;
  }

  NS_ASSERTION(JSVAL_IS_OBJECT(wrapper), "This should be an object!");

  NS_DOMClassInfo_SetXPCNativeWrapperClass(
    ::JS_GetClass(mContext, JSVAL_TO_OBJECT(wrapper)));
  return NS_OK;
}

static JSPropertySpec OptionsProperties[] = {
  {"strict",    (int8)JSOPTION_STRICT,   JSPROP_ENUMERATE | JSPROP_PERMANENT},
  {"werror",    (int8)JSOPTION_WERROR,   JSPROP_ENUMERATE | JSPROP_PERMANENT},
  {"relimit",   (int8)JSOPTION_RELIMIT,  JSPROP_ENUMERATE | JSPROP_PERMANENT},
  {0}
};

static JSBool JS_DLL_CALLBACK
GetOptionsProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  if (JSVAL_IS_INT(id)) {
    uint32 optbit = (uint32) JSVAL_TO_INT(id);
    if ((optbit & (optbit - 1)) == 0 && optbit <= JSOPTION_WERROR)
      *vp = (JS_GetOptions(cx) & optbit) ? JSVAL_TRUE : JSVAL_FALSE;
  }
  return JS_TRUE;
}

static JSBool JS_DLL_CALLBACK
SetOptionsProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  if (JSVAL_IS_INT(id)) {
    uint32 optbit = (uint32) JSVAL_TO_INT(id);

    // Don't let options other than strict, werror, or relimit be set -- it
    // would be bad if web page script could clear
    // JSOPTION_PRIVATE_IS_NSISUPPORTS!
    if (((optbit & (optbit - 1)) == 0 && optbit <= JSOPTION_WERROR) ||
        optbit == JSOPTION_RELIMIT) {
      JSBool optval;
      if (! ::JS_ValueToBoolean(cx, *vp, &optval))
        return JS_FALSE;

      uint32 optset = ::JS_GetOptions(cx);
      if (optval)
        optset |= optbit;
      else
        optset &= ~optbit;
      ::JS_SetOptions(cx, optset);
    }
  }
  return JS_TRUE;
}

static JSClass OptionsClass = {
  "JSOptions",
  0,
  JS_PropertyStub, JS_PropertyStub, GetOptionsProperty, SetOptionsProperty,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub
};

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
TraceMallocDisable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    NS_TraceMallocDisable();
    return JS_TRUE;
}

static JSBool
TraceMallocEnable(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    NS_TraceMallocEnable();
    return JS_TRUE;
}

static JSBool
TraceMallocOpenLogFile(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int fd;
    JSString *str;
    char *filename;

    if (argc == 0) {
        fd = -1;
    } else {
        str = JS_ValueToString(cx, argv[0]);
        if (!str)
            return JS_FALSE;
        filename = JS_GetStringBytes(str);
        fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            JS_ReportError(cx, "can't open %s: %s", filename, strerror(errno));
            return JS_FALSE;
        }
    }
    *rval = INT_TO_JSVAL(fd);
    return JS_TRUE;
}

static JSBool
TraceMallocChangeLogFD(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int32 fd, oldfd;

    if (argc == 0) {
        oldfd = -1;
    } else {
        if (!JS_ValueToECMAInt32(cx, argv[0], &fd))
            return JS_FALSE;
        oldfd = NS_TraceMallocChangeLogFD(fd);
        if (oldfd == -2) {
            JS_ReportOutOfMemory(cx);
            return JS_FALSE;
        }
    }
    *rval = INT_TO_JSVAL(oldfd);
    return JS_TRUE;
}

static JSBool
TraceMallocCloseLogFD(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    int32 fd;

    if (argc == 0)
        return JS_TRUE;
    if (!JS_ValueToECMAInt32(cx, argv[0], &fd))
        return JS_FALSE;
    NS_TraceMallocCloseLogFD((int) fd);
    return JS_TRUE;
}

static JSBool
TraceMallocLogTimestamp(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;
    const char *caption;

    str = JS_ValueToString(cx, argv[0]);
    if (!str)
        return JS_FALSE;
    caption = JS_GetStringBytes(str);
    NS_TraceMallocLogTimestamp(caption);
    return JS_TRUE;
}

static JSBool
TraceMallocDumpAllocations(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;
    const char *pathname;

    str = JS_ValueToString(cx, argv[0]);
    if (!str)
        return JS_FALSE;
    pathname = JS_GetStringBytes(str);
    if (NS_TraceMallocDumpAllocations(pathname) < 0) {
        JS_ReportError(cx, "can't dump to %s: %s", pathname, strerror(errno));
        return JS_FALSE;
    }
    return JS_TRUE;
}

static JSFunctionSpec TraceMallocFunctions[] = {
    {"TraceMallocDisable",         TraceMallocDisable,         0, 0, 0},
    {"TraceMallocEnable",          TraceMallocEnable,          0, 0, 0},
    {"TraceMallocOpenLogFile",     TraceMallocOpenLogFile,     1, 0, 0},
    {"TraceMallocChangeLogFD",     TraceMallocChangeLogFD,     1, 0, 0},
    {"TraceMallocCloseLogFD",      TraceMallocCloseLogFD,      1, 0, 0},
    {"TraceMallocLogTimestamp",    TraceMallocLogTimestamp,    1, 0, 0},
    {"TraceMallocDumpAllocations", TraceMallocDumpAllocations, 1, 0, 0},
    {nsnull,                       nsnull,                     0, 0, 0}
};

#endif /* NS_TRACE_MALLOC */

#ifdef MOZ_JPROF

#include <signal.h>

inline PRBool
IsJProfAction(struct sigaction *action)
{
    return (action->sa_sigaction &&
            action->sa_flags == SA_RESTART | SA_SIGINFO);
}

void NS_JProfStartProfiling();
void NS_JProfStopProfiling();

static JSBool
JProfStartProfilingJS(JSContext *cx, JSObject *obj,
                      uintN argc, jsval *argv, jsval *rval)
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

    sigaction(SIGALRM, nsnull, &action);
    if (IsJProfAction(&action)) {
        printf("Beginning real-time jprof profiling.\n");
        raise(SIGALRM);
        return;
    }

    sigaction(SIGPROF, nsnull, &action);
    if (IsJProfAction(&action)) {
        printf("Beginning process-time jprof profiling.\n");
        raise(SIGPROF);
        return;
    }

    sigaction(SIGPOLL, nsnull, &action);
    if (IsJProfAction(&action)) {
        printf("Beginning rtc-based jprof profiling.\n");
        raise(SIGPOLL);
        return;
    }

    printf("Could not start jprof-profiling since JPROF_FLAGS was not set.\n");
}

static JSBool
JProfStopProfilingJS(JSContext *cx, JSObject *obj,
                     uintN argc, jsval *argv, jsval *rval)
{
  NS_JProfStopProfiling();
  return JS_TRUE;
}

void
NS_JProfStopProfiling()
{
    raise(SIGUSR1);
    printf("Stopped jprof profiling.\n");
}

static JSFunctionSpec JProfFunctions[] = {
    {"JProfStartProfiling",        JProfStartProfilingJS,      0, 0, 0},
    {"JProfStopProfiling",         JProfStopProfilingJS,       0, 0, 0},
    {nsnull,                       nsnull,                     0, 0, 0}
};

#endif /* defined(MOZ_JPROF) */

nsresult
nsJSContext::InitClasses(void *aGlobalObj)
{
  nsresult rv = NS_OK;

  JSObject *globalObj = NS_STATIC_CAST(JSObject *, aGlobalObj);

  rv = InitializeExternalClasses();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = InitializeLiveConnectClasses(globalObj);
  NS_ENSURE_SUCCESS(rv, rv);

  JSAutoRequest ar(mContext);

  // Initialize the options object and set default options in mContext
  JSObject *optionsObj = ::JS_DefineObject(mContext, globalObj, "_options",
                                           &OptionsClass, nsnull, 0);
  if (optionsObj &&
      ::JS_DefineProperties(mContext, optionsObj, OptionsProperties)) {
    ::JS_SetOptions(mContext, mDefaultJSOptions);
  } else {
    rv = NS_ERROR_FAILURE;
  }

#ifdef NS_TRACE_MALLOC
  // Attempt to initialize TraceMalloc functions
  ::JS_DefineFunctions(mContext, globalObj, TraceMallocFunctions);
#endif

#ifdef MOZ_JPROF
  // Attempt to initialize JProf functions
  ::JS_DefineFunctions(mContext, globalObj, JProfFunctions);
#endif

  return rv;
}

void
nsJSContext::ClearScope(void *aGlobalObj, PRBool aClearFromProtoChain)
{
  if (aGlobalObj) {
    JSObject *obj = (JSObject *)aGlobalObj;
    JSAutoRequest ar(mContext);
    ::JS_ClearScope(mContext, obj);

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
  ::JS_ClearRegExpStatics(mContext);

}

void
nsJSContext::WillInitializeContext()
{
  mIsInitialized = PR_FALSE;
}

void
nsJSContext::DidInitializeContext()
{
  mIsInitialized = PR_TRUE;
}

PRBool
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
nsJSContext::GC()
{
  FireGCTimer(PR_FALSE);
}

void
nsJSContext::ScriptEvaluated(PRBool aTerminated)
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

  mNumEvaluations++;

#ifdef WAY_TOO_MUCH_GC
  ::JS_MaybeGC(mContext);
#else
  if (mNumEvaluations > 20) {
    mNumEvaluations = 0;
    ::JS_MaybeGC(mContext);
  }
#endif

  mBranchCallbackCount = 0;
  mBranchCallbackTime = LL_ZERO;
}

nsresult
nsJSContext::SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                    nsISupports* aRef)
{
  NS_PRECONDITION(mContext->fp, "should be executing script");

  nsJSContext::TerminationFuncClosure* newClosure =
    new nsJSContext::TerminationFuncClosure(aFunc, aRef, mTerminations);
  if (!newClosure) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mTerminations = newClosure;
  return NS_OK;
}

PRBool
nsJSContext::GetScriptsEnabled()
{
  return mScriptsEnabled;
}

void
nsJSContext::SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts)
{
  // eeek - this seems the wrong way around - the global should callback
  // into each context, so every language is disabled.
  mScriptsEnabled = aEnabled;

  nsIScriptGlobalObject *global = GetGlobalObject();

  if (global) {
    global->SetScriptsEnabled(aEnabled, aFireTimeouts);
  }
}


PRBool
nsJSContext::GetProcessingScriptTag()
{
  return mProcessingScriptTag;
}

void
nsJSContext::SetProcessingScriptTag(PRBool aFlag)
{
  mProcessingScriptTag = aFlag;
}

void
nsJSContext::SetGCOnDestruction(PRBool aGCOnDestruction)
{
  mGCOnDestruction = aGCOnDestruction;
}

NS_IMETHODIMP
nsJSContext::ScriptExecuted()
{
  ScriptEvaluated(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::PreserveWrapper(nsIXPConnectWrappedNative *aWrapper)
{
  return nsDOMClassInfo::PreserveNodeWrapper(aWrapper);
}

NS_IMETHODIMP
nsJSContext::Notify(nsITimer *timer)
{
  NS_ASSERTION(mContext, "No context in nsJSContext::Notify()!");

  NS_RELEASE(sGCTimer);

  if (sPendingLoadCount == 0 || sLoadInProgressGCTimer) {
    sLoadInProgressGCTimer = PR_FALSE;

    // Reset sPendingLoadCount in case the timer that fired was a
    // timer we scheduled due to a normal GC timer firing while
    // documents were loading. If this happens we're waiting for a
    // document that is taking a long time to load, and we effectively
    // ignore the fact that the currently loading documents are still
    // loading and move on as if they weren't.
    sPendingLoadCount = 0;

    // nsCycleCollector_collect() will run a ::JS_GC() indirectly,
    // so we do not explicitly call ::JS_GC() here. 
    nsCycleCollector_collect();
  } else {
    FireGCTimer(PR_TRUE);
  }

  sReadyForGC = PR_TRUE;

  return NS_OK;
}

// static
void
nsJSContext::LoadStart()
{
  ++sPendingLoadCount;
}

// static
void
nsJSContext::LoadEnd()
{
  // sPendingLoadCount is not a well managed load counter (and doesn't
  // need to be), so make sure we don't make it wrap backwards here.
  if (sPendingLoadCount > 0) {
    --sPendingLoadCount;
  }

  if (!sPendingLoadCount && sLoadInProgressGCTimer) {
    sGCTimer->Cancel();

    NS_RELEASE(sGCTimer);
    sLoadInProgressGCTimer = PR_FALSE;

    // nsCycleCollector_collect() will run a ::JS_GC() indirectly, so
    // we do not explicitly call ::JS_GC() here.
    nsCycleCollector_collect();
  }
}

void
nsJSContext::FireGCTimer(PRBool aLoadInProgress)
{
  // Always clear the newborn roots.  If there's already a timer, this
  // will let the GC from that timer clean up properly.  If we're going
  // to create a timer, we still want to do this now so that XPCOM
  // shutdown can clean up properly.
  ::JS_ClearNewbornRoots(mContext);

  if (sGCTimer) {
    // There's already a timer for GC'ing, just return
    return;
  }

  CallCreateInstance("@mozilla.org/timer;1", &sGCTimer);

  if (!sGCTimer) {
    NS_WARNING("Failed to create timer");

    // Reset sLoadInProgressGCTimer since we're not able to fire the
    // timer.
    sLoadInProgressGCTimer = PR_FALSE;

    // nsCycleCollector_collect() will run a ::JS_GC() indirectly, so
    // we do not explicitly call ::JS_GC() here.
    nsCycleCollector_collect();

    return;
  }

  static PRBool first = PR_TRUE;

  sGCTimer->InitWithCallback(this,
                             first ? NS_FIRST_GC_DELAY :
                             aLoadInProgress ? NS_LOAD_IN_PROCESS_GC_DELAY :
                                               NS_GC_DELAY,
                             nsITimer::TYPE_ONE_SHOT);

  sLoadInProgressGCTimer = aLoadInProgress;

  first = PR_FALSE;
}

static JSBool JS_DLL_CALLBACK
DOMGCCallback(JSContext *cx, JSGCStatus status)
{
  JSBool result = gOldJSGCCallback ? gOldJSGCCallback(cx, status) : JS_TRUE;

  if (status == JSGC_BEGIN && !NS_IsMainThread())
    return JS_FALSE;

  return result;
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

/**********************************************************************
 * nsJSRuntime implementation
 *********************************************************************/

// QueryInterface implementation for nsJSRuntime
NS_INTERFACE_MAP_BEGIN(nsJSRuntime)
  NS_INTERFACE_MAP_ENTRY(nsIScriptRuntime)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsJSRuntime)
NS_IMPL_RELEASE(nsJSRuntime)

nsresult
nsJSRuntime::CreateContext(nsIScriptContext **aContext)
{
  nsCOMPtr<nsIScriptContext> scriptContext;

  *aContext = new nsJSContext(sRuntime);
  NS_ENSURE_TRUE(*aContext, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aContext);
  return NS_OK;
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
  sGCTimer = nsnull;
  sReadyForGC = PR_FALSE;
  sLoadInProgressGCTimer = PR_FALSE;
  sPendingLoadCount = 0;
  gNameSpaceManager = nsnull;
  sRuntimeService = nsnull;
  sRuntime = nsnull;
  gOldJSGCCallback = nsnull;
  sIsInitialized = PR_FALSE;
  sDidShutdown = PR_FALSE;
  sContextCount = 0;
  sSecurityManager = nsnull;
  gCollation = nsnull;
}

static int PR_CALLBACK
MaxScriptRunTimePrefChangedCallback(const char *aPrefName, void *aClosure)
{
  // Default limit on script run time to 10 seconds. 0 means let
  // scripts run forever.
  PRBool isChromePref =
    strcmp(aPrefName, "dom.max_chrome_script_run_time") == 0;
  PRInt32 time = nsContentUtils::GetIntPref(aPrefName, isChromePref ? 20 : 10);

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

JS_STATIC_DLL_CALLBACK(JSPrincipals *)
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

//static
nsresult
nsJSRuntime::Init()
{
  if (sIsInitialized) {
    if (!nsContentUtils::XPConnect())
      return NS_ERROR_NOT_AVAILABLE;

    return NS_OK;
  }

  nsresult rv = CallGetService(kJSRuntimeServiceContractID, &sRuntimeService);
  // get the JSRuntime from the runtime svc, if possible
  NS_ENSURE_SUCCESS(rv, rv);

  rv = sRuntimeService->GetRuntime(&sRuntime);
  NS_ENSURE_SUCCESS(rv, rv);

  // Let's make sure that our main thread is the same as the xpcom main thread.
  NS_ASSERTION(NS_IsMainThread(), "bad");

  NS_ASSERTION(!gOldJSGCCallback,
               "nsJSRuntime initialized more than once");

  // Save the old GC callback to chain to it, for GC-observing generality.
  gOldJSGCCallback = ::JS_SetGCCallbackRT(sRuntime, DOMGCCallback);

  // No chaining to a pre-existing callback here, we own this problem space.
#ifdef NS_DEBUG
  JSObjectPrincipalsFinder oldfop =
#endif
    ::JS_SetObjectPrincipalsFinder(sRuntime, ObjectPrincipalFinder);
  NS_ASSERTION(!oldfop, " fighting over the findObjectPrincipals callback!");

  // Set these global xpconnect options...
  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  xpc->SetCollectGarbageOnMainThreadOnly(PR_TRUE);
  xpc->SetDeferReleasesUntilAfterGarbageCollection(PR_TRUE);

#ifdef OJI
  // Initialize LiveConnect.  XXXbe use contractid rather than GetCID
  // NOTE: LiveConnect is optional so initialisation will still succeed
  //       even if the service is not present.
  nsCOMPtr<nsILiveConnectManager> manager =
           do_GetService(nsIJVMManager::GetCID());

  // Should the JVM manager perhaps define methods for starting up
  // LiveConnect?
  if (manager) {
    PRBool started = PR_FALSE;
    rv = manager->StartupLiveConnect(sRuntime, started);
    // XXX Did somebody mean to check |rv| ?
  }
#endif /* OJI */

  nsContentUtils::RegisterPrefCallback("dom.max_script_run_time",
                                       MaxScriptRunTimePrefChangedCallback,
                                       nsnull);
  MaxScriptRunTimePrefChangedCallback("dom.max_script_run_time", nsnull);

  nsContentUtils::RegisterPrefCallback("dom.max_chrome_script_run_time",
                                       MaxScriptRunTimePrefChangedCallback,
                                       nsnull);
  MaxScriptRunTimePrefChangedCallback("dom.max_chrome_script_run_time",
                                      nsnull);

  rv = CallGetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &sSecurityManager);

  sIsInitialized = NS_SUCCEEDED(rv);

  return rv;
}

void nsJSRuntime::ShutDown()
{
  if (sGCTimer) {
    // We're being shut down, if we have a GC timer scheduled, cancel
    // it. The DOM factory will do one final GC once it's shut down.

    sGCTimer->Cancel();

    NS_RELEASE(sGCTimer);

    sLoadInProgressGCTimer = PR_FALSE;
  }

  delete gNameSpaceManager;
  gNameSpaceManager = nsnull;

  if (!sContextCount) {
    // We're being shutdown, and there are no more contexts
    // alive, release the JS runtime service and the security manager.

    if (sRuntimeService && sSecurityManager) {
      // No chaining to a pre-existing callback here, we own this problem space.
#ifdef NS_DEBUG
      JSObjectPrincipalsFinder oldfop =
#endif
        ::JS_SetObjectPrincipalsFinder(sRuntime, nsnull);
      NS_ASSERTION(oldfop == ObjectPrincipalFinder, " fighting over the findObjectPrincipals callback!");
    }
    NS_IF_RELEASE(sRuntimeService);
    NS_IF_RELEASE(sSecurityManager);
    NS_IF_RELEASE(gCollation);
    NS_IF_RELEASE(gDecoder);
  }

  sDidShutdown = PR_TRUE;
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
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsJSArgArray, nsIJSArgArray)

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
    mArgv(argv),
    mArgc(argc)
{
  // copy the array - we don't know its lifetime, and ours is tied to xpcom
  // refcounting.  Alloc zero'd array so cleanup etc is safe.
  mArgv = (jsval *) PR_CALLOC(argc * sizeof(jsval));
  if (!mArgv) {
    *prv = NS_ERROR_OUT_OF_MEMORY;
    return;
  }
  for (PRUint32 i = 0; i < argc; ++i) {
    if (argv)
      mArgv[i] = argv[i];
    if (!::JS_AddNamedRoot(aContext, &mArgv[i], "nsJSArgArray.mArgv[i]")) {
      *prv = NS_ERROR_UNEXPECTED;
      return;
    }
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
    NS_ASSERTION(nsJSRuntime::sRuntime, "Where's the runtime gone?");
    if (nsJSRuntime::sRuntime) {
      for (PRUint32 i = 0; i < mArgc; ++i) {
        ::JS_RemoveRootRT(nsJSRuntime::sRuntime, &mArgv[i]);
      }
    }
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
  {
    jsval *argv = tmp->mArgv;
    if (argv) {
      jsval *end;
      for (end = argv + tmp->mArgc; argv < end; ++argv) {
        if (JSVAL_IS_OBJECT(*argv))
          cb.NoteScriptChild(JAVASCRIPT, JSVAL_TO_OBJECT(*argv));
      }
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN(nsJSArgArray)
  NS_INTERFACE_MAP_ENTRY(nsIArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSArgArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIJSArgArray)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsJSArgArray)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsJSArgArray, nsIJSArgArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsJSArgArray, nsIJSArgArray)

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
                                       NS_STATIC_CAST(jsval *, argv), &rv);
  if (ret == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  if (NS_FAILED(rv)) {
    delete ret;
    return rv;
  }
  return ret->QueryInterface(NS_GET_IID(nsIArray), (void **)aArray);
}
