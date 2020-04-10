/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "Common.h"
#include "AnimationSurfaceProvider.h"
#include "Decoder.h"
#include "DecoderFactory.h"
#include "decoders/nsBMPDecoder.h"
#include "IDecodingTask.h"
#include "ImageOps.h"
#include "imgIContainer.h"
#include "ImageFactory.h"
#include "mozilla/gfx/2D.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "mozilla/RefPtr.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "ProgressTracker.h"
#include "SourceBuffer.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

static already_AddRefed<SourceSurface> CheckDecoderState(
    const ImageTestCase& aTestCase, image::Decoder* aDecoder) {
  // image::Decoder should match what we asked for in the MIME type.
  EXPECT_NE(aDecoder->GetType(), DecoderType::UNKNOWN);
  EXPECT_EQ(aDecoder->GetType(),
            DecoderFactory::GetDecoderType(aTestCase.mMimeType));

  EXPECT_TRUE(aDecoder->GetDecodeDone());
  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_HAS_ERROR), aDecoder->HasError());

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
  EXPECT_TRUE(surface->GetFormat() == SurfaceFormat::OS_RGBX ||
              surface->GetFormat() == SurfaceFormat::OS_RGBA);
  EXPECT_EQ(aTestCase.mOutputSize, surface->GetSize());

  return surface.forget();
}

static void CheckDecoderResults(const ImageTestCase& aTestCase,
                                image::Decoder* aDecoder) {
  RefPtr<SourceSurface> surface = CheckDecoderState(aTestCase, aDecoder);
  if (!surface) {
    return;
  }

  if (aTestCase.mFlags & TEST_CASE_IGNORE_OUTPUT) {
    return;
  }

  // Check the output.
  EXPECT_TRUE(IsSolidColor(surface, aTestCase.Color(), aTestCase.Fuzz()));
}

