/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GPUVideoTextureClient.h"
#include "GPUVideoImage.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace layers {

using namespace gfx;

GPUVideoTextureData::GPUVideoTextureData(IGPUVideoSurfaceManager* aManager,
                                         const SurfaceDescriptorGPUVideo& aSD,
                                         const gfx::IntSize& aSize)
    : mManager(aManager), mSD(aSD), mSize(aSize) {}

GPUVideoTextureData::~GPUVideoTextureData() = default;

bool GPUVideoTextureData::Serialize(SurfaceDescriptor& aOutDescriptor) {
  aOutDescriptor = mSD;
  return true;
}

void GPUVideoTextureData::FillInfo(TextureData::Info& aInfo) const {
  aInfo.size = mSize;
  // TODO: We should probably try do better for this.
  // layers::Image doesn't expose a format, so it's hard
  // to figure out in VideoDecoderParent.
  aInfo.format = SurfaceFormat::B8G8R8X8;
  aInfo.hasIntermediateBuffer = false;
  aInfo.hasSynchronization = false;
  aInfo.supportsMoz2D = false;
  aInfo.canExposeMappedData = false;
}

already_AddRefed<SourceSurface> GPUVideoTextureData::GetAsSourceSurface() {
  return mManager->Readback(mSD);
}

void GPUVideoTextureData::Deallocate(LayersIPCChannel* aAllocator) {
  mManager->DeallocateSurfaceDescriptor(mSD);
  mSD = SurfaceDescriptorGPUVideo();
}

void GPUVideoTextureData::Forget(LayersIPCChannel* aAllocator) {
  // We always need to manually deallocate on the client side.
  // Ideally we'd set up our TextureClient with the DEALLOCATE_CLIENT
  // flag, but that forces texture destruction to be synchronous.
  // Instead let's just deallocate from here as well.
  Deallocate(aAllocator);
}

}  // namespace layers
}  // namespace mozilla
