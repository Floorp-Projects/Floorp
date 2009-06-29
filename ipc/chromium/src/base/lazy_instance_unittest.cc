// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/atomic_sequence_num.h"
#include "base/lazy_instance.h"
#include "base/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ShadowingAtExitManager : public base::AtExitManager {
 public:
  ShadowingAtExitManager() : AtExitManager(true) { }
};

base::AtomicSequenceNumber constructed_seq_(base::LINKER_INITIALIZED);
base::AtomicSequenceNumber destructed_seq_(base::LINKER_INITIALIZED);

class ConstructAndDestructLogger {
 public:
  ConstructAndDestructLogger() {
    constructed_seq_.GetNext();
  }
  ~ConstructAndDestructLogger() {
    destructed_seq_.GetNext();
  }
};

class SlowConstructor {
 public:
  SlowConstructor() : some_int_(0) {
    PlatformThread::Sleep(1000);  // Sleep for 1 second to try to cause a race.
    ++constructed;
    some_int_ = 12;
  }
  int some_int() const { return some_int_; }

  static int constructed;
 private:
  int some_int_;
};

int SlowConstructor::constructed = 0;

class SlowDelegate : public base::DelegateSimpleThread::Delegate {
 public:
  SlowDelegate(base::LazyInstance<SlowConstructor>* lazy) : lazy_(lazy) { }
  virtual void Run() {
    EXPECT_EQ(12, lazy_->Get().some_int());
    EXPECT_EQ(12, lazy_->Pointer()->some_int());
  }

 private:
  base::LazyInstance<SlowConstructor>* lazy_;
};

}  // namespace

static base::LazyInstance<ConstructAndDestructLogger> lazy_logger(
    base::LINKER_INITIALIZED);

TEST(LazyInstanceTest, Basic) {
  {
    ShadowingAtExitManager shadow;

    EXPECT_EQ(0, constructed_seq_.GetNext());
    EXPECT_EQ(0, destructed_seq_.GetNext());

    lazy_logger.Get();
    EXPECT_EQ(2, constructed_seq_.GetNext());
    EXPECT_EQ(1, destructed_seq_.GetNext());

    lazy_logger.Pointer();
    EXPECT_EQ(3, constructed_seq_.GetNext());
    EXPECT_EQ(2, destructed_seq_.GetNext());
  }
  EXPECT_EQ(4, constructed_seq_.GetNext());
  EXPECT_EQ(4, destructed_seq_.GetNext());
}

static base::LazyInstance<SlowConstructor> lazy_slow(base::LINKER_INITIALIZED);

TEST(LazyInstanceTest, ConstructorThreadSafety) {
  {
    ShadowingAtExitManager shadow;

    SlowDelegate delegate(&lazy_slow);
    EXPECT_EQ(0, SlowConstructor::constructed);

    base::DelegateSimpleThreadPool pool("lazy_instance_cons", 5);
    pool.AddWork(&delegate, 20);
    EXPECT_EQ(0, SlowConstructor::constructed);

    pool.Start();
    pool.JoinAll();
    EXPECT_EQ(1, SlowConstructor::constructed);
  }
}
