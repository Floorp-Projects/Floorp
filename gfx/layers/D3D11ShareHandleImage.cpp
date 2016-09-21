/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMF.h"
#include "D3D11ShareHandleImage.h"
#include "gfxImageSurface.h"
#include "gfxWindowsPlatform.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositableForwarder.h"
#include "d3d11.h"

namespace mozilla {
namespace layers {

D3D11ShareHandleImage::D3D11ShareHandleImage(const gfx::IntSize& aSize,
                                             const gfx::IntRect& aRect)
 : Image(nullptr, ImageFormat::D3D11_SHARE_HANDLE_TEXTURE),
   mSize(aSize),
   mPictureRect(aRect)
{
}

bool
D3D11ShareHandleImage::AllocateTexture(D3D11RecycleAllocator* aAllocator)
{
  mTextureClient = aAllocator->CreateOrRecycleClient(gfx::SurfaceFormat::B8G8R8A8, mSize);
  return !!mTextureClient;
}

gfx::IntSize
D3D11ShareHandleImage::GetSize()
{
  return mSize;
}

TextureClient*
D3D11ShareHandleImage::GetTextureClient(CompositableClient* aClient)
{
  return mTextureClient;
}

already_AddRefed<gfx::SourceSurface>
D3D11ShareHandleImage::GetAsSourceSurface()
{
  RefPtr<ID3D11Texture2D> texture = GetTexture();
  if (!texture) {
    NS_WARNING("Cannot readback from shared texture because no texture is available.");
    return nullptr;
  }

  RefPtr<ID3D11Device> device;
  texture->GetDevice(getter_AddRefs(device));

  D3D11_TEXTURE2D_DESC desc;
  texture->GetDesc(&desc);

  CD3D11_TEXTURE2D_DESC softDesc(desc.Format, desc.Width, desc.Height);
  softDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  softDesc.BindFlags = 0;
  softDesc.MiscFlags = 0;
  softDesc.MipLevels = 1;
  softDesc.Usage = D3D11_USAGE_STAGING;

  RefPtr<ID3D11Texture2D> softTexture;
  HRESULT hr = device->CreateTexture2D(&softDesc,
                                       NULL,
                                       static_cast<ID3D11Texture2D**>(getter_AddRefs(softTexture)));

  if (FAILED(hr)) {
    NS_WARNING("Failed to create 2D staging texture.");
    return nullptr;
  }

  RefPtr<ID3D11DeviceContext> context;
  device->GetImmediateContext(getter_AddRefs(context));
  if (!context) {
    return nullptr;
  }

  context->CopyResource(softTexture, texture);

  RefPtr<gfx::DataSourceSurface> surface =
    gfx::Factory::CreateDataSourceSurface(mSize, gfx::SurfaceFormat::B8G8R8A8);
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
           (unsigned char*)(map.pData) + map.RowPitch * y,
           mSize.width * 4);
  }

  context->Unmap(softTexture, 0);
  surface->Unmap();

  return surface.forget();
}

ID3D11Texture2D*
D3D11ShareHandleImage::GetTexture() const {
  return static_cast<D3D11TextureData*>(mTextureClient->GetInternalData())->GetD3D11Texture();
}

already_AddRefed<TextureClient>
D3D11RecycleAllocator::Allocate(gfx::SurfaceFormat aFormat,
                                gfx::IntSize aSize,
                                BackendSelector aSelector,
                                TextureFlags aTextureFlags,
                                TextureAllocationFlags aAllocFlags)
{
  return CreateD3D11TextureClientWithDevice(aSize, aFormat,
                                            aTextureFlags, aAllocFlags,
                                            mDevice, mSurfaceAllocator);
}

already_AddRefed<TextureClient>
D3D11RecycleAllocator::CreateOrRecycleClient(gfx::SurfaceFormat aFormat,
                                             const gfx::IntSize& aSize)
{
  RefPtr<TextureClient> textureClient =
    CreateOrRecycle(aFormat,
                    aSize,
                    BackendSelector::Content,
                    layers::TextureFlags::DEFAULT,
                    TextureAllocationFlags::ALLOC_MANUAL_SYNCHRONIZATION);
  return textureClient.forget();
}


} // namespace layers
} // namespace mozilla
