/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaylandDMABUFTextureClientOGL.h"
#include "mozilla/widget/WaylandDMABufSurface.h"
#include "gfxPlatform.h"

namespace mozilla::layers {

using namespace gfx;

WaylandDMABUFTextureData::WaylandDMABUFTextureData(
    WaylandDMABufSurface* aSurface, BackendType aBackend)
    : mSurface(aSurface), mBackend(aBackend) {
  MOZ_ASSERT(mSurface);
}

WaylandDMABUFTextureData::~WaylandDMABUFTextureData() = default;

/* static */ WaylandDMABUFTextureData* WaylandDMABUFTextureData::Create(
    const IntSize& aSize, SurfaceFormat aFormat, BackendType aBackend) {
  if (aFormat != SurfaceFormat::B8G8R8A8 &&
      aFormat != SurfaceFormat::B8G8R8X8) {
    // TODO
    NS_WARNING("WaylandDMABUFTextureData::Create() - wrong surface format!");
    return nullptr;
  }

  int flags = DMABUF_TEXTURE;
  if (aFormat == SurfaceFormat::B8G8R8A8) {
    flags |= DMABUF_ALPHA;
  }
  RefPtr<WaylandDMABufSurface> surf = WaylandDMABufSurface::CreateDMABufSurface(
      aSize.width, aSize.height, flags);
  return new WaylandDMABUFTextureData(surf, aBackend);
}

TextureData* WaylandDMABUFTextureData::CreateSimilar(
    LayersIPCChannel* aAllocator, LayersBackend aLayersBackend,
    TextureFlags aFlags, TextureAllocationFlags aAllocFlags) const {
  return WaylandDMABUFTextureData::Create(
      gfx::IntSize(mSurface->GetWidth(), mSurface->GetHeight()),
      mSurface->GetFormat(), mBackend);
}

bool WaylandDMABUFTextureData::Serialize(SurfaceDescriptor& aOutDescriptor) {
  mSurface->Serialize(aOutDescriptor);
  return true;
}

void WaylandDMABUFTextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = gfx::IntSize(mSurface->GetWidth(), mSurface->GetHeight());
  aInfo.format = mSurface->GetFormat();
  aInfo.hasIntermediateBuffer = false;
  aInfo.hasSynchronization = false;
  aInfo.supportsMoz2D = true;
  aInfo.canExposeMappedData = false;
}

bool WaylandDMABUFTextureData::Lock(OpenMode) {
  MOZ_ASSERT(!mSurface->IsMapped(), "Already locked?");
  mSurface->Map();
  return true;
}

void WaylandDMABUFTextureData::Unlock() { mSurface->Unmap(); }

already_AddRefed<DataSourceSurface> WaylandDMABUFTextureData::GetAsSurface() {
  // TODO: Update for debug purposes.
  return nullptr;
}

already_AddRefed<DrawTarget> WaylandDMABUFTextureData::BorrowDrawTarget() {
  MOZ_ASSERT(mBackend != BackendType::NONE);
  if (mBackend == BackendType::NONE) {
    // shouldn't happen, but degrade gracefully
    return nullptr;
  }
  return Factory::CreateDrawTargetForData(
      mBackend, (unsigned char*)mSurface->GetMappedRegion(),
      IntSize(mSurface->GetWidth(), mSurface->GetHeight()),
      mSurface->GetMappedRegionStride(), mSurface->GetFormat(), true);
}

void WaylandDMABUFTextureData::Deallocate(LayersIPCChannel*) {
  mSurface = nullptr;
}

void WaylandDMABUFTextureData::Forget(LayersIPCChannel*) { mSurface = nullptr; }

bool WaylandDMABUFTextureData::UpdateFromSurface(gfx::SourceSurface* aSurface) {
  RefPtr<DrawTarget> dt = BorrowDrawTarget();
  if (!dt) {
    return false;
  }

  dt->CopySurface(aSurface, IntRect(IntPoint(), aSurface->GetSize()),
                  IntPoint());
  return true;
}

}  // namespace mozilla::layers
