/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "RuntimeService.h"

#include "nsIDOMChromeWindow.h"
#include "nsIDocument.h"
#include "nsIEffectiveTLDService.h"
#include "nsIObserverService.h"
#include "nsIPlatformCharset.h"
#include "nsIPrincipal.h"
#include "nsIJSContextStack.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupportsPriority.h"
#include "nsITimer.h"
#include "nsPIDOMWindow.h"

#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include <Navigator.h>
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"
#include "xpcpublic.h"

#include "Events.h"
#include "Worker.h"
#include "WorkerPrivate.h"

using namespace mozilla;
using namespace mozilla::dom;

USING_WORKERS_NAMESPACE

using mozilla::MutexAutoLock;
using mozilla::MutexAutoUnlock;
using mozilla::Preferences;

// The size of the worker runtime heaps in bytes. May be changed via pref.
#define WORKER_DEFAULT_RUNTIME_HEAPSIZE 32 * 1024 * 1024

// The C stack size. We use the same stack size on all platforms for
// consistency.
#define WORKER_STACK_SIZE 256 * sizeof(size_t) * 1024

// The stack limit the JS engine will check. 
#ifdef MOZ_ASAN
// For ASan, we need more stack space, so we use all that is available
#define WORKER_CONTEXT_NATIVE_STACK_LIMIT WORKER_STACK_SIZE
#else
// Half the size of the actual C stack, to be safe.
#define WORKER_CONTEXT_NATIVE_STACK_LIMIT 128 * sizeof(size_t) * 1024
#endif

// The maximum number of threads to use for workers, overridable via pref.
#define MAX_WORKERS_PER_DOMAIN 10

PR_STATIC_ASSERT(MAX_WORKERS_PER_DOMAIN >= 1);

// The default number of seconds that close handlers will be allowed to run.
#define MAX_SCRIPT_RUN_TIME_SEC 10

// The number of seconds that idle threads can hang around before being killed.
#define IDLE_THREAD_TIMEOUT_SEC 30

// The maximum number of threads that can be idle at one time.
#define MAX_IDLE_THREADS 20

#define PREF_WORKERS_ENABLED "dom.workers.enabled"
#define PREF_WORKERS_MAX_PER_DOMAIN "dom.workers.maxPerDomain"
#define PREF_WORKERS_GCZEAL "dom.workers.gczeal"
#define PREF_MAX_SCRIPT_RUN_TIME "dom.max_script_run_time"

#define GC_REQUEST_OBSERVER_TOPIC "child-gc-request"
#define MEMORY_PRESSURE_OBSERVER_TOPIC "memory-pressure"

