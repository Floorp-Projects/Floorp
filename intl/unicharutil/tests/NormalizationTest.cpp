/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "gtest/gtest.h"
#include "nsXPCOM.h"
#include "nsIUnicodeNormalizer.h"
#include "nsString.h"
#include "nsCharTraits.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/Sprintf.h"

struct testcaseLine {
  wchar_t* c1;
  wchar_t* c2;
  wchar_t* c3;
  wchar_t* c4;
  wchar_t* c5;
  char* description;
};

#ifdef DEBUG_smontagu
#define DEBUG_NAMED_TESTCASE(t, s) \
  printf(t ": "); \
  for (uint32_t i = 0; i < s.Length(); ++i) \
    printf("%x ", s.CharAt(i)); \
  printf("\n")
#else
#define DEBUG_NAMED_TESTCASE(t, s)
#endif

#define DEBUG_TESTCASE(x) DEBUG_NAMED_TESTCASE(#x, x)

#define NORMALIZE_AND_COMPARE(base, comparison, form, description) \
   normalized.Truncate();\
   normalizer->NormalizeUnicode##form(comparison, normalized);\
   DEBUG_NAMED_TESTCASE(#form "(" #comparison ")", normalized);\
   if (!base.Equals(normalized)) {\
     rv = false;\
     showError(description, #base " != " #form "(" #comparison ")\n");\
   }

NS_DEFINE_CID(kUnicodeNormalizerCID, NS_UNICODE_NORMALIZER_CID);

nsIUnicodeNormalizer *normalizer;

#include "NormalizationData.h"

void showError(const char* description, const char* errorText)
{
  printf("%s failed: %s", description, errorText);
}

bool TestInvariants(testcaseLine* testLine)
{
  nsAutoString c1, c2, c3, c4, c5, normalized;
  c1 = nsDependentString((char16_t*)testLine->c1);
  c2 = nsDependentString((char16_t*)testLine->c2);
  c3 = nsDependentString((char16_t*)testLine->c3);
  c4 = nsDependentString((char16_t*)testLine->c4);
  c5 = nsDependentString((char16_t*)testLine->c5);
  bool rv = true;
 
  /*
    1. The following invariants must be true for all conformant implementations

    NFC
      c2 ==  NFC(c1) ==  NFC(c2) ==  NFC(c3)
  */
  DEBUG_TESTCASE(c2);
  NORMALIZE_AND_COMPARE(c2, c1, NFC, testLine->description);  
  NORMALIZE_AND_COMPARE(c2, c2, NFC, testLine->description);
  NORMALIZE_AND_COMPARE(c2, c3, NFC, testLine->description);

  /*
      c4 ==  NFC(c4) ==  NFC(c5)
  */
  DEBUG_TESTCASE(c4);
  NORMALIZE_AND_COMPARE(c4, c4, NFC, testLine->description);
  NORMALIZE_AND_COMPARE(c4, c5, NFC, testLine->description);

  /*
    NFD
      c3 ==  NFD(c1) ==  NFD(c2) ==  NFD(c3)
  */
  DEBUG_TESTCASE(c3);
  NORMALIZE_AND_COMPARE(c3, c1, NFD, testLine->description);
  NORMALIZE_AND_COMPARE(c3, c2, NFD, testLine->description);
  NORMALIZE_AND_COMPARE(c3, c3, NFD, testLine->description);
  /*
      c5 ==  NFD(c4) ==  NFD(c5)
  */
  DEBUG_TESTCASE(c5);
  NORMALIZE_AND_COMPARE(c5, c4, NFD, testLine->description);
  NORMALIZE_AND_COMPARE(c5, c5, NFD, testLine->description);

  /*
    NFKC
      c4 == NFKC(c1) == NFKC(c2) == NFKC(c3) == NFKC(c4) == NFKC(c5)
  */
  DEBUG_TESTCASE(c4);
  NORMALIZE_AND_COMPARE(c4, c1, NFKC, testLine->description);
  NORMALIZE_AND_COMPARE(c4, c2, NFKC, testLine->description);
  NORMALIZE_AND_COMPARE(c4, c3, NFKC, testLine->description);
  NORMALIZE_AND_COMPARE(c4, c4, NFKC, testLine->description);
  NORMALIZE_AND_COMPARE(c4, c5, NFKC, testLine->description);

  /*
    NFKD
      c5 == NFKD(c1) == NFKD(c2) == NFKD(c3) == NFKD(c4) == NFKD(c5)
  */
  DEBUG_TESTCASE(c5);
  NORMALIZE_AND_COMPARE(c5, c1, NFKD, testLine->description);
  NORMALIZE_AND_COMPARE(c5, c2, NFKD, testLine->description);
  NORMALIZE_AND_COMPARE(c5, c3, NFKD, testLine->description);
  NORMALIZE_AND_COMPARE(c5, c4, NFKD, testLine->description);
  NORMALIZE_AND_COMPARE(c5, c5, NFKD, testLine->description);

  return rv;
}

uint32_t UTF32CodepointFromTestcase(testcaseLine* testLine)
{
  if (!IS_SURROGATE(testLine->c1[0]))
    return testLine->c1[0];

  NS_ASSERTION(NS_IS_HIGH_SURROGATE(testLine->c1[0]) &&
               NS_IS_LOW_SURROGATE(testLine->c1[1]),
               "Test data neither in BMP nor legal surrogate pair");
  return SURROGATE_TO_UCS4(testLine->c1[0], testLine->c1[1]);
}

bool TestUnspecifiedCodepoint(uint32_t codepoint)
{
  bool rv = true;
  char16_t unicharArray[3];
  nsAutoString X, normalized;

  if (IS_IN_BMP(codepoint)) {
    unicharArray[0] = codepoint;
    unicharArray[1] = 0;
    X = nsDependentString(unicharArray);
  }
  else {
    unicharArray[0] = H_SURROGATE(codepoint);
    unicharArray[1] = L_SURROGATE(codepoint);
    unicharArray[2] = 0;
    X = nsDependentString(unicharArray);
  }

  /*
 2. For every code point X assigned in this version of Unicode that is not specifically
    listed in Part 1, the following invariants must be true for all conformant
    implementations:

      X == NFC(X) == NFD(X) == NFKC(X) == NFKD(X)
  */
  static const size_t len = 9;
  char description[len];

  DEBUG_TESTCASE(X);
  snprintf(description, len, "U+%04X", codepoint);
  NORMALIZE_AND_COMPARE(X, X, NFC, description);
  NORMALIZE_AND_COMPARE(X, X, NFD, description);
  NORMALIZE_AND_COMPARE(X, X, NFKC, description);
  NORMALIZE_AND_COMPARE(X, X, NFKD, description);
  return rv;
}

void TestPart0()
{
  printf("Test Part0: Specific cases\n");

  uint32_t i = 0;
  uint32_t numFailed = 0;
  uint32_t numPassed = 0;

  while (Part0TestData[i].c1[0] != 0) {
    if (TestInvariants(&Part0TestData[i++]))
      ++numPassed;
    else
      ++numFailed;
  }
  printf(" %d cases passed, %d failed\n\n", numPassed, numFailed);
  EXPECT_EQ(0u, numFailed);
}

void TestPart1()
{
  printf("Test Part1: Character by character test\n");

  uint32_t i = 0;
  uint32_t numFailed = 0;
  uint32_t numPassed = 0;
  uint32_t codepoint;
  uint32_t testDataCodepoint = UTF32CodepointFromTestcase(&Part1TestData[i]);

  for (codepoint = 1; codepoint < 0x110000; ++codepoint) {
    if (testDataCodepoint == codepoint) {
      if (TestInvariants(&Part1TestData[i]))
        ++numPassed;
      else
        ++numFailed;
      testDataCodepoint = UTF32CodepointFromTestcase(&Part1TestData[++i]);
    } else {
      if (TestUnspecifiedCodepoint(codepoint))
        ++numPassed;
      else
        ++numFailed;
    }
  }
  printf(" %d cases passed, %d failed\n\n", numPassed, numFailed);
  EXPECT_EQ(0u, numFailed);
}

void TestPart2()
{
  printf("Test Part2: Canonical Order Test\n");

  uint32_t i = 0;
  uint32_t numFailed = 0;
  uint32_t numPassed = 0;

  while (Part2TestData[i].c1[0] != 0) {
    if (TestInvariants(&Part2TestData[i++]))
      ++numPassed;
    else
      ++numFailed;
  }
  printf(" %d cases passed, %d failed\n\n", numPassed, numFailed);
  EXPECT_EQ(0u, numFailed);
}

void TestPart3()
{
  printf("Test Part3: PRI #29 Test\n");

  uint32_t i = 0;
  uint32_t numFailed = 0;
  uint32_t numPassed = 0;

  while (Part3TestData[i].c1[0] != 0) {
    if (TestInvariants(&Part3TestData[i++]))
      ++numPassed;
    else
      ++numFailed;
  }
  printf(" %d cases passed, %d failed\n\n", numPassed, numFailed);
  EXPECT_EQ(0u, numFailed);
}

TEST(NormalizationTest, Main) {
  if (sizeof(wchar_t) != 2) {
    printf("This test can only be run where sizeof(wchar_t) == 2\n");
    return;
  }
  if (strlen(versionText) == 0) {
    printf("No testcases: to run the tests generate the header file using\n");
    printf(" perl genNormalizationData.pl\n");
    printf("in intl/unichar/tools and rebuild\n");
    return;
  }

  printf("NormalizationTest: test nsIUnicodeNormalizer. UCD version: %s\n", 
         versionText); 

  normalizer = nullptr;
  nsresult res;
  res = CallGetService(kUnicodeNormalizerCID, &normalizer);
  
  ASSERT_FALSE(NS_FAILED(res)) << "GetService failed";
  ASSERT_NE(nullptr, normalizer);

  TestPart0();
  TestPart1();
  TestPart2();
  TestPart3();
  
  NS_RELEASE(normalizer);
}
