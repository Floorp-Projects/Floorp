// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/idle_timer.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using base::IdleTimer;


// If the timers fire too quickly, it can be tricky to make timer tests
// reliable on all buildbots.  This constant sets a minimum timer delta where
// we expect that we should be able to reliably count timers without problems
// due to slight clock/scheduling variances.
const int kSafeTestIntervalMs = 500;

namespace {

// We Mock the GetLastInputInfo function to return
// the time stored here.
static Time mock_timer_started;

bool MockIdleTimeSource(int32 *milliseconds_interval_since_last_event) {
  TimeDelta delta = Time::Now() - mock_timer_started;
  *milliseconds_interval_since_last_event =
      static_cast<int32>(delta.InMilliseconds());
  return true;
}

// TestIdle task fires after 100ms of idle time.
class TestIdleTask : public IdleTimer {
 public:
  TestIdleTask(bool repeat)
      : IdleTimer(TimeDelta::FromMilliseconds(kSafeTestIntervalMs), repeat),
        idle_counter_(0) {
        set_idle_time_source(MockIdleTimeSource);
  }

  int get_idle_counter() { return idle_counter_; }

  virtual void OnIdle() {
    idle_counter_++;
  }

 private:
  int idle_counter_;
};

// A task to help us quit the test.
class TestFinishedTask {
 public:
  TestFinishedTask() {}
  void Run() {
    MessageLoop::current()->Quit();
  }
};

// A timer which resets the idle clock.
class ResetIdleTask {
 public:
  ResetIdleTask() {}
  void Run() {
    mock_timer_started = Time::Now();
  }
};

class IdleTimerTest : public testing::Test {
 private:
  // IdleTimer requires a UI message loop on the current thread.
  MessageLoopForUI message_loop_;
};

///////////////////////////////////////////////////////////////////////////////
// NoRepeat tests:
// A non-repeating idle timer will fire once on idle, and
// then will not fire again unless it goes non-idle first.

TEST_F(IdleTimerTest, NoRepeatIdle) {
  // Create an IdleTimer, which should fire once after 500ms.
  // Create a Quit timer which will fire after 1s.
  // Verify that we fired exactly once.

  mock_timer_started = Time::Now();
  TestIdleTask test_task(false);

  TestFinishedTask finish_task;
  base::OneShotTimer<TestFinishedTask> timer;
  timer.Start(TimeDelta::FromMilliseconds(2 * kSafeTestIntervalMs),
      &finish_task, &TestFinishedTask::Run);

  test_task.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(test_task.get_idle_counter(), 1);
}

TEST_F(IdleTimerTest, NoRepeatFlipIdleOnce) {
  // Create an IdleTimer, which should fire once after 500ms.
  // Create a Quit timer which will fire after 5s.
  // Create a timer to reset once, idle after 2s.
  // Verify that we fired exactly twice.

  mock_timer_started = Time::Now();
  TestIdleTask test_task(false);

  TestFinishedTask finish_task;
  ResetIdleTask reset_task;

  base::OneShotTimer<TestFinishedTask> t1;
  t1.Start(TimeDelta::FromMilliseconds(10 * kSafeTestIntervalMs), &finish_task,
           &TestFinishedTask::Run);

  base::OneShotTimer<ResetIdleTask> t2;
  t2.Start(TimeDelta::FromMilliseconds(4 * kSafeTestIntervalMs), &reset_task,
           &ResetIdleTask::Run);

  test_task.Start();
  MessageLoop::current()->Run();

  EXPECT_EQ(test_task.get_idle_counter(), 2);
}

TEST_F(IdleTimerTest, NoRepeatNotIdle) {
  // Create an IdleTimer, which should fire once after 500ms.
  // Create a Quit timer which will fire after 5s.
  // Create a timer to reset idle every 50ms.
  // Verify that we never fired.

  mock_timer_started = Time::Now();
  TestIdleTask test_task(false);

  TestFinishedTask finish_task;
  ResetIdleTask reset_task;

  base::OneShotTimer<TestFinishedTask> t;
  t.Start(TimeDelta::FromMilliseconds(10 * kSafeTestIntervalMs), &finish_task,
          &TestFinishedTask::Run);

  base::RepeatingTimer<ResetIdleTask> reset_timer;
  reset_timer.Start(TimeDelta::FromMilliseconds(50), &reset_task,
                    &ResetIdleTask::Run);

  test_task.Start();

  MessageLoop::current()->Run();

  reset_timer.Stop();

  EXPECT_EQ(test_task.get_idle_counter(), 0);
}

///////////////////////////////////////////////////////////////////////////////
// Repeat tests:
// A repeating idle timer will fire repeatedly on each interval, as long
// as it has been idle.  So, if the machine remains idle, it will continue
// firing over and over.

TEST_F(IdleTimerTest, Repeat) {
  // Create an IdleTimer, which should fire repeatedly after 500ms.
  // Create a Quit timer which will fire after 1.5s.
  // Verify that we fired 2-3 times.
  mock_timer_started = Time::Now();
  TestIdleTask test_task(true);

  TestFinishedTask finish_task;

  base::OneShotTimer<TestFinishedTask> t;
  t.Start(TimeDelta::FromMilliseconds(kSafeTestIntervalMs * 3), &finish_task,
          &TestFinishedTask::Run);

  test_task.Start();
  MessageLoop::current()->Run();

  // In a perfect world, the idle_counter should be 2.  However,
  // due to timer 'slop', accept 2 or 3.
  EXPECT_GE(test_task.get_idle_counter(), 2);
  EXPECT_LE(test_task.get_idle_counter(), 3);
}

TEST_F(IdleTimerTest, RepeatIdleReset) {
  // Create an IdleTimer, which should fire repeatedly after 500ms.
  // Create a Quit timer which will fire after 5s.
  // Create a reset timer, which fires after 2500ms
  // Verify that we fired 8-10 times.
  mock_timer_started = Time::Now();
  TestIdleTask test_task(true);

  ResetIdleTask reset_task;
  TestFinishedTask finish_task;

  base::OneShotTimer<TestFinishedTask> t1;
  t1.Start(TimeDelta::FromMilliseconds(10 * kSafeTestIntervalMs), &finish_task,
           &TestFinishedTask::Run);

  base::OneShotTimer<ResetIdleTask> t2;
  t2.Start(TimeDelta::FromMilliseconds(5 * kSafeTestIntervalMs), &reset_task,
           &ResetIdleTask::Run);

  test_task.Start();
  MessageLoop::current()->Run();

  // In a perfect world, the idle_counter should be 9.  However,
  // since timers aren't guaranteed to fire perfectly, this can
  // be less.  Accept 8-10.
  EXPECT_GE(test_task.get_idle_counter(), 8);
  EXPECT_LE(test_task.get_idle_counter(), 10);
}

TEST_F(IdleTimerTest, RepeatNotIdle) {
  // Create an IdleTimer, which should fire repeatedly after 500ms.
  // Create a Quit timer which will fire after 4s.
  // Create a timer to reset idle every 50ms.
  // Verify that we never fired.

  mock_timer_started = Time::Now();
  TestIdleTask test_task(true);

  TestFinishedTask finish_task;
  ResetIdleTask reset_task;

  base::OneShotTimer<TestFinishedTask> t;
  t.Start(TimeDelta::FromMilliseconds(8 * kSafeTestIntervalMs), &finish_task,
          &TestFinishedTask::Run);

  base::RepeatingTimer<ResetIdleTask> reset_timer;
  reset_timer.Start(TimeDelta::FromMilliseconds(50), &reset_task,
                    &ResetIdleTask::Run);

  test_task.Start();
  MessageLoop::current()->Run();

  reset_timer.Stop();

  EXPECT_EQ(test_task.get_idle_counter(), 0);
}

}  // namespace
