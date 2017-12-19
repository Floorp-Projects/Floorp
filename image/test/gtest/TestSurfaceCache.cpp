/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "Common.h"
#include "imgIContainer.h"
#include "imgITools.h"
#include "ImageFactory.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/RefPtr.h"
#include "nsIInputStream.h"
#include "nsString.h"
#include "ProgressTracker.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

class ImageSurfaceCache : public ::testing::Test
{
protected:
  AutoInitializeImageLib mInit;
};

TEST_F(ImageSurfaceCache, Factor2)
{
  ImageTestCase testCase = GreenPNGTestCase();

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

  // Ensures we meet the threshold for FLAG_SYNC_DECODE_IF_FAST to do sync
  // decoding without the implications of FLAG_SYNC_DECODE.
  ASSERT_LT(length, static_cast<uint64_t>(gfxPrefs::ImageMemDecodeBytesAtATime()));

  // Write the data into the image.
  rv = image->OnImageDataAvailable(nullptr, nullptr, inputStream, 0,
                                       static_cast<uint32_t>(length));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Let the image know we've sent all the data.
  rv = image->OnImageDataComplete(nullptr, nullptr, NS_OK, true);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  RefPtr<ProgressTracker> tracker = image->GetProgressTracker();
  tracker->SyncNotifyProgress(FLAG_LOAD_COMPLETE);

  const uint32_t whichFrame = imgIContainer::FRAME_CURRENT;

  // FLAG_SYNC_DECODE will make RasterImage::LookupFrame use
  // SurfaceCache::Lookup to force an exact match lookup (and potential decode).
  const uint32_t exactFlags = imgIContainer::FLAG_HIGH_QUALITY_SCALING |
                              imgIContainer::FLAG_SYNC_DECODE;

  // If the data stream is small enough, as we assert above,
  // FLAG_SYNC_DECODE_IF_FAST will allow us to decode sync, but avoid forcing
  // SurfaceCache::Lookup. Instead it will use SurfaceCache::LookupBestMatch.
  const uint32_t bestMatchFlags = imgIContainer::FLAG_HIGH_QUALITY_SCALING |
                                  imgIContainer::FLAG_SYNC_DECODE_IF_FAST;

  // We need the default threshold to be enabled (otherwise we should disable
  // this test).
  int32_t threshold = gfxPrefs::ImageCacheFactor2ThresholdSurfaces();
  ASSERT_TRUE(threshold >= 0);

  // We need to know what the native sizes are, otherwise factor of 2 mode will
  // be disabled.
  size_t nativeSizes = image->GetNativeSizesLength();
  ASSERT_EQ(nativeSizes, 1u);

  // Threshold is the native size count and the pref threshold added together.
  // Make sure the image is big enough that we can simply decrement and divide
  // off the size as we please and not hit unexpected duplicates.
  int32_t totalThreshold = static_cast<int32_t>(nativeSizes) + threshold;
  ASSERT_TRUE(testCase.mSize.width > totalThreshold * 4);

  // Request a bunch of slightly different sizes. We won't trip factor of 2 mode
  // in this loop.
  IntSize size = testCase.mSize;
  for (int32_t i = 0; i <= totalThreshold; ++i) {
    RefPtr<SourceSurface> surf =
      image->GetFrameAtSize(size, whichFrame, bestMatchFlags);
    ASSERT_TRUE(surf);
    EXPECT_EQ(surf->GetSize(), size);

    size.width -= 1;
    size.height -= 1;
  }

  // Now let's ask for a new size. Despite this being sync, it will return
  // the closest factor of 2 size we have and not the requested size.
  RefPtr<SourceSurface> surf =
    image->GetFrameAtSize(size, whichFrame, bestMatchFlags);
  ASSERT_TRUE(surf);

  EXPECT_EQ(surf->GetSize(), testCase.mSize);

  // Now we should be in factor of 2 mode but unless we trigger a decode no
  // pruning of the old sized surfaces should happen.
  size = testCase.mSize;
  for (int32_t i = 0; i < totalThreshold; ++i) {
    RefPtr<SourceSurface> surf =
      image->GetFrameAtSize(size, whichFrame, bestMatchFlags);
    ASSERT_TRUE(surf);
    EXPECT_EQ(surf->GetSize(), size);

    size.width -= 1;
    size.height -= 1;
  }

  // Now force an existing surface to be marked as explicit so that it
  // won't get freed upon pruning (gets marked in the Lookup).
  size.width += 1;
  size.height += 1;
  surf = image->GetFrameAtSize(size, whichFrame, exactFlags);
  ASSERT_TRUE(surf);
  EXPECT_EQ(surf->GetSize(), size);

  // Now force a new decode to happen by getting a new factor of 2 size.
  size.width = testCase.mSize.width / 2 - 1;
  size.height = testCase.mSize.height / 2 - 1;
  surf = image->GetFrameAtSize(size, whichFrame, bestMatchFlags);
  ASSERT_TRUE(surf);
  EXPECT_EQ(surf->GetSize().width, testCase.mSize.width / 2);
  EXPECT_EQ(surf->GetSize().height, testCase.mSize.height / 2);

  // The decode above would have forced a pruning to happen, so now if
  // we request all of the sizes we used to have decoded, only the explicit
  // size should have been kept.
  size = testCase.mSize;
  for (int32_t i = 0; i < totalThreshold - 1; ++i) {
    RefPtr<SourceSurface> surf =
      image->GetFrameAtSize(size, whichFrame, bestMatchFlags);
    ASSERT_TRUE(surf);
    EXPECT_EQ(surf->GetSize(), testCase.mSize);

    size.width -= 1;
    size.height -= 1;
  }

  // This lookup finds the surface that already existed that we later marked
  // as explicit. It should still exist after pruning.
  surf = image->GetFrameAtSize(size, whichFrame, bestMatchFlags);
  ASSERT_TRUE(surf);
  EXPECT_EQ(surf->GetSize(), size);
}
