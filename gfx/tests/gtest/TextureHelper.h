/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <vector>

#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/RefPtr.h"
#ifdef XP_WIN
#include "DeviceManagerD3D9.h"
#include "IMFYCbCrImage.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/layers/TextureD3D9.h"
#include "mozilla/layers/TextureDIB.h"
#endif

using mozilla::gfx::SurfaceFormat;

namespace mozilla {
namespace layers {
#ifdef XP_WIN

TextureData*
CreateDXGID3D9TextureData(IntSize aSize, SurfaceFormat aFormat,
                          TextureFlags aTextureFlag)
{

  RefPtr<IDirect3D9Ex> d3d9Ex;
  HMODULE d3d9lib = LoadLibraryW(L"d3d9.dll");
  decltype(Direct3DCreate9Ex)* d3d9Create =
    (decltype(Direct3DCreate9Ex)*)GetProcAddress(d3d9lib, "Direct3DCreate9Ex");
  HRESULT hr = d3d9Create(D3D_SDK_VERSION, getter_AddRefs(d3d9Ex));

  if (!d3d9Ex) {
    return nullptr;
  }

  D3DPRESENT_PARAMETERS params = { 0 };
  params.BackBufferWidth = 1;
  params.BackBufferHeight = 1;
  params.BackBufferFormat = D3DFMT_A8R8G8B8;
  params.BackBufferCount = 1;
  params.SwapEffect = D3DSWAPEFFECT_DISCARD;
  params.hDeviceWindow = nullptr;
  params.Windowed = TRUE;
  params.Flags = D3DPRESENTFLAG_VIDEO;

  RefPtr<IDirect3DDevice9Ex> device;
  hr = d3d9Ex->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, nullptr,
                              D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED |
                                D3DCREATE_MIXED_VERTEXPROCESSING,
                              &params, nullptr, getter_AddRefs(device));

  return DXGID3D9TextureData::Create(aSize, aFormat, aTextureFlag, device);
}
#endif

/**
 * Create a YCbCrTextureClient according to the given backend.
 */
static already_AddRefed<TextureClient>
CreateYCbCrTextureClientWithBackend(LayersBackend aLayersBackend)
{

  TextureData* data = nullptr;
  IntSize size = IntSize(200, 150);
  IntSize ySize = IntSize(400, 300);

  RefPtr<gfxImageSurface> ySurface =
    new gfxImageSurface(ySize, SurfaceFormat::A8);
  RefPtr<gfxImageSurface> cbSurface =
    new gfxImageSurface(size, SurfaceFormat::A8);
  RefPtr<gfxImageSurface> crSurface =
    new gfxImageSurface(size, SurfaceFormat::A8);

  PlanarYCbCrData clientData;
  clientData.mYChannel = ySurface->Data();
  clientData.mCbChannel = cbSurface->Data();
  clientData.mCrChannel = crSurface->Data();
  clientData.mYSize = ySurface->GetSize();
  clientData.mPicSize = ySurface->GetSize();
  clientData.mCbCrSize = cbSurface->GetSize();
  clientData.mYStride = ySurface->Stride();
  clientData.mCbCrStride = cbSurface->Stride();
  clientData.mStereoMode = StereoMode::MONO;
  clientData.mYSkip = 0;
  clientData.mCbSkip = 0;
  clientData.mCrSkip = 0;
  clientData.mCrSkip = 0;
  clientData.mPicX = 0;
  clientData.mPicX = 0;

  // Create YCbCrTexture for basice backend.
  if (aLayersBackend == LayersBackend::LAYERS_BASIC) {
    return TextureClient::CreateForYCbCr(nullptr, clientData.mYSize,
                                         clientData.mCbCrSize, StereoMode::MONO,
                                         YUVColorSpace::BT601,
                                         TextureFlags::DEALLOCATE_CLIENT);
  }

#ifdef XP_WIN
  RefPtr<ID3D11Device> device = DeviceManagerDx::Get()->GetContentDevice();

  if (!device || aLayersBackend != LayersBackend::LAYERS_D3D11) {
    if (aLayersBackend == LayersBackend::LAYERS_D3D11 ||
        aLayersBackend == LayersBackend::LAYERS_D3D9) {
      // Create GetD3D9TextureData.
      data = IMFYCbCrImage::GetD3D9TextureData(clientData, size);
    }
  } else {
    // Create YCbCrD3D11TextureData
    data = IMFYCbCrImage::GetD3D11TextureData(clientData, size);
  }
#endif

  if (data) {
    return MakeAndAddRef<TextureClient>(data, TextureFlags::DEALLOCATE_CLIENT,
                                        nullptr);
  }

  return nullptr;
}

/**
 * Create a TextureClient according to the given backend.
 */
static already_AddRefed<TextureClient>
CreateTextureClientWithBackend(LayersBackend aLayersBackend)
{
  TextureData* data = nullptr;
  SurfaceFormat format = gfxPlatform::GetPlatform()->Optimal2DFormatForContent(
    gfxContentType::COLOR_ALPHA);
  BackendType moz2DBackend =
    gfxPlatform::GetPlatform()->GetContentBackendFor(aLayersBackend);
  TextureAllocationFlags allocFlags = TextureAllocationFlags::ALLOC_DEFAULT;
  IntSize size = IntSize(400, 300);
  TextureFlags textureFlags = TextureFlags::DEALLOCATE_CLIENT;

  if (!gfx::Factory::AllowedSurfaceSize(size)) {
    return nullptr;
  }

#ifdef XP_WIN
  if (aLayersBackend == LayersBackend::LAYERS_D3D11 &&
      (moz2DBackend == BackendType::DIRECT2D ||
       moz2DBackend == BackendType::DIRECT2D1_1)) {
    // Create DXGITextureData.
    data = DXGITextureData::Create(size, format, allocFlags);
  } else if (aLayersBackend == LayersBackend::LAYERS_D3D9 &&
      moz2DBackend == BackendType::CAIRO) {
    // Create DXGID3D9TextureData or D3D9TextureData.
    data = CreateDXGID3D9TextureData(size, format, textureFlags);

    if (!data && DeviceManagerD3D9::GetDevice()) {
      data = D3D9TextureData::Create(size, format, allocFlags);
    }
  } else if (!data && format == SurfaceFormat::B8G8R8X8 &&
      moz2DBackend == BackendType::CAIRO) {
    // Create DIBTextureData.
    data = DIBTextureData::Create(size, format, nullptr);
  }
#endif

  if (!data && aLayersBackend == LayersBackend::LAYERS_BASIC) {
    // Create BufferTextureData.
    data = BufferTextureData::Create(size, format, moz2DBackend, aLayersBackend,
                                     textureFlags, allocFlags, nullptr);
  }

  if (data) {
    return MakeAndAddRef<TextureClient>(data, textureFlags, nullptr);
  }

  return nullptr;
}

/**
 * Create a TextureHost according to the given TextureClient.
 */
already_AddRefed<TextureHost>
CreateTextureHostWithBackend(TextureClient* aClient,
                             LayersBackend& aLayersBackend)
{
  if (!aClient) {
    return nullptr;
  }

  // client serialization
  SurfaceDescriptor descriptor;
  RefPtr<TextureHost> textureHost;

  aClient->ToSurfaceDescriptor(descriptor);

  return TextureHost::Create(descriptor, nullptr, aLayersBackend,
                             aClient->GetFlags());
}

} // namespace layers
} // namespace mozilla
