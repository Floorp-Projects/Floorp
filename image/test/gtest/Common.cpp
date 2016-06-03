/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Common.h"

#include <cstdlib>

#include "nsDirectoryServiceDefs.h"
#include "nsIDirectoryService.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIProperties.h"
#include "nsNetUtil.h"
#include "mozilla/RefPtr.h"
#include "nsStreamUtils.h"
#include "nsString.h"

namespace mozilla {
namespace image {

using namespace gfx;

using std::abs;

///////////////////////////////////////////////////////////////////////////////
// General Helpers
///////////////////////////////////////////////////////////////////////////////

// These macros work like gtest's ASSERT_* macros, except that they can be used
// in functions that return values.
#define ASSERT_TRUE_OR_RETURN(e, rv) \
  EXPECT_TRUE(e);                    \
  if (!(e)) {                        \
    return rv;                       \
  }

#define ASSERT_EQ_OR_RETURN(a, b, rv) \
  EXPECT_EQ(a, b);                    \
  if ((a) != (b)) {                   \
    return rv;                        \
  }

#define ASSERT_LE_OR_RETURN(a, b, rv) \
  EXPECT_LE(a, b);                    \
  if (!((a) <= (b))) {                 \
    return rv;                        \
  }

already_AddRefed<nsIInputStream>
LoadFile(const char* aRelativePath)
{
  nsresult rv;

  nsCOMPtr<nsIProperties> dirService =
    do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  ASSERT_TRUE_OR_RETURN(dirService != nullptr, nullptr);

  // Retrieve the current working directory.
  nsCOMPtr<nsIFile> file;
  rv = dirService->Get(NS_OS_CURRENT_WORKING_DIR,
                       NS_GET_IID(nsIFile), getter_AddRefs(file));
  ASSERT_TRUE_OR_RETURN(NS_SUCCEEDED(rv), nullptr);

  // Construct the final path by appending the working path to the current
  // working directory.
  file->AppendNative(nsDependentCString(aRelativePath));

  // Construct an input stream for the requested file.
  nsCOMPtr<nsIInputStream> inputStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), file);
  ASSERT_TRUE_OR_RETURN(NS_SUCCEEDED(rv), nullptr);

  // Ensure the resulting input stream is buffered.
  if (!NS_InputStreamIsBuffered(inputStream)) {
    nsCOMPtr<nsIInputStream> bufStream;
    rv = NS_NewBufferedInputStream(getter_AddRefs(bufStream),
                                   inputStream, 1024);
    ASSERT_TRUE_OR_RETURN(NS_SUCCEEDED(rv), nullptr);
    inputStream = bufStream;
  }

  return inputStream.forget();
}

bool
IsSolidColor(SourceSurface* aSurface,
             BGRAColor aColor,
             uint8_t aFuzz /* = 0 */)
{
  IntSize size = aSurface->GetSize();
  return RectIsSolidColor(aSurface, IntRect(0, 0, size.width, size.height),
                          aColor, aFuzz);
}

bool
IsSolidPalettedColor(Decoder* aDecoder, uint8_t aColor)
{
  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  return PalettedRectIsSolidColor(aDecoder, currentFrame->GetRect(), aColor);
}

bool
RowsAreSolidColor(SourceSurface* aSurface,
                  int32_t aStartRow,
                  int32_t aRowCount,
                  BGRAColor aColor,
                  uint8_t aFuzz /* = 0 */)
{
  IntSize size = aSurface->GetSize();
  return RectIsSolidColor(aSurface, IntRect(0, aStartRow, size.width, aRowCount),
                          aColor, aFuzz);
}

bool
PalettedRowsAreSolidColor(Decoder* aDecoder,
                          int32_t aStartRow,
                          int32_t aRowCount,
                          uint8_t aColor)
{
  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  IntRect frameRect = currentFrame->GetRect();
  IntRect solidColorRect(frameRect.x, aStartRow, frameRect.width, aRowCount);
  return PalettedRectIsSolidColor(aDecoder, solidColorRect, aColor);
}