#define BROADCAST_ALL_WORKERS(_func, ...)                                      \
  PR_BEGIN_MACRO                                                               \
    AssertIsOnMainThread();                                                    \
                                                                               \
    nsAutoTArray<WorkerPrivate*, 100> workers;                                 \
    {                                                                          \
      MutexAutoLock lock(mMutex);                                              \
                                                                               \
      mDomainMap.EnumerateRead(AddAllTopLevelWorkersToArray, &workers);        \
    }                                                                          \
                                                                               \
    if (!workers.IsEmpty()) {                                                  \
      AutoSafeJSContext cx;                                                    \
      for (PRUint32 index = 0; index < workers.Length(); index++) {            \
        workers[index]-> _func (cx, ##__VA_ARGS__);                            \
      }                                                                        \
    }                                                                          \
  PR_END_MACRO

namespace {

const PRUint32 kNoIndex = PRUint32(-1);

const PRUint32 kRequiredJSContextOptions =
  JSOPTION_DONT_REPORT_UNCAUGHT | JSOPTION_NO_SCRIPT_RVAL;

PRUint32 gMaxWorkersPerDomain = MAX_WORKERS_PER_DOMAIN;

// Does not hold an owning reference.
RuntimeService* gRuntimeService = nsnull;

enum {
  ID_Worker = 0,
  ID_ChromeWorker,
  ID_Event,
  ID_MessageEvent,
  ID_ErrorEvent,

  ID_COUNT
};

// These are jsids for the main runtime. Only touched on the main thread.
jsid gStringIDs[ID_COUNT] = { JSID_VOID };

const char* gStringChars[] = {
  "Worker",
  "ChromeWorker",
  "WorkerEvent",
  "WorkerMessageEvent",
  "WorkerErrorEvent"

  // XXX Don't care about ProgressEvent since it should never leak to the main
  // thread.
};

PR_STATIC_ASSERT(NS_ARRAY_LENGTH(gStringChars) == ID_COUNT);

enum {
  PREF_strict = 0,
  PREF_werror,
  PREF_relimit,
  PREF_methodjit,
  PREF_methodjit_always,
  PREF_typeinference,
  PREF_jit_hardening,
  PREF_mem_max,

#ifdef JS_GC_ZEAL
  PREF_gczeal,
#endif

  PREF_COUNT
};

#define JS_OPTIONS_DOT_STR "javascript.options."

const char* gPrefsToWatch[] = {
  JS_OPTIONS_DOT_STR "strict",
  JS_OPTIONS_DOT_STR "werror",
  JS_OPTIONS_DOT_STR "relimit",
  JS_OPTIONS_DOT_STR "methodjit.content",
  JS_OPTIONS_DOT_STR "methodjit_always",
  JS_OPTIONS_DOT_STR "typeinference",
  JS_OPTIONS_DOT_STR "jit_hardening",
  JS_OPTIONS_DOT_STR "mem.max"

#ifdef JS_GC_ZEAL
  , PREF_WORKERS_GCZEAL
#endif
};

PR_STATIC_ASSERT(NS_ARRAY_LENGTH(gPrefsToWatch) == PREF_COUNT);

int
PrefCallback(const char* aPrefName, void* aClosure)
{
  AssertIsOnMainThread();

  RuntimeService* rts = static_cast<RuntimeService*>(aClosure);
  NS_ASSERTION(rts, "This should never be null!");

  NS_NAMED_LITERAL_CSTRING(jsOptionStr, JS_OPTIONS_DOT_STR);

  if (!strcmp(aPrefName, gPrefsToWatch[PREF_mem_max])) {
    PRInt32 pref = Preferences::GetInt(aPrefName, -1);
    PRUint32 maxBytes = (pref <= 0 || pref >= 0x1000) ?
                        PRUint32(-1) :
                        PRUint32(pref) * 1024 * 1024;
    RuntimeService::SetDefaultJSRuntimeHeapSize(maxBytes);
    rts->UpdateAllWorkerJSRuntimeHeapSize();
  }
  else if (StringBeginsWith(nsDependentCString(aPrefName), jsOptionStr)) {
    PRUint32 newOptions = kRequiredJSContextOptions;
    if (Preferences::GetBool(gPrefsToWatch[PREF_strict])) {
      newOptions |= JSOPTION_STRICT;
    }
    if (Preferences::GetBool(gPrefsToWatch[PREF_werror])) {
      newOptions |= JSOPTION_WERROR;
    }
    if (Preferences::GetBool(gPrefsToWatch[PREF_relimit])) {
      newOptions |= JSOPTION_RELIMIT;
    }
    if (Preferences::GetBool(gPrefsToWatch[PREF_methodjit])) {
      newOptions |= JSOPTION_METHODJIT;
    }
    if (Preferences::GetBool(gPrefsToWatch[PREF_methodjit_always])) {
      newOptions |= JSOPTION_METHODJIT_ALWAYS;
    }
    if (Preferences::GetBool(gPrefsToWatch[PREF_typeinference])) {
      newOptions |= JSOPTION_TYPE_INFERENCE;
    }
    newOptions |= JSOPTION_ALLOW_XML;

    RuntimeService::SetDefaultJSContextOptions(newOptions);
    rts->UpdateAllWorkerJSContextOptions();
  }
#ifdef JS_GC_ZEAL
  else if (!strcmp(aPrefName, gPrefsToWatch[PREF_gczeal])) {
    PRInt32 gczeal = Preferences::GetInt(gPrefsToWatch[PREF_gczeal]);
    RuntimeService::SetDefaultGCZeal(PRUint8(clamped(gczeal, 0, 3)));
    rts->UpdateAllWorkerGCZeal();
  }
#endif
  return 0;
}

void
ErrorReporter(JSContext* aCx, const char* aMessage, JSErrorReport* aReport)
{
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  return worker->ReportError(aCx, aMessage, aReport);
}

JSBool
OperationCallback(JSContext* aCx)
{
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  return worker->OperationCallback(aCx);
}

JSContext*
CreateJSContextForWorker(WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(!aWorkerPrivate->GetJSContext(), "Already has a context!");

  // The number passed here doesn't matter, we're about to change it in the call
  // to JS_SetGCParameter.
  JSRuntime* runtime = JS_NewRuntime(WORKER_DEFAULT_RUNTIME_HEAPSIZE);
  if (!runtime) {
    NS_WARNING("Could not create new runtime!");
    return nsnull;
  }

  // This is the real place where we set the max memory for the runtime.
  JS_SetGCParameter(runtime, JSGC_MAX_BYTES,
                    aWorkerPrivate->GetJSRuntimeHeapSize());

  JS_SetNativeStackQuota(runtime, WORKER_CONTEXT_NATIVE_STACK_LIMIT);

  JSContext* workerCx = JS_NewContext(runtime, 0);
  if (!workerCx) {
    JS_DestroyRuntime(runtime);
    NS_WARNING("Could not create new context!");
    return nsnull;
  }

  JS_SetContextPrivate(workerCx, aWorkerPrivate);

  JS_SetErrorReporter(workerCx, ErrorReporter);

  JS_SetOperationCallback(workerCx, OperationCallback);

  NS_ASSERTION((aWorkerPrivate->GetJSContextOptions() &
                kRequiredJSContextOptions) == kRequiredJSContextOptions,
               "Somehow we lost our required options!");
  JS_SetOptions(workerCx, aWorkerPrivate->GetJSContextOptions());

#ifdef JS_GC_ZEAL
  {
    PRUint8 zeal = aWorkerPrivate->GetGCZeal();
    NS_ASSERTION(zeal <= 3, "Bad zeal value!");

    PRUint32 frequency = zeal <= 2 ? JS_DEFAULT_ZEAL_FREQ : 1;
    JS_SetGCZeal(workerCx, zeal, frequency);
  }
#endif

  if (aWorkerPrivate->IsChromeWorker()) {
    JS_SetVersion(workerCx, JSVERSION_LATEST);
  }

  return workerCx;
}

class WorkerThreadRunnable : public nsRunnable
{
  WorkerPrivate* mWorkerPrivate;

public:
  WorkerThreadRunnable(WorkerPrivate* aWorkerPrivate)
  : mWorkerPrivate(aWorkerPrivate)
  {
    NS_ASSERTION(mWorkerPrivate, "This should never be null!");
  }

  NS_IMETHOD
  Run()
  {
    WorkerPrivate* workerPrivate = mWorkerPrivate;
    mWorkerPrivate = nsnull;

    workerPrivate->AssertIsOnWorkerThread();

    JSContext* cx = CreateJSContextForWorker(workerPrivate);
    if (!cx) {
      // XXX need to fire an error at parent.
      NS_ERROR("Failed to create runtime and context!");
      return NS_ERROR_FAILURE;
    }

    {
      JSAutoRequest ar(cx);
      workerPrivate->DoRunLoop(cx);
    }

    JSRuntime* rt = JS_GetRuntime(cx);

    // XXX Bug 666963 - CTypes can create another JSContext for use with
    // closures, and then it holds that context in a reserved slot on the CType
    // prototype object. We have to destroy that context before we can destroy
    // the runtime, and we also have to make sure that it isn't the last context
    // to be destroyed (otherwise it will assert). To accomplish this we create
    // an unused dummy context, destroy our real context, and then destroy the
    // dummy. Once this bug is resolved we can remove this nastiness and simply
    // call JS_DestroyContextNoGC on our context.
    JSContext* dummyCx = JS_NewContext(rt, 0);
    if (dummyCx) {
      JS_DestroyContext(cx);
      JS_DestroyContext(dummyCx);
    }
    else {
      NS_WARNING("Failed to create dummy context!");
      JS_DestroyContext(cx);
    }

    JS_DestroyRuntime(rt);

    workerPrivate->ScheduleDeletion(false);
    return NS_OK;
  }
};

} /* anonymous namespace */

BEGIN_WORKERS_NAMESPACE

// Entry point for the DOM.
JSBool
ResolveWorkerClasses(JSContext* aCx, JSHandleObject aObj, JSHandleId aId, unsigned aFlags,
                     JSObject** aObjp)
{
  AssertIsOnMainThread();

  // Don't care about assignments or declarations, bail now.
  if (aFlags & (JSRESOLVE_ASSIGNING | JSRESOLVE_DECLARING)) {
    *aObjp = nsnull;
    return true;
  }

  // Make sure our strings are interned.
  if (JSID_IS_VOID(gStringIDs[0])) {
    for (PRUint32 i = 0; i < ID_COUNT; i++) {
      JSString* str = JS_InternString(aCx, gStringChars[i]);
      if (!str) {
        while (i) {
          gStringIDs[--i] = JSID_VOID;
        }
        return false;
      }
      gStringIDs[i] = INTERNED_STRING_TO_JSID(aCx, str);
    }
  }

  bool isChrome = false;
  bool shouldResolve = false;

  for (PRUint32 i = 0; i < ID_COUNT; i++) {
    if (gStringIDs[i] == aId) {
      nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
      NS_ASSERTION(ssm, "This should never be null!");

      bool enabled;
      if (NS_FAILED(ssm->IsCapabilityEnabled("UniversalXPConnect", &enabled))) {
        NS_WARNING("IsCapabilityEnabled failed!");
        isChrome = false;
      }

      isChrome = !!enabled;

      // Don't resolve if this is ChromeWorker and we're not chrome. Otherwise
      // always resolve.
      shouldResolve = gStringIDs[ID_ChromeWorker] == aId ? isChrome : true;
      break;
    }
  }

  if (shouldResolve) {
    // Don't do anything if workers are disabled.
    if (!isChrome && !Preferences::GetBool(PREF_WORKERS_ENABLED)) {
      *aObjp = nsnull;
      return true;
    }

    JSObject* eventTarget = EventTargetBinding_workers::GetProtoObject(aCx, aObj, aObj);
    if (!eventTarget) {
      return false;
    }

    JSObject* worker = worker::InitClass(aCx, aObj, eventTarget, true);
    if (!worker) {
      return false;
    }

    if (isChrome && !chromeworker::InitClass(aCx, aObj, worker, true)) {
      return false;
    }

    if (!events::InitClasses(aCx, aObj, true)) {
      return false;
    }

    *aObjp = aObj;
    return true;
  }

  // Not resolved.
  *aObjp = nsnull;
  return true;
}

void
CancelWorkersForWindow(JSContext* aCx, nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->CancelWorkersForWindow(aCx, aWindow);
  }
}

