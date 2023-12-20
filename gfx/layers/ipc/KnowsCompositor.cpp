/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KnowsCompositor.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/ipc/ProtocolUtils.h"

namespace mozilla::layers {

void KnowsCompositor::IdentifyTextureHost(
    const TextureFactoryIdentifier& aIdentifier) {
  auto lock = mData.Lock();
  lock.ref().mTextureFactoryIdentifier = aIdentifier;

  lock.ref().mSyncObject =
      SyncObjectClient::CreateSyncObjectClientForContentDevice(
          aIdentifier.mSyncHandle);
}

KnowsCompositor::KnowsCompositor()
    : mData("KnowsCompositorMutex"), mSerial(++sSerialCounter) {}

KnowsCompositor::~KnowsCompositor() = default;

KnowsCompositorMediaProxy::KnowsCompositorMediaProxy(
    const TextureFactoryIdentifier& aIdentifier) {
  auto lock = mData.Lock();
  lock.ref().mTextureFactoryIdentifier = aIdentifier;
  // overwrite mSerial's value set by the parent class because we use the same
  // serial as the KnowsCompositor we are proxying.
  mThreadSafeAllocator = ImageBridgeChild::GetSingleton();
  lock.ref().mSyncObject = mThreadSafeAllocator->GetSyncObject();
}

KnowsCompositorMediaProxy::~KnowsCompositorMediaProxy() = default;

TextureForwarder* KnowsCompositorMediaProxy::GetTextureForwarder() {
  return mThreadSafeAllocator->GetTextureForwarder();
}

LayersIPCActor* KnowsCompositorMediaProxy::GetLayersIPCActor() {
  return mThreadSafeAllocator->GetLayersIPCActor();
}

void KnowsCompositorMediaProxy::SyncWithCompositor() {
  mThreadSafeAllocator->SyncWithCompositor();
}

bool IsSurfaceDescriptorValid(const SurfaceDescriptor& aSurface) {
  return aSurface.type() != SurfaceDescriptor::T__None &&
         aSurface.type() != SurfaceDescriptor::Tnull_t;
}

uint8_t* GetAddressFromDescriptor(const SurfaceDescriptor& aDescriptor) {
  MOZ_ASSERT(IsSurfaceDescriptorValid(aDescriptor));
  MOZ_RELEASE_ASSERT(
      aDescriptor.type() == SurfaceDescriptor::TSurfaceDescriptorBuffer,
      "GFX: surface descriptor is not the right type.");

  auto memOrShmem = aDescriptor.get_SurfaceDescriptorBuffer().data();
  if (memOrShmem.type() == MemoryOrShmem::TShmem) {
    return memOrShmem.get_Shmem().get<uint8_t>();
  } else {
    return reinterpret_cast<uint8_t*>(memOrShmem.get_uintptr_t());
  }
}

already_AddRefed<gfx::DataSourceSurface> GetSurfaceForDescriptor(
    const SurfaceDescriptor& aDescriptor) {
  if (aDescriptor.type() != SurfaceDescriptor::TSurfaceDescriptorBuffer) {
    return nullptr;
  }
  uint8_t* data = GetAddressFromDescriptor(aDescriptor);
  auto rgb =
      aDescriptor.get_SurfaceDescriptorBuffer().desc().get_RGBDescriptor();
  uint32_t stride = ImageDataSerializer::GetRGBStride(rgb);
  return gfx::Factory::CreateWrappingDataSourceSurface(data, stride, rgb.size(),
                                                       rgb.format());
}

void DestroySurfaceDescriptor(ipc::IShmemAllocator* aAllocator,
                              SurfaceDescriptor* aSurface) {
  MOZ_ASSERT(aSurface);

  SurfaceDescriptorBuffer& desc = aSurface->get_SurfaceDescriptorBuffer();
  switch (desc.data().type()) {
    case MemoryOrShmem::TShmem: {
      aAllocator->DeallocShmem(desc.data().get_Shmem());
      break;
    }
    case MemoryOrShmem::Tuintptr_t: {
      uint8_t* ptr = (uint8_t*)desc.data().get_uintptr_t();
      GfxMemoryImageReporter::WillFree(ptr);
      delete[] ptr;
      break;
    }
    default:
      MOZ_CRASH("surface type not implemented!");
  }
  *aSurface = SurfaceDescriptor();
}

}  // namespace mozilla::layers
