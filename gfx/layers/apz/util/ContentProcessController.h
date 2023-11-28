/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_ContentProcessController_h
#define mozilla_layers_ContentProcessController_h

#include "mozilla/layers/GeckoContentController.h"

class nsIObserver;

namespace mozilla {

namespace dom {
class BrowserChild;
}  // namespace dom

namespace layers {

class APZChild;
struct DoubleTapToZoomMetrics;

/**
 * ContentProcessController is a GeckoContentController for a BrowserChild, and
 * is always remoted using PAPZ/APZChild.
 *
 * ContentProcessController is created in ContentChild when a layer tree id has
 * been allocated for a PBrowser that lives in that content process, and is
 * destroyed when the Destroy message is received, or when the tab dies.
 *
 * If ContentProcessController needs to implement a new method on
 * GeckoContentController PAPZ, APZChild, and RemoteContentController must be
 * updated to handle it.
 */
class ContentProcessController final : public GeckoContentController {
 public:
  explicit ContentProcessController(const RefPtr<dom::BrowserChild>& aBrowser);

  // GeckoContentController

  void NotifyLayerTransforms(nsTArray<MatrixMessage>&& aTransforms) override;

  void RequestContentRepaint(const RepaintRequest& aRequest) override;

  void HandleTap(TapType aType, const LayoutDevicePoint& aPoint,
                 Modifiers aModifiers, const ScrollableLayerGuid& aGuid,
                 uint64_t aInputBlockId,
                 const Maybe<DoubleTapToZoomMetrics>& aMetrics) override;

  void NotifyPinchGesture(PinchGestureInput::PinchGestureType aType,
                          const ScrollableLayerGuid& aGuid,
                          const LayoutDevicePoint& aFocusPoint,
                          LayoutDeviceCoord aSpanChange,
                          Modifiers aModifiers) override;

  void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                            APZStateChange aChange, int aArg,
                            Maybe<uint64_t> aInputBlockId) override;

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

  void NotifyScaleGestureComplete(const ScrollableLayerGuid& aGuid,
                                  float aScale) override;

  bool IsRepaintThread() override;

  void DispatchToRepaintThread(already_AddRefed<Runnable> aTask) override;

  PresShell* GetTopLevelPresShell() const override;

 private:
  RefPtr<dom::BrowserChild> mBrowser;
};

}  // namespace layers

}  // namespace mozilla

#endif  // mozilla_layers_ContentProcessController_h