template <typename Func>
void WithSingleChunkDecode(const ImageTestCase& aTestCase,
                           const Maybe<IntSize>& aOutputSize,
                           Func aResultChecker) {
  nsCOMPtr<nsIInputStream> inputStream = LoadFile(aTestCase.mPath);
  ASSERT_TRUE(inputStream != nullptr);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Write the data into a SourceBuffer.
  auto sourceBuffer = MakeNotNull<RefPtr<SourceBuffer>>();
  sourceBuffer->ExpectLength(length);
  rv = sourceBuffer->AppendFromInputStream(inputStream, length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  sourceBuffer->Complete(NS_OK);

  // Create a decoder.
  DecoderType decoderType = DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  RefPtr<image::Decoder> decoder = DecoderFactory::CreateAnonymousDecoder(
      decoderType, sourceBuffer, aOutputSize, DecoderFlags::FIRST_FRAME_ONLY,
      aTestCase.mSurfaceFlags);
  ASSERT_TRUE(decoder != nullptr);
  RefPtr<IDecodingTask> task =
      new AnonymousDecodingTask(WrapNotNull(decoder), /* aResumable */ false);

  // Run the full decoder synchronously.
  task->Run();

  // Call the lambda to verify the expected results.
  aResultChecker(decoder);
}

static void CheckDecoderSingleChunk(const ImageTestCase& aTestCase) {
  WithSingleChunkDecode(aTestCase, Nothing(), [&](image::Decoder* aDecoder) {
    CheckDecoderResults(aTestCase, aDecoder);
  });
}

template <typename Func>
void WithDelayedChunkDecode(const ImageTestCase& aTestCase,
                            const Maybe<IntSize>& aOutputSize,
                            Func aResultChecker) {
  nsCOMPtr<nsIInputStream> inputStream = LoadFile(aTestCase.mPath);
  ASSERT_TRUE(inputStream != nullptr);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Prepare an empty SourceBuffer.
  auto sourceBuffer = MakeNotNull<RefPtr<SourceBuffer>>();

  // Create a decoder.
  DecoderType decoderType = DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  RefPtr<image::Decoder> decoder = DecoderFactory::CreateAnonymousDecoder(
      decoderType, sourceBuffer, aOutputSize, DecoderFlags::FIRST_FRAME_ONLY,
      aTestCase.mSurfaceFlags);
  ASSERT_TRUE(decoder != nullptr);
  RefPtr<IDecodingTask> task =
      new AnonymousDecodingTask(WrapNotNull(decoder), /* aResumable */ true);

  // Run the full decoder synchronously. It should now be waiting on
  // the iterator to yield some data since we haven't written anything yet.
  task->Run();

  // Writing all of the data should wake up the decoder to complete.
  sourceBuffer->ExpectLength(length);
  rv = sourceBuffer->AppendFromInputStream(inputStream, length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  sourceBuffer->Complete(NS_OK);

  // It would have gotten posted to the main thread to avoid mutex contention.
  SpinPendingEvents();

  // Call the lambda to verify the expected results.
  aResultChecker(decoder);
}

static void CheckDecoderDelayedChunk(const ImageTestCase& aTestCase) {
  WithDelayedChunkDecode(aTestCase, Nothing(), [&](image::Decoder* aDecoder) {
    CheckDecoderResults(aTestCase, aDecoder);
  });
}

static void CheckDecoderMultiChunk(const ImageTestCase& aTestCase,
                                   uint64_t aChunkSize = 1) {
  nsCOMPtr<nsIInputStream> inputStream = LoadFile(aTestCase.mPath);
  ASSERT_TRUE(inputStream != nullptr);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Create a SourceBuffer and a decoder.
  auto sourceBuffer = MakeNotNull<RefPtr<SourceBuffer>>();
  sourceBuffer->ExpectLength(length);
  DecoderType decoderType = DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  RefPtr<image::Decoder> decoder = DecoderFactory::CreateAnonymousDecoder(
      decoderType, sourceBuffer, Nothing(), DecoderFlags::FIRST_FRAME_ONLY,
      aTestCase.mSurfaceFlags);
  ASSERT_TRUE(decoder != nullptr);
  RefPtr<IDecodingTask> task =
      new AnonymousDecodingTask(WrapNotNull(decoder), /* aResumable */ true);

  // Run the full decoder synchronously. It should now be waiting on
  // the iterator to yield some data since we haven't written anything yet.
  task->Run();

  while (length > 0) {
    uint64_t read = length > aChunkSize ? aChunkSize : length;
    length -= read;

    uint64_t available = 0;
    rv = inputStream->Available(&available);
    ASSERT_TRUE(available >= read);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    // Writing any data should wake up the decoder to complete.
    rv = sourceBuffer->AppendFromInputStream(inputStream, read);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    // It would have gotten posted to the main thread to avoid mutex contention.
    SpinPendingEvents();
  }

  sourceBuffer->Complete(NS_OK);
  SpinPendingEvents();

  CheckDecoderResults(aTestCase, decoder);
}

static void CheckDownscaleDuringDecode(const ImageTestCase& aTestCase) {
  // This function expects that |aTestCase| consists of 25 lines of green,
  // followed by 25 lines of red, followed by 25 lines of green, followed by 25
  // more lines of red. We'll downscale it from 100x100 to 20x20.
  IntSize outputSize(20, 20);

  WithSingleChunkDecode(
      aTestCase, Some(outputSize), [&](image::Decoder* aDecoder) {
        RefPtr<SourceSurface> surface = CheckDecoderState(aTestCase, aDecoder);

        // There are no downscale-during-decode tests that have
        // TEST_CASE_HAS_ERROR set, so we expect to always get a surface here.
        EXPECT_TRUE(surface != nullptr);

        if (aTestCase.mFlags & TEST_CASE_IGNORE_OUTPUT) {
          return;
        }

        // Check that the downscaled image is correct. Note that we skip rows
        // near the transitions between colors, since the downscaler does not
        // produce a sharp boundary at these points. Even some of the rows we
        // test need a small amount of fuzz; this is just the nature of Lanczos
        // downscaling.
        EXPECT_TRUE(RowsAreSolidColor(surface, 0, 4,
                                      aTestCase.ChooseColor(BGRAColor::Green()),
                                      /* aFuzz = */ 47));
        EXPECT_TRUE(RowsAreSolidColor(surface, 6, 3,
                                      aTestCase.ChooseColor(BGRAColor::Red()),
                                      /* aFuzz = */ 27));
        EXPECT_TRUE(RowsAreSolidColor(surface, 11, 3, BGRAColor::Green(),
                                      /* aFuzz = */ 47));
        EXPECT_TRUE(RowsAreSolidColor(surface, 16, 4,
                                      aTestCase.ChooseColor(BGRAColor::Red()),
                                      /* aFuzz = */ 27));
      });
}

static void CheckAnimationDecoderResults(const ImageTestCase& aTestCase,
                                         AnimationSurfaceProvider* aProvider,
                                         image::Decoder* aDecoder) {
  EXPECT_TRUE(aDecoder->GetDecodeDone());
  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_HAS_ERROR), aDecoder->HasError());

  if (aTestCase.mFlags & TEST_CASE_HAS_ERROR) {
    return;  // That's all we can check for bad images.
  }

  // The decoder should get the correct size.
  IntSize size = aDecoder->Size();
  EXPECT_EQ(aTestCase.mSize.width, size.width);
  EXPECT_EQ(aTestCase.mSize.height, size.height);

  if (aTestCase.mFlags & TEST_CASE_IGNORE_OUTPUT) {
    return;
  }

  // Check the output.
  AutoTArray<BGRAColor, 2> framePixels;
  framePixels.AppendElement(aTestCase.ChooseColor(BGRAColor::Green()));
  framePixels.AppendElement(
      aTestCase.ChooseColor(BGRAColor(0x7F, 0x7F, 0x7F, 0xFF)));

  DrawableSurface drawableSurface(WrapNotNull(aProvider));
  for (size_t i = 0; i < framePixels.Length(); ++i) {
    nsresult rv = drawableSurface.Seek(i);
    EXPECT_TRUE(NS_SUCCEEDED(rv));

    // Check the first frame, all green.
    RawAccessFrameRef rawFrame = drawableSurface->RawAccessRef();
    RefPtr<SourceSurface> surface = rawFrame->GetSourceSurface();

    // Verify that the resulting surfaces matches our expectations.
    EXPECT_TRUE(surface->IsDataSourceSurface());
    EXPECT_TRUE(surface->GetFormat() == SurfaceFormat::OS_RGBX ||
                surface->GetFormat() == SurfaceFormat::OS_RGBA);
    EXPECT_EQ(aTestCase.mOutputSize, surface->GetSize());
    EXPECT_TRUE(IsSolidColor(surface, framePixels[i], aTestCase.Fuzz()));
  }

  // Should be no more frames.
  nsresult rv = drawableSurface.Seek(framePixels.Length());
  EXPECT_TRUE(NS_FAILED(rv));
}

