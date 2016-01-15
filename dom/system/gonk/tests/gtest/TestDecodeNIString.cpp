/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "GeolocationUtil.h"
#include "mozilla/UniquePtr.h"
#include "nsTArray.h"

using namespace mozilla;
using mozilla::UniquePtr;

TEST(GSMDefaultDecode, DecodeAlphabet) {
  #define NUM_DECODE_GSM_TEST 4
  const char* testArray[NUM_DECODE_GSM_TEST] = {
    "\x41\xE1\x90\x58\x34\x1E\x91",
    "\x49\xE5\x92\xD9\x74\x3E\xA1",
    "\x51\xE9\x94\x5A\xB5\x5E\xB1",
    "\x41\xE1\x90\x58\x34\x1E\x91"
    "\x49\xE5\x92\xD9\x74\x3E\xA1"
    "\x51\xE9\x94\x5A\xB5\x5E\xB1\x59\x2D"};
  nsTArray<nsString> result;
  result.AppendElement(NS_LITERAL_STRING("ABCDEFGH"));
  result.AppendElement(NS_LITERAL_STRING("IJKLMNOP"));
  result.AppendElement(NS_LITERAL_STRING("QRSTUVWX"));
  result.AppendElement(NS_LITERAL_STRING("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));


  for (int i = 0; i < NUM_DECODE_GSM_TEST ; i++) {
    nsString decodedStr;
    UniquePtr<char[]> byteArray = MakeUnique<char[]>(strlen(testArray[i])+1);
    strcpy(byteArray.get(), testArray[i]);
    ASSERT_TRUE(DecodeGsmDefaultToString(byteArray, decodedStr));
    ASSERT_TRUE(decodedStr == result[i]) <<
      "[test " << i << "]" << "The expected result is " <<
        NS_ConvertUTF16toUTF8(result[i]).get() << "(" <<
          "current:" << NS_ConvertUTF16toUTF8(decodedStr).get() << ")!";
  }
}

TEST(GSMDefaultDecode, DecodeNullptr) {
  UniquePtr<char[]> byteArray;
  nsString decodedStr;
  ASSERT_FALSE(DecodeGsmDefaultToString(byteArray, decodedStr));
}

TEST(DecodeNIString, DecodeUTF8) {
  #define NUM_DECODE_UTF8_TEST 4
  const char* testArray[NUM_DECODE_UTF8_TEST] = {
    "4142434445464748",
    "494A4B4C4D4E4F50",
    "5152535455565758",
    "4142434445464748494A4B4C4D4E4F505152535455565758595A"};
  nsTArray<nsString> result;
  result.AppendElement(NS_LITERAL_STRING("ABCDEFGH"));
  result.AppendElement(NS_LITERAL_STRING("IJKLMNOP"));
  result.AppendElement(NS_LITERAL_STRING("QRSTUVWX"));
  result.AppendElement(NS_LITERAL_STRING("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));


  for (int i = 0; i < NUM_DECODE_UTF8_TEST ; i++) {
    nsString decodedStr;
    decodedStr = DecodeNIString(testArray[i], GPS_ENC_SUPL_UTF8);
    ASSERT_TRUE(decodedStr == result[i]) <<
      "[test " << i << "]" << "The expected result is " <<
        NS_ConvertUTF16toUTF8(result[i]).get() << "(" <<
          "current:" << NS_ConvertUTF16toUTF8(decodedStr).get() << ")!";
  }
}

TEST(DecodeNIString, DecodeUSC2) {
  #define NUM_DECODE_USC2_TEST 4
  const char* testArray[NUM_DECODE_USC2_TEST] = {
    "FEFF00410042004300440045004600470048",
    "FEFF0049004A004B004C004D004E004F0050",
    "FEFF00510052005300540055005600570058",
    "FEFF004100420043004400450046004700480049004A004B004C004D004E00"
    "4F0050005100520053005400550056005700580059005A"};
  nsTArray<nsString> result;
  result.AppendElement(NS_LITERAL_STRING("ABCDEFGH"));
  result.AppendElement(NS_LITERAL_STRING("IJKLMNOP"));
  result.AppendElement(NS_LITERAL_STRING("QRSTUVWX"));
  result.AppendElement(NS_LITERAL_STRING("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));


  for (int i = 0; i < NUM_DECODE_USC2_TEST ; i++) {
    nsString decodedStr;
    decodedStr = DecodeNIString(testArray[i], GPS_ENC_SUPL_UCS2);
    ASSERT_TRUE(decodedStr == result[i]) <<
      "[test " << i << "]" << "The expected result is " <<
        NS_ConvertUTF16toUTF8(result[i]).get() << "(" <<
          "current:" << NS_ConvertUTF16toUTF8(decodedStr).get() << ")!";
  }
}

TEST(DecodeNIString, DecodeUnknown) {
  const char* testStr= {"4F0050005100520053005400550056005700580059005A"};
  nsString decodedStr;
  #define GPS_ENC_SUPL_UNKNOWN 0xFF
  decodedStr = DecodeNIString(testStr, GPS_ENC_SUPL_UNKNOWN);
  ASSERT_TRUE(decodedStr.EqualsLiteral("Failed to decode string")) <<
    "The expected result is Failed to decode string(current:" <<
      NS_ConvertUTF16toUTF8(decodedStr).get() << ")!";

}
