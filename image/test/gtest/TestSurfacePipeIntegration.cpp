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
#include "SurfacePipe.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

namespace mozilla {
namespace image {

class TestSurfacePipeFactory
{
public:
  static SurfacePipe SimpleSurfacePipe()
  {
    SurfacePipe pipe;
    return Move(pipe);
  }

  template <typename T>
  static SurfacePipe SurfacePipeFromPipeline(T&& aPipeline)
  {
    return SurfacePipe { Move(aPipeline) };
  }

private:
  TestSurfacePipeFactory() { }
};

} // namespace image
} // namespace mozilla

TEST(ImageSurfacePipeIntegration, SurfacePipe)
{
  // Test that SurfacePipe objects can be initialized and move constructed.
  SurfacePipe pipe = TestSurfacePipeFactory::SimpleSurfacePipe();

  // Test that SurfacePipe objects can be move assigned.
  pipe = TestSurfacePipeFactory::SimpleSurfacePipe();

  // Test that SurfacePipe objects can be initialized with a pipeline.
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  auto sink = MakeUnique<SurfaceSink>();
  nsresult rv =
    sink->Configure(SurfaceConfig { decoder.get(), 0, IntSize(100, 100),
                                    SurfaceFormat::B8G8R8A8, false });
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  pipe = TestSurfacePipeFactory::SurfacePipeFromPipeline(sink);

  // Test that SurfacePipe passes through method calls to the underlying pipeline.
  int32_t count = 0;
  auto result = pipe.WritePixels<uint32_t>([&]() {
    ++count;
    return AsVariant(BGRAColor::Green().AsPixel());
  });
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_EQ(100 * 100, count);

  // Note that we're explicitly testing the SurfacePipe versions of these
  // methods, so we don't want to use AssertCorrectPipelineFinalState() here.
  EXPECT_TRUE(pipe.IsSurfaceFinished());
  Maybe<SurfaceInvalidRect> invalidRect = pipe.TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isSome());
  EXPECT_EQ(IntRect(0, 0, 100, 100), invalidRect->mInputSpaceRect);
  EXPECT_EQ(IntRect(0, 0, 100, 100), invalidRect->mOutputSpaceRect);

  CheckGeneratedImage(decoder, IntRect(0, 0, 100, 100));

  pipe.ResetToFirstRow();
  EXPECT_FALSE(pipe.IsSurfaceFinished());

  RawAccessFrameRef currentFrame = decoder->GetCurrentFrameRef();
  currentFrame->Finish();
}

TEST(ImageSurfacePipeIntegration, DeinterlaceDownscaleWritePixels)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  auto test = [](Decoder* aDecoder, SurfaceFilter* aFilter) {
    CheckWritePixels(aDecoder, aFilter,
                     /* aOutputRect = */ Some(IntRect(0, 0, 25, 25)));
  };

  WithFilterPipeline(decoder, test,
                     DeinterlacingConfig<uint32_t> { /* mProgressiveDisplay = */ true },
                     DownscalingConfig { IntSize(100, 100),
                                         SurfaceFormat::B8G8R8A8 },
                     SurfaceConfig { decoder, 0, IntSize(25, 25),
                                     SurfaceFormat::B8G8R8A8, false });
}

TEST(ImageSurfacePipeIntegration, DeinterlaceDownscaleWriteRows)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  auto test = [](Decoder* aDecoder, SurfaceFilter* aFilter) {
    CheckWriteRows(aDecoder, aFilter,
                   /* aOutputRect = */ Some(IntRect(0, 0, 25, 25)));
  };

  WithFilterPipeline(decoder, test,
                     DeinterlacingConfig<uint32_t> { /* mProgressiveDisplay = */ true },
                     DownscalingConfig { IntSize(100, 100),
                                         SurfaceFormat::B8G8R8A8 },
                     SurfaceConfig { decoder, 0, IntSize(25, 25),
                                     SurfaceFormat::B8G8R8A8, false });
}

