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
void WithDeinterlacingFilter(const IntSize& aSize, bool aProgressiveDisplay,
                             Func aFunc) {
  RefPtr<image::Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(bool(decoder));

  WithFilterPipeline(
      decoder, std::forward<Func>(aFunc),
      DeinterlacingConfig<uint32_t>{aProgressiveDisplay},
      SurfaceConfig{decoder, aSize, SurfaceFormat::OS_RGBA, false});
}

void AssertConfiguringDeinterlacingFilterFails(const IntSize& aSize) {
  RefPtr<image::Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AssertConfiguringPipelineFails(
      decoder, DeinterlacingConfig<uint32_t>{/* mProgressiveDisplay = */ true},
      SurfaceConfig{decoder, aSize, SurfaceFormat::OS_RGBA, false});
}

class ImageDeinterlacingFilter : public ::testing::Test {
 protected:
  AutoInitializeImageLib mInit;
};

TEST_F(ImageDeinterlacingFilter, WritePixels100_100) {
  WithDeinterlacingFilter(
      IntSize(100, 100), /* aProgressiveDisplay = */ true,
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)));
      });
}

TEST_F(ImageDeinterlacingFilter, WritePixels99_99) {
  WithDeinterlacingFilter(IntSize(99, 99), /* aProgressiveDisplay = */ true,
                          [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
                            CheckWritePixels(
                                aDecoder, aFilter,
                                /* aOutputRect = */ Some(IntRect(0, 0, 99, 99)),
                                /* aInputRect = */ Some(IntRect(0, 0, 99, 99)));
                          });
}

TEST_F(ImageDeinterlacingFilter, WritePixels8_8) {
  WithDeinterlacingFilter(IntSize(8, 8), /* aProgressiveDisplay = */ true,
                          [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
                            CheckWritePixels(
                                aDecoder, aFilter,
                                /* aOutputRect = */ Some(IntRect(0, 0, 8, 8)),
                                /* aInputRect = */ Some(IntRect(0, 0, 8, 8)));
                          });
}

TEST_F(ImageDeinterlacingFilter, WritePixels7_7) {
  WithDeinterlacingFilter(IntSize(7, 7), /* aProgressiveDisplay = */ true,
                          [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
                            CheckWritePixels(
                                aDecoder, aFilter,
                                /* aOutputRect = */ Some(IntRect(0, 0, 7, 7)),
                                /* aInputRect = */ Some(IntRect(0, 0, 7, 7)));
                          });
}

TEST_F(ImageDeinterlacingFilter, WritePixels3_3) {
  WithDeinterlacingFilter(IntSize(3, 3), /* aProgressiveDisplay = */ true,
                          [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
                            CheckWritePixels(
                                aDecoder, aFilter,
                                /* aOutputRect = */ Some(IntRect(0, 0, 3, 3)),
                                /* aInputRect = */ Some(IntRect(0, 0, 3, 3)));
                          });
}

TEST_F(ImageDeinterlacingFilter, WritePixels1_1) {
  WithDeinterlacingFilter(IntSize(1, 1), /* aProgressiveDisplay = */ true,
                          [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
                            CheckWritePixels(
                                aDecoder, aFilter,
                                /* aOutputRect = */ Some(IntRect(0, 0, 1, 1)),
                                /* aInputRect = */ Some(IntRect(0, 0, 1, 1)));
                          });
}

