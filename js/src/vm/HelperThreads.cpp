/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/HelperThreads.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include <algorithm>

#include "frontend/BytecodeCompilation.h"
#include "frontend/CompilationStencil.h"  // frontend::{CompilationStencil, ExtensibleCompilationStencil, CompilationInput, CompilationGCOutput, BorrowingCompilationStencil}
#include "frontend/ParserAtom.h"          // frontend::ParserAtomsTable
#include "gc/GC.h"                        // gc::MergeRealms
#include "jit/IonCompileTask.h"
#include "jit/JitRuntime.h"
#include "js/ContextOptions.h"      // JS::ContextOptions
#include "js/friend/StackLimits.h"  // js::ReportOverRecursed
#include "js/OffThreadScriptCompilation.h"  // JS::OffThreadToken, JS::OffThreadCompileCallback
#include "js/SourceText.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "threading/CpuCount.h"
#include "util/NativeStack.h"
#include "vm/ErrorReporting.h"
#include "vm/HelperThreadState.h"
#include "vm/MutexIDs.h"
#include "vm/SharedImmutableStringsCache.h"
#include "vm/Time.h"
#include "vm/TraceLogging.h"
#include "vm/Xdr.h"
#include "wasm/WasmGenerator.h"

#include "debugger/DebugAPI-inl.h"
#include "gc/ArenaList-inl.h"
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

Mutex gHelperThreadLock(mutexid::GlobalHelperThreadState);
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
  if (!gHelperThreadState->ensureContextList(gHelperThreadState->threadCount)) {
    js_delete(gHelperThreadState);
    gHelperThreadState = nullptr;
    return false;
  }
  return true;
}

void js::DestroyHelperThreadsState() {
  if (!gHelperThreadState) {
    return;
  }

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
  return std::min<size_t>(cpuCount, 8);
}

static size_t ThreadCountForCPUCount(size_t cpuCount) {
  // We need at least two threads for tier-2 wasm compilations, because
  // there's a master task that holds a thread while other threads do the
  // compilation.
  return std::max<size_t>(cpuCount, 2);
}

bool js::SetFakeCPUCount(size_t count) {
  // This must be called before the threads have been initialized.
  AutoLockHelperThreadState lock;
  MOZ_ASSERT(HelperThreadState().threads(lock).empty());

  HelperThreadState().cpuCount = count;
  HelperThreadState().threadCount = ThreadCountForCPUCount(count);
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
  return HelperThreadState().submitTask(task, mode);
}

bool GlobalHelperThreadState::submitTask(wasm::CompileTask* task,
                                         wasm::CompileMode mode) {
  AutoLockHelperThreadState lock;
  if (!wasmWorklist(lock, mode).pushBack(task)) {
    return false;
  }

  dispatch(lock);
  return true;
}

size_t js::RemovePendingWasmCompileTasks(
    const wasm::CompileTaskState& taskState, wasm::CompileMode mode,
    const AutoLockHelperThreadState& lock) {
  wasm::CompileTaskPtrFifo& worklist =
      HelperThreadState().wasmWorklist(lock, mode);
  return worklist.eraseIf([&taskState](wasm::CompileTask* task) {
    return &task->state == &taskState;
  });
}

void js::StartOffThreadWasmTier2Generator(wasm::UniqueTier2GeneratorTask task) {
  Unused << HelperThreadState().submitTask(std::move(task));
}

bool GlobalHelperThreadState::submitTask(wasm::UniqueTier2GeneratorTask task) {
  MOZ_ASSERT(CanUseExtraThreads());

  AutoLockHelperThreadState lock;
  if (!wasmTier2GeneratorWorklist(lock).append(task.get())) {
    return false;
  }
  Unused << task.release();

  dispatch(lock);
  return true;
}

