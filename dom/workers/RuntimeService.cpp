/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RuntimeService.h"

#include "nsAutoPtr.h"
#include "nsIChannel.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocument.h"
#include "nsIDOMChromeWindow.h"
#include "nsIEffectiveTLDService.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsIStreamTransportService.h"
#include "nsISupportsPriority.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIXULRuntime.h"
#include "nsPIDOMWindow.h"

#include <algorithm>
#include "mozilla/ipc/BackgroundChild.h"
#include "GeckoProfiler.h"
#include "jsfriendapi.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/AsyncEventDispatcher.h"
#include "mozilla/Atomics.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/asmjscache/AsmJSCache.h"
#include "mozilla/dom/AtomList.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/FetchUtil.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/PerformanceService.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/IndexedDatabaseManager.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPrefs.h"
#include "nsContentUtils.h"
#include "nsCycleCollector.h"
#include "nsDOMJSUtils.h"
#include "nsISupportsImpl.h"
#include "nsLayoutStatics.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"
#include "OSFileConstants.h"
#include "xpcpublic.h"

#include "Principal.h"
#include "SharedWorker.h"
#include "WorkerDebuggerManager.h"
#include "WorkerLoadInfo.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "WorkerThread.h"
#include "prsystem.h"

#define WORKERS_SHUTDOWN_TOPIC "web-workers-shutdown"

namespace mozilla {

using namespace ipc;

namespace dom {

using namespace workerinternals;

namespace workerinternals {

// The size of the worker runtime heaps in bytes. May be changed via pref.
#define WORKER_DEFAULT_RUNTIME_HEAPSIZE 32 * 1024 * 1024

// The size of the generational GC nursery for workers, in bytes.
#define WORKER_DEFAULT_NURSERY_SIZE 1 * 1024 * 1024

// The size of the worker JS allocation threshold in MB. May be changed via pref.
#define WORKER_DEFAULT_ALLOCATION_THRESHOLD 30

// Half the size of the actual C stack, to be safe.
#define WORKER_CONTEXT_NATIVE_STACK_LIMIT 128 * sizeof(size_t) * 1024

// The maximum number of hardware concurrency, overridable via pref.
#define MAX_HARDWARE_CONCURRENCY 8

// The maximum number of threads to use for workers, overridable via pref.
#define MAX_WORKERS_PER_DOMAIN 512

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
#define PREF_WORKERS_MAX_HARDWARE_CONCURRENCY "dom.maxHardwareConcurrency"

#define PREF_MAX_SCRIPT_RUN_TIME_CONTENT "dom.max_script_run_time"
#define PREF_MAX_SCRIPT_RUN_TIME_CHROME "dom.max_chrome_script_run_time"

#define GC_REQUEST_OBSERVER_TOPIC "child-gc-request"
#define CC_REQUEST_OBSERVER_TOPIC "child-cc-request"
#define MEMORY_PRESSURE_OBSERVER_TOPIC "memory-pressure"

#define BROADCAST_ALL_WORKERS(_func, ...)                                      \
  PR_BEGIN_MACRO                                                               \
    AssertIsOnMainThread();                                                    \
                                                                               \
    AutoTArray<WorkerPrivate*, 100> workers;                                   \
    {                                                                          \
      MutexAutoLock lock(mMutex);                                              \
                                                                               \
      AddAllTopLevelWorkersToArray(workers);                                   \
    }                                                                          \
                                                                               \
    if (!workers.IsEmpty()) {                                                  \
      for (uint32_t index = 0; index < workers.Length(); index++) {            \
        workers[index]-> _func (__VA_ARGS__);                                  \
      }                                                                        \
    }                                                                          \
  PR_END_MACRO

// Prefixes for observing preference changes.
#define PREF_JS_OPTIONS_PREFIX "javascript.options."
#define PREF_WORKERS_OPTIONS_PREFIX PREF_WORKERS_PREFIX "options."
#define PREF_MEM_OPTIONS_PREFIX "mem."
#define PREF_GCZEAL "gcZeal"

static NS_DEFINE_CID(kStreamTransportServiceCID, NS_STREAMTRANSPORTSERVICE_CID);

namespace {

const uint32_t kNoIndex = uint32_t(-1);

uint32_t gMaxWorkersPerDomain = MAX_WORKERS_PER_DOMAIN;
uint32_t gMaxHardwareConcurrency = MAX_HARDWARE_CONCURRENCY;

// Does not hold an owning reference.
RuntimeService* gRuntimeService = nullptr;

// Only true during the call to Init.
bool gRuntimeServiceDuringInit = false;

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
LoadContextOptions(const char* aPrefName, void* /* aClosure */)
{
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();
  if (!rts) {
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
                                          PREF_MEM_OPTIONS_PREFIX))) {
    return;
  }

#ifdef JS_GC_ZEAL
  if (prefName.EqualsLiteral(PREF_JS_OPTIONS_PREFIX PREF_GCZEAL) ||
      prefName.EqualsLiteral(PREF_WORKERS_OPTIONS_PREFIX PREF_GCZEAL)) {
    return;
  }
#endif

  // Context options.
  JS::ContextOptions contextOptions;
  contextOptions.setAsmJS(GetWorkerPref<bool>(NS_LITERAL_CSTRING("asmjs")))
                .setWasm(GetWorkerPref<bool>(NS_LITERAL_CSTRING("wasm")))
                .setWasmBaseline(GetWorkerPref<bool>(NS_LITERAL_CSTRING("wasm_baselinejit")))
                .setWasmIon(GetWorkerPref<bool>(NS_LITERAL_CSTRING("wasm_ionjit")))
#ifdef ENABLE_WASM_GC
                .setWasmGc(GetWorkerPref<bool>(NS_LITERAL_CSTRING("wasm_gc")))
#endif
                .setThrowOnAsmJSValidationFailure(GetWorkerPref<bool>(
                      NS_LITERAL_CSTRING("throw_on_asmjs_validation_failure")))
                .setBaseline(GetWorkerPref<bool>(NS_LITERAL_CSTRING("baselinejit")))
                .setIon(GetWorkerPref<bool>(NS_LITERAL_CSTRING("ion")))
                .setNativeRegExp(GetWorkerPref<bool>(NS_LITERAL_CSTRING("native_regexp")))
                .setAsyncStack(GetWorkerPref<bool>(NS_LITERAL_CSTRING("asyncstack")))
                .setWerror(GetWorkerPref<bool>(NS_LITERAL_CSTRING("werror")))
#ifdef FUZZING
                .setFuzzing(GetWorkerPref<bool>(NS_LITERAL_CSTRING("fuzzing.enabled")))
#endif
                .setStreams(GetWorkerPref<bool>(NS_LITERAL_CSTRING("streams")))
                .setExtraWarnings(GetWorkerPref<bool>(NS_LITERAL_CSTRING("strict")));

  nsCOMPtr<nsIXULRuntime> xr = do_GetService("@mozilla.org/xre/runtime;1");
  if (xr) {
    bool safeMode = false;
    xr->GetInSafeMode(&safeMode);
    if (safeMode) {
      contextOptions.disableOptionsForSafeMode();
    }
  }

  RuntimeService::SetDefaultContextOptions(contextOptions);

