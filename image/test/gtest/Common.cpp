/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Common.h"

#include <cstdlib>

#include "gfxPlatform.h"

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
using std::vector;

static bool sImageLibInitialized = false;

AutoInitializeImageLib::AutoInitializeImageLib() {
  if (MOZ_LIKELY(sImageLibInitialized)) {
    return;
  }

  EXPECT_TRUE(NS_IsMainThread());
  sImageLibInitialized = true;

  // Force sRGB to be consistent with reftests.
  nsresult rv = Preferences::SetBool("gfx.color_management.force_srgb", true);
  EXPECT_TRUE(rv == NS_OK);

  // Ensure WebP is enabled to run decoder tests.
  rv = Preferences::SetBool("image.webp.enabled", true);
  EXPECT_TRUE(rv == NS_OK);

  // Ensure that ImageLib services are initialized.
  nsCOMPtr<imgITools> imgTools =
      do_CreateInstance("@mozilla.org/image/tools;1");
  EXPECT_TRUE(imgTools != nullptr);

  // Ensure gfxPlatform is initialized.
  gfxPlatform::GetPlatform();

  // Depending on initialization order, it is possible that our pref changes
  // have not taken effect yet because there are pending gfx-related events on
  // the main thread.
  SpinPendingEvents();
}

void ImageBenchmarkBase::SetUp() {
  nsCOMPtr<nsIInputStream> inputStream = LoadFile(mTestCase.mPath);
  ASSERT_TRUE(inputStream != nullptr);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Write the data into a SourceBuffer.
  mSourceBuffer = new SourceBuffer();
  mSourceBuffer->ExpectLength(length);
  rv = mSourceBuffer->AppendFromInputStream(inputStream, length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  mSourceBuffer->Complete(NS_OK);
}

void ImageBenchmarkBase::TearDown() {}

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

#define ASSERT_GE_OR_RETURN(a, b, rv) \
  EXPECT_GE(a, b);                    \
  if (!((a) >= (b))) {                \
    return rv;                        \
  }

#define ASSERT_LE_OR_RETURN(a, b, rv) \
  EXPECT_LE(a, b);                    \
  if (!((a) <= (b))) {                \
    return rv;                        \
  }

#define ASSERT_LT_OR_RETURN(a, b, rv) \
  EXPECT_LT(a, b);                    \
  if (!((a) < (b))) {                 \
    return rv;                        \
  }

void SpinPendingEvents() {
  nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
  EXPECT_TRUE(mainThread != nullptr);

  bool processed;
  do {
    processed = false;
    nsresult rv = mainThread->ProcessNextEvent(false, &processed);
    EXPECT_TRUE(NS_SUCCEEDED(rv));
  } while (processed);
}

already_AddRefed<nsIInputStream> LoadFile(const char* aRelativePath) {
  nsresult rv;

  nsCOMPtr<nsIProperties> dirService =
      do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID);
  ASSERT_TRUE_OR_RETURN(dirService != nullptr, nullptr);

  // Retrieve the current working directory.
  nsCOMPtr<nsIFile> file;
  rv = dirService->Get(NS_OS_CURRENT_WORKING_DIR, NS_GET_IID(nsIFile),
                       getter_AddRefs(file));
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
                                   inputStream.forget(), 1024);
    ASSERT_TRUE_OR_RETURN(NS_SUCCEEDED(rv), nullptr);
    inputStream = bufStream;
  }

  return inputStream.forget();
}

bool IsSolidColor(SourceSurface* aSurface, BGRAColor aColor,
                  uint8_t aFuzz /* = 0 */) {
  IntSize size = aSurface->GetSize();
  return RectIsSolidColor(aSurface, IntRect(0, 0, size.width, size.height),
                          aColor, aFuzz);
}

bool RowsAreSolidColor(SourceSurface* aSurface, int32_t aStartRow,
                       int32_t aRowCount, BGRAColor aColor,
                       uint8_t aFuzz /* = 0 */) {
  IntSize size = aSurface->GetSize();
  return RectIsSolidColor(
      aSurface, IntRect(0, aStartRow, size.width, aRowCount), aColor, aFuzz);
}

