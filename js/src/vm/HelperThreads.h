/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Definitions for managing off-main-thread work using a process wide list
 * of worklist items and pool of threads. Worklist items are engine internal,
 * and are distinct from e.g. web workers.
 */

#ifndef vm_HelperThreads_h
#define vm_HelperThreads_h

#include "mozilla/GuardObjects.h"
#include "mozilla/PodOperations.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Variant.h"

#include "jscntxt.h"

#include "frontend/TokenStream.h"
#include "jit/Ion.h"
#include "threading/ConditionVariable.h"
#include "threading/Mutex.h"

namespace js {

class AutoLockHelperThreadState;
class AutoUnlockHelperThreadState;
class PromiseTask;
struct HelperThread;
struct ParseTask;
namespace jit {
  class IonBuilder;
} // namespace jit
namespace wasm {
  class FuncIR;
  class FunctionCompileResults;
  class IonCompileTask;
  typedef Vector<IonCompileTask*, 0, SystemAllocPolicy> IonCompileTaskPtrVector;
} // namespace wasm

enum class ParseTaskKind
{
    Script,
    Module
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
    typedef Vector<SourceCompressionTask*, 0, SystemAllocPolicy> SourceCompressionTaskVector;
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
    wasm::IonCompileTaskPtrVector wasmWorklist_, wasmFinishedList_;

  public:
    // For now, only allow a single parallel asm.js compilation to happen at a
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

    // Source compression worklist.
    SourceCompressionTaskVector compressionWorklist_;

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
        vector[(*index)--] = vector.back();
        vector.popBack();
    }

    IonBuilderVector& ionWorklist(const AutoLockHelperThreadState&) {
        return ionWorklist_;
    }
    IonBuilderVector& ionFinishedList(const AutoLockHelperThreadState&) {
        return ionFinishedList_;
    }