TEST_F(ImageDeinterlacingFilter, WritePixelsNonProgressiveOutput51_52) {
  WithDeinterlacingFilter(
      IntSize(51, 52), /* aProgressiveDisplay = */ false,
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Fill the image. The output should be green for even rows and red for
        // odd rows but we need to write the rows in the order that the
        // deinterlacer expects them.
        uint32_t count = 0;
        auto result = aFilter->WritePixels<uint32_t>([&]() {
          uint32_t row = count / 51;  // Integer division.
          ++count;

          // Note that we use a switch statement here, even though it's quite
          // verbose, because it's useful to have the mappings between input and
          // output rows available when debugging these tests.

          switch (row) {
            // First pass. Output rows are positioned at 8n + 0.
            case 0:  // Output row 0.
            case 1:  // Output row 8.
            case 2:  // Output row 16.
            case 3:  // Output row 24.
            case 4:  // Output row 32.
            case 5:  // Output row 40.
            case 6:  // Output row 48.
              return AsVariant(BGRAColor::Green().AsPixel());

            // Second pass. Rows are positioned at 8n + 4.
            case 7:   // Output row 4.
            case 8:   // Output row 12.
            case 9:   // Output row 20.
            case 10:  // Output row 28.
            case 11:  // Output row 36.
            case 12:  // Output row 44.
              return AsVariant(BGRAColor::Green().AsPixel());

            // Third pass. Rows are positioned at 4n + 2.
            case 13:  // Output row 2.
            case 14:  // Output row 6.
            case 15:  // Output row 10.
            case 16:  // Output row 14.
            case 17:  // Output row 18.
            case 18:  // Output row 22.
            case 19:  // Output row 26.
            case 20:  // Output row 30.
            case 21:  // Output row 34.
            case 22:  // Output row 38.
            case 23:  // Output row 42.
            case 24:  // Output row 46.
            case 25:  // Output row 50.
              return AsVariant(BGRAColor::Green().AsPixel());

            // Fourth pass. Rows are positioned at 2n + 1.
            case 26:  // Output row 1.
            case 27:  // Output row 3.
            case 28:  // Output row 5.
            case 29:  // Output row 7.
            case 30:  // Output row 9.
            case 31:  // Output row 11.
            case 32:  // Output row 13.
            case 33:  // Output row 15.
            case 34:  // Output row 17.
            case 35:  // Output row 19.
            case 36:  // Output row 21.
            case 37:  // Output row 23.
            case 38:  // Output row 25.
            case 39:  // Output row 27.
            case 40:  // Output row 29.
            case 41:  // Output row 31.
            case 42:  // Output row 33.
            case 43:  // Output row 35.
            case 44:  // Output row 37.
            case 45:  // Output row 39.
            case 46:  // Output row 41.
            case 47:  // Output row 43.
            case 48:  // Output row 45.
            case 49:  // Output row 47.
            case 50:  // Output row 49.
            case 51:  // Output row 51.
              return AsVariant(BGRAColor::Red().AsPixel());

            default:
              MOZ_ASSERT_UNREACHABLE("Unexpected row");
              return AsVariant(BGRAColor::Transparent().AsPixel());
          }
        });
        EXPECT_EQ(WriteState::FINISHED, result);
        EXPECT_EQ(51u * 52u, count);

        AssertCorrectPipelineFinalState(aFilter, IntRect(0, 0, 51, 52),
                                        IntRect(0, 0, 51, 52));

        // Check that the generated image is correct. As mentioned above, we
        // expect even rows to be green and odd rows to be red.
        RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
        RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();

        for (uint32_t row = 0; row < 52; ++row) {
          EXPECT_TRUE(RowsAreSolidColor(
              surface, row, 1,
              row % 2 == 0 ? BGRAColor::Green() : BGRAColor::Red()));
        }
      });
}

