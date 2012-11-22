/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsthreadpool_h___
#define jsthreadpool_h___

#if defined(JS_THREADSAFE) && defined(JS_ION)
# define JS_THREADSAFE_ION
#endif

#include <stddef.h>
#include "mozilla/StandardInteger.h"
#include "prtypes.h"
#include "js/Vector.h"
#include "jsalloc.h"
#include "prlock.h"
#include "prcvar.h"

struct JSContext;
struct JSRuntime;
struct JSCompartment;
struct JSScript;

namespace js {

class ThreadPoolWorker;

typedef void (*TaskFun)(void *userdata, size_t workerId, uintptr_t stackLimit);

class TaskExecutor
{
public:
    virtual void executeFromWorker(size_t workerId, uintptr_t stackLimit) = 0;
};

/*
 * ThreadPool used for parallel JavaScript execution as well as
 * parallel compilation.  Unless you are building a new kind of
 * parallel service, it is very likely that you do not wish to
 * interact with the threadpool directly.  In particular, if you wish
 * to execute JavaScript in parallel, you probably want to look at
 * |js::ForkJoin| in |forkjoin.cpp|.
 *
 * The ThreadPool always maintains a fixed pool of worker threads.
 * You can query the number of worker threads via the method
 * |numWorkers()|.  Note that this number may be zero (generally if
 * threads are disabled, or when manually specified for benchmarking
 * purposes).
 *
 * You can either submit jobs in one of two ways.  The first is
 * |submitOne()|, which submits a job to be executed by one worker
 * thread (this will fail if there are no worker threads).  The job
 * will be enqueued and executed by some worker (the current scheduler
 * uses round-robin load balancing; something more sophisticated,
 * e.g. a central queue or work stealing, might be better).
 *
 * The second way to submit a job is using |submitAll()|---in this
 * case, the job will be executed by all worker threads.  This does
 * not fail if there are no worker threads, it simply does nothing.
 * Of course, each thread may have any number of previously submitted
 * things that they are already working on, and so they will finish
 * those before they get to this job.  Therefore it is possible to
 * have some worker threads pick up (and even finish) their piece of
 * the job before others have even started.
 */
class ThreadPool
{
private:
    friend class ThreadPoolWorker;

    // Initialized at startup only:
    JSRuntime *const runtime_;
    js::Vector<ThreadPoolWorker*, 8, SystemAllocPolicy> workers_;

    // Next worker for |submitOne()|. Atomically modified.
    size_t nextId_;

    void terminateWorkers();

public:
    ThreadPool(JSRuntime *rt);
    ~ThreadPool();

    bool init();

    // Return number of worker threads in the pool.
    size_t numWorkers() { return workers_.length(); }

    // See comment on class:
    bool submitOne(TaskExecutor *executor);
    bool submitAll(TaskExecutor *executor);

    // Wait until all worker threads have finished their current set
    // of jobs and then return.  You must not submit new jobs after
    // invoking |terminate()|.
    bool terminate();
};

}



#endif