bool RectIsSolidColor(SourceSurface* aSurface, const IntRect& aRect,
                      BGRAColor aColor, uint8_t aFuzz /* = 0 */) {
  IntSize surfaceSize = aSurface->GetSize();
  IntRect rect =
      aRect.Intersect(IntRect(0, 0, surfaceSize.width, surfaceSize.height));

  RefPtr<DataSourceSurface> dataSurface = aSurface->GetDataSurface();
  ASSERT_TRUE_OR_RETURN(dataSurface != nullptr, false);

  DataSourceSurface::ScopedMap mapping(dataSurface,
                                       DataSourceSurface::MapType::READ);
  ASSERT_TRUE_OR_RETURN(mapping.IsMapped(), false);
  ASSERT_EQ_OR_RETURN(mapping.GetStride(), surfaceSize.width * 4, false);

  uint8_t* data = mapping.GetData();
  ASSERT_TRUE_OR_RETURN(data != nullptr, false);

  BGRAColor pmColor = aColor.Premultiply();
  int32_t rowLength = mapping.GetStride();
  for (int32_t row = rect.Y(); row < rect.YMost(); ++row) {
    for (int32_t col = rect.X(); col < rect.XMost(); ++col) {
      int32_t i = row * rowLength + col * 4;
      if (aFuzz != 0) {
        ASSERT_LE_OR_RETURN(abs(pmColor.mBlue - data[i + 0]), aFuzz, false);
        ASSERT_LE_OR_RETURN(abs(pmColor.mGreen - data[i + 1]), aFuzz, false);
        ASSERT_LE_OR_RETURN(abs(pmColor.mRed - data[i + 2]), aFuzz, false);
        ASSERT_LE_OR_RETURN(abs(pmColor.mAlpha - data[i + 3]), aFuzz, false);
      } else {
        ASSERT_EQ_OR_RETURN(pmColor.mBlue, data[i + 0], false);
        ASSERT_EQ_OR_RETURN(pmColor.mGreen, data[i + 1], false);
        ASSERT_EQ_OR_RETURN(pmColor.mRed, data[i + 2], false);
        ASSERT_EQ_OR_RETURN(pmColor.mAlpha, data[i + 3], false);
      }
    }
  }

  return true;
}