    wasm::IonCompileTaskPtrVector& wasmWorklist(const AutoLockHelperThreadState&) {
        return wasmWorklist_;
    }
    wasm::IonCompileTaskPtrVector& wasmFinishedList(const AutoLockHelperThreadState&) {
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

    SourceCompressionTaskVector& compressionWorklist(const AutoLockHelperThreadState&) {
        return compressionWorklist_;
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
    void noteWasmFailure(const AutoLockHelperThreadState&) {
        // Be mindful to signal the main thread after calling this function.
        numWasmFailedJobs++;
    }
    bool wasmFailed(const AutoLockHelperThreadState&) {
        return bool(numWasmFailedJobs);
    }

    JSScript* finishParseTask(JSContext* cx, ParseTaskKind kind, void* token);
    void cancelParseTask(JSContext* cx, ParseTaskKind kind, void* token);

    void mergeParseTaskCompartment(JSContext* cx, ParseTask* parseTask,
                                   Handle<GlobalObject*> global,
                                   JSCompartment* dest);

  private:
    /*
     * Number of wasm jobs that encountered failure for the active module.
     * Their parent is logically the main thread, and this number serves for harvesting.
     */
    uint32_t numWasmFailedJobs;

  public:
    JSScript* finishScriptParseTask(JSContext* cx, void* token);
    JSObject* finishModuleParseTask(JSContext* cx, void* token);
    bool compressionInProgress(SourceCompressionTask* task, const AutoLockHelperThreadState& lock);
    SourceCompressionTask* compressionTaskForSource(ScriptSource* ss, const AutoLockHelperThreadState& lock);

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
    mozilla::Maybe<PerThreadData> threadData;
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
                                    wasm::IonCompileTask*,
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
    wasm::IonCompileTask* wasmTask() {
        return maybeCurrentTaskAs<wasm::IonCompileTask*>();
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
    void handleParseWorkload(AutoLockHelperThreadState& locked, uintptr_t stackLimit);
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

// Pause the current thread until it's pause flag is unset.
void
PauseCurrentHelperThread();

/* Perform MIR optimization and LIR generation on a single function. */
bool
StartOffThreadWasmCompile(wasm::IonCompileTask* task);

/*
 * Start executing the given PromiseTask on a helper thread, finishing back on
 * the originating JSContext's owner thread.
 */
bool
StartPromiseTask(JSContext* cx, UniquePtr<PromiseTask> task);

/*
 * Schedule an Ion compilation for a script, given a builder which has been
 * generated and read everything needed from the VM state.
 */
bool
StartOffThreadIonCompile(JSContext* cx, jit::IonBuilder* builder);

/*
 * Cancel a scheduled or in progress Ion compilation for script. If script is
 * nullptr, all compilations for the compartment are cancelled.
 */
void
CancelOffThreadIonCompile(JSCompartment* compartment, JSScript* script,
                          bool discardLazyLinkList = true);

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

/* Start a compression job for the specified token. */
bool
StartOffThreadCompression(ExclusiveContext* cx, SourceCompressionTask* task);

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
    ExclusiveContext* cx;
    OwningCompileOptions options;
    const char16_t* chars;
    size_t length;
    LifoAlloc alloc;

    // Rooted pointer to the global object used by 'cx'.
    PersistentRootedObject exclusiveContextGlobal;

    // Callback invoked off the main thread when the parse finishes.
    JS::OffThreadCompileCallback callback;
    void* callbackData;

    // Holds the final script between the invocation of the callback and the
    // point where FinishOffThreadScript is called, which will destroy the
    // ParseTask.
    PersistentRootedScript script;

    // Holds the ScriptSourceObject generated for the script compilation.
    PersistentRooted<ScriptSourceObject*> sourceObject;

    // Any errors or warnings produced during compilation. These are reported
    // when finishing the script.
    Vector<frontend::CompileError*> errors;
    bool overRecursed;
    bool outOfMemory;

    ParseTask(ParseTaskKind kind, ExclusiveContext* cx, JSObject* exclusiveContextGlobal,
              JSContext* initCx, const char16_t* chars, size_t length,
              JS::OffThreadCompileCallback callback, void* callbackData);
    bool init(JSContext* cx, const ReadOnlyCompileOptions& options);

    void activate(JSRuntime* rt);
    virtual void parse() = 0;
    bool finish(JSContext* cx);

    bool runtimeMatches(JSRuntime* rt) {
        return exclusiveContextGlobal->runtimeFromAnyThread() == rt;
    }

    virtual ~ParseTask();
};

struct ScriptParseTask : public ParseTask
{
    ScriptParseTask(ExclusiveContext* cx, JSObject* exclusiveContextGlobal,
                    JSContext* initCx, const char16_t* chars, size_t length,
                    JS::OffThreadCompileCallback callback, void* callbackData);
    void parse() override;
};

struct ModuleParseTask : public ParseTask
{
    ModuleParseTask(ExclusiveContext* cx, JSObject* exclusiveContextGlobal,
                    JSContext* initCx, const char16_t* chars, size_t length,
                    JS::OffThreadCompileCallback callback, void* callbackData);
    void parse() override;
};

// Return whether, if a new parse task was started, it would need to wait for
// an in-progress GC to complete before starting.
extern bool
OffThreadParsingMustWaitForGC(JSRuntime* rt);

// Compression tasks are allocated on the stack by their triggering thread,
// which will block on the compression completing as the task goes out of scope
// to ensure it completes at the required time.
struct SourceCompressionTask
{
    friend class ScriptSource;
    friend struct HelperThread;

    // Thread performing the compression.
    HelperThread* helperThread;

  private:
    // Context from the triggering thread. Don't use this off thread!
    ExclusiveContext* cx;

    ScriptSource* ss;

    // Atomic flag to indicate to a helper thread that it should abort
    // compression on the source.
    mozilla::Atomic<bool, mozilla::Relaxed> abort_;

    // Stores the result of the compression.
    enum ResultType {
        OOM,
        Aborted,
        Success
    } result;

    mozilla::Maybe<SharedImmutableString> resultString;

  public:
    explicit SourceCompressionTask(ExclusiveContext* cx)
      : helperThread(nullptr)
      , cx(cx)
      , ss(nullptr)
      , abort_(false)
      , result(OOM)
    {}

    ~SourceCompressionTask()
    {
        complete();
    }

    ResultType work();
    bool complete();
    void abort() { abort_ = true; }
    bool active() const { return !!ss; }
    ScriptSource* source() { return ss; }
};

} /* namespace js */

#endif /* vm_HelperThreads_h */
