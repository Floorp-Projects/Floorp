/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/HelperThreads.h"

#include "mozilla/Maybe.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "builtin/Promise.h"
#include "frontend/BytecodeCompilation.h"
#include "gc/GCInternals.h"
#include "jit/IonBuilder.h"
#include "js/ContextOptions.h"  // JS::ContextOptions
#include "js/SourceText.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "threading/CpuCount.h"
#include "util/NativeStack.h"
#include "vm/ErrorReporting.h"
#include "vm/SharedImmutableStringsCache.h"
#include "vm/Time.h"
#include "vm/TraceLogging.h"
#include "vm/Xdr.h"
#include "wasm/WasmGenerator.h"

#include "debugger/DebugAPI-inl.h"
#include "gc/PrivateIterators-inl.h"
#include "vm/JSContext-inl.h"
#include "vm/JSObject-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/Realm-inl.h"

using namespace js;

using mozilla::Maybe;
using mozilla::TimeDuration;
using mozilla::TimeStamp;
using mozilla::Unused;
using mozilla::Utf8Unit;

using JS::CompileOptions;
using JS::ReadOnlyCompileOptions;

namespace js {

GlobalHelperThreadState* gHelperThreadState = nullptr;

}  // namespace js

// These macros are identical in function to the same-named ones in
// GeckoProfiler.h, but they are defined separately because SpiderMonkey can't
// use GeckoProfiler.h.
#define PROFILER_RAII_PASTE(id, line) id##line
#define PROFILER_RAII_EXPAND(id, line) PROFILER_RAII_PASTE(id, line)
#define PROFILER_RAII PROFILER_RAII_EXPAND(raiiObject, __LINE__)
#define AUTO_PROFILER_LABEL(label, categoryPair) \
  HelperThread::AutoProfilerLabel PROFILER_RAII( \
      this, label, JS::ProfilingCategoryPair::categoryPair)

bool js::CreateHelperThreadsState() {
  MOZ_ASSERT(!gHelperThreadState);
  UniquePtr<GlobalHelperThreadState> helperThreadState =
      MakeUnique<GlobalHelperThreadState>();
  if (!helperThreadState) {
    return false;
  }
  gHelperThreadState = helperThreadState.release();
  if (!gHelperThreadState->ensureContextListForThreadCount()) {
    js_delete(gHelperThreadState);
    return false;
  }
  return true;
}

void js::DestroyHelperThreadsState() {
  MOZ_ASSERT(gHelperThreadState);
  gHelperThreadState->finish();
  js_delete(gHelperThreadState);
  gHelperThreadState = nullptr;
}

bool js::EnsureHelperThreadsInitialized() {
  MOZ_ASSERT(gHelperThreadState);
  return gHelperThreadState->ensureInitialized();
}

static size_t ClampDefaultCPUCount(size_t cpuCount) {
  // It's extremely rare for SpiderMonkey to have more than a few cores worth
  // of work. At higher core counts, performance can even decrease due to NUMA
  // (and SpiderMonkey's lack of NUMA-awareness), contention, and general lack
  // of optimization for high core counts. So to avoid wasting thread stack
  // resources (and cluttering gdb and core dumps), clamp to 8 cores for now.
  return Min<size_t>(cpuCount, 8);
}

static size_t ThreadCountForCPUCount(size_t cpuCount) {
  // We need at least two threads for tier-2 wasm compilations, because
  // there's a master task that holds a thread while other threads do the
  // compilation.
  return Max<size_t>(cpuCount, 2);
}

bool js::SetFakeCPUCount(size_t count) {
  // This must be called before the threads have been initialized.
  MOZ_ASSERT(!HelperThreadState().threads);

  HelperThreadState().cpuCount = count;
  HelperThreadState().threadCount = ThreadCountForCPUCount(count);

  if (!HelperThreadState().ensureContextListForThreadCount()) {
    return false;
  }
  return true;
}

void JS::SetProfilingThreadCallbacks(
    JS::RegisterThreadCallback registerThread,
    JS::UnregisterThreadCallback unregisterThread) {
  HelperThreadState().registerThread = registerThread;
  HelperThreadState().unregisterThread = unregisterThread;
}

bool js::StartOffThreadWasmCompile(wasm::CompileTask* task,
                                   wasm::CompileMode mode) {
  AutoLockHelperThreadState lock;

  if (!HelperThreadState().wasmWorklist(lock, mode).pushBack(task)) {
    return false;
  }

  HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);
  return true;
}

void js::StartOffThreadWasmTier2Generator(wasm::UniqueTier2GeneratorTask task) {
  MOZ_ASSERT(CanUseExtraThreads());

  AutoLockHelperThreadState lock;

  if (!HelperThreadState().wasmTier2GeneratorWorklist(lock).append(
          task.get())) {
    return;
  }

  Unused << task.release();

  HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);
}

static void CancelOffThreadWasmTier2GeneratorLocked(
    AutoLockHelperThreadState& lock) {
  if (!HelperThreadState().threads) {
    return;
  }

  // Remove pending tasks from the tier2 generator worklist and cancel and
  // delete them.
  {
    wasm::Tier2GeneratorTaskPtrVector& worklist =
        HelperThreadState().wasmTier2GeneratorWorklist(lock);
    for (size_t i = 0; i < worklist.length(); i++) {
      wasm::Tier2GeneratorTask* task = worklist[i];
      HelperThreadState().remove(worklist, &i);
      js_delete(task);
    }
  }

  // There is at most one running Tier2Generator task and we assume that
  // below.
  static_assert(GlobalHelperThreadState::MaxTier2GeneratorTasks == 1,
                "code must be generalized");

  // If there is a running Tier2 generator task, shut it down in a predictable
  // way.  The task will be deleted by the normal deletion logic.
  for (auto& helper : *HelperThreadState().threads) {
    if (helper.wasmTier2GeneratorTask()) {
      // Set a flag that causes compilation to shortcut itself.
      helper.wasmTier2GeneratorTask()->cancel();

      // Wait for the generator task to finish.  This avoids a shutdown race
      // where the shutdown code is trying to shut down helper threads and the
      // ongoing tier2 compilation is trying to finish, which requires it to
      // have access to helper threads.
      uint32_t oldFinishedCount =
          HelperThreadState().wasmTier2GeneratorsFinished(lock);
      while (HelperThreadState().wasmTier2GeneratorsFinished(lock) ==
             oldFinishedCount) {
        HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
      }

      // At most one of these tasks.
      break;
    }
  }
}

void js::CancelOffThreadWasmTier2Generator() {
  AutoLockHelperThreadState lock;
  CancelOffThreadWasmTier2GeneratorLocked(lock);
}

bool js::StartOffThreadIonCompile(jit::IonBuilder* builder,
                                  const AutoLockHelperThreadState& lock) {
  if (!HelperThreadState().ionWorklist(lock).append(builder)) {
    return false;
  }

  // The build is moving off-thread. Freeze the LifoAlloc to prevent any
  // unwanted mutations.
  builder->alloc().lifoAlloc()->setReadOnly();

  HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);
  return true;
}

bool js::StartOffThreadIonFree(jit::IonBuilder* builder,
                               const AutoLockHelperThreadState& lock) {
  MOZ_ASSERT(CanUseExtraThreads());

  if (!HelperThreadState().ionFreeList(lock).append(builder)) {
    return false;
  }

  HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);
  return true;
}

/*
 * Move an IonBuilder for which compilation has either finished, failed, or
 * been cancelled into the global finished compilation list. All off thread
 * compilations which are started must eventually be finished.
 */
static void FinishOffThreadIonCompile(jit::IonBuilder* builder,
                                      const AutoLockHelperThreadState& lock) {
  AutoEnterOOMUnsafeRegion oomUnsafe;
  if (!HelperThreadState().ionFinishedList(lock).append(builder)) {
    oomUnsafe.crash("FinishOffThreadIonCompile");
  }
  builder->script()
      ->runtimeFromAnyThread()
      ->jitRuntime()
      ->numFinishedBuildersRef(lock)++;
}

static JSRuntime* GetSelectorRuntime(const CompilationSelector& selector) {
  struct Matcher {
    JSRuntime* operator()(JSScript* script) {
      return script->runtimeFromMainThread();
    }
    JSRuntime* operator()(Realm* realm) {
      return realm->runtimeFromMainThread();
    }
    JSRuntime* operator()(Zone* zone) { return zone->runtimeFromMainThread(); }
    JSRuntime* operator()(ZonesInState zbs) { return zbs.runtime; }
    JSRuntime* operator()(JSRuntime* runtime) { return runtime; }
    JSRuntime* operator()(CompilationsUsingNursery cun) { return cun.runtime; }
  };

  return selector.match(Matcher());
}

static bool JitDataStructuresExist(const CompilationSelector& selector) {
  struct Matcher {
    bool operator()(JSScript* script) { return !!script->realm()->jitRealm(); }
    bool operator()(Realm* realm) { return !!realm->jitRealm(); }
    bool operator()(Zone* zone) { return !!zone->jitZone(); }
    bool operator()(ZonesInState zbs) { return zbs.runtime->hasJitRuntime(); }
    bool operator()(JSRuntime* runtime) { return runtime->hasJitRuntime(); }
    bool operator()(CompilationsUsingNursery cun) {
      return cun.runtime->hasJitRuntime();
    }
  };

  return selector.match(Matcher());
}

static bool IonBuilderMatches(const CompilationSelector& selector,
                              jit::IonBuilder* builder) {
  struct BuilderMatches {
    jit::IonBuilder* builder_;

    bool operator()(JSScript* script) { return script == builder_->script(); }
    bool operator()(Realm* realm) {
      return realm == builder_->script()->realm();
    }
    bool operator()(Zone* zone) {
      return zone == builder_->script()->zoneFromAnyThread();
    }
    bool operator()(JSRuntime* runtime) {
      return runtime == builder_->script()->runtimeFromAnyThread();
    }
    bool operator()(ZonesInState zbs) {
      return zbs.runtime == builder_->script()->runtimeFromAnyThread() &&
             zbs.state == builder_->script()->zoneFromAnyThread()->gcState();
    }
    bool operator()(CompilationsUsingNursery cun) {
      return cun.runtime == builder_->script()->runtimeFromAnyThread() &&
             !builder_->safeForMinorGC();
    }
  };

  return selector.match(BuilderMatches{builder});
}

