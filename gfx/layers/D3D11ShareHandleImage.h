/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_D311_SHARE_HANDLE_IMAGE_H
#define GFX_D311_SHARE_HANDLE_IMAGE_H

#include "ImageContainer.h"
#include "d3d11.h"
#include "mozilla/Atomics.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"
#include "mozilla/layers/TextureD3D11.h"

namespace mozilla {
namespace gl {
class GLBlitHelper;
}
namespace layers {

class D3D11RecycleAllocator final : public TextureClientRecycleAllocator {
 public:
  D3D11RecycleAllocator(KnowsCompositor* aAllocator, ID3D11Device* aDevice,
                        gfx::SurfaceFormat aPreferredFormat);

  already_AddRefed<TextureClient> CreateOrRecycleClient(
      gfx::ColorSpace2 aColorSpace, gfx::ColorRange aColorRange,
      const gfx::IntSize& aSize);

  void SetPreferredSurfaceFormat(gfx::SurfaceFormat aPreferredFormat);

 private:
  const RefPtr<ID3D11Device> mDevice;
  const bool mCanUseNV12;
  const bool mCanUseP010;
  const bool mCanUseP016;
  /**
   * Used for checking if CompositorDevice/ContentDevice is updated.
   */
  RefPtr<ID3D11Device> mImageDevice;
  gfx::SurfaceFormat mUsableSurfaceFormat;
};

// Image class that wraps a ID3D11Texture2D. This class copies the image
// passed into SetData(), so that it can be accessed from other D3D devices.
// This class also manages the synchronization of the copy, to ensure the
// resource is ready to use.
class D3D11ShareHandleImage final : public Image {
 public:
  D3D11ShareHandleImage(const gfx::IntSize& aSize, const gfx::IntRect& aRect,
                        gfx::ColorSpace2 aColorSpace,
                        gfx::ColorRange aColorRange);
  virtual ~D3D11ShareHandleImage() = default;

  bool AllocateTexture(D3D11RecycleAllocator* aAllocator,
                       ID3D11Device* aDevice);

  gfx::IntSize GetSize() const override;
  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;
  TextureClient* GetTextureClient(KnowsCompositor* aKnowsCompositor) override;
  gfx::IntRect GetPictureRect() const override { return mPictureRect; }

  ID3D11Texture2D* GetTexture() const;

  gfx::ColorRange GetColorRange() const { return mColorRange; }

 private:
  friend class gl::GLBlitHelper;
  D3D11TextureData* GetData() const {
    if (!mTextureClient) {
      return nullptr;
    }
    return mTextureClient->GetInternalData()->AsD3D11TextureData();
  }

  gfx::IntSize mSize;
  gfx::IntRect mPictureRect;

 public:
  const gfx::ColorSpace2 mColorSpace;

 private:
  gfx::ColorRange mColorRange;
  RefPtr<TextureClient> mTextureClient;
  RefPtr<ID3D11Texture2D> mTexture;
};

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_D3DSURFACEIMAGE_H
