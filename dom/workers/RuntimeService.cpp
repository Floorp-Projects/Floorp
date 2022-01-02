/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RuntimeService.h"

#include "nsContentSecurityUtils.h"
#include "nsIContentSecurityPolicy.h"
#include "mozilla/dom/Document.h"
#include "nsIObserverService.h"
#include "nsIScriptContext.h"
#include "nsIStreamTransportService.h"
#include "nsISupportsPriority.h"
#include "nsITimer.h"
#include "nsIURI.h"
#include "nsIXULRuntime.h"
#include "nsPIDOMWindow.h"

#include <algorithm>
#include "mozilla/ipc/BackgroundChild.h"
#include "GeckoProfiler.h"
#include "js/experimental/CTypes.h"  // JS::CTypesActivityType, JS::SetCTypesActivityCallback
#include "jsfriendapi.h"
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "js/ContextOptions.h"
#include "js/Initialization.h"
#include "js/LocaleSensitive.h"
#include "js/WasmFeatures.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/dom/AtomList.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ErrorEventBinding.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/FetchUtil.h"
#include "mozilla/dom/MessageChannel.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/PerformanceService.h"
#include "mozilla/dom/RemoteWorkerChild.h"
#include "mozilla/dom/WorkerBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/IndexedDatabaseManager.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/Monitor.h"
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
#include "XPCPrefableContextOptions.h"
#include "XPCSelfHostedShmem.h"

#if defined(XP_MACOSX)
#  include "nsMacUtilsImpl.h"
#endif

#include "Principal.h"
#include "WorkerDebuggerManager.h"
#include "WorkerError.h"
#include "WorkerLoadInfo.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WorkerScope.h"
#include "WorkerThread.h"
#include "prsystem.h"

#ifdef DEBUG
#  include "nsICookieJarSettings.h"
#endif

#define WORKERS_SHUTDOWN_TOPIC "web-workers-shutdown"

namespace mozilla {

using namespace ipc;

namespace dom {

using namespace workerinternals;

namespace workerinternals {

// The size of the worker runtime heaps in bytes. May be changed via pref.
#define WORKER_DEFAULT_RUNTIME_HEAPSIZE 32 * 1024 * 1024

// The size of the worker JS allocation threshold in MB. May be changed via
// pref.
#define WORKER_DEFAULT_ALLOCATION_THRESHOLD 30

// Half the size of the actual C stack, to be safe.
#define WORKER_CONTEXT_NATIVE_STACK_LIMIT 128 * sizeof(size_t) * 1024

// The maximum number of hardware concurrency, overridable via pref.
#define MAX_HARDWARE_CONCURRENCY 8

// The maximum number of threads to use for workers, overridable via pref.
#define MAX_WORKERS_PER_DOMAIN 512

static_assert(MAX_WORKERS_PER_DOMAIN >= 1,
              "We should allow at least one worker per domain.");

// The number of seconds that idle threads can hang around before being killed.
#define IDLE_THREAD_TIMEOUT_SEC 30

// The maximum number of threads that can be idle at one time.
#define MAX_IDLE_THREADS 20

#define PREF_WORKERS_PREFIX "dom.workers."
#define PREF_WORKERS_MAX_PER_DOMAIN PREF_WORKERS_PREFIX "maxPerDomain"
#define PREF_WORKERS_MAX_HARDWARE_CONCURRENCY "dom.maxHardwareConcurrency"

#define GC_REQUEST_OBSERVER_TOPIC "child-gc-request"
#define CC_REQUEST_OBSERVER_TOPIC "child-cc-request"
#define MEMORY_PRESSURE_OBSERVER_TOPIC "memory-pressure"
#define LOW_MEMORY_DATA "low-memory"
#define LOW_MEMORY_ONGOING_DATA "low-memory-ongoing"
#define MEMORY_PRESSURE_STOP_OBSERVER_TOPIC "memory-pressure-stop"

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

class LiteralRebindingCString : public nsDependentCString {
 public:
  template <int N>
  void RebindLiteral(const char (&aStr)[N]) {
    Rebind(aStr, N - 1);
  }
};

template <typename T>
struct PrefTraits;

template <>
struct PrefTraits<bool> {
  using PrefValueType = bool;

  static const PrefValueType kDefaultValue = false;

  static inline PrefValueType Get(const char* aPref) {
    AssertIsOnMainThread();
    return Preferences::GetBool(aPref);
  }

  static inline bool Exists(const char* aPref) {
    AssertIsOnMainThread();
    return Preferences::GetType(aPref) == nsIPrefBranch::PREF_BOOL;
  }
};

template <>
struct PrefTraits<int32_t> {
  using PrefValueType = int32_t;

  static inline PrefValueType Get(const char* aPref) {
    AssertIsOnMainThread();
    return Preferences::GetInt(aPref);
  }

