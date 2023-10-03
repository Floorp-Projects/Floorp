/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "gtest/gtest.h"
#include "mozilla/intl/LineBreaker.h"
#include "mozilla/intl/WordBreaker.h"
#include "mozilla/Preferences.h"
#include "mozilla/Span.h"
#include "nsISupports.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsXPCOM.h"

using mozilla::intl::LineBreaker;
using mozilla::intl::WordBreaker;

// Turn off clang-format to align the ruler comments to the test strings.

// clang-format off
static char teng0[] =
  //           1         2         3         4         5         6         7
  // 01234567890123456789012345678901234567890123456789012345678901234567890123456789
    "hello world";
// clang-format on

static uint32_t lexp0[] = {5, 11};

static uint32_t wexp0[] = {5, 6, 11};

// clang-format off
static char teng1[] =
  //           1         2         3         4         5         6         7
  // 01234567890123456789012345678901234567890123456789012345678901234567890123456789
    "This is a test to test(reasonable) line    break. This 0.01123 = 45 x 48.";
// clang-format on

static uint32_t lexp1[] = {4,  7,  9,  14, 17, 34, 39, 40, 41,
                           42, 49, 54, 62, 64, 67, 69, 73};

static uint32_t wexp1[] = {4,  5,  7,  8,  9,  10, 14, 15, 17, 18, 22, 23,
                           33, 34, 35, 39, 43, 48, 49, 50, 54, 55, 56, 57,
                           62, 63, 64, 65, 67, 68, 69, 70, 72, 73};

// clang-format off
static char teng2[] =
  //           1         2         3         4         5         6         7
  // 01234567890123456789012345678901234567890123456789012345678901234567890123456789
    "()((reasonab(l)e) line  break. .01123=45x48.";
// clang-format on

static uint32_t lexp2[] = {17, 22, 23, 30, 44};

static uint32_t wexp2[] = {4,  12, 13, 14, 15, 16, 17, 18, 22,
                           24, 29, 30, 31, 32, 37, 38, 43, 44};

// clang-format off
static char teng3[] =
  //           1         2         3         4         5         6         7
  // 01234567890123456789012345678901234567890123456789012345678901234567890123456789
    "It's a test to test(ronae ) line break....";
// clang-format on

static uint32_t lexp3[] = {4, 6, 11, 14, 25, 27, 32, 42};

static uint32_t wexp3[] = {2,  3,  4,  5,  6,  7,  11, 12, 14, 15,
                           19, 20, 25, 26, 27, 28, 32, 33, 38, 42};

static char ruler1[] =
    "          1         2         3         4         5         6         7  ";
static char ruler2[] =
    "0123456789012345678901234567890123456789012345678901234567890123456789012";

bool Check(const char* in, mozilla::Span<const uint32_t> out,
           mozilla::Span<const uint32_t> res) {
  const uint32_t outlen = out.Length();
  const uint32_t i = res.Length();
  bool ok = true;

  if (i != outlen) {
    ok = false;
    printf("WARNING!!! return size wrong, expect %d but got %d \n", outlen, i);
  }

  for (uint32_t j = 0; j < i; j++) {
    if (j < outlen) {
      if (res[j] != out[j]) {
        ok = false;
        printf("[%d] expect %d but got %d\n", j, out[j], res[j]);
      }
    } else {
      ok = false;
      printf("[%d] additional %d\n", j, res[j]);
    }
  }

  if (!ok) {
    printf("string  = \n%s\n", in);
    printf("%s\n", ruler1);
    printf("%s\n", ruler2);

    printf("Expect = \n");
    for (uint32_t j = 0; j < outlen; j++) {
      printf("%d,", out[j]);
    }

    printf("\nResult = \n");
    for (uint32_t j = 0; j < i; j++) {
      printf("%d,", res[j]);
    }
    printf("\n");
  }

  return ok;
}

bool TestASCIILB(const char* in, mozilla::Span<const uint32_t> out) {
  NS_ConvertASCIItoUTF16 input(in);
  EXPECT_GT(input.Length(), 0u) << "Expect a non-empty input!";

  nsTArray<uint32_t> result;
  int32_t curr = 0;
  while (true) {
    curr = LineBreaker::Next(input.get(), input.Length(), curr);
    if (curr == NS_LINEBREAKER_NEED_MORE_TEXT) {
      break;
    }
    result.AppendElement(curr);
  }

  return Check(in, out, result);
}

bool TestASCIIWB(const char* in, mozilla::Span<const uint32_t> out) {
  NS_ConvertASCIItoUTF16 input(in);
  EXPECT_GT(input.Length(), 0u) << "Expect a non-empty input!";

  nsTArray<uint32_t> result;
  int32_t curr = 0;
  while (true) {
    curr = WordBreaker::Next(input.get(), input.Length(), curr);
    if (curr == NS_WORDBREAKER_NEED_MORE_TEXT) {
      break;
    }
    result.AppendElement(curr);
  }

  return Check(in, out, result);
}

