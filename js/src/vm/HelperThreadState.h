/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Definitions for managing off-thread work using a process wide list of
 * worklist items and pool of threads. Worklist items are engine internal, and
 * are distinct from e.g. web workers.
 */

#ifndef vm_HelperThreadState_h
#define vm_HelperThreadState_h

#include "mozilla/Attributes.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/TimeStamp.h"

#include "jsapi.h"

#include "ds/Fifo.h"
#include "frontend/CompilationStencil.h"  // CompilationStencil, ExtensibleCompilationStencil, CompilationGCOutput
#include "js/CompileOptions.h"
#include "js/TypeDecls.h"
#include "threading/ConditionVariable.h"
#include "threading/Thread.h"
#include "vm/HelperThreads.h"
#include "vm/HelperThreadTask.h"
#include "vm/JSContext.h"
#include "vm/OffThreadPromiseRuntimeState.h"  // js::OffThreadPromiseTask

namespace js {

class AutoLockHelperThreadState;
class AutoUnlockHelperThreadState;
class CompileError;
struct ParseTask;
struct PromiseHelperTask;
class PromiseObject;

namespace jit {
class IonCompileTask;
class IonFreeTask;
}  // namespace jit

namespace wasm {
struct Tier2GeneratorTask;
}  // namespace wasm

enum class ParseTaskKind { Script, Module, ScriptDecode, MultiScriptsDecode };
enum class StartEncoding { No, Yes };

namespace wasm {

struct CompileTask;
typedef Fifo<CompileTask*, 0, SystemAllocPolicy> CompileTaskPtrFifo;

struct Tier2GeneratorTask : public HelperThreadTask {
  virtual ~Tier2GeneratorTask() = default;
  virtual void cancel() = 0;
};

using UniqueTier2GeneratorTask = UniquePtr<Tier2GeneratorTask>;
typedef Vector<Tier2GeneratorTask*, 0, SystemAllocPolicy>
    Tier2GeneratorTaskPtrVector;

}  // namespace wasm

// Per-process state for off thread work items.
class GlobalHelperThreadState {
  friend class AutoLockHelperThreadState;
  friend class AutoUnlockHelperThreadState;

 public:
  // A single tier-2 ModuleGenerator job spawns many compilation jobs, and we
  // do not want to allow more than one such ModuleGenerator to run at a time.
  static const size_t MaxTier2GeneratorTasks = 1;

  // Number of CPUs to treat this machine as having when creating threads.
  // May be accessed without locking.
  size_t cpuCount;

  // Number of threads to create. May be accessed without locking.
  size_t threadCount;

  typedef Vector<jit::IonCompileTask*, 0, SystemAllocPolicy>
      IonCompileTaskVector;
  using IonFreeTaskVector =
      Vector<js::UniquePtr<jit::IonFreeTask>, 0, SystemAllocPolicy>;
  typedef Vector<UniquePtr<ParseTask>, 0, SystemAllocPolicy> ParseTaskVector;
  using ParseTaskList = mozilla::LinkedList<ParseTask>;
  typedef Vector<UniquePtr<SourceCompressionTask>, 0, SystemAllocPolicy>
      SourceCompressionTaskVector;
  using GCParallelTaskList = mozilla::LinkedList<GCParallelTask>;
  typedef Vector<PromiseHelperTask*, 0, SystemAllocPolicy>
      PromiseHelperTaskVector;
  typedef Vector<JSContext*, 0, SystemAllocPolicy> ContextVector;
  using HelperThreadVector =
      Vector<UniquePtr<HelperThread>, 0, SystemAllocPolicy>;

  // Count of running task by each threadType.
  mozilla::EnumeratedArray<ThreadType, ThreadType::THREAD_TYPE_MAX, size_t>
      runningTaskCount;
  size_t totalCountRunningTasks;

  WriteOnceData<JS::RegisterThreadCallback> registerThread;
  WriteOnceData<JS::UnregisterThreadCallback> unregisterThread;

 private:
  // The lists below are all protected by |lock|.

  // List of available helper threads.
  HelperThreadVector threads_;

  // Ion compilation worklist and finished jobs.
  IonCompileTaskVector ionWorklist_, ionFinishedList_;
  IonFreeTaskVector ionFreeList_;

