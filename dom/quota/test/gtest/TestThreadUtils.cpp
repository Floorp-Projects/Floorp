/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ThreadUtils.h"
#include "mozilla/gtest/MozAssertions.h"
#include "QuotaManagerDependencyFixture.h"

namespace mozilla::dom::quota::test {

namespace {

MozExternalRefCountType GetRefCount(nsISupports* aSupports) {
  if (!aSupports) {
    return 0;
  }
  aSupports->AddRef();
  return aSupports->Release();
}

class OneTimeRunnable final : public Runnable {
 public:
  enum class State { None, Initial, Created, Destroyed };

  static void Init() {
    ASSERT_TRUE(sState == State::None || sState == State::Destroyed);
    ASSERT_FALSE(sCurrent);

    sState = State::Initial;
  }

  static RefPtr<OneTimeRunnable> Create(std::function<void()>&& aTask) {
    EXPECT_EQ(sState, State::Initial);

    RefPtr<OneTimeRunnable> runnable = new OneTimeRunnable(std::move(aTask));
    return runnable;
  }

  static State GetState() { return sState; }

  static OneTimeRunnable* GetCurrent() { return sCurrent; }

  static MozExternalRefCountType GetCurrentRefCount() {
    return GetRefCount(static_cast<nsIRunnable*>(sCurrent));
  }

  NS_IMETHOD
  Run() override {
    auto task = std::move(mTask);
    task();

    return NS_OK;
  }

 private:
  explicit OneTimeRunnable(std::function<void()>&& aTask)
      : Runnable("dom::quota::test::OneTimeRunnable"), mTask(std::move(aTask)) {
    sCurrent = this;
    sState = State::Created;
  }

  ~OneTimeRunnable() override {
    sState = State::Destroyed;
    sCurrent = nullptr;
  }

  static Atomic<State> sState;
  static OneTimeRunnable* sCurrent;

  std::function<void()> mTask;
};

}  // namespace

Atomic<OneTimeRunnable::State> OneTimeRunnable::sState(
    OneTimeRunnable::State::None);
OneTimeRunnable* OneTimeRunnable::sCurrent(nullptr);

class TestThreadUtils : public QuotaManagerDependencyFixture {
 public:
  static void SetUpTestCase() { ASSERT_NO_FATAL_FAILURE(InitializeFixture()); }

  static void TearDownTestCase() { ASSERT_NO_FATAL_FAILURE(ShutdownFixture()); }
};

TEST_F(TestThreadUtils, RunAfterProcessingCurrentEvent_Release) {
  bool runnableCalled = false;
  bool callbackCalled = false;

  QuotaManager* quotaManager = QuotaManager::Get();
  ASSERT_TRUE(quotaManager);

  OneTimeRunnable::Init();

  auto runnable = OneTimeRunnable::Create([&runnableCalled, &callbackCalled]() {
    runnableCalled = true;

    EXPECT_EQ(OneTimeRunnable::GetState(), OneTimeRunnable::State::Created);
    EXPECT_EQ(OneTimeRunnable::GetCurrentRefCount(), 2u);

    RunAfterProcessingCurrentEvent([&callbackCalled]() {
      callbackCalled = true;

      // The runnable shouldn't be yet destroyed because we still have a strong
      // reference to it in `runnable`.
      EXPECT_EQ(OneTimeRunnable::GetState(), OneTimeRunnable::State::Created);

      // The runnable should be released once (by the event queue processing
      // code).
      EXPECT_EQ(OneTimeRunnable::GetCurrentRefCount(), 1u);
    });

    EXPECT_EQ(OneTimeRunnable::GetState(), OneTimeRunnable::State::Created);
    EXPECT_EQ(OneTimeRunnable::GetCurrentRefCount(), 2u);
  });

  ASSERT_FALSE(runnableCalled);
  ASSERT_FALSE(callbackCalled);

  // Note that we are keeping ownership of the runnable here.
  ASSERT_EQ(NS_OK,
            quotaManager->IOThread()->Dispatch(runnable, NS_DISPATCH_NORMAL));

  // Do a round-trip to the QM IO thread to ensure the test doesn't finish
  // before the OneTimeRunnable is fully processed.
  //
  // Note: In theory, we would use SyncRunnable wrapper here, but the code
  // reads better when both tests use the same way for blocking the current
  // thread.
  ASSERT_NO_FATAL_FAILURE(PerformOnIOThread([]() {}));

  // Check that the runnable and the callback were actually called.
  ASSERT_TRUE(runnableCalled);
  ASSERT_TRUE(callbackCalled);
}

TEST_F(TestThreadUtils, RunAfterProcessingCurrentEvent_ReleaseAndDestory) {
  bool runnableCalled = false;
  bool callbackCalled = false;

  QuotaManager* quotaManager = QuotaManager::Get();
  ASSERT_TRUE(quotaManager);

  OneTimeRunnable::Init();

  auto runnable = OneTimeRunnable::Create([&runnableCalled, &callbackCalled]() {
    runnableCalled = true;

    EXPECT_EQ(OneTimeRunnable::GetState(), OneTimeRunnable::State::Created);
    EXPECT_EQ(OneTimeRunnable::GetCurrentRefCount(), 1u);

    RunAfterProcessingCurrentEvent([&callbackCalled]() {
      callbackCalled = true;

      // The runnable should be destroyed because we don't have any other strong
      // references to it.
      EXPECT_EQ(OneTimeRunnable::GetState(), OneTimeRunnable::State::Destroyed);

      // The runnable should be released once (by the event queue processing
      // code).
      EXPECT_EQ(OneTimeRunnable::GetCurrentRefCount(), 0u);
    });

    EXPECT_EQ(OneTimeRunnable::GetState(), OneTimeRunnable::State::Created);
    EXPECT_EQ(OneTimeRunnable::GetCurrentRefCount(), 1u);
  });

  ASSERT_FALSE(runnableCalled);
  ASSERT_FALSE(callbackCalled);

  // Note that we are tranferring ownership of the runnable here.
  ASSERT_EQ(NS_OK, quotaManager->IOThread()->Dispatch(runnable.forget(),
                                                      NS_DISPATCH_NORMAL));

  // Do a round-trip to the QM IO thread to ensure the test doesn't finish
  // before the OneTimeRunnable is fully processed.
  //
  // Note: SyncRunnable wrapper can't be used here because that would hold our
  // runnable longer, and we couldn't test that our runnable is destroyed when
  // the callback for RunAfterProcessingCurrentEvent is executed.
  ASSERT_NO_FATAL_FAILURE(PerformOnIOThread([]() {}));

  // Check that the runnable and the callback were actually called.
  ASSERT_TRUE(runnableCalled);
  ASSERT_TRUE(callbackCalled);
}

}  // namespace mozilla::dom::quota::test
