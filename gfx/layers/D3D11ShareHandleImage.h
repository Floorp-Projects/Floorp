/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_D311_SHARE_HANDLE_IMAGE_H
#define GFX_D311_SHARE_HANDLE_IMAGE_H

#include "mozilla/RefPtr.h"
#include "ImageContainer.h"
#include "d3d11.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/layers/TextureClientRecycleAllocator.h"

namespace mozilla {
namespace layers {

class D3D11RecycleAllocator : public TextureClientRecycleAllocator
{
public:
  explicit D3D11RecycleAllocator(KnowsCompositor* aAllocator,
                                 ID3D11Device* aDevice)
    : TextureClientRecycleAllocator(aAllocator)
    , mDevice(aDevice)
  {}

  already_AddRefed<TextureClient>
  CreateOrRecycleClient(gfx::SurfaceFormat aFormat,
                        const gfx::IntSize& aSize);

protected:
  virtual already_AddRefed<TextureClient> Allocate(
    gfx::SurfaceFormat aFormat,
    gfx::IntSize aSize,
    BackendSelector aSelector,
    TextureFlags aTextureFlags,
    TextureAllocationFlags aAllocFlags) override;

  RefPtr<ID3D11Device> mDevice;
  /**
   * Used for checking if CompositorDevice/ContentDevice is updated.
   */
  RefPtr<ID3D11Device> mImageDevice;
};

// Image class that wraps a ID3D11Texture2D. This class copies the image
// passed into SetData(), so that it can be accessed from other D3D devices.
// This class also manages the synchronization of the copy, to ensure the
// resource is ready to use.
class D3D11ShareHandleImage final : public Image {
public:
  D3D11ShareHandleImage(const gfx::IntSize& aSize,
                        const gfx::IntRect& aRect);
  virtual ~D3D11ShareHandleImage() {}

  bool AllocateTexture(D3D11RecycleAllocator* aAllocator, ID3D11Device* aDevice);

  gfx::IntSize GetSize() const override;
  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;
  TextureClient* GetTextureClient(KnowsCompositor* aForwarder) override;
  gfx::IntRect GetPictureRect() const override { return mPictureRect; }

  ID3D11Texture2D* GetTexture() const;

private:
  gfx::IntSize mSize;
  gfx::IntRect mPictureRect;
  RefPtr<TextureClient> mTextureClient;
  RefPtr<ID3D11Texture2D> mTexture;
};

} // namepace layers
} // namespace mozilla

#endif // GFX_D3DSURFACEIMAGE_H
