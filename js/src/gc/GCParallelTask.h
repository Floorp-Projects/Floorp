/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_GCParallelTask_h
#define gc_GCParallelTask_h

#include "js/TypeDecls.h"
#include "threading/ProtectedData.h"

namespace js {

// A generic task used to dispatch work to the helper thread system.
// Users should derive from GCParallelTask add what data they need and
// override |run|.
class GCParallelTask
{
    JSRuntime* const runtime_;

    // The state of the parallel computation.
    enum TaskState {
        NotStarted,
        Dispatched,
        Finished,
    };
    UnprotectedData<TaskState> state;

    // Amount of time this task took to execute.
    ActiveThreadOrGCTaskData<mozilla::TimeDuration> duration_;

    explicit GCParallelTask(const GCParallelTask&) = delete;

  protected:
    // A flag to signal a request for early completion of the off-thread task.
    mozilla::Atomic<bool> cancel_;

    virtual void run() = 0;

  public:
    explicit GCParallelTask(JSRuntime* runtime) : runtime_(runtime), state(NotStarted), duration_(nullptr) {}
    GCParallelTask(GCParallelTask&& other)
      : runtime_(other.runtime_),
        state(other.state),
        duration_(nullptr),
        cancel_(false)
    {}

    // Derived classes must override this to ensure that join() gets called
    // before members get destructed.
    virtual ~GCParallelTask();

    JSRuntime* runtime() { return runtime_; }

    // Time spent in the most recent invocation of this task.
    mozilla::TimeDuration duration() const { return duration_; }

    // The simple interface to a parallel task works exactly like pthreads.
    bool start();
    void join();

    // If multiple tasks are to be started or joined at once, it is more
    // efficient to take the helper thread lock once and use these methods.
    bool startWithLockHeld(AutoLockHelperThreadState& locked);
    void joinWithLockHeld(AutoLockHelperThreadState& locked);

    // Instead of dispatching to a helper, run the task on the current thread.
    void runFromMainThread(JSRuntime* rt);

    // Dispatch a cancelation request.
    enum CancelMode { CancelNoWait, CancelAndWait};
    void cancel(CancelMode mode = CancelNoWait) {
        cancel_ = true;
        if (mode == CancelAndWait)
            join();
    }

    // Check if a task is actively running.
    bool isRunningWithLockHeld(const AutoLockHelperThreadState& locked) const;
    bool isRunning() const;

    // This should be friended to HelperThread, but cannot be because it
    // would introduce several circular dependencies.
  public:
    void runFromHelperThread(AutoLockHelperThreadState& locked);
};

} /* namespace js */
#endif /* gc_GCParallelTask_h */
