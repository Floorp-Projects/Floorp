/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_VIDEOBRIDGECHILD_H
#define MOZILLA_GFX_VIDEOBRIDGECHILD_H

#include "mozilla/layers/PVideoBridgeChild.h"
#include "mozilla/layers/VideoBridgeUtils.h"
#include "ISurfaceAllocator.h"
#include "TextureForwarder.h"

namespace mozilla {
namespace layers {

class SynchronousTask;

class VideoBridgeChild final : public PVideoBridgeChild,
                               public TextureForwarder {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoBridgeChild, override);

  static void StartupForGPUProcess();
  static void Shutdown();

  static VideoBridgeChild* GetSingleton();

  // PVideoBridgeChild
  PTextureChild* AllocPTextureChild(const SurfaceDescriptor& aSharedData,
                                    ReadLockDescriptor& aReadLock,
                                    const LayersBackend& aLayersBackend,
                                    const TextureFlags& aFlags,
                                    const dom::ContentParentId& aContentId,
                                    const uint64_t& aSerial);
  bool DeallocPTextureChild(PTextureChild* actor);

  mozilla::ipc::IPCResult RecvPing(PingResolver&& aResolver);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  // ISurfaceAllocator
  bool AllocUnsafeShmem(size_t aSize, mozilla::ipc::Shmem* aShmem) override;
  bool AllocShmem(size_t aSize, mozilla::ipc::Shmem* aShmem) override;
  bool DeallocShmem(mozilla::ipc::Shmem& aShmem) override;

  // TextureForwarder
  PTextureChild* CreateTexture(
      const SurfaceDescriptor& aSharedData, ReadLockDescriptor&& aReadLock,
      LayersBackend aLayersBackend, TextureFlags aFlags,
      const dom::ContentParentId& aContentId, uint64_t aSerial,
      wr::MaybeExternalImageId& aExternalImageId) override;

  // ClientIPCAllocator
  base::ProcessId GetParentPid() const override { return OtherPid(); }
  nsISerialEventTarget* GetThread() const override { return mThread; }
  void CancelWaitForNotifyNotUsed(uint64_t aTextureId) override {
    MOZ_ASSERT(false, "NO RECYCLING HERE");
  }

  // ISurfaceAllocator
  bool IsSameProcess() const override;

  bool CanSend() { return mCanSend; }

  static void Open(Endpoint<PVideoBridgeChild>&& aEndpoint);

 protected:
  void HandleFatalError(const char* aMsg) override;
  bool DispatchAllocShmemInternal(size_t aSize, mozilla::ipc::Shmem* aShmem,
                                  bool aUnsafe);
  void ProxyAllocShmemNow(SynchronousTask* aTask, size_t aSize,
                          mozilla::ipc::Shmem* aShmem, bool aUnsafe,
                          bool* aSuccess);
  void ProxyDeallocShmemNow(SynchronousTask* aTask, mozilla::ipc::Shmem* aShmem,
                            bool* aResult);

 private:
  VideoBridgeChild();
  virtual ~VideoBridgeChild();

  nsCOMPtr<nsISerialEventTarget> mThread;
  bool mCanSend;
};

}  // namespace layers
}  // namespace mozilla

#endif