static void CancelOffThreadIonCompileLocked(const CompilationSelector& selector,
                                            AutoLockHelperThreadState& lock) {
  if (!HelperThreadState().threads) {
    return;
  }

  /* Cancel any pending entries for which processing hasn't started. */
  GlobalHelperThreadState::IonBuilderVector& worklist =
      HelperThreadState().ionWorklist(lock);
  for (size_t i = 0; i < worklist.length(); i++) {
    jit::IonBuilder* builder = worklist[i];
    if (IonBuilderMatches(selector, builder)) {
      // Once finished, builders are handled by a Linked list which is
      // allocated with the IonBuilder class which is contained in the
      // LifoAlloc-ated structure. Thus we need it to be mutable.
      worklist[i]->alloc().lifoAlloc()->setReadWrite();

      FinishOffThreadIonCompile(builder, lock);
      HelperThreadState().remove(worklist, &i);
    }
  }

  /* Wait for in progress entries to finish up. */
  bool cancelled;
  do {
    cancelled = false;
    for (auto& helper : *HelperThreadState().threads) {
      if (helper.ionBuilder() &&
          IonBuilderMatches(selector, helper.ionBuilder())) {
        helper.ionBuilder()->cancel();
        cancelled = true;
      }
    }
    if (cancelled) {
      HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
    }
  } while (cancelled);

  /* Cancel code generation for any completed entries. */
  GlobalHelperThreadState::IonBuilderVector& finished =
      HelperThreadState().ionFinishedList(lock);
  for (size_t i = 0; i < finished.length(); i++) {
    jit::IonBuilder* builder = finished[i];
    if (IonBuilderMatches(selector, builder)) {
      JSRuntime* rt = builder->script()->runtimeFromAnyThread();
      rt->jitRuntime()->numFinishedBuildersRef(lock)--;
      jit::FinishOffThreadBuilder(rt, builder, lock);
      HelperThreadState().remove(finished, &i);
    }
  }

  /* Cancel lazy linking for pending builders (attached to the ionScript). */
  JSRuntime* runtime = GetSelectorRuntime(selector);
  jit::IonBuilder* builder =
      runtime->jitRuntime()->ionLazyLinkList(runtime).getFirst();
  while (builder) {
    jit::IonBuilder* next = builder->getNext();
    if (IonBuilderMatches(selector, builder)) {
      jit::FinishOffThreadBuilder(runtime, builder, lock);
    }
    builder = next;
  }
}

void js::CancelOffThreadIonCompile(const CompilationSelector& selector) {
  if (!JitDataStructuresExist(selector)) {
    return;
  }

  AutoLockHelperThreadState lock;
  CancelOffThreadIonCompileLocked(selector, lock);
}

#ifdef DEBUG
bool js::HasOffThreadIonCompile(Realm* realm) {
  AutoLockHelperThreadState lock;

  if (!HelperThreadState().threads) {
    return false;
  }

  GlobalHelperThreadState::IonBuilderVector& worklist =
      HelperThreadState().ionWorklist(lock);
  for (size_t i = 0; i < worklist.length(); i++) {
    jit::IonBuilder* builder = worklist[i];
    if (builder->script()->realm() == realm) {
      return true;
    }
  }

  for (auto& helper : *HelperThreadState().threads) {
    if (helper.ionBuilder() &&
        helper.ionBuilder()->script()->realm() == realm) {
      return true;
    }
  }

  GlobalHelperThreadState::IonBuilderVector& finished =
      HelperThreadState().ionFinishedList(lock);
  for (size_t i = 0; i < finished.length(); i++) {
    jit::IonBuilder* builder = finished[i];
    if (builder->script()->realm() == realm) {
      return true;
    }
  }

  JSRuntime* rt = realm->runtimeFromMainThread();
  jit::IonBuilder* builder = rt->jitRuntime()->ionLazyLinkList(rt).getFirst();
  while (builder) {
    if (builder->script()->realm() == realm) {
      return true;
    }
    builder = builder->getNext();
  }

  return false;
}
#endif

struct MOZ_RAII AutoSetContextRuntime {
  explicit AutoSetContextRuntime(JSRuntime* rt) {
    TlsContext.get()->setRuntime(rt);
  }
  ~AutoSetContextRuntime() { TlsContext.get()->setRuntime(nullptr); }
};

struct MOZ_RAII AutoSetContextParse {
  explicit AutoSetContextParse(ParseTask* task) {
    TlsContext.get()->setParseTask(task);
  }
  ~AutoSetContextParse() { TlsContext.get()->setParseTask(nullptr); }
};

// We want our default stack size limit to be approximately 2MB, to be safe, but
// expect most threads to use much less. On Linux, however, requesting a stack
// of 2MB or larger risks the kernel allocating an entire 2MB huge page for it
// on first access, which we do not want. To avoid this possibility, we subtract
// 2 standard VM page sizes from our default.
static const uint32_t kDefaultHelperStackSize = 2048 * 1024 - 2 * 4096;
static const uint32_t kDefaultHelperStackQuota = 1800 * 1024;

// TSan enforces a minimum stack size that's just slightly larger than our
// default helper stack size.  It does this to store blobs of TSan-specific
// data on each thread's stack.  Unfortunately, that means that even though
// we'll actually receive a larger stack than we requested, the effective
// usable space of that stack is significantly less than what we expect.
// To offset TSan stealing our stack space from underneath us, double the
// default.
//
// Note that we don't need this for ASan/MOZ_ASAN because ASan doesn't
// require all the thread-specific state that TSan does.
#if defined(MOZ_TSAN)
static const uint32_t HELPER_STACK_SIZE = 2 * kDefaultHelperStackSize;
static const uint32_t HELPER_STACK_QUOTA = 2 * kDefaultHelperStackQuota;
#else
static const uint32_t HELPER_STACK_SIZE = kDefaultHelperStackSize;
static const uint32_t HELPER_STACK_QUOTA = kDefaultHelperStackQuota;
#endif

AutoSetHelperThreadContext::AutoSetHelperThreadContext() {
  AutoLockHelperThreadState lock;
  cx = HelperThreadState().getFirstUnusedContext(lock);
  MOZ_ASSERT(cx);
  cx->setHelperThread(lock);
  cx->nativeStackBase = GetNativeStackBase();
  // When we set the JSContext, we need to reset the computed stack limits for
  // the current thread, so we also set the native stack quota.
  JS_SetNativeStackQuota(cx, HELPER_STACK_QUOTA);
}

static const JSClass parseTaskGlobalClass = {"internal-parse-task-global",
                                             JSCLASS_GLOBAL_FLAGS,
                                             &JS::DefaultGlobalClassOps};

ParseTask::ParseTask(ParseTaskKind kind, JSContext* cx,
                     JS::OffThreadCompileCallback callback, void* callbackData)
    : kind(kind),
      options(cx),
      parseGlobal(nullptr),
      callback(callback),
      callbackData(callbackData),
      overRecursed(false),
      outOfMemory(false) {
  // Note that |cx| is the main thread context here but the parse task will
  // run with a different, helper thread, context.
  MOZ_ASSERT(!cx->isHelperThreadContext());

  MOZ_ALWAYS_TRUE(scripts.reserve(scripts.capacity()));
  MOZ_ALWAYS_TRUE(sourceObjects.reserve(sourceObjects.capacity()));
}

bool ParseTask::init(JSContext* cx, const ReadOnlyCompileOptions& options,
                     JSObject* global) {
  MOZ_ASSERT(!cx->isHelperThreadContext());

  if (!this->options.copy(cx, options)) {
    return false;
  }

  parseGlobal = global;
  return true;
}

void ParseTask::activate(JSRuntime* rt) {
  rt->setUsedByHelperThread(parseGlobal->zone());
}

ParseTask::~ParseTask() = default;

void ParseTask::trace(JSTracer* trc) {
  if (parseGlobal->runtimeFromAnyThread() != trc->runtime()) {
    return;
  }

  Zone* zone = MaybeForwarded(parseGlobal)->zoneFromAnyThread();
  if (zone->usedByHelperThread()) {
    MOZ_ASSERT(!zone->isCollecting());
    return;
  }

  TraceManuallyBarrieredEdge(trc, &parseGlobal, "ParseTask::parseGlobal");
  scripts.trace(trc);
  sourceObjects.trace(trc);
}

size_t ParseTask::sizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return options.sizeOfExcludingThis(mallocSizeOf) +
         errors.sizeOfExcludingThis(mallocSizeOf);
}

void ParseTask::runTask() {
  AutoSetHelperThreadContext usesContext;

  JSContext* cx = TlsContext.get();
  JSRuntime* runtime = parseGlobal->runtimeFromAnyThread();

  AutoSetContextRuntime ascr(runtime);
  AutoSetContextParse parsetask(this);
  gc::AutoSuppressNurseryCellAlloc noNurseryAlloc(cx);

  Zone* zone = parseGlobal->zoneFromAnyThread();
  zone->setHelperThreadOwnerContext(cx);
  auto resetOwnerContext = mozilla::MakeScopeExit(
      [&] { zone->setHelperThreadOwnerContext(nullptr); });

  AutoRealm ar(cx, parseGlobal);

  parse(cx);

  MOZ_ASSERT(cx->tempLifoAlloc().isEmpty());
  cx->tempLifoAlloc().freeAll();
  cx->frontendCollectionPool().purge();
  cx->atomsZoneFreeLists().clear();
}

template <typename Unit>
struct ScriptParseTask : public ParseTask {
  JS::SourceText<Unit> data;

  ScriptParseTask(JSContext* cx, JS::SourceText<Unit>& srcBuf,
                  JS::OffThreadCompileCallback callback, void* callbackData);
  void parse(JSContext* cx) override;
};

template <typename Unit>
ScriptParseTask<Unit>::ScriptParseTask(JSContext* cx,
                                       JS::SourceText<Unit>& srcBuf,
                                       JS::OffThreadCompileCallback callback,
                                       void* callbackData)
    : ParseTask(ParseTaskKind::Script, cx, callback, callbackData),
      data(std::move(srcBuf)) {}

template <typename Unit>
void ScriptParseTask<Unit>::parse(JSContext* cx) {
  MOZ_ASSERT(cx->isHelperThreadContext());

  JSScript* script;
  Rooted<ScriptSourceObject*> sourceObject(cx);

  {
    ScopeKind scopeKind =
        options.nonSyntacticScope ? ScopeKind::NonSyntactic : ScopeKind::Global;
    frontend::GlobalScriptInfo info(cx, options, scopeKind);
    script = frontend::CompileGlobalScript(
        info, data,
        /* sourceObjectOut = */ &sourceObject.get());
  }

  if (script) {
    scripts.infallibleAppend(script);
  }

  // Whatever happens to the top-level script compilation (even if it fails),
  // we must finish initializing the SSO.  This is because there may be valid
  // inner scripts observable by the debugger which reference the partially-
  // initialized SSO.
  if (sourceObject) {
    sourceObjects.infallibleAppend(sourceObject);
  }
}

template <typename Unit>
struct ModuleParseTask : public ParseTask {
  JS::SourceText<Unit> data;

  ModuleParseTask(JSContext* cx, JS::SourceText<Unit>& srcBuf,
                  JS::OffThreadCompileCallback callback, void* callbackData);
  void parse(JSContext* cx) override;
};

