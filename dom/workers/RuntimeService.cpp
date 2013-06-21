/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RuntimeService.h"

#include "nsIContentSecurityPolicy.h"
#include "nsIDOMChromeWindow.h"
#include "nsIEffectiveTLDService.h"
#include "nsIObserverService.h"
#include "nsIPlatformCharset.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupportsPriority.h"
#include "nsITimer.h"
#include "nsPIDOMWindow.h"

#include <algorithm>
#include "GeckoProfiler.h"
#include "jsdbgapi.h"
#include "jsfriendapi.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Preferences.h"
#include "mozilla/Util.h"
#include <Navigator.h>
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsLayoutStatics.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"
#include "OSFileConstants.h"
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

// The size of the worker JS allocation threshold in MB. May be changed via pref.
#define WORKER_DEFAULT_ALLOCATION_THRESHOLD 30

// The C stack size. We use the same stack size on all platforms for
// consistency.
#define WORKER_STACK_SIZE 256 * sizeof(size_t) * 1024

// Half the size of the actual C stack, to be safe.
#define WORKER_CONTEXT_NATIVE_STACK_LIMIT 128 * sizeof(size_t) * 1024

// The maximum number of threads to use for workers, overridable via pref.
#define MAX_WORKERS_PER_DOMAIN 10

MOZ_STATIC_ASSERT(MAX_WORKERS_PER_DOMAIN >= 1,
                  "We should allow at least one worker per domain.");

// The default number of seconds that close handlers will be allowed to run for
// content workers.
#define MAX_SCRIPT_RUN_TIME_SEC 10

// The number of seconds that idle threads can hang around before being killed.
#define IDLE_THREAD_TIMEOUT_SEC 30

// The maximum number of threads that can be idle at one time.
#define MAX_IDLE_THREADS 20

#define PREF_WORKERS_PREFIX "dom.workers."
#define PREF_WORKERS_ENABLED PREF_WORKERS_PREFIX "enabled"
#define PREF_WORKERS_MAX_PER_DOMAIN PREF_WORKERS_PREFIX "maxPerDomain"

#define PREF_MAX_SCRIPT_RUN_TIME_CONTENT "dom.max_script_run_time"
#define PREF_MAX_SCRIPT_RUN_TIME_CHROME "dom.max_chrome_script_run_time"

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

namespace {

const uint32_t kNoIndex = uint32_t(-1);

const uint32_t kRequiredJSContextOptions =
  JSOPTION_DONT_REPORT_UNCAUGHT | JSOPTION_NO_SCRIPT_RVAL;

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
  "WorkerEvent",
  "WorkerMessageEvent",
  "WorkerErrorEvent"

  // XXX Don't care about ProgressEvent since it should never leak to the main
  // thread.
};

MOZ_STATIC_ASSERT(NS_ARRAY_LENGTH(gStringChars) == ID_COUNT,
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

  static const PrefValueType kDefaultValue = 0;

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

int
LoadJSContextOptions(const char* aPrefName, void* /* aClosure */)
{
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();
  if (!rts && !gRuntimeServiceDuringInit) {
    // May be shutting down, just bail.
    return 0;
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
    return 0;
  }

#ifdef JS_GC_ZEAL
  if (prefName.EqualsLiteral(PREF_JS_OPTIONS_PREFIX PREF_GCZEAL) ||
      prefName.EqualsLiteral(PREF_WORKERS_OPTIONS_PREFIX PREF_GCZEAL)) {
    return 0;
  }
#endif

  // Common options.
  uint32_t commonOptions = kRequiredJSContextOptions;
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("strict"))) {
    commonOptions |= JSOPTION_EXTRA_WARNINGS;
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("werror"))) {
    commonOptions |= JSOPTION_WERROR;
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("typeinference"))) {
    commonOptions |= JSOPTION_TYPE_INFERENCE;
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("asmjs"))) {
    commonOptions |= JSOPTION_ASMJS;
  }

  // Content options.
  uint32_t contentOptions = commonOptions;
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("pccounts.content"))) {
    contentOptions |= JSOPTION_PCCOUNT;
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("baselinejit.content"))) {
    contentOptions |= JSOPTION_BASELINE;
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("ion.content"))) {
    contentOptions |= JSOPTION_ION;
  }

  // Chrome options.
  uint32_t chromeOptions = commonOptions;
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("pccounts.chrome"))) {
    chromeOptions |= JSOPTION_PCCOUNT;
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("baselinejit.chrome"))) {
    chromeOptions |= JSOPTION_BASELINE;
  }
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("ion.chrome"))) {
    chromeOptions |= JSOPTION_ION;
  }
