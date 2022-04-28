/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <d3d11.h>
#include <memory>
#include <mfobjects.h>

#include "D3D11TextureIMFSampleImage.h"
#include "WMF.h"
#include "mozilla/layers/KnowsCompositor.h"
#include "mozilla/layers/TextureForwarder.h"

namespace mozilla {
namespace layers {

using namespace gfx;

D3D11TextureIMFSampleImage::D3D11TextureIMFSampleImage(
    IMFSample* aVideoSample, ID3D11Texture2D* aTexture, uint32_t aArrayIndex,
    const gfx::IntSize& aSize, const gfx::IntRect& aRect,
    gfx::YUVColorSpace aColorSpace, gfx::ColorRange aColorRange)
    : Image(nullptr, ImageFormat::D3D11_TEXTURE_IMF_SAMPLE),
      mVideoSample(aVideoSample),
      mTexture(aTexture),
      mArrayIndex(aArrayIndex),
      mSize(aSize),
      mPictureRect(aRect),
      mYUVColorSpace(aColorSpace),
      mColorRange(aColorRange) {
  MOZ_ASSERT(XRE_IsGPUProcess());
}

void D3D11TextureIMFSampleImage::AllocateTextureClient(
    KnowsCompositor* aKnowsCompositor) {
  mTextureClient = D3D11TextureData::CreateTextureClient(
      mTexture, mArrayIndex, mSize, gfx::SurfaceFormat::NV12, mYUVColorSpace,
      mColorRange, aKnowsCompositor);
  MOZ_ASSERT(mTextureClient);
}

gfx::IntSize D3D11TextureIMFSampleImage::GetSize() const { return mSize; }

TextureClient* D3D11TextureIMFSampleImage::GetTextureClient(
    KnowsCompositor* aKnowsCompositor) {
  return mTextureClient;
}

already_AddRefed<gfx::SourceSurface>
D3D11TextureIMFSampleImage::GetAsSourceSurface() {
  RefPtr<ID3D11Texture2D> src = GetTexture();
  if (!src) {
    gfxWarning() << "Cannot readback from shared texture because no texture is "
                    "available.";
    return nullptr;
  }

  return gfx::Factory::CreateBGRA8DataSourceSurfaceForD3D11Texture(src,
                                                                   mArrayIndex);
}

ID3D11Texture2D* D3D11TextureIMFSampleImage::GetTexture() const {
  return mTexture;
}

}  // namespace layers
}  // namespace mozilla
