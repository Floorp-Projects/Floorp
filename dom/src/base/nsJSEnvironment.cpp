/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIDOMWindowInternal.h"
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
#include "nsIScriptSecurityManager.h"
#include "nsDOMCID.h"
#include "nsIServiceManager.h"
#include "nsIXPConnect.h"
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsCOMPtr.h"
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
#include "nsIThread.h"
#include "nsITimer.h"
#include "nsDOMClassInfo.h"
#include "nsIAtom.h"
#include "nsContentUtils.h"

// For locale aware string methods
#include "plstr.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsUnicharUtils.h"
#include "nsILocaleService.h"
#include "nsICollation.h"
#include "nsCollationCID.h"

static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);

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
#ifdef CompareString
#undef CompareString
#endif

#define NS_GC_DELAY                2000 // ms
#define NS_FIRST_GC_DELAY          10000 // ms

// if you add statics here, add them to the list in nsJSEnvironment::Startup

static nsITimer *sGCTimer;
static PRBool sReadyForGC;

nsScriptNameSpaceManager *gNameSpaceManager;

static nsIJSRuntimeService *sRuntimeService;
JSRuntime *nsJSEnvironment::sRuntime;

static const char kJSRuntimeServiceContractID[] =
  "@mozilla.org/js/xpc/RuntimeService;1";

static PRThread *gDOMThread;

static JSGCCallback gOldJSGCCallback;

static PRBool sIsInitialized;
static PRBool sDidShutdown;

static PRInt32 sContextCount;

static PRTime sMaxScriptRunTime;

static nsIScriptSecurityManager *sSecurityManager;

static nsICollation *gCollation;

static nsIUnicodeDecoder *gDecoder;

