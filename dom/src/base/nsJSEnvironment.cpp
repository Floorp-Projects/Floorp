/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsJSEnvironment.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
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
#include "nsIPref.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsJSUtils.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIPresContext.h"
#include "nsIScriptError.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPrompt.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"
#include "nsGUIEvent.h"
#include "nsScriptNameSpaceManager.h"
#include "nsIThread.h"
#include "nsDOMClassInfo.h"

#ifdef MOZ_LOGGING
// Force PR_LOGGING so we can get JS strict warnings even in release builds
#define FORCE_PR_LOG 1
#endif
#include "prlog.h"
#include "prthread.h"

#include "nsIJVMManager.h"
#include "nsILiveConnectManager.h"

const size_t gStackSize = 8192;

static NS_DEFINE_IID(kPrefServiceCID, NS_PREF_CID);

#ifdef PR_LOGGING
static PRLogModuleInfo* gJSDiagnostics = nsnull;
#endif

nsScriptNameSpaceManager *gNameSpaceManager;

void JS_DLL_CALLBACK
NS_ScriptErrorReporter(JSContext *cx,
                       const char *message,
                       JSErrorReport *report)
{
  nsCOMPtr<nsIScriptContext> context;
  nsEventStatus status = nsEventStatus_eIgnore;

  // XXX this means we are not going to get error reports on non DOM contexts
  nsJSUtils::GetDynamicScriptContext(cx, getter_AddRefs(context));
  if (context) {
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    context->GetGlobalObject(getter_AddRefs(globalObject));

    if (globalObject) {
      nsCOMPtr<nsIScriptGlobalObjectOwner> owner;
      if(NS_FAILED(globalObject->GetGlobalObjectOwner(getter_AddRefs(owner))) ||
         !owner) {
        NS_WARN_IF_FALSE(PR_FALSE, "Failed to get a global Object Owner");
        return;
      }

      //send error event first, then proceed
      nsCOMPtr<nsIDocShell> docShell;
      globalObject->GetDocShell(getter_AddRefs(docShell));
      if (docShell) {
        static PRInt32 errorDepth = 0; // Recursion prevention
        errorDepth++;

        nsCOMPtr<nsIPresContext> presContext;
        docShell->GetPresContext(getter_AddRefs(presContext));

        if(presContext && errorDepth < 2) {
          nsEvent errorevent;
          errorevent.eventStructType = NS_EVENT;
          errorevent.message = NS_SCRIPT_ERROR;

          // HandleDOMEvent() must be synchronous for the recursion block
          // (errorDepth) to work.
          globalObject->HandleDOMEvent(presContext, &errorevent, nsnull, NS_EVENT_FLAG_INIT, &status);
        }

        errorDepth--;
      }

      if (status != nsEventStatus_eConsumeNoDefault) {

        // Make an nsIScriptError and populate it with information from
        // this error.
        nsCOMPtr<nsIScriptError>
          errorObject(do_CreateInstance("@mozilla.org/scripterror;1"));

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
            nsAutoString fileUni, msg;
            fileUni.AssignWithConversion(report->filename);
            PRUint32 column = report->uctokenptr - report->uclinebuf;

            const PRUnichar *m = NS_REINTERPRET_CAST(const PRUnichar*,
                                                     report->ucmessage);

            if (!m && message) {
              msg.AssignWithConversion(message);

              m = msg.get();
            }

            rv = errorObject->Init(m, fileUni.get(),
                                   NS_REINTERPRET_CAST(const PRUnichar*,
                                                       report->uclinebuf),
                                   report->lineno, column, report->flags,
                                   category);
          } else if (message) {
            nsAutoString messageUni;
            messageUni.AssignWithConversion(message);

            rv = errorObject->Init(messageUni.get(), nsnull, nsnull,
                                   0, 0, 0, category);
          }

          if (NS_SUCCEEDED(rv))
            owner->ReportScriptError(errorObject);
        }
      }
    }
  }

  // Print it to stderr as well, for the benefit of those invoking
  // mozilla with -console.
  nsAutoString error;
  error.AssignWithConversion("JavaScript ");
  if (JSREPORT_IS_STRICT(report->flags))
    error.AppendWithConversion("strict ");
  error.AppendWithConversion(JSREPORT_IS_WARNING(report->flags) ? "warning: " : "error: ");
  error.AppendWithConversion("\n");
  error.AppendWithConversion(report->filename);
  error.AppendWithConversion(" line ");
  error.AppendInt(report->lineno, 10);
  error.AppendWithConversion(": ");
  error.Append(NS_REINTERPRET_CAST(const PRUnichar*, report->ucmessage));
  error.AppendWithConversion("\n");
  if (status != nsEventStatus_eIgnore && !JSREPORT_IS_WARNING(report->flags))
    error.AppendWithConversion("Error was suppressed by event handler\n");