bool
RectIsSolidColor(SourceSurface* aSurface,
                 const IntRect& aRect,
                 BGRAColor aColor,
                 uint8_t aFuzz /* = 0 */)
{
  IntSize surfaceSize = aSurface->GetSize();
  IntRect rect =
    aRect.Intersect(IntRect(0, 0, surfaceSize.width, surfaceSize.height));

  RefPtr<DataSourceSurface> dataSurface = aSurface->GetDataSurface();
  ASSERT_TRUE_OR_RETURN(dataSurface != nullptr, false);

  ASSERT_EQ_OR_RETURN(dataSurface->Stride(), surfaceSize.width * 4, false);

  DataSourceSurface::ScopedMap mapping(dataSurface,
                                       DataSourceSurface::MapType::READ);
  ASSERT_TRUE_OR_RETURN(mapping.IsMapped(), false);

  uint8_t* data = dataSurface->GetData();
  ASSERT_TRUE_OR_RETURN(data != nullptr, false);

  int32_t rowLength = dataSurface->Stride();
  for (int32_t row = rect.y; row < rect.YMost(); ++row) {
    for (int32_t col = rect.x; col < rect.XMost(); ++col) {
      int32_t i = row * rowLength + col * 4;
      if (aFuzz != 0) {
        ASSERT_LE_OR_RETURN(abs(aColor.mBlue - data[i + 0]), aFuzz, false);
        ASSERT_LE_OR_RETURN(abs(aColor.mGreen - data[i + 1]), aFuzz, false);
        ASSERT_LE_OR_RETURN(abs(aColor.mRed - data[i + 2]), aFuzz, false);
        ASSERT_LE_OR_RETURN(abs(aColor.mAlpha - data[i + 3]), aFuzz, false);
      } else {
        ASSERT_EQ_OR_RETURN(aColor.mBlue,  data[i + 0], false);
        ASSERT_EQ_OR_RETURN(aColor.mGreen, data[i + 1], false);
        ASSERT_EQ_OR_RETURN(aColor.mRed,   data[i + 2], false);
        ASSERT_EQ_OR_RETURN(aColor.mAlpha, data[i + 3], false);
      }
    }
  }

  return true;
}

bool
PalettedRectIsSolidColor(Decoder* aDecoder, const IntRect& aRect, uint8_t aColor)
{
  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  uint8_t* imageData;
  uint32_t imageLength;
  currentFrame->GetImageData(&imageData, &imageLength);
  ASSERT_TRUE_OR_RETURN(imageData, false);

  // Clamp to the frame rect. If any pixels outside the frame rect are included,
  // we immediately fail, because such pixels don't have any "color" in the
  // sense this function measures - they're transparent, and that doesn't
  // necessarily correspond to any color palette index at all.
  IntRect frameRect = currentFrame->GetRect();
  ASSERT_EQ_OR_RETURN(imageLength, uint32_t(frameRect.Area()), false);
  IntRect rect = aRect.Intersect(frameRect);
  ASSERT_EQ_OR_RETURN(rect.Area(), aRect.Area(), false);

  // Translate |rect| by |frameRect.TopLeft()| to reflect the fact that the
  // frame rect's offset doesn't actually mean anything in terms of the
  // in-memory representation of the surface. The image data starts at the upper
  // left corner of the frame rect, in other words.
  rect -= frameRect.TopLeft();

  // Walk through the image data and make sure that the entire rect has the
  // palette index |aColor|.
  int32_t rowLength = frameRect.width;
  for (int32_t row = rect.y; row < rect.YMost(); ++row) {
    for (int32_t col = rect.x; col < rect.XMost(); ++col) {
      int32_t i = row * rowLength + col;
      ASSERT_EQ_OR_RETURN(aColor, imageData[i], false);
    }
  }

  return true;
}


///////////////////////////////////////////////////////////////////////////////
// SurfacePipe Helpers
///////////////////////////////////////////////////////////////////////////////

already_AddRefed<Decoder>
CreateTrivialDecoder()
{
  gfxPrefs::GetSingleton();
  DecoderType decoderType = DecoderFactory::GetDecoderType("image/gif");
  RefPtr<SourceBuffer> sourceBuffer = new SourceBuffer();
  RefPtr<Decoder> decoder =
    DecoderFactory::CreateAnonymousDecoder(decoderType, sourceBuffer, Nothing(),
                                           DefaultSurfaceFlags());
  return decoder.forget();
}

void
AssertCorrectPipelineFinalState(SurfaceFilter* aFilter,
                                const gfx::IntRect& aInputSpaceRect,
                                const gfx::IntRect& aOutputSpaceRect)
{
  EXPECT_TRUE(aFilter->IsSurfaceFinished());
  Maybe<SurfaceInvalidRect> invalidRect = aFilter->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isSome());
  EXPECT_EQ(aInputSpaceRect, invalidRect->mInputSpaceRect);
  EXPECT_EQ(aOutputSpaceRect, invalidRect->mOutputSpaceRect);
}

