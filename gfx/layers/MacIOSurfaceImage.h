/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_MACIOSURFACEIMAGE_H
#define GFX_MACIOSURFACEIMAGE_H

#include "ImageContainer.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/TextureClient.h"

namespace mozilla {

namespace layers {

class MacIOSurfaceImage : public Image {
 public:
  explicit MacIOSurfaceImage(MacIOSurface* aSurface)
      : Image(nullptr, ImageFormat::MAC_IOSURFACE), mSurface(aSurface) {
    if (aSurface) {
      mPictureRect = gfx::IntRect(
          gfx::IntPoint{}, gfx::IntSize(aSurface->GetDevicePixelWidth(0),
                                        aSurface->GetDevicePixelHeight(0)));
    }
  }

  bool SetData(ImageContainer* aContainer, const PlanarYCbCrData& aData);

  MacIOSurface* GetSurface() { return mSurface; }

  gfx::IntSize GetSize() const override {
    return gfx::IntSize::Truncate(mSurface->GetDevicePixelWidth(),
                                  mSurface->GetDevicePixelHeight());
  }

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  nsresult BuildSurfaceDescriptorBuffer(
      SurfaceDescriptorBuffer& aSdBuffer, BuildSdbFlags aFlags,
      const std::function<MemoryOrShmem(uint32_t)>& aAllocate) override;

  TextureClient* GetTextureClient(KnowsCompositor* aKnowsCompositor) override;

  MacIOSurfaceImage* AsMacIOSurfaceImage() override { return this; }

  gfx::IntRect GetPictureRect() const override { return mPictureRect; }

  gfx::ColorDepth GetColorDepth() const override;

 private:
  RefPtr<MacIOSurface> mSurface;
  RefPtr<TextureClient> mTextureClient;
  gfx::IntRect mPictureRect;
};

class MacIOSurfaceRecycleAllocator {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MacIOSurfaceRecycleAllocator)

  already_AddRefed<MacIOSurface> Allocate(
      const gfx::IntSize aYSize, const gfx::IntSize& aCbCrSize,
      gfx::YUVColorSpace aYUVColorSpace,
      gfx::TransferFunction aTransferFunction, gfx::ColorRange aColorRange,
      gfx::ColorDepth aColorDepth);

 private:
  ~MacIOSurfaceRecycleAllocator() = default;

  nsTArray<CFTypeRefPtr<IOSurfaceRef>> mSurfaces;
};

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_SHAREDTEXTUREIMAGE_H
