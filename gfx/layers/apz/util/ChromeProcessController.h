/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ChromeProcessController_h
#define mozilla_layers_ChromeProcessController_h

#include "mozilla/layers/GeckoContentController.h"
#include "nsCOMPtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/layers/MatrixMessage.h"

class nsIDOMWindowUtils;

class nsIWidget;
class MessageLoop;

namespace mozilla {
class PresShell;
namespace dom {
class Document;
}

namespace layers {

class IAPZCTreeManager;
class APZEventState;

/**
 * ChromeProcessController is a GeckoContentController attached to the root of
 * a compositor's layer tree. It's used directly by APZ by default, and remoted
 * using PAPZ if there is a gpu process.
 *
 * If ChromeProcessController needs to implement a new method on
 * GeckoContentController PAPZ, APZChild, and RemoteContentController must be
 * updated to handle it.
 */
class ChromeProcessController : public mozilla::layers::GeckoContentController {
 protected:
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;

 public:
  explicit ChromeProcessController(nsIWidget* aWidget,
                                   APZEventState* aAPZEventState,
                                   IAPZCTreeManager* aAPZCTreeManager);
  virtual ~ChromeProcessController();
  void Destroy() override;

  // GeckoContentController interface
  void NotifyLayerTransforms(
      const nsTArray<MatrixMessage>& aTransforms) override;
  void RequestContentRepaint(const RepaintRequest& aRequest) override;
  void PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs) override;
  bool IsRepaintThread() override;
  void DispatchToRepaintThread(already_AddRefed<Runnable> aTask) override;
  MOZ_CAN_RUN_SCRIPT
  void HandleTap(TapType aType, const mozilla::LayoutDevicePoint& aPoint,
                 Modifiers aModifiers, const ScrollableLayerGuid& aGuid,
                 uint64_t aInputBlockId) override;
  void NotifyPinchGesture(PinchGestureInput::PinchGestureType aType,
                          const ScrollableLayerGuid& aGuid,
                          LayoutDeviceCoord aSpanChange,
                          Modifiers aModifiers) override;
  void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                            APZStateChange aChange, int aArg) override;
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
  void CancelAutoscroll(const ScrollableLayerGuid& aGuid) override;

 private:
  nsCOMPtr<nsIWidget> mWidget;
  RefPtr<APZEventState> mAPZEventState;
  RefPtr<IAPZCTreeManager> mAPZCTreeManager;
  MessageLoop* mUILoop;

  void InitializeRoot();
  PresShell* GetPresShell() const;
  dom::Document* GetRootDocument() const;
  dom::Document* GetRootContentDocument(
      const ScrollableLayerGuid::ViewID& aScrollId) const;
  void HandleDoubleTap(const mozilla::CSSPoint& aPoint, Modifiers aModifiers,
                       const ScrollableLayerGuid& aGuid);
};

}  // namespace layers
}  // namespace mozilla

#endif /* mozilla_layers_ChromeProcessController_h */
