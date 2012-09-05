// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multi-threaded tests of ConditionVariable class.

#include <time.h>
#include <algorithm>
#include <vector>

#include "base/condition_variable.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/scoped_ptr.h"
#include "base/spin_wait.h"
#include "base/thread_collision_warner.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::TimeDelta;
using base::TimeTicks;

namespace {
//------------------------------------------------------------------------------
// Define our test class, with several common variables.
//------------------------------------------------------------------------------

class ConditionVariableTest : public PlatformTest {
 public:
  const TimeDelta kZeroMs;
  const TimeDelta kTenMs;
  const TimeDelta kThirtyMs;
  const TimeDelta kFortyFiveMs;
  const TimeDelta kSixtyMs;
  const TimeDelta kOneHundredMs;

  explicit ConditionVariableTest()
    : kZeroMs(TimeDelta::FromMilliseconds(0)),
      kTenMs(TimeDelta::FromMilliseconds(10)),
      kThirtyMs(TimeDelta::FromMilliseconds(30)),
      kFortyFiveMs(TimeDelta::FromMilliseconds(45)),
      kSixtyMs(TimeDelta::FromMilliseconds(60)),
      kOneHundredMs(TimeDelta::FromMilliseconds(100)) {
  }
};

//------------------------------------------------------------------------------
// Define a class that will control activities an several multi-threaded tests.
// The general structure of multi-threaded tests is that a test case will
// construct an instance of a WorkQueue.  The WorkQueue will spin up some
// threads and control them throughout their lifetime, as well as maintaining
// a central repository of the work thread's activity.  Finally, the WorkQueue
// will command the the worker threads to terminate.  At that point, the test
// cases will validate that the WorkQueue has records showing that the desired
// activities were performed.
//------------------------------------------------------------------------------

// Callers are responsible for synchronizing access to the following class.
// The WorkQueue::lock_, as accessed via WorkQueue::lock(), should be used for
// all synchronized access.
class WorkQueue : public PlatformThread::Delegate {
 public:
  explicit WorkQueue(int thread_count);
  ~WorkQueue();

  // PlatformThread::Delegate interface.
  void ThreadMain();

  //----------------------------------------------------------------------------
  // Worker threads only call the following methods.
  // They should use the lock to get exclusive access.
  int GetThreadId();  // Get an ID assigned to a thread..
  bool EveryIdWasAllocated() const;  // Indicates that all IDs were handed out.
  TimeDelta GetAnAssignment(int thread_id);  // Get a work task duration.
  void WorkIsCompleted(int thread_id);

  int task_count() const;
  bool allow_help_requests() const;  // Workers can signal more workers.
  bool shutdown() const;  // Check if shutdown has been requested.

  void thread_shutting_down();


  //----------------------------------------------------------------------------
  // Worker threads can call them but not needed to acquire a lock.
  Lock* lock();

  ConditionVariable* work_is_available();
  ConditionVariable* all_threads_have_ids();
  ConditionVariable* no_more_tasks();

  //----------------------------------------------------------------------------
  // The rest of the methods are for use by the controlling master thread (the
  // test case code).
  void ResetHistory();
  int GetMinCompletionsByWorkerThread() const;
  int GetMaxCompletionsByWorkerThread() const;
  int GetNumThreadsTakingAssignments() const;
  int GetNumThreadsCompletingTasks() const;
  int GetNumberOfCompletedTasks() const;
  TimeDelta GetWorkTime() const;

  void SetWorkTime(TimeDelta delay);
  void SetTaskCount(int count);
  void SetAllowHelp(bool allow);

  // Caller must acquire lock before calling.
  void SetShutdown();

  // Compares the |shutdown_task_count_| to the |thread_count| and returns true
  // if they are equal.  This check will acquire the |lock_| so the caller
  // should not hold the lock when calling this method.
  bool ThreadSafeCheckShutdown(int thread_count);

 private:
  // Both worker threads and controller use the following to synchronize.
  Lock lock_;
  ConditionVariable work_is_available_;  // To tell threads there is work.

