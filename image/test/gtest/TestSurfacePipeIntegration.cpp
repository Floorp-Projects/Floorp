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

void
CheckSurfacePipeMethodResults(SurfacePipe* aPipe,
                              Decoder* aDecoder,
                              const IntRect& aRect = IntRect(0, 0, 100, 100))
{
  // Check that the pipeline ended up in the state we expect.  Note that we're
  // explicitly testing the SurfacePipe versions of these methods, so we don't
  // want to use AssertCorrectPipelineFinalState() here.
  EXPECT_TRUE(aPipe->IsSurfaceFinished());
  Maybe<SurfaceInvalidRect> invalidRect = aPipe->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isSome());
  EXPECT_EQ(IntRect(0, 0, 100, 100), invalidRect->mInputSpaceRect);
  EXPECT_EQ(IntRect(0, 0, 100, 100), invalidRect->mOutputSpaceRect);

  // Check the generated image.
  CheckGeneratedImage(aDecoder, aRect);

  // Reset and clear the image before the next test.
  aPipe->ResetToFirstRow();
  EXPECT_FALSE(aPipe->IsSurfaceFinished());
  invalidRect = aPipe->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());

  uint32_t count = 0;
  auto result = aPipe->WritePixels<uint32_t>([&]() {
    ++count;
    return AsVariant(BGRAColor::Transparent().AsPixel());
  });
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_EQ(100u * 100u, count);

  EXPECT_TRUE(aPipe->IsSurfaceFinished());
  invalidRect = aPipe->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isSome());
  EXPECT_EQ(IntRect(0, 0, 100, 100), invalidRect->mInputSpaceRect);
  EXPECT_EQ(IntRect(0, 0, 100, 100), invalidRect->mOutputSpaceRect);

  aPipe->ResetToFirstRow();
  EXPECT_FALSE(aPipe->IsSurfaceFinished());
  invalidRect = aPipe->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());
}

void
CheckPalettedSurfacePipeMethodResults(SurfacePipe* aPipe,
                                      Decoder* aDecoder,
                                      const IntRect& aRect
                                        = IntRect(0, 0, 100, 100))
{
  // Check that the pipeline ended up in the state we expect.  Note that we're
  // explicitly testing the SurfacePipe versions of these methods, so we don't
  // want to use AssertCorrectPipelineFinalState() here.
  EXPECT_TRUE(aPipe->IsSurfaceFinished());
  Maybe<SurfaceInvalidRect> invalidRect = aPipe->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isSome());
  EXPECT_EQ(IntRect(0, 0, 100, 100), invalidRect->mInputSpaceRect);
  EXPECT_EQ(IntRect(0, 0, 100, 100), invalidRect->mOutputSpaceRect);

  // Check the generated image.
  CheckGeneratedPalettedImage(aDecoder, aRect);

  // Reset and clear the image before the next test.
  aPipe->ResetToFirstRow();
  EXPECT_FALSE(aPipe->IsSurfaceFinished());
  invalidRect = aPipe->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());

  uint32_t count = 0;
  auto result = aPipe->WritePixels<uint8_t>([&]() {
    ++count;
    return AsVariant(uint8_t(0));
  });
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_EQ(100u * 100u, count);

  EXPECT_TRUE(aPipe->IsSurfaceFinished());
  invalidRect = aPipe->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isSome());
  EXPECT_EQ(IntRect(0, 0, 100, 100), invalidRect->mInputSpaceRect);
  EXPECT_EQ(IntRect(0, 0, 100, 100), invalidRect->mOutputSpaceRect);

  aPipe->ResetToFirstRow();
  EXPECT_FALSE(aPipe->IsSurfaceFinished());
  invalidRect = aPipe->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());
}

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
    sink->Configure(SurfaceConfig { decoder, 0, IntSize(100, 100),
                                    SurfaceFormat::B8G8R8A8, false });
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  pipe = TestSurfacePipeFactory::SurfacePipeFromPipeline(sink);

  // Test that WritePixels() gets passed through to the underlying pipeline.
  {
    uint32_t count = 0;
    auto result = pipe.WritePixels<uint32_t>([&]() {
      ++count;
      return AsVariant(BGRAColor::Green().AsPixel());
    });
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(100u * 100u, count);
    CheckSurfacePipeMethodResults(&pipe, decoder);
  }

  // Create a buffer the same size as one row of the surface, containing all
  // green pixels. We'll use this for the WriteBuffer() tests.
  uint32_t buffer[100];
  for (int i = 0; i < 100; ++i) {
    buffer[i] = BGRAColor::Green().AsPixel();
  }

  // Test that WriteBuffer() gets passed through to the underlying pipeline.
  {
    uint32_t count = 0;
    WriteState result = WriteState::NEED_MORE_DATA;
    while (result == WriteState::NEED_MORE_DATA) {
      result = pipe.WriteBuffer(buffer);
      ++count;
    }
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(100u, count);
    CheckSurfacePipeMethodResults(&pipe, decoder);
  }

  // Test that the 3 argument version of WriteBuffer() gets passed through to
  // the underlying pipeline.
  {
    uint32_t count = 0;
    WriteState result = WriteState::NEED_MORE_DATA;
    while (result == WriteState::NEED_MORE_DATA) {
      result = pipe.WriteBuffer(buffer, 0, 100);
      ++count;
    }
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(100u, count);
    CheckSurfacePipeMethodResults(&pipe, decoder);
  }

  // Test that WriteEmptyRow() gets passed through to the underlying pipeline.
  {
    uint32_t count = 0;
    WriteState result = WriteState::NEED_MORE_DATA;
    while (result == WriteState::NEED_MORE_DATA) {
      result = pipe.WriteEmptyRow();
      ++count;
    }
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(100u, count);
    CheckSurfacePipeMethodResults(&pipe, decoder, IntRect(0, 0, 0, 0));
  }

  // Mark the frame as finished so we don't get an assertion.
  RawAccessFrameRef currentFrame = decoder->GetCurrentFrameRef();
  currentFrame->Finish();
}

