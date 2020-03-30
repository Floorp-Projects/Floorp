/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/gfx/2D.h"
#include "Common.h"
#include "Decoder.h"
#include "DecoderFactory.h"
#include "SourceBuffer.h"
#include "SurfaceFilters.h"
#include "SurfacePipe.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

template <typename Func>
void WithDownscalingFilter(const IntSize& aInputSize,
                           const IntSize& aOutputSize, Func aFunc) {
  RefPtr<image::Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  WithFilterPipeline(
      decoder, std::forward<Func>(aFunc),
      DownscalingConfig{aInputSize, SurfaceFormat::OS_RGBA},
      SurfaceConfig{decoder, aOutputSize, SurfaceFormat::OS_RGBA, false});
}

void AssertConfiguringDownscalingFilterFails(const IntSize& aInputSize,
                                             const IntSize& aOutputSize) {
  RefPtr<image::Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AssertConfiguringPipelineFails(
      decoder, DownscalingConfig{aInputSize, SurfaceFormat::OS_RGBA},
      SurfaceConfig{decoder, aOutputSize, SurfaceFormat::OS_RGBA, false});
}

TEST(ImageDownscalingFilter, WritePixels100_100to99_99)
{
  WithDownscalingFilter(IntSize(100, 100), IntSize(99, 99),
                        [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
                          CheckWritePixels(
                              aDecoder, aFilter,
                              /* aOutputRect = */ Some(IntRect(0, 0, 99, 99)));
                        });
}

TEST(ImageDownscalingFilter, WritePixels100_100to33_33)
{
  WithDownscalingFilter(IntSize(100, 100), IntSize(33, 33),
                        [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
                          CheckWritePixels(
                              aDecoder, aFilter,
                              /* aOutputRect = */ Some(IntRect(0, 0, 33, 33)));
                        });
}

TEST(ImageDownscalingFilter, WritePixels100_100to1_1)
{
  WithDownscalingFilter(IntSize(100, 100), IntSize(1, 1),
                        [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
                          CheckWritePixels(
                              aDecoder, aFilter,
                              /* aOutputRect = */ Some(IntRect(0, 0, 1, 1)));
                        });
}

TEST(ImageDownscalingFilter, WritePixels100_100to33_99)
{
  WithDownscalingFilter(IntSize(100, 100), IntSize(33, 99),
                        [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
                          CheckWritePixels(
                              aDecoder, aFilter,
                              /* aOutputRect = */ Some(IntRect(0, 0, 33, 99)));
                        });
}

TEST(ImageDownscalingFilter, WritePixels100_100to99_33)
{
  WithDownscalingFilter(IntSize(100, 100), IntSize(99, 33),
                        [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
                          CheckWritePixels(
                              aDecoder, aFilter,
                              /* aOutputRect = */ Some(IntRect(0, 0, 99, 33)));
                        });
}

TEST(ImageDownscalingFilter, WritePixels100_100to99_1)
{
  WithDownscalingFilter(IntSize(100, 100), IntSize(99, 1),
                        [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
                          CheckWritePixels(
                              aDecoder, aFilter,
                              /* aOutputRect = */ Some(IntRect(0, 0, 99, 1)));
                        });
}

TEST(ImageDownscalingFilter, WritePixels100_100to1_99)
{
  WithDownscalingFilter(IntSize(100, 100), IntSize(1, 99),
                        [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
                          CheckWritePixels(
                              aDecoder, aFilter,
                              /* aOutputRect = */ Some(IntRect(0, 0, 1, 99)));
                        });
}

TEST(ImageDownscalingFilter, DownscalingFailsFor100_100to101_101)
{
  // Upscaling is disallowed.
  AssertConfiguringDownscalingFilterFails(IntSize(100, 100), IntSize(101, 101));
}

TEST(ImageDownscalingFilter, DownscalingFailsFor100_100to100_100)
{
  // "Scaling" to the same size is disallowed.
  AssertConfiguringDownscalingFilterFails(IntSize(100, 100), IntSize(100, 100));
}

TEST(ImageDownscalingFilter, DownscalingFailsFor0_0toMinus1_Minus1)
{
  // A 0x0 input size is disallowed.
  AssertConfiguringDownscalingFilterFails(IntSize(0, 0), IntSize(-1, -1));
}

