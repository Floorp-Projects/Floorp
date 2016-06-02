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

enum class Orient
{
  NORMAL,
  FLIP_VERTICALLY
};

template <Orient Orientation, typename Func> void
WithSurfaceSink(Func aFunc)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  const bool flipVertically = Orientation == Orient::FLIP_VERTICALLY;

  WithFilterPipeline(decoder, Forward<Func>(aFunc),
                     SurfaceConfig { decoder, 0, IntSize(100, 100),
                                     SurfaceFormat::B8G8R8A8, flipVertically });
}

template <typename Func> void
WithPalettedSurfaceSink(const IntRect& aFrameRect, Func aFunc)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  WithFilterPipeline(decoder, Forward<Func>(aFunc),
                     PalettedSurfaceConfig { decoder, 0, IntSize(100, 100),
                                             aFrameRect, SurfaceFormat::B8G8R8A8,
                                             8, false });
}

TEST(ImageSurfaceSink, NullSurfaceSink)
{
  // Create the NullSurfaceSink.
  NullSurfaceSink sink;
  nsresult rv = sink.Configure(NullSurfaceConfig { });
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  EXPECT_TRUE(!sink.IsValidPalettedPipe());

  // Ensure that we can't write anything.
  bool gotCalled = false;
  auto result = sink.WritePixels<uint32_t>([&]() {
    gotCalled = true;
    return AsVariant(BGRAColor::Green().AsPixel());
  });
  EXPECT_FALSE(gotCalled);
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_TRUE(sink.IsSurfaceFinished());
  Maybe<SurfaceInvalidRect> invalidRect = sink.TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());

  result = sink.WriteRows<uint32_t>([&](uint32_t* aRow, uint32_t aLength) {
    gotCalled = true;
    for (; aLength > 0; --aLength, ++aRow) {
      *aRow = BGRAColor::Green().AsPixel();
    }
    return Nothing();
  });
  EXPECT_FALSE(gotCalled);
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_TRUE(sink.IsSurfaceFinished());
  invalidRect = sink.TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());

  // Attempt to advance to the next row and make sure nothing changes.
  sink.AdvanceRow();
  EXPECT_TRUE(sink.IsSurfaceFinished());
  invalidRect = sink.TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());

  // Attempt to advance to the next pass and make sure nothing changes.
  sink.ResetToFirstRow();
  EXPECT_TRUE(sink.IsSurfaceFinished());
  invalidRect = sink.TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());
}

TEST(ImageSurfaceSink, SurfaceSinkInitialization)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Check initial state.
    EXPECT_FALSE(aSink->IsSurfaceFinished());
    Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
    EXPECT_TRUE(invalidRect.isNothing());

    // Check that the surface is zero-initialized. We verify this by calling
    // CheckGeneratedImage() and telling it that we didn't write to the surface
    // anyway (i.e., we wrote to the empty rect); it will then expect the entire
    // surface to be transparent, which is what it should be if it was
    // zero-initialied.
    CheckGeneratedImage(aDecoder, IntRect(0, 0, 0, 0));
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWritePixels)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    CheckWritePixels(aDecoder, aSink);
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWriteRows)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    CheckWriteRows(aDecoder, aSink);
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWritePixelsFinish)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Write nothing into the surface; just finish immediately.
    uint32_t count = 0;
    auto result = aSink->WritePixels<uint32_t>([&]() {
      count++;
      return AsVariant(WriteState::FINISHED);
    });
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(1u, count);
    EXPECT_TRUE(aSink->IsSurfaceFinished());

    // Attempt to write more and make sure that nothing gets written.
    count = 0;
    result = aSink->WritePixels<uint32_t>([&]() {
      count++;
      return AsVariant(BGRAColor::Red().AsPixel());
    });
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(0u, count);
    EXPECT_TRUE(aSink->IsSurfaceFinished());

    // Check that the generated image is correct.
    RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
    RefPtr<SourceSurface> surface = currentFrame->GetSurface();
    EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Transparent()));
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWriteRowsFinish)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Write nothing into the surface; just finish immediately.
    uint32_t count = 0;
    auto result = aSink->WriteRows<uint32_t>([&](uint32_t* aRow, uint32_t aLength) {
      count++;
      return Some(WriteState::FINISHED);
    });
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(1u, count);
    EXPECT_TRUE(aSink->IsSurfaceFinished());

    // Attempt to write more and make sure that nothing gets written.
    count = 0;
    result = aSink->WriteRows<uint32_t>([&](uint32_t* aRow, uint32_t aLength) {
      count++;
      for (; aLength > 0; --aLength, ++aRow) {
        *aRow = BGRAColor::Green().AsPixel();
      }
      return Nothing();
    });
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(0u, count);
    EXPECT_TRUE(aSink->IsSurfaceFinished());

    // Check that the generated image is correct.
    RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
    RefPtr<SourceSurface> surface = currentFrame->GetSurface();
    EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Transparent()));
  });
}