template <typename Unit>
ModuleParseTask<Unit>::ModuleParseTask(JSContext* cx,
                                       JS::SourceText<Unit>& srcBuf,
                                       JS::OffThreadCompileCallback callback,
                                       void* callbackData)
    : ParseTask(ParseTaskKind::Module, cx, callback, callbackData),
      data(std::move(srcBuf)) {}

template <typename Unit>
void ModuleParseTask<Unit>::parse(JSContext* cx) {
  MOZ_ASSERT(cx->isHelperThreadContext());

  Rooted<ScriptSourceObject*> sourceObject(cx);

  ModuleObject* module =
      frontend::ParseModule(cx, options, data, &sourceObject.get());
  if (module) {
    scripts.infallibleAppend(module->script());
    if (sourceObject) {
      sourceObjects.infallibleAppend(sourceObject);
    }
  }
}

ScriptDecodeTask::ScriptDecodeTask(JSContext* cx,
                                   const JS::TranscodeRange& range,
                                   JS::OffThreadCompileCallback callback,
                                   void* callbackData)
    : ParseTask(ParseTaskKind::ScriptDecode, cx, callback, callbackData),
      range(range) {}

void ScriptDecodeTask::parse(JSContext* cx) {
  MOZ_ASSERT(cx->isHelperThreadContext());

  RootedScript resultScript(cx);
  Rooted<ScriptSourceObject*> sourceObject(cx);

  XDROffThreadDecoder decoder(
      cx, &options, /* sourceObjectOut = */ &sourceObject.get(), range);
  XDRResult res = decoder.codeScript(&resultScript);
  MOZ_ASSERT(bool(resultScript) == res.isOk());
  if (res.isOk()) {
    scripts.infallibleAppend(resultScript);
    if (sourceObject) {
      sourceObjects.infallibleAppend(sourceObject);
    }
  }
}

#if defined(JS_BUILD_BINAST)

BinASTDecodeTask::BinASTDecodeTask(JSContext* cx, const uint8_t* buf,
                                   size_t length,
                                   JS::OffThreadCompileCallback callback,
                                   void* callbackData)
    : ParseTask(ParseTaskKind::BinAST, cx, callback, callbackData),
      data(buf, length) {}

void BinASTDecodeTask::parse(JSContext* cx) {
  MOZ_ASSERT(cx->isHelperThreadContext());

  RootedScriptSourceObject sourceObject(cx);

  JSScript* script = frontend::CompileGlobalBinASTScript(
      cx, cx->tempLifoAlloc(), options, data.begin().get(), data.length(),
      &sourceObject.get());
  if (script) {
    scripts.infallibleAppend(script);
    if (sourceObject) {
      sourceObjects.infallibleAppend(sourceObject);
    }
  }
}

#endif /* JS_BUILD_BINAST */

MultiScriptsDecodeTask::MultiScriptsDecodeTask(
    JSContext* cx, JS::TranscodeSources& sources,
    JS::OffThreadCompileCallback callback, void* callbackData)
    : ParseTask(ParseTaskKind::MultiScriptsDecode, cx, callback, callbackData),
      sources(&sources) {}

void MultiScriptsDecodeTask::parse(JSContext* cx) {
  MOZ_ASSERT(cx->isHelperThreadContext());

  if (!scripts.reserve(sources->length()) ||
      !sourceObjects.reserve(sources->length())) {
    ReportOutOfMemory(cx);  // This sets |outOfMemory|.
    return;
  }

  for (auto& source : *sources) {
    CompileOptions opts(cx, options);
    opts.setFileAndLine(source.filename, source.lineno);

    RootedScript resultScript(cx);
    Rooted<ScriptSourceObject*> sourceObject(cx);

    XDROffThreadDecoder decoder(cx, &opts, &sourceObject.get(), source.range);
    XDRResult res = decoder.codeScript(&resultScript);
    MOZ_ASSERT(bool(resultScript) == res.isOk());

    if (res.isErr()) {
      break;
    }
    MOZ_ASSERT(resultScript);
    scripts.infallibleAppend(resultScript);
    sourceObjects.infallibleAppend(sourceObject);
  }
}

void js::CancelOffThreadParses(JSRuntime* rt) {
  AutoLockHelperThreadState lock;

  if (!HelperThreadState().threads) {
    return;
  }

#ifdef DEBUG
  GlobalHelperThreadState::ParseTaskVector& waitingOnGC =
      HelperThreadState().parseWaitingOnGC(lock);
  for (size_t i = 0; i < waitingOnGC.length(); i++) {
    MOZ_ASSERT(!waitingOnGC[i]->runtimeMatches(rt));
  }
#endif

  // Instead of forcibly canceling pending parse tasks, just wait for all
  // scheduled and in progress ones to complete. Otherwise the final GC may not
  // collect everything due to zones being used off thread.
  while (true) {
    bool pending = false;
    GlobalHelperThreadState::ParseTaskVector& worklist =
        HelperThreadState().parseWorklist(lock);
    for (size_t i = 0; i < worklist.length(); i++) {
      ParseTask* task = worklist[i];
      if (task->runtimeMatches(rt)) {
        pending = true;
      }
    }
    if (!pending) {
      bool inProgress = false;
      for (auto& thread : *HelperThreadState().threads) {
        ParseTask* task = thread.parseTask();
        if (task && task->runtimeMatches(rt)) {
          inProgress = true;
        }
      }
      if (!inProgress) {
        break;
      }
    }
    HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
  }

  // Clean up any parse tasks which haven't been finished by the main thread.
  auto& finished = HelperThreadState().parseFinishedList(lock);
  while (true) {
    bool found = false;
    ParseTask* next;
    ParseTask* task = finished.getFirst();
    while (task) {
      next = task->getNext();
      if (task->runtimeMatches(rt)) {
        found = true;
        task->remove();
        HelperThreadState().destroyParseTask(rt, task);
      }
      task = next;
    }
    if (!found) {
      break;
    }
  }

#ifdef DEBUG
  GlobalHelperThreadState::ParseTaskVector& worklist =
      HelperThreadState().parseWorklist(lock);
  for (size_t i = 0; i < worklist.length(); i++) {
    ParseTask* task = worklist[i];
    MOZ_ASSERT(!task->runtimeMatches(rt));
  }
#endif
}

bool js::OffThreadParsingMustWaitForGC(JSRuntime* rt) {
  // Off thread parsing can't occur during incremental collections on the
  // atoms zone, to avoid triggering barriers. (Outside the atoms zone, the
  // compilation will use a new zone that is never collected.) If an
  // atoms-zone GC is in progress, hold off on executing the parse task until
  // the atoms-zone GC completes (see EnqueuePendingParseTasksAfterGC).
  return rt->activeGCInAtomsZone();
}

static bool EnsureConstructor(JSContext* cx, Handle<GlobalObject*> global,
                              JSProtoKey key) {
  if (!GlobalObject::ensureConstructor(cx, global, key)) {
    return false;
  }

  MOZ_ASSERT(global->getPrototype(key).toObject().isDelegate(),
             "standard class prototype wasn't a delegate from birth");
  return true;
}

// Initialize all classes potentially created during parsing for use in parser
// data structures, template objects, &c.
static bool EnsureParserCreatedClasses(JSContext* cx, ParseTaskKind kind) {
  Handle<GlobalObject*> global = cx->global();

  if (!EnsureConstructor(cx, global, JSProto_Function)) {
    return false;  // needed by functions, also adds object literals' proto
  }

  if (!EnsureConstructor(cx, global, JSProto_Array)) {
    return false;  // needed by array literals
  }

  if (!EnsureConstructor(cx, global, JSProto_RegExp)) {
    return false;  // needed by regular expression literals
  }

  if (!GlobalObject::initGenerators(cx, global)) {
    return false;  // needed by function*() {}
  }

  if (!GlobalObject::initAsyncFunction(cx, global)) {
    return false;  // needed by async function() {}
  }

  if (!GlobalObject::initAsyncGenerators(cx, global)) {
    return false;  // needed by async function*() {}
  }

  if (kind == ParseTaskKind::Module &&
      !GlobalObject::ensureModulePrototypesCreated(cx, global)) {
    return false;
  }

  return true;
}

class MOZ_RAII AutoSetCreatedForHelperThread {
  Zone* zone;

 public:
  explicit AutoSetCreatedForHelperThread(JSObject* global)
      : zone(global->zone()) {
    zone->setCreatedForHelperThread();
  }

  void forget() { zone = nullptr; }

  ~AutoSetCreatedForHelperThread() {
    if (zone) {
      zone->clearUsedByHelperThread();
    }
  }
};

static JSObject* CreateGlobalForOffThreadParse(JSContext* cx,
                                               const gc::AutoSuppressGC& nogc) {
  JS::Realm* currentRealm = cx->realm();

  JS::RealmOptions realmOptions(currentRealm->creationOptions(),
                                currentRealm->behaviors());

  auto& creationOptions = realmOptions.creationOptions();

  creationOptions.setInvisibleToDebugger(true)
      .setMergeable(true)
      .setNewCompartmentAndZone();

  // Don't falsely inherit the host's global trace hook.
  creationOptions.setTrace(nullptr);

  return JS_NewGlobalObject(cx, &parseTaskGlobalClass,
                            currentRealm->principals(),
                            JS::DontFireOnNewGlobalHook, realmOptions);
}

static bool QueueOffThreadParseTask(JSContext* cx, UniquePtr<ParseTask> task) {
  AutoLockHelperThreadState lock;

  bool mustWait = OffThreadParsingMustWaitForGC(cx->runtime());

  // Append null first, then overwrite it on  success, to avoid having two
  // |task| pointers (one ostensibly "unique") in flight at once.  (Obviously it
  // would be better if these vectors stored unique pointers themselves....)
  auto& queue = mustWait ? HelperThreadState().parseWaitingOnGC(lock)
                         : HelperThreadState().parseWorklist(lock);
  if (!queue.append(nullptr)) {
    ReportOutOfMemory(cx);
    return false;
  }

  queue.back() = task.release();

  if (!mustWait) {
    queue.back()->activate(cx->runtime());
    HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);
  }

  return true;
}

static bool StartOffThreadParseTask(JSContext* cx, UniquePtr<ParseTask> task,
                                    const ReadOnlyCompileOptions& options) {
  // Suppress GC so that calls below do not trigger a new incremental GC
  // which could require barriers on the atoms zone.
  gc::AutoSuppressGC nogc(cx);
  gc::AutoSuppressNurseryCellAlloc noNurseryAlloc(cx);
  AutoSuppressAllocationMetadataBuilder suppressMetadata(cx);

  JSObject* global = CreateGlobalForOffThreadParse(cx, nogc);
  if (!global) {
    return false;
  }

  // Mark the global's zone as created for a helper thread. This prevents it
  // from being collected until clearUsedByHelperThread() is called after
  // parsing is complete. If this function exits due to error this state is
  // cleared automatically.
  AutoSetCreatedForHelperThread createdForHelper(global);

  if (!task->init(cx, options, global)) {
    return false;
  }

  if (!QueueOffThreadParseTask(cx, std::move(task))) {
    return false;
  }

  createdForHelper.forget();
  return true;
}

