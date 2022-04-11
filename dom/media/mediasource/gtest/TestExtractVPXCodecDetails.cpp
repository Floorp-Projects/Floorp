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
  const char* const mComment;
};

TEST(ExtractVPXCodecDetails, TestInputData)
{
  TestData tests[] = {
      // <sample entry 4CC>.<profile>.<level>.<bitDepth>.<chromaSubsampling>.
      // <colourPrimaries>.<transferCharacteristics>.<matrixCoefficients>.
      // <videoFullRangeFlag>

      // Format checks
      {u"vp09.0.10.8", true, "Valid minimum length"},
      {u"vp9.00.10.08", false, "Invalid 4CC"},
      {u"vp09.00..08", false, "Blank field"},
      {u"vp09", false, "0 of 3 required fields"},
      {u"vp09.00", false, "1 of 3 required fields"},
      {u"vp09.00.10", false, "2 of 3 required fields"},

      // Profiles
      {u"vp09.00.10.08", true, "Profile 0"},
      {u"vp09.01.10.08", true, "Profile 1"},
      {u"vp09.02.10.10", true, "Profile 2"},
      {u"vp09.03.10.10", true, "Profile 3"},
      {u"vp09.-1.10.08", false, "Invalid profile < 0"},
      {u"vp09.04.10.08", false, "Invalid profile > 3"},

      // Levels
      {u"vp09.00.11.08", true, "Level 1.1"},
      {u"vp09.00.12.08", false, "Invalid level 1.2"},
      {u"vp09.00.52.08", true, "Level 5.2"},
      {u"vp09.00.64.08", false, "Level greater than max"},

      // Bit depths
      // - 8-bit tested in Profiles section
      // - 10-bit tested in Profiles section
      {u"vp09.02.10.12", true, "12-bit"},
      {u"vp09.00.10.07", false, "Invalid, 7-bit"},
      {u"vp09.02.10.11", false, "Invalid, 11-bit"},
      {u"vp09.02.10.13", false, "Invalid, 13-bit"},

      // Chroma subsampling
      {u"vp09.00.10.08.00", true, "4:2:0 vertical"},
      {u"vp09.00.10.08.01", true, "4:2:0 colocated"},
      {u"vp09.00.10.08.02", true, "4:2:2"},
      {u"vp09.00.10.08.03", true, "4:4:4"},
      {u"vp09.00.10.08.04", false, "Invalid chroma"},

      // Color primaries
      {u"vp09.00.10.08.01.00", false, "CP 0: Reserved"},
      {u"vp09.00.10.08.01.01", true, "CP 1: BT.709"},
      {u"vp09.00.10.08.01.03", false, "CP 3: Reserved"},
      {u"vp09.00.10.08.01.09", true, "CP 9: BT.2020"},
      {u"vp09.00.10.08.01.21", false, "CP 21: Reserved"},
      {u"vp09.00.10.08.01.22", true, "CP 22: EBU Tech 3213"},
      {u"vp09.00.10.08.01.23", false, "CP 23: Out of range"},

      // Transfer characteristics
      {u"vp09.00.10.08.01.01.00", false, "TC 0: Reserved"},
      {u"vp09.00.10.08.01.01.01", true, "TC 1: BT.709"},
      {u"vp09.00.10.08.01.01.03", false, "TC 3: Reserved"},
      {u"vp09.00.10.08.01.09.16", true, "TC 16: ST 2084"},
      {u"vp09.00.10.08.01.09.19", false, "TC 19: Out of range"},

      // Matrix coefficients
      {u"vp09.00.10.08.03.09.16.00", true, "MC 0: Identity"},
      {u"vp09.00.10.08.01.09.16.00", false, "MC 0: Identity without 4:4:4"},
      {u"vp09.00.10.08.01.09.16.01", true, "MC 1: BT.709"},
      {u"vp09.00.10.08.01.09.16.03", false, "MC 3: Reserved"},
      {u"vp09.00.10.08.01.09.16.09", true, "MC 9: BT.2020"},
      {u"vp09.00.10.08.01.09.16.15", false, "MC 15: Out of range"},

      // Color range
      {u"vp09.00.10.08.01.09.16.09.00", true, "Limited range"},
      {u"vp09.00.10.08.01.09.16.09.01", true, "Full range"},
      {u"vp09.00.10.08.01.09.16.09.02", false, "Invalid range value"},

      {u"vp09.00.10.08.01.09.16.09.00.", false, "Extra ."},
      {u"vp09.00.10.08.01.09.16.09.00.00", false, "More than 9 fields"},
  };

  for (const auto& data : tests) {
    uint8_t profile = 0;
    uint8_t level = 0;
    uint8_t bitDepth = 0;
    bool result = ExtractVPXCodecDetails(nsString(data.mCodecParameterString),
                                         profile, level, bitDepth);
    EXPECT_EQ(result, data.mExpectedValue)
        << NS_ConvertUTF16toUTF8(data.mCodecParameterString).get() << " ("
        << data.mComment << ")";
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
  EXPECT_EQ(colorSpace.mPrimaries, gfx::CICP::CP_BT709);
  EXPECT_EQ(colorSpace.mTransfer, gfx::CICP::TC_BT709);
  EXPECT_EQ(colorSpace.mMatrix, gfx::CICP::MC_BT709);
  EXPECT_EQ(colorSpace.mRange, gfx::ColorRange::LIMITED);

  data = u"vp09.02.10.10.01.09.16.09.01";
  result = ExtractVPXCodecDetails(nsString(data), profile, level, bitDepth,
                                  chromaSubsampling, colorSpace);
  EXPECT_EQ(result, true);
  EXPECT_EQ(profile, 2);
  EXPECT_EQ(level, 10);
  EXPECT_EQ(bitDepth, 10);
  EXPECT_EQ(chromaSubsampling, 1);
  EXPECT_EQ(colorSpace.mPrimaries, gfx::CICP::CP_BT2020);
  EXPECT_EQ(colorSpace.mTransfer, gfx::CICP::TC_SMPTE2084);
  EXPECT_EQ(colorSpace.mMatrix, gfx::CICP::MC_BT2020_NCL);
  EXPECT_EQ(colorSpace.mRange, gfx::ColorRange::FULL);
}