#ifdef DEBUG
  if (GetWorkerPref<bool>(NS_LITERAL_CSTRING("strict.debug"))) {
    chromeOptions |= JSOPTION_EXTRA_WARNINGS;
  }
#endif

  RuntimeService::SetDefaultJSContextOptions(contentOptions, chromeOptions);

  if (rts) {
    rts->UpdateAllWorkerJSContextOptions();
  }

  return 0;
}

#ifdef JS_GC_ZEAL
int
LoadGCZealOptions(const char* /* aPrefName */, void* /* aClosure */)
{
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();
  if (!rts && !gRuntimeServiceDuringInit) {
    // May be shutting down, just bail.
    return 0;
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

  return 0;
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
    (prefValue <= 0 || prefValue >= 10000) ? 0 : uint32_t(prefValue);

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


int
LoadJSGCMemoryOptions(const char* aPrefName, void* /* aClosure */)
{
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();

  if (!rts && !gRuntimeServiceDuringInit) {
    // May be shutting down, just bail.
    return 0;
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
    return 0;
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

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "analysis_purge_mb");
    if (memPrefName == matchName || (!rts && index == 8)) {
      UpdateCommonJSGCMemoryOption(rts, matchName, JSGC_ANALYSIS_PURGE_TRIGGER);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX
                            "gc_allocation_threshold_mb");
    if (memPrefName == matchName || (!rts && index == 9)) {
      UpdateCommonJSGCMemoryOption(rts, matchName, JSGC_ALLOCATION_THRESHOLD);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "gc_incremental_slice_ms");
    if (memPrefName == matchName || (!rts && index == 10)) {
      int32_t prefValue = GetWorkerPref(matchName, -1);
      uint32_t value =
        (prefValue <= 0 || prefValue >= 100000) ? 0 : uint32_t(prefValue);
      UpdatOtherJSGCMemoryOption(rts, JSGC_SLICE_TIME_BUDGET, value);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "gc_dynamic_heap_growth");
    if (memPrefName == matchName || (!rts && index == 11)) {
      bool prefValue = GetWorkerPref(matchName, false);
      UpdatOtherJSGCMemoryOption(rts, JSGC_DYNAMIC_HEAP_GROWTH,
                                 prefValue ? 0 : 1);
      continue;
    }

    matchName.RebindLiteral(PREF_MEM_OPTIONS_PREFIX "gc_dynamic_mark_slice");
    if (memPrefName == matchName || (!rts && index == 12)) {
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

  return 0;
}

int
LoadJITHardeningOption(const char* /* aPrefName */, void* /* aClosure */)
{
  AssertIsOnMainThread();

  RuntimeService* rts = RuntimeService::GetService();

  if (!rts && !gRuntimeServiceDuringInit) {
    // May be shutting down, just bail.
    return 0;
  }

  bool value = GetWorkerPref(NS_LITERAL_CSTRING(PREF_JIT_HARDENING), false);

  RuntimeService::SetDefaultJITHardening(value);

  if (rts) {
    rts->UpdateAllWorkerJITHardening(value);
  }

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
      csp->LogViolationDetails(nsIContentSecurityPolicy::VIOLATION_TYPE_EVAL,
                               mFileName, scriptSample, mLineNum);
    }

    nsRefPtr<LogViolationDetailsResponseRunnable> response =
        new LogViolationDetailsResponseRunnable(mWorkerPrivate, mSyncQueueKey);
    if (!response->Dispatch(nullptr)) {
      NS_WARNING("Failed to dispatch response!");
    }

    return NS_OK;
  }
};

JSBool
ContentSecurityPolicyAllows(JSContext* aCx)
{
  WorkerPrivate* worker = GetWorkerPrivateFromContext(aCx);
  worker->AssertIsOnWorkerThread();

  if (worker->GetReportCSPViolations()) {
    nsString fileName;
    uint32_t lineNum = 0;

    JSScript* script;
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
      MOZ_NOT_REACHED("Unknown type flag!");
  }
}

JSContext*
CreateJSContextForWorker(WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnWorkerThread();
  NS_ASSERTION(!aWorkerPrivate->GetJSContext(), "Already has a context!");

  // The number passed here doesn't matter, we're about to change it in the call
  // to JS_SetGCParameter.
  JSRuntime* runtime =
    JS_NewRuntime(WORKER_DEFAULT_RUNTIME_HEAPSIZE, JS_NO_HELPER_THREADS);
  if (!runtime) {
    NS_WARNING("Could not create new runtime!");
    return nullptr;
  }

  JSSettings settings;
  aWorkerPrivate->CopyJSSettings(settings);

  NS_ASSERTION((settings.chrome.options & kRequiredJSContextOptions) ==
               kRequiredJSContextOptions,
               "Somehow we lost our required chrome options!");
  NS_ASSERTION((settings.content.options & kRequiredJSContextOptions) ==
               kRequiredJSContextOptions,
               "Somehow we lost our required content options!");

  JSSettings::JSGCSettingsArray& gcSettings = settings.gcSettings;

  // This is the real place where we set the max memory for the runtime.
  for (uint32_t index = 0; index < ArrayLength(gcSettings); index++) {
    const JSSettings::JSGCSetting& setting = gcSettings[index];
    if (setting.IsSet()) {
      NS_ASSERTION(setting.value, "Can't handle 0 values!");
      JS_SetGCParameter(runtime, setting.key, setting.value);
    }
  }

  JS_SetNativeStackQuota(runtime, WORKER_CONTEXT_NATIVE_STACK_LIMIT);

  // Security policy:
  static JSSecurityCallbacks securityCallbacks = {
    NULL,
    ContentSecurityPolicyAllows
  };
  JS_SetSecurityCallbacks(runtime, &securityCallbacks);

  // DOM helpers:
  static js::DOMCallbacks DOMCallbacks = {
    InstanceClassHasProtoAtDepth
  };
  SetDOMCallbacks(runtime, &DOMCallbacks);

  JSContext* workerCx = JS_NewContext(runtime, 0);
  if (!workerCx) {
    JS_DestroyRuntime(runtime);
    NS_WARNING("Could not create new context!");
    return nullptr;
  }

  JS_SetRuntimePrivate(runtime, aWorkerPrivate);

  JS_SetErrorReporter(workerCx, ErrorReporter);

  JS_SetOperationCallback(workerCx, OperationCallback);

  js::SetCTypesActivityCallback(runtime, CTypesActivityCallback);

  JS_SetOptions(workerCx,
                aWorkerPrivate->IsChromeWorker() ? settings.chrome.options :
                                                   settings.content.options);

  JS_SetJitHardening(runtime, settings.jitHardening);

#ifdef JS_GC_ZEAL
  JS_SetGCZeal(workerCx, settings.gcZeal, settings.gcZealFrequency);
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
    mWorkerPrivate = nullptr;

    workerPrivate->AssertIsOnWorkerThread();

    JSContext* cx = CreateJSContextForWorker(workerPrivate);
    if (!cx) {
      // XXX need to fire an error at parent.
      NS_ERROR("Failed to create runtime and context!");
      return NS_ERROR_FAILURE;
    }

    JSRuntime* rt = JS_GetRuntime(cx);

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

#ifdef MOZ_ENABLE_PROFILER_SPS
    if (PseudoStack* stack = mozilla_get_pseudo_stack())
      stack->sampleRuntime(nullptr);
#endif
    JS_DestroyRuntime(rt);

    workerPrivate->ScheduleDeletion(false);
    profiler_unregister_thread();
    return NS_OK;
  }
};

} /* anonymous namespace */

