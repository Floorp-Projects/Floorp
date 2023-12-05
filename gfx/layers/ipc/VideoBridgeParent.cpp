/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoBridgeParent.h"
#include "CompositorThread.h"
#include "mozilla/DataMutex.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/VideoBridgeUtils.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla::layers {

using namespace mozilla::ipc;
using namespace mozilla::gfx;

using VideoBridgeTable =
    EnumeratedArray<VideoBridgeSource, VideoBridgeSource::_Count,
                    VideoBridgeParent*>;

static StaticDataMutex<VideoBridgeTable> sVideoBridgeFromProcess(
    "VideoBridges");
static Atomic<bool> sVideoBridgeParentShutDown(false);

VideoBridgeParent::VideoBridgeParent(VideoBridgeSource aSource)
    : mMonitor("VideoBridgeParent::mMonitor"),
      mCompositorThreadHolder(CompositorThreadHolder::GetSingleton()),
      mClosed(false) {
  auto videoBridgeFromProcess = sVideoBridgeFromProcess.Lock();
  switch (aSource) {
    case VideoBridgeSource::RddProcess:
    case VideoBridgeSource::GpuProcess:
    case VideoBridgeSource::MFMediaEngineCDMProcess:
      (*videoBridgeFromProcess)[aSource] = this;
      break;
    default:
      MOZ_CRASH("Unhandled case");
  }
}

