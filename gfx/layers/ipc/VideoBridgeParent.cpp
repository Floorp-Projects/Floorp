/* vim: set ts=2 sw=2 et tw=80: */
/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoBridgeParent.h"
#include "CompositorThread.h"
#include "mozilla/layers/TextureHost.h"

namespace mozilla {
namespace layers {

using namespace mozilla::ipc;
using namespace mozilla::gfx;

static VideoBridgeParent* sVideoBridgeSingleton;

VideoBridgeParent::VideoBridgeParent()
  : mClosed(false)
{
  mSelfRef = this;
  sVideoBridgeSingleton = this;
  mCompositorThreadRef = CompositorThreadHolder::GetSingleton();
}

VideoBridgeParent::~VideoBridgeParent()
{
  sVideoBridgeSingleton = nullptr;
}

/* static */ VideoBridgeParent*
VideoBridgeParent::GetSingleton()
{
  return sVideoBridgeSingleton;
}

TextureHost*
VideoBridgeParent::LookupTexture(uint64_t aSerial)
{
  return TextureHost::AsTextureHost(mTextureMap[aSerial]);
}

void
VideoBridgeParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Can't alloc/dealloc shmems from now on.
  mClosed = true;
}

void
VideoBridgeParent::DeallocPVideoBridgeParent()
{
  mCompositorThreadRef = nullptr;
  mSelfRef = nullptr;
}

PTextureParent*
VideoBridgeParent::AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                       const LayersBackend& aLayersBackend,
                                       const TextureFlags& aFlags,
                                       const uint64_t& aSerial)
{
  PTextureParent* parent =
    TextureHost::CreateIPDLActor(this, aSharedData, aLayersBackend, aFlags, aSerial, Nothing());
  mTextureMap[aSerial] = parent;
  return parent;
}

bool
VideoBridgeParent::DeallocPTextureParent(PTextureParent* actor)
{
  mTextureMap.erase(TextureHost::GetTextureSerial(actor));
  return TextureHost::DestroyIPDLActor(actor);
}

void
VideoBridgeParent::SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage)
{
  MOZ_ASSERT(false, "AsyncMessages not supported");
}

bool
VideoBridgeParent::AllocShmem(size_t aSize,
                              ipc::SharedMemory::SharedMemoryType aType,
                              ipc::Shmem* aShmem)
{
  if (mClosed) {
    return false;
  }
  return PVideoBridgeParent::AllocShmem(aSize, aType, aShmem);
}

bool
VideoBridgeParent::AllocUnsafeShmem(size_t aSize,
                                    ipc::SharedMemory::SharedMemoryType aType,
                                    ipc::Shmem* aShmem)
{
  if (mClosed) {
    return false;
  }
  return PVideoBridgeParent::AllocUnsafeShmem(aSize, aType, aShmem);
}

void
VideoBridgeParent::DeallocShmem(ipc::Shmem& aShmem)
{
  if (mClosed) {
    return;
  }
  PVideoBridgeParent::DeallocShmem(aShmem);
}

bool
VideoBridgeParent::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

void
VideoBridgeParent::NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId)
{
}

} // namespace layers
} // namespace mozilla