TEST(LineBreak, LineBreaker)
{
  ASSERT_TRUE(TestASCIILB(teng0, lexp0));
  ASSERT_TRUE(TestASCIILB(teng1, lexp1));
  ASSERT_TRUE(TestASCIILB(teng2, lexp2));
  ASSERT_TRUE(TestASCIILB(teng3, lexp3));
}

TEST(WordBreak, WordBreaker)
{
  ASSERT_TRUE(TestASCIIWB(teng0, wexp0));
  ASSERT_TRUE(TestASCIIWB(teng1, wexp1));
  ASSERT_TRUE(TestASCIIWB(teng2, wexp2));
  ASSERT_TRUE(TestASCIIWB(teng3, wexp3));
}

//                         012345678901234
static const char wb0[] = "T";
static const char wb1[] = "h";
static const char wb2[] = "";
static const char wb3[] = "is   is a int";
static const char wb4[] = "";
static const char wb5[] = "";
static const char wb6[] = "ernationali";
static const char wb7[] = "zation work.";

static const char* wb[] = {wb0, wb1, wb2, wb3, wb4, wb5, wb6, wb7};

TEST(WordBreak, TestPrintWordWithBreak)
{
  uint32_t numOfFragment = sizeof(wb) / sizeof(char*);

  // This test generate the result string by appending '^' at every word break
  // opportunity except the one at end of the text.
  nsAutoString result;

  for (uint32_t i = 0; i < numOfFragment; i++) {
    NS_ConvertASCIItoUTF16 fragText(wb[i]);

    int32_t cur = 0;
    cur = WordBreaker::Next(fragText.get(), fragText.Length(), cur);
    uint32_t start = 0;
    while (cur != NS_WORDBREAKER_NEED_MORE_TEXT) {
      result.Append(Substring(fragText, start, cur - start));

      // Append '^' only if cur is within the fragText. We'll check the word
      // break opportunity between fragText and nextFragText using
      // BreakInBetween() below.
      if (cur < static_cast<int32_t>(fragText.Length())) {
        result.Append('^');
      }
      start = (cur >= 0 ? cur : cur - start);
      cur = WordBreaker::Next(fragText.get(), fragText.Length(), cur);
    }

    if (i != numOfFragment - 1) {
      NS_ConvertASCIItoUTF16 nextFragText(wb[i + 1]);
      if (nextFragText.IsEmpty()) {
        // If nextFragText is empty, there's no new possible word break
        // opportunity.
        continue;
      }

      const auto origFragLen = static_cast<int32_t>(fragText.Length());
      fragText.Append(nextFragText);

      bool canBreak =
          origFragLen ==
          WordBreaker::Next(fragText.get(), fragText.Length(), origFragLen - 1);
      if (canBreak) {
        result.Append('^');
      }
    }
  }
  ASSERT_STREQ("This^   ^is^ ^a^ ^internationalization^ ^work^.",
               NS_ConvertUTF16toUTF8(result).get());
}

// This function searches a complete word starting from |offset| in wb[fragN].
// If it reaches the end of wb[fragN], and there is no word break opportunity
// between wb[fragN] and wb[fragN+1], it will continue the search in wb[fragN+1]
// until a word break.
void TestFindWordBreakFromPosition(uint32_t fragN, uint32_t offset,
                                   const char* expected) {
  uint32_t numOfFragment = sizeof(wb) / sizeof(char*);

  NS_ConvertASCIItoUTF16 fragText(wb[fragN]);

  mozilla::intl::WordRange res = WordBreaker::FindWord(fragText, offset);

  nsAutoString result(Substring(fragText, res.mBegin, res.mEnd - res.mBegin));

  if ((uint32_t)fragText.Length() <= res.mEnd) {
    // if we hit the end of the fragment
    nsAutoString curFragText = fragText;
    for (uint32_t p = fragN + 1; p < numOfFragment; p++) {
      NS_ConvertASCIItoUTF16 nextFragText(wb[p]);
      if (nextFragText.IsEmpty()) {
        // If nextFragText is empty, there's no new possible word break
        // opportunity between curFragText and nextFragText.
        continue;
      }

      const auto origFragLen = static_cast<int32_t>(curFragText.Length());
      curFragText.Append(nextFragText);
      bool canBreak = origFragLen == WordBreaker::Next(curFragText.get(),
                                                       curFragText.Length(),
                                                       origFragLen - 1);
      if (canBreak) {
        break;
      }
      mozilla::intl::WordRange r = WordBreaker::FindWord(nextFragText, 0);

      result.Append(Substring(nextFragText, r.mBegin, r.mEnd - r.mBegin));

      if ((uint32_t)nextFragText.Length() != r.mEnd) {
        break;
      }
    }
  }

  ASSERT_STREQ(expected, NS_ConvertUTF16toUTF8(result).get())
      << "FindWordBreakFromPosition(" << fragN << ", " << offset << ")";
}