TEST_F(ImageDeinterlacingFilter, WritePixelsOutput20_20) {
  WithDeinterlacingFilter(
      IntSize(20, 20), /* aProgressiveDisplay = */ true,
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Fill the image. The output should be green for even rows and red for
        // odd rows but we need to write the rows in the order that the
        // deinterlacer expects them.
        uint32_t count = 0;
        auto result = aFilter->WritePixels<uint32_t>([&]() {
          uint32_t row = count / 20;  // Integer division.
          ++count;

          // Note that we use a switch statement here, even though it's quite
          // verbose, because it's useful to have the mappings between input and
          // output rows available when debugging these tests.

          switch (row) {
            // First pass. Output rows are positioned at 8n + 0.
            case 0:  // Output row 0.
            case 1:  // Output row 8.
            case 2:  // Output row 16.
              return AsVariant(BGRAColor::Green().AsPixel());

            // Second pass. Rows are positioned at 8n + 4.
            case 3:  // Output row 4.
            case 4:  // Output row 12.
              return AsVariant(BGRAColor::Green().AsPixel());

            // Third pass. Rows are positioned at 4n + 2.
            case 5:  // Output row 2.
            case 6:  // Output row 6.
            case 7:  // Output row 10.
            case 8:  // Output row 14.
            case 9:  // Output row 18.
              return AsVariant(BGRAColor::Green().AsPixel());

            // Fourth pass. Rows are positioned at 2n + 1.
            case 10:  // Output row 1.
            case 11:  // Output row 3.
            case 12:  // Output row 5.
            case 13:  // Output row 7.
            case 14:  // Output row 9.
            case 15:  // Output row 11.
            case 16:  // Output row 13.
            case 17:  // Output row 15.
            case 18:  // Output row 17.
            case 19:  // Output row 19.
              return AsVariant(BGRAColor::Red().AsPixel());

            default:
              MOZ_ASSERT_UNREACHABLE("Unexpected row");
              return AsVariant(BGRAColor::Transparent().AsPixel());
          }
        });
        EXPECT_EQ(WriteState::FINISHED, result);
        EXPECT_EQ(20u * 20u, count);

        AssertCorrectPipelineFinalState(aFilter, IntRect(0, 0, 20, 20),
                                        IntRect(0, 0, 20, 20));

        // Check that the generated image is correct. As mentioned above, we
        // expect even rows to be green and odd rows to be red.
        RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
        RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();

        for (uint32_t row = 0; row < 20; ++row) {
          EXPECT_TRUE(RowsAreSolidColor(
              surface, row, 1,
              row % 2 == 0 ? BGRAColor::Green() : BGRAColor::Red()));
        }
      });
}

TEST_F(ImageDeinterlacingFilter, WritePixelsOutput7_7) {
  WithDeinterlacingFilter(
      IntSize(7, 7), /* aProgressiveDisplay = */ true,
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Fill the image. The output should be a repeating pattern of two green
        // rows followed by two red rows but we need to write the rows in the
        // order that the deinterlacer expects them.
        uint32_t count = 0;
        auto result = aFilter->WritePixels<uint32_t>([&]() {
          uint32_t row = count / 7;  // Integer division.
          ++count;

          switch (row) {
            // First pass. Output rows are positioned at 8n + 0.
            case 0:  // Output row 0.
              return AsVariant(BGRAColor::Green().AsPixel());

            // Second pass. Rows are positioned at 8n + 4.
            case 1:  // Output row 4.
              return AsVariant(BGRAColor::Green().AsPixel());

            // Third pass. Rows are positioned at 4n + 2.
            case 2:  // Output row 2.
            case 3:  // Output row 6.
              return AsVariant(BGRAColor::Red().AsPixel());

            // Fourth pass. Rows are positioned at 2n + 1.
            case 4:  // Output row 1.
              return AsVariant(BGRAColor::Green().AsPixel());

            case 5:  // Output row 3.
              return AsVariant(BGRAColor::Red().AsPixel());

            case 6:  // Output row 5.
              return AsVariant(BGRAColor::Green().AsPixel());

            default:
              MOZ_ASSERT_UNREACHABLE("Unexpected row");
              return AsVariant(BGRAColor::Transparent().AsPixel());
          }
        });
        EXPECT_EQ(WriteState::FINISHED, result);
        EXPECT_EQ(7u * 7u, count);

        AssertCorrectPipelineFinalState(aFilter, IntRect(0, 0, 7, 7),
                                        IntRect(0, 0, 7, 7));

        // Check that the generated image is correct. As mentioned above, we
        // expect two green rows, followed by two red rows, then two green rows,
        // etc.
        RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
        RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();

        for (uint32_t row = 0; row < 7; ++row) {
          BGRAColor color = row == 0 || row == 1 || row == 4 || row == 5
                                ? BGRAColor::Green()
                                : BGRAColor::Red();
          EXPECT_TRUE(RowsAreSolidColor(surface, row, 1, color));
        }
      });
}

