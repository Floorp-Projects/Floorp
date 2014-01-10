/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "gfxSkipChars.h"

static bool
TestConstructor()
{
  gfxSkipChars skipChars;

  EXPECT_TRUE(skipChars.GetOriginalCharCount() == 0) <<
    "[1] Make sure the gfxSkipChars was properly initialized with constructor";

  return true;
}

static bool
TestLength()
{
  gfxSkipCharsBuilder builder;

  builder.KeepChars(100);

  EXPECT_TRUE(builder.GetCharCount() == 100) <<
    "[1] Check length after keeping chars";

  builder.SkipChars(50);

  EXPECT_TRUE(builder.GetCharCount() == 150) <<
    "[2] Check length after skipping chars";

  builder.SkipChars(50);

  EXPECT_TRUE(builder.GetCharCount() == 200) <<
    "[3] Check length after skipping more chars";

  builder.KeepChar();

  EXPECT_TRUE(builder.GetCharCount() == 201) <<
    "[4] Check length after keeping a final char";

  return true;
}

static bool
TestIterator()
{
  // Test a gfxSkipChars that starts with kept chars
  gfxSkipCharsBuilder builder1;

  builder1.KeepChars(9);
  builder1.SkipChar();
  builder1.KeepChars(9);
  builder1.SkipChar();
  builder1.KeepChars(9);

  gfxSkipChars skipChars;
  skipChars.TakeFrom(&builder1);

  EXPECT_TRUE(skipChars.GetOriginalCharCount() == 29) <<
    "[1] Check length";

  gfxSkipCharsIterator iter(skipChars);

  EXPECT_TRUE(iter.GetOriginalOffset() == 0) <<
    "[2] Check initial original offset";
  EXPECT_TRUE(iter.GetSkippedOffset() == 0) <<
    "[3] Check initial skipped offset";

  uint32_t expectSkipped1[] =
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
     9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
    18, 19, 20, 21, 22, 23, 24, 25, 26, 27 };

  for (uint32_t i = 0; i < mozilla::ArrayLength(expectSkipped1); i++) {
    EXPECT_TRUE(iter.ConvertOriginalToSkipped(i) == expectSkipped1[i]) <<
      "[4] Check mapping of original to skipped for " << i;
  }

  uint32_t expectOriginal1[] =
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,
    10, 11, 12, 13, 14, 15, 16, 17, 18,
    20, 21, 22, 23, 24, 25, 26, 27, 28 };

  for (uint32_t i = 0; i < mozilla::ArrayLength(expectOriginal1); i++) {
    EXPECT_TRUE(iter.ConvertSkippedToOriginal(i) == expectOriginal1[i]) <<
      "[5] Check mapping of skipped to original for " << i;
  }

  // Test a gfxSkipChars that starts with skipped chars
  gfxSkipCharsBuilder builder2;

  builder2.SkipChars(9);
  builder2.KeepChar();
  builder2.SkipChars(9);
  builder2.KeepChar();
  builder2.SkipChars(9);

  skipChars.TakeFrom(&builder2);

  EXPECT_TRUE(skipChars.GetOriginalCharCount() == 29) <<
    "[6] Check length";

  gfxSkipCharsIterator iter2(skipChars);

  EXPECT_TRUE(iter2.GetOriginalOffset() == 0) <<
    "[7] Check initial original offset";
  EXPECT_TRUE(iter2.GetSkippedOffset() == 0) <<
    "[8] Check initial skipped offset";

  uint32_t expectSkipped2[] =
  {  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2 };

  for (uint32_t i = 0; i < mozilla::ArrayLength(expectSkipped2); i++) {
    EXPECT_TRUE(iter2.ConvertOriginalToSkipped(i) == expectSkipped2[i]) <<
      "[9] Check mapping of original to skipped for " << i;
  }

  uint32_t expectOriginal2[] = { 9, 19, 29 };

  for (uint32_t i = 0; i < mozilla::ArrayLength(expectOriginal2); i++) {
    EXPECT_TRUE(iter2.ConvertSkippedToOriginal(i) == expectOriginal2[i]) <<
      "[10] Check mapping of skipped to original for " << i;
  }

  return true;
}

TEST(Gfx, gfxSkipChars) {
  TestConstructor();
  TestLength();
  TestIterator();
}
