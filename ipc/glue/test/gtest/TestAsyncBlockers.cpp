/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/ipc/AsyncBlockers.h"
#include "mozilla/gtest/MozHelpers.h"

#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsINamed.h"

using namespace mozilla;
using namespace mozilla::ipc;

#define PROCESS_EVENTS_UNTIL(_done) \
  SpinEventLoopUntil("TestAsyncBlockers"_ns, [&]() { return _done; });

class TestAsyncBlockers : public ::testing::Test {
 protected:
  void SetUp() override {
    SAVE_GDB_SLEEP(mOldSleepDuration);
    return;
  }

  void TearDown() final { RESTORE_GDB_SLEEP(mOldSleepDuration); }

 private:
#if defined(HAS_GDB_SLEEP_DURATION)
  unsigned int mOldSleepDuration = 0;
#endif  // defined(HAS_GDB_SLEEP_DURATION)
};

class Blocker {};

TEST_F(TestAsyncBlockers, Register) {
  AsyncBlockers blockers;
  Blocker* blocker = new Blocker();
  blockers.Register(blocker);
  EXPECT_TRUE(true);
}

TEST_F(TestAsyncBlockers, Register_Deregister) {
  AsyncBlockers blockers;
  Blocker* blocker = new Blocker();
  blockers.Register(blocker);
  blockers.Deregister(blocker);
  EXPECT_TRUE(true);
}

TEST_F(TestAsyncBlockers, Register_WaitUntilClear) {
  AsyncBlockers blockers;
  bool done = false;

  Blocker* blocker = new Blocker();
  blockers.Register(blocker);

  blockers.WaitUntilClear(5 * 1000)->Then(GetCurrentSerialEventTarget(),
                                          __func__, [&]() {
                                            EXPECT_TRUE(true);
                                            done = true;
                                          });

  NS_ProcessPendingEvents(nullptr);

  blockers.Deregister(blocker);

  PROCESS_EVENTS_UNTIL(done);
}

class AsyncBlockerTimerCallback : public nsITimerCallback, public nsINamed {
 protected:
  virtual ~AsyncBlockerTimerCallback();

 public:
  explicit AsyncBlockerTimerCallback() {}

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED
};

NS_IMPL_ISUPPORTS(AsyncBlockerTimerCallback, nsITimerCallback, nsINamed)

AsyncBlockerTimerCallback::~AsyncBlockerTimerCallback() = default;

NS_IMETHODIMP
AsyncBlockerTimerCallback::Notify(nsITimer* timer) {
  // If we resolve through this, it means
  // blockers.WaitUntilClear() started to wait for
  // the completion of the timeout which is not
  // good.
  EXPECT_TRUE(false);
  return NS_OK;
}

NS_IMETHODIMP
AsyncBlockerTimerCallback::GetName(nsACString& aName) {
  aName.AssignLiteral("AsyncBlockerTimerCallback");
  return NS_OK;
}

TEST_F(TestAsyncBlockers, NoRegister_WaitUntilClear) {
  AsyncBlockers blockers;
  bool done = false;

  nsCOMPtr<nsITimer> timer = NS_NewTimer();
  ASSERT_TRUE(timer);

  RefPtr<AsyncBlockerTimerCallback> timerCb = new AsyncBlockerTimerCallback();
  timer->InitWithCallback(timerCb, 1 * 1000, nsITimer::TYPE_ONE_SHOT);

  blockers.WaitUntilClear(10 * 1000)->Then(GetCurrentSerialEventTarget(),
                                           __func__, [&]() {
                                             // If we resolve through this
                                             // before the nsITimer it means we
                                             // have been resolved before the 5s
                                             // timeout
                                             EXPECT_TRUE(true);
                                             timer->Cancel();
                                             done = true;
                                           });

  PROCESS_EVENTS_UNTIL(done);
}

TEST_F(TestAsyncBlockers, Register_WaitUntilClear_0s) {
  AsyncBlockers blockers;
  bool done = false;

  Blocker* blocker = new Blocker();
  blockers.Register(blocker);

  blockers.WaitUntilClear(0)->Then(GetCurrentSerialEventTarget(), __func__,
                                   [&]() {
                                     EXPECT_TRUE(true);
                                     done = true;
                                   });

  NS_ProcessPendingEvents(nullptr);

  blockers.Deregister(blocker);

  PROCESS_EVENTS_UNTIL(done);
}

#if defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED) && !defined(ANDROID) && \
    !(defined(XP_DARWIN) && !defined(MOZ_DEBUG))
static void DeregisterEmpty_Test() {
  mozilla::gtest::DisableCrashReporter();

  AsyncBlockers blockers;
  Blocker* blocker = new Blocker();
  blockers.Deregister(blocker);
}

TEST_F(TestAsyncBlockers, DeregisterEmpty) {
  ASSERT_DEATH_IF_SUPPORTED(DeregisterEmpty_Test(), "");
}
#endif  // defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED) && !defined(ANDROID) &&
        // !(defined(XP_DARWIN) && !defined(MOZ_DEBUG))

#undef PROCESS_EVENTS_UNTIL