TEST_F(ImageDeinterlacingFilter, WritePixelsOutput3_3) {
  WithDeinterlacingFilter(
      IntSize(3, 3), /* aProgressiveDisplay = */ true,
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Fill the image. The output should be green, red, green in that order,
        // but we need to write the rows in the order that the deinterlacer
        // expects them.
        uint32_t count = 0;
        auto result = aFilter->WritePixels<uint32_t>([&]() {
          uint32_t row = count / 3;  // Integer division.
          ++count;

          switch (row) {
            // First pass. Output rows are positioned at 8n + 0.
            case 0:  // Output row 0.
              return AsVariant(BGRAColor::Green().AsPixel());

            // Second pass. Rows are positioned at 8n + 4.
            // No rows for this pass.

            // Third pass. Rows are positioned at 4n + 2.
            case 1:  // Output row 2.
              return AsVariant(BGRAColor::Green().AsPixel());

            // Fourth pass. Rows are positioned at 2n + 1.
            case 2:  // Output row 1.
              return AsVariant(BGRAColor::Red().AsPixel());

            default:
              MOZ_ASSERT_UNREACHABLE("Unexpected row");
              return AsVariant(BGRAColor::Transparent().AsPixel());
          }
        });
        EXPECT_EQ(WriteState::FINISHED, result);
        EXPECT_EQ(3u * 3u, count);

        AssertCorrectPipelineFinalState(aFilter, IntRect(0, 0, 3, 3),
                                        IntRect(0, 0, 3, 3));

        // Check that the generated image is correct. As mentioned above, we
        // expect green, red, green in that order.
        RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
        RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();

        for (uint32_t row = 0; row < 3; ++row) {
          EXPECT_TRUE(RowsAreSolidColor(
              surface, row, 1,
              row == 0 || row == 2 ? BGRAColor::Green() : BGRAColor::Red()));
        }
      });
}

TEST_F(ImageDeinterlacingFilter, WritePixelsOutput1_1) {
  WithDeinterlacingFilter(
      IntSize(1, 1), /* aProgressiveDisplay = */ true,
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Fill the image. The output should be a single red row.
        uint32_t count = 0;
        auto result = aFilter->WritePixels<uint32_t>([&]() {
          ++count;
          return AsVariant(BGRAColor::Red().AsPixel());
        });
        EXPECT_EQ(WriteState::FINISHED, result);
        EXPECT_EQ(1u, count);

        AssertCorrectPipelineFinalState(aFilter, IntRect(0, 0, 1, 1),
                                        IntRect(0, 0, 1, 1));

        // Check that the generated image is correct. As mentioned above, we
        // expect a single red row.
        RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
        RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();

        EXPECT_TRUE(RowsAreSolidColor(surface, 0, 1, BGRAColor::Red()));
      });
}

void WriteRowAndCheckInterlacerOutput(image::Decoder* aDecoder,
                                      SurfaceFilter* aFilter, BGRAColor aColor,
                                      WriteState aNextState,
                                      IntRect aInvalidRect,
                                      uint32_t aFirstHaeberliRow,
                                      uint32_t aLastHaeberliRow) {
  uint32_t count = 0;

  auto result = aFilter->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
    if (count < 7) {
      ++count;
      return AsVariant(aColor.AsPixel());
    }
    return AsVariant(WriteState::NEED_MORE_DATA);
  });

  EXPECT_EQ(aNextState, result);
  EXPECT_EQ(7u, count);

  // Assert that we got the expected invalidation region.
  Maybe<SurfaceInvalidRect> invalidRect = aFilter->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isSome());
  EXPECT_EQ(aInvalidRect, invalidRect->mInputSpaceRect);
  EXPECT_EQ(aInvalidRect, invalidRect->mOutputSpaceRect);

  // Check that the portion of the image generated so far is correct. The rows
  // from aFirstHaeberliRow to aLastHaeberliRow should be filled with aColor.
  // Note that this is not the same as the set of rows in aInvalidRect, because
  // after writing a row the deinterlacer seeks to the next row to write, which
  // may involve copying previously-written rows in the buffer to the output
  // even though they don't change in this pass.
  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();

  for (uint32_t row = aFirstHaeberliRow; row <= aLastHaeberliRow; ++row) {
    EXPECT_TRUE(RowsAreSolidColor(surface, row, 1, aColor));
  }
}

