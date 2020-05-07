/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoBridgeParent.h"
#include "CompositorThread.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/VideoBridgeUtils.h"

namespace mozilla {
namespace layers {

using namespace mozilla::ipc;
using namespace mozilla::gfx;

static VideoBridgeParent* sVideoBridgeFromRddProcess;
static VideoBridgeParent* sVideoBridgeFromGpuProcess;

VideoBridgeParent::VideoBridgeParent(VideoBridgeSource aSource)
    : mCompositorThreadHolder(CompositorThreadHolder::GetSingleton()),
      mClosed(false) {
  mSelfRef = this;
  switch (aSource) {
    default:
      MOZ_CRASH("Unhandled case");
    case VideoBridgeSource::RddProcess:
      sVideoBridgeFromRddProcess = this;
      break;
    case VideoBridgeSource::GpuProcess:
      sVideoBridgeFromGpuProcess = this;
      break;
  }
}

VideoBridgeParent::~VideoBridgeParent() {
  if (sVideoBridgeFromRddProcess == this) {
    sVideoBridgeFromRddProcess = nullptr;
  }
  if (sVideoBridgeFromGpuProcess == this) {
    sVideoBridgeFromGpuProcess = nullptr;
  }
}

/* static */
void VideoBridgeParent::Open(Endpoint<PVideoBridgeParent>&& aEndpoint,
                             VideoBridgeSource aSource) {
  RefPtr<VideoBridgeParent> parent = new VideoBridgeParent(aSource);

  CompositorThread()->Dispatch(
      NewRunnableMethod<Endpoint<PVideoBridgeParent>&&>(
          "gfx::layers::VideoBridgeParent::Bind", parent,
          &VideoBridgeParent::Bind, std::move(aEndpoint)));
}

void VideoBridgeParent::Bind(Endpoint<PVideoBridgeParent>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    // We can't recover from this.
    MOZ_CRASH("Failed to bind VideoBridgeParent to endpoint");
  }
}

/* static */
VideoBridgeParent* VideoBridgeParent::GetSingleton(
    Maybe<VideoBridgeSource>& aSource) {
  MOZ_ASSERT(aSource.isSome());
  switch (aSource.value()) {
    default:
      MOZ_CRASH("Unhandled case");
    case VideoBridgeSource::RddProcess:
      MOZ_ASSERT(sVideoBridgeFromRddProcess);
      return sVideoBridgeFromRddProcess;
    case VideoBridgeSource::GpuProcess:
      MOZ_ASSERT(sVideoBridgeFromGpuProcess);
      return sVideoBridgeFromGpuProcess;
  }
}

TextureHost* VideoBridgeParent::LookupTexture(uint64_t aSerial) {
  return TextureHost::AsTextureHost(mTextureMap[aSerial]);
}

void VideoBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Can't alloc/dealloc shmems from now on.
  mClosed = true;
}

void VideoBridgeParent::ActorDealloc() {
  mCompositorThreadHolder = nullptr;
  mSelfRef = nullptr;
}

PTextureParent* VideoBridgeParent::AllocPTextureParent(
    const SurfaceDescriptor& aSharedData, const ReadLockDescriptor& aReadLock,
    const LayersBackend& aLayersBackend, const TextureFlags& aFlags,
    const uint64_t& aSerial) {
  PTextureParent* parent = TextureHost::CreateIPDLActor(
      this, aSharedData, aReadLock, aLayersBackend, aFlags, aSerial, Nothing());

  if (!parent) {
    return nullptr;
  }

  mTextureMap[aSerial] = parent;
  return parent;
}

bool VideoBridgeParent::DeallocPTextureParent(PTextureParent* actor) {
  mTextureMap.erase(TextureHost::GetTextureSerial(actor));
  return TextureHost::DestroyIPDLActor(actor);
}

void VideoBridgeParent::SendAsyncMessage(
    const nsTArray<AsyncParentMessageData>& aMessage) {
  MOZ_ASSERT(false, "AsyncMessages not supported");
}

bool VideoBridgeParent::AllocShmem(size_t aSize,
                                   ipc::SharedMemory::SharedMemoryType aType,
                                   ipc::Shmem* aShmem) {
  if (mClosed) {
    return false;
  }
  return PVideoBridgeParent::AllocShmem(aSize, aType, aShmem);
}

bool VideoBridgeParent::AllocUnsafeShmem(
    size_t aSize, ipc::SharedMemory::SharedMemoryType aType,
    ipc::Shmem* aShmem) {
  if (mClosed) {
    return false;
  }
  return PVideoBridgeParent::AllocUnsafeShmem(aSize, aType, aShmem);
}

bool VideoBridgeParent::DeallocShmem(ipc::Shmem& aShmem) {
  if (mClosed) {
    return false;
  }
  return PVideoBridgeParent::DeallocShmem(aShmem);
}

bool VideoBridgeParent::IsSameProcess() const {
  return OtherPid() == base::GetCurrentProcId();
}

void VideoBridgeParent::NotifyNotUsed(PTextureParent* aTexture,
                                      uint64_t aTransactionId) {}

}  // namespace layers
}  // namespace mozilla