template <typename Unit>
static bool StartOffThreadParseScriptInternal(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<Unit>& srcBuf, JS::OffThreadCompileCallback callback,
    void* callbackData) {
  auto task = cx->make_unique<ScriptParseTask<Unit>>(cx, srcBuf, callback,
                                                     callbackData);
  if (!task) {
    return false;
  }

  return StartOffThreadParseTask(cx, std::move(task), options);
}

bool js::StartOffThreadParseScript(JSContext* cx,
                                   const ReadOnlyCompileOptions& options,
                                   JS::SourceText<char16_t>& srcBuf,
                                   JS::OffThreadCompileCallback callback,
                                   void* callbackData) {
  return StartOffThreadParseScriptInternal(cx, options, srcBuf, callback,
                                           callbackData);
}

bool js::StartOffThreadParseScript(JSContext* cx,
                                   const ReadOnlyCompileOptions& options,
                                   JS::SourceText<Utf8Unit>& srcBuf,
                                   JS::OffThreadCompileCallback callback,
                                   void* callbackData) {
  return StartOffThreadParseScriptInternal(cx, options, srcBuf, callback,
                                           callbackData);
}

template <typename Unit>
static bool StartOffThreadParseModuleInternal(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<Unit>& srcBuf, JS::OffThreadCompileCallback callback,
    void* callbackData) {
  auto task = cx->make_unique<ModuleParseTask<Unit>>(cx, srcBuf, callback,
                                                     callbackData);
  if (!task) {
    return false;
  }

  return StartOffThreadParseTask(cx, std::move(task), options);
}

bool js::StartOffThreadParseModule(JSContext* cx,
                                   const ReadOnlyCompileOptions& options,
                                   JS::SourceText<char16_t>& srcBuf,
                                   JS::OffThreadCompileCallback callback,
                                   void* callbackData) {
  return StartOffThreadParseModuleInternal(cx, options, srcBuf, callback,
                                           callbackData);
}

bool js::StartOffThreadParseModule(JSContext* cx,
                                   const ReadOnlyCompileOptions& options,
                                   JS::SourceText<Utf8Unit>& srcBuf,
                                   JS::OffThreadCompileCallback callback,
                                   void* callbackData) {
  return StartOffThreadParseModuleInternal(cx, options, srcBuf, callback,
                                           callbackData);
}

bool js::StartOffThreadDecodeScript(JSContext* cx,
                                    const ReadOnlyCompileOptions& options,
                                    const JS::TranscodeRange& range,
                                    JS::OffThreadCompileCallback callback,
                                    void* callbackData) {
  auto task =
      cx->make_unique<ScriptDecodeTask>(cx, range, callback, callbackData);
  if (!task) {
    return false;
  }

  return StartOffThreadParseTask(cx, std::move(task), options);
}

bool js::StartOffThreadDecodeMultiScripts(JSContext* cx,
                                          const ReadOnlyCompileOptions& options,
                                          JS::TranscodeSources& sources,
                                          JS::OffThreadCompileCallback callback,
                                          void* callbackData) {
  auto task = cx->make_unique<MultiScriptsDecodeTask>(cx, sources, callback,
                                                      callbackData);
  if (!task) {
    return false;
  }

  return StartOffThreadParseTask(cx, std::move(task), options);
}

#if defined(JS_BUILD_BINAST)

bool js::StartOffThreadDecodeBinAST(JSContext* cx,
                                    const ReadOnlyCompileOptions& options,
                                    const uint8_t* buf, size_t length,
                                    JS::OffThreadCompileCallback callback,
                                    void* callbackData) {
  if (!cx->runtime()->binast().ensureBinTablesInitialized(cx)) {
    return false;
  }

  auto task = cx->make_unique<BinASTDecodeTask>(cx, buf, length, callback,
                                                callbackData);
  if (!task) {
    return false;
  }

  return StartOffThreadParseTask(cx, std::move(task), options);
}

#endif /* JS_BUILD_BINAST */

void js::EnqueuePendingParseTasksAfterGC(JSRuntime* rt) {
  MOZ_ASSERT(!OffThreadParsingMustWaitForGC(rt));

  GlobalHelperThreadState::ParseTaskVector newTasks;
  {
    AutoLockHelperThreadState lock;
    GlobalHelperThreadState::ParseTaskVector& waiting =
        HelperThreadState().parseWaitingOnGC(lock);

    for (size_t i = 0; i < waiting.length(); i++) {
      ParseTask* task = waiting[i];
      if (task->runtimeMatches(rt)) {
        AutoEnterOOMUnsafeRegion oomUnsafe;
        if (!newTasks.append(task)) {
          oomUnsafe.crash("EnqueuePendingParseTasksAfterGC");
        }
        HelperThreadState().remove(waiting, &i);
      }
    }
  }

  if (newTasks.empty()) {
    return;
  }

  // This logic should mirror the contents of the
  // !OffThreadParsingMustWaitForGC() branch in QueueOffThreadParseTask:

  for (size_t i = 0; i < newTasks.length(); i++) {
    newTasks[i]->activate(rt);
  }

  AutoLockHelperThreadState lock;

  {
    AutoEnterOOMUnsafeRegion oomUnsafe;
    if (!HelperThreadState().parseWorklist(lock).appendAll(newTasks)) {
      oomUnsafe.crash("EnqueuePendingParseTasksAfterGC");
    }
  }

  HelperThreadState().notifyAll(GlobalHelperThreadState::PRODUCER, lock);
}

#ifdef DEBUG
bool js::CurrentThreadIsParseThread() {
  JSContext* cx = TlsContext.get();
  return cx->isHelperThreadContext() && cx->parseTask();
}
#endif

bool GlobalHelperThreadState::ensureInitialized() {
  MOZ_ASSERT(CanUseExtraThreads());

  MOZ_ASSERT(this == &HelperThreadState());
  AutoLockHelperThreadState lock;

  if (threads) {
    return true;
  }

  threads = js::MakeUnique<HelperThreadVector>();
  if (!threads || !threads->initCapacity(threadCount)) {
    return false;
  }

  for (size_t i = 0; i < threadCount; i++) {
    threads->infallibleEmplaceBack();
    HelperThread& helper = (*threads)[i];

    helper.thread = mozilla::Some(
        Thread(Thread::Options().setStackSize(HELPER_STACK_SIZE)));
    if (!helper.thread->init(HelperThread::ThreadMain, &helper)) {
      goto error;
    }

    continue;

  error:
    // Ensure that we do not leave uninitialized threads in the `threads`
    // vector.
    threads->popBack();
    finishThreads();
    return false;
  }

  return true;
}

GlobalHelperThreadState::GlobalHelperThreadState()
    : cpuCount(0),
      threadCount(0),
      threads(nullptr),
      registerThread(nullptr),
      unregisterThread(nullptr),
      wasmTier2GeneratorsFinished_(0),
      helperLock(mutexid::GlobalHelperThreadState) {
  cpuCount = ClampDefaultCPUCount(GetCPUCount());
  threadCount = ThreadCountForCPUCount(cpuCount);

  MOZ_ASSERT(cpuCount > 0, "GetCPUCount() seems broken");
}

void GlobalHelperThreadState::finish() {
  CancelOffThreadWasmTier2Generator();
  finishThreads();

  // Make sure there are no Ion free tasks left. We check this here because,
  // unlike the other tasks, we don't explicitly block on this when
  // destroying a runtime.
  AutoLockHelperThreadState lock;
  auto& freeList = ionFreeList(lock);
  while (!freeList.empty()) {
    jit::FreeIonBuilder(freeList.popCopy());
  }
  destroyHelperContexts(lock);
}

void GlobalHelperThreadState::finishThreads() {
  if (!threads) {
    return;
  }

  MOZ_ASSERT(CanUseExtraThreads());
  for (auto& thread : *threads) {
    thread.destroy();
  }
  threads.reset(nullptr);
}

bool GlobalHelperThreadState::ensureContextListForThreadCount() {
  if (helperContexts_.length() >= threadCount) {
    return true;
  }
  AutoLockHelperThreadState lock;

  // SetFakeCPUCount() may cause the context list to contain less contexts
  // than there are helper threads, which could potentially lead to a crash.
  // Append more initialized contexts to the list until there are enough.
  while (helperContexts_.length() < threadCount) {
    UniquePtr<JSContext> cx =
        js::MakeUnique<JSContext>(nullptr, JS::ContextOptions());
    // To initialize context-specific protected data, the context must
    // temporarily set itself to the main thread. After initialization,
    // cx can clear itself from the thread.
    cx->setHelperThread(lock);
    if (!cx->init(ContextKind::HelperThread)) {
      return false;
    }
    cx->clearHelperThread(lock);
    if (!helperContexts_.append(cx.release())) {
      return false;
    }
  }
  return true;
}

JSContext* GlobalHelperThreadState::getFirstUnusedContext(
    AutoLockHelperThreadState& locked) {
  for (auto& cx : helperContexts_) {
    if (cx->contextAvailable(locked)) {
      return cx;
    }
  }
  MOZ_CRASH("Expected available JSContext");
}

void GlobalHelperThreadState::destroyHelperContexts(
    AutoLockHelperThreadState& lock) {
  while (helperContexts_.length() > 0) {
    JSContext* cx = helperContexts_.popCopy();
    // Before cx can be destroyed, it has to set itself to the main thread.
    // This enables it to pass its context-specific data checks.
    cx->setHelperThread(lock);
    js_delete(cx);
  }
}

#ifdef DEBUG
bool GlobalHelperThreadState::isLockedByCurrentThread() const {
  return helperLock.ownedByCurrentThread();
}
#endif  // DEBUG

void GlobalHelperThreadState::wait(
    AutoLockHelperThreadState& locked, CondVar which,
    TimeDuration timeout /* = TimeDuration::Forever() */) {
  whichWakeup(which).wait_for(locked, timeout);
}

void GlobalHelperThreadState::notifyAll(CondVar which,
                                        const AutoLockHelperThreadState&) {
  whichWakeup(which).notify_all();
}

void GlobalHelperThreadState::notifyOne(CondVar which,
                                        const AutoLockHelperThreadState&) {
  whichWakeup(which).notify_one();
}

bool GlobalHelperThreadState::hasActiveThreads(
    const AutoLockHelperThreadState&) {
  if (!threads) {
    return false;
  }

  for (auto& thread : *threads) {
    if (!thread.idle()) {
      return true;
    }
  }

  return false;
}

void GlobalHelperThreadState::waitForAllThreads() {
  AutoLockHelperThreadState lock;
  waitForAllThreadsLocked(lock);
}