TEST(ImageSurfaceSink, SurfaceSinkProgressivePasses)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    {
      // Fill the image with a first pass of red.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() {
        ++count;
        return AsVariant(BGRAColor::Red().AsPixel());
      });
      EXPECT_EQ(WriteState::FINISHED, result);
      EXPECT_EQ(100u * 100u, count);

      AssertCorrectPipelineFinalState(aSink,
                                      IntRect(0, 0, 100, 100),
                                      IntRect(0, 0, 100, 100));

      // Check that the generated image is correct.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Red()));
    }

    {
      // Reset for the second pass.
      aSink->ResetToFirstRow();
      EXPECT_FALSE(aSink->IsSurfaceFinished());
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isNothing());

      // Check that the generated image is still the first pass image.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Red()));
    }

    {
      // Fill the image with a second pass of green.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() {
        ++count;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::FINISHED, result);
      EXPECT_EQ(100u * 100u, count);

      AssertCorrectPipelineFinalState(aSink,
                                      IntRect(0, 0, 100, 100),
                                      IntRect(0, 0, 100, 100));

      // Check that the generated image is correct.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Green()));
    }
  });
}

TEST(ImageSurfaceSink, SurfaceSinkInvalidRect)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    {
      // Write one row.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
        if (count == 100) {
          return AsVariant(WriteState::NEED_MORE_DATA);
        }
        count++;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(100u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Assert that we have the right invalid rect.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isSome());
      EXPECT_EQ(IntRect(0, 0, 100, 1), invalidRect->mInputSpaceRect);
      EXPECT_EQ(IntRect(0, 0, 100, 1), invalidRect->mOutputSpaceRect);
    }

    {
      // Write eight rows.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
        if (count == 100 * 8) {
          return AsVariant(WriteState::NEED_MORE_DATA);
        }
        count++;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(100u * 8u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Assert that we have the right invalid rect.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isSome());
      EXPECT_EQ(IntRect(0, 1, 100, 8), invalidRect->mInputSpaceRect);
      EXPECT_EQ(IntRect(0, 1, 100, 8), invalidRect->mOutputSpaceRect);
    }

    {
      // Write the left half of one row.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
        if (count == 50) {
          return AsVariant(WriteState::NEED_MORE_DATA);
        }
        count++;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(50u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Assert that we don't have an invalid rect, since the invalid rect only
      // gets updated when a row gets completed.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isNothing());
    }

    {
      // Write the right half of the same row.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
        if (count == 50) {
          return AsVariant(WriteState::NEED_MORE_DATA);
        }
        count++;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(50u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Assert that we have the right invalid rect, which will include both the
      // left and right halves of this row now that we've completed it.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isSome());
      EXPECT_EQ(IntRect(0, 9, 100, 1), invalidRect->mInputSpaceRect);
      EXPECT_EQ(IntRect(0, 9, 100, 1), invalidRect->mOutputSpaceRect);
    }

    {
      // Write no rows.
      auto result = aSink->WritePixels<uint32_t>([&]() {
        return AsVariant(WriteState::NEED_MORE_DATA);
      });
      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Assert that we don't have an invalid rect.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isNothing());
    }

    {
      // Fill the rest of the image.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() {
        count++;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::FINISHED, result);
      EXPECT_EQ(100u * 90u, count);
      EXPECT_TRUE(aSink->IsSurfaceFinished());

      // Assert that we have the right invalid rect.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isSome());
      EXPECT_EQ(IntRect(0, 10, 100, 90), invalidRect->mInputSpaceRect);
      EXPECT_EQ(IntRect(0, 10, 100, 90), invalidRect->mOutputSpaceRect);

      // Check that the generated image is correct.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Green()));
    }
  });
}