void
SuspendWorkersForWindow(JSContext* aCx, nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->SuspendWorkersForWindow(aCx, aWindow);
  }
}

void
ResumeWorkersForWindow(JSContext* aCx, nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->ResumeWorkersForWindow(aCx, aWindow);
  }
}

namespace {

class WorkerTaskRunnable : public WorkerRunnable
{
public:
  WorkerTaskRunnable(WorkerPrivate* aPrivate, WorkerTask* aTask)
    : WorkerRunnable(aPrivate, WorkerThread, UnchangedBusyCount,
                     SkipWhenClearing),
      mTask(aTask)
  { }

  virtual bool PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate) {
    return true;
  }

  virtual void PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                            bool aDispatchResult)
  { }

  virtual bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate);

private:
  nsRefPtr<WorkerTask> mTask;
};

bool
WorkerTaskRunnable::WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
  return mTask->RunTask(aCx);
}

}

bool
WorkerCrossThreadDispatcher::PostTask(WorkerTask* aTask)
{
  mozilla::MutexAutoLock lock(mMutex);
  if (!mPrivate) {
    return false;
  }

  nsRefPtr<WorkerTaskRunnable> runnable = new WorkerTaskRunnable(mPrivate, aTask);
  runnable->Dispatch(nsnull);
  return true;
}

