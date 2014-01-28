/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "D3D9SurfaceImage.h"
#include "gfxImageSurface.h"
#include "gfx2DGlue.h"

namespace mozilla {
namespace layers {

HRESULT
D3D9SurfaceImage::SetData(const Data& aData)
{
  NS_ENSURE_TRUE(aData.mSurface, E_POINTER);
  HRESULT hr;
  RefPtr<IDirect3DSurface9> surface = aData.mSurface;

  RefPtr<IDirect3DDevice9> device;
  hr = surface->GetDevice(byRef(device));
  NS_ENSURE_TRUE(SUCCEEDED(hr), E_FAIL);

  RefPtr<IDirect3D9> d3d9;
  hr = device->GetDirect3D(byRef(d3d9));
  NS_ENSURE_TRUE(SUCCEEDED(hr), E_FAIL);

  D3DSURFACE_DESC desc;
  surface->GetDesc(&desc);
  // Ensure we can convert the textures format to RGB conversion
  // in StretchRect. Fail if we can't.
  hr = d3d9->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT,
                                         D3DDEVTYPE_HAL,
                                         desc.Format,
                                         D3DFMT_X8R8G8B8);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // DXVA surfaces aren't created sharable, so we need to copy the surface
  // to a sharable texture to that it's accessible to the layer manager's
  // device.
  const nsIntRect& region = aData.mRegion;
  RefPtr<IDirect3DTexture9> texture;
  HANDLE shareHandle = nullptr;
  hr = device->CreateTexture(region.width,
                             region.height,
                             1,
                             D3DUSAGE_RENDERTARGET,
                             D3DFMT_X8R8G8B8,
                             D3DPOOL_DEFAULT,
                             byRef(texture),
                             &shareHandle);
  NS_ENSURE_TRUE(SUCCEEDED(hr) && shareHandle, hr);

  // Copy the image onto the texture, preforming YUV -> RGB conversion if necessary.
  RefPtr<IDirect3DSurface9> textureSurface;
  hr = texture->GetSurfaceLevel(0, byRef(textureSurface));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Stash the surface description for later use.
  textureSurface->GetDesc(&mDesc);

  RECT src = { region.x, region.y, region.x+region.width, region.y+region.height };
  hr = device->StretchRect(surface, &src, textureSurface, nullptr, D3DTEXF_NONE);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Flush the draw command now, so that by the time we come to draw this
  // image, we're less likely to need to wait for the draw operation to
  // complete.
  RefPtr<IDirect3DQuery9> query;
  hr = device->CreateQuery(D3DQUERYTYPE_EVENT, byRef(query));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  hr = query->Issue(D3DISSUE_END);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  mTexture = texture;
  mShareHandle = shareHandle;
  mSize = gfx::IntSize(region.width, region.height);
  mQuery = query;

