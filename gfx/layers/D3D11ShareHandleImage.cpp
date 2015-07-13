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

HRESULT
D3D11ShareHandleImage::SetData(const Data& aData)
{
  NS_ENSURE_TRUE(aData.mTexture, E_POINTER);
  mTexture = aData.mTexture;
  mPictureRect = aData.mRegion;

  D3D11_TEXTURE2D_DESC frameDesc;
  mTexture->GetDesc(&frameDesc);

  mFormat = gfx::SurfaceFormat::B8G8R8A8;
  mSize.width = frameDesc.Width;
  mSize.height = frameDesc.Height;

  return S_OK;
}

gfx::IntSize
D3D11ShareHandleImage::GetSize()
{
  return mSize;
}

TextureClient*
D3D11ShareHandleImage::GetTextureClient(CompositableClient* aClient)
{
  if (!mTextureClient) {
    mTextureClient = TextureClientD3D11::Create(aClient->GetForwarder(),
                                                mFormat,
                                                TextureFlags::DEFAULT,
                                                mTexture,
                                                mSize);
  }
  return mTextureClient;
}

already_AddRefed<gfx::SourceSurface>
D3D11ShareHandleImage::GetAsSourceSurface()
{
  if (!mTexture) {
    NS_WARNING("Cannot readback from shared texture because no texture is available.");
    return nullptr;
  }

  RefPtr<ID3D11Device> device;
  mTexture->GetDevice(byRef(device));

  RefPtr<IDXGIKeyedMutex> keyedMutex;
  if (FAILED(mTexture->QueryInterface(static_cast<IDXGIKeyedMutex**>(byRef(keyedMutex))))) {
    NS_WARNING("Failed to QueryInterface for IDXGIKeyedMutex, strange.");
    return nullptr;
  }

  if (FAILED(keyedMutex->AcquireSync(0, 0))) {
    NS_WARNING("Failed to acquire sync for keyedMutex, plugin failed to release?");
    return nullptr;
  }

  D3D11_TEXTURE2D_DESC desc;
  mTexture->GetDesc(&desc);

  CD3D11_TEXTURE2D_DESC softDesc(desc.Format, desc.Width, desc.Height);
  softDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  softDesc.BindFlags = 0;
  softDesc.MiscFlags = 0;
  softDesc.MipLevels = 1;
  softDesc.Usage = D3D11_USAGE_STAGING;

  RefPtr<ID3D11Texture2D> softTexture;
  HRESULT hr = device->CreateTexture2D(&softDesc,
                                       NULL,
                                       static_cast<ID3D11Texture2D**>(byRef(softTexture)));

  if (FAILED(hr)) {
    NS_WARNING("Failed to create 2D staging texture.");
    keyedMutex->ReleaseSync(0);
    return nullptr;
  }

  RefPtr<ID3D11DeviceContext> context;
  device->GetImmediateContext(byRef(context));
  if (!context) {
    keyedMutex->ReleaseSync(0);
    return nullptr;
  }

  context->CopyResource(softTexture, mTexture);
  keyedMutex->ReleaseSync(0);

  RefPtr<gfx::DataSourceSurface> surface =
    gfx::Factory::CreateDataSourceSurface(mSize, gfx::SurfaceFormat::B8G8R8X8);
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
  return mTexture;
}

} // namespace layers
} // namespace mozilla