END_WORKERS_NAMESPACE

PRUint32 RuntimeService::sDefaultJSContextOptions = kRequiredJSContextOptions;

PRUint32 RuntimeService::sDefaultJSRuntimeHeapSize =
  WORKER_DEFAULT_RUNTIME_HEAPSIZE;

PRInt32 RuntimeService::sCloseHandlerTimeoutSeconds = MAX_SCRIPT_RUN_TIME_SEC;

#ifdef JS_GC_ZEAL
PRUint8 RuntimeService::sDefaultGCZeal = 0;
#endif

RuntimeService::RuntimeService()
: mMutex("RuntimeService::mMutex"), mObserved(false),
  mShuttingDown(false), mNavigatorStringsLoaded(false)
{
  AssertIsOnMainThread();
  NS_ASSERTION(!gRuntimeService, "More than one service!");
}

RuntimeService::~RuntimeService()
{
  AssertIsOnMainThread();

  // gRuntimeService can be null if Init() fails.
  NS_ASSERTION(!gRuntimeService || gRuntimeService == this,
               "More than one service!");

  gRuntimeService = nsnull;
}

// static
RuntimeService*
RuntimeService::GetOrCreateService()
{
  AssertIsOnMainThread();

  if (!gRuntimeService) {
    nsRefPtr<RuntimeService> service = new RuntimeService();
    if (NS_FAILED(service->Init())) {
      NS_WARNING("Failed to initialize!");
      service->Cleanup();
      return nsnull;
    }

    // The observer service now owns us until shutdown.
    gRuntimeService = service;
  }

  return gRuntimeService;
}

// static
RuntimeService*
RuntimeService::GetService()
{
  return gRuntimeService;
}

bool
RuntimeService::RegisterWorker(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnParentThread();

  WorkerPrivate* parent = aWorkerPrivate->GetParent();
  if (!parent) {
    AssertIsOnMainThread();

    if (mShuttingDown) {
      JS_ReportError(aCx, "Cannot create worker during shutdown!");
      return false;
    }
  }

  WorkerDomainInfo* domainInfo;
  bool queued = false;
  {
    const nsCString& domain = aWorkerPrivate->Domain();

    MutexAutoLock lock(mMutex);

    if (!mDomainMap.Get(domain, &domainInfo)) {
      NS_ASSERTION(!parent, "Shouldn't have a parent here!");

      domainInfo = new WorkerDomainInfo();
      domainInfo->mDomain = domain;
      mDomainMap.Put(domain, domainInfo);
    }

    if (domainInfo) {
      queued = gMaxWorkersPerDomain &&
               domainInfo->ActiveWorkerCount() >= gMaxWorkersPerDomain &&
               !domain.IsEmpty();

      if (queued) {
        domainInfo->mQueuedWorkers.AppendElement(aWorkerPrivate);
      }
      else if (parent) {
        domainInfo->mChildWorkerCount++;
      }
      else {
        domainInfo->mActiveWorkers.AppendElement(aWorkerPrivate);
      }
    }
  }

  if (!domainInfo) {
    JS_ReportOutOfMemory(aCx);
    return false;
  }

  // From here on out we must call UnregisterWorker if something fails!
  if (parent) {
    if (!parent->AddChildWorker(aCx, aWorkerPrivate)) {
      UnregisterWorker(aCx, aWorkerPrivate);
      return false;
    }
  }
  else {
    if (!mNavigatorStringsLoaded) {
      if (NS_FAILED(NS_GetNavigatorAppName(mNavigatorStrings.mAppName)) ||
          NS_FAILED(NS_GetNavigatorAppVersion(mNavigatorStrings.mAppVersion)) ||
          NS_FAILED(NS_GetNavigatorPlatform(mNavigatorStrings.mPlatform)) ||
          NS_FAILED(NS_GetNavigatorUserAgent(mNavigatorStrings.mUserAgent))) {
        JS_ReportError(aCx, "Failed to load navigator strings!");
        UnregisterWorker(aCx, aWorkerPrivate);
        return false;
      }

      mNavigatorStringsLoaded = true;
    }

    nsPIDOMWindow* window = aWorkerPrivate->GetWindow();

    nsTArray<WorkerPrivate*>* windowArray;
    if (!mWindowMap.Get(window, &windowArray)) {
      NS_ASSERTION(!parent, "Shouldn't have a parent here!");

      windowArray = new nsTArray<WorkerPrivate*>(1);
      mWindowMap.Put(window, windowArray);
    }

    NS_ASSERTION(!windowArray->Contains(aWorkerPrivate),
                 "Already know about this worker!");
    windowArray->AppendElement(aWorkerPrivate);
  }

  if (!queued && !ScheduleWorker(aCx, aWorkerPrivate)) {
    return false;
  }

  return true;
}

