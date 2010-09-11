/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 * The Original Code is worker threads.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (Original Author)
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsDOMThreadService.h"

// Interfaces
#include "nsIComponentManager.h"
#include "nsIConsoleService.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMWindowInternal.h"
#include "nsIEventTarget.h"
#include "nsIJSContextStack.h"
#include "nsIJSRuntimeService.h"
#include "nsIObserverService.h"
#include "nsIScriptError.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsISupportsPriority.h"
#include "nsIThreadPool.h"
#include "nsIXPConnect.h"
#include "nsPIDOMWindow.h"

// Other includes
#include "nsAutoLock.h"
#include "nsAutoPtr.h"
#include "nsContentUtils.h"
#include "nsDeque.h"
#include "nsIClassInfoImpl.h"
#include "nsStringBuffer.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCID.h"
#include "nsXPCOMCIDInternal.h"
#include "pratom.h"
#include "prthread.h"

// DOMWorker includes
#include "nsDOMWorker.h"
#include "nsDOMWorkerEvents.h"
#include "nsDOMWorkerMacros.h"
#include "nsDOMWorkerMessageHandler.h"
#include "nsDOMWorkerPool.h"
#include "nsDOMWorkerSecurityManager.h"
#include "nsDOMWorkerTimeout.h"

#ifdef PR_LOGGING
PRLogModuleInfo *gDOMThreadsLog = nsnull;
#endif
#define LOG(_args) PR_LOG(gDOMThreadsLog, PR_LOG_DEBUG, _args)

// The maximum number of threads in the internal thread pool
#define THREADPOOL_MAX_THREADS 3

PR_STATIC_ASSERT(THREADPOOL_MAX_THREADS >= 1);

// The maximum number of idle threads in the internal thread pool
#define THREADPOOL_IDLE_THREADS 3

PR_STATIC_ASSERT(THREADPOOL_MAX_THREADS >= THREADPOOL_IDLE_THREADS);

// As we suspend threads for various reasons (navigating away from the page,
// loading scripts, etc.) we open another slot in the thread pool for another
// worker to use. We can't do this forever so we set an absolute cap on the
// number of threads we'll allow to prevent DOS attacks.
#define THREADPOOL_THREAD_CAP 20

PR_STATIC_ASSERT(THREADPOOL_THREAD_CAP >= THREADPOOL_MAX_THREADS);

// A "bad" value for the NSPR TLS functions.
#define BAD_TLS_INDEX (PRUintn)-1

// Easy access for static functions. No reference here.
static nsDOMThreadService* gDOMThreadService = nsnull;

// These pointers actually carry references and must be released.
static nsIObserverService* gObserverService = nsnull;
static nsIJSRuntimeService* gJSRuntimeService = nsnull;
static nsIThreadJSContextStack* gThreadJSContextStack = nsnull;
static nsIXPCSecurityManager* gWorkerSecurityManager = nsnull;

PRUintn gJSContextIndex = BAD_TLS_INDEX;

static const char* sPrefsToWatch[] = {
  "dom.max_script_run_time"
};

// The length of time the close handler is allowed to run in milliseconds.
static PRUint32 gWorkerCloseHandlerTimeoutMS = 10000;

/**
 * Simple class to automatically destroy a JSContext to make error handling
 * easier.
 */
class JSAutoContextDestroyer
{
public:
  JSAutoContextDestroyer(JSContext* aCx)
  : mCx(aCx) { }

  ~JSAutoContextDestroyer() {
    if (mCx) {
      nsContentUtils::XPConnect()->ReleaseJSContext(mCx, PR_TRUE);
    }
  }

  operator JSContext*() {
    return mCx;
  }

  JSContext* forget() {
    JSContext* cx = mCx;
    mCx = nsnull;
    return cx;
  }

private:
  JSContext* mCx;
};

/**
 * This class is used as to post an error to the worker's outer handler.
 */
class nsReportErrorRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  nsReportErrorRunnable(nsDOMWorker* aWorker,
                        nsIScriptError* aScriptError)
  : mWorker(aWorker), mWorkerWN(aWorker->GetWrappedNative()),
    mScriptError(aScriptError) {
      NS_ASSERTION(aScriptError, "Null pointer!");
    }

  NS_IMETHOD Run() {
    if (mWorker->IsCanceled()) {
      return NS_OK;
    }

#ifdef DEBUG
    {
      nsRefPtr<nsDOMWorker> parent = mWorker->GetParent();
      if (NS_IsMainThread()) {
        NS_ASSERTION(!parent, "Shouldn't have a parent on the main thread!");
      }
      else {
        NS_ASSERTION(parent, "Should have a parent!");

        JSContext* cx = nsDOMThreadService::get()->GetCurrentContext();
        NS_ASSERTION(cx, "No context!");

        nsDOMWorker* currentWorker = (nsDOMWorker*)JS_GetContextPrivate(cx);
        NS_ASSERTION(currentWorker == parent, "Wrong worker!");
      }
    }
#endif

    NS_NAMED_LITERAL_STRING(errorStr, "error");

    nsresult rv;

    if (mWorker->HasListeners(errorStr)) {
      // Construct the error event.
      nsString message;
      rv = mScriptError->GetErrorMessage(message);
      NS_ENSURE_SUCCESS(rv, rv);

      nsString filename;
      rv = mScriptError->GetSourceName(filename);
      NS_ENSURE_SUCCESS(rv, rv);

      PRUint32 lineno;
      rv = mScriptError->GetLineNumber(&lineno);
      NS_ENSURE_SUCCESS(rv, rv);

      nsRefPtr<nsDOMWorkerErrorEvent> event(new nsDOMWorkerErrorEvent());
      NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

      rv = event->InitErrorEvent(errorStr, PR_FALSE, PR_TRUE, message,
                                 filename, lineno);
      NS_ENSURE_SUCCESS(rv, rv);

      event->SetTarget(static_cast<nsDOMWorkerMessageHandler*>(mWorker));

      PRBool stopPropagation = PR_FALSE;
      rv = mWorker->DispatchEvent(static_cast<nsDOMWorkerEvent*>(event),
                                  &stopPropagation);
      if (NS_SUCCEEDED(rv) && stopPropagation) {
        return NS_OK;
      }
    }

    nsRefPtr<nsDOMWorker> parent = mWorker->GetParent();
    if (!parent) {
      NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
      nsCOMPtr<nsIConsoleService> consoleService =
        do_GetService(NS_CONSOLESERVICE_CONTRACTID);
      if (consoleService) {
        rv = consoleService->LogMessage(mScriptError);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      return NS_OK;
    }

    nsRefPtr<nsReportErrorRunnable> runnable =
      new nsReportErrorRunnable(parent, mScriptError);
    if (runnable) {
      nsRefPtr<nsDOMWorker> grandparent = parent->GetParent();
      rv = grandparent ?
           nsDOMThreadService::get()->Dispatch(grandparent, runnable) :
           NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }

private:
  nsRefPtr<nsDOMWorker> mWorker;
  nsCOMPtr<nsIXPConnectWrappedNative> mWorkerWN;
  nsCOMPtr<nsIScriptError> mScriptError;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsReportErrorRunnable, nsIRunnable)

/**
 * Used to post an expired timeout to the correct worker.
 */
class nsDOMWorkerTimeoutRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  nsDOMWorkerTimeoutRunnable(nsDOMWorkerTimeout* aTimeout)
  : mTimeout(aTimeout) { }

  NS_IMETHOD Run() {
    return mTimeout->Run();
  }
protected:
  nsRefPtr<nsDOMWorkerTimeout> mTimeout;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDOMWorkerTimeoutRunnable, nsIRunnable)

