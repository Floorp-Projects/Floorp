/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <vector>

#include "gtest/gtest.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/Maybe.h"
#include "Common.h"
#include "Decoder.h"
#include "DecoderFactory.h"
#include "SourceBuffer.h"
#include "SurfaceFilters.h"
#include "SurfacePipe.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

using std::generate;
using std::vector;

template <typename Func>
void WithADAM7InterpolatingFilter(const IntSize& aSize, Func aFunc) {
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(bool(decoder));

  WithFilterPipeline(
      decoder, std::forward<Func>(aFunc), ADAM7InterpolatingConfig{},
      SurfaceConfig{decoder, aSize, SurfaceFormat::B8G8R8A8, false});
}

void AssertConfiguringADAM7InterpolatingFilterFails(const IntSize& aSize) {
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(bool(decoder));

  AssertConfiguringPipelineFails(
      decoder, ADAM7InterpolatingConfig{},
      SurfaceConfig{decoder, aSize, SurfaceFormat::B8G8R8A8, false});
}

uint8_t InterpolateByte(uint8_t aByteA, uint8_t aByteB, float aWeight) {
  return uint8_t(aByteA * aWeight + aByteB * (1.0f - aWeight));
}

BGRAColor InterpolateColors(BGRAColor aColor1, BGRAColor aColor2,
                            float aWeight) {
  return BGRAColor(InterpolateByte(aColor1.mBlue, aColor2.mBlue, aWeight),
                   InterpolateByte(aColor1.mGreen, aColor2.mGreen, aWeight),
                   InterpolateByte(aColor1.mRed, aColor2.mRed, aWeight),
                   InterpolateByte(aColor1.mAlpha, aColor2.mAlpha, aWeight));
}

enum class ShouldInterpolate { eYes, eNo };

BGRAColor HorizontallyInterpolatedPixel(uint32_t aCol, uint32_t aWidth,
                                        const vector<float>& aWeights,
                                        ShouldInterpolate aShouldInterpolate,
                                        const vector<BGRAColor>& aColors) {
  // We cycle through the vector of weights forever.
  float weight = aWeights[aCol % aWeights.size()];

  // Find the columns of the two final pixels for this set of weights.
  uint32_t finalPixel1 = aCol - aCol % aWeights.size();
  uint32_t finalPixel2 = finalPixel1 + aWeights.size();

  // If |finalPixel2| is past the end of the row, that means that there is no
  // final pixel after the pixel at |finalPixel1|. In that case, we just want to
  // duplicate |finalPixel1|'s color until the end of the row. We can do that by
  // setting |finalPixel2| equal to |finalPixel1| so that the interpolation has
  // no effect.
  if (finalPixel2 >= aWidth) {
    finalPixel2 = finalPixel1;
  }

  // We cycle through the vector of colors forever (subject to the above
  // constraint about the end of the row).
  BGRAColor color1 = aColors[finalPixel1 % aColors.size()];
  BGRAColor color2 = aColors[finalPixel2 % aColors.size()];

  // If we're not interpolating, we treat all pixels which aren't final as
  // transparent. Since the number of weights we have is equal to the stride
  // between final pixels, we can check if |aCol| is a final pixel by checking
  // whether |aCol| is a multiple of |aWeights.size()|.
  if (aShouldInterpolate == ShouldInterpolate::eNo) {
    return aCol % aWeights.size() == 0 ? color1 : BGRAColor::Transparent();
  }

  // Interpolate.
  return InterpolateColors(color1, color2, weight);
}

vector<float>& InterpolationWeights(int32_t aStride) {
  // Precalculated interpolation weights. These are used to interpolate
  // between final pixels or between important rows. Although no interpolation
  // is actually applied to the previous final pixel or important row value,
  // the arrays still start with 1.0f, which is always skipped, primarily
  // because otherwise |stride1Weights| would have zero elements.
  static vector<float> stride8Weights = {1.0f,     7 / 8.0f, 6 / 8.0f,
                                         5 / 8.0f, 4 / 8.0f, 3 / 8.0f,
                                         2 / 8.0f, 1 / 8.0f};
  static vector<float> stride4Weights = {1.0f, 3 / 4.0f, 2 / 4.0f, 1 / 4.0f};
  static vector<float> stride2Weights = {1.0f, 1 / 2.0f};
  static vector<float> stride1Weights = {1.0f};

  switch (aStride) {
    case 8:
      return stride8Weights;
    case 4:
      return stride4Weights;
    case 2:
      return stride2Weights;
    case 1:
      return stride1Weights;
    default:
      MOZ_CRASH();
  }
}