  if (rts) {
    rts->UpdateAllWorkerContextOptions();
  }
}

#ifdef JS_GC_ZEAL
void
LoadGCZealOptions(const char* /* aPrefName */, void* /* aClosure */)
{
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();
  if (!rts) {
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
UpdateOtherJSGCMemoryOption(RuntimeService* aRuntimeService,
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

  if (!rts) {
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
  for (uint32_t index = !gRuntimeServiceDuringInit
                          ? JSSettings::kGCSettingsArraySize - 1 : 0;
       index < JSSettings::kGCSettingsArraySize;
       index++) {
    LiteralRebindingCString matchName;

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "max");
    if (memPrefName == matchName || (gRuntimeServiceDuringInit && index == 0)) {
      int32_t prefValue = GetWorkerPref(matchName, -1);
      uint32_t value = (prefValue <= 0 || prefValue >= 0x1000) ?
                       uint32_t(-1) :
                       uint32_t(prefValue) * 1024 * 1024;
      UpdateOtherJSGCMemoryOption(rts, JSGC_MAX_BYTES, value);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "high_water_mark");
    if (memPrefName == matchName || (gRuntimeServiceDuringInit && index == 1)) {
      int32_t prefValue = GetWorkerPref(matchName, 128);
      UpdateOtherJSGCMemoryOption(rts, JSGC_MAX_MALLOC_BYTES,
                                 uint32_t(prefValue) * 1024 * 1024);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_high_frequency_time_limit_ms");
    if (memPrefName == matchName || (gRuntimeServiceDuringInit && index == 2)) {
      UpdateCommonJSGCMemoryOption(rts, matchName,
                                   JSGC_HIGH_FREQUENCY_TIME_LIMIT);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_low_frequency_heap_growth");
    if (memPrefName == matchName || (gRuntimeServiceDuringInit && index == 3)) {
      UpdateCommonJSGCMemoryOption(rts, matchName,
                                   JSGC_LOW_FREQUENCY_HEAP_GROWTH);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_high_frequency_heap_growth_min");
    if (memPrefName == matchName || (gRuntimeServiceDuringInit && index == 4)) {
      UpdateCommonJSGCMemoryOption(rts, matchName,
                                   JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MIN);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_high_frequency_heap_growth_max");
    if (memPrefName == matchName || (gRuntimeServiceDuringInit && index == 5)) {
      UpdateCommonJSGCMemoryOption(rts, matchName,
                                   JSGC_HIGH_FREQUENCY_HEAP_GROWTH_MAX);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_high_frequency_low_limit_mb");
    if (memPrefName == matchName || (gRuntimeServiceDuringInit && index == 6)) {
      UpdateCommonJSGCMemoryOption(rts, matchName,
                                   JSGC_HIGH_FREQUENCY_LOW_LIMIT);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_high_frequency_high_limit_mb");
    if (memPrefName == matchName || (gRuntimeServiceDuringInit && index == 7)) {
      UpdateCommonJSGCMemoryOption(rts, matchName,
                                   JSGC_HIGH_FREQUENCY_HIGH_LIMIT);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_allocation_threshold_mb");
    if (memPrefName == matchName || (gRuntimeServiceDuringInit && index == 8)) {
      UpdateCommonJSGCMemoryOption(rts, matchName, JSGC_ALLOCATION_THRESHOLD);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "gc_incremental_slice_ms");
    if (memPrefName == matchName || (gRuntimeServiceDuringInit && index == 9)) {
      int32_t prefValue = GetWorkerPref(matchName, -1);
      uint32_t value =
        (prefValue <= 0 || prefValue >= 100000) ? 0 : uint32_t(prefValue);
      UpdateOtherJSGCMemoryOption(rts, JSGC_SLICE_TIME_BUDGET, value);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "gc_dynamic_heap_growth");
    if (memPrefName == matchName ||
        (gRuntimeServiceDuringInit && index == 10)) {
      bool prefValue = GetWorkerPref(matchName, false);
      UpdateOtherJSGCMemoryOption(rts, JSGC_DYNAMIC_HEAP_GROWTH,
                                 prefValue ? 0 : 1);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "gc_dynamic_mark_slice");
    if (memPrefName == matchName ||
        (gRuntimeServiceDuringInit && index == 11)) {
      bool prefValue = GetWorkerPref(matchName, false);
      UpdateOtherJSGCMemoryOption(rts, JSGC_DYNAMIC_MARK_SLICE,
                                 prefValue ? 0 : 1);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "gc_min_empty_chunk_count");
    if (memPrefName == matchName ||
        (gRuntimeServiceDuringInit && index == 12)) {
      UpdateCommonJSGCMemoryOption(rts, matchName, JSGC_MIN_EMPTY_CHUNK_COUNT);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "gc_max_empty_chunk_count");
    if (memPrefName == matchName ||
        (gRuntimeServiceDuringInit && index == 13)) {
      UpdateCommonJSGCMemoryOption(rts, matchName, JSGC_MAX_EMPTY_CHUNK_COUNT);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "gc_compacting");
    if (memPrefName == matchName ||
        (gRuntimeServiceDuringInit && index == 14)) {
      bool prefValue = GetWorkerPref(matchName, false);
      UpdateOtherJSGCMemoryOption(rts, JSGC_COMPACTING_ENABLED,
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

bool
InterruptCallback(JSContext* aCx)
{
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(worker);

  // Now is a good time to turn on profiling if it's pending.
  PROFILER_JS_INTERRUPT_CALLBACK();

  return worker->InterruptCallback(aCx);
}

class LogViolationDetailsRunnable final : public WorkerMainThreadRunnable
{
  nsString mFileName;
  uint32_t mLineNum;
  uint32_t mColumnNum;

public:
  LogViolationDetailsRunnable(WorkerPrivate* aWorker,
                              const nsString& aFileName,
                              uint32_t aLineNum,
                              uint32_t aColumnNum)
    : WorkerMainThreadRunnable(aWorker,
                               NS_LITERAL_CSTRING("RuntimeService :: LogViolationDetails"))
    , mFileName(aFileName)
    , mLineNum(aLineNum)
    , mColumnNum(aColumnNum)
  {
    MOZ_ASSERT(aWorker);
  }

  virtual bool MainThreadRun() override;

private:
  ~LogViolationDetailsRunnable() {}
};

bool
ContentSecurityPolicyAllows(JSContext* aCx)
{
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  worker->AssertIsOnWorkerThread();

  if (worker->GetReportCSPViolations()) {
    nsString fileName;
    uint32_t lineNum = 0;
    uint32_t columnNum = 0;

    JS::AutoFilename file;
    if (JS::DescribeScriptedCaller(aCx, &file, &lineNum, &columnNum) && file.get()) {
      fileName = NS_ConvertUTF8toUTF16(file.get());
    } else {
      MOZ_ASSERT(!JS_IsExceptionPending(aCx));
    }

    RefPtr<LogViolationDetailsRunnable> runnable =
        new LogViolationDetailsRunnable(worker, fileName, lineNum, columnNum);

    ErrorResult rv;
    runnable->Dispatch(Killing, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
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
                           const char16_t* aBegin,
                           const char16_t* aLimit,
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

static JS::AsmJSCacheResult
AsmJSCacheOpenEntryForWrite(JS::Handle<JSObject*> aGlobal,
                            const char16_t* aBegin,
                            const char16_t* aEnd,
                            size_t aSize,
                            uint8_t** aMemory,
                            intptr_t* aHandle)
{
  nsIPrincipal* principal = GetPrincipalForAsmJSCacheOp();
  if (!principal) {
    return JS::AsmJSCache_InternalError;
  }

  return asmjscache::OpenEntryForWrite(principal, aBegin, aEnd, aSize, aMemory,
                                       aHandle);
}

// JSDispatchableRunnables are WorkerRunnables used to dispatch JS::Dispatchable
// back to their worker thread. A WorkerRunnable is used for two reasons:
//
// 1. The JS::Dispatchable::run() callback may run JS so we cannot use a control
// runnable since they use async interrupts and break JS run-to-completion.
//
// 2. The DispatchToEventLoopCallback interface is *required* to fail during
// shutdown (see jsapi.h) which is exactly what WorkerRunnable::Dispatch() will
// do. Moreover, JS_DestroyContext() does *not* block on JS::Dispatchable::run
// being called, DispatchToEventLoopCallback failure is expected to happen
// during shutdown.
class JSDispatchableRunnable final : public WorkerRunnable
{
  JS::Dispatchable* mDispatchable;

  ~JSDispatchableRunnable()
  {
    MOZ_ASSERT(!mDispatchable);
  }

  // Disable the usual pre/post-dispatch thread assertions since we are
  // dispatching from some random JS engine internal thread:

  bool PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    return true;
  }

  void PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {
    // For the benefit of the destructor assert.
    if (!aDispatchResult) {
      mDispatchable = nullptr;
    }
  }

public:
  JSDispatchableRunnable(WorkerPrivate* aWorkerPrivate,
                         JS::Dispatchable* aDispatchable)
    : WorkerRunnable(aWorkerPrivate,
                     WorkerRunnable::WorkerThreadUnchangedBusyCount)
    , mDispatchable(aDispatchable)
  {
    MOZ_ASSERT(mDispatchable);
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate == mWorkerPrivate);
    MOZ_ASSERT(aCx == mWorkerPrivate->GetJSContext());
    MOZ_ASSERT(mDispatchable);

    AutoJSAPI jsapi;
    jsapi.Init();

    mDispatchable->run(mWorkerPrivate->GetJSContext(),
                       JS::Dispatchable::NotShuttingDown);
    mDispatchable = nullptr;  // mDispatchable may delete itself

    return true;
  }

  nsresult Cancel() override
  {
    MOZ_ASSERT(mDispatchable);

    AutoJSAPI jsapi;
    jsapi.Init();

    mDispatchable->run(mWorkerPrivate->GetJSContext(),
                       JS::Dispatchable::ShuttingDown);
    mDispatchable = nullptr;  // mDispatchable may delete itself

    return WorkerRunnable::Cancel();
  }
};

static bool
DispatchToEventLoop(void* aClosure, JS::Dispatchable* aDispatchable)
{
  // This callback may execute either on the worker thread or a random
  // JS-internal helper thread.

  // See comment at JS::InitDispatchToEventLoop() below for how we know the
  // WorkerPrivate is alive.
  WorkerPrivate* workerPrivate = reinterpret_cast<WorkerPrivate*>(aClosure);

  // Dispatch is expected to fail during shutdown for the reasons outlined in
  // the JSDispatchableRunnable comment above.
  RefPtr<JSDispatchableRunnable> r =
    new JSDispatchableRunnable(workerPrivate, aDispatchable);
  return r->Dispatch();
}

static bool
ConsumeStream(JSContext* aCx,
              JS::HandleObject aObj,
              JS::MimeType aMimeType,
              JS::StreamConsumer* aConsumer)
{
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  if (!worker) {
    JS_ReportErrorNumberASCII(aCx, js::GetErrorMessage, nullptr,
                              JSMSG_ERROR_CONSUMING_RESPONSE);
    return false;
  }

  return FetchUtil::StreamResponseToJS(aCx, aObj, aMimeType, aConsumer, worker);
}

bool
InitJSContextForWorker(WorkerPrivate* aWorkerPrivate, JSContext* aWorkerCx)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(!aWorkerPrivate->GetJSContext(), "Already has a context!");

  JSSettings settings;
  aWorkerPrivate->CopyJSSettings(settings);

  JS::ContextOptionsRef(aWorkerCx) = settings.contextOptions;

  JSSettings::JSGCSettingsArray& gcSettings = settings.gcSettings;

  // This is the real place where we set the max memory for the runtime.
  for (uint32_t index = 0; index < ArrayLength(gcSettings); index++) {
    const JSSettings::JSGCSetting& setting = gcSettings[index];
    if (setting.key.isSome()) {
      NS_ASSERTION(setting.value, "Can't handle 0 values!");
      JS_SetGCParameter(aWorkerCx, *setting.key, setting.value);
    }
  }

  JS_SetNativeStackQuota(aWorkerCx, WORKER_CONTEXT_NATIVE_STACK_LIMIT);

  // Security policy:
  static const JSSecurityCallbacks securityCallbacks = {
    ContentSecurityPolicyAllows
  };
  JS_SetSecurityCallbacks(aWorkerCx, &securityCallbacks);

  // Set up the asm.js cache callbacks
  static const JS::AsmJSCacheOps asmJSCacheOps = {
    AsmJSCacheOpenEntryForRead,
    asmjscache::CloseEntryForRead,
    AsmJSCacheOpenEntryForWrite,
    asmjscache::CloseEntryForWrite
  };
  JS::SetAsmJSCacheOps(aWorkerCx, &asmJSCacheOps);

  // A WorkerPrivate lives strictly longer than its JSRuntime so we can safely
  // store a raw pointer as the callback's closure argument on the JSRuntime.
  JS::InitDispatchToEventLoop(aWorkerCx, DispatchToEventLoop, (void*)aWorkerPrivate);

  JS::InitConsumeStreamCallback(aWorkerCx, ConsumeStream);

  if (!JS::InitSelfHostedCode(aWorkerCx)) {
    NS_WARNING("Could not init self-hosted code!");
    return false;
  }

  JS_AddInterruptCallback(aWorkerCx, InterruptCallback);

  js::SetCTypesActivityCallback(aWorkerCx, CTypesActivityCallback);

#ifdef JS_GC_ZEAL
  JS_SetGCZeal(aWorkerCx, settings.gcZeal, settings.gcZealFrequency);
#endif

  return true;
}

static bool
PreserveWrapper(JSContext *cx, JSObject *obj)
{
    MOZ_ASSERT(cx);
    MOZ_ASSERT(obj);
    MOZ_ASSERT(mozilla::dom::IsDOMObject(obj));

    return mozilla::dom::TryPreserveWrapper(obj);
}

JSObject*
Wrap(JSContext *cx, JS::HandleObject existing, JS::HandleObject obj)
{
  JSObject* targetGlobal = JS::CurrentGlobalOrNull(cx);
  if (!IsWorkerDebuggerGlobal(targetGlobal) &&
      !IsWorkerDebuggerSandbox(targetGlobal)) {
    MOZ_CRASH("There should be no edges from the debuggee to the debugger.");
  }

  // Note: the JS engine unwraps CCWs before calling this callback.
  JSObject* originGlobal = JS::GetNonCCWObjectGlobal(obj);

  const js::Wrapper* wrapper = nullptr;
  if (IsWorkerDebuggerGlobal(originGlobal) ||
      IsWorkerDebuggerSandbox(originGlobal)) {
    wrapper = &js::CrossCompartmentWrapper::singleton;
  } else {
    wrapper = &js::OpaqueCrossCompartmentWrapper::singleton;
  }

  if (existing) {
    js::Wrapper::Renew(existing, obj, wrapper);
  }
  return js::Wrapper::New(cx, obj, wrapper);
}

static const JSWrapObjectCallbacks WrapObjectCallbacks = {
  Wrap,
  nullptr,
};

class WorkerJSRuntime final : public mozilla::CycleCollectedJSRuntime
{
public:
  // The heap size passed here doesn't matter, we will change it later in the
  // call to JS_SetGCParameter inside InitJSContextForWorker.
  explicit WorkerJSRuntime(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
    : CycleCollectedJSRuntime(aCx)
    , mWorkerPrivate(aWorkerPrivate)
  {
    MOZ_COUNT_CTOR_INHERITED(WorkerJSRuntime, CycleCollectedJSRuntime);
    MOZ_ASSERT(aWorkerPrivate);

    {
      JS::UniqueChars defaultLocale = aWorkerPrivate->AdoptDefaultLocale();
      MOZ_ASSERT(defaultLocale,
                 "failure of a WorkerPrivate to have a default locale should "
                 "have made the worker fail to spawn");

      if (!JS_SetDefaultLocale(Runtime(), defaultLocale.get())) {
        NS_WARNING("failed to set workerCx's default locale");
      }
    }
  }

  void Shutdown(JSContext* cx) override
  {
    // The CC is shut down, and the superclass destructor will GC, so make sure
    // we don't try to CC again.
    mWorkerPrivate = nullptr;

    CycleCollectedJSRuntime::Shutdown(cx);
  }

  ~WorkerJSRuntime()
  {
    MOZ_COUNT_DTOR_INHERITED(WorkerJSRuntime, CycleCollectedJSRuntime);
  }

  virtual void
  PrepareForForgetSkippable() override
  {
  }

  virtual void
  BeginCycleCollectionCallback() override
  {
  }

  virtual void
  EndCycleCollectionCallback(CycleCollectorResults &aResults) override
  {
  }

  void
  DispatchDeferredDeletion(bool aContinuation, bool aPurge) override
  {
    MOZ_ASSERT(!aContinuation);

    // Do it immediately, no need for asynchronous behavior here.
    nsCycleCollector_doDeferredDeletion();
  }

  virtual void CustomGCCallback(JSGCStatus aStatus) override
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

} // anonymous namespace

} // workerinternals namespace

class WorkerJSContext final : public mozilla::CycleCollectedJSContext
{
public:
  // The heap size passed here doesn't matter, we will change it later in the
  // call to JS_SetGCParameter inside InitJSContextForWorker.
  explicit WorkerJSContext(WorkerPrivate* aWorkerPrivate)
    : mWorkerPrivate(aWorkerPrivate)
  {
    MOZ_COUNT_CTOR_INHERITED(WorkerJSContext, CycleCollectedJSContext);
    MOZ_ASSERT(aWorkerPrivate);
    // Magical number 2. Workers have the base recursion depth 1, and normal
    // runnables run at level 2, and we don't want to process microtasks
    // at any other level.
    SetTargetedMicroTaskRecursionDepth(2);
  }

  ~WorkerJSContext()
  {
    MOZ_COUNT_DTOR_INHERITED(WorkerJSContext, CycleCollectedJSContext);
    JSContext* cx = MaybeContext();
    if (!cx) {
      return;   // Initialize() must have failed
    }

    // The worker global should be unrooted and the shutdown cycle collection
    // should break all remaining cycles. The superclass destructor will run
    // the GC one final time and finalize any JSObjects that were participating
    // in cycles that were broken during CC shutdown.
    nsCycleCollector_shutdown();

    // The CC is shut down, and the superclass destructor will GC, so make sure
    // we don't try to CC again.
    mWorkerPrivate = nullptr;
  }

  WorkerJSContext* GetAsWorkerJSContext() override { return this; }

  CycleCollectedJSRuntime* CreateRuntime(JSContext* aCx) override
  {
    return new WorkerJSRuntime(aCx, mWorkerPrivate);
  }

  nsresult Initialize(JSRuntime* aParentRuntime)
  {
    nsresult rv =
      CycleCollectedJSContext::Initialize(aParentRuntime,
                                          WORKER_DEFAULT_RUNTIME_HEAPSIZE,
                                          WORKER_DEFAULT_NURSERY_SIZE);
     if (NS_WARN_IF(NS_FAILED(rv))) {
       return rv;
     }

    JSContext* cx = Context();

    js::SetPreserveWrapperCallback(cx, PreserveWrapper);
    JS_InitDestroyPrincipalsCallback(cx, DestroyWorkerPrincipals);
    JS_SetWrapObjectCallbacks(cx, &WrapObjectCallbacks);
    if (mWorkerPrivate->IsDedicatedWorker()) {
      JS_SetFutexCanWait(cx);
    }

    return NS_OK;
  }

  virtual void DispatchToMicroTask(already_AddRefed<MicroTaskRunnable> aRunnable) override
  {
    RefPtr<MicroTaskRunnable> runnable(aRunnable);

    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(runnable);

    std::queue<RefPtr<MicroTaskRunnable>>* microTaskQueue = nullptr;

    JSContext* cx = GetCurrentWorkerThreadJSContext();
    NS_ASSERTION(cx, "This should never be null!");

    JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
    NS_ASSERTION(global, "This should never be null!");

    // On worker threads, if the current global is the worker global, we use the
    // main micro task queue. Otherwise, the current global must be
    // either the debugger global or a debugger sandbox, and we use the debugger
    // micro task queue instead.
    if (IsWorkerGlobal(global)) {
      microTaskQueue = &GetMicroTaskQueue();
    } else {
      MOZ_ASSERT(IsWorkerDebuggerGlobal(global) ||
                 IsWorkerDebuggerSandbox(global));

      microTaskQueue = &GetDebuggerMicroTaskQueue();
    }

    microTaskQueue->push(runnable.forget());
  }

  bool IsSystemCaller() const override
  {
    return mWorkerPrivate->UsesSystemPrincipal();
  }

  WorkerPrivate* GetWorkerPrivate() const
  {
    return mWorkerPrivate;
  }

private:
  WorkerPrivate* mWorkerPrivate;
};

namespace workerinternals {

namespace {

class WorkerThreadPrimaryRunnable final : public Runnable
{
  WorkerPrivate* mWorkerPrivate;
  RefPtr<WorkerThread> mThread;
  JSRuntime* mParentRuntime;

  class FinishedRunnable final : public Runnable
  {
    RefPtr<WorkerThread> mThread;

  public:
    explicit FinishedRunnable(already_AddRefed<WorkerThread> aThread)
    : Runnable("WorkerThreadPrimaryRunnable::FinishedRunnable")
    , mThread(aThread)
    {
      MOZ_ASSERT(mThread);
    }

    NS_INLINE_DECL_REFCOUNTING_INHERITED(FinishedRunnable, Runnable)

  private:
    ~FinishedRunnable()
    { }

    NS_DECL_NSIRUNNABLE
  };

public:
  WorkerThreadPrimaryRunnable(WorkerPrivate* aWorkerPrivate,
                              WorkerThread* aThread,
                              JSRuntime* aParentRuntime)
    : mozilla::Runnable("WorkerThreadPrimaryRunnable")
    , mWorkerPrivate(aWorkerPrivate)
    , mThread(aThread)
    , mParentRuntime(aParentRuntime)
  {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(aThread);
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(WorkerThreadPrimaryRunnable, Runnable)

private:
  ~WorkerThreadPrimaryRunnable()
  { }

  NS_DECL_NSIRUNNABLE
};

void
PrefLanguagesChanged(const char* /* aPrefName */, void* /* aClosure */)
{
  AssertIsOnMainThread();

  nsTArray<nsString> languages;
  Navigator::GetAcceptLanguages(languages);

  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->UpdateAllWorkerLanguages(languages);
  }
}

void
AppNameOverrideChanged(const char* /* aPrefName */, void* /* aClosure */)
{
  AssertIsOnMainThread();

  nsAutoString override;
  Preferences::GetString("general.appname.override", override);

  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->UpdateAppNameOverridePreference(override);
  }
}

void
AppVersionOverrideChanged(const char* /* aPrefName */, void* /* aClosure */)
{
  AssertIsOnMainThread();

  nsAutoString override;
  Preferences::GetString("general.appversion.override", override);

  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->UpdateAppVersionOverridePreference(override);
  }
}

void
PlatformOverrideChanged(const char* /* aPrefName */, void* /* aClosure */)
{
  AssertIsOnMainThread();

  nsAutoString override;
  Preferences::GetString("general.platform.override", override);

  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->UpdatePlatformOverridePreference(override);
  }
}

} /* anonymous namespace */

struct RuntimeService::IdleThreadInfo
{
  RefPtr<WorkerThread> mThread;
  mozilla::TimeStamp mExpirationTime;
};

// This is only touched on the main thread. Initialized in Init() below.
JSSettings RuntimeService::sDefaultJSSettings;

RuntimeService::RuntimeService()
: mMutex("RuntimeService::mMutex"), mObserved(false),
  mShuttingDown(false), mNavigatorPropertiesLoaded(false)
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
    // The observer service now owns us until shutdown.
    gRuntimeService = new RuntimeService();
    if (NS_FAILED(gRuntimeService->Init())) {
      NS_WARNING("Failed to initialize!");
      gRuntimeService->Cleanup();
      gRuntimeService = nullptr;
      return nullptr;
    }
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
RuntimeService::RegisterWorker(WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnParentThread();

  WorkerPrivate* parent = aWorkerPrivate->GetParent();
  if (!parent) {
    AssertIsOnMainThread();

    if (mShuttingDown) {
      return false;
    }
  }

  const bool isServiceWorker = aWorkerPrivate->IsServiceWorker();
  const bool isSharedWorker = aWorkerPrivate->IsSharedWorker();
  const bool isDedicatedWorker = aWorkerPrivate->IsDedicatedWorker();
  if (isServiceWorker) {
    AssertIsOnMainThread();
    Telemetry::Accumulate(Telemetry::SERVICE_WORKER_SPAWN_ATTEMPTS, 1);
  }

  nsCString sharedWorkerScriptSpec;
  if (isSharedWorker) {
    AssertIsOnMainThread();

    nsCOMPtr<nsIURI> scriptURI = aWorkerPrivate->GetResolvedScriptURI();
    NS_ASSERTION(scriptURI, "Null script URI!");

    nsresult rv = scriptURI->GetSpec(sharedWorkerScriptSpec);
    if (NS_FAILED(rv)) {
      NS_WARNING("GetSpec failed?!");
      return false;
    }

    NS_ASSERTION(!sharedWorkerScriptSpec.IsEmpty(), "Empty spec!");
  }

  bool exemptFromPerDomainMax = false;
  if (isServiceWorker) {
    AssertIsOnMainThread();
    exemptFromPerDomainMax = Preferences::GetBool("dom.serviceWorkers.exemptFromPerDomainMax",
                                                  false);
  }

  const nsCString& domain = aWorkerPrivate->Domain();

  WorkerDomainInfo* domainInfo;
  bool queued = false;
  {
    MutexAutoLock lock(mMutex);

    domainInfo = mDomainMap.LookupForAdd(domain).OrInsert(
      [&domain, parent] () {
        NS_ASSERTION(!parent, "Shouldn't have a parent here!");
        Unused << parent; // silence clang -Wunused-lambda-capture in opt builds
        WorkerDomainInfo* wdi = new WorkerDomainInfo();
        wdi->mDomain = domain;
        return wdi;
      });

    queued = gMaxWorkersPerDomain &&
             domainInfo->ActiveWorkerCount() >= gMaxWorkersPerDomain &&
             !domain.IsEmpty() &&
             !exemptFromPerDomainMax;

    if (queued) {
      domainInfo->mQueuedWorkers.AppendElement(aWorkerPrivate);

      // Worker spawn gets queued due to hitting max workers per domain
      // limit so let's log a warning.
      WorkerPrivate::ReportErrorToConsole("HittingMaxWorkersPerDomain2");

      if (isServiceWorker) {
        Telemetry::Accumulate(Telemetry::SERVICE_WORKER_SPAWN_GETS_QUEUED, 1);
      } else if (isSharedWorker) {
        Telemetry::Accumulate(Telemetry::SHARED_WORKER_SPAWN_GETS_QUEUED, 1);
      } else if (isDedicatedWorker) {
        Telemetry::Accumulate(Telemetry::DEDICATED_WORKER_SPAWN_GETS_QUEUED, 1);
      }
    }
    else if (parent) {
      domainInfo->mChildWorkerCount++;
    }
    else if (isServiceWorker) {
      domainInfo->mActiveServiceWorkers.AppendElement(aWorkerPrivate);
    }
    else {
      domainInfo->mActiveWorkers.AppendElement(aWorkerPrivate);
    }

    if (isSharedWorker) {
#ifdef DEBUG
      for (const UniquePtr<SharedWorkerInfo>& data : domainInfo->mSharedWorkerInfos) {
         if (data->mScriptSpec == sharedWorkerScriptSpec &&
             data->mName == aWorkerPrivate->WorkerName() &&
             // We want to be sure that the window's principal subsumes the
             // SharedWorker's principal and vice versa.
             data->mWorkerPrivate->GetPrincipal()->Subsumes(aWorkerPrivate->GetPrincipal()) &&
             aWorkerPrivate->GetPrincipal()->Subsumes(data->mWorkerPrivate->GetPrincipal())) {
           MOZ_CRASH("We should not instantiate a new SharedWorker!");
         }
      }
#endif

      UniquePtr<SharedWorkerInfo> sharedWorkerInfo(
        new SharedWorkerInfo(aWorkerPrivate, sharedWorkerScriptSpec,
                             aWorkerPrivate->WorkerName()));
      domainInfo->mSharedWorkerInfos.AppendElement(std::move(sharedWorkerInfo));
    }
  }

  // From here on out we must call UnregisterWorker if something fails!
  if (parent) {
    if (!parent->AddChildWorker(aWorkerPrivate)) {
      UnregisterWorker(aWorkerPrivate);
      return false;
    }
  }
  else {
    if (!mNavigatorPropertiesLoaded) {
      Navigator::AppName(mNavigatorProperties.mAppName,
                         false /* aUsePrefOverriddenValue */);
      if (NS_FAILED(Navigator::GetAppVersion(mNavigatorProperties.mAppVersion,
                                             false /* aUsePrefOverriddenValue */)) ||
          NS_FAILED(Navigator::GetPlatform(mNavigatorProperties.mPlatform,
                                           false /* aUsePrefOverriddenValue */))) {
        UnregisterWorker(aWorkerPrivate);
        return false;
      }

      // The navigator overridden properties should have already been read.

      Navigator::GetAcceptLanguages(mNavigatorProperties.mLanguages);
      mNavigatorPropertiesLoaded = true;
    }

    nsPIDOMWindowInner* window = aWorkerPrivate->GetWindow();

    if (!isServiceWorker) {
      // Service workers are excluded since their lifetime is separate from
      // that of dom windows.
      nsTArray<WorkerPrivate*>* windowArray =
        mWindowMap.LookupForAdd(window).OrInsert(
          [] () { return new nsTArray<WorkerPrivate*>(1); });
      if (!windowArray->Contains(aWorkerPrivate)) {
        windowArray->AppendElement(aWorkerPrivate);
      } else {
        MOZ_ASSERT(aWorkerPrivate->IsSharedWorker());
      }
    }
  }

  if (!queued && !ScheduleWorker(aWorkerPrivate)) {
    return false;
  }

  if (isServiceWorker) {
    AssertIsOnMainThread();
    Telemetry::Accumulate(Telemetry::SERVICE_WORKER_WAS_SPAWNED, 1);
  }
  return true;
}

void
RuntimeService::RemoveSharedWorker(WorkerDomainInfo* aDomainInfo,
                                   WorkerPrivate* aWorkerPrivate)
{
  for (uint32_t i = 0; i < aDomainInfo->mSharedWorkerInfos.Length(); ++i) {
    const UniquePtr<SharedWorkerInfo>& data =
      aDomainInfo->mSharedWorkerInfos[i];
    if (data->mWorkerPrivate == aWorkerPrivate) {
      aDomainInfo->mSharedWorkerInfos.RemoveElementAt(i);
      break;
    }
  }
}

void
RuntimeService::UnregisterWorker(WorkerPrivate* aWorkerPrivate)
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
      MOZ_ASSERT(domainInfo->mChildWorkerCount, "Must be non-zero!");
      domainInfo->mChildWorkerCount--;
    }
    else if (aWorkerPrivate->IsServiceWorker()) {
      MOZ_ASSERT(domainInfo->mActiveServiceWorkers.Contains(aWorkerPrivate),
                 "Don't know about this worker!");
      domainInfo->mActiveServiceWorkers.RemoveElement(aWorkerPrivate);
    }
    else {
      MOZ_ASSERT(domainInfo->mActiveWorkers.Contains(aWorkerPrivate),
                 "Don't know about this worker!");
      domainInfo->mActiveWorkers.RemoveElement(aWorkerPrivate);
    }

    if (aWorkerPrivate->IsSharedWorker()) {
      RemoveSharedWorker(domainInfo, aWorkerPrivate);
    }

    // See if there's a queued worker we can schedule.
    if (domainInfo->ActiveWorkerCount() < gMaxWorkersPerDomain &&
        !domainInfo->mQueuedWorkers.IsEmpty()) {
      queuedWorker = domainInfo->mQueuedWorkers[0];
      domainInfo->mQueuedWorkers.RemoveElementAt(0);

      if (queuedWorker->GetParent()) {
        domainInfo->mChildWorkerCount++;
      }
      else if (queuedWorker->IsServiceWorker()) {
        domainInfo->mActiveServiceWorkers.AppendElement(queuedWorker);
      }
      else {
        domainInfo->mActiveWorkers.AppendElement(queuedWorker);
      }
    }

    if (domainInfo->HasNoWorkers()) {
      MOZ_ASSERT(domainInfo->mQueuedWorkers.IsEmpty());
      mDomainMap.Remove(domain);
    }
  }

  if (aWorkerPrivate->IsServiceWorker()) {
    AssertIsOnMainThread();
    Telemetry::AccumulateTimeDelta(Telemetry::SERVICE_WORKER_LIFE_TIME,
                                   aWorkerPrivate->CreationTimeStamp());
  }

  if (aWorkerPrivate->IsSharedWorker() ||
      aWorkerPrivate->IsServiceWorker()) {
    AssertIsOnMainThread();
    aWorkerPrivate->CloseAllSharedWorkers();
  }

  if (parent) {
    parent->RemoveChildWorker(aWorkerPrivate);
  }
  else if (aWorkerPrivate->IsSharedWorker()) {
    AssertIsOnMainThread();

    for (auto iter = mWindowMap.Iter(); !iter.Done(); iter.Next()) {
      nsAutoPtr<nsTArray<WorkerPrivate*>>& workers = iter.Data();
      MOZ_ASSERT(workers.get());

      if (workers->RemoveElement(aWorkerPrivate)) {
        MOZ_ASSERT(!workers->Contains(aWorkerPrivate),
                   "Added worker more than once!");

        if (workers->IsEmpty()) {
          iter.Remove();
        }
      }
    }
  }
  else if (aWorkerPrivate->IsDedicatedWorker()) {
    // May be null.
    nsPIDOMWindowInner* window = aWorkerPrivate->GetWindow();
    if (auto entry = mWindowMap.Lookup(window)) {
      MOZ_ALWAYS_TRUE(entry.Data()->RemoveElement(aWorkerPrivate));
      if (entry.Data()->IsEmpty()) {
        entry.Remove();
      }
    } else {
      MOZ_ASSERT_UNREACHABLE("window is not in mWindowMap");
    }
  }

  if (queuedWorker && !ScheduleWorker(queuedWorker)) {
    UnregisterWorker(queuedWorker);
  }
}

bool
RuntimeService::ScheduleWorker(WorkerPrivate* aWorkerPrivate)
{
  if (!aWorkerPrivate->Start()) {
    // This is ok, means that we didn't need to make a thread for this worker.
    return true;
  }

  RefPtr<WorkerThread> thread;
  {
    MutexAutoLock lock(mMutex);
    if (!mIdleThreadArray.IsEmpty()) {
      uint32_t index = mIdleThreadArray.Length() - 1;
      mIdleThreadArray[index].mThread.swap(thread);
      mIdleThreadArray.RemoveElementAt(index);
    }
  }

  const WorkerThreadFriendKey friendKey;

  if (!thread) {
    thread = WorkerThread::Create(friendKey);
    if (!thread) {
      UnregisterWorker(aWorkerPrivate);
      return false;
    }
  }

  int32_t priority = aWorkerPrivate->IsChromeWorker() ?
                     nsISupportsPriority::PRIORITY_NORMAL :
                     nsISupportsPriority::PRIORITY_LOW;

  if (NS_FAILED(thread->SetPriority(priority))) {
    NS_WARNING("Could not set the thread's priority!");
  }

  JSContext* cx = CycleCollectedJSContext::Get()->Context();
  nsCOMPtr<nsIRunnable> runnable =
    new WorkerThreadPrimaryRunnable(aWorkerPrivate, thread,
                                    JS_GetParentRuntime(cx));
  if (NS_FAILED(thread->DispatchPrimaryRunnable(friendKey, runnable.forget()))) {
    UnregisterWorker(aWorkerPrivate);
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
  TimeStamp now = TimeStamp::NowLoRes() + TimeDuration::FromSeconds(1);

  TimeStamp nextExpiration;

  AutoTArray<RefPtr<WorkerThread>, 20> expiredThreads;
  {
    MutexAutoLock lock(runtime->mMutex);

    for (uint32_t index = 0; index < runtime->mIdleThreadArray.Length();
         index++) {
      IdleThreadInfo& info = runtime->mIdleThreadArray[index];
      if (info.mExpirationTime > now) {
        nextExpiration = info.mExpirationTime;
        break;
      }

      RefPtr<WorkerThread>* thread = expiredThreads.AppendElement();
      thread->swap(info.mThread);
    }

    if (!expiredThreads.IsEmpty()) {
      runtime->mIdleThreadArray.RemoveElementsAt(0, expiredThreads.Length());
    }
  }

  if (!nextExpiration.IsNull()) {
    TimeDuration delta = nextExpiration - TimeStamp::NowLoRes();
    uint32_t delay(delta > TimeDuration(0) ? delta.ToMilliseconds() : 0);

    // Reschedule the timer.
    MOZ_ALWAYS_SUCCEEDS(
      aTimer->InitWithNamedFuncCallback(ShutdownIdleThreads,
                                        nullptr,
                                        delay,
                                        nsITimer::TYPE_ONE_SHOT,
                                        "RuntimeService::ShutdownIdleThreads"));
  }

  for (uint32_t index = 0; index < expiredThreads.Length(); index++) {
    if (NS_FAILED(expiredThreads[index]->Shutdown())) {
      NS_WARNING("Failed to shutdown thread!");
    }
  }
}

nsresult
RuntimeService::Init()
{
  AssertIsOnMainThread();

  nsLayoutStatics::AddRef();

  // Initialize JSSettings.
  if (sDefaultJSSettings.gcSettings[0].key.isNothing()) {
    sDefaultJSSettings.contextOptions = JS::ContextOptions();
    sDefaultJSSettings.chrome.maxScriptRuntime = -1;
    sDefaultJSSettings.content.maxScriptRuntime = MAX_SCRIPT_RUN_TIME_SEC;
#ifdef JS_GC_ZEAL
    sDefaultJSSettings.gcZealFrequency = JS_DEFAULT_ZEAL_FREQ;
    sDefaultJSSettings.gcZeal = 0;
#endif
    SetDefaultJSGCSettings(JSGC_MAX_BYTES, WORKER_DEFAULT_RUNTIME_HEAPSIZE);
    SetDefaultJSGCSettings(JSGC_ALLOCATION_THRESHOLD,
                           WORKER_DEFAULT_ALLOCATION_THRESHOLD);
  }

  // nsIStreamTransportService is thread-safe but it must be initialized on the
  // main-thread. FileReader needs it, so, let's initialize it now.
  nsresult rv;
  nsCOMPtr<nsIStreamTransportService> sts =
    do_GetService(kStreamTransportServiceCID, &rv);
  NS_ENSURE_TRUE(sts, NS_ERROR_FAILURE);

  mIdleThreadTimer = NS_NewTimer();
  NS_ENSURE_STATE(mIdleThreadTimer);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_FAILURE);

  rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID, false);
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

  if (NS_FAILED(obs->AddObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC, false))) {
    NS_WARNING("Failed to register for offline notification event!");
  }

  MOZ_ASSERT(!gRuntimeServiceDuringInit, "This should be false!");
  gRuntimeServiceDuringInit = true;

  if (NS_FAILED(Preferences::RegisterPrefixCallback(
                                 LoadJSGCMemoryOptions,
                                 PREF_JS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX)) ||
      NS_FAILED(Preferences::RegisterPrefixCallbackAndCall(
                            LoadJSGCMemoryOptions,
                            PREF_WORKERS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX)) ||
#ifdef JS_GC_ZEAL
      NS_FAILED(Preferences::RegisterCallback(
                                             LoadGCZealOptions,
                                             PREF_JS_OPTIONS_PREFIX PREF_GCZEAL)) ||
#endif

#define WORKER_PREF(name, callback)                                           \
      NS_FAILED(Preferences::RegisterCallbackAndCall(                         \
                  callback,                                                   \
                  name)) ||
      WORKER_PREF("intl.accept_languages", PrefLanguagesChanged)
      WORKER_PREF("general.appname.override", AppNameOverrideChanged)
      WORKER_PREF("general.appversion.override", AppVersionOverrideChanged)
      WORKER_PREF("general.platform.override", PlatformOverrideChanged)
#ifdef JS_GC_ZEAL
      WORKER_PREF("dom.workers.options.gcZeal", LoadGCZealOptions)
#endif
#undef WORKER_PREF

      NS_FAILED(Preferences::RegisterPrefixCallbackAndCall(
                                                   LoadContextOptions,
                                                   PREF_WORKERS_OPTIONS_PREFIX)) ||
      NS_FAILED(Preferences::RegisterPrefixCallback(LoadContextOptions,
                                                    PREF_JS_OPTIONS_PREFIX))) {
    NS_WARNING("Failed to register pref callbacks!");
  }

  MOZ_ASSERT(gRuntimeServiceDuringInit, "Should be true!");
  gRuntimeServiceDuringInit = false;

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

  int32_t maxHardwareConcurrency =
    Preferences::GetInt(PREF_WORKERS_MAX_HARDWARE_CONCURRENCY,
                        MAX_HARDWARE_CONCURRENCY);
  gMaxHardwareConcurrency = std::max(0, maxHardwareConcurrency);

  RefPtr<OSFileConstantsService> osFileConstantsService =
    OSFileConstantsService::GetOrCreate();
  if (NS_WARN_IF(!osFileConstantsService)) {
    return NS_ERROR_FAILURE;
  }

  if (NS_WARN_IF(!IndexedDatabaseManager::GetOrCreate())) {
    return NS_ERROR_UNEXPECTED;
  }

  // PerformanceService must be initialized on the main-thread.
  PerformanceService::GetOrCreate();

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
  NS_WARNING_ASSERTION(obs, "Failed to get observer service?!");

  // Tell anyone that cares that they're about to lose worker support.
  if (obs && NS_FAILED(obs->NotifyObservers(nullptr, WORKERS_SHUTDOWN_TOPIC,
                                            nullptr))) {
    NS_WARNING("NotifyObservers failed!");
  }

  {
    MutexAutoLock lock(mMutex);

    AutoTArray<WorkerPrivate*, 100> workers;
    AddAllTopLevelWorkersToArray(workers);

    if (!workers.IsEmpty()) {
      // Cancel all top-level workers.
      {
        MutexAutoUnlock unlock(mMutex);

        for (uint32_t index = 0; index < workers.Length(); index++) {
          if (!workers[index]->Kill()) {
            NS_WARNING("Failed to cancel worker!");
          }
        }
      }
    }
  }
}

namespace {

class CrashIfHangingRunnable : public WorkerControlRunnable
{
public:
  explicit CrashIfHangingRunnable(WorkerPrivate* aWorkerPrivate)
    : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount)
    , mMonitor("CrashIfHangingRunnable::mMonitor")
  {}

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    aWorkerPrivate->DumpCrashInformation(mMsg);

    MonitorAutoLock lock(mMonitor);
    lock.Notify();
    return true;
  }

  nsresult
  Cancel() override
  {
    mMsg.Assign("Canceled");

    MonitorAutoLock lock(mMonitor);
    lock.Notify();

    return NS_OK;
  }

  void
  DispatchAndWait()
  {
    MonitorAutoLock lock(mMonitor);

    if (!Dispatch()) {
      mMsg.Assign("Dispatch Error");
      return;
    }

    lock.Wait();
  }

  const nsCString&
  MsgData() const
  {
    return mMsg;
  }

private:
  bool
  PreDispatch(WorkerPrivate* aWorkerPrivate) override
  {
    return true;
  }

  void
  PostDispatch(WorkerPrivate* aWorkerPrivate, bool aDispatchResult) override
  {}

  Monitor mMonitor;
  nsCString mMsg;
};

} // anonymous

void
RuntimeService::CrashIfHanging()
{
  MutexAutoLock lock(mMutex);

  if (mDomainMap.IsEmpty()) {
    return;
  }

  uint32_t activeWorkers = 0;
  uint32_t activeServiceWorkers = 0;
  uint32_t inactiveWorkers = 0;

  nsTArray<WorkerPrivate*> workers;

  for (auto iter = mDomainMap.Iter(); !iter.Done(); iter.Next()) {
    WorkerDomainInfo* aData = iter.UserData();

    activeWorkers += aData->mActiveWorkers.Length();
    activeServiceWorkers += aData->mActiveServiceWorkers.Length();

    workers.AppendElements(aData->mActiveWorkers);
    workers.AppendElements(aData->mActiveServiceWorkers);

    // These might not be top-level workers...
    for (uint32_t index = 0; index < aData->mQueuedWorkers.Length(); index++) {
      WorkerPrivate* worker = aData->mQueuedWorkers[index];
      if (!worker->GetParent()) {
        ++inactiveWorkers;
      }
    }
  }

  // We must have something pending...
  MOZ_DIAGNOSTIC_ASSERT(activeWorkers + activeServiceWorkers + inactiveWorkers);

  nsCString msg;

  // A: active Workers | S: active ServiceWorkers | Q: queued Workers
  msg.AppendPrintf("Workers Hanging - %d|A:%d|S:%d|Q:%d", mShuttingDown ? 1 : 0,
                   activeWorkers, activeServiceWorkers, inactiveWorkers);

  // For each thread, let's print some data to know what is going wrong.
  for (uint32_t i = 0; i < workers.Length(); ++i) {
    WorkerPrivate* workerPrivate = workers[i];

    // BC: Busy Count
    msg.AppendPrintf("-BC:%d", workerPrivate->BusyCount());

    RefPtr<CrashIfHangingRunnable> runnable = new
      CrashIfHangingRunnable(workerPrivate);
    runnable->DispatchAndWait();

    msg.Append(runnable->MsgData());
  }

  // This string will be leaked.
  MOZ_CRASH_UNSAFE_OOL(strdup(msg.BeginReading()));
}

// This spins the event loop until all workers are finished and their threads
// have been joined.
void
RuntimeService::Cleanup()
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_WARNING_ASSERTION(obs, "Failed to get observer service?!");

  if (mIdleThreadTimer) {
    if (NS_FAILED(mIdleThreadTimer->Cancel())) {
      NS_WARNING("Failed to cancel idle timer!");
    }
    mIdleThreadTimer = nullptr;
  }

  {
    MutexAutoLock lock(mMutex);

    AutoTArray<WorkerPrivate*, 100> workers;
    AddAllTopLevelWorkersToArray(workers);

    if (!workers.IsEmpty()) {
      nsIThread* currentThread = NS_GetCurrentThread();
      NS_ASSERTION(currentThread, "This should never be null!");

      // Shut down any idle threads.
      if (!mIdleThreadArray.IsEmpty()) {
        AutoTArray<RefPtr<WorkerThread>, 20> idleThreads;

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
    if (NS_FAILED(Preferences::UnregisterPrefixCallback(LoadContextOptions,
                                                        PREF_JS_OPTIONS_PREFIX)) ||
        NS_FAILED(Preferences::UnregisterPrefixCallback(LoadContextOptions,
                                                        PREF_WORKERS_OPTIONS_PREFIX)) ||

#define WORKER_PREF(name, callback)                                           \
      NS_FAILED(Preferences::UnregisterCallback(                              \
                  callback,                                                   \
                  name)) ||
      WORKER_PREF("intl.accept_languages", PrefLanguagesChanged)
      WORKER_PREF("general.appname.override", AppNameOverrideChanged)
      WORKER_PREF("general.appversion.override", AppVersionOverrideChanged)
      WORKER_PREF("general.platform.override", PlatformOverrideChanged)
#ifdef JS_GC_ZEAL
      WORKER_PREF("dom.workers.options.gcZeal", LoadGCZealOptions)
#endif
#undef WORKER_PREF

#ifdef JS_GC_ZEAL
        NS_FAILED(Preferences::UnregisterCallback(
                                             LoadGCZealOptions,
                                             PREF_JS_OPTIONS_PREFIX PREF_GCZEAL)) ||
#endif
        NS_FAILED(Preferences::UnregisterPrefixCallback(
                                 LoadJSGCMemoryOptions,
                                 PREF_JS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX)) ||
        NS_FAILED(Preferences::UnregisterPrefixCallback(
                            LoadJSGCMemoryOptions,
                            PREF_WORKERS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX))) {
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

      if (NS_FAILED(obs->RemoveObserver(this,
                                        NS_IOSERVICE_OFFLINE_STATUS_TOPIC))) {
        NS_WARNING("Failed to unregister for offline notification event!");
      }
      obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID);
      obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
      mObserved = false;
    }
  }

  nsLayoutStatics::Release();
}

void
RuntimeService::AddAllTopLevelWorkersToArray(nsTArray<WorkerPrivate*>& aWorkers)
{
  for (auto iter = mDomainMap.Iter(); !iter.Done(); iter.Next()) {

    WorkerDomainInfo* aData = iter.UserData();

#ifdef DEBUG
    for (uint32_t index = 0; index < aData->mActiveWorkers.Length(); index++) {
      MOZ_ASSERT(!aData->mActiveWorkers[index]->GetParent(),
                 "Shouldn't have a parent in this list!");
    }
    for (uint32_t index = 0; index < aData->mActiveServiceWorkers.Length(); index++) {
      MOZ_ASSERT(!aData->mActiveServiceWorkers[index]->GetParent(),
                 "Shouldn't have a parent in this list!");
    }
#endif

    aWorkers.AppendElements(aData->mActiveWorkers);
    aWorkers.AppendElements(aData->mActiveServiceWorkers);

    // These might not be top-level workers...
    for (uint32_t index = 0; index < aData->mQueuedWorkers.Length(); index++) {
      WorkerPrivate* worker = aData->mQueuedWorkers[index];
      if (!worker->GetParent()) {
        aWorkers.AppendElement(worker);
      }
    }
  }
}

void
RuntimeService::GetWorkersForWindow(nsPIDOMWindowInner* aWindow,
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
RuntimeService::CancelWorkersForWindow(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();

  nsTArray<WorkerPrivate*> workers;
  GetWorkersForWindow(aWindow, workers);

  if (!workers.IsEmpty()) {
    for (uint32_t index = 0; index < workers.Length(); index++) {
      WorkerPrivate*& worker = workers[index];

      if (worker->IsSharedWorker()) {
        worker->CloseSharedWorkersForWindow(aWindow);
      } else {
        worker->Cancel();
      }
    }
  }
}

void
RuntimeService::FreezeWorkersForWindow(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  nsTArray<WorkerPrivate*> workers;
  GetWorkersForWindow(aWindow, workers);

  for (uint32_t index = 0; index < workers.Length(); index++) {
    workers[index]->Freeze(aWindow);
  }
}

void
RuntimeService::ThawWorkersForWindow(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  nsTArray<WorkerPrivate*> workers;
  GetWorkersForWindow(aWindow, workers);

  for (uint32_t index = 0; index < workers.Length(); index++) {
    workers[index]->Thaw(aWindow);
  }
}

void
RuntimeService::SuspendWorkersForWindow(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  nsTArray<WorkerPrivate*> workers;
  GetWorkersForWindow(aWindow, workers);

  for (uint32_t index = 0; index < workers.Length(); index++) {
    workers[index]->ParentWindowPaused();
  }
}

void
RuntimeService::ResumeWorkersForWindow(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);

  nsTArray<WorkerPrivate*> workers;
  GetWorkersForWindow(aWindow, workers);

  for (uint32_t index = 0; index < workers.Length(); index++) {
    workers[index]->ParentWindowResumed();
  }
}

void
RuntimeService::PropagateFirstPartyStorageAccessGranted(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(StaticPrefs::privacy_restrict3rdpartystorage_enabled());

  nsTArray<WorkerPrivate*> workers;
  GetWorkersForWindow(aWindow, workers);

  for (uint32_t index = 0; index < workers.Length(); index++) {
    workers[index]->PropagateFirstPartyStorageAccessGranted();
  }
}

nsresult
RuntimeService::CreateSharedWorker(const GlobalObject& aGlobal,
                                   const nsAString& aScriptURL,
                                   const nsAString& aName,
                                   SharedWorker** aSharedWorker)
{
  AssertIsOnMainThread();

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(window);

  // If the window is blocked from accessing storage, do not allow it
  // to connect to a SharedWorker.  This would potentially allow it
  // to communicate with other windows that do have storage access.
  // Allow private browsing, however, as we handle that isolation
  // via the principal.
  auto storageAllowed = nsContentUtils::StorageAllowedForWindow(window);
  if (storageAllowed != nsContentUtils::StorageAccess::eAllow &&
      storageAllowed != nsContentUtils::StorageAccess::ePrivateBrowsing) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  // Assert that the principal private browsing state matches the
  // StorageAccess value.
#ifdef  MOZ_DIAGNOSTIC_ASSERT_ENABLED
  if (storageAllowed == nsContentUtils::StorageAccess::ePrivateBrowsing) {
    nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
    nsCOMPtr<nsIPrincipal> principal = doc ? doc->NodePrincipal() : nullptr;
    uint32_t privateBrowsingId = 0;
    if (principal) {
      MOZ_ALWAYS_SUCCEEDS(principal->GetPrivateBrowsingId(&privateBrowsingId));
    }
    MOZ_DIAGNOSTIC_ASSERT(privateBrowsingId != 0);
  }
#endif // MOZ_DIAGNOSTIC_ASSERT_ENABLED

  JSContext* cx = aGlobal.Context();

  WorkerLoadInfo loadInfo;
  nsresult rv = WorkerPrivate::GetLoadInfo(cx, window, nullptr, aScriptURL,
                                           false,
                                           WorkerPrivate::OverrideLoadGroup,
                                           WorkerTypeShared, &loadInfo);
  NS_ENSURE_SUCCESS(rv, rv);

  return CreateSharedWorkerFromLoadInfo(cx, &loadInfo, aScriptURL, aName,
                                        aSharedWorker);
}

nsresult
RuntimeService::CreateSharedWorkerFromLoadInfo(JSContext* aCx,
                                               WorkerLoadInfo* aLoadInfo,
                                               const nsAString& aScriptURL,
                                               const nsAString& aName,
                                               SharedWorker** aSharedWorker)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aLoadInfo);
  MOZ_ASSERT(aLoadInfo->mResolvedScriptURI);

  RefPtr<WorkerPrivate> workerPrivate;
  {
    MutexAutoLock lock(mMutex);

    nsCString scriptSpec;
    nsresult rv = aLoadInfo->mResolvedScriptURI->GetSpec(scriptSpec);
    NS_ENSURE_SUCCESS(rv, rv);

    MOZ_DIAGNOSTIC_ASSERT(aLoadInfo->mPrincipal && aLoadInfo->mLoadingPrincipal);

    WorkerDomainInfo* domainInfo;
    if (mDomainMap.Get(aLoadInfo->mDomain, &domainInfo)) {
      for (const UniquePtr<SharedWorkerInfo>& data : domainInfo->mSharedWorkerInfos) {
        if (data->mScriptSpec == scriptSpec &&
            data->mName == aName &&
            // We want to be sure that the window's principal subsumes the
            // SharedWorker's loading principal and vice versa.
            aLoadInfo->mLoadingPrincipal->Subsumes(data->mWorkerPrivate->GetLoadingPrincipal()) &&
            data->mWorkerPrivate->GetLoadingPrincipal()->Subsumes(aLoadInfo->mLoadingPrincipal)) {
          workerPrivate = data->mWorkerPrivate;
          break;
        }
      }
    }
  }

  // Keep a reference to the window before spawning the worker. If the worker is
  // a Shared/Service worker and the worker script loads and executes before
  // the SharedWorker object itself is created before then WorkerScriptLoaded()
  // will reset the loadInfo's window.
  nsCOMPtr<nsPIDOMWindowInner> window = aLoadInfo->mWindow;

  // shouldAttachToWorkerPrivate tracks whether our SharedWorker should actually
  // get attached to the WorkerPrivate we're using.  It will become false if the
  // WorkerPrivate already exists and its secure context state doesn't match
  // what we want for the new SharedWorker.
  bool shouldAttachToWorkerPrivate = true;
  bool created = false;
  ErrorResult rv;
  if (!workerPrivate) {
    workerPrivate =
      WorkerPrivate::Constructor(aCx, aScriptURL, false,
                                 WorkerTypeShared, aName, VoidCString(),
                                 aLoadInfo, rv);
    NS_ENSURE_TRUE(workerPrivate, rv.StealNSResult());

    created = true;
  } else {
    // Check whether the secure context state matches.  The current realm
    // of aCx is the realm of the SharedWorker constructor that was invoked,
    // which is the realm of the document that will be hooked up to the worker,
    // so that's what we want to check.
    shouldAttachToWorkerPrivate =
      workerPrivate->IsSecureContext() ==
        JS::GetIsSecureContext(js::GetContextRealm(aCx));

    // If we're attaching to an existing SharedWorker private, then we
    // must update the overriden load group to account for our document's
    // load group.
    if (shouldAttachToWorkerPrivate) {
      workerPrivate->UpdateOverridenLoadGroup(aLoadInfo->mLoadGroup);
    }
  }

  // We don't actually care about this MessageChannel, but we use it to 'steal'
  // its 2 connected ports.
  RefPtr<MessageChannel> channel =
    MessageChannel::Constructor(window->AsGlobal(), rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  RefPtr<SharedWorker> sharedWorker = new SharedWorker(window, workerPrivate,
                                                       channel->Port1());

  if (!shouldAttachToWorkerPrivate) {
    // We're done here.  Just queue up our error event and return our
    // dead-on-arrival SharedWorker.
    RefPtr<AsyncEventDispatcher> errorEvent =
      new AsyncEventDispatcher(sharedWorker,
                               NS_LITERAL_STRING("error"),
                               CanBubble::eNo);
    errorEvent->PostDOMEvent();
    sharedWorker.forget(aSharedWorker);
    return NS_OK;
  }

  if (!workerPrivate->RegisterSharedWorker(sharedWorker, channel->Port2())) {
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
    RemoveSharedWorker(domainInfo, aWorkerPrivate);
  }
}

void
RuntimeService::NoteIdleThread(WorkerThread* aThread)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aThread);

  bool shutdownThread = mShuttingDown;
  bool scheduleTimer = false;

  if (!shutdownThread) {
    static TimeDuration timeout =
      TimeDuration::FromSeconds(IDLE_THREAD_TIMEOUT_SEC);

    TimeStamp expirationTime = TimeStamp::NowLoRes() + timeout;

    MutexAutoLock lock(mMutex);

    uint32_t previousIdleCount = mIdleThreadArray.Length();

    if (previousIdleCount < MAX_IDLE_THREADS) {
      IdleThreadInfo* info = mIdleThreadArray.AppendElement();
      info->mThread = aThread;
      info->mExpirationTime = expirationTime;

      scheduleTimer = previousIdleCount == 0;
    } else {
      shutdownThread = true;
    }
  }

  MOZ_ASSERT_IF(shutdownThread, !scheduleTimer);
  MOZ_ASSERT_IF(scheduleTimer, !shutdownThread);

  // Too many idle threads, just shut this one down.
  if (shutdownThread) {
    MOZ_ALWAYS_SUCCEEDS(aThread->Shutdown());
  } else if (scheduleTimer) {
    MOZ_ALWAYS_SUCCEEDS(
      mIdleThreadTimer->InitWithNamedFuncCallback(ShutdownIdleThreads,
                                                  nullptr,
                                                  IDLE_THREAD_TIMEOUT_SEC * 1000,
                                                  nsITimer::TYPE_ONE_SHOT,
                                                  "RuntimeService::ShutdownIdleThreads"));
  }
}

void
RuntimeService::UpdateAllWorkerContextOptions()
{
  BROADCAST_ALL_WORKERS(UpdateContextOptions, sDefaultJSSettings.contextOptions);
}

void
RuntimeService::UpdateAppNameOverridePreference(const nsAString& aValue)
{
  AssertIsOnMainThread();
  mNavigatorProperties.mAppNameOverridden = aValue;
}

void
RuntimeService::UpdateAppVersionOverridePreference(const nsAString& aValue)
{
  AssertIsOnMainThread();
  mNavigatorProperties.mAppVersionOverridden = aValue;
}

void
RuntimeService::UpdatePlatformOverridePreference(const nsAString& aValue)
{
  AssertIsOnMainThread();
  mNavigatorProperties.mPlatformOverridden = aValue;
}

void
RuntimeService::UpdateAllWorkerLanguages(const nsTArray<nsString>& aLanguages)
{
  MOZ_ASSERT(NS_IsMainThread());

  mNavigatorProperties.mLanguages = aLanguages;
  BROADCAST_ALL_WORKERS(UpdateLanguages, aLanguages);
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
RuntimeService::GarbageCollectAllWorkers(bool aShrinking)
{
  BROADCAST_ALL_WORKERS(GarbageCollect, aShrinking);
}

void
RuntimeService::CycleCollectAllWorkers()
{
  BROADCAST_ALL_WORKERS(CycleCollect, /* dummy = */ false);
}

void
RuntimeService::SendOfflineStatusChangeEventToAllWorkers(bool aIsOffline)
{
  BROADCAST_ALL_WORKERS(OfflineStatusChangeEvent, aIsOffline);
}

void
RuntimeService::MemoryPressureAllWorkers()
{
  BROADCAST_ALL_WORKERS(MemoryPressure, /* dummy = */ false);
}

uint32_t
RuntimeService::ClampedHardwareConcurrency() const
{
  // The Firefox Hardware Report says 70% of Firefox users have exactly 2 cores.
  // When the resistFingerprinting pref is set, we want to blend into the crowd
  // so spoof navigator.hardwareConcurrency = 2 to reduce user uniqueness.
  if (MOZ_UNLIKELY(nsContentUtils::ShouldResistFingerprinting())) {
    return 2;
  }

  // This needs to be atomic, because multiple workers, and even mainthread,
  // could race to initialize it at once.
  static Atomic<uint32_t> clampedHardwareConcurrency;

  // No need to loop here: if compareExchange fails, that just means that some
  // other worker has initialized numberOfProcessors, so we're good to go.
  if (!clampedHardwareConcurrency) {
    int32_t numberOfProcessors = PR_GetNumberOfProcessors();
    if (numberOfProcessors <= 0) {
      numberOfProcessors = 1; // Must be one there somewhere
    }
    uint32_t clampedValue = std::min(uint32_t(numberOfProcessors),
                                     gMaxHardwareConcurrency);
    Unused << clampedHardwareConcurrency.compareExchange(0, clampedValue);
  }

  return clampedHardwareConcurrency;
}

// nsISupports
NS_IMPL_ISUPPORTS(RuntimeService, nsIObserver)

// nsIObserver
NS_IMETHODIMP
RuntimeService::Observe(nsISupports* aSubject, const char* aTopic,
                        const char16_t* aData)
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
    MemoryPressureAllWorkers();
    return NS_OK;
  }
  if (!strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC)) {
    SendOfflineStatusChangeEventToAllWorkers(NS_IsOffline());
    return NS_OK;
  }

