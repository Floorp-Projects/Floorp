/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Definitions for managing off-thread work using a process wide list
 * of worklist items and pool of threads. Worklist items are engine internal,
 * and are distinct from e.g. web workers.
 */

#ifndef vm_HelperThreads_h
#define vm_HelperThreads_h

#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/PodOperations.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Variant.h"

#include "jsapi.h"
#include "jscntxt.h"

#include "frontend/TokenStream.h"
#include "jit/Ion.h"
#include "threading/ConditionVariable.h"
#include "vm/MutexIDs.h"

namespace JS {
struct Zone;
} // namespace JS

namespace js {

class AutoLockHelperThreadState;
class AutoUnlockHelperThreadState;
class CompileError;
class PromiseTask;
struct HelperThread;
struct ParseTask;
namespace jit {
  class IonBuilder;
} // namespace jit
namespace wasm {
  class FuncIR;
  class FunctionCompileResults;
  class CompileTask;
  typedef Vector<CompileTask*, 0, SystemAllocPolicy> CompileTaskPtrVector;
} // namespace wasm

enum class ParseTaskKind
{
    Script,
    Module,
    ScriptDecode
};

// Per-process state for off thread work items.
class GlobalHelperThreadState
{
    friend class AutoLockHelperThreadState;
    friend class AutoUnlockHelperThreadState;

  public:
    // Number of CPUs to treat this machine as having when creating threads.
    // May be accessed without locking.
    size_t cpuCount;

    // Number of threads to create. May be accessed without locking.
    size_t threadCount;

    typedef Vector<jit::IonBuilder*, 0, SystemAllocPolicy> IonBuilderVector;
    typedef Vector<ParseTask*, 0, SystemAllocPolicy> ParseTaskVector;
    typedef Vector<UniquePtr<SourceCompressionTask>, 0, SystemAllocPolicy> SourceCompressionTaskVector;
    typedef Vector<GCHelperState*, 0, SystemAllocPolicy> GCHelperStateVector;
    typedef Vector<GCParallelTask*, 0, SystemAllocPolicy> GCParallelTaskVector;
    typedef Vector<PromiseTask*, 0, SystemAllocPolicy> PromiseTaskVector;

    // List of available threads, or null if the thread state has not been initialized.
    using HelperThreadVector = Vector<HelperThread, 0, SystemAllocPolicy>;
    UniquePtr<HelperThreadVector> threads;

  private:
    // The lists below are all protected by |lock|.

    // Ion compilation worklist and finished jobs.
    IonBuilderVector ionWorklist_, ionFinishedList_;

    // wasm worklist and finished jobs.
    wasm::CompileTaskPtrVector wasmWorklist_, wasmFinishedList_;

  public:
    // For now, only allow a single parallel wasm compilation to happen at a
    // time. This avoids race conditions on wasmWorklist/wasmFinishedList/etc.
    mozilla::Atomic<bool> wasmCompilationInProgress;

  private:
    // Async tasks that, upon completion, are dispatched back to the JSContext's
    // owner thread via embedding callbacks instead of a finished list.
    PromiseTaskVector promiseTasks_;

    // Script parsing/emitting worklist and finished jobs.
    ParseTaskVector parseWorklist_, parseFinishedList_;

    // Parse tasks waiting for an atoms-zone GC to complete.
    ParseTaskVector parseWaitingOnGC_;

    // Source compression worklist of tasks that we do not yet know can start.
    SourceCompressionTaskVector compressionPendingList_;

    // Source compression worklist of tasks that can start.
    SourceCompressionTaskVector compressionWorklist_;

    // Finished source compression tasks.
    SourceCompressionTaskVector compressionFinishedList_;

    // Runtimes which have sweeping / allocating work to do.
    GCHelperStateVector gcHelperWorklist_;

    // GC tasks needing to be done in parallel.
    GCParallelTaskVector gcParallelWorklist_;

    ParseTask* removeFinishedParseTask(ParseTaskKind kind, void* token);

