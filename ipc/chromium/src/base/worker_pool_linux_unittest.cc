// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/worker_pool_linux.h"

#include <set>

#include "base/condition_variable.h"
#include "base/lock.h"
#include "base/platform_thread.h"
#include "base/task.h"
#include "base/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Peer class to provide passthrough access to LinuxDynamicThreadPool internals.
class LinuxDynamicThreadPool::LinuxDynamicThreadPoolPeer {
 public:
  explicit LinuxDynamicThreadPoolPeer(LinuxDynamicThreadPool* pool)
      : pool_(pool) {}

  Lock* lock() { return &pool_->lock_; }
  ConditionVariable* tasks_available_cv() {
    return &pool_->tasks_available_cv_;
  }
  const std::queue<Task*>& tasks() const { return pool_->tasks_; }
  int num_idle_threads() const { return pool_->num_idle_threads_; }
  ConditionVariable* num_idle_threads_cv() {
    return pool_->num_idle_threads_cv_.get();
  }
  void set_num_idle_threads_cv(ConditionVariable* cv) {
    pool_->num_idle_threads_cv_.reset(cv);
  }

 private:
  LinuxDynamicThreadPool* pool_;

  DISALLOW_COPY_AND_ASSIGN(LinuxDynamicThreadPoolPeer);
};

}  // namespace base

namespace {

// IncrementingTask's main purpose is to increment a counter.  It also updates a
// set of unique thread ids, and signals a ConditionVariable on completion.
// Note that since it does not block, there is no way to control the number of
// threads used if more than one IncrementingTask is consecutively posted to the
// thread pool, since the first one might finish executing before the subsequent
// PostTask() calls get invoked.
class IncrementingTask : public Task {
 public:
  IncrementingTask(Lock* counter_lock,
                   int* counter,
                   Lock* unique_threads_lock,
                   std::set<PlatformThreadId>* unique_threads)
      : counter_lock_(counter_lock),
        unique_threads_lock_(unique_threads_lock),
        unique_threads_(unique_threads),
        counter_(counter) {}

  virtual void Run() {
    AddSelfToUniqueThreadSet();
    AutoLock locked(*counter_lock_);
    (*counter_)++;
  }

  void AddSelfToUniqueThreadSet() {
    AutoLock locked(*unique_threads_lock_);
    unique_threads_->insert(PlatformThread::CurrentId());
  }

 private:
  Lock* counter_lock_;
  Lock* unique_threads_lock_;
  std::set<PlatformThreadId>* unique_threads_;
  int* counter_;

  DISALLOW_COPY_AND_ASSIGN(IncrementingTask);
};

// BlockingIncrementingTask is a simple wrapper around IncrementingTask that
// allows for waiting at the start of Run() for a WaitableEvent to be signalled.
class BlockingIncrementingTask : public Task {
 public:
  BlockingIncrementingTask(Lock* counter_lock,
                           int* counter,
                           Lock* unique_threads_lock,
                           std::set<PlatformThreadId>* unique_threads,
                           Lock* num_waiting_to_start_lock,
                           int* num_waiting_to_start,
                           ConditionVariable* num_waiting_to_start_cv,
                           base::WaitableEvent* start)
      : incrementer_(
          counter_lock, counter, unique_threads_lock, unique_threads),
        num_waiting_to_start_lock_(num_waiting_to_start_lock),
        num_waiting_to_start_(num_waiting_to_start),
        num_waiting_to_start_cv_(num_waiting_to_start_cv),
        start_(start) {}

  virtual void Run() {
    {
      AutoLock num_waiting_to_start_locked(*num_waiting_to_start_lock_);
      (*num_waiting_to_start_)++;
    }
    num_waiting_to_start_cv_->Signal();
    CHECK(start_->Wait());
    incrementer_.Run();
  }

 private:
  IncrementingTask incrementer_;
  Lock* num_waiting_to_start_lock_;
  int* num_waiting_to_start_;
  ConditionVariable* num_waiting_to_start_cv_;
  base::WaitableEvent* start_;

  DISALLOW_COPY_AND_ASSIGN(BlockingIncrementingTask);
};

class LinuxDynamicThreadPoolTest : public testing::Test {
 protected:
  LinuxDynamicThreadPoolTest()
      : pool_(new base::LinuxDynamicThreadPool("dynamic_pool", 60*60)),
        peer_(pool_.get()),
        counter_(0),
        num_waiting_to_start_(0),
        num_waiting_to_start_cv_(&num_waiting_to_start_lock_),
        start_(true, false) {}

  virtual void SetUp() {
    peer_.set_num_idle_threads_cv(new ConditionVariable(peer_.lock()));
  }

  virtual void TearDown() {
    // Wake up the idle threads so they can terminate.
    if (pool_.get()) pool_->Terminate();
  }