template <typename Func>
static void WithSingleChunkAnimationDecode(const ImageTestCase& aTestCase,
                                           Func aResultChecker) {
  // Create an image.
  RefPtr<Image> image = ImageFactory::CreateAnonymousImage(
      nsDependentCString(aTestCase.mMimeType));
  ASSERT_TRUE(!image->HasError());

  NotNull<RefPtr<RasterImage>> rasterImage =
      WrapNotNull(static_cast<RasterImage*>(image.get()));

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

  // Create a metadata decoder first, because otherwise RasterImage will get
  // unhappy about finding out the image is animated during a full decode.
  DecoderType decoderType = DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  RefPtr<IDecodingTask> task = DecoderFactory::CreateMetadataDecoder(
      decoderType, rasterImage, sourceBuffer);
  ASSERT_TRUE(task != nullptr);

  // Run the metadata decoder synchronously.
  task->Run();

  // Create a decoder.
  DecoderFlags decoderFlags = DefaultDecoderFlags();
  SurfaceFlags surfaceFlags = aTestCase.mSurfaceFlags;
  RefPtr<image::Decoder> decoder = DecoderFactory::CreateAnonymousDecoder(
      decoderType, sourceBuffer, Nothing(), decoderFlags, surfaceFlags);
  ASSERT_TRUE(decoder != nullptr);

  // Create an AnimationSurfaceProvider which will manage the decoding process
  // and make this decoder's output available in the surface cache.
  SurfaceKey surfaceKey = RasterSurfaceKey(aTestCase.mOutputSize, surfaceFlags,
                                           PlaybackType::eAnimated);
  RefPtr<AnimationSurfaceProvider> provider = new AnimationSurfaceProvider(
      rasterImage, surfaceKey, WrapNotNull(decoder),
      /* aCurrentFrame */ 0);

  // Run the full decoder synchronously.
  provider->Run();

  // Call the lambda to verify the expected results.
  aResultChecker(provider, decoder);
}

static void CheckAnimationDecoderSingleChunk(const ImageTestCase& aTestCase) {
  WithSingleChunkAnimationDecode(
      aTestCase,
      [&](AnimationSurfaceProvider* aProvider, image::Decoder* aDecoder) {
        CheckAnimationDecoderResults(aTestCase, aProvider, aDecoder);
      });
}