void GlobalHelperThreadState::waitForAllThreadsLocked(
    AutoLockHelperThreadState& lock) {
  CancelOffThreadWasmTier2GeneratorLocked(lock);

  while (hasActiveThreads(lock)) {
    wait(lock, CONSUMER);
  }
}

// A task can be a "master" task, ie, it will block waiting for other worker
// threads that perform work on its behalf.  If so it must not take the last
// available thread; there must always be at least one worker thread able to do
// the actual work.  (Or the system may deadlock.)
//
// If a task is a master task it *must* pass isMaster=true here, or perform a
// similar calculation to avoid deadlock from starvation.
//
// isMaster should only be true if the thread calling checkTaskThreadLimit() is
// a helper thread.
//
// NOTE: Calling checkTaskThreadLimit() from a helper thread in the dynamic
// region after currentTask.emplace() and before currentTask.reset() may cause
// it to return a different result than if it is called outside that dynamic
// region, as the predicate inspects the values of the threads' currentTask
// members.

template <typename T>
bool GlobalHelperThreadState::checkTaskThreadLimit(size_t maxThreads,
                                                   bool isMaster) const {
  MOZ_ASSERT(maxThreads > 0);

  if (!isMaster && maxThreads >= threadCount) {
    return true;
  }

  size_t count = 0;
  size_t idle = 0;
  for (auto& thread : *threads) {
    if (thread.currentTask.isSome()) {
      if (thread.currentTask->is<T>()) {
        count++;
      }
    } else {
      idle++;
    }
    if (count >= maxThreads) {
      return false;
    }
  }

  // It is possible for the number of idle threads to be zero here, because
  // checkTaskThreadLimit() can be called from non-helper threads.  Notably,
  // the compression task scheduler invokes it, and runs off a helper thread.
  if (idle == 0) {
    return false;
  }

  // A master thread that's the last available thread must not be allowed to
  // run.
  if (isMaster && idle == 1) {
    return false;
  }

  return true;
}

void GlobalHelperThreadState::triggerFreeUnusedMemory() {
  if (!CanUseExtraThreads()) {
    return;
  }

  AutoLockHelperThreadState lock;
  for (auto& context : helperContexts_) {
    if (context->shouldFreeUnusedMemory() && context->contextAvailable(lock)) {
      // This context hasn't been used since the last time freeUnusedMemory
      // was set. Free the temp LifoAlloc from the main thread.
      context->tempLifoAllocNoCheck().freeAll();
      context->setFreeUnusedMemory(false);
    } else {
      context->setFreeUnusedMemory(true);
    }
  }
}

static inline bool IsHelperThreadSimulatingOOM(js::ThreadType threadType) {
#if defined(DEBUG) || defined(JS_OOM_BREAKPOINT)
  return js::oom::simulator.targetThread() == threadType;
#else
  return false;
#endif
}

void GlobalHelperThreadState::addSizeOfIncludingThis(
    JS::GlobalStats* stats, AutoLockHelperThreadState& lock) const {
  MOZ_ASSERT(isLockedByCurrentThread());

  mozilla::MallocSizeOf mallocSizeOf = stats->mallocSizeOf_;
  JS::HelperThreadStats& htStats = stats->helperThread;

  htStats.stateData += mallocSizeOf(this);

  if (threads) {
    htStats.stateData += threads->sizeOfIncludingThis(mallocSizeOf);
  }

  // Report memory used by various containers
  htStats.stateData +=
      ionWorklist_.sizeOfExcludingThis(mallocSizeOf) +
      ionFinishedList_.sizeOfExcludingThis(mallocSizeOf) +
      ionFreeList_.sizeOfExcludingThis(mallocSizeOf) +
      wasmWorklist_tier1_.sizeOfExcludingThis(mallocSizeOf) +
      wasmWorklist_tier2_.sizeOfExcludingThis(mallocSizeOf) +
      wasmTier2GeneratorWorklist_.sizeOfExcludingThis(mallocSizeOf) +
      promiseHelperTasks_.sizeOfExcludingThis(mallocSizeOf) +
      parseWorklist_.sizeOfExcludingThis(mallocSizeOf) +
      parseFinishedList_.sizeOfExcludingThis(mallocSizeOf) +
      parseWaitingOnGC_.sizeOfExcludingThis(mallocSizeOf) +
      compressionPendingList_.sizeOfExcludingThis(mallocSizeOf) +
      compressionWorklist_.sizeOfExcludingThis(mallocSizeOf) +
      compressionFinishedList_.sizeOfExcludingThis(mallocSizeOf) +
      gcParallelWorklist_.sizeOfExcludingThis(mallocSizeOf);

  // Report ParseTasks on wait lists
  for (auto task : parseWorklist_) {
    htStats.parseTask += task->sizeOfIncludingThis(mallocSizeOf);
  }
  for (auto task : parseFinishedList_) {
    htStats.parseTask += task->sizeOfIncludingThis(mallocSizeOf);
  }
  for (auto task : parseWaitingOnGC_) {
    htStats.parseTask += task->sizeOfIncludingThis(mallocSizeOf);
  }

  // Report IonBuilders on wait lists
  for (auto builder : ionWorklist_) {
    htStats.ionBuilder += builder->sizeOfExcludingThis(mallocSizeOf);
  }
  for (auto builder : ionFinishedList_) {
    htStats.ionBuilder += builder->sizeOfExcludingThis(mallocSizeOf);
  }
  for (auto builder : ionFreeList_) {
    htStats.ionBuilder += builder->sizeOfExcludingThis(mallocSizeOf);
  }

  // Report wasm::CompileTasks on wait lists
  for (auto task : wasmWorklist_tier1_) {
    htStats.wasmCompile += task->sizeOfExcludingThis(mallocSizeOf);
  }
  for (auto task : wasmWorklist_tier2_) {
    htStats.wasmCompile += task->sizeOfExcludingThis(mallocSizeOf);
  }

  // Report number of helper threads.
  MOZ_ASSERT(htStats.idleThreadCount == 0);
  if (threads) {
    for (auto& thread : *threads) {
      if (thread.idle()) {
        htStats.idleThreadCount++;
      } else {
        htStats.activeThreadCount++;
      }
    }
  }
}

size_t GlobalHelperThreadState::maxIonCompilationThreads() const {
  if (IsHelperThreadSimulatingOOM(js::THREAD_TYPE_ION)) {
    return 1;
  }
  return threadCount;
}

size_t GlobalHelperThreadState::maxWasmCompilationThreads() const {
  if (IsHelperThreadSimulatingOOM(js::THREAD_TYPE_WASM)) {
    return 1;
  }
  return cpuCount;
}

size_t GlobalHelperThreadState::maxWasmTier2GeneratorThreads() const {
  return MaxTier2GeneratorTasks;
}

size_t GlobalHelperThreadState::maxPromiseHelperThreads() const {
  if (IsHelperThreadSimulatingOOM(js::THREAD_TYPE_WASM)) {
    return 1;
  }
  return cpuCount;
}

size_t GlobalHelperThreadState::maxParseThreads() const {
  if (IsHelperThreadSimulatingOOM(js::THREAD_TYPE_PARSE)) {
    return 1;
  }
  return cpuCount;
}

size_t GlobalHelperThreadState::maxCompressionThreads() const {
  if (IsHelperThreadSimulatingOOM(js::THREAD_TYPE_COMPRESS)) {
    return 1;
  }

  // Compression is triggered on major GCs to compress ScriptSources. It is
  // considered low priority work.
  return 1;
}

size_t GlobalHelperThreadState::maxGCParallelThreads() const {
  if (IsHelperThreadSimulatingOOM(js::THREAD_TYPE_GCPARALLEL)) {
    return 1;
  }
  return threadCount;
}

bool GlobalHelperThreadState::canStartWasmTier1Compile(
    const AutoLockHelperThreadState& lock) {
  return canStartWasmCompile(lock, wasm::CompileMode::Tier1);
}

bool GlobalHelperThreadState::canStartWasmTier2Compile(
    const AutoLockHelperThreadState& lock) {
  return canStartWasmCompile(lock, wasm::CompileMode::Tier2);
}

bool GlobalHelperThreadState::canStartWasmCompile(
    const AutoLockHelperThreadState& lock, wasm::CompileMode mode) {
  if (wasmWorklist(lock, mode).empty()) {
    return false;
  }

  // Parallel compilation and background compilation should be disabled on
  // unicore systems.

  MOZ_RELEASE_ASSERT(cpuCount > 1);

  // If Tier2 is very backlogged we must give priority to it, since the Tier2
  // queue holds onto Tier1 tasks.  Indeed if Tier2 is backlogged we will
  // devote more resources to Tier2 and not start any Tier1 work at all.

  bool tier2oversubscribed = wasmTier2GeneratorWorklist(lock).length() > 20;

  // For Tier1 and Once compilation, honor the maximum allowed threads to
  // compile wasm jobs at once, to avoid oversaturating the machine.
  //
  // For Tier2 compilation we need to allow other things to happen too, so we
  // do not allow all logical cores to be used for background work; instead we
  // wish to use a fraction of the physical cores.  We can't directly compute
  // the physical cores from the logical cores, but 1/3 of the logical cores
  // is a safe estimate for the number of physical cores available for
  // background work.

  size_t physCoresAvailable = size_t(ceil(cpuCount / 3.0));

  size_t threads;
  if (mode == wasm::CompileMode::Tier2) {
    if (tier2oversubscribed) {
      threads = maxWasmCompilationThreads();
    } else {
      threads = physCoresAvailable;
    }
  } else {
    if (tier2oversubscribed) {
      threads = 0;
    } else {
      threads = maxWasmCompilationThreads();
    }
  }

  if (!threads || !checkTaskThreadLimit<wasm::CompileTask*>(threads)) {
    return false;
  }

  return true;
}

bool GlobalHelperThreadState::canStartWasmTier2Generator(
    const AutoLockHelperThreadState& lock) {
  return !wasmTier2GeneratorWorklist(lock).empty() &&
         checkTaskThreadLimit<wasm::Tier2GeneratorTask*>(
             maxWasmTier2GeneratorThreads(),
             /*isMaster=*/true);
}

bool GlobalHelperThreadState::canStartPromiseHelperTask(
    const AutoLockHelperThreadState& lock) {
  // PromiseHelperTasks can be wasm compilation tasks that in turn block on
  // wasm compilation so set isMaster = true.
  return !promiseHelperTasks(lock).empty() &&
         checkTaskThreadLimit<PromiseHelperTask*>(maxPromiseHelperThreads(),
                                                  /*isMaster=*/true);
}

