/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DecoderBenchmark.h"

#include "gmock/gmock.h"
#include "gtest/gtest-printers.h"
#include "gtest/gtest.h"

using ::testing::Return;
using namespace mozilla;

TEST(DecoderBenchmark, CreateKey)
{
  DecoderBenchmarkInfo info{NS_LITERAL_CSTRING("video/av1"), 1, 1, 1, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info),
            NS_LITERAL_CSTRING("ResolutionLevel0-FrameRateLevel0-8bit"))
      << "Min level";

  DecoderBenchmarkInfo info1{NS_LITERAL_CSTRING("video/av1"), 5000, 5000, 100,
                             8};
  EXPECT_EQ(KeyUtil::CreateKey(info1),
            NS_LITERAL_CSTRING("ResolutionLevel7-FrameRateLevel4-8bit"))
      << "Max level";

  DecoderBenchmarkInfo info2{NS_LITERAL_CSTRING("video/av1"), 854, 480, 30, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info2),
            NS_LITERAL_CSTRING("ResolutionLevel3-FrameRateLevel2-8bit"))
      << "On the top of 4th resolution level";

  DecoderBenchmarkInfo info3{NS_LITERAL_CSTRING("video/av1"), 1270, 710, 24, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info3),
            NS_LITERAL_CSTRING("ResolutionLevel4-FrameRateLevel1-8bit"))
      << "Closer to 5th resolution level - bellow";

  DecoderBenchmarkInfo info4{NS_LITERAL_CSTRING("video/av1"), 1290, 730, 24, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info4),
            NS_LITERAL_CSTRING("ResolutionLevel4-FrameRateLevel1-8bit"))
      << "Closer to 5th resolution level - above";

  DecoderBenchmarkInfo info5{NS_LITERAL_CSTRING("video/av1"), 854, 480, 20, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info5),
            NS_LITERAL_CSTRING("ResolutionLevel3-FrameRateLevel1-8bit"))
      << "Closer to 2nd frame rate level - bellow";

  DecoderBenchmarkInfo info6{NS_LITERAL_CSTRING("video/av1"), 854, 480, 26, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info6),
            NS_LITERAL_CSTRING("ResolutionLevel3-FrameRateLevel1-8bit"))
      << "Closer to 2nd frame rate level - above";

  DecoderBenchmarkInfo info7{NS_LITERAL_CSTRING("video/av1"), 1280, 720, 24,
                             10};
  EXPECT_EQ(KeyUtil::CreateKey(info7),
            NS_LITERAL_CSTRING("ResolutionLevel4-FrameRateLevel1-non8bit"))
      << "Bit depth 10 bits";

  DecoderBenchmarkInfo info8{NS_LITERAL_CSTRING("video/av1"), 1280, 720, 24,
                             12};
  EXPECT_EQ(KeyUtil::CreateKey(info8),
            NS_LITERAL_CSTRING("ResolutionLevel4-FrameRateLevel1-non8bit"))
      << "Bit depth 12 bits";

  DecoderBenchmarkInfo info9{NS_LITERAL_CSTRING("video/av1"), 1280, 720, 24,
                             16};
  EXPECT_EQ(KeyUtil::CreateKey(info9),
            NS_LITERAL_CSTRING("ResolutionLevel4-FrameRateLevel1-non8bit"))
      << "Bit depth 16 bits";
}
