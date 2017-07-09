/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
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

class nsBaseWidget;

namespace mozilla {
namespace layers {

class UiCompositorControllerChild final : protected PUiCompositorControllerChild
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(UiCompositorControllerChild)

  static RefPtr<UiCompositorControllerChild> CreateForSameProcess(const int64_t& aRootLayerTreeId);
  static RefPtr<UiCompositorControllerChild> CreateForGPUProcess(const uint64_t& aProcessToken,
                                                                 Endpoint<PUiCompositorControllerChild>&& aEndpoint);

  bool Pause();
  bool Resume();
  bool ResumeAndResize(const int32_t& aHeight, const int32_t& aWidth);
  bool InvalidateAndRender();
  bool SetMaxToolbarHeight(const int32_t& aHeight);
  bool SetPinned(const bool& aPinned, const int32_t& aReason);
  bool ToolbarAnimatorMessageFromUI(const int32_t& aMessage);
  bool SetDefaultClearColor(const uint32_t& aColor);
  bool RequestScreenPixels();
  bool EnableLayerUpdateNotifications(const bool& aEnable);
  bool ToolbarPixelsToCompositor(Shmem& aMem, const ScreenIntSize& aSize);

  void Destroy();

  void SetBaseWidget(nsBaseWidget* aWidget);
  bool AllocPixelBuffer(const int32_t aSize, Shmem* aMem);
  bool DeallocPixelBuffer(Shmem& aMem);

protected:
  void ActorDestroy(ActorDestroyReason aWhy) override;
  void DeallocPUiCompositorControllerChild() override;
  void ProcessingError(Result aCode, const char* aReason) override;
  virtual void HandleFatalError(const char* aName, const char* aMsg) const override;
  mozilla::ipc::IPCResult RecvToolbarAnimatorMessageFromCompositor(const int32_t& aMessage) override;
  mozilla::ipc::IPCResult RecvRootFrameMetrics(const ScreenPoint& aScrollOffset, const CSSToScreenScale& aZoom) override;
  mozilla::ipc::IPCResult RecvScreenPixels(ipc::Shmem&& aMem, const ScreenIntSize& aSize) override;
private:
  explicit UiCompositorControllerChild(const uint64_t& aProcessToken);
  ~UiCompositorControllerChild();
  void OpenForSameProcess();
  void OpenForGPUProcess(Endpoint<PUiCompositorControllerChild>&& aEndpoint);
  void SendCachedValues();

  bool mIsOpen;
  uint64_t mProcessToken;
  Maybe<gfx::IntSize> mResize;
  Maybe<int32_t> mMaxToolbarHeight;
  Maybe<uint32_t> mDefaultClearColor;
  Maybe<bool> mLayerUpdateEnabled;
  nsBaseWidget* mWidget;
  // Should only be set when compositor is in process.
  RefPtr<UiCompositorControllerParent> mParent;
};

} // namespace layers
} // namespace mozilla

#endif // include_gfx_ipc_UiCompositorControllerChild_h
