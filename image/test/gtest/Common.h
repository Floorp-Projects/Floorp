/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_test_gtest_Common_h
#define mozilla_image_test_gtest_Common_h

#include <vector>

#include "gtest/gtest.h"

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/2D.h"
#include "Decoder.h"
#include "gfxColor.h"
#include "imgITools.h"
#include "nsCOMPtr.h"
#include "SurfacePipe.h"
#include "SurfacePipeFactory.h"

class nsIInputStream;

namespace mozilla {
namespace image {

///////////////////////////////////////////////////////////////////////////////
// Types
///////////////////////////////////////////////////////////////////////////////

enum TestCaseFlags
{
  TEST_CASE_DEFAULT_FLAGS   = 0,
  TEST_CASE_IS_FUZZY        = 1 << 0,
  TEST_CASE_HAS_ERROR       = 1 << 1,
  TEST_CASE_IS_TRANSPARENT  = 1 << 2,
  TEST_CASE_IS_ANIMATED     = 1 << 3,
  TEST_CASE_IGNORE_OUTPUT   = 1 << 4,
};

struct ImageTestCase
{
  ImageTestCase(const char* aPath,
                const char* aMimeType,
                gfx::IntSize aSize,
                uint32_t aFlags = TEST_CASE_DEFAULT_FLAGS)
    : mPath(aPath)
    , mMimeType(aMimeType)
    , mSize(aSize)
    , mOutputSize(aSize)
    , mFlags(aFlags)
  { }

  ImageTestCase(const char* aPath,
                const char* aMimeType,
                gfx::IntSize aSize,
                gfx::IntSize aOutputSize,
                uint32_t aFlags = TEST_CASE_DEFAULT_FLAGS)
    : mPath(aPath)
    , mMimeType(aMimeType)
    , mSize(aSize)
    , mOutputSize(aOutputSize)
    , mFlags(aFlags)
  { }

  const char* mPath;
  const char* mMimeType;
  gfx::IntSize mSize;
  gfx::IntSize mOutputSize;
  uint32_t mFlags;
};

struct BGRAColor
{
  BGRAColor() : BGRAColor(0, 0, 0, 0) { }

  BGRAColor(uint8_t aBlue, uint8_t aGreen, uint8_t aRed, uint8_t aAlpha)
    : mBlue(aBlue)
    , mGreen(aGreen)
    , mRed(aRed)
    , mAlpha(aAlpha)
  { }

  static BGRAColor Green() { return BGRAColor(0x00, 0xFF, 0x00, 0xFF); }
  static BGRAColor Red()   { return BGRAColor(0x00, 0x00, 0xFF, 0xFF); }
  static BGRAColor Blue()   { return BGRAColor(0xFF, 0x00, 0x00, 0xFF); }
  static BGRAColor Transparent() { return BGRAColor(0x00, 0x00, 0x00, 0x00); }

  uint32_t AsPixel() const { return gfxPackedPixel(mAlpha, mRed, mGreen, mBlue); }

