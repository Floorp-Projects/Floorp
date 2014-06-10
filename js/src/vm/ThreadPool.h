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

namespace gc {
struct ForkJoinNurseryChunk;
}

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
    void submitSlices(uint16_t sliceStart, uint16_t sliceEnd);

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

    // Initialized at startup only.
    JSRuntime *const runtime_;
#ifdef DEBUG
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

    static const uint16_t MAX_SLICE_ID = UINT16_MAX;

    explicit ThreadPool(JSRuntime *rt);
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

    // Chunk pool for the PJS parallel nurseries.  The nurseries need
    // to have a useful pool of cheap chunks, they cannot map/unmap
    // chunks as needed, as that slows down collection much too much.
    //
    // Technically the following should be #ifdef JSGC_FJGENERATIONAL
    // but that affects the observed size of JSRuntime, of which
    // ThreadPool is a member.  JSGC_FJGENERATIONAL can only be set if
    // PJS is enabled, but the latter is enabled in js/src/moz.build;
    // meanwhile, JSGC_FJGENERATIONAL must be enabled globally if it
    // is enabled at all, since plenty of Firefox code includes files
    // to make JSRuntime visible.  JSGC_FJGENERATIONAL will go away
    // soon, in the mean time the problem is resolved by not making
    // definitions exported from SpiderMonkey dependent on it.

    // Obtain chunk memory from the cache, or allocate new.  In debug
    // mode poison the memory, see poisionChunk().
    //
    // Returns nullptr on OOM.
    gc::ForkJoinNurseryChunk *getChunk();

    // Free chunk memory to the cache.  In debug mode poison it, see
    // poisionChunk().
    void putFreeChunk(gc::ForkJoinNurseryChunk *mem);

    // If enough time has passed since any allocation activity on the
    // chunk pool then release any free chunks.  It's meaningful to
    // call this from the main GC's chunk expiry mechanism; it has low
    // cost if it does not do anything.
    //
    // This must be called with the GC lock taken.
    void pruneChunkCache();

  private:
    // Ignore requests to prune the pool until this number of seconds
    // has passed since the last allocation request.
    static const int32_t secondsBeforePrune = 10;

    // This lock controls access to the following variables and to the
    // 'next' field of any ChunkFreeList object reachable from freeChunks_.
    //
    // You will be tempted to remove this lock and instead introduce a
    // lock-free push/pop data structure using Atomic.compareExchange.
    // Before you do that, consider that such a data structure
    // implemented naively is vulnerable to the ABA problem in a way
    // that leads to a corrupt free list; the problem occurs in
    // practice during very heavily loaded runs where preeption
    // windows can be long (eg, running the parallel jit_tests on all
    // cores means having a number of runnable threads quadratic in
    // the number of cores).  To do better some ABA-defeating scheme
    // is needed additionally.
    PRLock *chunkLock_;

    // Timestamp of last allocation from the chunk pool, in seconds.
    int32_t timeOfLastAllocation_;

    // This structure overlays the beginning of the chunk when the
    // chunk is on the free list; the rest of the chunk is unused.
    struct ChunkFreeList {
        ChunkFreeList *next;
    };

    // List of free chunks.
    ChunkFreeList *freeChunks_;

    // Poison a free chunk by filling with JS_POISONED_FORKJOIN_CHUNK
    // and setting the runtime pointer to null.
    void poisonChunk(gc::ForkJoinNurseryChunk *c);

    // Release the memory of the chunks that are on the free list.
    //
    // This should be called only from the ThreadPool's destructor or
    // from pruneChunkCache().
    void clearChunkCache();
};

} // namespace js

#endif /* vm_ThreadPool_h */