  MOZ_ASSERT_UNREACHABLE("Unknown observer topic!");
  return NS_OK;
}

bool
LogViolationDetailsRunnable::MainThreadRun()
{
  AssertIsOnMainThread();

  nsIContentSecurityPolicy* csp = mWorkerPrivate->GetCSP();
  if (csp) {
    NS_NAMED_LITERAL_STRING(scriptSample,
        "Call to eval() or related function blocked by CSP.");
    if (mWorkerPrivate->GetReportCSPViolations()) {
      csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                               mFileName, scriptSample, mLineNum, mColumnNum,
                               EmptyString(), EmptyString());
    }
  }

  return true;
}

NS_IMETHODIMP
WorkerThreadPrimaryRunnable::Run()
{
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
    "WorkerThreadPrimaryRunnable::Run", OTHER, mWorkerPrivate->ScriptURL());

  using mozilla::ipc::BackgroundChild;

  // Note: GetOrCreateForCurrentThread() must be called prior to
  //       mWorkerPrivate->SetThread() in order to avoid accidentally consuming
  //       worker messages here.
  if (NS_WARN_IF(!BackgroundChild::GetOrCreateForCurrentThread())) {
    // XXX need to fire an error at parent.
    // Failed in creating BackgroundChild: probably in shutdown. Continue to run
    // without BackgroundChild created.
  }

  class MOZ_STACK_CLASS SetThreadHelper final
  {
    // Raw pointer: this class is on the stack.
    WorkerPrivate* mWorkerPrivate;
    RefPtr<AbstractThread> mAbstractThread;

  public:
    SetThreadHelper(WorkerPrivate* aWorkerPrivate, WorkerThread* aThread)
      : mWorkerPrivate(aWorkerPrivate)
      , mAbstractThread(AbstractThread::CreateXPCOMThreadWrapper(NS_GetCurrentThread(), false))
    {
      MOZ_ASSERT(aWorkerPrivate);
      MOZ_ASSERT(aThread);

      mWorkerPrivate->SetThread(aThread);
    }

    ~SetThreadHelper()
    {
      if (mWorkerPrivate) {
        mWorkerPrivate->SetThread(nullptr);
      }
    }

    void Nullify()
    {
      MOZ_ASSERT(mWorkerPrivate);
      mWorkerPrivate->SetThread(nullptr);
      mWorkerPrivate = nullptr;
    }
  };

  SetThreadHelper threadHelper(mWorkerPrivate, mThread);

  mWorkerPrivate->AssertIsOnWorkerThread();

  {
    nsCycleCollector_startup();

    auto context = MakeUnique<WorkerJSContext>(mWorkerPrivate);
    nsresult rv = context->Initialize(mParentRuntime);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    JSContext* cx = context->Context();

    if (!InitJSContextForWorker(mWorkerPrivate, cx)) {
      // XXX need to fire an error at parent.
      NS_ERROR("Failed to create context!");
      return NS_ERROR_FAILURE;
    }

    {
      PROFILER_SET_JS_CONTEXT(cx);

      {
        JSAutoRequest ar(cx);

        mWorkerPrivate->DoRunLoop(cx);
        // The AutoJSAPI in DoRunLoop should have reported any exceptions left
        // on cx.  Note that we still need the JSAutoRequest above because
        // AutoJSAPI on workers does NOT enter a request!
        MOZ_ASSERT(!JS_IsExceptionPending(cx));
      }

      BackgroundChild::CloseForCurrentThread();

      PROFILER_CLEAR_JS_CONTEXT();
    }

    // There may still be runnables on the debugger event queue that hold a
    // strong reference to the debugger global scope. These runnables are not
    // visible to the cycle collector, so we need to make sure to clear the
    // debugger event queue before we try to destroy the context. If we don't,
    // the garbage collector will crash.
    mWorkerPrivate->ClearDebuggerEventQueue();

    // Perform a full GC. This will collect the main worker global and CC,
    // which should break all cycles that touch JS.
    JS_GC(cx);

    // Before shutting down the cycle collector we need to do one more pass
    // through the event loop to clean up any C++ objects that need deferred
    // cleanup.
    mWorkerPrivate->ClearMainEventQueue(WorkerPrivate::WorkerRan);

    // Now WorkerJSContext goes out of scope and its destructor will shut
    // down the cycle collector. This breaks any remaining cycles and collects
    // any remaining C++ objects.
  }

  threadHelper.Nullify();

  mWorkerPrivate->ScheduleDeletion(WorkerPrivate::WorkerRan);

  // It is no longer safe to touch mWorkerPrivate.
  mWorkerPrivate = nullptr;

  // Now recycle this thread.
  nsCOMPtr<nsIEventTarget> mainTarget = GetMainThreadEventTarget();
  MOZ_ASSERT(mainTarget);

  RefPtr<FinishedRunnable> finishedRunnable =
    new FinishedRunnable(mThread.forget());
  MOZ_ALWAYS_SUCCEEDS(mainTarget->Dispatch(finishedRunnable,
                                           NS_DISPATCH_NORMAL));

  return NS_OK;
}

