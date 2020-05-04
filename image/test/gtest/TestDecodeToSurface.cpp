/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "Common.h"
#include "imgIContainer.h"
#include "ImageOps.h"
#include "mozilla/gfx/2D.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsIInputStream.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "mozilla/RefPtr.h"
#include "nsString.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

class DecodeToSurfaceRunnable : public Runnable {
 public:
  DecodeToSurfaceRunnable(RefPtr<SourceSurface>& aSurface,
                          nsIInputStream* aInputStream,
                          ImageOps::ImageBuffer* aImageBuffer,
                          const ImageTestCase& aTestCase)
      : mozilla::Runnable("DecodeToSurfaceRunnable"),
        mSurface(aSurface),
        mInputStream(aInputStream),
        mImageBuffer(aImageBuffer),
        mTestCase(aTestCase) {}

  NS_IMETHOD Run() override {
    Go();
    return NS_OK;
  }

  void Go() {
    Maybe<IntSize> outputSize;
    if (mTestCase.mOutputSize != mTestCase.mSize) {
      outputSize.emplace(mTestCase.mOutputSize);
    }

    uint32_t flags = FromSurfaceFlags(mTestCase.mSurfaceFlags);

    if (mImageBuffer) {
      mSurface = ImageOps::DecodeToSurface(
          mImageBuffer, nsDependentCString(mTestCase.mMimeType), flags,
          outputSize);
    } else {
      mSurface = ImageOps::DecodeToSurface(
          mInputStream.forget(), nsDependentCString(mTestCase.mMimeType), flags,
          outputSize);
    }
    ASSERT_TRUE(mSurface != nullptr);

    EXPECT_TRUE(mSurface->IsDataSourceSurface());
    EXPECT_TRUE(mSurface->GetFormat() == SurfaceFormat::OS_RGBX ||
                mSurface->GetFormat() == SurfaceFormat::OS_RGBA);

    if (outputSize) {
      EXPECT_EQ(*outputSize, mSurface->GetSize());
    } else {
      EXPECT_EQ(mTestCase.mSize, mSurface->GetSize());
    }

    EXPECT_TRUE(IsSolidColor(mSurface, mTestCase.Color(), mTestCase.Fuzz()));
  }

 private:
  RefPtr<SourceSurface>& mSurface;
  nsCOMPtr<nsIInputStream> mInputStream;
  RefPtr<ImageOps::ImageBuffer> mImageBuffer;
  ImageTestCase mTestCase;
};

static void RunDecodeToSurface(const ImageTestCase& aTestCase,
                               ImageOps::ImageBuffer* aImageBuffer = nullptr) {
  nsCOMPtr<nsIInputStream> inputStream;
  if (!aImageBuffer) {
    inputStream = LoadFile(aTestCase.mPath);
    ASSERT_TRUE(inputStream != nullptr);
  }

  nsCOMPtr<nsIThread> thread;
  nsresult rv =
      NS_NewNamedThread("DecodeToSurface", getter_AddRefs(thread), nullptr);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // We run the DecodeToSurface tests off-main-thread to ensure that
  // DecodeToSurface doesn't require any main-thread-only code.
  RefPtr<SourceSurface> surface;
  nsCOMPtr<nsIRunnable> runnable = new DecodeToSurfaceRunnable(
      surface, inputStream, aImageBuffer, aTestCase);
  thread->Dispatch(runnable, nsIThread::DISPATCH_SYNC);

  thread->Shutdown();

  // Explicitly release the SourceSurface on the main thread.
  surface = nullptr;
}

class ImageDecodeToSurface : public ::testing::Test {
 protected:
  AutoInitializeImageLib mInit;
};

TEST_F(ImageDecodeToSurface, PNG) { RunDecodeToSurface(GreenPNGTestCase()); }
TEST_F(ImageDecodeToSurface, GIF) { RunDecodeToSurface(GreenGIFTestCase()); }
TEST_F(ImageDecodeToSurface, JPG) { RunDecodeToSurface(GreenJPGTestCase()); }
TEST_F(ImageDecodeToSurface, BMP) { RunDecodeToSurface(GreenBMPTestCase()); }
TEST_F(ImageDecodeToSurface, ICO) { RunDecodeToSurface(GreenICOTestCase()); }
TEST_F(ImageDecodeToSurface, Icon) { RunDecodeToSurface(GreenIconTestCase()); }
TEST_F(ImageDecodeToSurface, WebP) { RunDecodeToSurface(GreenWebPTestCase()); }

TEST_F(ImageDecodeToSurface, AnimatedGIF) {
  RunDecodeToSurface(GreenFirstFrameAnimatedGIFTestCase());
}

TEST_F(ImageDecodeToSurface, AnimatedPNG) {
  RunDecodeToSurface(GreenFirstFrameAnimatedPNGTestCase());
}

TEST_F(ImageDecodeToSurface, Corrupt) {
  ImageTestCase testCase = CorruptTestCase();

  nsCOMPtr<nsIInputStream> inputStream = LoadFile(testCase.mPath);
  ASSERT_TRUE(inputStream != nullptr);

  RefPtr<SourceSurface> surface = ImageOps::DecodeToSurface(
      inputStream.forget(), nsDependentCString(testCase.mMimeType),
      imgIContainer::DECODE_FLAGS_DEFAULT);
  EXPECT_TRUE(surface == nullptr);
}

TEST_F(ImageDecodeToSurface, ICOMultipleSizes) {
  ImageTestCase testCase = GreenMultipleSizesICOTestCase();

  nsCOMPtr<nsIInputStream> inputStream = LoadFile(testCase.mPath);
  ASSERT_TRUE(inputStream != nullptr);

  RefPtr<ImageOps::ImageBuffer> buffer =
      ImageOps::CreateImageBuffer(inputStream.forget());
  ASSERT_TRUE(buffer != nullptr);

  ImageMetadata metadata;
  nsresult rv = ImageOps::DecodeMetadata(
      buffer, nsDependentCString(testCase.mMimeType), metadata);
  EXPECT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(metadata.HasSize());
  EXPECT_EQ(testCase.mSize, metadata.GetSize());

  const nsTArray<IntSize>& nativeSizes = metadata.GetNativeSizes();
  ASSERT_EQ(6u, nativeSizes.Length());

  IntSize expectedSizes[] = {
      IntSize(16, 16),   IntSize(32, 32),   IntSize(64, 64),
      IntSize(128, 128), IntSize(256, 256), IntSize(256, 128),
  };

  for (int i = 0; i < 6; ++i) {
    EXPECT_EQ(expectedSizes[i], nativeSizes[i]);

    // Request decoding at native size
    testCase.mOutputSize = nativeSizes[i];
    RunDecodeToSurface(testCase, buffer);
  }
}
