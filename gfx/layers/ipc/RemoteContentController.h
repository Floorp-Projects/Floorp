/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_RemoteContentController_h
#define mozilla_layers_RemoteContentController_h

#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/PAPZParent.h"

namespace mozilla {

namespace dom {
class BrowserParent;
}

namespace layers {

/**
 * RemoteContentController implements PAPZChild and is used to access a
 * GeckoContentController that lives in a different process.
 *
 * RemoteContentController lives on the compositor thread. All methods can
 * be called off the compositor thread and will get dispatched to the right
 * thread, with the exception of RequestContentRepaint and NotifyFlushComplete,
 * which must be called on the repaint thread, which in this case is the
 * compositor thread.
 */
class RemoteContentController : public GeckoContentController,
                                public PAPZParent {
  using GeckoContentController::APZStateChange;
  using GeckoContentController::TapType;

 public:
  RemoteContentController();

  virtual ~RemoteContentController();

  void NotifyLayerTransforms(
      const nsTArray<MatrixMessage>& aTransforms) override;

  void RequestContentRepaint(const RepaintRequest& aRequest) override;

  void HandleTap(TapType aTapType, const LayoutDevicePoint& aPoint,
                 Modifiers aModifiers, const ScrollableLayerGuid& aGuid,
                 uint64_t aInputBlockId) override;

  void NotifyPinchGesture(PinchGestureInput::PinchGestureType aType,
                          const ScrollableLayerGuid& aGuid,
                          LayoutDeviceCoord aSpanChange,
                          Modifiers aModifiers) override;

  void PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs) override;

  bool IsRepaintThread() override;

  void DispatchToRepaintThread(already_AddRefed<Runnable> aTask) override;

  void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                            APZStateChange aChange, int aArg) override;

  void UpdateOverscrollVelocity(float aX, float aY,
                                bool aIsRootContent) override;

  void UpdateOverscrollOffset(float aX, float aY, bool aIsRootContent) override;

  void NotifyMozMouseScrollEvent(const ScrollableLayerGuid::ViewID& aScrollId,
                                 const nsString& aEvent) override;

  void NotifyFlushComplete() override;

  void NotifyAsyncScrollbarDragInitiated(
      uint64_t aDragBlockId, const ScrollableLayerGuid::ViewID& aScrollId,
      ScrollDirection aDirection) override;
  void NotifyAsyncScrollbarDragRejected(
      const ScrollableLayerGuid::ViewID& aScrollId) override;

  void NotifyAsyncAutoscrollRejected(
      const ScrollableLayerGuid::ViewID& aScrollId) override;

  void CancelAutoscroll(const ScrollableLayerGuid& aScrollId) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  void Destroy() override;
  mozilla::ipc::IPCResult RecvDestroy();

  bool IsRemote() override;

 private:
  MessageLoop* mCompositorThread;
  bool mCanSend;

  void HandleTapOnMainThread(TapType aType, LayoutDevicePoint aPoint,
                             Modifiers aModifiers, ScrollableLayerGuid aGuid,
                             uint64_t aInputBlockId);
  void HandleTapOnCompositorThread(TapType aType, LayoutDevicePoint aPoint,
                                   Modifiers aModifiers,
                                   ScrollableLayerGuid aGuid,
                                   uint64_t aInputBlockId);
  void NotifyPinchGestureOnCompositorThread(
      PinchGestureInput::PinchGestureType aType,
      const ScrollableLayerGuid& aGuid, LayoutDeviceCoord aSpanChange,
      Modifiers aModifiers);

  void CancelAutoscrollInProcess(const ScrollableLayerGuid& aScrollId);
  void CancelAutoscrollCrossProcess(const ScrollableLayerGuid& aScrollId);
};

}  // namespace layers

}  // namespace mozilla

#endif  // mozilla_layers_RemoteContentController_h