void
CheckGeneratedImage(Decoder* aDecoder,
                    const IntRect& aRect,
                    uint8_t aFuzz /* = 0 */)
{
  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  RefPtr<SourceSurface> surface = currentFrame->GetSurface();
  const IntSize surfaceSize = surface->GetSize();

  // This diagram shows how the surface is divided into regions that the code
  // below tests for the correct content. The output rect is the bounds of the
  // region labeled 'C'.
  //
  // +---------------------------+
  // |             A             |
  // +---------+--------+--------+
  // |    B    |   C    |   D    |
  // +---------+--------+--------+
  // |             E             |
  // +---------------------------+

  // Check that the output rect itself is green. (Region 'C'.)
  EXPECT_TRUE(RectIsSolidColor(surface, aRect, BGRAColor::Green(), aFuzz));

  // Check that the area above the output rect is transparent. (Region 'A'.)
  EXPECT_TRUE(RectIsSolidColor(surface,
                               IntRect(0, 0, surfaceSize.width, aRect.y),
                               BGRAColor::Transparent(), aFuzz));

  // Check that the area to the left of the output rect is transparent. (Region 'B'.)
  EXPECT_TRUE(RectIsSolidColor(surface,
                               IntRect(0, aRect.y, aRect.x, aRect.YMost()),
                               BGRAColor::Transparent(), aFuzz));

  // Check that the area to the right of the output rect is transparent. (Region 'D'.)
  const int32_t widthOnRight = surfaceSize.width - aRect.XMost();
  EXPECT_TRUE(RectIsSolidColor(surface,
                               IntRect(aRect.XMost(), aRect.y, widthOnRight, aRect.YMost()),
                               BGRAColor::Transparent(), aFuzz));

  // Check that the area below the output rect is transparent. (Region 'E'.)
  const int32_t heightBelow = surfaceSize.height - aRect.YMost();
  EXPECT_TRUE(RectIsSolidColor(surface,
                               IntRect(0, aRect.YMost(), surfaceSize.width, heightBelow),
                               BGRAColor::Transparent(), aFuzz));
}

void
CheckGeneratedPalettedImage(Decoder* aDecoder, const IntRect& aRect)
{
  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  IntSize imageSize = currentFrame->GetImageSize();

  // This diagram shows how the surface is divided into regions that the code
  // below tests for the correct content. The output rect is the bounds of the
  // region labeled 'C'.
  //
  // +---------------------------+
  // |             A             |
  // +---------+--------+--------+
  // |    B    |   C    |   D    |
  // +---------+--------+--------+
  // |             E             |
  // +---------------------------+

  // Check that the output rect itself is all 255's. (Region 'C'.)
  EXPECT_TRUE(PalettedRectIsSolidColor(aDecoder, aRect, 255));

  // Check that the area above the output rect is all 0's. (Region 'A'.)
  EXPECT_TRUE(PalettedRectIsSolidColor(aDecoder,
                                       IntRect(0, 0, imageSize.width, aRect.y),
                                       0));

  // Check that the area to the left of the output rect is all 0's. (Region 'B'.)
  EXPECT_TRUE(PalettedRectIsSolidColor(aDecoder,
                                       IntRect(0, aRect.y, aRect.x, aRect.YMost()),
                                       0));

  // Check that the area to the right of the output rect is all 0's. (Region 'D'.)
  const int32_t widthOnRight = imageSize.width - aRect.XMost();
  EXPECT_TRUE(PalettedRectIsSolidColor(aDecoder,
                                       IntRect(aRect.XMost(), aRect.y, widthOnRight, aRect.YMost()),
                                       0));

  // Check that the area below the output rect is transparent. (Region 'E'.)
  const int32_t heightBelow = imageSize.height - aRect.YMost();
  EXPECT_TRUE(PalettedRectIsSolidColor(aDecoder,
                                       IntRect(0, aRect.YMost(), imageSize.width, heightBelow),
                                       0));
}

