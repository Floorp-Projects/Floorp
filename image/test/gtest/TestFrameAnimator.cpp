/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "Common.h"
#include "AnimationSurfaceProvider.h"
#include "Decoder.h"
#include "ImageFactory.h"
#include "nsIInputStream.h"
#include "RasterImage.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

static void CheckFrameAnimatorBlendResults(const ImageTestCase& aTestCase,
                                           RasterImage* aImage, uint8_t aFuzz) {
  // Allow the animation to actually begin.
  aImage->IncrementAnimationConsumers();

  // Initialize for the first frame so we can advance.
  TimeStamp now = TimeStamp::Now();
  aImage->RequestRefresh(now);
  EXPECT_EQ(aImage->GetFrameIndex(imgIContainer::FRAME_CURRENT), 0);

  RefPtr<SourceSurface> surface =
      aImage->GetFrame(imgIContainer::FRAME_CURRENT, imgIContainer::FLAG_NONE);
  ASSERT_TRUE(surface != nullptr);

  CheckGeneratedSurface(surface, IntRect(0, 0, 50, 50),
                        BGRAColor::Transparent(),
                        aTestCase.ChooseColor(BGRAColor::Red()), aFuzz);

  // Advance to the next/final frame.
  now = TimeStamp::Now() + TimeDuration::FromMilliseconds(500);
  aImage->RequestRefresh(now);
  EXPECT_EQ(aImage->GetFrameIndex(imgIContainer::FRAME_CURRENT), 1);

  surface =
      aImage->GetFrame(imgIContainer::FRAME_CURRENT, imgIContainer::FLAG_NONE);
  ASSERT_TRUE(surface != nullptr);
  CheckGeneratedSurface(surface, IntRect(0, 0, 50, 50),
                        aTestCase.ChooseColor(BGRAColor::Green()),
                        aTestCase.ChooseColor(BGRAColor::Red()), aFuzz);
}

template <typename Func>
static void WithFrameAnimatorDecode(const ImageTestCase& aTestCase,
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
  ASSERT_NS_SUCCEEDED(rv);

  // Write the data into a SourceBuffer.
  NotNull<RefPtr<SourceBuffer>> sourceBuffer = WrapNotNull(new SourceBuffer());
  sourceBuffer->ExpectLength(length);
  rv = sourceBuffer->AppendFromInputStream(inputStream, length);
  ASSERT_NS_SUCCEEDED(rv);
  sourceBuffer->Complete(NS_OK);

  // Create a metadata decoder first, because otherwise RasterImage will get
  // unhappy about finding out the image is animated during a full decode.
  DecoderType decoderType = DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  DecoderFlags decoderFlags =
      DecoderFactory::GetDefaultDecoderFlagsForType(decoderType);
  RefPtr<IDecodingTask> task = DecoderFactory::CreateMetadataDecoder(
      decoderType, rasterImage, decoderFlags, sourceBuffer);
  ASSERT_TRUE(task != nullptr);

  // Run the metadata decoder synchronously.
  task->Run();
  task = nullptr;

  // Create an AnimationSurfaceProvider which will manage the decoding process
  // and make this decoder's output available in the surface cache.
  SurfaceFlags surfaceFlags = aTestCase.mSurfaceFlags;
  rv = DecoderFactory::CreateAnimationDecoder(
      decoderType, rasterImage, sourceBuffer, aTestCase.mSize, decoderFlags,
      surfaceFlags, 0, getter_AddRefs(task));
  EXPECT_EQ(rv, NS_OK);
  ASSERT_TRUE(task != nullptr);

  // Run the full decoder synchronously.
  task->Run();

  // Call the lambda to verify the expected results.
  aResultChecker(rasterImage.get());
}

static void CheckFrameAnimatorBlend(const ImageTestCase& aTestCase,
                                    uint8_t aFuzz = 0) {
  WithFrameAnimatorDecode(aTestCase, [&](RasterImage* aImage) {
    CheckFrameAnimatorBlendResults(aTestCase, aImage, aFuzz);
  });
}

class ImageFrameAnimator : public ::testing::Test {
 protected:
  AutoInitializeImageLib mInit;
};

TEST_F(ImageFrameAnimator, BlendGIFWithFilter) {
  CheckFrameAnimatorBlend(BlendAnimatedGIFTestCase());
}

TEST_F(ImageFrameAnimator, BlendPNGWithFilter) {
  CheckFrameAnimatorBlend(BlendAnimatedPNGTestCase());
}

TEST_F(ImageFrameAnimator, BlendWebPWithFilter) {
  CheckFrameAnimatorBlend(BlendAnimatedWebPTestCase());
}

TEST_F(ImageFrameAnimator, BlendAVIFWithFilter) {
  CheckFrameAnimatorBlend(BlendAnimatedAVIFTestCase(), 1);
}