  // wasm worklists.
  wasm::CompileTaskPtrFifo wasmWorklist_tier1_;
  wasm::CompileTaskPtrFifo wasmWorklist_tier2_;
  wasm::Tier2GeneratorTaskPtrVector wasmTier2GeneratorWorklist_;

  // Count of finished Tier2Generator tasks.
  uint32_t wasmTier2GeneratorsFinished_;

  // Async tasks that, upon completion, are dispatched back to the JSContext's
  // owner thread via embedding callbacks instead of a finished list.
  PromiseHelperTaskVector promiseHelperTasks_;

  // Script parsing/emitting worklist and finished jobs.
  ParseTaskVector parseWorklist_;
  ParseTaskList parseFinishedList_;

  // Parse tasks waiting for an atoms-zone GC to complete.
  ParseTaskVector parseWaitingOnGC_;

  // Source compression worklist of tasks that we do not yet know can start.
  SourceCompressionTaskVector compressionPendingList_;

  // Source compression worklist of tasks that can start.
  SourceCompressionTaskVector compressionWorklist_;

  // Finished source compression tasks.
  SourceCompressionTaskVector compressionFinishedList_;

  // GC tasks needing to be done in parallel.
  GCParallelTaskList gcParallelWorklist_;
  size_t gcParallelThreadCount;

  // Global list of JSContext for GlobalHelperThreadState to use.
  ContextVector helperContexts_;

  using HelperThreadTaskVector =
      Vector<HelperThreadTask*, 0, SystemAllocPolicy>;
  // Vector of running HelperThreadTask.
  // This is used to get the HelperThreadTask that are currently running.
  HelperThreadTaskVector helperTasks_;

  ParseTask* removeFinishedParseTask(JSContext* cx, ParseTaskKind kind,
                                     JS::OffThreadToken* token);

 public:
  void addSizeOfIncludingThis(JS::GlobalStats* stats,
                              AutoLockHelperThreadState& lock) const;

  size_t maxIonCompilationThreads() const;
  size_t maxWasmCompilationThreads() const;
  size_t maxWasmTier2GeneratorThreads() const;
  size_t maxPromiseHelperThreads() const;
  size_t maxParseThreads() const;
  size_t maxCompressionThreads() const;
  size_t maxGCParallelThreads(const AutoLockHelperThreadState& lock) const;

  GlobalHelperThreadState();

  HelperThreadVector& threads(const AutoLockHelperThreadState& lock) {
    return threads_;
  }
  const HelperThreadVector& threads(
      const AutoLockHelperThreadState& lock) const {
    return threads_;
  }

  bool ensureInitialized();
  bool ensureThreadCount(size_t count);
  void finish();
  void finishThreads();

  [[nodiscard]] bool ensureContextList(size_t count);
  JSContext* getFirstUnusedContext(AutoLockHelperThreadState& locked);
  void destroyHelperContexts(AutoLockHelperThreadState& lock);

#ifdef DEBUG
  void assertIsLockedByCurrentThread() const;
#endif

  enum CondVar {
    // For notifying threads waiting for work that they may be able to make
    // progress, ie, a work item has been completed by a helper thread and
    // the thread that created the work item can now consume it.
    CONSUMER,

    // For notifying helper threads doing the work that they may be able to
    // make progress, ie, a work item has been enqueued and an idle helper
    // thread may pick up up the work item and perform it.
    PRODUCER,
  };

  void wait(AutoLockHelperThreadState& locked, CondVar which,
            mozilla::TimeDuration timeout = mozilla::TimeDuration::Forever());
  void notifyAll(CondVar which, const AutoLockHelperThreadState&);

 private:
  void notifyOne(CondVar which, const AutoLockHelperThreadState&);

 public:
  // Helper method for removing items from the vectors below while iterating
  // over them.
  template <typename T>
  void remove(T& vector, size_t* index) {
    // Self-moving is undefined behavior.
    if (*index != vector.length() - 1) {
      vector[*index] = std::move(vector.back());
    }
    (*index)--;
    vector.popBack();
  }

  IonCompileTaskVector& ionWorklist(const AutoLockHelperThreadState&) {
    return ionWorklist_;
  }
  IonCompileTaskVector& ionFinishedList(const AutoLockHelperThreadState&) {
    return ionFinishedList_;
  }
  IonFreeTaskVector& ionFreeList(const AutoLockHelperThreadState&) {
    return ionFreeList_;
  }

