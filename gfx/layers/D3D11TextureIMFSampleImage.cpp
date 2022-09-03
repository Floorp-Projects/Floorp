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
#include "mozilla/gfx/SourceSurfaceRawData.h"
#include "mozilla/layers/KnowsCompositor.h"
#include "mozilla/layers/TextureForwarder.h"

namespace mozilla {
namespace layers {

using namespace gfx;

/* static */
RefPtr<IMFSampleWrapper> IMFSampleWrapper::Create(IMFSample* aVideoSample) {
  RefPtr<IMFSampleWrapper> wrapper = new IMFSampleWrapper(aVideoSample);
  return wrapper;
}

IMFSampleWrapper::IMFSampleWrapper(IMFSample* aVideoSample)
    : mVideoSample(aVideoSample) {}

IMFSampleWrapper::~IMFSampleWrapper() {}

void IMFSampleWrapper::ClearVideoSample() { mVideoSample = nullptr; }

D3D11TextureIMFSampleImage::D3D11TextureIMFSampleImage(
    IMFSample* aVideoSample, ID3D11Texture2D* aTexture, uint32_t aArrayIndex,
    const gfx::IntSize& aSize, const gfx::IntRect& aRect,
    gfx::YUVColorSpace aColorSpace, gfx::ColorRange aColorRange)
    : Image(nullptr, ImageFormat::D3D11_TEXTURE_IMF_SAMPLE),
      mVideoSample(IMFSampleWrapper::Create(aVideoSample)),
      mTexture(aTexture),
      mArrayIndex(aArrayIndex),
      mSize(aSize),
      mPictureRect(aRect),
      mYUVColorSpace(aColorSpace),
      mColorRange(aColorRange) {
  MOZ_ASSERT(XRE_IsGPUProcess());
}

void D3D11TextureIMFSampleImage::AllocateTextureClient(
    KnowsCompositor* aKnowsCompositor, RefPtr<IMFSampleUsageInfo> aUsageInfo) {
  mTextureClient = D3D11TextureData::CreateTextureClient(
      mTexture, mArrayIndex, mSize, gfx::SurfaceFormat::NV12, mYUVColorSpace,
      mColorRange, aKnowsCompositor, aUsageInfo);
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

  RefPtr<gfx::SourceSurface> sourceSurface =
      gfx::Factory::CreateBGRA8DataSourceSurfaceForD3D11Texture(src,
                                                                mArrayIndex);

  // There is a case that mSize and size of mTexture are different. In this
  // case, size of sourceSurface is different from mSize.
  if (sourceSurface && sourceSurface->GetSize() != mSize) {
    MOZ_RELEASE_ASSERT(sourceSurface->GetType() == SurfaceType::DATA_ALIGNED);
    RefPtr<gfx::SourceSurfaceAlignedRawData> rawData =
        static_cast<gfx::SourceSurfaceAlignedRawData*>(sourceSurface.get());
    auto data = rawData->GetData();
    auto stride = rawData->Stride();
    auto size = rawData->GetSize();
    auto format = rawData->GetFormat();
    sourceSurface = gfx::Factory::CreateWrappingDataSourceSurface(
        data, stride, Min(size, mSize), format,
        [](void* aClosure) {
          RefPtr<SourceSurfaceAlignedRawData> surface =
              dont_AddRef(static_cast<SourceSurfaceAlignedRawData*>(aClosure));
        },
        rawData.forget().take());
  }
  return sourceSurface.forget();
}

ID3D11Texture2D* D3D11TextureIMFSampleImage::GetTexture() const {
  return mTexture;
}

RefPtr<IMFSampleWrapper> D3D11TextureIMFSampleImage::GetIMFSampleWrapper() {
  return mVideoSample;
}

}  // namespace layers
}  // namespace mozilla