void
RuntimeService::UnregisterWorker(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnParentThread();

  WorkerPrivate* parent = aWorkerPrivate->GetParent();
  if (!parent) {
    AssertIsOnMainThread();
  }

  WorkerPrivate* queuedWorker = nsnull;
  {
    const nsCString& domain = aWorkerPrivate->Domain();

    MutexAutoLock lock(mMutex);

    WorkerDomainInfo* domainInfo;
    if (!mDomainMap.Get(domain, &domainInfo)) {
      NS_ERROR("Don't have an entry for this domain!");
    }

    // Remove old worker from everywhere.
    PRUint32 index = domainInfo->mQueuedWorkers.IndexOf(aWorkerPrivate);
    if (index != kNoIndex) {
      // Was queued, remove from the list.
      domainInfo->mQueuedWorkers.RemoveElementAt(index);
    }
    else if (parent) {
      NS_ASSERTION(domainInfo->mChildWorkerCount, "Must be non-zero!");
      domainInfo->mChildWorkerCount--;
    }
    else {
      NS_ASSERTION(domainInfo->mActiveWorkers.Contains(aWorkerPrivate),
                   "Don't know about this worker!");
      domainInfo->mActiveWorkers.RemoveElement(aWorkerPrivate);
    }

    // See if there's a queued worker we can schedule.
    if (domainInfo->ActiveWorkerCount() < gMaxWorkersPerDomain &&
        !domainInfo->mQueuedWorkers.IsEmpty()) {
      queuedWorker = domainInfo->mQueuedWorkers[0];
      domainInfo->mQueuedWorkers.RemoveElementAt(0);

      if (queuedWorker->GetParent()) {
        domainInfo->mChildWorkerCount++;
      }
      else {
        domainInfo->mActiveWorkers.AppendElement(queuedWorker);
      }
    }

    if (!domainInfo->ActiveWorkerCount()) {
      NS_ASSERTION(domainInfo->mQueuedWorkers.IsEmpty(), "Huh?!");
      mDomainMap.Remove(domain);
    }
  }

  if (parent) {
    parent->RemoveChildWorker(aCx, aWorkerPrivate);
  }
  else {
    nsPIDOMWindow* window = aWorkerPrivate->GetWindow();

    nsTArray<WorkerPrivate*>* windowArray;
    if (!mWindowMap.Get(window, &windowArray)) {
      NS_ERROR("Don't have an entry for this window!");
    }

    NS_ASSERTION(windowArray->Contains(aWorkerPrivate),
                 "Don't know about this worker!");
    windowArray->RemoveElement(aWorkerPrivate);

    if (windowArray->IsEmpty()) {
      NS_ASSERTION(!queuedWorker, "How can this be?!");
      mWindowMap.Remove(window);
    }
  }

  if (queuedWorker && !ScheduleWorker(aCx, queuedWorker)) {
    UnregisterWorker(aCx, queuedWorker);
  }
}

bool
RuntimeService::ScheduleWorker(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
  if (!aWorkerPrivate->Start()) {
    // This is ok, means that we didn't need to make a thread for this worker.
    return true;
  }

  nsCOMPtr<nsIThread> thread;
  {
    MutexAutoLock lock(mMutex);
    if (!mIdleThreadArray.IsEmpty()) {
      PRUint32 index = mIdleThreadArray.Length() - 1;
      mIdleThreadArray[index].mThread.swap(thread);
      mIdleThreadArray.RemoveElementAt(index);
    }
  }

  if (!thread) {
    if (NS_FAILED(NS_NewThread(getter_AddRefs(thread), nsnull,
                               WORKER_STACK_SIZE))) {
      UnregisterWorker(aCx, aWorkerPrivate);
      JS_ReportError(aCx, "Could not create new thread!");
      return false;
    }

    nsCOMPtr<nsISupportsPriority> priority = do_QueryInterface(thread);
    if (!priority ||
        NS_FAILED(priority->SetPriority(nsISupportsPriority::PRIORITY_LOW))) {
      NS_WARNING("Could not lower the new thread's priority!");
    }
  }

#ifdef DEBUG
  aWorkerPrivate->SetThread(thread);
#endif

  nsCOMPtr<nsIRunnable> runnable = new WorkerThreadRunnable(aWorkerPrivate);
  if (NS_FAILED(thread->Dispatch(runnable, NS_DISPATCH_NORMAL))) {
    UnregisterWorker(aCx, aWorkerPrivate);
    JS_ReportError(aCx, "Could not dispatch to thread!");
    return false;
  }

  return true;
}

