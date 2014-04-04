/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ThreadPool.h"

#include "mozilla/Atomics.h"

#include "jslock.h"

#include "vm/ForkJoin.h"
#include "vm/Monitor.h"
#include "vm/Runtime.h"

using namespace js;

const size_t WORKER_THREAD_STACK_SIZE = 1*1024*1024;

static inline uint32_t
ComposeSliceBounds(uint16_t from, uint16_t to)
{
    MOZ_ASSERT(from <= to);
    return (uint32_t(from) << 16) | to;
}

static inline void
DecomposeSliceBounds(uint32_t bounds, uint16_t *from, uint16_t *to)
{
    *from = bounds >> 16;
    *to = bounds & uint16_t(~0);
    MOZ_ASSERT(*from <= *to);
}

ThreadPoolWorker::ThreadPoolWorker(uint32_t workerId, uint32_t rngSeed, ThreadPool *pool)
  : workerId_(workerId),
    pool_(pool),
    sliceBounds_(0),
    state_(CREATED),
    schedulerRNGState_(rngSeed)
{ }

bool
ThreadPoolWorker::hasWork() const
{
    uint16_t from, to;
    DecomposeSliceBounds(sliceBounds_, &from, &to);
    return from != to;
}

bool
ThreadPoolWorker::popSliceFront(uint16_t *sliceId)
{
    uint32_t bounds;
    uint16_t from, to;
    do {
        bounds = sliceBounds_;
        DecomposeSliceBounds(bounds, &from, &to);
        if (from == to)
            return false;
    } while (!sliceBounds_.compareExchange(bounds, ComposeSliceBounds(from + 1, to)));

    *sliceId = from;
    pool_->pendingSlices_--;
    return true;
}

bool
ThreadPoolWorker::popSliceBack(uint16_t *sliceId)
{
    uint32_t bounds;
    uint16_t from, to;
    do {
        bounds = sliceBounds_;
        DecomposeSliceBounds(bounds, &from, &to);
        if (from == to)
            return false;
    } while (!sliceBounds_.compareExchange(bounds, ComposeSliceBounds(from, to - 1)));

    *sliceId = to - 1;
    pool_->pendingSlices_--;
    return true;
}

void
ThreadPoolWorker::discardSlices()
{
    uint32_t bounds;
    uint16_t from, to;
    do {
        bounds = sliceBounds_;
        DecomposeSliceBounds(bounds, &from, &to);
    } while (!sliceBounds_.compareExchange(bounds, 0));

    pool_->pendingSlices_ -= to - from;
}

bool
ThreadPoolWorker::stealFrom(ThreadPoolWorker *victim, uint16_t *sliceId)
{
    // Instead of popping the slice from the front by incrementing sliceStart_,
    // decrement sliceEnd_. Usually this gives us better locality.
    if (!victim->popSliceBack(sliceId))
        return false;
#ifdef DEBUG
    pool_->stolenSlices_++;
#endif
    return true;
}

ThreadPoolWorker *
ThreadPoolWorker::randomWorker()
{
    // Perform 32-bit xorshift.
    uint32_t x = schedulerRNGState_;
    x ^= x << XORSHIFT_A;
    x ^= x >> XORSHIFT_B;
    x ^= x << XORSHIFT_C;
    schedulerRNGState_ = x;
    return pool_->workers_[x % pool_->numWorkers()];
}

bool
ThreadPoolWorker::start()
{
#ifndef JS_THREADSAFE
    return false;
#else
    if (isMainThread())
        return true;

    MOZ_ASSERT(state_ == CREATED);

    // Set state to active now, *before* the thread starts:
    state_ = ACTIVE;

    if (!PR_CreateThread(PR_USER_THREAD,
                         HelperThreadMain, this,
                         PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                         PR_UNJOINABLE_THREAD,
                         WORKER_THREAD_STACK_SIZE))
    {
        // If the thread failed to start, call it TERMINATED.
        state_ = TERMINATED;
        return false;
    }

    return true;
#endif
}

void
ThreadPoolWorker::HelperThreadMain(void *arg)
{
    ThreadPoolWorker *worker = (ThreadPoolWorker*) arg;
    worker->helperLoop();
}