TEST_F(ImageDeinterlacingFilter, WritePixelsIntermediateOutput7_7) {
  WithDeinterlacingFilter(
      IntSize(7, 7), /* aProgressiveDisplay = */ true,
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Fill the image. The output should be a repeating pattern of two green
        // rows followed by two red rows but we need to write the rows in the
        // order that the deinterlacer expects them.

        // First pass. Output rows are positioned at 8n + 0.

        // Output row 0. The invalid rect is the entire image because this is
        // the end of the first pass.
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Green(),
                                         WriteState::NEED_MORE_DATA,
                                         IntRect(0, 0, 7, 7), 0, 4);

        // Second pass. Rows are positioned at 8n + 4.

        // Output row 4. The invalid rect is the entire image because this is
        // the end of the second pass.
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Green(),
                                         WriteState::NEED_MORE_DATA,
                                         IntRect(0, 0, 7, 7), 1, 4);

        // Third pass. Rows are positioned at 4n + 2.

        // Output row 2. The invalid rect contains the Haeberli rows for this
        // output row (rows 2 and 3) as well as the rows that we copy from
        // previous passes when seeking to the next output row (rows 4 and 5).
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Red(),
                                         WriteState::NEED_MORE_DATA,
                                         IntRect(0, 2, 7, 4), 2, 3);

        // Output row 6. The invalid rect is the entire image because this is
        // the end of the third pass.
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Red(),
                                         WriteState::NEED_MORE_DATA,
                                         IntRect(0, 0, 7, 7), 6, 6);

        // Fourth pass. Rows are positioned at 2n + 1.

        // Output row 1. The invalid rect contains the Haeberli rows for this
        // output row (just row 1) as well as the rows that we copy from
        // previous passes when seeking to the next output row (row 2).
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Green(),
                                         WriteState::NEED_MORE_DATA,
                                         IntRect(0, 1, 7, 2), 1, 1);

        // Output row 3. The invalid rect contains the Haeberli rows for this
        // output row (just row 3) as well as the rows that we copy from
        // previous passes when seeking to the next output row (row 4).
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Red(),
                                         WriteState::NEED_MORE_DATA,
                                         IntRect(0, 3, 7, 2), 3, 3);

        // Output row 5. The invalid rect contains the Haeberli rows for this
        // output row (just row 5) as well as the rows that we copy from
        // previous passes when seeking to the next output row (row 6).
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Green(),
                                         WriteState::FINISHED,
                                         IntRect(0, 5, 7, 2), 5, 5);

        // Assert that we're in the expected final state.
        EXPECT_TRUE(aFilter->IsSurfaceFinished());
        Maybe<SurfaceInvalidRect> invalidRect = aFilter->TakeInvalidRect();
        EXPECT_TRUE(invalidRect.isNothing());

        // Check that the generated image is correct. As mentioned above, we
        // expect two green rows, followed by two red rows, then two green rows,
        // etc.
        RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
        RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();

        for (uint32_t row = 0; row < 7; ++row) {
          BGRAColor color = row == 0 || row == 1 || row == 4 || row == 5
                                ? BGRAColor::Green()
                                : BGRAColor::Red();
          EXPECT_TRUE(RowsAreSolidColor(surface, row, 1, color));
        }
      });
}

