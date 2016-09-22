/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoBridgeChild.h"
#include "VideoBridgeParent.h"
#include "CompositorThread.h"

namespace mozilla {
namespace layers {

StaticRefPtr<VideoBridgeChild> sVideoBridgeChildSingleton;

/* static */ void
VideoBridgeChild::Startup()
{
  sVideoBridgeChildSingleton = new VideoBridgeChild();
  RefPtr<VideoBridgeParent> parent = new VideoBridgeParent();

  MessageLoop* loop = CompositorThreadHolder::Loop();

  sVideoBridgeChildSingleton->Open(parent->GetIPCChannel(),
                                   loop,
                                   ipc::ChildSide);
}

/* static */ void
VideoBridgeChild::Shutdown()
{
  sVideoBridgeChildSingleton = nullptr;

}

VideoBridgeChild::VideoBridgeChild()
  : mMessageLoop(MessageLoop::current())
{
  sVideoBridgeChildSingleton = this;
}

VideoBridgeChild::~VideoBridgeChild()
{
}

VideoBridgeChild*
VideoBridgeChild::GetSingleton()
{
  return sVideoBridgeChildSingleton;
}

bool
VideoBridgeChild::AllocUnsafeShmem(size_t aSize,
                                   ipc::SharedMemory::SharedMemoryType aType,
                                   ipc::Shmem* aShmem)
{
  return PVideoBridgeChild::AllocUnsafeShmem(aSize, aType, aShmem);
}

bool
VideoBridgeChild::AllocShmem(size_t aSize,
                             ipc::SharedMemory::SharedMemoryType aType,
                             ipc::Shmem* aShmem)
{
  MOZ_ASSERT(CanSend());
  return PVideoBridgeChild::AllocShmem(aSize, aType, aShmem);
}

void
VideoBridgeChild::DeallocShmem(ipc::Shmem& aShmem)
{
  PVideoBridgeChild::DeallocShmem(aShmem);
}

PTextureChild*
VideoBridgeChild::AllocPTextureChild(const SurfaceDescriptor&,
                                     const LayersBackend&,
                                     const TextureFlags&,
                                     const uint64_t& aSerial)
{
  MOZ_ASSERT(CanSend());
  return TextureClient::CreateIPDLActor();
}

bool
VideoBridgeChild::DeallocPTextureChild(PTextureChild* actor)
{
  return TextureClient::DestroyIPDLActor(actor);
}

PTextureChild*
VideoBridgeChild::CreateTexture(const SurfaceDescriptor& aSharedData,
                                LayersBackend aLayersBackend,
                                TextureFlags aFlags,
                                uint64_t aSerial)
{
  MOZ_ASSERT(CanSend());
  return SendPTextureConstructor(aSharedData, aLayersBackend, aFlags, aSerial);
}

bool VideoBridgeChild::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

} // namespace layers
} // namespace mozilla