void
ThreadPoolWorker::helperLoop()
{
    MOZ_ASSERT(!isMainThread());

    // This is hokey in the extreme.  To compute the stack limit,
    // subtract the size of the stack from the address of a local
    // variable and give a 100k buffer.  Is there a better way?
    // (Note: 2k proved to be fine on Mac, but too little on Linux)
    uintptr_t stackLimitOffset = WORKER_THREAD_STACK_SIZE - 100*1024;
    uintptr_t stackLimit = (((uintptr_t)&stackLimitOffset) +
                             stackLimitOffset * JS_STACK_GROWTH_DIRECTION);


    for (;;) {
        // Wait for work to arrive or for us to terminate.
        {
            AutoLockMonitor lock(*pool_);
            while (state_ == ACTIVE && !pool_->hasWork())
                lock.wait();

            if (state_ == TERMINATED) {
                pool_->join(lock);
                return;
            }

            pool_->activeWorkers_++;
        }

        if (!pool_->job()->executeFromWorker(this, stackLimit))
            pool_->abortJob();

        // Join the pool.
        {
            AutoLockMonitor lock(*pool_);
            pool_->join(lock);
        }
    }
}

void
ThreadPoolWorker::submitSlices(uint16_t sliceStart, uint16_t sliceEnd)
{
    MOZ_ASSERT(!hasWork());
    sliceBounds_ = ComposeSliceBounds(sliceStart, sliceEnd);
}

bool
ThreadPoolWorker::getSlice(ForkJoinContext *cx, uint16_t *sliceId)
{
    // First see whether we have any work ourself.
    if (popSliceFront(sliceId))
        return true;

    // Try to steal work.
    if (!pool_->workStealing())
        return false;

    do {
        if (!pool_->hasWork())
            return false;
    } while (!stealFrom(randomWorker(), sliceId));

    return true;
}

void
ThreadPoolWorker::terminate(AutoLockMonitor &lock)
{
    MOZ_ASSERT(lock.isFor(*pool_));
    MOZ_ASSERT(state_ != TERMINATED);
    state_ = TERMINATED;
}

/////////////////////////////////////////////////////////////////////////////
// ThreadPool
//
// The |ThreadPool| starts up workers, submits work to them, and shuts
// them down when requested.

ThreadPool::ThreadPool(JSRuntime *rt)
  : activeWorkers_(0),
    joinBarrier_(nullptr),
    job_(nullptr),
#ifdef DEBUG
    runtime_(rt),
    stolenSlices_(0),
#endif
    pendingSlices_(0),
    isMainThreadActive_(false)
{ }

ThreadPool::~ThreadPool()
{
    terminateWorkers();
#ifdef JS_THREADSAFE
    if (joinBarrier_)
        PR_DestroyCondVar(joinBarrier_);
#endif
}

bool
ThreadPool::init()
{
#ifdef JS_THREADSAFE
    if (!Monitor::init())
        return false;
    joinBarrier_ = PR_NewCondVar(lock_);
    return !!joinBarrier_;
#else
    return true;
#endif
}

uint32_t
ThreadPool::numWorkers() const
{
#ifdef JS_THREADSAFE
    return WorkerThreadState().cpuCount;
#else
    return 1;
#endif
}

bool
ThreadPool::workStealing() const
{
#ifdef DEBUG
    if (char *stealEnv = getenv("JS_THREADPOOL_STEAL"))
        return !!strtol(stealEnv, nullptr, 10);
#endif

    return true;
}

extern uint64_t random_next(uint64_t *, int);

bool
ThreadPool::lazyStartWorkers(JSContext *cx)
{
    // Starts the workers if they have not already been started.  If
    // something goes wrong, reports an error and ensures that all
    // partially started threads are terminated.  Therefore, upon exit
    // from this function, the workers array is either full (upon
    // success) or empty (upon failure).

#ifdef JS_THREADSAFE
    if (!workers_.empty()) {
        MOZ_ASSERT(workers_.length() == numWorkers());
        return true;
    }

    // Allocate workers array and then start the worker threads.
    // Note that numWorkers() is the number of *desired* workers,
    // but workers_.length() is the number of *successfully
    // initialized* workers.
    uint64_t rngState = 0;
    for (uint32_t workerId = 0; workerId < numWorkers(); workerId++) {
        uint32_t rngSeed = uint32_t(random_next(&rngState, 32));
        ThreadPoolWorker *worker = cx->new_<ThreadPoolWorker>(workerId, rngSeed, this);
        if (!worker || !workers_.append(worker)) {
            terminateWorkersAndReportOOM(cx);
            return false;
        }
    }

    for (uint32_t workerId = 0; workerId < numWorkers(); workerId++) {
        if (!workers_[workerId]->start()) {
            // Note: do not delete worker here because it has been
            // added to the array and hence will be deleted by
            // |terminateWorkersAndReportOOM()|.
            terminateWorkersAndReportOOM(cx);
            return false;
        }
    }
#endif

    return true;
}