NS_IMETHODIMP
WorkerThreadPrimaryRunnable::FinishedRunnable::Run()
{
  AssertIsOnMainThread();

  RefPtr<WorkerThread> thread;
  mThread.swap(thread);

  RuntimeService* rts = RuntimeService::GetService();
  if (rts) {
    rts->NoteIdleThread(thread);
  }
  else if (thread->ShutdownRequired()) {
    MOZ_ALWAYS_SUCCEEDS(thread->Shutdown());
  }

  return NS_OK;
}

} // workerinternals namespace

void
CancelWorkersForWindow(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->CancelWorkersForWindow(aWindow);
  }
}

void
FreezeWorkersForWindow(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->FreezeWorkersForWindow(aWindow);
  }
}

void
ThawWorkersForWindow(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->ThawWorkersForWindow(aWindow);
  }
}

void
SuspendWorkersForWindow(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->SuspendWorkersForWindow(aWindow);
  }
}

void
ResumeWorkersForWindow(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->ResumeWorkersForWindow(aWindow);
  }
}

void
PropagateFirstPartyStorageAccessGrantedToWorkers(nsPIDOMWindowInner* aWindow)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(StaticPrefs::privacy_restrict3rdpartystorage_enabled());

  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->PropagateFirstPartyStorageAccessGranted(aWindow);
  }
}