class nsDOMWorkerKillRunnable : public nsIRunnable
{
public:
  NS_DECL_ISUPPORTS

  nsDOMWorkerKillRunnable(nsDOMWorker* aWorker)
  : mWorker(aWorker) { }

  NS_IMETHOD Run() {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    mWorker->Kill();
    return NS_OK;
  }

private:
  nsRefPtr<nsDOMWorker> mWorker;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDOMWorkerKillRunnable, nsIRunnable)

/**
 * This class exists to solve a particular problem: Calling Dispatch on a
 * thread pool will always create a new thread to service the runnable as long
 * as the thread limit has not been reached. Since our DOM workers can only be
 * accessed by one thread at a time we could end up spawning a new thread that
 * does nothing but wait initially. There is no way to control this behavior
 * currently so we cheat by using a runnable that emulates a thread. The
 * nsDOMThreadService's monitor protects the queue of events.
 */
class nsDOMWorkerRunnable : public nsIRunnable
{
  friend class nsDOMThreadService;

public:
  NS_DECL_ISUPPORTS

  nsDOMWorkerRunnable(nsDOMWorker* aWorker)
  : mWorker(aWorker), mCloseTimeoutInterval(0), mKillWorkerWhenDone(PR_FALSE) {
  }

  virtual ~nsDOMWorkerRunnable() {
    ClearQueue();
  }

  void PutRunnable(nsIRunnable* aRunnable,
                   PRIntervalTime aTimeoutInterval,
                   PRBool aClearQueue) {
    NS_ASSERTION(aRunnable, "Null pointer!");

    PR_ASSERT_CURRENT_THREAD_IN_MONITOR(gDOMThreadService->mMonitor);

    if (NS_LIKELY(!aTimeoutInterval)) {
      NS_ADDREF(aRunnable);
      mRunnables.Push(aRunnable);
    }
    else {
      NS_ASSERTION(!mCloseRunnable, "More than one close runnable?!");
      if (aClearQueue) {
        ClearQueue();
      }
      mCloseRunnable = aRunnable;
      mCloseTimeoutInterval = aTimeoutInterval;
      mKillWorkerWhenDone = PR_TRUE;
    }
  }

  void SetCloseRunnableTimeout(PRIntervalTime aTimeoutInterval) {
    NS_ASSERTION(aTimeoutInterval, "No timeout specified!");
    NS_ASSERTION(aTimeoutInterval!= PR_INTERVAL_NO_TIMEOUT, "Bad timeout!");

    // No need to enter the monitor because we should already be in it.

    NS_ASSERTION(mWorker->GetExpirationTime() == PR_INTERVAL_NO_TIMEOUT,
                 "Asked to set timeout on a runnable with no close handler!");

    // This may actually overflow but we don't care - the worst that could
    // happen is that the close handler could run for a slightly different
    // amount of time and the spec leaves the time up to us anyway.
    mWorker->SetExpirationTime(PR_IntervalNow() + aTimeoutInterval);
  }

