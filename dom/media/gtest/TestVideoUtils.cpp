/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsString.h"
#include "VideoUtils.h"

using namespace mozilla;

TEST(StringListRange, MakeStringListRange)
{
  static const struct
  {
    const char* mList;
    const char* mExpected;
  } tests[] =
  {
    { "", "" },
    { " ", "" },
    { ",", "" },
    { " , ", "" },
    { "a", "a|" },
    { "  a  ", "a|" },
    { "aa,bb", "aa|bb|" },
    { " a a ,  b b  ", "a a|b b|" },
    { " , ,a 1,,  ,b  2,", "a 1|b  2|" }
  };

  for (const auto& test : tests) {
    nsCString list(test.mList);
    nsCString out;
    for (const auto& item : MakeStringListRange(list)) {
      out += item;
      out += "|";
    }
    EXPECT_STREQ(test.mExpected, out.Data());
  }
}

TEST(StringListRange, StringListContains)
{
  static const struct
  {
    const char* mList;
    const char* mItemToSearch;
    bool mExpected;
  } tests[] =
  {
    { "", "", false },
    { "", "a", false },
    { " ", "a", false },
    { ",", "a", false },
    { " , ", "", false },
    { " , ", "a", false },
    { "a", "a", true },
    { "a", "b", false },
    { "  a  ", "a", true },
    { "aa,bb", "aa", true },
    { "aa,bb", "bb", true },
    { "aa,bb", "cc", false },
    { "aa,bb", " aa ", false },
    { " a a ,  b b  ", "a a", true },
    { " , ,a 1,,  ,b  2,", "a 1", true },
    { " , ,a 1,,  ,b  2,", "b  2", true },
    { " , ,a 1,,  ,b  2,", "", false },
    { " , ,a 1,,  ,b  2,", " ", false },
    { " , ,a 1,,  ,b  2,", "A 1", false },
    { " , ,A 1,,  ,b  2,", "a 1", false }
  };

  for (const auto& test : tests) {
    nsCString list(test.mList);
    nsCString itemToSearch(test.mItemToSearch);
    EXPECT_EQ(test.mExpected, StringListContains(list, itemToSearch))
      << "trying to find \"" << itemToSearch.Data()
      << "\" in \"" << list.Data() << "\"";
  }
}