TEST(WordBreak, TestNextWordBreakWithComplexLanguage)
{
  nsString fragText(u"\u0e40\u0e1b\u0e47\u0e19\u0e19\u0e31\u0e01");

  int32_t offset = 0;
  while (offset != NS_WORDBREAKER_NEED_MORE_TEXT) {
    int32_t newOffset =
        WordBreaker::Next(fragText.get(), fragText.Length(), offset);
    ASSERT_NE(offset, newOffset);
    offset = newOffset;
  }
  ASSERT_TRUE(true);
}

TEST(WordBreak, TestFindWordWithEmptyString)
{
  mozilla::intl::WordRange expect{0, 0};
  mozilla::intl::WordRange result = WordBreaker::FindWord(EmptyString(), 0);
  ASSERT_EQ(expect.mBegin, result.mBegin);
  ASSERT_EQ(expect.mEnd, result.mEnd);
}

TEST(WordBreak, TestNextWordBreakWithEmptyString)
{
  char16_t empty[] = {};
  ASSERT_EQ(NS_WORDBREAKER_NEED_MORE_TEXT, WordBreaker::Next(empty, 0, 0));
  ASSERT_EQ(NS_WORDBREAKER_NEED_MORE_TEXT, WordBreaker::Next(empty, 0, 1));
}

TEST(WordBreak, TestFindWordBreakFromPosition)
{
  TestFindWordBreakFromPosition(0, 0, "This");
  TestFindWordBreakFromPosition(1, 0, "his");
  TestFindWordBreakFromPosition(2, 0, "is");
  TestFindWordBreakFromPosition(3, 0, "is");
  TestFindWordBreakFromPosition(3, 1, "is");
  TestFindWordBreakFromPosition(3, 9, " ");
  TestFindWordBreakFromPosition(3, 10, "internationalization");
  TestFindWordBreakFromPosition(4, 0, "ernationalization");
  TestFindWordBreakFromPosition(5, 0, "ernationalization");
  TestFindWordBreakFromPosition(6, 4, "ernationalization");
  TestFindWordBreakFromPosition(6, 8, "ernationalization");
  TestFindWordBreakFromPosition(7, 6, " ");
  TestFindWordBreakFromPosition(7, 7, "work");
}

// Test for StopAtPunctuation option.
TEST(WordBreak, TestFindBreakWithStopAtPunctuation)
{
  bool original =
      mozilla::Preferences::GetBool("intl.icu4x.segmenter.enabled", true);

  // Not UAX#29 rule
  mozilla::Preferences::SetBool("intl.icu4x.segmenter.enabled", false);

  nsString fragText(u"one.two");

  mozilla::intl::WordRange result1 = WordBreaker::FindWord(fragText, 0);
  ASSERT_EQ(0u, result1.mBegin);
  ASSERT_EQ(3u, result1.mEnd);
  mozilla::intl::WordRange result2 = WordBreaker::FindWord(fragText, 3);
  ASSERT_EQ(3u, result2.mBegin);
  ASSERT_EQ(4u, result2.mEnd);
  mozilla::intl::WordRange result3 = WordBreaker::FindWord(fragText, 4);
  ASSERT_EQ(4u, result3.mBegin);
  ASSERT_EQ(7u, result3.mEnd);

  // UAX#29 rule
  mozilla::Preferences::SetBool("intl.icu4x.segmenter.enabled", true);

  mozilla::intl::WordRange result4 = WordBreaker::FindWord(
      fragText, 0, WordBreaker::FindWordOptions::StopAtPunctuation);
  ASSERT_EQ(0u, result4.mBegin);
  ASSERT_EQ(3u, result4.mEnd);
  mozilla::intl::WordRange result5 = WordBreaker::FindWord(
      fragText, 3, WordBreaker::FindWordOptions::StopAtPunctuation);
  ASSERT_EQ(3u, result5.mBegin);
  ASSERT_EQ(4u, result5.mEnd);
  mozilla::intl::WordRange result6 = WordBreaker::FindWord(
      fragText, 4, WordBreaker::FindWordOptions::StopAtPunctuation);
  ASSERT_EQ(4u, result6.mBegin);
  ASSERT_EQ(7u, result6.mEnd);

  // Default (without StopAtPunctuation)
  mozilla::intl::WordRange result7 = WordBreaker::FindWord(fragText, 0);
  ASSERT_EQ(0u, result7.mBegin);
  ASSERT_EQ(7u, result7.mEnd);
  mozilla::intl::WordRange result8 = WordBreaker::FindWord(fragText, 3);
  ASSERT_EQ(0u, result8.mBegin);
  ASSERT_EQ(7u, result8.mEnd);
  mozilla::intl::WordRange result9 = WordBreaker::FindWord(fragText, 4);
  ASSERT_EQ(0u, result9.mBegin);
  ASSERT_EQ(7u, result9.mEnd);

  mozilla::Preferences::SetBool("intl.icu4x.segmenter.enabled", original);
}