int32_t ImportantRowStride(uint8_t aPass) {
  // The stride between important rows for each pass, with a dummy value for
  // the nonexistent pass 0 and for pass 8, since the tests run an extra pass to
  // make sure nothing breaks.
  static int32_t strides[] = {1, 8, 8, 4, 4, 2, 2, 1, 1};

  return strides[aPass];
}

size_t FinalPixelStride(uint8_t aPass) {
  // The stride between the final pixels in important rows for each pass, with
  // a dummy value for the nonexistent pass 0 and for pass 8, since the tests
  // run an extra pass to make sure nothing breaks.
  static size_t strides[] = {1, 8, 4, 4, 2, 2, 1, 1, 1};

  return strides[aPass];
}

bool IsImportantRow(int32_t aRow, uint8_t aPass) {
  return aRow % ImportantRowStride(aPass) == 0;
}

/**
 * ADAM7 breaks up the image into 8x8 blocks. On each of the 7 passes, a new
 * set of pixels in each block receives their final values, according to the
 * following pattern:
 *
 *    1 6 4 6 2 6 4 6
 *    7 7 7 7 7 7 7 7
 *    5 6 5 6 5 6 5 6
 *    7 7 7 7 7 7 7 7
 *    3 6 4 6 3 6 4 6
 *    7 7 7 7 7 7 7 7
 *    5 6 5 6 5 6 5 6
 *    7 7 7 7 7 7 7 7
 *
 * This function produces a row of pixels @aWidth wide, suitable for testing
 * horizontal interpolation on pass @aPass. The pattern of pixels used is
 * determined by @aPass and @aRow, which determine which pixels are final
 * according to the table above, and @aColors, from which the pixel values
 * are selected.
 *
 * There are two different behaviors: if |eNo| is passed for
 * @aShouldInterpolate, non-final pixels are treated as transparent. If |eNo|
 * is passed, non-final pixels get interpolated in from the surrounding final
 * pixels. The intention is that |eNo| is passed to generate input which will
 * be run through ADAM7InterpolatingFilter, and |eYes| is passed to generate
 * reference data to check that the filter is performing horizontal
 * interpolation correctly.
 *
 * This function does not perform vertical interpolation. Rows which aren't on
 * the current pass are filled with transparent pixels.
 *
 * @return a vector<BGRAColor> representing a row of pixels.
 */
vector<BGRAColor> ADAM7HorizontallyInterpolatedRow(
    uint8_t aPass, uint32_t aRow, uint32_t aWidth,
    ShouldInterpolate aShouldInterpolate, const vector<BGRAColor>& aColors) {
  EXPECT_GT(aPass, 0);
  EXPECT_LE(aPass, 8);
  EXPECT_GT(aColors.size(), 0u);

  vector<BGRAColor> result(aWidth);

  if (IsImportantRow(aRow, aPass)) {
    vector<float>& weights = InterpolationWeights(FinalPixelStride(aPass));

    // Compute the horizontally interpolated row.
    uint32_t col = 0;
    generate(result.begin(), result.end(), [&] {
      return HorizontallyInterpolatedPixel(col++, aWidth, weights,
                                           aShouldInterpolate, aColors);
    });
  } else {
    // This is an unimportant row; just make the entire thing transparent.
    generate(result.begin(), result.end(),
             [] { return BGRAColor::Transparent(); });
  }

  EXPECT_EQ(result.size(), size_t(aWidth));

  return result;
}

WriteState WriteUninterpolatedPixels(SurfaceFilter* aFilter,
                                     const IntSize& aSize, uint8_t aPass,
                                     const vector<BGRAColor>& aColors) {
  WriteState result = WriteState::NEED_MORE_DATA;

  for (int32_t row = 0; row < aSize.height; ++row) {
    // Compute uninterpolated pixels for this row.
    vector<BGRAColor> pixels = ADAM7HorizontallyInterpolatedRow(
        aPass, row, aSize.width, ShouldInterpolate::eNo, aColors);

    // Write them to the surface.
    auto pixelIterator = pixels.cbegin();
    result = aFilter->WritePixelsToRow<uint32_t>(
        [&] { return AsVariant((*pixelIterator++).AsPixel()); });

    if (result != WriteState::NEED_MORE_DATA) {
      break;
    }
  }

  return result;
}