static bool IonBuilderHasHigherPriority(jit::IonBuilder* first,
                                        jit::IonBuilder* second) {
  // Return true if priority(first) > priority(second).
  //
  // This method can return whatever it wants, though it really ought to be a
  // total order. The ordering is allowed to race (change on the fly), however.

  // A lower optimization level indicates a higher priority.
  if (first->optimizationInfo().level() != second->optimizationInfo().level()) {
    return first->optimizationInfo().level() <
           second->optimizationInfo().level();
  }

  // A script without an IonScript has precedence on one with.
  if (first->scriptHasIonScript() != second->scriptHasIonScript()) {
    return !first->scriptHasIonScript();
  }

  // A higher warm-up counter indicates a higher priority.
  return first->script()->getWarmUpCount() / first->script()->length() >
         second->script()->getWarmUpCount() / second->script()->length();
}

bool GlobalHelperThreadState::canStartIonCompile(
    const AutoLockHelperThreadState& lock) {
  return !ionWorklist(lock).empty() &&
         checkTaskThreadLimit<jit::IonBuilder*>(maxIonCompilationThreads());
}

bool GlobalHelperThreadState::canStartIonFreeTask(
    const AutoLockHelperThreadState& lock) {
  return !ionFreeList(lock).empty();
}

jit::IonBuilder* GlobalHelperThreadState::highestPriorityPendingIonCompile(
    const AutoLockHelperThreadState& lock) {
  auto& worklist = ionWorklist(lock);
  MOZ_ASSERT(!worklist.empty());

  // Get the highest priority IonBuilder which has not started compilation yet.
  size_t index = 0;
  for (size_t i = 1; i < worklist.length(); i++) {
    if (IonBuilderHasHigherPriority(worklist[i], worklist[index])) {
      index = i;
    }
  }

  jit::IonBuilder* builder = worklist[index];
  worklist.erase(&worklist[index]);
  return builder;
}

bool GlobalHelperThreadState::canStartParseTask(
    const AutoLockHelperThreadState& lock) {
  // Parse tasks that end up compiling asm.js in turn may use Wasm compilation
  // threads to generate machine code.  We have no way (at present) to know
  // ahead of time whether a parse task is going to parse asm.js content or
  // not, so we just assume that all parse tasks are master tasks.
  return !parseWorklist(lock).empty() &&
         checkTaskThreadLimit<ParseTask*>(maxParseThreads(), /*isMaster=*/true);
}

bool GlobalHelperThreadState::canStartCompressionTask(
    const AutoLockHelperThreadState& lock) {
  return !compressionWorklist(lock).empty() &&
         checkTaskThreadLimit<SourceCompressionTask*>(maxCompressionThreads());
}

void GlobalHelperThreadState::startHandlingCompressionTasks(
    const AutoLockHelperThreadState& lock) {
  scheduleCompressionTasks(lock);
  if (canStartCompressionTask(lock)) {
    notifyOne(PRODUCER, lock);
  }
}

void GlobalHelperThreadState::scheduleCompressionTasks(
    const AutoLockHelperThreadState& lock) {
  auto& pending = compressionPendingList(lock);
  auto& worklist = compressionWorklist(lock);

  for (size_t i = 0; i < pending.length(); i++) {
    if (pending[i]->shouldStart()) {
      // OOMing during appending results in the task not being scheduled
      // and deleted.
      Unused << worklist.append(std::move(pending[i]));
      remove(pending, &i);
    }
  }
}

bool GlobalHelperThreadState::canStartGCParallelTask(
    const AutoLockHelperThreadState& lock) {
  return !gcParallelWorklist(lock).empty() &&
         checkTaskThreadLimit<GCParallelTask*>(maxGCParallelThreads());
}

js::GCParallelTask::~GCParallelTask() {
  // Only most-derived classes' destructors may do the join: base class
  // destructors run after those for derived classes' members, so a join in a
  // base class can't ensure that the task is done using the members. All we
  // can do now is check that someone has previously stopped the task.
  assertNotStarted();
}

bool js::GCParallelTask::startWithLockHeld(AutoLockHelperThreadState& lock) {
  MOZ_ASSERT(CanUseExtraThreads());
  assertNotStarted();

  // If we do the shutdown GC before running anything, we may never
  // have initialized the helper threads. Just use the serial path
  // since we cannot safely intialize them at this point.
  if (!HelperThreadState().threads) {
    return false;
  }

  if (!HelperThreadState().gcParallelWorklist(lock).append(this)) {
    return false;
  }
  setDispatched(lock);

  HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);

  return true;
}

bool js::GCParallelTask::start() {
  AutoLockHelperThreadState helperLock;
  return startWithLockHeld(helperLock);
}

void js::GCParallelTask::startOrRunIfIdle(AutoLockHelperThreadState& lock) {
  if (isRunningWithLockHeld(lock)) {
    return;
  }

  // Join the previous invocation of the task. This will return immediately
  // if the thread has never been started.
  joinWithLockHeld(lock);

  if (!(CanUseExtraThreads() && startWithLockHeld(lock))) {
    AutoUnlockHelperThreadState unlock(lock);
    runFromMainThread(runtime());
  }
}

void js::GCParallelTask::joinWithLockHeld(AutoLockHelperThreadState& lock) {
  if (isNotStarted(lock)) {
    return;
  }

  while (!isFinished(lock)) {
    HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
  }

  setNotStarted(lock);
  cancel_ = false;
}

void js::GCParallelTask::join() {
  AutoLockHelperThreadState helperLock;
  joinWithLockHeld(helperLock);
}

static inline TimeDuration TimeSince(TimeStamp prev) {
  TimeStamp now = ReallyNow();
  // Sadly this happens sometimes.
  MOZ_ASSERT(now >= prev);
  if (now < prev) {
    now = prev;
  }
  return now - prev;
}

void GCParallelTask::joinAndRunFromMainThread(JSRuntime* rt) {
  {
    AutoLockHelperThreadState lock;
    MOZ_ASSERT(!isRunningWithLockHeld(lock));
    joinWithLockHeld(lock);
  }

  runFromMainThread(rt);
}

void js::GCParallelTask::runFromMainThread(JSRuntime* rt) {
  assertNotStarted();
  MOZ_ASSERT(js::CurrentThreadCanAccessRuntime(rt));
  TimeStamp timeStart = ReallyNow();
  runTask();
  duration_ = TimeSince(timeStart);
}

void js::GCParallelTask::runFromHelperThread(AutoLockHelperThreadState& lock) {
  MOZ_ASSERT(isDispatched(lock));

  {
    AutoUnlockHelperThreadState parallelSection(lock);
    AutoSetHelperThreadContext usesContext;
    AutoSetContextRuntime ascr(runtime());
    gc::AutoSetThreadIsPerformingGC performingGC;
    TimeStamp timeStart = ReallyNow();
    runTask();
    duration_ = TimeSince(timeStart);
  }

  setFinished(lock);
  HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, lock);
}

bool js::GCParallelTask::isRunning() const {
  AutoLockHelperThreadState lock;
  return isRunningWithLockHeld(lock);
}

void HelperThread::handleGCParallelWorkload(AutoLockHelperThreadState& lock) {
  MOZ_ASSERT(HelperThreadState().canStartGCParallelTask(lock));
  MOZ_ASSERT(idle());

  TraceLoggerThread* logger = TraceLoggerForCurrentThread();
  AutoTraceLog logCompile(logger, TraceLogger_GC);

  currentTask.emplace(HelperThreadState().gcParallelWorklist(lock).popCopy());
  gcParallelTask()->runFromHelperThread(lock);
  currentTask.reset();
}

static void LeaveParseTaskZone(JSRuntime* rt, ParseTask* task) {
  // Mark the zone as no longer in use by a helper thread, and available
  // to be collected by the GC.
  rt->clearUsedByHelperThread(task->parseGlobal->zoneFromAnyThread());
}

ParseTask* GlobalHelperThreadState::removeFinishedParseTask(
    ParseTaskKind kind, JS::OffThreadToken* token) {
  // The token is really a ParseTask* which should be in the finished list.
  // Remove its entry.
  auto task = static_cast<ParseTask*>(token);
  MOZ_ASSERT(task->kind == kind);

  AutoLockHelperThreadState lock;

#ifdef DEBUG
  auto& finished = parseFinishedList(lock);
  bool found = false;
  for (auto t : finished) {
    if (t == task) {
      found = true;
      break;
    }
  }
  MOZ_ASSERT(found);
#endif

  task->remove();
  return task;
}

UniquePtr<ParseTask> GlobalHelperThreadState::finishParseTaskCommon(
    JSContext* cx, ParseTaskKind kind, JS::OffThreadToken* token) {
  MOZ_ASSERT(!cx->isHelperThreadContext());
  MOZ_ASSERT(cx->realm());

  Rooted<UniquePtr<ParseTask>> parseTask(cx,
                                         removeFinishedParseTask(kind, token));

  // Make sure we have all the constructors we need for the prototype
  // remapping below, since we can't GC while that's happening.
  if (!EnsureParserCreatedClasses(cx, kind)) {
    LeaveParseTaskZone(cx->runtime(), parseTask.get().get());
    return nullptr;
  }

  mergeParseTaskRealm(cx, parseTask.get().get(), cx->realm());

  for (auto& script : parseTask->scripts) {
    cx->releaseCheck(script);
  }

  for (auto& sourceObject : parseTask->sourceObjects) {
    RootedScriptSourceObject sso(cx, sourceObject);
    if (!ScriptSourceObject::initFromOptions(cx, sso, parseTask->options)) {
      return nullptr;
    }
    if (!sso->source()->tryCompressOffThread(cx)) {
      return nullptr;
    }
  }

  // Report out of memory errors eagerly, or errors could be malformed.
  if (parseTask->outOfMemory) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  // Report any error or warnings generated during the parse.
  for (size_t i = 0; i < parseTask->errors.length(); i++) {
    parseTask->errors[i]->throwError(cx);
  }
  if (parseTask->overRecursed) {
    ReportOverRecursed(cx);
  }
  if (cx->isExceptionPending()) {
    return nullptr;
  }

  return std::move(parseTask.get());
}

JSScript* GlobalHelperThreadState::finishSingleParseTask(
    JSContext* cx, ParseTaskKind kind, JS::OffThreadToken* token) {
  JS::RootedScript script(cx);

  Rooted<UniquePtr<ParseTask>> parseTask(
      cx, finishParseTaskCommon(cx, kind, token));
  if (!parseTask) {
    return nullptr;
  }

  MOZ_RELEASE_ASSERT(parseTask->scripts.length() <= 1);

  if (parseTask->scripts.length() > 0) {
    script = parseTask->scripts[0];
  }

  if (!script) {
    // No error was reported, but no script produced. Assume we hit out of
    // memory.
    MOZ_ASSERT(false, "Expected script");
    ReportOutOfMemory(cx);
    return nullptr;
  }

  // The Debugger only needs to be told about the topmost script that was
  // compiled.
  DebugAPI::onNewScript(cx, script);

  return script;
}