  public:
    size_t maxIonCompilationThreads() const;
    size_t maxUnpausedIonCompilationThreads() const;
    size_t maxWasmCompilationThreads() const;
    size_t maxParseThreads() const;
    size_t maxCompressionThreads() const;
    size_t maxGCHelperThreads() const;
    size_t maxGCParallelThreads() const;

    GlobalHelperThreadState();

    bool ensureInitialized();
    void finish();
    void finishThreads();

    void lock();
    void unlock();
#ifdef DEBUG
    bool isLockedByCurrentThread();
#endif

    enum CondVar {
        // For notifying threads waiting for work that they may be able to make progress.
        CONSUMER,

        // For notifying threads doing work that they may be able to make progress.
        PRODUCER,

        // For notifying threads doing work which are paused that they may be
        // able to resume making progress.
        PAUSE
    };

    void wait(AutoLockHelperThreadState& locked, CondVar which,
              mozilla::TimeDuration timeout = mozilla::TimeDuration::Forever());
    void notifyAll(CondVar which, const AutoLockHelperThreadState&);
    void notifyOne(CondVar which, const AutoLockHelperThreadState&);

    // Helper method for removing items from the vectors below while iterating over them.
    template <typename T>
    void remove(T& vector, size_t* index)
    {
        // Self-moving is undefined behavior.
        if (*index != vector.length() - 1)
            vector[*index] = mozilla::Move(vector.back());
        (*index)--;
        vector.popBack();
    }

    IonBuilderVector& ionWorklist(const AutoLockHelperThreadState&) {
        return ionWorklist_;
    }
    IonBuilderVector& ionFinishedList(const AutoLockHelperThreadState&) {
        return ionFinishedList_;
    }

    wasm::CompileTaskPtrVector& wasmWorklist(const AutoLockHelperThreadState&) {
        return wasmWorklist_;
    }
    wasm::CompileTaskPtrVector& wasmFinishedList(const AutoLockHelperThreadState&) {
        return wasmFinishedList_;
    }

    PromiseTaskVector& promiseTasks(const AutoLockHelperThreadState&) {
        return promiseTasks_;
    }

    ParseTaskVector& parseWorklist(const AutoLockHelperThreadState&) {
        return parseWorklist_;
    }
    ParseTaskVector& parseFinishedList(const AutoLockHelperThreadState&) {
        return parseFinishedList_;
    }
    ParseTaskVector& parseWaitingOnGC(const AutoLockHelperThreadState&) {
        return parseWaitingOnGC_;
    }

    SourceCompressionTaskVector& compressionPendingList(const AutoLockHelperThreadState&) {
        return compressionPendingList_;
    }

    SourceCompressionTaskVector& compressionWorklist(const AutoLockHelperThreadState&) {
        return compressionWorklist_;
    }

    SourceCompressionTaskVector& compressionFinishedList(const AutoLockHelperThreadState&) {
        return compressionFinishedList_;
    }

    GCHelperStateVector& gcHelperWorklist(const AutoLockHelperThreadState&) {
        return gcHelperWorklist_;
    }

    GCParallelTaskVector& gcParallelWorklist(const AutoLockHelperThreadState&) {
        return gcParallelWorklist_;
    }

    bool canStartWasmCompile(const AutoLockHelperThreadState& lock);
    bool canStartPromiseTask(const AutoLockHelperThreadState& lock);
    bool canStartIonCompile(const AutoLockHelperThreadState& lock);
    bool canStartParseTask(const AutoLockHelperThreadState& lock);
    bool canStartCompressionTask(const AutoLockHelperThreadState& lock);
    bool canStartGCHelperTask(const AutoLockHelperThreadState& lock);
    bool canStartGCParallelTask(const AutoLockHelperThreadState& lock);

    // Used by a major GC to signal processing enqueued compression tasks.
    void startHandlingCompressionTasks(const AutoLockHelperThreadState&);
    void scheduleCompressionTasks(const AutoLockHelperThreadState&);

