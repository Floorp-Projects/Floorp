/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_ipc_VideoDecoderManagerParent_h
#define include_dom_ipc_VideoDecoderManagerParent_h

#include "mozilla/dom/PVideoDecoderManagerParent.h"

namespace mozilla {
namespace dom {

class VideoDecoderManagerParent final : public PVideoDecoderManagerParent
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoDecoderManagerParent)

  static bool CreateForContent(Endpoint<PVideoDecoderManagerParent>&& aEndpoint);

  // Can be called from any thread
  SurfaceDescriptorGPUVideo StoreImage(layers::Image* aImage, layers::TextureClient* aTexture);

  static void StartupThreads();
  static void ShutdownThreads();

  bool OnManagerThread();

protected:
  PVideoDecoderParent* AllocPVideoDecoderParent() override;
  bool DeallocPVideoDecoderParent(PVideoDecoderParent* actor) override;

  bool RecvReadback(const SurfaceDescriptorGPUVideo& aSD, SurfaceDescriptor* aResult) override;
  bool RecvDeallocateSurfaceDescriptorGPUVideo(const SurfaceDescriptorGPUVideo& aSD) override;

  void ActorDestroy(mozilla::ipc::IProtocol::ActorDestroyReason) override {}

  void DeallocPVideoDecoderManagerParent() override;

 private:
  VideoDecoderManagerParent();
  ~VideoDecoderManagerParent();

  void Open(Endpoint<PVideoDecoderManagerParent>&& aEndpoint);

  std::map<uint64_t, RefPtr<layers::Image>> mImageMap;
  std::map<uint64_t, RefPtr<layers::TextureClient>> mTextureMap;
};

} // namespace dom
} // namespace mozilla

#endif // include_dom_ipc_VideoDecoderManagerParent_h
