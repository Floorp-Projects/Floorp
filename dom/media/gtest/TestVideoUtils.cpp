/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsString.h"
#include "VideoUtils.h"

using namespace mozilla;

TEST(MediaMIMETypes, IsMediaMIMEType)
{
  EXPECT_TRUE(IsMediaMIMEType("audio/mp4"));
  EXPECT_TRUE(IsMediaMIMEType("video/mp4"));
  EXPECT_TRUE(IsMediaMIMEType("application/x-mp4"));

  EXPECT_TRUE(IsMediaMIMEType("audio/m"));
  EXPECT_FALSE(IsMediaMIMEType("audio/"));

  EXPECT_FALSE(IsMediaMIMEType("vide/mp4"));
  EXPECT_FALSE(IsMediaMIMEType("videos/mp4"));

  // Expect lowercase only.
  EXPECT_FALSE(IsMediaMIMEType("Video/mp4"));
}

TEST(StringListRange, MakeStringListRange)
{
  static const struct
  {
    const char* mList;
    const char* mExpectedSkipEmpties;
    const char* mExpectedProcessAll;
    const char* mExpectedProcessEmpties;
  } tests[] =
  { // string              skip         all               empties
    { "",                  "",          "|",              "" },
    { " ",                 "",          "|",              "|" },
    { ",",                 "",          "||",             "||" },
    { " , ",               "",          "||",             "||" },
    { "a",                 "a|",        "a|",             "a|" },
    { "  a  ",             "a|",        "a|",             "a|" },
    { "a,",                "a|",        "a||",            "a||" },
    { "a, ",               "a|",        "a||",            "a||" },
    { ",a",                "a|",        "|a|",            "|a|" },
    { " ,a",               "a|",        "|a|",            "|a|" },
    { "aa,bb",             "aa|bb|",    "aa|bb|",         "aa|bb|" },
    { " a a ,  b b  ",     "a a|b b|",  "a a|b b|",       "a a|b b|" },
    { " , ,a 1,,  ,b  2,", "a 1|b  2|", "||a 1|||b  2||", "||a 1|||b  2||" }
  };

  for (const auto& test : tests) {
    nsCString list(test.mList);
    nsCString out;
    for (const auto& item : MakeStringListRange(list)) {
      out += item;
      out += "|";
    }
    EXPECT_STREQ(test.mExpectedSkipEmpties, out.Data());
    out.SetLength(0);

    for (const auto& item :
         MakeStringListRange<StringListRangeEmptyItems::ProcessAll>(list)) {
      out += item;
      out += "|";
    }
    EXPECT_STREQ(test.mExpectedProcessAll, out.Data());
    out.SetLength(0);

    for (const auto& item :
         MakeStringListRange<StringListRangeEmptyItems::ProcessEmptyItems>(list)) {
      out += item;
      out += "|";
    }
    EXPECT_STREQ(test.mExpectedProcessEmpties, out.Data());
  }
}

TEST(StringListRange, StringListContains)
{
  static const struct
  {
    const char* mList;
    const char* mItemToSearch;
    bool mExpectedSkipEmpties;
    bool mExpectedProcessAll;
    bool mExpectedProcessEmpties;
  } tests[] =
  { // haystack            needle  skip   all    empties
    { "",                  "",     false, true,  false },
    { " ",                 "",     false, true,  true  },
    { "",                  "a",    false, false, false },
    { " ",                 "a",    false, false, false },
    { ",",                 "a",    false, false, false },
    { " , ",               "",     false, true,  true  },
    { " , ",               "a",    false, false, false },
    { "a",                 "a",    true,  true,  true  },
    { "a",                 "b",    false, false, false },
    { "  a  ",             "a",    true,  true,  true  },
    { "aa,bb",             "aa",   true,  true,  true  },
    { "aa,bb",             "bb",   true,  true,  true  },
    { "aa,bb",             "cc",   false, false, false },
    { "aa,bb",             " aa ", false, false, false },
    { " a a ,  b b  ",     "a a",  true,  true,  true  },
    { " , ,a 1,,  ,b  2,", "a 1",  true,  true,  true  },
    { " , ,a 1,,  ,b  2,", "b  2", true,  true,  true  },
    { " , ,a 1,,  ,b  2,", "",     false, true,  true  },
    { " , ,a 1,,  ,b  2,", " ",    false, false, false },
    { " , ,a 1,,  ,b  2,", "A 1",  false, false, false },
    { " , ,A 1,,  ,b  2,", "a 1",  false, false, false }
  };

  for (const auto& test : tests) {
    nsCString list(test.mList);
    nsCString itemToSearch(test.mItemToSearch);
    EXPECT_EQ(test.mExpectedSkipEmpties, StringListContains(list, itemToSearch))
      << "trying to find \"" << itemToSearch.Data()
      << "\" in \"" << list.Data() << "\" (skipping empties)";
    EXPECT_EQ(test.mExpectedProcessAll,
              StringListContains<StringListRangeEmptyItems::ProcessAll>
                                (list, itemToSearch))
      << "trying to find \"" << itemToSearch.Data()
      << "\" in \"" << list.Data() << "\" (processing everything)";
    EXPECT_EQ(test.mExpectedProcessEmpties,
              StringListContains<StringListRangeEmptyItems::ProcessEmptyItems>
                                (list, itemToSearch))
      << "trying to find \"" << itemToSearch.Data()
      << "\" in \"" << list.Data() << "\" (processing empties)";
  }
}