  wasm::CompileTaskPtrFifo& wasmWorklist(const AutoLockHelperThreadState&,
                                         wasm::CompileMode m) {
    switch (m) {
      case wasm::CompileMode::Once:
      case wasm::CompileMode::Tier1:
        return wasmWorklist_tier1_;
      case wasm::CompileMode::Tier2:
        return wasmWorklist_tier2_;
      default:
        MOZ_CRASH();
    }
  }

  wasm::Tier2GeneratorTaskPtrVector& wasmTier2GeneratorWorklist(
      const AutoLockHelperThreadState&) {
    return wasmTier2GeneratorWorklist_;
  }

  void incWasmTier2GeneratorsFinished(const AutoLockHelperThreadState&) {
    wasmTier2GeneratorsFinished_++;
  }

  uint32_t wasmTier2GeneratorsFinished(const AutoLockHelperThreadState&) const {
    return wasmTier2GeneratorsFinished_;
  }

  PromiseHelperTaskVector& promiseHelperTasks(
      const AutoLockHelperThreadState&) {
    return promiseHelperTasks_;
  }

  ParseTaskVector& parseWorklist(const AutoLockHelperThreadState&) {
    return parseWorklist_;
  }
  ParseTaskList& parseFinishedList(const AutoLockHelperThreadState&) {
    return parseFinishedList_;
  }
  ParseTaskVector& parseWaitingOnGC(const AutoLockHelperThreadState&) {
    return parseWaitingOnGC_;
  }

  SourceCompressionTaskVector& compressionPendingList(
      const AutoLockHelperThreadState&) {
    return compressionPendingList_;
  }

  SourceCompressionTaskVector& compressionWorklist(
      const AutoLockHelperThreadState&) {
    return compressionWorklist_;
  }

  SourceCompressionTaskVector& compressionFinishedList(
      const AutoLockHelperThreadState&) {
    return compressionFinishedList_;
  }

  GCParallelTaskList& gcParallelWorklist(const AutoLockHelperThreadState&) {
    return gcParallelWorklist_;
  }

  void setGCParallelThreadCount(size_t count,
                                const AutoLockHelperThreadState&) {
    MOZ_ASSERT(count >= 1);
    MOZ_ASSERT(count <= threadCount);
    gcParallelThreadCount = count;
  }

  HelperThreadTaskVector& helperTasks(const AutoLockHelperThreadState&) {
    return helperTasks_;
  }

  HelperThreadTask* maybeGetWasmCompile(const AutoLockHelperThreadState& lock,
                                        wasm::CompileMode mode);

  HelperThreadTask* maybeGetWasmTier1CompileTask(
      const AutoLockHelperThreadState& lock);
  HelperThreadTask* maybeGetWasmTier2CompileTask(
      const AutoLockHelperThreadState& lock);
  HelperThreadTask* maybeGetWasmTier2GeneratorTask(
      const AutoLockHelperThreadState& lock);
  HelperThreadTask* maybeGetPromiseHelperTask(
      const AutoLockHelperThreadState& lock);
  HelperThreadTask* maybeGetIonCompileTask(
      const AutoLockHelperThreadState& lock);
  HelperThreadTask* maybeGetIonFreeTask(const AutoLockHelperThreadState& lock);
  HelperThreadTask* maybeGetParseTask(const AutoLockHelperThreadState& lock);
  HelperThreadTask* maybeGetCompressionTask(
      const AutoLockHelperThreadState& lock);
  HelperThreadTask* maybeGetGCParallelTask(
      const AutoLockHelperThreadState& lock);

  enum class ScheduleCompressionTask { GC, API };

  // Used by a major GC to signal processing enqueued compression tasks.
  void startHandlingCompressionTasks(ScheduleCompressionTask schedule,
                                     JSRuntime* maybeRuntime,
                                     const AutoLockHelperThreadState& lock);

  jit::IonCompileTask* highestPriorityPendingIonCompile(
      const AutoLockHelperThreadState& lock);

 private:
  UniquePtr<ParseTask> finishParseTaskCommon(JSContext* cx, ParseTaskKind kind,
                                             JS::OffThreadToken* token);

