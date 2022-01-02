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

enum class BMPWithinICO { NO, YES };

static void CheckMetadata(const ImageTestCase& aTestCase,
                          BMPWithinICO aBMPWithinICO = BMPWithinICO::NO) {
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

  // Create a metadata decoder.
  DecoderType decoderType = DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  RefPtr<image::Decoder> decoder =
      DecoderFactory::CreateAnonymousMetadataDecoder(decoderType, sourceBuffer);
  ASSERT_TRUE(decoder != nullptr);
  RefPtr<IDecodingTask> task =
      new AnonymousDecodingTask(WrapNotNull(decoder), /* aResumable */ false);

  if (aBMPWithinICO == BMPWithinICO::YES) {
    static_cast<nsBMPDecoder*>(decoder.get())->SetIsWithinICO();
  }

  // Run the metadata decoder synchronously.
  task->Run();

  // Ensure that the metadata decoder didn't make progress it shouldn't have
  // (which would indicate that it decoded past the header of the image).
  Progress metadataProgress = decoder->TakeProgress();
  EXPECT_TRUE(
      0 == (metadataProgress &
            ~(FLAG_SIZE_AVAILABLE | FLAG_HAS_TRANSPARENCY | FLAG_IS_ANIMATED)));

  // If the test case is corrupt, assert what we can and return early.
  if (aTestCase.mFlags & TEST_CASE_HAS_ERROR) {
    EXPECT_TRUE(decoder->GetDecodeDone());
    EXPECT_TRUE(decoder->HasError());
    return;
  }

  EXPECT_TRUE(decoder->GetDecodeDone() && !decoder->HasError());

  // Check that we got the expected metadata.
  EXPECT_TRUE(metadataProgress & FLAG_SIZE_AVAILABLE);

  OrientedIntSize metadataSize = decoder->Size();
  EXPECT_EQ(aTestCase.mSize.width, metadataSize.width);
  if (aBMPWithinICO == BMPWithinICO::YES) {
    // Half the data is considered to be part of the AND mask if embedded
    EXPECT_EQ(aTestCase.mSize.height / 2, metadataSize.height);
  } else {
    EXPECT_EQ(aTestCase.mSize.height, metadataSize.height);
  }

  bool expectTransparency =
      aBMPWithinICO == BMPWithinICO::YES
          ? true
          : bool(aTestCase.mFlags & TEST_CASE_IS_TRANSPARENT);
  EXPECT_EQ(expectTransparency, bool(metadataProgress & FLAG_HAS_TRANSPARENCY));

  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_IS_ANIMATED),
            bool(metadataProgress & FLAG_IS_ANIMATED));

  // Create a full decoder, so we can compare the result.
  decoder = DecoderFactory::CreateAnonymousDecoder(
      decoderType, sourceBuffer, Nothing(), DecoderFlags::FIRST_FRAME_ONLY,
      aTestCase.mSurfaceFlags);
  ASSERT_TRUE(decoder != nullptr);
  task =
      new AnonymousDecodingTask(WrapNotNull(decoder), /* aResumable */ false);

  if (aBMPWithinICO == BMPWithinICO::YES) {
    static_cast<nsBMPDecoder*>(decoder.get())->SetIsWithinICO();
  }

  // Run the full decoder synchronously.
  task->Run();

  EXPECT_TRUE(decoder->GetDecodeDone() && !decoder->HasError());
  Progress fullProgress = decoder->TakeProgress();

  // If the metadata decoder set a progress bit, the full decoder should also
  // have set the same bit.
  EXPECT_EQ(fullProgress, metadataProgress | fullProgress);

  // The full decoder and the metadata decoder should agree on the image's size.
  OrientedIntSize fullSize = decoder->Size();
  EXPECT_EQ(metadataSize.width, fullSize.width);
  EXPECT_EQ(metadataSize.height, fullSize.height);

  // We should not discover transparency during the full decode that we didn't
  // discover during the metadata decode, unless the image is animated.
  EXPECT_TRUE(!(fullProgress & FLAG_HAS_TRANSPARENCY) ||
              (metadataProgress & FLAG_HAS_TRANSPARENCY) ||
              (fullProgress & FLAG_IS_ANIMATED));
}

