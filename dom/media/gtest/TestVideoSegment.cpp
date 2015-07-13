/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "VideoSegment.h"

using namespace mozilla;

namespace mozilla {
  namespace layer {
    class Image;
  } // namespace layer
} // namespace mozilla

TEST(VideoSegment, TestAppendFrameForceBlack)
{
  nsRefPtr<layers::Image> testImage = nullptr;

  VideoSegment segment;
  segment.AppendFrame(testImage.forget(),
                      mozilla::StreamTime(90000),
                      mozilla::gfx::IntSize(640, 480),
                      true);

  VideoSegment::ChunkIterator iter(segment);
  while (!iter.IsEnded()) {
    VideoChunk chunk = *iter;
    EXPECT_TRUE(chunk.mFrame.GetForceBlack());
    iter.Next();
  }
}

TEST(VideoSegment, TestAppendFrameNotForceBlack)
{
  nsRefPtr<layers::Image> testImage = nullptr;

  VideoSegment segment;
  segment.AppendFrame(testImage.forget(),
                      mozilla::StreamTime(90000),
                      mozilla::gfx::IntSize(640, 480));

  VideoSegment::ChunkIterator iter(segment);
  while (!iter.IsEnded()) {
    VideoChunk chunk = *iter;
    EXPECT_FALSE(chunk.mFrame.GetForceBlack());
    iter.Next();
  }
}
