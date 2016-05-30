/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "Common.h"
#include "Decoder.h"
#include "DecoderFactory.h"
#include "decoders/nsBMPDecoder.h"
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

TEST(ImageMetadata, ImageModuleAvailable)
{
  // We can run into problems if XPCOM modules get initialized in the wrong
  // order. It's important that this test run first, both as a sanity check and
  // to ensure we get the module initialization order we want.
  nsCOMPtr<imgITools> imgTools =
    do_CreateInstance("@mozilla.org/image/tools;1");
  EXPECT_TRUE(imgTools != nullptr);
}

enum class BMPWithinICO
{
  NO,
  YES
};

static void
CheckMetadata(const ImageTestCase& aTestCase,
              BMPWithinICO aBMPWithinICO = BMPWithinICO::NO)
{
  nsCOMPtr<nsIInputStream> inputStream = LoadFile(aTestCase.mPath);
  ASSERT_TRUE(inputStream != nullptr);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Write the data into a SourceBuffer.
  RefPtr<SourceBuffer> sourceBuffer = new SourceBuffer();
  sourceBuffer->ExpectLength(length);
  rv = sourceBuffer->AppendFromInputStream(inputStream, length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  sourceBuffer->Complete(NS_OK);

  // Create a metadata decoder.
  DecoderType decoderType =
    DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  RefPtr<Decoder> decoder =
    DecoderFactory::CreateAnonymousMetadataDecoder(decoderType, sourceBuffer);
  ASSERT_TRUE(decoder != nullptr);

  if (aBMPWithinICO == BMPWithinICO::YES) {
    static_cast<nsBMPDecoder*>(decoder.get())->SetIsWithinICO();
  }

  // Run the metadata decoder synchronously.
  decoder->Decode();

  // Ensure that the metadata decoder didn't make progress it shouldn't have
  // (which would indicate that it decoded past the header of the image).
  Progress metadataProgress = decoder->TakeProgress();
  EXPECT_TRUE(0 == (metadataProgress & ~(FLAG_SIZE_AVAILABLE |
                                         FLAG_HAS_TRANSPARENCY |
                                         FLAG_IS_ANIMATED)));

  // If the test case is corrupt, assert what we can and return early.
  if (aTestCase.mFlags & TEST_CASE_HAS_ERROR) {
    EXPECT_TRUE(decoder->GetDecodeDone());
    EXPECT_TRUE(decoder->HasError());
    return;
  }

  EXPECT_TRUE(decoder->GetDecodeDone() && !decoder->HasError());

  // Check that we got the expected metadata.
  EXPECT_TRUE(metadataProgress & FLAG_SIZE_AVAILABLE);

  IntSize metadataSize = decoder->GetSize();
  EXPECT_EQ(aTestCase.mSize.width, metadataSize.width);
  EXPECT_EQ(aTestCase.mSize.height, metadataSize.height);

  bool expectTransparency = aBMPWithinICO == BMPWithinICO::YES
                          ? true
                          : bool(aTestCase.mFlags & TEST_CASE_IS_TRANSPARENT);
  EXPECT_EQ(expectTransparency, bool(metadataProgress & FLAG_HAS_TRANSPARENCY));

  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_IS_ANIMATED),
            bool(metadataProgress & FLAG_IS_ANIMATED));

  // Create a full decoder, so we can compare the result.
  decoder =
    DecoderFactory::CreateAnonymousDecoder(decoderType, sourceBuffer, Nothing(),
                                           DefaultSurfaceFlags());
  ASSERT_TRUE(decoder != nullptr);

  if (aBMPWithinICO == BMPWithinICO::YES) {
    static_cast<nsBMPDecoder*>(decoder.get())->SetIsWithinICO();
  }

  // Run the full decoder synchronously.
  decoder->Decode();

  EXPECT_TRUE(decoder->GetDecodeDone() && !decoder->HasError());
  Progress fullProgress = decoder->TakeProgress();

  // If the metadata decoder set a progress bit, the full decoder should also
  // have set the same bit.
  EXPECT_EQ(fullProgress, metadataProgress | fullProgress);

  // The full decoder and the metadata decoder should agree on the image's size.
  IntSize fullSize = decoder->GetSize();
  EXPECT_EQ(metadataSize.width, fullSize.width);
  EXPECT_EQ(metadataSize.height, fullSize.height);

  // We should not discover transparency during the full decode that we didn't
  // discover during the metadata decode, unless the image is animated.
  EXPECT_TRUE(!(fullProgress & FLAG_HAS_TRANSPARENCY) ||
              (metadataProgress & FLAG_HAS_TRANSPARENCY) ||
              (fullProgress & FLAG_IS_ANIMATED));
}

