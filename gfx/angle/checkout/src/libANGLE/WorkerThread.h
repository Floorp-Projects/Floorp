//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WorkerThread:
//   Asychronous tasks/threads for ANGLE, similar to a TaskRunner in Chromium.
//   Can be implemented as different targets, depending on platform.
//

#ifndef LIBANGLE_WORKER_THREAD_H_
#define LIBANGLE_WORKER_THREAD_H_

#include <array>
#include <memory>
#include <vector>

#include "common/debug.h"
#include "libANGLE/features.h"

namespace angle
{

// A callback function with no return value and no arguments.
class Closure
{
  public:
    virtual ~Closure()        = default;
    virtual void operator()() = 0;
};

// An event that we can wait on, useful for joining worker threads.
class WaitableEvent : angle::NonCopyable
{
  public:
    WaitableEvent();
    virtual ~WaitableEvent();

    // Waits indefinitely for the event to be signaled.
    virtual void wait() = 0;

    // Peeks whether the event is ready. If ready, wait() will not block.
    virtual bool isReady() = 0;

    template <size_t Count>
    static void WaitMany(std::array<std::shared_ptr<WaitableEvent>, Count> *waitables)
    {
        ASSERT(Count > 0);
        for (size_t index = 0; index < Count; ++index)
        {
            (*waitables)[index]->wait();
        }
    }
};

// Request WorkerThreads from the WorkerThreadPool. Each pool can keep worker threads around so
// we avoid the costly spin up and spin down time.
class WorkerThreadPool : angle::NonCopyable
{
  public:
    WorkerThreadPool();
    virtual ~WorkerThreadPool();

    static std::shared_ptr<WorkerThreadPool> Create(bool multithreaded);

    // Returns an event to wait on for the task to finish.
    // If the pool fails to create the task, returns null.
    virtual std::shared_ptr<WaitableEvent> postWorkerTask(std::shared_ptr<Closure> task) = 0;

    virtual void setMaxThreads(size_t maxThreads) = 0;

    virtual bool isAsync() = 0;
};

}  // namespace angle

#endif  // LIBANGLE_WORKER_THREAD_H_