  void WaitForTasksToStart(int num_tasks) {
    AutoLock num_waiting_to_start_locked(num_waiting_to_start_lock_);
    while (num_waiting_to_start_ < num_tasks) {
      num_waiting_to_start_cv_.Wait();
    }
  }

  void WaitForIdleThreads(int num_idle_threads) {
    AutoLock pool_locked(*peer_.lock());
    while (peer_.num_idle_threads() < num_idle_threads) {
      peer_.num_idle_threads_cv()->Wait();
    }
  }

  Task* CreateNewIncrementingTask() {
    return new IncrementingTask(&counter_lock_, &counter_,
                                &unique_threads_lock_, &unique_threads_);
  }

  Task* CreateNewBlockingIncrementingTask() {
    return new BlockingIncrementingTask(
        &counter_lock_, &counter_, &unique_threads_lock_, &unique_threads_,
        &num_waiting_to_start_lock_, &num_waiting_to_start_,
        &num_waiting_to_start_cv_, &start_);
  }

  scoped_refptr<base::LinuxDynamicThreadPool> pool_;
  base::LinuxDynamicThreadPool::LinuxDynamicThreadPoolPeer peer_;
  Lock counter_lock_;
  int counter_;
  Lock unique_threads_lock_;
  std::set<PlatformThreadId> unique_threads_;
  Lock num_waiting_to_start_lock_;
  int num_waiting_to_start_;
  ConditionVariable num_waiting_to_start_cv_;
  base::WaitableEvent start_;
};

TEST_F(LinuxDynamicThreadPoolTest, Basic) {
  EXPECT_EQ(0, peer_.num_idle_threads());
  EXPECT_EQ(0U, unique_threads_.size());
  EXPECT_EQ(0U, peer_.tasks().size());

  // Add one task and wait for it to be completed.
  pool_->PostTask(CreateNewIncrementingTask());

  WaitForIdleThreads(1);

  EXPECT_EQ(1U, unique_threads_.size()) <<
      "There should be only one thread allocated for one task.";
  EXPECT_EQ(1, peer_.num_idle_threads());
  EXPECT_EQ(1, counter_);
}

TEST_F(LinuxDynamicThreadPoolTest, ReuseIdle) {
  // Add one task and wait for it to be completed.
  pool_->PostTask(CreateNewIncrementingTask());

  WaitForIdleThreads(1);

  // Add another 2 tasks.  One should reuse the existing worker thread.
  pool_->PostTask(CreateNewBlockingIncrementingTask());
  pool_->PostTask(CreateNewBlockingIncrementingTask());

  WaitForTasksToStart(2);
  start_.Signal();
  WaitForIdleThreads(2);

  EXPECT_EQ(2U, unique_threads_.size());
  EXPECT_EQ(2, peer_.num_idle_threads());
  EXPECT_EQ(3, counter_);
}

TEST_F(LinuxDynamicThreadPoolTest, TwoActiveTasks) {
  // Add two blocking tasks.
  pool_->PostTask(CreateNewBlockingIncrementingTask());
  pool_->PostTask(CreateNewBlockingIncrementingTask());

  EXPECT_EQ(0, counter_) << "Blocking tasks should not have started yet.";

  WaitForTasksToStart(2);
  start_.Signal();
  WaitForIdleThreads(2);

  EXPECT_EQ(2U, unique_threads_.size());
  EXPECT_EQ(2, peer_.num_idle_threads()) << "Existing threads are now idle.";
  EXPECT_EQ(2, counter_);
}

TEST_F(LinuxDynamicThreadPoolTest, Complex) {
  // Add two non blocking tasks and wait for them to finish.
  pool_->PostTask(CreateNewIncrementingTask());

  WaitForIdleThreads(1);

  // Add two blocking tasks, start them simultaneously, and wait for them to
  // finish.
  pool_->PostTask(CreateNewBlockingIncrementingTask());
  pool_->PostTask(CreateNewBlockingIncrementingTask());

  WaitForTasksToStart(2);
  start_.Signal();
  WaitForIdleThreads(2);

  EXPECT_EQ(3, counter_);
  EXPECT_EQ(2, peer_.num_idle_threads());
  EXPECT_EQ(2U, unique_threads_.size());

  // Wake up all idle threads so they can exit.
  {
    AutoLock locked(*peer_.lock());
    while (peer_.num_idle_threads() > 0) {
      peer_.tasks_available_cv()->Signal();
      peer_.num_idle_threads_cv()->Wait();
    }
  }

  // Add another non blocking task.  There are no threads to reuse.
  pool_->PostTask(CreateNewIncrementingTask());
  WaitForIdleThreads(1);

  EXPECT_EQ(3U, unique_threads_.size());
  EXPECT_EQ(1, peer_.num_idle_threads());
  EXPECT_EQ(4, counter_);
}

}  // namespace
