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
class TabChild;
} // namespace dom

namespace layers {

class APZChild;

/**
 * ContentProcessController is a GeckoContentController for a TabChild, and is always
 * remoted using PAPZ/APZChild.
 *
 * ContentProcessController is created in ContentChild when a layer tree id has
 * been allocated for a PBrowser that lives in that content process, and is destroyed
 * when the Destroy message is received, or when the tab dies.
 *
 * If ContentProcessController needs to implement a new method on GeckoContentController
 * PAPZ, APZChild, and RemoteContentController must be updated to handle it.
 */
class ContentProcessController final
      : public GeckoContentController
{
public:
  explicit ContentProcessController(const RefPtr<dom::TabChild>& aBrowser);

  // GeckoContentController

  void RequestContentRepaint(const FrameMetrics& frame) override;

  void HandleTap(TapType aType,
                 const LayoutDevicePoint& aPoint,
                 Modifiers aModifiers,
                 const ScrollableLayerGuid& aGuid,
                 uint64_t aInputBlockId) override;

  void NotifyPinchGesture(PinchGestureInput::PinchGestureType aType,
                          const ScrollableLayerGuid& aGuid,
                          LayoutDeviceCoord aSpanChange,
                          Modifiers aModifiers) override;

  void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                            APZStateChange aChange,
                            int aArg) override;

  void NotifyMozMouseScrollEvent(const FrameMetrics::ViewID& aScrollId,
                                 const nsString& aEvent) override;

  void NotifyFlushComplete() override;

  void NotifyAsyncScrollbarDragRejected(const FrameMetrics::ViewID& aScrollId) override;

  void NotifyAsyncAutoscrollRejected(const FrameMetrics::ViewID& aScrollId) override;

  void CancelAutoscroll(const ScrollableLayerGuid& aGuid) override;

  void PostDelayedTask(already_AddRefed<Runnable> aRunnable, int aDelayMs) override;

  bool IsRepaintThread() override;

  void DispatchToRepaintThread(already_AddRefed<Runnable> aTask) override;

private:
  RefPtr<dom::TabChild> mBrowser;
};

} // namespace layers

} // namespace mozilla

#endif // mozilla_layers_ContentProcessController_h