TEST(ImageDownscalingFilter, DownscalingFailsForMinus1_Minus1toMinus2_Minus2)
{
  // A negative input size is disallowed.
  AssertConfiguringDownscalingFilterFails(IntSize(-1, -1), IntSize(-2, -2));
}

TEST(ImageDownscalingFilter, DownscalingFailsFor100_100to0_0)
{
  // A 0x0 output size is disallowed.
  AssertConfiguringDownscalingFilterFails(IntSize(100, 100), IntSize(0, 0));
}

TEST(ImageDownscalingFilter, DownscalingFailsFor100_100toMinus1_Minus1)
{
  // A negative output size is disallowed.
  AssertConfiguringDownscalingFilterFails(IntSize(100, 100), IntSize(-1, -1));
}

TEST(ImageDownscalingFilter, WritePixelsOutput100_100to20_20)
{
  WithDownscalingFilter(
      IntSize(100, 100), IntSize(20, 20),
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Fill the image. It consists of 25 lines of green, followed by 25
        // lines of red, followed by 25 lines of green, followed by 25 more
        // lines of red.
        uint32_t count = 0;
        auto result =
            aFilter->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
              uint32_t color =
                  (count <= 25 * 100) || (count > 50 * 100 && count <= 75 * 100)
                      ? BGRAColor::Green().AsPixel()
                      : BGRAColor::Red().AsPixel();
              ++count;
              return AsVariant(color);
            });
        EXPECT_EQ(WriteState::FINISHED, result);
        EXPECT_EQ(100u * 100u, count);

        AssertCorrectPipelineFinalState(aFilter, IntRect(0, 0, 100, 100),
                                        IntRect(0, 0, 20, 20));

        // Check that the generated image is correct. Note that we skip rows
        // near the transitions between colors, since the downscaler does not
        // produce a sharp boundary at these points. Even some of the rows we
        // test need a small amount of fuzz; this is just the nature of Lanczos
        // downscaling.
        RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
        RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
        EXPECT_TRUE(RowsAreSolidColor(surface, 0, 4, BGRAColor::Green(),
                                      /* aFuzz = */ 2));
        EXPECT_TRUE(RowsAreSolidColor(surface, 6, 3, BGRAColor::Red(),
                                      /* aFuzz = */ 3));
        EXPECT_TRUE(RowsAreSolidColor(surface, 11, 3, BGRAColor::Green(),
                                      /* aFuzz = */ 3));
        EXPECT_TRUE(RowsAreSolidColor(surface, 16, 4, BGRAColor::Red(),
                                      /* aFuzz = */ 3));
      });
}

TEST(ImageDownscalingFilter, WritePixelsOutput100_100to10_20)
{
  WithDownscalingFilter(
      IntSize(100, 100), IntSize(10, 20),
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Fill the image. It consists of 25 lines of green, followed by 25
        // lines of red, followed by 25 lines of green, followed by 25 more
        // lines of red.
        uint32_t count = 0;
        auto result =
            aFilter->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
              uint32_t color =
                  (count <= 25 * 100) || (count > 50 * 100 && count <= 75 * 100)
                      ? BGRAColor::Green().AsPixel()
                      : BGRAColor::Red().AsPixel();
              ++count;
              return AsVariant(color);
            });
        EXPECT_EQ(WriteState::FINISHED, result);
        EXPECT_EQ(100u * 100u, count);

        AssertCorrectPipelineFinalState(aFilter, IntRect(0, 0, 100, 100),
                                        IntRect(0, 0, 10, 20));

        // Check that the generated image is correct. Note that we skip rows
        // near the transitions between colors, since the downscaler does not
        // produce a sharp boundary at these points. Even some of the rows we
        // test need a small amount of fuzz; this is just the nature of Lanczos
        // downscaling.
        RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
        RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
        EXPECT_TRUE(RowsAreSolidColor(surface, 0, 4, BGRAColor::Green(),
                                      /* aFuzz = */ 2));
        EXPECT_TRUE(RowsAreSolidColor(surface, 6, 3, BGRAColor::Red(),
                                      /* aFuzz = */ 3));
        EXPECT_TRUE(RowsAreSolidColor(surface, 11, 3, BGRAColor::Green(),
                                      /* aFuzz = */ 3));
        EXPECT_TRUE(RowsAreSolidColor(surface, 16, 4, BGRAColor::Red(),
                                      /* aFuzz = */ 3));
      });
}
