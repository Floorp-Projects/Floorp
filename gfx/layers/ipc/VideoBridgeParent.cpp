/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoBridgeParent.h"
#include "CompositorThread.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/VideoBridgeUtils.h"

namespace mozilla {
namespace layers {

using namespace mozilla::ipc;
using namespace mozilla::gfx;

static EnumeratedArray<VideoBridgeSource, VideoBridgeSource::_Count,
                       VideoBridgeParent*>
    sVideoBridgeFromProcess;

VideoBridgeParent::VideoBridgeParent(VideoBridgeSource aSource)
    : mCompositorThreadHolder(CompositorThreadHolder::GetSingleton()),
      mClosed(false) {
  mSelfRef = this;
  switch (aSource) {
    case VideoBridgeSource::RddProcess:
    case VideoBridgeSource::GpuProcess:
    case VideoBridgeSource::MFMediaEngineCDMProcess:
      sVideoBridgeFromProcess[aSource] = this;
      break;
    default:
      MOZ_CRASH("Unhandled case");
  }
}

VideoBridgeParent::~VideoBridgeParent() {
  for (auto& bridgeParent : sVideoBridgeFromProcess) {
    if (bridgeParent == this) {
      bridgeParent = nullptr;
    }
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
    const Maybe<VideoBridgeSource>& aSource) {
  MOZ_ASSERT(aSource.isSome());
  switch (aSource.value()) {
    case VideoBridgeSource::RddProcess:
    case VideoBridgeSource::GpuProcess:
    case VideoBridgeSource::MFMediaEngineCDMProcess:
      MOZ_ASSERT(sVideoBridgeFromProcess[aSource.value()]);
      return sVideoBridgeFromProcess[aSource.value()];
    default:
      MOZ_CRASH("Unhandled case");
  }
}

TextureHost* VideoBridgeParent::LookupTexture(uint64_t aSerial) {
  MOZ_DIAGNOSTIC_ASSERT(CompositorThread() &&
                        CompositorThread()->IsOnCurrentThread());
  return TextureHost::AsTextureHost(mTextureMap[aSerial]);
}

void VideoBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  // Can't alloc/dealloc shmems from now on.
  mClosed = true;
}

/* static */
void VideoBridgeParent::Shutdown() {
  for (auto& bridgeParent : sVideoBridgeFromProcess) {
    if (bridgeParent) {
      bridgeParent->ReleaseCompositorThread();
    }
  }
}

void VideoBridgeParent::ReleaseCompositorThread() {
  mCompositorThreadHolder = nullptr;
}

void VideoBridgeParent::ActorDealloc() {
  mCompositorThreadHolder = nullptr;
  ReleaseCompositorThread();
  mSelfRef = nullptr;
}

PTextureParent* VideoBridgeParent::AllocPTextureParent(
    const SurfaceDescriptor& aSharedData, ReadLockDescriptor& aReadLock,
    const LayersBackend& aLayersBackend, const TextureFlags& aFlags,
    const uint64_t& aSerial) {
  PTextureParent* parent =
      TextureHost::CreateIPDLActor(this, aSharedData, std::move(aReadLock),
                                   aLayersBackend, aFlags, aSerial, Nothing());

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

bool VideoBridgeParent::AllocShmem(size_t aSize, ipc::Shmem* aShmem) {
  if (mClosed) {
    return false;
  }
  return PVideoBridgeParent::AllocShmem(aSize, aShmem);
}

bool VideoBridgeParent::AllocUnsafeShmem(size_t aSize, ipc::Shmem* aShmem) {
  if (mClosed) {
    return false;
  }
  return PVideoBridgeParent::AllocUnsafeShmem(aSize, aShmem);
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
