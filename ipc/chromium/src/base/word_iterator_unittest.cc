// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/word_iterator.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(WordIteratorTest, BreakWord) {
  std::wstring str(L" foo bar! \npouet boom");
  WordIterator iter(str, WordIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(L" ", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(L"foo", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(L" ", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(L"bar", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(L"!", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(L" ", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(L"\n", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(L"pouet", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(L" ", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(L"boom", iter.GetWord());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}

TEST(WordIteratorTest, BreakLine) {
  std::wstring str(L" foo bar! \npouet boom");
  WordIterator iter(str, WordIterator::BREAK_LINE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(L" ", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(L"foo ", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(L"bar! \n", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(L"pouet ", iter.GetWord());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(L"boom", iter.GetWord());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}
