/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RuntimeService.h"

#include "nsIChannel.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocument.h"
#include "nsIDOMChromeWindow.h"
#include "nsIEffectiveTLDService.h"
#include "nsIObserverService.h"
#include "nsIPlatformCharset.h"
#include "nsIPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupportsPriority.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsPIDOMWindow.h"

#include <algorithm>
#include "GeckoProfiler.h"
#include "js/OldDebugAPI.h"
#include "jsfriendapi.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/dom/asmjscache/AsmJSCache.h"
#include "mozilla/dom/AtomList.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Navigator.h"
#include "nsContentUtils.h"
#include "nsCycleCollector.h"
#include "nsDOMJSUtils.h"
#include "nsLayoutStatics.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsTraceRefcnt.h"
#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"
#include "OSFileConstants.h"
#include "xpcpublic.h"

#include "SharedWorker.h"
#include "WorkerPrivate.h"

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

USING_WORKERS_NAMESPACE

using mozilla::MutexAutoLock;
using mozilla::MutexAutoUnlock;
using mozilla::Preferences;

// The size of the worker runtime heaps in bytes. May be changed via pref.
#define WORKER_DEFAULT_RUNTIME_HEAPSIZE 32 * 1024 * 1024

// The size of the worker JS allocation threshold in MB. May be changed via pref.
#define WORKER_DEFAULT_ALLOCATION_THRESHOLD 30

// The C stack size. We use the same stack size on all platforms for
// consistency.
#define WORKER_STACK_SIZE 256 * sizeof(size_t) * 1024

// Half the size of the actual C stack, to be safe.
#define WORKER_CONTEXT_NATIVE_STACK_LIMIT 128 * sizeof(size_t) * 1024

// The maximum number of threads to use for workers, overridable via pref.
#define MAX_WORKERS_PER_DOMAIN 10

static_assert(MAX_WORKERS_PER_DOMAIN >= 1,
              "We should allow at least one worker per domain.");

// The default number of seconds that close handlers will be allowed to run for
// content workers.
#define MAX_SCRIPT_RUN_TIME_SEC 10

// The number of seconds that idle threads can hang around before being killed.
#define IDLE_THREAD_TIMEOUT_SEC 30

// The maximum number of threads that can be idle at one time.
#define MAX_IDLE_THREADS 20

#define PREF_WORKERS_PREFIX "dom.workers."
#define PREF_WORKERS_MAX_PER_DOMAIN PREF_WORKERS_PREFIX "maxPerDomain"

#define PREF_MAX_SCRIPT_RUN_TIME_CONTENT "dom.max_script_run_time"
#define PREF_MAX_SCRIPT_RUN_TIME_CHROME "dom.max_chrome_script_run_time"

#define GC_REQUEST_OBSERVER_TOPIC "child-gc-request"
#define CC_REQUEST_OBSERVER_TOPIC "child-cc-request"
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
      JSAutoRequest ar(cx);                                                    \
      for (uint32_t index = 0; index < workers.Length(); index++) {            \
        workers[index]-> _func (cx, __VA_ARGS__);                              \
      }                                                                        \
    }                                                                          \
  PR_END_MACRO

// Prefixes for observing preference changes.
#define PREF_JS_OPTIONS_PREFIX "javascript.options."
#define PREF_WORKERS_OPTIONS_PREFIX PREF_WORKERS_PREFIX "options."
#define PREF_MEM_OPTIONS_PREFIX "mem."
#define PREF_JIT_HARDENING "jit_hardening"
#define PREF_GCZEAL "gcZeal"

#if !(defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP))
#define DUMP_CONTROLLED_BY_PREF 1
#define PREF_DOM_WINDOW_DUMP_ENABLED "browser.dom.window.dump.enabled"
#endif

#define PREF_PROMISE_ENABLED "dom.promise.enabled"
#define PREF_WORKERS_LATEST_JS_VERSION "dom.workers.latestJSVersion"

namespace {

const uint32_t kNoIndex = uint32_t(-1);

const JS::ContextOptions kRequiredJSContextOptions =
  JS::ContextOptions().setDontReportUncaught(true)
                      .setNoScriptRval(true);

uint32_t gMaxWorkersPerDomain = MAX_WORKERS_PER_DOMAIN;

// Does not hold an owning reference.
RuntimeService* gRuntimeService = nullptr;

// Only non-null during the call to Init.
RuntimeService* gRuntimeServiceDuringInit = nullptr;

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
  "Event",
  "MessageEvent",
  "ErrorEvent"

  // XXX Don't care about ProgressEvent since it should never leak to the main
  // thread.
};

static_assert(NS_ARRAY_LENGTH(gStringChars) == ID_COUNT,
              "gStringChars should have the right length.");

class LiteralRebindingCString : public nsDependentCString
{
public:
  template<int N>
  void RebindLiteral(const char (&aStr)[N])
  {
    Rebind(aStr, N-1);
  }
};

template <typename T>
struct PrefTraits;

template <>
struct PrefTraits<bool>
{
  typedef bool PrefValueType;

  static const PrefValueType kDefaultValue = false;

  static inline PrefValueType
  Get(const char* aPref)
  {
    AssertIsOnMainThread();
    return Preferences::GetBool(aPref);
  }

  static inline bool
  Exists(const char* aPref)
  {
    AssertIsOnMainThread();
    return Preferences::GetType(aPref) == nsIPrefBranch::PREF_BOOL;
  }
};

template <>
struct PrefTraits<int32_t>
{
  typedef int32_t PrefValueType;

  static inline PrefValueType
  Get(const char* aPref)
  {
    AssertIsOnMainThread();
    return Preferences::GetInt(aPref);
  }

  static inline bool
  Exists(const char* aPref)
  {
    AssertIsOnMainThread();
    return Preferences::GetType(aPref) == nsIPrefBranch::PREF_INT;
  }
};

template <typename T>
T
GetWorkerPref(const nsACString& aPref,
              const T aDefault = PrefTraits<T>::kDefaultValue)
{
  AssertIsOnMainThread();

  typedef PrefTraits<T> PrefHelper;

  T result;

  nsAutoCString prefName;
  prefName.AssignLiteral(PREF_WORKERS_OPTIONS_PREFIX);
  prefName.Append(aPref);

  if (PrefHelper::Exists(prefName.get())) {
    result = PrefHelper::Get(prefName.get());
  }
  else {
    prefName.AssignLiteral(PREF_JS_OPTIONS_PREFIX);
    prefName.Append(aPref);

    if (PrefHelper::Exists(prefName.get())) {
      result = PrefHelper::Get(prefName.get());
    }
    else {
      result = aDefault;
    }
  }

  return result;
}

void
LoadJSContextOptions(const char* aPrefName, void* /* aClosure */)
{
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();
  if (!rts && !gRuntimeServiceDuringInit) {
    // May be shutting down, just bail.
    return;
  }

  const nsDependentCString prefName(aPrefName);

  // Several other pref branches will get included here so bail out if there is
  // another callback that will handle this change.
  if (StringBeginsWith(prefName,
                       NS_LITERAL_CSTRING(PREF_JS_OPTIONS_PREFIX
                                          PREF_MEM_OPTIONS_PREFIX)) ||
      StringBeginsWith(prefName,
                       NS_LITERAL_CSTRING(PREF_WORKERS_OPTIONS_PREFIX
                                          PREF_MEM_OPTIONS_PREFIX)) ||
      prefName.EqualsLiteral(PREF_JS_OPTIONS_PREFIX PREF_JIT_HARDENING) ||
      prefName.EqualsLiteral(PREF_WORKERS_OPTIONS_PREFIX PREF_JIT_HARDENING)) {
    return;
  }

#ifdef JS_GC_ZEAL
  if (prefName.EqualsLiteral(PREF_JS_OPTIONS_PREFIX PREF_GCZEAL) ||
      prefName.EqualsLiteral(PREF_WORKERS_OPTIONS_PREFIX PREF_GCZEAL)) {
    return;
  }
#endif

  // Common options.
  JS::ContextOptions commonOptions = kRequiredJSContextOptions;
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("strict"))) {
    commonOptions.setExtraWarnings(true);
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("werror"))) {
    commonOptions.setWerror(true);
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("asmjs"))) {
    commonOptions.setAsmJS(true);
  }

  // Content options.
  JS::ContextOptions contentOptions = commonOptions;
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("baselinejit.content"))) {
    contentOptions.setBaseline(true);
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("ion.content"))) {
    contentOptions.setIon(true);
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("typeinference.content"))) {
    contentOptions.setTypeInference(true);
  }

  // Chrome options.
  JS::ContextOptions chromeOptions = commonOptions;
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("baselinejit.chrome"))) {
    chromeOptions.setBaseline(true);
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("ion.chrome"))) {
    chromeOptions.setIon(true);
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("typeinference.chrome"))) {
    chromeOptions.setTypeInference(true);
  }
