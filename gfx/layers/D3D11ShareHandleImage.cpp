/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "D3D11ShareHandleImage.h"
#include <memory>
#include "DXVA2Manager.h"
#include "WMF.h"
#include "d3d11.h"
#include "gfxImageSurface.h"
#include "gfxPrefs.h"
#include "gfxWindowsPlatform.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureD3D11.h"

namespace mozilla {
namespace layers {

using namespace gfx;

D3D11ShareHandleImage::D3D11ShareHandleImage(const gfx::IntSize& aSize,
                                             const gfx::IntRect& aRect,
                                             gfx::YUVColorSpace aColorSpace)
    : Image(nullptr, ImageFormat::D3D11_SHARE_HANDLE_TEXTURE),
      mSize(aSize),
      mPictureRect(aRect),
      mYUVColorSpace(aColorSpace) {}

bool D3D11ShareHandleImage::AllocateTexture(D3D11RecycleAllocator* aAllocator,
                                            ID3D11Device* aDevice) {
  if (aAllocator) {
    mTextureClient = aAllocator->CreateOrRecycleClient(mYUVColorSpace, mSize);
    if (mTextureClient) {
      D3D11TextureData* textureData = GetData();
      MOZ_DIAGNOSTIC_ASSERT(textureData, "Wrong TextureDataType");
      mTexture = textureData->GetD3D11Texture();
      return true;
    }
    return false;
  } else {
    MOZ_ASSERT(aDevice);
    CD3D11_TEXTURE2D_DESC newDesc(
        DXGI_FORMAT_B8G8R8A8_UNORM, mSize.width, mSize.height, 1, 1,
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
    newDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr =
        aDevice->CreateTexture2D(&newDesc, nullptr, getter_AddRefs(mTexture));
    return SUCCEEDED(hr);
  }
}

gfx::IntSize D3D11ShareHandleImage::GetSize() const { return mSize; }

TextureClient* D3D11ShareHandleImage::GetTextureClient(
    KnowsCompositor* aForwarder) {
  return mTextureClient;
}

already_AddRefed<gfx::SourceSurface>
D3D11ShareHandleImage::GetAsSourceSurface() {
  RefPtr<ID3D11Texture2D> texture = GetTexture();
  if (!texture) {
    gfxWarning() << "Cannot readback from shared texture because no texture is "
                    "available.";
    return nullptr;
  }

  RefPtr<ID3D11Device> device;
  texture->GetDevice(getter_AddRefs(device));

  D3D11_TEXTURE2D_DESC desc;
  texture->GetDesc(&desc);

  HRESULT hr;

  if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
    nsAutoCString error;
    std::unique_ptr<DXVA2Manager> manager(
        DXVA2Manager::CreateD3D11DXVA(nullptr, error, device));

    if (!manager) {
      gfxWarning() << "Failed to create DXVA2 manager!";
      return nullptr;
    }

    RefPtr<ID3D11Texture2D> outTexture;

    hr = manager->CopyToBGRATexture(texture, getter_AddRefs(outTexture));

    if (FAILED(hr)) {
      gfxWarning() << "Failed to copy to BGRA texture.";
      return nullptr;
    }

    texture = outTexture;
    texture->GetDesc(&desc);
  }

  CD3D11_TEXTURE2D_DESC softDesc(desc.Format, desc.Width, desc.Height);
  softDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  softDesc.BindFlags = 0;
  softDesc.MiscFlags = 0;
  softDesc.MipLevels = 1;
  softDesc.Usage = D3D11_USAGE_STAGING;

  RefPtr<ID3D11Texture2D> softTexture;
  hr = device->CreateTexture2D(
      &softDesc, NULL,
      static_cast<ID3D11Texture2D**>(getter_AddRefs(softTexture)));

  if (FAILED(hr)) {
    NS_WARNING("Failed to create 2D staging texture.");
    return nullptr;
  }

  RefPtr<ID3D11DeviceContext> context;
  device->GetImmediateContext(getter_AddRefs(context));
  if (!context) {
    gfxCriticalError() << "Failed to get immediate context.";
    return nullptr;
  }

  RefPtr<IDXGIKeyedMutex> mutex;
  hr = texture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));

  if (SUCCEEDED(hr) && mutex) {
    hr = mutex->AcquireSync(0, 2000);
    if (hr != S_OK) {
      NS_WARNING("Acquire sync didn't manage to return within 2 seconds.");
    }
  }
  context->CopyResource(softTexture, texture);
  if (SUCCEEDED(hr) && mutex) {
    mutex->ReleaseSync(0);
  }

  RefPtr<gfx::DataSourceSurface> surface =
      gfx::Factory::CreateDataSourceSurface(mSize,
                                            gfx::SurfaceFormat::B8G8R8A8);
  if (NS_WARN_IF(!surface)) {
    return nullptr;
  }

  gfx::DataSourceSurface::MappedSurface mappedSurface;
  if (!surface->Map(gfx::DataSourceSurface::WRITE, &mappedSurface)) {
    return nullptr;
  }

  D3D11_MAPPED_SUBRESOURCE map;
  hr = context->Map(softTexture, 0, D3D11_MAP_READ, 0, &map);
  if (!SUCCEEDED(hr)) {
    surface->Unmap();
    return nullptr;
  }

