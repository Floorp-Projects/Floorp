/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfx_layers_ipc_VideoBridgeParent_h_
#define gfx_layers_ipc_VideoBridgeParent_h_

#include "mozilla/Monitor.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/PVideoBridgeParent.h"

namespace mozilla::layers {

enum class VideoBridgeSource : uint8_t;
class CompositorThreadHolder;

class VideoBridgeParent final : public PVideoBridgeParent,
                                public HostIPCAllocator,
                                public mozilla::ipc::IShmemAllocator {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(VideoBridgeParent, HostIPCAllocator)

  static RefPtr<VideoBridgeParent> GetSingleton(
      const Maybe<VideoBridgeSource>& aSource);

  static void Open(Endpoint<PVideoBridgeParent>&& aEndpoint,
                   VideoBridgeSource aSource);
  static void Shutdown();
  static void UnregisterExternalImages();

  already_AddRefed<TextureHost> LookupTextureAsync(
      const dom::ContentParentId& aContentId, uint64_t aSerial);
  already_AddRefed<TextureHost> LookupTexture(
      const dom::ContentParentId& aContentId, uint64_t aSerial);

  // PVideoBridgeParent
  void ActorDestroy(ActorDestroyReason aWhy) override;
  PTextureParent* AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                      ReadLockDescriptor& aReadLock,
                                      const LayersBackend& aLayersBackend,
                                      const TextureFlags& aFlags,
                                      const dom::ContentParentId& aContentId,
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
  bool AllocShmem(size_t aSize, mozilla::ipc::Shmem* aShmem) override;

  bool AllocUnsafeShmem(size_t aSize, mozilla::ipc::Shmem* aShmem) override;

  bool DeallocShmem(mozilla::ipc::Shmem& aShmem) override;

  void OnChannelError() override;

 private:
  ~VideoBridgeParent();

  explicit VideoBridgeParent(VideoBridgeSource aSource);
  void Bind(Endpoint<PVideoBridgeParent>&& aEndpoint);
  static void ShutdownInternal();

  void DoUnregisterExternalImages();

  Monitor mMonitor;
  RefPtr<CompositorThreadHolder> mCompositorThreadHolder
      MOZ_GUARDED_BY(mMonitor);
  std::map<uint64_t, PTextureParent*> mTextureMap MOZ_GUARDED_BY(mMonitor);
  bool mClosed;
};

}  // namespace mozilla::layers

#endif  // gfx_layers_ipc_VideoBridgeParent_h_