    // Unlike the methods above, the value returned by this method can change
    // over time, even if the helper thread state lock is held throughout.
    bool pendingIonCompileHasSufficientPriority(const AutoLockHelperThreadState& lock);

    jit::IonBuilder* highestPriorityPendingIonCompile(const AutoLockHelperThreadState& lock,
                                                      bool remove = false);
    HelperThread* lowestPriorityUnpausedIonCompileAtThreshold(
        const AutoLockHelperThreadState& lock);
    HelperThread* highestPriorityPausedIonCompile(const AutoLockHelperThreadState& lock);

    uint32_t harvestFailedWasmJobs(const AutoLockHelperThreadState&) {
        uint32_t n = numWasmFailedJobs;
        numWasmFailedJobs = 0;
        return n;
    }
    UniqueChars harvestWasmError(const AutoLockHelperThreadState&) {
        return Move(firstWasmError);
    }
    void noteWasmFailure(const AutoLockHelperThreadState&) {
        // Be mindful to signal the active thread after calling this function.
        numWasmFailedJobs++;
    }
    void setWasmError(const AutoLockHelperThreadState&, UniqueChars error) {
        if (!firstWasmError)
            firstWasmError = Move(error);
    }
    bool wasmFailed(const AutoLockHelperThreadState&) {
        return bool(numWasmFailedJobs);
    }

    JSScript* finishParseTask(JSContext* cx, ParseTaskKind kind, void* token);
    void cancelParseTask(JSRuntime* rt, ParseTaskKind kind, void* token);

    void mergeParseTaskCompartment(JSContext* cx, ParseTask* parseTask,
                                   Handle<GlobalObject*> global,
                                   JSCompartment* dest);

    void trace(JSTracer* trc);

  private:
    /*
     * Number of wasm jobs that encountered failure for the active module.
     * Their parent is logically the active thread, and this number serves for harvesting.
     */
    uint32_t numWasmFailedJobs;
    /*
     * Error string from wasm validation. Arbitrarily choose to keep the first one that gets
     * reported. Nondeterministic if multiple threads have errors.
     */
    UniqueChars firstWasmError;

  public:
    JSScript* finishScriptParseTask(JSContext* cx, void* token);
    JSScript* finishScriptDecodeTask(JSContext* cx, void* token);
    JSObject* finishModuleParseTask(JSContext* cx, void* token);

    bool hasActiveThreads(const AutoLockHelperThreadState&);
    void waitForAllThreads();

    template <typename T>
    bool checkTaskThreadLimit(size_t maxThreads) const;

  private:

    /*
     * Lock protecting all mutable shared state accessed by helper threads, and
     * used by all condition variables.
     */
    js::Mutex helperLock;

    /* Condvars for threads waiting/notifying each other. */
    js::ConditionVariable consumerWakeup;
    js::ConditionVariable producerWakeup;
    js::ConditionVariable pauseWakeup;

    js::ConditionVariable& whichWakeup(CondVar which) {
        switch (which) {
          case CONSUMER: return consumerWakeup;
          case PRODUCER: return producerWakeup;
          case PAUSE: return pauseWakeup;
          default: MOZ_CRASH("Invalid CondVar in |whichWakeup|");
        }
    }
};

static inline GlobalHelperThreadState&
HelperThreadState()
{
    extern GlobalHelperThreadState* gHelperThreadState;

    MOZ_ASSERT(gHelperThreadState);
    return *gHelperThreadState;
}

/* Individual helper thread, one allocated per core. */
struct HelperThread
{
    mozilla::Maybe<Thread> thread;

    /*
     * Indicate to a thread that it should terminate itself. This is only read
     * or written with the helper thread state lock held.
     */
    bool terminate;

    /*
     * Indicate to a thread that it should pause execution. This is only
     * written with the helper thread state lock held, but may be read from
     * without the lock held.
     */
    mozilla::Atomic<bool, mozilla::Relaxed> pause;

