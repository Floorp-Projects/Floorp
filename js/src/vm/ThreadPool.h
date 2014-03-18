/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ThreadPool_h
#define vm_ThreadPool_h

#include "mozilla/Atomics.h"

#include "jsalloc.h"
#include "jslock.h"
#include "jsmath.h"
#include "jspubtd.h"

#include "js/Vector.h"
#include "vm/Monitor.h"

struct JSRuntime;
struct JSCompartment;

namespace js {

class ThreadPool;

/////////////////////////////////////////////////////////////////////////////
// ThreadPoolWorker
//
// Class for worker threads in the pool. All threads (i.e. helpers and main
// thread) have a worker associted with them. By convention, the worker id of
// the main thread is 0.

class ThreadPoolWorker
{
    const uint32_t workerId_;
    ThreadPool *pool_;

    // Slices this thread is responsible for.
    //
    // This a uint32 composed of two uint16s (the lower and upper bounds) so
    // that we may do a single CAS. See {Compose,Decompose}SliceBounds
    // functions below.
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> sliceBounds_;

    // Current point in the worker's lifecycle.
    volatile enum WorkerState {
        CREATED, ACTIVE, TERMINATED
    } state_;

    // Per-worker scheduler RNG state used for picking a random worker during
    // work stealing.
    uint32_t schedulerRNGState_;

    // The thread's main function.
    static void HelperThreadMain(void *arg);
    void helperLoop();

    bool hasWork() const;
    bool popSliceFront(uint16_t *sliceId);
    bool popSliceBack(uint16_t *sliceId);
    bool stealFrom(ThreadPoolWorker *victim, uint16_t *sliceId);

    // Get a worker at random from the pool using our own thread-local RNG
    // state. This is a weak, but very fast, random function [1]. We choose
    // [a,b,c] = 11,21,13.
    //
    // [1] http://www.jstatsoft.org/v08/i14/paper
  public:
    static const uint32_t XORSHIFT_A = 11;
    static const uint32_t XORSHIFT_B = 21;
    static const uint32_t XORSHIFT_C = 13;

  private:
    ThreadPoolWorker *randomWorker();

  public:
    ThreadPoolWorker(uint32_t workerId, uint32_t rngSeed, ThreadPool *pool);

    uint32_t id() const { return workerId_; }
    bool isMainThread() const { return id() == 0; }

    // Submits a new set of slices. Assumes !hasWork().
    void submitSlices(uint16_t sliceFrom, uint16_t sliceTo);

    // Get the next slice; work stealing happens here if work stealing is
    // on. Returns false if there are no more slices to hand out.
    bool getSlice(ForkJoinContext *cx, uint16_t *sliceId);

    // Discard remaining slices. Used for aborting jobs.
    void discardSlices();

    // Invoked from the main thread; signals worker to start.
    bool start();

    // Invoked from the main thread; signals the worker loop to return.
    void terminate(AutoLockMonitor &lock);

    static size_t offsetOfSliceBounds() {
        return offsetof(ThreadPoolWorker, sliceBounds_);
    }