TEST(ImageSurfacePipeIntegration, RemoveFrameRectBottomRightDownscaleWritePixels)
{
  // This test case uses a frame rect that extends beyond the borders of the
  // image to the bottom and to the right. It looks roughly like this (with the
  // box made of '#'s representing the frame rect):
  //
  // +------------+
  // +            +
  // +      +------------+
  // +      +############+
  // +------+############+
  //        +############+
  //        +------------+

  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  // Note that aInputWriteRect is 100x50 because RemoveFrameRectFilter ignores
  // trailing rows that don't show up in the output. (Leading rows unfortunately
  // can't be ignored.) So the action of the pipeline is as follows:
  //
  // (1) RemoveFrameRectFilter reads a 100x50 region of the input.
  //     (aInputWriteRect captures this fact.) The remaining 50 rows are ignored
  //     because they extend off the bottom of the image due to the frame rect's
  //     (50, 50) offset. The 50 columns on the right also don't end up in the
  //     output, so ultimately only a 50x50 region in the output contains data
  //     from the input. The filter's output is not 50x50, though, but 100x100,
  //     because what RemoveFrameRectFilter does is introduce blank rows or
  //     columns as necessary to transform an image that needs a frame rect into
  //     an image that doesn't.
  //
  // (2) DownscalingFilter reads the output of RemoveFrameRectFilter (100x100)
  //     and downscales it to 20x20.
  //
  // (3) The surface owned by SurfaceSink logically has only a 10x10 region
  //     region in it that's non-blank; this is the downscaled version of the
  //     50x50 region discussed in (1). (aOutputWriteRect captures this fact.)
  //     Some fuzz, as usual, is necessary when dealing with Lanczos downscaling.

  auto test = [](Decoder* aDecoder, SurfaceFilter* aFilter) {
    CheckWritePixels(aDecoder, aFilter,
                     /* aOutputRect = */ Some(IntRect(0, 0, 20, 20)),
                     /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                     /* aInputWriteRect = */ Some(IntRect(50, 50, 100, 50)),
                     /* aOutputWriteRect = */ Some(IntRect(10, 10, 10, 10)),
                     /* aFuzz = */ 0x33);
  };

  WithFilterPipeline(decoder, test,
                     RemoveFrameRectConfig { IntRect(50, 50, 100, 100) },
                     DownscalingConfig { IntSize(100, 100),
                                         SurfaceFormat::B8G8R8A8 },
                     SurfaceConfig { decoder, 0, IntSize(20, 20),
                                     SurfaceFormat::B8G8R8A8, false });
}

TEST(ImageSurfacePipeIntegration, RemoveFrameRectBottomRightDownscaleWriteRows)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  // See the WritePixels version of this test for a discussion of where the
  // numbers below come from.

  auto test = [](Decoder* aDecoder, SurfaceFilter* aFilter) {
    CheckWriteRows(aDecoder, aFilter,
                   /* aOutputRect = */ Some(IntRect(0, 0, 20, 20)),
                   /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                   /* aInputWriteRect = */ Some(IntRect(50, 50, 100, 50)),
                   /* aOutputWriteRect = */ Some(IntRect(10, 10, 10, 10)),
                   /* aFuzz = */ 0x33);
  };

  WithFilterPipeline(decoder, test,
                     RemoveFrameRectConfig { IntRect(50, 50, 100, 100) },
                     DownscalingConfig { IntSize(100, 100),
                                         SurfaceFormat::B8G8R8A8 },
                     SurfaceConfig { decoder, 0, IntSize(20, 20),
                                     SurfaceFormat::B8G8R8A8, false });
}

TEST(ImageSurfacePipeIntegration, RemoveFrameRectTopLeftDownscaleWritePixels)
{
  // This test case uses a frame rect that extends beyond the borders of the
  // image to the top and to the left. It looks roughly like this (with the
  // box made of '#'s representing the frame rect):
  //
  // +------------+
  // +############+
  // +############+------+
  // +############+      +
  // +------------+      +
  //        +            +
  //        +------------+

  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  auto test = [](Decoder* aDecoder, SurfaceFilter* aFilter) {
    CheckWritePixels(aDecoder, aFilter,
                     /* aOutputRect = */ Some(IntRect(0, 0, 20, 20)),
                     /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                     /* aInputWriteRect = */ Some(IntRect(0, 0, 100, 100)),
                     /* aOutputWriteRect = */ Some(IntRect(0, 0, 10, 10)),
                     /* aFuzz = */ 0x21);
  };

  WithFilterPipeline(decoder, test,
                     RemoveFrameRectConfig { IntRect(-50, -50, 100, 100) },
                     DownscalingConfig { IntSize(100, 100),
                                         SurfaceFormat::B8G8R8A8 },
                     SurfaceConfig { decoder, 0, IntSize(20, 20),
                                     SurfaceFormat::B8G8R8A8, false });
}

TEST(ImageSurfacePipeIntegration, RemoveFrameRectTopLeftDownscaleWriteRows)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  auto test = [](Decoder* aDecoder, SurfaceFilter* aFilter) {
    CheckWriteRows(aDecoder, aFilter,
                   /* aOutputRect = */ Some(IntRect(0, 0, 20, 20)),
                   /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                   /* aInputWriteRect = */ Some(IntRect(0, 0, 100, 100)),
                   /* aOutputWriteRect = */ Some(IntRect(0, 0, 10, 10)),
                   /* aFuzz = */ 0x21);
  };

  WithFilterPipeline(decoder, test,
                     RemoveFrameRectConfig { IntRect(-50, -50, 100, 100) },
                     DownscalingConfig { IntSize(100, 100),
                                         SurfaceFormat::B8G8R8A8 },
                     SurfaceConfig { decoder, 0, IntSize(20, 20),
                                     SurfaceFormat::B8G8R8A8, false });
}