BEGIN_WORKERS_NAMESPACE

// Entry point for the DOM.
JSBool
ResolveWorkerClasses(JSContext* aCx, JS::Handle<JSObject*> aObj, JS::Handle<jsid> aId,
                     unsigned aFlags, JS::MutableHandle<JSObject*> aObjp)
{
  AssertIsOnMainThread();

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

  bool isChrome = false;
  bool shouldResolve = false;

  for (uint32_t i = 0; i < ID_COUNT; i++) {
    if (gStringIDs[i] == aId) {
      isChrome = nsContentUtils::IsCallerChrome();

      // Don't resolve if this is ChromeWorker and we're not chrome. Otherwise
      // always resolve.
      shouldResolve = gStringIDs[ID_ChromeWorker] == aId ? isChrome : true;
      break;
    }
  }

  if (shouldResolve) {
    // Don't do anything if workers are disabled.
    if (!isChrome && !Preferences::GetBool(PREF_WORKERS_ENABLED)) {
      aObjp.set(nullptr);
      return true;
    }

    JSObject* eventTarget = EventTargetBinding_workers::GetProtoObject(aCx, aObj);
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

    aObjp.set(aObj);
    return true;
  }

  // Not resolved.
  aObjp.set(nullptr);
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
ResumeWorkersForWindow(nsIScriptContext* aCx, nsPIDOMWindow* aWindow)
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
  runnable->Dispatch(nullptr);
  return true;
}

