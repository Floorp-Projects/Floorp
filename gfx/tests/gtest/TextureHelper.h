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
#include "IMFYCbCrImage.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/TextureD3D11.h"
#include "mozilla/layers/TextureDIB.h"
#endif

using mozilla::gfx::SurfaceFormat;

namespace mozilla {
namespace layers {

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

  if (device && aLayersBackend == LayersBackend::LAYERS_D3D11) {
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

  wr::MaybeExternalImageId id = Nothing();
  return TextureHost::Create(descriptor, nullptr, aLayersBackend,
                             aClient->GetFlags(), id);
}

} // namespace layers
} // namespace mozilla
