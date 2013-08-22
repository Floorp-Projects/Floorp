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

#include "jit/Ion.h"

namespace js {

struct WorkerThread;
struct AsmJSParallelTask;
struct ParseTask;
namespace ion {
  class IonBuilder;
}

#if defined(JS_THREADSAFE) && defined(JS_ION)
# define JS_WORKER_THREADS

/* Per-runtime state for off thread work items. */
class WorkerThreadState
{
  public:
    /* Available threads. */
    WorkerThread *threads;
    size_t numThreads;

    /*
     * Whether all worker threads thread should pause their activity. This acts
     * like the runtime's interrupt field and may be read without locking.
     */
    volatile size_t shouldPause;

    /* After shouldPause is set, the number of threads which are paused. */
    uint32_t numPaused;

    enum CondVar {
        MAIN,
        WORKER
    };

    /* Shared worklist for Ion worker threads. */
    Vector<ion::IonBuilder*, 0, SystemAllocPolicy> ionWorklist;

    /* Worklist for AsmJS worker threads. */
    Vector<AsmJSParallelTask*, 0, SystemAllocPolicy> asmJSWorklist;

    /*
     * Finished list for AsmJS worker threads.
     * Simultaneous AsmJS compilations all service the same AsmJS module.
     * The main thread must pick up finished optimizations and perform codegen.
     */
    Vector<AsmJSParallelTask*, 0, SystemAllocPolicy> asmJSFinishedList;

    /* Shared worklist for parsing/emitting scripts on worker threads. */
    Vector<ParseTask*, 0, SystemAllocPolicy> parseWorklist, parseFinishedList;

    WorkerThreadState() { mozilla::PodZero(this); }
    ~WorkerThreadState();

    bool init(JSRuntime *rt);

    void lock();
    void unlock();

# ifdef DEBUG
    bool isLocked();
# endif

    void wait(CondVar which, uint32_t timeoutMillis = 0);
    void notify(CondVar which);
    void notifyAll(CondVar which);

    bool canStartAsmJSCompile();
    bool canStartIonCompile();
    bool canStartParseTask();

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
        asmJSFailedFunction = NULL;
    }
    void *maybeAsmJSFailedFunction() const {
        return asmJSFailedFunction;
    }

    void finishParseTaskForScript(JSRuntime *rt, JSScript *script);

  private:

    /*
     * Lock protecting all mutable shared state accessed by helper threads, and
     * used by all condition variables.
     */
    PRLock *workerLock;

# ifdef DEBUG
    PRThread *lockOwner;
# endif

    /* Condvar to notify the main thread that work has been completed. */
    PRCondVar *mainWakeup;

    /* Condvar to notify helper threads that they may be able to make progress. */
    PRCondVar *helperWakeup;

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
    ion::IonBuilder *ionBuilder;

    /* Any AsmJS data currently being optimized by Ion on this thread. */
    AsmJSParallelTask *asmData;

    /* Any source being parsed/emitted on this thread */
    ParseTask *parseTask;

    bool idle() const {
        return !ionBuilder && !asmData && !parseTask;
    }

    void pause();
    void destroy();

    void handleAsmJSWorkload(WorkerThreadState &state);
    void handleIonWorkload(WorkerThreadState &state);
    void handleParseWorkload(WorkerThreadState &state);

    static void ThreadMain(void *arg);
    void threadLoop();
};

#endif /* JS_THREADSAFE && JS_ION */

inline bool
OffThreadCompilationEnabled(JSContext *cx)
{
#ifdef JS_WORKER_THREADS
    return ion::js_IonOptions.parallelCompilation
        && cx->runtime()->useHelperThreads()
        && cx->runtime()->helperThreadCount() != 0;
#else
    return false;
#endif
}

/* Methods for interacting with worker threads. */

/* Initialize worker threads unless already initialized. */
bool
EnsureWorkerThreadsInitialized(ExclusiveContext *cx);

/* Perform MIR optimization and LIR generation on a single function. */
bool
StartOffThreadAsmJSCompile(ExclusiveContext *cx, AsmJSParallelTask *asmData);