  // Conditions to notify the controlling process (if it is interested).
  ConditionVariable all_threads_have_ids_;  // All threads are running.
  ConditionVariable no_more_tasks_;  // Task count is zero.

  const int thread_count_;
  scoped_array<PlatformThreadHandle> thread_handles_;
  std::vector<int> assignment_history_;  // Number of assignment per worker.
  std::vector<int> completion_history_;  // Number of completions per worker.
  int thread_started_counter_;  // Used to issue unique id to workers.
  int shutdown_task_count_;  // Number of tasks told to shutdown
  int task_count_;  // Number of assignment tasks waiting to be processed.
  TimeDelta worker_delay_;  // Time each task takes to complete.
  bool allow_help_requests_;  // Workers can signal more workers.
  bool shutdown_;  // Set when threads need to terminate.

  DFAKE_MUTEX(locked_methods_)
};

//------------------------------------------------------------------------------
// The next section contains the actual tests.
//------------------------------------------------------------------------------

TEST_F(ConditionVariableTest, StartupShutdownTest) {
  Lock lock;

  // First try trivial startup/shutdown.
  {
    ConditionVariable cv1(&lock);
  }  // Call for cv1 destruction.

  // Exercise with at least a few waits.
  ConditionVariable cv(&lock);

  lock.Acquire();
  cv.TimedWait(kTenMs);  // Wait for 10 ms.
  cv.TimedWait(kTenMs);  // Wait for 10 ms.
  lock.Release();

  lock.Acquire();
  cv.TimedWait(kTenMs);  // Wait for 10 ms.
  cv.TimedWait(kTenMs);  // Wait for 10 ms.
  cv.TimedWait(kTenMs);  // Wait for 10 ms.
  lock.Release();
}  // Call for cv destruction.

TEST_F(ConditionVariableTest, TimeoutTest) {
  Lock lock;
  ConditionVariable cv(&lock);
  lock.Acquire();

  TimeTicks start = TimeTicks::Now();
  const TimeDelta WAIT_TIME = TimeDelta::FromMilliseconds(300);
  // Allow for clocking rate granularity.
  const TimeDelta FUDGE_TIME = TimeDelta::FromMilliseconds(50);

  cv.TimedWait(WAIT_TIME + FUDGE_TIME);
  TimeDelta duration = TimeTicks::Now() - start;
  // We can't use EXPECT_GE here as the TimeDelta class does not support the
  // required stream conversion.
  EXPECT_TRUE(duration >= WAIT_TIME);

  lock.Release();
}

// Test serial task servicing, as well as two parallel task servicing methods.
// TODO(maruel): http://crbug.com/10607
TEST_F(ConditionVariableTest, DISABLED_MultiThreadConsumerTest) {
  const int kThreadCount = 10;
  WorkQueue queue(kThreadCount);  // Start the threads.

  const int kTaskCount = 10;  // Number of tasks in each mini-test here.

  base::Time start_time;  // Used to time task processing.

  {
    AutoLock auto_lock(*queue.lock());
    while (!queue.EveryIdWasAllocated())
      queue.all_threads_have_ids()->Wait();
  }

  // Wait a bit more to allow threads to reach their wait state.
  // If threads aren't in a wait state, they may start to gobble up tasks in
  // parallel, short-circuiting (breaking) this test.
  PlatformThread::Sleep(100);

  {
    // Since we have no tasks yet, all threads should be waiting by now.
    AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(0, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(0, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.task_count());
    EXPECT_EQ(0, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetNumberOfCompletedTasks());

    // Set up to make one worker do 30ms tasks sequentially.
    queue.ResetHistory();
    queue.SetTaskCount(kTaskCount);
    queue.SetWorkTime(kThirtyMs);
    queue.SetAllowHelp(false);

    start_time = base::Time::Now();
  }

  queue.work_is_available()->Signal();  // Start up one thread.


  {
    // Wait until all 10 work tasks have at least been assigned.
    AutoLock auto_lock(*queue.lock());
    while(queue.task_count())
      queue.no_more_tasks()->Wait();
    // The last of the tasks *might* still be running, but... all but one should
    // be done by now, since tasks are being done serially.
    EXPECT_LE(queue.GetWorkTime().InMilliseconds() * (kTaskCount - 1),
              (base::Time::Now() - start_time).InMilliseconds());

    EXPECT_EQ(1, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(1, queue.GetNumThreadsCompletingTasks());
    EXPECT_LE(kTaskCount - 1, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_LE(kTaskCount - 1, queue.GetNumberOfCompletedTasks());
  }

  // Wait to be sure all tasks are done.
  while (1) {
    {
      AutoLock auto_lock(*queue.lock());
      if (kTaskCount == queue.GetNumberOfCompletedTasks())
        break;
    }
    PlatformThread::Sleep(30);  // Wait a little.
  }

  {
    // Check that all work was done by one thread id.
    AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(1, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(1, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.task_count());
    EXPECT_EQ(kTaskCount, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(kTaskCount, queue.GetNumberOfCompletedTasks());

    // Set up to make each task include getting help from another worker, so
    // so that the work gets done in paralell.
    queue.ResetHistory();
    queue.SetTaskCount(kTaskCount);
    queue.SetWorkTime(kThirtyMs);
    queue.SetAllowHelp(true);

    start_time = base::Time::Now();
  }

  queue.work_is_available()->Signal();  // But each worker can signal another.
  // Wait to allow the all workers to get done.
  while (1) {
    {
      AutoLock auto_lock(*queue.lock());
      if (kTaskCount == queue.GetNumberOfCompletedTasks())
        break;
    }
    PlatformThread::Sleep(30);  // Wait a little.
  }

  {
    // Wait until all work tasks have at least been assigned.
    AutoLock auto_lock(*queue.lock());
    while(queue.task_count())
      queue.no_more_tasks()->Wait();
    // Since they can all run almost in parallel, there is no guarantee that all
    // tasks are finished, but we should have gotten here faster than it would
    // take to run all tasks serially.
    EXPECT_GT(queue.GetWorkTime().InMilliseconds() * (kTaskCount - 1),
              (base::Time::Now() - start_time).InMilliseconds());

    // To avoid racy assumptions, we'll just assert that at least 2 threads
    // did work.
    EXPECT_LE(2, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(kTaskCount, queue.GetNumberOfCompletedTasks());

    // Try to ask all workers to help, and only a few will do the work.
    queue.ResetHistory();
    queue.SetTaskCount(3);
    queue.SetWorkTime(kThirtyMs);
    queue.SetAllowHelp(false);
  }
  queue.work_is_available()->Broadcast();  // Make them all try.
  // Wait to allow the 3 workers to get done.
  PlatformThread::Sleep(45);

  {
    AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(3, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(3, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.task_count());
    EXPECT_EQ(1, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(3, queue.GetNumberOfCompletedTasks());

    // Set up to make each task get help from another worker.
    queue.ResetHistory();
    queue.SetTaskCount(3);
    queue.SetWorkTime(kThirtyMs);
    queue.SetAllowHelp(true);  // Allow (unnecessary) help requests.
  }
  queue.work_is_available()->Broadcast();  // We already signal all threads.
  // Wait to allow the 3 workers to get done.
  PlatformThread::Sleep(100);

  {
    AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(3, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(3, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.task_count());
    EXPECT_EQ(1, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(3, queue.GetNumberOfCompletedTasks());

    // Set up to make each task get help from another worker.
    queue.ResetHistory();
    queue.SetTaskCount(20);
    queue.SetWorkTime(kThirtyMs);
    queue.SetAllowHelp(true);
  }
  queue.work_is_available()->Signal();  // But each worker can signal another.
  // Wait to allow the 10 workers to get done.
  PlatformThread::Sleep(100);  // Should take about 60 ms.

  {
    AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(10, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(10, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.task_count());
    EXPECT_EQ(2, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(2, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(20, queue.GetNumberOfCompletedTasks());

    // Same as last test, but with Broadcast().
    queue.ResetHistory();
    queue.SetTaskCount(20);  // 2 tasks per process.
    queue.SetWorkTime(kThirtyMs);
    queue.SetAllowHelp(true);
  }
  queue.work_is_available()->Broadcast();
  // Wait to allow the 10 workers to get done.
  PlatformThread::Sleep(100);  // Should take about 60 ms.

  {
    AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(10, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(10, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.task_count());
    EXPECT_EQ(2, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(2, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(20, queue.GetNumberOfCompletedTasks());

    queue.SetShutdown();
  }
  queue.work_is_available()->Broadcast();  // Force check for shutdown.

  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(TimeDelta::FromMinutes(1),
                                   queue.ThreadSafeCheckShutdown(kThreadCount));
  PlatformThread::Sleep(10);  // Be sure they're all shutdown.
}

TEST_F(ConditionVariableTest, LargeFastTaskTest) {
  const int kThreadCount = 200;
  WorkQueue queue(kThreadCount);  // Start the threads.

  Lock private_lock;  // Used locally for master to wait.
  AutoLock private_held_lock(private_lock);
  ConditionVariable private_cv(&private_lock);

  {
    AutoLock auto_lock(*queue.lock());
    while (!queue.EveryIdWasAllocated())
      queue.all_threads_have_ids()->Wait();
  }

  // Wait a bit more to allow threads to reach their wait state.
  private_cv.TimedWait(kThirtyMs);

  {
    // Since we have no tasks, all threads should be waiting by now.
    AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(0, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(0, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.task_count());
    EXPECT_EQ(0, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetMinCompletionsByWorkerThread());
    EXPECT_EQ(0, queue.GetNumberOfCompletedTasks());

    // Set up to make all workers do (an average of) 20 tasks.
    queue.ResetHistory();
    queue.SetTaskCount(20 * kThreadCount);
    queue.SetWorkTime(kFortyFiveMs);
    queue.SetAllowHelp(false);
  }
  queue.work_is_available()->Broadcast();  // Start up all threads.
  // Wait until we've handed out all tasks.
  {
    AutoLock auto_lock(*queue.lock());
    while (queue.task_count() != 0)
      queue.no_more_tasks()->Wait();
  }

  // Wait till the last of the tasks complete.
  // Don't bother to use locks: We may not get info in time... but we'll see it
  // eventually.
  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(TimeDelta::FromMinutes(1),
                                    20 * kThreadCount ==
                                      queue.GetNumberOfCompletedTasks());

  {
    // With Broadcast(), every thread should have participated.
    // but with racing.. they may not all have done equal numbers of tasks.
    AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(kThreadCount, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(kThreadCount, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.task_count());
    EXPECT_LE(20, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(20 * kThreadCount, queue.GetNumberOfCompletedTasks());

    // Set up to make all workers do (an average of) 4 tasks.
    queue.ResetHistory();
    queue.SetTaskCount(kThreadCount * 4);
    queue.SetWorkTime(kFortyFiveMs);
    queue.SetAllowHelp(true);  // Might outperform Broadcast().
  }
  queue.work_is_available()->Signal();  // Start up one thread.

  // Wait until we've handed out all tasks
  {
    AutoLock auto_lock(*queue.lock());
    while (queue.task_count() != 0)
      queue.no_more_tasks()->Wait();
  }

  // Wait till the last of the tasks complete.
  // Don't bother to use locks: We may not get info in time... but we'll see it
  // eventually.
  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(TimeDelta::FromMinutes(1),
                                    4 * kThreadCount ==
                                      queue.GetNumberOfCompletedTasks());

  {
    // With Signal(), every thread should have participated.
    // but with racing.. they may not all have done four tasks.
    AutoLock auto_lock(*queue.lock());
    EXPECT_EQ(kThreadCount, queue.GetNumThreadsTakingAssignments());
    EXPECT_EQ(kThreadCount, queue.GetNumThreadsCompletingTasks());
    EXPECT_EQ(0, queue.task_count());
    EXPECT_LE(4, queue.GetMaxCompletionsByWorkerThread());
    EXPECT_EQ(4 * kThreadCount, queue.GetNumberOfCompletedTasks());

    queue.SetShutdown();
  }
  queue.work_is_available()->Broadcast();  // Force check for shutdown.

  // Wait for shutdowns to complete.
  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(TimeDelta::FromMinutes(1),
                                   queue.ThreadSafeCheckShutdown(kThreadCount));
  PlatformThread::Sleep(10);  // Be sure they're all shutdown.
}

//------------------------------------------------------------------------------
// Finally we provide the implementation for the methods in the WorkQueue class.
//------------------------------------------------------------------------------

WorkQueue::WorkQueue(int thread_count)
  : lock_(),
    work_is_available_(&lock_),
    all_threads_have_ids_(&lock_),
    no_more_tasks_(&lock_),
    thread_count_(thread_count),
    thread_handles_(new PlatformThreadHandle[thread_count]),
    assignment_history_(thread_count),
    completion_history_(thread_count),
    thread_started_counter_(0),
    shutdown_task_count_(0),
    task_count_(0),
    allow_help_requests_(false),
    shutdown_(false) {
  EXPECT_GE(thread_count_, 1);
  ResetHistory();
  SetTaskCount(0);
  SetWorkTime(TimeDelta::FromMilliseconds(30));

  for (int i = 0; i < thread_count_; ++i) {
    PlatformThreadHandle pth;
    EXPECT_TRUE(PlatformThread::Create(0, this, &pth));
    thread_handles_[i] = pth;
  }
}

WorkQueue::~WorkQueue() {
  {
    AutoLock auto_lock(lock_);
    SetShutdown();
  }
  work_is_available_.Broadcast();  // Tell them all to terminate.

  for (int i = 0; i < thread_count_; ++i) {
    PlatformThread::Join(thread_handles_[i]);
  }
}

int WorkQueue::GetThreadId() {
  DFAKE_SCOPED_RECURSIVE_LOCK(locked_methods_);
  DCHECK(!EveryIdWasAllocated());
  return thread_started_counter_++;  // Give out Unique IDs.
}

bool WorkQueue::EveryIdWasAllocated() const {
  DFAKE_SCOPED_RECURSIVE_LOCK(locked_methods_);
  return thread_count_ == thread_started_counter_;
}

TimeDelta WorkQueue::GetAnAssignment(int thread_id) {
  DFAKE_SCOPED_RECURSIVE_LOCK(locked_methods_);
  DCHECK_LT(0, task_count_);
  assignment_history_[thread_id]++;
  if (0 == --task_count_) {
    no_more_tasks_.Signal();
  }
  return worker_delay_;
}

void WorkQueue::WorkIsCompleted(int thread_id) {
  DFAKE_SCOPED_RECURSIVE_LOCK(locked_methods_);
  completion_history_[thread_id]++;
}

int WorkQueue::task_count() const {
  DFAKE_SCOPED_RECURSIVE_LOCK(locked_methods_);
  return task_count_;
}

bool WorkQueue::allow_help_requests() const {
  DFAKE_SCOPED_RECURSIVE_LOCK(locked_methods_);
  return allow_help_requests_;
}

bool WorkQueue::shutdown() const {
  lock_.AssertAcquired();
  DFAKE_SCOPED_RECURSIVE_LOCK(locked_methods_);
  return shutdown_;
}

// Because this method is called from the test's main thread we need to actually
// take the lock.  Threads will call the thread_shutting_down() method with the
// lock already acquired.
bool WorkQueue::ThreadSafeCheckShutdown(int thread_count) {
  bool all_shutdown;
  AutoLock auto_lock(lock_);
  {
    // Declare in scope so DFAKE is guranteed to be destroyed before AutoLock.
    DFAKE_SCOPED_RECURSIVE_LOCK(locked_methods_);
    all_shutdown = (shutdown_task_count_ == thread_count);
  }
  return all_shutdown;
}

void WorkQueue::thread_shutting_down() {
  lock_.AssertAcquired();
  DFAKE_SCOPED_RECURSIVE_LOCK(locked_methods_);
  shutdown_task_count_++;
}

Lock* WorkQueue::lock() {
  return &lock_;
}

ConditionVariable* WorkQueue::work_is_available() {
  return &work_is_available_;
}

ConditionVariable* WorkQueue::all_threads_have_ids() {
  return &all_threads_have_ids_;
}

ConditionVariable* WorkQueue::no_more_tasks() {
  return &no_more_tasks_;
}

void WorkQueue::ResetHistory() {
  for (int i = 0; i < thread_count_; ++i) {
    assignment_history_[i] = 0;
    completion_history_[i] = 0;
  }
}

int WorkQueue::GetMinCompletionsByWorkerThread() const {
  int minumum = completion_history_[0];
  for (int i = 0; i < thread_count_; ++i)
    minumum = std::min(minumum, completion_history_[i]);
  return minumum;
}

int WorkQueue::GetMaxCompletionsByWorkerThread() const {
  int maximum = completion_history_[0];
  for (int i = 0; i < thread_count_; ++i)
    maximum = std::max(maximum, completion_history_[i]);
  return maximum;
}

int WorkQueue::GetNumThreadsTakingAssignments() const {
  int count = 0;
  for (int i = 0; i < thread_count_; ++i)
    if (assignment_history_[i])
      count++;
  return count;
}

int WorkQueue::GetNumThreadsCompletingTasks() const {
  int count = 0;
  for (int i = 0; i < thread_count_; ++i)
    if (completion_history_[i])
      count++;
  return count;
}

int WorkQueue::GetNumberOfCompletedTasks() const {
  int total = 0;
  for (int i = 0; i < thread_count_; ++i)
    total += completion_history_[i];
  return total;
}

TimeDelta WorkQueue::GetWorkTime() const {
  return worker_delay_;
}

void WorkQueue::SetWorkTime(TimeDelta delay) {
  worker_delay_ = delay;
}

void WorkQueue::SetTaskCount(int count) {
  task_count_ = count;
}

void WorkQueue::SetAllowHelp(bool allow) {
  allow_help_requests_ = allow;
}

void WorkQueue::SetShutdown() {
  lock_.AssertAcquired();
  shutdown_ = true;
}

//------------------------------------------------------------------------------
// Define the standard worker task. Several tests will spin out many of these
// threads.
//------------------------------------------------------------------------------

// The multithread tests involve several threads with a task to perform as
// directed by an instance of the class WorkQueue.
// The task is to:
// a) Check to see if there are more tasks (there is a task counter).
//    a1) Wait on condition variable if there are no tasks currently.
// b) Call a function to see what should be done.
// c) Do some computation based on the number of milliseconds returned in (b).
// d) go back to (a).

// WorkQueue::ThreadMain() implements the above task for all threads.
// It calls the controlling object to tell the creator about progress, and to
// ask about tasks.

void WorkQueue::ThreadMain() {
  int thread_id;
  {
    AutoLock auto_lock(lock_);
    thread_id = GetThreadId();
    if (EveryIdWasAllocated())
      all_threads_have_ids()->Signal();  // Tell creator we're ready.
  }

  Lock private_lock;  // Used to waste time on "our work".
  while (1) {  // This is the main consumer loop.
    TimeDelta work_time;
    bool could_use_help;
    {
      AutoLock auto_lock(lock_);
      while (0 == task_count() && !shutdown()) {
        work_is_available()->Wait();
      }
      if (shutdown()) {
        // Ack the notification of a shutdown message back to the controller.
        thread_shutting_down();
        return;  // Terminate.
      }
      // Get our task duration from the queue.
      work_time = GetAnAssignment(thread_id);
      could_use_help = (task_count() > 0) && allow_help_requests();
    }  // Release lock

    // Do work (outside of locked region.
    if (could_use_help)
      work_is_available()->Signal();  // Get help from other threads.

    if (work_time > TimeDelta::FromMilliseconds(0)) {
      // We could just sleep(), but we'll instead further exercise the
      // condition variable class, and do a timed wait.
      AutoLock auto_lock(private_lock);
      ConditionVariable private_cv(&private_lock);
      private_cv.TimedWait(work_time);  // Unsynchronized waiting.
    }

    {
      AutoLock auto_lock(lock_);
      // Send notification that we completed our "work."
      WorkIsCompleted(thread_id);
    }
  }
}

}  // namespace
