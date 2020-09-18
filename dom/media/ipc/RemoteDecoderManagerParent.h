/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderManagerParent_h
#define include_dom_media_ipc_RemoteDecoderManagerParent_h

#include "GPUVideoImage.h"
#include "mozilla/PRemoteDecoderManagerParent.h"
#include "mozilla/layers/VideoBridgeChild.h"

namespace mozilla {

class RemoteDecoderManagerThreadHolder;

class RemoteDecoderManagerParent final
    : public PRemoteDecoderManagerParent,
      public layers::IGPUVideoSurfaceManager {
  friend class PRemoteDecoderManagerParent;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteDecoderManagerParent, override)

  static bool CreateForContent(
      Endpoint<PRemoteDecoderManagerParent>&& aEndpoint);

  static bool CreateVideoBridgeToOtherProcess(
      Endpoint<layers::PVideoBridgeChild>&& aEndpoint);

  // Must be called on manager thread.
  // Store the image so that it can be used out of process. Will be released
  // when DeallocateSurfaceDescriptor is called.
  void StoreImage(const SurfaceDescriptorGPUVideo& aSD, layers::Image* aImage,
                  layers::TextureClient* aTexture);

  // IGPUVideoSurfaceManager methods
  already_AddRefed<gfx::SourceSurface> Readback(
      const SurfaceDescriptorGPUVideo& aSD) override {
    MOZ_ASSERT_UNREACHABLE("Not usable from the parent");
    return nullptr;
  }
  void DeallocateSurfaceDescriptor(
      const SurfaceDescriptorGPUVideo& aSD) override;

  static bool StartupThreads();
  static void ShutdownThreads();

  static void ShutdownVideoBridge();

  bool OnManagerThread();

 protected:
  PRemoteDecoderParent* AllocPRemoteDecoderParent(
      const RemoteDecoderInfoIPDL& aRemoteDecoderInfo,
      const CreateDecoderParams::OptionSet& aOptions,
      const Maybe<layers::TextureFactoryIdentifier>& aIdentifier,
      bool* aSuccess, nsCString* aErrorDescription);
  bool DeallocPRemoteDecoderParent(PRemoteDecoderParent* actor);

  mozilla::ipc::IPCResult RecvReadback(const SurfaceDescriptorGPUVideo& aSD,
                                       SurfaceDescriptor* aResult);
  mozilla::ipc::IPCResult RecvDeallocateSurfaceDescriptorGPUVideo(
      const SurfaceDescriptorGPUVideo& aSD);

  void ActorDestroy(mozilla::ipc::IProtocol::ActorDestroyReason) override;
  void ActorDealloc() override;

 private:
  explicit RemoteDecoderManagerParent(nsISerialEventTarget* aThread);
  ~RemoteDecoderManagerParent();

  void Open(Endpoint<PRemoteDecoderManagerParent>&& aEndpoint);

  std::map<uint64_t, RefPtr<layers::Image>> mImageMap;
  std::map<uint64_t, RefPtr<layers::TextureClient>> mTextureMap;

  nsCOMPtr<nsISerialEventTarget> mThread;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderManagerParent_h
