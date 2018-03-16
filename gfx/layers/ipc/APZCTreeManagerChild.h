/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZCTreeManagerChild_h
#define mozilla_layers_APZCTreeManagerChild_h

#include "mozilla/layers/APZInputBridge.h"
#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/PAPZCTreeManagerChild.h"

namespace mozilla {
namespace layers {

class APZInputBridgeChild;
class RemoteCompositorSession;

class APZCTreeManagerChild
  : public IAPZCTreeManager
  , public PAPZCTreeManagerChild
{
public:
  APZCTreeManagerChild();

  void SetCompositorSession(RemoteCompositorSession* aSession);
  void SetInputBridge(APZInputBridgeChild* aInputBridge);
  void Destroy();

  void
  SetKeyboardMap(const KeyboardMap& aKeyboardMap) override;

  void
  ZoomToRect(
          const ScrollableLayerGuid& aGuid,
          const CSSRect& aRect,
          const uint32_t aFlags = DEFAULT_BEHAVIOR) override;

  void
  ContentReceivedInputBlock(
          uint64_t aInputBlockId,
          bool aPreventDefault) override;

  void
  SetTargetAPZC(
          uint64_t aInputBlockId,
          const nsTArray<ScrollableLayerGuid>& aTargets) override;

  void
  UpdateZoomConstraints(
          const ScrollableLayerGuid& aGuid,
          const Maybe<ZoomConstraints>& aConstraints) override;

  void
  SetDPI(float aDpiValue) override;

  void
  SetAllowedTouchBehavior(
          uint64_t aInputBlockId,
          const nsTArray<TouchBehaviorFlags>& aValues) override;

  void
  StartScrollbarDrag(
          const ScrollableLayerGuid& aGuid,
          const AsyncDragMetrics& aDragMetrics) override;

  bool
  StartAutoscroll(
          const ScrollableLayerGuid& aGuid,
          const ScreenPoint& aAnchorLocation) override;

  void
  StopAutoscroll(const ScrollableLayerGuid& aGuid) override;

  void
  SetLongTapEnabled(bool aTapGestureEnabled) override;

  APZInputBridge*
  InputBridge() override;

protected:
  mozilla::ipc::IPCResult RecvHandleTap(const TapType& aType,
                                        const LayoutDevicePoint& aPoint,
                                        const Modifiers& aModifiers,
                                        const ScrollableLayerGuid& aGuid,
                                        const uint64_t& aInputBlockId) override;

  mozilla::ipc::IPCResult RecvNotifyPinchGesture(const PinchGestureType& aType,
                                                 const ScrollableLayerGuid& aGuid,
                                                 const LayoutDeviceCoord& aSpanChange,
                                                 const Modifiers& aModifiers) override;

  mozilla::ipc::IPCResult RecvCancelAutoscroll(const FrameMetrics::ViewID& aScrollId) override;

  virtual ~APZCTreeManagerChild();

private:
  MOZ_NON_OWNING_REF RemoteCompositorSession* mCompositorSession;
  RefPtr<APZInputBridgeChild> mInputBridge;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_APZCTreeManagerChild_h