  NS_IMETHOD Run() {
    NS_ASSERTION(!NS_IsMainThread(),
                 "This should *never* run on the main thread!");

    // This must have been set up by the thread service
    NS_ASSERTION(gJSContextIndex != BAD_TLS_INDEX, "No context index!");

    // Make sure we have a JSContext to run everything on.
    JSContext* cx = (JSContext*)PR_GetThreadPrivate(gJSContextIndex);
    if (!cx) {
        NS_ERROR("nsDOMThreadService didn't give us a context! Are we out of memory?");
        return NS_ERROR_FAILURE;
    }

    NS_ASSERTION(!JS_GetGlobalObject(cx), "Shouldn't have a global!");

    if (mWorker->IsPrivileged()) {
      JS_SetVersion(cx, JSVERSION_LATEST);
    }
    else {
      JS_SetVersion(cx, JSVERSION_DEFAULT);
    }

    JS_SetContextPrivate(cx, mWorker);

    // Go ahead and trigger the operation callback for this context before we
    // try to run any JS. That way we'll be sure to cancel or suspend as soon as
    // possible if the compilation takes too long.
    JS_TriggerOperationCallback(cx);

    PRBool killWorkerWhenDone;
    {
      nsLazyAutoRequest ar;
      JSAutoCrossCompartmentCall axcc;

      // Tell the worker which context it will be using
      if (mWorker->SetGlobalForContext(cx, &ar, &axcc)) {
        NS_ASSERTION(ar.entered(), "SetGlobalForContext must enter request on success");
        NS_ASSERTION(axcc.entered(), "SetGlobalForContext must enter xcc on success");

        RunQueue(cx, &killWorkerWhenDone);

        // Remove the global object from the context so that it might be garbage
        // collected.
        JS_SetGlobalObject(cx, NULL);
        JS_SetContextPrivate(cx, NULL);
      }
      else {
        NS_ASSERTION(!ar.entered(), "SetGlobalForContext must not enter request on failure");
        NS_ASSERTION(!axcc.entered(), "SetGlobalForContext must not enter xcc on failure");

        {
          // Code in XPConnect assumes that the context's global object won't be
          // replaced outside of a request.
          JSAutoRequest ar2(cx);

          // This is usually due to a parse error in the worker script...
          JS_SetGlobalObject(cx, NULL);
          JS_SetContextPrivate(cx, NULL);
        }

        nsAutoMonitor mon(gDOMThreadService->mMonitor);
        killWorkerWhenDone = mKillWorkerWhenDone;
        gDOMThreadService->WorkerComplete(this);
        mon.NotifyAll();
      }
    }

    if (killWorkerWhenDone) {
      nsCOMPtr<nsIRunnable> runnable = new nsDOMWorkerKillRunnable(mWorker);
      NS_ENSURE_TRUE(runnable, NS_ERROR_OUT_OF_MEMORY);

      nsresult rv = NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
  }

protected:
  void ClearQueue() {
    nsCOMPtr<nsIRunnable> runnable;
    while ((runnable = dont_AddRef((nsIRunnable*)mRunnables.PopFront()))) {
      // Loop until all the runnables are dead.
    }
  }

  void RunQueue(JSContext* aCx, PRBool* aCloseRunnableSet) {
    while (1) {
      nsCOMPtr<nsIRunnable> runnable;
      {
        nsAutoMonitor mon(gDOMThreadService->mMonitor);

        runnable = dont_AddRef((nsIRunnable*)mRunnables.PopFront());

        if (!runnable && mCloseRunnable) {
          PRIntervalTime expirationTime;
          if (mCloseTimeoutInterval == PR_INTERVAL_NO_TIMEOUT) {
            expirationTime = mCloseTimeoutInterval;
          }
          else {
            expirationTime = PR_IntervalNow() + mCloseTimeoutInterval;
          }
          mWorker->SetExpirationTime(expirationTime);

          runnable.swap(mCloseRunnable);
        }

        if (!runnable || mWorker->IsCanceled()) {
#ifdef PR_LOGGING
          if (mWorker->IsCanceled()) {
            LOG(("Bailing out of run loop for canceled worker[0x%p]",
                 static_cast<void*>(mWorker.get())));
          }
#endif
          *aCloseRunnableSet = mKillWorkerWhenDone;
          gDOMThreadService->WorkerComplete(this);
          mon.NotifyAll();
          return;
        }
      }

      // Clear out any old cruft hanging around in the regexp statics.
      JS_ClearRegExpStatics(aCx);

      runnable->Run();
    }
    NS_NOTREACHED("Shouldn't ever get here!");
  }

  // Set at construction
  nsRefPtr<nsDOMWorker> mWorker;

  // Protected by mMonitor
  nsDeque mRunnables;
  nsCOMPtr<nsIRunnable> mCloseRunnable;
  PRIntervalTime mCloseTimeoutInterval;
  PRBool mKillWorkerWhenDone;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDOMWorkerRunnable, nsIRunnable)

/*******************************************************************************
 * JS environment function and callbacks
 */

JSBool
DOMWorkerOperationCallback(JSContext* aCx)
{
  nsDOMWorker* worker = (nsDOMWorker*)JS_GetContextPrivate(aCx);
  NS_ASSERTION(worker, "This must never be null!");

  PRBool canceled = worker->IsCanceled();
  if (!canceled && worker->IsSuspended()) {
    JSAutoSuspendRequest suspended(aCx);

    // Since we're going to block this thread we should open up a new thread
    // in the thread pool for other workers. Must check the return value to
    // make sure we don't decrement when we failed.
    PRBool extraThreadAllowed =
      NS_SUCCEEDED(gDOMThreadService->ChangeThreadPoolMaxThreads(1));

    // Flush JIT caches now before suspending to avoid holding memory that we
    // are not going to use.
    JS_FlushCaches(aCx);

    for (;;) {
      nsAutoMonitor mon(worker->Pool()->Monitor());

      // There's a small chance that the worker was canceled after our check
      // above in which case we shouldn't wait here. We're guaranteed not to
      // race here because the pool reenters its monitor after canceling each
      // worker in order to notify its condition variable.
      canceled = worker->IsCanceled();
      if (!canceled && worker->IsSuspended()) {
        mon.Wait();
      }
      else {
        break;
      }
    }

    if (extraThreadAllowed) {
      gDOMThreadService->ChangeThreadPoolMaxThreads(-1);
    }
  }

  if (canceled) {
    LOG(("Forcefully killing JS for worker [0x%p]",
         static_cast<void*>(worker)));
    // Kill execution of the currently running JS.
    JS_ClearPendingException(aCx);
    return JS_FALSE;
  }
  return JS_TRUE;
}

void
DOMWorkerErrorReporter(JSContext* aCx,
                       const char* aMessage,
                       JSErrorReport* aReport)
{
  NS_ASSERTION(!NS_IsMainThread(), "Huh?!");

  nsDOMWorker* worker = (nsDOMWorker*)JS_GetContextPrivate(aCx);

  if (worker->IsCanceled()) {
    // We don't want to report errors from canceled workers. It's very likely
    // that we only returned an error in the first place because the worker was
    // already canceled.
    return;
  }

  if (worker->mErrorHandlerRecursionCount == 2) {
    // We've somehow ended up in a recursive onerror loop. Bail out.
    return;
  }

  nsresult rv;
  nsCOMPtr<nsIScriptError> scriptError;

  {
    // CreateInstance will lock, make sure we suspend our request!
    JSAutoSuspendRequest ar(aCx);

    scriptError = do_CreateInstance(NS_SCRIPTERROR_CONTRACTID, &rv);
  }

  if (NS_FAILED(rv)) {
    return;
  }

  nsAutoString message, filename, line;
  PRUint32 lineNumber, columnNumber, flags, errorNumber;

  if (aReport) {
    if (aReport->ucmessage) {
      message.Assign(reinterpret_cast<const PRUnichar*>(aReport->ucmessage));
    }
    filename.AssignWithConversion(aReport->filename);
    line.Assign(reinterpret_cast<const PRUnichar*>(aReport->uclinebuf));
    lineNumber = aReport->lineno;
    columnNumber = aReport->uctokenptr - aReport->uclinebuf;
    flags = aReport->flags;
    errorNumber = aReport->errorNumber;
  }
  else {
    lineNumber = columnNumber = errorNumber = 0;
    flags = nsIScriptError::errorFlag | nsIScriptError::exceptionFlag;
  }

  if (message.IsEmpty()) {
    message.AssignWithConversion(aMessage);
  }

  rv = scriptError->Init(message.get(), filename.get(), line.get(), lineNumber,
                         columnNumber, flags, "DOM Worker javascript");
  if (NS_FAILED(rv)) {
    return;
  }

  // Don't call the error handler if we're out of stack space.
  if (errorNumber != JSMSG_SCRIPT_STACK_QUOTA &&
      errorNumber != JSMSG_OVER_RECURSED) {
    // Try the onerror handler for the worker's scope.
    nsRefPtr<nsDOMWorkerScope> scope = worker->GetInnerScope();
    NS_ASSERTION(scope, "Null scope!");

    PRBool hasListeners = scope->HasListeners(NS_LITERAL_STRING("error"));
    if (hasListeners) {
      nsRefPtr<nsDOMWorkerErrorEvent> event(new nsDOMWorkerErrorEvent());
      if (event) {
        rv = event->InitErrorEvent(NS_LITERAL_STRING("error"), PR_FALSE,
                                   PR_TRUE, nsDependentString(message),
                                   filename, lineNumber);
        if (NS_SUCCEEDED(rv)) {
          event->SetTarget(scope);

          NS_ASSERTION(worker->mErrorHandlerRecursionCount >= 0,
                       "Bad recursion count logic!");
          worker->mErrorHandlerRecursionCount++;

          PRBool preventDefaultCalled = PR_FALSE;
          scope->DispatchEvent(static_cast<nsDOMWorkerEvent*>(event),
                               &preventDefaultCalled);

          worker->mErrorHandlerRecursionCount--;

          if (preventDefaultCalled) {
            return;
          }
        }
      }
    }
  }

  // Still unhandled, fire at the onerror handler on the worker.
  nsCOMPtr<nsIRunnable> runnable =
    new nsReportErrorRunnable(worker, scriptError);
  NS_ENSURE_TRUE(runnable,);

  nsRefPtr<nsDOMWorker> parent = worker->GetParent();

  // If this worker has a parent then we need to send the message through the
  // thread service to be run on the parent's thread. Otherwise it is a
  // top-level worker and we send the message to the main thread.
  rv = parent ? nsDOMThreadService::get()->Dispatch(parent, runnable)
              : NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL);
  if (NS_FAILED(rv)) {
    return;
  }
}