    /* The current task being executed by this thread, if any. */
    mozilla::Maybe<mozilla::Variant<jit::IonBuilder*,
                                    wasm::CompileTask*,
                                    PromiseTask*,
                                    ParseTask*,
                                    SourceCompressionTask*,
                                    GCHelperState*,
                                    GCParallelTask*>> currentTask;

    bool idle() const {
        return currentTask.isNothing();
    }

    /* Any builder currently being compiled by Ion on this thread. */
    jit::IonBuilder* ionBuilder() {
        return maybeCurrentTaskAs<jit::IonBuilder*>();
    }

    /* Any wasm data currently being optimized on this thread. */
    wasm::CompileTask* wasmTask() {
        return maybeCurrentTaskAs<wasm::CompileTask*>();
    }

    /* Any source being parsed/emitted on this thread. */
    ParseTask* parseTask() {
        return maybeCurrentTaskAs<ParseTask*>();
    }

    /* Any source being compressed on this thread. */
    SourceCompressionTask* compressionTask() {
        return maybeCurrentTaskAs<SourceCompressionTask*>();
    }

    /* Any GC state for background sweeping or allocating being performed. */
    GCHelperState* gcHelperTask() {
        return maybeCurrentTaskAs<GCHelperState*>();
    }

    /* State required to perform a GC parallel task. */
    GCParallelTask* gcParallelTask() {
        return maybeCurrentTaskAs<GCParallelTask*>();
    }

    void destroy();

    static void ThreadMain(void* arg);
    void threadLoop();

  private:
    template <typename T>
    T maybeCurrentTaskAs() {
        if (currentTask.isSome() && currentTask->is<T>())
            return currentTask->as<T>();

        return nullptr;
    }