static void CheckDecoderFrameFirst(const ImageTestCase& aTestCase) {
  // Verify that we can decode this test case and retrieve the first frame using
  // imgIContainer::FRAME_FIRST. This ensures that we correctly trigger a
  // single-frame decode rather than an animated decode when
  // imgIContainer::FRAME_FIRST is requested.

  // Create an image.
  RefPtr<Image> image = ImageFactory::CreateAnonymousImage(
      nsDependentCString(aTestCase.mMimeType));
  ASSERT_TRUE(!image->HasError());

  nsCOMPtr<nsIInputStream> inputStream = LoadFile(aTestCase.mPath);
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

  auto unlock = mozilla::MakeScopeExit([&] { image->UnlockImage(); });

  // Use GetFrame() to force a sync decode of the image, specifying FRAME_FIRST
  // to ensure that we don't get an animated decode.
  RefPtr<SourceSurface> surface = image->GetFrame(
      imgIContainer::FRAME_FIRST, imgIContainer::FLAG_SYNC_DECODE);

  // Ensure that the image's metadata meets our expectations.
  IntSize imageSize(0, 0);
  rv = image->GetWidth(&imageSize.width);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  rv = image->GetHeight(&imageSize.height);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  EXPECT_EQ(aTestCase.mSize.width, imageSize.width);
  EXPECT_EQ(aTestCase.mSize.height, imageSize.height);

  Progress imageProgress = tracker->GetProgress();

  EXPECT_TRUE(bool(imageProgress & FLAG_HAS_TRANSPARENCY) == false);
  EXPECT_TRUE(bool(imageProgress & FLAG_IS_ANIMATED) == true);

  // Ensure that we decoded the static version of the image.
  {
    LookupResult result = SurfaceCache::Lookup(
        ImageKey(image.get()),
        RasterSurfaceKey(imageSize, aTestCase.mSurfaceFlags,
                         PlaybackType::eStatic),
        /* aMarkUsed = */ false);
    ASSERT_EQ(MatchType::EXACT, result.Type());
    EXPECT_TRUE(bool(result.Surface()));
  }

  // Ensure that we didn't decode the animated version of the image.
  {
    LookupResult result = SurfaceCache::Lookup(
        ImageKey(image.get()),
        RasterSurfaceKey(imageSize, aTestCase.mSurfaceFlags,
                         PlaybackType::eAnimated),
        /* aMarkUsed = */ false);
    ASSERT_EQ(MatchType::NOT_FOUND, result.Type());
  }

  // Use GetFrame() to force a sync decode of the image, this time specifying
  // FRAME_CURRENT to ensure that we get an animated decode.
  RefPtr<SourceSurface> animatedSurface = image->GetFrame(
      imgIContainer::FRAME_CURRENT, imgIContainer::FLAG_SYNC_DECODE);

  // Ensure that we decoded both frames of the animated version of the image.
  {
    LookupResult result = SurfaceCache::Lookup(
        ImageKey(image.get()),
        RasterSurfaceKey(imageSize, aTestCase.mSurfaceFlags,
                         PlaybackType::eAnimated),
        /* aMarkUsed = */ true);
    ASSERT_EQ(MatchType::EXACT, result.Type());

    EXPECT_TRUE(NS_SUCCEEDED(result.Surface().Seek(0)));
    EXPECT_TRUE(bool(result.Surface()));

    RefPtr<imgFrame> partialFrame = result.Surface().GetFrame(1);
    EXPECT_TRUE(bool(partialFrame));
  }

  // Ensure that the static version is still around.
  {
    LookupResult result = SurfaceCache::Lookup(
        ImageKey(image.get()),
        RasterSurfaceKey(imageSize, aTestCase.mSurfaceFlags,
                         PlaybackType::eStatic),
        /* aMarkUsed = */ true);
    ASSERT_EQ(MatchType::EXACT, result.Type());
    EXPECT_TRUE(bool(result.Surface()));
  }
}

