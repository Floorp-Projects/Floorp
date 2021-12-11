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
  DecoderBenchmarkInfo info{"video/av1"_ns, 1, 1, 1, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info),
            "ResolutionLevel0-FrameRateLevel0-8bit"_ns)
      << "Min level";

  DecoderBenchmarkInfo info1{"video/av1"_ns, 5000, 5000, 100, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info1),
            "ResolutionLevel7-FrameRateLevel4-8bit"_ns)
      << "Max level";

  DecoderBenchmarkInfo info2{"video/av1"_ns, 854, 480, 30, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info2),
            "ResolutionLevel3-FrameRateLevel2-8bit"_ns)
      << "On the top of 4th resolution level";

  DecoderBenchmarkInfo info3{"video/av1"_ns, 1270, 710, 24, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info3),
            "ResolutionLevel4-FrameRateLevel1-8bit"_ns)
      << "Closer to 5th resolution level - bellow";

  DecoderBenchmarkInfo info4{"video/av1"_ns, 1290, 730, 24, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info4),
            "ResolutionLevel4-FrameRateLevel1-8bit"_ns)
      << "Closer to 5th resolution level - above";

  DecoderBenchmarkInfo info5{"video/av1"_ns, 854, 480, 20, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info5),
            "ResolutionLevel3-FrameRateLevel1-8bit"_ns)
      << "Closer to 2nd frame rate level - bellow";

  DecoderBenchmarkInfo info6{"video/av1"_ns, 854, 480, 26, 8};
  EXPECT_EQ(KeyUtil::CreateKey(info6),
            "ResolutionLevel3-FrameRateLevel1-8bit"_ns)
      << "Closer to 2nd frame rate level - above";

  DecoderBenchmarkInfo info7{"video/av1"_ns, 1280, 720, 24, 10};
  EXPECT_EQ(KeyUtil::CreateKey(info7),
            "ResolutionLevel4-FrameRateLevel1-non8bit"_ns)
      << "Bit depth 10 bits";

  DecoderBenchmarkInfo info8{"video/av1"_ns, 1280, 720, 24, 12};
  EXPECT_EQ(KeyUtil::CreateKey(info8),
            "ResolutionLevel4-FrameRateLevel1-non8bit"_ns)
      << "Bit depth 12 bits";

  DecoderBenchmarkInfo info9{"video/av1"_ns, 1280, 720, 24, 16};
  EXPECT_EQ(KeyUtil::CreateKey(info9),
            "ResolutionLevel4-FrameRateLevel1-non8bit"_ns)
      << "Bit depth 16 bits";
}
