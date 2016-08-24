/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZCTreeManagerParent.h"

#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/APZThreadUtils.h"

namespace mozilla {
namespace layers {

APZCTreeManagerParent::APZCTreeManagerParent(RefPtr<APZCTreeManager> aAPZCTreeManager)
  : mTreeManager(aAPZCTreeManager)
{
  MOZ_ASSERT(aAPZCTreeManager != nullptr);
}

bool
APZCTreeManagerParent::RecvReceiveMultiTouchInputEvent(
    const MultiTouchInput& aEvent,
    nsEventStatus* aOutStatus,
    MultiTouchInput* aOutEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  MultiTouchInput event = aEvent;

  *aOutStatus = mTreeManager->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return true;
}

bool
APZCTreeManagerParent::RecvReceiveMouseInputEvent(
    const MouseInput& aEvent,
    nsEventStatus* aOutStatus,
    MouseInput* aOutEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  MouseInput event = aEvent;

  *aOutStatus = mTreeManager->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return true;
}

bool
APZCTreeManagerParent::RecvReceivePanGestureInputEvent(
    const PanGestureInput& aEvent,
    nsEventStatus* aOutStatus,
    PanGestureInput* aOutEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  PanGestureInput event = aEvent;

  *aOutStatus = mTreeManager->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return true;
}

bool
APZCTreeManagerParent::RecvReceivePinchGestureInputEvent(
    const PinchGestureInput& aEvent,
    nsEventStatus* aOutStatus,
    PinchGestureInput* aOutEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  PinchGestureInput event = aEvent;

  *aOutStatus = mTreeManager->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return true;
}

bool
APZCTreeManagerParent::RecvReceiveTapGestureInputEvent(
    const TapGestureInput& aEvent,
    nsEventStatus* aOutStatus,
    TapGestureInput* aOutEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  TapGestureInput event = aEvent;

  *aOutStatus = mTreeManager->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return true;
}

bool
APZCTreeManagerParent::RecvReceiveScrollWheelInputEvent(
    const ScrollWheelInput& aEvent,
    nsEventStatus* aOutStatus,
    ScrollWheelInput* aOutEvent,
    ScrollableLayerGuid* aOutTargetGuid,
    uint64_t* aOutInputBlockId)
{
  ScrollWheelInput event = aEvent;

  *aOutStatus = mTreeManager->ReceiveInputEvent(
    event,
    aOutTargetGuid,
    aOutInputBlockId);
  *aOutEvent = event;

  return true;
}

bool
APZCTreeManagerParent::RecvZoomToRect(
    const ScrollableLayerGuid& aGuid,
    const CSSRect& aRect,
    const uint32_t& aFlags)
{
  mTreeManager->ZoomToRect(aGuid, aRect, aFlags);
  return true;
}

bool
APZCTreeManagerParent::RecvContentReceivedInputBlock(
    const uint64_t& aInputBlockId,
    const bool& aPreventDefault)
{
  mTreeManager->ContentReceivedInputBlock(aInputBlockId, aPreventDefault);
  return true;
}

bool
APZCTreeManagerParent::RecvSetTargetAPZC(
    const uint64_t& aInputBlockId,
    nsTArray<ScrollableLayerGuid>&& aTargets)
{
  mTreeManager->SetTargetAPZC(aInputBlockId, aTargets);
  return true;
}

bool
APZCTreeManagerParent::RecvUpdateZoomConstraints(
    const ScrollableLayerGuid& aGuid,
    const MaybeZoomConstraints& aConstraints)
{
  mTreeManager->UpdateZoomConstraints(aGuid, aConstraints);
  return true;
}

bool
APZCTreeManagerParent::RecvCancelAnimation(const ScrollableLayerGuid& aGuid)
{
  mTreeManager->CancelAnimation(aGuid);
  return true;
}

bool
APZCTreeManagerParent::RecvAdjustScrollForSurfaceShift(const ScreenPoint& aShift)
{
  mTreeManager->AdjustScrollForSurfaceShift(aShift);
  return true;
}

bool
APZCTreeManagerParent::RecvSetDPI(const float& aDpiValue)
{
  mTreeManager->SetDPI(aDpiValue);
  return true;
}

bool
APZCTreeManagerParent::RecvSetAllowedTouchBehavior(
    const uint64_t& aInputBlockId,
    nsTArray<TouchBehaviorFlags>&& aValues)
{
  mTreeManager->SetAllowedTouchBehavior(aInputBlockId, aValues);
  return true;
}

bool
APZCTreeManagerParent::RecvStartScrollbarDrag(
    const ScrollableLayerGuid& aGuid,
    const AsyncDragMetrics& aDragMetrics)
{
  mTreeManager->StartScrollbarDrag(aGuid, aDragMetrics);
  return true;
}

bool
APZCTreeManagerParent::RecvSetLongTapEnabled(const bool& aTapGestureEnabled)
{
  mTreeManager->SetLongTapEnabled(aTapGestureEnabled);
  return true;
}

bool
APZCTreeManagerParent::RecvProcessTouchVelocity(
  const uint32_t& aTimestampMs,
  const float& aSpeedY)
{
  mTreeManager->ProcessTouchVelocity(aTimestampMs, aSpeedY);
  return true;
}

bool
APZCTreeManagerParent::RecvUpdateWheelTransaction(
        const LayoutDeviceIntPoint& aRefPoint,
        const EventMessage& aEventMessage)
{
  mTreeManager->UpdateWheelTransaction(aRefPoint, aEventMessage);
  return true;
}

bool
APZCTreeManagerParent::RecvTransformEventRefPoint(
        const LayoutDeviceIntPoint& aRefPoint,
        LayoutDeviceIntPoint* aOutRefPoint,
        ScrollableLayerGuid* aOutTargetGuid)
{
  LayoutDeviceIntPoint refPoint = aRefPoint;
  mTreeManager->TransformEventRefPoint(&refPoint, aOutTargetGuid);
  *aOutRefPoint = refPoint;

  return true;
}

} // namespace layers
} // namespace mozilla