TEST(ImageSurfaceSink, SurfaceSinkFlipVertically)
{
  WithSurfaceSink<Orient::FLIP_VERTICALLY>([](Decoder* aDecoder,
                                              SurfaceSink* aSink) {
    {
      // Fill the image with a first pass of red.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() {
        ++count;
        return AsVariant(BGRAColor::Red().AsPixel());
      });
      EXPECT_EQ(WriteState::FINISHED, result);
      EXPECT_EQ(100u * 100u, count);

      AssertCorrectPipelineFinalState(aSink,
                                      IntRect(0, 0, 100, 100),
                                      IntRect(0, 0, 100, 100));

      // Check that the generated image is correct.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Red()));
    }

    {
      // Reset for the second pass.
      aSink->ResetToFirstRow();
      EXPECT_FALSE(aSink->IsSurfaceFinished());
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isNothing());

      // Check that the generated image is still the first pass image.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Red()));
    }

    {
      // Fill 25 rows of the image with green and make sure everything is OK.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
        if (count == 25 * 100) {
          return AsVariant(WriteState::NEED_MORE_DATA);
        }
        count++;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(25u * 100u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Assert that we have the right invalid rect, which should include the
      // *bottom* (since we're flipping vertically) 25 rows of the image.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isSome());
      EXPECT_EQ(IntRect(0, 75, 100, 25), invalidRect->mInputSpaceRect);
      EXPECT_EQ(IntRect(0, 75, 100, 25), invalidRect->mOutputSpaceRect);

      // Check that the generated image is correct.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSurface();
      EXPECT_TRUE(RowsAreSolidColor(surface, 0, 75, BGRAColor::Red()));
      EXPECT_TRUE(RowsAreSolidColor(surface, 75, 25, BGRAColor::Green()));
    }

    {
      // Fill the rest of the image with a second pass of green.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() {
        ++count;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::FINISHED, result);
      EXPECT_EQ(75u * 100u, count);

      AssertCorrectPipelineFinalState(aSink,
                                      IntRect(0, 0, 100, 75),
                                      IntRect(0, 0, 100, 75));

      // Check that the generated image is correct.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Green()));
    }
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkInitialization)
{
  WithPalettedSurfaceSink(IntRect(0, 0, 100, 100),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    // Check initial state.
    EXPECT_FALSE(aSink->IsSurfaceFinished());
    Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
    EXPECT_TRUE(invalidRect.isNothing());

    // Check that the paletted image data is zero-initialized.
    RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
    uint8_t* imageData = nullptr;
    uint32_t imageLength = 0;
    currentFrame->GetImageData(&imageData, &imageLength);
    ASSERT_TRUE(imageData != nullptr);
    ASSERT_EQ(100u * 100u, imageLength);
    for (uint32_t i = 0; i < imageLength; ++i) {
      ASSERT_EQ(uint8_t(0), imageData[i]);
    }
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkWritePixelsFor0_0_100_100)
{
  WithPalettedSurfaceSink(IntRect(0, 0, 100, 100),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    CheckPalettedWritePixels(aDecoder, aSink);
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkWriteRowsFor0_0_100_100)
{
  WithPalettedSurfaceSink(IntRect(0, 0, 100, 100),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    CheckPalettedWriteRows(aDecoder, aSink);
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkWritePixelsFor25_25_50_50)
{
  WithPalettedSurfaceSink(IntRect(25, 25, 50, 50),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    CheckPalettedWritePixels(aDecoder, aSink,
                             /* aOutputRect = */ Some(IntRect(0, 0, 50, 50)),
                             /* aInputRect = */ Some(IntRect(0, 0, 50, 50)),
                             /* aInputWriteRect = */ Some(IntRect(25, 25, 50, 50)),
                             /* aOutputWriteRect = */ Some(IntRect(25, 25, 50, 50)));
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkWriteRowsFor25_25_50_50)
{
  WithPalettedSurfaceSink(IntRect(25, 25, 50, 50),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    CheckPalettedWriteRows(aDecoder, aSink,
                           /* aOutputRect = */ Some(IntRect(0, 0, 50, 50)),
                           /* aInputRect = */ Some(IntRect(0, 0, 50, 50)),
                           /* aInputWriteRect = */ Some(IntRect(25, 25, 50, 50)),
                           /* aOutputWriteRect = */ Some(IntRect(25, 25, 50, 50)));
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkWritePixelsForMinus25_Minus25_50_50)
{
  WithPalettedSurfaceSink(IntRect(-25, -25, 50, 50),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    CheckPalettedWritePixels(aDecoder, aSink,
                             /* aOutputRect = */ Some(IntRect(0, 0, 50, 50)),
                             /* aInputRect = */ Some(IntRect(0, 0, 50, 50)),
                             /* aInputWriteRect = */ Some(IntRect(-25, -25, 50, 50)),
                             /* aOutputWriteRect = */ Some(IntRect(-25, -25, 50, 50)));
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkWriteRowsForMinus25_Minus25_50_50)
{
  WithPalettedSurfaceSink(IntRect(-25, -25, 50, 50),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    CheckPalettedWriteRows(aDecoder, aSink,
                           /* aOutputRect = */ Some(IntRect(0, 0, 50, 50)),
                           /* aInputRect = */ Some(IntRect(0, 0, 50, 50)),
                           /* aInputWriteRect = */ Some(IntRect(-25, -25, 50, 50)),
                           /* aOutputWriteRect = */ Some(IntRect(-25, -25, 50, 50)));
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkWritePixelsFor75_Minus25_50_50)
{
  WithPalettedSurfaceSink(IntRect(75, -25, 50, 50),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    CheckPalettedWritePixels(aDecoder, aSink,
                             /* aOutputRect = */ Some(IntRect(0, 0, 50, 50)),
                             /* aInputRect = */ Some(IntRect(0, 0, 50, 50)),
                             /* aInputWriteRect = */ Some(IntRect(75, -25, 50, 50)),
                             /* aOutputWriteRect = */ Some(IntRect(75, -25, 50, 50)));
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkWriteRowsFor75_Minus25_50_50)
{
  WithPalettedSurfaceSink(IntRect(75, -25, 50, 50),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    CheckPalettedWriteRows(aDecoder, aSink,
                           /* aOutputRect = */ Some(IntRect(0, 0, 50, 50)),
                           /* aInputRect = */ Some(IntRect(0, 0, 50, 50)),
                           /* aInputWriteRect = */ Some(IntRect(75, -25, 50, 50)),
                           /* aOutputWriteRect = */ Some(IntRect(75, -25, 50, 50)));
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkWritePixelsForMinus25_75_50_50)
{
  WithPalettedSurfaceSink(IntRect(-25, 75, 50, 50),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    CheckPalettedWritePixels(aDecoder, aSink,
                             /* aOutputRect = */ Some(IntRect(0, 0, 50, 50)),
                             /* aInputRect = */ Some(IntRect(0, 0, 50, 50)),
                             /* aInputWriteRect = */ Some(IntRect(-25, 75, 50, 50)),
                             /* aOutputWriteRect = */ Some(IntRect(-25, 75, 50, 50)));
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkWriteRowsForMinus25_75_50_50)
{
  WithPalettedSurfaceSink(IntRect(-25, 75, 50, 50),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    CheckPalettedWriteRows(aDecoder, aSink,
                           /* aOutputRect = */ Some(IntRect(0, 0, 50, 50)),
                           /* aInputRect = */ Some(IntRect(0, 0, 50, 50)),
                           /* aInputWriteRect = */ Some(IntRect(-25, 75, 50, 50)),
                           /* aOutputWriteRect = */ Some(IntRect(-25, 75, 50, 50)));
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkWritePixelsFor75_75_50_50)
{
  WithPalettedSurfaceSink(IntRect(75, 75, 50, 50),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    CheckPalettedWritePixels(aDecoder, aSink,
                             /* aOutputRect = */ Some(IntRect(0, 0, 50, 50)),
                             /* aInputRect = */ Some(IntRect(0, 0, 50, 50)),
                             /* aInputWriteRect = */ Some(IntRect(75, 75, 50, 50)),
                             /* aOutputWriteRect = */ Some(IntRect(75, 75, 50, 50)));
  });
}

TEST(ImageSurfaceSink, PalettedSurfaceSinkWriteRowsFor75_75_50_50)
{
  WithPalettedSurfaceSink(IntRect(75, 75, 50, 50),
                          [](Decoder* aDecoder, PalettedSurfaceSink* aSink) {
    CheckPalettedWriteRows(aDecoder, aSink,
                           /* aOutputRect = */ Some(IntRect(0, 0, 50, 50)),
                           /* aInputRect = */ Some(IntRect(0, 0, 50, 50)),
                           /* aInputWriteRect = */ Some(IntRect(75, 75, 50, 50)),
                           /* aOutputWriteRect = */ Some(IntRect(75, 75, 50, 50)));
  });
}
