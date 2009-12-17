// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

namespace {

class OneShotTimerTester {
 public:
  OneShotTimerTester(bool* did_run, unsigned milliseconds = 10)
      : did_run_(did_run),
        delay_ms_(milliseconds) {
  }
  void Start() {
    timer_.Start(TimeDelta::FromMilliseconds(delay_ms_), this,
                 &OneShotTimerTester::Run);
  }
 private:
  void Run() {
    *did_run_ = true;
    MessageLoop::current()->Quit();
  }
  bool* did_run_;
  base::OneShotTimer<OneShotTimerTester> timer_;
  const unsigned delay_ms_;
};

class OneShotSelfDeletingTimerTester {
 public:
  OneShotSelfDeletingTimerTester(bool* did_run) :
      did_run_(did_run),
      timer_(new base::OneShotTimer<OneShotSelfDeletingTimerTester>()) {
  }
  void Start() {
    timer_->Start(TimeDelta::FromMilliseconds(10), this,
                  &OneShotSelfDeletingTimerTester::Run);
  }
 private:
  void Run() {
    *did_run_ = true;
    timer_.reset();
    MessageLoop::current()->Quit();
  }
  bool* did_run_;
  scoped_ptr<base::OneShotTimer<OneShotSelfDeletingTimerTester> > timer_;
};

class RepeatingTimerTester {
 public:
  RepeatingTimerTester(bool* did_run) : did_run_(did_run), counter_(10) {
  }
  void Start() {
    timer_.Start(TimeDelta::FromMilliseconds(10), this,
                 &RepeatingTimerTester::Run);
  }
 private:
  void Run() {
    if (--counter_ == 0) {
      *did_run_ = true;
      MessageLoop::current()->Quit();
    }
  }
  bool* did_run_;
  int counter_;
  base::RepeatingTimer<RepeatingTimerTester> timer_;
};

void RunTest_OneShotTimer(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  bool did_run = false;
  OneShotTimerTester f(&did_run);
  f.Start();

  MessageLoop::current()->Run();

  EXPECT_TRUE(did_run);
}

void RunTest_OneShotTimer_Cancel(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  bool did_run_a = false;
  OneShotTimerTester* a = new OneShotTimerTester(&did_run_a);

  // This should run before the timer expires.
  MessageLoop::current()->DeleteSoon(FROM_HERE, a);

  // Now start the timer.
  a->Start();

  bool did_run_b = false;
  OneShotTimerTester b(&did_run_b);
  b.Start();

  MessageLoop::current()->Run();

  EXPECT_FALSE(did_run_a);
  EXPECT_TRUE(did_run_b);
}

void RunTest_OneShotSelfDeletingTimer(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  bool did_run = false;
  OneShotSelfDeletingTimerTester f(&did_run);
  f.Start();

  MessageLoop::current()->Run();

  EXPECT_TRUE(did_run);
}

void RunTest_RepeatingTimer(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  bool did_run = false;
  RepeatingTimerTester f(&did_run);
  f.Start();

  MessageLoop::current()->Run();

  EXPECT_TRUE(did_run);
}

void RunTest_RepeatingTimer_Cancel(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  bool did_run_a = false;
  RepeatingTimerTester* a = new RepeatingTimerTester(&did_run_a);

  // This should run before the timer expires.
  MessageLoop::current()->DeleteSoon(FROM_HERE, a);

  // Now start the timer.
  a->Start();

  bool did_run_b = false;
  RepeatingTimerTester b(&did_run_b);
  b.Start();

  MessageLoop::current()->Run();

  EXPECT_FALSE(did_run_a);
  EXPECT_TRUE(did_run_b);
}

class DelayTimerTarget {
 public:
  DelayTimerTarget()
      : signaled_(false) {
  }

  bool signaled() const { return signaled_; }

  void Signal() {
    ASSERT_FALSE(signaled_);
    signaled_ = true;
  }

 private:
  bool signaled_;
};

void RunTest_DelayTimer_NoCall(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // If Delay is never called, the timer shouldn't go off.
  DelayTimerTarget target;
  base::DelayTimer<DelayTimerTarget> timer(
      TimeDelta::FromMilliseconds(1), &target, &DelayTimerTarget::Signal);

  bool did_run = false;
  OneShotTimerTester tester(&did_run);
  tester.Start();
  MessageLoop::current()->Run();

  ASSERT_FALSE(target.signaled());
}

void RunTest_DelayTimer_OneCall(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  DelayTimerTarget target;
  base::DelayTimer<DelayTimerTarget> timer(
      TimeDelta::FromMilliseconds(1), &target, &DelayTimerTarget::Signal);
  timer.Reset();

  bool did_run = false;
  OneShotTimerTester tester(&did_run, 100 /* milliseconds */);
  tester.Start();
  MessageLoop::current()->Run();

  ASSERT_TRUE(target.signaled());
}

struct ResetHelper {
  ResetHelper(base::DelayTimer<DelayTimerTarget>* timer,
              DelayTimerTarget* target)
      : timer_(timer),
        target_(target) {
  }

  void Reset() {
    ASSERT_FALSE(target_->signaled());
    timer_->Reset();
  }