VideoBridgeParent::~VideoBridgeParent() {
  auto videoBridgeFromProcess = sVideoBridgeFromProcess.Lock();
  for (auto& bridgeParent : *videoBridgeFromProcess) {
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
RefPtr<VideoBridgeParent> VideoBridgeParent::GetSingleton(
    const Maybe<VideoBridgeSource>& aSource) {
  MOZ_ASSERT(aSource.isSome());
  auto videoBridgeFromProcess = sVideoBridgeFromProcess.Lock();
  switch (aSource.value()) {
    case VideoBridgeSource::RddProcess:
    case VideoBridgeSource::GpuProcess:
    case VideoBridgeSource::MFMediaEngineCDMProcess:
      MOZ_ASSERT((*videoBridgeFromProcess)[aSource.value()]);
      return RefPtr{(*videoBridgeFromProcess)[aSource.value()]};
    default:
      MOZ_CRASH("Unhandled case");
  }
}

already_AddRefed<TextureHost> VideoBridgeParent::LookupTextureAsync(
    const dom::ContentParentId& aContentId, uint64_t aSerial) {
  MonitorAutoLock lock(mMonitor);

  // We raced shutting down the actor.
  if (NS_WARN_IF(!mCompositorThreadHolder)) {
    MOZ_ASSERT_UNREACHABLE("Called on destroyed VideoBridgeParent actor!");
    return nullptr;
  }

  MOZ_ASSERT(mCompositorThreadHolder->IsInThread());

  auto* actor = mTextureMap[aSerial];
  if (NS_WARN_IF(!actor)) {
    return nullptr;
  }

  if (NS_WARN_IF(aContentId != TextureHost::GetTextureContentId(actor))) {
    return nullptr;
  }

  return do_AddRef(TextureHost::AsTextureHost(actor));
}

already_AddRefed<TextureHost> VideoBridgeParent::LookupTexture(
    const dom::ContentParentId& aContentId, uint64_t aSerial) {
  MonitorAutoLock lock(mMonitor);

  // We raced shutting down the actor.
  if (NS_WARN_IF(!mCompositorThreadHolder)) {
    return nullptr;
  }

  auto* actor = mTextureMap[aSerial];
  if (actor) {
    if (NS_WARN_IF(aContentId != TextureHost::GetTextureContentId(actor))) {
      return nullptr;
    }
    return do_AddRef(TextureHost::AsTextureHost(actor));
  }

  // We cannot block on the Compositor thread because that is the thread we get
  // the IPC calls for the update on.
  if (NS_WARN_IF(mCompositorThreadHolder->IsInThread())) {
    MOZ_ASSERT_UNREACHABLE("Should never call on Compositor thread!");
    return nullptr;
  }

  // Canvas may have raced ahead of VideoBridgeParent setting up the
  // PTextureParent IPDL object. This should happen only rarely/briefly. Since
  // we know that the PTexture constructor must be in the send queue, we can
  // block until the IPDL ping comes back.
  bool complete = false;

  auto resolve = [&](void_t&&) {
    MonitorAutoLock lock(mMonitor);
    complete = true;
    lock.NotifyAll();
  };

  auto reject = [&](ipc::ResponseRejectReason) {
    MonitorAutoLock lock(mMonitor);
    complete = true;
    lock.NotifyAll();
  };

  mCompositorThreadHolder->Dispatch(
      NS_NewRunnableFunction("VideoBridgeParent::LookupTexture", [&]() {
        if (CanSend()) {
          SendPing(std::move(resolve), std::move(reject));
        } else {
          reject(ipc::ResponseRejectReason::ChannelClosed);
        }
      }));

  while (!complete) {
    lock.Wait();
  }

  actor = mTextureMap[aSerial];
  if (!actor) {
    return nullptr;
  }

  if (NS_WARN_IF(aContentId != TextureHost::GetTextureContentId(actor))) {
    return nullptr;
  }

  return do_AddRef(TextureHost::AsTextureHost(actor));
}

void VideoBridgeParent::ActorDestroy(ActorDestroyReason aWhy) {
  bool shutdown = sVideoBridgeParentShutDown;

  if (!shutdown && aWhy == AbnormalShutdown) {
    gfxCriticalNote
        << "VideoBridgeParent receives IPC close with reason=AbnormalShutdown";
  }

  {
    MonitorAutoLock lock(mMonitor);
    // Can't alloc/dealloc shmems from now on.
    mClosed = true;
    mCompositorThreadHolder = nullptr;
  }
}

/* static */
void VideoBridgeParent::Shutdown() {
  CompositorThread()->Dispatch(NS_NewRunnableFunction(
      "VideoBridgeParent::Shutdown",
      []() -> void { VideoBridgeParent::ShutdownInternal(); }));
}

/* static */
void VideoBridgeParent::ShutdownInternal() {
  sVideoBridgeParentShutDown = true;

  nsTArray<RefPtr<VideoBridgeParent>> bridges;

  // We don't want to hold the sVideoBridgeFromProcess lock when the
  // VideoBridgeParent objects are closed without holding a reference to them.
  {
    auto videoBridgeFromProcess = sVideoBridgeFromProcess.Lock();
    for (auto& bridgeParent : *videoBridgeFromProcess) {
      if (bridgeParent) {
        bridges.AppendElement(bridgeParent);
      }
    }
  }

  for (auto& bridge : bridges) {
    bridge->Close();
  }
}

/* static */
void VideoBridgeParent::UnregisterExternalImages() {
  MOZ_ASSERT(sVideoBridgeParentShutDown);

  auto videoBridgeFromProcess = sVideoBridgeFromProcess.Lock();
  for (auto& bridgeParent : *videoBridgeFromProcess) {
    if (bridgeParent) {
      bridgeParent->DoUnregisterExternalImages();
    }
  }
}

void VideoBridgeParent::DoUnregisterExternalImages() {
  const ManagedContainer<PTextureParent>& textures = ManagedPTextureParent();
  for (const auto& key : textures) {
    RefPtr<TextureHost> texture = TextureHost::AsTextureHost(key);

    if (texture) {
      texture->MaybeDestroyRenderTexture();
    }
  }
}

PTextureParent* VideoBridgeParent::AllocPTextureParent(
    const SurfaceDescriptor& aSharedData, ReadLockDescriptor& aReadLock,
    const LayersBackend& aLayersBackend, const TextureFlags& aFlags,
    const dom::ContentParentId& aContentId, const uint64_t& aSerial) {
  PTextureParent* parent = TextureHost::CreateIPDLActor(
      this, aSharedData, std::move(aReadLock), aLayersBackend, aFlags,
      aContentId, aSerial, Nothing());

  if (!parent) {
    return nullptr;
  }

  MonitorAutoLock lock(mMonitor);
  mTextureMap[aSerial] = parent;
  return parent;
}

bool VideoBridgeParent::DeallocPTextureParent(PTextureParent* actor) {
  MonitorAutoLock lock(mMonitor);
  mTextureMap.erase(TextureHost::GetTextureSerial(actor));
  return TextureHost::DestroyIPDLActor(actor);
}

void VideoBridgeParent::SendAsyncMessage(
    const nsTArray<AsyncParentMessageData>& aMessage) {
  MOZ_ASSERT(false, "AsyncMessages not supported");
}

bool VideoBridgeParent::AllocShmem(size_t aSize, ipc::Shmem* aShmem) {
  {
    MonitorAutoLock lock(mMonitor);
    if (mClosed) {
      return false;
    }
  }
  return PVideoBridgeParent::AllocShmem(aSize, aShmem);
}

bool VideoBridgeParent::AllocUnsafeShmem(size_t aSize, ipc::Shmem* aShmem) {
  {
    MonitorAutoLock lock(mMonitor);
    if (mClosed) {
      return false;
    }
  }
  return PVideoBridgeParent::AllocUnsafeShmem(aSize, aShmem);
}

bool VideoBridgeParent::DeallocShmem(ipc::Shmem& aShmem) {
  {
    MonitorAutoLock lock(mMonitor);
    if (mCompositorThreadHolder && !mCompositorThreadHolder->IsInThread()) {
      mCompositorThreadHolder->Dispatch(NS_NewRunnableFunction(
          "gfx::layers::VideoBridgeParent::DeallocShmem",
          [self = RefPtr{this}, shmem = std::move(aShmem)]() mutable {
            self->DeallocShmem(shmem);
          }));
      return true;
    }

    if (mClosed) {
      return false;
    }
  }

  return PVideoBridgeParent::DeallocShmem(aShmem);
}

bool VideoBridgeParent::IsSameProcess() const {
  return OtherPid() == base::GetCurrentProcId();
}

void VideoBridgeParent::NotifyNotUsed(PTextureParent* aTexture,
                                      uint64_t aTransactionId) {}

void VideoBridgeParent::OnChannelError() {
  bool shutdown = sVideoBridgeParentShutDown;
  if (!shutdown) {
    // Destory RenderBufferTextureHosts. Shmems of ShmemTextureHosts are going
    // to be destroyed
    std::vector<wr::ExternalImageId> ids;
    auto& ptextures = ManagedPTextureParent();
    for (const auto& ptexture : ptextures) {
      RefPtr<TextureHost> texture = TextureHost::AsTextureHost(ptexture);
      if (texture && texture->AsShmemTextureHost() &&
          texture->GetMaybeExternalImageId().isSome()) {
        ids.emplace_back(texture->GetMaybeExternalImageId().ref());
      }
    }
    if (!ids.empty()) {
      wr::RenderThread::Get()->DestroyExternalImagesSyncWait(std::move(ids));
    }
  }

  PVideoBridgeParent::OnChannelError();
}

}  // namespace mozilla::layers