#ifdef DEBUG
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("strict.debug"))) {
    chromeOptions.setExtraWarnings(true);
  }
#endif

  RuntimeService::SetDefaultJSContextOptions(contentOptions, chromeOptions);

  if (rts) {
    rts->UpdateAllWorkerJSContextOptions();
  }
}

#ifdef JS_GC_ZEAL
void
LoadGCZealOptions(const char* /* aPrefName */, void* /* aClosure */)
{
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();
  if (!rts && !gRuntimeServiceDuringInit) {
    // May be shutting down, just bail.
    return;
  }

  int32_t gczeal = GetWorkerPref<int32_t>(NS_LITERAL_CSTRING(PREF_GCZEAL), -1);
  if (gczeal < 0) {
    gczeal = 0;
  }

  int32_t frequency =
    GetWorkerPref<int32_t>(NS_LITERAL_CSTRING("gcZeal.frequency"), -1);
  if (frequency < 0) {
    frequency = JS_DEFAULT_ZEAL_FREQ;
  }

  RuntimeService::SetDefaultGCZeal(uint8_t(gczeal), uint32_t(frequency));

  if (rts) {
    rts->UpdateAllWorkerGCZeal();
  }
}
#endif

void
UpdateCommonJSGCMemoryOption(RuntimeService* aRuntimeService,
                             const nsACString& aPrefName, JSGCParamKey aKey)
{
  AssertIsOnMainThread();
  NS_ASSERTION(!aPrefName.IsEmpty(), "Empty pref name!");

  int32_t prefValue = GetWorkerPref(aPrefName, -1);
  uint32_t value =
    (prefValue < 0 || prefValue >= 10000) ? 0 : uint32_t(prefValue);

  RuntimeService::SetDefaultJSGCSettings(aKey, value);

  if (aRuntimeService) {
    aRuntimeService->UpdateAllWorkerMemoryParameter(aKey, value);
  }
}

void
UpdatOtherJSGCMemoryOption(RuntimeService* aRuntimeService,
                           JSGCParamKey aKey, uint32_t aValue)
{
  AssertIsOnMainThread();

  RuntimeService::SetDefaultJSGCSettings(aKey, aValue);

  if (aRuntimeService) {
    aRuntimeService->UpdateAllWorkerMemoryParameter(aKey, aValue);
  }
}