TEST(ImageMetadata, PNG) { CheckMetadata(GreenPNGTestCase()); }
TEST(ImageMetadata, TransparentPNG) { CheckMetadata(TransparentPNGTestCase()); }
TEST(ImageMetadata, GIF) { CheckMetadata(GreenGIFTestCase()); }
TEST(ImageMetadata, TransparentGIF) { CheckMetadata(TransparentGIFTestCase()); }
TEST(ImageMetadata, JPG) { CheckMetadata(GreenJPGTestCase()); }
TEST(ImageMetadata, BMP) { CheckMetadata(GreenBMPTestCase()); }
TEST(ImageMetadata, ICO) { CheckMetadata(GreenICOTestCase()); }
TEST(ImageMetadata, Icon) { CheckMetadata(GreenIconTestCase()); }

TEST(ImageMetadata, AnimatedGIF)
{
  CheckMetadata(GreenFirstFrameAnimatedGIFTestCase());
}

TEST(ImageMetadata, AnimatedPNG)
{
  CheckMetadata(GreenFirstFrameAnimatedPNGTestCase());
}

TEST(ImageMetadata, FirstFramePaddingGIF)
{
  CheckMetadata(FirstFramePaddingGIFTestCase());
}

TEST(ImageMetadata, TransparentIfWithinICOBMPNotWithinICO)
{
  CheckMetadata(TransparentIfWithinICOBMPTestCase(TEST_CASE_DEFAULT_FLAGS),
                BMPWithinICO::NO);
}

TEST(ImageMetadata, TransparentIfWithinICOBMPWithinICO)
{
  CheckMetadata(TransparentIfWithinICOBMPTestCase(TEST_CASE_IS_TRANSPARENT),
                BMPWithinICO::YES);
}

TEST(ImageMetadata, RLE4BMP) { CheckMetadata(RLE4BMPTestCase()); }
TEST(ImageMetadata, RLE8BMP) { CheckMetadata(RLE8BMPTestCase()); }

TEST(ImageMetadata, Corrupt) { CheckMetadata(CorruptTestCase()); }

TEST(ImageMetadata, NoFrameDelayGIF)
{
  CheckMetadata(NoFrameDelayGIFTestCase());
}

TEST(ImageMetadata, NoFrameDelayGIFFullDecode)
{
  ImageTestCase testCase = NoFrameDelayGIFTestCase();

  // The previous test (NoFrameDelayGIF) verifies that we *don't* detect that
  // this test case is animated, because it has a zero frame delay for the first
  // frame. This test verifies that when we do a full decode, we detect the
  // animation at that point and successfully decode all the frames.

  // Create an image.
  RefPtr<Image> image =
    ImageFactory::CreateAnonymousImage(nsDependentCString(testCase.mMimeType));
  ASSERT_TRUE(!image->HasError());

  nsCOMPtr<nsIInputStream> inputStream = LoadFile(testCase.mPath);
  ASSERT_TRUE(inputStream != nullptr);

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
  LookupResult firstFrameLookupResult =
    SurfaceCache::Lookup(ImageKey(image.get()),
                         RasterSurfaceKey(imageSize,
                                          DefaultSurfaceFlags(),
                                          /* aFrameNum = */ 0));
  EXPECT_EQ(MatchType::EXACT, firstFrameLookupResult.Type());
                                                             
  LookupResult secondFrameLookupResult =
    SurfaceCache::Lookup(ImageKey(image.get()),
                         RasterSurfaceKey(imageSize,
                                          DefaultSurfaceFlags(),
                                          /* aFrameNum = */ 1));
  EXPECT_EQ(MatchType::EXACT, secondFrameLookupResult.Type());
}