#ifdef DEBUG  
  char *errorStr = ToNewCString(error);
  if (errorStr) {
    fprintf(stderr, "%s\n", errorStr);
    fflush(stderr);
    nsMemory::Free(errorStr);
  }
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

#define MAYBE_GC_BRANCH_COUNT_MASK 0x00000fff // 4095
#define MAYBE_STOP_BRANCH_COUNT_MASK 0x003fffff

JSBool JS_DLL_CALLBACK
nsJSContext::DOMBranchCallback(JSContext *cx, JSScript *script)
{
  // Get the native context
  nsJSContext *ctx = NS_STATIC_CAST(nsJSContext *, ::JS_GetContextPrivate(cx));
  if (!ctx)
    return JS_TRUE;

  // Filter out most of the calls to this callback
  if (++ctx->mBranchCallbackCount & MAYBE_GC_BRANCH_COUNT_MASK)
    return JS_TRUE;

  // Run the GC if we get this far.
  JS_MaybeGC(cx);

  // Filter out most of the calls to this callback that make it this far
  if (ctx->mBranchCallbackCount & MAYBE_STOP_BRANCH_COUNT_MASK)
    return JS_TRUE;

  // If we get here we're most likely executing an infinite loop in JS,
  // we'll tell the user about this and we'll give the user the option
  // of stopping the execution of the script.
  nsCOMPtr<nsIScriptGlobalObject> global;
  ctx->GetGlobalObject(getter_AddRefs(global));
  NS_ENSURE_TRUE(global, JS_TRUE);

  nsCOMPtr<nsIDocShell> docShell;
  global->GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_TRUE(docShell, JS_TRUE);

  nsCOMPtr<nsIInterfaceRequestor> ireq(do_QueryInterface(docShell));
  NS_ENSURE_TRUE(ireq, JS_TRUE);

  // Get the nsIPrompt interface from the docshell
  nsCOMPtr<nsIPrompt> prompt;
  ireq->GetInterface(NS_GET_IID(nsIPrompt), getter_AddRefs(prompt));
  NS_ENSURE_TRUE(prompt, JS_TRUE);

  nsAutoString title, msg;
  title.AssignWithConversion("Script warning");
  msg.AssignWithConversion("A script on this page is causing mozilla to "
                           "run slowly. If it continues to run, your "
                           "computer may become unresponsive.\n\nDo you "
                           "want to abort the script?");

  JSBool ret = JS_TRUE;

  // Open the dialog.
  if (NS_FAILED(prompt->Confirm(title.get(), msg.get(), &ret)))
    return JS_TRUE;

  return !ret;
}

#define JS_OPTIONS_DOT_STR "javascript.options."

static const char js_options_dot_str[]   = JS_OPTIONS_DOT_STR;
static const char js_strict_option_str[] = JS_OPTIONS_DOT_STR "strict";
static const char js_werror_option_str[] = JS_OPTIONS_DOT_STR "werror";

int PR_CALLBACK
nsJSContext::JSOptionChangedCallback(const char *pref, void *data)
{
  nsresult rv;
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &rv));
  if (NS_SUCCEEDED(rv)) {
    nsJSContext *context = NS_REINTERPRET_CAST(nsJSContext *, data);
    PRUint32 oldDefaultJSOptions = context->mDefaultJSOptions;
    PRUint32 newDefaultJSOptions = oldDefaultJSOptions;

    PRBool strict;
    if (NS_SUCCEEDED(prefs->GetBoolPref(js_strict_option_str, &strict))) {
      if (strict)
        newDefaultJSOptions |= JSOPTION_STRICT;
      else
        newDefaultJSOptions &= ~JSOPTION_STRICT;
    }

    PRBool werror;
    if (NS_SUCCEEDED(prefs->GetBoolPref(js_werror_option_str, &werror))) {
      if (werror)
        newDefaultJSOptions |= JSOPTION_WERROR;
      else
        newDefaultJSOptions &= ~JSOPTION_WERROR;
    }

    if (newDefaultJSOptions != oldDefaultJSOptions) {
      // Set options only if we used the old defaults; otherwise the page has
      // customized some via the options object and we defer to its wisdom.
      if (::JS_GetOptions(context->mContext) == oldDefaultJSOptions)
        ::JS_SetOptions(context->mContext, newDefaultJSOptions);

      // Save the new defaults for the next page load (InitContext).
      context->mDefaultJSOptions = newDefaultJSOptions;
    }
  }
  return 0;
}

