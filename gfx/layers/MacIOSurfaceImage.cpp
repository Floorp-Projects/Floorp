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
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/UniquePtr.h"
#include "YCbCrUtils.h"

using namespace mozilla;
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

  // The RDD sandbox won't allow us to create an IOSurface.
  if (XRE_IsRDDProcess()) {
    return false;
  }

  if (aData.mYSkip != 0 || aData.mCbSkip != 0 || aData.mCrSkip != 0 ||
      !(aData.mYUVColorSpace == YUVColorSpace::BT601 ||
        aData.mYUVColorSpace == YUVColorSpace::BT709) ||
      !(aData.mColorRange == ColorRange::FULL ||
        aData.mColorRange == ColorRange::LIMITED) ||
      aData.mColorDepth != ColorDepth::COLOR_8) {
    return false;
  }

  RefPtr<MacIOSurfaceRecycleAllocator> allocator =
      aContainer->GetMacIOSurfaceRecycleAllocator();

  RefPtr<MacIOSurface> surf = allocator->Allocate(
      aData.mYSize, aData.mCbCrSize, aData.mYUVColorSpace, aData.mColorRange);

  surf->Lock(false);

  MOZ_ASSERT(aData.mYSize.height > 0);
  uint8_t* dst = (uint8_t*)surf->GetBaseAddressOfPlane(0);
  size_t stride = surf->GetBytesPerRow(0);
  for (size_t i = 0; i < (size_t)aData.mYSize.height; i++) {
    uint8_t* rowSrc = aData.mYChannel + aData.mYStride * i;
    uint8_t* rowDst = dst + stride * i;
    memcpy(rowDst, rowSrc, aData.mYSize.width);
  }

  // CoreAnimation doesn't appear to support planar YCbCr formats, so we
  // allocated an NV12 surface and now we need to copy and interleave the Cb and
  // Cr channels.
  MOZ_ASSERT(aData.mCbCrSize.height > 0);
  dst = (uint8_t*)surf->GetBaseAddressOfPlane(1);
  stride = surf->GetBytesPerRow(1);
  for (size_t i = 0; i < (size_t)aData.mCbCrSize.height; i++) {
    uint8_t* rowCbSrc = aData.mCbChannel + aData.mCbCrStride * i;
    uint8_t* rowCrSrc = aData.mCrChannel + aData.mCbCrStride * i;
    uint8_t* rowDst = dst + stride * i;

    for (size_t j = 0; j < (size_t)aData.mCbCrSize.width; j++) {
      *rowDst = *rowCbSrc;
      rowDst++;
      rowCbSrc++;
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
    MOZ_ASSERT(::IOSurfaceGetPlaneCount(surf.get()) == 2);

    // If the surface size has changed, then discard any surfaces of the old
    // size.
    if (::IOSurfaceGetWidthOfPlane(surf.get(), 0) != (size_t)aYSize.width ||
        ::IOSurfaceGetHeightOfPlane(surf.get(), 0) != (size_t)aYSize.height ||
        ::IOSurfaceGetWidthOfPlane(surf.get(), 1) != (size_t)aCbCrSize.width ||
        ::IOSurfaceGetHeightOfPlane(surf.get(), 1) !=
            (size_t)aCbCrSize.height) {
      continue;
    }

    // Only construct a MacIOSurface object when we find one that isn't
    // in-use, since the constructor adds a usage ref.
    if (!result && !::IOSurfaceIsInUse(surf.get())) {
      result = new MacIOSurface(surf, 1.0, false, aYUVColorSpace);
    }

    mSurfaces.AppendElement(surf);
  }

  if (!result) {
    result = MacIOSurface::CreateNV12Surface(aYSize, aCbCrSize, aYUVColorSpace,
                                             aColorRange);

    if (mSurfaces.Length() <
        StaticPrefs::layers_iosurfaceimage_recycle_limit()) {
      mSurfaces.AppendElement(result->GetIOSurfaceRef());
    }
  }

  return result.forget();
}
