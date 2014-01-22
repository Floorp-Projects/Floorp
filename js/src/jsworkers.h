/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Definitions for managing off-main-thread work using a shared, per runtime
 * worklist. Worklist items are engine internal, and are distinct from e.g.
 * web workers.
 */

#ifndef jsworkers_h
#define jsworkers_h

#include "mozilla/GuardObjects.h"
#include "mozilla/PodOperations.h"

#include "jscntxt.h"
#include "jslock.h"

#include "frontend/TokenStream.h"
#include "jit/Ion.h"

namespace js {

struct WorkerThread;
struct AsmJSParallelTask;
struct ParseTask;
namespace jit {
  class IonBuilder;
}

#ifdef JS_THREADSAFE

/* Per-runtime state for off thread work items. */
class WorkerThreadState
{
  public:
    /* Available threads. */
    WorkerThread *threads;
    size_t numThreads;

    enum CondVar {
        /* For notifying threads waiting for work that they may be able to make progress. */
        CONSUMER,

        /* For notifying threads doing work that they may be able to make progress. */
        PRODUCER
    };

    /* Shared worklist for Ion worker threads. */
    Vector<jit::IonBuilder*, 0, SystemAllocPolicy> ionWorklist;

    /* Worklist for AsmJS worker threads. */
    Vector<AsmJSParallelTask*, 0, SystemAllocPolicy> asmJSWorklist;

    /*
     * Finished list for AsmJS worker threads.
     * Simultaneous AsmJS compilations all service the same AsmJS module.
     * The main thread must pick up finished optimizations and perform codegen.
     */
    Vector<AsmJSParallelTask*, 0, SystemAllocPolicy> asmJSFinishedList;

    /*
     * For now, only allow a single parallel asm.js compilation to happen at a
     * time. This avoids race conditions on asmJSWorklist/asmJSFinishedList/etc.
     */
    mozilla::Atomic<uint32_t> asmJSCompilationInProgress;

    /* Shared worklist for parsing/emitting scripts on worker threads. */
    Vector<ParseTask*, 0, SystemAllocPolicy> parseWorklist, parseFinishedList;

    /* Main-thread-only list of parse tasks waiting for an atoms-zone GC to complete. */
    Vector<ParseTask*, 0, SystemAllocPolicy> parseWaitingOnGC;

    /* Worklist for source compression worker threads. */
    Vector<SourceCompressionTask *, 0, SystemAllocPolicy> compressionWorklist;

    WorkerThreadState(JSRuntime *rt) {
        mozilla::PodZero(this);
        runtime = rt;
    }
    ~WorkerThreadState();

    bool init();
    void cleanup();

    void lock();
    void unlock();

# ifdef DEBUG
    bool isLocked();
# endif

    void wait(CondVar which, uint32_t timeoutMillis = 0);
    void notifyAll(CondVar which);
    void notifyOne(CondVar which);

    bool canStartAsmJSCompile();
    bool canStartIonCompile();
    bool canStartParseTask();
    bool canStartCompressionTask();

    uint32_t harvestFailedAsmJSJobs() {
        JS_ASSERT(isLocked());
        uint32_t n = numAsmJSFailedJobs;
        numAsmJSFailedJobs = 0;
        return n;
    }
    void noteAsmJSFailure(void *func) {
        // Be mindful to signal the main thread after calling this function.
        JS_ASSERT(isLocked());
        if (!asmJSFailedFunction)
            asmJSFailedFunction = func;
        numAsmJSFailedJobs++;
    }
    bool asmJSWorkerFailed() const {
        return bool(numAsmJSFailedJobs);
    }
    void resetAsmJSFailureState() {
        numAsmJSFailedJobs = 0;
        asmJSFailedFunction = nullptr;
    }
    void *maybeAsmJSFailedFunction() const {
        return asmJSFailedFunction;
    }

    JSScript *finishParseTask(JSContext *maybecx, JSRuntime *rt, void *token);
    bool compressionInProgress(SourceCompressionTask *task);
    SourceCompressionTask *compressionTaskForSource(ScriptSource *ss);

