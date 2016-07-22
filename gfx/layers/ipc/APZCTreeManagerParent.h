/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZCTreeManagerParent_h
#define mozilla_layers_APZCTreeManagerParent_h

#include "mozilla/layers/PAPZCTreeManagerParent.h"

namespace mozilla {
namespace layers {

class APZCTreeManager;

class APZCTreeManagerParent
    : public PAPZCTreeManagerParent
{
public:

  explicit APZCTreeManagerParent(RefPtr<APZCTreeManager> aAPZCTreeManager);
  virtual ~APZCTreeManagerParent() { }

  bool
  RecvReceiveMultiTouchInputEvent(
          const MultiTouchInput& aEvent,
          nsEventStatus* aOutStatus,
          MultiTouchInput* aOutEvent,
          ScrollableLayerGuid* aOutTargetGuid,
          uint64_t* aOutInputBlockId) override;

  bool
  RecvReceiveMouseInputEvent(
          const MouseInput& aEvent,
          nsEventStatus* aOutStatus,
          MouseInput* aOutEvent,
          ScrollableLayerGuid* aOutTargetGuid,
          uint64_t* aOutInputBlockId) override;

  bool
  RecvReceivePanGestureInputEvent(
          const PanGestureInput& aEvent,
          nsEventStatus* aOutStatus,
          PanGestureInput* aOutEvent,
          ScrollableLayerGuid* aOutTargetGuid,
          uint64_t* aOutInputBlockId) override;

  bool
  RecvReceivePinchGestureInputEvent(
          const PinchGestureInput& aEvent,
          nsEventStatus* aOutStatus,
          PinchGestureInput* aOutEvent,
          ScrollableLayerGuid* aOutTargetGuid,
          uint64_t* aOutInputBlockId) override;

  bool
  RecvReceiveTapGestureInputEvent(
          const TapGestureInput& aEvent,
          nsEventStatus* aOutStatus,
          TapGestureInput* aOutEvent,
          ScrollableLayerGuid* aOutTargetGuid,
          uint64_t* aOutInputBlockId) override;

  bool
  RecvReceiveScrollWheelInputEvent(
          const ScrollWheelInput& aEvent,
          nsEventStatus* aOutStatus,
          ScrollWheelInput* aOutEvent,
          ScrollableLayerGuid* aOutTargetGuid,
          uint64_t* aOutInputBlockId) override;

  bool
  RecvZoomToRect(
          const ScrollableLayerGuid& aGuid,
          const CSSRect& aRect,
          const uint32_t& aFlags) override;

  bool
  RecvContentReceivedInputBlock(
          const uint64_t& aInputBlockId,
          const bool& aPreventDefault) override;

  bool
  RecvSetTargetAPZC(
          const uint64_t& aInputBlockId,
          nsTArray<ScrollableLayerGuid>&& aTargets) override;

  bool
  RecvUpdateZoomConstraints(
          const ScrollableLayerGuid& aGuid,
          const MaybeZoomConstraints& aConstraints) override;

  bool
  RecvCancelAnimation(const ScrollableLayerGuid& aGuid) override;

  bool
  RecvAdjustScrollForSurfaceShift(const ScreenPoint& aShift) override;

  bool
  RecvSetDPI(const float& aDpiValue) override;

  bool
  RecvSetAllowedTouchBehavior(
          const uint64_t& aInputBlockId,
          nsTArray<TouchBehaviorFlags>&& aValues) override;

  bool
  RecvStartScrollbarDrag(
          const ScrollableLayerGuid& aGuid,
          const AsyncDragMetrics& aDragMetrics) override;

  bool
  RecvSetLongTapEnabled(const bool& aTapGestureEnabled) override;

  bool
  RecvProcessTouchVelocity(
          const uint32_t& aTimestampMs,
          const float& aSpeedY) override;

  bool
  RecvUpdateWheelTransaction(
          const LayoutDeviceIntPoint& aRefPoint,
          const EventMessage& aEventMessage) override;

  bool
  RecvTransformEventRefPoint(
          const LayoutDeviceIntPoint& aRefPoint,
          LayoutDeviceIntPoint* aOutRefPoint,
          ScrollableLayerGuid* aOutTargetGuid) override;

  void
  ActorDestroy(ActorDestroyReason aWhy) override { }

private:
  RefPtr<APZCTreeManager> mTreeManager;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_APZCTreeManagerParent_h
