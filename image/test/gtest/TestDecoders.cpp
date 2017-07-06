/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "Common.h"
#include "Decoder.h"
#include "DecoderFactory.h"
#include "decoders/nsBMPDecoder.h"
#include "IDecodingTask.h"
#include "imgIContainer.h"
#include "imgITools.h"
#include "ImageFactory.h"
#include "mozilla/gfx/2D.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "mozilla/RefPtr.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "ProgressTracker.h"
#include "SourceBuffer.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

static already_AddRefed<SourceSurface>
CheckDecoderState(const ImageTestCase& aTestCase, Decoder* aDecoder)
{
  EXPECT_TRUE(aDecoder->GetDecodeDone());
  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_HAS_ERROR),
            aDecoder->HasError());

  // Verify that the decoder made the expected progress.
  Progress progress = aDecoder->TakeProgress();
  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_HAS_ERROR),
            bool(progress & FLAG_HAS_ERROR));

  if (aTestCase.mFlags & TEST_CASE_HAS_ERROR) {
    return nullptr;  // That's all we can check for bad images.
  }

  EXPECT_TRUE(bool(progress & FLAG_SIZE_AVAILABLE));
  EXPECT_TRUE(bool(progress & FLAG_DECODE_COMPLETE));
  EXPECT_TRUE(bool(progress & FLAG_FRAME_COMPLETE));
  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_IS_TRANSPARENT),
            bool(progress & FLAG_HAS_TRANSPARENCY));
  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_IS_ANIMATED),
            bool(progress & FLAG_IS_ANIMATED));

  // The decoder should get the correct size.
  IntSize size = aDecoder->Size();
  EXPECT_EQ(aTestCase.mSize.width, size.width);
  EXPECT_EQ(aTestCase.mSize.height, size.height);

  // Get the current frame, which is always the first frame of the image
  // because CreateAnonymousDecoder() forces a first-frame-only decode.
  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();

  // Verify that the resulting surfaces matches our expectations.
  EXPECT_TRUE(surface->IsDataSourceSurface());
  EXPECT_TRUE(surface->GetFormat() == SurfaceFormat::B8G8R8X8 ||
              surface->GetFormat() == SurfaceFormat::B8G8R8A8);
  EXPECT_EQ(aTestCase.mOutputSize, surface->GetSize());

  return surface.forget();
}

static void
CheckDecoderResults(const ImageTestCase& aTestCase, Decoder* aDecoder)
{
  RefPtr<SourceSurface> surface = CheckDecoderState(aTestCase, aDecoder);
  if (!surface) {
    return;
  }

  if (aTestCase.mFlags & TEST_CASE_IGNORE_OUTPUT) {
    return;
  }

  // Check the output.
  EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Green(),
                           aTestCase.mFlags & TEST_CASE_IS_FUZZY ? 1 : 0));
}