// static
void
RuntimeService::ShutdownIdleThreads(nsITimer* aTimer, void* /* aClosure */)
{
  AssertIsOnMainThread();

  RuntimeService* runtime = RuntimeService::GetService();
  NS_ASSERTION(runtime, "This should never be null!");

  NS_ASSERTION(aTimer == runtime->mIdleThreadTimer, "Wrong timer!");

  // Cheat a little and grab all threads that expire within one second of now.
  TimeStamp now = TimeStamp::Now() + TimeDuration::FromSeconds(1);

  TimeStamp nextExpiration;

  nsAutoTArray<nsCOMPtr<nsIThread>, 20> expiredThreads;
  {
    MutexAutoLock lock(runtime->mMutex);

    for (PRUint32 index = 0; index < runtime->mIdleThreadArray.Length();
         index++) {
      IdleThreadInfo& info = runtime->mIdleThreadArray[index];
      if (info.mExpirationTime > now) {
        nextExpiration = info.mExpirationTime;
        break;
      }

      nsCOMPtr<nsIThread>* thread = expiredThreads.AppendElement();
      thread->swap(info.mThread);
    }

    if (!expiredThreads.IsEmpty()) {
      runtime->mIdleThreadArray.RemoveElementsAt(0, expiredThreads.Length());
    }
  }

  NS_ASSERTION(nextExpiration.IsNull() || !expiredThreads.IsEmpty(),
               "Should have a new time or there should be some threads to shut "
               "down");

  for (PRUint32 index = 0; index < expiredThreads.Length(); index++) {
    if (NS_FAILED(expiredThreads[index]->Shutdown())) {
      NS_WARNING("Failed to shutdown thread!");
    }
  }

  if (!nextExpiration.IsNull()) {
    TimeDuration delta = nextExpiration - TimeStamp::Now();
    PRUint32 delay(delta > TimeDuration(0) ? delta.ToMilliseconds() : 0);

    // Reschedule the timer.
    if (NS_FAILED(aTimer->InitWithFuncCallback(ShutdownIdleThreads, nsnull,
                                               delay,
                                               nsITimer::TYPE_ONE_SHOT))) {
      NS_ERROR("Can't schedule timer!");
    }
  }
}

nsresult
RuntimeService::Init()
{
  AssertIsOnMainThread();

  mIdleThreadTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  NS_ENSURE_STATE(mIdleThreadTimer);

  mDomainMap.Init();
  mWindowMap.Init();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_FAILURE);

  nsresult rv =
    obs->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false);
  NS_ENSURE_SUCCESS(rv, rv);

  mObserved = true;

  if (NS_FAILED(obs->AddObserver(this, GC_REQUEST_OBSERVER_TOPIC, false))) {
    NS_WARNING("Failed to register for GC request notifications!");
  }

  if (NS_FAILED(obs->AddObserver(this, MEMORY_PRESSURE_OBSERVER_TOPIC,
                                 false))) {
    NS_WARNING("Failed to register for memory pressure notifications!");
  }

  for (PRUint32 index = 0; index < ArrayLength(gPrefsToWatch); index++) {
    if (NS_FAILED(Preferences::RegisterCallback(PrefCallback,
                                                gPrefsToWatch[index], this))) {
      NS_WARNING("Failed to register pref callback?!");
    }
    PrefCallback(gPrefsToWatch[index], this);
  }

  // We assume atomic 32bit reads/writes. If this assumption doesn't hold on
  // some wacky platform then the worst that could happen is that the close
  // handler will run for a slightly different amount of time.
  if (NS_FAILED(Preferences::AddIntVarCache(&sCloseHandlerTimeoutSeconds,
                                            PREF_MAX_SCRIPT_RUN_TIME,
                                            MAX_SCRIPT_RUN_TIME_SEC))) {
      NS_WARNING("Failed to register timeout cache?!");
  }

  PRInt32 maxPerDomain = Preferences::GetInt(PREF_WORKERS_MAX_PER_DOMAIN,
                                             MAX_WORKERS_PER_DOMAIN);
  gMaxWorkersPerDomain = NS_MAX(0, maxPerDomain);

  mDetectorName = Preferences::GetLocalizedCString("intl.charset.detector");

  nsCOMPtr<nsIPlatformCharset> platformCharset =
    do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = platformCharset->GetCharset(kPlatformCharsetSel_PlainTextInFile,
                                     mSystemCharset);
  }

  return NS_OK;
}

