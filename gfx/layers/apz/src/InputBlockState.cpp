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

  // note that aTargetApzc MAY be null here.
  mTargetApzc = aTargetApzc;
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

TouchBlockState::TouchBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                                 bool aTargetConfirmed)
  : InputBlockState(aTargetApzc, aTargetConfirmed)
  , mAllowedTouchBehaviorSet(false)
  , mPreventDefault(false)
  , mContentResponded(false)
  , mContentResponseTimerExpired(false)
  , mSingleTapDisallowed(false)
  , mSingleTapOccurred(false)
{
  TBS_LOG("Creating %p\n", this);
}

bool
TouchBlockState::SetContentResponse(bool aPreventDefault)
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
TouchBlockState::TimeoutContentResponse()
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
TouchBlockState::CopyAllowedTouchBehaviorsFrom(const TouchBlockState& aOther)
{
  TBS_LOG("%p copying allowed touch behaviours from %p\n", this, &aOther);
  MOZ_ASSERT(aOther.mAllowedTouchBehaviorSet);
  return SetAllowedTouchBehaviors(aOther.mAllowedTouchBehaviors);
}

bool
TouchBlockState::IsReadyForHandling() const
{
  if (!IsTargetConfirmed()) {
    return false;
  }
  // TODO: for long-tap blocks we probably don't need the touch behaviour?
  if (gfxPrefs::TouchActionEnabled() && !mAllowedTouchBehaviorSet) {
    return false;
  }
  if (!mContentResponded && !mContentResponseTimerExpired) {
    return false;
  }
  return true;
}

bool
TouchBlockState::IsDefaultPrevented() const
{
  MOZ_ASSERT(mContentResponded || mContentResponseTimerExpired);
  return mPreventDefault;
}

void
TouchBlockState::DisallowSingleTap()
{
  TBS_LOG("%p disallowing single-tap\n", this);
  mSingleTapDisallowed = true;
}

bool
TouchBlockState::SetSingleTapOccurred()
{
  TBS_LOG("%p attempting to set single-tap occurred; disallowed=%d\n", this, mSingleTapDisallowed);
  if (!mSingleTapDisallowed) {
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

void
TouchBlockState::DropEvents()
{
  TBS_LOG("%p dropping %lu events\n", this, mEvents.Length());
  mEvents.Clear();
}

MultiTouchInput
TouchBlockState::RemoveFirstEvent()
{
  MOZ_ASSERT(!mEvents.IsEmpty());
  TBS_LOG("%p returning first of %lu events\n", this, mEvents.Length());
  MultiTouchInput event = mEvents[0];
  mEvents.RemoveElementAt(0);
  return event;
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
