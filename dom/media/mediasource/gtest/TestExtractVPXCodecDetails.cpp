/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtest/gtest.h>
#include <stdint.h>

#include "VideoUtils.h"

using namespace mozilla;

struct TestData {
  const char16_t* const mCodecParameterString;
  const bool mExpectedValue;
};

TEST(ExtractVPXCodecDetails, TestDataLength)
{
  TestData tests[] = {
      {u"vp09.00.11.08", true},     // valid case
      {u"vp09.00.11.08.00", true},  // valid case, have extra optional field
      {u"vp09.02.10.10.01.09.16.09.01", true},  // maximum length valid case
      {u"vp09", false},                         // lack of mandatory fields
      {u"vp09.00", false},                      // lack of mandatory fields
      {u"vp09.00.11", false},                   // lack of mandatory fields
      {u"vp09.02.10.10.01.09.16.09.01.00",
       false}  // more than 9 fields, invalid case.
  };

  for (const auto& data : tests) {
    uint8_t profile = 0;
    uint8_t level = 0;
    uint8_t bitDepth = 0;
    bool result = ExtractVPXCodecDetails(nsString(data.mCodecParameterString),
                                         profile, level, bitDepth);
    EXPECT_EQ(result, data.mExpectedValue)
        << NS_ConvertUTF16toUTF8(data.mCodecParameterString).get();
  }
}

TEST(ExtractVPXCodecDetails, TestInputData)
{
  TestData tests[] = {
      {u"vp09.02..08", false},       // malformed
      {u"vp9.02.10.08", false},      // invalid 4CC
      {u"vp09.03.11.08", false},     // profile should < 3
      {u"vp09.00.63.08.00", false},  // invalid level
      {u"vp09.02.10.13", false},     // invalid bitDepth
      {u"vp09.02.10.10.04", false},  // invalid chromasubsampling, should < 4
      {u"vp09.02.10.10.01.00",
       false},  // invalid Colour primaries, should not be 0,3 or < 23.
      {u"vp09.02.10.10.01.03", false},  // invalid Colour primaries.
      {u"vp09.02.10.10.01.23", false},  // invalid Colour primaries.
      {u"vp09.02.10.10.01.09.00",
       false},  // invalid Transfer characteristics, should not be 0,3 or < 19.
      {u"vp09.02.10.10.01.09.03", false},  // invalid Transfer characteristics.
      {u"vp09.02.10.10.01.09.19", false},  // invalid Transfer characteristics.
      {u"vp09.02.10.10.01.09.16.12",
       false},  // invalid Matrix coefficients, should not be 3 or < 12.
      {u"vp09.02.10.10.01.09.16.03", false},     // invalid matrix.
      {u"vp09.02.10.10.01.09.16.09.02", false},  // invalid range, should < 2.
      // Test if matrixCoefficients is 0 (RGB), then chroma subsampling MUST be
      // 3 (4:4:4).
      {u"vp09.02.10.08.03.09.16.00.00", true}  // invalid combination.
  };

  for (const auto& data : tests) {
    uint8_t profile = 0;
    uint8_t level = 0;
    uint8_t bitDepth = 0;
    bool result = ExtractVPXCodecDetails(nsString(data.mCodecParameterString),
                                         profile, level, bitDepth);
    EXPECT_EQ(result, data.mExpectedValue)
        << NS_ConvertUTF16toUTF8(data.mCodecParameterString).get();
  }
}

TEST(ExtractVPXCodecDetails, TestParsingOutput)
{
  uint8_t profile = 0;
  uint8_t level = 0;
  uint8_t bitDepth = 0;
  uint8_t chromaSubsampling = 0;
  VideoColorSpace colorSpace;
  auto data = u"vp09.01.11.08";
  bool result = ExtractVPXCodecDetails(nsString(data), profile, level, bitDepth,
                                       chromaSubsampling, colorSpace);
  EXPECT_EQ(result, true);
  EXPECT_EQ(profile, 1);
  EXPECT_EQ(level, 11);
  EXPECT_EQ(bitDepth, 8);
  // Should keep spec defined default value.
  EXPECT_EQ(chromaSubsampling, 1);
  EXPECT_EQ(colorSpace.mPrimaryId, 1);
  EXPECT_EQ(colorSpace.mTransferId, 1);
  EXPECT_EQ(colorSpace.mMatrixId, 1);
  EXPECT_EQ(colorSpace.mRangeId, 0);

  data = u"vp09.02.10.10.01.09.16.09.01";
  result = ExtractVPXCodecDetails(nsString(data), profile, level, bitDepth,
                                  chromaSubsampling, colorSpace);
  EXPECT_EQ(result, true);
  EXPECT_EQ(profile, 2);
  EXPECT_EQ(level, 10);
  EXPECT_EQ(bitDepth, 10);
  EXPECT_EQ(chromaSubsampling, 1);
  EXPECT_EQ(colorSpace.mPrimaryId, 9);
  EXPECT_EQ(colorSpace.mTransferId, 16);
  EXPECT_EQ(colorSpace.mMatrixId, 9);
  EXPECT_EQ(colorSpace.mRangeId, 1);
}