static void CheckDecoderFrameCurrent(const ImageTestCase& aTestCase) {
  // Verify that we can decode this test case and retrieve the entire sequence
  // of frames using imgIContainer::FRAME_CURRENT. This ensures that we
  // correctly trigger an animated decode rather than a single-frame decode when
  // imgIContainer::FRAME_CURRENT is requested.

  // Create an image.
  RefPtr<Image> image = ImageFactory::CreateAnonymousImage(
      nsDependentCString(aTestCase.mMimeType));
  ASSERT_TRUE(!image->HasError());

  nsCOMPtr<nsIInputStream> inputStream = LoadFile(aTestCase.mPath);
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
  RefPtr<SourceSurface> surface = image->GetFrame(
      imgIContainer::FRAME_CURRENT, imgIContainer::FLAG_SYNC_DECODE);

  // Ensure that the image's metadata meets our expectations.
  IntSize imageSize(0, 0);
  rv = image->GetWidth(&imageSize.width);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  rv = image->GetHeight(&imageSize.height);
  EXPECT_TRUE(NS_SUCCEEDED(rv));

  EXPECT_EQ(aTestCase.mSize.width, imageSize.width);
  EXPECT_EQ(aTestCase.mSize.height, imageSize.height);

  Progress imageProgress = tracker->GetProgress();

  EXPECT_TRUE(bool(imageProgress & FLAG_HAS_TRANSPARENCY) == false);
  EXPECT_TRUE(bool(imageProgress & FLAG_IS_ANIMATED) == true);

  // Ensure that we decoded both frames of the animated version of the image.
  {
    LookupResult result = SurfaceCache::Lookup(
        ImageKey(image.get()),
        RasterSurfaceKey(imageSize, aTestCase.mSurfaceFlags,
                         PlaybackType::eAnimated),
        /* aMarkUsed = */ true);
    ASSERT_EQ(MatchType::EXACT, result.Type());

    EXPECT_TRUE(NS_SUCCEEDED(result.Surface().Seek(0)));
    EXPECT_TRUE(bool(result.Surface()));

    RefPtr<imgFrame> partialFrame = result.Surface().GetFrame(1);
    EXPECT_TRUE(bool(partialFrame));
  }

  // Ensure that we didn't decode the static version of the image.
  {
    LookupResult result = SurfaceCache::Lookup(
        ImageKey(image.get()),
        RasterSurfaceKey(imageSize, aTestCase.mSurfaceFlags,
                         PlaybackType::eStatic),
        /* aMarkUsed = */ false);
    ASSERT_EQ(MatchType::NOT_FOUND, result.Type());
  }

  // Use GetFrame() to force a sync decode of the image, this time specifying
  // FRAME_FIRST to ensure that we get a single-frame decode.
  RefPtr<SourceSurface> animatedSurface = image->GetFrame(
      imgIContainer::FRAME_FIRST, imgIContainer::FLAG_SYNC_DECODE);

  // Ensure that we decoded the static version of the image.
  {
    LookupResult result = SurfaceCache::Lookup(
        ImageKey(image.get()),
        RasterSurfaceKey(imageSize, aTestCase.mSurfaceFlags,
                         PlaybackType::eStatic),
        /* aMarkUsed = */ true);
    ASSERT_EQ(MatchType::EXACT, result.Type());
    EXPECT_TRUE(bool(result.Surface()));
  }

  // Ensure that both frames of the animated version are still around.
  {
    LookupResult result = SurfaceCache::Lookup(
        ImageKey(image.get()),
        RasterSurfaceKey(imageSize, aTestCase.mSurfaceFlags,
                         PlaybackType::eAnimated),
        /* aMarkUsed = */ true);
    ASSERT_EQ(MatchType::EXACT, result.Type());

    EXPECT_TRUE(NS_SUCCEEDED(result.Surface().Seek(0)));
    EXPECT_TRUE(bool(result.Surface()));

    RefPtr<imgFrame> partialFrame = result.Surface().GetFrame(1);
    EXPECT_TRUE(bool(partialFrame));
  }
}

class ImageDecoders : public ::testing::Test {
 protected:
  AutoInitializeImageLib mInit;
};