  JSScript* finishSingleParseTask(
      JSContext* cx, ParseTaskKind kind, JS::OffThreadToken* token,
      StartEncoding startEncoding = StartEncoding::No);
  bool generateLCovSources(JSContext* cx, ParseTask* parseTask);
  bool finishMultiParseTask(JSContext* cx, ParseTaskKind kind,
                            JS::OffThreadToken* token,
                            MutableHandle<ScriptVector> scripts);

  void mergeParseTaskRealm(JSContext* cx, ParseTask* parseTask,
                           JS::Realm* dest);

 public:
  void cancelParseTask(JSRuntime* rt, ParseTaskKind kind,
                       JS::OffThreadToken* token);
  void destroyParseTask(JSRuntime* rt, ParseTask* parseTask);

  void trace(JSTracer* trc);

  JSScript* finishScriptParseTask(
      JSContext* cx, JS::OffThreadToken* token,
      StartEncoding startEncoding = StartEncoding::No);
  JSScript* finishScriptDecodeTask(JSContext* cx, JS::OffThreadToken* token);
  bool finishMultiScriptsDecodeTask(JSContext* cx, JS::OffThreadToken* token,
                                    MutableHandle<ScriptVector> scripts);
  JSObject* finishModuleParseTask(JSContext* cx, JS::OffThreadToken* token);

  frontend::CompilationStencil* finishStencilParseTask(
      JSContext* cx, JS::OffThreadToken* token);

  bool hasActiveThreads(const AutoLockHelperThreadState&);
  bool hasQueuedTasks(const AutoLockHelperThreadState& locked);
  void waitForAllThreads();
  void waitForAllThreadsLocked(AutoLockHelperThreadState&);

  bool checkTaskThreadLimit(ThreadType threadType, size_t maxThreads,
                            bool isMaster,
                            const AutoLockHelperThreadState& lock) const;
  bool checkTaskThreadLimit(ThreadType threadType, size_t maxThreads,
                            const AutoLockHelperThreadState& lock) const {
    return checkTaskThreadLimit(threadType, maxThreads, /* isMaster */ false,
                                lock);
  }

  void triggerFreeUnusedMemory();

 private:
  /* Condvars for threads waiting/notifying each other. */
  js::ConditionVariable consumerWakeup;
  js::ConditionVariable producerWakeup;

  js::ConditionVariable& whichWakeup(CondVar which) {
    switch (which) {
      case CONSUMER:
        return consumerWakeup;
      case PRODUCER:
        return producerWakeup;
      default:
        MOZ_CRASH("Invalid CondVar in |whichWakeup|");
    }
  }

  void dispatch(const AutoLockHelperThreadState& locked);

 public:
  bool submitTask(wasm::UniqueTier2GeneratorTask task);
  bool submitTask(wasm::CompileTask* task, wasm::CompileMode mode);
  bool submitTask(UniquePtr<jit::IonFreeTask> task,
                  const AutoLockHelperThreadState& lock);
  bool submitTask(jit::IonCompileTask* task,
                  const AutoLockHelperThreadState& locked);
  bool submitTask(UniquePtr<SourceCompressionTask> task,
                  const AutoLockHelperThreadState& locked);
  bool submitTask(JSRuntime* rt, UniquePtr<ParseTask> task,
                  const AutoLockHelperThreadState& locked);
  bool submitTask(PromiseHelperTask* task);
  bool submitTask(GCParallelTask* task,
                  const AutoLockHelperThreadState& locked);
  void runTaskLocked(HelperThreadTask* task, AutoLockHelperThreadState& lock);
};

static inline GlobalHelperThreadState& HelperThreadState() {
  extern GlobalHelperThreadState* gHelperThreadState;

  MOZ_ASSERT(gHelperThreadState);
  return *gHelperThreadState;
}

/* Individual helper thread, one allocated per core. */
class HelperThread {
  Thread thread;

  /*
   * The profiling thread for this helper thread, which can be used to push
   * and pop label frames.
   * This field being non-null indicates that this thread has been registered
   * and needs to be unregistered at shutdown.
   */
  ProfilingStack* profilingStack = nullptr;

  /*
   * Indicate to a thread that it should terminate itself. This is only read
   * or written with the helper thread state lock held.
   */
  bool terminate = false;

