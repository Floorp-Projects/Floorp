/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_gfx_ipc_UiCompositorControllerParent_h
#define include_gfx_ipc_UiCompositorControllerParent_h

#include "mozilla/layers/PUiCompositorControllerParent.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
namespace layers {

struct FrameMetrics;

class UiCompositorControllerParent final
    : public PUiCompositorControllerParent {
  // UiCompositorControllerChild needs to call the private constructor when
  // running in process.
  friend class UiCompositorControllerChild;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UiCompositorControllerParent)

  static RefPtr<UiCompositorControllerParent> GetFromRootLayerTreeId(
      const LayersId& aRootLayerTreeId);
  static RefPtr<UiCompositorControllerParent> Start(
      const LayersId& aRootLayerTreeId,
      Endpoint<PUiCompositorControllerParent>&& aEndpoint);

  // PUiCompositorControllerParent functions
  mozilla::ipc::IPCResult RecvPause();
  mozilla::ipc::IPCResult RecvResume();
  mozilla::ipc::IPCResult RecvResumeAndResize(const int32_t& aX,
                                              const int32_t& aY,
                                              const int32_t& aHeight,
                                              const int32_t& aWidth);
  mozilla::ipc::IPCResult RecvInvalidateAndRender();
  mozilla::ipc::IPCResult RecvMaxToolbarHeight(const int32_t& aHeight);
  mozilla::ipc::IPCResult RecvFixedBottomOffset(const int32_t& aOffset);
  mozilla::ipc::IPCResult RecvDefaultClearColor(const uint32_t& aColor);
  mozilla::ipc::IPCResult RecvRequestScreenPixels();
  mozilla::ipc::IPCResult RecvEnableLayerUpdateNotifications(
      const bool& aEnable);
  void ActorDestroy(ActorDestroyReason aWhy) override;
  void ActorDealloc() override;

  // Class specific functions
  void ToolbarAnimatorMessageFromCompositor(int32_t aMessage);
  bool AllocPixelBuffer(const int32_t aSize, Shmem* aMem);

  // Called when a layer has been updated so the UI thread may be notified if
  // necessary.
  void NotifyLayersUpdated();
  void NotifyFirstPaint();
  void NotifyUpdateScreenMetrics(const FrameMetrics& aMetrics);

 private:
  explicit UiCompositorControllerParent(const LayersId& aRootLayerTreeId);
  virtual ~UiCompositorControllerParent();
  void InitializeForSameProcess();
  void InitializeForOutOfProcess();
  void Initialize();
  void Open(Endpoint<PUiCompositorControllerParent>&& aEndpoint);
  void Shutdown();

  LayersId mRootLayerTreeId;

#if defined(MOZ_WIDGET_ANDROID)
  bool mCompositorLayersUpdateEnabled;  // Flag set to true when the UI thread
                                        // is expecting to be notified when a
                                        // layer has been updated
#endif                                  // defined(MOZ_WIDGET_ANDROID)

  int32_t mMaxToolbarHeight;
};

}  // namespace layers
}  // namespace mozilla

#endif  // include_gfx_ipc_UiCompositorControllerParent_h