bool GlobalHelperThreadState::finishMultiParseTask(
    JSContext* cx, ParseTaskKind kind, JS::OffThreadToken* token,
    MutableHandle<ScriptVector> scripts) {
  Rooted<UniquePtr<ParseTask>> parseTask(
      cx, finishParseTaskCommon(cx, kind, token));
  if (!parseTask) {
    return false;
  }

  MOZ_ASSERT(parseTask->kind == ParseTaskKind::MultiScriptsDecode);
  auto task = static_cast<MultiScriptsDecodeTask*>(parseTask.get().get());
  size_t expectedLength = task->sources->length();

  if (!scripts.reserve(parseTask->scripts.length())) {
    ReportOutOfMemory(cx);
    return false;
  }

  for (auto& script : parseTask->scripts) {
    scripts.infallibleAppend(script);
  }

  if (scripts.length() != expectedLength) {
    // No error was reported, but fewer scripts produced than expected.
    // Assume we hit out of memory.
    MOZ_ASSERT(false, "Expected more scripts");
    ReportOutOfMemory(cx);
    return false;
  }

  // The Debugger only needs to be told about the topmost script that was
  // compiled.
  JS::RootedScript rooted(cx);
  for (auto& script : scripts) {
    MOZ_ASSERT(script->isGlobalCode());

    rooted = script;
    DebugAPI::onNewScript(cx, rooted);
  }

  return true;
}

JSScript* GlobalHelperThreadState::finishScriptParseTask(
    JSContext* cx, JS::OffThreadToken* token) {
  JSScript* script = finishSingleParseTask(cx, ParseTaskKind::Script, token);
  MOZ_ASSERT_IF(script, script->isGlobalCode());
  return script;
}

JSScript* GlobalHelperThreadState::finishScriptDecodeTask(
    JSContext* cx, JS::OffThreadToken* token) {
  JSScript* script =
      finishSingleParseTask(cx, ParseTaskKind::ScriptDecode, token);
  MOZ_ASSERT_IF(script, script->isGlobalCode());
  return script;
}

#if defined(JS_BUILD_BINAST)

JSScript* GlobalHelperThreadState::finishBinASTDecodeTask(
    JSContext* cx, JS::OffThreadToken* token) {
  JSScript* script = finishSingleParseTask(cx, ParseTaskKind::BinAST, token);
  MOZ_ASSERT_IF(script, script->isGlobalCode());
  return script;
}

#endif /* JS_BUILD_BINAST */

bool GlobalHelperThreadState::finishMultiScriptsDecodeTask(
    JSContext* cx, JS::OffThreadToken* token,
    MutableHandle<ScriptVector> scripts) {
  return finishMultiParseTask(cx, ParseTaskKind::MultiScriptsDecode, token,
                              scripts);
}

JSObject* GlobalHelperThreadState::finishModuleParseTask(
    JSContext* cx, JS::OffThreadToken* token) {
  JSScript* script = finishSingleParseTask(cx, ParseTaskKind::Module, token);
  if (!script) {
    return nullptr;
  }

  MOZ_ASSERT(script->module());

  RootedModuleObject module(cx, script->module());
  module->fixEnvironmentsAfterRealmMerge();
  if (!ModuleObject::Freeze(cx, module)) {
    return nullptr;
  }

  return module;
}

void GlobalHelperThreadState::cancelParseTask(JSRuntime* rt, ParseTaskKind kind,
                                              JS::OffThreadToken* token) {
  destroyParseTask(rt, removeFinishedParseTask(kind, token));
}

void GlobalHelperThreadState::destroyParseTask(JSRuntime* rt,
                                               ParseTask* parseTask) {
  MOZ_ASSERT(!parseTask->isInList());
  LeaveParseTaskZone(rt, parseTask);
  js_delete(parseTask);
}

void GlobalHelperThreadState::mergeParseTaskRealm(JSContext* cx,
                                                  ParseTask* parseTask,
                                                  Realm* dest) {
  // After we call LeaveParseTaskZone() it's not safe to GC until we have
  // finished merging the contents of the parse task's realm into the
  // destination realm.
  JS::AutoAssertNoGC nogc(cx);

  LeaveParseTaskZone(cx->runtime(), parseTask);

  // Move the parsed script and all its contents into the desired realm.
  gc::MergeRealms(parseTask->parseGlobal->as<GlobalObject>().realm(), dest);
}

void HelperThread::destroy() {
  if (thread.isSome()) {
    {
      AutoLockHelperThreadState lock;
      terminate = true;

      /* Notify all helpers, to ensure that this thread wakes up. */
      HelperThreadState().notifyAll(GlobalHelperThreadState::PRODUCER, lock);
    }

    thread->join();
    thread.reset();
  }
}

void HelperThread::ensureRegisteredWithProfiler() {
  if (profilingStack || mozilla::recordreplay::IsRecordingOrReplaying()) {
    return;
  }

  JS::RegisterThreadCallback callback = HelperThreadState().registerThread;
  if (callback) {
    profilingStack =
        callback("JS Helper", reinterpret_cast<void*>(GetNativeStackBase()));
  }
}

void HelperThread::unregisterWithProfilerIfNeeded() {
  if (!profilingStack) {
    return;
  }

  JS::UnregisterThreadCallback callback = HelperThreadState().unregisterThread;
  if (callback) {
    callback();
    profilingStack = nullptr;
  }
}

/* static */
void HelperThread::ThreadMain(void* arg) {
  ThisThread::SetName("JS Helper");

  // Helper threads are allowed to run differently during recording and
  // replay, as compiled scripts and GCs are allowed to vary. Because of
  // this, no recorded events at all should occur while on helper threads.
  mozilla::recordreplay::AutoDisallowThreadEvents d;

  static_cast<HelperThread*>(arg)->threadLoop();
  Mutex::ShutDown();
}

void HelperThread::handleWasmTier1Workload(AutoLockHelperThreadState& locked) {
  handleWasmWorkload(locked, wasm::CompileMode::Tier1);
}

void HelperThread::handleWasmTier2Workload(AutoLockHelperThreadState& locked) {
  handleWasmWorkload(locked, wasm::CompileMode::Tier2);
}

void HelperThread::handleWasmWorkload(AutoLockHelperThreadState& locked,
                                      wasm::CompileMode mode) {
  MOZ_ASSERT(HelperThreadState().canStartWasmCompile(locked, mode));
  MOZ_ASSERT(idle());

  currentTask.emplace(
      HelperThreadState().wasmWorklist(locked, mode).popCopyFront());

  wasm::CompileTask* task = wasmTask();
  {
    AutoUnlockHelperThreadState unlock(locked);
    task->runTask();
  }

  currentTask.reset();

  // Since currentTask is only now reset(), this could be the last active thread
  // waitForAllThreads() is waiting for. No one else should be waiting, though.
  HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
}

void HelperThread::handleWasmTier2GeneratorWorkload(
    AutoLockHelperThreadState& locked) {
  MOZ_ASSERT(HelperThreadState().canStartWasmTier2Generator(locked));
  MOZ_ASSERT(idle());

  currentTask.emplace(
      HelperThreadState().wasmTier2GeneratorWorklist(locked).popCopy());

  wasm::Tier2GeneratorTask* task = wasmTier2GeneratorTask();
  {
    AutoUnlockHelperThreadState unlock(locked);
    task->runTask();
  }

  currentTask.reset();
  js_delete(task);

  // During shutdown the main thread will wait for any ongoing (cancelled)
  // tier-2 generation to shut down normally.  To do so, it waits on the
  // CONSUMER condition for the count of finished generators to rise.
  HelperThreadState().incWasmTier2GeneratorsFinished(locked);
  HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
}

void HelperThread::handlePromiseHelperTaskWorkload(
    AutoLockHelperThreadState& locked) {
  MOZ_ASSERT(HelperThreadState().canStartPromiseHelperTask(locked));
  MOZ_ASSERT(idle());

  PromiseHelperTask* task =
      HelperThreadState().promiseHelperTasks(locked).popCopy();
  currentTask.emplace(task);

  {
    AutoUnlockHelperThreadState unlock(locked);
    task->runTask();
  }

  currentTask.reset();

  // Since currentTask is only now reset(), this could be the last active thread
  // waitForAllThreads() is waiting for. No one else should be waiting, though.
  HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
}

void HelperThread::handleIonWorkload(AutoLockHelperThreadState& locked) {
  MOZ_ASSERT(HelperThreadState().canStartIonCompile(locked));
  MOZ_ASSERT(idle());
  // Find the IonBuilder in the worklist with the highest priority, and
  // remove it from the worklist.
  jit::IonBuilder* builder =
      HelperThreadState().highestPriorityPendingIonCompile(locked);

  // The build is taken by this thread. Unfreeze the LifoAlloc to allow
  // mutations.
  builder->alloc().lifoAlloc()->setReadWrite();

  currentTask.emplace(builder);

  JSRuntime* rt = builder->script()->runtimeFromAnyThread();

  {
    AutoUnlockHelperThreadState unlock(locked);

    builder->runTask();
  }

  FinishOffThreadIonCompile(builder, locked);

  // Ping the main thread so that the compiled code can be incorporated at the
  // next interrupt callback.
  //
  // This must happen before the current task is reset. DestroyContext
  // cancels in progress Ion compilations before destroying its target
  // context, and after we reset the current task we are no longer considered
  // to be Ion compiling.
  rt->mainContextFromAnyThread()->requestInterrupt(
      InterruptReason::AttachIonCompilations);

  currentTask.reset();

  // Notify the main thread in case it is waiting for the compilation to finish.
  HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
}

void HelperThread::handleIonFreeWorkload(AutoLockHelperThreadState& locked) {
  MOZ_ASSERT(idle());
  MOZ_ASSERT(HelperThreadState().canStartIonFreeTask(locked));

  auto& freeList = HelperThreadState().ionFreeList(locked);

  jit::IonBuilder* builder = freeList.popCopy();
  {
    AutoUnlockHelperThreadState unlock(locked);
    FreeIonBuilder(builder);
  }
}

HelperThread* js::CurrentHelperThread() {
  if (!HelperThreadState().threads) {
    return nullptr;
  }
  auto threadId = ThisThread::GetId();
  for (auto& thisThread : *HelperThreadState().threads) {
    if (thisThread.thread.isSome() && threadId == thisThread.thread->get_id()) {
      return &thisThread;
    }
  }
  return nullptr;
}

bool JSContext::addPendingCompileError(js::CompileError** error) {
  auto errorPtr = make_unique<js::CompileError>();
  if (!errorPtr) {
    return false;
  }
  if (!parseTask_->errors.append(std::move(errorPtr))) {
    ReportOutOfMemory(this);
    return false;
  }
  *error = parseTask_->errors.back().get();
  return true;
}

bool JSContext::isCompileErrorPending() const {
  return parseTask_->errors.length() > 0;
}

