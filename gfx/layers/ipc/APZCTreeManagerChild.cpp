/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZCTreeManagerChild.h"

#include "InputData.h"                  // for InputData

namespace mozilla {
namespace layers {

nsEventStatus
APZCTreeManagerChild::ReceiveInputEvent(
    InputData& aEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  switch (aEvent.mInputType) {
  case MULTITOUCH_INPUT: {
    MultiTouchInput& event = aEvent.AsMultiTouchInput();
    MultiTouchInput processedEvent;

    nsEventStatus res;
    SendReceiveMultiTouchInputEvent(event,
                                    &res,
                                    &processedEvent,
                                    aOutTargetGuid,
                                    aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  case MOUSE_INPUT: {
    MouseInput& event = aEvent.AsMouseInput();
    MouseInput processedEvent;

    nsEventStatus res;
    SendReceiveMouseInputEvent(event,
                               &res,
                               &processedEvent,
                               aOutTargetGuid,
                               aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  case PANGESTURE_INPUT: {
    PanGestureInput& event = aEvent.AsPanGestureInput();
    PanGestureInput processedEvent;

    nsEventStatus res;
    SendReceivePanGestureInputEvent(event,
                                    &res,
                                    &processedEvent,
                                    aOutTargetGuid,
                                    aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  case PINCHGESTURE_INPUT: {
    PinchGestureInput& event = aEvent.AsPinchGestureInput();
    PinchGestureInput processedEvent;

    nsEventStatus res;
    SendReceivePinchGestureInputEvent(event,
                                      &res,
                                      &processedEvent,
                                      aOutTargetGuid,
                                      aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  case TAPGESTURE_INPUT: {
    TapGestureInput& event = aEvent.AsTapGestureInput();
    TapGestureInput processedEvent;

    nsEventStatus res;
    SendReceiveTapGestureInputEvent(event,
                                    &res,
                                    &processedEvent,
                                    aOutTargetGuid,
                                    aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  case SCROLLWHEEL_INPUT: {
    ScrollWheelInput& event = aEvent.AsScrollWheelInput();
    ScrollWheelInput processedEvent;

    nsEventStatus res;
    SendReceiveScrollWheelInputEvent(event,
                                     &res,
                                     &processedEvent,
                                     aOutTargetGuid,
                                     aOutInputBlockId);

    event = processedEvent;
    return res;
  }
  default: {
    MOZ_ASSERT_UNREACHABLE("Invalid InputData type.");
    return nsEventStatus_eConsumeNoDefault;
  }
  }
}

void
APZCTreeManagerChild::ZoomToRect(
    const ScrollableLayerGuid& aGuid,
    const CSSRect& aRect,
    const uint32_t aFlags)
{
  SendZoomToRect(aGuid, aRect, aFlags);
}

void
APZCTreeManagerChild::ContentReceivedInputBlock(
    uint64_t aInputBlockId,
    bool aPreventDefault)
{
  SendContentReceivedInputBlock(aInputBlockId, aPreventDefault);
}

void
APZCTreeManagerChild::SetTargetAPZC(
    uint64_t aInputBlockId,
    const nsTArray<ScrollableLayerGuid>& aTargets)
{
  SendSetTargetAPZC(aInputBlockId, aTargets);
}

void
APZCTreeManagerChild::UpdateZoomConstraints(
    const ScrollableLayerGuid& aGuid,
    const Maybe<ZoomConstraints>& aConstraints)
{
  SendUpdateZoomConstraints(aGuid, aConstraints);
}

void
APZCTreeManagerChild::CancelAnimation(const ScrollableLayerGuid &aGuid)
{
  SendCancelAnimation(aGuid);
}

void
APZCTreeManagerChild::AdjustScrollForSurfaceShift(const ScreenPoint& aShift)
{
  SendAdjustScrollForSurfaceShift(aShift);
}

void
APZCTreeManagerChild::SetDPI(float aDpiValue)
{
  SendSetDPI(aDpiValue);
}

void
APZCTreeManagerChild::SetAllowedTouchBehavior(
    uint64_t aInputBlockId,
    const nsTArray<TouchBehaviorFlags>& aValues)
{
  SendSetAllowedTouchBehavior(aInputBlockId, aValues);
}

void
APZCTreeManagerChild::StartScrollbarDrag(
    const ScrollableLayerGuid& aGuid,
    const AsyncDragMetrics& aDragMetrics)
{
  SendStartScrollbarDrag(aGuid, aDragMetrics);
}

void
APZCTreeManagerChild::SetLongTapEnabled(bool aTapGestureEnabled)
{
  SendSetLongTapEnabled(aTapGestureEnabled);
}

void
APZCTreeManagerChild::ProcessTouchVelocity(uint32_t aTimestampMs, float aSpeedY)
{
  SendProcessTouchVelocity(aTimestampMs, aSpeedY);
}

void
APZCTreeManagerChild::UpdateWheelTransaction(
    LayoutDeviceIntPoint aRefPoint,
    EventMessage aEventMessage)
{
  SendUpdateWheelTransaction(aRefPoint, aEventMessage);
}

void APZCTreeManagerChild::TransformEventRefPoint(
    LayoutDeviceIntPoint* aRefPoint,
    ScrollableLayerGuid* aOutTargetGuid)
{
  SendTransformEventRefPoint(*aRefPoint, aRefPoint, aOutTargetGuid);
}

void
APZCTreeManagerChild::OnProcessingError(
        Result aCode,
        const char* aReason)
{
  MOZ_RELEASE_ASSERT(aCode != MsgDropped);
}

} // namespace layers
} // namespace mozilla