END_WORKERS_NAMESPACE

// This is only touched on the main thread. Initialized in Init() below.
JSSettings RuntimeService::sDefaultJSSettings;

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

  WorkerPrivate* queuedWorker = nullptr;
  {
    const nsCString& domain = aWorkerPrivate->Domain();

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
    sDefaultJSSettings.chrome.options = kRequiredJSContextOptions;
    sDefaultJSSettings.chrome.maxScriptRuntime = -1;
    sDefaultJSSettings.content.options = kRequiredJSContextOptions;
    sDefaultJSSettings.content.maxScriptRuntime = MAX_SCRIPT_RUN_TIME_SEC;
#ifdef JS_GC_ZEAL
    sDefaultJSSettings.gcZealFrequency = JS_DEFAULT_ZEAL_FREQ;
    sDefaultJSSettings.gcZeal = 0;
#endif
    SetDefaultJSGCSettings(JSGC_MAX_BYTES, WORKER_DEFAULT_RUNTIME_HEAPSIZE);
    SetDefaultJSGCSettings(JSGC_ALLOCATION_THRESHOLD,
                           WORKER_DEFAULT_ALLOCATION_THRESHOLD);
  }

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
      NS_FAILED(Preferences::RegisterCallback(LoadJSContextOptions,
                                              PREF_JS_OPTIONS_PREFIX,
                                              nullptr)) ||
      NS_FAILED(Preferences::RegisterCallbackAndCall(
                                                    LoadJSContextOptions,
                                                    PREF_WORKERS_OPTIONS_PREFIX,
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

// This spins the event loop until all workers are finished and their threads
// have been joined.
void
RuntimeService::Cleanup()
{
  AssertIsOnMainThread();

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_WARN_IF_FALSE(obs, "Failed to get observer service?!");

  // Tell anyone that cares that they're about to lose worker support.
  if (obs && NS_FAILED(obs->NotifyObservers(nullptr, WORKERS_SHUTDOWN_TOPIC,
                                            nullptr))) {
    NS_WARNING("NotifyObservers failed!");
  }

  // That's it, no more workers.
  mShuttingDown = true;

  if (mIdleThreadTimer) {
    if (NS_FAILED(mIdleThreadTimer->Cancel())) {
      NS_WARNING("Failed to cancel idle timer!");
    }
    mIdleThreadTimer = nullptr;
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
        JSAutoRequest ar(cx);

        for (uint32_t index = 0; index < workers.Length(); index++) {
          if (!workers[index]->Kill(cx)) {
            NS_WARNING("Failed to cancel worker!");
          }
        }
      }

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

  if (mWindowMap.IsInitialized()) {
    NS_ASSERTION(!mWindowMap.Count(), "All windows should have been released!");
  }

  if (mObserved) {
    if (NS_FAILED(Preferences::UnregisterCallback(LoadJSContextOptions,
                                                  PREF_JS_OPTIONS_PREFIX,
                                                  nullptr)) ||
        NS_FAILED(Preferences::UnregisterCallback(LoadJSContextOptions,
                                                  PREF_WORKERS_OPTIONS_PREFIX,
                                                  nullptr)) ||
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

      if (NS_FAILED(obs->RemoveObserver(this,
                                        MEMORY_PRESSURE_OBSERVER_TOPIC))) {
        NS_WARNING("Failed to unregister for memory pressure notifications!");
      }

      nsresult rv =
        obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID);
      mObserved = NS_FAILED(rv);
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
    for (uint32_t index = 0; index < workers.Length(); index++) {
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
    for (uint32_t index = 0; index < workers.Length(); index++) {
      if (!workers[index]->Suspend(aCx)) {
        NS_WARNING("Failed to cancel worker!");
      }
    }
  }
}

void
RuntimeService::ResumeWorkersForWindow(nsIScriptContext* aCx,
                                       nsPIDOMWindow* aWindow)
{
  AssertIsOnMainThread();

  nsAutoTArray<WorkerPrivate*, 100> workers;
  GetWorkersForWindow(aWindow, workers);

  if (!workers.IsEmpty()) {
    for (uint32_t index = 0; index < workers.Length(); index++) {
      if (!workers[index]->SynchronizeAndResume(aCx)) {
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
                        sDefaultJSSettings.content.options,
                        sDefaultJSSettings.chrome.options);
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