/*
 * Schedule an Ion compilation for a script, given a builder which has been
 * generated and read everything needed from the VM state.
 */
bool
StartOffThreadIonCompile(JSContext *cx, ion::IonBuilder *builder);

/*
 * Cancel a scheduled or in progress Ion compilation for script. If script is
 * NULL, all compilations for the compartment are cancelled.
 */
void
CancelOffThreadIonCompile(JSCompartment *compartment, JSScript *script);

/*
 * Start a parse/emit cycle for a stream of source. The characters must stay
 * alive until the compilation finishes.
 */
bool
StartOffThreadParseScript(JSContext *cx, const CompileOptions &options,
                          const jschar *chars, size_t length, HandleObject scopeChain,
                          JS::OffThreadCompileCallback callback, void *callbackData);

/* Block until in progress and pending off thread parse jobs have finished. */
void
WaitForOffThreadParsingToFinish(JSRuntime *rt);

class AutoLockWorkerThreadState
{
    WorkerThreadState &state;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    AutoLockWorkerThreadState(WorkerThreadState &state
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : state(state)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
#ifdef JS_WORKER_THREADS
        state.lock();
#else
        (void)state;
#endif
    }

    ~AutoLockWorkerThreadState() {
#ifdef JS_WORKER_THREADS
        state.unlock();
#endif
    }
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
#ifdef JS_WORKER_THREADS
        JS_ASSERT(rt->workerThreadState);
        rt->workerThreadState->unlock();
#else
        (void)this->rt;
#endif
    }

    ~AutoUnlockWorkerThreadState()
    {
#ifdef JS_WORKER_THREADS
        rt->workerThreadState->lock();
#endif
    }
};

/* Pause any threads that are running jobs off thread during GC activity. */
class AutoPauseWorkersForGC
{
#ifdef JS_WORKER_THREADS
    JSRuntime *runtime;
    bool needsUnpause;
    mozilla::DebugOnly<bool> oldExclusiveThreadsPaused;
#endif
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:
    AutoPauseWorkersForGC(JSRuntime *rt MOZ_GUARD_OBJECT_NOTIFIER_PARAM);
    ~AutoPauseWorkersForGC();
};

/* Wait for any in progress off thread parses to halt. */
void
PauseOffThreadParsing();

/* Resume any paused off thread parses. */
void
ResumeOffThreadParsing();

#ifdef JS_ION
struct AsmJSParallelTask
{
    LifoAlloc lifo;         // Provider of all heap memory used for compilation.
    void *func;             // Really, a ModuleCompiler::Func*
    ion::MIRGenerator *mir; // Passed from main thread to worker.
    ion::LIRGraph *lir;     // Passed from worker to main thread.
    unsigned compileTime;

    AsmJSParallelTask(size_t defaultChunkSize)
      : lifo(defaultChunkSize), func(NULL), mir(NULL), lir(NULL), compileTime(0)
    { }

    void init(void *func, ion::MIRGenerator *mir) {
        this->func = func;
        this->mir = mir;
        this->lir = NULL;
    }
};
#endif

struct ParseTask
{
    Zone *zone;
    ExclusiveContext *cx;
    CompileOptions options;
    const jschar *chars;
    size_t length;
    LifoAlloc alloc;

    // Rooted pointer to the scope in the target compartment which the
    // resulting script will be merged into. This is not safe to use off the
    // main thread.
    JSObject *scopeChain;

    // Callback invoked off the main thread when the parse finishes.
    JS::OffThreadCompileCallback callback;
    void *callbackData;

    // Holds the final script between the invocation of the callback and the
    // point where FinishOffThreadScript is called, which will destroy the
    // ParseTask.
    JSScript *script;

    ParseTask(Zone *zone, ExclusiveContext *cx, const CompileOptions &options,
              const jschar *chars, size_t length, JSObject *scopeChain,
              JS::OffThreadCompileCallback callback, void *callbackData);

    ~ParseTask();
};

} /* namespace js */

#endif /* jsworkers_h */