    void handleWasmWorkload(AutoLockHelperThreadState& locked);
    void handlePromiseTaskWorkload(AutoLockHelperThreadState& locked);
    void handleIonWorkload(AutoLockHelperThreadState& locked);
    void handleParseWorkload(AutoLockHelperThreadState& locked);
    void handleCompressionWorkload(AutoLockHelperThreadState& locked);
    void handleGCHelperWorkload(AutoLockHelperThreadState& locked);
    void handleGCParallelWorkload(AutoLockHelperThreadState& locked);
};

/* Methods for interacting with helper threads. */

// Create data structures used by helper threads.
bool
CreateHelperThreadsState();

// Destroy data structures used by helper threads.
void
DestroyHelperThreadsState();

// Initialize helper threads unless already initialized.
bool
EnsureHelperThreadsInitialized();

// This allows the JS shell to override GetCPUCount() when passed the
// --thread-count=N option.
void
SetFakeCPUCount(size_t count);

// Get the current helper thread, or null.
HelperThread*
CurrentHelperThread();

// Pause the current thread until it's pause flag is unset.
void
PauseCurrentHelperThread();

// Enqueues a wasm compilation task.
bool
StartOffThreadWasmCompile(wasm::CompileTask* task);

namespace wasm {

// Performs MIR optimization and LIR generation on one or several functions.
MOZ_MUST_USE bool
CompileFunction(CompileTask* task, UniqueChars* error);

}

/*
 * If helper threads are available, start executing the given PromiseTask on a
 * helper thread, finishing back on the originating JSContext's owner thread. If
 * no helper threads are available, the PromiseTask is synchronously executed
 * and finished.
 */
bool
StartPromiseTask(JSContext* cx, UniquePtr<PromiseTask> task);

/*
 * Schedule an Ion compilation for a script, given a builder which has been
 * generated and read everything needed from the VM state.
 */
bool
StartOffThreadIonCompile(JSContext* cx, jit::IonBuilder* builder);

struct AllCompilations {};
struct ZonesInState { JSRuntime* runtime; JS::Zone::GCState state; };

using CompilationSelector = mozilla::Variant<JSScript*,
                                             JSCompartment*,
                                             ZonesInState,
                                             JSRuntime*,
                                             AllCompilations>;

/*
 * Cancel scheduled or in progress Ion compilations.
 */
void
CancelOffThreadIonCompile(const CompilationSelector& selector, bool discardLazyLinkList);

inline void
CancelOffThreadIonCompile(JSScript* script)
{
    CancelOffThreadIonCompile(CompilationSelector(script), true);
}

inline void
CancelOffThreadIonCompile(JSCompartment* comp)
{
    CancelOffThreadIonCompile(CompilationSelector(comp), true);
}

inline void
CancelOffThreadIonCompile(JSRuntime* runtime, JS::Zone::GCState state)
{
    CancelOffThreadIonCompile(CompilationSelector(ZonesInState{runtime, state}), true);
}

inline void
CancelOffThreadIonCompile(JSRuntime* runtime)
{
    CancelOffThreadIonCompile(CompilationSelector(runtime), true);
}

inline void
CancelOffThreadIonCompile()
{
    CancelOffThreadIonCompile(CompilationSelector(AllCompilations()), false);
}

#ifdef DEBUG
bool
HasOffThreadIonCompile(JSCompartment* comp);
#endif

/* Cancel all scheduled, in progress or finished parses for runtime. */
void
CancelOffThreadParses(JSRuntime* runtime);

/*
 * Start a parse/emit cycle for a stream of source. The characters must stay
 * alive until the compilation finishes.
 */
bool
StartOffThreadParseScript(JSContext* cx, const ReadOnlyCompileOptions& options,
                          const char16_t* chars, size_t length,
                          JS::OffThreadCompileCallback callback, void* callbackData);

bool
StartOffThreadParseModule(JSContext* cx, const ReadOnlyCompileOptions& options,
                          const char16_t* chars, size_t length,
                          JS::OffThreadCompileCallback callback, void* callbackData);

bool
StartOffThreadDecodeScript(JSContext* cx, const ReadOnlyCompileOptions& options,
                           JS::TranscodeBuffer& buffer, size_t cursor,
                           JS::OffThreadCompileCallback callback, void* callbackData);

/*
 * Called at the end of GC to enqueue any Parse tasks that were waiting on an
 * atoms-zone GC to finish.
 */
void
EnqueuePendingParseTasksAfterGC(JSRuntime* rt);

struct AutoEnqueuePendingParseTasksAfterGC {
    const gc::GCRuntime& gc_;
    explicit AutoEnqueuePendingParseTasksAfterGC(const gc::GCRuntime& gc) : gc_(gc) {}
    ~AutoEnqueuePendingParseTasksAfterGC();
};

// Enqueue a compression job to be processed if there's a major GC.
bool
EnqueueOffThreadCompression(JSContext* cx, UniquePtr<SourceCompressionTask> task);

// Cancel all scheduled, in progress, or finished compression tasks for
// runtime.
void
CancelOffThreadCompressions(JSRuntime* runtime);

class MOZ_RAII AutoLockHelperThreadState : public LockGuard<Mutex>
{
    using Base = LockGuard<Mutex>;

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    explicit AutoLockHelperThreadState(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM)
      : Base(HelperThreadState().helperLock)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
};

class MOZ_RAII AutoUnlockHelperThreadState : public UnlockGuard<Mutex>
{
    using Base = UnlockGuard<Mutex>;

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:

    explicit AutoUnlockHelperThreadState(AutoLockHelperThreadState& locked
                                         MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : Base(locked)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
};

struct ParseTask
{
    ParseTaskKind kind;
    OwningCompileOptions options;
    // Anonymous union, the only correct interpretation is provided by the
    // ParseTaskKind value, or from the virtual parse function.
    union {
        struct {
            const char16_t* chars;
            size_t length;
        };
        struct {
            // This should be a reference, but C++ prevents us from using union
            // with references as it assumes the reference constness might be
            // violated.
            JS::TranscodeBuffer* const buffer;
            size_t cursor;
        };
    };
    LifoAlloc alloc;

    // Rooted pointer to the global object to use while parsing.
    JSObject* parseGlobal;

    // Callback invoked off thread when the parse finishes.
    JS::OffThreadCompileCallback callback;
    void* callbackData;