bool CheckHorizontallyInterpolatedImage(Decoder* aDecoder, const IntSize& aSize,
                                        uint8_t aPass,
                                        const vector<BGRAColor>& aColors) {
  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();

  for (int32_t row = 0; row < aSize.height; ++row) {
    if (!IsImportantRow(row, aPass)) {
      continue;  // Don't check rows which aren't important on this pass.
    }

    // Compute the expected pixels, *with* interpolation to match what the
    // filter should have done.
    vector<BGRAColor> expectedPixels = ADAM7HorizontallyInterpolatedRow(
        aPass, row, aSize.width, ShouldInterpolate::eYes, aColors);

    if (!RowHasPixels(surface, row, expectedPixels)) {
      return false;
    }
  }

  return true;
}

void CheckHorizontalInterpolation(const IntSize& aSize,
                                  const vector<BGRAColor>& aColors) {
  const IntRect surfaceRect(IntPoint(0, 0), aSize);

  WithADAM7InterpolatingFilter(
      aSize, [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        // We check horizontal interpolation behavior for each pass
        // individually. In addition to the normal 7 passes that ADAM7 includes,
        // we also check an eighth pass to verify that nothing breaks if extra
        // data is written.
        for (uint8_t pass = 1; pass <= 8; ++pass) {
          // Write our color pattern to the surface. We don't perform any
          // interpolation when writing to the filter so that we can check that
          // the filter itself *does*.
          WriteState result =
              WriteUninterpolatedPixels(aFilter, aSize, pass, aColors);

          EXPECT_EQ(WriteState::FINISHED, result);
          AssertCorrectPipelineFinalState(aFilter, surfaceRect, surfaceRect);

          // Check that the generated image matches the expected pattern, with
          // interpolation applied.
          EXPECT_TRUE(CheckHorizontallyInterpolatedImage(aDecoder, aSize, pass,
                                                         aColors));

          // Prepare for the next pass.
          aFilter->ResetToFirstRow();
        }
      });
}

BGRAColor ADAM7RowColor(int32_t aRow, uint8_t aPass,
                        const vector<BGRAColor>& aColors) {
  EXPECT_LT(0, aPass);
  EXPECT_GE(8, aPass);
  EXPECT_LT(0u, aColors.size());

  // If this is an important row, select the color from the provided vector of
  // colors, which we cycle through infinitely. If not, just fill the row with
  // transparent pixels.
  return IsImportantRow(aRow, aPass) ? aColors[aRow % aColors.size()]
                                     : BGRAColor::Transparent();
}

WriteState WriteRowColorPixels(SurfaceFilter* aFilter, const IntSize& aSize,
                               uint8_t aPass,
                               const vector<BGRAColor>& aColors) {
  WriteState result = WriteState::NEED_MORE_DATA;

  for (int32_t row = 0; row < aSize.height; ++row) {
    const uint32_t color = ADAM7RowColor(row, aPass, aColors).AsPixel();

    // Fill the surface with |color| pixels.
    result =
        aFilter->WritePixelsToRow<uint32_t>([&] { return AsVariant(color); });

    if (result != WriteState::NEED_MORE_DATA) {
      break;
    }
  }

  return result;
}

bool CheckVerticallyInterpolatedImage(Decoder* aDecoder, const IntSize& aSize,
                                      uint8_t aPass,
                                      const vector<BGRAColor>& aColors) {
  vector<float>& weights = InterpolationWeights(ImportantRowStride(aPass));

  for (int32_t row = 0; row < aSize.height; ++row) {
    // Vertically interpolation takes place between two important rows. The
    // separation between the important rows is determined by the stride of this
    // pass. When there is no "next" important row because we'd run off the
    // bottom of the image, we use the same row for both. This matches
    // ADAM7InterpolatingFilter's behavior of duplicating the last important row
    // since there isn't another important row to vertically interpolate it
    // with.
    const int32_t stride = ImportantRowStride(aPass);
    const int32_t prevImportantRow = row - row % stride;
    const int32_t maybeNextImportantRow = prevImportantRow + stride;
    const int32_t nextImportantRow = maybeNextImportantRow < aSize.height
                                         ? maybeNextImportantRow
                                         : prevImportantRow;

    // Retrieve the colors for the important rows we're going to interpolate.
    const BGRAColor prevImportantRowColor =
        ADAM7RowColor(prevImportantRow, aPass, aColors);
    const BGRAColor nextImportantRowColor =
        ADAM7RowColor(nextImportantRow, aPass, aColors);

    // The weight we'll use for interpolation is also determined by the stride.
    // A row halfway between two important rows should have pixels that have a
    // 50% contribution from each of the important rows, for example.
    const float weight = weights[row % stride];
    const BGRAColor interpolatedColor =
        InterpolateColors(prevImportantRowColor, nextImportantRowColor, weight);

    // Generate a row of expected pixels. Every pixel in the row is always the
    // same color since we're only testing vertical interpolation between
    // solid-colored rows.
    vector<BGRAColor> expectedPixels(aSize.width);
    generate(expectedPixels.begin(), expectedPixels.end(),
             [&] { return interpolatedColor; });

    // Check that the pixels match.
    RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
    RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
    if (!RowHasPixels(surface, row, expectedPixels)) {
      return false;
    }
  }

  return true;
}