/*******************************************************************************
 * nsDOMThreadService
 */

nsDOMThreadService::nsDOMThreadService()
: mMonitor(nsnull),
  mNavigatorStringsLoaded(PR_FALSE)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
#ifdef PR_LOGGING
  if (!gDOMThreadsLog) {
    gDOMThreadsLog = PR_NewLogModule("nsDOMThreads");
  }
#endif
  LOG(("Initializing DOM Thread service"));
}

nsDOMThreadService::~nsDOMThreadService()
{
  LOG(("DOM Thread service destroyed"));

  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  Cleanup();

  if (mMonitor) {
    nsAutoMonitor::DestroyMonitor(mMonitor);
  }
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsDOMThreadService, nsIEventTarget,
                                                  nsIObserver,
                                                  nsIThreadPoolListener)

nsresult
nsDOMThreadService::Init()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gDOMThreadService, "Only one instance should ever be created!");

  nsresult rv;
  nsCOMPtr<nsIObserverService> obs =
    do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  obs.forget(&gObserverService);

  RegisterPrefCallbacks();

  mThreadPool = do_CreateInstance(NS_THREADPOOL_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetListener(this);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetThreadLimit(THREADPOOL_MAX_THREADS);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mThreadPool->SetIdleThreadLimit(THREADPOOL_IDLE_THREADS);
  NS_ENSURE_SUCCESS(rv, rv);

  mMonitor = nsAutoMonitor::NewMonitor("nsDOMThreadService::mMonitor");
  NS_ENSURE_TRUE(mMonitor, NS_ERROR_OUT_OF_MEMORY);

  PRBool success = mWorkersInProgress.Init();
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  success = mPools.Init();
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  success = mJSContexts.SetCapacity(THREADPOOL_THREAD_CAP);
  NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

  nsCOMPtr<nsIJSRuntimeService>
    runtimeSvc(do_GetService("@mozilla.org/js/xpc/RuntimeService;1"));
  NS_ENSURE_TRUE(runtimeSvc, NS_ERROR_FAILURE);
  runtimeSvc.forget(&gJSRuntimeService);

  nsCOMPtr<nsIThreadJSContextStack>
    contextStack(do_GetService("@mozilla.org/js/xpc/ContextStack;1"));
  NS_ENSURE_TRUE(contextStack, NS_ERROR_FAILURE);
  contextStack.forget(&gThreadJSContextStack);

  nsCOMPtr<nsIXPCSecurityManager> secMan(new nsDOMWorkerSecurityManager());
  NS_ENSURE_TRUE(secMan, NS_ERROR_OUT_OF_MEMORY);
  secMan.forget(&gWorkerSecurityManager);

  if (gJSContextIndex == BAD_TLS_INDEX &&
      PR_NewThreadPrivateIndex(&gJSContextIndex, NULL) != PR_SUCCESS) {
    NS_ERROR("PR_NewThreadPrivateIndex failed!");
    gJSContextIndex = BAD_TLS_INDEX;
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

/* static */
already_AddRefed<nsDOMThreadService>
nsDOMThreadService::GetOrInitService()
{
  if (!gDOMThreadService) {
    nsRefPtr<nsDOMThreadService> service = new nsDOMThreadService();
    NS_ENSURE_TRUE(service, nsnull);

    nsresult rv = service->Init();
    NS_ENSURE_SUCCESS(rv, nsnull);

    service.swap(gDOMThreadService);
  }

  nsRefPtr<nsDOMThreadService> service(gDOMThreadService);
  return service.forget();
}

/* static */
nsDOMThreadService*
nsDOMThreadService::get()
{
  return gDOMThreadService;
}

/* static */
JSContext*
nsDOMThreadService::GetCurrentContext()
{
  JSContext* cx;

  if (NS_IsMainThread()) {
    nsresult rv = ThreadJSContextStack()->GetSafeJSContext(&cx);
    NS_ENSURE_SUCCESS(rv, nsnull);
  }
  else {
    NS_ENSURE_TRUE(gJSContextIndex, nsnull);
    cx = static_cast<JSContext*>(PR_GetThreadPrivate(gJSContextIndex));
  }

  return cx;
}

/* static */
void
nsDOMThreadService::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_IF_RELEASE(gDOMThreadService);
}

void
nsDOMThreadService::Cleanup()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // This will either be called at 'xpcom-shutdown' or earlier if the call to
  // Init fails somehow. We can therefore assume that all services will still
  // be available here.

  {
    nsAutoMonitor mon(mMonitor);

    NS_ASSERTION(!mPools.Count(), "Live workers left!");
    mPools.Clear();

    NS_ASSERTION(!mSuspendedWorkers.Length(), "Suspended workers left!");
    mSuspendedWorkers.Clear();
  }

  if (gObserverService) {
    gObserverService->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    NS_RELEASE(gObserverService);

    UnregisterPrefCallbacks();
  }

  // The thread pool holds a circular reference to this service through its
  // listener. We must shut down the thread pool manually to break this cycle.
  if (mThreadPool) {
    mThreadPool->Shutdown();
    mThreadPool = nsnull;
  }

  // Need to force a GC so that all of our workers get cleaned up.
  if (gThreadJSContextStack) {
    JSContext* safeContext;
    if (NS_SUCCEEDED(gThreadJSContextStack->GetSafeJSContext(&safeContext))) {
      JS_GC(safeContext);
    }
    NS_RELEASE(gThreadJSContextStack);
  }

  // These must be released after the thread pool is shut down.
  NS_IF_RELEASE(gJSRuntimeService);
  NS_IF_RELEASE(gWorkerSecurityManager);
}