void
LoadJSGCMemoryOptions(const char* aPrefName, void* /* aClosure */)
{
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();

  if (!rts && !gRuntimeServiceDuringInit) {
    // May be shutting down, just bail.
    return;
  }

  NS_NAMED_LITERAL_CSTRING(jsPrefix, PREF_JS_OPTIONS_PREFIX);
  NS_NAMED_LITERAL_CSTRING(workersPrefix, PREF_WORKERS_OPTIONS_PREFIX);

  const nsDependentCString fullPrefName(aPrefName);

  // Pull out the string that actually distinguishes the parameter we need to
  // change.
  nsDependentCSubstring memPrefName;
  if (StringBeginsWith(fullPrefName, jsPrefix)) {
    memPrefName.Rebind(fullPrefName, jsPrefix.Length());
  }
  else if (StringBeginsWith(fullPrefName, workersPrefix)) {
    memPrefName.Rebind(fullPrefName, workersPrefix.Length());
  }
  else {
    NS_ERROR("Unknown pref name!");
    return;
  }

#ifdef DEBUG
  // During Init() we get called back with a branch string here, so there should
  // be no just a "mem." pref here.
  if (!rts) {
    NS_ASSERTION(memPrefName.EqualsLiteral(PREF_MEM_OPTIONS_PREFIX), "Huh?!");
  }
#endif

  // If we're running in Init() then do this for every pref we care about.
  // Otherwise we just want to update the parameter that changed.
  for (uint32_t index = rts ? JSSettings::kGCSettingsArraySize - 1 : 0;
       index < JSSettings::kGCSettingsArraySize;
       index++) {
    LiteralRebindingCString matchName;

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "max");
    if (memPrefName == matchName || (!rts && index == 0)) {
      int32_t prefValue = GetWorkerPref(matchName, -1);
      uint32_t value = (prefValue <= 0 || prefValue >= 0x1000) ?
                       uint32_t(-1) :
                       uint32_t(prefValue) * 1024 * 1024;
      UpdatOtherJSGCMemoryOption(rts, JSGC_MAX_BYTES, value);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "high_water_mark");
    if (memPrefName == matchName || (!rts && index == 1)) {
      int32_t prefValue = GetWorkerPref(matchName, 128);
      UpdatOtherJSGCMemoryOption(rts, JSGC_MAX_MALLOC_BYTES,
                                 uint32_t(prefValue) * 1024 * 1024);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX 
                            "gc_high_frequency_time_limit_ms");
    if (memPrefName == matchName || (!rts && index == 2)) {
      UpdateCommonJSGCMemoryOption(rts, matchName,
                                   JSGC_HIGH_FREQUENCY_TIME_LIMIT);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_low_frequency_heap_growth");
    if (memPrefName == matchName || (!rts && index == 3)) {
      UpdateCommonJSGCMemoryOption(rts, matchName,
                                   JSGC_LOW_FREQUENCY_HEAP_GROWTH);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_high_frequency_heap_growth_min");
    if (memPrefName == matchName || (!rts && index == 4)) {
      UpdateCommonJSGCMemoryOption(rts, matchName,
                                   JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_high_frequency_heap_growth_max");
    if (memPrefName == matchName || (!rts && index == 5)) {
      UpdateCommonJSGCMemoryOption(rts, matchName,
                                   JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_high_frequency_low_limit_mb");
    if (memPrefName == matchName || (!rts && index == 6)) {
      UpdateCommonJSGCMemoryOption(rts, matchName,
                                   JSGC_HIGH_FREQUENCY_LOW_LIMIT);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_high_frequency_high_limit_mb");
    if (memPrefName == matchName || (!rts && index == 7)) {
      UpdateCommonJSGCMemoryOption(rts, matchName,
                                   JSGC_HIGH_FREQUENCY_HIGH_LIMIT);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_allocation_threshold_mb");
    if (memPrefName == matchName || (!rts && index == 8)) {
      UpdateCommonJSGCMemoryOption(rts, matchName, JSGC_ALLOCATION_THRESHOLD);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "gc_incremental_slice_ms");
    if (memPrefName == matchName || (!rts && index == 9)) {
      int32_t prefValue = GetWorkerPref(matchName, -1);
      uint32_t value =
        (prefValue <= 0 || prefValue >= 100000) ? 0 : uint32_t(prefValue);
      UpdatOtherJSGCMemoryOption(rts, JSGC_SLICE_TIME_BUDGET, value);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "gc_dynamic_heap_growth");
    if (memPrefName == matchName || (!rts && index == 10)) {
      bool prefValue = GetWorkerPref(matchName, false);
      UpdatOtherJSGCMemoryOption(rts, JSGC_DYNAMIC_HEAP_GROWTH,
                                 prefValue ? 0 : 1);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "gc_dynamic_mark_slice");
    if (memPrefName == matchName || (!rts && index == 11)) {
      bool prefValue = GetWorkerPref(matchName, false);
      UpdatOtherJSGCMemoryOption(rts, JSGC_DYNAMIC_MARK_SLICE,
                                 prefValue ? 0 : 1);
      continue;
    }

#ifdef DEBUG
    nsAutoCString message("Workers don't support the 'mem.");
    message.Append(memPrefName);
    message.AppendLiteral("' preference!");
    NS_WARNING(message.get());
#endif
  }
}

void
LoadJITHardeningOption(const char* /* aPrefName */, void* /* aClosure */)
{
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();

  if (!rts && !gRuntimeServiceDuringInit) {
    // May be shutting down, just bail.
    return;
  }

  bool value = GetWorkerPref(NS_LITERAL_CSTRING(PREF_JIT_HARDENING), false);

  RuntimeService::SetDefaultJITHardening(value);

  if (rts) {
    rts->UpdateAllWorkerJITHardening(value);
  }
}

void
ErrorReporter(JSContext* aCx, const char* aMessage, JSErrorReport* aReport)
{
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  return worker->ReportError(aCx, aMessage, aReport);
}

bool
OperationCallback(JSContext* aCx)
{
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);

  // Now is a good time to turn on profiling if it's pending.
  profiler_js_operation_callback();

  return worker->OperationCallback(aCx);
}

class LogViolationDetailsRunnable : public nsRunnable
{
  WorkerPrivate* mWorkerPrivate;
  nsString mFileName;
  uint32_t mLineNum;
  uint32_t mSyncQueueKey;

private:
  class LogViolationDetailsResponseRunnable : public WorkerSyncRunnable
  {
    uint32_t mSyncQueueKey;

  public:
    LogViolationDetailsResponseRunnable(WorkerPrivate* aWorkerPrivate,
                                        uint32_t aSyncQueueKey)
    : WorkerSyncRunnable(aWorkerPrivate, aSyncQueueKey, false),
      mSyncQueueKey(aSyncQueueKey)
    {
      NS_ASSERTION(aWorkerPrivate, "Don't hand me a null WorkerPrivate!");
    }

    bool
    WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    {
      aWorkerPrivate->StopSyncLoop(mSyncQueueKey, true);
      return true;
    }

    bool
    PreDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    {
      AssertIsOnMainThread();
      return true;
    }

    void
    PostDispatch(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                 bool aDispatchResult)
    {
      AssertIsOnMainThread();
    }
  };

public:
  LogViolationDetailsRunnable(WorkerPrivate* aWorker,
                              const nsString& aFileName,
                              uint32_t aLineNum)
  : mWorkerPrivate(aWorker),
    mFileName(aFileName),
    mLineNum(aLineNum),
    mSyncQueueKey(0)
  {
    NS_ASSERTION(aWorker, "WorkerPrivate cannot be null");
  }

  bool
  Dispatch(JSContext* aCx)
  {
    AutoSyncLoopHolder syncLoop(mWorkerPrivate);
    mSyncQueueKey = syncLoop.SyncQueueKey();

    if (NS_FAILED(NS_DispatchToMainThread(this, NS_DISPATCH_NORMAL))) {
      JS_ReportError(aCx, "Failed to dispatch to main thread!");
      return false;
    }

    return syncLoop.RunAndForget(aCx);
  }

  NS_IMETHOD
  Run()
  {
    AssertIsOnMainThread();

    nsIContentSecurityPolicy* csp = mWorkerPrivate->GetCSP();
    if (csp) {
      NS_NAMED_LITERAL_STRING(scriptSample,
         "Call to eval() or related function blocked by CSP.");
      if (mWorkerPrivate->GetReportCSPViolations()) {
        csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                                 mFileName, scriptSample, mLineNum, EmptyString());
      }
    }

    nsRefPtr<LogViolationDetailsResponseRunnable> response =
        new LogViolationDetailsResponseRunnable(mWorkerPrivate, mSyncQueueKey);
    if (!response->Dispatch(nullptr)) {
      NS_WARNING("Failed to dispatch response!");
    }

    return NS_OK;
  }
};

bool
ContentSecurityPolicyAllows(JSContext* aCx)
{
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  worker->AssertIsOnWorkerThread();

  if (worker->GetReportCSPViolations()) {
    nsString fileName;
    uint32_t lineNum = 0;

    JS::Rooted<JSScript*> script(aCx);
    const char* file;
    if (JS_DescribeScriptedCaller(aCx, &script, &lineNum) &&
        (file = JS_GetScriptFilename(aCx, script))) {
      fileName.AssignASCII(file);
    } else {
      JS_ReportPendingException(aCx);
    }

    nsRefPtr<LogViolationDetailsRunnable> runnable =
        new LogViolationDetailsRunnable(worker, fileName, lineNum);

    if (!runnable->Dispatch(aCx)) {
      JS_ReportPendingException(aCx);
    }
  }

  return worker->IsEvalAllowed();
}

void
CTypesActivityCallback(JSContext* aCx,
                       js::CTypesActivityType aType)
{
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  worker->AssertIsOnWorkerThread();

  switch (aType) {
    case js::CTYPES_CALL_BEGIN:
      worker->BeginCTypesCall();
      break;

    case js::CTYPES_CALL_END:
      worker->EndCTypesCall();
      break;

    case js::CTYPES_CALLBACK_BEGIN:
      worker->BeginCTypesCallback();
      break;

    case js::CTYPES_CALLBACK_END:
      worker->EndCTypesCallback();
      break;

    default:
      MOZ_CRASH("Unknown type flag!");
  }
}

static nsIPrincipal*
GetPrincipalForAsmJSCacheOp()
{
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  if (!workerPrivate) {
    return nullptr;
  }

  // asmjscache::OpenEntryForX guarnatee to only access the given nsIPrincipal
  // from the main thread.
  return workerPrivate->GetPrincipalDontAssertMainThread();
}

static bool
AsmJSCacheOpenEntryForRead(JS::Handle<JSObject*> aGlobal,
                           const jschar* aBegin,
                           const jschar* aLimit,
                           size_t* aSize,
                           const uint8_t** aMemory,
                           intptr_t *aHandle)
{
  nsIPrincipal* principal = GetPrincipalForAsmJSCacheOp();
  if (!principal) {
    return false;
  }

  return asmjscache::OpenEntryForRead(principal, aBegin, aLimit, aSize, aMemory,
                                      aHandle);
}

static bool
AsmJSCacheOpenEntryForWrite(JS::Handle<JSObject*> aGlobal,
                            const jschar* aBegin,
                            const jschar* aEnd,
                            size_t aSize,
                            uint8_t** aMemory,
                            intptr_t* aHandle)
{
  nsIPrincipal* principal = GetPrincipalForAsmJSCacheOp();
  if (!principal) {
    return false;
  }

  return asmjscache::OpenEntryForWrite(principal, aBegin, aEnd, aSize, aMemory,
                                       aHandle);
}

struct WorkerThreadRuntimePrivate : public PerThreadAtomCache
{
  WorkerPrivate* mWorkerPrivate;
};

JSContext*
CreateJSContextForWorker(WorkerPrivate* aWorkerPrivate, JSRuntime* aRuntime)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(!aWorkerPrivate->GetJSContext(), "Already has a context!");

  JSSettings settings;
  aWorkerPrivate->CopyJSSettings(settings);

  JSSettings::JSGCSettingsArray& gcSettings = settings.gcSettings;

  // This is the real place where we set the max memory for the runtime.
  for (uint32_t index = 0; index < ArrayLength(gcSettings); index++) {
    const JSSettings::JSGCSetting& setting = gcSettings[index];
    if (setting.IsSet()) {
      NS_ASSERTION(setting.value, "Can't handle 0 values!");
      JS_SetGCParameter(aRuntime, setting.key, setting.value);
    }
  }

  JS_SetIsWorkerRuntime(aRuntime);

  JS_SetNativeStackQuota(aRuntime, WORKER_CONTEXT_NATIVE_STACK_LIMIT);

  // Security policy:
  static JSSecurityCallbacks securityCallbacks = {
    nullptr,
    ContentSecurityPolicyAllows
  };
  JS_SetSecurityCallbacks(aRuntime, &securityCallbacks);

  // DOM helpers:
  static js::DOMCallbacks DOMCallbacks = {
    InstanceClassHasProtoAtDepth
  };
  SetDOMCallbacks(aRuntime, &DOMCallbacks);

  // Set up the asm.js cache callbacks
  static JS::AsmJSCacheOps asmJSCacheOps = {
    AsmJSCacheOpenEntryForRead,
    asmjscache::CloseEntryForRead,
    AsmJSCacheOpenEntryForWrite,
    asmjscache::CloseEntryForWrite,
    asmjscache::GetBuildId
  };
  JS::SetAsmJSCacheOps(aRuntime, &asmJSCacheOps);

  JSContext* workerCx = JS_NewContext(aRuntime, 0);
  if (!workerCx) {
    NS_WARNING("Could not create new context!");
    return nullptr;
  }

  auto rtPrivate = new WorkerThreadRuntimePrivate();
  memset(rtPrivate, 0, sizeof(WorkerThreadRuntimePrivate));
  rtPrivate->mWorkerPrivate = aWorkerPrivate;
  JS_SetRuntimePrivate(aRuntime, rtPrivate);

  JS_SetErrorReporter(workerCx, ErrorReporter);

  JS_SetOperationCallback(aRuntime, OperationCallback);

  js::SetCTypesActivityCallback(aRuntime, CTypesActivityCallback);

  JS::ContextOptionsRef(workerCx) =
    aWorkerPrivate->IsChromeWorker() ? settings.chrome.contextOptions
                                     : settings.content.contextOptions;

  JS_SetJitHardening(aRuntime, settings.jitHardening);

#ifdef JS_GC_ZEAL
  JS_SetGCZeal(workerCx, settings.gcZeal, settings.gcZealFrequency);
#endif

  return workerCx;
}

class WorkerJSRuntime : public mozilla::CycleCollectedJSRuntime
{
public:
  // The heap size passed here doesn't matter, we will change it later in the
  // call to JS_SetGCParameter inside CreateJSContextForWorker.
  WorkerJSRuntime(WorkerPrivate* aWorkerPrivate)
  : CycleCollectedJSRuntime(WORKER_DEFAULT_RUNTIME_HEAPSIZE,
                            JS_NO_HELPER_THREADS),
    mWorkerPrivate(aWorkerPrivate)
  { }

  ~WorkerJSRuntime()
  {
    auto rtPrivate = static_cast<WorkerThreadRuntimePrivate*>(JS_GetRuntimePrivate(Runtime()));
    delete rtPrivate;
    JS_SetRuntimePrivate(Runtime(), nullptr);

    // The worker global should be unrooted and the shutdown cycle collection
    // should break all remaining cycles. The superclass destructor will run
    // the GC one final time and finalize any JSObjects that were participating
    // in cycles that were broken during CC shutdown.
    nsCycleCollector_shutdown();

    // The CC is shut down, and the superclass destructor will GC, so make sure
    // we don't try to CC again.
    mWorkerPrivate = nullptr;
  }

  virtual void
  PrepareForForgetSkippable() MOZ_OVERRIDE
  {
  }

  virtual void
  BeginCycleCollectionCallback() MOZ_OVERRIDE
  {
  }

  virtual void
  EndCycleCollectionCallback(CycleCollectorResults &aResults) MOZ_OVERRIDE
  {
  }

  void
  DispatchDeferredDeletion(bool aContinuation) MOZ_OVERRIDE
  {
    MOZ_ASSERT(!aContinuation);

    // Do it immediately, no need for asynchronous behavior here.
    nsCycleCollector_doDeferredDeletion();
  }

  virtual void CustomGCCallback(JSGCStatus aStatus) MOZ_OVERRIDE
  {
    if (!mWorkerPrivate) {
      // We're shutting down, no need to do anything.
      return;
    }

    mWorkerPrivate->AssertIsOnWorkerThread();

    if (aStatus == JSGC_END) {
      nsCycleCollector_collect(nullptr);
    }
  }

private:
  WorkerPrivate* mWorkerPrivate;
};

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
#ifdef MOZ_NUWA_PROCESS
    if (IsNuwaProcess()) {
      NS_ASSERTION(NuwaMarkCurrentThread != nullptr,
                   "NuwaMarkCurrentThread is undefined!");
      NuwaMarkCurrentThread(nullptr, nullptr);
      NuwaFreezeCurrentThread();
    }
#endif
    WorkerPrivate* workerPrivate = mWorkerPrivate;
    mWorkerPrivate = nullptr;

    workerPrivate->AssertIsOnWorkerThread();

    {
      nsCycleCollector_startup();

      WorkerJSRuntime runtime(workerPrivate);
      JSRuntime* rt = runtime.Runtime();
      JSContext* cx = CreateJSContextForWorker(workerPrivate, rt);
      if (!cx) {
        // XXX need to fire an error at parent.
        NS_ERROR("Failed to create runtime and context!");
        return NS_ERROR_FAILURE;
      }

      char aLocal;
      profiler_register_thread("WebWorker", &aLocal);
  #ifdef MOZ_ENABLE_PROFILER_SPS
      if (PseudoStack* stack = mozilla_get_pseudo_stack())
        stack->sampleRuntime(rt);
  #endif

      {
        JSAutoRequest ar(cx);
        workerPrivate->DoRunLoop(cx);
      }

      // Destroy the main context.  This will unroot the main worker global and
      // GC.  This is not the last JSContext (WorkerJSRuntime maintains an
      // internal JSContext).
      JS_DestroyContext(cx);

      // Now WorkerJSRuntime goes out of scope and its destructor will shut
      // down the cycle collector and destroy the final JSContext.  This
      // breaks any remaining cycles and collects the C++ and JS objects
      // participating.
    }

#ifdef MOZ_ENABLE_PROFILER_SPS
    if (PseudoStack* stack = mozilla_get_pseudo_stack())
      stack->sampleRuntime(nullptr);
#endif

    workerPrivate->ScheduleDeletion(false);
    profiler_unregister_thread();
    return NS_OK;
  }
};

} /* anonymous namespace */

BEGIN_WORKERS_NAMESPACE

// Entry point for main thread non-window globals.
bool
ResolveWorkerClasses(JSContext* aCx, JS::Handle<JSObject*> aObj, JS::Handle<jsid> aId,
                     unsigned aFlags, JS::MutableHandle<JSObject*> aObjp)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(nsContentUtils::IsCallerChrome());

  // Make sure our strings are interned.
  if (JSID_IS_VOID(gStringIDs[0])) {
    for (uint32_t i = 0; i < ID_COUNT; i++) {
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

  bool shouldResolve = false;

  for (uint32_t i = 0; i < ID_COUNT; i++) {
    if (gStringIDs[i] == aId) {
      shouldResolve = true;
      break;
    }
  }

  if (!shouldResolve) {
    aObjp.set(nullptr);
    return true;
  }

  if (!WorkerBinding::GetConstructorObject(aCx, aObj) ||
      !ChromeWorkerBinding::GetConstructorObject(aCx, aObj) ||
      !ErrorEventBinding::GetConstructorObject(aCx, aObj) ||
      !MessageEventBinding::GetConstructorObject(aCx, aObj)) {
    return false;
  }

  aObjp.set(aObj);
  return true;
}

void
CancelWorkersForWindow(nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->CancelWorkersForWindow(aWindow);
  }
}

void
SuspendWorkersForWindow(nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->SuspendWorkersForWindow(aWindow);
  }
}

