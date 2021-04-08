/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceHelpers.h"
#include "MacIOSurfaceImage.h"
#include "gfxPlatform.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/MacIOSurfaceTextureClientOGL.h"
#include "mozilla/layers/TextureForwarder.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/UniquePtr.h"
#include "YCbCrUtils.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;

TextureClient* MacIOSurfaceImage::GetTextureClient(
    KnowsCompositor* aKnowsCompositor) {
  if (!mTextureClient) {
    BackendType backend = BackendType::NONE;
    mTextureClient = TextureClient::CreateWithData(
        MacIOSurfaceTextureData::Create(mSurface, backend),
        TextureFlags::DEFAULT, aKnowsCompositor->GetTextureForwarder());
  }
  return mTextureClient;
}

already_AddRefed<SourceSurface> MacIOSurfaceImage::GetAsSourceSurface() {
  return CreateSourceSurfaceFromMacIOSurface(mSurface);
}

bool MacIOSurfaceImage::SetData(ImageContainer* aContainer,
                                const PlanarYCbCrData& aData) {
  MOZ_ASSERT(!mSurface);

  if (aData.mYSkip != 0 || aData.mCbSkip != 0 || aData.mCrSkip != 0 ||
      !(aData.mYUVColorSpace == YUVColorSpace::BT601 ||
        aData.mYUVColorSpace == YUVColorSpace::BT709) ||
      !(aData.mColorRange == ColorRange::FULL ||
        aData.mColorRange == ColorRange::LIMITED) ||
      aData.mColorDepth != ColorDepth::COLOR_8) {
    return false;
  }

  if (aData.mCbCrSize.width * 2 != aData.mYSize.width) {
    return false;
  }

  // We can only support 4:2:2 and 4:2:0 formats currently.
  if (aData.mCbCrSize.height != aData.mYSize.height &&
      aData.mCbCrSize.height * 2 != aData.mYSize.height) {
    return false;
  }

  RefPtr<MacIOSurfaceRecycleAllocator> allocator =
      aContainer->GetMacIOSurfaceRecycleAllocator();

  RefPtr<MacIOSurface> surf = allocator->Allocate(
      aData.mYSize, aData.mCbCrSize, aData.mYUVColorSpace, aData.mColorRange);

  surf->Lock(false);

  // If the CbCrSize's height is half of the YSize's height, then we'll
  // need to duplicate the CbCr data on every second row.
  size_t heightScale = aData.mYSize.height / aData.mCbCrSize.height;

  MOZ_ASSERT(surf->GetFormat() == SurfaceFormat::YUV422);

  // The underlying IOSurface has format kCVPixelFormatType_422YpCbCr8FullRange
  // or kCVPixelFormatType_422YpCbCr8_yuvs, which uses a 4:2:2 Y`0 Cb Y`1 Cr
  // layout. See CVPixelBuffer.h for the full list of format descriptions.
  MOZ_ASSERT(aData.mYSize.height > 0);
  uint8_t* dst = (uint8_t*)surf->GetBaseAddressOfPlane(0);
  size_t stride = surf->GetBytesPerRow(0);
  for (size_t i = 0; i < (size_t)aData.mYSize.height; i++) {
    // Compute the row addresses. If the input was 4:2:0, then
    // we divide i by 2, so that each source row of CbCr maps to
    // two dest rows.
    uint8_t* rowYSrc = aData.mYChannel + aData.mYStride * i;
    uint8_t* rowCbSrc =
        aData.mCbChannel + aData.mCbCrStride * (i / heightScale);
    uint8_t* rowCrSrc =
        aData.mCrChannel + aData.mCbCrStride * (i / heightScale);
    uint8_t* rowDst = dst + stride * i;

    // Iterate across the CbCr width (which we have guaranteed to be half of
    // the surface width), and write two 16bit pixels each time.
    for (size_t j = 0; j < (size_t)aData.mCbCrSize.width; j++) {
      *rowDst = *rowYSrc;
      rowDst++;
      rowYSrc++;

      *rowDst = *rowCbSrc;
      rowDst++;
      rowCbSrc++;

      *rowDst = *rowYSrc;
      rowDst++;
      rowYSrc++;

      *rowDst = *rowCrSrc;
      rowDst++;
      rowCrSrc++;
    }
  }

  surf->Unlock(false);
  mSurface = surf;
  mPictureRect = aData.GetPictureRect();
  return true;
}

already_AddRefed<MacIOSurface> MacIOSurfaceRecycleAllocator::Allocate(
    const gfx::IntSize aYSize, const gfx::IntSize& aCbCrSize,
    gfx::YUVColorSpace aYUVColorSpace, gfx::ColorRange aColorRange) {
  nsTArray<CFTypeRefPtr<IOSurfaceRef>> surfaces = std::move(mSurfaces);
  RefPtr<MacIOSurface> result;
  for (auto& surf : surfaces) {
    // If the surface size has changed, then discard any surfaces of the old
    // size.
    if (::IOSurfaceGetWidthOfPlane(surf.get(), 0) != (size_t)aYSize.width ||
        ::IOSurfaceGetHeightOfPlane(surf.get(), 0) != (size_t)aYSize.height) {
      continue;
    }

    // Only construct a MacIOSurface object when we find one that isn't
    // in-use, since the constructor adds a usage ref.
    if (!result && !::IOSurfaceIsInUse(surf.get())) {
      result = new MacIOSurface(surf, false, aYUVColorSpace);
    }

    mSurfaces.AppendElement(surf);
  }

  if (!result) {
    result =
        MacIOSurface::CreateYUV422Surface(aYSize, aYUVColorSpace, aColorRange);

    if (mSurfaces.Length() <
        StaticPrefs::layers_iosurfaceimage_recycle_limit()) {
      mSurfaces.AppendElement(result->GetIOSurfaceRef());
    }
  }

  return result.forget();
}
