// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/atomic_sequence_num.h"
#include "base/lock.h"
#include "base/simple_thread.h"
#include "base/string_util.h"
#include "base/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SetIntRunner : public base::DelegateSimpleThread::Delegate {
 public:
  SetIntRunner(int* ptr, int val) : ptr_(ptr), val_(val) { }
  ~SetIntRunner() { }

  virtual void Run() {
    *ptr_ = val_;
  }

 private:
  int* ptr_;
  int val_;
};

class WaitEventRunner : public base::DelegateSimpleThread::Delegate {
 public:
  WaitEventRunner(base::WaitableEvent* event) : event_(event) { }
  ~WaitEventRunner() { }

  virtual void Run() {
    EXPECT_FALSE(event_->IsSignaled());
    event_->Signal();
    EXPECT_TRUE(event_->IsSignaled());
  }
 private:
  base::WaitableEvent* event_;
};

class SeqRunner : public base::DelegateSimpleThread::Delegate {
 public:
  SeqRunner(base::AtomicSequenceNumber* seq) : seq_(seq) { }
  virtual void Run() {
    seq_->GetNext();
  }

 private:
  base::AtomicSequenceNumber* seq_;
};

// We count up on a sequence number, firing on the event when we've hit our
// expected amount, otherwise we wait on the event.  This will ensure that we
// have all threads outstanding until we hit our expected thread pool size.
class VerifyPoolRunner : public base::DelegateSimpleThread::Delegate {
 public:
  VerifyPoolRunner(base::AtomicSequenceNumber* seq,
                   int total, base::WaitableEvent* event)
      : seq_(seq), total_(total), event_(event) { }

  virtual void Run() {
    if (seq_->GetNext() == total_) {
      event_->Signal();
    } else {
      event_->Wait();
    }
  }

 private:
  base::AtomicSequenceNumber* seq_;
  int total_;
  base::WaitableEvent* event_;
};

}  // namespace

TEST(SimpleThreadTest, CreateAndJoin) {
  int stack_int = 0;

  SetIntRunner runner(&stack_int, 7);
  EXPECT_EQ(0, stack_int);

  base::DelegateSimpleThread thread(&runner, "int_setter");
  EXPECT_FALSE(thread.HasBeenStarted());
  EXPECT_FALSE(thread.HasBeenJoined());
  EXPECT_EQ(0, stack_int);

  thread.Start();
  EXPECT_TRUE(thread.HasBeenStarted());
  EXPECT_FALSE(thread.HasBeenJoined());

  thread.Join();
  EXPECT_TRUE(thread.HasBeenStarted());
  EXPECT_TRUE(thread.HasBeenJoined());
  EXPECT_EQ(7, stack_int);
}

TEST(SimpleThreadTest, WaitForEvent) {
  // Create a thread, and wait for it to signal us.
  base::WaitableEvent event(true, false);

  WaitEventRunner runner(&event);
  base::DelegateSimpleThread thread(&runner, "event_waiter");

  EXPECT_FALSE(event.IsSignaled());
  thread.Start();
  event.Wait();
  EXPECT_TRUE(event.IsSignaled());
  thread.Join();
}

TEST(SimpleThreadTest, NamedWithOptions) {
  base::WaitableEvent event(true, false);

  WaitEventRunner runner(&event);
  base::SimpleThread::Options options;
  base::DelegateSimpleThread thread(&runner, "event_waiter", options);
  EXPECT_EQ(thread.name_prefix(), "event_waiter");
  EXPECT_FALSE(event.IsSignaled());

  thread.Start();
  EXPECT_EQ(thread.name_prefix(), "event_waiter");
  EXPECT_EQ(thread.name(), std::string("event_waiter/") +
                            IntToString(thread.tid()));
  event.Wait();

  EXPECT_TRUE(event.IsSignaled());
  thread.Join();

  // We keep the name and tid, even after the thread is gone.
  EXPECT_EQ(thread.name_prefix(), "event_waiter");
  EXPECT_EQ(thread.name(), std::string("event_waiter/") +
                            IntToString(thread.tid()));
}

TEST(SimpleThreadTest, ThreadPool) {
  base::AtomicSequenceNumber seq;
  SeqRunner runner(&seq);
  base::DelegateSimpleThreadPool pool("seq_runner", 10);

  // Add work before we're running.
  pool.AddWork(&runner, 300);

  EXPECT_EQ(seq.GetNext(), 0);
  pool.Start();

  // Add work while we're running.
  pool.AddWork(&runner, 300);

  pool.JoinAll();

  EXPECT_EQ(seq.GetNext(), 601);

  // We can reuse our pool.  Verify that all 10 threads can actually run in
  // parallel, so this test will only pass if there are actually 10 threads.
  base::AtomicSequenceNumber seq2;
  base::WaitableEvent event(true, false);
  // Changing 9 to 10, for example, would cause us JoinAll() to never return.
  VerifyPoolRunner verifier(&seq2, 9, &event);
  pool.Start();

  pool.AddWork(&verifier, 10);

  pool.JoinAll();
  EXPECT_EQ(seq2.GetNext(), 10);
}
