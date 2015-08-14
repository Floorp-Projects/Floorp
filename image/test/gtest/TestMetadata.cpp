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
#include "mozilla/gfx/2D.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "mozilla/nsRefPtr.h"
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

static void
CheckMetadata(const ImageTestCase& aTestCase, bool aEnableBMPAlpha = false)
{
  nsCOMPtr<nsIInputStream> inputStream = LoadFile(aTestCase.mPath);
  ASSERT_TRUE(inputStream != nullptr);

  // Figure out how much data we have.
  uint64_t length;
  nsresult rv = inputStream->Available(&length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Write the data into a SourceBuffer.
  nsRefPtr<SourceBuffer> sourceBuffer = new SourceBuffer();
  sourceBuffer->ExpectLength(length);
  rv = sourceBuffer->AppendFromInputStream(inputStream, length);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  sourceBuffer->Complete(NS_OK);

  // Create a metadata decoder.
  DecoderType decoderType =
    DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  nsRefPtr<Decoder> decoder =
    DecoderFactory::CreateAnonymousMetadataDecoder(decoderType, sourceBuffer);
  ASSERT_TRUE(decoder != nullptr);

  if (aEnableBMPAlpha) {
    static_cast<nsBMPDecoder*>(decoder.get())->SetUseAlphaData(true);
  }

  // Run the metadata decoder synchronously.
  decoder->Decode();
  
  // Ensure that the metadata decoder didn't make progress it shouldn't have
  // (which would indicate that it decoded past the header of the image).
  Progress metadataProgress = decoder->TakeProgress();
  EXPECT_TRUE(0 == (metadataProgress & ~(FLAG_SIZE_AVAILABLE |
                                         FLAG_HAS_TRANSPARENCY)));

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

  bool expectTransparency = aEnableBMPAlpha
                          ? true
                          : bool(aTestCase.mFlags & TEST_CASE_IS_TRANSPARENT);
  EXPECT_EQ(expectTransparency, bool(metadataProgress & FLAG_HAS_TRANSPARENCY));

  // Create a full decoder, so we can compare the result.
  decoder =
    DecoderFactory::CreateAnonymousDecoder(decoderType, sourceBuffer,
                                           imgIContainer::DECODE_FLAGS_DEFAULT);
  ASSERT_TRUE(decoder != nullptr);

  if (aEnableBMPAlpha) {
    static_cast<nsBMPDecoder*>(decoder.get())->SetUseAlphaData(true);
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

TEST(ImageMetadata, TransparentBMPWithBMPAlphaOff)
{
  CheckMetadata(TransparentBMPWhenBMPAlphaEnabledTestCase(),
                /* aEnableBMPAlpha = */ false);
}

TEST(ImageMetadata, TransparentBMPWithBMPAlphaOn)
{
  CheckMetadata(TransparentBMPWhenBMPAlphaEnabledTestCase(),
                /* aEnableBMPAlpha = */ true);
}

TEST(ImageMetadata, RLE4BMP) { CheckMetadata(RLE4BMPTestCase()); }
TEST(ImageMetadata, RLE8BMP) { CheckMetadata(RLE8BMPTestCase()); }

TEST(ImageMetadata, Corrupt) { CheckMetadata(CorruptTestCase()); }