    static size_t offsetOfSchedulerRNGState() {
        return offsetof(ThreadPoolWorker, schedulerRNGState_);
    }
};

/////////////////////////////////////////////////////////////////////////////
// A ParallelJob is the main runnable abstraction in the ThreadPool.
//
// The unit of work here is in terms of threads, *not* slices. The
// user-provided function has the responsibility of getting slices of work via
// the |ForkJoinGetSlice| intrinsic.

class ParallelJob
{
  public:
    virtual bool executeFromWorker(ThreadPoolWorker *worker, uintptr_t stackLimit) = 0;
    virtual bool executeFromMainThread(ThreadPoolWorker *mainWorker) = 0;
};

/////////////////////////////////////////////////////////////////////////////
// ThreadPool used for parallel JavaScript execution. Unless you are building
// a new kind of parallel service, it is very likely that you do not wish to
// interact with the threadpool directly. In particular, if you wish to
// execute JavaScript in parallel, you probably want to look at |js::ForkJoin|
// in |forkjoin.cpp|.
//
// The ThreadPool always maintains a fixed pool of worker threads.  You can
// query the number of worker threads via the method |numWorkers()|.  Note
// that this number may be zero (generally if threads are disabled, or when
// manually specified for benchmarking purposes).
//
// The way to submit a job is using |executeJob()|---in this case, the job
// will be executed by all worker threads, including the main thread. This
// does not fail if there are no worker threads, it simply runs all the work
// using the main thread only.
//
// Of course, each thread may have any number of previously submitted things
// that they are already working on, and so they will finish those before they
// get to this job.  Therefore it is possible to have some worker threads pick
// up (and even finish) their piece of the job before others have even
// started. The main thread is also used by the pool as a worker thread.
//
// The ThreadPool supports work stealing. Every time a worker completes all
// the slices in its local queue, it tries to acquire some work from other
// workers (including the main thread).  Execution terminates when there is no
// work left to be done, i.e., when all the workers have an empty queue. The
// stealing algorithm operates in 2 phases: (1) workers process all the slices
// in their local queue, and then (2) workers try to steal from other peers.
// Since workers start to steal only *after* they have completed all the
// slices in their queue, the design is particularly convenient in the context
// of Fork/Join-like parallelism, where workers receive a bunch of slices to
// be done at the very beginning of the job, and have to wait until all the
// threads have joined back. During phase (1) there is no synchronization
// overhead between workers introduced by the stealing algorithm, and
// therefore the execution overhead introduced is almost zero with balanced
// workloads. The way a |ParallelJob| is divided into multiple slices has to
// be specified by the instance implementing the job (e.g., |ForkJoinShared|
// in |ForkJoin.cpp|).

class ThreadPool : public Monitor
{
  private:
    friend class ThreadPoolWorker;

    // Initialized lazily.
    js::Vector<ThreadPoolWorker *, 8, SystemAllocPolicy> workers_;

    // The number of active workers. Should only access under lock.
    uint32_t activeWorkers_;
    PRCondVar *joinBarrier_;

    // The current job.
    ParallelJob *job_;

#ifdef DEBUG
    // Initialized at startup only.
    JSRuntime *const runtime_;

    // Number of stolen slices in the last parallel job.
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> stolenSlices_;
#endif

    // Number of pending slices in the current job.
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> pendingSlices_;

    // Whether the main thread is currently processing slices.
    bool isMainThreadActive_;

    bool lazyStartWorkers(JSContext *cx);
    void terminateWorkers();
    void terminateWorkersAndReportOOM(JSContext *cx);
    void join(AutoLockMonitor &lock);
    void waitForWorkers(AutoLockMonitor &lock);
    ThreadPoolWorker *mainThreadWorker() { return workers_[0]; }

  public:
#ifdef DEBUG
    static size_t offsetOfStolenSlices() {
        return offsetof(ThreadPool, stolenSlices_);
    }
#endif
    static size_t offsetOfPendingSlices() {
        return offsetof(ThreadPool, pendingSlices_);
    }
    static size_t offsetOfWorkers() {
        return offsetof(ThreadPool, workers_);
    }

    ThreadPool(JSRuntime *rt);
    ~ThreadPool();

    bool init();

    // Return number of worker threads in the pool, counting the main thread.
    uint32_t numWorkers() const;

    // Returns whether we have any pending slices.
    bool hasWork() const { return pendingSlices_ != 0; }

    // Returns the current job. Must have one.
    ParallelJob *job() const {
        MOZ_ASSERT(job_);
        return job_;
    }

    // Returns whether or not the scheduler should perform work stealing.
    bool workStealing() const;

    // Returns whether or not the main thread is working.
    bool isMainThreadActive() const { return isMainThreadActive_; }

#ifdef DEBUG
    // Return the number of stolen slices in the last parallel job.
    uint16_t stolenSlices() { return stolenSlices_; }
#endif

    // Wait until all worker threads have finished their current set
    // of slices and then return.  You must not submit new jobs after
    // invoking |terminate()|.
    void terminate();

    // Execute the given ParallelJob using the main thread and any available worker.
    // Blocks until the main thread has completed execution.
    ParallelResult executeJob(JSContext *cx, ParallelJob *job, uint16_t sliceStart,
                              uint16_t numSlices);

    // Abort the current job.
    void abortJob();
};

} // namespace js

#endif /* vm_ThreadPool_h */
