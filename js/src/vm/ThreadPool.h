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
#include "jspubtd.h"

#include "js/Vector.h"
#include "vm/Monitor.h"

struct JSRuntime;
struct JSCompartment;

namespace js {

class ThreadPoolBaseWorker;
class ThreadPoolWorker;
class ThreadPoolMainWorker;

// A ParallelJob is the main runnable abstraction in the ThreadPool.
//
// The unit of work here is in terms of threads, *not* slices. The
// user-provided function has the responsibility of getting slices of work via
// the |ForkJoinGetSlice| intrinsic.
class ParallelJob
{
  public:
    virtual bool executeFromWorker(uint32_t workerId, uintptr_t stackLimit) = 0;
    virtual bool executeFromMainThread() = 0;
};

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
    friend class ThreadPoolBaseWorker;
    friend class ThreadPoolWorker;
    friend class ThreadPoolMainWorker;

    // Initialized at startup only.
    JSRuntime *const runtime_;

    // Worker threads and the main thread worker have different
    // logic. Initialized lazily.
    js::Vector<ThreadPoolWorker *, 8, SystemAllocPolicy> workers_;
    ThreadPoolMainWorker *mainWorker_;

    // The number of active workers. Should only access under lock.
    uint32_t activeWorkers_;
    PRCondVar *joinBarrier_;

    // The current job.
    ParallelJob *job_;

#ifdef DEBUG
    // Number of stolen slices in the last parallel job.
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> stolenSlices_;
#endif

    // Number of pending slices in the current job.
    mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> pendingSlices_;

    bool lazyStartWorkers(JSContext *cx);
    void terminateWorkers();
    void terminateWorkersAndReportOOM(JSContext *cx);
    void join(AutoLockMonitor &lock);
    void waitForWorkers(AutoLockMonitor &lock);

  public:
    ThreadPool(JSRuntime *rt);
    ~ThreadPool();

    bool init();

    // Return number of worker threads in the pool, not counting the main thread.
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
    bool isMainThreadActive() const;

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

    // Get the next slice; work stealing happens here if work stealing is
    // on. Returns false if there are no more slices to hand out.
    bool getSliceForWorker(uint32_t workerId, uint16_t *sliceId);
    bool getSliceForMainThread(uint16_t *sliceId);

    // Abort the current job.
    void abortJob();
};

} // namespace js

#endif /* vm_ThreadPool_h */
