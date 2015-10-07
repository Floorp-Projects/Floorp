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
#include "mozilla/nsRefPtr.h"
#include "nsStreamUtils.h"
#include "nsString.h"
#include "nsThreadUtils.h"
#include "ProgressTracker.h"
#include "SourceBuffer.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

TEST(ImageDecoders, ImageModuleAvailable)
{
  // We can run into problems if XPCOM modules get initialized in the wrong
  // order. It's important that this test run first, both as a sanity check and
  // to ensure we get the module initialization order we want.
  nsCOMPtr<imgITools> imgTools =
    do_CreateInstance("@mozilla.org/image/tools;1");
  EXPECT_TRUE(imgTools != nullptr);
}

static void
CheckDecoderResults(const ImageTestCase& aTestCase, Decoder* aDecoder)
{
  EXPECT_TRUE(aDecoder->GetDecodeDone());
  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_HAS_ERROR),
            aDecoder->HasError());
  EXPECT_TRUE(!aDecoder->WasAborted());

  // Verify that the decoder made the expected progress.
  Progress progress = aDecoder->TakeProgress();
  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_HAS_ERROR),
            bool(progress & FLAG_HAS_ERROR));

  if (aTestCase.mFlags & TEST_CASE_HAS_ERROR) {
    return;  // That's all we can check for bad images.
  }

  EXPECT_TRUE(bool(progress & FLAG_SIZE_AVAILABLE));
  EXPECT_TRUE(bool(progress & FLAG_DECODE_COMPLETE));
  EXPECT_TRUE(bool(progress & FLAG_FRAME_COMPLETE));
  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_IS_TRANSPARENT),
            bool(progress & FLAG_HAS_TRANSPARENCY));
  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_IS_ANIMATED),
            bool(progress & FLAG_IS_ANIMATED));

  // The decoder should get the correct size.
  IntSize size = aDecoder->GetSize();
  EXPECT_EQ(aTestCase.mSize.width, size.width);
  EXPECT_EQ(aTestCase.mSize.height, size.height);

  // Get the current frame, which is always the first frame of the image
  // because CreateAnonymousDecoder() forces a first-frame-only decode.
  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  nsRefPtr<SourceSurface> surface = currentFrame->GetSurface();

  // Verify that the resulting surfaces matches our expectations.
  EXPECT_EQ(SurfaceType::DATA, surface->GetType());
  EXPECT_TRUE(surface->GetFormat() == SurfaceFormat::B8G8R8X8 ||
              surface->GetFormat() == SurfaceFormat::B8G8R8A8);
  EXPECT_EQ(aTestCase.mSize, surface->GetSize());
  EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Green(),
                           aTestCase.mFlags & TEST_CASE_IS_FUZZY));
}

static void
CheckDecoderSingleChunk(const ImageTestCase& aTestCase)
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

  // Create a decoder.
  DecoderType decoderType =
    DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  nsRefPtr<Decoder> decoder =
    DecoderFactory::CreateAnonymousDecoder(decoderType, sourceBuffer,
                                           DefaultSurfaceFlags());
  ASSERT_TRUE(decoder != nullptr);

  // Run the full decoder synchronously.
  decoder->Decode();
  
  CheckDecoderResults(aTestCase, decoder);
}

class NoResume : public IResumable
{
public:
  NS_INLINE_DECL_REFCOUNTING(NoResume, override)
  virtual void Resume() override { }

private:
  ~NoResume() { }
};

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
  nsRefPtr<SourceBuffer> sourceBuffer = new SourceBuffer();
  sourceBuffer->ExpectLength(length);
  DecoderType decoderType =
    DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  nsRefPtr<Decoder> decoder =
    DecoderFactory::CreateAnonymousDecoder(decoderType, sourceBuffer,
                                           DefaultSurfaceFlags());
  ASSERT_TRUE(decoder != nullptr);

  // Decode synchronously, using a |NoResume| IResumable so the Decoder doesn't
  // attempt to schedule itself on a nonexistent DecodePool when we write more
  // data into the SourceBuffer.
  nsRefPtr<NoResume> noResume = new NoResume();
  for (uint64_t read = 0; read < length ; ++read) {
    uint64_t available = 0;
    rv = inputStream->Available(&available);
    ASSERT_TRUE(available > 0);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    rv = sourceBuffer->AppendFromInputStream(inputStream, 1);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    decoder->Decode(noResume);
  }

  sourceBuffer->Complete(NS_OK);
  decoder->Decode(noResume);
  
  CheckDecoderResults(aTestCase, decoder);
}

TEST(ImageDecoders, PNGSingleChunk)
{
  CheckDecoderSingleChunk(GreenPNGTestCase());
}

TEST(ImageDecoders, PNGMultiChunk)
{
  CheckDecoderMultiChunk(GreenPNGTestCase());
}

TEST(ImageDecoders, GIFSingleChunk)
{
  CheckDecoderSingleChunk(GreenGIFTestCase());
}

TEST(ImageDecoders, GIFMultiChunk)
{
  CheckDecoderMultiChunk(GreenGIFTestCase());
}

TEST(ImageDecoders, JPGSingleChunk)
{
  CheckDecoderSingleChunk(GreenJPGTestCase());
}

TEST(ImageDecoders, JPGMultiChunk)
{
  CheckDecoderMultiChunk(GreenJPGTestCase());
}

TEST(ImageDecoders, BMPSingleChunk)
{
  CheckDecoderSingleChunk(GreenBMPTestCase());
}

TEST(ImageDecoders, BMPMultiChunk)
{
  CheckDecoderMultiChunk(GreenBMPTestCase());
}

TEST(ImageDecoders, ICOSingleChunk)
{
  CheckDecoderSingleChunk(GreenICOTestCase());
}

TEST(ImageDecoders, ICOMultiChunk)
{
  CheckDecoderMultiChunk(GreenICOTestCase());
}

TEST(ImageDecoders, IconSingleChunk)
{
  CheckDecoderSingleChunk(GreenIconTestCase());
}

TEST(ImageDecoders, IconMultiChunk)
{
  CheckDecoderMultiChunk(GreenIconTestCase());
}

TEST(ImageDecoders, AnimatedGIFSingleChunk)
{
  CheckDecoderSingleChunk(GreenFirstFrameAnimatedGIFTestCase());
}

TEST(ImageDecoders, AnimatedGIFMultiChunk)
{
  CheckDecoderMultiChunk(GreenFirstFrameAnimatedGIFTestCase());
}

TEST(ImageDecoders, AnimatedPNGSingleChunk)
{
  CheckDecoderSingleChunk(GreenFirstFrameAnimatedPNGTestCase());
}

TEST(ImageDecoders, AnimatedPNGMultiChunk)
{
  CheckDecoderMultiChunk(GreenFirstFrameAnimatedPNGTestCase());
}

TEST(ImageDecoders, CorruptSingleChunk)
{
  CheckDecoderSingleChunk(CorruptTestCase());
}

TEST(ImageDecoders, CorruptMultiChunk)
{
  CheckDecoderMultiChunk(CorruptTestCase());
}