// This spins the event loop until all workers are finished and their threads
// have been joined.
void
RuntimeService::Cleanup()
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_WARN_IF_FALSE(obs, "Failed to get observer service?!");

  // Tell anyone that cares that they're about to lose worker support.
  if (obs && NS_FAILED(obs->NotifyObservers(nsnull, WORKERS_SHUTDOWN_TOPIC,
                                            nsnull))) {
    NS_WARNING("NotifyObservers failed!");
  }

  // That's it, no more workers.
  mShuttingDown = true;

  if (mIdleThreadTimer) {
    if (NS_FAILED(mIdleThreadTimer->Cancel())) {
      NS_WARNING("Failed to cancel idle timer!");
    }
    mIdleThreadTimer = nsnull;
  }

  if (mDomainMap.IsInitialized()) {
    MutexAutoLock lock(mMutex);

    nsAutoTArray<WorkerPrivate*, 100> workers;
    mDomainMap.EnumerateRead(AddAllTopLevelWorkersToArray, &workers);

    if (!workers.IsEmpty()) {
      nsIThread* currentThread;

      // Cancel all top-level workers.
      {
        MutexAutoUnlock unlock(mMutex);

        currentThread = NS_GetCurrentThread();
        NS_ASSERTION(currentThread, "This should never be null!");

        AutoSafeJSContext cx;

        for (PRUint32 index = 0; index < workers.Length(); index++) {
          if (!workers[index]->Kill(cx)) {
            NS_WARNING("Failed to cancel worker!");
          }
        }
      }

      // Shut down any idle threads.
      if (!mIdleThreadArray.IsEmpty()) {
        nsAutoTArray<nsCOMPtr<nsIThread>, 20> idleThreads;

        PRUint32 idleThreadCount = mIdleThreadArray.Length();
        idleThreads.SetLength(idleThreadCount);

        for (PRUint32 index = 0; index < idleThreadCount; index++) {
          NS_ASSERTION(mIdleThreadArray[index].mThread, "Null thread!");
          idleThreads[index].swap(mIdleThreadArray[index].mThread);
        }

        mIdleThreadArray.Clear();

        MutexAutoUnlock unlock(mMutex);

        for (PRUint32 index = 0; index < idleThreadCount; index++) {
          if (NS_FAILED(idleThreads[index]->Shutdown())) {
            NS_WARNING("Failed to shutdown thread!");
          }
        }
      }

      // And make sure all their final messages have run and all their threads
      // have joined.
      while (mDomainMap.Count()) {
        MutexAutoUnlock unlock(mMutex);

        if (!NS_ProcessNextEvent(currentThread)) {
          NS_WARNING("Something bad happened!");
          break;
        }
      }
    }
  }

  if (mWindowMap.IsInitialized()) {
    NS_ASSERTION(!mWindowMap.Count(), "All windows should have been released!");
  }

  if (mObserved) {
    for (PRUint32 index = 0; index < ArrayLength(gPrefsToWatch); index++) {
      Preferences::UnregisterCallback(PrefCallback, gPrefsToWatch[index], this);
    }

    if (obs) {
      if (NS_FAILED(obs->RemoveObserver(this, GC_REQUEST_OBSERVER_TOPIC))) {
        NS_WARNING("Failed to unregister for GC request notifications!");
      }

      if (NS_FAILED(obs->RemoveObserver(this,
                                        MEMORY_PRESSURE_OBSERVER_TOPIC))) {
        NS_WARNING("Failed to unregister for memory pressure notifications!");
      }

      nsresult rv =
        obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID);
      mObserved = NS_FAILED(rv);
    }
  }
}

// static
PLDHashOperator
RuntimeService::AddAllTopLevelWorkersToArray(const nsACString& aKey,
                                             WorkerDomainInfo* aData,
                                             void* aUserArg)
{
  nsTArray<WorkerPrivate*>* array =
    static_cast<nsTArray<WorkerPrivate*>*>(aUserArg);

#ifdef DEBUG
  for (PRUint32 index = 0; index < aData->mActiveWorkers.Length(); index++) {
    NS_ASSERTION(!aData->mActiveWorkers[index]->GetParent(),
                 "Shouldn't have a parent in this list!");
  }
#endif

  array->AppendElements(aData->mActiveWorkers);

  // These might not be top-level workers...
  for (PRUint32 index = 0; index < aData->mQueuedWorkers.Length(); index++) {
    WorkerPrivate* worker = aData->mQueuedWorkers[index];
    if (!worker->GetParent()) {
      array->AppendElement(worker);
    }
  }

  return PL_DHASH_NEXT;
}

void
RuntimeService::GetWorkersForWindow(nsPIDOMWindow* aWindow,
                                    nsTArray<WorkerPrivate*>& aWorkers)
{
  AssertIsOnMainThread();

  nsTArray<WorkerPrivate*>* workers;
  if (mWindowMap.Get(aWindow, &workers)) {
    NS_ASSERTION(!workers->IsEmpty(), "Should have been removed!");
    aWorkers.AppendElements(*workers);
  }
  else {
    NS_ASSERTION(aWorkers.IsEmpty(), "Should be empty!");
  }
}

void
RuntimeService::CancelWorkersForWindow(JSContext* aCx,
                                       nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();

  nsAutoTArray<WorkerPrivate*, 100> workers;
  GetWorkersForWindow(aWindow, workers);

  if (!workers.IsEmpty()) {
    AutoSafeJSContext cx(aCx);
    for (PRUint32 index = 0; index < workers.Length(); index++) {
      if (!workers[index]->Cancel(aCx)) {
        NS_WARNING("Failed to cancel worker!");
      }
    }
  }
}

void
RuntimeService::SuspendWorkersForWindow(JSContext* aCx,
                                        nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();

  nsAutoTArray<WorkerPrivate*, 100> workers;
  GetWorkersForWindow(aWindow, workers);

  if (!workers.IsEmpty()) {
    AutoSafeJSContext cx(aCx);
    for (PRUint32 index = 0; index < workers.Length(); index++) {
      if (!workers[index]->Suspend(aCx)) {
        NS_WARNING("Failed to cancel worker!");
      }
    }
  }
}

