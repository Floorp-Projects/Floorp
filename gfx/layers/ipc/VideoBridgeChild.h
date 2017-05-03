/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_VIDEOBRIDGECHILD_H
#define MOZILLA_GFX_VIDEOBRIDGECHILD_H

#include "mozilla/layers/PVideoBridgeChild.h"
#include "ISurfaceAllocator.h"
#include "TextureForwarder.h"

namespace mozilla {
namespace layers {

class VideoBridgeChild final : public PVideoBridgeChild
                             , public TextureForwarder
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoBridgeChild, override);

  static void Startup();
  static void Shutdown();

  static VideoBridgeChild* GetSingleton();

  // PVideoBridgeChild
  PTextureChild* AllocPTextureChild(const SurfaceDescriptor& aSharedData,
                                    const LayersBackend& aLayersBackend,
                                    const TextureFlags& aFlags,
                                    const uint64_t& aSerial) override;
  bool DeallocPTextureChild(PTextureChild* actor) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeallocPVideoBridgeChild() override;


  // ISurfaceAllocator
  bool AllocUnsafeShmem(size_t aSize,
                        mozilla::ipc::SharedMemory::SharedMemoryType aShmType,
                        mozilla::ipc::Shmem* aShmem) override;
  bool AllocShmem(size_t aSize,
                  mozilla::ipc::SharedMemory::SharedMemoryType aShmType,
                  mozilla::ipc::Shmem* aShmem) override;
  bool DeallocShmem(mozilla::ipc::Shmem& aShmem) override;

  // TextureForwarder
  PTextureChild* CreateTexture(const SurfaceDescriptor& aSharedData,
                               LayersBackend aLayersBackend,
                               TextureFlags aFlags,
                               uint64_t aSerial,
                               wr::MaybeExternalImageId& aExternalImageId) override;

  // ClientIPCAllocator
  base::ProcessId GetParentPid() const override { return OtherPid(); }
  MessageLoop * GetMessageLoop() const override { return mMessageLoop; }
  void CancelWaitForRecycle(uint64_t aTextureId) override { MOZ_ASSERT(false, "NO RECYCLING HERE"); }

  // ISurfaceAllocator
  bool IsSameProcess() const override;

  bool CanSend() { return mCanSend; }

private:
  VideoBridgeChild();
  ~VideoBridgeChild();

  RefPtr<VideoBridgeChild> mIPDLSelfRef;
  MessageLoop* mMessageLoop;
  bool mCanSend;
};

} // namespace layers
} // namespace mozilla

#endif
