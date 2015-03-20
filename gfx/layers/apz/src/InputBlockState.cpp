/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputBlockState.h"
#include "mozilla/layers/APZCTreeManager.h" // for AllowedTouchBehavior
#include "AsyncPanZoomController.h"         // for AsyncPanZoomController
#include "gfxPrefs.h"                       // for gfxPrefs
#include "OverscrollHandoffState.h"

#define TBS_LOG(...)
// #define TBS_LOG(...) printf_stderr("TBS: " __VA_ARGS__)

namespace mozilla {
namespace layers {

static uint64_t sBlockCounter = InputBlockState::NO_BLOCK_ID + 1;

InputBlockState::InputBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                                 bool aTargetConfirmed)
  : mTargetApzc(aTargetApzc)
  , mTargetConfirmed(aTargetConfirmed)
  , mBlockId(sBlockCounter++)
  , mTransformToApzc(aTargetApzc->GetTransformToThis())
{
  // We should never be constructed with a nullptr target.
  MOZ_ASSERT(mTargetApzc);
  mOverscrollHandoffChain = mTargetApzc->BuildOverscrollHandoffChain();
}

bool
InputBlockState::SetConfirmedTargetApzc(const nsRefPtr<AsyncPanZoomController>& aTargetApzc)
{
  if (mTargetConfirmed) {
    return false;
  }
  mTargetConfirmed = true;

  TBS_LOG("%p got confirmed target APZC %p\n", this, mTargetApzc.get());
  if (mTargetApzc == aTargetApzc) {
    // The confirmed target is the same as the tentative one, so we're done.
    return true;
  }

  // Log enabled by default for now, we will put it in a TBS_LOG eventually
  // once this code is more baked
  printf_stderr("%p replacing unconfirmed target %p with real target %p\n",
      this, mTargetApzc.get(), aTargetApzc.get());

  // note that aTargetApzc MAY be null here.
  mTargetApzc = aTargetApzc;
  mTransformToApzc = aTargetApzc ? aTargetApzc->GetTransformToThis() : gfx::Matrix4x4();
  mOverscrollHandoffChain = (mTargetApzc ? mTargetApzc->BuildOverscrollHandoffChain() : nullptr);
  return true;
}

const nsRefPtr<AsyncPanZoomController>&
InputBlockState::GetTargetApzc() const
{
  return mTargetApzc;
}

const nsRefPtr<const OverscrollHandoffChain>&
InputBlockState::GetOverscrollHandoffChain() const
{
  return mOverscrollHandoffChain;
}

uint64_t
InputBlockState::GetBlockId() const
{
  return mBlockId;
}

bool
InputBlockState::IsTargetConfirmed() const
{
  return mTargetConfirmed;
}

CancelableBlockState::CancelableBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                                           bool aTargetConfirmed)
  : InputBlockState(aTargetApzc, aTargetConfirmed)
  , mPreventDefault(false)
  , mContentResponded(false)
  , mContentResponseTimerExpired(false)
{
}

bool
CancelableBlockState::SetContentResponse(bool aPreventDefault)
{
  if (mContentResponded) {
    return false;
  }
  TBS_LOG("%p got content response %d with timer expired %d\n",
    this, aPreventDefault, mContentResponseTimerExpired);
  if (!mContentResponseTimerExpired) {
    mPreventDefault = aPreventDefault;
  }
  mContentResponded = true;
  return true;
}

bool
CancelableBlockState::TimeoutContentResponse()
{
  if (mContentResponseTimerExpired) {
    return false;
  }
  TBS_LOG("%p got content timer expired with response received %d\n",
    this, mContentResponded);
  if (!mContentResponded) {
    mPreventDefault = false;
  }
  mContentResponseTimerExpired = true;
  return true;
}

bool
CancelableBlockState::IsDefaultPrevented() const
{
  MOZ_ASSERT(mContentResponded || mContentResponseTimerExpired);
  return mPreventDefault;
}

bool
CancelableBlockState::IsReadyForHandling() const
{
  if (!IsTargetConfirmed())
    return false;
  return mContentResponded || mContentResponseTimerExpired;
}

void
CancelableBlockState::DispatchImmediate(const InputData& aEvent) const
{
  MOZ_ASSERT(!HasEvents());
  MOZ_ASSERT(GetTargetApzc());
  GetTargetApzc()->HandleInputEvent(aEvent, mTransformToApzc);
}

WheelBlockState::WheelBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                                 bool aTargetConfirmed)
  : CancelableBlockState(aTargetApzc, aTargetConfirmed)
{
}

void
WheelBlockState::AddEvent(const ScrollWheelInput& aEvent)
{
  mEvents.AppendElement(aEvent);
}

bool
WheelBlockState::IsReadyForHandling() const
{
  if (!CancelableBlockState::IsReadyForHandling()) {
    return false;
  }
  return true;
}

bool
WheelBlockState::HasEvents() const
{
  return !mEvents.IsEmpty();
}

void
WheelBlockState::DropEvents()
{
  TBS_LOG("%p dropping %lu events\n", this, mEvents.Length());
  mEvents.Clear();
}

void
WheelBlockState::HandleEvents()
{
  while (HasEvents()) {
    TBS_LOG("%p returning first of %lu events\n", this, mEvents.Length());
    ScrollWheelInput event = mEvents[0];
    mEvents.RemoveElementAt(0);
    GetTargetApzc()->HandleInputEvent(event, mTransformToApzc);
  }
}

bool
WheelBlockState::MustStayActive()
{
  return false;
}

const char*
WheelBlockState::Type()
{
  return "scroll wheel";
}

