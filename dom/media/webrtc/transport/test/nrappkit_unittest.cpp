
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com
#include <iostream>

// nrappkit includes
extern "C" {
#include "nr_api.h"
#include "async_timer.h"
}

#include "runnable_utils.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;

namespace {

class TimerTest : public MtransportTest {
 public:
  TimerTest() : handle_(nullptr), fired_(false) {}
  virtual ~TimerTest() = default;

  int ArmTimer(int timeout) {
    int ret;

    test_utils_->SyncDispatchToSTS(
        WrapRunnableRet(&ret, this, &TimerTest::ArmTimer_w, timeout));

    return ret;
  }

  int ArmCancelTimer(int timeout) {
    int ret;

    test_utils_->SyncDispatchToSTS(
        WrapRunnableRet(&ret, this, &TimerTest::ArmCancelTimer_w, timeout));

    return ret;
  }

  int ArmTimer_w(int timeout) {
    return NR_ASYNC_TIMER_SET(timeout, cb, this, &handle_);
  }

  int ArmCancelTimer_w(int timeout) {
    int r;
    r = ArmTimer_w(timeout);
    if (r) return r;

    return CancelTimer_w();
  }

  int CancelTimer() {
    int ret;

    test_utils_->SyncDispatchToSTS(
        WrapRunnableRet(&ret, this, &TimerTest::CancelTimer_w));

    return ret;
  }

  int CancelTimer_w() { return NR_async_timer_cancel(handle_); }

  int Schedule() {
    int ret;

    test_utils_->SyncDispatchToSTS(
        WrapRunnableRet(&ret, this, &TimerTest::Schedule_w));

    return ret;
  }

  int Schedule_w() {
    NR_ASYNC_SCHEDULE(cb, this);

    return 0;
  }

  static void cb(NR_SOCKET r, int how, void* arg) {
    std::cerr << "Timer fired\n";

    TimerTest* t = static_cast<TimerTest*>(arg);

    t->fired_ = true;
  }

 protected:
  void* handle_;
  bool fired_;
};
}  // namespace

TEST_F(TimerTest, SimpleTimer) {
  ArmTimer(100);
  ASSERT_TRUE_WAIT(fired_, 1000);
}

TEST_F(TimerTest, CancelTimer) {
  ArmTimer(1000);
  CancelTimer();
  PR_Sleep(2000);
  ASSERT_FALSE(fired_);
}

TEST_F(TimerTest, CancelTimer0) {
  ArmCancelTimer(0);
  PR_Sleep(100);
  ASSERT_FALSE(fired_);
}

TEST_F(TimerTest, ScheduleTest) {
  Schedule();
  ASSERT_TRUE_WAIT(fired_, 1000);
}
