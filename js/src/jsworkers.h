/* -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*- */
/* vim: set ts=4 sw=4 et tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Definitions for managing off-main-thread work using a shared, per runtime
 * worklist. Worklist items are engine internal, and are distinct from e.g.
 * web workers.
 */

#ifndef jsworkers_h___
#define jsworkers_h___

#include "mozilla/GuardObjects.h"

#include "jscntxt.h"
#include "jslock.h"

#include "ion/Ion.h"

namespace js {

namespace ion {
  class IonBuilder;
}

#if defined(JS_THREADSAFE) && defined(JS_ION)
# define JS_PARALLEL_COMPILATION

struct WorkerThread;
struct AsmJSParallelTask;

/* Per-runtime state for off thread work items. */
class WorkerThreadState
{
  public:
    /* Available threads. */
    WorkerThread *threads;
    size_t numThreads;

    enum CondVar {
        MAIN,
        WORKER
    };

    /* Shared worklist for Ion worker threads. */
    js::Vector<ion::IonBuilder*, 0, SystemAllocPolicy> ionWorklist;

    /* Worklist for AsmJS worker threads. */
    js::Vector<AsmJSParallelTask*, 0, SystemAllocPolicy> asmJSWorklist;

    /*
     * Finished list for AsmJS worker threads.
     * Simultaneous AsmJS compilations all service the same AsmJS module.
     * The main thread must pick up finished optimizations and perform codegen.
     */
    js::Vector<AsmJSParallelTask*, 0, SystemAllocPolicy> asmJSFinishedList;

    WorkerThreadState() { PodZero(this); }
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

    uint32_t harvestFailedAsmJSJobs() {
        JS_ASSERT(isLocked());
        uint32_t n = numAsmJSFailedJobs;
        numAsmJSFailedJobs = 0;
        return n;
    }
    void noteAsmJSFailure(int32_t func) {
        // Be mindful to signal the main thread after calling this function.
        JS_ASSERT(isLocked());
        if (asmJSFailedFunctionIndex < 0)
            asmJSFailedFunctionIndex = func;
        numAsmJSFailedJobs++;
    }
    bool asmJSWorkerFailed() const {
        return bool(numAsmJSFailedJobs);
    }
    void resetAsmJSFailureState() {
        numAsmJSFailedJobs = 0;
        asmJSFailedFunctionIndex = -1;
    }
    int32_t maybeGetAsmJSFailedFunctionIndex() const {
        return asmJSFailedFunctionIndex;
    }

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
    int32_t asmJSFailedFunctionIndex;
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

    void destroy();

    void handleAsmJSWorkload(WorkerThreadState &state);
    void handleIonWorkload(WorkerThreadState &state);

    static void ThreadMain(void *arg);
    void threadLoop();
};

#endif /* JS_THREADSAFE && JS_ION */

inline bool
OffThreadCompilationEnabled(JSContext *cx)
{
#ifdef JS_PARALLEL_COMPILATION
    return ion::js_IonOptions.parallelCompilation
        && cx->runtime->useHelperThreads()
        && cx->runtime->helperThreadCount() != 0;
#else
    return false;
#endif
}

/* Methods for interacting with worker threads. */

/* Initialize worker threads unless already initialized. */
bool
EnsureParallelCompilationInitialized(JSRuntime *rt);

/* Perform MIR optimization and LIR generation on a single function. */
bool
StartOffThreadAsmJSCompile(JSContext *cx, AsmJSParallelTask *asmData);

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

class AutoLockWorkerThreadState
{
    JSRuntime *rt;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER

  public:

    AutoLockWorkerThreadState(JSRuntime *rt
                              MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : rt(rt)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
#ifdef JS_PARALLEL_COMPILATION
        JS_ASSERT(rt->workerThreadState);
        rt->workerThreadState->lock();
#else
        (void)this->rt;
#endif
    }

    ~AutoLockWorkerThreadState()
    {
#ifdef JS_PARALLEL_COMPILATION
        rt->workerThreadState->unlock();
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
#ifdef JS_PARALLEL_COMPILATION
        JS_ASSERT(rt->workerThreadState);
        rt->workerThreadState->unlock();
#else
        (void)this->rt;
#endif
    }

    ~AutoUnlockWorkerThreadState()
    {
#ifdef JS_PARALLEL_COMPILATION
        rt->workerThreadState->lock();
#endif
    }
};

} /* namespace js */

#endif // jsworkers_h___