nsresult
nsDOMThreadService::Dispatch(nsDOMWorker* aWorker,
                             nsIRunnable* aRunnable,
                             PRIntervalTime aTimeoutInterval,
                             PRBool aClearQueue)
{
  NS_ASSERTION(aWorker, "Null pointer!");
  NS_ASSERTION(aRunnable, "Null pointer!");

  if (!mThreadPool) {
    // This can happen from a nsDOMWorker::Finalize call after the thread pool
    // has been shutdown. It should never be possible off the main thread.
    NS_ASSERTION(NS_IsMainThread(),
                 "This should be impossible on a non-main thread!");
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  // Don't accept the runnable if the worker's close handler has been triggered
  // (unless, of course, this is the close runnable as indicated by the non-0
  // timeout value).
  if (aWorker->IsClosing() && !aTimeoutInterval) {
    LOG(("Will not dispatch runnable [0x%p] for closing worker [0x%p]",
         static_cast<void*>(aRunnable), static_cast<void*>(aWorker)));
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<nsDOMWorkerRunnable> workerRunnable;
  {
    nsAutoMonitor mon(mMonitor);

    if (mWorkersInProgress.Get(aWorker, getter_AddRefs(workerRunnable))) {
      workerRunnable->PutRunnable(aRunnable, aTimeoutInterval, aClearQueue);
      return NS_OK;
    }

    workerRunnable = new nsDOMWorkerRunnable(aWorker);
    NS_ENSURE_TRUE(workerRunnable, NS_ERROR_OUT_OF_MEMORY);

    workerRunnable->PutRunnable(aRunnable, aTimeoutInterval, PR_FALSE);

    PRBool success = mWorkersInProgress.Put(aWorker, workerRunnable);
    NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);
  }

  nsresult rv = mThreadPool->Dispatch(workerRunnable, NS_DISPATCH_NORMAL);

  // XXX This is a mess and it could probably be removed once we have an
  // infallible malloc implementation.
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch runnable to thread pool!");

    nsAutoMonitor mon(mMonitor);

    // We exited the monitor after inserting the runnable into the table so make
    // sure we're removing the right one!
    nsRefPtr<nsDOMWorkerRunnable> tableRunnable;
    if (mWorkersInProgress.Get(aWorker, getter_AddRefs(tableRunnable)) &&
        workerRunnable == tableRunnable) {
      mWorkersInProgress.Remove(aWorker);

      // And don't forget to tell anyone who's waiting.
      mon.NotifyAll();
    }

    return rv;
  }

  return NS_OK;
}

void
nsDOMThreadService::SetWorkerTimeout(nsDOMWorker* aWorker,
                                     PRIntervalTime aTimeoutInterval)
{
  NS_ASSERTION(aWorker, "Null pointer!");
  NS_ASSERTION(aTimeoutInterval, "No timeout specified!");

  NS_ASSERTION(mThreadPool, "Dispatch called after 'xpcom-shutdown'!");

  nsAutoMonitor mon(mMonitor);

  nsRefPtr<nsDOMWorkerRunnable> workerRunnable;
  if (mWorkersInProgress.Get(aWorker, getter_AddRefs(workerRunnable))) {
    workerRunnable->SetCloseRunnableTimeout(aTimeoutInterval);
  }
}

void
nsDOMThreadService::WorkerComplete(nsDOMWorkerRunnable* aRunnable)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