  uint8_t mBlue;
  uint8_t mGreen;
  uint8_t mRed;
  uint8_t mAlpha;
};


///////////////////////////////////////////////////////////////////////////////
// General Helpers
///////////////////////////////////////////////////////////////////////////////

/**
 * A RAII class that ensure that ImageLib services are available. Any tests that
 * require ImageLib to be initialized (for example, any test that uses the
 * SurfaceCache; see image::EnsureModuleInitialized() for the full list) can
 * use this class to ensure that ImageLib services are available. Failure to do
 * so can result in strange, non-deterministic failures.
 */
class AutoInitializeImageLib
{
public:
  AutoInitializeImageLib();
};

/// Loads a file from the current directory. @return an nsIInputStream for it.
already_AddRefed<nsIInputStream> LoadFile(const char* aRelativePath);

/**
 * @returns true if every pixel of @aSurface is @aColor.
 *
 * If @aFuzz is nonzero, a tolerance of @aFuzz is allowed in each color
 * component. This may be necessary for tests that involve JPEG images or
 * downscaling.
 */
bool IsSolidColor(gfx::SourceSurface* aSurface,
                  BGRAColor aColor,
                  uint8_t aFuzz = 0);

/**
 * @returns true if every pixel of @aDecoder's surface has the palette index
 * specified by @aColor.
 */
bool IsSolidPalettedColor(Decoder* aDecoder, uint8_t aColor);

/**
 * @returns true if every pixel in the range of rows specified by @aStartRow and
 * @aRowCount of @aSurface is @aColor.
 *
 * If @aFuzz is nonzero, a tolerance of @aFuzz is allowed in each color
 * component. This may be necessary for tests that involve JPEG images or
 * downscaling.
 */
bool RowsAreSolidColor(gfx::SourceSurface* aSurface,
                       int32_t aStartRow,
                       int32_t aRowCount,
                       BGRAColor aColor,
                       uint8_t aFuzz = 0);

/**
 * @returns true if every pixel in the range of rows specified by @aStartRow and
 * @aRowCount of @aDecoder's surface has the palette index specified by @aColor.
 */
bool PalettedRowsAreSolidColor(Decoder* aDecoder,
                               int32_t aStartRow,
                               int32_t aRowCount,
                               uint8_t aColor);

/**
 * @returns true if every pixel in the rect specified by @aRect is @aColor.
 *
 * If @aFuzz is nonzero, a tolerance of @aFuzz is allowed in each color
 * component. This may be necessary for tests that involve JPEG images or
 * downscaling.
 */
bool RectIsSolidColor(gfx::SourceSurface* aSurface,
                      const gfx::IntRect& aRect,
                      BGRAColor aColor,
                      uint8_t aFuzz = 0);

/**
 * @returns true if every pixel in the rect specified by @aRect has the palette
 * index specified by @aColor.
 */
bool PalettedRectIsSolidColor(Decoder* aDecoder,
                              const gfx::IntRect& aRect,
                              uint8_t aColor);

/**
 * @returns true if the pixels in @aRow of @aSurface match the pixels given in
 * @aPixels.
 */
bool RowHasPixels(gfx::SourceSurface* aSurface,
                  int32_t aRow,
                  const std::vector<BGRAColor>& aPixels);

// ExpectNoResume is an IResumable implementation for use by tests that expect
// Resume() to never get called.
class ExpectNoResume final : public IResumable
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ExpectNoResume, override)

  void Resume() override { FAIL() << "Resume() should not get called"; }

private:
  ~ExpectNoResume() override { }
};

// CountResumes is an IResumable implementation for use by tests that expect
// Resume() to get called a certain number of times.
class CountResumes : public IResumable
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CountResumes, override)

  CountResumes() : mCount(0) { }

  void Resume() override { mCount++; }
  uint32_t Count() const { return mCount; }

private:
  ~CountResumes() override { }

  uint32_t mCount;
};


///////////////////////////////////////////////////////////////////////////////
// SurfacePipe Helpers
///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a decoder with no data associated with, suitable for testing code
 * that requires a decoder to initialize or to allocate surfaces but doesn't
 * actually need the decoder to do any decoding.
 *
 * XXX(seth): We only need this because SurfaceSink and PalettedSurfaceSink
 * defer to the decoder for surface allocation. Once all decoders use
 * SurfacePipe we won't need to do that anymore and we can remove this function.
 */
already_AddRefed<Decoder> CreateTrivialDecoder();

/**
 * Creates a pipeline of SurfaceFilters from a list of Config structs and passes
 * it to the provided lambda @aFunc. Assertions that the pipeline is constructly
 * correctly and cleanup of any allocated surfaces is handled automatically.
 *
 * @param aDecoder The decoder to use for allocating surfaces.
 * @param aFunc The lambda function to pass the filter pipeline to.
 * @param aConfigs The configuration for the pipeline.
 */
