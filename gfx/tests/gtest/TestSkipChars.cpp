/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "gfxSkipChars.h"
#include "mozilla/ArrayUtils.h"

static bool TestConstructor() {
  gfxSkipChars skipChars;

  EXPECT_TRUE(skipChars.GetOriginalCharCount() == 0)
      << "[1] Make sure the gfxSkipChars was properly initialized with "
         "constructor";

  return true;
}

static bool TestLength() {
  gfxSkipChars skipChars;

  skipChars.KeepChars(100);

  EXPECT_TRUE(skipChars.GetOriginalCharCount() == 100)
      << "[1] Check length after keeping chars";

  skipChars.SkipChars(50);

  EXPECT_TRUE(skipChars.GetOriginalCharCount() == 150)
      << "[2] Check length after skipping chars";

  skipChars.SkipChars(50);

  EXPECT_TRUE(skipChars.GetOriginalCharCount() == 200)
      << "[3] Check length after skipping more chars";

  skipChars.KeepChar();

  EXPECT_TRUE(skipChars.GetOriginalCharCount() == 201)
      << "[4] Check length after keeping a final char";

  return true;
}

static bool TestIterator() {
  // Test a gfxSkipChars that starts with kept chars
  gfxSkipChars skipChars1;

  skipChars1.KeepChars(9);
  skipChars1.SkipChar();
  skipChars1.KeepChars(9);
  skipChars1.SkipChar();
  skipChars1.KeepChars(9);

  EXPECT_TRUE(skipChars1.GetOriginalCharCount() == 29) << "[1] Check length";

  gfxSkipCharsIterator iter1(skipChars1);

  EXPECT_TRUE(iter1.GetOriginalOffset() == 0)
      << "[2] Check initial original offset";
  EXPECT_TRUE(iter1.GetSkippedOffset() == 0)
      << "[3] Check initial skipped offset";

  EXPECT_TRUE(iter1.IsOriginalCharSkipped() == false)
      << "[3a] Check IsOriginalCharSkipped for initial position";

  uint32_t expectSkipped1[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
                               9,  10, 11, 12, 13, 14, 15, 16, 17, 18,
                               18, 19, 20, 21, 22, 23, 24, 25, 26, 27};

  for (uint32_t i = 0; i < mozilla::ArrayLength(expectSkipped1); i++) {
    EXPECT_TRUE(iter1.ConvertOriginalToSkipped(i) == expectSkipped1[i])
        << "[4] Check mapping of original to skipped for " << i;
  }

  int32_t expectOriginal1[] = {0,  1,  2,  3,  4,  5,  6,  7,  8,
                               10, 11, 12, 13, 14, 15, 16, 17, 18,
                               20, 21, 22, 23, 24, 25, 26, 27, 28};

  for (uint32_t i = 0; i < mozilla::ArrayLength(expectOriginal1); i++) {
    EXPECT_TRUE(iter1.ConvertSkippedToOriginal(i) == expectOriginal1[i])
        << "[5] Check mapping of skipped to original for " << i;
  }

  bool expectIsOriginalSkipped1[] = {
      false, false, false, false, false, false, false, false, false, true,
      false, false, false, false, false, false, false, false, false, true,
      false, false, false, false, false, false, false, false, false};

  for (uint32_t i = 0; i < mozilla::ArrayLength(expectIsOriginalSkipped1);
       i++) {
    iter1.SetOriginalOffset(i);
    EXPECT_TRUE(iter1.IsOriginalCharSkipped() == expectIsOriginalSkipped1[i])
        << "[5.a] Check IsOriginalCharSkipped for " << i;
  }

  // Test a gfxSkipChars that starts with skipped chars
  gfxSkipChars skipChars2;

  skipChars2.SkipChars(9);
  skipChars2.KeepChar();
  skipChars2.SkipChars(9);
  skipChars2.KeepChar();
  skipChars2.SkipChars(9);

  EXPECT_TRUE(skipChars2.GetOriginalCharCount() == 29) << "[6] Check length";

  gfxSkipCharsIterator iter2(skipChars2);

  EXPECT_TRUE(iter2.GetOriginalOffset() == 0)
      << "[7] Check initial original offset";
  EXPECT_TRUE(iter2.GetSkippedOffset() == 0)
      << "[8] Check initial skipped offset";

  EXPECT_TRUE(iter2.IsOriginalCharSkipped() == true)
      << "[8a] Check IsOriginalCharSkipped for initial position";

  uint32_t expectSkipped2[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1,
                               1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};

  for (uint32_t i = 0; i < mozilla::ArrayLength(expectSkipped2); i++) {
    EXPECT_TRUE(iter2.ConvertOriginalToSkipped(i) == expectSkipped2[i])
        << "[9] Check mapping of original to skipped for " << i;
  }

  int32_t expectOriginal2[] = {9, 19, 29};

  for (uint32_t i = 0; i < mozilla::ArrayLength(expectOriginal2); i++) {
    EXPECT_TRUE(iter2.ConvertSkippedToOriginal(i) == expectOriginal2[i])
        << "[10] Check mapping of skipped to original for " << i;
  }

  bool expectIsOriginalSkipped2[] = {
      true, true, true, true, true, true, true, true, true, false,
      true, true, true, true, true, true, true, true, true, false,
      true, true, true, true, true, true, true, true, true};

  for (uint32_t i = 0; i < mozilla::ArrayLength(expectIsOriginalSkipped2);
       i++) {
    iter2.SetOriginalOffset(i);
    EXPECT_TRUE(iter2.IsOriginalCharSkipped() == expectIsOriginalSkipped2[i])
        << "[10.a] Check IsOriginalCharSkipped for " << i;
  }

  return true;
}

TEST(Gfx, gfxSkipChars)
{
  TestConstructor();
  TestLength();
  TestIterator();
}