  for (int y = 0; y < mSize.height; y++) {
    memcpy(mappedSurface.mData + mappedSurface.mStride * y,
           (unsigned char*)(map.pData) + map.RowPitch * y, mSize.width * 4);
  }

  context->Unmap(softTexture, 0);
  surface->Unmap();

  return surface.forget();
}

ID3D11Texture2D* D3D11ShareHandleImage::GetTexture() const { return mTexture; }

class MOZ_RAII D3D11TextureClientAllocationHelper
    : public ITextureClientAllocationHelper {
 public:
  D3D11TextureClientAllocationHelper(gfx::SurfaceFormat aFormat,
                                     gfx::YUVColorSpace aColorSpace,
                                     const gfx::IntSize& aSize,
                                     TextureAllocationFlags aAllocFlags,
                                     ID3D11Device* aDevice,
                                     TextureFlags aTextureFlags)
      : ITextureClientAllocationHelper(aFormat, aSize, BackendSelector::Content,
                                       aTextureFlags, aAllocFlags),
        mYUVColorSpace(aColorSpace),
        mDevice(aDevice) {}

  bool IsCompatible(TextureClient* aTextureClient) override {
    D3D11TextureData* textureData =
        aTextureClient->GetInternalData()->AsD3D11TextureData();
    if (!textureData || aTextureClient->GetFormat() != mFormat ||
        aTextureClient->GetSize() != mSize) {
      return false;
    }
    // TODO: Should we also check for change in the allocation flags if RGBA?
    return (aTextureClient->GetFormat() != gfx::SurfaceFormat::NV12 &&
            aTextureClient->GetFormat() != gfx::SurfaceFormat::P010 &&
            aTextureClient->GetFormat() != gfx::SurfaceFormat::P016) ||
           (textureData->GetYUVColorSpace() == mYUVColorSpace &&
            textureData->GetTextureAllocationFlags() == mAllocationFlags);
  }

  already_AddRefed<TextureClient> Allocate(
      KnowsCompositor* aAllocator) override {
    D3D11TextureData* data =
        D3D11TextureData::Create(mSize, mFormat, mAllocationFlags, mDevice);
    if (!data) {
      return nullptr;
    }
    data->SetYUVColorSpace(mYUVColorSpace);
    return MakeAndAddRef<TextureClient>(data, mTextureFlags,
                                        aAllocator->GetTextureForwarder());
  }

 private:
  gfx::YUVColorSpace mYUVColorSpace;
  const RefPtr<ID3D11Device> mDevice;
};

D3D11RecycleAllocator::D3D11RecycleAllocator(
    KnowsCompositor* aAllocator, ID3D11Device* aDevice,
    gfx::SurfaceFormat aPreferredFormat)
    : TextureClientRecycleAllocator(aAllocator),
      mDevice(aDevice),
      mCanUseNV12(gfxPrefs::PDMWMFUseNV12Format() &&
                  gfx::DeviceManagerDx::Get()->CanUseNV12()),
      mCanUseP010(gfxPrefs::PDMWMFUseNV12Format() &&
                  gfx::DeviceManagerDx::Get()->CanUseP010()),
      mCanUseP016(gfxPrefs::PDMWMFUseNV12Format() &&
                  gfx::DeviceManagerDx::Get()->CanUseP016()) {
  SetPreferredSurfaceFormat(aPreferredFormat);
}

void D3D11RecycleAllocator::SetPreferredSurfaceFormat(
    gfx::SurfaceFormat aPreferredFormat) {
  if ((aPreferredFormat == gfx::SurfaceFormat::NV12 && mCanUseNV12) ||
      (aPreferredFormat == gfx::SurfaceFormat::P010 && mCanUseP010) ||
      (aPreferredFormat == gfx::SurfaceFormat::P016 && mCanUseP016)) {
    mUsableSurfaceFormat = aPreferredFormat;
    return;
  }
  // We can't handle the native source format, set it to BGRA which will
  // force the caller to convert it later.
  mUsableSurfaceFormat = gfx::SurfaceFormat::B8G8R8A8;
}

already_AddRefed<TextureClient> D3D11RecycleAllocator::CreateOrRecycleClient(
    gfx::YUVColorSpace aColorSpace, const gfx::IntSize& aSize) {
  // When CompositorDevice or ContentDevice is updated,
  // we could not reuse old D3D11Textures. It could cause video flickering.
  RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetImageDevice();
  if (!!mImageDevice && mImageDevice != device) {
    ShrinkToMinimumSize();
  }
  mImageDevice = device;

  TextureAllocationFlags allocFlags = TextureAllocationFlags::ALLOC_DEFAULT;
  if (gfxPrefs::PDMWMFUseSyncTexture() ||
      mDevice == DeviceManagerDx::Get()->GetCompositorDevice()) {
    // If our device is the compositor device, we don't need any synchronization
    // in practice.
    allocFlags = TextureAllocationFlags::ALLOC_MANUAL_SYNCHRONIZATION;
  }

  D3D11TextureClientAllocationHelper helper(mUsableSurfaceFormat, aColorSpace,
                                            aSize, allocFlags, mDevice,
                                            layers::TextureFlags::DEFAULT);

  RefPtr<TextureClient> textureClient = CreateOrRecycle(helper);
  return textureClient.forget();
}

}  // namespace layers
}  // namespace mozilla