template <typename Func>
void WithSingleChunkDecode(const ImageTestCase& aTestCase,
                           const Maybe<IntSize>& aOutputSize,
                           Func aResultChecker)
{
  nsCOMPtr<nsIInputStream> inputStream = LoadFile(aTestCase.mPath);
  ASSERT_TRUE(inputStream != nullptr);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Write the data into a SourceBuffer.
  NotNull<RefPtr<SourceBuffer>> sourceBuffer = WrapNotNull(new SourceBuffer());
  sourceBuffer->ExpectLength(length);
  rv = sourceBuffer->AppendFromInputStream(inputStream, length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  sourceBuffer->Complete(NS_OK);

  // Create a decoder.
  DecoderType decoderType =
    DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  RefPtr<Decoder> decoder =
    DecoderFactory::CreateAnonymousDecoder(decoderType, sourceBuffer, aOutputSize,
                                           DefaultSurfaceFlags());
  ASSERT_TRUE(decoder != nullptr);
  RefPtr<IDecodingTask> task = new AnonymousDecodingTask(WrapNotNull(decoder));

  // Run the full decoder synchronously.
  task->Run();

  // Call the lambda to verify the expected results.
  aResultChecker(decoder);
}

static void
CheckDecoderSingleChunk(const ImageTestCase& aTestCase)
{
  WithSingleChunkDecode(aTestCase, Nothing(), [&](Decoder* aDecoder) {
    CheckDecoderResults(aTestCase, aDecoder);
  });
}

static void
CheckDecoderMultiChunk(const ImageTestCase& aTestCase)
{
  nsCOMPtr<nsIInputStream> inputStream = LoadFile(aTestCase.mPath);
  ASSERT_TRUE(inputStream != nullptr);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Create a SourceBuffer and a decoder.
  NotNull<RefPtr<SourceBuffer>> sourceBuffer = WrapNotNull(new SourceBuffer());
  sourceBuffer->ExpectLength(length);
  DecoderType decoderType =
    DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  RefPtr<Decoder> decoder =
    DecoderFactory::CreateAnonymousDecoder(decoderType, sourceBuffer, Nothing(),
                                           DefaultSurfaceFlags());
  ASSERT_TRUE(decoder != nullptr);
  RefPtr<IDecodingTask> task = new AnonymousDecodingTask(WrapNotNull(decoder));

  for (uint64_t read = 0; read < length ; ++read) {
    uint64_t available = 0;
    rv = inputStream->Available(&available);
    ASSERT_TRUE(available > 0);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    rv = sourceBuffer->AppendFromInputStream(inputStream, 1);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    task->Run();
  }

  sourceBuffer->Complete(NS_OK);
  task->Run();

  CheckDecoderResults(aTestCase, decoder);
}

static void
CheckDownscaleDuringDecode(const ImageTestCase& aTestCase)
{
  // This function expects that |aTestCase| consists of 25 lines of green,
  // followed by 25 lines of red, followed by 25 lines of green, followed by 25
  // more lines of red. We'll downscale it from 100x100 to 20x20.
  IntSize outputSize(20, 20);

  WithSingleChunkDecode(aTestCase, Some(outputSize), [&](Decoder* aDecoder) {
    RefPtr<SourceSurface> surface = CheckDecoderState(aTestCase, aDecoder);

    // There are no downscale-during-decode tests that have TEST_CASE_HAS_ERROR
    // set, so we expect to always get a surface here.
    EXPECT_TRUE(surface != nullptr);

    if (aTestCase.mFlags & TEST_CASE_IGNORE_OUTPUT) {
      return;
    }

    // Check that the downscaled image is correct. Note that we skip rows near
    // the transitions between colors, since the downscaler does not produce a
    // sharp boundary at these points. Even some of the rows we test need a
    // small amount of fuzz; this is just the nature of Lanczos downscaling.
    EXPECT_TRUE(RowsAreSolidColor(surface, 0, 4, BGRAColor::Green(), /* aFuzz = */ 47));
    EXPECT_TRUE(RowsAreSolidColor(surface, 6, 3, BGRAColor::Red(), /* aFuzz = */ 27));
    EXPECT_TRUE(RowsAreSolidColor(surface, 11, 3, BGRAColor::Green(), /* aFuzz = */ 47));
    EXPECT_TRUE(RowsAreSolidColor(surface, 16, 4, BGRAColor::Red(), /* aFuzz = */ 27));
  });
}

class ImageDecoders : public ::testing::Test
{
protected:
  AutoInitializeImageLib mInit;
};

TEST_F(ImageDecoders, PNGSingleChunk)
{
  CheckDecoderSingleChunk(GreenPNGTestCase());
}

TEST_F(ImageDecoders, PNGMultiChunk)
{
  CheckDecoderMultiChunk(GreenPNGTestCase());
}

TEST_F(ImageDecoders, PNGDownscaleDuringDecode)
{
  CheckDownscaleDuringDecode(DownscaledPNGTestCase());
}

TEST_F(ImageDecoders, GIFSingleChunk)
{
  CheckDecoderSingleChunk(GreenGIFTestCase());
}

TEST_F(ImageDecoders, GIFMultiChunk)
{
  CheckDecoderMultiChunk(GreenGIFTestCase());
}

TEST_F(ImageDecoders, GIFDownscaleDuringDecode)
{
  CheckDownscaleDuringDecode(DownscaledGIFTestCase());
}

TEST_F(ImageDecoders, JPGSingleChunk)
{
  CheckDecoderSingleChunk(GreenJPGTestCase());
}

TEST_F(ImageDecoders, JPGMultiChunk)
{
  CheckDecoderMultiChunk(GreenJPGTestCase());
}

TEST_F(ImageDecoders, JPGDownscaleDuringDecode)
{
  CheckDownscaleDuringDecode(DownscaledJPGTestCase());
}

TEST_F(ImageDecoders, BMPSingleChunk)
{
  CheckDecoderSingleChunk(GreenBMPTestCase());
}

TEST_F(ImageDecoders, BMPMultiChunk)
{
  CheckDecoderMultiChunk(GreenBMPTestCase());
}

TEST_F(ImageDecoders, BMPDownscaleDuringDecode)
{
  CheckDownscaleDuringDecode(DownscaledBMPTestCase());
}

TEST_F(ImageDecoders, ICOSingleChunk)
{
  CheckDecoderSingleChunk(GreenICOTestCase());
}

TEST_F(ImageDecoders, ICOMultiChunk)
{
  CheckDecoderMultiChunk(GreenICOTestCase());
}

TEST_F(ImageDecoders, ICODownscaleDuringDecode)
{
  CheckDownscaleDuringDecode(DownscaledICOTestCase());
}

TEST_F(ImageDecoders, ICOWithANDMaskDownscaleDuringDecode)
{
  CheckDownscaleDuringDecode(DownscaledTransparentICOWithANDMaskTestCase());
}

TEST_F(ImageDecoders, IconSingleChunk)
{
  CheckDecoderSingleChunk(GreenIconTestCase());
}

TEST_F(ImageDecoders, IconMultiChunk)
{
  CheckDecoderMultiChunk(GreenIconTestCase());
}

TEST_F(ImageDecoders, IconDownscaleDuringDecode)
{
  CheckDownscaleDuringDecode(DownscaledIconTestCase());
}

TEST_F(ImageDecoders, AnimatedGIFSingleChunk)
{
  CheckDecoderSingleChunk(GreenFirstFrameAnimatedGIFTestCase());
}

TEST_F(ImageDecoders, AnimatedGIFMultiChunk)
{
  CheckDecoderMultiChunk(GreenFirstFrameAnimatedGIFTestCase());
}

TEST_F(ImageDecoders, AnimatedPNGSingleChunk)
{
  CheckDecoderSingleChunk(GreenFirstFrameAnimatedPNGTestCase());
}

TEST_F(ImageDecoders, AnimatedPNGMultiChunk)
{
  CheckDecoderMultiChunk(GreenFirstFrameAnimatedPNGTestCase());
}

TEST_F(ImageDecoders, CorruptSingleChunk)
{
  CheckDecoderSingleChunk(CorruptTestCase());
}

TEST_F(ImageDecoders, CorruptMultiChunk)
{
  CheckDecoderMultiChunk(CorruptTestCase());
}

TEST_F(ImageDecoders, CorruptBMPWithTruncatedHeaderSingleChunk)
{
  CheckDecoderSingleChunk(CorruptBMPWithTruncatedHeader());
}

TEST_F(ImageDecoders, CorruptBMPWithTruncatedHeaderMultiChunk)
{
  CheckDecoderMultiChunk(CorruptBMPWithTruncatedHeader());
}

TEST_F(ImageDecoders, CorruptICOWithBadBMPWidthSingleChunk)
{
  CheckDecoderSingleChunk(CorruptICOWithBadBMPWidthTestCase());
}

TEST_F(ImageDecoders, CorruptICOWithBadBMPWidthMultiChunk)
{
  CheckDecoderMultiChunk(CorruptICOWithBadBMPWidthTestCase());
}

TEST_F(ImageDecoders, CorruptICOWithBadBMPHeightSingleChunk)
{
  CheckDecoderSingleChunk(CorruptICOWithBadBMPHeightTestCase());
}

TEST_F(ImageDecoders, CorruptICOWithBadBMPHeightMultiChunk)
{
  CheckDecoderMultiChunk(CorruptICOWithBadBMPHeightTestCase());
}

TEST_F(ImageDecoders, CorruptICOWithBadBppSingleChunk)
{
  CheckDecoderSingleChunk(CorruptICOWithBadBppTestCase());
}

TEST_F(ImageDecoders, AnimatedGIFWithFRAME_FIRST)
{
  ImageTestCase testCase = GreenFirstFrameAnimatedGIFTestCase();

  // Verify that we can decode this test case and retrieve the first frame using
  // imgIContainer::FRAME_FIRST. This ensures that we correctly trigger a
  // single-frame decode rather than an animated decode when
  // imgIContainer::FRAME_FIRST is requested.

  // Create an image.
  RefPtr<Image> image =
    ImageFactory::CreateAnonymousImage(nsDependentCString(testCase.mMimeType));
  ASSERT_TRUE(!image->HasError());

  nsCOMPtr<nsIInputStream> inputStream = LoadFile(testCase.mPath);
  ASSERT_TRUE(inputStream);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Write the data into the image.
  rv = image->OnImageDataAvailable(nullptr, nullptr, inputStream, 0,
                                   static_cast<uint32_t>(length));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Let the image know we've sent all the data.
  rv = image->OnImageDataComplete(nullptr, nullptr, NS_OK, true);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  RefPtr<ProgressTracker> tracker = image->GetProgressTracker();
  tracker->SyncNotifyProgress(FLAG_LOAD_COMPLETE);

  // Lock the image so its surfaces don't disappear during the test.
  image->LockImage();

  // Use GetFrame() to force a sync decode of the image, specifying FRAME_FIRST
  // to ensure that we don't get an animated decode.
  RefPtr<SourceSurface> surface =
    image->GetFrame(imgIContainer::FRAME_FIRST,
                    imgIContainer::FLAG_SYNC_DECODE);

  // Ensure that the image's metadata meets our expectations.
  IntSize imageSize(0, 0);
  rv = image->GetWidth(&imageSize.width);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  rv = image->GetHeight(&imageSize.height);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  EXPECT_EQ(testCase.mSize.width, imageSize.width);
  EXPECT_EQ(testCase.mSize.height, imageSize.height);

  Progress imageProgress = tracker->GetProgress();

  EXPECT_TRUE(bool(imageProgress & FLAG_HAS_TRANSPARENCY) == false);
  EXPECT_TRUE(bool(imageProgress & FLAG_IS_ANIMATED) == true);

  // Ensure that we decoded the static version of the image.
  {
    LookupResult result =
      SurfaceCache::Lookup(ImageKey(image.get()),
                           RasterSurfaceKey(imageSize,
                                            DefaultSurfaceFlags(),
                                            PlaybackType::eStatic));
    ASSERT_EQ(MatchType::EXACT, result.Type());
    EXPECT_TRUE(bool(result.Surface()));
  }

  // Ensure that we didn't decode the animated version of the image.
  {
    LookupResult result =
      SurfaceCache::Lookup(ImageKey(image.get()),
                           RasterSurfaceKey(imageSize,
                                            DefaultSurfaceFlags(),
                                            PlaybackType::eAnimated));
    ASSERT_EQ(MatchType::NOT_FOUND, result.Type());
  }

  // Use GetFrame() to force a sync decode of the image, this time specifying
  // FRAME_CURRENT to ensure that we get an animated decode.
  RefPtr<SourceSurface> animatedSurface =
    image->GetFrame(imgIContainer::FRAME_CURRENT,
                    imgIContainer::FLAG_SYNC_DECODE);

  // Ensure that we decoded both frames of the animated version of the image.
  {
    LookupResult result =
      SurfaceCache::Lookup(ImageKey(image.get()),
                           RasterSurfaceKey(imageSize,
                                            DefaultSurfaceFlags(),
                                            PlaybackType::eAnimated));
    ASSERT_EQ(MatchType::EXACT, result.Type());

    EXPECT_TRUE(NS_SUCCEEDED(result.Surface().Seek(0)));
    EXPECT_TRUE(bool(result.Surface()));

    EXPECT_TRUE(NS_SUCCEEDED(result.Surface().Seek(1)));
    EXPECT_TRUE(bool(result.Surface()));
  }

  // Ensure that the static version is still around.
  {
    LookupResult result =
      SurfaceCache::Lookup(ImageKey(image.get()),
                           RasterSurfaceKey(imageSize,
                                            DefaultSurfaceFlags(),
                                            PlaybackType::eStatic));
    ASSERT_EQ(MatchType::EXACT, result.Type());
    EXPECT_TRUE(bool(result.Surface()));
  }
}

TEST_F(ImageDecoders, AnimatedGIFWithFRAME_CURRENT)
{
  ImageTestCase testCase = GreenFirstFrameAnimatedGIFTestCase();

  // Verify that we can decode this test case and retrieve the entire sequence
  // of frames using imgIContainer::FRAME_CURRENT. This ensures that we
  // correctly trigger an animated decode rather than a single-frame decode when
  // imgIContainer::FRAME_CURRENT is requested.

  // Create an image.
  RefPtr<Image> image =
    ImageFactory::CreateAnonymousImage(nsDependentCString(testCase.mMimeType));
  ASSERT_TRUE(!image->HasError());

  nsCOMPtr<nsIInputStream> inputStream = LoadFile(testCase.mPath);
  ASSERT_TRUE(inputStream);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Write the data into the image.
  rv = image->OnImageDataAvailable(nullptr, nullptr, inputStream, 0,
                                   static_cast<uint32_t>(length));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Let the image know we've sent all the data.
  rv = image->OnImageDataComplete(nullptr, nullptr, NS_OK, true);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  RefPtr<ProgressTracker> tracker = image->GetProgressTracker();
  tracker->SyncNotifyProgress(FLAG_LOAD_COMPLETE);

  // Lock the image so its surfaces don't disappear during the test.
  image->LockImage();

  // Use GetFrame() to force a sync decode of the image, specifying
  // FRAME_CURRENT to ensure we get an animated decode.
  RefPtr<SourceSurface> surface =
    image->GetFrame(imgIContainer::FRAME_CURRENT,
                    imgIContainer::FLAG_SYNC_DECODE);

  // Ensure that the image's metadata meets our expectations.
  IntSize imageSize(0, 0);
  rv = image->GetWidth(&imageSize.width);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  rv = image->GetHeight(&imageSize.height);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  EXPECT_EQ(testCase.mSize.width, imageSize.width);
  EXPECT_EQ(testCase.mSize.height, imageSize.height);

  Progress imageProgress = tracker->GetProgress();

  EXPECT_TRUE(bool(imageProgress & FLAG_HAS_TRANSPARENCY) == false);
  EXPECT_TRUE(bool(imageProgress & FLAG_IS_ANIMATED) == true);

  // Ensure that we decoded both frames of the animated version of the image.
  {
    LookupResult result =
      SurfaceCache::Lookup(ImageKey(image.get()),
                           RasterSurfaceKey(imageSize,
                                            DefaultSurfaceFlags(),
                                            PlaybackType::eAnimated));
    ASSERT_EQ(MatchType::EXACT, result.Type());

    EXPECT_TRUE(NS_SUCCEEDED(result.Surface().Seek(0)));
    EXPECT_TRUE(bool(result.Surface()));

    EXPECT_TRUE(NS_SUCCEEDED(result.Surface().Seek(1)));
    EXPECT_TRUE(bool(result.Surface()));
  }

  // Ensure that we didn't decode the static version of the image.
  {
    LookupResult result =
      SurfaceCache::Lookup(ImageKey(image.get()),
                           RasterSurfaceKey(imageSize,
                                            DefaultSurfaceFlags(),
                                            PlaybackType::eStatic));
    ASSERT_EQ(MatchType::NOT_FOUND, result.Type());
  }

  // Use GetFrame() to force a sync decode of the image, this time specifying
  // FRAME_FIRST to ensure that we get a single-frame decode.
  RefPtr<SourceSurface> animatedSurface =
    image->GetFrame(imgIContainer::FRAME_FIRST,
                    imgIContainer::FLAG_SYNC_DECODE);

  // Ensure that we decoded the static version of the image.
  {
    LookupResult result =
      SurfaceCache::Lookup(ImageKey(image.get()),
                           RasterSurfaceKey(imageSize,
                                            DefaultSurfaceFlags(),
                                            PlaybackType::eStatic));
    ASSERT_EQ(MatchType::EXACT, result.Type());
    EXPECT_TRUE(bool(result.Surface()));
  }

  // Ensure that both frames of the animated version are still around.
  {
    LookupResult result =
      SurfaceCache::Lookup(ImageKey(image.get()),
                           RasterSurfaceKey(imageSize,
                                            DefaultSurfaceFlags(),
                                            PlaybackType::eAnimated));
    ASSERT_EQ(MatchType::EXACT, result.Type());

    EXPECT_TRUE(NS_SUCCEEDED(result.Surface().Seek(0)));
    EXPECT_TRUE(bool(result.Surface()));

    EXPECT_TRUE(NS_SUCCEEDED(result.Surface().Seek(1)));
    EXPECT_TRUE(bool(result.Surface()));
  }
}

TEST_F(ImageDecoders, AnimatedGIFWithExtraImageSubBlocks)
{
  ImageTestCase testCase = ExtraImageSubBlocksAnimatedGIFTestCase();

  // Verify that we can decode this test case and get two frames, even though
  // there are extra image sub blocks between the first and second frame. The
  // extra data shouldn't confuse the decoder or cause the decode to fail.

  // Create an image.
  RefPtr<Image> image =
    ImageFactory::CreateAnonymousImage(nsDependentCString(testCase.mMimeType));
  ASSERT_TRUE(!image->HasError());

  nsCOMPtr<nsIInputStream> inputStream = LoadFile(testCase.mPath);
  ASSERT_TRUE(inputStream);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Write the data into the image.
  rv = image->OnImageDataAvailable(nullptr, nullptr, inputStream, 0,
                                   static_cast<uint32_t>(length));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Let the image know we've sent all the data.
  rv = image->OnImageDataComplete(nullptr, nullptr, NS_OK, true);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  RefPtr<ProgressTracker> tracker = image->GetProgressTracker();
  tracker->SyncNotifyProgress(FLAG_LOAD_COMPLETE);

  // Use GetFrame() to force a sync decode of the image.
  RefPtr<SourceSurface> surface =
    image->GetFrame(imgIContainer::FRAME_CURRENT,
                    imgIContainer::FLAG_SYNC_DECODE);

  // Ensure that the image's metadata meets our expectations.
  IntSize imageSize(0, 0);
  rv = image->GetWidth(&imageSize.width);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  rv = image->GetHeight(&imageSize.height);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  EXPECT_EQ(testCase.mSize.width, imageSize.width);
  EXPECT_EQ(testCase.mSize.height, imageSize.height);

  Progress imageProgress = tracker->GetProgress();

  EXPECT_TRUE(bool(imageProgress & FLAG_HAS_TRANSPARENCY) == false);
  EXPECT_TRUE(bool(imageProgress & FLAG_IS_ANIMATED) == true);

  // Ensure that we decoded both frames of the image.
  LookupResult result =
    SurfaceCache::Lookup(ImageKey(image.get()),
                         RasterSurfaceKey(imageSize,
                                          DefaultSurfaceFlags(),
                                          PlaybackType::eAnimated));
  ASSERT_EQ(MatchType::EXACT, result.Type());

  EXPECT_TRUE(NS_SUCCEEDED(result.Surface().Seek(0)));
  EXPECT_TRUE(bool(result.Surface()));

  EXPECT_TRUE(NS_SUCCEEDED(result.Surface().Seek(1)));
  EXPECT_TRUE(bool(result.Surface()));
}

TEST_F(ImageDecoders, TruncatedSmallGIFSingleChunk)
{
  CheckDecoderSingleChunk(TruncatedSmallGIFTestCase());
}

TEST_F(ImageDecoders, MultipleSizesICOSingleChunk)
{
  ImageTestCase testCase = GreenMultipleSizesICOTestCase();

  // Create an image.
  RefPtr<Image> image =
    ImageFactory::CreateAnonymousImage(nsDependentCString(testCase.mMimeType));
  ASSERT_TRUE(!image->HasError());

  nsCOMPtr<nsIInputStream> inputStream = LoadFile(testCase.mPath);
  ASSERT_TRUE(inputStream);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Write the data into the image.
  rv = image->OnImageDataAvailable(nullptr, nullptr, inputStream, 0,
                                   static_cast<uint32_t>(length));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Let the image know we've sent all the data.
  rv = image->OnImageDataComplete(nullptr, nullptr, NS_OK, true);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  RefPtr<ProgressTracker> tracker = image->GetProgressTracker();
  tracker->SyncNotifyProgress(FLAG_LOAD_COMPLETE);

  // Use GetFrame() to force a sync decode of the image.
  RefPtr<SourceSurface> surface =
    image->GetFrame(imgIContainer::FRAME_CURRENT,
                    imgIContainer::FLAG_SYNC_DECODE);

  // Ensure that the image's metadata meets our expectations.
  IntSize imageSize(0, 0);
  rv = image->GetWidth(&imageSize.width);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  rv = image->GetHeight(&imageSize.height);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  EXPECT_EQ(testCase.mSize.width, imageSize.width);
  EXPECT_EQ(testCase.mSize.height, imageSize.height);

  nsTArray<IntSize> nativeSizes;
  rv = image->GetNativeSizes(nativeSizes);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ(6u, nativeSizes.Length());

  IntSize expectedSizes[] = {
    IntSize(16, 16),
    IntSize(32, 32),
    IntSize(64, 64),
    IntSize(128, 128),
    IntSize(256, 256),
    IntSize(256, 128)
  };

  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(expectedSizes[i], nativeSizes[i]);
  }

  RefPtr<Image> image90 =
    ImageOps::Orient(image, Orientation(Angle::D90, Flip::Unflipped));
  rv = image90->GetNativeSizes(nativeSizes);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ(6u, nativeSizes.Length());

  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(expectedSizes[i], nativeSizes[i]);
  }
  EXPECT_EQ(IntSize(128, 256), nativeSizes[5]);

  RefPtr<Image> image180 =
    ImageOps::Orient(image, Orientation(Angle::D180, Flip::Unflipped));
  rv = image180->GetNativeSizes(nativeSizes);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_EQ(6u, nativeSizes.Length());

  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(expectedSizes[i], nativeSizes[i]);
  }
}
