/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "D3D11ShareHandleImage.h"
#include "gfxImageSurface.h"
#include "gfxWindowsPlatform.h"
#include "mozilla/layers/TextureClient.h"
#include "d3d11.h"


namespace mozilla {
namespace layers {

HRESULT
D3D11ShareHandleImage::SetData(const Data& aData)
{
  NS_ENSURE_TRUE(aData.mTexture, E_POINTER);
  HRESULT hr;

  RefPtr<ID3D11Texture2D> texture = aData.mTexture;
  RefPtr<ID3D11Device> device = aData.mDevice;
  RefPtr<ID3D11DeviceContext> context = aData.mContext;
  
  // Create a sharable ID3D11Texture2D of the same format.
  D3D11_TEXTURE2D_DESC frameDesc;
  texture->GetDesc(&frameDesc);

  D3D11_TEXTURE2D_DESC desc;
  desc.Width = aData.mRegion.width;
  desc.Height = aData.mRegion.height;
  desc.Format = frameDesc.Format;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  RefPtr<ID3D11Texture2D> sharableTexture;
  hr = device->CreateTexture2D(&desc, nullptr, byRef(sharableTexture));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IDXGIResource> sharableDXGIRes;
  hr = sharableTexture->QueryInterface(static_cast<IDXGIResource**>(byRef(sharableDXGIRes)));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Obtain handle to IDXGIResource object.
  HANDLE sharedHandle = 0;
  hr = sharableDXGIRes->GetSharedHandle(&sharedHandle);
  NS_ENSURE_TRUE(SUCCEEDED(hr) && sharedHandle, hr);

  // Lock the shared texture.
  RefPtr<IDXGIKeyedMutex> keyedMutex;
  hr = sharableTexture->QueryInterface(static_cast<IDXGIKeyedMutex**>(byRef(keyedMutex)));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && keyedMutex, hr);

  hr = keyedMutex->AcquireSync(0, INFINITE);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  const nsIntRect& r = aData.mRegion;
  D3D11_BOX srcRect = { r.x, r.y, 0, r.x+r.width, r.y+r.height, 0 };
  context->CopySubresourceRegion(sharableTexture, 0, 0, 0, 0, texture, 0, &srcRect);

  hr = keyedMutex->ReleaseSync(0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  const nsIntRect& region = aData.mRegion;
  mSize = gfxIntSize(region.width, region.height);
  mTexture = sharableTexture;
  mShareHandle = sharedHandle;

  mSurfaceDescriptor = SurfaceDescriptorD3D10((WindowsHandle)sharedHandle, true);
  return S_OK;
}

HANDLE
D3D11ShareHandleImage::GetShareHandle()
{
  // Ensure the image has completed its synchronization,
  // and safe to used by the caller on another device.
  //EnsureSynchronized();
  return mShareHandle;
}

gfxIntSize
D3D11ShareHandleImage::GetSize()
{
  return mSize;
}

already_AddRefed<gfxASurface>
D3D11ShareHandleImage::GetAsSurface()
{
  HRESULT hr;
  RefPtr<ID3D11Device> device =
    gfxWindowsPlatform::GetPlatform()->GetD3D11Device();
  if (!device || !mTexture) {
    NS_WARNING("Cannot readback from shared texture because no D3D11 device is available.");
    return nullptr;
  }

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
  hr = device->CreateTexture2D(&softDesc,
                               NULL,
                               static_cast<ID3D11Texture2D**>(byRef(softTexture)));

  if (FAILED(hr)) {
    NS_WARNING("Failed to create 2D staging texture.");
    return nullptr;
  }

  RefPtr<ID3D11DeviceContext> context;
  device->GetImmediateContext(byRef(context));
  NS_ENSURE_TRUE(context, nullptr);

  context->CopyResource(softTexture, mTexture);
  keyedMutex->ReleaseSync(0);


  nsRefPtr<gfxImageSurface> surface =
    new gfxImageSurface(mSize, gfxASurface::ImageFormatRGB24);

  if (!surface->CairoSurface() || surface->CairoStatus()) {
    NS_WARNING("Failed to created image surface for DXGI texture.");
    return nullptr;
  }

  D3D11_MAPPED_SUBRESOURCE map;
  context->Map(softTexture, 0, D3D11_MAP_READ, 0, &map);

  for (int y = 0; y < mSize.height; y++) {
    memcpy(surface->Data() + surface->Stride() * y,
           (unsigned char*)(map.pData) + map.RowPitch * y,
           mSize.width * 4);
  }

  context->Unmap(softTexture, 0);

  return surface.forget();  
}

ID3D11Texture2D*
D3D11ShareHandleImage::GetTexture() const {
  return mTexture;
}

} /* layers */
} /* mozilla */
