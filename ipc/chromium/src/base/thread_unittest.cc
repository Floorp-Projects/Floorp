// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/lock.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Thread;

typedef PlatformTest ThreadTest;

namespace {

class ToggleValue : public Task {
 public:
  explicit ToggleValue(bool* value) : value_(value) {
  }
  virtual void Run() {
    *value_ = !*value_;
  }
 private:
  bool* value_;
};

class SleepSome : public Task {
 public:
  explicit SleepSome(int msec) : msec_(msec) {
  }
  virtual void Run() {
    PlatformThread::Sleep(msec_);
  }
 private:
  int msec_;
};

class SleepInsideInitThread : public Thread {
 public:
  SleepInsideInitThread() : Thread("none") { init_called_ = false; }
  virtual ~SleepInsideInitThread() { }

  virtual void Init() {
    PlatformThread::Sleep(500);
    init_called_ = true;
  }
  bool InitCalled() { return init_called_; }
 private:
  bool init_called_;
};

}  // namespace

TEST_F(ThreadTest, Restart) {
  Thread a("Restart");
  a.Stop();
  EXPECT_FALSE(a.message_loop());
  EXPECT_FALSE(a.IsRunning());
  EXPECT_TRUE(a.Start());
  EXPECT_TRUE(a.message_loop());
  EXPECT_TRUE(a.IsRunning());
  a.Stop();
  EXPECT_FALSE(a.message_loop());
  EXPECT_FALSE(a.IsRunning());
  EXPECT_TRUE(a.Start());
  EXPECT_TRUE(a.message_loop());
  EXPECT_TRUE(a.IsRunning());
  a.Stop();
  EXPECT_FALSE(a.message_loop());
  EXPECT_FALSE(a.IsRunning());
  a.Stop();
  EXPECT_FALSE(a.message_loop());
  EXPECT_FALSE(a.IsRunning());
}

TEST_F(ThreadTest, StartWithOptions_StackSize) {
  Thread a("StartWithStackSize");
  // Ensure that the thread can work with only 12 kb and still process a
  // message.
  Thread::Options options;
  options.stack_size = 12*1024;
  EXPECT_TRUE(a.StartWithOptions(options));
  EXPECT_TRUE(a.message_loop());
  EXPECT_TRUE(a.IsRunning());

  bool was_invoked = false;
  a.message_loop()->PostTask(FROM_HERE, new ToggleValue(&was_invoked));

  // wait for the task to run (we could use a kernel event here
  // instead to avoid busy waiting, but this is sufficient for
  // testing purposes).
  for (int i = 100; i >= 0 && !was_invoked; --i) {
    PlatformThread::Sleep(10);
  }
  EXPECT_TRUE(was_invoked);
}

TEST_F(ThreadTest, TwoTasks) {
  bool was_invoked = false;
  {
    Thread a("TwoTasks");
    EXPECT_TRUE(a.Start());
    EXPECT_TRUE(a.message_loop());

    // Test that all events are dispatched before the Thread object is
    // destroyed.  We do this by dispatching a sleep event before the
    // event that will toggle our sentinel value.
    a.message_loop()->PostTask(FROM_HERE, new SleepSome(20));
    a.message_loop()->PostTask(FROM_HERE, new ToggleValue(&was_invoked));
  }
  EXPECT_TRUE(was_invoked);
}

TEST_F(ThreadTest, StopSoon) {
  Thread a("StopSoon");
  EXPECT_TRUE(a.Start());
  EXPECT_TRUE(a.message_loop());
  EXPECT_TRUE(a.IsRunning());
  a.StopSoon();
  a.StopSoon();
  a.Stop();
  EXPECT_FALSE(a.message_loop());
  EXPECT_FALSE(a.IsRunning());
}

TEST_F(ThreadTest, ThreadName) {
  Thread a("ThreadName");
  EXPECT_TRUE(a.Start());
  EXPECT_EQ("ThreadName", a.thread_name());
}

// Make sure we can't use a thread between Start() and Init().
TEST_F(ThreadTest, SleepInsideInit) {
  SleepInsideInitThread t;
  EXPECT_FALSE(t.InitCalled());
  t.Start();
  EXPECT_TRUE(t.InitCalled());
}
