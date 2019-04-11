/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_D3D11_YCBCR_IMAGE_H
#define GFX_D3D11_YCBCR_IMAGE_H

#include "d3d11.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/Maybe.h"
#include "ImageContainer.h"

namespace mozilla {
namespace gl {
class GLBlitHelper;
}
namespace layers {

class ImageContainer;
class DXGIYCbCrTextureClient;
class DXGIYCbCrTextureData;

class MOZ_RAII DXGIYCbCrTextureAllocationHelper
    : public ITextureClientAllocationHelper {
 public:
  DXGIYCbCrTextureAllocationHelper(const PlanarYCbCrData& aData,
                                   TextureFlags aTextureFlags,
                                   ID3D11Device* aDevice);

  bool IsCompatible(TextureClient* aTextureClient) override;

  already_AddRefed<TextureClient> Allocate(
      KnowsCompositor* aAllocator) override;

 protected:
  const PlanarYCbCrData& mData;
  RefPtr<ID3D11Device> mDevice;
};

class D3D11YCbCrRecycleAllocator : public TextureClientRecycleAllocator {
 public:
  explicit D3D11YCbCrRecycleAllocator(KnowsCompositor* aAllocator)
      : TextureClientRecycleAllocator(aAllocator) {}

  KnowsCompositor* GetAllocator() const { return mSurfaceAllocator; }

 protected:
  already_AddRefed<TextureClient> Allocate(
      gfx::SurfaceFormat aFormat, gfx::IntSize aSize, BackendSelector aSelector,
      TextureFlags aTextureFlags, TextureAllocationFlags aAllocFlags) override;
};

class D3D11YCbCrImage : public Image {
  friend class gl::GLBlitHelper;

 public:
  D3D11YCbCrImage();
  virtual ~D3D11YCbCrImage();

  // Copies the surface into a sharable texture's surface, and initializes
  // the image.
  bool SetData(KnowsCompositor* aAllocator, ImageContainer* aContainer,
               const PlanarYCbCrData& aData);

  gfx::IntSize GetSize() const override;

  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;

  TextureClient* GetTextureClient(KnowsCompositor* aForwarder) override;

  gfx::IntRect GetPictureRect() const override { return mPictureRect; }

 private:
  const DXGIYCbCrTextureData* GetData() const;

  gfx::IntSize mYSize;
  gfx::IntSize mCbCrSize;
  gfx::IntRect mPictureRect;
  gfx::ColorDepth mColorDepth;
  gfx::YUVColorSpace mColorSpace;
  RefPtr<TextureClient> mTextureClient;
};

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_D3D11_YCBCR_IMAGE_H