 private:
  base::DelayTimer<DelayTimerTarget> *const timer_;
  DelayTimerTarget *const target_;
};

void RunTest_DelayTimer_Reset(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  // If Delay is never called, the timer shouldn't go off.
  DelayTimerTarget target;
  base::DelayTimer<DelayTimerTarget> timer(
      TimeDelta::FromMilliseconds(50), &target, &DelayTimerTarget::Signal);
  timer.Reset();

  ResetHelper reset_helper(&timer, &target);

  base::OneShotTimer<ResetHelper> timers[20];
  for (size_t i = 0; i < arraysize(timers); ++i) {
    timers[i].Start(TimeDelta::FromMilliseconds(i * 10), &reset_helper,
                    &ResetHelper::Reset);
  }

  bool did_run = false;
  OneShotTimerTester tester(&did_run, 300);
  tester.Start();
  MessageLoop::current()->Run();

  ASSERT_TRUE(target.signaled());
}

class DelayTimerFatalTarget {
 public:
  void Signal() {
    ASSERT_TRUE(false);
  }
};


void RunTest_DelayTimer_Deleted(MessageLoop::Type message_loop_type) {
  MessageLoop loop(message_loop_type);

  DelayTimerFatalTarget target;

  {
    base::DelayTimer<DelayTimerFatalTarget> timer(
        TimeDelta::FromMilliseconds(50), &target,
        &DelayTimerFatalTarget::Signal);
    timer.Reset();
  }

  // When the timer is deleted, the DelayTimerFatalTarget should never be
  // called.
  PlatformThread::Sleep(100);
}

}  // namespace

//-----------------------------------------------------------------------------
// Each test is run against each type of MessageLoop.  That way we are sure
// that timers work properly in all configurations.

TEST(TimerTest, OneShotTimer) {
  RunTest_OneShotTimer(MessageLoop::TYPE_DEFAULT);
  RunTest_OneShotTimer(MessageLoop::TYPE_UI);
  RunTest_OneShotTimer(MessageLoop::TYPE_IO);
}

TEST(TimerTest, OneShotTimer_Cancel) {
  RunTest_OneShotTimer_Cancel(MessageLoop::TYPE_DEFAULT);
  RunTest_OneShotTimer_Cancel(MessageLoop::TYPE_UI);
  RunTest_OneShotTimer_Cancel(MessageLoop::TYPE_IO);
}

// If underline timer does not handle properly, we will crash or fail
// in full page heap or purify environment.
TEST(TimerTest, OneShotSelfDeletingTimer) {
  RunTest_OneShotSelfDeletingTimer(MessageLoop::TYPE_DEFAULT);
  RunTest_OneShotSelfDeletingTimer(MessageLoop::TYPE_UI);
  RunTest_OneShotSelfDeletingTimer(MessageLoop::TYPE_IO);
}

TEST(TimerTest, RepeatingTimer) {
  RunTest_RepeatingTimer(MessageLoop::TYPE_DEFAULT);
  RunTest_RepeatingTimer(MessageLoop::TYPE_UI);
  RunTest_RepeatingTimer(MessageLoop::TYPE_IO);
}

TEST(TimerTest, RepeatingTimer_Cancel) {
  RunTest_RepeatingTimer_Cancel(MessageLoop::TYPE_DEFAULT);
  RunTest_RepeatingTimer_Cancel(MessageLoop::TYPE_UI);
  RunTest_RepeatingTimer_Cancel(MessageLoop::TYPE_IO);
}

TEST(TimerTest, DelayTimer_NoCall) {
  RunTest_DelayTimer_NoCall(MessageLoop::TYPE_DEFAULT);
  RunTest_DelayTimer_NoCall(MessageLoop::TYPE_UI);
  RunTest_DelayTimer_NoCall(MessageLoop::TYPE_IO);
}

TEST(TimerTest, DelayTimer_OneCall) {
  RunTest_DelayTimer_OneCall(MessageLoop::TYPE_DEFAULT);
  RunTest_DelayTimer_OneCall(MessageLoop::TYPE_UI);
  RunTest_DelayTimer_OneCall(MessageLoop::TYPE_IO);
}

// Disabled because it's flaky on the buildbot. Bug not filed because this
// kind of test is difficult to make not-flaky on buildbots.
TEST(TimerTest, DISABLED_DelayTimer_Reset) {
  RunTest_DelayTimer_Reset(MessageLoop::TYPE_DEFAULT);
  RunTest_DelayTimer_Reset(MessageLoop::TYPE_UI);
  RunTest_DelayTimer_Reset(MessageLoop::TYPE_IO);
}

TEST(TimerTest, DelayTimer_Deleted) {
  RunTest_DelayTimer_Deleted(MessageLoop::TYPE_DEFAULT);
  RunTest_DelayTimer_Deleted(MessageLoop::TYPE_UI);
  RunTest_DelayTimer_Deleted(MessageLoop::TYPE_IO);
}

TEST(TimerTest, MessageLoopShutdown) {
  // This test is designed to verify that shutdown of the
  // message loop does not cause crashes if there were pending
  // timers not yet fired.  It may only trigger exceptions
  // if debug heap checking (or purify) is enabled.
  bool did_run = false;
  {
    OneShotTimerTester a(&did_run);
    OneShotTimerTester b(&did_run);
    OneShotTimerTester c(&did_run);
    OneShotTimerTester d(&did_run);
    {
      MessageLoop loop(MessageLoop::TYPE_DEFAULT);
      a.Start();
      b.Start();
    }  // MessageLoop destructs by falling out of scope.
  }  // OneShotTimers destruct.  SHOULD NOT CRASH, of course.

  EXPECT_EQ(false, did_run);
}
