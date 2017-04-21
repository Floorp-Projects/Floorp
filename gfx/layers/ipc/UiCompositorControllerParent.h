/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_gfx_ipc_UiCompositorControllerParent_h
#define include_gfx_ipc_UiCompositorControllerParent_h

#include "mozilla/layers/PUiCompositorControllerParent.h"
#if defined(MOZ_WIDGET_ANDROID)
#include "mozilla/layers/AndroidDynamicToolbarAnimator.h"
#endif // defined(MOZ_WIDGET_ANDROID)
#include "mozilla/ipc/Shmem.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace layers {

class UiCompositorControllerParent final : public PUiCompositorControllerParent
{
// UiCompositorControllerChild needs to call the private constructor when running in process.
friend class UiCompositorControllerChild;
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UiCompositorControllerParent)

  static RefPtr<UiCompositorControllerParent> GetFromRootLayerTreeId(const uint64_t& aRootLayerTreeId);
  static RefPtr<UiCompositorControllerParent> Start(const uint64_t& aRootLayerTreeId, Endpoint<PUiCompositorControllerParent>&& aEndpoint);

  // PUiCompositorControllerParent functions
  mozilla::ipc::IPCResult RecvPause() override;
  mozilla::ipc::IPCResult RecvResume() override;
  mozilla::ipc::IPCResult RecvResumeAndResize(const int32_t& aHeight,
                                              const int32_t& aWidth) override;
  mozilla::ipc::IPCResult RecvInvalidateAndRender() override;
  mozilla::ipc::IPCResult RecvMaxToolbarHeight(const int32_t& aHeight) override;
  mozilla::ipc::IPCResult RecvPinned(const bool& aPinned, const int32_t& aReason) override;
  mozilla::ipc::IPCResult RecvToolbarAnimatorMessageFromUI(const int32_t& aMessage) override;
  mozilla::ipc::IPCResult RecvDefaultClearColor(const uint32_t& aColor) override;
  mozilla::ipc::IPCResult RecvRequestScreenPixels() override;
  mozilla::ipc::IPCResult RecvEnableLayerUpdateNotifications(const bool& aEnable) override;
  mozilla::ipc::IPCResult RecvToolbarPixelsToCompositor(Shmem&& aMem, const ScreenIntSize& aSize) override;
  void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeallocPUiCompositorControllerParent() override;

  // Class specific functions
#if defined(MOZ_WIDGET_ANDROID)
  void RegisterAndroidDynamicToolbarAnimator(AndroidDynamicToolbarAnimator* aAnimator);
#endif // MOZ_WIDGET_ANDROID
  void ToolbarAnimatorMessageFromCompositor(int32_t aMessage);
  bool AllocPixelBuffer(const int32_t aSize, Shmem* aMem);

private:
  explicit UiCompositorControllerParent(const uint64_t& aRootLayerTreeId);
  ~UiCompositorControllerParent();
  void InitializeForSameProcess();
  void InitializeForOutOfProcess();
  void Initialize();
  void Open(Endpoint<PUiCompositorControllerParent>&& aEndpoint);
  void Shutdown();

  uint64_t mRootLayerTreeId;

#if defined(MOZ_WIDGET_ANDROID)
  RefPtr<AndroidDynamicToolbarAnimator> mAnimator;
#endif // defined(MOZ_WIDGET_ANDROID)

  int32_t mMaxToolbarHeight;
};

} // namespace layers
} // namespace mozilla

#endif // include_gfx_ipc_UiCompositorControllerParent_h
