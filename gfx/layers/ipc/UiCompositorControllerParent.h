/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_gfx_ipc_UiCompositorControllerParent_h
#define include_gfx_ipc_UiCompositorControllerParent_h

#include "mozilla/RefPtr.h"
#include "mozilla/layers/PUiCompositorControllerParent.h"

namespace mozilla {
namespace layers {

class UiCompositorControllerParent final : public PUiCompositorControllerParent
{
public:
  UiCompositorControllerParent();
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UiCompositorControllerParent)

  static RefPtr<UiCompositorControllerParent> Start(Endpoint<PUiCompositorControllerParent>&& aEndpoint);

  mozilla::ipc::IPCResult RecvPause(const uint64_t& aLayersId) override;
  mozilla::ipc::IPCResult RecvResume(const uint64_t& aLayersId) override;
  mozilla::ipc::IPCResult RecvResumeAndResize(const uint64_t& aLayersId,
                                              const int32_t& aHeight,
                                              const int32_t& aWidth) override;
  mozilla::ipc::IPCResult RecvInvalidateAndRender(const uint64_t& aLayersId) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeallocPUiCompositorControllerParent() override;

  void Shutdown();

private:
  ~UiCompositorControllerParent();

  void Open(Endpoint<PUiCompositorControllerParent>&& aEndpoint);
  void ShutdownImpl();

private:
};

} // namespace layers
} // namespace mozilla

#endif // include_gfx_ipc_UiCompositorControllerParent_h
