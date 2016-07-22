/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZCTreeManagerChild_h
#define mozilla_layers_APZCTreeManagerChild_h

#include "mozilla/layers/IAPZCTreeManager.h"
#include "mozilla/layers/PAPZCTreeManagerChild.h"

namespace mozilla {
namespace layers {


class APZCTreeManagerChild
  : public IAPZCTreeManager
  , public PAPZCTreeManagerChild
{
public:
  APZCTreeManagerChild() { }

  nsEventStatus
  ReceiveInputEvent(
          InputData& aEvent,
          ScrollableLayerGuid* aOutTargetGuid,
          uint64_t* aOutInputBlockId) override;

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
  CancelAnimation(const ScrollableLayerGuid &aGuid) override;

  void
  AdjustScrollForSurfaceShift(const ScreenPoint& aShift) override;

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

  void
  SetLongTapEnabled(bool aTapGestureEnabled) override;

  void
  ProcessTouchVelocity(uint32_t aTimestampMs, float aSpeedY) override;

  void
  TransformEventRefPoint(
          LayoutDeviceIntPoint* aRefPoint,
          ScrollableLayerGuid* aOutTargetGuid) override;

  void
  UpdateWheelTransaction(
          LayoutDeviceIntPoint aRefPoint,
          EventMessage aEventMessage) override;

  void
  OnProcessingError(
          Result aCode,
          const char* aReason) override;

protected:

  virtual
  ~APZCTreeManagerChild() { }
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_APZCTreeManagerChild_h