  return S_OK;
}

void
D3D9SurfaceImage::EnsureSynchronized()
{
  if (!mQuery) {
    // Not setup, or already synchronized.
    return;
  }
  int iterations = 0;
  while (iterations < 10 && S_FALSE == mQuery->GetData(nullptr, 0, D3DGETDATA_FLUSH)) {
    Sleep(1);
    iterations++;
  }
  mQuery = nullptr;
}

HANDLE
D3D9SurfaceImage::GetShareHandle()
{
  // Ensure the image has completed its synchronization,
  // and safe to used by the caller on another device.
  EnsureSynchronized();
  return mShareHandle;
}

const D3DSURFACE_DESC&
D3D9SurfaceImage::GetDesc() const
{
  return mDesc;
}

gfx::IntSize
D3D9SurfaceImage::GetSize()
{
  return mSize;
}

already_AddRefed<gfxASurface>
D3D9SurfaceImage::DeprecatedGetAsSurface()
{
  NS_ENSURE_TRUE(mTexture, nullptr);

  HRESULT hr;
  nsRefPtr<gfxImageSurface> surface =
    new gfxImageSurface(gfx::ThebesIntSize(mSize), gfxImageFormat::RGB24);

  if (!surface->CairoSurface() || surface->CairoStatus()) {
    NS_WARNING("Failed to created Cairo image surface for D3D9SurfaceImage.");
    return nullptr;
  }

  // Ensure that the texture is ready to be used.
  EnsureSynchronized();

  // Readback the texture from GPU memory into system memory, so that
  // we can copy it into the Cairo image. This is expensive.
  RefPtr<IDirect3DSurface9> textureSurface;
  hr = mTexture->GetSurfaceLevel(0, byRef(textureSurface));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  RefPtr<IDirect3DDevice9> device;
  hr = mTexture->GetDevice(byRef(device));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  RefPtr<IDirect3DSurface9> systemMemorySurface;
  hr = device->CreateOffscreenPlainSurface(mDesc.Width,
                                           mDesc.Height,
                                           D3DFMT_X8R8G8B8,
                                           D3DPOOL_SYSTEMMEM,
                                           byRef(systemMemorySurface),
                                           0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  hr = device->GetRenderTargetData(textureSurface, systemMemorySurface);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  D3DLOCKED_RECT rect;
  hr = systemMemorySurface->LockRect(&rect, nullptr, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  const unsigned char* src = (const unsigned char*)(rect.pBits);
  const unsigned srcPitch = rect.Pitch;
  for (int y = 0; y < mSize.height; y++) {
    memcpy(surface->Data() + surface->Stride() * y,
           (unsigned char*)(src) + srcPitch * y,
           mSize.width * 4);
  }

  systemMemorySurface->UnlockRect();

  return surface.forget();
}

TemporaryRef<gfx::SourceSurface>
D3D9SurfaceImage::GetAsSourceSurface()
{
  NS_ENSURE_TRUE(mTexture, nullptr);

  HRESULT hr;
  RefPtr<gfx::DataSourceSurface> surface = gfx::Factory::CreateDataSourceSurface(mSize, gfx::SurfaceFormat::B8G8R8X8);

  if (!surface) {
    NS_WARNING("Failed to created SourceSurface for D3D9SurfaceImage.");
    return nullptr;
  }

  // Ensure that the texture is ready to be used.
  EnsureSynchronized();

  // Readback the texture from GPU memory into system memory, so that
  // we can copy it into the Cairo image. This is expensive.
  RefPtr<IDirect3DSurface9> textureSurface;
  hr = mTexture->GetSurfaceLevel(0, byRef(textureSurface));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  RefPtr<IDirect3DDevice9> device;
  hr = mTexture->GetDevice(byRef(device));
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  RefPtr<IDirect3DSurface9> systemMemorySurface;
  hr = device->CreateOffscreenPlainSurface(mDesc.Width,
                                           mDesc.Height,
                                           D3DFMT_X8R8G8B8,
                                           D3DPOOL_SYSTEMMEM,
                                           byRef(systemMemorySurface),
                                           0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  hr = device->GetRenderTargetData(textureSurface, systemMemorySurface);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  D3DLOCKED_RECT rect;
  hr = systemMemorySurface->LockRect(&rect, nullptr, 0);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);

  gfx::DataSourceSurface::MappedSurface mappedSurface;
  if (!surface->Map(gfx::DataSourceSurface::WRITE, &mappedSurface)) {
    systemMemorySurface->UnlockRect();
    return nullptr;
  }

  const unsigned char* src = (const unsigned char*)(rect.pBits);
  const unsigned srcPitch = rect.Pitch;
  for (int y = 0; y < mSize.height; y++) {
    memcpy(mappedSurface.mData + mappedSurface.mStride * y,
           (unsigned char*)(src) + srcPitch * y,
           mSize.width * 4);
  }

  systemMemorySurface->UnlockRect();
  surface->Unmap();

  return surface;
}

} /* layers */
} /* mozilla */