#ifdef DEBUG
  nsRefPtr<nsDOMWorker>& debugWorker = aRunnable->mWorker;

  nsRefPtr<nsDOMWorkerRunnable> runnable;
  NS_ASSERTION(mWorkersInProgress.Get(debugWorker, getter_AddRefs(runnable)) &&
               runnable == aRunnable,
               "Removing a worker that isn't in our hashtable?!");
#endif

  mWorkersInProgress.Remove(aRunnable->mWorker);
}

PRBool
nsDOMThreadService::QueueSuspendedWorker(nsDOMWorkerRunnable* aRunnable)
{
  nsAutoMonitor mon(mMonitor);

#ifdef DEBUG
    {
      // Make sure that the runnable is in mWorkersInProgress.
      nsRefPtr<nsDOMWorkerRunnable> current;
      mWorkersInProgress.Get(aRunnable->mWorker, getter_AddRefs(current));
      NS_ASSERTION(current == aRunnable, "Something crazy wrong here!");
    }
#endif

  return mSuspendedWorkers.AppendElement(aRunnable) ? PR_TRUE : PR_FALSE;
}

/* static */
JSContext*
nsDOMThreadService::CreateJSContext()
{
  JSRuntime* rt;
  gJSRuntimeService->GetRuntime(&rt);
  NS_ENSURE_TRUE(rt, nsnull);

  JSAutoContextDestroyer cx(JS_NewContext(rt, 8192));
  NS_ENSURE_TRUE(cx, nsnull);

  JS_SetErrorReporter(cx, DOMWorkerErrorReporter);

  JS_SetOperationCallback(cx, DOMWorkerOperationCallback);

  static JSSecurityCallbacks securityCallbacks = {
    nsDOMWorkerSecurityManager::JSCheckAccess,
    nsDOMWorkerSecurityManager::JSTranscodePrincipals,
    nsDOMWorkerSecurityManager::JSFindPrincipal
  };
  JS_SetContextSecurityCallbacks(cx, &securityCallbacks);

  JS_ClearContextDebugHooks(cx);

  nsresult rv = nsContentUtils::XPConnect()->
    SetSecurityManagerForJSContext(cx, gWorkerSecurityManager, 0);
  NS_ENSURE_SUCCESS(rv, nsnull);

  JS_SetNativeStackQuota(cx, 256*1024);
  JS_SetScriptStackQuota(cx, 100*1024*1024);

  JS_SetOptions(cx, JS_GetOptions(cx) | JSOPTION_JIT | JSOPTION_ANONFUNFIX);
  JS_SetGCParameterForThread(cx, JSGC_MAX_CODE_CACHE_BYTES, 1 * 1024 * 1024);

  return cx.forget();
}

already_AddRefed<nsDOMWorkerPool>
nsDOMThreadService::GetPoolForGlobal(nsIScriptGlobalObject* aGlobalObject,
                                     PRBool aRemove)
{
  NS_ASSERTION(aGlobalObject, "Null pointer!");

  nsAutoMonitor mon(mMonitor);

  nsRefPtr<nsDOMWorkerPool> pool;
  mPools.Get(aGlobalObject, getter_AddRefs(pool));

  if (aRemove) {
    mPools.Remove(aGlobalObject);
  }

  return pool.forget();
}

void
nsDOMThreadService::TriggerOperationCallbackForPool(nsDOMWorkerPool* aPool)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  // See if we need to trigger the operation callback on any currently running
  // contexts.
  PRUint32 contextCount = mJSContexts.Length();
  for (PRUint32 index = 0; index < contextCount; index++) {
    JSContext*& cx = mJSContexts[index];
    nsDOMWorker* worker = (nsDOMWorker*)JS_GetContextPrivate(cx);
    if (worker && worker->Pool() == aPool) {
      JS_TriggerOperationCallback(cx);
    }
  }
}

void
nsDOMThreadService::RescheduleSuspendedWorkerForPool(nsDOMWorkerPool* aPool)
{
  PR_ASSERT_CURRENT_THREAD_IN_MONITOR(mMonitor);

  PRUint32 count = mSuspendedWorkers.Length();
  if (!count) {
    // Nothing to do here.
    return;
  }

  nsTArray<nsDOMWorkerRunnable*> others(count);

  for (PRUint32 index = 0; index < count; index++) {
    nsDOMWorkerRunnable* runnable = mSuspendedWorkers[index];

#ifdef DEBUG
    {
      // Make sure that the runnable never left mWorkersInProgress.
      nsRefPtr<nsDOMWorkerRunnable> current;
      mWorkersInProgress.Get(runnable->mWorker, getter_AddRefs(current));
      NS_ASSERTION(current == runnable, "Something crazy wrong here!");
    }
#endif

    if (runnable->mWorker->Pool() == aPool) {
#ifdef DEBUG
      nsresult rv =
#endif
      mThreadPool->Dispatch(runnable, NS_DISPATCH_NORMAL);
      NS_ASSERTION(NS_SUCCEEDED(rv), "This shouldn't ever fail!");
    }
    else {
      others.AppendElement(runnable);
    }
  }

  mSuspendedWorkers.SwapElements(others);
}

void
nsDOMThreadService::CancelWorkersForGlobal(nsIScriptGlobalObject* aGlobalObject)
{
  NS_ASSERTION(aGlobalObject, "Null pointer!");

  nsRefPtr<nsDOMWorkerPool> pool = GetPoolForGlobal(aGlobalObject, PR_TRUE);
  if (pool) {
    pool->Cancel();

    nsAutoMonitor mon(mMonitor);

    TriggerOperationCallbackForPool(pool);
    RescheduleSuspendedWorkerForPool(pool);
  }
}

void
nsDOMThreadService::SuspendWorkersForGlobal(nsIScriptGlobalObject* aGlobalObject)
{
  NS_ASSERTION(aGlobalObject, "Null pointer!");

  nsRefPtr<nsDOMWorkerPool> pool = GetPoolForGlobal(aGlobalObject, PR_FALSE);
  if (pool) {
    pool->Suspend();

    nsAutoMonitor mon(mMonitor);
    TriggerOperationCallbackForPool(pool);
  }
}