void
ThreadPool::terminateWorkersAndReportOOM(JSContext *cx)
{
    terminateWorkers();
    MOZ_ASSERT(workers_.empty());
    js_ReportOutOfMemory(cx);
}

void
ThreadPool::terminateWorkers()
{
    if (workers_.length() > 0) {
        AutoLockMonitor lock(*this);

        // Signal to the workers they should quit.
        for (uint32_t i = 0; i < workers_.length(); i++)
            workers_[i]->terminate(lock);

        // Wake up all the workers. Set the number of active workers to the
        // current number of workers so we can make sure they all join.
        activeWorkers_ = workers_.length() - 1;
        lock.notifyAll();

        // Wait for all workers to join.
        waitForWorkers(lock);

        while (workers_.length() > 0)
            js_delete(workers_.popCopy());
    }
}

void
ThreadPool::terminate()
{
    terminateWorkers();
}

void
ThreadPool::join(AutoLockMonitor &lock)
{
    MOZ_ASSERT(lock.isFor(*this));
    if (--activeWorkers_ == 0)
        lock.notify(joinBarrier_);
}

void
ThreadPool::waitForWorkers(AutoLockMonitor &lock)
{
    MOZ_ASSERT(lock.isFor(*this));
    while (activeWorkers_ > 0)
        lock.wait(joinBarrier_);
    job_ = nullptr;
}

ParallelResult
ThreadPool::executeJob(JSContext *cx, ParallelJob *job, uint16_t sliceStart, uint16_t sliceMax)
{
    MOZ_ASSERT(sliceStart < sliceMax);
    MOZ_ASSERT(CurrentThreadCanAccessRuntime(runtime_));
    MOZ_ASSERT(activeWorkers_ == 0);
    MOZ_ASSERT(!hasWork());

    if (!lazyStartWorkers(cx))
        return TP_FATAL;

    // Evenly distribute slices to the workers.
    uint16_t numSlices = sliceMax - sliceStart;
    uint16_t slicesPerWorker = numSlices / numWorkers();
    uint16_t leftover = numSlices % numWorkers();
    uint16_t sliceEnd = sliceStart;
    for (uint32_t workerId = 0; workerId < numWorkers(); workerId++) {
        if (leftover > 0) {
            sliceEnd += slicesPerWorker + 1;
            leftover--;
        } else {
            sliceEnd += slicesPerWorker;
        }
        workers_[workerId]->submitSlices(sliceStart, sliceEnd);
        sliceStart = sliceEnd;
    }
    MOZ_ASSERT(leftover == 0);

    // Notify the worker threads that there's work now.
    {
        job_ = job;
        pendingSlices_ = numSlices;
#ifdef DEBUG
        stolenSlices_ = 0;
#endif
        AutoLockMonitor lock(*this);
        lock.notifyAll();
    }

    // Do work on the main thread.
    isMainThreadActive_ = true;
    if (!job->executeFromMainThread(mainThreadWorker()))
        abortJob();
    isMainThreadActive_ = false;

    // Wait for all threads to join. While there are no pending slices at this
    // point, the slices themselves may not be finished processing.
    {
        AutoLockMonitor lock(*this);
        waitForWorkers(lock);
    }

    // Guard against errors in the self-hosted slice processing function. If
    // we still have work at this point, it is the user function's fault.
    MOZ_ASSERT(!hasWork(), "User function did not process all the slices!");

    // Everything went swimmingly. Give yourself a pat on the back.
    return TP_SUCCESS;
}

void
ThreadPool::abortJob()
{
    for (uint32_t workerId = 0; workerId < numWorkers(); workerId++)
        workers_[workerId]->discardSlices();

    // Spin until pendingSlices_ reaches 0.
    //
    // The reason for this is that while calling discardSlices() clears all
    // workers' bounds, the pendingSlices_ cache might still be > 0 due to
    // still-executing calls to popSliceBack or popSliceFront in other
    // threads. When those finish, we will be sure that !hasWork(), which is
    // important to ensure that an aborted worker does not start again due to
    // the thread pool having more work.
    while (hasWork());
}
