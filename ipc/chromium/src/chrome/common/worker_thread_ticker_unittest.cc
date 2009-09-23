// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/worker_thread_ticker.h"

#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestCallback : public WorkerThreadTicker::Callback {
 public:
  TestCallback() : counter_(0), message_loop_(MessageLoop::current()) {
  }

  virtual void OnTick() {
    counter_++;

    // Finish the test faster.
    message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

  int counter() const { return counter_; }

 private:
  int counter_;
  MessageLoop* message_loop_;
};

class LongCallback : public WorkerThreadTicker::Callback {
 public:
  virtual void OnTick() {
    PlatformThread::Sleep(1500);
  }
};

void RunMessageLoopForAWhile() {
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                          new MessageLoop::QuitTask(), 500);
  MessageLoop::current()->Run();
}

}  // namespace

TEST(WorkerThreadTickerTest, Basic) {
  MessageLoop loop;

  WorkerThreadTicker ticker(50);
  TestCallback callback;
  EXPECT_FALSE(ticker.IsRunning());
  EXPECT_TRUE(ticker.RegisterTickHandler(&callback));
  EXPECT_TRUE(ticker.UnregisterTickHandler(&callback));
  EXPECT_TRUE(ticker.Start());
  EXPECT_FALSE(ticker.RegisterTickHandler(&callback));
  EXPECT_FALSE(ticker.UnregisterTickHandler(&callback));
  EXPECT_TRUE(ticker.IsRunning());
  EXPECT_FALSE(ticker.Start());  // Can't start when it is running.
  EXPECT_TRUE(ticker.Stop());
  EXPECT_FALSE(ticker.IsRunning());
  EXPECT_FALSE(ticker.Stop());  // Can't stop when it isn't running.
}

TEST(WorkerThreadTickerTest, Callback) {
  MessageLoop loop;

  WorkerThreadTicker ticker(50);
  TestCallback callback;
  ASSERT_TRUE(ticker.RegisterTickHandler(&callback));

  ASSERT_TRUE(ticker.Start());
  RunMessageLoopForAWhile();
  EXPECT_TRUE(callback.counter() > 0);

  ASSERT_TRUE(ticker.Stop());
  const int counter_value = callback.counter();
  RunMessageLoopForAWhile();
  EXPECT_EQ(counter_value, callback.counter());
}

TEST(WorkerThreadTickerTest, OutOfScope) {
  MessageLoop loop;

  TestCallback callback;
  {
    WorkerThreadTicker ticker(50);
    ASSERT_TRUE(ticker.RegisterTickHandler(&callback));

    ASSERT_TRUE(ticker.Start());
    RunMessageLoopForAWhile();
    EXPECT_TRUE(callback.counter() > 0);
  }
  const int counter_value = callback.counter();
  RunMessageLoopForAWhile();
  EXPECT_EQ(counter_value, callback.counter());
}

TEST(WorkerThreadTickerTest, LongCallback) {
  MessageLoop loop;

  WorkerThreadTicker ticker(50);
  LongCallback callback;
  ASSERT_TRUE(ticker.RegisterTickHandler(&callback));

  ASSERT_TRUE(ticker.Start());
  RunMessageLoopForAWhile();
}
