/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_GCParallelTask_h
#define gc_GCParallelTask_h

#include "mozilla/Move.h"

#include "js/TypeDecls.h"
#include "threading/ProtectedData.h"

namespace js {

class AutoLockHelperThreadState;

// A generic task used to dispatch work to the helper thread system.
// Users supply a function pointer to call.
//
// Note that we don't use virtual functions here because destructors can write
// the vtable pointer on entry, which can causes races if synchronization
// happens there.
class GCParallelTask : public RunnableTask {
 public:
  using TaskFunc = void (*)(GCParallelTask*);

 private:
  JSRuntime* const runtime_;
  TaskFunc func_;

  // The state of the parallel computation.
  enum class State { NotStarted, Dispatched, Finishing, Finished };
  UnprotectedData<State> state_;

  // Amount of time this task took to execute.
  MainThreadOrGCTaskData<mozilla::TimeDuration> duration_;

  explicit GCParallelTask(const GCParallelTask&) = delete;

 protected:
  // A flag to signal a request for early completion of the off-thread task.
  mozilla::Atomic<bool, mozilla::MemoryOrdering::ReleaseAcquire,
                  mozilla::recordreplay::Behavior::DontPreserve>
      cancel_;

 public:
  explicit GCParallelTask(JSRuntime* runtime, TaskFunc func)
      : runtime_(runtime),
        func_(func),
        state_(State::NotStarted),
        duration_(nullptr),
        cancel_(false) {}
  GCParallelTask(GCParallelTask&& other)
      : runtime_(other.runtime_),
        func_(other.func_),
        state_(other.state_),
        duration_(nullptr),
        cancel_(false) {}

  // Derived classes must override this to ensure that join() gets called
  // before members get destructed.
  virtual ~GCParallelTask();

  JSRuntime* runtime() { return runtime_; }

  // Time spent in the most recent invocation of this task.
  mozilla::TimeDuration duration() const { return duration_; }

  // The simple interface to a parallel task works exactly like pthreads.
  MOZ_MUST_USE bool start();
  void join();

  // If multiple tasks are to be started or joined at once, it is more
  // efficient to take the helper thread lock once and use these methods.
  MOZ_MUST_USE bool startWithLockHeld(AutoLockHelperThreadState& locked);
  void joinWithLockHeld(AutoLockHelperThreadState& locked);

  // Instead of dispatching to a helper, run the task on the current thread.
  void runFromMainThread(JSRuntime* rt);
  void joinAndRunFromMainThread(JSRuntime* rt);

  // If the task is not already running, either start it or run it on the main
  // thread if that fails.
  void startOrRunIfIdle(AutoLockHelperThreadState& lock);

  // Dispatch a cancelation request.
  void cancelAndWait() {
    cancel_ = true;
    join();
  }

  // Check if a task is running and has not called setFinishing().
  bool isRunningWithLockHeld(const AutoLockHelperThreadState& lock) const {
    return isDispatched(lock);
  }
  bool isRunning() const;

  void runTask() override { func_(this); }

 private:
  void assertNotStarted() const {
    // Don't lock here because that adds extra synchronization in debug
    // builds that may hide bugs. There's no race if the assertion passes.
    MOZ_ASSERT(state_ == State::NotStarted);
  }
  bool isNotStarted(const AutoLockHelperThreadState& lock) const {
    return state_ == State::NotStarted;
  }
  bool isDispatched(const AutoLockHelperThreadState& lock) const {
    return state_ == State::Dispatched;
  }
  bool isFinished(const AutoLockHelperThreadState& lock) const {
    return state_ == State::Finished;
  }
  void setDispatched(const AutoLockHelperThreadState& lock) {
    MOZ_ASSERT(state_ == State::NotStarted);
    state_ = State::Dispatched;
  }
  void setFinished(const AutoLockHelperThreadState& lock) {
    MOZ_ASSERT(state_ == State::Dispatched || state_ == State::Finishing);
    state_ = State::Finished;
  }
  void setNotStarted(const AutoLockHelperThreadState& lock) {
    MOZ_ASSERT(state_ == State::Finished);
    state_ = State::NotStarted;
  }

 protected:
  // Can be called to indicate that although the task is still
  // running, it is about to finish.
  void setFinishing(const AutoLockHelperThreadState& lock) {
    MOZ_ASSERT(state_ == State::NotStarted || state_ == State::Dispatched);
    if (state_ == State::Dispatched) {
      state_ = State::Finishing;
    }
  }

  // This should be friended to HelperThread, but cannot be because it
  // would introduce several circular dependencies.
 public:
  void runFromHelperThread(AutoLockHelperThreadState& locked);
};

// CRTP template to handle cast to derived type when calling run().
template <typename Derived>
class GCParallelTaskHelper : public GCParallelTask {
 public:
  explicit GCParallelTaskHelper(JSRuntime* runtime)
      : GCParallelTask(runtime, &runTaskTyped) {}
  GCParallelTaskHelper(GCParallelTaskHelper&& other)
      : GCParallelTask(std::move(other)) {}

 private:
  static void runTaskTyped(GCParallelTask* task) {
    static_cast<Derived*>(task)->run();
  }
};

} /* namespace js */
#endif /* gc_GCParallelTask_h */