void
CheckWritePixels(Decoder* aDecoder,
                 SurfaceFilter* aFilter,
                 Maybe<IntRect> aOutputRect /* = Nothing() */,
                 Maybe<IntRect> aInputRect /* = Nothing() */,
                 Maybe<IntRect> aInputWriteRect /* = Nothing() */,
                 Maybe<IntRect> aOutputWriteRect /* = Nothing() */,
                 uint8_t aFuzz /* = 0 */)
{
  IntRect outputRect = aOutputRect.valueOr(IntRect(0, 0, 100, 100));
  IntRect inputRect = aInputRect.valueOr(IntRect(0, 0, 100, 100));
  IntRect inputWriteRect = aInputWriteRect.valueOr(inputRect);
  IntRect outputWriteRect = aOutputWriteRect.valueOr(outputRect);

  // Fill the image.
  int32_t count = 0;
  auto result = aFilter->WritePixels<uint32_t>([&] {
    ++count;
    return AsVariant(BGRAColor::Green().AsPixel());
  });
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_EQ(inputWriteRect.width * inputWriteRect.height, count);

  AssertCorrectPipelineFinalState(aFilter, inputRect, outputRect);

  // Attempt to write more data and make sure nothing changes.
  const int32_t oldCount = count;
  result = aFilter->WritePixels<uint32_t>([&] {
    ++count;
    return AsVariant(BGRAColor::Green().AsPixel());
  });
  EXPECT_EQ(oldCount, count);
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_TRUE(aFilter->IsSurfaceFinished());
  Maybe<SurfaceInvalidRect> invalidRect = aFilter->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());

  // Attempt to advance to the next row and make sure nothing changes.
  aFilter->AdvanceRow();
  EXPECT_TRUE(aFilter->IsSurfaceFinished());
  invalidRect = aFilter->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());

  // Check that the generated image is correct.
  CheckGeneratedImage(aDecoder, outputWriteRect, aFuzz);
}

void
CheckPalettedWritePixels(Decoder* aDecoder,
                         SurfaceFilter* aFilter,
                         Maybe<IntRect> aOutputRect /* = Nothing() */,
                         Maybe<IntRect> aInputRect /* = Nothing() */,
                         Maybe<IntRect> aInputWriteRect /* = Nothing() */,
                         Maybe<IntRect> aOutputWriteRect /* = Nothing() */,
                         uint8_t aFuzz /* = 0 */)
{
  IntRect outputRect = aOutputRect.valueOr(IntRect(0, 0, 100, 100));
  IntRect inputRect = aInputRect.valueOr(IntRect(0, 0, 100, 100));
  IntRect inputWriteRect = aInputWriteRect.valueOr(inputRect);
  IntRect outputWriteRect = aOutputWriteRect.valueOr(outputRect);

  // Fill the image.
  int32_t count = 0;
  auto result = aFilter->WritePixels<uint8_t>([&] {
    ++count;
    return AsVariant(uint8_t(255));
  });
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_EQ(inputWriteRect.width * inputWriteRect.height, count);

  AssertCorrectPipelineFinalState(aFilter, inputRect, outputRect);

  // Attempt to write more data and make sure nothing changes.
  const int32_t oldCount = count;
  result = aFilter->WritePixels<uint8_t>([&] {
    ++count;
    return AsVariant(uint8_t(255));
  });
  EXPECT_EQ(oldCount, count);
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_TRUE(aFilter->IsSurfaceFinished());
  Maybe<SurfaceInvalidRect> invalidRect = aFilter->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());

  // Attempt to advance to the next row and make sure nothing changes.
  aFilter->AdvanceRow();
  EXPECT_TRUE(aFilter->IsSurfaceFinished());
  invalidRect = aFilter->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());

  // Check that the generated image is correct.
  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  uint8_t* imageData;
  uint32_t imageLength;
  currentFrame->GetImageData(&imageData, &imageLength);
  ASSERT_TRUE(imageData != nullptr);
  ASSERT_EQ(outputWriteRect.width * outputWriteRect.height, int32_t(imageLength));
  for (uint32_t i = 0; i < imageLength; ++i) {
    ASSERT_EQ(uint8_t(255), imageData[i]);
  }
}


///////////////////////////////////////////////////////////////////////////////
// Test Data
///////////////////////////////////////////////////////////////////////////////

ImageTestCase GreenPNGTestCase()
{
  return ImageTestCase("green.png", "image/png", IntSize(100, 100));
}

ImageTestCase GreenGIFTestCase()
{
  return ImageTestCase("green.gif", "image/gif", IntSize(100, 100));
}

ImageTestCase GreenJPGTestCase()
{
  return ImageTestCase("green.jpg", "image/jpeg", IntSize(100, 100),
                       TEST_CASE_IS_FUZZY);
}

ImageTestCase GreenBMPTestCase()
{
  return ImageTestCase("green.bmp", "image/bmp", IntSize(100, 100));
}

