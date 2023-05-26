/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_gfx_ipc_UiCompositorControllerChild_h
#define include_gfx_ipc_UiCompositorControllerChild_h

#include "mozilla/layers/PUiCompositorControllerChild.h"

#include "mozilla/gfx/2D.h"
#include "mozilla/Maybe.h"
#include "mozilla/ipc/Shmem.h"
#include "mozilla/layers/UiCompositorControllerParent.h"
#include "mozilla/RefPtr.h"
#include "nsThread.h"
#ifdef MOZ_WIDGET_ANDROID
#  include "SurfaceTexture.h"
#  include "mozilla/java/CompositorSurfaceManagerWrappers.h"
#endif

class nsBaseWidget;

namespace mozilla {
namespace layers {

class UiCompositorControllerChild final
    : protected PUiCompositorControllerChild {
  friend class PUiCompositorControllerChild;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UiCompositorControllerChild, final)

  static RefPtr<UiCompositorControllerChild> CreateForSameProcess(
      const LayersId& aRootLayerTreeId, nsBaseWidget* aWidget);
  static RefPtr<UiCompositorControllerChild> CreateForGPUProcess(
      const uint64_t& aProcessToken,
      Endpoint<PUiCompositorControllerChild>&& aEndpoint,
      nsBaseWidget* aWidget);

  bool Pause();
  bool Resume();
  bool ResumeAndResize(const int32_t& aX, const int32_t& aY,
                       const int32_t& aHeight, const int32_t& aWidth);
  bool InvalidateAndRender();
  bool SetMaxToolbarHeight(const int32_t& aHeight);
  bool SetFixedBottomOffset(int32_t aOffset);
  bool ToolbarAnimatorMessageFromUI(const int32_t& aMessage);
  bool SetDefaultClearColor(const uint32_t& aColor);
  bool RequestScreenPixels();
  bool EnableLayerUpdateNotifications(const bool& aEnable);

  void Destroy();

  bool DeallocPixelBuffer(Shmem& aMem);

#ifdef MOZ_WIDGET_ANDROID
  // Set mCompositorSurfaceManager. Must be called straight after initialization
  // for GPU process controllers. Do not call for in-process controllers. This
  // is separate from CreateForGPUProcess to avoid cluttering its declaration
  // with JNI types.
  void SetCompositorSurfaceManager(
      java::CompositorSurfaceManager::Param aCompositorSurfaceManager);

  // Send a Surface to the GPU process that a given widget ID should be
  // composited in to. If not using a GPU process this function does nothing, as
  // the InProcessCompositorWidget can read the Surface directly from the
  // widget.
  //
  // Note that this function does not actually use the PUiCompositorController
  // IPDL protocol, and instead uses Android's binder IPC mechanism via
  // mCompositorSurfaceManager. It can be called from any thread.
  void OnCompositorSurfaceChanged(int32_t aWidgetId,
                                  java::sdk::Surface::Param aSurface);
#endif

 protected:
  void ActorDestroy(ActorDestroyReason aWhy) override;
  void ProcessingError(Result aCode, const char* aReason) override;
  void HandleFatalError(const char* aMsg) override;
  mozilla::ipc::IPCResult RecvToolbarAnimatorMessageFromCompositor(
      const int32_t& aMessage);
  mozilla::ipc::IPCResult RecvRootFrameMetrics(const ScreenPoint& aScrollOffset,
                                               const CSSToScreenScale& aZoom);
  mozilla::ipc::IPCResult RecvScreenPixels(Shmem&& aMem,
                                           const ScreenIntSize& aSize,
                                           bool aNeedsYFlip);

 private:
  explicit UiCompositorControllerChild(const uint64_t& aProcessToken,
                                       nsBaseWidget* aWidget);
  virtual ~UiCompositorControllerChild();
  void OpenForSameProcess();
  void OpenForGPUProcess(Endpoint<PUiCompositorControllerChild>&& aEndpoint);
  void SendCachedValues();

  bool mIsOpen;
  uint64_t mProcessToken;
  Maybe<gfx::IntRect> mResize;
  Maybe<int32_t> mMaxToolbarHeight;
  Maybe<uint32_t> mDefaultClearColor;
  Maybe<bool> mLayerUpdateEnabled;
  RefPtr<nsBaseWidget> mWidget;
  // Should only be set when compositor is in process.
  RefPtr<UiCompositorControllerParent> mParent;

#ifdef MOZ_WIDGET_ANDROID
  // Android interface to send Surfaces to the GPU process. This uses Android
  // binder rather than IPDL because Surfaces cannot be sent via IPDL. It lives
  // here regardless because it is a conceptually logical location, even if the
  // underlying IPC mechanism is different.
  // This will be null if there is no GPU process.
  mozilla::java::CompositorSurfaceManager::GlobalRef mCompositorSurfaceManager;
#endif
};

}  // namespace layers
}  // namespace mozilla

#endif  // include_gfx_ipc_UiCompositorControllerChild_h