 public:
  HelperThread();
  [[nodiscard]] bool init();

  ThreadId threadId() { return thread.get_id(); }

  void setTerminate(const AutoLockHelperThreadState& lock);
  void join();

  static void ThreadMain(void* arg);
  void threadLoop();

  void ensureRegisteredWithProfiler();
  void unregisterWithProfilerIfNeeded();

 private:
  struct AutoProfilerLabel {
    AutoProfilerLabel(HelperThread* helperThread, const char* label,
                      JS::ProfilingCategoryPair categoryPair);
    ~AutoProfilerLabel();

   private:
    ProfilingStack* profilingStack;
  };

  using Selector = HelperThreadTask* (
      GlobalHelperThreadState::*)(const AutoLockHelperThreadState&);
  static const Selector selectors[];

  HelperThreadTask* findHighestPriorityTask(
      const AutoLockHelperThreadState& locked);
};

class MOZ_RAII AutoSetHelperThreadContext {
  JSContext* cx;
  AutoLockHelperThreadState& lock;

 public:
  explicit AutoSetHelperThreadContext(AutoLockHelperThreadState& lock);
  ~AutoSetHelperThreadContext();
};

struct MOZ_RAII AutoSetContextRuntime {
  explicit AutoSetContextRuntime(JSRuntime* rt) {
    TlsContext.get()->setRuntime(rt);
  }
  ~AutoSetContextRuntime() { TlsContext.get()->setRuntime(nullptr); }
};

struct ParseTask : public mozilla::LinkedListElement<ParseTask>,
                   public JS::OffThreadToken,
                   public HelperThreadTask {
  ParseTaskKind kind;
  JS::OwningCompileOptions options;

  // HelperThreads are shared between all runtimes in the process so explicitly
  // track which one we are associated with.
  JSRuntime* runtime = nullptr;

  // The global object to use while parsing.
  JSObject* parseGlobal;

  // Callback invoked off thread when the parse finishes.
  JS::OffThreadCompileCallback callback;
  void* callbackData;

  // Holds the final scripts between the invocation of the callback and the
  // point where FinishOffThreadScript is called, which will destroy the
  // ParseTask.
  GCVector<JSScript*, 1, SystemAllocPolicy> scripts;

  // Holds the ScriptSourceObjects generated for the script compilation.
  GCVector<ScriptSourceObject*, 1, SystemAllocPolicy> sourceObjects;

  // The input of the compilation.
  UniquePtr<frontend::CompilationInput> stencilInput_;

  // The output of the decode task.
  UniquePtr<frontend::CompilationStencil> stencil_;

  // The output of the script/module compilation task.
  UniquePtr<frontend::ExtensibleCompilationStencil> extensibleStencil_;

  frontend::CompilationGCOutput gcOutput_;

  // Any errors or warnings produced during compilation. These are reported
  // when finishing the script.
  Vector<UniquePtr<CompileError>, 0, SystemAllocPolicy> errors;
  bool overRecursed;
  bool outOfMemory;

  ParseTask(ParseTaskKind kind, JSContext* cx,
            JS::OffThreadCompileCallback callback, void* callbackData);
  virtual ~ParseTask();

  bool init(JSContext* cx, const JS::ReadOnlyCompileOptions& options,
            JSObject* global);

  void activate(JSRuntime* rt);
  virtual void parse(JSContext* cx) = 0;
  bool instantiateStencils(JSContext* cx);

  bool runtimeMatches(JSRuntime* rt) { return runtime == rt; }

  void trace(JSTracer* trc);

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
    return mallocSizeOf(this) + sizeOfExcludingThis(mallocSizeOf);
  }

  void runHelperThreadTask(AutoLockHelperThreadState& locked) override;
  void runTask(AutoLockHelperThreadState& lock);
  ThreadType threadType() override { return ThreadType::THREAD_TYPE_PARSE; }
};

struct ScriptDecodeTask : public ParseTask {
  const JS::TranscodeRange range;

  ScriptDecodeTask(JSContext* cx, const JS::TranscodeRange& range,
                   JS::OffThreadCompileCallback callback, void* callbackData);
  void parse(JSContext* cx) override;
};

struct MultiScriptsDecodeTask : public ParseTask {
  JS::TranscodeSources* sources;