TEST(ImageSurfacePipeIntegration, DeinterlaceRemoveFrameRectWritePixels)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  // Note that aInputRect is the full 100x100 size even though
  // RemoveFrameRectFilter is part of this pipeline, because deinterlacing
  // requires reading every row.

  auto test = [](Decoder* aDecoder, SurfaceFilter* aFilter) {
    CheckWritePixels(aDecoder, aFilter,
                     /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                     /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                     /* aInputWriteRect = */ Some(IntRect(50, 50, 100, 100)),
                     /* aOutputWriteRect = */ Some(IntRect(50, 50, 50, 50)));
  };

  WithFilterPipeline(decoder, test,
                     DeinterlacingConfig<uint32_t> { /* mProgressiveDisplay = */ true },
                     RemoveFrameRectConfig { IntRect(50, 50, 100, 100) },
                     SurfaceConfig { decoder, 0, IntSize(100, 100),
                                     SurfaceFormat::B8G8R8A8, false });
}

TEST(ImageSurfacePipeIntegration, DeinterlaceRemoveFrameRectWriteRows)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  // Note that aInputRect is the full 100x100 size even though
  // RemoveFrameRectFilter is part of this pipeline, because deinterlacing
  // requires reading every row.

  auto test = [](Decoder* aDecoder, SurfaceFilter* aFilter) {
    CheckWriteRows(aDecoder, aFilter,
                   /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                   /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                   /* aInputWriteRect = */ Some(IntRect(50, 50, 100, 100)),
                   /* aOutputWriteRect = */ Some(IntRect(50, 50, 50, 50)));
  };

  WithFilterPipeline(decoder, test,
                     DeinterlacingConfig<uint32_t> { /* mProgressiveDisplay = */ true },
                     RemoveFrameRectConfig { IntRect(50, 50, 100, 100) },
                     SurfaceConfig { decoder, 0, IntSize(100, 100),
                                     SurfaceFormat::B8G8R8A8, false });
}

TEST(ImageSurfacePipeIntegration, DeinterlaceRemoveFrameRectDownscaleWritePixels)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  auto test = [](Decoder* aDecoder, SurfaceFilter* aFilter) {
    CheckWritePixels(aDecoder, aFilter,
                     /* aOutputRect = */ Some(IntRect(0, 0, 20, 20)),
                     /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                     /* aInputWriteRect = */ Some(IntRect(50, 50, 100, 100)),
                     /* aOutputWriteRect = */ Some(IntRect(10, 10, 10, 10)),
                     /* aFuzz = */ 33);
  };

  WithFilterPipeline(decoder, test,
                     DeinterlacingConfig<uint32_t> { /* mProgressiveDisplay = */ true },
                     RemoveFrameRectConfig { IntRect(50, 50, 100, 100) },
                     DownscalingConfig { IntSize(100, 100),
                                         SurfaceFormat::B8G8R8A8 },
                     SurfaceConfig { decoder, 0, IntSize(20, 20),
                                     SurfaceFormat::B8G8R8A8, false });
}

TEST(ImageSurfacePipeIntegration, DeinterlaceRemoveFrameRectDownscaleWriteRows)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  auto test = [](Decoder* aDecoder, SurfaceFilter* aFilter) {
    CheckWriteRows(aDecoder, aFilter,
                   /* aOutputRect = */ Some(IntRect(0, 0, 20, 20)),
                   /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                   /* aInputWriteRect = */ Some(IntRect(50, 50, 100, 100)),
                   /* aOutputWriteRect = */ Some(IntRect(10, 10, 10, 10)),
                   /* aFuzz = */ 33);
  };

  WithFilterPipeline(decoder, test,
                     DeinterlacingConfig<uint32_t> { /* mProgressiveDisplay = */ true },
                     RemoveFrameRectConfig { IntRect(50, 50, 100, 100) },
                     DownscalingConfig { IntSize(100, 100),
                                         SurfaceFormat::B8G8R8A8 },
                     SurfaceConfig { decoder, 0, IntSize(20, 20),
                                     SurfaceFormat::B8G8R8A8, false });
}

TEST(ImageSurfacePipeIntegration, ConfiguringPalettedRemoveFrameRectDownscaleFails)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  // This is an invalid pipeline for paletted images, so configuration should
  // fail.
  AssertConfiguringPipelineFails(decoder,
                                 RemoveFrameRectConfig { IntRect(0, 0, 50, 50) },
                                 DownscalingConfig { IntSize(100, 100),
                                                     SurfaceFormat::B8G8R8A8 },
                                 PalettedSurfaceConfig { decoder, 0, IntSize(100, 100),
                                                         IntRect(0, 0, 50, 50),
                                                         SurfaceFormat::B8G8R8A8, 8,
                                                         false });
}

TEST(ImageSurfacePipeIntegration, ConfiguringPalettedDeinterlaceDownscaleFails)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  // This is an invalid pipeline for paletted images, so configuration should
  // fail.
  AssertConfiguringPipelineFails(decoder,
                                 DeinterlacingConfig<uint8_t> { /* mProgressiveDisplay = */ true},
                                 DownscalingConfig { IntSize(100, 100),
                                                     SurfaceFormat::B8G8R8A8 },
                                 PalettedSurfaceConfig { decoder, 0, IntSize(100, 100),
                                                         IntRect(0, 0, 20, 20),
                                                         SurfaceFormat::B8G8R8A8, 8,
                                                         false });
}