void JS_DLL_CALLBACK
NS_ScriptErrorReporter(JSContext *cx,
                       const char *message,
                       JSErrorReport *report)
{
  // XXX this means we are not going to get error reports on non DOM contexts
  nsIScriptContext *context = nsJSUtils::GetDynamicScriptContext(cx);

  nsEventStatus status = nsEventStatus_eIgnore;

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
      nsIDocShell *docShell = globalObject->GetDocShell();
      if (docShell && !JSREPORT_IS_WARNING(report->flags)) {
        static PRInt32 errorDepth; // Recursion prevention
        ++errorDepth;

        nsCOMPtr<nsPresContext> presContext;
        docShell->GetPresContext(getter_AddRefs(presContext));

        if (presContext && errorDepth < 2) {
          nsScriptErrorEvent errorevent(NS_SCRIPT_ERROR);

          errorevent.fileName = fileName.get();
          errorevent.errorMsg = msg.get();
          errorevent.lineNr = report ? report->lineno : 0;

          // HandleDOMEvent() must be synchronous for the recursion block
          // (errorDepth) to work.
          globalObject->HandleDOMEvent(presContext, &errorevent, nsnull,
                                       NS_EVENT_FLAG_INIT, &status);
        }

        --errorDepth;
      }

      if (status != nsEventStatus_eConsumeNoDefault) {
        // Make an nsIScriptError and populate it with information from
        // this error.
        nsCOMPtr<nsIScriptError> errorObject =
          do_CreateInstance("@mozilla.org/scripterror;1");

        if (errorObject != nsnull) {
          nsresult rv;

          const char *category = nsnull;
          // Set category to XUL or content, if possible.
          if (docShell) {
            nsCOMPtr<nsIDocShellTreeItem> docShellTI(do_QueryInterface(docShell, &rv));
            if (NS_SUCCEEDED(rv) && docShellTI) {
              PRInt32 docShellType;
              rv = docShellTI->GetItemType(&docShellType);
              if (NS_SUCCEEDED(rv)) {
                category = docShellType == nsIDocShellTreeItem::typeChrome
                  ? "chrome javascript"
                  : "content javascript";
              }
            }
          }

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
  AppendUTF16toUTF8(NS_REINTERPRET_CAST(const PRUnichar*, report->ucmessage),
                    error);
  if (status != nsEventStatus_eIgnore && !JSREPORT_IS_WARNING(report->flags))
    error.Append(" Error was suppressed by event handler\n");

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

  // XXX do we really want to be doing this?
  ::JS_ClearPendingException(cx);
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
    PRUnichar *unichars = (PRUnichar *)malloc((srcLength + 1) * sizeof(PRUnichar));
    if (unichars) {
      rv = gDecoder->Convert(src, &srcLength, unichars, &unicharLength);
      if (NS_SUCCEEDED(rv)) {
        // terminate the returned string
        unichars[unicharLength] = 0;

        // nsIUnicodeDecoder::Convert may use fewer than srcLength PRUnichars
        if (unicharLength + 1 < srcLength + 1) {
          PRUnichar *shrunkUnichars =
            (PRUnichar *)realloc(unichars, (unicharLength + 1) * sizeof(PRUnichar));
          if (shrunkUnichars)
            unichars = shrunkUnichars;
        }
        str = JS_NewUCString(cx,
                             NS_REINTERPRET_CAST(jschar*, unichars),
                             unicharLength);
      }
      if (!str)
        free(unichars);
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
          do_CreateInstance(kCollationFactoryCID, &rv);

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

static void
NotifyXPCIfExceptionPending(JSContext *cx)
{
  if (!::JS_IsExceptionPending(cx)) {
    return;
  }

  nsCOMPtr<nsIXPCNativeCallContext> nccx;
  nsContentUtils::XPConnect()->
    GetCurrentNativeCallContext(getter_AddRefs(nccx));
  if (nccx) {
    nccx->SetExceptionWasThrown(PR_TRUE);
  }
}

// The number of branch callbacks between calls to JS_MaybeGC
#define MAYBE_GC_BRANCH_COUNT_MASK 0x00000fff // 4095

// The number of branch callbacks between elapsed time checks.  This is
// slightly more than the number of callbacks a slow machine makes in a second
// when running a script that makes callbacks infrequently.
#define MAYBE_STOP_BRANCH_COUNT_MASK 0x00007fff // 32767

// This function is called after each JS branch callback
JSBool JS_DLL_CALLBACK
nsJSContext::DOMBranchCallback(JSContext *cx, JSScript *script)
{
  // Get the native context
  nsJSContext *ctx = NS_STATIC_CAST(nsJSContext *, ::JS_GetContextPrivate(cx));

  // Filter out most of the calls to this callback
  if (++ctx->mBranchCallbackCount & MAYBE_GC_BRANCH_COUNT_MASK)
    return JS_TRUE;

  // Run the GC if we get this far.
  JS_MaybeGC(cx);

  // Filter out even more of the calls to this callback
  if (ctx->mBranchCallbackCount & MAYBE_STOP_BRANCH_COUNT_MASK)
    return JS_TRUE;

  PRTime now = PR_Now();

  // If this is the first time we've made it this far, we start timing how
  // long the script has run
  if (LL_IS_ZERO(ctx->mBranchCallbackTime)) {
    ctx->mBranchCallbackTime = now;
    return JS_TRUE;
  }

  PRTime duration;
  LL_SUB(duration, now, ctx->mBranchCallbackTime);

  // Check the amount of time this script has been running
  if (LL_CMP(duration, <, sMaxScriptRunTime))
    return JS_TRUE;

  // If we get here we're most likely executing an infinite loop in JS,
  // we'll tell the user about this and we'll give the user the option
  // of stopping the execution of the script.
  nsIScriptGlobalObject *global = ctx->GetGlobalObject();
  NS_ENSURE_TRUE(global, JS_TRUE);

  nsIDocShell *docShell = global->GetDocShell();
  NS_ENSURE_TRUE(docShell, JS_TRUE);

  nsCOMPtr<nsIInterfaceRequestor> ireq(do_QueryInterface(docShell));
  NS_ENSURE_TRUE(ireq, JS_TRUE);

  // Get the nsIPrompt interface from the docshell
  nsCOMPtr<nsIPrompt> prompt;
  ireq->GetInterface(NS_GET_IID(nsIPrompt), getter_AddRefs(prompt));
  NS_ENSURE_TRUE(prompt, JS_TRUE);

  NS_NAMED_LITERAL_STRING(title, "Script warning");
  NS_NAMED_MULTILINE_LITERAL_STRING(msg,
      NS_L("A script on this page is causing mozilla to ")
      NS_L("run slowly. If it continues to run, your ")
      NS_L("computer may become unresponsive.\n\nDo you ")
      NS_L("want to abort the script?"));

  // Open the dialog.
  PRInt32 buttonPressed = 0;
  nsresult rv = prompt->ConfirmEx(title.get(), msg.get(),
                                  (nsIPrompt::BUTTON_TITLE_YES * nsIPrompt::BUTTON_POS_0) +
                                  (nsIPrompt::BUTTON_TITLE_NO * nsIPrompt::BUTTON_POS_1),
                                  nsnull, nsnull, nsnull, nsnull, nsnull, &buttonPressed);

  if (NS_FAILED(rv) || (buttonPressed == 1)) {

    // Allow the script to run this long again
    ctx->mBranchCallbackTime = PR_Now();
    return JS_TRUE;
  }

  return JS_FALSE;
}

#define JS_OPTIONS_DOT_STR "javascript.options."

static const char js_options_dot_str[]   = JS_OPTIONS_DOT_STR;
static const char js_strict_option_str[] = JS_OPTIONS_DOT_STR "strict";
static const char js_werror_option_str[] = JS_OPTIONS_DOT_STR "werror";

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

static jsuword
GetThreadStackLimit()
{
  // Store the thread stack limit in a static local to ensure that all
  // contexts get the same stack limit (they're all on the same thread
  // anyways), and this also helps prevent returning a stack limit
  // that is beyond the end of the stack if this method is called way
  // deep on the stack.

  static jsuword sThreadStackLimit;

  if (sThreadStackLimit == 0) {
    int stackDummy;
    jsuword currentStackAddr = (jsuword)&stackDummy;

    // We assume here that the stack grows down, and that a stack
    // limit of 512k below the current stack addr is ok. If this is
    // not the case on some platforms, #ifdef's are needed for those
    // platforms.
    sThreadStackLimit = currentStackAddr +
      (0x80000 * JS_STACK_GROWTH_DIRECTION);
  }

  return sThreadStackLimit;
}

nsJSContext::nsJSContext(JSRuntime *aRuntime) : mGCOnDestruction(PR_TRUE)
{

  ++sContextCount;

  mDefaultJSOptions = JSOPTION_PRIVATE_IS_NSISUPPORTS
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

    ::JS_SetThreadStackLimit(mContext, GetThreadStackLimit());

    // Make sure the new context gets the default context options
    ::JS_SetOptions(mContext, mDefaultJSOptions);

    // Check for the JS strict option, which enables extra error checks
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
  mOwner = nsnull;
  mTerminationFunc = nsnull;
  mScriptsEnabled = PR_TRUE;
  mBranchCallbackCount = 0;
  mBranchCallbackTime = LL_ZERO;
  mProcessingScriptTag=PR_FALSE;

  InvalidateContextAndWrapperCache();
}

nsJSContext::~nsJSContext()
{
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
NS_INTERFACE_MAP_BEGIN(nsJSContext)
  NS_INTERFACE_MAP_ENTRY(nsIScriptContext)
  NS_INTERFACE_MAP_ENTRY(nsIXPCScriptNotify)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIScriptContext)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsJSContext)
NS_IMPL_RELEASE(nsJSContext)


nsresult
nsJSContext::EvaluateStringWithValue(const nsAString& aScript,
                                     void *aScopeObject,
                                     nsIPrincipal *aPrincipal,
                                     const char *aURL,
                                     PRUint32 aLineNo,
                                     const char* aVersion,
                                     void* aRetValue,
                                     PRBool* aIsUndefined)
{
  // Beware that the result is not rooted! Be very careful not to run
  // the GC before rooting the result somehow!
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
  if (aPrincipal) {
    aPrincipal->GetJSPrincipals(mContext, &jsprin);
  }
  else {
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

  if (ok) {
    JSVersion newVersion = JSVERSION_UNKNOWN;

    // SecurityManager said "ok", but don't execute if aVersion is specified
    // and unknown.  Do execute with the default version (and avoid thrashing
    // the context's version) if aVersion is not specified.
    ok = (!aVersion ||
          (newVersion = ::JS_StringToVersion(aVersion)) != JSVERSION_UNKNOWN);
    if (ok) {
      JSVersion oldVersion = JSVERSION_UNKNOWN;

      if (aVersion)
        oldVersion = ::JS_SetVersion(mContext, newVersion);
      mTerminationFuncArg = nsnull;
      mTerminationFunc = nsnull;
      ok = ::JS_EvaluateUCScriptForPrincipals(mContext,
                                              (JSObject *)aScopeObject,
                                              jsprin,
                                              (jschar*)(const PRUnichar*)PromiseFlatString(aScript).get(),
                                              aScript.Length(),
                                              aURL,
                                              aLineNo,
                                              &val);

      if (aVersion) {
        ::JS_SetVersion(mContext, oldVersion);
      }

      if (!ok) {
        // Tell XPConnect about any pending exceptions. This is needed
        // to avoid dropping JS exceptions in case we got here through
        // nested calls through XPConnect.

        NotifyXPCIfExceptionPending(mContext);
      }
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
  }
  else {
    if (aIsUndefined) {
      *aIsUndefined = PR_TRUE;
    }
  }

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

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

    NotifyXPCIfExceptionPending(cx);
  }

  return NS_OK;
}

nsresult
nsJSContext::EvaluateString(const nsAString& aScript,
                            void *aScopeObject,
                            nsIPrincipal *aPrincipal,
                            const char *aURL,
                            PRUint32 aLineNo,
                            const char* aVersion,
                            nsAString *aRetValue,
                            PRBool* aIsUndefined)
{
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

  if (ok) {
    JSVersion newVersion = JSVERSION_UNKNOWN;

    // SecurityManager said "ok", but don't execute if aVersion is specified
    // and unknown.  Do execute with the default version (and avoid thrashing
    // the context's version) if aVersion is not specified.
    ok = (!aVersion ||
          (newVersion = ::JS_StringToVersion(aVersion)) != JSVERSION_UNKNOWN);
    if (ok) {
      JSVersion oldVersion = JSVERSION_UNKNOWN;

      if (aVersion)
        oldVersion = ::JS_SetVersion(mContext, newVersion);
      mTerminationFuncArg = nsnull;
      mTerminationFunc = nsnull;
      ok = ::JS_EvaluateUCScriptForPrincipals(mContext,
                                              (JSObject *)aScopeObject,
                                              jsprin,
                                              (jschar*)(const PRUnichar*)PromiseFlatString(aScript).get(),
                                              aScript.Length(),
                                              aURL,
                                              aLineNo,
                                              &val);

      if (aVersion) {
        ::JS_SetVersion(mContext, oldVersion);
      }

      if (!ok) {
        // Tell XPConnect about any pending exceptions. This is needed
        // to avoid dropping JS exceptions in case we got here through
        // nested calls through XPConnect.

        NotifyXPCIfExceptionPending(mContext);
      }
    }
  }

  // Whew!  Finally done with these manually ref-counted things.
  JSPRINCIPALS_DROP(mContext, jsprin);

  // If all went well, convert val to a string (XXXbe unless undefined?).
  if (ok) {
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

  ScriptEvaluated(PR_TRUE);

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  return rv;
}

nsresult
nsJSContext::CompileScript(const PRUnichar* aText,
                           PRInt32 aTextLength,
                           void *aScopeObject,
                           nsIPrincipal *aPrincipal,
                           const char *aURL,
                           PRUint32 aLineNo,
                           const char* aVersion,
                           void** aScriptObject)
{
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

  *aScriptObject = nsnull;
  if (ok) {
    JSVersion newVersion = JSVERSION_UNKNOWN;

    // SecurityManager said "ok", but don't compile if aVersion is specified
    // and unknown.  Do compile with the default version (and avoid thrashing
    // the context's version) if aVersion is not specified.
    if (!aVersion ||
        (newVersion = ::JS_StringToVersion(aVersion)) != JSVERSION_UNKNOWN) {
      JSVersion oldVersion = JSVERSION_UNKNOWN;
      if (aVersion)
        oldVersion = ::JS_SetVersion(mContext, newVersion);

      JSScript* script =
        ::JS_CompileUCScriptForPrincipals(mContext,
                                          (JSObject*) aScopeObject,
                                          jsprin,
                                          (jschar*) aText,
                                          aTextLength,
                                          aURL,
                                          aLineNo);
      if (script) {
        *aScriptObject = (void*) ::JS_NewScriptObject(mContext, script);
        if (! *aScriptObject) {
          ::JS_DestroyScript(mContext, script);
          script = nsnull;
        }
      }
      if (!script)
        rv = NS_ERROR_OUT_OF_MEMORY;

      if (aVersion)
        ::JS_SetVersion(mContext, oldVersion);
    }
  }

  // Whew!  Finally done with these manually ref-counted things.
  JSPRINCIPALS_DROP(mContext, jsprin);
  return rv;
}

nsresult
nsJSContext::ExecuteScript(void* aScriptObject,
                           void *aScopeObject,
                           nsAString* aRetValue,
                           PRBool* aIsUndefined)
{
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

  mTerminationFuncArg = nsnull;
  mTerminationFunc = nsnull;
  ok = ::JS_ExecuteScript(mContext,
                          (JSObject*) aScopeObject,
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

    NotifyXPCIfExceptionPending(mContext);
  }

  ScriptEvaluated(PR_TRUE);

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

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

nsresult
nsJSContext::CompileEventHandler(void *aTarget, nsIAtom *aName,
                                 const char *aEventName,
                                 const nsAString& aBody,
                                 const char *aURL, PRUint32 aLineNo,
                                 PRBool aShared, void** aHandler)
{
  if (!sSecurityManager) {
    NS_ERROR("Huh, we need a script security manager to compile "
             "an event handler!");

    return NS_ERROR_UNEXPECTED;
  }

  JSObject *target = (JSObject*)aTarget;

  JSPrincipals *jsprin = nsnull;

  if (target) {
    // Get the principal of the event target (the object principal),
    // don't get the principal of the global object in this context
    // since that opens up security exploits with delayed event
    // handler compilation on stale DOM objects (objects that live in
    // a document that has already been unloaded).
    nsCOMPtr<nsIPrincipal> prin;
    nsresult rv = sSecurityManager->GetObjectPrincipal(mContext, target,
                                                       getter_AddRefs(prin));
    NS_ENSURE_SUCCESS(rv, rv);

    prin->GetJSPrincipals(mContext, &jsprin);
    NS_ENSURE_TRUE(jsprin, NS_ERROR_NOT_AVAILABLE);
  }

  const char *charName = AtomToEventHandlerName(aName);

  const char *argList[] = { aEventName };

  JSFunction* fun =
      ::JS_CompileUCFunctionForPrincipals(mContext, target, jsprin,
                                          charName, 1, argList,
                                          (jschar*)(const PRUnichar*)PromiseFlatString(aBody).get(),
                                          aBody.Length(),
                                          aURL, aLineNo);

  if (jsprin) {
    JSPRINCIPALS_DROP(mContext, jsprin);
  }
  if (!fun) {
    return NS_ERROR_FAILURE;
  }

  JSObject *handler = ::JS_GetFunctionObject(fun);
  if (aHandler)
    *aHandler = (void*) handler;

  if (aShared) {
    /* Break scope link to avoid entraining shared compilation scope. */
    ::JS_SetParent(mContext, handler, nsnull);
  }
  return NS_OK;
}

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
  JSFunction* fun =
      ::JS_CompileUCFunctionForPrincipals(mContext, target, jsprin,
                                          PromiseFlatCString(aName).get(),
                                          aArgCount, aArgArray,
                                          (jschar*)(const PRUnichar*)PromiseFlatString(aBody).get(),
                                          aBody.Length(),
                                          aURL, aLineNo);

  if (jsprin)
    JSPRINCIPALS_DROP(mContext, jsprin);
  if (!fun)
    return NS_ERROR_FAILURE;

  JSObject *handler = ::JS_GetFunctionObject(fun);
  if (aFunctionObject)
    *aFunctionObject = (void*) handler;

  // Prevent entraining just like CompileEventHandler does?
  if (aShared) {
    /* Break scope link to avoid entraining shared compilation scope. */
    ::JS_SetParent(mContext, handler, nsnull);
  }
  return NS_OK;
}

nsresult
nsJSContext::CallEventHandler(JSObject *aTarget, JSObject *aHandler,
                              uintN argc, jsval *argv, jsval *rval)
{
  *rval = JSVAL_VOID;

  if (!mScriptsEnabled) {
    return NS_OK;
  }

  // This one's a lot easier than EvaluateString because we don't have to
  // hassle with principals: they're already compiled into the JS function.
  nsresult rv;

  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext)))
    return NS_ERROR_FAILURE;

  mTerminationFuncArg = nsnull;
  mTerminationFunc = nsnull;

  // check if the event handler can be run on the object in question
  rv = sSecurityManager->CheckFunctionAccess(mContext, aHandler, aTarget);

  if (NS_SUCCEEDED(rv)) {
    jsval funval = OBJECT_TO_JSVAL(aHandler);
    PRBool ok = ::JS_CallFunctionValue(mContext, aTarget, funval, argc, argv,
                                       rval);

    ScriptEvaluated(PR_TRUE);

    if (!ok) {
      // Tell XPConnect about any pending exceptions. This is needed
      // to avoid dropping JS exceptions in case we got here through
      // nested calls through XPConnect.

      NotifyXPCIfExceptionPending(mContext);

      // Don't pass back results from failed calls.
      *rval = JSVAL_VOID;

      // Tell the caller that the handler threw an error.
      rv = NS_ERROR_FAILURE;
    }
  }

  if (NS_FAILED(stack->Pop(nsnull)))
    return NS_ERROR_FAILURE;

  return rv;
}

nsresult
nsJSContext::BindCompiledEventHandler(void *aTarget, nsIAtom *aName,
                                      void *aHandler)
{
  const char *charName = AtomToEventHandlerName(aName);

  JSObject *funobj = (JSObject*) aHandler;
  JSObject *target = (JSObject*) aTarget;

  // Make sure the handler function is parented by its event target object
  if (funobj && ::JS_GetParent(mContext, funobj) != target) {
    funobj = ::JS_CloneFunctionObject(mContext, funobj, target);
    if (!funobj)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!::JS_DefineProperty(mContext, target, charName,
                           OBJECT_TO_JSVAL(funobj), nsnull, nsnull,
                           JSPROP_ENUMERATE | JSPROP_PERMANENT)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

void
nsJSContext::SetDefaultLanguageVersion(const char* aVersion)
{
  ::JS_SetVersion(mContext, ::JS_StringToVersion(aVersion));
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

    nsCOMPtr<nsISupports> native;
    wrapped_native->GetNative(getter_AddRefs(native));

    NS_WARN_IF_FALSE(native,
                     "XPConnect wrapped native doesn't wrap anything.");

    sgo = do_QueryInterface(native);
  } else {
    sgo = do_QueryInterface(priv);
  }

  // This'll return a pointer to something we're about to release, but
  // that's ok, the JS object will hold it alive long enough.
  return sgo;
}

void *
nsJSContext::GetNativeContext()
{
  return mContext;
}


nsresult
nsJSContext::InitContext(nsIScriptGlobalObject *aGlobalObject)
{
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

  mIsInitialized = PR_FALSE;

  nsIXPConnect *xpc = nsContentUtils::XPConnect();

  JSObject *global = ::JS_GetGlobalObject(mContext);

  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

  // If there's already a global object in mContext we won't tell
  // XPConnect to wrap aGlobalObject since it's already wrapped.

  if (!global) {
    rv = xpc->InitClassesWithNewWrappedGlobal(mContext, aGlobalObject,
                                              NS_GET_IID(nsISupports),
                                              PR_FALSE,
                                              getter_AddRefs(holder));
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    // If there's already a global object in mContext we're called
    // after ::JS_ClearScope() was called. We'll have to tell XPConnect
    // to re-initialize the global object to do things like define the
    // Components object on the global again.
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

  rv = InitClasses(); // this will complete global object initialization
  NS_ENSURE_SUCCESS(rv, rv);

  mIsInitialized = PR_TRUE;

  return rv;
}

nsresult
nsJSContext::InitializeExternalClasses()
{
  NS_ENSURE_TRUE(gNameSpaceManager, NS_ERROR_NOT_INITIALIZED);

  return gNameSpaceManager->InitForContext(this);
}

nsresult
nsJSContext::InitializeLiveConnectClasses()
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
        rv = liveConnectManager->InitLiveConnectClasses(mContext, ::JS_GetGlobalObject(mContext));
      }
    }
  }
#endif /* OJI */

  // return all is well until things are stable.
  return NS_OK;
}

