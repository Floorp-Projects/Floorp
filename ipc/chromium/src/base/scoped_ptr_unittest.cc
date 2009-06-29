// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ConDecLogger {
 public:
  ConDecLogger() : ptr_(NULL) { }
  explicit ConDecLogger(int* ptr) { set_ptr(ptr); }
  ~ConDecLogger() { --*ptr_; }

  void set_ptr(int* ptr) { ptr_ = ptr; ++*ptr_; }

  int SomeMeth(int x) { return x; }

 private:
  int* ptr_;
  DISALLOW_COPY_AND_ASSIGN(ConDecLogger);
};

}  // namespace

TEST(ScopedPtrTest, ScopedPtr) {
  int constructed = 0;

  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    EXPECT_EQ(10, scoper->SomeMeth(10));
    EXPECT_EQ(10, scoper.get()->SomeMeth(10));
    EXPECT_EQ(10, (*scoper).SomeMeth(10));
  }
  EXPECT_EQ(0, constructed);

  // Test reset() and release()
  {
    scoped_ptr<ConDecLogger> scoper(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoper.reset(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    scoper.reset();
    EXPECT_EQ(0, constructed);
    EXPECT_FALSE(scoper.get());

    scoper.reset(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());

    ConDecLogger* take = scoper.release();
    EXPECT_EQ(1, constructed);
    EXPECT_FALSE(scoper.get());
    delete take;
    EXPECT_EQ(0, constructed);

    scoper.reset(new ConDecLogger(&constructed));
    EXPECT_EQ(1, constructed);
    EXPECT_TRUE(scoper.get());
  }
  EXPECT_EQ(0, constructed);

  // Test swap(), == and !=
  {
    scoped_ptr<ConDecLogger> scoper1;
    scoped_ptr<ConDecLogger> scoper2;
    EXPECT_TRUE(scoper1 == scoper2.get());
    EXPECT_FALSE(scoper1 != scoper2.get());

    ConDecLogger* logger = new ConDecLogger(&constructed);
    scoper1.reset(logger);
    EXPECT_EQ(logger, scoper1.get());
    EXPECT_FALSE(scoper2.get());
    EXPECT_FALSE(scoper1 == scoper2.get());
    EXPECT_TRUE(scoper1 != scoper2.get());

    scoper2.swap(scoper1);
    EXPECT_EQ(logger, scoper2.get());
    EXPECT_FALSE(scoper1.get());
    EXPECT_FALSE(scoper1 == scoper2.get());
    EXPECT_TRUE(scoper1 != scoper2.get());
  }
  EXPECT_EQ(0, constructed);
}

TEST(ScopedPtrTest, ScopedArray) {
  static const int kNumLoggers = 12;

  int constructed = 0;

  {
    scoped_array<ConDecLogger> scoper(new ConDecLogger[kNumLoggers]);
    EXPECT_TRUE(scoper.get());
    EXPECT_EQ(&scoper[0], scoper.get());
    for (int i = 0; i < kNumLoggers; ++i) {
      scoper[i].set_ptr(&constructed);
    }
    EXPECT_EQ(12, constructed);

    EXPECT_EQ(10, scoper.get()->SomeMeth(10));
    EXPECT_EQ(10, scoper[2].SomeMeth(10));
  }
  EXPECT_EQ(0, constructed);

  // Test reset() and release()
  {
    scoped_array<ConDecLogger> scoper;
    EXPECT_FALSE(scoper.get());
    scoper.release();
    EXPECT_FALSE(scoper.get());
    scoper.reset();
    EXPECT_FALSE(scoper.get());

    scoper.reset(new ConDecLogger[kNumLoggers]);
    for (int i = 0; i < kNumLoggers; ++i) {
      scoper[i].set_ptr(&constructed);
    }
    EXPECT_EQ(12, constructed);
    scoper.reset();
    EXPECT_EQ(0, constructed);

    scoper.reset(new ConDecLogger[kNumLoggers]);
    for (int i = 0; i < kNumLoggers; ++i) {
      scoper[i].set_ptr(&constructed);
    }
    EXPECT_EQ(12, constructed);
    ConDecLogger* ptr = scoper.release();
    EXPECT_EQ(12, constructed);
    delete[] ptr;
    EXPECT_EQ(0, constructed);
  }
  EXPECT_EQ(0, constructed);

  // Test swap(), == and !=
  {
    scoped_array<ConDecLogger> scoper1;
    scoped_array<ConDecLogger> scoper2;
    EXPECT_TRUE(scoper1 == scoper2.get());
    EXPECT_FALSE(scoper1 != scoper2.get());

    ConDecLogger* loggers = new ConDecLogger[kNumLoggers];
    for (int i = 0; i < kNumLoggers; ++i) {
      loggers[i].set_ptr(&constructed);
    }
    scoper1.reset(loggers);
    EXPECT_EQ(loggers, scoper1.get());
    EXPECT_FALSE(scoper2.get());
    EXPECT_FALSE(scoper1 == scoper2.get());
    EXPECT_TRUE(scoper1 != scoper2.get());

    scoper2.swap(scoper1);
    EXPECT_EQ(loggers, scoper2.get());
    EXPECT_FALSE(scoper1.get());
    EXPECT_FALSE(scoper1 == scoper2.get());
    EXPECT_TRUE(scoper1 != scoper2.get());
  }
  EXPECT_EQ(0, constructed);
}

// TODO scoped_ptr_malloc
