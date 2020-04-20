/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderManagerChild_h
#define include_dom_media_ipc_RemoteDecoderManagerChild_h
#include "mozilla/PRemoteDecoderManagerChild.h"
#include "mozilla/layers/VideoBridgeUtils.h"
#include "GPUVideoImage.h"

namespace mozilla {

class RemoteDecoderManagerChild final
    : public PRemoteDecoderManagerChild,
      public mozilla::ipc::IShmemAllocator,
      public mozilla::layers::IGPUVideoSurfaceManager {
  friend class PRemoteDecoderManagerChild;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteDecoderManagerChild, override)

  // Can only be called from the manager thread
  static RemoteDecoderManagerChild* GetRDDProcessSingleton();
  static RemoteDecoderManagerChild* GetGPUProcessSingleton();

  // Can be called from any thread.
  static nsIThread* GetManagerThread();

  // Can be called from any thread, dispatches the request to the IPDL thread
  // internally and will be ignored if the IPDL actor has been destroyed.
  already_AddRefed<gfx::SourceSurface> Readback(
      const SurfaceDescriptorGPUVideo& aSD) override;
  void DeallocateSurfaceDescriptor(
      const SurfaceDescriptorGPUVideo& aSD) override;

  bool AllocShmem(size_t aSize,
                  mozilla::ipc::SharedMemory::SharedMemoryType aShmType,
                  mozilla::ipc::Shmem* aShmem) override {
    return PRemoteDecoderManagerChild::AllocShmem(aSize, aShmType, aShmem);
  }
  bool AllocUnsafeShmem(size_t aSize,
                        mozilla::ipc::SharedMemory::SharedMemoryType aShmType,
                        mozilla::ipc::Shmem* aShmem) override {
    return PRemoteDecoderManagerChild::AllocUnsafeShmem(aSize, aShmType,
                                                        aShmem);
  }

  // Can be called from any thread, dispatches the request to the IPDL thread
  // internally and will be ignored if the IPDL actor has been destroyed.
  bool DeallocShmem(mozilla::ipc::Shmem& aShmem) override;

  // Main thread only
  static void InitForRDDProcess(
      Endpoint<PRemoteDecoderManagerChild>&& aVideoManager);
  static void InitForGPUProcess(
      Endpoint<PRemoteDecoderManagerChild>&& aVideoManager);
  static void Shutdown();

  // Run aTask (on the manager thread) when we next attempt to create a new
  // manager (even if creation fails). Intended to be called from ActorDestroy
  // when we get notified that the old manager is being destroyed. Can only be
  // called from the manager thread.
  void RunWhenGPUProcessRecreated(already_AddRefed<Runnable> aTask);

  bool CanSend();
  layers::VideoBridgeSource GetSource() const { return mSource; }

 protected:
  void InitIPDL();

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void ActorDealloc() override;

  void HandleFatalError(const char* aMsg) const override;

  PRemoteDecoderChild* AllocPRemoteDecoderChild(
      const RemoteDecoderInfoIPDL& aRemoteDecoderInfo,
      const CreateDecoderParams::OptionSet& aOptions,
      const Maybe<layers::TextureFactoryIdentifier>& aIdentifier,
      bool* aSuccess, nsCString* aErrorDescription);
  bool DeallocPRemoteDecoderChild(PRemoteDecoderChild* actor);

 private:
  // Main thread only
  static void InitializeThread();

  explicit RemoteDecoderManagerChild(layers::VideoBridgeSource aSource);
  ~RemoteDecoderManagerChild() = default;

  static void OpenForRDDProcess(
      Endpoint<PRemoteDecoderManagerChild>&& aEndpoint);
  static void OpenForGPUProcess(
      Endpoint<PRemoteDecoderManagerChild>&& aEndpoint);

  RefPtr<RemoteDecoderManagerChild> mIPDLSelfRef;

  // The associated source of this decoder manager
  layers::VideoBridgeSource mSource;

  // Should only ever be accessed on the manager thread.
  bool mCanSend = false;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderManagerChild_h
