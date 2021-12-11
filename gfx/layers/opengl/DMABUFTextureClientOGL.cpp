/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DMABUFTextureClientOGL.h"
#include "mozilla/widget/DMABufSurface.h"
#include "gfxPlatform.h"

namespace mozilla::layers {

using namespace gfx;

DMABUFTextureData::DMABUFTextureData(DMABufSurface* aSurface,
                                     BackendType aBackend)
    : mSurface(aSurface), mBackend(aBackend) {
  MOZ_ASSERT(mSurface);
}

DMABUFTextureData::~DMABUFTextureData() = default;

/* static */ DMABUFTextureData* DMABUFTextureData::Create(
    const IntSize& aSize, SurfaceFormat aFormat, BackendType aBackend) {
  if (aFormat != SurfaceFormat::B8G8R8A8 &&
      aFormat != SurfaceFormat::B8G8R8X8) {
    // TODO
    NS_WARNING("DMABUFTextureData::Create() - wrong surface format!");
    return nullptr;
  }

  int flags = DMABUF_TEXTURE;
  if (aFormat == SurfaceFormat::B8G8R8A8) {
    flags |= DMABUF_ALPHA;
  }
  RefPtr<DMABufSurface> surf =
      DMABufSurfaceRGBA::CreateDMABufSurface(aSize.width, aSize.height, flags);
  if (!surf) {
    NS_WARNING("DMABUFTextureData::Create() failed!");
    return nullptr;
  }
  return new DMABUFTextureData(surf, aBackend);
}

TextureData* DMABUFTextureData::CreateSimilar(
    LayersIPCChannel* aAllocator, LayersBackend aLayersBackend,
    TextureFlags aFlags, TextureAllocationFlags aAllocFlags) const {
  return DMABUFTextureData::Create(
      gfx::IntSize(mSurface->GetWidth(), mSurface->GetHeight()),
      mSurface->GetFormat(), mBackend);
}

bool DMABUFTextureData::Serialize(SurfaceDescriptor& aOutDescriptor) {
  return mSurface->Serialize(aOutDescriptor);
}

void DMABUFTextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = gfx::IntSize(mSurface->GetWidth(), mSurface->GetHeight());
  aInfo.format = mSurface->GetFormat();
  aInfo.hasIntermediateBuffer = false;
  aInfo.hasSynchronization = false;
  aInfo.supportsMoz2D = true;
  aInfo.canExposeMappedData = false;
}

bool DMABUFTextureData::Lock(OpenMode) {
  auto surf = mSurface->GetAsDMABufSurfaceRGBA();
  MOZ_ASSERT(!surf->IsMapped(), "Already locked?");
  surf->Map();
  return true;
}

void DMABUFTextureData::Unlock() {
  auto surf = mSurface->GetAsDMABufSurfaceRGBA();
  MOZ_ASSERT(surf->IsMapped(), "Already unlocked?");
  surf->Unmap();
}

already_AddRefed<DataSourceSurface> DMABUFTextureData::GetAsSurface() {
  // TODO: Update for debug purposes.
  return nullptr;
}

already_AddRefed<DrawTarget> DMABUFTextureData::BorrowDrawTarget() {
  MOZ_ASSERT(mBackend != BackendType::NONE);
  if (mBackend == BackendType::NONE) {
    // shouldn't happen, but degrade gracefully
    return nullptr;
  }
  auto surf = mSurface->GetAsDMABufSurfaceRGBA();
  if (!surf->GetMappedRegion()) {
    return nullptr;
  }
  return Factory::CreateDrawTargetForData(
      mBackend, (unsigned char*)surf->GetMappedRegion(),
      IntSize(surf->GetWidth(), surf->GetHeight()),
      surf->GetMappedRegionStride(), surf->GetFormat(), true);
}

void DMABUFTextureData::Deallocate(LayersIPCChannel*) { mSurface = nullptr; }

void DMABUFTextureData::Forget(LayersIPCChannel*) { mSurface = nullptr; }

bool DMABUFTextureData::UpdateFromSurface(gfx::SourceSurface* aSurface) {
  RefPtr<DrawTarget> dt = BorrowDrawTarget();
  if (!dt) {
    return false;
  }

  dt->CopySurface(aSurface, IntRect(IntPoint(), aSurface->GetSize()),
                  IntPoint());
  return true;
}

}  // namespace mozilla::layers