class ImageDecoderMetadata : public ::testing::Test {
 protected:
  AutoInitializeImageLib mInit;
};

TEST_F(ImageDecoderMetadata, TransparentAVIF) {
  CheckMetadata(TransparentAVIFTestCase());
}

TEST_F(ImageDecoderMetadata, PNG) { CheckMetadata(GreenPNGTestCase()); }
TEST_F(ImageDecoderMetadata, TransparentPNG) {
  CheckMetadata(TransparentPNGTestCase());
}
TEST_F(ImageDecoderMetadata, GIF) { CheckMetadata(GreenGIFTestCase()); }
TEST_F(ImageDecoderMetadata, TransparentGIF) {
  CheckMetadata(TransparentGIFTestCase());
}
TEST_F(ImageDecoderMetadata, JPG) { CheckMetadata(GreenJPGTestCase()); }
TEST_F(ImageDecoderMetadata, BMP) { CheckMetadata(GreenBMPTestCase()); }
TEST_F(ImageDecoderMetadata, ICO) { CheckMetadata(GreenICOTestCase()); }
TEST_F(ImageDecoderMetadata, Icon) { CheckMetadata(GreenIconTestCase()); }
TEST_F(ImageDecoderMetadata, WebP) { CheckMetadata(GreenWebPTestCase()); }

#ifdef MOZ_JXL
TEST_F(ImageDecoderMetadata, JXL) { CheckMetadata(GreenJXLTestCase()); }
TEST_F(ImageDecoderMetadata, TransparentJXL) {
  CheckMetadata(TransparentJXLTestCase());
}
#endif

TEST_F(ImageDecoderMetadata, AnimatedGIF) {
  CheckMetadata(GreenFirstFrameAnimatedGIFTestCase());
}

TEST_F(ImageDecoderMetadata, AnimatedPNG) {
  CheckMetadata(GreenFirstFrameAnimatedPNGTestCase());
}

TEST_F(ImageDecoderMetadata, FirstFramePaddingGIF) {
  CheckMetadata(FirstFramePaddingGIFTestCase());
}

TEST_F(ImageDecoderMetadata, TransparentIfWithinICOBMPNotWithinICO) {
  CheckMetadata(TransparentIfWithinICOBMPTestCase(TEST_CASE_DEFAULT_FLAGS),
                BMPWithinICO::NO);
}

TEST_F(ImageDecoderMetadata, TransparentIfWithinICOBMPWithinICO) {
  CheckMetadata(TransparentIfWithinICOBMPTestCase(TEST_CASE_IS_TRANSPARENT),
                BMPWithinICO::YES);
}

TEST_F(ImageDecoderMetadata, RLE4BMP) { CheckMetadata(RLE4BMPTestCase()); }
TEST_F(ImageDecoderMetadata, RLE8BMP) { CheckMetadata(RLE8BMPTestCase()); }

TEST_F(ImageDecoderMetadata, Corrupt) { CheckMetadata(CorruptTestCase()); }

TEST_F(ImageDecoderMetadata, NoFrameDelayGIF) {
  CheckMetadata(NoFrameDelayGIFTestCase());
}

TEST_F(ImageDecoderMetadata, NoFrameDelayGIFFullDecode) {
  ImageTestCase testCase = NoFrameDelayGIFTestCase();

  // The previous test (NoFrameDelayGIF) verifies that we *don't* detect that
  // this test case is animated, because it has a zero frame delay for the first
  // frame. This test verifies that when we do a full decode, we detect the
  // animation at that point and successfully decode all the frames.

  // Create an image.
  RefPtr<Image> image = ImageFactory::CreateAnonymousImage(
      nsDependentCString(testCase.mMimeType));
  ASSERT_TRUE(!image->HasError());

  nsCOMPtr<nsIInputStream> inputStream = LoadFile(testCase.mPath);
  ASSERT_TRUE(inputStream != nullptr);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Write the data into the image.
  rv = image->OnImageDataAvailable(nullptr, inputStream, 0,
                                   static_cast<uint32_t>(length));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Let the image know we've sent all the data.
  rv = image->OnImageDataComplete(nullptr, NS_OK, true);
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