  MultiScriptsDecodeTask(JSContext* cx, JS::TranscodeSources& sources,
                         JS::OffThreadCompileCallback callback,
                         void* callbackData);
  void parse(JSContext* cx) override;
};

// It is not desirable to eagerly compress: if lazy functions that are tied to
// the ScriptSource were to be executed relatively soon after parsing, they
// would need to block on decompression, which hurts responsiveness.
//
// To this end, compression tasks are heap allocated and enqueued in a pending
// list by ScriptSource::setSourceCopy. When a major GC occurs, we schedule
// pending compression tasks and move the ones that are ready to be compressed
// to the worklist. Currently, a compression task is considered ready 2 major
// GCs after being enqueued. Completed tasks are handled during the sweeping
// phase by AttachCompressedSourcesTask, which runs in parallel with other GC
// sweeping tasks.
class SourceCompressionTask : public HelperThreadTask {
  friend class HelperThread;
  friend class ScriptSource;

  // The runtime that the ScriptSource is associated with, in the sense that
  // it uses the runtime's immutable string cache.
  JSRuntime* runtime_;

  // The major GC number of the runtime when the task was enqueued.
  uint64_t majorGCNumber_;

  // The source to be compressed.
  RefPtr<ScriptSource> source_;

  // The resultant compressed string. If the compressed string is larger
  // than the original, or we OOM'd during compression, or nothing else
  // except the task is holding the ScriptSource alive when scheduled to
  // compress, this will remain None upon completion.
  mozilla::Maybe<SharedImmutableString> resultString_;

 public:
  // The majorGCNumber is used for scheduling tasks.
  SourceCompressionTask(JSRuntime* rt, ScriptSource* source)
      : runtime_(rt), majorGCNumber_(rt->gc.majorGCCount()), source_(source) {
    source->noteSourceCompressionTask();
  }
  virtual ~SourceCompressionTask() = default;

  bool runtimeMatches(JSRuntime* runtime) const { return runtime == runtime_; }
  bool shouldStart() const {
    // We wait 2 major GCs to start compressing, in order to avoid
    // immediate compression.
    return runtime_->gc.majorGCCount() > majorGCNumber_ + 1;
  }

  bool shouldCancel() const {
    // If the refcount is exactly 1, then nothing else is holding on to the
    // ScriptSource, so no reason to compress it and we should cancel the task.
    return source_->refs == 1;
  }

  void runTask();
  void runHelperThreadTask(AutoLockHelperThreadState& locked) override;
  void complete();

  ThreadType threadType() override { return ThreadType::THREAD_TYPE_COMPRESS; }

 private:
  struct PerformTaskWork;
  friend struct PerformTaskWork;

  // The work algorithm, aware whether it's compressing one-byte UTF-8 source
  // text or UTF-16, for CharT either Utf8Unit or char16_t.  Invoked by
  // work() after doing a type-test of the ScriptSource*.
  template <typename CharT>
  void workEncodingSpecific();
};

// A PromiseHelperTask is an OffThreadPromiseTask that executes a single job on
// a helper thread. Call js::StartOffThreadPromiseHelperTask to submit a
// PromiseHelperTask for execution.
//
// Concrete subclasses must implement execute and OffThreadPromiseTask::resolve.
// The helper thread will call execute() to do the main work. Then, the thread
// of the JSContext used to create the PromiseHelperTask will call resolve() to
// resolve promise according to those results.
struct PromiseHelperTask : OffThreadPromiseTask, public HelperThreadTask {
  PromiseHelperTask(JSContext* cx, Handle<PromiseObject*> promise)
      : OffThreadPromiseTask(cx, promise) {}

  // To be called on a helper thread and implemented by the derived class.
  virtual void execute() = 0;

  // May be called in the absence of helper threads or off-thread promise
  // support to synchronously execute and resolve a PromiseTask.
  //
  // Warning: After this function returns, 'this' can be deleted at any time, so
  // the caller must immediately return from the stream callback.
  void executeAndResolveAndDestroy(JSContext* cx);

  void runHelperThreadTask(AutoLockHelperThreadState& locked) override;
  ThreadType threadType() override { return THREAD_TYPE_PROMISE_TASK; }
};

} /* namespace js */

#endif /* vm_HelperThreadState_h */