WorkerPrivate*
GetWorkerPrivateFromContext(JSContext* aCx)
{
  MOZ_ASSERT(!NS_IsMainThread());
  MOZ_ASSERT(aCx);

  CycleCollectedJSContext* ccjscx = CycleCollectedJSContext::GetFor(aCx);
  if (!ccjscx) {
    return nullptr;
  }

  WorkerJSContext* workerjscx = ccjscx->GetAsWorkerJSContext();
  // GetWorkerPrivateFromContext is called only for worker contexts.  The
  // context private is cleared early in ~CycleCollectedJSContext() and so
  // GetFor() returns null above if called after ccjscx is no longer a
  // WorkerJSContext.
  MOZ_ASSERT(workerjscx);
  return workerjscx->GetWorkerPrivate();
}

WorkerPrivate*
GetCurrentThreadWorkerPrivate()
{
  MOZ_ASSERT(!NS_IsMainThread());

  CycleCollectedJSContext* ccjscx = CycleCollectedJSContext::Get();
  if (!ccjscx) {
    return nullptr;
  }

  WorkerJSContext* workerjscx = ccjscx->GetAsWorkerJSContext();
  // Although GetCurrentThreadWorkerPrivate() is called only for worker
  // threads, the ccjscx will no longer be a WorkerJSContext if called from
  // stable state events during ~CycleCollectedJSContext().
  if (!workerjscx) {
    return nullptr;
  }

  return workerjscx->GetWorkerPrivate();
}

bool
IsCurrentThreadRunningWorker()
{
  return !NS_IsMainThread() && !!GetCurrentThreadWorkerPrivate();
}

bool
IsCurrentThreadRunningChromeWorker()
{
  return GetCurrentThreadWorkerPrivate()->UsesSystemPrincipal();
}

JSContext*
GetCurrentWorkerThreadJSContext()
{
  WorkerPrivate* wp = GetCurrentThreadWorkerPrivate();
  if (!wp) {
    return nullptr;
  }
  return wp->GetJSContext();
}

JSObject*
GetCurrentThreadWorkerGlobal()
{
  WorkerPrivate* wp = GetCurrentThreadWorkerPrivate();
  if (!wp) {
    return nullptr;
  }
  WorkerGlobalScope* scope = wp->GlobalScope();
  if (!scope) {
    return nullptr;
  }
  return scope->GetGlobalJSObject();
}

} // dom namespace
} // mozilla namespace