  private:

    JSRuntime *runtime;

    /*
     * Lock protecting all mutable shared state accessed by helper threads, and
     * used by all condition variables.
     */
    PRLock *workerLock;

# ifdef DEBUG
    PRThread *lockOwner;
# endif

    /* Condvars for threads waiting/notifying each other. */
    PRCondVar *consumerWakeup;
    PRCondVar *producerWakeup;

    /*
     * Number of AsmJS workers that encountered failure for the active module.
     * Their parent is logically the main thread, and this number serves for harvesting.
     */
    uint32_t numAsmJSFailedJobs;

    /*
     * Function index |i| in |Module.function(i)| of first failed AsmJS function.
     * -1 if no function has failed.
     */
    void *asmJSFailedFunction;
};

/* Individual helper thread, one allocated per core. */
struct WorkerThread
{
    JSRuntime *runtime;

    mozilla::Maybe<PerThreadData> threadData;
    PRThread *thread;

    /* Indicate to an idle thread that it should finish executing. */
    bool terminate;

    /* Any builder currently being compiled by Ion on this thread. */
    jit::IonBuilder *ionBuilder;

    /* Any AsmJS data currently being optimized by Ion on this thread. */
    AsmJSParallelTask *asmData;

    /* Any source being parsed/emitted on this thread. */
    ParseTask *parseTask;

    /* Any source being compressed on this thread. */
    SourceCompressionTask *compressionTask;

    bool idle() const {
        return !ionBuilder && !asmData && !parseTask && !compressionTask;
    }

    void destroy();

    void handleAsmJSWorkload(WorkerThreadState &state);
    void handleIonWorkload(WorkerThreadState &state);
    void handleParseWorkload(WorkerThreadState &state);
    void handleCompressionWorkload(WorkerThreadState &state);

    static void ThreadMain(void *arg);
    void threadLoop();
};

#endif /* JS_THREADSAFE */

/* Methods for interacting with worker threads. */

/* Initialize worker threads unless already initialized. */
bool
EnsureWorkerThreadsInitialized(ExclusiveContext *cx);

#ifdef JS_ION

/* Perform MIR optimization and LIR generation on a single function. */
bool
StartOffThreadAsmJSCompile(ExclusiveContext *cx, AsmJSParallelTask *asmData);

/*
 * Schedule an Ion compilation for a script, given a builder which has been
 * generated and read everything needed from the VM state.
 */
bool
StartOffThreadIonCompile(JSContext *cx, jit::IonBuilder *builder);

#endif // JS_ION

/*
 * Cancel a scheduled or in progress Ion compilation for script. If script is
 * nullptr, all compilations for the compartment are cancelled.
 */
void
CancelOffThreadIonCompile(JSCompartment *compartment, JSScript *script);

/*
 * Start a parse/emit cycle for a stream of source. The characters must stay
 * alive until the compilation finishes.
 */
bool
StartOffThreadParseScript(JSContext *cx, const ReadOnlyCompileOptions &options,
                          const jschar *chars, size_t length, HandleObject scopeChain,
                          JS::OffThreadCompileCallback callback, void *callbackData);

/*
 * Called at the end of GC to enqueue any Parse tasks that were waiting on an
 * atoms-zone GC to finish.
 */
void
EnqueuePendingParseTasksAfterGC(JSRuntime *rt);

/* Block until in progress and pending off thread parse jobs have finished. */
void
WaitForOffThreadParsingToFinish(JSRuntime *rt);

/* Start a compression job for the specified token. */
bool
StartOffThreadCompression(ExclusiveContext *cx, SourceCompressionTask *task);

class AutoLockWorkerThreadState
{
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

#ifdef JS_THREADSAFE
    WorkerThreadState &state;

  public:
    AutoLockWorkerThreadState(WorkerThreadState &state
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : state(state)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        state.lock();
    }

    ~AutoLockWorkerThreadState() {
        state.unlock();
    }
#else
  public:
    AutoLockWorkerThreadState(WorkerThreadState &state
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }
#endif
};

class AutoUnlockWorkerThreadState
{
    JSRuntime *rt;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:

