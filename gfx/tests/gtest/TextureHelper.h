/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <vector>

#include "Types.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#ifdef XP_WIN
#  include "IMFYCbCrImage.h"
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/layers/D3D11YCbCrImage.h"
#  include "mozilla/layers/TextureD3D11.h"
#endif

namespace mozilla {
namespace layers {

using gfx::BackendType;
using gfx::IntSize;
using gfx::SurfaceFormat;

/**
 * Create a YCbCrTextureClient according to the given backend.
 */
static already_AddRefed<TextureClient> CreateYCbCrTextureClientWithBackend(
    LayersBackend aLayersBackend) {
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
  clientData.mPictureRect = IntRect(IntPoint(0, 0), ySurface->GetSize());
  clientData.mYStride = ySurface->Stride();
  clientData.mCbCrStride = cbSurface->Stride();
  clientData.mStereoMode = StereoMode::MONO;
  clientData.mChromaSubsampling = ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;
  clientData.mYSkip = 0;
  clientData.mCbSkip = 0;
  clientData.mCrSkip = 0;
  clientData.mCrSkip = 0;

  // Create YCbCrTexture for basic backend.
  if (aLayersBackend == LayersBackend::LAYERS_BASIC) {
    return TextureClient::CreateForYCbCr(
        nullptr, clientData.mPictureRect, clientData.YDataSize(),
        clientData.mYStride, clientData.CbCrDataSize(), clientData.mCbCrStride,
        StereoMode::MONO, gfx::ColorDepth::COLOR_8, gfx::YUVColorSpace::BT601,
        gfx::ColorRange::LIMITED, clientData.mChromaSubsampling,
        TextureFlags::DEALLOCATE_CLIENT);
  }

  if (data) {
    return MakeAndAddRef<TextureClient>(data, TextureFlags::DEALLOCATE_CLIENT,
                                        nullptr);
  }

  return nullptr;
}

/**
 * Create a TextureClient according to the given backend.
 */
static already_AddRefed<TextureClient> CreateTextureClientWithBackend(
    LayersBackend aLayersBackend) {
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
already_AddRefed<TextureHost> CreateTextureHostWithBackend(
    TextureClient* aClient, ISurfaceAllocator* aDeallocator,
    LayersBackend& aLayersBackend) {
  if (!aClient) {
    return nullptr;
  }

  // client serialization
  SurfaceDescriptor descriptor;
  ReadLockDescriptor readLock = null_t();
  RefPtr<TextureHost> textureHost;

  aClient->ToSurfaceDescriptor(descriptor);

  wr::MaybeExternalImageId id = Nothing();
  return TextureHost::Create(descriptor, readLock, aDeallocator, aLayersBackend,
                             aClient->GetFlags(), id);
}

}  // namespace layers
}  // namespace mozilla