ImageTestCase GreenICOTestCase()
{
  // This ICO contains a 32-bit BMP, and we use a BMP's alpha data by default
  // when the BMP is embedded in an ICO, so it's transparent.
  return ImageTestCase("green.ico", "image/x-icon", IntSize(100, 100),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase GreenIconTestCase()
{
  return ImageTestCase("green.icon", "image/icon", IntSize(100, 100),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase GreenFirstFrameAnimatedGIFTestCase()
{
  return ImageTestCase("first-frame-green.gif", "image/gif", IntSize(100, 100),
                       TEST_CASE_IS_ANIMATED);
}

ImageTestCase GreenFirstFrameAnimatedPNGTestCase()
{
  return ImageTestCase("first-frame-green.png", "image/png", IntSize(100, 100),
                       TEST_CASE_IS_TRANSPARENT | TEST_CASE_IS_ANIMATED);
}

ImageTestCase CorruptTestCase()
{
  return ImageTestCase("corrupt.jpg", "image/jpeg", IntSize(100, 100),
                       TEST_CASE_HAS_ERROR);
}

ImageTestCase TransparentPNGTestCase()
{
  return ImageTestCase("transparent.png", "image/png", IntSize(32, 32),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase TransparentGIFTestCase()
{
  return ImageTestCase("transparent.gif", "image/gif", IntSize(16, 16),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase FirstFramePaddingGIFTestCase()
{
  return ImageTestCase("transparent.gif", "image/gif", IntSize(16, 16),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase TransparentIfWithinICOBMPTestCase(TestCaseFlags aFlags)
{
  // This is a BMP that is only transparent when decoded as if it is within an
  // ICO file. (Note: aFlags needs to be set to TEST_CASE_DEFAULT_FLAGS or
  // TEST_CASE_IS_TRANSPARENT accordingly.)
  return ImageTestCase("transparent-if-within-ico.bmp", "image/bmp",
                       IntSize(32, 32), aFlags);
}

ImageTestCase RLE4BMPTestCase()
{
  return ImageTestCase("rle4.bmp", "image/bmp", IntSize(320, 240),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase RLE8BMPTestCase()
{
  return ImageTestCase("rle8.bmp", "image/bmp", IntSize(32, 32),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase NoFrameDelayGIFTestCase()
{
  // This is an invalid (or at least, questionably valid) GIF that's animated
  // even though it specifies a frame delay of zero. It's animated, but it's not
  // marked TEST_CASE_IS_ANIMATED because the metadata decoder can't detect that
  // it's animated.
  return ImageTestCase("no-frame-delay.gif", "image/gif", IntSize(100, 100));
}

ImageTestCase DownscaledPNGTestCase()
{
  // This testcase (and all the other "downscaled") testcases) consists of 25
  // lines of green, followed by 25 lines of red, followed by 25 lines of green,
  // followed by 25 more lines of red. It's intended that tests downscale it
  // from 100x100 to 20x20, so we specify a 20x20 output size.
  return ImageTestCase("downscaled.png", "image/png", IntSize(100, 100),
                       IntSize(20, 20));
}

ImageTestCase DownscaledGIFTestCase()
{
  return ImageTestCase("downscaled.gif", "image/gif", IntSize(100, 100),
                       IntSize(20, 20));
}

ImageTestCase DownscaledJPGTestCase()
{
  return ImageTestCase("downscaled.jpg", "image/jpeg", IntSize(100, 100),
                       IntSize(20, 20));
}

ImageTestCase DownscaledBMPTestCase()
{
  return ImageTestCase("downscaled.bmp", "image/bmp", IntSize(100, 100),
                       IntSize(20, 20));
}

ImageTestCase DownscaledICOTestCase()
{
  return ImageTestCase("downscaled.ico", "image/x-icon", IntSize(100, 100),
                       IntSize(20, 20), TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase DownscaledIconTestCase()
{
  return ImageTestCase("downscaled.icon", "image/icon", IntSize(100, 100),
                       IntSize(20, 20), TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase DownscaledTransparentICOWithANDMaskTestCase()
{
  // This test case is an ICO with AND mask transparency. We want to ensure that
  // we can downscale it without crashing or triggering ASAN failures, but its
  // content isn't simple to verify, so for now we don't check the output.
  return ImageTestCase("transparent-ico-with-and-mask.ico", "image/x-icon",
                       IntSize(32, 32), IntSize(20, 20),
                       TEST_CASE_IS_TRANSPARENT | TEST_CASE_IGNORE_OUTPUT);
}

} // namespace image
} // namespace mozilla
