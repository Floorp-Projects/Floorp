/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_ipc_VideoDecoderManagerChild_h
#define include_dom_ipc_VideoDecoderManagerChild_h

#include "mozilla/RefPtr.h"
#include "mozilla/dom/PVideoDecoderManagerChild.h"

namespace mozilla {
namespace dom {

class VideoDecoderManagerChild final : public PVideoDecoderManagerChild
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(VideoDecoderManagerChild)

  static VideoDecoderManagerChild* GetSingleton();
  static nsIThread* GetManagerThread();
  static AbstractThread* GetManagerAbstractThread();

  // Can be called from any thread, dispatches the request to the IPDL thread internally.
  void DeallocateSurfaceDescriptorGPUVideo(const SurfaceDescriptorGPUVideo& aSD);

  void DeallocPVideoDecoderManagerChild() override;

  void FatalError(const char* const aName, const char* const aMsg) const override;

  // Main thread only
  static void Initialize();
  static void Shutdown();

protected:
  PVideoDecoderChild* AllocPVideoDecoderChild() override;
  bool DeallocPVideoDecoderChild(PVideoDecoderChild* actor) override;

private:
  VideoDecoderManagerChild()
  {}
  ~VideoDecoderManagerChild() {}

  void Open(Endpoint<PVideoDecoderManagerChild>&& aEndpoint);
};

} // namespace dom
} // namespace mozilla

#endif // include_dom_ipc_VideoDecoderManagerChild_h