void
ResumeWorkersForWindow(nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->ResumeWorkersForWindow(aWindow);
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
  runnable->Dispatch(nullptr);
  return true;
}

WorkerPrivate*
GetWorkerPrivateFromContext(JSContext* aCx)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  return static_cast<WorkerThreadRuntimePrivate*>(JS_GetRuntimePrivate(JS_GetRuntime(aCx)))->mWorkerPrivate;
}

WorkerPrivate*
GetCurrentThreadWorkerPrivate()
{
  MOZ_ASSERT(!NS_IsMainThread(), "Wrong thread!");
  CycleCollectedJSRuntime* ccrt = CycleCollectedJSRuntime::Get();
  if (!ccrt) {
    return nullptr;
  }

  JSRuntime* rt = ccrt->Runtime();
  return static_cast<WorkerThreadRuntimePrivate*>(JS_GetRuntimePrivate(rt))->
    mWorkerPrivate;
}

bool
IsCurrentThreadRunningChromeWorker()
{
  return GetCurrentThreadWorkerPrivate()->UsesSystemPrincipal();
}

JSContext*
GetCurrentThreadJSContext()
{
  return GetCurrentThreadWorkerPrivate()->GetJSContext();
}

END_WORKERS_NAMESPACE

// This is only touched on the main thread. Initialized in Init() below.
JSSettings RuntimeService::sDefaultJSSettings;
bool RuntimeService::sDefaultPreferences[WORKERPREF_COUNT] = { false };

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

  gRuntimeService = nullptr;
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
      return nullptr;
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

  bool isSharedWorker = aWorkerPrivate->IsSharedWorker();

  const nsString& sharedWorkerName = aWorkerPrivate->SharedWorkerName();
  nsCString sharedWorkerScriptSpec;

  if (isSharedWorker) {
    AssertIsOnMainThread();

    nsCOMPtr<nsIURI> scriptURI = aWorkerPrivate->GetResolvedScriptURI();
    NS_ASSERTION(scriptURI, "Null script URI!");

    nsresult rv = scriptURI->GetSpec(sharedWorkerScriptSpec);
    if (NS_FAILED(rv)) {
      NS_WARNING("GetSpec failed?!");
      xpc::Throw(aCx, rv);
      return false;
    }

    NS_ASSERTION(!sharedWorkerScriptSpec.IsEmpty(), "Empty spec!");
  }

  const nsCString& domain = aWorkerPrivate->Domain();

  WorkerDomainInfo* domainInfo;
  bool queued = false;
  {
    MutexAutoLock lock(mMutex);

    if (!mDomainMap.Get(domain, &domainInfo)) {
      NS_ASSERTION(!parent, "Shouldn't have a parent here!");

      domainInfo = new WorkerDomainInfo();
      domainInfo->mDomain = domain;
      mDomainMap.Put(domain, domainInfo);
    }

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

    if (isSharedWorker) {
      MOZ_ASSERT(!domainInfo->mSharedWorkerInfos.Get(sharedWorkerScriptSpec));

      SharedWorkerInfo* sharedWorkerInfo =
        new SharedWorkerInfo(aWorkerPrivate, sharedWorkerScriptSpec,
                             sharedWorkerName);
      domainInfo->mSharedWorkerInfos.Put(sharedWorkerScriptSpec,
                                         sharedWorkerInfo);
    }
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
      NS_GetNavigatorAppName(mNavigatorStrings.mAppName);
      if (NS_FAILED(NS_GetNavigatorAppVersion(mNavigatorStrings.mAppVersion)) ||
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
      windowArray = new nsTArray<WorkerPrivate*>(1);
      mWindowMap.Put(window, windowArray);
    }

    if (!windowArray->Contains(aWorkerPrivate)) {
      windowArray->AppendElement(aWorkerPrivate);
    } else {
      MOZ_ASSERT(aWorkerPrivate->IsSharedWorker());
    }
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

  const nsCString& domain = aWorkerPrivate->Domain();

  WorkerPrivate* queuedWorker = nullptr;
  {
    MutexAutoLock lock(mMutex);

    WorkerDomainInfo* domainInfo;
    if (!mDomainMap.Get(domain, &domainInfo)) {
      NS_ERROR("Don't have an entry for this domain!");
    }

    // Remove old worker from everywhere.
    uint32_t index = domainInfo->mQueuedWorkers.IndexOf(aWorkerPrivate);
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

    if (aWorkerPrivate->IsSharedWorker()) {
      MatchSharedWorkerInfo match(aWorkerPrivate);
      domainInfo->mSharedWorkerInfos.EnumerateRead(FindSharedWorkerInfo,
                                                   &match);

      if (match.mSharedWorkerInfo) {
        domainInfo->mSharedWorkerInfos.Remove(
          match.mSharedWorkerInfo->mScriptSpec);
      }
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

  if (aWorkerPrivate->IsSharedWorker()) {
    AssertIsOnMainThread();

    nsAutoTArray<nsRefPtr<SharedWorker>, 5> sharedWorkersToNotify;
    aWorkerPrivate->GetAllSharedWorkers(sharedWorkersToNotify);

    for (uint32_t index = 0; index < sharedWorkersToNotify.Length(); index++) {
      MOZ_ASSERT(sharedWorkersToNotify[index]);
      sharedWorkersToNotify[index]->NoteDeadWorker(aCx);
    }
  }

  if (parent) {
    parent->RemoveChildWorker(aCx, aWorkerPrivate);
  }
  else if (aWorkerPrivate->IsSharedWorker()) {
    mWindowMap.Enumerate(RemoveSharedWorkerFromWindowMap, aWorkerPrivate);
  }
  else {
    // May be null.
    nsPIDOMWindow* window = aWorkerPrivate->GetWindow();

    nsTArray<WorkerPrivate*>* windowArray;
    if (!mWindowMap.Get(window, &windowArray)) {
      MOZ_ASSERT(false, "Don't have an entry for this window!");
    }

    if (!windowArray->RemoveElement(aWorkerPrivate)) {
      MOZ_ASSERT(false, "Worker wasn't in the correct window array!");
    }

    if (windowArray->IsEmpty()) {
      MOZ_ASSERT(!queuedWorker, "queuedWorker should be in this array!");
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
      uint32_t index = mIdleThreadArray.Length() - 1;
      mIdleThreadArray[index].mThread.swap(thread);
      mIdleThreadArray.RemoveElementAt(index);
    }
  }

  if (!thread) {
    if (NS_FAILED(NS_NewNamedThread("DOM Worker",
                                    getter_AddRefs(thread), nullptr,
                                    WORKER_STACK_SIZE))) {
      UnregisterWorker(aCx, aWorkerPrivate);
      JS_ReportError(aCx, "Could not create new thread!");
      return false;
    }
  }

  int32_t priority = aWorkerPrivate->IsChromeWorker() ?
                     nsISupportsPriority::PRIORITY_NORMAL :
                     nsISupportsPriority::PRIORITY_LOW;

  nsCOMPtr<nsISupportsPriority> threadPriority = do_QueryInterface(thread);
  if (!threadPriority || NS_FAILED(threadPriority->SetPriority(priority))) {
    NS_WARNING("Could not set the thread's priority!");
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

    for (uint32_t index = 0; index < runtime->mIdleThreadArray.Length();
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

  for (uint32_t index = 0; index < expiredThreads.Length(); index++) {
    if (NS_FAILED(expiredThreads[index]->Shutdown())) {
      NS_WARNING("Failed to shutdown thread!");
    }
  }

  if (!nextExpiration.IsNull()) {
    TimeDuration delta = nextExpiration - TimeStamp::Now();
    uint32_t delay(delta > TimeDuration(0) ? delta.ToMilliseconds() : 0);

    // Reschedule the timer.
    if (NS_FAILED(aTimer->InitWithFuncCallback(ShutdownIdleThreads, nullptr,
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

  nsLayoutStatics::AddRef();

  // Initialize JSSettings.
  if (!sDefaultJSSettings.gcSettings[0].IsSet()) {
    sDefaultJSSettings.chrome.contextOptions = kRequiredJSContextOptions;
    sDefaultJSSettings.chrome.maxScriptRuntime = -1;
    sDefaultJSSettings.chrome.compartmentOptions.setVersion(JSVERSION_LATEST);
    sDefaultJSSettings.content.contextOptions = kRequiredJSContextOptions;
    sDefaultJSSettings.content.maxScriptRuntime = MAX_SCRIPT_RUN_TIME_SEC;
#ifdef JS_GC_ZEAL
    sDefaultJSSettings.gcZealFrequency = JS_DEFAULT_ZEAL_FREQ;
    sDefaultJSSettings.gcZeal = 0;
#endif
    SetDefaultJSGCSettings(JSGC_MAX_BYTES, WORKER_DEFAULT_RUNTIME_HEAPSIZE);
    SetDefaultJSGCSettings(JSGC_ALLOCATION_THRESHOLD,
                           WORKER_DEFAULT_ALLOCATION_THRESHOLD);
  }

// If dump is not controlled by pref, it's set to true.
#ifndef DUMP_CONTROLLED_BY_PREF
  sDefaultPreferences[WORKERPREF_DUMP] = true;
#endif

  mIdleThreadTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
  NS_ENSURE_STATE(mIdleThreadTimer);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_FAILURE);

  nsresult rv =
    obs->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  NS_ENSURE_SUCCESS(rv, rv);

  mObserved = true;

  if (NS_FAILED(obs->AddObserver(this, GC_REQUEST_OBSERVER_TOPIC, false))) {
    NS_WARNING("Failed to register for GC request notifications!");
  }

  if (NS_FAILED(obs->AddObserver(this, CC_REQUEST_OBSERVER_TOPIC, false))) {
    NS_WARNING("Failed to register for CC request notifications!");
  }

  if (NS_FAILED(obs->AddObserver(this, MEMORY_PRESSURE_OBSERVER_TOPIC,
                                 false))) {
    NS_WARNING("Failed to register for memory pressure notifications!");
  }

  NS_ASSERTION(!gRuntimeServiceDuringInit, "This should be null!");
  gRuntimeServiceDuringInit = this;

  if (NS_FAILED(Preferences::RegisterCallback(
                                 LoadJSGCMemoryOptions,
                                 PREF_JS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX,
                                 nullptr)) ||
      NS_FAILED(Preferences::RegisterCallbackAndCall(
                            LoadJSGCMemoryOptions,
                            PREF_WORKERS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX,
                            nullptr)) ||
      NS_FAILED(Preferences::RegisterCallback(
                                      LoadJITHardeningOption,
                                      PREF_JS_OPTIONS_PREFIX PREF_JIT_HARDENING,
                                      nullptr)) ||
      NS_FAILED(Preferences::RegisterCallbackAndCall(
                                 LoadJITHardeningOption,
                                 PREF_WORKERS_OPTIONS_PREFIX PREF_JIT_HARDENING,
                                 nullptr)) ||
#ifdef JS_GC_ZEAL
      NS_FAILED(Preferences::RegisterCallback(
                                             LoadGCZealOptions,
                                             PREF_JS_OPTIONS_PREFIX PREF_GCZEAL,
                                             nullptr)) ||
      NS_FAILED(Preferences::RegisterCallbackAndCall(
                                        LoadGCZealOptions,
                                        PREF_WORKERS_OPTIONS_PREFIX PREF_GCZEAL,
                                        nullptr)) ||
#endif
#if DUMP_CONTROLLED_BY_PREF
      NS_FAILED(Preferences::RegisterCallbackAndCall(
                                  WorkerPrefChanged,
                                  PREF_DOM_WINDOW_DUMP_ENABLED,
                                  reinterpret_cast<void *>(WORKERPREF_DUMP))) ||
#endif
      NS_FAILED(Preferences::RegisterCallbackAndCall(
                               WorkerPrefChanged,
                               PREF_PROMISE_ENABLED,
                               reinterpret_cast<void *>(WORKERPREF_PROMISE))) ||
      NS_FAILED(Preferences::RegisterCallback(LoadJSContextOptions,
                                              PREF_JS_OPTIONS_PREFIX,
                                              nullptr)) ||
      NS_FAILED(Preferences::RegisterCallbackAndCall(
                                                    LoadJSContextOptions,
                                                    PREF_WORKERS_OPTIONS_PREFIX,
                                                    nullptr)) ||
      NS_FAILED(Preferences::RegisterCallbackAndCall(
                                                 JSVersionChanged,
                                                 PREF_WORKERS_LATEST_JS_VERSION,
                                                 nullptr))) {
    NS_WARNING("Failed to register pref callbacks!");
  }

  NS_ASSERTION(gRuntimeServiceDuringInit == this, "Should be 'this'!");
  gRuntimeServiceDuringInit = nullptr;

  // We assume atomic 32bit reads/writes. If this assumption doesn't hold on
  // some wacky platform then the worst that could happen is that the close
  // handler will run for a slightly different amount of time.
  if (NS_FAILED(Preferences::AddIntVarCache(
                                   &sDefaultJSSettings.content.maxScriptRuntime,
                                   PREF_MAX_SCRIPT_RUN_TIME_CONTENT,
                                   MAX_SCRIPT_RUN_TIME_SEC)) ||
      NS_FAILED(Preferences::AddIntVarCache(
                                    &sDefaultJSSettings.chrome.maxScriptRuntime,
                                    PREF_MAX_SCRIPT_RUN_TIME_CHROME, -1))) {
    NS_WARNING("Failed to register timeout cache!");
  }

  int32_t maxPerDomain = Preferences::GetInt(PREF_WORKERS_MAX_PER_DOMAIN,
                                             MAX_WORKERS_PER_DOMAIN);
  gMaxWorkersPerDomain = std::max(0, maxPerDomain);

  mDetectorName = Preferences::GetLocalizedCString("intl.charset.detector");

  nsCOMPtr<nsIPlatformCharset> platformCharset =
    do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = platformCharset->GetCharset(kPlatformCharsetSel_PlainTextInFile,
                                     mSystemCharset);
  }

  rv = InitOSFileConstants();
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

void
RuntimeService::Shutdown()
{
  AssertIsOnMainThread();

  MOZ_ASSERT(!mShuttingDown);
  // That's it, no more workers.
  mShuttingDown = true;

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_WARN_IF_FALSE(obs, "Failed to get observer service?!");

  // Tell anyone that cares that they're about to lose worker support.
  if (obs && NS_FAILED(obs->NotifyObservers(nullptr, WORKERS_SHUTDOWN_TOPIC,
                                            nullptr))) {
    NS_WARNING("NotifyObservers failed!");
  }

  {
    MutexAutoLock lock(mMutex);

    nsAutoTArray<WorkerPrivate*, 100> workers;
    mDomainMap.EnumerateRead(AddAllTopLevelWorkersToArray, &workers);

    if (!workers.IsEmpty()) {

      // Cancel all top-level workers.
      {
        MutexAutoUnlock unlock(mMutex);

        AutoSafeJSContext cx;
        JSAutoRequest ar(cx);

        for (uint32_t index = 0; index < workers.Length(); index++) {
          if (!workers[index]->Kill(cx)) {
            NS_WARNING("Failed to cancel worker!");
          }
        }
      }
    }
  }
}

// This spins the event loop until all workers are finished and their threads
// have been joined.
void
RuntimeService::Cleanup()
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_WARN_IF_FALSE(obs, "Failed to get observer service?!");

  if (mIdleThreadTimer) {
    if (NS_FAILED(mIdleThreadTimer->Cancel())) {
      NS_WARNING("Failed to cancel idle timer!");
    }
    mIdleThreadTimer = nullptr;
  }

  {
    MutexAutoLock lock(mMutex);

    nsAutoTArray<WorkerPrivate*, 100> workers;
    mDomainMap.EnumerateRead(AddAllTopLevelWorkersToArray, &workers);

    if (!workers.IsEmpty()) {
      nsIThread* currentThread = NS_GetCurrentThread();
      NS_ASSERTION(currentThread, "This should never be null!");

      // Shut down any idle threads.
      if (!mIdleThreadArray.IsEmpty()) {
        nsAutoTArray<nsCOMPtr<nsIThread>, 20> idleThreads;

        uint32_t idleThreadCount = mIdleThreadArray.Length();
        idleThreads.SetLength(idleThreadCount);

        for (uint32_t index = 0; index < idleThreadCount; index++) {
          NS_ASSERTION(mIdleThreadArray[index].mThread, "Null thread!");
          idleThreads[index].swap(mIdleThreadArray[index].mThread);
        }

        mIdleThreadArray.Clear();

        MutexAutoUnlock unlock(mMutex);

        for (uint32_t index = 0; index < idleThreadCount; index++) {
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

  NS_ASSERTION(!mWindowMap.Count(), "All windows should have been released!");

  if (mObserved) {
    if (NS_FAILED(Preferences::UnregisterCallback(JSVersionChanged,
                                                  PREF_WORKERS_LATEST_JS_VERSION,
                                                  nullptr)) ||
        NS_FAILED(Preferences::UnregisterCallback(LoadJSContextOptions,
                                                  PREF_JS_OPTIONS_PREFIX,
                                                  nullptr)) ||
        NS_FAILED(Preferences::UnregisterCallback(LoadJSContextOptions,
                                                  PREF_WORKERS_OPTIONS_PREFIX,
                                                  nullptr)) ||
        NS_FAILED(Preferences::UnregisterCallback(
                               WorkerPrefChanged,
                               PREF_PROMISE_ENABLED,
                               reinterpret_cast<void *>(WORKERPREF_PROMISE))) ||
#if DUMP_CONTROLLED_BY_PREF
        NS_FAILED(Preferences::UnregisterCallback(
                                  WorkerPrefChanged,
                                  PREF_DOM_WINDOW_DUMP_ENABLED,
                                  reinterpret_cast<void *>(WORKERPREF_DUMP))) ||
#endif
#ifdef JS_GC_ZEAL
        NS_FAILED(Preferences::UnregisterCallback(
                                             LoadGCZealOptions,
                                             PREF_JS_OPTIONS_PREFIX PREF_GCZEAL,
                                             nullptr)) ||
        NS_FAILED(Preferences::UnregisterCallback(
                                        LoadGCZealOptions,
                                        PREF_WORKERS_OPTIONS_PREFIX PREF_GCZEAL,
                                        nullptr)) ||
#endif
        NS_FAILED(Preferences::UnregisterCallback(
                                 LoadJSGCMemoryOptions,
                                 PREF_JS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX,
                                 nullptr)) ||
        NS_FAILED(Preferences::UnregisterCallback(
                            LoadJSGCMemoryOptions,
                            PREF_WORKERS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX,
                            nullptr)) ||
        NS_FAILED(Preferences::UnregisterCallback(
                                      LoadJITHardeningOption,
                                      PREF_JS_OPTIONS_PREFIX PREF_JIT_HARDENING,
                                      nullptr)) ||
        NS_FAILED(Preferences::UnregisterCallback(
                                 LoadJITHardeningOption,
                                 PREF_WORKERS_OPTIONS_PREFIX PREF_JIT_HARDENING,
                                 nullptr))) {
      NS_WARNING("Failed to unregister pref callbacks!");
    }

    if (obs) {
      if (NS_FAILED(obs->RemoveObserver(this, GC_REQUEST_OBSERVER_TOPIC))) {
        NS_WARNING("Failed to unregister for GC request notifications!");
      }

      if (NS_FAILED(obs->RemoveObserver(this, CC_REQUEST_OBSERVER_TOPIC))) {
        NS_WARNING("Failed to unregister for CC request notifications!");
      }

      if (NS_FAILED(obs->RemoveObserver(this,
                                        MEMORY_PRESSURE_OBSERVER_TOPIC))) {
        NS_WARNING("Failed to unregister for memory pressure notifications!");
      }

      obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID);
      obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
      mObserved = false;
    }
  }

  CleanupOSFileConstants();
  nsLayoutStatics::Release();
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
  for (uint32_t index = 0; index < aData->mActiveWorkers.Length(); index++) {
    NS_ASSERTION(!aData->mActiveWorkers[index]->GetParent(),
                 "Shouldn't have a parent in this list!");
  }
#endif

  array->AppendElements(aData->mActiveWorkers);

  // These might not be top-level workers...
  for (uint32_t index = 0; index < aData->mQueuedWorkers.Length(); index++) {
    WorkerPrivate* worker = aData->mQueuedWorkers[index];
    if (!worker->GetParent()) {
      array->AppendElement(worker);
    }
  }

  return PL_DHASH_NEXT;
}

// static
PLDHashOperator
RuntimeService::RemoveSharedWorkerFromWindowMap(
                                  nsPIDOMWindow* aKey,
                                  nsAutoPtr<nsTArray<WorkerPrivate*> >& aData,
                                  void* aUserArg)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aData.get());
  MOZ_ASSERT(aUserArg);

  auto workerPrivate = static_cast<WorkerPrivate*>(aUserArg);

  MOZ_ASSERT(workerPrivate->IsSharedWorker());

  if (aData->RemoveElement(workerPrivate)) {
    MOZ_ASSERT(!aData->Contains(workerPrivate), "Added worker more than once!");

    if (aData->IsEmpty()) {
      return PL_DHASH_REMOVE;
    }
  }

  return PL_DHASH_NEXT;
}

// static
PLDHashOperator
RuntimeService::FindSharedWorkerInfo(const nsACString& aKey,
                                     SharedWorkerInfo* aData,
                                     void* aUserArg)
{
  auto match = static_cast<MatchSharedWorkerInfo*>(aUserArg);

  if (aData->mWorkerPrivate == match->mWorkerPrivate) {
    match->mSharedWorkerInfo = aData;
    return PL_DHASH_STOP;
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
RuntimeService::CancelWorkersForWindow(nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();

  nsAutoTArray<WorkerPrivate*, MAX_WORKERS_PER_DOMAIN> workers;
  GetWorkersForWindow(aWindow, workers);

  if (!workers.IsEmpty()) {
    nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aWindow);
    MOZ_ASSERT(sgo);

    nsIScriptContext* scx = sgo->GetContext();

    AutoPushJSContext cx(scx ?
                         scx->GetNativeContext() :
                         nsContentUtils::GetSafeJSContext());

    for (uint32_t index = 0; index < workers.Length(); index++) {
      WorkerPrivate*& worker = workers[index];

      if (worker->IsSharedWorker()) {
        worker->CloseSharedWorkersForWindow(aWindow);
      } else if (!worker->Cancel(cx)) {
        JS_ReportPendingException(cx);
      }
    }
  }
}

void
RuntimeService::SuspendWorkersForWindow(nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  nsAutoTArray<WorkerPrivate*, MAX_WORKERS_PER_DOMAIN> workers;
  GetWorkersForWindow(aWindow, workers);

  if (!workers.IsEmpty()) {
    nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aWindow);
    MOZ_ASSERT(sgo);

    nsIScriptContext* scx = sgo->GetContext();

    AutoPushJSContext cx(scx ?
                         scx->GetNativeContext() :
                         nsContentUtils::GetSafeJSContext());

    for (uint32_t index = 0; index < workers.Length(); index++) {
      if (!workers[index]->Suspend(cx, aWindow)) {
        JS_ReportPendingException(cx);
      }
    }
  }
}

void
RuntimeService::ResumeWorkersForWindow(nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  nsAutoTArray<WorkerPrivate*, MAX_WORKERS_PER_DOMAIN> workers;
  GetWorkersForWindow(aWindow, workers);

  if (!workers.IsEmpty()) {
    nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aWindow);
    MOZ_ASSERT(sgo);

    nsIScriptContext* scx = sgo->GetContext();

    AutoPushJSContext cx(scx ?
                         scx->GetNativeContext() :
                         nsContentUtils::GetSafeJSContext());

    for (uint32_t index = 0; index < workers.Length(); index++) {
      if (!workers[index]->SynchronizeAndResume(cx, aWindow, scx)) {
        JS_ReportPendingException(cx);
      }
    }
  }
}

nsresult
RuntimeService::CreateSharedWorker(const GlobalObject& aGlobal,
                                   const nsAString& aScriptURL,
                                   const nsAString& aName,
                                   SharedWorker** aSharedWorker)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(window);

  JSContext* cx = aGlobal.GetContext();

  WorkerPrivate::LoadInfo loadInfo;
  nsresult rv = WorkerPrivate::GetLoadInfo(cx, window, nullptr, aScriptURL,
                                           false, &loadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(loadInfo.mResolvedScriptURI);

  nsCString scriptSpec;
  rv = loadInfo.mResolvedScriptURI->GetSpec(scriptSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<WorkerPrivate> workerPrivate;
  {
    MutexAutoLock lock(mMutex);

    WorkerDomainInfo* domainInfo;
    SharedWorkerInfo* sharedWorkerInfo;

    if (mDomainMap.Get(loadInfo.mDomain, &domainInfo) &&
        domainInfo->mSharedWorkerInfos.Get(scriptSpec, &sharedWorkerInfo) &&
        sharedWorkerInfo->mName == aName) {
      workerPrivate = sharedWorkerInfo->mWorkerPrivate;
    }
  }

  bool created = false;

  if (!workerPrivate) {
    ErrorResult rv;
    workerPrivate =
      WorkerPrivate::Constructor(aGlobal, aScriptURL, false,
                                 WorkerPrivate::WorkerTypeShared, aName,
                                 &loadInfo, rv);
    NS_ENSURE_TRUE(workerPrivate, rv.ErrorCode());

    created = true;
  }

  MOZ_ASSERT(workerPrivate->IsSharedWorker());

  nsRefPtr<SharedWorker> sharedWorker =
    new SharedWorker(window, workerPrivate);

  if (!workerPrivate->RegisterSharedWorker(cx, sharedWorker)) {
    NS_WARNING("Worker is unreachable, this shouldn't happen!");
    sharedWorker->Close();
    return NS_ERROR_FAILURE;
  }

  // This is normally handled in RegisterWorker, but that wasn't called if the
  // worker already existed.
  if (!created) {
    nsTArray<WorkerPrivate*>* windowArray;
    if (!mWindowMap.Get(window, &windowArray)) {
      windowArray = new nsTArray<WorkerPrivate*>(1);
      mWindowMap.Put(window, windowArray);
    }

    if (!windowArray->Contains(workerPrivate)) {
      windowArray->AppendElement(workerPrivate);
    }
  }

  sharedWorker.forget(aSharedWorker);
  return NS_OK;
}

void
RuntimeService::ForgetSharedWorker(WorkerPrivate* aWorkerPrivate)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWorkerPrivate);
  MOZ_ASSERT(aWorkerPrivate->IsSharedWorker());

  MutexAutoLock lock(mMutex);

  WorkerDomainInfo* domainInfo;
  if (mDomainMap.Get(aWorkerPrivate->Domain(), &domainInfo)) {
    MatchSharedWorkerInfo match(aWorkerPrivate);
    domainInfo->mSharedWorkerInfos.EnumerateRead(FindSharedWorkerInfo,
                                                 &match);

    if (match.mSharedWorkerInfo) {
      domainInfo->mSharedWorkerInfos.Remove(
        match.mSharedWorkerInfo->mScriptSpec);
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
                  InitWithFuncCallback(ShutdownIdleThreads, nullptr,
                                       IDLE_THREAD_TIMEOUT_SEC * 1000,
                                       nsITimer::TYPE_ONE_SHOT))) {
    NS_ERROR("Can't schedule timer!");
  }
}

void
RuntimeService::UpdateAllWorkerJSContextOptions()
{
  BROADCAST_ALL_WORKERS(UpdateJSContextOptions,
                        sDefaultJSSettings.content.contextOptions,
                        sDefaultJSSettings.chrome.contextOptions);
}

void
RuntimeService::UpdateAllWorkerPreference(WorkerPreference aPref, bool aValue)
{
  BROADCAST_ALL_WORKERS(UpdatePreference, aPref, aValue);
}

void
RuntimeService::UpdateAllWorkerMemoryParameter(JSGCParamKey aKey,
                                               uint32_t aValue)
{
  BROADCAST_ALL_WORKERS(UpdateJSWorkerMemoryParameter, aKey, aValue);
}

#ifdef JS_GC_ZEAL
void
RuntimeService::UpdateAllWorkerGCZeal()
{
  BROADCAST_ALL_WORKERS(UpdateGCZeal, sDefaultJSSettings.gcZeal,
                        sDefaultJSSettings.gcZealFrequency);
}
#endif

void
RuntimeService::UpdateAllWorkerJITHardening(bool aJITHardening)
{
  BROADCAST_ALL_WORKERS(UpdateJITHardening, aJITHardening);
}

void
RuntimeService::GarbageCollectAllWorkers(bool aShrinking)
{
  BROADCAST_ALL_WORKERS(GarbageCollect, aShrinking);
}

void
RuntimeService::CycleCollectAllWorkers()
{
  BROADCAST_ALL_WORKERS(CycleCollect, /* dummy = */ false);
}

// nsISupports
NS_IMPL_ISUPPORTS1(RuntimeService, nsIObserver)

// nsIObserver
NS_IMETHODIMP
RuntimeService::Observe(nsISupports* aSubject, const char* aTopic,
                        const PRUnichar* aData)
{
  AssertIsOnMainThread();

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Shutdown();
    return NS_OK;
  }
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID)) {
    Cleanup();
    return NS_OK;
  }
  if (!strcmp(aTopic, GC_REQUEST_OBSERVER_TOPIC)) {
    GarbageCollectAllWorkers(/* shrinking = */ false);
    return NS_OK;
  }
  if (!strcmp(aTopic, CC_REQUEST_OBSERVER_TOPIC)) {
    CycleCollectAllWorkers();
    return NS_OK;
  }
  if (!strcmp(aTopic, MEMORY_PRESSURE_OBSERVER_TOPIC)) {
    GarbageCollectAllWorkers(/* shrinking = */ true);
    CycleCollectAllWorkers();
    return NS_OK;
  }

  NS_NOTREACHED("Unknown observer topic!");
  return NS_OK;
}

/* static */ void
RuntimeService::WorkerPrefChanged(const char* aPrefName, void* aClosure)
{
  AssertIsOnMainThread();

  uintptr_t tmp = reinterpret_cast<uintptr_t>(aClosure);
  MOZ_ASSERT(tmp < WORKERPREF_COUNT);
  WorkerPreference key = static_cast<WorkerPreference>(tmp);

  if (key == WORKERPREF_PROMISE) {
    sDefaultPreferences[WORKERPREF_PROMISE] =
      Preferences::GetBool(PREF_PROMISE_ENABLED, false);
#ifdef DUMP_CONTROLLED_BY_PREF
  } else if (key == WORKERPREF_DUMP) {
    key = WORKERPREF_DUMP;
    sDefaultPreferences[WORKERPREF_DUMP] =
      Preferences::GetBool(PREF_DOM_WINDOW_DUMP_ENABLED, false);
#endif
  }

  // This function should never be registered as a callback for a preference it
  // does not handle.
  MOZ_ASSERT(key != WORKERPREF_COUNT);

  RuntimeService* rts = RuntimeService::GetService();
  if (rts) {
    rts->UpdateAllWorkerPreference(key, sDefaultPreferences[key]);
  }
}

void
RuntimeService::JSVersionChanged(const char* /* aPrefName */, void* /* aClosure */)
{
  AssertIsOnMainThread();

  bool useLatest = Preferences::GetBool(PREF_WORKERS_LATEST_JS_VERSION, false);
  JS::CompartmentOptions& options = sDefaultJSSettings.content.compartmentOptions;
  options.setVersion(useLatest ? JSVERSION_LATEST : JSVERSION_DEFAULT);
}