    // Holds the final script between the invocation of the callback and the
    // point where FinishOffThreadScript is called, which will destroy the
    // ParseTask.
    JSScript* script;

    // Holds the ScriptSourceObject generated for the script compilation.
    ScriptSourceObject* sourceObject;

    // Any errors or warnings produced during compilation. These are reported
    // when finishing the script.
    Vector<CompileError*, 0, SystemAllocPolicy> errors;
    bool overRecursed;
    bool outOfMemory;

    ParseTask(ParseTaskKind kind, JSContext* cx, JSObject* parseGlobal,
              const char16_t* chars, size_t length,
              JS::OffThreadCompileCallback callback, void* callbackData);
    ParseTask(ParseTaskKind kind, JSContext* cx, JSObject* parseGlobal,
              JS::TranscodeBuffer& buffer, size_t cursor,
              JS::OffThreadCompileCallback callback, void* callbackData);
    bool init(JSContext* cx, const ReadOnlyCompileOptions& options);

    void activate(JSRuntime* rt);
    virtual void parse(JSContext* cx) = 0;
    bool finish(JSContext* cx);

    bool runtimeMatches(JSRuntime* rt) {
        return parseGlobal->runtimeFromAnyThread() == rt;
    }

    virtual ~ParseTask();

    void trace(JSTracer* trc);
};

struct ScriptParseTask : public ParseTask
{
    ScriptParseTask(JSContext* cx, JSObject* parseGlobal,
                    const char16_t* chars, size_t length,
                    JS::OffThreadCompileCallback callback, void* callbackData);
    void parse(JSContext* cx) override;
};

struct ModuleParseTask : public ParseTask
{
    ModuleParseTask(JSContext* cx, JSObject* parseGlobal,
                    const char16_t* chars, size_t length,
                    JS::OffThreadCompileCallback callback, void* callbackData);
    void parse(JSContext* cx) override;
};

struct ScriptDecodeTask : public ParseTask
{
    ScriptDecodeTask(JSContext* cx, JSObject* parseGlobal,
                     JS::TranscodeBuffer& buffer, size_t cursor,
                     JS::OffThreadCompileCallback callback, void* callbackData);
    void parse(JSContext* cx) override;
};

// Return whether, if a new parse task was started, it would need to wait for
// an in-progress GC to complete before starting.
extern bool
OffThreadParsingMustWaitForGC(JSRuntime* rt);

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
class SourceCompressionTask
{
    friend struct HelperThread;
    friend class ScriptSource;

    // The runtime that the ScriptSource is associated with, in the sense that
    // it uses the runtime's immutable string cache.
    JSRuntime* runtime_;

    // The major GC number of the runtime when the task was enqueued.
    uint64_t majorGCNumber_;

    // The source to be compressed.
    ScriptSourceHolder sourceHolder_;

    // The resultant compressed string. If the compressed string is larger
    // than the original, or we OOM'd during compression, or nothing else
    // except the task is holding the ScriptSource alive when scheduled to
    // compress, this will remain None upon completion.
    mozilla::Maybe<SharedImmutableString> resultString_;

  public:
    // The majorGCNumber is used for scheduling tasks.
    SourceCompressionTask(JSRuntime* rt, ScriptSource* source)
      : runtime_(rt),
        majorGCNumber_(rt->gc.majorGCCount()),
        sourceHolder_(source)
    { }

    bool runtimeMatches(JSRuntime* runtime) const {
        return runtime == runtime_;
    }
    bool shouldStart() const {
        // We wait 2 major GCs to start compressing, in order to avoid
        // immediate compression.
        return runtime_->gc.majorGCCount() > majorGCNumber_ + 1;
    }

    bool shouldCancel() const {
        // If the refcount is exactly 1, then nothing else is holding on to the
        // ScriptSource, so no reason to compress it and we should cancel the task.
        return sourceHolder_.get()->refs == 1;
    }

    void work();
    void complete();
};

} /* namespace js */

#endif /* vm_HelperThreads_h */