nsJSContext::nsJSContext(JSRuntime *aRuntime) : mGCOnDestruction(PR_TRUE)
{
  NS_INIT_REFCNT();

  mDefaultJSOptions = JSOPTION_PRIVATE_IS_NSISUPPORTS
#ifdef DEBUG
    | JSOPTION_STRICT   // lint catching for development
#endif
    ;

  // Let xpconnect resync its JSContext tracker. We do this before creating
  // a new JSContext just in case the heap manager recycles the JSContext
  // struct.
  nsresult rv;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  if (NS_SUCCEEDED(rv))
    xpc->SyncJSContexts();

  mContext = ::JS_NewContext(aRuntime, gStackSize);
  if (mContext) {
    ::JS_SetContextPrivate(mContext, (void *)this);

    // Make sure the new context gets the default context options
    ::JS_SetOptions(mContext, mDefaultJSOptions);

    // Check for the JS strict option, which enables extra error checks
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &rv));
    if (NS_SUCCEEDED(rv)) {
      (void) prefs->RegisterCallback(js_options_dot_str,
                                     JSOptionChangedCallback,
                                     this);
      (void) JSOptionChangedCallback(js_options_dot_str, this);
    }

    ::JS_SetBranchCallback(mContext, DOMBranchCallback);
  }
  mIsInitialized = PR_FALSE;
  mNumEvaluations = 0;
  mOwner = nsnull;
  mTerminationFunc = nsnull;
  mScriptsEnabled = PR_TRUE;
  mBranchCallbackCount = 0;
  mProcessingScriptTag=PR_FALSE;

  InvalidateContextAndWrapperCache();
}

const char kScriptSecurityManagerContractID[] = NS_SCRIPTSECURITYMANAGER_CONTRACTID;

nsJSContext::~nsJSContext()
{
  mSecurityManager = nsnull; // Force release

  // Cope with JS_NewContext failure in ctor (XXXbe move NewContext to Init?)
  if (!mContext)
    return;

  // Clear our entry in the JSContext, bugzilla bug 66413
  ::JS_SetContextPrivate(mContext, nsnull);

  // Unregister our "javascript.options.*" pref-changed callback.
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
  if (prefs) {
    prefs->UnregisterCallback(js_options_dot_str, JSOptionChangedCallback,
                              this);
  }

  // Let xpconnect destroy the JSContext when it thinks the time is right.
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID()));
  if (xpc) {
    xpc->ReleaseJSContext(mContext, !mGCOnDestruction);
  } else {
    ::JS_DestroyContext(mContext);
  }
}

NS_IMPL_ISUPPORTS2(nsJSContext, nsIScriptContext, nsIXPCScriptNotify)

