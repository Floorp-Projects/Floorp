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
#include "nsGlobalWindow.h"

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

// The max number of delayed cycle collects..
#define NS_MAX_DELAYED_CCOLLECT     45
// The max number of user interaction notifications in inactive state before
// we try to call cycle collector more aggressively.
#define NS_CC_SOFT_LIMIT_INACTIVE   6
// The max number of user interaction notifications in active state before
// we try to call cycle collector more aggressively.
#define NS_CC_SOFT_LIMIT_ACTIVE     12
// When higher probability MaybeCC is used, the number of sDelayedCCollectCount
// is multiplied with this number.
#define NS_PROBABILITY_MULTIPLIER   3
// Cycle collector is never called more often than every NS_MIN_CC_INTERVAL
// milliseconds. Exceptions are low memory situation and memory pressure
// notification.
#define NS_MIN_CC_INTERVAL          10000 // ms
// If previous cycle collection collected more than this number of objects,
// the next collection will happen somewhat soon.
#define NS_COLLECTED_OBJECTS_LIMIT  5000
// CC will be called if GC has been called at least this number of times and
// there are at least NS_MIN_SUSPECT_CHANGES new suspected objects.
#define NS_MAX_GC_COUNT             5
#define NS_MIN_SUSPECT_CHANGES      10
// CC will be called if there are at least NS_MAX_SUSPECT_CHANGES new suspected
// objects.
#define NS_MAX_SUSPECT_CHANGES      100

// if you add statics here, add them to the list in nsJSRuntime::Startup

static PRUint32 sDelayedCCollectCount;
static PRUint32 sCCollectCount;
static PRBool sUserIsActive;
static PRTime sPreviousCCTime;
static PRUint32 sCollectedObjectsCounts;
static PRUint32 sSavedGCCount;
static PRUint32 sCCSuspectChanges;
static PRUint32 sCCSuspectedCount;
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

static JSGCCallback gOldJSGCCallback;

static PRBool sIsInitialized;
static PRBool sDidShutdown;

static PRInt32 sContextCount;

static PRTime sMaxScriptRunTime;
static PRTime sMaxChromeScriptRunTime;

static nsIScriptSecurityManager *sSecurityManager;

static nsICollation *gCollation;

static nsIUnicodeDecoder *gDecoder;

// nsUserActivityObserver observes user-interaction-active and
// user-interaction-inactive notifications. It counts the number of
// notifications and if the number is bigger than NS_CC_SOFT_LIMIT_ACTIVE
// (in case the current notification is user-interaction-active) or
// NS_CC_SOFT_LIMIT_INACTIVE (current notification is user-interaction-inactive)
// MaybeCC is called with aHigherParameter set to PR_TRUE, otherwise PR_FALSE.
//
// When moving from active state to inactive, nsJSContext::IntervalCC() is
// called unless the timer related to page load is active.

class nsUserActivityObserver : public nsIObserver
{
public:
  nsUserActivityObserver()
  : mUserActivityCounter(0), mOldCCollectCount(0) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
private:
  PRUint32 mUserActivityCounter;
  PRUint32 mOldCCollectCount;
};

NS_IMPL_ISUPPORTS1(nsUserActivityObserver, nsIObserver)

NS_IMETHODIMP
nsUserActivityObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                const PRUnichar* aData)
{
  if (mOldCCollectCount != sCCollectCount) {
    mOldCCollectCount = sCCollectCount;
    // Cycle collector was called between user interaction notifications, so
    // we can reset the counter.
    mUserActivityCounter = 0;
  }
  PRBool higherProbability = PR_FALSE;
  ++mUserActivityCounter;
  if (!strcmp(aTopic, "user-interaction-inactive")) {
#ifdef DEBUG_smaug
    printf("user-interaction-inactive\n");
#endif
    if (sUserIsActive) {
      sUserIsActive = PR_FALSE;
      if (!sGCTimer) {
        nsJSContext::IntervalCC();
        return NS_OK;
      }
    }
    higherProbability = (mUserActivityCounter > NS_CC_SOFT_LIMIT_INACTIVE);
  } else if (!strcmp(aTopic, "user-interaction-active")) {
#ifdef DEBUG_smaug
    printf("user-interaction-active\n");
#endif
    sUserIsActive = PR_TRUE;
    higherProbability = (mUserActivityCounter > NS_CC_SOFT_LIMIT_ACTIVE);
  } else if (!strcmp(aTopic, "xpcom-shutdown")) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, "user-interaction-active");
      obs->RemoveObserver(this, "user-interaction-inactive");
      obs->RemoveObserver(this, "xpcom-shutdown");
    }
    return NS_OK;
  }
  nsJSContext::MaybeCC(higherProbability);
  return NS_OK;
}

// nsCCMemoryPressureObserver observes the memory-pressure notifications
// and forces a cycle collection when it happens.

class nsCCMemoryPressureObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(nsCCMemoryPressureObserver, nsIObserver)