  static inline bool Exists(const char* aPref) {
    AssertIsOnMainThread();
    return Preferences::GetType(aPref) == nsIPrefBranch::PREF_INT;
  }
};

template <typename T>
T GetWorkerPref(const nsACString& aPref,
                const T aDefault = PrefTraits<T>::kDefaultValue,
                bool* aPresent = nullptr) {
  AssertIsOnMainThread();

  using PrefHelper = PrefTraits<T>;

  T result;
  bool present = true;

  nsAutoCString prefName;
  prefName.AssignLiteral(PREF_WORKERS_OPTIONS_PREFIX);
  prefName.Append(aPref);

  if (PrefHelper::Exists(prefName.get())) {
    result = PrefHelper::Get(prefName.get());
  } else {
    prefName.AssignLiteral(PREF_JS_OPTIONS_PREFIX);
    prefName.Append(aPref);

    if (PrefHelper::Exists(prefName.get())) {
      result = PrefHelper::Get(prefName.get());
    } else {
      result = aDefault;
      present = false;
    }
  }

  if (aPresent) {
    *aPresent = present;
  }
  return result;
}

// Optimized version for bool that receives already-concatenated pref names.
//
// Used by xpc::SetPrefableContextOptions.
bool GetWorkerBoolPref(const char* jsPref, const char* workerPref) {
  using PrefHelper = PrefTraits<bool>;

  if (PrefHelper::Exists(workerPref)) {
    return PrefHelper::Get(workerPref);
  }

  if (PrefHelper::Exists(jsPref)) {
    return PrefHelper::Get(jsPref);
  }

  return PrefHelper::kDefaultValue;
}

void LoadContextOptions(const char* aPrefName, void* /* aClosure */) {
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();
  if (!rts) {
    // May be shutting down, just bail.
    return;
  }

  const nsDependentCString prefName(aPrefName);

  // Several other pref branches will get included here so bail out if there is
  // another callback that will handle this change.
  if (StringBeginsWith(
          prefName,
          nsLiteralCString(PREF_JS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX)) ||
      StringBeginsWith(
          prefName, nsLiteralCString(
                        PREF_WORKERS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX))) {
    return;
  }

#ifdef JS_GC_ZEAL
  if (prefName.EqualsLiteral(PREF_JS_OPTIONS_PREFIX PREF_GCZEAL) ||
      prefName.EqualsLiteral(PREF_WORKERS_OPTIONS_PREFIX PREF_GCZEAL)) {
    return;
  }
#endif

  JS::ContextOptions contextOptions;
  xpc::SetPrefableContextOptions(contextOptions, GetWorkerBoolPref);

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
void LoadGCZealOptions(const char* /* aPrefName */, void* /* aClosure */) {
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();
  if (!rts) {
    // May be shutting down, just bail.
    return;
  }

  int32_t gczeal = GetWorkerPref<int32_t>(nsLiteralCString(PREF_GCZEAL), -1);
  if (gczeal < 0) {
    gczeal = 0;
  }

  int32_t frequency = GetWorkerPref<int32_t>("gcZeal.frequency"_ns, -1);
  if (frequency < 0) {
    frequency = JS_DEFAULT_ZEAL_FREQ;
  }

  RuntimeService::SetDefaultGCZeal(uint8_t(gczeal), uint32_t(frequency));

  if (rts) {
    rts->UpdateAllWorkerGCZeal();
  }
}
#endif

void UpdateCommonJSGCMemoryOption(RuntimeService* aRuntimeService,
                                  const nsACString& aPrefName,
                                  JSGCParamKey aKey) {
  AssertIsOnMainThread();
  NS_ASSERTION(!aPrefName.IsEmpty(), "Empty pref name!");

  int32_t prefValue = GetWorkerPref(aPrefName, -1);
  Maybe<uint32_t> value = (prefValue < 0 || prefValue >= 10000)
                              ? Nothing()
                              : Some(uint32_t(prefValue));

  RuntimeService::SetDefaultJSGCSettings(aKey, value);

  if (aRuntimeService) {
    aRuntimeService->UpdateAllWorkerMemoryParameter(aKey, value);
  }
}

void UpdateOtherJSGCMemoryOption(RuntimeService* aRuntimeService,
                                 JSGCParamKey aKey, Maybe<uint32_t> aValue) {
  AssertIsOnMainThread();

  RuntimeService::SetDefaultJSGCSettings(aKey, aValue);

  if (aRuntimeService) {
    aRuntimeService->UpdateAllWorkerMemoryParameter(aKey, aValue);
  }
}

void LoadJSGCMemoryOptions(const char* aPrefName, void* /* aClosure */) {
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();

  if (!rts) {
    // May be shutting down, just bail.
    return;
  }

  constexpr auto jsPrefix = nsLiteralCString{PREF_JS_OPTIONS_PREFIX};
  constexpr auto workersPrefix = nsLiteralCString{PREF_WORKERS_OPTIONS_PREFIX};

  const nsDependentCString fullPrefName(aPrefName);

  // Pull out the string that actually distinguishes the parameter we need to
  // change.
  nsDependentCSubstring memPrefName;
  if (StringBeginsWith(fullPrefName, jsPrefix)) {
    memPrefName.Rebind(fullPrefName, jsPrefix.Length());
  } else if (StringBeginsWith(fullPrefName, workersPrefix)) {
    memPrefName.Rebind(fullPrefName, workersPrefix.Length());
  } else {
    NS_ERROR("Unknown pref name!");
    return;
  }

  struct WorkerGCPref {
    nsLiteralCString name;
    JSGCParamKey key;
  };

#define PREF(suffix_, key_) \
  { nsLiteralCString(PREF_MEM_OPTIONS_PREFIX suffix_), key_ }
  constexpr WorkerGCPref kWorkerPrefs[] = {
      PREF("max", JSGC_MAX_BYTES),
      PREF("gc_high_frequency_time_limit_ms", JSGC_HIGH_FREQUENCY_TIME_LIMIT),
      PREF("gc_low_frequency_heap_growth", JSGC_LOW_FREQUENCY_HEAP_GROWTH),
      PREF("gc_high_frequency_large_heap_growth",
           JSGC_HIGH_FREQUENCY_LARGE_HEAP_GROWTH),
      PREF("gc_high_frequency_small_heap_growth",
           JSGC_HIGH_FREQUENCY_SMALL_HEAP_GROWTH),
      PREF("gc_small_heap_size_max_mb", JSGC_SMALL_HEAP_SIZE_MAX),
      PREF("gc_large_heap_size_min_mb", JSGC_LARGE_HEAP_SIZE_MIN),
      PREF("gc_allocation_threshold_mb", JSGC_ALLOCATION_THRESHOLD),
      PREF("gc_malloc_threshold_base_mb", JSGC_MALLOC_THRESHOLD_BASE),
      PREF("gc_small_heap_incremental_limit",
           JSGC_SMALL_HEAP_INCREMENTAL_LIMIT),
      PREF("gc_large_heap_incremental_limit",
           JSGC_LARGE_HEAP_INCREMENTAL_LIMIT),
      PREF("gc_urgent_threshold_mb", JSGC_URGENT_THRESHOLD_MB),
      PREF("gc_incremental_slice_ms", JSGC_SLICE_TIME_BUDGET_MS),
      PREF("gc_min_empty_chunk_count", JSGC_MIN_EMPTY_CHUNK_COUNT),
      PREF("gc_max_empty_chunk_count", JSGC_MAX_EMPTY_CHUNK_COUNT),
      PREF("gc_compacting", JSGC_COMPACTING_ENABLED),
  };
#undef PREF

  auto pref = kWorkerPrefs;
  auto end = kWorkerPrefs + ArrayLength(kWorkerPrefs);

  if (gRuntimeServiceDuringInit) {
    // During init, we want to update every pref in kWorkerPrefs.
    MOZ_ASSERT(memPrefName.EqualsLiteral(PREF_MEM_OPTIONS_PREFIX),
               "Pref branch prefix only expected during init");
  } else {
    // Otherwise, find the single pref that changed.
    while (pref != end) {
      if (pref->name == memPrefName) {
        end = pref + 1;
        break;
      }
      ++pref;
    }
#ifdef DEBUG
    if (pref == end) {
      nsAutoCString message("Workers don't support the '");
      message.Append(memPrefName);
      message.AppendLiteral("' preference!");
      NS_WARNING(message.get());
    }
#endif
  }

  while (pref != end) {
    switch (pref->key) {
      case JSGC_MAX_BYTES: {
        int32_t prefValue = GetWorkerPref(pref->name, -1);
        Maybe<uint32_t> value = (prefValue <= 0 || prefValue >= 0x1000)
                                    ? Nothing()
                                    : Some(uint32_t(prefValue) * 1024 * 1024);
        UpdateOtherJSGCMemoryOption(rts, pref->key, value);
        break;
      }
      case JSGC_SLICE_TIME_BUDGET_MS: {
        int32_t prefValue = GetWorkerPref(pref->name, -1);
        Maybe<uint32_t> value = (prefValue <= 0 || prefValue >= 100000)
                                    ? Nothing()
                                    : Some(uint32_t(prefValue));
        UpdateOtherJSGCMemoryOption(rts, pref->key, value);
        break;
      }
      case JSGC_COMPACTING_ENABLED: {
        bool present;
        bool prefValue = GetWorkerPref(pref->name, false, &present);
        Maybe<uint32_t> value = present ? Some(prefValue ? 1 : 0) : Nothing();
        UpdateOtherJSGCMemoryOption(rts, pref->key, value);
        break;
      }
      case JSGC_HIGH_FREQUENCY_TIME_LIMIT:
      case JSGC_LOW_FREQUENCY_HEAP_GROWTH:
      case JSGC_HIGH_FREQUENCY_LARGE_HEAP_GROWTH:
      case JSGC_HIGH_FREQUENCY_SMALL_HEAP_GROWTH:
      case JSGC_SMALL_HEAP_SIZE_MAX:
      case JSGC_LARGE_HEAP_SIZE_MIN:
      case JSGC_ALLOCATION_THRESHOLD:
      case JSGC_MALLOC_THRESHOLD_BASE:
      case JSGC_SMALL_HEAP_INCREMENTAL_LIMIT:
      case JSGC_LARGE_HEAP_INCREMENTAL_LIMIT:
      case JSGC_URGENT_THRESHOLD_MB:
      case JSGC_MIN_EMPTY_CHUNK_COUNT:
      case JSGC_MAX_EMPTY_CHUNK_COUNT:
        UpdateCommonJSGCMemoryOption(rts, pref->name, pref->key);
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown JSGCParamKey value");
        break;
    }
    ++pref;
  }
}

bool InterruptCallback(JSContext* aCx) {
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  MOZ_ASSERT(worker);

  // Now is a good time to turn on profiling if it's pending.
  PROFILER_JS_INTERRUPT_CALLBACK();

  return worker->InterruptCallback(aCx);
}

class LogViolationDetailsRunnable final : public WorkerMainThreadRunnable {
  nsString mFileName;
  uint32_t mLineNum;
  uint32_t mColumnNum;
  nsString mScriptSample;

 public:
  LogViolationDetailsRunnable(WorkerPrivate* aWorker, const nsString& aFileName,
                              uint32_t aLineNum, uint32_t aColumnNum,
                              const nsAString& aScriptSample)
      : WorkerMainThreadRunnable(aWorker,
                                 "RuntimeService :: LogViolationDetails"_ns),
        mFileName(aFileName),
        mLineNum(aLineNum),
        mColumnNum(aColumnNum),
        mScriptSample(aScriptSample) {
    MOZ_ASSERT(aWorker);
  }

  virtual bool MainThreadRun() override;

 private:
  ~LogViolationDetailsRunnable() = default;
};

bool ContentSecurityPolicyAllows(JSContext* aCx, JS::HandleString aCode) {
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  worker->AssertIsOnWorkerThread();

  nsAutoJSString scriptSample;
  if (NS_WARN_IF(!scriptSample.init(aCx, aCode))) {
    JS_ClearPendingException(aCx);
    return false;
  }

  if (!nsContentSecurityUtils::IsEvalAllowed(aCx, worker->UsesSystemPrincipal(),
                                             scriptSample)) {
    return false;
  }

  if (worker->GetReportCSPViolations()) {
    nsString fileName;
    uint32_t lineNum = 0;
    uint32_t columnNum = 0;

    JS::AutoFilename file;
    if (JS::DescribeScriptedCaller(aCx, &file, &lineNum, &columnNum) &&
        file.get()) {
      CopyUTF8toUTF16(MakeStringSpan(file.get()), fileName);
    } else {
      MOZ_ASSERT(!JS_IsExceptionPending(aCx));
    }

    RefPtr<LogViolationDetailsRunnable> runnable =
        new LogViolationDetailsRunnable(worker, fileName, lineNum, columnNum,
                                        scriptSample);

    ErrorResult rv;
    runnable->Dispatch(Killing, rv);
    if (NS_WARN_IF(rv.Failed())) {
      rv.SuppressException();
    }
  }

  return worker->IsEvalAllowed();
}

void CTypesActivityCallback(JSContext* aCx, JS::CTypesActivityType aType) {
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  worker->AssertIsOnWorkerThread();

  switch (aType) {
    case JS::CTypesActivityType::BeginCall:
      worker->BeginCTypesCall();
      break;

    case JS::CTypesActivityType::EndCall:
      worker->EndCTypesCall();
      break;

    case JS::CTypesActivityType::BeginCallback:
      worker->BeginCTypesCallback();
      break;

    case JS::CTypesActivityType::EndCallback:
      worker->EndCTypesCallback();
      break;

    default:
      MOZ_CRASH("Unknown type flag!");
  }
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
class JSDispatchableRunnable final : public WorkerRunnable {
  JS::Dispatchable* mDispatchable;

  ~JSDispatchableRunnable() { MOZ_ASSERT(!mDispatchable); }

  // Disable the usual pre/post-dispatch thread assertions since we are
  // dispatching from some random JS engine internal thread:

  bool PreDispatch(WorkerPrivate* aWorkerPrivate) override { return true; }

  void PostDispatch(WorkerPrivate* aWorkerPrivate,
                    bool aDispatchResult) override {
    // For the benefit of the destructor assert.
    if (!aDispatchResult) {
      mDispatchable = nullptr;
    }
  }

 public:
  JSDispatchableRunnable(WorkerPrivate* aWorkerPrivate,
                         JS::Dispatchable* aDispatchable)
      : WorkerRunnable(aWorkerPrivate,
                       WorkerRunnable::WorkerThreadUnchangedBusyCount),
        mDispatchable(aDispatchable) {
    MOZ_ASSERT(mDispatchable);
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
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

  nsresult Cancel() override {
    MOZ_ASSERT(mDispatchable);

    AutoJSAPI jsapi;
    jsapi.Init();

    mDispatchable->run(mWorkerPrivate->GetJSContext(),
                       JS::Dispatchable::ShuttingDown);
    mDispatchable = nullptr;  // mDispatchable may delete itself

    return WorkerRunnable::Cancel();
  }
};

static bool DispatchToEventLoop(void* aClosure,
                                JS::Dispatchable* aDispatchable) {
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

static bool ConsumeStream(JSContext* aCx, JS::HandleObject aObj,
                          JS::MimeType aMimeType,
                          JS::StreamConsumer* aConsumer) {
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  if (!worker) {
    JS_ReportErrorNumberASCII(aCx, js::GetErrorMessage, nullptr,
                              JSMSG_WASM_ERROR_CONSUMING_RESPONSE);
    return false;
  }

  return FetchUtil::StreamResponseToJS(aCx, aObj, aMimeType, aConsumer, worker);
}

bool InitJSContextForWorker(WorkerPrivate* aWorkerPrivate,
                            JSContext* aWorkerCx) {
  aWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(!aWorkerPrivate->GetJSContext(), "Already has a context!");

  JSSettings settings;
  aWorkerPrivate->CopyJSSettings(settings);

  JS::ContextOptionsRef(aWorkerCx) = settings.contextOptions;

  // This is the real place where we set the max memory for the runtime.
  for (const auto& setting : settings.gcSettings) {
    if (setting.value) {
      JS_SetGCParameter(aWorkerCx, setting.key, *setting.value);
    } else {
      JS_ResetGCParameter(aWorkerCx, setting.key);
    }
  }

  JS_SetNativeStackQuota(aWorkerCx, WORKER_CONTEXT_NATIVE_STACK_LIMIT);

  // Security policy:
  static const JSSecurityCallbacks securityCallbacks = {
      ContentSecurityPolicyAllows};
  JS_SetSecurityCallbacks(aWorkerCx, &securityCallbacks);

  // A WorkerPrivate lives strictly longer than its JSRuntime so we can safely
  // store a raw pointer as the callback's closure argument on the JSRuntime.
  JS::InitDispatchToEventLoop(aWorkerCx, DispatchToEventLoop,
                              (void*)aWorkerPrivate);

  JS::InitConsumeStreamCallback(aWorkerCx, ConsumeStream,
                                FetchUtil::ReportJSStreamError);

  // When available, set the self-hosted shared memory to be read, so that we
  // can decode the self-hosted content instead of parsing it.
  auto& shm = xpc::SelfHostedShmem::GetSingleton();
  JS::SelfHostedCache selfHostedContent = shm.Content();

  if (!JS::InitSelfHostedCode(aWorkerCx, selfHostedContent)) {
    NS_WARNING("Could not init self-hosted code!");
    return false;
  }

  JS_AddInterruptCallback(aWorkerCx, InterruptCallback);

  JS::SetCTypesActivityCallback(aWorkerCx, CTypesActivityCallback);

#ifdef JS_GC_ZEAL
  JS_SetGCZeal(aWorkerCx, settings.gcZeal, settings.gcZealFrequency);
#endif

  return true;
}

static bool PreserveWrapper(JSContext* cx, JS::HandleObject obj) {
  MOZ_ASSERT(cx);
  MOZ_ASSERT(obj);
  MOZ_ASSERT(mozilla::dom::IsDOMObject(obj));

  return mozilla::dom::TryPreserveWrapper(obj);
}

static bool IsWorkerDebuggerGlobalOrSandbox(JS::HandleObject aGlobal) {
  return IsWorkerDebuggerGlobal(aGlobal) || IsWorkerDebuggerSandbox(aGlobal);
}

JSObject* Wrap(JSContext* cx, JS::HandleObject existing, JS::HandleObject obj) {
  JS::RootedObject targetGlobal(cx, JS::CurrentGlobalOrNull(cx));

  // Note: the JS engine unwraps CCWs before calling this callback.
  JS::RootedObject originGlobal(cx, JS::GetNonCCWObjectGlobal(obj));

  const js::Wrapper* wrapper = nullptr;
  if (IsWorkerDebuggerGlobalOrSandbox(targetGlobal) &&
      IsWorkerDebuggerGlobalOrSandbox(originGlobal)) {
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

class WorkerJSRuntime final : public mozilla::CycleCollectedJSRuntime {
 public:
  // The heap size passed here doesn't matter, we will change it later in the
  // call to JS_SetGCParameter inside InitJSContextForWorker.
  explicit WorkerJSRuntime(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
      : CycleCollectedJSRuntime(aCx), mWorkerPrivate(aWorkerPrivate) {
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

  void Shutdown(JSContext* cx) override {
    // The CC is shut down, and the superclass destructor will GC, so make sure
    // we don't try to CC again.
    mWorkerPrivate = nullptr;

    CycleCollectedJSRuntime::Shutdown(cx);
  }

  ~WorkerJSRuntime() {
    MOZ_COUNT_DTOR_INHERITED(WorkerJSRuntime, CycleCollectedJSRuntime);
  }

  virtual void PrepareForForgetSkippable() override {}

  virtual void BeginCycleCollectionCallback(
      mozilla::CCReason aReason) override {}

  virtual void EndCycleCollectionCallback(
      CycleCollectorResults& aResults) override {}

  void DispatchDeferredDeletion(bool aContinuation, bool aPurge) override {
    MOZ_ASSERT(!aContinuation);

    // Do it immediately, no need for asynchronous behavior here.
    nsCycleCollector_doDeferredDeletion();
  }

  virtual void CustomGCCallback(JSGCStatus aStatus) override {
    if (!mWorkerPrivate) {
      // We're shutting down, no need to do anything.
      return;
    }

    mWorkerPrivate->AssertIsOnWorkerThread();

    if (aStatus == JSGC_END) {
      bool collectedAnything =
          nsCycleCollector_collect(CCReason::GC_FINISHED, nullptr);
      mWorkerPrivate->SetCCCollectedAnything(collectedAnything);
    }
  }

 private:
  WorkerPrivate* mWorkerPrivate;
};

}  // anonymous namespace

}  // namespace workerinternals

class WorkerJSContext final : public mozilla::CycleCollectedJSContext {
 public:
  // The heap size passed here doesn't matter, we will change it later in the
  // call to JS_SetGCParameter inside InitJSContextForWorker.
  explicit WorkerJSContext(WorkerPrivate* aWorkerPrivate)
      : mWorkerPrivate(aWorkerPrivate) {
    MOZ_COUNT_CTOR_INHERITED(WorkerJSContext, CycleCollectedJSContext);
    MOZ_ASSERT(aWorkerPrivate);
    // Magical number 2. Workers have the base recursion depth 1, and normal
    // runnables run at level 2, and we don't want to process microtasks
    // at any other level.
    SetTargetedMicroTaskRecursionDepth(2);
  }

  // MOZ_CAN_RUN_SCRIPT_BOUNDARY because otherwise we have to annotate the
  // SpiderMonkey JS::JobQueue's destructor as MOZ_CAN_RUN_SCRIPT, which is a
  // bit of a pain.
  MOZ_CAN_RUN_SCRIPT_BOUNDARY ~WorkerJSContext() {
    MOZ_COUNT_DTOR_INHERITED(WorkerJSContext, CycleCollectedJSContext);
    JSContext* cx = MaybeContext();
    if (!cx) {
      return;  // Initialize() must have failed
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

  CycleCollectedJSRuntime* CreateRuntime(JSContext* aCx) override {
    return new WorkerJSRuntime(aCx, mWorkerPrivate);
  }

  nsresult Initialize(JSRuntime* aParentRuntime) {
    nsresult rv = CycleCollectedJSContext::Initialize(
        aParentRuntime, WORKER_DEFAULT_RUNTIME_HEAPSIZE);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    JSContext* cx = Context();

    js::SetPreserveWrapperCallbacks(cx, PreserveWrapper, HasReleasedWrapper);
    JS_InitDestroyPrincipalsCallback(cx, WorkerPrincipal::Destroy);
    JS_SetWrapObjectCallbacks(cx, &WrapObjectCallbacks);
    if (mWorkerPrivate->IsDedicatedWorker()) {
      JS_SetFutexCanWait(cx);
    }

    return NS_OK;
  }

  virtual void DispatchToMicroTask(
      already_AddRefed<MicroTaskRunnable> aRunnable) override {
    RefPtr<MicroTaskRunnable> runnable(aRunnable);

    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(runnable);

    std::deque<RefPtr<MicroTaskRunnable>>* microTaskQueue = nullptr;

    JSContext* cx = Context();
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

    JS::JobQueueMayNotBeEmpty(cx);
    microTaskQueue->push_back(std::move(runnable));
  }

  bool IsSystemCaller() const override {
    return mWorkerPrivate->UsesSystemPrincipal();
  }

  void ReportError(JSErrorReport* aReport,
                   JS::ConstUTF8CharsZ aToStringResult) override {
    mWorkerPrivate->ReportError(Context(), aToStringResult, aReport);
  }

  WorkerPrivate* GetWorkerPrivate() const { return mWorkerPrivate; }

 private:
  WorkerPrivate* mWorkerPrivate;
};

namespace workerinternals {

namespace {

class WorkerThreadPrimaryRunnable final : public Runnable {
  WorkerPrivate* mWorkerPrivate;
  SafeRefPtr<WorkerThread> mThread;
  JSRuntime* mParentRuntime;

  class FinishedRunnable final : public Runnable {
    SafeRefPtr<WorkerThread> mThread;

   public:
    explicit FinishedRunnable(SafeRefPtr<WorkerThread> aThread)
        : Runnable("WorkerThreadPrimaryRunnable::FinishedRunnable"),
          mThread(std::move(aThread)) {
      MOZ_ASSERT(mThread);
    }

    NS_INLINE_DECL_REFCOUNTING_INHERITED(FinishedRunnable, Runnable)

   private:
    ~FinishedRunnable() = default;

    NS_DECL_NSIRUNNABLE
  };

 public:
  WorkerThreadPrimaryRunnable(WorkerPrivate* aWorkerPrivate,
                              SafeRefPtr<WorkerThread> aThread,
                              JSRuntime* aParentRuntime)
      : mozilla::Runnable("WorkerThreadPrimaryRunnable"),
        mWorkerPrivate(aWorkerPrivate),
        mThread(std::move(aThread)),
        mParentRuntime(aParentRuntime) {
    MOZ_ASSERT(aWorkerPrivate);
    MOZ_ASSERT(mThread);
  }

  NS_INLINE_DECL_REFCOUNTING_INHERITED(WorkerThreadPrimaryRunnable, Runnable)

 private:
  ~WorkerThreadPrimaryRunnable() = default;

  NS_DECL_NSIRUNNABLE
};

void PrefLanguagesChanged(const char* /* aPrefName */, void* /* aClosure */) {
  AssertIsOnMainThread();

  nsTArray<nsString> languages;
  Navigator::GetAcceptLanguages(languages);

  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->UpdateAllWorkerLanguages(languages);
  }
}

void AppNameOverrideChanged(const char* /* aPrefName */, void* /* aClosure */) {
  AssertIsOnMainThread();

  nsAutoString override;
  Preferences::GetString("general.appname.override", override);

  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->UpdateAppNameOverridePreference(override);
  }
}

void AppVersionOverrideChanged(const char* /* aPrefName */,
                               void* /* aClosure */) {
  AssertIsOnMainThread();

  nsAutoString override;
  Preferences::GetString("general.appversion.override", override);

  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->UpdateAppVersionOverridePreference(override);
  }
}

void PlatformOverrideChanged(const char* /* aPrefName */,
                             void* /* aClosure */) {
  AssertIsOnMainThread();

  nsAutoString override;
  Preferences::GetString("general.platform.override", override);

  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->UpdatePlatformOverridePreference(override);
  }
}

} /* anonymous namespace */

// This is only touched on the main thread. Initialized in Init() below.
UniquePtr<JSSettings> RuntimeService::sDefaultJSSettings;

RuntimeService::RuntimeService()
    : mMutex("RuntimeService::mMutex"),
      mObserved(false),
      mShuttingDown(false),
      mNavigatorPropertiesLoaded(false) {
  AssertIsOnMainThread();
  NS_ASSERTION(!gRuntimeService, "More than one service!");
}

RuntimeService::~RuntimeService() {
  AssertIsOnMainThread();

  // gRuntimeService can be null if Init() fails.
  NS_ASSERTION(!gRuntimeService || gRuntimeService == this,
               "More than one service!");

  gRuntimeService = nullptr;
}

// static
RuntimeService* RuntimeService::GetOrCreateService() {
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
RuntimeService* RuntimeService::GetService() { return gRuntimeService; }

bool RuntimeService::RegisterWorker(WorkerPrivate& aWorkerPrivate) {
  aWorkerPrivate.AssertIsOnParentThread();

  WorkerPrivate* parent = aWorkerPrivate.GetParent();
  if (!parent) {
    AssertIsOnMainThread();

    if (mShuttingDown) {
      return false;
    }
  }

  const bool isServiceWorker = aWorkerPrivate.IsServiceWorker();
  const bool isSharedWorker = aWorkerPrivate.IsSharedWorker();
  const bool isDedicatedWorker = aWorkerPrivate.IsDedicatedWorker();
  if (isServiceWorker) {
    AssertIsOnMainThread();
    Telemetry::Accumulate(Telemetry::SERVICE_WORKER_SPAWN_ATTEMPTS, 1);
  }

  nsCString sharedWorkerScriptSpec;
  if (isSharedWorker) {
    AssertIsOnMainThread();

    nsCOMPtr<nsIURI> scriptURI = aWorkerPrivate.GetResolvedScriptURI();
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
    exemptFromPerDomainMax = Preferences::GetBool(
        "dom.serviceWorkers.exemptFromPerDomainMax", false);
  }

  const nsCString& domain = aWorkerPrivate.Domain();

  bool queued = false;
  {
    MutexAutoLock lock(mMutex);

    auto* const domainInfo =
        mDomainMap
            .LookupOrInsertWith(
                domain,
                [&domain, parent] {
                  NS_ASSERTION(!parent, "Shouldn't have a parent here!");
                  Unused << parent;  // silence clang -Wunused-lambda-capture in
                                     // opt builds
                  auto wdi = MakeUnique<WorkerDomainInfo>();
                  wdi->mDomain = domain;
                  return wdi;
                })
            .get();

    queued = gMaxWorkersPerDomain &&
             domainInfo->ActiveWorkerCount() >= gMaxWorkersPerDomain &&
             !domain.IsEmpty() && !exemptFromPerDomainMax;

    if (queued) {
      domainInfo->mQueuedWorkers.AppendElement(&aWorkerPrivate);

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
    } else if (parent) {
      domainInfo->mChildWorkerCount++;
    } else if (isServiceWorker) {
      domainInfo->mActiveServiceWorkers.AppendElement(&aWorkerPrivate);
    } else {
      domainInfo->mActiveWorkers.AppendElement(&aWorkerPrivate);
    }
  }

  // From here on out we must call UnregisterWorker if something fails!
  if (parent) {
    if (!parent->AddChildWorker(&aWorkerPrivate)) {
      UnregisterWorker(aWorkerPrivate);
      return false;
    }
  } else {
    if (!mNavigatorPropertiesLoaded) {
      Navigator::AppName(mNavigatorProperties.mAppName,
                         aWorkerPrivate.GetPrincipal(),
                         false /* aUsePrefOverriddenValue */);
      if (NS_FAILED(Navigator::GetAppVersion(
              mNavigatorProperties.mAppVersion, aWorkerPrivate.GetPrincipal(),
              false /* aUsePrefOverriddenValue */)) ||
          NS_FAILED(Navigator::GetPlatform(
              mNavigatorProperties.mPlatform, aWorkerPrivate.GetPrincipal(),
              false /* aUsePrefOverriddenValue */))) {
        UnregisterWorker(aWorkerPrivate);
        return false;
      }

      // The navigator overridden properties should have already been read.

      Navigator::GetAcceptLanguages(mNavigatorProperties.mLanguages);
      mNavigatorPropertiesLoaded = true;
    }

    nsPIDOMWindowInner* window = aWorkerPrivate.GetWindow();

    if (!isServiceWorker) {
      // Service workers are excluded since their lifetime is separate from
      // that of dom windows.
      if (auto* const windowArray = mWindowMap.GetOrInsertNew(window, 1);
          !windowArray->Contains(&aWorkerPrivate)) {
        windowArray->AppendElement(&aWorkerPrivate);
      } else {
        MOZ_ASSERT(aWorkerPrivate.IsSharedWorker());
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

void RuntimeService::UnregisterWorker(WorkerPrivate& aWorkerPrivate) {
  aWorkerPrivate.AssertIsOnParentThread();

  WorkerPrivate* parent = aWorkerPrivate.GetParent();
  if (!parent) {
    AssertIsOnMainThread();
  }

  const nsCString& domain = aWorkerPrivate.Domain();

  WorkerPrivate* queuedWorker = nullptr;
  {
    MutexAutoLock lock(mMutex);

    WorkerDomainInfo* domainInfo;
    if (!mDomainMap.Get(domain, &domainInfo)) {
      NS_ERROR("Don't have an entry for this domain!");
    }

    // Remove old worker from everywhere.
    uint32_t index = domainInfo->mQueuedWorkers.IndexOf(&aWorkerPrivate);
    if (index != kNoIndex) {
      // Was queued, remove from the list.
      domainInfo->mQueuedWorkers.RemoveElementAt(index);
    } else if (parent) {
      MOZ_ASSERT(domainInfo->mChildWorkerCount, "Must be non-zero!");
      domainInfo->mChildWorkerCount--;
    } else if (aWorkerPrivate.IsServiceWorker()) {
      MOZ_ASSERT(domainInfo->mActiveServiceWorkers.Contains(&aWorkerPrivate),
                 "Don't know about this worker!");
      domainInfo->mActiveServiceWorkers.RemoveElement(&aWorkerPrivate);
    } else {
      MOZ_ASSERT(domainInfo->mActiveWorkers.Contains(&aWorkerPrivate),
                 "Don't know about this worker!");
      domainInfo->mActiveWorkers.RemoveElement(&aWorkerPrivate);
    }

    // See if there's a queued worker we can schedule.
    if (domainInfo->ActiveWorkerCount() < gMaxWorkersPerDomain &&
        !domainInfo->mQueuedWorkers.IsEmpty()) {
      queuedWorker = domainInfo->mQueuedWorkers[0];
      domainInfo->mQueuedWorkers.RemoveElementAt(0);

      if (queuedWorker->GetParent()) {
        domainInfo->mChildWorkerCount++;
      } else if (queuedWorker->IsServiceWorker()) {
        domainInfo->mActiveServiceWorkers.AppendElement(queuedWorker);
      } else {
        domainInfo->mActiveWorkers.AppendElement(queuedWorker);
      }
    }

    if (domainInfo->HasNoWorkers()) {
      MOZ_ASSERT(domainInfo->mQueuedWorkers.IsEmpty());
      mDomainMap.Remove(domain);
    }
  }

  if (aWorkerPrivate.IsServiceWorker()) {
    AssertIsOnMainThread();
    Telemetry::AccumulateTimeDelta(Telemetry::SERVICE_WORKER_LIFE_TIME,
                                   aWorkerPrivate.CreationTimeStamp());
  }

  // NB: For Shared Workers we used to call ShutdownOnMainThread on the
  // RemoteWorkerController; however, that was redundant because
  // RemoteWorkerChild uses a WeakWorkerRef which notifies at about the
  // same time as us calling into the code here and would race with us.

  if (parent) {
    parent->RemoveChildWorker(&aWorkerPrivate);
  } else if (aWorkerPrivate.IsSharedWorker()) {
    AssertIsOnMainThread();

    mWindowMap.RemoveIf([&aWorkerPrivate](const auto& iter) {
      const auto& workers = iter.Data();
      MOZ_ASSERT(workers);

      if (workers->RemoveElement(&aWorkerPrivate)) {
        MOZ_ASSERT(!workers->Contains(&aWorkerPrivate),
                   "Added worker more than once!");

        return workers->IsEmpty();
      }

      return false;
    });
  } else if (aWorkerPrivate.IsDedicatedWorker()) {
    // May be null.
    nsPIDOMWindowInner* window = aWorkerPrivate.GetWindow();
    if (auto entry = mWindowMap.Lookup(window)) {
      MOZ_ALWAYS_TRUE(entry.Data()->RemoveElement(&aWorkerPrivate));
      if (entry.Data()->IsEmpty()) {
        entry.Remove();
      }
    } else {
      MOZ_ASSERT_UNREACHABLE("window is not in mWindowMap");
    }
  }

  if (queuedWorker && !ScheduleWorker(*queuedWorker)) {
    UnregisterWorker(*queuedWorker);
  }
}

bool RuntimeService::ScheduleWorker(WorkerPrivate& aWorkerPrivate) {
  if (!aWorkerPrivate.Start()) {
    // This is ok, means that we didn't need to make a thread for this worker.
    return true;
  }

  SafeRefPtr<WorkerThread> thread;
  {
    MutexAutoLock lock(mMutex);
    if (!mIdleThreadArray.IsEmpty()) {
      thread = std::move(mIdleThreadArray.PopLastElement().mThread);
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

  if (NS_FAILED(thread->SetPriority(nsISupportsPriority::PRIORITY_NORMAL))) {
    NS_WARNING("Could not set the thread's priority!");
  }

  aWorkerPrivate.SetThread(thread.unsafeGetRawPtr());
  JSContext* cx = CycleCollectedJSContext::Get()->Context();
  nsCOMPtr<nsIRunnable> runnable = new WorkerThreadPrimaryRunnable(
      &aWorkerPrivate, thread.clonePtr(), JS_GetParentRuntime(cx));
  if (NS_FAILED(
          thread->DispatchPrimaryRunnable(friendKey, runnable.forget()))) {
    UnregisterWorker(aWorkerPrivate);
    return false;
  }

  return true;
}

// static
void RuntimeService::ShutdownIdleThreads(nsITimer* aTimer,
                                         void* /* aClosure */) {
  AssertIsOnMainThread();

  RuntimeService* runtime = RuntimeService::GetService();
  NS_ASSERTION(runtime, "This should never be null!");

  NS_ASSERTION(aTimer == runtime->mIdleThreadTimer, "Wrong timer!");

  // Cheat a little and grab all threads that expire within one second of now.
  const TimeStamp now = TimeStamp::NowLoRes() + TimeDuration::FromSeconds(1);

  TimeStamp nextExpiration;

  AutoTArray<SafeRefPtr<WorkerThread>, 20> expiredThreads;
  {
    MutexAutoLock lock(runtime->mMutex);

    for (auto& info : runtime->mIdleThreadArray) {
      if (info.mExpirationTime > now) {
        nextExpiration = info.mExpirationTime;
        break;
      }

      expiredThreads.AppendElement(std::move(info.mThread));
    }

    runtime->mIdleThreadArray.RemoveElementsAt(0, expiredThreads.Length());
  }

  if (!nextExpiration.IsNull()) {
    const TimeDuration delta = nextExpiration - TimeStamp::NowLoRes();
    const uint32_t delay = delta > TimeDuration{} ? delta.ToMilliseconds() : 0;

    // Reschedule the timer.
    MOZ_ALWAYS_SUCCEEDS(aTimer->InitWithNamedFuncCallback(
        ShutdownIdleThreads, nullptr, delay, nsITimer::TYPE_ONE_SHOT,
        "RuntimeService::ShutdownIdleThreads"));
  }

  for (const auto& expiredThread : expiredThreads) {
    if (NS_FAILED(expiredThread->Shutdown())) {
      NS_WARNING("Failed to shutdown thread!");
    }
  }
}

nsresult RuntimeService::Init() {
  AssertIsOnMainThread();

  nsLayoutStatics::AddRef();

  // Initialize JSSettings.
  sDefaultJSSettings = MakeUnique<JSSettings>();
  SetDefaultJSGCSettings(JSGC_MAX_BYTES, Some(WORKER_DEFAULT_RUNTIME_HEAPSIZE));
  SetDefaultJSGCSettings(JSGC_ALLOCATION_THRESHOLD,
                         Some(WORKER_DEFAULT_ALLOCATION_THRESHOLD));

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

  if (NS_FAILED(
          obs->AddObserver(this, MEMORY_PRESSURE_OBSERVER_TOPIC, false))) {
    NS_WARNING("Failed to register for memory pressure notifications!");
  }

  if (NS_FAILED(
          obs->AddObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC, false))) {
    NS_WARNING("Failed to register for offline notification event!");
  }

  MOZ_ASSERT(!gRuntimeServiceDuringInit, "This should be false!");
  gRuntimeServiceDuringInit = true;

#define WORKER_PREF(name, callback) \
  NS_FAILED(Preferences::RegisterCallbackAndCall(callback, name))

  if (NS_FAILED(Preferences::RegisterPrefixCallback(
          LoadJSGCMemoryOptions,
          PREF_JS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX)) ||
      NS_FAILED(Preferences::RegisterPrefixCallbackAndCall(
          LoadJSGCMemoryOptions,
          PREF_WORKERS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX)) ||
#ifdef JS_GC_ZEAL
      NS_FAILED(Preferences::RegisterCallback(
          LoadGCZealOptions, PREF_JS_OPTIONS_PREFIX PREF_GCZEAL)) ||
#endif
      WORKER_PREF("intl.accept_languages", PrefLanguagesChanged) ||
      WORKER_PREF("general.appname.override", AppNameOverrideChanged) ||
      WORKER_PREF("general.appversion.override", AppVersionOverrideChanged) ||
      WORKER_PREF("general.platform.override", PlatformOverrideChanged) ||
#ifdef JS_GC_ZEAL
      WORKER_PREF("dom.workers.options.gcZeal", LoadGCZealOptions) ||
#endif
      NS_FAILED(Preferences::RegisterPrefixCallbackAndCall(
          LoadContextOptions, PREF_WORKERS_OPTIONS_PREFIX)) ||
      NS_FAILED(Preferences::RegisterPrefixCallback(LoadContextOptions,
                                                    PREF_JS_OPTIONS_PREFIX))) {
    NS_WARNING("Failed to register pref callbacks!");
  }

#undef WORKER_PREF

  MOZ_ASSERT(gRuntimeServiceDuringInit, "Should be true!");
  gRuntimeServiceDuringInit = false;

  int32_t maxPerDomain =
      Preferences::GetInt(PREF_WORKERS_MAX_PER_DOMAIN, MAX_WORKERS_PER_DOMAIN);
  gMaxWorkersPerDomain = std::max(0, maxPerDomain);

  int32_t maxHardwareConcurrency = Preferences::GetInt(
      PREF_WORKERS_MAX_HARDWARE_CONCURRENCY, MAX_HARDWARE_CONCURRENCY);
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

void RuntimeService::Shutdown() {
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
    AutoTArray<WorkerPrivate*, 100> workers;

    {
      MutexAutoLock lock(mMutex);

      AddAllTopLevelWorkersToArray(workers);
    }

    // Cancel all top-level workers.
    for (const auto& worker : workers) {
      if (!worker->Cancel()) {
        NS_WARNING("Failed to cancel worker!");
      }
    }
  }

  sDefaultJSSettings = nullptr;
}

namespace {

class CrashIfHangingRunnable : public WorkerControlRunnable {
 public:
  explicit CrashIfHangingRunnable(WorkerPrivate* aWorkerPrivate)
      : WorkerControlRunnable(aWorkerPrivate, WorkerThreadUnchangedBusyCount),
        mMonitor("CrashIfHangingRunnable::mMonitor") {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    MonitorAutoLock lock(mMonitor);
    if (!mHasMsg) {
      aWorkerPrivate->DumpCrashInformation(mMsg);
      mHasMsg.Flip();
    }
    lock.Notify();
    return true;
  }

  nsresult Cancel() override {
    MonitorAutoLock lock(mMonitor);
    if (!mHasMsg) {
      mMsg.Assign("Canceled");
      mHasMsg.Flip();
    }
    lock.Notify();

    return NS_OK;
  }

  bool DispatchAndWait() {
    MonitorAutoLock lock(mMonitor);

    if (!Dispatch()) {
      // The worker is already dead but the main thread still didn't remove it
      // from RuntimeService's registry.
      return false;
    }

    // To avoid any possibility of process hangs we never receive reports on
    // we give the worker 1sec to react.
    lock.Wait(TimeDuration::FromMilliseconds(1000));
    if (!mHasMsg) {
      mMsg.Append("NoResponse");
      mHasMsg.Flip();
    }
    return true;
  }

  const nsCString& MsgData() const { return mMsg; }

 private:
  bool PreDispatch(WorkerPrivate* aWorkerPrivate) override { return true; }

  void PostDispatch(WorkerPrivate* aWorkerPrivate,
                    bool aDispatchResult) override {}

  Monitor mMonitor;
  nsCString mMsg;
  FlippedOnce<false> mHasMsg;
};

struct ActiveWorkerStats {
  template <uint32_t ActiveWorkerStats::*Category>
  void Update(const nsTArray<WorkerPrivate*>& aWorkers) {
    for (const auto worker : aWorkers) {
      RefPtr<CrashIfHangingRunnable> runnable =
          new CrashIfHangingRunnable(worker);
      if (runnable->DispatchAndWait()) {
        ++(this->*Category);

        // BC: Busy Count
        mMessage.AppendPrintf("-BC:%d", worker->BusyCount());
        mMessage.Append(runnable->MsgData());
      } else {
        mMessage.AppendPrintf("-BC:%d DispatchFailed", worker->BusyCount());
      }
    }
  }

  uint32_t mWorkers = 0;
  uint32_t mServiceWorkers = 0;
  nsCString mMessage;
};

}  // namespace

void RuntimeService::CrashIfHanging() {
  MutexAutoLock lock(mMutex);

  ActiveWorkerStats activeStats;
  uint32_t inactiveWorkers = 0;

  for (const auto& aData : mDomainMap.Values()) {
    activeStats.Update<&ActiveWorkerStats::mWorkers>(aData->mActiveWorkers);
    activeStats.Update<&ActiveWorkerStats::mServiceWorkers>(
        aData->mActiveServiceWorkers);

    // These might not be top-level workers...
    inactiveWorkers += std::count_if(
        aData->mQueuedWorkers.begin(), aData->mQueuedWorkers.end(),
        [](const auto* const worker) { return !worker->GetParent(); });
  }

  if (activeStats.mWorkers + activeStats.mServiceWorkers + inactiveWorkers ==
      0) {
    return;
  }

  nsCString msg;

  // A: active Workers | S: active ServiceWorkers | Q: queued Workers
  msg.AppendPrintf("Workers Hanging - %d|A:%d|S:%d|Q:%d", mShuttingDown ? 1 : 0,
                   activeStats.mWorkers, activeStats.mServiceWorkers,
                   inactiveWorkers);
  msg.Append(activeStats.mMessage);

  // This string will be leaked.
  MOZ_CRASH_UNSAFE(strdup(msg.BeginReading()));
}

// This spins the event loop until all workers are finished and their threads
// have been joined.
void RuntimeService::Cleanup() {
  AssertIsOnMainThread();

  if (!mShuttingDown) {
    Shutdown();
  }

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
        AutoTArray<SafeRefPtr<WorkerThread>, 20> idleThreads;
        idleThreads.SetCapacity(mIdleThreadArray.Length());

#ifdef DEBUG
        const bool anyNullThread = std::any_of(
            mIdleThreadArray.begin(), mIdleThreadArray.end(),
            [](const auto& entry) { return entry.mThread == nullptr; });
        MOZ_ASSERT(!anyNullThread);
#endif

        std::transform(mIdleThreadArray.begin(), mIdleThreadArray.end(),
                       MakeBackInserter(idleThreads),
                       [](auto& entry) { return std::move(entry.mThread); });

        mIdleThreadArray.Clear();

        MutexAutoUnlock unlock(mMutex);

        for (const auto& idleThread : idleThreads) {
          if (NS_FAILED(idleThread->Shutdown())) {
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

#define WORKER_PREF(name, callback) \
  NS_FAILED(Preferences::UnregisterCallback(callback, name))

  if (mObserved) {
    if (NS_FAILED(Preferences::UnregisterPrefixCallback(
            LoadContextOptions, PREF_JS_OPTIONS_PREFIX)) ||
        NS_FAILED(Preferences::UnregisterPrefixCallback(
            LoadContextOptions, PREF_WORKERS_OPTIONS_PREFIX)) ||
        WORKER_PREF("intl.accept_languages", PrefLanguagesChanged) ||
        WORKER_PREF("general.appname.override", AppNameOverrideChanged) ||
        WORKER_PREF("general.appversion.override", AppVersionOverrideChanged) ||
        WORKER_PREF("general.platform.override", PlatformOverrideChanged) ||
#ifdef JS_GC_ZEAL
        WORKER_PREF("dom.workers.options.gcZeal", LoadGCZealOptions) ||
        NS_FAILED(Preferences::UnregisterCallback(
            LoadGCZealOptions, PREF_JS_OPTIONS_PREFIX PREF_GCZEAL)) ||
#endif
        NS_FAILED(Preferences::UnregisterPrefixCallback(
            LoadJSGCMemoryOptions,
            PREF_JS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX)) ||
        NS_FAILED(Preferences::UnregisterPrefixCallback(
            LoadJSGCMemoryOptions,
            PREF_WORKERS_OPTIONS_PREFIX PREF_MEM_OPTIONS_PREFIX))) {
      NS_WARNING("Failed to unregister pref callbacks!");
    }

#undef WORKER_PREF

    if (obs) {
      if (NS_FAILED(obs->RemoveObserver(this, GC_REQUEST_OBSERVER_TOPIC))) {
        NS_WARNING("Failed to unregister for GC request notifications!");
      }

      if (NS_FAILED(obs->RemoveObserver(this, CC_REQUEST_OBSERVER_TOPIC))) {
        NS_WARNING("Failed to unregister for CC request notifications!");
      }

      if (NS_FAILED(
              obs->RemoveObserver(this, MEMORY_PRESSURE_OBSERVER_TOPIC))) {
        NS_WARNING("Failed to unregister for memory pressure notifications!");
      }

      if (NS_FAILED(
              obs->RemoveObserver(this, NS_IOSERVICE_OFFLINE_STATUS_TOPIC))) {
        NS_WARNING("Failed to unregister for offline notification event!");
      }
      obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID);
      obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
      mObserved = false;
    }
  }

  nsLayoutStatics::Release();
}

void RuntimeService::AddAllTopLevelWorkersToArray(
    nsTArray<WorkerPrivate*>& aWorkers) {
  for (const auto& aData : mDomainMap.Values()) {
#ifdef DEBUG
    for (const auto& activeWorker : aData->mActiveWorkers) {
      MOZ_ASSERT(!activeWorker->GetParent(),
                 "Shouldn't have a parent in this list!");
    }
    for (const auto& activeServiceWorker : aData->mActiveServiceWorkers) {
      MOZ_ASSERT(!activeServiceWorker->GetParent(),
                 "Shouldn't have a parent in this list!");
    }
#endif

    aWorkers.AppendElements(aData->mActiveWorkers);
    aWorkers.AppendElements(aData->mActiveServiceWorkers);

    // These might not be top-level workers...
    std::copy_if(aData->mQueuedWorkers.begin(), aData->mQueuedWorkers.end(),
                 MakeBackInserter(aWorkers),
                 [](const auto& worker) { return !worker->GetParent(); });
  }
}

nsTArray<WorkerPrivate*> RuntimeService::GetWorkersForWindow(
    const nsPIDOMWindowInner& aWindow) const {
  AssertIsOnMainThread();

  nsTArray<WorkerPrivate*> result;
  if (nsTArray<WorkerPrivate*>* const workers = mWindowMap.Get(&aWindow)) {
    NS_ASSERTION(!workers->IsEmpty(), "Should have been removed!");
    result.AppendElements(*workers);
  }
  return result;
}

void RuntimeService::CancelWorkersForWindow(const nsPIDOMWindowInner& aWindow) {
  AssertIsOnMainThread();

  for (WorkerPrivate* const worker : GetWorkersForWindow(aWindow)) {
    MOZ_ASSERT(!worker->IsSharedWorker());
    worker->Cancel();
  }
}

void RuntimeService::FreezeWorkersForWindow(const nsPIDOMWindowInner& aWindow) {
  AssertIsOnMainThread();

  for (WorkerPrivate* const worker : GetWorkersForWindow(aWindow)) {
    MOZ_ASSERT(!worker->IsSharedWorker());
    worker->Freeze(&aWindow);
  }
}

void RuntimeService::ThawWorkersForWindow(const nsPIDOMWindowInner& aWindow) {
  AssertIsOnMainThread();

  for (WorkerPrivate* const worker : GetWorkersForWindow(aWindow)) {
    MOZ_ASSERT(!worker->IsSharedWorker());
    worker->Thaw(&aWindow);
  }
}

void RuntimeService::SuspendWorkersForWindow(
    const nsPIDOMWindowInner& aWindow) {
  AssertIsOnMainThread();

  for (WorkerPrivate* const worker : GetWorkersForWindow(aWindow)) {
    MOZ_ASSERT(!worker->IsSharedWorker());
    worker->ParentWindowPaused();
  }
}

void RuntimeService::ResumeWorkersForWindow(const nsPIDOMWindowInner& aWindow) {
  AssertIsOnMainThread();

  for (WorkerPrivate* const worker : GetWorkersForWindow(aWindow)) {
    MOZ_ASSERT(!worker->IsSharedWorker());
    worker->ParentWindowResumed();
  }
}

void RuntimeService::PropagateStorageAccessPermissionGranted(
    const nsPIDOMWindowInner& aWindow) {
  AssertIsOnMainThread();
  MOZ_ASSERT_IF(aWindow.GetExtantDoc(), aWindow.GetExtantDoc()
                                            ->CookieJarSettings()
                                            ->GetRejectThirdPartyContexts());

  for (WorkerPrivate* const worker : GetWorkersForWindow(aWindow)) {
    worker->PropagateStorageAccessPermissionGranted();
  }
}

void RuntimeService::NoteIdleThread(SafeRefPtr<WorkerThread> aThread) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aThread);

  bool shutdownThread = mShuttingDown;
  bool scheduleTimer = false;

  if (!shutdownThread) {
    static TimeDuration timeout =
        TimeDuration::FromSeconds(IDLE_THREAD_TIMEOUT_SEC);

    const TimeStamp expirationTime = TimeStamp::NowLoRes() + timeout;

    MutexAutoLock lock(mMutex);

    const uint32_t previousIdleCount = mIdleThreadArray.Length();

    if (previousIdleCount < MAX_IDLE_THREADS) {
      IdleThreadInfo* const info = mIdleThreadArray.AppendElement();
      info->mThread = std::move(aThread);
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
    MOZ_ALWAYS_SUCCEEDS(mIdleThreadTimer->InitWithNamedFuncCallback(
        ShutdownIdleThreads, nullptr, IDLE_THREAD_TIMEOUT_SEC * 1000,
        nsITimer::TYPE_ONE_SHOT, "RuntimeService::ShutdownIdleThreads"));
  }
}

template <typename Func>
void RuntimeService::BroadcastAllWorkers(const Func& aFunc) {
  AssertIsOnMainThread();

  AutoTArray<WorkerPrivate*, 100> workers;
  {
    MutexAutoLock lock(mMutex);

    AddAllTopLevelWorkersToArray(workers);
  }

  for (const auto& worker : workers) {
    aFunc(*worker);
  }
}

void RuntimeService::UpdateAllWorkerContextOptions() {
  BroadcastAllWorkers([](auto& worker) {
    worker.UpdateContextOptions(sDefaultJSSettings->contextOptions);
  });
}

void RuntimeService::UpdateAppNameOverridePreference(const nsAString& aValue) {
  AssertIsOnMainThread();
  mNavigatorProperties.mAppNameOverridden = aValue;
}

void RuntimeService::UpdateAppVersionOverridePreference(
    const nsAString& aValue) {
  AssertIsOnMainThread();
  mNavigatorProperties.mAppVersionOverridden = aValue;
}

void RuntimeService::UpdatePlatformOverridePreference(const nsAString& aValue) {
  AssertIsOnMainThread();
  mNavigatorProperties.mPlatformOverridden = aValue;
}

void RuntimeService::UpdateAllWorkerLanguages(
    const nsTArray<nsString>& aLanguages) {
  MOZ_ASSERT(NS_IsMainThread());

  mNavigatorProperties.mLanguages = aLanguages.Clone();
  BroadcastAllWorkers(
      [&aLanguages](auto& worker) { worker.UpdateLanguages(aLanguages); });
}

void RuntimeService::UpdateAllWorkerMemoryParameter(JSGCParamKey aKey,
                                                    Maybe<uint32_t> aValue) {
  BroadcastAllWorkers([aKey, aValue](auto& worker) {
    worker.UpdateJSWorkerMemoryParameter(aKey, aValue);
  });
}

#ifdef JS_GC_ZEAL
void RuntimeService::UpdateAllWorkerGCZeal() {
  BroadcastAllWorkers([](auto& worker) {
    worker.UpdateGCZeal(sDefaultJSSettings->gcZeal,
                        sDefaultJSSettings->gcZealFrequency);
  });
}
#endif

void RuntimeService::SetLowMemoryStateAllWorkers(bool aState) {
  BroadcastAllWorkers(
      [aState](auto& worker) { worker.SetLowMemoryState(aState); });
}

void RuntimeService::GarbageCollectAllWorkers(bool aShrinking) {
  BroadcastAllWorkers(
      [aShrinking](auto& worker) { worker.GarbageCollect(aShrinking); });
}

void RuntimeService::CycleCollectAllWorkers() {
  BroadcastAllWorkers([](auto& worker) { worker.CycleCollect(); });
}

void RuntimeService::SendOfflineStatusChangeEventToAllWorkers(bool aIsOffline) {
  BroadcastAllWorkers([aIsOffline](auto& worker) {
    worker.OfflineStatusChangeEvent(aIsOffline);
  });
}

void RuntimeService::MemoryPressureAllWorkers() {
  BroadcastAllWorkers([](auto& worker) { worker.MemoryPressure(); });
}

uint32_t RuntimeService::ClampedHardwareConcurrency() const {
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
    int32_t numberOfProcessors = 0;
#if defined(XP_MACOSX)
    if (nsMacUtilsImpl::IsTCSMAvailable()) {
      // On failure, zero is returned from GetPhysicalCPUCount()
      // and we fallback to PR_GetNumberOfProcessors below.
      numberOfProcessors = nsMacUtilsImpl::GetPhysicalCPUCount();
    }
#endif
    if (numberOfProcessors == 0) {
      numberOfProcessors = PR_GetNumberOfProcessors();
    }
    if (numberOfProcessors <= 0) {
      numberOfProcessors = 1;  // Must be one there somewhere
    }
    uint32_t clampedValue =
        std::min(uint32_t(numberOfProcessors), gMaxHardwareConcurrency);
    Unused << clampedHardwareConcurrency.compareExchange(0, clampedValue);
  }

  return clampedHardwareConcurrency;
}

// nsISupports
NS_IMPL_ISUPPORTS(RuntimeService, nsIObserver)

// nsIObserver
NS_IMETHODIMP
RuntimeService::Observe(nsISupports* aSubject, const char* aTopic,
                        const char16_t* aData) {
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
    nsDependentString data(aData);
    // Don't continue to GC/CC if we are in an ongoing low-memory state since
    // its very slow and it likely won't help us anyway.
    if (data.EqualsLiteral(LOW_MEMORY_ONGOING_DATA)) {
      return NS_OK;
    }
    if (data.EqualsLiteral(LOW_MEMORY_DATA)) {
      SetLowMemoryStateAllWorkers(true);
    }
    GarbageCollectAllWorkers(/* shrinking = */ true);
    CycleCollectAllWorkers();
    MemoryPressureAllWorkers();
    return NS_OK;
  }
  if (!strcmp(aTopic, MEMORY_PRESSURE_STOP_OBSERVER_TOPIC)) {
    SetLowMemoryStateAllWorkers(false);
    return NS_OK;
  }
  if (!strcmp(aTopic, NS_IOSERVICE_OFFLINE_STATUS_TOPIC)) {
    SendOfflineStatusChangeEventToAllWorkers(NS_IsOffline());
    return NS_OK;
  }

  MOZ_ASSERT_UNREACHABLE("Unknown observer topic!");
  return NS_OK;
}

bool LogViolationDetailsRunnable::MainThreadRun() {
  AssertIsOnMainThread();

  nsIContentSecurityPolicy* csp = mWorkerPrivate->GetCSP();
  if (csp) {
    if (mWorkerPrivate->GetReportCSPViolations()) {
      csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                               nullptr,  // triggering element
                               mWorkerPrivate->CSPEventListener(), mFileName,
                               mScriptSample, mLineNum, mColumnNum, u""_ns,
                               u""_ns);
    }
  }

  return true;
}

// MOZ_CAN_RUN_SCRIPT_BOUNDARY until Runnable::Run is MOZ_CAN_RUN_SCRIPT.  See
// bug 1535398.
MOZ_CAN_RUN_SCRIPT_BOUNDARY
NS_IMETHODIMP
WorkerThreadPrimaryRunnable::Run() {
  AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
      "WorkerThreadPrimaryRunnable::Run", OTHER, mWorkerPrivate->ScriptURL());

  using mozilla::ipc::BackgroundChild;

  {
    auto failureCleanup = MakeScopeExit([&]() {
      // The creation of threadHelper above is the point at which a worker is
      // considered to have run, because the `mPreStartRunnables` are all
      // re-dispatched after `mThread` is set.  We need to let the WorkerPrivate
      // know so it can clean up the various event loops and delete the worker.
      mWorkerPrivate->RunLoopNeverRan();
    });

    mWorkerPrivate->SetWorkerPrivateInWorkerThread(mThread.unsafeGetRawPtr());

    const auto threadCleanup = MakeScopeExit([&] {
      // This must be called before ScheduleDeletion, which is either called
      // from failureCleanup leaving scope, or from the outer scope.
      mWorkerPrivate->ResetWorkerPrivateInWorkerThread();
    });

    mWorkerPrivate->AssertIsOnWorkerThread();

    // This needs to be initialized on the worker thread before being used on
    // the main thread and calling BackgroundChild::GetOrCreateForCurrentThread
    // exposes it to the main thread.
    mWorkerPrivate->EnsurePerformanceStorage();

    if (NS_WARN_IF(!BackgroundChild::GetOrCreateForCurrentThread())) {
      return NS_ERROR_FAILURE;
    }

    {
      nsCycleCollector_startup();

      auto context = MakeUnique<WorkerJSContext>(mWorkerPrivate);
      nsresult rv = context->Initialize(mParentRuntime);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      JSContext* cx = context->Context();

      if (!InitJSContextForWorker(mWorkerPrivate, cx)) {
        return NS_ERROR_FAILURE;
      }

      failureCleanup.release();

      {
        PROFILER_SET_JS_CONTEXT(cx);

        {
          // We're on the worker thread here, and WorkerPrivate's refcounting is
          // non-threadsafe: you can only do it on the parent thread.  What that
          // means in practice is that we're relying on it being kept alive
          // while we run.  Hopefully.
          MOZ_KnownLive(mWorkerPrivate)->DoRunLoop(cx);
          // The AutoJSAPI in DoRunLoop should have reported any exceptions left
          // on cx.
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
      JS_GC(cx, JS::GCReason::WORKER_SHUTDOWN);

      // Before shutting down the cycle collector we need to do one more pass
      // through the event loop to clean up any C++ objects that need deferred
      // cleanup.
      mWorkerPrivate->ClearMainEventQueue(WorkerPrivate::WorkerRan);

      // Now WorkerJSContext goes out of scope and its destructor will shut
      // down the cycle collector. This breaks any remaining cycles and collects
      // any remaining C++ objects.
    }
  }

  mWorkerPrivate->ScheduleDeletion(WorkerPrivate::WorkerRan);

  // It is no longer safe to touch mWorkerPrivate.
  mWorkerPrivate = nullptr;

  // Now recycle this thread.
  nsCOMPtr<nsIEventTarget> mainTarget = GetMainThreadEventTarget();
  MOZ_ASSERT(mainTarget);

  RefPtr<FinishedRunnable> finishedRunnable =
      new FinishedRunnable(std::move(mThread));
  MOZ_ALWAYS_SUCCEEDS(
      mainTarget->Dispatch(finishedRunnable, NS_DISPATCH_NORMAL));

  return NS_OK;
}

NS_IMETHODIMP
WorkerThreadPrimaryRunnable::FinishedRunnable::Run() {
  AssertIsOnMainThread();

  SafeRefPtr<WorkerThread> thread = std::move(mThread);

  RuntimeService* rts = RuntimeService::GetService();
  if (rts) {
    rts->NoteIdleThread(std::move(thread));
  } else if (thread->ShutdownRequired()) {
    MOZ_ALWAYS_SUCCEEDS(thread->Shutdown());
  }

  return NS_OK;
}

}  // namespace workerinternals

void CancelWorkersForWindow(const nsPIDOMWindowInner& aWindow) {
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->CancelWorkersForWindow(aWindow);
  }
}

void FreezeWorkersForWindow(const nsPIDOMWindowInner& aWindow) {
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->FreezeWorkersForWindow(aWindow);
  }
}

void ThawWorkersForWindow(const nsPIDOMWindowInner& aWindow) {
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->ThawWorkersForWindow(aWindow);
  }
}

void SuspendWorkersForWindow(const nsPIDOMWindowInner& aWindow) {
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->SuspendWorkersForWindow(aWindow);
  }
}

void ResumeWorkersForWindow(const nsPIDOMWindowInner& aWindow) {
  AssertIsOnMainThread();
  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->ResumeWorkersForWindow(aWindow);
  }
}

void PropagateStorageAccessPermissionGrantedToWorkers(
    const nsPIDOMWindowInner& aWindow) {
  AssertIsOnMainThread();
  MOZ_ASSERT_IF(aWindow.GetExtantDoc(), aWindow.GetExtantDoc()
                                            ->CookieJarSettings()
                                            ->GetRejectThirdPartyContexts());

  RuntimeService* runtime = RuntimeService::GetService();
  if (runtime) {
    runtime->PropagateStorageAccessPermissionGranted(aWindow);
  }
}

WorkerPrivate* GetWorkerPrivateFromContext(JSContext* aCx) {
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

WorkerPrivate* GetCurrentThreadWorkerPrivate() {
  if (NS_IsMainThread()) {
    return nullptr;
  }

  CycleCollectedJSContext* ccjscx = CycleCollectedJSContext::Get();
  if (!ccjscx) {
    return nullptr;
  }

  WorkerJSContext* workerjscx = ccjscx->GetAsWorkerJSContext();
  // Even when GetCurrentThreadWorkerPrivate() is called on worker
  // threads, the ccjscx will no longer be a WorkerJSContext if called from
  // stable state events during ~CycleCollectedJSContext().
  if (!workerjscx) {
    return nullptr;
  }

  return workerjscx->GetWorkerPrivate();
}

bool IsCurrentThreadRunningWorker() {
  return !NS_IsMainThread() && !!GetCurrentThreadWorkerPrivate();
}

bool IsCurrentThreadRunningChromeWorker() {
  WorkerPrivate* wp = GetCurrentThreadWorkerPrivate();
  return wp && wp->UsesSystemPrincipal();
}

JSContext* GetCurrentWorkerThreadJSContext() {
  WorkerPrivate* wp = GetCurrentThreadWorkerPrivate();
  if (!wp) {
    return nullptr;
  }
  return wp->GetJSContext();
}

JSObject* GetCurrentThreadWorkerGlobal() {
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

JSObject* GetCurrentThreadWorkerDebuggerGlobal() {
  WorkerPrivate* wp = GetCurrentThreadWorkerPrivate();
  if (!wp) {
    return nullptr;
  }
  WorkerDebuggerGlobalScope* scope = wp->DebuggerGlobalScope();
  if (!scope) {
    return nullptr;
  }
  return scope->GetGlobalJSObject();
}

}  // namespace dom
}  // namespace mozilla