NS_IMETHODIMP
nsJSContext::EvaluateStringWithValue(const nsAReadableString& aScript,
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
    *aIsUndefined = PR_TRUE;
    return NS_OK;
  }

  nsresult rv;
  if (!aScopeObject)
    aScopeObject = ::JS_GetGlobalObject(mContext);

  // Safety first: get an object representing the script's principals, i.e.,
  // the entities who signed this script, or the fully-qualified-domain-name
  // or "codebase" from which it was loaded.
  JSPrincipals *jsprin;
  nsCOMPtr<nsIPrincipal> principal = aPrincipal;
  if (aPrincipal) {
    aPrincipal->GetJSPrincipals(&jsprin);
  }
  else {
    nsCOMPtr<nsIScriptGlobalObject> global;
    GetGlobalObject(getter_AddRefs(global));
    if (!global)
      return NS_ERROR_FAILURE;
    nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal = do_QueryInterface(global, &rv);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    rv = objPrincipal->GetPrincipal(getter_AddRefs(principal));
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    principal->GetJSPrincipals(&jsprin);
  }
  // From here on, we must JSPRINCIPALS_DROP(jsprin) before returning...

  PRBool ok = PR_FALSE;
  nsCOMPtr<nsIScriptSecurityManager> securityManager;
  rv = GetSecurityManager(getter_AddRefs(securityManager));
  if (NS_SUCCEEDED(rv))
    rv = securityManager->CanExecuteScripts(mContext, principal, &ok);
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
    JSVersion newVersion;

    // SecurityManager said "ok", but don't execute if aVersion is specified
    // and unknown.  Do execute with the default version (and avoid thrashing
    // the context's version) if aVersion is not specified.
    ok = (!aVersion ||
          (newVersion = ::JS_StringToVersion(aVersion)) != JSVERSION_UNKNOWN);
    if (ok) {
      JSVersion oldVersion;

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
      if (aVersion)
        ::JS_SetVersion(mContext, oldVersion);
    }
  }

  // Whew!  Finally done with these manually ref-counted things.
  JSPRINCIPALS_DROP(mContext, jsprin);

  // If all went well, convert val to a string (XXXbe unless undefined?).
  if (ok) {
    if (aIsUndefined) *aIsUndefined = JSVAL_IS_VOID(val);
    *NS_STATIC_CAST(jsval*, aRetValue) = val;
  }
  else {
    if (aIsUndefined) *aIsUndefined = PR_TRUE;
  }

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  return rv;

}

NS_IMETHODIMP
nsJSContext::EvaluateString(const nsAReadableString& aScript,
                            void *aScopeObject,
                            nsIPrincipal *aPrincipal,
                            const char *aURL,
                            PRUint32 aLineNo,
                            const char* aVersion,
                            nsAWritableString& aRetValue,
                            PRBool* aIsUndefined)
{
  if (!mScriptsEnabled) {
    *aIsUndefined = PR_TRUE;
    aRetValue.Truncate();
    return NS_OK;
  }

  nsresult rv;
  if (!aScopeObject)
    aScopeObject = ::JS_GetGlobalObject(mContext);

  // Safety first: get an object representing the script's principals, i.e.,
  // the entities who signed this script, or the fully-qualified-domain-name
  // or "codebase" from which it was loaded.
  JSPrincipals *jsprin;
  nsCOMPtr<nsIPrincipal> principal = aPrincipal;
  if (aPrincipal) {
    aPrincipal->GetJSPrincipals(&jsprin);
  }
  else {
    nsCOMPtr<nsIScriptGlobalObject> global;
    GetGlobalObject(getter_AddRefs(global));
    if (!global)
      return NS_ERROR_FAILURE;
    nsCOMPtr<nsIScriptObjectPrincipal> objPrincipal = do_QueryInterface(global, &rv);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    rv = objPrincipal->GetPrincipal(getter_AddRefs(principal));
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    principal->GetJSPrincipals(&jsprin);
  }
  // From here on, we must JSPRINCIPALS_DROP(jsprin) before returning...

  PRBool ok = PR_FALSE;
  nsCOMPtr<nsIScriptSecurityManager> securityManager;
  rv = GetSecurityManager(getter_AddRefs(securityManager));
  if (NS_SUCCEEDED(rv))
    rv = securityManager->CanExecuteScripts(mContext, principal, &ok);
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
    JSVersion newVersion;

    // SecurityManager said "ok", but don't execute if aVersion is specified
    // and unknown.  Do execute with the default version (and avoid thrashing
    // the context's version) if aVersion is not specified.
    ok = (!aVersion ||
          (newVersion = ::JS_StringToVersion(aVersion)) != JSVERSION_UNKNOWN);
    if (ok) {
      JSVersion oldVersion;

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
      if (aVersion)
        ::JS_SetVersion(mContext, oldVersion);
    }
  }

  // Whew!  Finally done with these manually ref-counted things.
  JSPRINCIPALS_DROP(mContext, jsprin);

  // If all went well, convert val to a string (XXXbe unless undefined?).
  if (ok) {
    if (aIsUndefined) *aIsUndefined = JSVAL_IS_VOID(val);
    JSString* jsstring = ::JS_ValueToString(mContext, val);
    if (jsstring) {
      aRetValue.Assign(NS_REINTERPRET_CAST(const PRUnichar*,
                                           ::JS_GetStringChars(jsstring)),
                       ::JS_GetStringLength(jsstring));
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }
  }
  else {
    if (aIsUndefined) *aIsUndefined = PR_TRUE;
    aRetValue.Truncate();
  }

  ScriptEvaluated(PR_TRUE);

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  return rv;
}

