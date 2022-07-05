/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfx_layers_ipc_VideoBridgeParent_h_
#define gfx_layers_ipc_VideoBridgeParent_h_

#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/PVideoBridgeParent.h"

namespace mozilla {
namespace layers {

enum class VideoBridgeSource : uint8_t;
class CompositorThreadHolder;

class VideoBridgeParent final : public PVideoBridgeParent,
                                public HostIPCAllocator,
                                public mozilla::ipc::IShmemAllocator {
 public:
  ~VideoBridgeParent();

  static VideoBridgeParent* GetSingleton(
      const Maybe<VideoBridgeSource>& aSource);

  static void Open(Endpoint<PVideoBridgeParent>&& aEndpoint,
                   VideoBridgeSource aSource);
  static void Shutdown();

  TextureHost* LookupTexture(uint64_t aSerial);

  // PVideoBridgeParent
  void ActorDestroy(ActorDestroyReason aWhy) override;
  PTextureParent* AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                      ReadLockDescriptor& aReadLock,
                                      const LayersBackend& aLayersBackend,
                                      const TextureFlags& aFlags,
                                      const uint64_t& aSerial);
  bool DeallocPTextureParent(PTextureParent* actor);

  // HostIPCAllocator
  base::ProcessId GetChildProcessId() override { return OtherPid(); }
  void NotifyNotUsed(PTextureParent* aTexture,
                     uint64_t aTransactionId) override;
  void SendAsyncMessage(
      const nsTArray<AsyncParentMessageData>& aMessage) override;

  // ISurfaceAllocator
  IShmemAllocator* AsShmemAllocator() override { return this; }
  bool IsSameProcess() const override;
  bool IPCOpen() const override { return !mClosed; }

  // IShmemAllocator
  bool AllocShmem(size_t aSize, ipc::Shmem* aShmem) override;

  bool AllocUnsafeShmem(size_t aSize, ipc::Shmem* aShmem) override;

  bool DeallocShmem(ipc::Shmem& aShmem) override;

 private:
  explicit VideoBridgeParent(VideoBridgeSource aSource);
  void Bind(Endpoint<PVideoBridgeParent>&& aEndpoint);

  void ActorDealloc() override;
  void ReleaseCompositorThread();

  // This keeps us alive until ActorDestroy(), at which point we do a
  // deferred destruction of ourselves.
  RefPtr<VideoBridgeParent> mSelfRef;
  RefPtr<CompositorThreadHolder> mCompositorThreadHolder;

  std::map<uint64_t, PTextureParent*> mTextureMap;

  bool mClosed;
};

}  // namespace layers
}  // namespace mozilla

#endif  // gfx_layers_ipc_VideoBridgeParent_h_