    AutoUnlockWorkerThreadState(JSRuntime *rt
                                MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : rt(rt)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
#ifdef JS_THREADSAFE
        JS_ASSERT(rt->workerThreadState);
        rt->workerThreadState->unlock();
#else
        (void)this->rt;
#endif
    }

    ~AutoUnlockWorkerThreadState()
    {
#ifdef JS_THREADSAFE
        rt->workerThreadState->lock();
#endif
    }
};

#ifdef JS_ION
struct AsmJSParallelTask
{
    LifoAlloc lifo;         // Provider of all heap memory used for compilation.
    void *func;             // Really, a ModuleCompiler::Func*
    jit::MIRGenerator *mir; // Passed from main thread to worker.
    jit::LIRGraph *lir;     // Passed from worker to main thread.
    unsigned compileTime;

    AsmJSParallelTask(size_t defaultChunkSize)
      : lifo(defaultChunkSize), func(nullptr), mir(nullptr), lir(nullptr), compileTime(0)
    { }

    void init(void *func, jit::MIRGenerator *mir) {
        this->func = func;
        this->mir = mir;
        this->lir = nullptr;
    }
};
#endif

struct ParseTask
{
    ExclusiveContext *cx;
    OwningCompileOptions options;
    const jschar *chars;
    size_t length;
    LifoAlloc alloc;

    // Rooted pointer to the scope in the target compartment which the
    // resulting script will be merged into. This is not safe to use off the
    // main thread.
    JSObject *scopeChain;

    // Rooted pointer to the global object used by 'cx'.
    JSObject *exclusiveContextGlobal;

    // Callback invoked off the main thread when the parse finishes.
    JS::OffThreadCompileCallback callback;
    void *callbackData;

    // Holds the final script between the invocation of the callback and the
    // point where FinishOffThreadScript is called, which will destroy the
    // ParseTask.
    JSScript *script;

    // Any errors or warnings produced during compilation. These are reported
    // when finishing the script.
    Vector<frontend::CompileError *> errors;
    bool overRecursed;

    ParseTask(ExclusiveContext *cx, JSObject *exclusiveContextGlobal, JSContext *initCx,
              const jschar *chars, size_t length, JSObject *scopeChain,
              JS::OffThreadCompileCallback callback, void *callbackData);
    bool init(JSContext *cx, const ReadOnlyCompileOptions &options);

    void activate(JSRuntime *rt);

    ~ParseTask();
};

// Return whether, if a new parse task was started, it would need to wait for
// an in-progress GC to complete before starting.
extern bool
OffThreadParsingMustWaitForGC(JSRuntime *rt);

// Compression tasks are allocated on the stack by their triggering thread,
// which will block on the compression completing as the task goes out of scope
// to ensure it completes at the required time.
struct SourceCompressionTask
{
    friend class ScriptSource;

#ifdef JS_THREADSAFE
    // Thread performing the compression.
    WorkerThread *workerThread;
#endif

  private:
    // Context from the triggering thread. Don't use this off thread!
    ExclusiveContext *cx;

    ScriptSource *ss;
    const jschar *chars;
    bool oom;

    // Atomic flag to indicate to a worker thread that it should abort
    // compression on the source.
#ifdef JS_THREADSAFE
    mozilla::Atomic<int32_t, mozilla::Relaxed> abort_;
#else
    int32_t abort_;
#endif

  public:
    explicit SourceCompressionTask(ExclusiveContext *cx)
      : cx(cx), ss(nullptr), chars(nullptr), oom(false), abort_(0)
    {
#ifdef JS_THREADSAFE
        workerThread = nullptr;
#endif
    }

    ~SourceCompressionTask()
    {
        complete();
    }

    bool work();
    bool complete();
    void abort() { abort_ = 1; }
    bool active() const { return !!ss; }
    ScriptSource *source() { return ss; }
    const jschar *uncompressedChars() { return chars; }
    void setOOM() { oom = true; }
};

} /* namespace js */

#endif /* jsworkers_h */
