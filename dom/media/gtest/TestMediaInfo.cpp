/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaInfo.h"
#include "gtest/gtest.h"

using namespace mozilla;

TEST(TestMediaInfo, CloneVideoInfo)
{
  VideoInfo info;
  info.mDisplay = info.mImage = gfx::IntSize{640, 360};
  info.mStereoMode = StereoMode::BOTTOM_TOP;
  info.mRotation = VideoRotation::kDegree_270;
  info.mColorDepth = gfx::ColorDepth::COLOR_16;
  info.mColorRange = gfx::ColorRange::FULL;

  UniquePtr<TrackInfo> clone1 = info.Clone();
  UniquePtr<TrackInfo> clone2 = info.Clone();

  auto* info1 = clone1->GetAsVideoInfo();
  auto* info2 = clone2->GetAsVideoInfo();

  EXPECT_EQ(info1->mDisplay, info2->mDisplay);
  EXPECT_EQ(info1->mImage, info2->mImage);
  EXPECT_EQ(info1->mStereoMode, info2->mStereoMode);
  EXPECT_EQ(info1->mRotation, info2->mRotation);
  EXPECT_EQ(info1->mColorDepth, info2->mColorDepth);
  EXPECT_EQ(info1->mColorRange, info2->mColorRange);
  // They should have their own media byte buffers which have different address
  EXPECT_NE(info1->mExtraData.get(), info2->mExtraData.get());
  EXPECT_NE(info1->mCodecSpecificConfig.get(),
            info2->mCodecSpecificConfig.get());
}