#define IMAGE_GTEST_DECODER_BASE_F(test_prefix)                              \
  TEST_F(ImageDecoders, test_prefix##SingleChunk) {                          \
    CheckDecoderSingleChunk(Green##test_prefix##TestCase());                 \
  }                                                                          \
                                                                             \
  TEST_F(ImageDecoders, test_prefix##DelayedChunk) {                         \
    CheckDecoderDelayedChunk(Green##test_prefix##TestCase());                \
  }                                                                          \
                                                                             \
  TEST_F(ImageDecoders, test_prefix##MultiChunk) {                           \
    CheckDecoderMultiChunk(Green##test_prefix##TestCase());                  \
  }                                                                          \
                                                                             \
  TEST_F(ImageDecoders, test_prefix##DownscaleDuringDecode) {                \
    CheckDownscaleDuringDecode(Downscaled##test_prefix##TestCase());         \
  }                                                                          \
                                                                             \
  TEST_F(ImageDecoders, test_prefix##ForceSRGB) {                            \
    CheckDecoderSingleChunk(Green##test_prefix##TestCase().WithSurfaceFlags( \
        SurfaceFlags::TO_SRGB_COLORSPACE));                                  \
  }

IMAGE_GTEST_DECODER_BASE_F(PNG)
IMAGE_GTEST_DECODER_BASE_F(GIF)
IMAGE_GTEST_DECODER_BASE_F(JPG)
IMAGE_GTEST_DECODER_BASE_F(BMP)
IMAGE_GTEST_DECODER_BASE_F(ICO)
IMAGE_GTEST_DECODER_BASE_F(Icon)
IMAGE_GTEST_DECODER_BASE_F(WebP)

TEST_F(ImageDecoders, ICOWithANDMaskDownscaleDuringDecode) {
  CheckDownscaleDuringDecode(DownscaledTransparentICOWithANDMaskTestCase());
}

TEST_F(ImageDecoders, WebPLargeMultiChunk) {
  CheckDecoderMultiChunk(LargeWebPTestCase(), /* aChunkSize */ 64);
}

TEST_F(ImageDecoders, WebPIccSrgbMultiChunk) {
  CheckDecoderMultiChunk(GreenWebPIccSrgbTestCase());
}

TEST_F(ImageDecoders, WebPTransparentSingleChunk) {
  CheckDecoderSingleChunk(TransparentWebPTestCase());
}

TEST_F(ImageDecoders, WebPTransparentNoAlphaHeaderSingleChunk) {
  CheckDecoderSingleChunk(TransparentNoAlphaHeaderWebPTestCase());
}

TEST_F(ImageDecoders, AnimatedGIFSingleChunk) {
  CheckDecoderSingleChunk(GreenFirstFrameAnimatedGIFTestCase());
}

TEST_F(ImageDecoders, AnimatedGIFMultiChunk) {
  CheckDecoderMultiChunk(GreenFirstFrameAnimatedGIFTestCase());
}

TEST_F(ImageDecoders, AnimatedGIFWithBlendedFrames) {
  CheckAnimationDecoderSingleChunk(GreenFirstFrameAnimatedGIFTestCase());
}

TEST_F(ImageDecoders, AnimatedPNGSingleChunk) {
  CheckDecoderSingleChunk(GreenFirstFrameAnimatedPNGTestCase());
}

TEST_F(ImageDecoders, AnimatedPNGMultiChunk) {
  CheckDecoderMultiChunk(GreenFirstFrameAnimatedPNGTestCase());
}

TEST_F(ImageDecoders, AnimatedPNGWithBlendedFrames) {
  CheckAnimationDecoderSingleChunk(GreenFirstFrameAnimatedPNGTestCase());
}

TEST_F(ImageDecoders, AnimatedWebPSingleChunk) {
  CheckDecoderSingleChunk(GreenFirstFrameAnimatedWebPTestCase());
}

TEST_F(ImageDecoders, AnimatedWebPMultiChunk) {
  CheckDecoderMultiChunk(GreenFirstFrameAnimatedWebPTestCase());
}

TEST_F(ImageDecoders, AnimatedWebPWithBlendedFrames) {
  CheckAnimationDecoderSingleChunk(GreenFirstFrameAnimatedWebPTestCase());
}

TEST_F(ImageDecoders, CorruptSingleChunk) {
  CheckDecoderSingleChunk(CorruptTestCase());
}

TEST_F(ImageDecoders, CorruptMultiChunk) {
  CheckDecoderMultiChunk(CorruptTestCase());
}

TEST_F(ImageDecoders, CorruptBMPWithTruncatedHeaderSingleChunk) {
  CheckDecoderSingleChunk(CorruptBMPWithTruncatedHeader());
}

TEST_F(ImageDecoders, CorruptBMPWithTruncatedHeaderMultiChunk) {
  CheckDecoderMultiChunk(CorruptBMPWithTruncatedHeader());
}

TEST_F(ImageDecoders, CorruptICOWithBadBMPWidthSingleChunk) {
  CheckDecoderSingleChunk(CorruptICOWithBadBMPWidthTestCase());
}

TEST_F(ImageDecoders, CorruptICOWithBadBMPWidthMultiChunk) {
  CheckDecoderMultiChunk(CorruptICOWithBadBMPWidthTestCase());
}

TEST_F(ImageDecoders, CorruptICOWithBadBMPHeightSingleChunk) {
  CheckDecoderSingleChunk(CorruptICOWithBadBMPHeightTestCase());
}

TEST_F(ImageDecoders, CorruptICOWithBadBMPHeightMultiChunk) {
  CheckDecoderMultiChunk(CorruptICOWithBadBMPHeightTestCase());
}

TEST_F(ImageDecoders, CorruptICOWithBadBppSingleChunk) {
  CheckDecoderSingleChunk(CorruptICOWithBadBppTestCase());
}

TEST_F(ImageDecoders, AnimatedGIFWithFRAME_FIRST) {
  CheckDecoderFrameFirst(GreenFirstFrameAnimatedGIFTestCase());
}

TEST_F(ImageDecoders, AnimatedGIFWithFRAME_CURRENT) {
  CheckDecoderFrameCurrent(GreenFirstFrameAnimatedGIFTestCase());
}

TEST_F(ImageDecoders, AnimatedGIFWithExtraImageSubBlocks) {
  ImageTestCase testCase = ExtraImageSubBlocksAnimatedGIFTestCase();

  // Verify that we can decode this test case and get two frames, even though
  // there are extra image sub blocks between the first and second frame. The
  // extra data shouldn't confuse the decoder or cause the decode to fail.

  // Create an image.
  RefPtr<Image> image = ImageFactory::CreateAnonymousImage(
      nsDependentCString(testCase.mMimeType));
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
  RefPtr<SourceSurface> surface = image->GetFrame(
      imgIContainer::FRAME_CURRENT, imgIContainer::FLAG_SYNC_DECODE);

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
                           RasterSurfaceKey(imageSize, testCase.mSurfaceFlags,
                                            PlaybackType::eAnimated),
                           /* aMarkUsed = */ true);
  ASSERT_EQ(MatchType::EXACT, result.Type());

  EXPECT_TRUE(NS_SUCCEEDED(result.Surface().Seek(0)));
  EXPECT_TRUE(bool(result.Surface()));

  RefPtr<imgFrame> partialFrame = result.Surface().GetFrame(1);
  EXPECT_TRUE(bool(partialFrame));
}

TEST_F(ImageDecoders, AnimatedWebPWithFRAME_FIRST) {
  CheckDecoderFrameFirst(GreenFirstFrameAnimatedWebPTestCase());
}

TEST_F(ImageDecoders, AnimatedWebPWithFRAME_CURRENT) {
  CheckDecoderFrameCurrent(GreenFirstFrameAnimatedWebPTestCase());
}

TEST_F(ImageDecoders, TruncatedSmallGIFSingleChunk) {
  CheckDecoderSingleChunk(TruncatedSmallGIFTestCase());
}

TEST_F(ImageDecoders, LargeICOWithBMPSingleChunk) {
  CheckDecoderSingleChunk(LargeICOWithBMPTestCase());
}

TEST_F(ImageDecoders, LargeICOWithBMPMultiChunk) {
  CheckDecoderMultiChunk(LargeICOWithBMPTestCase(), /* aChunkSize */ 64);
}

TEST_F(ImageDecoders, LargeICOWithPNGSingleChunk) {
  CheckDecoderSingleChunk(LargeICOWithPNGTestCase());
}

TEST_F(ImageDecoders, LargeICOWithPNGMultiChunk) {
  CheckDecoderMultiChunk(LargeICOWithPNGTestCase());
}

TEST_F(ImageDecoders, MultipleSizesICOSingleChunk) {
  ImageTestCase testCase = GreenMultipleSizesICOTestCase();

  // Create an image.
  RefPtr<Image> image = ImageFactory::CreateAnonymousImage(
      nsDependentCString(testCase.mMimeType));
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
  RefPtr<SourceSurface> surface = image->GetFrame(
      imgIContainer::FRAME_CURRENT, imgIContainer::FLAG_SYNC_DECODE);

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

  IntSize expectedSizes[] = {IntSize(16, 16),   IntSize(32, 32),
                             IntSize(64, 64),   IntSize(128, 128),
                             IntSize(256, 256), IntSize(256, 128)};

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