void JSContext::addPendingOverRecursed() {
  if (parseTask_) {
    parseTask_->overRecursed = true;
  }
}

void JSContext::addPendingOutOfMemory() {
  // Keep in sync with recoverFromOutOfMemory.
  if (parseTask_) {
    parseTask_->outOfMemory = true;
  }
}

void HelperThread::handleParseWorkload(AutoLockHelperThreadState& locked) {
  MOZ_ASSERT(HelperThreadState().canStartParseTask(locked));
  MOZ_ASSERT(idle());

  currentTask.emplace(HelperThreadState().parseWorklist(locked).popCopy());
  ParseTask* task = parseTask();

#ifdef DEBUG
  JSRuntime* runtime = task->parseGlobal->runtimeFromAnyThread();
  runtime->incOffThreadParsesRunning();
#endif

  {
    AutoUnlockHelperThreadState unlock(locked);
    task->runTask();
  }
  // The callback is invoked while we are still off thread.
  task->callback(task, task->callbackData);

  // FinishOffThreadScript will need to be called on the script to
  // migrate it into the correct compartment.
  HelperThreadState().parseFinishedList(locked).insertBack(task);

#ifdef DEBUG
  runtime->decOffThreadParsesRunning();
#endif

  currentTask.reset();

  // Notify the main thread in case it is waiting for the parse/emit to finish.
  HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
}

void HelperThread::handleCompressionWorkload(
    AutoLockHelperThreadState& locked) {
  MOZ_ASSERT(HelperThreadState().canStartCompressionTask(locked));
  MOZ_ASSERT(idle());

  UniquePtr<SourceCompressionTask> task;
  {
    auto& worklist = HelperThreadState().compressionWorklist(locked);
    task = std::move(worklist.back());
    worklist.popBack();
    currentTask.emplace(task.get());
  }

  {
    AutoUnlockHelperThreadState unlock(locked);

    TraceLoggerThread* logger = TraceLoggerForCurrentThread();
    AutoTraceLog logCompile(logger, TraceLogger_CompressSource);

    task->runTask();
  }

  {
    AutoEnterOOMUnsafeRegion oomUnsafe;
    if (!HelperThreadState().compressionFinishedList(locked).append(
            std::move(task))) {
      oomUnsafe.crash("handleCompressionWorkload");
    }
  }

  currentTask.reset();

  // Notify the main thread in case it is waiting for the compression to finish.
  HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
}

bool js::EnqueueOffThreadCompression(JSContext* cx,
                                     UniquePtr<SourceCompressionTask> task) {
  AutoLockHelperThreadState lock;

  auto& pending = HelperThreadState().compressionPendingList(lock);
  if (!pending.append(std::move(task))) {
    if (!cx->isHelperThreadContext()) {
      ReportOutOfMemory(cx);
    }
    return false;
  }

  return true;
}

template <typename T>
static void ClearCompressionTaskList(T& list, JSRuntime* runtime) {
  for (size_t i = 0; i < list.length(); i++) {
    if (list[i]->runtimeMatches(runtime)) {
      HelperThreadState().remove(list, &i);
    }
  }
}

void js::CancelOffThreadCompressions(JSRuntime* runtime) {
  AutoLockHelperThreadState lock;

  if (!HelperThreadState().threads) {
    return;
  }

  // Cancel all pending compression tasks.
  ClearCompressionTaskList(HelperThreadState().compressionPendingList(lock),
                           runtime);
  ClearCompressionTaskList(HelperThreadState().compressionWorklist(lock),
                           runtime);

  // Cancel all in-process compression tasks and wait for them to join so we
  // clean up the finished tasks.
  while (true) {
    bool inProgress = false;
    for (auto& thread : *HelperThreadState().threads) {
      SourceCompressionTask* task = thread.compressionTask();
      if (task && task->runtimeMatches(runtime)) {
        inProgress = true;
      }
    }

    if (!inProgress) {
      break;
    }

    HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
  }

  // Clean up finished tasks.
  ClearCompressionTaskList(HelperThreadState().compressionFinishedList(lock),
                           runtime);
}

void js::AttachFinishedCompressions(JSRuntime* runtime,
                                    AutoLockHelperThreadState& lock) {
  auto& finished = HelperThreadState().compressionFinishedList(lock);
  for (size_t i = 0; i < finished.length(); i++) {
    if (finished[i]->runtimeMatches(runtime)) {
      UniquePtr<SourceCompressionTask> compressionTask(std::move(finished[i]));
      HelperThreadState().remove(finished, &i);
      compressionTask->complete();
    }
  }
}

void js::RunPendingSourceCompressions(JSRuntime* runtime) {
  AutoLockHelperThreadState lock;

  if (!HelperThreadState().threads) {
    return;
  }

  HelperThreadState().startHandlingCompressionTasks(lock);

  // Wait for all in-process compression tasks to complete.
  while (!HelperThreadState().compressionWorklist(lock).empty()) {
    HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
  }

  AttachFinishedCompressions(runtime, lock);
}

void PromiseHelperTask::executeAndResolveAndDestroy(JSContext* cx) {
  execute();
  run(cx, JS::Dispatchable::NotShuttingDown);
}

void PromiseHelperTask::runTask() {
  execute();
  dispatchResolveAndDestroy();
}

bool js::StartOffThreadPromiseHelperTask(JSContext* cx,
                                         UniquePtr<PromiseHelperTask> task) {
  // Execute synchronously if there are no helper threads.
  if (!CanUseExtraThreads()) {
    task.release()->executeAndResolveAndDestroy(cx);
    return true;
  }

  AutoLockHelperThreadState lock;

  if (!HelperThreadState().promiseHelperTasks(lock).append(task.get())) {
    ReportOutOfMemory(cx);
    return false;
  }

  Unused << task.release();

  HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);
  return true;
}

bool js::StartOffThreadPromiseHelperTask(PromiseHelperTask* task) {
  MOZ_ASSERT(CanUseExtraThreads());

  AutoLockHelperThreadState lock;

  if (!HelperThreadState().promiseHelperTasks(lock).append(task)) {
    return false;
  }

  HelperThreadState().notifyOne(GlobalHelperThreadState::PRODUCER, lock);
  return true;
}

void GlobalHelperThreadState::trace(JSTracer* trc) {
  AutoLockHelperThreadState lock;
  for (auto builder : ionWorklist(lock)) {
    builder->alloc().lifoAlloc()->setReadWrite();
    builder->trace(trc);
    builder->alloc().lifoAlloc()->setReadOnly();
  }
  for (auto builder : ionFinishedList(lock)) {
    builder->trace(trc);
  }

  if (HelperThreadState().threads) {
    for (auto& helper : *HelperThreadState().threads) {
      if (auto builder = helper.ionBuilder()) {
        builder->trace(trc);
      }
    }
  }

  JSRuntime* rt = trc->runtime();
  if (auto* jitRuntime = rt->jitRuntime()) {
    jit::IonBuilder* builder = jitRuntime->ionLazyLinkList(rt).getFirst();
    while (builder) {
      builder->trace(trc);
      builder = builder->getNext();
    }
  }

  for (auto parseTask : parseWorklist_) {
    parseTask->trace(trc);
  }
  for (auto parseTask : parseFinishedList_) {
    parseTask->trace(trc);
  }
  for (auto parseTask : parseWaitingOnGC_) {
    parseTask->trace(trc);
  }
}

// Definition of helper thread tasks.
//
// Priority is determined by the order they're listed here.
const HelperThread::TaskSpec HelperThread::taskSpecs[] = {
    {THREAD_TYPE_GCPARALLEL, &GlobalHelperThreadState::canStartGCParallelTask,
     &HelperThread::handleGCParallelWorkload},
    {THREAD_TYPE_ION, &GlobalHelperThreadState::canStartIonCompile,
     &HelperThread::handleIonWorkload},
    {THREAD_TYPE_WASM, &GlobalHelperThreadState::canStartWasmTier1Compile,
     &HelperThread::handleWasmTier1Workload},
    {THREAD_TYPE_PROMISE_TASK,
     &GlobalHelperThreadState::canStartPromiseHelperTask,
     &HelperThread::handlePromiseHelperTaskWorkload},
    {THREAD_TYPE_PARSE, &GlobalHelperThreadState::canStartParseTask,
     &HelperThread::handleParseWorkload},
    {THREAD_TYPE_COMPRESS, &GlobalHelperThreadState::canStartCompressionTask,
     &HelperThread::handleCompressionWorkload},
    {THREAD_TYPE_ION_FREE, &GlobalHelperThreadState::canStartIonFreeTask,
     &HelperThread::handleIonFreeWorkload},
    {THREAD_TYPE_WASM, &GlobalHelperThreadState::canStartWasmTier2Compile,
     &HelperThread::handleWasmTier2Workload},
    {THREAD_TYPE_WASM_TIER2,
     &GlobalHelperThreadState::canStartWasmTier2Generator,
     &HelperThread::handleWasmTier2GeneratorWorkload}};

HelperThread::AutoProfilerLabel::AutoProfilerLabel(
    HelperThread* helperThread, const char* label,
    JS::ProfilingCategoryPair categoryPair)
    : profilingStack(helperThread->profilingStack) {
  if (profilingStack) {
    profilingStack->pushLabelFrame(label, nullptr, this, categoryPair);
  }
}

HelperThread::AutoProfilerLabel::~AutoProfilerLabel() {
  if (profilingStack) {
    profilingStack->pop();
  }
}

void HelperThread::threadLoop() {
  MOZ_ASSERT(CanUseExtraThreads());

  JS::AutoSuppressGCAnalysis nogc;
  AutoLockHelperThreadState lock;

  ensureRegisteredWithProfiler();

  while (!terminate) {
    MOZ_ASSERT(idle());

    // The selectors may depend on the HelperThreadState not changing
    // between task selection and task execution, in particular, on new
    // tasks not being added (because of the lifo structure of the work
    // lists). Unlocking the HelperThreadState between task selection and
    // execution is not well-defined.

    const TaskSpec* task = findHighestPriorityTask(lock);
    if (!task) {
      AUTO_PROFILER_LABEL("HelperThread::threadLoop::wait", IDLE);
      HelperThreadState().wait(lock, GlobalHelperThreadState::PRODUCER);
      continue;
    }

    js::oom::SetThreadType(task->type);
    (this->*(task->handleWorkload))(lock);
    js::oom::SetThreadType(js::THREAD_TYPE_NONE);
  }

  unregisterWithProfilerIfNeeded();
}

const HelperThread::TaskSpec* HelperThread::findHighestPriorityTask(
    const AutoLockHelperThreadState& locked) {
  // Return the highest priority task that is ready to start, or nullptr.

  for (const auto& task : taskSpecs) {
    if ((HelperThreadState().*(task.canStart))(locked)) {
      return &task;
    }
  }

  return nullptr;
}