void CheckVerticalInterpolation(const IntSize& aSize,
                                const vector<BGRAColor>& aColors) {
  const IntRect surfaceRect(IntPoint(0, 0), aSize);

  WithADAM7InterpolatingFilter(aSize, [&](Decoder* aDecoder,
                                          SurfaceFilter* aFilter) {
    for (uint8_t pass = 1; pass <= 8; ++pass) {
      // Write a pattern of rows to the surface. Important rows will receive a
      // color selected from |aColors|; unimportant rows will be transparent.
      WriteState result = WriteRowColorPixels(aFilter, aSize, pass, aColors);

      EXPECT_EQ(WriteState::FINISHED, result);
      AssertCorrectPipelineFinalState(aFilter, surfaceRect, surfaceRect);

      // Check that the generated image matches the expected pattern, with
      // interpolation applied.
      EXPECT_TRUE(
          CheckVerticallyInterpolatedImage(aDecoder, aSize, pass, aColors));

      // Prepare for the next pass.
      aFilter->ResetToFirstRow();
    }
  });
}

void CheckInterpolation(const IntSize& aSize,
                        const vector<BGRAColor>& aColors) {
  CheckHorizontalInterpolation(aSize, aColors);
  CheckVerticalInterpolation(aSize, aColors);
}

void CheckADAM7InterpolatingWritePixels(const IntSize& aSize) {
  // This test writes 8 passes of green pixels (the seven ADAM7 passes, plus one
  // extra to make sure nothing goes wrong if we write too much input) and
  // verifies that the output is a solid green surface each time. Because all
  // the pixels are the same color, interpolation doesn't matter; we test the
  // correctness of the interpolation algorithm itself separately.
  WithADAM7InterpolatingFilter(
      aSize, [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        IntRect rect(IntPoint(0, 0), aSize);

        for (int32_t pass = 1; pass <= 8; ++pass) {
          // We only actually write up to the last important row for each pass,
          // because that row unambiguously determines the remaining rows.
          const int32_t lastRow = aSize.height - 1;
          const int32_t lastImportantRow =
              lastRow - (lastRow % ImportantRowStride(pass));
          const IntRect inputWriteRect(0, 0, aSize.width, lastImportantRow + 1);

          CheckWritePixels(aDecoder, aFilter,
                           /* aOutputRect = */ Some(rect),
                           /* aInputRect = */ Some(rect),
                           /* aInputWriteRect = */ Some(inputWriteRect));

          aFilter->ResetToFirstRow();
          EXPECT_FALSE(aFilter->IsSurfaceFinished());
          Maybe<SurfaceInvalidRect> invalidRect = aFilter->TakeInvalidRect();
          EXPECT_TRUE(invalidRect.isNothing());
        }
      });
}

TEST(ImageADAM7InterpolatingFilter, WritePixels100_100)
{ CheckADAM7InterpolatingWritePixels(IntSize(100, 100)); }

TEST(ImageADAM7InterpolatingFilter, WritePixels99_99)
{ CheckADAM7InterpolatingWritePixels(IntSize(99, 99)); }

TEST(ImageADAM7InterpolatingFilter, WritePixels66_33)
{ CheckADAM7InterpolatingWritePixels(IntSize(66, 33)); }

TEST(ImageADAM7InterpolatingFilter, WritePixels33_66)
{ CheckADAM7InterpolatingWritePixels(IntSize(33, 66)); }

TEST(ImageADAM7InterpolatingFilter, WritePixels15_15)
{ CheckADAM7InterpolatingWritePixels(IntSize(15, 15)); }

