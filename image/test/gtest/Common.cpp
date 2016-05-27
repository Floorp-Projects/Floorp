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

void AssertCorrectPipelineFinalState(SurfaceFilter* aFilter,
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

template <typename Func> void
CheckSurfacePipeWrite(Decoder* aDecoder,
                      SurfaceFilter* aFilter,
                      Maybe<IntRect> aOutputRect,
                      Maybe<IntRect> aInputRect,
                      Maybe<IntRect> aInputWriteRect,
                      Maybe<IntRect> aOutputWriteRect,
                      uint8_t aFuzz,
                      Func aFunc)
{
  IntRect outputRect = aOutputRect.valueOr(IntRect(0, 0, 100, 100));
  IntRect inputRect = aInputRect.valueOr(IntRect(0, 0, 100, 100));
  IntRect inputWriteRect = aInputWriteRect.valueOr(inputRect);
  IntRect outputWriteRect = aOutputWriteRect.valueOr(outputRect);

  // Fill the image.
  int32_t count = 0;
  auto result = aFunc(count);
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_EQ(inputWriteRect.width * inputWriteRect.height, count);

  AssertCorrectPipelineFinalState(aFilter, inputRect, outputRect);

  // Attempt to write more data and make sure nothing changes.
  const int32_t oldCount = count;
  result = aFunc(count);
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
CheckWritePixels(Decoder* aDecoder,
                 SurfaceFilter* aFilter,
                 Maybe<IntRect> aOutputRect /* = Nothing() */,
                 Maybe<IntRect> aInputRect /* = Nothing() */,
                 Maybe<IntRect> aInputWriteRect /* = Nothing() */,
                 Maybe<IntRect> aOutputWriteRect /* = Nothing() */,
                 uint8_t aFuzz /* = 0 */)
{
  CheckSurfacePipeWrite(aDecoder, aFilter,
                        aOutputRect, aInputRect,
                        aInputWriteRect, aOutputWriteRect,
                        aFuzz,
                        [&](int32_t& aCount) {
    return aFilter->WritePixels<uint32_t>([&] {
      ++aCount;
      return AsVariant(BGRAColor::Green().AsPixel());
    });
  });
}

void
CheckWriteRows(Decoder* aDecoder,
               SurfaceFilter* aFilter,
               Maybe<IntRect> aOutputRect /* = Nothing() */,
               Maybe<IntRect> aInputRect /* = Nothing() */,
               Maybe<IntRect> aInputWriteRect /* = Nothing() */,
               Maybe<IntRect> aOutputWriteRect /* = Nothing() */,
               uint8_t aFuzz /* = 0 */)
{
  CheckSurfacePipeWrite(aDecoder, aFilter,
                        aOutputRect, aInputRect,
                        aInputWriteRect, aOutputWriteRect,
                        aFuzz,
                        [&](int32_t& aCount) {
    return aFilter->WriteRows<uint32_t>([&](uint32_t* aRow, uint32_t aLength) {
      for (; aLength > 0; --aLength, ++aRow, ++aCount) {
        *aRow = BGRAColor::Green().AsPixel();
      }
      return Nothing();
    });
  });
}

template <typename Func> void
CheckPalettedSurfacePipeWrite(Decoder* aDecoder,
                              SurfaceFilter* aFilter,
                              Maybe<IntRect> aOutputRect,
                              Maybe<IntRect> aInputRect,
                              Maybe<IntRect> aInputWriteRect,
                              Maybe<IntRect> aOutputWriteRect,
                              uint8_t aFuzz,
                              Func aFunc)
{
  IntRect outputRect = aOutputRect.valueOr(IntRect(0, 0, 100, 100));
  IntRect inputRect = aInputRect.valueOr(IntRect(0, 0, 100, 100));
  IntRect inputWriteRect = aInputWriteRect.valueOr(inputRect);
  IntRect outputWriteRect = aOutputWriteRect.valueOr(outputRect);

  // Fill the image.
  int32_t count = 0;
  auto result = aFunc(count);
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_EQ(inputWriteRect.width * inputWriteRect.height, count);

  AssertCorrectPipelineFinalState(aFilter, inputRect, outputRect);

  // Attempt to write more data and make sure nothing changes.
  const int32_t oldCount = count;
  result = aFunc(count);
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

void
CheckPalettedWritePixels(Decoder* aDecoder,
                         SurfaceFilter* aFilter,
                         Maybe<IntRect> aOutputRect /* = Nothing() */,
                         Maybe<IntRect> aInputRect /* = Nothing() */,
                         Maybe<IntRect> aInputWriteRect /* = Nothing() */,
                         Maybe<IntRect> aOutputWriteRect /* = Nothing() */,
                         uint8_t aFuzz /* = 0 */)
{
  CheckPalettedSurfacePipeWrite(aDecoder, aFilter,
                                aOutputRect, aInputRect,
                                aInputWriteRect, aOutputWriteRect,
                                aFuzz,
                                [&](int32_t& aCount) {
    return aFilter->WritePixels<uint8_t>([&] {
      ++aCount;
      return AsVariant(uint8_t(255));
    });
  });
}

void
CheckPalettedWriteRows(Decoder* aDecoder,
                       SurfaceFilter* aFilter,
                       Maybe<IntRect> aOutputRect /* = Nothing() */,
                       Maybe<IntRect> aInputRect /* = Nothing() */,
                       Maybe<IntRect> aInputWriteRect /* = Nothing() */,
                       Maybe<IntRect> aOutputWriteRect /* = Nothing() */,
                       uint8_t aFuzz /* = 0*/)
{
  CheckPalettedSurfacePipeWrite(aDecoder, aFilter,
                                aOutputRect, aInputRect,
                                aInputWriteRect, aOutputWriteRect,
                                aFuzz,
                                [&](int32_t& aCount) {
    return aFilter->WriteRows<uint8_t>([&](uint8_t* aRow, uint32_t aLength) {
      for (; aLength > 0; --aLength, ++aRow, ++aCount) {
        *aRow = uint8_t(255);
      }
      return Nothing();
    });
  });
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

} // namespace image
} // namespace mozilla