TouchBlockState::TouchBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                                 bool aTargetConfirmed)
  : CancelableBlockState(aTargetApzc, aTargetConfirmed)
  , mAllowedTouchBehaviorSet(false)
  , mDuringFastMotion(false)
  , mSingleTapOccurred(false)
{
  TBS_LOG("Creating %p\n", this);
}

bool
TouchBlockState::SetAllowedTouchBehaviors(const nsTArray<TouchBehaviorFlags>& aBehaviors)
{
  if (mAllowedTouchBehaviorSet) {
    return false;
  }
  TBS_LOG("%p got allowed touch behaviours for %lu points\n", this, aBehaviors.Length());
  mAllowedTouchBehaviors.AppendElements(aBehaviors);
  mAllowedTouchBehaviorSet = true;
  return true;
}

bool
TouchBlockState::GetAllowedTouchBehaviors(nsTArray<TouchBehaviorFlags>& aOutBehaviors) const
{
  if (!mAllowedTouchBehaviorSet) {
    return false;
  }
  aOutBehaviors.AppendElements(mAllowedTouchBehaviors);
  return true;
}

void
TouchBlockState::CopyPropertiesFrom(const TouchBlockState& aOther)
{
  TBS_LOG("%p copying properties from %p\n", this, &aOther);
  if (gfxPrefs::TouchActionEnabled()) {
    MOZ_ASSERT(aOther.mAllowedTouchBehaviorSet);
    SetAllowedTouchBehaviors(aOther.mAllowedTouchBehaviors);
  }
  mTransformToApzc = aOther.mTransformToApzc;
}

bool
TouchBlockState::IsReadyForHandling() const
{
  if (!CancelableBlockState::IsReadyForHandling()) {
    return false;
  }

  // TODO: for long-tap blocks we probably don't need the touch behaviour?
  if (gfxPrefs::TouchActionEnabled() && !mAllowedTouchBehaviorSet) {
    return false;
  }
  return true;
}

void
TouchBlockState::SetDuringFastMotion()
{
  TBS_LOG("%p setting fast-motion flag\n", this);
  mDuringFastMotion = true;
}

bool
TouchBlockState::IsDuringFastMotion() const
{
  return mDuringFastMotion;
}

bool
TouchBlockState::SetSingleTapOccurred()
{
  TBS_LOG("%p attempting to set single-tap occurred; disallowed=%d\n",
    this, mDuringFastMotion);
  if (!mDuringFastMotion) {
    mSingleTapOccurred = true;
    return true;
  }
  return false;
}

bool
TouchBlockState::SingleTapOccurred() const
{
  return mSingleTapOccurred;
}

bool
TouchBlockState::HasEvents() const
{
  return !mEvents.IsEmpty();
}

void
TouchBlockState::AddEvent(const MultiTouchInput& aEvent)
{
  TBS_LOG("%p adding event of type %d\n", this, aEvent.mType);
  mEvents.AppendElement(aEvent);
}

bool
TouchBlockState::MustStayActive()
{
  return true;
}

const char*
TouchBlockState::Type()
{
  return "touch";
}

void
TouchBlockState::DropEvents()
{
  TBS_LOG("%p dropping %lu events\n", this, mEvents.Length());
  mEvents.Clear();
}

void
TouchBlockState::HandleEvents()
{
  while (HasEvents()) {
    TBS_LOG("%p returning first of %lu events\n", this, mEvents.Length());
    MultiTouchInput event = mEvents[0];
    mEvents.RemoveElementAt(0);
    GetTargetApzc()->HandleInputEvent(event, mTransformToApzc);
  }
}

bool
TouchBlockState::TouchActionAllowsPinchZoom() const
{
  if (!gfxPrefs::TouchActionEnabled()) {
    return true;
  }
  // Pointer events specification requires that all touch points allow zoom.
  for (size_t i = 0; i < mAllowedTouchBehaviors.Length(); i++) {
    if (!(mAllowedTouchBehaviors[i] & AllowedTouchBehavior::PINCH_ZOOM)) {
      return false;
    }
  }
  return true;
}

bool
TouchBlockState::TouchActionAllowsDoubleTapZoom() const
{
  if (!gfxPrefs::TouchActionEnabled()) {
    return true;
  }
  for (size_t i = 0; i < mAllowedTouchBehaviors.Length(); i++) {
    if (!(mAllowedTouchBehaviors[i] & AllowedTouchBehavior::DOUBLE_TAP_ZOOM)) {
      return false;
    }
  }
  return true;
}

bool
TouchBlockState::TouchActionAllowsPanningX() const
{
  if (!gfxPrefs::TouchActionEnabled()) {
    return true;
  }
  if (mAllowedTouchBehaviors.IsEmpty()) {
    // Default to allowed
    return true;
  }
  TouchBehaviorFlags flags = mAllowedTouchBehaviors[0];
  return (flags & AllowedTouchBehavior::HORIZONTAL_PAN);
}

bool
TouchBlockState::TouchActionAllowsPanningY() const
{
  if (!gfxPrefs::TouchActionEnabled()) {
    return true;
  }
  if (mAllowedTouchBehaviors.IsEmpty()) {
    // Default to allowed
    return true;
  }
  TouchBehaviorFlags flags = mAllowedTouchBehaviors[0];
  return (flags & AllowedTouchBehavior::VERTICAL_PAN);
}

bool
TouchBlockState::TouchActionAllowsPanningXY() const
{
  if (!gfxPrefs::TouchActionEnabled()) {
    return true;
  }
  if (mAllowedTouchBehaviors.IsEmpty()) {
    // Default to allowed
    return true;
  }
  TouchBehaviorFlags flags = mAllowedTouchBehaviors[0];
  return (flags & AllowedTouchBehavior::HORIZONTAL_PAN)
      && (flags & AllowedTouchBehavior::VERTICAL_PAN);
}

} // namespace layers
} // namespace mozilla
