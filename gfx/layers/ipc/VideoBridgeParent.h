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
  NS_INLINE_DECL_REFCOUNTING_INHERITED(VideoBridgeParent, HostIPCAllocator)

  static VideoBridgeParent* GetSingleton(
      const Maybe<VideoBridgeSource>& aSource);

  static void Open(Endpoint<PVideoBridgeParent>&& aEndpoint,
                   VideoBridgeSource aSource);
  static void Shutdown();
  static void UnregisterExternalImages();

  TextureHost* LookupTexture(const dom::ContentParentId& aContentId,
                             uint64_t aSerial);

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
  bool AllocShmem(size_t aSize, ipc::Shmem* aShmem) override;

  bool AllocUnsafeShmem(size_t aSize, ipc::Shmem* aShmem) override;

  bool DeallocShmem(ipc::Shmem& aShmem) override;

  void OnChannelError() override;

 private:
  ~VideoBridgeParent();

  explicit VideoBridgeParent(VideoBridgeSource aSource);
  void Bind(Endpoint<PVideoBridgeParent>&& aEndpoint);

  void ReleaseCompositorThread();
  void DoUnregisterExternalImages();

  RefPtr<CompositorThreadHolder> mCompositorThreadHolder;

  std::map<uint64_t, PTextureParent*> mTextureMap;

  bool mClosed;
};

}  // namespace layers
}  // namespace mozilla

#endif  // gfx_layers_ipc_VideoBridgeParent_h_
