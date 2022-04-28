/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_D311_TEXTURE_IMF_SAMPLE_IMAGE_H
#define GFX_D311_TEXTURE_IMF_SAMPLE_IMAGE_H

#include "ImageContainer.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureD3D11.h"

struct ID3D11Texture2D;
struct IMFSample;

namespace mozilla {
namespace gl {
class GLBlitHelper;
}
namespace layers {

// Image class that wraps ID3D11Texture2D of IMFSample
// Expected to be used in GPU process.
class D3D11TextureIMFSampleImage final : public Image {
 public:
  D3D11TextureIMFSampleImage(IMFSample* aVideoSample, ID3D11Texture2D* aTexture,
                             uint32_t aArrayIndex, const gfx::IntSize& aSize,
                             const gfx::IntRect& aRect,
                             gfx::YUVColorSpace aColorSpace,
                             gfx::ColorRange aColorRange);
  virtual ~D3D11TextureIMFSampleImage() = default;

  void AllocateTextureClient(KnowsCompositor* aKnowsCompositor);

  gfx::IntSize GetSize() const override;
  already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() override;
  TextureClient* GetTextureClient(KnowsCompositor* aKnowsCompositor) override;
  gfx::IntRect GetPictureRect() const override { return mPictureRect; }

  ID3D11Texture2D* GetTexture() const;

  gfx::YUVColorSpace GetYUVColorSpace() const { return mYUVColorSpace; }
  gfx::ColorRange GetColorRange() const { return mColorRange; }

 private:
  friend class gl::GLBlitHelper;
  D3D11TextureData* GetData() const {
    if (!mTextureClient) {
      return nullptr;
    }
    return mTextureClient->GetInternalData()->AsD3D11TextureData();
  }

  // When ref of IMFSample is held, its ID3D11Texture2D is not reused by
  // IMFTransform.
  RefPtr<IMFSample> mVideoSample;
  RefPtr<ID3D11Texture2D> mTexture;
  const uint32_t mArrayIndex;
  const gfx::IntSize mSize;
  const gfx::IntRect mPictureRect;
  const gfx::YUVColorSpace mYUVColorSpace;
  const gfx::ColorRange mColorRange;
  RefPtr<TextureClient> mTextureClient;
};

}  // namespace layers
}  // namespace mozilla

#endif  // GFX_D311_TEXTURE_IMF_SAMPLE_IMAGE_H