TEST(ImageADAM7InterpolatingFilter, WritePixels9_9)
{ CheckADAM7InterpolatingWritePixels(IntSize(9, 9)); }

TEST(ImageADAM7InterpolatingFilter, WritePixels8_8)
{ CheckADAM7InterpolatingWritePixels(IntSize(8, 8)); }

TEST(ImageADAM7InterpolatingFilter, WritePixels7_7)
{ CheckADAM7InterpolatingWritePixels(IntSize(7, 7)); }

TEST(ImageADAM7InterpolatingFilter, WritePixels3_3)
{ CheckADAM7InterpolatingWritePixels(IntSize(3, 3)); }

TEST(ImageADAM7InterpolatingFilter, WritePixels1_1)
{ CheckADAM7InterpolatingWritePixels(IntSize(1, 1)); }

TEST(ImageADAM7InterpolatingFilter, TrivialInterpolation48_48)
{ CheckInterpolation(IntSize(48, 48), {BGRAColor::Green()}); }

TEST(ImageADAM7InterpolatingFilter, InterpolationOutput33_17)
{
  // We check interpolation using irregular patterns to make sure that the
  // interpolation will look different for different passes.
  CheckInterpolation(
      IntSize(33, 17),
      {BGRAColor::Green(), BGRAColor::Red(),   BGRAColor::Green(),
       BGRAColor::Blue(),  BGRAColor::Blue(),  BGRAColor::Blue(),
       BGRAColor::Red(),   BGRAColor::Green(), BGRAColor::Red(),
       BGRAColor::Red(),   BGRAColor::Blue(),  BGRAColor::Blue(),
       BGRAColor::Green(), BGRAColor::Blue(),  BGRAColor::Red(),
       BGRAColor::Blue(),  BGRAColor::Red(),   BGRAColor::Green(),
       BGRAColor::Blue(),  BGRAColor::Red(),   BGRAColor::Green(),
       BGRAColor::Red(),   BGRAColor::Red(),   BGRAColor::Blue(),
       BGRAColor::Blue(),  BGRAColor::Blue(),  BGRAColor::Red(),
       BGRAColor::Green(), BGRAColor::Green(), BGRAColor::Blue(),
       BGRAColor::Red(),   BGRAColor::Blue()});
}

TEST(ImageADAM7InterpolatingFilter, InterpolationOutput32_16)
{
  CheckInterpolation(
      IntSize(32, 16),
      {BGRAColor::Green(), BGRAColor::Red(),   BGRAColor::Green(),
       BGRAColor::Blue(),  BGRAColor::Blue(),  BGRAColor::Blue(),
       BGRAColor::Red(),   BGRAColor::Green(), BGRAColor::Red(),
       BGRAColor::Red(),   BGRAColor::Blue(),  BGRAColor::Blue(),
       BGRAColor::Green(), BGRAColor::Blue(),  BGRAColor::Red(),
       BGRAColor::Blue(),  BGRAColor::Red(),   BGRAColor::Green(),
       BGRAColor::Blue(),  BGRAColor::Red(),   BGRAColor::Green(),
       BGRAColor::Red(),   BGRAColor::Red(),   BGRAColor::Blue(),
       BGRAColor::Blue(),  BGRAColor::Blue(),  BGRAColor::Red(),
       BGRAColor::Green(), BGRAColor::Green(), BGRAColor::Blue(),
       BGRAColor::Red(),   BGRAColor::Blue()});
}

TEST(ImageADAM7InterpolatingFilter, InterpolationOutput31_15)
{
  CheckInterpolation(
      IntSize(31, 15),
      {BGRAColor::Green(), BGRAColor::Red(),   BGRAColor::Green(),
       BGRAColor::Blue(),  BGRAColor::Blue(),  BGRAColor::Blue(),
       BGRAColor::Red(),   BGRAColor::Green(), BGRAColor::Red(),
       BGRAColor::Red(),   BGRAColor::Blue(),  BGRAColor::Blue(),
       BGRAColor::Green(), BGRAColor::Blue(),  BGRAColor::Red(),
       BGRAColor::Blue(),  BGRAColor::Red(),   BGRAColor::Green(),
       BGRAColor::Blue(),  BGRAColor::Red(),   BGRAColor::Green(),
       BGRAColor::Red(),   BGRAColor::Red(),   BGRAColor::Blue(),
       BGRAColor::Blue(),  BGRAColor::Blue(),  BGRAColor::Red(),
       BGRAColor::Green(), BGRAColor::Green(), BGRAColor::Blue(),
       BGRAColor::Red(),   BGRAColor::Blue()});
}