void
nsDOMThreadService::ResumeWorkersForGlobal(nsIScriptGlobalObject* aGlobalObject)
{
  NS_ASSERTION(aGlobalObject, "Null pointer!");

  nsRefPtr<nsDOMWorkerPool> pool = GetPoolForGlobal(aGlobalObject, PR_FALSE);
  if (pool) {
    pool->Resume();

    nsAutoMonitor mon(mMonitor);

    TriggerOperationCallbackForPool(pool);
    RescheduleSuspendedWorkerForPool(pool);
  }
}

void
nsDOMThreadService::NoteEmptyPool(nsDOMWorkerPool* aPool)
{
  NS_ASSERTION(aPool, "Null pointer!");

  nsAutoMonitor mon(mMonitor);
  mPools.Remove(aPool->ScriptGlobalObject());
}

void
nsDOMThreadService::TimeoutReady(nsDOMWorkerTimeout* aTimeout)
{
  nsRefPtr<nsDOMWorkerTimeoutRunnable> runnable =
    new nsDOMWorkerTimeoutRunnable(aTimeout);
  NS_ENSURE_TRUE(runnable,);

  Dispatch(aTimeout->GetWorker(), runnable);
}

nsresult
nsDOMThreadService::ChangeThreadPoolMaxThreads(PRInt16 aDelta)
{
  NS_ENSURE_ARG(aDelta == 1 || aDelta == -1);

  nsAutoMonitor mon(mMonitor);

  PRUint32 currentThreadCount;
  nsresult rv = mThreadPool->GetThreadLimit(&currentThreadCount);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 newThreadCount = (PRInt32)currentThreadCount + (PRInt32)aDelta;
  NS_ASSERTION(newThreadCount >= THREADPOOL_MAX_THREADS,
               "Can't go below initial thread count!");

  if (newThreadCount > THREADPOOL_THREAD_CAP) {
    NS_WARNING("Thread pool cap reached!");
    return NS_ERROR_FAILURE;
  }

  rv = mThreadPool->SetThreadLimit((PRUint32)newThreadCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we're allowing an extra thread then post a dummy event to the thread
  // pool so that any pending workers can get started. The thread pool doesn't
  // do this on its own like it probably should...
  if (aDelta == 1) {
    nsCOMPtr<nsIRunnable> dummy(new nsRunnable());
    if (dummy) {
      rv = mThreadPool->Dispatch(dummy, NS_DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

// static
nsIJSRuntimeService*
nsDOMThreadService::JSRuntimeService()
{
  return gJSRuntimeService;
}

// static
nsIThreadJSContextStack*
nsDOMThreadService::ThreadJSContextStack()
{
  return gThreadJSContextStack;
}

// static
nsIXPCSecurityManager*
nsDOMThreadService::WorkerSecurityManager()
{
  return gWorkerSecurityManager;
}

/**
 * See nsIEventTarget
 */
NS_IMETHODIMP
nsDOMThreadService::Dispatch(nsIRunnable* aEvent,
                             PRUint32 aFlags)
{
  NS_ENSURE_ARG_POINTER(aEvent);
  NS_ENSURE_FALSE(aFlags & NS_DISPATCH_SYNC, NS_ERROR_NOT_IMPLEMENTED);

  // This should only ever be called by the timer code! We run the event right
  // now, but all that does is queue the real event for the proper worker.
  aEvent->Run();

  return NS_OK;
}

/**
 * See nsIEventTarget
 */
NS_IMETHODIMP
nsDOMThreadService::IsOnCurrentThread(PRBool* _retval)
{
  NS_NOTREACHED("No one should call this!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

/**
 * See nsIObserver
 */
NS_IMETHODIMP
nsDOMThreadService::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const PRUnichar* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Cleanup();
    return NS_OK;
  }

  NS_NOTREACHED("Unknown observer topic!");
  return NS_OK;
}

/**
 * See nsIThreadPoolListener
 */
NS_IMETHODIMP
nsDOMThreadService::OnThreadCreated()
{
  LOG(("Thread created"));

  nsIThread* current = NS_GetCurrentThread();

  // We want our worker threads to always have a lower priority than the main
  // thread. NSPR docs say that this isn't incredibly reliable across all
  // platforms but we hope for the best.
  nsCOMPtr<nsISupportsPriority> priority(do_QueryInterface(current));
  NS_ENSURE_TRUE(priority, NS_ERROR_FAILURE);

  nsresult rv = priority->SetPriority(nsISupportsPriority::PRIORITY_LOWEST);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ASSERTION(gJSContextIndex != BAD_TLS_INDEX, "No context index!");

  // Set the context up for the worker.
  JSContext* cx = (JSContext*)PR_GetThreadPrivate(gJSContextIndex);
  if (!cx) {
    cx = nsDOMThreadService::CreateJSContext();
    NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);

    PRStatus status = PR_SetThreadPrivate(gJSContextIndex, cx);
    if (status != PR_SUCCESS) {
      NS_WARNING("Failed to set context on thread!");
      nsContentUtils::XPConnect()->ReleaseJSContext(cx, PR_TRUE);
      return NS_ERROR_FAILURE;
    }

    nsAutoMonitor mon(mMonitor);

#ifdef DEBUG
    JSContext** newContext =
#endif
    mJSContexts.AppendElement(cx);

    // We ensure the capacity of this array in Init.
    NS_ASSERTION(newContext, "Should never fail!");
  }

  // Make sure that XPConnect knows about this context.
  gThreadJSContextStack->Push(cx);
  gThreadJSContextStack->SetSafeJSContext(cx);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMThreadService::OnThreadShuttingDown()
{
  LOG(("Thread shutting down"));

  NS_ASSERTION(gJSContextIndex != BAD_TLS_INDEX, "No context index!");

  JSContext* cx = (JSContext*)PR_GetThreadPrivate(gJSContextIndex);
  NS_WARN_IF_FALSE(cx, "Thread died with no context?");
  if (cx) {
    {
      nsAutoMonitor mon(mMonitor);
      mJSContexts.RemoveElement(cx);
    }

    JSContext* pushedCx;
    gThreadJSContextStack->Pop(&pushedCx);
    NS_ASSERTION(pushedCx == cx, "Popped the wrong context!");

    gThreadJSContextStack->SetSafeJSContext(nsnull);

    nsContentUtils::XPConnect()->ReleaseJSContext(cx, PR_TRUE);
  }

  return NS_OK;
}

nsresult
nsDOMThreadService::RegisterWorker(nsDOMWorker* aWorker,
                                   nsIScriptGlobalObject* aGlobalObject)
{
  NS_ASSERTION(aWorker, "Null pointer!");
  NS_ASSERTION(aGlobalObject, "Null pointer!");

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindow> domWindow(do_QueryInterface(aGlobalObject));
    NS_ENSURE_TRUE(domWindow, NS_ERROR_NO_INTERFACE);

    nsPIDOMWindow* innerWindow = domWindow->IsOuterWindow() ?
                                 domWindow->GetCurrentInnerWindow() :
                                 domWindow.get();
    NS_ENSURE_STATE(innerWindow);

    nsCOMPtr<nsIScriptGlobalObject> newGlobal(do_QueryInterface(innerWindow));
    NS_ENSURE_TRUE(newGlobal, NS_ERROR_NO_INTERFACE);

    aGlobalObject = newGlobal;
  }

  nsRefPtr<nsDOMWorkerPool> pool;
  {
    nsAutoMonitor mon(mMonitor);

    if (!mThreadPool) {
      // Shutting down!
      return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
    }

    mPools.Get(aGlobalObject, getter_AddRefs(pool));
  }

  nsresult rv;

  if (!pool) {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

    if (!mNavigatorStringsLoaded) {
      nsCOMPtr<nsIDOMWindowInternal> internal(do_QueryInterface(aGlobalObject));
      NS_ENSURE_TRUE(internal, NS_ERROR_NO_INTERFACE);

      nsCOMPtr<nsIDOMNavigator> navigator;
      rv = internal->GetNavigator(getter_AddRefs(navigator));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = navigator->GetAppName(mAppName);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = navigator->GetAppVersion(mAppVersion);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = navigator->GetPlatform(mPlatform);
      NS_ENSURE_SUCCESS(rv, rv);

      rv = navigator->GetUserAgent(mUserAgent);
      NS_ENSURE_SUCCESS(rv, rv);

      mNavigatorStringsLoaded = PR_TRUE;
    }

    nsCOMPtr<nsPIDOMWindow> domWindow(do_QueryInterface(aGlobalObject));
    NS_ENSURE_TRUE(domWindow, NS_ERROR_NO_INTERFACE);

    nsIDOMDocument* domDocument = domWindow->GetExtantDocument();
    NS_ENSURE_STATE(domDocument);

    nsCOMPtr<nsIDocument> document(do_QueryInterface(domDocument));
    NS_ENSURE_STATE(document);

    pool = new nsDOMWorkerPool(aGlobalObject, document);
    NS_ENSURE_TRUE(pool, NS_ERROR_OUT_OF_MEMORY);

    rv = pool->Init();
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoMonitor mon(mMonitor);

    PRBool success = mPools.Put(aGlobalObject, pool);
    NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);
  }

  rv = pool->NoteWorker(aWorker);
  NS_ENSURE_SUCCESS(rv, rv);

  aWorker->SetPool(pool);
  return NS_OK;
}

void
nsDOMThreadService::GetAppName(nsAString& aAppName)
{
  NS_ASSERTION(mNavigatorStringsLoaded,
               "Shouldn't call this before we have loaded strings!");
  aAppName.Assign(mAppName);
}

void
nsDOMThreadService::GetAppVersion(nsAString& aAppVersion)
{
  NS_ASSERTION(mNavigatorStringsLoaded,
               "Shouldn't call this before we have loaded strings!");
  aAppVersion.Assign(mAppVersion);
}

void
nsDOMThreadService::GetPlatform(nsAString& aPlatform)
{
  NS_ASSERTION(mNavigatorStringsLoaded,
               "Shouldn't call this before we have loaded strings!");
  aPlatform.Assign(mPlatform);
}

void
nsDOMThreadService::GetUserAgent(nsAString& aUserAgent)
{
  NS_ASSERTION(mNavigatorStringsLoaded,
               "Shouldn't call this before we have loaded strings!");
  aUserAgent.Assign(mUserAgent);
}

void
nsDOMThreadService::RegisterPrefCallbacks()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  for (PRUint32 index = 0; index < NS_ARRAY_LENGTH(sPrefsToWatch); index++) {
    nsContentUtils::RegisterPrefCallback(sPrefsToWatch[index], PrefCallback,
                                         nsnull);
    PrefCallback(sPrefsToWatch[index], nsnull);
  }
}

void
nsDOMThreadService::UnregisterPrefCallbacks()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  for (PRUint32 index = 0; index < NS_ARRAY_LENGTH(sPrefsToWatch); index++) {
    nsContentUtils::UnregisterPrefCallback(sPrefsToWatch[index], PrefCallback,
                                           nsnull);
  }
}

// static
int
nsDOMThreadService::PrefCallback(const char* aPrefName,
                                 void* aClosure)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  if(!strcmp(aPrefName, "dom.max_script_run_time")) {
    // We assume atomic 32bit reads/writes. If this assumption doesn't hold on
    // some wacky platform then the worst that could happen is that the close
    // handler will run for a slightly different amount of time.
    PRUint32 timeoutMS =
      nsContentUtils::GetIntPref(aPrefName, gWorkerCloseHandlerTimeoutMS);

    // We must have a timeout value, 0 is not ok. If the pref is set to 0 then
    // fall back to our default.
    if (timeoutMS) {
      gWorkerCloseHandlerTimeoutMS = timeoutMS;
    }
  }
  return 0;
}

// static
PRUint32
nsDOMThreadService::GetWorkerCloseHandlerTimeoutMS()
{
  return gWorkerCloseHandlerTimeoutMS;
}