void
RuntimeService::ResumeWorkersForWindow(JSContext* aCx,
                                       nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();

  nsAutoTArray<WorkerPrivate*, 100> workers;
  GetWorkersForWindow(aWindow, workers);

  if (!workers.IsEmpty()) {
    AutoSafeJSContext cx(aCx);
    for (PRUint32 index = 0; index < workers.Length(); index++) {
      if (!workers[index]->Resume(aCx)) {
        NS_WARNING("Failed to cancel worker!");
      }
    }
  }
}

void
RuntimeService::NoteIdleThread(nsIThread* aThread)
{
  AssertIsOnMainThread();
  NS_ASSERTION(aThread, "Null pointer!");

  static TimeDuration timeout =
    TimeDuration::FromSeconds(IDLE_THREAD_TIMEOUT_SEC);

  TimeStamp expirationTime = TimeStamp::Now() + timeout;

  bool shutdown;
  if (mShuttingDown) {
    shutdown = true;
  }
  else {
    MutexAutoLock lock(mMutex);

    if (mIdleThreadArray.Length() < MAX_IDLE_THREADS) {
      IdleThreadInfo* info = mIdleThreadArray.AppendElement();
      info->mThread = aThread;
      info->mExpirationTime = expirationTime;
      shutdown = false;
    }
    else {
      shutdown = true;
    }
  }

  // Too many idle threads, just shut this one down.
  if (shutdown) {
    if (NS_FAILED(aThread->Shutdown())) {
      NS_WARNING("Failed to shutdown thread!");
    }
    return;
  }

  // Schedule timer.
  if (NS_FAILED(mIdleThreadTimer->
                  InitWithFuncCallback(ShutdownIdleThreads, nsnull,
                                       IDLE_THREAD_TIMEOUT_SEC * 1000,
                                       nsITimer::TYPE_ONE_SHOT))) {
    NS_ERROR("Can't schedule timer!");
  }
}

void
RuntimeService::UpdateAllWorkerJSContextOptions()
{
  BROADCAST_ALL_WORKERS(UpdateJSContextOptions, GetDefaultJSContextOptions());
}

void
RuntimeService::UpdateAllWorkerJSRuntimeHeapSize()
{
  BROADCAST_ALL_WORKERS(UpdateJSRuntimeHeapSize, GetDefaultJSRuntimeHeapSize());
}

#ifdef JS_GC_ZEAL
void
RuntimeService::UpdateAllWorkerGCZeal()
{
  BROADCAST_ALL_WORKERS(UpdateGCZeal, GetDefaultGCZeal());
}
#endif

void
RuntimeService::GarbageCollectAllWorkers(bool aShrinking)
{
  BROADCAST_ALL_WORKERS(GarbageCollect, aShrinking);
}

// nsISupports
NS_IMPL_ISUPPORTS1(RuntimeService, nsIObserver)

// nsIObserver
NS_IMETHODIMP
RuntimeService::Observe(nsISupports* aSubject, const char* aTopic,
                        const PRUnichar* aData)
{
  AssertIsOnMainThread();

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID)) {
    Cleanup();
    return NS_OK;
  }
  if (!strcmp(aTopic, GC_REQUEST_OBSERVER_TOPIC)) {
    GarbageCollectAllWorkers(false);
    return NS_OK;
  }
  if (!strcmp(aTopic, MEMORY_PRESSURE_OBSERVER_TOPIC)) {
    GarbageCollectAllWorkers(true);
    return NS_OK;
  }

  NS_NOTREACHED("Unknown observer topic!");
  return NS_OK;
}

RuntimeService::AutoSafeJSContext::AutoSafeJSContext(JSContext* aCx)
: mContext(aCx ? aCx : GetSafeContext())
{
  AssertIsOnMainThread();

  if (mContext) {
    nsIThreadJSContextStack* stack = nsContentUtils::ThreadJSContextStack();
    NS_ASSERTION(stack, "This should never be null!");

    if (NS_FAILED(stack->Push(mContext))) {
      NS_ERROR("Couldn't push safe JSContext!");
      mContext = nsnull;
      return;
    }

    JS_BeginRequest(mContext);
  }
}

RuntimeService::AutoSafeJSContext::~AutoSafeJSContext()
{
  AssertIsOnMainThread();

  if (mContext) {
    JS_ReportPendingException(mContext);

    JS_EndRequest(mContext);

    nsIThreadJSContextStack* stack = nsContentUtils::ThreadJSContextStack();
    NS_ASSERTION(stack, "This should never be null!");

    JSContext* cx;
    if (NS_FAILED(stack->Pop(&cx))) {
      NS_ERROR("Failed to pop safe context!");
    }
    if (cx != mContext) {
      NS_ERROR("Mismatched context!");
    }
  }
}

// static
JSContext*
RuntimeService::AutoSafeJSContext::GetSafeContext()
{
  AssertIsOnMainThread();

  nsIThreadJSContextStack* stack = nsContentUtils::ThreadJSContextStack();
  NS_ASSERTION(stack, "This should never be null!");

  JSContext* cx = stack->GetSafeJSContext();
  if (!cx) {
    NS_ERROR("Couldn't get safe JSContext!");
    return nsnull;
  }

  NS_ASSERTION(!JS_IsExceptionPending(cx), "Already has an exception?!");
  return cx;
}