TEST(ImageADAM7InterpolatingFilter, InterpolationOutput17_33)
{
  CheckInterpolation(IntSize(17, 33),
                     {BGRAColor::Green(), BGRAColor::Red(), BGRAColor::Green(),
                      BGRAColor::Blue(), BGRAColor::Red(), BGRAColor::Green(),
                      BGRAColor::Blue(), BGRAColor::Red(), BGRAColor::Blue(),
                      BGRAColor::Blue(), BGRAColor::Red(), BGRAColor::Green(),
                      BGRAColor::Green(), BGRAColor::Red(), BGRAColor::Red(),
                      BGRAColor::Blue()});
}

TEST(ImageADAM7InterpolatingFilter, InterpolationOutput16_32)
{
  CheckInterpolation(IntSize(16, 32),
                     {BGRAColor::Green(), BGRAColor::Red(), BGRAColor::Green(),
                      BGRAColor::Blue(), BGRAColor::Red(), BGRAColor::Green(),
                      BGRAColor::Blue(), BGRAColor::Red(), BGRAColor::Blue(),
                      BGRAColor::Blue(), BGRAColor::Red(), BGRAColor::Green(),
                      BGRAColor::Green(), BGRAColor::Red(), BGRAColor::Red(),
                      BGRAColor::Blue()});
}

TEST(ImageADAM7InterpolatingFilter, InterpolationOutput15_31)
{
  CheckInterpolation(IntSize(15, 31),
                     {BGRAColor::Green(), BGRAColor::Red(), BGRAColor::Green(),
                      BGRAColor::Blue(), BGRAColor::Red(), BGRAColor::Green(),
                      BGRAColor::Blue(), BGRAColor::Red(), BGRAColor::Blue(),
                      BGRAColor::Blue(), BGRAColor::Red(), BGRAColor::Green(),
                      BGRAColor::Green(), BGRAColor::Red(), BGRAColor::Red(),
                      BGRAColor::Blue()});
}

TEST(ImageADAM7InterpolatingFilter, InterpolationOutput9_9)
{
  CheckInterpolation(IntSize(9, 9),
                     {BGRAColor::Blue(), BGRAColor::Blue(), BGRAColor::Red(),
                      BGRAColor::Green(), BGRAColor::Green(), BGRAColor::Red(),
                      BGRAColor::Red(), BGRAColor::Blue()});
}

TEST(ImageADAM7InterpolatingFilter, InterpolationOutput8_8)
{
  CheckInterpolation(IntSize(8, 8),
                     {BGRAColor::Blue(), BGRAColor::Blue(), BGRAColor::Red(),
                      BGRAColor::Green(), BGRAColor::Green(), BGRAColor::Red(),
                      BGRAColor::Red(), BGRAColor::Blue()});
}

TEST(ImageADAM7InterpolatingFilter, InterpolationOutput7_7)
{
  CheckInterpolation(IntSize(7, 7),
                     {BGRAColor::Blue(), BGRAColor::Blue(), BGRAColor::Red(),
                      BGRAColor::Green(), BGRAColor::Green(), BGRAColor::Red(),
                      BGRAColor::Red(), BGRAColor::Blue()});
}

TEST(ImageADAM7InterpolatingFilter, InterpolationOutput3_3)
{
  CheckInterpolation(IntSize(3, 3), {BGRAColor::Green(), BGRAColor::Red(),
                                     BGRAColor::Blue(), BGRAColor::Red()});
}

TEST(ImageADAM7InterpolatingFilter, InterpolationOutput1_1)
{ CheckInterpolation(IntSize(1, 1), {BGRAColor::Blue()}); }

TEST(ImageADAM7InterpolatingFilter, ADAM7InterpolationFailsFor0_0)
{
  // A 0x0 input size is invalid, so configuration should fail.
  AssertConfiguringADAM7InterpolatingFilterFails(IntSize(0, 0));
}

TEST(ImageADAM7InterpolatingFilter, ADAM7InterpolationFailsForMinus1_Minus1)
{
  // A negative input size is invalid, so configuration should fail.
  AssertConfiguringADAM7InterpolatingFilterFails(IntSize(-1, -1));
}