TEST_F(ImageDeinterlacingFilter,
       WritePixelsNonProgressiveIntermediateOutput7_7) {
  WithDeinterlacingFilter(
      IntSize(7, 7), /* aProgressiveDisplay = */ false,
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Fill the image. The output should be a repeating pattern of two green
        // rows followed by two red rows but we need to write the rows in the
        // order that the deinterlacer expects them.

        // First pass. Output rows are positioned at 8n + 0.

        // Output row 0. The invalid rect is the entire image because this is
        // the end of the first pass.
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Green(),
                                         WriteState::NEED_MORE_DATA,
                                         IntRect(0, 0, 7, 7), 0, 0);

        // Second pass. Rows are positioned at 8n + 4.

        // Output row 4. The invalid rect is the entire image because this is
        // the end of the second pass.
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Green(),
                                         WriteState::NEED_MORE_DATA,
                                         IntRect(0, 0, 7, 7), 4, 4);

        // Third pass. Rows are positioned at 4n + 2.

        // Output row 2. The invalid rect contains the Haeberli rows for this
        // output row (rows 2 and 3) as well as the rows that we copy from
        // previous passes when seeking to the next output row (rows 4 and 5).
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Red(),
                                         WriteState::NEED_MORE_DATA,
                                         IntRect(0, 2, 7, 4), 2, 2);

        // Output row 6. The invalid rect is the entire image because this is
        // the end of the third pass.
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Red(),
                                         WriteState::NEED_MORE_DATA,
                                         IntRect(0, 0, 7, 7), 6, 6);

        // Fourth pass. Rows are positioned at 2n + 1.

        // Output row 1. The invalid rect contains the Haeberli rows for this
        // output row (just row 1) as well as the rows that we copy from
        // previous passes when seeking to the next output row (row 2).
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Green(),
                                         WriteState::NEED_MORE_DATA,
                                         IntRect(0, 1, 7, 2), 1, 1);

        // Output row 3. The invalid rect contains the Haeberli rows for this
        // output row (just row 3) as well as the rows that we copy from
        // previous passes when seeking to the next output row (row 4).
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Red(),
                                         WriteState::NEED_MORE_DATA,
                                         IntRect(0, 3, 7, 2), 3, 3);

        // Output row 5. The invalid rect contains the Haeberli rows for this
        // output row (just row 5) as well as the rows that we copy from
        // previous passes when seeking to the next output row (row 6).
        WriteRowAndCheckInterlacerOutput(aDecoder, aFilter, BGRAColor::Green(),
                                         WriteState::FINISHED,
                                         IntRect(0, 5, 7, 2), 5, 5);

        // Assert that we're in the expected final state.
        EXPECT_TRUE(aFilter->IsSurfaceFinished());
        Maybe<SurfaceInvalidRect> invalidRect = aFilter->TakeInvalidRect();
        EXPECT_TRUE(invalidRect.isNothing());

        // Check that the generated image is correct. As mentioned above, we
        // expect two green rows, followed by two red rows, then two green rows,
        // etc.
        RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
        RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();

        for (uint32_t row = 0; row < 7; ++row) {
          BGRAColor color = row == 0 || row == 1 || row == 4 || row == 5
                                ? BGRAColor::Green()
                                : BGRAColor::Red();
          EXPECT_TRUE(RowsAreSolidColor(surface, row, 1, color));
        }
      });
}

TEST_F(ImageDeinterlacingFilter, DeinterlacingFailsFor0_0) {
  // A 0x0 input size is invalid, so configuration should fail.
  AssertConfiguringDeinterlacingFilterFails(IntSize(0, 0));
}

TEST_F(ImageDeinterlacingFilter, DeinterlacingFailsForMinus1_Minus1) {
  // A negative input size is invalid, so configuration should fail.
  AssertConfiguringDeinterlacingFilterFails(IntSize(-1, -1));
}
