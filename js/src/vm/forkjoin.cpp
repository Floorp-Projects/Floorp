/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "forkjoin.h"
#include "monitor.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "prthread.h"
#include "forkjoininlines.h"

namespace js {

class ForkJoinShared
    : public TaskExecutor,
      public Monitor
{
    ////////////////////////////////////////////////////////////////////////
    // Constant fields

    JSContext *const cx_;          // Current context
    ThreadPool *const threadPool_; // The thread pool.
    ForkJoinOp &op_;               // User-defined operations to be perf. in par.
    const size_t numThreads_;      // Total number of threads.
    PRCondVar *rendezvousEnd_;     // Cond. var used to signal end of rendezvous.

    ////////////////////////////////////////////////////////////////////////
    // Per-thread arenas
    //
    // Each worker thread gets an arena to use when allocating.

    Vector<gc::ArenaLists *, 16> arenaListss_;

    ////////////////////////////////////////////////////////////////////////
    // Locked Fields
    //
    // Only to be accessed while holding the lock.

    size_t uncompleted_;     // Number of uncompleted worker threads.
    size_t blocked_;         // Number of threads that have joined the rendezvous.
    size_t rendezvousIndex_; // Number of rendezvous attempts

    ////////////////////////////////////////////////////////////////////////
    // Asynchronous Flags
    //
    // These can be read without the lock (hence the |volatile| declaration).

    // A thread has bailed and others should follow suit.  Set and
    // read asynchronously.  After setting abort, workers will acquire
    // the lock, decrement uncompleted, and then notify if uncompleted
    // has reached blocked.
    volatile bool abort_;

    // Set to true when a worker bails for a fatal reason.
    volatile bool fatal_;

    // A thread has request a rendezvous.  Only *written* with the
    // lock (in |initiateRendezvous()| and |endRendezvous()|) but may
    // be *read* without the lock.
    volatile bool rendezvous_;

    // Invoked only from the main thread:
    void executeFromMainThread(uintptr_t stackLimit);

    // Executes slice #threadId of the work, either from a worker or
    // the main thread.
    void executePortion(PerThreadData *perThread, size_t threadId, uintptr_t stackLimit);

    // Rendezvous protocol:
    //
    // Use AutoRendezvous rather than invoking initiateRendezvous()
    // and endRendezvous() directly.

    friend class AutoRendezvous;

    // Requests that the other threads stop.  Must be invoked from the
    // main thread.
    void initiateRendezvous(ForkJoinSlice &threadCx);

    // If a rendezvous has been requested, blocks until the main
    // thread says we may continue.
    void joinRendezvous(ForkJoinSlice &threadCx);

    // Permits other threads to resume execution.  Must be invoked
    // from the main thread after a call to initiateRendezvous().
    void endRendezvous(ForkJoinSlice &threadCx);

public:
    ForkJoinShared(JSContext *cx,
                   ThreadPool *threadPool,
                   ForkJoinOp &op,
                   size_t numThreads,
                   size_t uncompleted);
    ~ForkJoinShared();

    bool init();

    ParallelResult execute();

    // Invoked from parallel worker threads:
    virtual void executeFromWorker(size_t threadId, uintptr_t stackLimit);

    // Moves all the per-thread arenas into the main compartment.
    // This can only safely be invoked on the main thread, either
    // during a rendezvous or after the workers have completed.
    void transferArenasToCompartment();

    // Invoked during processing by worker threads to "check in"
    bool check(ForkJoinSlice &threadCx);

    // See comment on |ForkJoinSlice::setFatal()| in forkjoin.h
    bool setFatal();

    JSRuntime *runtime() { return cx_->runtime; }
};

class AutoRendezvous {
private:
    ForkJoinSlice &threadCx;

public:
    AutoRendezvous(ForkJoinSlice &threadCx)
        : threadCx(threadCx)
    {
        threadCx.shared->initiateRendezvous(threadCx);
    }

    ~AutoRendezvous()
    {
        threadCx.shared->endRendezvous(threadCx);
    }
};

PRUintn ForkJoinSlice::ThreadPrivateIndex;

class AutoSetForkJoinSlice
{
public:
    AutoSetForkJoinSlice(ForkJoinSlice *threadCx)
    {
        PR_SetThreadPrivate(ForkJoinSlice::ThreadPrivateIndex, threadCx);
    }

