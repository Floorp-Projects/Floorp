/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mozilla/gtest/WaitFor.h"
#include "Pacer.h"
#include "VideoUtils.h"

using namespace mozilla;

template <typename T>
class PacerTest {
 protected:
  explicit PacerTest(TimeDuration aDuplicationInterval)
      : mTaskQueue(TaskQueue::Create(
            GetMediaThreadPool(MediaThreadType::WEBRTC_WORKER), "PacerTest")),
        mPacer(MakeRefPtr<Pacer<T>>(mTaskQueue, aDuplicationInterval)),
        mInterval(aDuplicationInterval) {}

  // Helper for calling `mPacer->Enqueue(...)`. Dispatches an event to the
  // current thread which will enqueue the event to make sure that any listeners
  // registered by a call to `WaitFor(...)` have been registered before events
  // start being processed on a background queue.
  void EnqueueSoon(T aItem, TimeStamp aTime) {
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToCurrentThread(NS_NewRunnableFunction(
        "PacerTest::EnqueueSoon",
        [pacer = mPacer, aItem = std::move(aItem), aTime] {
          pacer->Enqueue(std::move(aItem), aTime);
        })));
  }

  void TearDown() {
    mPacer->Shutdown()->Then(mTaskQueue, __func__,
                             [tq = mTaskQueue] { tq->BeginShutdown(); });
  }

  const RefPtr<TaskQueue> mTaskQueue;
  const RefPtr<Pacer<T>> mPacer;
  const TimeDuration mInterval;
};

class PacerTestInt : public PacerTest<int>, public ::testing::Test {
 protected:
  explicit PacerTestInt(TimeDuration aDuplicationInterval)
      : PacerTest<int>(aDuplicationInterval) {}

  void TearDown() override { PacerTest::TearDown(); }
};

class PacerTestIntLongDuplication : public PacerTestInt {
 protected:
  PacerTestIntLongDuplication() : PacerTestInt(TimeDuration::FromSeconds(10)) {}
};

class PacerTestIntTenMsDuplication : public PacerTestInt {
 protected:
  PacerTestIntTenMsDuplication()
      : PacerTestInt(TimeDuration::FromMilliseconds(10)) {}
};

TEST_F(PacerTestIntLongDuplication, Single) {
  auto now = TimeStamp::Now();
  auto d1 = TimeDuration::FromMilliseconds(100);
  EnqueueSoon(1, now + d1);

  auto [i, time] = WaitFor(TakeN(mPacer->PacedItemEvent(), 1)).unwrap()[0];
  EXPECT_GE(TimeStamp::Now() - now, d1);
  EXPECT_EQ(i, 1);
  EXPECT_EQ(time - now, d1);
}

TEST_F(PacerTestIntLongDuplication, Past) {
  auto now = TimeStamp::Now();
  auto d1 = TimeDuration::FromMilliseconds(100);
  EnqueueSoon(1, now - d1);

  auto [i, time] = WaitFor(TakeN(mPacer->PacedItemEvent(), 1)).unwrap()[0];
  EXPECT_GE(TimeStamp::Now() - now, -d1);
  EXPECT_EQ(i, 1);
  EXPECT_EQ(time - now, -d1);
}

TEST_F(PacerTestIntLongDuplication, TimeReset) {
  auto now = TimeStamp::Now();
  auto d1 = TimeDuration::FromMilliseconds(100);
  auto d2 = TimeDuration::FromMilliseconds(200);
  auto d3 = TimeDuration::FromMilliseconds(300);
  EnqueueSoon(1, now + d1);
  EnqueueSoon(2, now + d3);
  EnqueueSoon(3, now + d2);

  auto items = WaitFor(TakeN(mPacer->PacedItemEvent(), 2)).unwrap();

  {
    auto [i, time] = items[0];
    EXPECT_GE(TimeStamp::Now() - now, d1);
    EXPECT_EQ(i, 1);
    EXPECT_EQ(time - now, d1);
  }
  {
    auto [i, time] = items[1];
    EXPECT_GE(TimeStamp::Now() - now, d2);
    EXPECT_EQ(i, 3);
    EXPECT_EQ(time - now, d2);
  }
}

TEST_F(PacerTestIntTenMsDuplication, SingleDuplication) {
  auto now = TimeStamp::Now();
  auto d1 = TimeDuration::FromMilliseconds(100);
  EnqueueSoon(1, now + d1);

  auto items = WaitFor(TakeN(mPacer->PacedItemEvent(), 2)).unwrap();

  {
    auto [i, time] = items[0];
    EXPECT_GE(TimeStamp::Now() - now, d1);
    EXPECT_EQ(i, 1);
    EXPECT_EQ(time - now, d1);
  }
  {
    auto [i, time] = items[1];
    EXPECT_GE(TimeStamp::Now() - now, d1 + mInterval);
    EXPECT_EQ(i, 1);
    EXPECT_EQ(time - now, d1 + mInterval);
  }
}

TEST_F(PacerTestIntTenMsDuplication, RacyDuplication1) {
  auto now = TimeStamp::Now();
  auto d1 = TimeDuration::FromMilliseconds(100);
  auto d2 = d1 + mInterval - TimeDuration::FromMicroseconds(1);
  EnqueueSoon(1, now + d1);
  EnqueueSoon(2, now + d2);

  auto items = WaitFor(TakeN(mPacer->PacedItemEvent(), 3)).unwrap();

  {
    auto [i, time] = items[0];
    EXPECT_GE(TimeStamp::Now() - now, d1);
    EXPECT_EQ(i, 1);
    EXPECT_EQ(time - now, d1);
  }
  {
    auto [i, time] = items[1];
    EXPECT_GE(TimeStamp::Now() - now, d2);
    EXPECT_EQ(i, 2);
    EXPECT_EQ(time - now, d2);
  }
  {
    auto [i, time] = items[2];
    EXPECT_GE(TimeStamp::Now() - now, d2 + mInterval);
    EXPECT_EQ(i, 2);
    EXPECT_EQ(time - now, d2 + mInterval);
  }
}

TEST_F(PacerTestIntTenMsDuplication, RacyDuplication2) {
  auto now = TimeStamp::Now();
  auto d1 = TimeDuration::FromMilliseconds(100);
  auto d2 = d1 + mInterval + TimeDuration::FromMicroseconds(1);
  EnqueueSoon(1, now + d1);
  EnqueueSoon(2, now + d2);

  auto items = WaitFor(TakeN(mPacer->PacedItemEvent(), 3)).unwrap();

  {
    auto [i, time] = items[0];
    EXPECT_GE(TimeStamp::Now() - now, d1);
    EXPECT_EQ(i, 1);
    EXPECT_EQ(time - now, d1);
  }
  {
    auto [i, time] = items[1];
    EXPECT_GE(TimeStamp::Now() - now, d1 + mInterval);
    EXPECT_EQ(i, 1);
    EXPECT_EQ(time - now, d1 + mInterval);
  }
  {
    auto [i, time] = items[2];
    EXPECT_GE(TimeStamp::Now() - now, d2);
    EXPECT_EQ(i, 2);
    EXPECT_EQ(time - now, d2);
  }
}