template <typename Func, typename... Configs>
void WithFilterPipeline(Decoder* aDecoder, Func aFunc, const Configs&... aConfigs)
{
  auto pipe = MakeUnique<typename detail::FilterPipeline<Configs...>::Type>();
  nsresult rv = pipe->Configure(aConfigs...);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  aFunc(aDecoder, pipe.get());

  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  if (currentFrame) {
    currentFrame->Finish();
  }
}

/**
 * Creates a pipeline of SurfaceFilters from a list of Config structs and
 * asserts that configuring it fails. Cleanup of any allocated surfaces is
 * handled automatically.
 *
 * @param aDecoder The decoder to use for allocating surfaces.
 * @param aConfigs The configuration for the pipeline.
 */
template <typename... Configs>
void AssertConfiguringPipelineFails(Decoder* aDecoder, const Configs&... aConfigs)
{
  auto pipe = MakeUnique<typename detail::FilterPipeline<Configs...>::Type>();
  nsresult rv = pipe->Configure(aConfigs...);

  // Callers expect configuring the pipeline to fail.
  ASSERT_TRUE(NS_FAILED(rv));

  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  if (currentFrame) {
    currentFrame->Finish();
  }
}

/**
 * Asserts that the provided filter pipeline is in the correct final state,
 * which is to say, the entire surface has been written to (IsSurfaceFinished()
 * returns true) and the invalid rects are as expected.
 *
 * @param aFilter The filter pipeline to check.
 * @param aInputSpaceRect The expect invalid rect, in input space.
 * @param aoutputSpaceRect The expect invalid rect, in output space.
 */
void AssertCorrectPipelineFinalState(SurfaceFilter* aFilter,
                                     const gfx::IntRect& aInputSpaceRect,
                                     const gfx::IntRect& aOutputSpaceRect);

/**
 * Checks a generated image for correctness. Reports any unexpected deviation
 * from the expected image as GTest failures.
 *
 * @param aDecoder The decoder which contains the image. The decoder's current
 *                 frame will be checked.
 * @param aRect The region in the space of the output surface that the filter
 *              pipeline will actually write to. It's expected that pixels in
 *              this region are green, while pixels outside this region are
 *              transparent.
 * @param aFuzz The amount of fuzz to use in pixel comparisons.
 */
void CheckGeneratedImage(Decoder* aDecoder,
                         const gfx::IntRect& aRect,
                         uint8_t aFuzz = 0);

/**
 * Checks a generated paletted image for correctness. Reports any unexpected
 * deviation from the expected image as GTest failures.
 *
 * @param aDecoder The decoder which contains the image. The decoder's current
 *                 frame will be checked.
 * @param aRect The region in the space of the output surface that the filter
 *              pipeline will actually write to. It's expected that pixels in
 *              this region have a palette index of 255, while pixels outside
 *              this region have a palette index of 0.
 */
void CheckGeneratedPalettedImage(Decoder* aDecoder, const gfx::IntRect& aRect);

/**
 * Tests the result of calling WritePixels() using the provided SurfaceFilter
 * pipeline. The pipeline must be a normal (i.e., non-paletted) pipeline.
 *
 * The arguments are specified in the an order intended to minimize the number
 * of arguments that most test cases need to pass.
 *
 * @param aDecoder The decoder whose current frame will be written to.
 * @param aFilter The SurfaceFilter pipeline to use.
 * @param aOutputRect The region in the space of the output surface that will be
 *                    invalidated by the filter pipeline. Defaults to
 *                    (0, 0, 100, 100).
 * @param aInputRect The region in the space of the input image that will be
 *                   invalidated by the filter pipeline. Defaults to
 *                   (0, 0, 100, 100).
 * @param aInputWriteRect The region in the space of the input image that the
 *                        filter pipeline will allow writes to. Note the
 *                        difference from @aInputRect: @aInputRect is the actual
 *                        region invalidated, while @aInputWriteRect is the
 *                        region that is written to. These can differ in cases
 *                        where the input is not clipped to the size of the image.
 *                        Defaults to the entire input rect.
 * @param aOutputWriteRect The region in the space of the output surface that
 *                         the filter pipeline will actually write to. It's
 *                         expected that pixels in this region are green, while
 *                         pixels outside this region are transparent. Defaults
 *                         to the entire output rect.
 */