static JSPropertySpec OptionsProperties[] = {
  {"strict",    JSOPTION_STRICT,    JSPROP_ENUMERATE | JSPROP_PERMANENT},
  {"werror",    JSOPTION_WERROR,    JSPROP_ENUMERATE | JSPROP_PERMANENT},
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

    // Don't let options other than strict and werror be set -- it would be
    // bad if web page script could clear JSOPTION_PRIVATE_IS_NSISUPPORTS!
    if ((optbit & (optbit - 1)) == 0 && optbit <= JSOPTION_WERROR) {
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

static JSBool
JProfStartProfiling(JSContext *cx, JSObject *obj,
                    uintN argc, jsval *argv, jsval *rval)
{
    // Figure out whether we're dealing with SIGPROF, SIGALRM, or
    // SIGPOLL profiling (SIGALRM for JP_REALTIME, SIGPOLL for
    // JP_RTC_HZ)
    struct sigaction action;

    sigaction(SIGALRM, nsnull, &action);
    if (IsJProfAction(&action)) {
        printf("Beginning real-time jprof profiling.\n");
        raise(SIGALRM);
        return JS_TRUE;
    }

    sigaction(SIGPROF, nsnull, &action);
    if (IsJProfAction(&action)) {
        printf("Beginning process-time jprof profiling.\n");
        raise(SIGPROF);
        return JS_TRUE;
    }

    sigaction(SIGPOLL, nsnull, &action);
    if (IsJProfAction(&action)) {
        printf("Beginning rtc-based jprof profiling.\n");
        raise(SIGPOLL);
        return JS_TRUE;
    }

    printf("Could not start jprof-profiling since JPROF_FLAGS was not set.\n");
    return JS_TRUE;
}

static JSBool
JProfStopProfiling(JSContext *cx, JSObject *obj,
                   uintN argc, jsval *argv, jsval *rval)
{
    raise(SIGUSR1);
    printf("Stopped jprof profiling.\n");
    return JS_TRUE;
}

static JSFunctionSpec JProfFunctions[] = {
    {"JProfStartProfiling",        JProfStartProfiling,        0, 0, 0},
    {"JProfStopProfiling",         JProfStopProfiling,         0, 0, 0},
    {nsnull,                       nsnull,                     0, 0, 0}
};

#endif /* defined(MOZ_JPROF) */

nsresult
nsJSContext::InitClasses()
{
  nsresult rv = NS_OK;
  JSObject *globalObj = ::JS_GetGlobalObject(mContext);

  rv = InitializeExternalClasses();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = InitializeLiveConnectClasses();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsDOMClassInfo::InitDOMJSClass(mContext, globalObj);
  NS_ENSURE_SUCCESS(rv, rv);

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

PRBool
nsJSContext::IsContextInitialized()
{
  return mIsInitialized;
}

void
nsJSContext::GC()
{
  FireGCTimer();
}

void
nsJSContext::ScriptEvaluated(PRBool aTerminated)
{
  if (aTerminated && mTerminationFunc) {
    (*mTerminationFunc)(mTerminationFuncArg);
    mTerminationFuncArg = nsnull;
    mTerminationFunc = nsnull;
  }

  mNumEvaluations++;

  if (mNumEvaluations > 20) {
    mNumEvaluations = 0;
    ::JS_MaybeGC(mContext);
  }

  mBranchCallbackCount = 0;
  mBranchCallbackTime = LL_ZERO;
}

void
nsJSContext::SetOwner(nsIScriptContextOwner* owner)
{
  // The owner should not be addrefed!! We'll be told
  // when the owner goes away.
  mOwner = owner;
}

nsIScriptContextOwner *
nsJSContext::GetOwner()
{
  return mOwner;
}

void
nsJSContext::SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                    nsISupports* aRef)
{
  mTerminationFunc = aFunc;
  mTerminationFuncArg = aRef;
}

PRBool
nsJSContext::GetScriptsEnabled()
{
  return mScriptsEnabled;
}

void
nsJSContext::SetScriptsEnabled(PRBool aEnabled, PRBool aFireTimeouts)
{
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
nsJSContext::Notify(nsITimer *timer)
{
  NS_ASSERTION(mContext, "No context in nsJSContext::Notify()!");

  ::JS_GC(mContext);

  sReadyForGC = PR_TRUE;

  NS_RELEASE(sGCTimer);
  return NS_OK;
}

void
nsJSContext::FireGCTimer()
{
  if (sGCTimer) {
    // There's already a timer for GC'ing, just clear newborn roots
    // and return

    ::JS_ClearNewbornRoots(mContext);

    return;
  }

  CallCreateInstance("@mozilla.org/timer;1", &sGCTimer);

  if (!sGCTimer) {
    NS_WARNING("Failed to create timer");

    ::JS_GC(mContext);

    return;
  }

  static PRBool first = PR_TRUE;

  sGCTimer->InitWithCallback(this,
                             first ? NS_FIRST_GC_DELAY : NS_GC_DELAY,
                             nsITimer::TYPE_ONE_SHOT);

  first = PR_FALSE;
}

static JSBool JS_DLL_CALLBACK
DOMGCCallback(JSContext *cx, JSGCStatus status)
{
  if (status == JSGC_BEGIN && PR_GetCurrentThread() != gDOMThread)
    return JS_FALSE;
  return gOldJSGCCallback ? gOldJSGCCallback(cx, status) : JS_TRUE;
}

//static
void
nsJSEnvironment::Startup()
{
  // initialize all our statics, so that we can restart XPCOM
  sGCTimer = nsnull;
  sReadyForGC = PR_FALSE;
  gNameSpaceManager = nsnull;
  sRuntimeService = nsnull;
  sRuntime = nsnull;
  gDOMThread = nsnull;
  gOldJSGCCallback = nsnull;
  sIsInitialized = PR_FALSE;
  sDidShutdown = PR_FALSE;
  sContextCount = 0;
  sSecurityManager = nsnull;
  gCollation = nsnull;
}

// static
nsresult
nsJSEnvironment::Init()
{
  if (sIsInitialized) {
    return NS_OK;
  }

  nsresult rv = CallGetService(kJSRuntimeServiceContractID, &sRuntimeService);
  // get the JSRuntime from the runtime svc, if possible
  NS_ENSURE_SUCCESS(rv, rv);

  rv = sRuntimeService->GetRuntime(&sRuntime);
  NS_ENSURE_SUCCESS(rv, rv);

  gDOMThread = PR_GetCurrentThread();

#ifdef DEBUG
  // Let's make sure that our main thread is the same as the xpcom main thread.
  {
    nsCOMPtr<nsIThread> t;
    nsresult rv;
    PRThread* mainThread;
    rv = nsIThread::GetMainThread(getter_AddRefs(t));
    NS_ASSERTION(NS_SUCCEEDED(rv) && t, "bad");
    rv = t->GetPRThread(&mainThread);
    NS_ASSERTION(NS_SUCCEEDED(rv) && mainThread == gDOMThread, "bad");
  }
#endif

  NS_ASSERTION(!gOldJSGCCallback,
               "nsJSEnvironment initialized more than once");

  gOldJSGCCallback = ::JS_SetGCCallbackRT(sRuntime, DOMGCCallback);

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
  }
#endif /* OJI */

  // Initialize limit on script run time to 5 seconds
  PRInt32 maxtime = 5;
  PRInt32 time = nsContentUtils::GetIntPref("dom.max_script_run_time");

  // Force the default for unreasonable values
  if (time > 0)
    maxtime = time;

  PRTime usec_per_sec;
  LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
  LL_I2L(sMaxScriptRunTime, maxtime);
  LL_MUL(sMaxScriptRunTime, sMaxScriptRunTime, usec_per_sec);

  rv = CallGetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &sSecurityManager);

  sIsInitialized = NS_SUCCEEDED(rv);

  return rv;
}

// static
void nsJSEnvironment::ShutDown()
{
  if (sGCTimer) {
    // We're being shut down, if we have a GC timer scheduled, cancel
    // it. The DOM factory will do one final GC once it's shut down.

    sGCTimer->Cancel();

    NS_RELEASE(sGCTimer);
  }

  delete gNameSpaceManager;
  gNameSpaceManager = nsnull;

  if (!sContextCount) {
    // We're being shutdown, and there are no more contexts
    // alive, release the JS runtime service and the security manager.

    NS_IF_RELEASE(sRuntimeService);
    NS_IF_RELEASE(sSecurityManager);
    NS_IF_RELEASE(gCollation);
    NS_IF_RELEASE(gDecoder);
  }

  sDidShutdown = PR_TRUE;
}

// static
nsresult
nsJSEnvironment::CreateNewContext(nsIScriptContext **aContext)
{
  *aContext = new nsJSContext(sRuntime);
  NS_ENSURE_TRUE(*aContext, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aContext);
  return NS_OK;
}

nsresult
NS_CreateScriptContext(nsIScriptGlobalObject *aGlobal,
                       nsIScriptContext **aContext)
{
  nsresult rv = nsJSEnvironment::Init();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptContext> scriptContext;
  rv = nsJSEnvironment::CreateNewContext(getter_AddRefs(scriptContext));
  NS_ENSURE_SUCCESS(rv, rv);

  // Bind the script context and the global object
  rv = scriptContext->InitContext(aGlobal);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aGlobal) {
    aGlobal->SetContext(scriptContext);
  }

  *aContext = scriptContext;

  NS_ADDREF(*aContext);

  return rv;
}