static void CancelOffThreadWasmTier2GeneratorLocked(
    AutoLockHelperThreadState& lock) {
  if (HelperThreadState().threads(lock).empty()) {
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
  for (auto* helper : HelperThreadState().helperTasks(lock)) {
    if (helper->is<wasm::Tier2GeneratorTask>()) {
      // Set a flag that causes compilation to shortcut itself.
      helper->as<wasm::Tier2GeneratorTask>()->cancel();

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

bool js::StartOffThreadIonCompile(jit::IonCompileTask* task,
                                  const AutoLockHelperThreadState& lock) {
  return HelperThreadState().submitTask(task, lock);
}

bool GlobalHelperThreadState::submitTask(
    jit::IonCompileTask* task, const AutoLockHelperThreadState& locked) {
  MOZ_ASSERT(CanUseExtraThreads());

  if (!ionWorklist(locked).append(task)) {
    return false;
  }

  // The build is moving off-thread. Freeze the LifoAlloc to prevent any
  // unwanted mutations.
  task->alloc().lifoAlloc()->setReadOnly();

  dispatch(locked);
  return true;
}

bool js::StartOffThreadIonFree(jit::IonCompileTask* task,
                               const AutoLockHelperThreadState& lock) {
  js::UniquePtr<jit::IonFreeTask> freeTask =
      js::MakeUnique<jit::IonFreeTask>(task);
  if (!freeTask) {
    return false;
  }

  return HelperThreadState().submitTask(std::move(freeTask), lock);
}

bool GlobalHelperThreadState::submitTask(
    UniquePtr<jit::IonFreeTask> task, const AutoLockHelperThreadState& locked) {
  MOZ_ASSERT(CanUseExtraThreads());

  if (!ionFreeList(locked).append(std::move(task))) {
    return false;
  }

  dispatch(locked);
  return true;
}

/*
 * Move an IonCompilationTask for which compilation has either finished, failed,
 * or been cancelled into the global finished compilation list. All off thread
 * compilations which are started must eventually be finished.
 */
void js::FinishOffThreadIonCompile(jit::IonCompileTask* task,
                                   const AutoLockHelperThreadState& lock) {
  AutoEnterOOMUnsafeRegion oomUnsafe;
  if (!HelperThreadState().ionFinishedList(lock).append(task)) {
    oomUnsafe.crash("FinishOffThreadIonCompile");
  }
  task->script()
      ->runtimeFromAnyThread()
      ->jitRuntime()
      ->numFinishedOffThreadTasksRef(lock)++;
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
  };

  return selector.match(Matcher());
}

static bool IonCompileTaskMatches(const CompilationSelector& selector,
                                  jit::IonCompileTask* task) {
  struct TaskMatches {
    jit::IonCompileTask* task_;

    bool operator()(JSScript* script) { return script == task_->script(); }
    bool operator()(Realm* realm) { return realm == task_->script()->realm(); }
    bool operator()(Zone* zone) {
      return zone == task_->script()->zoneFromAnyThread();
    }
    bool operator()(JSRuntime* runtime) {
      return runtime == task_->script()->runtimeFromAnyThread();
    }
    bool operator()(ZonesInState zbs) {
      return zbs.runtime == task_->script()->runtimeFromAnyThread() &&
             zbs.state == task_->script()->zoneFromAnyThread()->gcState();
    }
  };

  return selector.match(TaskMatches{task});
}

static void CancelOffThreadIonCompileLocked(const CompilationSelector& selector,
                                            AutoLockHelperThreadState& lock) {
  if (HelperThreadState().threads(lock).empty()) {
    return;
  }

  /* Cancel any pending entries for which processing hasn't started. */
  GlobalHelperThreadState::IonCompileTaskVector& worklist =
      HelperThreadState().ionWorklist(lock);
  for (size_t i = 0; i < worklist.length(); i++) {
    jit::IonCompileTask* task = worklist[i];
    if (IonCompileTaskMatches(selector, task)) {
      // Once finished, tasks are added to a Linked list which is
      // allocated with the IonCompileTask class. The IonCompileTask is
      // allocated in the LifoAlloc so we need the LifoAlloc to be mutable.
      worklist[i]->alloc().lifoAlloc()->setReadWrite();

      FinishOffThreadIonCompile(task, lock);
      HelperThreadState().remove(worklist, &i);
    }
  }

  /* Wait for in progress entries to finish up. */
  bool cancelled;
  do {
    cancelled = false;
    for (auto* helper : HelperThreadState().helperTasks(lock)) {
      if (!helper->is<jit::IonCompileTask>()) {
        continue;
      }

      jit::IonCompileTask* ionCompileTask = helper->as<jit::IonCompileTask>();
      if (IonCompileTaskMatches(selector, ionCompileTask)) {
        ionCompileTask->mirGen().cancel();
        cancelled = true;
      }
    }
    if (cancelled) {
      HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
    }
  } while (cancelled);

  /* Cancel code generation for any completed entries. */
  GlobalHelperThreadState::IonCompileTaskVector& finished =
      HelperThreadState().ionFinishedList(lock);
  for (size_t i = 0; i < finished.length(); i++) {
    jit::IonCompileTask* task = finished[i];
    if (IonCompileTaskMatches(selector, task)) {
      JSRuntime* rt = task->script()->runtimeFromAnyThread();
      rt->jitRuntime()->numFinishedOffThreadTasksRef(lock)--;
      jit::FinishOffThreadTask(rt, task, lock);
      HelperThreadState().remove(finished, &i);
    }
  }

  /* Cancel lazy linking for pending tasks (attached to the ionScript). */
  JSRuntime* runtime = GetSelectorRuntime(selector);
  jit::IonCompileTask* task =
      runtime->jitRuntime()->ionLazyLinkList(runtime).getFirst();
  while (task) {
    jit::IonCompileTask* next = task->getNext();
    if (IonCompileTaskMatches(selector, task)) {
      jit::FinishOffThreadTask(runtime, task, lock);
    }
    task = next;
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

  if (HelperThreadState().threads(lock).empty()) {
    return false;
  }

  GlobalHelperThreadState::IonCompileTaskVector& worklist =
      HelperThreadState().ionWorklist(lock);
  for (size_t i = 0; i < worklist.length(); i++) {
    jit::IonCompileTask* task = worklist[i];
    if (task->script()->realm() == realm) {
      return true;
    }
  }

  for (auto* helper : HelperThreadState().helperTasks(lock)) {
    if (helper->is<jit::IonCompileTask>() &&
        helper->as<jit::IonCompileTask>()->script()->realm() == realm) {
      return true;
    }
  }

  GlobalHelperThreadState::IonCompileTaskVector& finished =
      HelperThreadState().ionFinishedList(lock);
  for (size_t i = 0; i < finished.length(); i++) {
    jit::IonCompileTask* task = finished[i];
    if (task->script()->realm() == realm) {
      return true;
    }
  }

  JSRuntime* rt = realm->runtimeFromMainThread();
  jit::IonCompileTask* task = rt->jitRuntime()->ionLazyLinkList(rt).getFirst();
  while (task) {
    if (task->script()->realm() == realm) {
      return true;
    }
    task = task->getNext();
  }

  return false;
}
#endif

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

AutoSetHelperThreadContext::AutoSetHelperThreadContext(
    AutoLockHelperThreadState& lock)
    : lock(lock) {
  cx = HelperThreadState().getFirstUnusedContext(lock);
  MOZ_ASSERT(cx);
  cx->setHelperThread(lock);
  // When we set the JSContext, we need to reset the computed stack limits for
  // the current thread, so we also set the native stack quota.
  JS_SetNativeStackQuota(cx, HELPER_STACK_QUOTA);
}

AutoSetHelperThreadContext::~AutoSetHelperThreadContext() {
  cx->tempLifoAlloc().releaseAll();
  if (cx->shouldFreeUnusedMemory()) {
    cx->tempLifoAlloc().freeAll();
    cx->setFreeUnusedMemory(false);
  }
  cx->clearHelperThread(lock);
  cx = nullptr;
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

  runtime = cx->runtime();
  parseGlobal = global;

  return true;
}

void ParseTask::activate(JSRuntime* rt) {
  rt->addParseTaskRef();
  if (parseGlobal) {
    rt->setUsedByHelperThread(parseGlobal->zone());
  }
}

ParseTask::~ParseTask() = default;

void ParseTask::trace(JSTracer* trc) {
  if (runtime != trc->runtime()) {
    return;
  }

  if (parseGlobal) {
    Zone* zone = MaybeForwarded(parseGlobal)->zoneFromAnyThread();
    if (zone->usedByHelperThread()) {
      MOZ_ASSERT(!zone->isCollecting());
      return;
    }
  }

  TraceNullableRoot(trc, &parseGlobal, "ParseTask::parseGlobal");
  scripts.trace(trc);
  sourceObjects.trace(trc);

  if (stencilInput_) {
    stencilInput_->trace(trc);
  }

  gcOutput_.trace(trc);
}

size_t ParseTask::sizeOfExcludingThis(
    mozilla::MallocSizeOf mallocSizeOf) const {
  size_t stencilInputSize =
      stencilInput_ ? stencilInput_->sizeOfIncludingThis(mallocSizeOf) : 0;
  size_t stencilSize =
      stencil_ ? stencil_->sizeOfIncludingThis(mallocSizeOf) : 0;
  size_t extensibleStencilSize =
      extensibleStencil_ ? extensibleStencil_->sizeOfIncludingThis(mallocSizeOf)
                         : 0;

  // TODO: 'errors' requires adding support to `CompileError`. They are not
  // common though.

  return options.sizeOfExcludingThis(mallocSizeOf) +
         scripts.sizeOfExcludingThis(mallocSizeOf) +
         sourceObjects.sizeOfExcludingThis(mallocSizeOf) + stencilInputSize +
         stencilSize + extensibleStencilSize +
         gcOutput_.sizeOfExcludingThis(mallocSizeOf);
}

void ParseTask::runHelperThreadTask(AutoLockHelperThreadState& locked) {
#ifdef DEBUG
  if (parseGlobal) {
    runtime->incOffThreadParsesRunning();
  }
#endif

  runTask(locked);

  // The callback is invoked while we are still off thread.
  callback(this, callbackData);

  // FinishOffThreadScript will need to be called on the script to
  // migrate it into the correct compartment.
  HelperThreadState().parseFinishedList(locked).insertBack(this);

#ifdef DEBUG
  if (parseGlobal) {
    runtime->decOffThreadParsesRunning();
  }
#endif
}

void ParseTask::runTask(AutoLockHelperThreadState& lock) {
  AutoSetHelperThreadContext usesContext(lock);

  AutoUnlockHelperThreadState unlock(lock);

  JSContext* cx = TlsContext.get();

  AutoSetContextRuntime ascr(runtime);
  AutoSetContextParse parsetask(this);
  gc::AutoSuppressNurseryCellAlloc noNurseryAlloc(cx);

  Zone* zone = nullptr;
  if (parseGlobal) {
    zone = parseGlobal->zoneFromAnyThread();
    zone->setHelperThreadOwnerContext(cx);
  }

  auto resetOwnerContext = mozilla::MakeScopeExit([&] {
    if (zone) {
      zone->setHelperThreadOwnerContext(nullptr);
    }
  });

  Maybe<AutoRealm> ar;
  if (parseGlobal) {
    ar.emplace(cx, parseGlobal);
  }

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

  ScopeKind scopeKind =
      options.nonSyntacticScope ? ScopeKind::NonSyntactic : ScopeKind::Global;

  stencilInput_ = cx->make_unique<frontend::CompilationInput>(options);

  if (stencilInput_) {
    extensibleStencil_ = frontend::CompileGlobalScriptToExtensibleStencil(
        cx, *stencilInput_, data, scopeKind);
  }

  if (extensibleStencil_) {
    frontend::BorrowingCompilationStencil borrowingStencil(*extensibleStencil_);
    if (!frontend::PrepareForInstantiate(cx, *stencilInput_, borrowingStencil,
                                         gcOutput_)) {
      extensibleStencil_ = nullptr;
    }
  }

  if (options.useOffThreadParseGlobal) {
    Unused << instantiateStencils(cx);
  }
}

bool ParseTask::instantiateStencils(JSContext* cx) {
  if (!stencil_ && !extensibleStencil_) {
    return false;
  }

  bool result;
  if (stencil_) {
    result =
        frontend::InstantiateStencils(cx, *stencilInput_, *stencil_, gcOutput_);
  } else {
    frontend::BorrowingCompilationStencil borrowingStencil(*extensibleStencil_);
    result = frontend::InstantiateStencils(cx, *stencilInput_, borrowingStencil,
                                           gcOutput_);
  }

  // Whatever happens to the top-level script compilation (even if it fails),
  // we must finish initializing the SSO.  This is because there may be valid
  // inner scripts observable by the debugger which reference the partially-
  // initialized SSO.
  if (gcOutput_.sourceObject) {
    sourceObjects.infallibleAppend(gcOutput_.sourceObject);
  }

  if (result) {
    MOZ_ASSERT(gcOutput_.script);
    MOZ_ASSERT_IF(gcOutput_.module,
                  gcOutput_.module->script() == gcOutput_.script);
    scripts.infallibleAppend(gcOutput_.script);
  }

  return result;
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

  options.setModule();

  stencilInput_ = cx->make_unique<frontend::CompilationInput>(options);

  if (stencilInput_) {
    extensibleStencil_ =
        frontend::ParseModuleToExtensibleStencil(cx, *stencilInput_, data);
  }

  if (extensibleStencil_) {
    frontend::BorrowingCompilationStencil borrowingStencil(*extensibleStencil_);
    if (!frontend::PrepareForInstantiate(cx, *stencilInput_, borrowingStencil,
                                         gcOutput_)) {
      extensibleStencil_ = nullptr;
    }
  }

  if (options.useOffThreadParseGlobal) {
    Unused << instantiateStencils(cx);
  }
}

ScriptDecodeTask::ScriptDecodeTask(JSContext* cx,
                                   const JS::TranscodeRange& range,
                                   JS::OffThreadCompileCallback callback,
                                   void* callbackData)
    : ParseTask(ParseTaskKind::ScriptDecode, cx, callback, callbackData),
      range(range) {
  MOZ_ASSERT(JS::IsTranscodingBytecodeAligned(range.begin().get()));
}

void ScriptDecodeTask::parse(JSContext* cx) {
  MOZ_ASSERT(cx->isHelperThreadContext());

  RootedScript resultScript(cx);
  Rooted<ScriptSourceObject*> sourceObject(cx);

  if (options.useStencilXDR) {
    // The buffer contains stencil.

    stencilInput_ = cx->make_unique<frontend::CompilationInput>(options);
    if (!stencilInput_) {
      return;
    }
    if (!stencilInput_->initForGlobal(cx)) {
      return;
    }

    stencil_ =
        cx->make_unique<frontend::CompilationStencil>(stencilInput_->source);
    if (!stencil_) {
      return;
    }

    XDRStencilDecoder decoder(cx, range);
    XDRResult res = decoder.codeStencil(*stencilInput_, *stencil_);
    if (!res.isOk()) {
      stencil_.reset();
      return;
    }

    if (!frontend::PrepareForInstantiate(cx, *stencilInput_, *stencil_,
                                         gcOutput_)) {
      stencil_.reset();
    }

    if (options.useOffThreadParseGlobal) {
      Unused << instantiateStencils(cx);
    }

    return;
  }

  // The buffer contains JSScript.
  auto decoder = js::MakeUnique<XDROffThreadDecoder>(
      cx, &options, XDROffThreadDecoder::Type::Single,
      /* sourceObjectOut = */ &sourceObject.get(), range);
  if (!decoder) {
    ReportOutOfMemory(cx);
    return;
  }

  mozilla::DebugOnly<XDRResult> res = decoder->codeScript(&resultScript);
  MOZ_ASSERT(bool(resultScript) == static_cast<const XDRResult&>(res).isOk());

  if (sourceObject) {
    sourceObjects.infallibleAppend(sourceObject);
  }

  if (resultScript) {
    scripts.infallibleAppend(resultScript);
  }
}

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

    auto decoder = js::MakeUnique<XDROffThreadDecoder>(
        cx, &opts, XDROffThreadDecoder::Type::Multi, &sourceObject.get(),
        source.range);
    if (!decoder) {
      ReportOutOfMemory(cx);
      return;
    }

    mozilla::DebugOnly<XDRResult> res = decoder->codeScript(&resultScript);
    MOZ_ASSERT(bool(resultScript) == static_cast<const XDRResult&>(res).isOk());

    if (sourceObject) {
      sourceObjects.infallibleAppend(sourceObject);
    }

    if (resultScript) {
      scripts.infallibleAppend(resultScript);
    } else {
      // If any decodes fail, don't process the rest. We likely are hitting OOM.
      break;
    }
  }
}

static void WaitForOffThreadParses(JSRuntime* rt,
                                   AutoLockHelperThreadState& lock) {
  if (HelperThreadState().threads(lock).empty()) {
    return;
  }

  GlobalHelperThreadState::ParseTaskVector& worklist =
      HelperThreadState().parseWorklist(lock);

  while (true) {
    bool pending = false;
    for (const auto& task : worklist) {
      if (task->runtimeMatches(rt)) {
        pending = true;
        break;
      }
    }
    if (!pending) {
      bool inProgress = false;
      for (auto* helper : HelperThreadState().helperTasks(lock)) {
        if (helper->is<ParseTask>() &&
            helper->as<ParseTask>()->runtimeMatches(rt)) {
          inProgress = true;
          break;
        }
      }
      if (!inProgress) {
        break;
      }
    }
    HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
  }

#ifdef DEBUG
  for (const auto& task : worklist) {
    MOZ_ASSERT(!task->runtimeMatches(rt));
  }
  for (auto* helper : HelperThreadState().helperTasks(lock)) {
    MOZ_ASSERT_IF(helper->is<ParseTask>(),
                  !helper->as<ParseTask>()->runtimeMatches(rt));
  }
#endif
}

void js::WaitForOffThreadParses(JSRuntime* rt) {
  AutoLockHelperThreadState lock;
  WaitForOffThreadParses(rt, lock);
}

void js::CancelOffThreadParses(JSRuntime* rt) {
  AutoLockHelperThreadState lock;

#ifdef DEBUG
  for (const auto& task : HelperThreadState().parseWaitingOnGC(lock)) {
    MOZ_ASSERT(!task->runtimeMatches(rt));
  }
#endif

  // Instead of forcibly canceling pending parse tasks, just wait for all
  // scheduled and in progress ones to complete. Otherwise the final GC may not
  // collect everything due to zones being used off thread.
  WaitForOffThreadParses(rt, lock);

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
  for (ParseTask* task : finished) {
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

  // Set the used-as-prototype flag here because we can't GC in mergeRealms.
  RootedObject proto(cx, &global->getPrototype(key).toObject());
  return JSObject::setIsUsedAsPrototype(cx, proto);
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

  if (!EnsureConstructor(cx, global, JSProto_GeneratorFunction)) {
    return false;  // needed by function*() {}
  }

  if (!EnsureConstructor(cx, global, JSProto_AsyncFunction)) {
    return false;  // needed by async function() {}
  }

  if (!EnsureConstructor(cx, global, JSProto_AsyncGeneratorFunction)) {
    return false;  // needed by async function*() {}
  }

  if (kind == ParseTaskKind::Module) {
    // Set the used-as-prototype flag on the prototype objects because we can't
    // GC in mergeRealms.
    bool setUsedAsPrototype = true;
    if (!GlobalObject::ensureModulePrototypesCreated(cx, global,
                                                     setUsedAsPrototype)) {
      return false;
    }
  }

  return true;
}

class MOZ_RAII AutoSetCreatedForHelperThread {
  Zone* zone;

 public:
  explicit AutoSetCreatedForHelperThread(JSObject* global)
      : zone(global ? global->zone() : nullptr) {
    if (zone) {
      zone->setCreatedForHelperThread();
    }
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

  bool mustWait = task->options.useOffThreadParseGlobal &&
                  OffThreadParsingMustWaitForGC(cx->runtime());
  bool result;
  if (mustWait) {
    result = HelperThreadState().parseWaitingOnGC(lock).append(std::move(task));
  } else {
    result =
        HelperThreadState().submitTask(cx->runtime(), std::move(task), lock);
  }

  if (!result) {
    ReportOutOfMemory(cx);
  }
  return result;
}

bool GlobalHelperThreadState::submitTask(
    JSRuntime* rt, UniquePtr<ParseTask> task,
    const AutoLockHelperThreadState& locked) {
  if (!parseWorklist(locked).append(std::move(task))) {
    return false;
  }

  parseWorklist(locked).back()->activate(rt);

  dispatch(locked);
  return true;
}

static JS::OffThreadToken* StartOffThreadParseTask(
    JSContext* cx, UniquePtr<ParseTask> task,
    const ReadOnlyCompileOptions& options) {
  // Suppress GC so that calls below do not trigger a new incremental GC
  // which could require barriers on the atoms zone.
  gc::AutoSuppressGC nogc(cx);
  gc::AutoSuppressNurseryCellAlloc noNurseryAlloc(cx);
  AutoSuppressAllocationMetadataBuilder suppressMetadata(cx);

  JSObject* global = nullptr;
  if (options.useOffThreadParseGlobal) {
    global = CreateGlobalForOffThreadParse(cx, nogc);
    if (!global) {
      return nullptr;
    }
  }

  // Mark the global's zone as created for a helper thread. This prevents it
  // from being collected until clearUsedByHelperThread() is called after
  // parsing is complete. If this function exits due to error this state is
  // cleared automatically.
  AutoSetCreatedForHelperThread createdForHelper(global);

  if (!task->init(cx, options, global)) {
    return nullptr;
  }

  JS::OffThreadToken* token = task.get();
  if (!QueueOffThreadParseTask(cx, std::move(task))) {
    return nullptr;
  }

  createdForHelper.forget();

  // Return an opaque pointer to caller so that it may query/cancel the task
  // before the callback is fired.
  return token;
}

template <typename Unit>
static JS::OffThreadToken* StartOffThreadParseScriptInternal(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<Unit>& srcBuf, JS::OffThreadCompileCallback callback,
    void* callbackData) {
  auto task = cx->make_unique<ScriptParseTask<Unit>>(cx, srcBuf, callback,
                                                     callbackData);
  if (!task) {
    return nullptr;
  }

  return StartOffThreadParseTask(cx, std::move(task), options);
}

JS::OffThreadToken* js::StartOffThreadParseScript(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, JS::OffThreadCompileCallback callback,
    void* callbackData) {
  return StartOffThreadParseScriptInternal(cx, options, srcBuf, callback,
                                           callbackData);
}

JS::OffThreadToken* js::StartOffThreadParseScript(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<Utf8Unit>& srcBuf, JS::OffThreadCompileCallback callback,
    void* callbackData) {
  return StartOffThreadParseScriptInternal(cx, options, srcBuf, callback,
                                           callbackData);
}

template <typename Unit>
static JS::OffThreadToken* StartOffThreadParseModuleInternal(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<Unit>& srcBuf, JS::OffThreadCompileCallback callback,
    void* callbackData) {
  auto task = cx->make_unique<ModuleParseTask<Unit>>(cx, srcBuf, callback,
                                                     callbackData);
  if (!task) {
    return nullptr;
  }

  return StartOffThreadParseTask(cx, std::move(task), options);
}

JS::OffThreadToken* js::StartOffThreadParseModule(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<char16_t>& srcBuf, JS::OffThreadCompileCallback callback,
    void* callbackData) {
  return StartOffThreadParseModuleInternal(cx, options, srcBuf, callback,
                                           callbackData);
}

JS::OffThreadToken* js::StartOffThreadParseModule(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::SourceText<Utf8Unit>& srcBuf, JS::OffThreadCompileCallback callback,
    void* callbackData) {
  return StartOffThreadParseModuleInternal(cx, options, srcBuf, callback,
                                           callbackData);
}

JS::OffThreadToken* js::StartOffThreadDecodeScript(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    const JS::TranscodeRange& range, JS::OffThreadCompileCallback callback,
    void* callbackData) {
  // XDR data must be Stencil format, or a parse-global must be available.
  MOZ_RELEASE_ASSERT(options.useStencilXDR || options.useOffThreadParseGlobal);

  auto task =
      cx->make_unique<ScriptDecodeTask>(cx, range, callback, callbackData);
  if (!task) {
    return nullptr;
  }

  return StartOffThreadParseTask(cx, std::move(task), options);
}

JS::OffThreadToken* js::StartOffThreadDecodeMultiScripts(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    JS::TranscodeSources& sources, JS::OffThreadCompileCallback callback,
    void* callbackData) {
  auto task = cx->make_unique<MultiScriptsDecodeTask>(cx, sources, callback,
                                                      callbackData);
  if (!task) {
    return nullptr;
  }

  // NOTE: All uses of DecodeMulti are currently generated by non-incremental
  //       XDR and therefore do not support the stencil format. As a result,
  //       they must continue to use the off-thread-parse-global in order to
  //       decode.
  CompileOptions optionsCopy(cx, options);
  optionsCopy.useStencilXDR = false;
  optionsCopy.useOffThreadParseGlobal = true;

  return StartOffThreadParseTask(cx, std::move(task), optionsCopy);
}

void js::EnqueuePendingParseTasksAfterGC(JSRuntime* rt) {
  MOZ_ASSERT(!OffThreadParsingMustWaitForGC(rt));

  AutoLockHelperThreadState lock;

  GlobalHelperThreadState::ParseTaskVector& waiting =
      HelperThreadState().parseWaitingOnGC(lock);
  for (size_t i = 0; i < waiting.length(); i++) {
    if (!waiting[i]->runtimeMatches(rt)) {
      continue;
    }

    {
      AutoEnterOOMUnsafeRegion oomUnsafe;
      if (!HelperThreadState().submitTask(rt, std::move(waiting[i]), lock)) {
        oomUnsafe.crash("EnqueuePendingParseTasksAfterGC");
      }
    }
    HelperThreadState().remove(waiting, &i);
  }
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

  return ensureThreadCount(threadCount);
}

bool GlobalHelperThreadState::ensureThreadCount(size_t count) {
  if (!ensureContextList(count)) {
    return false;
  }

  AutoLockHelperThreadState lock;

  if (threads(lock).length() >= count) {
    return true;
  }

  if (!threads(lock).reserve(count)) {
    return false;
  }

  if (!helperTasks_.reserve(count)) {
    return false;
  }

  for (size_t& i : runningTaskCount) {
    i = 0;
  }

  // Update threadCount on exit so this stays consistent with how many threads
  // there are.
  auto updateThreadCount =
      mozilla::MakeScopeExit([&] { threadCount = threads(lock).length(); });

  while (threads(lock).length() < count) {
    auto thread = js::MakeUnique<HelperThread>();
    if (!thread || !thread->init()) {
      return false;
    }

    threads(lock).infallibleEmplaceBack(std::move(thread));
  }

  return true;
}

GlobalHelperThreadState::GlobalHelperThreadState()
    : cpuCount(0),
      threadCount(0),
      totalCountRunningTasks(0),
      registerThread(nullptr),
      unregisterThread(nullptr),
      wasmTier2GeneratorsFinished_(0) {
  cpuCount = ClampDefaultCPUCount(GetCPUCount());
  threadCount = ThreadCountForCPUCount(cpuCount);
  gcParallelThreadCount = threadCount;

  MOZ_ASSERT(cpuCount > 0, "GetCPUCount() seems broken");
}

void GlobalHelperThreadState::finish() {
  finishThreads();

  // Make sure there are no Ion free tasks left. We check this here because,
  // unlike the other tasks, we don't explicitly block on this when
  // destroying a runtime.
  AutoLockHelperThreadState lock;
  auto& freeList = ionFreeList(lock);
  while (!freeList.empty()) {
    UniquePtr<jit::IonFreeTask> task = std::move(freeList.back());
    freeList.popBack();
    jit::FreeIonCompileTask(task->compileTask());
  }
  destroyHelperContexts(lock);
}

void GlobalHelperThreadState::finishThreads() {
  HelperThreadVector oldThreads;

  {
    AutoLockHelperThreadState lock;

    if (threads(lock).empty()) {
      return;
    }

    MOZ_ASSERT(CanUseExtraThreads());

    waitForAllThreadsLocked(lock);

    for (auto& thread : threads(lock)) {
      thread->setTerminate(lock);
    }

    notifyAll(GlobalHelperThreadState::PRODUCER, lock);

    std::swap(threads_, oldThreads);
  }

  for (auto& thread : oldThreads) {
    thread->join();
  }
}

bool GlobalHelperThreadState::ensureContextList(size_t count) {
  AutoLockHelperThreadState lock;

  if (helperContexts_.length() >= count) {
    return true;
  }

  while (helperContexts_.length() < count) {
    auto cx = js::MakeUnique<JSContext>(nullptr, JS::ContextOptions());
    if (!cx || !cx->init(ContextKind::HelperThread) ||
        !helperContexts_.append(cx.release())) {
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
    js_delete(helperContexts_.popCopy());
  }
}

#ifdef DEBUG
void GlobalHelperThreadState::assertIsLockedByCurrentThread() const {
  gHelperThreadLock.assertOwnedByCurrentThread();
}
#endif  // DEBUG

void GlobalHelperThreadState::dispatch(
    const AutoLockHelperThreadState& locked) {
  notifyOne(PRODUCER, locked);
}

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
    const AutoLockHelperThreadState& lock) {
  return !helperTasks(lock).empty();
}

void js::WaitForAllHelperThreads() { HelperThreadState().waitForAllThreads(); }

void js::WaitForAllHelperThreads(AutoLockHelperThreadState& lock) {
  HelperThreadState().waitForAllThreadsLocked(lock);
}

void GlobalHelperThreadState::waitForAllThreads() {
  AutoLockHelperThreadState lock;
  waitForAllThreadsLocked(lock);
}

void GlobalHelperThreadState::waitForAllThreadsLocked(
    AutoLockHelperThreadState& lock) {
  CancelOffThreadWasmTier2GeneratorLocked(lock);

  while (hasActiveThreads(lock) || hasQueuedTasks(lock)) {
    wait(lock, CONSUMER);
  }

  MOZ_ASSERT(!hasActiveThreads(lock));
  MOZ_ASSERT(!hasQueuedTasks(lock));
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

bool GlobalHelperThreadState::checkTaskThreadLimit(
    ThreadType threadType, size_t maxThreads, bool isMaster,
    const AutoLockHelperThreadState& lock) const {
  MOZ_ASSERT(maxThreads > 0);

  if (!isMaster && maxThreads >= threadCount) {
    return true;
  }

  size_t count = runningTaskCount[threadType];
  if (count >= maxThreads) {
    return false;
  }

  MOZ_ASSERT(threadCount >= totalCountRunningTasks);
  size_t idle = threadCount - totalCountRunningTasks;

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
#ifdef DEBUG
  assertIsLockedByCurrentThread();
#endif

  mozilla::MallocSizeOf mallocSizeOf = stats->mallocSizeOf_;
  JS::HelperThreadStats& htStats = stats->helperThread;

  htStats.stateData += mallocSizeOf(this);

  htStats.stateData += threads(lock).sizeOfExcludingThis(mallocSizeOf);

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
      gcParallelWorklist_.sizeOfExcludingThis(mallocSizeOf) +
      helperContexts_.sizeOfExcludingThis(mallocSizeOf) +
      helperTasks_.sizeOfExcludingThis(mallocSizeOf);

  // Report ParseTasks on wait lists
  for (const auto& task : parseWorklist_) {
    htStats.parseTask += task->sizeOfIncludingThis(mallocSizeOf);
  }
  for (auto task : parseFinishedList_) {
    htStats.parseTask += task->sizeOfIncludingThis(mallocSizeOf);
  }
  for (const auto& task : parseWaitingOnGC_) {
    htStats.parseTask += task->sizeOfIncludingThis(mallocSizeOf);
  }

  // Report IonCompileTasks on wait lists
  for (auto task : ionWorklist_) {
    htStats.ionCompileTask += task->sizeOfExcludingThis(mallocSizeOf);
  }
  for (auto task : ionFinishedList_) {
    htStats.ionCompileTask += task->sizeOfExcludingThis(mallocSizeOf);
  }
  for (const auto& task : ionFreeList_) {
    htStats.ionCompileTask +=
        task->compileTask()->sizeOfExcludingThis(mallocSizeOf);
  }

  // Report wasm::CompileTasks on wait lists
  for (auto task : wasmWorklist_tier1_) {
    htStats.wasmCompile += task->sizeOfExcludingThis(mallocSizeOf);
  }
  for (auto task : wasmWorklist_tier2_) {
    htStats.wasmCompile += task->sizeOfExcludingThis(mallocSizeOf);
  }

  {
    // Report memory used by the JSContexts.
    // We're holding the helper state lock, and the JSContext memory reporter
    // won't do anything more substantial than traversing data structures and
    // getting their size, so disable ProtectedData checks.
    AutoNoteSingleThreadedRegion anstr;
    for (auto* cx : helperContexts_) {
      htStats.contexts += cx->sizeOfIncludingThis(mallocSizeOf);
    }
  }

  // Report number of helper threads.
  MOZ_ASSERT(htStats.idleThreadCount == 0);
  MOZ_ASSERT(threadCount >= totalCountRunningTasks);
  htStats.activeThreadCount = totalCountRunningTasks;
  htStats.idleThreadCount = threadCount - totalCountRunningTasks;
}

size_t GlobalHelperThreadState::maxIonCompilationThreads() const {
  if (IsHelperThreadSimulatingOOM(js::THREAD_TYPE_ION)) {
    return 1;
  }
  return threadCount;
}

size_t GlobalHelperThreadState::maxWasmCompilationThreads() const {
  if (IsHelperThreadSimulatingOOM(js::THREAD_TYPE_WASM_COMPILE_TIER1) ||
      IsHelperThreadSimulatingOOM(js::THREAD_TYPE_WASM_COMPILE_TIER2)) {
    return 1;
  }
  return cpuCount;
}

size_t GlobalHelperThreadState::maxWasmTier2GeneratorThreads() const {
  return MaxTier2GeneratorTasks;
}

size_t GlobalHelperThreadState::maxPromiseHelperThreads() const {
  if (IsHelperThreadSimulatingOOM(js::THREAD_TYPE_WASM_COMPILE_TIER1) ||
      IsHelperThreadSimulatingOOM(js::THREAD_TYPE_WASM_COMPILE_TIER2)) {
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

size_t GlobalHelperThreadState::maxGCParallelThreads(
    const AutoLockHelperThreadState& lock) const {
  if (IsHelperThreadSimulatingOOM(js::THREAD_TYPE_GCPARALLEL)) {
    return 1;
  }
  return gcParallelThreadCount;
}

HelperThreadTask* GlobalHelperThreadState::maybeGetWasmTier1CompileTask(
    const AutoLockHelperThreadState& lock) {
  return maybeGetWasmCompile(lock, wasm::CompileMode::Tier1);
}

HelperThreadTask* GlobalHelperThreadState::maybeGetWasmTier2CompileTask(
    const AutoLockHelperThreadState& lock) {
  return maybeGetWasmCompile(lock, wasm::CompileMode::Tier2);
}

HelperThreadTask* GlobalHelperThreadState::maybeGetWasmCompile(
    const AutoLockHelperThreadState& lock, wasm::CompileMode mode) {
  if (wasmWorklist(lock, mode).empty()) {
    return nullptr;
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
  ThreadType threadType;
  if (mode == wasm::CompileMode::Tier2) {
    if (tier2oversubscribed) {
      threads = maxWasmCompilationThreads();
    } else {
      threads = physCoresAvailable;
    }
    threadType = THREAD_TYPE_WASM_COMPILE_TIER2;
  } else {
    if (tier2oversubscribed) {
      threads = 0;
    } else {
      threads = maxWasmCompilationThreads();
    }
    threadType = THREAD_TYPE_WASM_COMPILE_TIER1;
  }

  if (!threads || !checkTaskThreadLimit(threadType, threads, lock)) {
    return nullptr;
  }

  return wasmWorklist(lock, mode).popCopyFront();
}

HelperThreadTask* GlobalHelperThreadState::maybeGetWasmTier2GeneratorTask(
    const AutoLockHelperThreadState& lock) {
  if (wasmTier2GeneratorWorklist(lock).empty() ||
      !checkTaskThreadLimit(THREAD_TYPE_WASM_GENERATOR_TIER2,
                            maxWasmTier2GeneratorThreads(),
                            /*isMaster=*/true, lock)) {
    return nullptr;
  }

  return wasmTier2GeneratorWorklist(lock).popCopy();
}

HelperThreadTask* GlobalHelperThreadState::maybeGetPromiseHelperTask(
    const AutoLockHelperThreadState& lock) {
  // PromiseHelperTasks can be wasm compilation tasks that in turn block on
  // wasm compilation so set isMaster = true.
  if (promiseHelperTasks(lock).empty() ||
      !checkTaskThreadLimit(THREAD_TYPE_PROMISE_TASK, maxPromiseHelperThreads(),
                            /*isMaster=*/true, lock)) {
    return nullptr;
  }

  return promiseHelperTasks(lock).popCopy();
}

static bool IonCompileTaskHasHigherPriority(jit::IonCompileTask* first,
                                            jit::IonCompileTask* second) {
  // Return true if priority(first) > priority(second).
  //
  // This method can return whatever it wants, though it really ought to be a
  // total order. The ordering is allowed to race (change on the fly), however.

  // A higher warm-up counter indicates a higher priority.
  jit::JitScript* firstJitScript = first->script()->jitScript();
  jit::JitScript* secondJitScript = second->script()->jitScript();
  return firstJitScript->warmUpCount() / first->script()->length() >
         secondJitScript->warmUpCount() / second->script()->length();
}

HelperThreadTask* GlobalHelperThreadState::maybeGetIonCompileTask(
    const AutoLockHelperThreadState& lock) {
  if (ionWorklist(lock).empty() ||
      !checkTaskThreadLimit(THREAD_TYPE_ION, maxIonCompilationThreads(),
                            lock)) {
    return nullptr;
  }

  return highestPriorityPendingIonCompile(lock);
}

HelperThreadTask* GlobalHelperThreadState::maybeGetIonFreeTask(
    const AutoLockHelperThreadState& lock) {
  if (ionFreeList(lock).empty()) {
    return nullptr;
  }

  UniquePtr<jit::IonFreeTask> task = std::move(ionFreeList(lock).back());
  ionFreeList(lock).popBack();
  return task.release();
}

jit::IonCompileTask* GlobalHelperThreadState::highestPriorityPendingIonCompile(
    const AutoLockHelperThreadState& lock) {
  auto& worklist = ionWorklist(lock);
  MOZ_ASSERT(!worklist.empty());

  // Get the highest priority IonCompileTask which has not started compilation
  // yet.
  size_t index = 0;
  for (size_t i = 1; i < worklist.length(); i++) {
    if (IonCompileTaskHasHigherPriority(worklist[i], worklist[index])) {
      index = i;
    }
  }

  jit::IonCompileTask* task = worklist[index];
  worklist.erase(&worklist[index]);
  return task;
}

HelperThreadTask* GlobalHelperThreadState::maybeGetParseTask(
    const AutoLockHelperThreadState& lock) {
  // Parse tasks that end up compiling asm.js in turn may use Wasm compilation
  // threads to generate machine code.  We have no way (at present) to know
  // ahead of time whether a parse task is going to parse asm.js content or
  // not, so we just assume that all parse tasks are master tasks.
  if (parseWorklist(lock).empty() ||
      !checkTaskThreadLimit(THREAD_TYPE_PARSE, maxParseThreads(),
                            /*isMaster=*/true, lock)) {
    return nullptr;
  }

  auto& worklist = parseWorklist(lock);
  UniquePtr<ParseTask> task = std::move(worklist.back());
  worklist.popBack();
  return task.release();
}

HelperThreadTask* GlobalHelperThreadState::maybeGetCompressionTask(
    const AutoLockHelperThreadState& lock) {
  if (compressionWorklist(lock).empty() ||
      !checkTaskThreadLimit(THREAD_TYPE_COMPRESS, maxCompressionThreads(),
                            lock)) {
    return nullptr;
  }

  auto& worklist = compressionWorklist(lock);
  UniquePtr<SourceCompressionTask> task = std::move(worklist.back());
  worklist.popBack();
  return task.release();
}

void GlobalHelperThreadState::startHandlingCompressionTasks(
    ScheduleCompressionTask schedule, JSRuntime* maybeRuntime,
    const AutoLockHelperThreadState& lock) {
  MOZ_ASSERT((schedule == ScheduleCompressionTask::GC) ==
             (maybeRuntime != nullptr));

  auto& pending = compressionPendingList(lock);

  for (size_t i = 0; i < pending.length(); i++) {
    UniquePtr<SourceCompressionTask>& task = pending[i];
    if (schedule == ScheduleCompressionTask::API ||
        (task->runtimeMatches(maybeRuntime) && task->shouldStart())) {
      // OOMing during appending results in the task not being scheduled
      // and deleted.
      Unused << submitTask(std::move(task), lock);
      remove(pending, &i);
    }
  }
}

bool GlobalHelperThreadState::submitTask(
    UniquePtr<SourceCompressionTask> task,
    const AutoLockHelperThreadState& locked) {
  if (!compressionWorklist(locked).append(std::move(task))) {
    return false;
  }

  dispatch(locked);
  return true;
}

bool GlobalHelperThreadState::submitTask(
    GCParallelTask* task, const AutoLockHelperThreadState& locked) {
  gcParallelWorklist(locked).insertBack(task);
  dispatch(locked);
  return true;
}

HelperThreadTask* GlobalHelperThreadState::maybeGetGCParallelTask(
    const AutoLockHelperThreadState& lock) {
  if (gcParallelWorklist(lock).isEmpty() ||
      !checkTaskThreadLimit(THREAD_TYPE_GCPARALLEL, maxGCParallelThreads(lock),
                            lock)) {
    return nullptr;
  }

  return gcParallelWorklist(lock).popFirst();
}

static void LeaveParseTaskZone(JSRuntime* rt, ParseTask* task) {
  // Mark the zone as no longer in use by a helper thread, and available
  // to be collected by the GC.
  if (task->parseGlobal) {
    rt->clearUsedByHelperThread(task->parseGlobal->zoneFromAnyThread());
  }
  rt->decParseTaskRef();
}

ParseTask* GlobalHelperThreadState::removeFinishedParseTask(
    JSContext* cx, ParseTaskKind kind, JS::OffThreadToken* token) {
  // The token is really a ParseTask* which should be in the finished list.
  auto task = static_cast<ParseTask*>(token);

  // The token was passed in from the browser. Check that the pointer is likely
  // a valid parse task of the expected kind.
  MOZ_RELEASE_ASSERT(task->runtime == cx->runtime());
  MOZ_RELEASE_ASSERT(task->kind == kind);

  // Remove the task from the finished list.
  AutoLockHelperThreadState lock;
  MOZ_ASSERT(parseFinishedList(lock).contains(task));
  task->remove();
  return task;
}

UniquePtr<ParseTask> GlobalHelperThreadState::finishParseTaskCommon(
    JSContext* cx, ParseTaskKind kind, JS::OffThreadToken* token) {
  MOZ_ASSERT(!cx->isHelperThreadContext());
  MOZ_ASSERT(cx->realm());

  Rooted<UniquePtr<ParseTask>> parseTask(
      cx, removeFinishedParseTask(cx, kind, token));

  if (parseTask->options.useOffThreadParseGlobal) {
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

    if (kind == ParseTaskKind::Module) {
      if (parseTask->scripts.length() > 0) {
        MOZ_ASSERT(parseTask->scripts[0]->isModule());
        parseTask->scripts[0]->module()->fixEnvironmentsAfterRealmMerge();
      }
    }

    // Finish initializing ScriptSourceObject now that we are back on
    // main-thread and in the correct realm.
    for (auto& sourceObject : parseTask->sourceObjects) {
      RootedScriptSourceObject sso(cx, sourceObject);

      if (!ScriptSourceObject::initFromOptions(cx, sso, parseTask->options)) {
        return nullptr;
      }

      if (!sso->source()->tryCompressOffThread(cx)) {
        return nullptr;
      }
    }
  } else {
    // GC things should be allocated in finishSingleParseTask, after
    // calling finishParseTaskCommon.
    MOZ_ASSERT(parseTask->scripts.length() == 0);
    MOZ_ASSERT(parseTask->sourceObjects.length() == 0);
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

  if (parseTask->options.useOffThreadParseGlobal) {
    if (coverage::IsLCovEnabled()) {
      if (!generateLCovSources(cx, parseTask.get().get())) {
        return nullptr;
      }
    }
  }

  return std::move(parseTask.get());
}

// Generate initial LCovSources for generated inner functions.
bool GlobalHelperThreadState::generateLCovSources(JSContext* cx,
                                                  ParseTask* parseTask) {
  Rooted<GCVector<JSScript*>> workList(cx, GCVector<JSScript*>(cx));

  if (!workList.appendAll(parseTask->scripts)) {
    return false;
  }

  RootedScript elem(cx);
  while (!workList.empty()) {
    elem = workList.popCopy();

    // Initialize LCov data for the script.
    if (!coverage::InitScriptCoverage(cx, elem)) {
      return false;
    }

    // Add inner-function scripts to the work-list.
    for (JS::GCCellPtr gcThing : elem->gcthings()) {
      if (!gcThing.is<JSObject>()) {
        continue;
      }
      JSObject* obj = &gcThing.as<JSObject>();

      if (!obj->is<JSFunction>()) {
        continue;
      }
      JSFunction* fun = &obj->as<JSFunction>();

      // Ignore asm.js functions
      if (!fun->isInterpreted()) {
        continue;
      }

      MOZ_ASSERT(fun->hasBytecode(),
                 "No lazy scripts exist when collecting coverage");
      if (!workList.append(fun->nonLazyScript())) {
        return false;
      }
    }
  }

  return true;
}

JSScript* GlobalHelperThreadState::finishSingleParseTask(
    JSContext* cx, ParseTaskKind kind, JS::OffThreadToken* token,
    StartEncoding startEncoding /* = StartEncoding::No */) {
  Rooted<UniquePtr<ParseTask>> parseTask(
      cx, finishParseTaskCommon(cx, kind, token));
  if (!parseTask) {
    return nullptr;
  }

  JS::RootedScript script(cx);

  // Finish main-thread initialization of scripts.
  if (parseTask->options.useOffThreadParseGlobal) {
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

    if (kind == ParseTaskKind::Module) {
      // See: InstantiateTopLevel in frontend/Stencil.cpp.
      MOZ_ASSERT(script->isModule());
      RootedModuleObject module(cx, script->module());
      if (!ModuleObject::Freeze(cx, module)) {
        return nullptr;
      }
    }

    // The Debugger only needs to be told about the topmost script that was
    // compiled.
    if (!parseTask->options.hideScriptFromDebugger) {
      DebugAPI::onNewScript(cx, script);
    }
  } else {
    MOZ_ASSERT(parseTask->stencil_.get() ||
               parseTask->extensibleStencil_.get());

    if (!parseTask->instantiateStencils(cx)) {
      return nullptr;
    }

    MOZ_RELEASE_ASSERT(parseTask->scripts.length() == 1);
    script = parseTask->scripts[0];
  }

  // Start the incremental-XDR encoder.
  if (startEncoding == StartEncoding::Yes) {
    MOZ_DIAGNOSTIC_ASSERT(parseTask->options.useStencilXDR);

    if (parseTask->stencil_) {
      auto initial = js::MakeUnique<frontend::ExtensibleCompilationStencil>(
          cx, *parseTask->stencilInput_);
      if (!initial) {
        ReportOutOfMemory(cx);
        return nullptr;
      }
      if (!initial->steal(cx, std::move(*parseTask->stencil_))) {
        return nullptr;
      }

      if (!script->scriptSource()->startIncrementalEncoding(
              cx, parseTask->options, std::move(initial))) {
        return nullptr;
      }
    } else if (parseTask->extensibleStencil_) {
      if (!script->scriptSource()->startIncrementalEncoding(
              cx, parseTask->options,
              std::move(parseTask->extensibleStencil_))) {
        return nullptr;
      }
    }
  }

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

  // The Debugger only needs to be told about the topmost scripts that were
  // compiled.
  if (!parseTask->options.hideScriptFromDebugger) {
    JS::RootedScript rooted(cx);
    for (auto& script : scripts) {
      MOZ_ASSERT(script->isGlobalCode());

      rooted = script;
      DebugAPI::onNewScript(cx, rooted);
    }
  }

  return true;
}

JSScript* GlobalHelperThreadState::finishScriptParseTask(
    JSContext* cx, JS::OffThreadToken* token,
    StartEncoding startEncoding /* = StartEncoding::No */) {
  JSScript* script =
      finishSingleParseTask(cx, ParseTaskKind::Script, token, startEncoding);
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

  return script->module();
}

frontend::CompilationStencil* GlobalHelperThreadState::finishStencilParseTask(
    JSContext* cx, JS::OffThreadToken* token) {
  // TODO: The Script and Module task kinds should be combined in future since
  //       they both generate the same Stencil type.
  auto task = static_cast<ParseTask*>(token);
  MOZ_RELEASE_ASSERT(task->kind == ParseTaskKind::Script ||
                     task->kind == ParseTaskKind::Module);

  Rooted<UniquePtr<ParseTask>> parseTask(
      cx, finishParseTaskCommon(cx, task->kind, token));
  if (!parseTask) {
    return nullptr;
  }

  MOZ_ASSERT(!parseTask->options.useOffThreadParseGlobal);
  MOZ_ASSERT(parseTask->extensibleStencil_);

  UniquePtr<frontend::CompilationStencil> stencil =
      cx->make_unique<frontend::CompilationStencil>(
          parseTask->stencilInput_->source);
  if (!stencil) {
    return nullptr;
  }

  if (!stencil->steal(cx, std::move(*parseTask->extensibleStencil_))) {
    return nullptr;
  }

  return stencil.release();
}

void GlobalHelperThreadState::cancelParseTask(JSRuntime* rt, ParseTaskKind kind,
                                              JS::OffThreadToken* token) {
  AutoLockHelperThreadState lock;
  MOZ_ASSERT(token);

  ParseTask* task = static_cast<ParseTask*>(token);

  // Check pending queues to see if we can simply remove the task.
  GlobalHelperThreadState::ParseTaskVector& waitingOnGC =
      HelperThreadState().parseWaitingOnGC(lock);
  for (size_t i = 0; i < waitingOnGC.length(); i++) {
    if (task == waitingOnGC[i]) {
      MOZ_ASSERT(task->kind == kind);
      MOZ_ASSERT(task->runtimeMatches(rt));
      task->parseGlobal->zoneFromAnyThread()->clearUsedByHelperThread();
      HelperThreadState().remove(waitingOnGC, &i);
      return;
    }
  }

  GlobalHelperThreadState::ParseTaskVector& worklist =
      HelperThreadState().parseWorklist(lock);
  for (size_t i = 0; i < worklist.length(); i++) {
    if (task == worklist[i]) {
      MOZ_ASSERT(task->kind == kind);
      MOZ_ASSERT(task->runtimeMatches(rt));
      LeaveParseTaskZone(rt, task);
      HelperThreadState().remove(worklist, &i);
      return;
    }
  }

  // If task is currently running, wait for it to complete.
  while (true) {
    bool foundTask = false;
    for (auto* helper : HelperThreadState().helperTasks(lock)) {
      if (helper->is<ParseTask>() && helper->as<ParseTask>() == task) {
        MOZ_ASSERT(helper->as<ParseTask>()->kind == kind);
        MOZ_ASSERT(helper->as<ParseTask>()->runtimeMatches(rt));
        foundTask = true;
        break;
      }
    }

    if (!foundTask) {
      break;
    }

    HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
  }

  auto& finished = HelperThreadState().parseFinishedList(lock);
  for (auto* t : finished) {
    if (task == t) {
      MOZ_ASSERT(task->kind == kind);
      MOZ_ASSERT(task->runtimeMatches(rt));
      task->remove();
      HelperThreadState().destroyParseTask(rt, task);
      return;
    }
  }
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
  MOZ_ASSERT(parseTask->parseGlobal);

  // After we call LeaveParseTaskZone() it's not safe to GC until we have
  // finished merging the contents of the parse task's realm into the
  // destination realm.
  JS::AutoAssertNoGC nogc(cx);

  LeaveParseTaskZone(cx->runtime(), parseTask);

  // Move the parsed script and all its contents into the desired realm.
  gc::MergeRealms(parseTask->parseGlobal->as<GlobalObject>().realm(), dest);
}

HelperThread::HelperThread()
    : thread(Thread::Options().setStackSize(HELPER_STACK_SIZE)) {}

bool HelperThread::init() {
  return thread.init(HelperThread::ThreadMain, this);
}

void HelperThread::setTerminate(const AutoLockHelperThreadState& lock) {
  terminate = true;
}

void HelperThread::join() { thread.join(); }

void HelperThread::ensureRegisteredWithProfiler() {
  if (profilingStack) {
    return;
  }

  // Note: To avoid dead locks, we should not hold on the helper thread lock
  // while calling this function. This is safe because the registerThread field
  // is a WriteOnceData<> type stored on the global helper tread state.
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

  // Note: To avoid dead locks, we should not hold on the helper thread lock
  // while calling this function. This is safe because the unregisterThread
  // field is a WriteOnceData<> type stored on the global helper tread state.
  JS::UnregisterThreadCallback callback = HelperThreadState().unregisterThread;
  if (callback) {
    callback();
    profilingStack = nullptr;
  }
}

/* static */
void HelperThread::ThreadMain(void* arg) {
  ThisThread::SetName("JS Helper");

  auto helper = static_cast<HelperThread*>(arg);

  helper->ensureRegisteredWithProfiler();
  helper->threadLoop();
  helper->unregisterWithProfilerIfNeeded();
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

void js::StartHandlingCompressionsOnGC(JSRuntime* runtime) {
  AutoLockHelperThreadState lock;
  HelperThreadState().startHandlingCompressionTasks(
      GlobalHelperThreadState::ScheduleCompressionTask::GC, runtime, lock);
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

  if (HelperThreadState().threads(lock).empty()) {
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
    for (auto* helper : HelperThreadState().helperTasks(lock)) {
      if (!helper->is<SourceCompressionTask>()) {
        continue;
      }

      if (helper->as<SourceCompressionTask>()->runtimeMatches(runtime)) {
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

void js::SweepPendingCompressions(AutoLockHelperThreadState& lock) {
  auto& pending = HelperThreadState().compressionPendingList(lock);
  for (size_t i = 0; i < pending.length(); i++) {
    if (pending[i]->shouldCancel()) {
      HelperThreadState().remove(pending, &i);
    }
  }
}

void js::RunPendingSourceCompressions(JSRuntime* runtime) {
  AutoLockHelperThreadState lock;

  if (HelperThreadState().threads(lock).empty()) {
    return;
  }

  HelperThreadState().startHandlingCompressionTasks(
      GlobalHelperThreadState::ScheduleCompressionTask::API, nullptr, lock);

  // Wait until all tasks have started compression.
  while (!HelperThreadState().compressionWorklist(lock).empty()) {
    HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
  }

  // Wait for all in-process compression tasks to complete.
  HelperThreadState().waitForAllThreadsLocked(lock);

  AttachFinishedCompressions(runtime, lock);
}

void PromiseHelperTask::executeAndResolveAndDestroy(JSContext* cx) {
  execute();
  run(cx, JS::Dispatchable::NotShuttingDown);
}

void PromiseHelperTask::runHelperThreadTask(AutoLockHelperThreadState& lock) {
  {
    AutoUnlockHelperThreadState unlock(lock);
    execute();
  }

  // Don't release the lock between dispatching the resolve and destroy
  // operation (which may start immediately on another thread) and returning
  // from this method.

  dispatchResolveAndDestroy(lock);
}

bool js::StartOffThreadPromiseHelperTask(JSContext* cx,
                                         UniquePtr<PromiseHelperTask> task) {
  // Execute synchronously if there are no helper threads.
  if (!CanUseExtraThreads()) {
    task.release()->executeAndResolveAndDestroy(cx);
    return true;
  }

  if (!HelperThreadState().submitTask(task.get())) {
    ReportOutOfMemory(cx);
    return false;
  }

  Unused << task.release();
  return true;
}

bool js::StartOffThreadPromiseHelperTask(PromiseHelperTask* task) {
  MOZ_ASSERT(CanUseExtraThreads());

  return HelperThreadState().submitTask(task);
}

bool GlobalHelperThreadState::submitTask(PromiseHelperTask* task) {
  AutoLockHelperThreadState lock;

  if (!promiseHelperTasks(lock).append(task)) {
    return false;
  }

  dispatch(lock);
  return true;
}

void GlobalHelperThreadState::trace(JSTracer* trc) {
  AutoLockHelperThreadState lock;

#ifdef DEBUG
  // Since we hold the helper thread lock here we must disable GCMarker's
  // checking of the atom marking bitmap since that also relies on taking the
  // lock.
  GCMarker* marker = nullptr;
  if (trc->isMarkingTracer()) {
    marker = GCMarker::fromTracer(trc);
    marker->setCheckAtomMarking(false);
  }
  auto reenableAtomMarkingCheck = mozilla::MakeScopeExit([marker] {
    if (marker) {
      marker->setCheckAtomMarking(true);
    }
  });
#endif

  for (auto task : ionWorklist(lock)) {
    task->alloc().lifoAlloc()->setReadWrite();
    task->trace(trc);
    task->alloc().lifoAlloc()->setReadOnly();
  }
  for (auto task : ionFinishedList(lock)) {
    task->trace(trc);
  }

  for (auto* helper : HelperThreadState().helperTasks(lock)) {
    if (helper->is<jit::IonCompileTask>()) {
      helper->as<jit::IonCompileTask>()->trace(trc);
    }
  }

  JSRuntime* rt = trc->runtime();
  if (auto* jitRuntime = rt->jitRuntime()) {
    jit::IonCompileTask* task = jitRuntime->ionLazyLinkList(rt).getFirst();
    while (task) {
      task->trace(trc);
      task = task->getNext();
    }
  }

  for (auto& parseTask : parseWorklist_) {
    parseTask->trace(trc);
  }
  for (auto parseTask : parseFinishedList_) {
    parseTask->trace(trc);
  }
  for (auto& parseTask : parseWaitingOnGC_) {
    parseTask->trace(trc);
  }
}

// Definition of helper thread tasks.
//
// Priority is determined by the order they're listed here.
const HelperThread::Selector HelperThread::selectors[] = {
    &GlobalHelperThreadState::maybeGetGCParallelTask,
    &GlobalHelperThreadState::maybeGetIonCompileTask,
    &GlobalHelperThreadState::maybeGetWasmTier1CompileTask,
    &GlobalHelperThreadState::maybeGetPromiseHelperTask,
    &GlobalHelperThreadState::maybeGetParseTask,
    &GlobalHelperThreadState::maybeGetCompressionTask,
    &GlobalHelperThreadState::maybeGetIonFreeTask,
    &GlobalHelperThreadState::maybeGetWasmTier2CompileTask,
    &GlobalHelperThreadState::maybeGetWasmTier2GeneratorTask};

bool GlobalHelperThreadState::hasQueuedTasks(
    const AutoLockHelperThreadState& lock) {
  return !gcParallelWorklist(lock).isEmpty() || !ionWorklist(lock).empty() ||
         !wasmWorklist(lock, wasm::CompileMode::Tier1).empty() ||
         !promiseHelperTasks(lock).empty() || !parseWorklist(lock).empty() ||
         !compressionWorklist(lock).empty() || !ionFreeList(lock).empty() ||
         !wasmWorklist(lock, wasm::CompileMode::Tier2).empty() ||
         !wasmTier2GeneratorWorklist(lock).empty();
}

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

  AutoLockHelperThreadState lock;

  while (!terminate) {
    // The selectors may depend on the HelperThreadState not changing
    // between task selection and task execution, in particular, on new
    // tasks not being added (because of the lifo structure of the work
    // lists). Unlocking the HelperThreadState between task selection and
    // execution is not well-defined.

    HelperThreadTask* task = findHighestPriorityTask(lock);
    if (!task) {
      AUTO_PROFILER_LABEL("HelperThread::threadLoop::wait", IDLE);
      HelperThreadState().wait(lock, GlobalHelperThreadState::PRODUCER);
      continue;
    }

    HelperThreadState().runTaskLocked(task, lock);
  }
}

HelperThreadTask* HelperThread::findHighestPriorityTask(
    const AutoLockHelperThreadState& locked) {
  // Return the highest priority task that is ready to start, or nullptr.

  for (const auto& selector : selectors) {
    if (auto* task = (HelperThreadState().*(selector))(locked)) {
      return task;
    }
  }

  return nullptr;
}

void GlobalHelperThreadState::runTaskLocked(HelperThreadTask* task,
                                            AutoLockHelperThreadState& locked) {
  JS::AutoSuppressGCAnalysis nogc;

  HelperThreadState().helperTasks(locked).infallibleEmplaceBack(task);

  ThreadType threadType = task->threadType();
  js::oom::SetThreadType(threadType);
  runningTaskCount[threadType]++;
  totalCountRunningTasks++;

  task->runHelperThreadTask(locked);

  // Delete task from helperTasks.
  HelperThreadState().helperTasks(locked).eraseIfEqual(task);

  totalCountRunningTasks--;
  runningTaskCount[threadType]--;

  js::oom::SetThreadType(js::THREAD_TYPE_NONE);
  HelperThreadState().notifyAll(GlobalHelperThreadState::CONSUMER, locked);
}
