/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "VideoSegment.h"

using namespace mozilla;

namespace mozilla::layer {
class Image;
}  // namespace mozilla::layer

TEST(VideoSegment, TestAppendFrameForceBlack)
{
  RefPtr<layers::Image> testImage = nullptr;

  VideoSegment segment;
  segment.AppendFrame(testImage.forget(), mozilla::gfx::IntSize(640, 480),
                      PRINCIPAL_HANDLE_NONE, true);

  VideoSegment::ChunkIterator iter(segment);
  while (!iter.IsEnded()) {
    VideoChunk chunk = *iter;
    EXPECT_TRUE(chunk.mFrame.GetForceBlack());
    iter.Next();
  }
}

TEST(VideoSegment, TestAppendFrameNotForceBlack)
{
  RefPtr<layers::Image> testImage = nullptr;

  VideoSegment segment;
  segment.AppendFrame(testImage.forget(), mozilla::gfx::IntSize(640, 480),
                      PRINCIPAL_HANDLE_NONE);

  VideoSegment::ChunkIterator iter(segment);
  while (!iter.IsEnded()) {
    VideoChunk chunk = *iter;
    EXPECT_FALSE(chunk.mFrame.GetForceBlack());
    iter.Next();
  }
}
