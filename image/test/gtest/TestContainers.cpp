/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "WindowRenderer.h"
#include "Common.h"
#include "imgIContainer.h"
#include "ImageFactory.h"
#include "ImageContainer.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsIInputStream.h"
#include "nsString.h"
#include "ProgressTracker.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

class ImageContainers : public ::testing::Test {
 protected:
  AutoInitializeImageLib mInit;
};

TEST_F(ImageContainers, RasterImageContainer) {
  ImageTestCase testCase = GreenPNGTestCase();

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

  RefPtr<WindowRenderer> renderer = new FallbackRenderer;

  // Get at native size.
  ImgDrawResult drawResult;
  RefPtr<layers::ImageContainer> nativeContainer;
  drawResult = image->GetImageContainerAtSize(
      renderer, testCase.mSize, Nothing(), Nothing(),
      imgIContainer::FLAG_SYNC_DECODE, getter_AddRefs(nativeContainer));
  EXPECT_EQ(drawResult, ImgDrawResult::SUCCESS);
  ASSERT_TRUE(nativeContainer != nullptr);
  IntSize containerSize = nativeContainer->GetCurrentSize();
  EXPECT_EQ(testCase.mSize.width, containerSize.width);
  EXPECT_EQ(testCase.mSize.height, containerSize.height);

  // Upscaling should give the native size.
  IntSize requestedSize = testCase.mSize;
  requestedSize.Scale(2, 2);
  RefPtr<layers::ImageContainer> upscaleContainer;
  drawResult = image->GetImageContainerAtSize(
      renderer, requestedSize, Nothing(), Nothing(),
      imgIContainer::FLAG_SYNC_DECODE |
          imgIContainer::FLAG_HIGH_QUALITY_SCALING,
      getter_AddRefs(upscaleContainer));
  EXPECT_EQ(drawResult, ImgDrawResult::SUCCESS);
  ASSERT_TRUE(upscaleContainer != nullptr);
  containerSize = upscaleContainer->GetCurrentSize();
  EXPECT_EQ(testCase.mSize.width, containerSize.width);
  EXPECT_EQ(testCase.mSize.height, containerSize.height);

  // Downscaling should give the downscaled size.
  requestedSize = testCase.mSize;
  requestedSize.width /= 2;
  requestedSize.height /= 2;
  RefPtr<layers::ImageContainer> downscaleContainer;
  drawResult = image->GetImageContainerAtSize(
      renderer, requestedSize, Nothing(), Nothing(),
      imgIContainer::FLAG_SYNC_DECODE |
          imgIContainer::FLAG_HIGH_QUALITY_SCALING,
      getter_AddRefs(downscaleContainer));
  EXPECT_EQ(drawResult, ImgDrawResult::SUCCESS);
  ASSERT_TRUE(downscaleContainer != nullptr);
  containerSize = downscaleContainer->GetCurrentSize();
  EXPECT_EQ(requestedSize.width, containerSize.width);
  EXPECT_EQ(requestedSize.height, containerSize.height);

  // Get at native size again. Should give same container.
  RefPtr<layers::ImageContainer> againContainer;
  drawResult = image->GetImageContainerAtSize(
      renderer, testCase.mSize, Nothing(), Nothing(),
      imgIContainer::FLAG_SYNC_DECODE, getter_AddRefs(againContainer));
  EXPECT_EQ(drawResult, ImgDrawResult::SUCCESS);
  ASSERT_EQ(nativeContainer.get(), againContainer.get());
}
