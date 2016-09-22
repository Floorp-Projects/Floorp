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

APZCTreeManagerParent::APZCTreeManagerParent(uint64_t aLayersId, RefPtr<APZCTreeManager> aAPZCTreeManager)
  : mLayersId(aLayersId)
  , mTreeManager(aAPZCTreeManager)
{
  MOZ_ASSERT(aAPZCTreeManager != nullptr);
}

APZCTreeManagerParent::~APZCTreeManagerParent()
{
}

void
APZCTreeManagerParent::ChildAdopted(RefPtr<APZCTreeManager> aAPZCTreeManager)
{
  MOZ_ASSERT(aAPZCTreeManager != nullptr);
  mTreeManager = aAPZCTreeManager;
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
  if (aGuid.mLayersId != mLayersId) {
    // Guard against bad data from hijacked child processes
    NS_ERROR("Unexpected layers id in RecvZoomToRect; dropping message...");
    return false;
  }

  mTreeManager->ZoomToRect(aGuid, aRect, aFlags);
  return true;
}

bool
APZCTreeManagerParent::RecvContentReceivedInputBlock(
    const uint64_t& aInputBlockId,
    const bool& aPreventDefault)
{
  APZThreadUtils::RunOnControllerThread(
    NewRunnableMethod<uint64_t, bool>(mTreeManager,
      &IAPZCTreeManager::ContentReceivedInputBlock,
      aInputBlockId,
      aPreventDefault));

  return true;
}

bool
APZCTreeManagerParent::RecvSetTargetAPZC(
    const uint64_t& aInputBlockId,
    nsTArray<ScrollableLayerGuid>&& aTargets)
{
  for (size_t i = 0; i < aTargets.Length(); i++) {
    if (aTargets[i].mLayersId != mLayersId) {
      // Guard against bad data from hijacked child processes
      NS_ERROR("Unexpected layers id in RecvSetTargetAPZC; dropping message...");
      return false;
    }
  }
  APZThreadUtils::RunOnControllerThread(NewRunnableMethod
                                        <uint64_t,
                                         StoreCopyPassByRRef<nsTArray<ScrollableLayerGuid>>>
                                        (mTreeManager, &IAPZCTreeManager::SetTargetAPZC, aInputBlockId, aTargets));

  return true;
}

bool
APZCTreeManagerParent::RecvUpdateZoomConstraints(
    const ScrollableLayerGuid& aGuid,
    const MaybeZoomConstraints& aConstraints)
{
  if (aGuid.mLayersId != mLayersId) {
    // Guard against bad data from hijacked child processes
    NS_ERROR("Unexpected layers id in RecvUpdateZoomConstraints; dropping message...");
    return false;
  }

  mTreeManager->UpdateZoomConstraints(aGuid, aConstraints);
  return true;
}

bool
APZCTreeManagerParent::RecvCancelAnimation(const ScrollableLayerGuid& aGuid)
{
  if (aGuid.mLayersId != mLayersId) {
    // Guard against bad data from hijacked child processes
    NS_ERROR("Unexpected layers id in RecvCancelAnimation; dropping message...");
    return false;
  }

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
  APZThreadUtils::RunOnControllerThread(NewRunnableMethod
                                        <uint64_t,
                                         StoreCopyPassByRRef<nsTArray<TouchBehaviorFlags>>>
                                        (mTreeManager,
                                         &IAPZCTreeManager::SetAllowedTouchBehavior,
                                         aInputBlockId, Move(aValues)));

  return true;
}

bool
APZCTreeManagerParent::RecvStartScrollbarDrag(
    const ScrollableLayerGuid& aGuid,
    const AsyncDragMetrics& aDragMetrics)
{
  if (aGuid.mLayersId != mLayersId) {
    // Guard against bad data from hijacked child processes
    NS_ERROR("Unexpected layers id in RecvStartScrollbarDrag; dropping message...");
    return false;
  }

    APZThreadUtils::RunOnControllerThread(
        NewRunnableMethod<ScrollableLayerGuid, AsyncDragMetrics>(
          mTreeManager,
          &IAPZCTreeManager::StartScrollbarDrag,
          aGuid, aDragMetrics));

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
