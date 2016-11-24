/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gfx_layers_ipc_VideoBridgeParent_h_
#define gfx_layers_ipc_VideoBridgeParent_h_

#include "mozilla/layers/PVideoBridgeParent.h"
#include "mozilla/layers/ISurfaceAllocator.h"

namespace mozilla {
namespace layers {

class VideoBridgeParent final : public PVideoBridgeParent,
                                public HostIPCAllocator,
                                public ShmemAllocator
{
public:
  VideoBridgeParent();
  ~VideoBridgeParent();

  static VideoBridgeParent* GetSingleton();
  TextureHost* LookupTexture(uint64_t aSerial);

  // PVideoBridgeParent
  void ActorDestroy(ActorDestroyReason aWhy) override;
  PTextureParent* AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                      const LayersBackend& aLayersBackend,
                                      const TextureFlags& aFlags,
                                      const uint64_t& aSerial) override;
  bool DeallocPTextureParent(PTextureParent* actor) override;

  // HostIPCAllocator
  base::ProcessId GetChildProcessId() override
  {
    return OtherPid();
  }
  void NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId) override;
  void SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage) override;

  // ISurfaceAllocator
  ShmemAllocator* AsShmemAllocator() override { return this; }
  bool IsSameProcess() const override;
  bool IPCOpen() const override { return !mClosed; }

  // ShmemAllocator
  bool AllocShmem(size_t aSize,
                  ipc::SharedMemory::SharedMemoryType aType,
                  ipc::Shmem* aShmem) override;

  bool AllocUnsafeShmem(size_t aSize,
                        ipc::SharedMemory::SharedMemoryType aType,
                        ipc::Shmem* aShmem) override;

  void DeallocShmem(ipc::Shmem& aShmem) override;

private:
  void DeallocPVideoBridgeParent() override;

  // This keeps us alive until ActorDestroy(), at which point we do a
  // deferred destruction of ourselves.
  RefPtr<VideoBridgeParent> mSelfRef;

  std::map<uint64_t, PTextureParent*> mTextureMap;

  bool mClosed;
};

} // namespace layers
} // namespace mozilla

#endif // gfx_layers_ipc_VideoBridgeParent_h_