    ~AutoSetForkJoinSlice()
    {
        PR_SetThreadPrivate(ForkJoinSlice::ThreadPrivateIndex, NULL);
    }
};

bool
ForkJoinSlice::Initialize()
{
    PRStatus status = PR_NewThreadPrivateIndex(&ThreadPrivateIndex, NULL);
    return status == PR_SUCCESS;
}

ParallelResult ExecuteForkJoinOp(JSContext *cx, ForkJoinOp &op)
{
#   ifndef JS_THREADSAFE_ION
    return TP_RETRY_SEQUENTIALLY;
#   else
    JS_ASSERT(!InParallelSection()); // Recursive use of the ThreadPool is not supported.

    ThreadPool *threadPool = &cx->runtime->threadPool;
    size_t numThreads = threadPool->numWorkers() + 1; // parallel workers plus this main thread

    ForkJoinShared shared(cx, threadPool, op, numThreads, numThreads - 1);
    if (!shared.init())
        return TP_RETRY_SEQUENTIALLY;

    return shared.execute();
#   endif
}

/****************************************************************************
 * ForkJoinShared
 */

ForkJoinShared::ForkJoinShared(JSContext *cx,
                               ThreadPool *threadPool,
                               ForkJoinOp &op,
                               size_t numThreads,
                               size_t uncompleted)
    : cx_(cx),
      threadPool_(threadPool),
      op_(op),
      numThreads_(numThreads),
      arenaListss_(cx),
      uncompleted_(uncompleted),
      blocked_(0),
      rendezvousIndex_(0),
      abort_(false),
      fatal_(false),
      rendezvous_(false)
{}

bool
ForkJoinShared::init()
{
    // Create temporary arenas to hold the data allocated during the
    // parallel code.
    //
    // Note: you might think (as I did, initially) that we could use
    // compartment ArenaLists for the main thread.  This is not true,
    // because when executing parallel code we sometimes check what
    // arena list an object is in to decide if it is writable.  If we
    // used the compartment ArenaLists for the main thread, then the
    // main thread would be permitted to write to any object it wants.

    if (!Monitor::init())
        return false;

    rendezvousEnd_ = PR_NewCondVar(lock_);
    if (!rendezvousEnd_)
        return false;

    for (unsigned i = 0; i < numThreads_; i++) {
        gc::ArenaLists *arenaLists = cx_->new_<gc::ArenaLists>();
        if (!arenaLists)
            return false;

        if (!arenaListss_.append(arenaLists)) {
            delete arenaLists;
            return false;
        }
    }

    return true;
}

ForkJoinShared::~ForkJoinShared()
{
    PR_DestroyCondVar(rendezvousEnd_);

    while (arenaListss_.length() > 0) {
        delete arenaListss_.popCopy();
    }
}

ParallelResult
ForkJoinShared::execute()
{
    AutoLockMonitor lock(*this);

    // give the task set a chance to prepare for parallel workload
    if (!op_.pre(numThreads_))
        return TP_RETRY_SEQUENTIALLY;

    // notify workers to start and execute one portion on this thread
    {
        AutoUnlockMonitor unlock(*this);
        threadPool_->submitAll(this);
        executeFromMainThread(cx_->runtime->ionStackLimit);
    }

    // wait for workers to complete
    while (uncompleted_ > 0)
        lock.wait();

    // check if any of the workers failed
    if (abort_) {
        if (fatal_)
            return TP_FATAL;
        else
            return TP_RETRY_SEQUENTIALLY;
    }

    transferArenasToCompartment();

    // give task set a chance to cleanup after parallel execution
    if (!op_.post(numThreads_))
        return TP_RETRY_SEQUENTIALLY;

    return TP_SUCCESS; // everything went swimmingly. give yourself a pat on the back.
}

void
ForkJoinShared::transferArenasToCompartment()
{
#if 0
    // This code will become relevant once other
    // bugs are merged down.

    JSRuntime *rt = cx_->runtime;
    JSCompartment *comp = cx_->compartment;
    for (unsigned i = 0; i < numThreads_; i++) {
        comp->arenas.adoptArenas(rt, arenaListss_[i]);
    }
#endif
}

void
ForkJoinShared::executeFromWorker(size_t workerId, uintptr_t stackLimit)
{
    JS_ASSERT(workerId < numThreads_ - 1);

    PerThreadData thisThread(cx_->runtime);
    TlsPerThreadData.set(&thisThread);
    executePortion(&thisThread, workerId, stackLimit);
    TlsPerThreadData.set(NULL);

    AutoLockMonitor lock(*this);
    uncompleted_ -= 1;
    if (blocked_ == uncompleted_) {
        // Signal the main thread that we have terminated.  It will be
        // either working, arranging a rendezvous, or waiting for
        // workers to complete.
        lock.notify();
    }
}

void
ForkJoinShared::executeFromMainThread(uintptr_t stackLimit)
{
    executePortion(&cx_->runtime->mainThread, numThreads_ - 1, stackLimit);
}

void
ForkJoinShared::executePortion(PerThreadData *perThread,
                               size_t threadId,
                               uintptr_t stackLimit)
{
    gc::ArenaLists *arenaLists = arenaListss_[threadId];
    ForkJoinSlice slice(perThread, threadId, numThreads_,
                        stackLimit, arenaLists, this);
    AutoSetForkJoinSlice autoContext(&slice);

    if (!op_.parallel(slice))
        abort_ = true;
}

bool
ForkJoinShared::setFatal()
{
    // Might as well set the abort flag to true, it will make
    // propagation faster:
    abort_ = true;
    fatal_ = true;
    return false;
}

bool
ForkJoinShared::check(ForkJoinSlice &slice)
{
    if (abort_)
        return false;

    if (slice.isMainThread()) {
        if (cx_->runtime->interrupt) {
            // If interrupt is requested, bring worker threads to a
            // halt, service the interrupt, then let them start back
            // up again.
            AutoRendezvous autoRendezvous(slice);
            if (!js_HandleExecutionInterrupt(cx_)) {
                return setFatal();
            }
        }
    } else if (rendezvous_) {
        joinRendezvous(slice);
    }

    return true;
}

void
ForkJoinShared::initiateRendezvous(ForkJoinSlice &slice) {
    /*
      The rendezvous protocol is always initiated by the main thread.
      The main thread sets the rendezvous flag to true.  Seeing this
      flag, other threads will invoke |joinRendezvous()|, which causes
      them to (1) read |rendezvousIndex| and (2) increment the
      |blocked| counter.  Once the |blocked| counter is equal to
      |uncompleted|, all parallel threads have joined the rendezvous,
      and so the main thread is signaled.  That will cause this
      function to return.

      Some subtle points:

      - Worker threads may potentially terminate their work before
        they see the rendezvous flag.  In this case, they would
        decrement |uncompleted| rather than incrementing |blocked|.
        Either way, if the two variables become equal, the main thread
        will be notified

      - The |rendezvousIndex| counter is used to detect the case where
        the main thread signals the end of the rendezvous and then
        starts another rendezvous before the workers have a chance to
        exit.  We circumvent this by having the workers read the
        |rendezvousIndex| counter as they enter the rendezvous, and
        then they only block until that counter is incremented.
        Another alternative would be for the main thread to block in
        |endRendezvous()| until all workers have exited, but that
        would be slower and involve unnecessary synchronization.

        Note that the main thread cannot ever get more than one
        rendezvous ahead of the workers, because it must wait for all
        of them to enter the rendezvous before it can end it, so the
        solution of using a counter is perfectly general and we need
        not fear rollover.
     */


    JS_ASSERT(slice.isMainThread());
    JS_ASSERT(!rendezvous_ && blocked_ == 0);

    AutoLockMonitor lock(*this);

    // signal other threads we want to start a rendezvous
    rendezvous_ = true;

    // wait until all the other threads blocked themselves
    while (blocked_ != uncompleted_) {
        lock.wait();
    }
}

void
ForkJoinShared::joinRendezvous(ForkJoinSlice &slice) {
    JS_ASSERT(!slice.isMainThread());
    JS_ASSERT(rendezvous_);

    AutoLockMonitor lock(*this);
    const size_t index = rendezvousIndex_;
    blocked_ += 1;

    // If we're the last to arrive, let the main thread know about it.
    if (blocked_ == uncompleted_) {
        lock.notify();
    }

    // Wait until the main thread terminates the rendezvous.  We use a
    // separate condition variable here to distinguish between workers
    // notifying the main thread that they have completed and the main
    // thread notifying the workers to resume.
    while (rendezvousIndex_ == index) {
        PR_WaitCondVar(rendezvousEnd_, PR_INTERVAL_NO_TIMEOUT);
    }
}

void
ForkJoinShared::endRendezvous(ForkJoinSlice &slice) {
    JS_ASSERT(slice.isMainThread());

    AutoLockMonitor lock(*this);
    rendezvous_ = false;
    blocked_ = 0;
    rendezvousIndex_ += 1;

    // signal other threads that rendezvous is over
    PR_NotifyAllCondVar(rendezvousEnd_);
}

/****************************************************************************
 * ForkJoinSlice
 */

ForkJoinSlice::ForkJoinSlice(PerThreadData *perThreadData,
                             size_t sliceId, size_t numSlices,
                             uintptr_t stackLimit, gc::ArenaLists *arenaLists,
                             ForkJoinShared *shared)
    : perThreadData(perThreadData),
      sliceId(sliceId),
      numSlices(numSlices),
      ionStackLimit(stackLimit),
      arenaLists(arenaLists),
      shared(shared)
{}

bool
ForkJoinSlice::isMainThread()
{
    return perThreadData == &shared->runtime()->mainThread;
}

JSRuntime *
ForkJoinSlice::runtime()
{
    return shared->runtime();
}

bool
ForkJoinSlice::check()
{
    return shared->check(*this);
}

bool
ForkJoinSlice::setFatal()
{
    return shared->setFatal();
}

}
