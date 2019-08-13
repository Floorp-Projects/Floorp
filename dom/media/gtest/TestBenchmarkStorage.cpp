/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BenchmarkStorageParent.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

using ::testing::Return;
using namespace mozilla;

TEST(BenchmarkStorage, MovingAverage)
{
  int32_t av = 0;
  int32_t win = 0;
  int32_t val = 100;
  BenchmarkStorageParent::MovingAverage(av, win, val);
  EXPECT_EQ(av, val) << "1st average";
  EXPECT_EQ(win, 1) << "1st window";

  av = 50;
  win = 1;
  val = 100;
  BenchmarkStorageParent::MovingAverage(av, win, val);
  EXPECT_EQ(av, 75) << "2nd average";
  EXPECT_EQ(win, 2) << "2nd window";

  av = 100;
  win = 9;
  val = 90;
  BenchmarkStorageParent::MovingAverage(av, win, val);
  EXPECT_EQ(av, 99) << "9th average";
  EXPECT_EQ(win, 10) << "9th window";

  av = 90;
  win = 19;
  val = 90;
  BenchmarkStorageParent::MovingAverage(av, win, val);
  EXPECT_EQ(av, 90) << "19th average";
  EXPECT_EQ(win, 20) << "19th window";

  av = 90;
  win = 20;
  val = 100;
  BenchmarkStorageParent::MovingAverage(av, win, val);
  EXPECT_EQ(av, 91) << "20th average";
  EXPECT_EQ(win, 20) << "20th window";
}

TEST(BenchmarkStorage, ParseStoredValue)
{
  int32_t win = 0;
  int32_t score = BenchmarkStorageParent::ParseStoredValue(1100, win);
  EXPECT_EQ(win, 1) << "Window";
  EXPECT_EQ(score, 100) << "Score/Percentage";

  win = 0;
  score = BenchmarkStorageParent::ParseStoredValue(10099, win);
  EXPECT_EQ(win, 10) << "Window";
  EXPECT_EQ(score, 99) << "Score/Percentage";

  win = 0;
  score = BenchmarkStorageParent::ParseStoredValue(15038, win);
  EXPECT_EQ(win, 15) << "Window";
  EXPECT_EQ(score, 38) << "Score/Percentage";

  win = 0;
  score = BenchmarkStorageParent::ParseStoredValue(20099, win);
  EXPECT_EQ(win, 20) << "Window";
  EXPECT_EQ(score, 99) << "Score/Percentage";
}

TEST(BenchmarkStorage, PrepareStoredValue)
{
  int32_t stored_value = BenchmarkStorageParent::PrepareStoredValue(80, 1);
  EXPECT_EQ(stored_value, 1080) << "Window";

  stored_value = BenchmarkStorageParent::PrepareStoredValue(100, 6);
  EXPECT_EQ(stored_value, 6100) << "Window";

  stored_value = BenchmarkStorageParent::PrepareStoredValue(1, 10);
  EXPECT_EQ(stored_value, 10001) << "Window";

  stored_value = BenchmarkStorageParent::PrepareStoredValue(88, 13);
  EXPECT_EQ(stored_value, 13088) << "Window";

  stored_value = BenchmarkStorageParent::PrepareStoredValue(100, 20);
  EXPECT_EQ(stored_value, 20100) << "Window";
}