TEST(ImageSurfacePipeIntegration, PalettedSurfacePipe)
{
  // Create a SurfacePipe containing a PalettedSurfaceSink.
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  auto sink = MakeUnique<PalettedSurfaceSink>();
  nsresult rv =
    sink->Configure(PalettedSurfaceConfig { decoder, 0, IntSize(100, 100),
                                            IntRect(0, 0, 100, 100),
                                            SurfaceFormat::B8G8R8A8,
                                            8, false });
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  SurfacePipe pipe = TestSurfacePipeFactory::SurfacePipeFromPipeline(sink);

  // Test that WritePixels() gets passed through to the underlying pipeline.
  {
    uint32_t count = 0;
    auto result = pipe.WritePixels<uint8_t>([&]() {
      ++count;
      return AsVariant(uint8_t(255));
    });
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(100u * 100u, count);
    CheckPalettedSurfacePipeMethodResults(&pipe, decoder);
  }

  // Create a buffer the same size as one row of the surface, containing all
  // 255 pixels. We'll use this for the WriteBuffer() tests.
  uint8_t buffer[100];
  for (int i = 0; i < 100; ++i) {
    buffer[i] = 255;
  }

  // Test that WriteBuffer() gets passed through to the underlying pipeline.
  {
    uint32_t count = 0;
    WriteState result = WriteState::NEED_MORE_DATA;
    while (result == WriteState::NEED_MORE_DATA) {
      result = pipe.WriteBuffer(buffer);
      ++count;
    }
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(100u, count);
    CheckPalettedSurfacePipeMethodResults(&pipe, decoder);
  }

  // Test that the 3 argument version of WriteBuffer() gets passed through to
  // the underlying pipeline.
  {
    uint32_t count = 0;
    WriteState result = WriteState::NEED_MORE_DATA;
    while (result == WriteState::NEED_MORE_DATA) {
      result = pipe.WriteBuffer(buffer, 0, 100);
      ++count;
    }
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(100u, count);
    CheckPalettedSurfacePipeMethodResults(&pipe, decoder);
  }

  // Test that WriteEmptyRow() gets passed through to the underlying pipeline.
  {
    uint32_t count = 0;
    WriteState result = WriteState::NEED_MORE_DATA;
    while (result == WriteState::NEED_MORE_DATA) {
      result = pipe.WriteEmptyRow();
      ++count;
    }
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(100u, count);
    CheckPalettedSurfacePipeMethodResults(&pipe, decoder, IntRect(0, 0, 0, 0));
  }

  // Mark the frame as finished so we don't get an assertion.
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