NS_IMETHODIMP
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
  aPrincipal->GetJSPrincipals(&jsprin);
  // From here on, we must JSPRINCIPALS_DROP(jsprin) before returning...

  PRBool ok = PR_FALSE;
  nsCOMPtr<nsIScriptSecurityManager> securityManager;
  rv = GetSecurityManager(getter_AddRefs(securityManager));
  if (NS_SUCCEEDED(rv))
    rv = securityManager->CanExecuteScripts(mContext, aPrincipal, &ok);
  if (NS_FAILED(rv)) {
    JSPRINCIPALS_DROP(mContext, jsprin);
    return NS_ERROR_FAILURE;
  }

  *aScriptObject = nsnull;
  if (ok) {
    JSVersion newVersion;

    // SecurityManager said "ok", but don't compile if aVersion is specified
    // and unknown.  Do compile with the default version (and avoid thrashing
    // the context's version) if aVersion is not specified.
    if (!aVersion ||
        (newVersion = ::JS_StringToVersion(aVersion)) != JSVERSION_UNKNOWN) {
      JSVersion oldVersion;
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

NS_IMETHODIMP
nsJSContext::ExecuteScript(void* aScriptObject,
                           void *aScopeObject,
                           nsAWritableString* aRetValue,
                           PRBool* aIsUndefined)
{
  if (!mScriptsEnabled) {
    if (aIsUndefined)
      *aIsUndefined = PR_TRUE;
    if (aRetValue)
      aRetValue->Truncate();
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
    if (aIsUndefined)
      *aIsUndefined = JSVAL_IS_VOID(val);
    if (aRetValue) {
      JSString* jsstring = ::JS_ValueToString(mContext, val);
      if (jsstring) {
        aRetValue->Assign(NS_REINTERPRET_CAST(const PRUnichar*,
                                              ::JS_GetStringChars(jsstring)),
                          ::JS_GetStringLength(jsstring));
      } else {
        rv = NS_ERROR_OUT_OF_MEMORY;
      }
    }
  } else {
    if (aIsUndefined)
      *aIsUndefined = PR_TRUE;
    if (aRetValue)
      aRetValue->Truncate();
  }

  ScriptEvaluated(PR_TRUE);

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  return rv;
}

const char *gEventArgv[] = {"event"};

void
AtomToEventHandlerName(nsIAtom *aName, char *charName, PRUint32 charNameSize)
{
  // optimized to avoid ns*Str*.h explicit/implicit copying and malloc'ing
  // even nsCAutoString may call an Append that copy-constructs an nsStr from
  // a const PRUnichar*
  const PRUnichar *name;
  aName->GetUnicode(&name);
  char c;
  PRUint32 i = 0;

  do {
    NS_ASSERTION(name[i] < 128, "non-ASCII event handler name");
    c = char(name[i]);

    // The HTML content sink must have folded to lowercase already.
    NS_ASSERTION(c == '\0' || ('a' <= c && c <= 'z'), "non-alphabetic event handler name");

    NS_ASSERTION(i < charNameSize, "overlong event handler name");
    charName[i++] = c;
  } while (c != '\0');
}

NS_IMETHODIMP
nsJSContext::CompileEventHandler(void *aTarget, nsIAtom *aName,
                                 const nsAReadableString& aBody,
                                 PRBool aShared, void** aHandler)
{
  JSPrincipals *jsprin = nsnull;

  nsCOMPtr<nsIScriptGlobalObject> global;
  GetGlobalObject(getter_AddRefs(global));
  if (global) {
    // XXXbe why the two-step QI? speed up via a new GetGlobalObjectData func?
    nsCOMPtr<nsIScriptObjectPrincipal> globalData = do_QueryInterface(global);
    if (globalData) {
      nsCOMPtr<nsIPrincipal> prin;
      if (NS_FAILED(globalData->GetPrincipal(getter_AddRefs(prin))))
        return NS_ERROR_FAILURE;
      prin->GetJSPrincipals(&jsprin);
    }
  }

  char charName[64];
  AtomToEventHandlerName(aName, charName, sizeof charName);

  JSObject *target = (JSObject*)aTarget;
  JSFunction* fun =
      ::JS_CompileUCFunctionForPrincipals(mContext, target, jsprin,
                                          charName, 1, gEventArgv,
                                          (jschar*)(const PRUnichar*)PromiseFlatString(aBody).get(),
                                          aBody.Length(),
                                          //XXXbe filename, lineno:
                                          nsnull, 0);

  if (jsprin)
    JSPRINCIPALS_DROP(mContext, jsprin);
  if (!fun)
    return NS_ERROR_FAILURE;

  JSObject *handler = ::JS_GetFunctionObject(fun);
  if (aHandler)
    *aHandler = (void*) handler;

  if (aShared) {
    /* Break scope link to avoid entraining shared compilation scope. */
    ::JS_SetParent(mContext, handler, nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::CompileFunction(void* aTarget,
                             const nsCString& aName,
                             PRUint32 aArgCount,
                             const char** aArgArray,
                             const nsAReadableString& aBody,
                             const char* aURL,
                             PRUint32 aLineNo,
                             PRBool aShared,
                             void** aFunctionObject)
{
  JSPrincipals *jsprin = nsnull;

  nsCOMPtr<nsIScriptGlobalObject> global;
  GetGlobalObject(getter_AddRefs(global));
  if (global) {
    // XXXbe why the two-step QI? speed up via a new GetGlobalObjectData func?
    nsCOMPtr<nsIScriptObjectPrincipal> globalData = do_QueryInterface(global);
    if (globalData) {
      nsCOMPtr<nsIPrincipal> prin;
      if (NS_FAILED(globalData->GetPrincipal(getter_AddRefs(prin))))
        return NS_ERROR_FAILURE;
      prin->GetJSPrincipals(&jsprin);
    }
  }

  JSObject *target = (JSObject*)aTarget;
  JSFunction* fun =
      ::JS_CompileUCFunctionForPrincipals(mContext, target, jsprin,
                                          aName, aArgCount, aArgArray,
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

NS_IMETHODIMP
nsJSContext::CallEventHandler(void *aTarget, void *aHandler, PRUint32 argc,
                              void *argv, PRBool *aBoolResult, PRBool aReverseReturnResult)
{
  // This one's a lot easier than EvaluateString because we don't have to
  // hassle with principals: they're already compiled into the JS function.
  nsresult rv;
  nsCOMPtr<nsIScriptSecurityManager> securityManager;
  rv = GetSecurityManager(getter_AddRefs(securityManager));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIJSContextStack> stack = 
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext)))
    return NS_ERROR_FAILURE;

  // this context can be deleted unexpectedly if the JS closes
  // the owning window. we ran into this problem specifically
  // when going through the "close window" key event handler
  // (that is, hitting ^W on Windows). the addref just below
  // prevents our untimely destruction.
  nsCOMPtr<nsIScriptContext> kungFuDeathGrip(this);
  mTerminationFuncArg = nsnull;
  mTerminationFunc = nsnull;

  // check if the event handler can be run on the object in question
  rv = securityManager->CheckFunctionAccess(mContext, aHandler, aTarget);

  if (NS_SUCCEEDED(rv)) {
    jsval val;
    jsval funval = OBJECT_TO_JSVAL(aHandler);
    PRBool ok = ::JS_CallFunctionValue(mContext, (JSObject *)aTarget, funval,
                                argc, (jsval *)argv, &val);
    *aBoolResult = ok
                   ? !JSVAL_IS_BOOLEAN(val) || (aReverseReturnResult ? !JSVAL_TO_BOOLEAN(val) : JSVAL_TO_BOOLEAN(val))
                   : JS_TRUE;

    ScriptEvaluated(PR_TRUE);
  }

  if (NS_FAILED(stack->Pop(nsnull)))
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::BindCompiledEventHandler(void *aTarget, nsIAtom *aName, void *aHandler)
{
  char charName[64];
  AtomToEventHandlerName(aName, charName, sizeof charName);

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

NS_IMETHODIMP
nsJSContext::SetDefaultLanguageVersion(const char* aVersion)
{
  (void) ::JS_SetVersion(mContext, ::JS_StringToVersion(aVersion));
  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::GetGlobalObject(nsIScriptGlobalObject** aGlobalObject)
{
  JSObject *global = ::JS_GetGlobalObject(mContext);
  *aGlobalObject = nsnull;

  if (!global) {
    NS_WARNING("Context has no global.");
    return NS_OK;
  }

  JSClass *c = JS_GET_CLASS(mContext, global);

  if (!c || ((~c->flags) & (JSCLASS_HAS_PRIVATE |
                            JSCLASS_PRIVATE_IS_NSISUPPORTS))) {
    NS_WARNING("Global is not an nsISupports.");
    return NS_OK;
  }

  nsCOMPtr<nsISupports> native =
    (nsISupports *)::JS_GetPrivate(mContext, global);

  nsCOMPtr<nsIXPConnectWrappedNative> wrapped_native =
    do_QueryInterface(native);

  if (wrapped_native) {
    // The global object is a XPConnect wrapped native, the native in
    // the wrapper might be the nsIScriptGlobalObject

    wrapped_native->GetNative(getter_AddRefs(native));
  }

  if (!native) {
    NS_WARNING("XPConnect wrapped native doesn't wrap anything.");
    return NS_OK;
  }

  // We have private data (either directly from ::JS_GetPrivate() or
  // through wrapped_native->GetNative()), check if it's a
  // nsIScriptGlobalObject

  return CallQueryInterface(native, aGlobalObject);
}

NS_IMETHODIMP_(void*)
nsJSContext::GetNativeContext()
{
  return (void *)mContext;
}


NS_IMETHODIMP
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

  nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID(), &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject *global = ::JS_GetGlobalObject(mContext);

  // If there's already a global object in mContext we won't tell
  // XPConnect to wrap aGlobalObject since it's already wrapped.

  if (!global) {
    nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

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
      nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

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

nsresult
nsJSContext::InitClasses()
{
  nsresult rv = NS_OK;
  JSObject *globalObj = ::JS_GetGlobalObject(mContext);

  rv = InitializeExternalClasses();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = InitializeLiveConnectClasses();
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

  return rv;
}

NS_IMETHODIMP
nsJSContext::IsContextInitialized()
{
  return (mIsInitialized) ? NS_OK : NS_ERROR_NOT_INITIALIZED;
}

NS_IMETHODIMP
nsJSContext::GC()
{
  ::JS_GC(mContext);
  return NS_OK;
}

NS_IMETHODIMP
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

  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::GetSecurityManager(nsIScriptSecurityManager **aInstancePtr)
{
  if (!mSecurityManager) {
    nsresult rv = NS_OK;

    mSecurityManager = do_GetService(kScriptSecurityManagerContractID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  *aInstancePtr = mSecurityManager;
  NS_ADDREF(*aInstancePtr);

  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::SetOwner(nsIScriptContextOwner* owner)
{
  // The owner should not be addrefed!! We'll be told
  // when the owner goes away.
  mOwner = owner;

  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::GetOwner(nsIScriptContextOwner** owner)
{
  *owner = mOwner;
  NS_IF_ADDREF(*owner);

  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                    nsISupports* aRef)
{
  mTerminationFunc = aFunc;
  mTerminationFuncArg = aRef;

  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::GetScriptsEnabled(PRBool *aEnabled)
{
  *aEnabled = mScriptsEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::SetScriptsEnabled(PRBool aEnabled)
{
  mScriptsEnabled = aEnabled;
  return NS_OK;
}


NS_IMETHODIMP
nsJSContext::GetProcessingScriptTag(PRBool * aResult)
{
  *aResult = mProcessingScriptTag;
  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::SetProcessingScriptTag(PRBool aFlag) 
{
  mProcessingScriptTag = aFlag;
  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::SetGCOnDestruction(PRBool aGCOnDestruction)
{
  mGCOnDestruction = aGCOnDestruction;

  return NS_OK;
}

NS_IMETHODIMP
nsJSContext::ScriptExecuted()
{
  return ScriptEvaluated(PR_FALSE);
}

nsJSEnvironment *nsJSEnvironment::sTheEnvironment = nsnull;

nsJSEnvironment *
nsJSEnvironment::GetScriptingEnvironment()
{
  if (nsnull == sTheEnvironment) {
    sTheEnvironment = new nsJSEnvironment();
    NS_IF_ADDREF(sTheEnvironment); // released in |Observe|
  }
  return sTheEnvironment;
}

const char kJSRuntimeServiceContractID[] = "@mozilla.org/js/xpc/RuntimeService;1";
static int globalCount;
static PRThread *gDOMThread;

static JSGCCallback gOldJSGCCallback;

static JSBool JS_DLL_CALLBACK
DOMGCCallback(JSContext *cx, JSGCStatus status)
{
  if (status == JSGC_BEGIN && PR_GetCurrentThread() != gDOMThread)
    return JS_FALSE;
  return gOldJSGCCallback ? gOldJSGCCallback(cx, status) : JS_TRUE;
}

nsJSEnvironment::nsJSEnvironment()
{
  NS_INIT_ISUPPORTS();

  // So that we get deleted on XPCOM shutdown, set up an
  // observer.
  nsresult rv;
  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  NS_ASSERTION(NS_SUCCEEDED(rv), "going to leak a nsJSEnvironment");
  if (NS_SUCCEEDED(rv))
  {
    observerService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
  }

  mRuntimeService = nsnull;
  rv = nsServiceManager::GetService(kJSRuntimeServiceContractID,
                                    NS_GET_IID(nsIJSRuntimeService),
                                    (nsISupports**)&mRuntimeService);
  // get the JSRuntime from the runtime svc, if possible
  if (NS_FAILED(rv))
    return;                     // XXX swallow error! need Init()?
  rv = mRuntimeService->GetRuntime(&mRuntime);
  if (NS_FAILED(rv))
    return;                     // XXX swallow error! need Init()?

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

  NS_ASSERTION(!gOldJSGCCallback, "nsJSEnvironment created more than once");
  gOldJSGCCallback = ::JS_SetGCCallbackRT(mRuntime, DOMGCCallback);

  // Set these global xpconnect options...
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  if (NS_SUCCEEDED(rv)) {
    xpc->SetCollectGarbageOnMainThreadOnly(PR_TRUE); 
    xpc->SetDeferReleasesUntilAfterGarbageCollection(PR_TRUE); 
  } else {
    NS_WARNING("Failed to get XPConnect service!");
  }

  // Initialize LiveConnect.  XXXbe use contractid rather than GetCID
  nsCOMPtr<nsILiveConnectManager> manager = 
           do_GetService(nsIJVMManager::GetCID(), &rv);

  // Should the JVM manager perhaps define methods for starting up LiveConnect?
  if (NS_SUCCEEDED(rv) && manager != nsnull) {
    PRBool started = PR_FALSE;
    rv = manager->StartupLiveConnect(mRuntime, started);
  }

  globalCount++;
}

nsJSEnvironment::~nsJSEnvironment()
{
  if (--globalCount == 0) {
    nsJSUtils::ClearCachedSecurityManager();

    delete gNameSpaceManager;
    gNameSpaceManager = nsnull;
  }

  if (mRuntimeService)
    nsServiceManager::ReleaseService(kJSRuntimeServiceContractID,
                                     mRuntimeService);
}

NS_IMPL_ISUPPORTS1(nsJSEnvironment,nsIObserver);

NS_IMETHODIMP nsJSEnvironment::Observe(nsISupports *aSubject,
                                       const char *aTopic,
                                       const PRUnichar *someData)
{
#ifdef DEBUG
  if (nsCRT::strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
    NS_ASSERTION(0,"not shutdown");
#endif
  NS_RELEASE_THIS(); // release ref from |GetScriptingEnvironment|
  return NS_OK;
}

nsIScriptContext* nsJSEnvironment::GetNewContext()
{
  nsIScriptContext *context;
  context = new nsJSContext(mRuntime);
  NS_ADDREF(context);
  return context;
}

nsresult
NS_CreateScriptContext(nsIScriptGlobalObject *aGlobal,
                       nsIScriptContext **aContext)
{
  nsJSEnvironment *environment = nsJSEnvironment::GetScriptingEnvironment();
  if (!environment)
    return NS_ERROR_OUT_OF_MEMORY;

  nsIScriptContext *scriptContext = environment->GetNewContext();
  if (!scriptContext)
    return NS_ERROR_OUT_OF_MEMORY;
  *aContext = scriptContext;

  // Bind the script context and the global object
  nsresult rv = scriptContext->InitContext(aGlobal);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aGlobal) {
    rv = aGlobal->SetContext(scriptContext);
  }

  return rv;
}