bool RowHasPixels(SourceSurface* aSurface, int32_t aRow,
                  const vector<BGRAColor>& aPixels) {
  ASSERT_GE_OR_RETURN(aRow, 0, false);

  IntSize surfaceSize = aSurface->GetSize();
  ASSERT_EQ_OR_RETURN(aPixels.size(), size_t(surfaceSize.width), false);
  ASSERT_LT_OR_RETURN(aRow, surfaceSize.height, false);

  RefPtr<DataSourceSurface> dataSurface = aSurface->GetDataSurface();
  ASSERT_TRUE_OR_RETURN(dataSurface, false);

  DataSourceSurface::ScopedMap mapping(dataSurface,
                                       DataSourceSurface::MapType::READ);
  ASSERT_TRUE_OR_RETURN(mapping.IsMapped(), false);
  ASSERT_EQ_OR_RETURN(mapping.GetStride(), surfaceSize.width * 4, false);

  uint8_t* data = mapping.GetData();
  ASSERT_TRUE_OR_RETURN(data != nullptr, false);

  int32_t rowLength = mapping.GetStride();
  for (int32_t col = 0; col < surfaceSize.width; ++col) {
    int32_t i = aRow * rowLength + col * 4;
    ASSERT_EQ_OR_RETURN(aPixels[col].mBlue, data[i + 0], false);
    ASSERT_EQ_OR_RETURN(aPixels[col].mGreen, data[i + 1], false);
    ASSERT_EQ_OR_RETURN(aPixels[col].mRed, data[i + 2], false);
    ASSERT_EQ_OR_RETURN(aPixels[col].mAlpha, data[i + 3], false);
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// SurfacePipe Helpers
///////////////////////////////////////////////////////////////////////////////

already_AddRefed<Decoder> CreateTrivialDecoder() {
  DecoderType decoderType = DecoderFactory::GetDecoderType("image/gif");
  auto sourceBuffer = MakeNotNull<RefPtr<SourceBuffer>>();
  RefPtr<Decoder> decoder = DecoderFactory::CreateAnonymousDecoder(
      decoderType, sourceBuffer, Nothing(), DefaultDecoderFlags(),
      DefaultSurfaceFlags());
  return decoder.forget();
}

void AssertCorrectPipelineFinalState(SurfaceFilter* aFilter,
                                     const gfx::IntRect& aInputSpaceRect,
                                     const gfx::IntRect& aOutputSpaceRect) {
  EXPECT_TRUE(aFilter->IsSurfaceFinished());
  Maybe<SurfaceInvalidRect> invalidRect = aFilter->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isSome());
  EXPECT_EQ(aInputSpaceRect, invalidRect->mInputSpaceRect);
  EXPECT_EQ(aOutputSpaceRect, invalidRect->mOutputSpaceRect);
}

void CheckGeneratedImage(Decoder* aDecoder, const IntRect& aRect,
                         uint8_t aFuzz /* = 0 */) {
  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
  CheckGeneratedSurface(surface, aRect, BGRAColor::Green(),
                        BGRAColor::Transparent(), aFuzz);
}

void CheckGeneratedSurface(SourceSurface* aSurface, const IntRect& aRect,
                           const BGRAColor& aInnerColor,
                           const BGRAColor& aOuterColor,
                           uint8_t aFuzz /* = 0 */) {
  const IntSize surfaceSize = aSurface->GetSize();

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

  // Check that the output rect itself is the inner color. (Region 'C'.)
  EXPECT_TRUE(RectIsSolidColor(aSurface, aRect, aInnerColor, aFuzz));

  // Check that the area above the output rect is the outer color. (Region 'A'.)
  EXPECT_TRUE(RectIsSolidColor(aSurface,
                               IntRect(0, 0, surfaceSize.width, aRect.Y()),
                               aOuterColor, aFuzz));

  // Check that the area to the left of the output rect is the outer color.
  // (Region 'B'.)
  EXPECT_TRUE(RectIsSolidColor(aSurface,
                               IntRect(0, aRect.Y(), aRect.X(), aRect.YMost()),
                               aOuterColor, aFuzz));

  // Check that the area to the right of the output rect is the outer color.
  // (Region 'D'.)
  const int32_t widthOnRight = surfaceSize.width - aRect.XMost();
  EXPECT_TRUE(RectIsSolidColor(
      aSurface, IntRect(aRect.XMost(), aRect.Y(), widthOnRight, aRect.YMost()),
      aOuterColor, aFuzz));

  // Check that the area below the output rect is the outer color. (Region 'E'.)
  const int32_t heightBelow = surfaceSize.height - aRect.YMost();
  EXPECT_TRUE(RectIsSolidColor(
      aSurface, IntRect(0, aRect.YMost(), surfaceSize.width, heightBelow),
      aOuterColor, aFuzz));
}

void CheckWritePixels(Decoder* aDecoder, SurfaceFilter* aFilter,
                      const Maybe<IntRect>& aOutputRect /* = Nothing() */,
                      const Maybe<IntRect>& aInputRect /* = Nothing() */,
                      const Maybe<IntRect>& aInputWriteRect /* = Nothing() */,
                      const Maybe<IntRect>& aOutputWriteRect /* = Nothing() */,
                      uint8_t aFuzz /* = 0 */) {
  CheckTransformedWritePixels(aDecoder, aFilter, BGRAColor::Green(),
                              BGRAColor::Green(), aOutputRect, aInputRect,
                              aInputWriteRect, aOutputWriteRect, aFuzz);
}

void CheckTransformedWritePixels(
    Decoder* aDecoder, SurfaceFilter* aFilter, const BGRAColor& aInputColor,
    const BGRAColor& aOutputColor,
    const Maybe<IntRect>& aOutputRect /* = Nothing() */,
    const Maybe<IntRect>& aInputRect /* = Nothing() */,
    const Maybe<IntRect>& aInputWriteRect /* = Nothing() */,
    const Maybe<IntRect>& aOutputWriteRect /* = Nothing() */,
    uint8_t aFuzz /* = 0 */) {
  IntRect outputRect = aOutputRect.valueOr(IntRect(0, 0, 100, 100));
  IntRect inputRect = aInputRect.valueOr(IntRect(0, 0, 100, 100));
  IntRect inputWriteRect = aInputWriteRect.valueOr(inputRect);
  IntRect outputWriteRect = aOutputWriteRect.valueOr(outputRect);

  // Fill the image.
  int32_t count = 0;
  auto result = aFilter->WritePixels<uint32_t>([&] {
    ++count;
    return AsVariant(aInputColor.AsPixel());
  });
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_EQ(inputWriteRect.Width() * inputWriteRect.Height(), count);

  AssertCorrectPipelineFinalState(aFilter, inputRect, outputRect);

  // Attempt to write more data and make sure nothing changes.
  const int32_t oldCount = count;
  result = aFilter->WritePixels<uint32_t>([&] {
    ++count;
    return AsVariant(aInputColor.AsPixel());
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
  RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
  CheckGeneratedSurface(surface, outputWriteRect, aOutputColor,
                        BGRAColor::Transparent(), aFuzz);
}

///////////////////////////////////////////////////////////////////////////////
// Test Data
///////////////////////////////////////////////////////////////////////////////

ImageTestCase GreenPNGTestCase() {
  return ImageTestCase("green.png", "image/png", IntSize(100, 100));
}

ImageTestCase GreenGIFTestCase() {
  return ImageTestCase("green.gif", "image/gif", IntSize(100, 100));
}

ImageTestCase GreenJPGTestCase() {
  return ImageTestCase("green.jpg", "image/jpeg", IntSize(100, 100),
                       TEST_CASE_IS_FUZZY);
}

ImageTestCase GreenBMPTestCase() {
  return ImageTestCase("green.bmp", "image/bmp", IntSize(100, 100));
}

ImageTestCase GreenICOTestCase() {
  // This ICO contains a 32-bit BMP, and we use a BMP's alpha data by default
  // when the BMP is embedded in an ICO, so it's transparent.
  return ImageTestCase("green.ico", "image/x-icon", IntSize(100, 100),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase GreenIconTestCase() {
  return ImageTestCase("green.icon", "image/icon", IntSize(100, 100),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase GreenWebPTestCase() {
  return ImageTestCase("green.webp", "image/webp", IntSize(100, 100));
}

ImageTestCase LargeWebPTestCase() {
  return ImageTestCase("large.webp", "image/webp", IntSize(1200, 660),
                       TEST_CASE_IGNORE_OUTPUT);
}

ImageTestCase GreenWebPIccSrgbTestCase() {
  return ImageTestCase("green.icc_srgb.webp", "image/webp", IntSize(100, 100));
}

ImageTestCase GreenFirstFrameAnimatedGIFTestCase() {
  return ImageTestCase("first-frame-green.gif", "image/gif", IntSize(100, 100),
                       TEST_CASE_IS_ANIMATED);
}

ImageTestCase GreenFirstFrameAnimatedPNGTestCase() {
  return ImageTestCase("first-frame-green.png", "image/png", IntSize(100, 100),
                       TEST_CASE_IS_TRANSPARENT | TEST_CASE_IS_ANIMATED);
}

ImageTestCase GreenFirstFrameAnimatedWebPTestCase() {
  return ImageTestCase("first-frame-green.webp", "image/webp",
                       IntSize(100, 100), TEST_CASE_IS_ANIMATED);
}

ImageTestCase BlendAnimatedGIFTestCase() {
  return ImageTestCase("blend.gif", "image/gif", IntSize(100, 100),
                       TEST_CASE_IS_ANIMATED);
}

ImageTestCase BlendAnimatedPNGTestCase() {
  return ImageTestCase("blend.png", "image/png", IntSize(100, 100),
                       TEST_CASE_IS_TRANSPARENT | TEST_CASE_IS_ANIMATED);
}

ImageTestCase BlendAnimatedWebPTestCase() {
  return ImageTestCase("blend.webp", "image/webp", IntSize(100, 100),
                       TEST_CASE_IS_TRANSPARENT | TEST_CASE_IS_ANIMATED);
}

ImageTestCase CorruptTestCase() {
  return ImageTestCase("corrupt.jpg", "image/jpeg", IntSize(100, 100),
                       TEST_CASE_HAS_ERROR);
}

ImageTestCase CorruptBMPWithTruncatedHeader() {
  // This BMP has a header which is truncated right between the BIH and the
  // bitfields, which is a particularly error-prone place w.r.t. the BMP decoder
  // state machine.
  return ImageTestCase("invalid-truncated-metadata.bmp", "image/bmp",
                       IntSize(100, 100), TEST_CASE_HAS_ERROR);
}

ImageTestCase CorruptICOWithBadBMPWidthTestCase() {
  // This ICO contains a BMP icon which has a width that doesn't match the size
  // listed in the corresponding ICO directory entry.
  return ImageTestCase("corrupt-with-bad-bmp-width.ico", "image/x-icon",
                       IntSize(100, 100), TEST_CASE_HAS_ERROR);
}

ImageTestCase CorruptICOWithBadBMPHeightTestCase() {
  // This ICO contains a BMP icon which has a height that doesn't match the size
  // listed in the corresponding ICO directory entry.
  return ImageTestCase("corrupt-with-bad-bmp-height.ico", "image/x-icon",
                       IntSize(100, 100), TEST_CASE_HAS_ERROR);
}

ImageTestCase CorruptICOWithBadBppTestCase() {
  // This test case is an ICO with a BPP (15) in the ICO header which differs
  // from that in the BMP header itself (1). It should ignore the ICO BPP when
  // the BMP BPP is available and thus correctly decode the image.
  return ImageTestCase("corrupt-with-bad-ico-bpp.ico", "image/x-icon",
                       IntSize(100, 100), TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase TransparentPNGTestCase() {
  return ImageTestCase("transparent.png", "image/png", IntSize(32, 32),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase TransparentGIFTestCase() {
  return ImageTestCase("transparent.gif", "image/gif", IntSize(16, 16),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase TransparentWebPTestCase() {
  ImageTestCase test("transparent.webp", "image/webp", IntSize(100, 100),
                     TEST_CASE_IS_TRANSPARENT);
  test.mColor = BGRAColor::Transparent();
  return test;
}

ImageTestCase TransparentNoAlphaHeaderWebPTestCase() {
  ImageTestCase test("transparent-no-alpha-header.webp", "image/webp",
                     IntSize(100, 100), TEST_CASE_IS_FUZZY);
  test.mColor = BGRAColor(0x00, 0x00, 0x00, 0xFF);  // black
  return test;
}

ImageTestCase FirstFramePaddingGIFTestCase() {
  return ImageTestCase("transparent.gif", "image/gif", IntSize(16, 16),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase TransparentIfWithinICOBMPTestCase(TestCaseFlags aFlags) {
  // This is a BMP that is only transparent when decoded as if it is within an
  // ICO file. (Note: aFlags needs to be set to TEST_CASE_DEFAULT_FLAGS or
  // TEST_CASE_IS_TRANSPARENT accordingly.)
  return ImageTestCase("transparent-if-within-ico.bmp", "image/bmp",
                       IntSize(32, 32), aFlags);
}

ImageTestCase RLE4BMPTestCase() {
  return ImageTestCase("rle4.bmp", "image/bmp", IntSize(320, 240),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase RLE8BMPTestCase() {
  return ImageTestCase("rle8.bmp", "image/bmp", IntSize(32, 32),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase NoFrameDelayGIFTestCase() {
  // This is an invalid (or at least, questionably valid) GIF that's animated
  // even though it specifies a frame delay of zero. It's animated, but it's not
  // marked TEST_CASE_IS_ANIMATED because the metadata decoder can't detect that
  // it's animated.
  return ImageTestCase("no-frame-delay.gif", "image/gif", IntSize(100, 100));
}

ImageTestCase ExtraImageSubBlocksAnimatedGIFTestCase() {
  // This is a corrupt GIF that has extra image sub blocks between the first and
  // second frame.
  return ImageTestCase("animated-with-extra-image-sub-blocks.gif", "image/gif",
                       IntSize(100, 100));
}

ImageTestCase DownscaledPNGTestCase() {
  // This testcase (and all the other "downscaled") testcases) consists of 25
  // lines of green, followed by 25 lines of red, followed by 25 lines of green,
  // followed by 25 more lines of red. It's intended that tests downscale it
  // from 100x100 to 20x20, so we specify a 20x20 output size.
  return ImageTestCase("downscaled.png", "image/png", IntSize(100, 100),
                       IntSize(20, 20));
}

ImageTestCase DownscaledGIFTestCase() {
  return ImageTestCase("downscaled.gif", "image/gif", IntSize(100, 100),
                       IntSize(20, 20));
}

ImageTestCase DownscaledJPGTestCase() {
  return ImageTestCase("downscaled.jpg", "image/jpeg", IntSize(100, 100),
                       IntSize(20, 20));
}

ImageTestCase DownscaledBMPTestCase() {
  return ImageTestCase("downscaled.bmp", "image/bmp", IntSize(100, 100),
                       IntSize(20, 20));
}

ImageTestCase DownscaledICOTestCase() {
  return ImageTestCase("downscaled.ico", "image/x-icon", IntSize(100, 100),
                       IntSize(20, 20), TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase DownscaledIconTestCase() {
  return ImageTestCase("downscaled.icon", "image/icon", IntSize(100, 100),
                       IntSize(20, 20), TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase DownscaledWebPTestCase() {
  return ImageTestCase("downscaled.webp", "image/webp", IntSize(100, 100),
                       IntSize(20, 20));
}

ImageTestCase DownscaledTransparentICOWithANDMaskTestCase() {
  // This test case is an ICO with AND mask transparency. We want to ensure that
  // we can downscale it without crashing or triggering ASAN failures, but its
  // content isn't simple to verify, so for now we don't check the output.
  return ImageTestCase("transparent-ico-with-and-mask.ico", "image/x-icon",
                       IntSize(32, 32), IntSize(20, 20),
                       TEST_CASE_IS_TRANSPARENT | TEST_CASE_IGNORE_OUTPUT);
}

ImageTestCase TruncatedSmallGIFTestCase() {
  return ImageTestCase("green-1x1-truncated.gif", "image/gif", IntSize(1, 1));
}

ImageTestCase LargeICOWithBMPTestCase() {
  return ImageTestCase("green-large-bmp.ico", "image/x-icon", IntSize(256, 256),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase LargeICOWithPNGTestCase() {
  return ImageTestCase("green-large-png.ico", "image/x-icon", IntSize(512, 512),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase GreenMultipleSizesICOTestCase() {
  return ImageTestCase("green-multiple-sizes.ico", "image/x-icon",
                       IntSize(256, 256));
}

ImageTestCase PerfGrayJPGTestCase() {
  return ImageTestCase("perf_gray.jpg", "image/jpeg", IntSize(1000, 1000));
}

ImageTestCase PerfCmykJPGTestCase() {
  return ImageTestCase("perf_cmyk.jpg", "image/jpeg", IntSize(1000, 1000));
}

ImageTestCase PerfYCbCrJPGTestCase() {
  return ImageTestCase("perf_ycbcr.jpg", "image/jpeg", IntSize(1000, 1000));
}

ImageTestCase PerfRgbPNGTestCase() {
  return ImageTestCase("perf_srgb.png", "image/png", IntSize(1000, 1000));
}

ImageTestCase PerfRgbAlphaPNGTestCase() {
  return ImageTestCase("perf_srgb_alpha.png", "image/png", IntSize(1000, 1000),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase PerfGrayPNGTestCase() {
  return ImageTestCase("perf_gray.png", "image/png", IntSize(1000, 1000));
}

ImageTestCase PerfGrayAlphaPNGTestCase() {
  return ImageTestCase("perf_gray_alpha.png", "image/png", IntSize(1000, 1000),
                       TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase PerfRgbLosslessWebPTestCase() {
  return ImageTestCase("perf_srgb_lossless.webp", "image/webp",
                       IntSize(1000, 1000));
}

ImageTestCase PerfRgbAlphaLosslessWebPTestCase() {
  return ImageTestCase("perf_srgb_alpha_lossless.webp", "image/webp",
                       IntSize(1000, 1000), TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase PerfRgbLossyWebPTestCase() {
  return ImageTestCase("perf_srgb_lossy.webp", "image/webp",
                       IntSize(1000, 1000));
}

ImageTestCase PerfRgbAlphaLossyWebPTestCase() {
  return ImageTestCase("perf_srgb_alpha_lossy.webp", "image/webp",
                       IntSize(1000, 1000), TEST_CASE_IS_TRANSPARENT);
}

ImageTestCase PerfRgbGIFTestCase() {
  return ImageTestCase("perf_srgb.gif", "image/gif", IntSize(1000, 1000));
}

}  // namespace image
}  // namespace mozilla