void CheckWritePixels(Decoder* aDecoder,
                      SurfaceFilter* aFilter,
                      const Maybe<gfx::IntRect>& aOutputRect = Nothing(),
                      const Maybe<gfx::IntRect>& aInputRect = Nothing(),
                      const Maybe<gfx::IntRect>& aInputWriteRect = Nothing(),
                      const Maybe<gfx::IntRect>& aOutputWriteRect = Nothing(),
                      uint8_t aFuzz = 0);

/**
 * Tests the result of calling WritePixels() using the provided SurfaceFilter
 * pipeline. The pipeline must be a paletted pipeline.
 * @see CheckWritePixels() for documentation of the arguments.
 */
void CheckPalettedWritePixels(Decoder* aDecoder,
                              SurfaceFilter* aFilter,
                              const Maybe<gfx::IntRect>& aOutputRect = Nothing(),
                              const Maybe<gfx::IntRect>& aInputRect = Nothing(),
                              const Maybe<gfx::IntRect>& aInputWriteRect = Nothing(),
                              const Maybe<gfx::IntRect>& aOutputWriteRect = Nothing(),
                              uint8_t aFuzz = 0);


///////////////////////////////////////////////////////////////////////////////
// Test Data
///////////////////////////////////////////////////////////////////////////////

ImageTestCase GreenPNGTestCase();
ImageTestCase GreenGIFTestCase();
ImageTestCase GreenJPGTestCase();
ImageTestCase GreenBMPTestCase();
ImageTestCase GreenICOTestCase();
ImageTestCase GreenIconTestCase();

ImageTestCase GreenFirstFrameAnimatedGIFTestCase();
ImageTestCase GreenFirstFrameAnimatedPNGTestCase();

ImageTestCase CorruptTestCase();
ImageTestCase CorruptBMPWithTruncatedHeader();
ImageTestCase CorruptICOWithBadBMPWidthTestCase();
ImageTestCase CorruptICOWithBadBMPHeightTestCase();
ImageTestCase CorruptICOWithBadBppTestCase();

ImageTestCase TransparentPNGTestCase();
ImageTestCase TransparentGIFTestCase();
ImageTestCase FirstFramePaddingGIFTestCase();
ImageTestCase TransparentIfWithinICOBMPTestCase(TestCaseFlags aFlags);
ImageTestCase NoFrameDelayGIFTestCase();
ImageTestCase ExtraImageSubBlocksAnimatedGIFTestCase();

ImageTestCase TransparentBMPWhenBMPAlphaEnabledTestCase();
ImageTestCase RLE4BMPTestCase();
ImageTestCase RLE8BMPTestCase();

ImageTestCase DownscaledPNGTestCase();
ImageTestCase DownscaledGIFTestCase();
ImageTestCase DownscaledJPGTestCase();
ImageTestCase DownscaledBMPTestCase();
ImageTestCase DownscaledICOTestCase();
ImageTestCase DownscaledIconTestCase();
ImageTestCase DownscaledTransparentICOWithANDMaskTestCase();

ImageTestCase TruncatedSmallGIFTestCase();

ImageTestCase LargeICOWithBMPTestCase();
ImageTestCase LargeICOWithPNGTestCase();
ImageTestCase GreenMultipleSizesICOTestCase();

} // namespace image
} // namespace mozilla

#endif // mozilla_image_test_gtest_Common_h
