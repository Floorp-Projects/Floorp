/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RollingNumber.h"

#include "mozilla/Assertions.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <type_traits>

using RN8 = mozilla::RollingNumber<uint8_t>;

TEST(RollingNumber, Value)
{
  // Value type should reflect template argument.
  static_assert(std::is_same_v<RN8::ValueType, uint8_t>, "");

  // Default init to 0.
  const RN8 n;
  // Access through Value().
  EXPECT_EQ(0, n.Value());

  // Conversion constructor.
  RN8 n42{42};
  EXPECT_EQ(42, n42.Value());

  // Copy Constructor.
  RN8 n42Copied{n42};
  EXPECT_EQ(42, n42Copied.Value());

  // Assignment construction.
  RN8 n42Assigned = n42;
  EXPECT_EQ(42, n42Assigned.Value());

  // Assignment.
  n42 = n;
  EXPECT_EQ(0, n42.Value());
}

TEST(RollingNumber, Operations)
{
  RN8 n;
  EXPECT_EQ(0, n.Value());

  RN8 nPreInc = ++n;
  EXPECT_EQ(1, n.Value());
  EXPECT_EQ(1, nPreInc.Value());

  RN8 nPostInc = n++;
  EXPECT_EQ(2, n.Value());
  EXPECT_EQ(1, nPostInc.Value());

  RN8 nPreDec = --n;
  EXPECT_EQ(1, n.Value());
  EXPECT_EQ(1, nPreDec.Value());

  RN8 nPostDec = n--;
  EXPECT_EQ(0, n.Value());
  EXPECT_EQ(1, nPostDec.Value());

  RN8 nPlus = n + 10;
  EXPECT_EQ(0, n.Value());
  EXPECT_EQ(10, nPlus.Value());

  n += 20;
  EXPECT_EQ(20, n.Value());

  RN8 nMinus = n - 2;
  EXPECT_EQ(20, n.Value());
  EXPECT_EQ(18, nMinus.Value());

  n -= 5;
  EXPECT_EQ(15, n.Value());

  uint8_t diff = nMinus - n;
  EXPECT_EQ(3, diff);

  // Overflows.
  n = RN8(0);
  EXPECT_EQ(0, n.Value());
  n--;
  EXPECT_EQ(255, n.Value());
  n++;
  EXPECT_EQ(0, n.Value());
  n -= 10;
  EXPECT_EQ(246, n.Value());
  n += 20;
  EXPECT_EQ(10, n.Value());
}

TEST(RollingNumber, Comparisons)
{
  uint8_t i = 0;
  do {
    RN8 n{i};
    EXPECT_EQ(i, n.Value());
    EXPECT_TRUE(n == n);
    EXPECT_FALSE(n != n);
    EXPECT_FALSE(n < n);
    EXPECT_TRUE(n <= n);
    EXPECT_FALSE(n > n);
    EXPECT_TRUE(n >= n);

    RN8 same = n;
    EXPECT_TRUE(n == same);
    EXPECT_FALSE(n != same);
    EXPECT_FALSE(n < same);
    EXPECT_TRUE(n <= same);
    EXPECT_FALSE(n > same);
    EXPECT_TRUE(n >= same);

#ifdef DEBUG
    // In debug builds, we are only allowed a quarter of the type range.
    const uint8_t maxDiff = 255 / 4;
#else
    // In non-debug builds, we can go half-way up or down the type range, and
    // still conserve the expected ordering.
    const uint8_t maxDiff = 255 / 2;
#endif
    for (uint8_t add = 1; add <= maxDiff; ++add) {
      RN8 bigger = n + add;
      EXPECT_FALSE(n == bigger);
      EXPECT_TRUE(n != bigger);
      EXPECT_TRUE(n < bigger);
      EXPECT_TRUE(n <= bigger);
      EXPECT_FALSE(n > bigger);
      EXPECT_FALSE(n >= bigger);
    }

    for (uint8_t sub = 1; sub <= maxDiff; ++sub) {
      RN8 smaller = n - sub;
      EXPECT_FALSE(n == smaller);
      EXPECT_TRUE(n != smaller);
      EXPECT_FALSE(n < smaller);
      EXPECT_FALSE(n <= smaller);
      EXPECT_TRUE(n > smaller);
      EXPECT_TRUE(n >= smaller);
    }

    ++i;
  } while (i != 0);
}