NS_IMETHODIMP
nsCCMemoryPressureObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                    const PRUnichar* aData)
{
  nsJSContext::CC(nsnull);
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

class nsAutoPoolRelease {
public:
  nsAutoPoolRelease(JSArenaPool *p, void *m) : mPool(p), mMark(m) {}
  ~nsAutoPoolRelease() { JS_ARENA_RELEASE(mPool, mMark); }
private:
  JSArenaPool *mPool;
  void *mMark;
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
    nsRefPtr<nsPresContext> presContext;
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

class ScriptErrorEvent : public nsRunnable
{
public:
  ScriptErrorEvent(nsIScriptGlobalObject* aScriptGlobal,
                   PRUint32 aLineNr, PRUint32 aColumn, PRUint32 aFlags,
                   const nsAString& aErrorMsg,
                   const nsAString& aFileName,
                   const nsAString& aSourceLine,
                   PRBool aDispatchEvent)
  : mScriptGlobal(aScriptGlobal), mLineNr(aLineNr), mColumn(aColumn),
    mFlags(aFlags), mErrorMsg(aErrorMsg), mFileName(aFileName),
    mSourceLine(aSourceLine), mDispatchEvent(aDispatchEvent) {}

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
        sHandlingScriptError = PR_TRUE; // Recursion prevention

        nsRefPtr<nsPresContext> presContext;
        docShell->GetPresContext(getter_AddRefs(presContext));

        if (presContext) {
          nsScriptErrorEvent errorevent(PR_TRUE, NS_LOAD_ERROR);

          errorevent.fileName = mFileName.get();

          nsCOMPtr<nsIScriptObjectPrincipal> sop(do_QueryInterface(win));
          NS_ENSURE_STATE(sop);
          nsIPrincipal* p = sop->GetPrincipal();
          NS_ENSURE_STATE(p);

          PRBool sameOrigin = mFileName.IsVoid();

          if (p && !sameOrigin) {
            nsCOMPtr<nsIURI> errorURI;
            NS_NewURI(getter_AddRefs(errorURI), mFileName);
            if (errorURI) {
              // FIXME: Once error reports contain the origin of the
              // error (principals) we should change this to do the
              // security check based on the principals and not
              // URIs. See bug 387476.
              sameOrigin = NS_SUCCEEDED(p->CheckMayLoad(errorURI, PR_FALSE));
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

        sHandlingScriptError = PR_FALSE;
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

        rv = errorObject->Init(mErrorMsg.get(), mFileName.get(),
                               mSourceLine.get(),
                               mLineNr, mColumn, mFlags,
                               category);

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
  PRBool                          mDispatchEvent;

  static PRBool sHandlingScriptError;
};

PRBool ScriptErrorEvent::sHandlingScriptError = PR_FALSE;

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
        fileName.SetIsVoid(PR_TRUE);
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
      nsContentUtils::AddScriptRunner(
        new ScriptErrorEvent(globalObject, report->lineno,
                             report->uctokenptr - report->uclinebuf,
                             report->flags, msg, fileName, sourceLine,
                             report->errorNumber != JSMSG_OUT_OF_MEMORY));
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

static JSBool
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
                             reinterpret_cast<jschar*>(unichars),
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

static JSBool
LocaleToUpperCase(JSContext *cx, JSString *src, jsval *rval)
{
  return ChangeCase(cx, src, rval, ToUpperCase);
}

static JSBool
LocaleToLowerCase(JSContext *cx, JSString *src, jsval *rval)
{
  return ChangeCase(cx, src, rval, ToLowerCase);
}

static JSBool
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

static void
MaybeGC(JSContext *cx)
{
  size_t bytes = cx->runtime->gcBytes;
  size_t lastBytes = cx->runtime->gcLastBytes;
  if ((bytes > 8192 && bytes > lastBytes * 16)
#ifdef DEBUG
      || cx->runtime->gcZeal > 0
#endif
      ) {
    JS_GC(cx);
  }
}

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

  MaybeGC(cx);

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
  PRBool isTrackingChromeCodeTime =
    ::JS_IsSystemObject(cx, ::JS_GetGlobalObject(cx));
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
  PRBool debugPossible = (fp != nsnull && cx->debugHooks &&
                          cx->debugHooks->debuggerHandler != nsnull);
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
  PRBool neverShowDlgChk = PR_FALSE;
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
      nsIPrefBranch *prefBranch = nsContentUtils::GetPrefBranch();

      if (prefBranch) {
        prefBranch->SetIntPref(isTrackingChromeCodeTime ?
                               "dom.max_chrome_script_run_time" :
                               "dom.max_script_run_time", 0);
      }
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
        fp->setReturnValue(js::Valueify(rval));
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
static const char js_zeal_option_str[]   = JS_OPTIONS_DOT_STR "gczeal";
#endif
static const char js_tracejit_content_str[]   = JS_OPTIONS_DOT_STR "tracejit.content";
static const char js_tracejit_chrome_str[]    = JS_OPTIONS_DOT_STR "tracejit.chrome";
static const char js_methodjit_content_str[]   = JS_OPTIONS_DOT_STR "methodjit.content";
static const char js_methodjit_chrome_str[]    = JS_OPTIONS_DOT_STR "methodjit.chrome";

int
nsJSContext::JSOptionChangedCallback(const char *pref, void *data)
{
  nsJSContext *context = reinterpret_cast<nsJSContext *>(data);
  PRUint32 oldDefaultJSOptions = context->mDefaultJSOptions;
  PRUint32 newDefaultJSOptions = oldDefaultJSOptions;

  PRBool strict = nsContentUtils::GetBoolPref(js_strict_option_str);
  if (strict)
    newDefaultJSOptions |= JSOPTION_STRICT;
  else
    newDefaultJSOptions &= ~JSOPTION_STRICT;

  nsIScriptGlobalObject *global = context->GetGlobalObject();
  // XXX should we check for sysprin instead of a chrome window, to make
  // XXX components be covered by the chrome pref instead of the content one?
  nsCOMPtr<nsIDOMChromeWindow> chromeWindow(do_QueryInterface(global));

  PRBool useTraceJIT = nsContentUtils::GetBoolPref(chromeWindow ?
                                                   js_tracejit_chrome_str :
                                                   js_tracejit_content_str);
  PRBool useMethodJIT = nsContentUtils::GetBoolPref(chromeWindow ?
                                                    js_methodjit_chrome_str :
                                                    js_methodjit_content_str);
  nsCOMPtr<nsIXULRuntime> xr = do_GetService(XULRUNTIME_SERVICE_CONTRACTID);
  if (xr) {
    PRBool safeMode = PR_FALSE;
    xr->GetInSafeMode(&safeMode);
    if (safeMode) {
      useTraceJIT = PR_FALSE;
      useMethodJIT = PR_FALSE;
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

#ifdef DEBUG
  // In debug builds, warnings are enabled in chrome context if javascript.options.strict.debug is true
  PRBool strictDebug = nsContentUtils::GetBoolPref(js_strict_debug_option_str);
  // Note this callback is also called from context's InitClasses thus we don't
  // need to enable this directly from InitContext
  if (strictDebug && (newDefaultJSOptions & JSOPTION_STRICT) == 0) {
    if (chromeWindow)
      newDefaultJSOptions |= JSOPTION_STRICT;
  }
#endif

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

#ifdef JS_GC_ZEAL
  PRInt32 zeal = nsContentUtils::GetIntPref(js_zeal_option_str, -1);
  if (zeal >= 0)
    ::JS_SetGCZeal(context->mContext, (PRUint8)zeal);
#endif

  return 0;
}

nsJSContext::nsJSContext(JSRuntime *aRuntime)
  : mGCOnDestruction(PR_TRUE),
    mExecuteDepth(0)
{

  ++sContextCount;

  mDefaultJSOptions = JSOPTION_PRIVATE_IS_NSISUPPORTS | JSOPTION_ANONFUNFIX;

  mContext = ::JS_NewContext(aRuntime, gStackSize);
  if (mContext) {
    ::JS_SetContextPrivate(mContext, static_cast<nsIScriptContext *>(this));

    // Preserve any flags the context callback might have set.
    mDefaultJSOptions |= ::JS_GetOptions(mContext);

    // Make sure the new context gets the default context options
    ::JS_SetOptions(mContext, mDefaultJSOptions);

    // Watch for the JS boolean options
    nsContentUtils::RegisterPrefCallback(js_options_dot_str,
                                         JSOptionChangedCallback,
                                         this);

    ::JS_SetOperationCallback(mContext, DOMOperationCallback);

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
  mOperationCallbackTime = 0;
  mModalStateTime = 0;
  mModalStateDepth = 0;
  mProcessingScriptTag = PR_FALSE;
}

nsJSContext::~nsJSContext()
{
#ifdef DEBUG
  nsCycleCollector_DEBUG_wasFreed(static_cast<nsIScriptContext*>(this));
#endif
  NS_PRECONDITION(!mTerminations, "Shouldn't have termination funcs by now");

  mGlobalObjectRef = nsnull;

  DestroyJSContext();

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

void
nsJSContext::DestroyJSContext()
{
  if (!mContext)
    return;

  // Clear our entry in the JSContext, bugzilla bug 66413
  ::JS_SetContextPrivate(mContext, nsnull);

  // Unregister our "javascript.options.*" pref-changed callback.
  nsContentUtils::UnregisterPrefCallback(js_options_dot_str,
                                         JSOptionChangedCallback,
                                         this);

  PRBool do_gc = mGCOnDestruction && !sGCTimer && sReadyForGC;

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
NS_IMPL_CYCLE_COLLECTION_ROOT_BEGIN(nsJSContext)
  NS_ASSERTION(!tmp->mContext || tmp->mContext->outstandingRequests == 0,
               "Trying to unlink a context with outstanding requests.");
  tmp->mIsInitialized = PR_FALSE;
  tmp->mGCOnDestruction = PR_FALSE;
  tmp->DestroyJSContext();
NS_IMPL_CYCLE_COLLECTION_ROOT_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsJSContext)
NS_IMPL_CYCLE_COLLECTION_TRACE_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsJSContext)
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


NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsJSContext, nsIScriptContext)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsJSContext, nsIScriptContext)

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
                                     void *aScopeObject,
                                     nsIPrincipal *aPrincipal,
                                     const char *aURL,
                                     PRUint32 aLineNo,
                                     PRUint32 aVersion,
                                     void* aRetValue,
                                     PRBool* aIsUndefined)
{
  NS_TIME_FUNCTION_MIN_FMT(1.0, "%s (line %d) (url: %s, line: %d)", MOZ_FUNCTION_NAME,
                           __LINE__, aURL, aLineNo);

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

  rv = sSecurityManager->PushContextPrincipal(mContext, nsnull, principal);
  NS_ENSURE_SUCCESS(rv, rv);

  nsJSContext::TerminationFuncHolder holder(this);

  // SecurityManager said "ok", but don't compile if aVersion is unknown.
  // Since the caller is responsible for parsing the version strings, we just
  // check it isn't JSVERSION_UNKNOWN.
  if (ok && ((JSVersion)aVersion) != JSVERSION_UNKNOWN) {

    JSAutoRequest ar(mContext);

    JSAutoEnterCompartment ac;
    if (!ac.enter(mContext, (JSObject *)aScopeObject)) {
      JSPRINCIPALS_DROP(mContext, jsprin);
      stack->Pop(nsnull);
      return NS_ERROR_FAILURE;
    }

    ++mExecuteDepth;

    ok = ::JS_EvaluateUCScriptForPrincipalsVersion(mContext,
                                                   (JSObject *)aScopeObject,
                                                   jsprin,
                                                   (jschar*)PromiseFlatString(aScript).get(),
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

    *static_cast<jsval*>(aRetValue) = val;
    // XXX - nsScriptObjectHolder should be used once this method moves to
    // the new world order. However, use of 'jsval' appears to make this
    // tricky...
  }
  else {
    if (aIsUndefined) {
      *aIsUndefined = PR_TRUE;
    }
  }

  sSecurityManager->PopContextPrincipal(mContext);

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
    result->Assign(reinterpret_cast<const PRUnichar*>
                                   (::JS_GetStringChars(jsstring)),
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
                            void *aScopeObject,
                            nsIPrincipal *aPrincipal,
                            const char *aURL,
                            PRUint32 aLineNo,
                            PRUint32 aVersion,
                            nsAString *aRetValue,
                            PRBool* aIsUndefined)
{
  NS_TIME_FUNCTION_MIN_FMT(1.0, "%s (line %d) (url: %s, line: %d)", MOZ_FUNCTION_NAME,
                           __LINE__, aURL, aLineNo);

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
  if (ok && ((JSVersion)aVersion) != JSVERSION_UNKNOWN) {
    JSAutoRequest ar(mContext);
    JSAutoEnterCompartment ac;
    if (!ac.enter(mContext, (JSObject *)aScopeObject)) {
      stack->Pop(nsnull);
      JSPRINCIPALS_DROP(mContext, jsprin);
      return NS_ERROR_FAILURE;
    }

    ok = ::JS_EvaluateUCScriptForPrincipalsVersion(mContext,
                                                   (JSObject *)aScopeObject,
                                                   jsprin,
                                                   (jschar*)PromiseFlatString(aScript).get(),
                                                   aScript.Length(),
                                                   aURL,
                                                   aLineNo,
                                                   vp,
                                                   JSVersion(aVersion));

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

  --mExecuteDepth;

  sSecurityManager->PopContextPrincipal(mContext);

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
  // Since the caller is responsible for parsing the version strings, we just
  // check it isn't JSVERSION_UNKNOWN.
  if (ok && ((JSVersion)aVersion) != JSVERSION_UNKNOWN) {
    JSAutoRequest ar(mContext);

    JSScript* script =
        ::JS_CompileUCScriptForPrincipalsVersion(mContext,
                                                 (JSObject *)aScopeObject,
                                                 jsprin,
                                                 (jschar*) aText,
                                                 aTextLength,
                                                 aURL,
                                                 aLineNo,
                                                 JSVersion(aVersion));
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
    } else {
      rv = NS_ERROR_OUT_OF_MEMORY;
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
  // not be a GC root currently, provided we run the GC only from the
  // operation callback or from ScriptEvaluated.
  jsval val;
  JSBool ok;

  JSObject *scriptObj = (JSObject*)aScriptObject;
  nsCOMPtr<nsIPrincipal> principal;

  rv = sSecurityManager->GetObjectPrincipal(mContext, scriptObj, getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = sSecurityManager->PushContextPrincipal(mContext, nsnull, principal);
  NS_ENSURE_SUCCESS(rv, rv);

  nsJSContext::TerminationFuncHolder holder(this);
  JSAutoRequest ar(mContext);
  ++mExecuteDepth;
  ok = ::JS_ExecuteScript(mContext,
                          (JSObject *)aScopeObject,
                          (JSScript*)::JS_GetPrivate(mContext, scriptObj),
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
  }

  --mExecuteDepth;

  sSecurityManager->PopContextPrincipal(mContext);

  // Pop here, after JS_ValueToString and any other possible evaluation.
  if (NS_FAILED(stack->Pop(nsnull)))
    rv = NS_ERROR_FAILURE;

  // ScriptEvaluated needs to come after we pop the stack
  ScriptEvaluated(PR_TRUE);

  return rv;
}


#ifdef DEBUG
PRBool
AtomIsEventHandlerName(nsIAtom *aName)
{
  const PRUnichar *name = aName->GetUTF16String();

  const PRUnichar *cp;
  PRUnichar c;
  for (cp = name; *cp != '\0'; ++cp)
  {
    c = *cp;
    if ((c < 'A' || c > 'Z') && (c < 'a' || c > 'z'))
      return PR_FALSE;
  }

  return PR_TRUE;
}
#endif

// Helper function to find the JSObject associated with a (presumably DOM)
// interface.
nsresult
nsJSContext::JSObjectFromInterface(nsISupports* aTarget, void *aScope, JSObject **aRet)
{
  // It is legal to specify a null target.
  if (!aTarget) {
      *aRet = nsnull;
      return NS_OK;
  }

  // Get the jsobject associated with this target
  // We don't wrap here because we trust the JS engine to wrap the target
  // later.
  nsresult rv;
  jsval v;
  rv = nsContentUtils::WrapNative(mContext, (JSObject *)aScope, aTarget, &v);
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
                             PRBool aShared,
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
nsJSContext::CallEventHandler(nsISupports* aTarget, void *aScope, void *aHandler,
                              nsIArray *aargv, nsIVariant **arv)
{
  NS_ENSURE_TRUE(mIsInitialized, NS_ERROR_NOT_INITIALIZED);

  if (!mScriptsEnabled) {
    return NS_OK;
  }

  NS_TIME_FUNCTION_FMT(1.0, "%s (line %d) (function: %s)", MOZ_FUNCTION_NAME,
                       __LINE__, JS_GetFunctionName(static_cast<JSFunction *>(JS_GetPrivate(mContext, static_cast<JSObject *>(aHandler)))));

 
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

  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext)))
    return NS_ERROR_FAILURE;

  // check if the event handler can be run on the object in question
  rv = sSecurityManager->CheckFunctionAccess(mContext, aHandler, target);

  nsJSContext::TerminationFuncHolder holder(this);

  if (NS_SUCCEEDED(rv)) {
    // Convert args to jsvals.
    PRUint32 argc = 0;
    jsval *argv = nsnull;

    js::LazilyConstructed<nsAutoPoolRelease> poolRelease;
    js::LazilyConstructed<js::AutoArrayRooter> tvr;

    // Use |target| as the scope for wrapping the arguments, since aScope is
    // the safe scope in many cases, which isn't very useful.  Wrapping aTarget
    // was OK because those typically have PreCreate methods that give them the
    // right scope anyway, and we want to make sure that the arguments end up
    // in the same scope as aTarget.
    rv = ConvertSupportsTojsvals(aargv, target, &argc,
                                 &argv, poolRelease, tvr);
    if (NS_FAILED(rv)) {
      stack->Pop(nsnull);
      return rv;
    }

    jsval funval = OBJECT_TO_JSVAL(static_cast<JSObject *>(aHandler));
    JSAutoEnterCompartment ac;
    if (!ac.enter(mContext, target)) {
      stack->Pop(nsnull);
      return NS_ERROR_FAILURE;
    }

    ++mExecuteDepth;
    PRBool ok = ::JS_CallFunctionValue(mContext, target,
                                       funval, argc, argv, &rval);
    --mExecuteDepth;

    if (!ok) {
      // Tell XPConnect about any pending exceptions. This is needed
      // to avoid dropping JS exceptions in case we got here through
      // nested calls through XPConnect.

      ReportPendingException();

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
  if (NS_SUCCEEDED(rv)) {
    if (rval == JSVAL_NULL)
      *arv = nsnull;
    else
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

  NS_PRECONDITION(AtomIsEventHandlerName(aName), "Bad event name");
  nsresult rv;

  // Get the jsobject associated with this target
  JSObject *target = nsnull;
  nsAutoGCRoot root(&target, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = JSObjectFromInterface(aTarget, aScope, &target);
  NS_ENSURE_SUCCESS(rv, rv);

  JSObject *funobj = (JSObject*) aHandler;
  JSAutoRequest ar(mContext);

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

  // Push our JSContext on our thread's context stack, in case native code
  // called from JS calls back into JS via XPConnect.
  nsCOMPtr<nsIJSContextStack> stack =
           do_GetService("@mozilla.org/js/xpc/ContextStack;1", &rv);
  if (NS_FAILED(rv) || NS_FAILED(stack->Push(mContext))) {
    return NS_ERROR_FAILURE;
  }

  // Make sure the handler function is parented by its event target object
  if (funobj) { // && ::JS_GetParent(mContext, funobj) != target) {
    funobj = ::JS_CloneFunctionObject(mContext, funobj, target);
    if (!funobj)
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (NS_SUCCEEDED(rv) &&
      // Make sure the flags here match those in nsEventReceiverSH::NewResolve
      !::JS_DefineProperty(mContext, target, nsAtomCString(aName).get(),
                           OBJECT_TO_JSVAL(funobj), nsnull, nsnull,
                           JSPROP_ENUMERATE | JSPROP_PERMANENT)) {
    ReportPendingException();
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
    NS_PRECONDITION(AtomIsEventHandlerName(aName), "Bad event name");

    nsresult rv;
    JSObject *obj = nsnull;
    nsAutoGCRoot root(&obj, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    JSAutoRequest ar(mContext);
    rv = JSObjectFromInterface(aTarget, aScope, &obj);
    NS_ENSURE_SUCCESS(rv, rv);

    JSAutoEnterCompartment ac;
    if (!ac.enter(mContext, obj)) {
      return NS_ERROR_FAILURE;
    }

    jsval funval;
    if (!JS_LookupProperty(mContext, obj,
                           nsAtomCString(aName).get(), &funval))
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
    JSScript *script = reinterpret_cast<JSScript*>
                                       (::JS_GetPrivate(cx, mJSObject));
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
    JSObject *result = nsnull;
    nsresult rv;

    NS_TIME_FUNCTION_MIN(1.0);

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

  OBJ_TO_INNER_OBJECT(mContext, global);
  if (!global) {
    return nsnull;
  }

  JSClass *c = JS_GET_CLASS(mContext, global);

  if (!c || ((~c->flags) & (JSCLASS_HAS_PRIVATE |
                            JSCLASS_PRIVATE_IS_NSISUPPORTS))) {
    return nsnull;
  }

  JSAutoEnterCompartment ac;

  // NB: This AutoCrossCompartmentCall is only here to silence a warning. If
  // it fails, nothing bad will happen.
  ac.enterAndIgnoreErrors(mContext, global);

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
  nsCOMPtr<nsPIDOMWindow> pwin(do_QueryInterface(sgo));
  if (!pwin)
    return sgo;

  return static_cast<nsGlobalWindow *>(pwin->GetOuterWindow());
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
                                          EmptyCString(), flags,
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
  JSObject *newInnerJSObject = (JSObject *)aNewInner->GetScriptGlobal(JAVASCRIPT);
  JSObject *outerGlobal = (JSObject *)aOuterGlobal;

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

  // Now that we're connecting the outer global to the inner one,
  // we must have transplanted it. The JS engine tries to maintain
  // the global object's compartment as its default compartment,
  // so update that now since it might have changed.
  JS_SetGlobalObject(mContext, outerGlobal);

  // We do *not* want to use anything else out of the outer
  // object's prototype chain than the first prototype, which is
  // the XPConnect prototype. The rest we want from the inner
  // window's prototype, i.e. the global scope polluter and
  // Object.prototype. This way the outer also gets the benefits
  // of the global scope polluter, and the inner window's
  // Object.prototype.
  JSObject *proto = JS_GetPrototype(mContext, outerGlobal);
  JSObject *innerProto = JS_GetPrototype(mContext, newInnerJSObject);
  JSObject *innerProtoProto = JS_GetPrototype(mContext, innerProto);

  JS_SetPrototype(mContext, newInnerJSObject, proto);
  JS_SetPrototype(mContext, proto, innerProtoProto);

  return NS_OK;
}

void *
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

  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  if (!nsDOMClassInfo::GetXPCNativeWrapperGetPropertyOp()) {
    JSPropertyOp getProperty;
    xpc->GetNativeWrapperGetPropertyOp(&getProperty);
    nsDOMClassInfo::SetXPCNativeWrapperGetPropertyOp(getProperty);
  }

  if (!nsDOMClassInfo::GetXrayWrapperPropertyHolderGetPropertyOp()) {
    JSPropertyOp getProperty;
    xpc->GetXrayWrapperPropertyHolderGetPropertyOp(&getProperty);
    nsDOMClassInfo::SetXrayWrapperPropertyHolderGetPropertyOp(getProperty);
  }

  return NS_OK;
}

nsresult
nsJSContext::CreateOuterObject(nsIScriptGlobalObject *aGlobalObject,
                               nsIScriptGlobalObject *aCurrentInner)
{
  mGlobalObjectRef = aGlobalObject;

  nsCOMPtr<nsIDOMChromeWindow> chromeWindow(do_QueryInterface(aGlobalObject));
  PRUint32 flags = 0;

  if (chromeWindow) {
    // Flag this window's global object and objects under it as "system",
    // for optional automated XPCNativeWrapper construction when chrome JS
    // views a content DOM.
    flags = nsIXPConnect::FLAG_SYSTEM_GLOBAL_OBJECT;

    // Always enable E4X for XUL and other chrome content -- there is no
    // need to preserve the <!-- script hiding hack from JS-in-HTML daze
    // (introduced in 1995 for graceful script degradation in Netscape 1,
    // Mosaic, and other pre-JS browsers).
    JS_SetOptions(mContext, JS_GetOptions(mContext) | JSOPTION_XML);
  }

  nsIXPConnect *xpc = nsContentUtils::XPConnect();
  nsCOMPtr<nsIXPConnectJSObjectHolder> holder;

  nsresult rv = xpc->WrapNative(mContext, aCurrentInner->GetGlobalJSObject(),
                                aCurrentInner, NS_GET_IID(nsISupports),
                                getter_AddRefs(holder));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper(do_QueryInterface(holder));
  NS_ABORT_IF_FALSE(wrapper, "bad wrapper");

  wrapper->RefreshPrototype();

  JSObject *outer =
    NS_NewOuterWindowProxy(mContext, aCurrentInner->GetGlobalJSObject());
  if (!outer) {
    return NS_ERROR_FAILURE;
  }

  return SetOuterObject(outer);
}

nsresult
nsJSContext::SetOuterObject(void *aOuterObject)
{
  JSObject *outer = static_cast<JSObject *>(aOuterObject);

  // Force our context's global object to be the outer.
  JS_SetGlobalObject(mContext, outer);
  return NS_OK;
}

nsresult
nsJSContext::InitOuterWindow()
{
  JSObject *global = JS_GetGlobalObject(mContext);
  OBJ_TO_INNER_OBJECT(mContext, global);

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

  js::LazilyConstructed<nsAutoPoolRelease> poolRelease;
  js::LazilyConstructed<js::AutoArrayRooter> tvr;

  nsresult rv;
  rv = ConvertSupportsTojsvals(aArgs, GetNativeGlobal(), &argc,
                               &argv, poolRelease, tvr);
  NS_ENSURE_SUCCESS(rv, rv);

  jsval vargs;

  // got the arguments, now attach them.

  // window.dialogArguments is supposed to be an array if a JS array
  // was passed to showModalDialog(), deal with that here.
  if (strcmp(aPropName, "dialogArguments") == 0 && argc <= 1) {
    vargs = argc ? argv[0] : JSVAL_VOID;
  } else {
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
                                     void *aScope,
                                     PRUint32 *aArgc,
                                     jsval **aArgv,
                                     js::LazilyConstructed<nsAutoPoolRelease> &aPoolRelease,
                                     js::LazilyConstructed<js::AutoArrayRooter> &aRooter)
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

  void *mark = JS_ARENA_MARK(&mContext->tempPool);
  jsval *argv;
  size_t nbytes = argCount * sizeof(jsval);
  JS_ARENA_ALLOCATE_CAST(argv, jsval *, &mContext->tempPool, nbytes);
  NS_ENSURE_TRUE(argv, NS_ERROR_OUT_OF_MEMORY);
  memset(argv, 0, nbytes);  /* initialize so GC-able */

  // Use the caller's auto guards to release and unroot.
  aPoolRelease.construct(&mContext->tempPool, mark);
  aRooter.construct(mContext, argCount, argv);

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
          rv = nsContentUtils::WrapNative(mContext, (JSObject *)aScope, arg,
                                          &v, getter_AddRefs(wrapper));
          if (NS_SUCCEEDED(rv)) {
            *thisval = v;
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

static JSPropertySpec OptionsProperties[] = {
  {"strict",    (int8)JSOPTION_STRICT,   JSPROP_ENUMERATE | JSPROP_PERMANENT},
  {"werror",    (int8)JSOPTION_WERROR,   JSPROP_ENUMERATE | JSPROP_PERMANENT},
  {"relimit",   (int8)JSOPTION_RELIMIT,  JSPROP_ENUMERATE | JSPROP_PERMANENT},
  {0}
};

static JSBool
GetOptionsProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  if (JSID_IS_INT(id)) {
    uint32 optbit = (uint32) JSID_TO_INT(id);
    if (((optbit & (optbit - 1)) == 0 && optbit <= JSOPTION_WERROR) ||
          optbit == JSOPTION_RELIMIT)
      *vp = (JS_GetOptions(cx) & optbit) ? JSVAL_TRUE : JSVAL_FALSE;
  }
  return JS_TRUE;
}

static JSBool
SetOptionsProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
  if (JSID_IS_INT(id)) {
    uint32 optbit = (uint32) JSID_TO_INT(id);

    // Don't let options other than strict, werror, or relimit be set -- it
    // would be bad if web page script could clear
    // JSOPTION_PRIVATE_IS_NSISUPPORTS!
    if (((optbit & (optbit - 1)) == 0 && optbit <= JSOPTION_WERROR) ||
        optbit == JSOPTION_RELIMIT) {
      JSBool optval;
      JS_ValueToBoolean(cx, *vp, &optval);

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
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, nsnull
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
TraceMallocDisable(JSContext *cx, uintN argc, jsval *vp)
{
    NS_TraceMallocDisable();
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
TraceMallocEnable(JSContext *cx, uintN argc, jsval *vp)
{
    NS_TraceMallocEnable();
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
TraceMallocOpenLogFile(JSContext *cx, uintN argc, jsval *vp)
{
    int fd;
    JSString *str;
    char *filename;

    if (argc == 0) {
        fd = -1;
    } else {
        str = JS_ValueToString(cx, JS_ARGV(cx, vp)[0]);
        if (!str)
            return JS_FALSE;
        filename = JS_GetStringBytes(str);
        fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0) {
            JS_ReportError(cx, "can't open %s: %s", filename, strerror(errno));
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
    JSString *str;
    const char *caption;

    str = JS_ValueToString(cx, argc ? JS_ARGV(cx, vp)[0] : JSVAL_VOID);
    if (!str)
        return JS_FALSE;
    caption = JS_GetStringBytes(str);
    NS_TraceMallocLogTimestamp(caption);
    JS_SET_RVAL(cx, vp, JSVAL_VOID);
    return JS_TRUE;
}

static JSBool
TraceMallocDumpAllocations(JSContext *cx, uintN argc, jsval *vp)
{
    JSString *str;
    const char *pathname;

    str = JS_ValueToString(cx, argc ? JS_ARGV(cx, vp)[0] : JSVAL_VOID);
    if (!str)
        return JS_FALSE;
    pathname = JS_GetStringBytes(str);
    if (NS_TraceMallocDumpAllocations(pathname) < 0) {
        JS_ReportError(cx, "can't dump to %s: %s", pathname, strerror(errno));
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

inline PRBool
IsJProfAction(struct sigaction *action)
{
    return (action->sa_sigaction &&
            action->sa_flags == (SA_RESTART | SA_SIGINFO));
}

void NS_JProfStartProfiling();
void NS_JProfStopProfiling();

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
JProfStopProfilingJS(JSContext *cx, uintN argc, jsval *vp)
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
    {"JProfStartProfiling",        JProfStartProfilingJS,      0, 0},
    {"JProfStopProfiling",         JProfStopProfilingJS,       0, 0},
    {nsnull,                       nsnull,                     0, 0}
};

#endif /* defined(MOZ_JPROF) */

#ifdef MOZ_SHARK
static JSFunctionSpec SharkFunctions[] = {
    {"startShark",                 js_StartShark,              0, 0},
    {"stopShark",                  js_StopShark,               0, 0},
    {"connectShark",               js_ConnectShark,            0, 0},
    {"disconnectShark",            js_DisconnectShark,         0, 0},
    {nsnull,                       nsnull,                     0, 0}
};
#endif

#ifdef MOZ_CALLGRIND
static JSFunctionSpec CallgrindFunctions[] = {
    {"startCallgrind",             js_StartCallgrind,          0, 0},
    {"stopCallgrind",              js_StopCallgrind,           0, 0},
    {"dumpCallgrind",              js_DumpCallgrind,           1, 0},
    {nsnull,                       nsnull,                     0, 0}
};
#endif

#ifdef MOZ_VTUNE
static JSFunctionSpec VtuneFunctions[] = {
    {"startVtune",                 js_StartVtune,              1, 0},
    {"stopVtune",                  js_StopVtune,               0, 0},
    {"pauseVtune",                 js_PauseVtune,              0, 0},
    {"resumeVtune",                js_ResumeVtune,             0, 0},
    {nsnull,                       nsnull,                     0, 0}
};
#endif

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

#ifdef MOZ_SHARK
  // Attempt to initialize Shark functions
  ::JS_DefineFunctions(mContext, globalObj, SharkFunctions);
#endif

#ifdef MOZ_CALLGRIND
  // Attempt to initialize Callgrind functions
  ::JS_DefineFunctions(mContext, globalObj, CallgrindFunctions);
#endif

#ifdef MOZ_VTUNE
  // Attempt to initialize Vtune functions
  ::JS_DefineFunctions(mContext, globalObj, VtuneFunctions);
#endif

#ifdef MOZ_TRACEVIS
  // Attempt to initialize Ethogram functions
  ::JS_DefineFunctions(mContext, globalObj, EthogramFunctions);
#endif

  JSOptionChangedCallback(js_options_dot_str, this);

  return rv;
}

void
nsJSContext::ClearScope(void *aGlobalObj, PRBool aClearFromProtoChain)
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

    JS_ClearScope(mContext, obj);
    if (!obj->getParent()) {
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

#ifdef JS_GC_ZEAL
  if (mContext->runtime->gcZeal >= 2) {
    MaybeGC(mContext);
  } else
#endif
  if (mNumEvaluations > 20) {
    mNumEvaluations = 0;
    MaybeGC(mContext);
  }

  if (aTerminated) {
    mOperationCallbackTime = 0;
    mModalStateTime = 0;
  }
}

nsresult
nsJSContext::SetTerminationFunction(nsScriptTerminationFunc aFunc,
                                    nsISupports* aRef)
{
  NS_PRECONDITION(JS_IsRunning(mContext), "should be executing script");

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

PRBool
nsJSContext::GetExecutingScript()
{
  return JS_IsRunning(mContext) || mExecuteDepth > 0;
}

void
nsJSContext::SetGCOnDestruction(PRBool aGCOnDestruction)
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
nsJSContext::CC(nsICycleCollectorListener *aListener)
{
  NS_TIME_FUNCTION_MIN(1.0);

  ++sCCollectCount;
#ifdef DEBUG_smaug
  printf("Will run cycle collector (%i), %lldms since previous.\n",
         sCCollectCount, (PR_Now() - sPreviousCCTime) / PR_USEC_PER_MSEC);
#endif
  sPreviousCCTime = PR_Now();
  sDelayedCCollectCount = 0;
  sCCSuspectChanges = 0;
  // nsCycleCollector_collect() no longer forces a JS garbage collection,
  // so we have to do it ourselves here.
  nsContentUtils::XPConnect()->GarbageCollect();
  sCollectedObjectsCounts = nsCycleCollector_collect(aListener);
  sCCSuspectedCount = nsCycleCollector_suspectedCount();
  sSavedGCCount = JS_GetGCParameter(nsJSRuntime::sRuntime, JSGC_NUMBER);
#ifdef DEBUG_smaug
  printf("Collected %u objects, %u suspected objects, took %lldms\n",
         sCollectedObjectsCounts, sCCSuspectedCount,
         (PR_Now() - sPreviousCCTime) / PR_USEC_PER_MSEC);
#endif
}

static inline uint32
GetGCRunsSinceLastCC()
{
    // To avoid crash if nsJSRuntime is not properly initialized.
    // See the bug 474586
    if (!nsJSRuntime::sRuntime)
        return 0;

    // Since JS_GetGCParameter() and sSavedGCCount are unsigned, the following
    // gives the correct result even when the GC counter wraps around
    // UINT32_MAX since the last call to JS_GetGCParameter(). 
    return JS_GetGCParameter(nsJSRuntime::sRuntime, JSGC_NUMBER) -
           sSavedGCCount;
}

//static
PRBool
nsJSContext::MaybeCC(PRBool aHigherProbability)
{
  ++sDelayedCCollectCount;

  // Don't check suspected count if CC will be called anyway.
  if (sCCSuspectChanges <= NS_MIN_SUSPECT_CHANGES ||
      GetGCRunsSinceLastCC() <= NS_MAX_GC_COUNT) {
#ifdef DEBUG_smaug
    PRTime now = PR_Now();
#endif
    PRUint32 suspected = nsCycleCollector_suspectedCount();
#ifdef DEBUG_smaug
    printf("%u suspected objects (%lldms), sCCSuspectedCount %u\n",
            suspected, (PR_Now() - now) / PR_USEC_PER_MSEC,
            sCCSuspectedCount);
#endif
    // Update only when suspected count has increased.
    if (suspected > sCCSuspectedCount) {
      sCCSuspectChanges += (suspected - sCCSuspectedCount);
      sCCSuspectedCount = suspected;
    }
  }
#ifdef DEBUG_smaug
  printf("sCCSuspectChanges %u, GC runs %u\n",
         sCCSuspectChanges, GetGCRunsSinceLastCC());
#endif

  // Increase the probability also if the previous call to cycle collector
  // collected something.
  if (aHigherProbability ||
      sCollectedObjectsCounts > NS_COLLECTED_OBJECTS_LIMIT) {
    sDelayedCCollectCount *= NS_PROBABILITY_MULTIPLIER;
  }

  if (!sGCTimer &&
      (sDelayedCCollectCount > NS_MAX_DELAYED_CCOLLECT) &&
      ((sCCSuspectChanges > NS_MIN_SUSPECT_CHANGES &&
        GetGCRunsSinceLastCC() > NS_MAX_GC_COUNT) ||
       (sCCSuspectChanges > NS_MAX_SUSPECT_CHANGES))) {
    return IntervalCC();
  }
  return PR_FALSE;
}

//static
void
nsJSContext::CCIfUserInactive()
{
  if (sUserIsActive) {
    MaybeCC(PR_TRUE);
  } else {
    IntervalCC();
  }
}

//static
PRBool
nsJSContext::IntervalCC()
{
  if ((PR_Now() - sPreviousCCTime) >=
      PRTime(NS_MIN_CC_INTERVAL * PR_USEC_PER_MSEC)) {
    nsJSContext::CC(nsnull);
    return PR_TRUE;
  }
#ifdef DEBUG_smaug
  printf("Running CC was delayed because of NS_MIN_CC_INTERVAL.\n");
#endif
  return PR_FALSE;
}

// static
void
GCTimerFired(nsITimer *aTimer, void *aClosure)
{
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

    nsJSContext::CCIfUserInactive();
  } else {
    nsJSContext::FireGCTimer(PR_TRUE);
  }

  sReadyForGC = PR_TRUE;
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

    CCIfUserInactive();
  }
}

// static
void
nsJSContext::FireGCTimer(PRBool aLoadInProgress)
{
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

    CCIfUserInactive();
    return;
  }

  static PRBool first = PR_TRUE;

  sGCTimer->InitWithFuncCallback(GCTimerFired, nsnull,
                                 first ? NS_FIRST_GC_DELAY :
                                 aLoadInProgress ? NS_LOAD_IN_PROCESS_GC_DELAY :
                                                   NS_GC_DELAY,
                                 nsITimer::TYPE_ONE_SHOT);

  sLoadInProgressGCTimer = aLoadInProgress;

  first = PR_FALSE;
}

static JSBool
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

void
nsJSContext::ReportPendingException()
{
  // set aside the frame chain, since it has nothing to do with the
  // exception we're reporting.
  if (mIsInitialized && ::JS_IsExceptionPending(mContext)) {
    JSStackFrame* frame = JS_SaveFrameChain(mContext);
    ::JS_ReportPendingException(mContext);
    JS_RestoreFrameChain(mContext, frame);
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
  sDelayedCCollectCount = 0;
  sCCollectCount = 0;
  sUserIsActive = PR_FALSE;
  sPreviousCCTime = 0;
  sCollectedObjectsCounts = 0;
  sSavedGCCount = 0;
  sCCSuspectChanges = 0;
  sCCSuspectedCount = 0;
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

static int
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

static int
ReportAllJSExceptionsPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  PRBool reportAll = nsContentUtils::GetBoolPref(aPrefName, PR_FALSE);
  nsContentUtils::XPConnect()->SetReportAllJSExceptions(reportAll);
  return 0;
}

static int
SetMemoryHighWaterMarkPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  PRInt32 highwatermark = nsContentUtils::GetIntPref(aPrefName, 32);

  if (highwatermark >= 32) {
    /*
     * There are two ways to allocate memory in SpiderMonkey. One is
     * to use jsmalloc() and the other is to use GC-owned memory
     * (e.g. js_NewGCThing()).
     *
     * In the browser, we don't cap the amount of GC-owned memory.
     */
    JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_MAX_MALLOC_BYTES,
                      64L * 1024L * 1024L);
    JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_MAX_BYTES,
                      0xffffffff);
  } else {
    JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_MAX_MALLOC_BYTES,
                      highwatermark * 1024L * 1024L);
    JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_MAX_BYTES,
                      highwatermark * 1024L * 1024L);
  }
  return 0;
}

static int
SetMemoryGCFrequencyPrefChangedCallback(const char* aPrefName, void* aClosure)
{
  PRInt32 triggerFactor = nsContentUtils::GetIntPref(aPrefName, 1600);
  JS_SetGCParameter(nsJSRuntime::sRuntime, JSGC_TRIGGER_FACTOR, triggerFactor);
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

  NS_ASSERTION(!gOldJSGCCallback,
               "nsJSRuntime initialized more than once");

  sSavedGCCount = JS_GetGCParameter(nsJSRuntime::sRuntime, JSGC_NUMBER);

  // Save the old GC callback to chain to it, for GC-observing generality.
  gOldJSGCCallback = ::JS_SetGCCallbackRT(sRuntime, DOMGCCallback);

  JSSecurityCallbacks *callbacks = JS_GetRuntimeSecurityCallbacks(sRuntime);
  NS_ASSERTION(callbacks, "SecMan should have set security callbacks!");

  callbacks->findObjectPrincipals = ObjectPrincipalFinder;

  // Set these global xpconnect options...
  nsContentUtils::RegisterPrefCallback("dom.max_script_run_time",
                                       MaxScriptRunTimePrefChangedCallback,
                                       nsnull);
  MaxScriptRunTimePrefChangedCallback("dom.max_script_run_time", nsnull);

  nsContentUtils::RegisterPrefCallback("dom.max_chrome_script_run_time",
                                       MaxScriptRunTimePrefChangedCallback,
                                       nsnull);
  MaxScriptRunTimePrefChangedCallback("dom.max_chrome_script_run_time",
                                      nsnull);

  nsContentUtils::RegisterPrefCallback("dom.report_all_js_exceptions",
                                       ReportAllJSExceptionsPrefChangedCallback,
                                       nsnull);
  ReportAllJSExceptionsPrefChangedCallback("dom.report_all_js_exceptions",
                                           nsnull);

  nsContentUtils::RegisterPrefCallback("javascript.options.mem.high_water_mark",
                                       SetMemoryHighWaterMarkPrefChangedCallback,
                                       nsnull);
  SetMemoryHighWaterMarkPrefChangedCallback("javascript.options.mem.high_water_mark",
                                            nsnull);

  nsContentUtils::RegisterPrefCallback("javascript.options.mem.gc_frequency",
                                       SetMemoryGCFrequencyPrefChangedCallback,
                                       nsnull);
  SetMemoryGCFrequencyPrefChangedCallback("javascript.options.mem.gc_frequency",
                                          nsnull);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs)
    return NS_ERROR_FAILURE;
  nsIObserver* activityObserver = new nsUserActivityObserver();
  NS_ENSURE_TRUE(activityObserver, NS_ERROR_OUT_OF_MEMORY);
  obs->AddObserver(activityObserver, "user-interaction-inactive", PR_FALSE);
  obs->AddObserver(activityObserver, "user-interaction-active", PR_FALSE);
  obs->AddObserver(activityObserver, "xpcom-shutdown", PR_FALSE);

  nsIObserver* ccMemPressureObserver = new nsCCMemoryPressureObserver();
  NS_ENSURE_TRUE(ccMemPressureObserver, NS_ERROR_OUT_OF_MEMORY);
  obs->AddObserver(ccMemPressureObserver, "memory-pressure", PR_FALSE);

  sIsInitialized = PR_TRUE;

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
    NS_ENSURE_TRUE(gNameSpaceManager, nsnull);

    nsresult rv = gNameSpaceManager->Init();
    NS_ENSURE_SUCCESS(rv, nsnull);
  }

  return gNameSpaceManager;
}

/* static */
void
nsJSRuntime::Shutdown()
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
      JSSecurityCallbacks *callbacks = JS_GetRuntimeSecurityCallbacks(sRuntime);
      if (callbacks) {
        NS_ASSERTION(callbacks->findObjectPrincipals == ObjectPrincipalFinder,
                     "Fighting over the findObjectPrincipals callback!");
        callbacks->findObjectPrincipals = NULL;
      }
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
NS_IMPL_CYCLE_COLLECTION_ROOT_BEGIN(nsJSArgArray)
  tmp->ReleaseJSObjects();
NS_IMPL_CYCLE_COLLECTION_ROOT_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsJSArgArray)
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
                                                JSVAL_TO_GCTHING(*argv))
    }
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsJSArgArray)
  NS_INTERFACE_MAP_ENTRY(nsIArray)
  NS_INTERFACE_MAP_ENTRY(nsIJSArgArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIJSArgArray)
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
                                       static_cast<jsval *>(argv), &rv);
  if (ret == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;
  if (NS_FAILED(rv)) {
    delete ret;
    return rv;
  }
  return ret->QueryInterface(NS_GET_IID(nsIArray), (void **)aArray);
}
