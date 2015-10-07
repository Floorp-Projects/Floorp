/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputBlockState.h"
#include "AsyncPanZoomController.h"         // for AsyncPanZoomController
#include "gfxPrefs.h"                       // for gfxPrefs
#include "mozilla/SizePrintfMacros.h"       // for PRIuSIZE
#include "mozilla/layers/APZCTreeManager.h" // for AllowedTouchBehavior
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

  TBS_LOG("%p replacing unconfirmed target %p with real target %p\n",
      this, mTargetApzc.get(), aTargetApzc.get());

  UpdateTargetApzc(aTargetApzc);
  return true;
}

void
InputBlockState::UpdateTargetApzc(const nsRefPtr<AsyncPanZoomController>& aTargetApzc)
{
  // note that aTargetApzc MAY be null here.
  mTargetApzc = aTargetApzc;
  mTransformToApzc = aTargetApzc ? aTargetApzc->GetTransformToThis() : gfx::Matrix4x4();
  mOverscrollHandoffChain = (mTargetApzc ? mTargetApzc->BuildOverscrollHandoffChain() : nullptr);
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
CancelableBlockState::IsContentResponseTimerExpired() const
{
  return mContentResponseTimerExpired;
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
  if (!IsTargetConfirmed()) {
    return false;
  }
  return mContentResponded || mContentResponseTimerExpired;
}

void
CancelableBlockState::DispatchImmediate(const InputData& aEvent) const
{
  MOZ_ASSERT(!HasEvents());
  MOZ_ASSERT(GetTargetApzc());
  DispatchEvent(aEvent);
}

void
CancelableBlockState::DispatchEvent(const InputData& aEvent) const
{
  GetTargetApzc()->HandleInputEvent(aEvent, mTransformToApzc);
}

// This is used to track the current wheel transaction.
static uint64_t sLastWheelBlockId = InputBlockState::NO_BLOCK_ID;

WheelBlockState::WheelBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                                 bool aTargetConfirmed,
                                 const ScrollWheelInput& aInitialEvent)
  : CancelableBlockState(aTargetApzc, aTargetConfirmed)
  , mTransactionEnded(false)
{
  sLastWheelBlockId = GetBlockId();

  if (aTargetConfirmed) {
    // Find the nearest APZC in the overscroll handoff chain that is scrollable.
    // If we get a content confirmation later that the apzc is different, then
    // content should have found a scrollable apzc, so we don't need to handle
    // that case.
    nsRefPtr<AsyncPanZoomController> apzc =
      mOverscrollHandoffChain->FindFirstScrollable(aInitialEvent);

    // If nothing is scrollable, we don't consider this block as starting a
    // transaction.
    if (!apzc) {
      EndTransaction();
      return;
    }

    if (apzc != GetTargetApzc()) {
      UpdateTargetApzc(apzc);
    }
  }
}

bool
WheelBlockState::SetContentResponse(bool aPreventDefault)
{
  if (aPreventDefault) {
    EndTransaction();
  }
  return CancelableBlockState::SetContentResponse(aPreventDefault);
}

bool
WheelBlockState::SetConfirmedTargetApzc(const nsRefPtr<AsyncPanZoomController>& aTargetApzc)
{
  // The APZC that we find via APZCCallbackHelpers may not be the same APZC
  // ESM or OverscrollHandoff would have computed. Make sure we get the right
  // one by looking for the first apzc the next pending event can scroll.
  nsRefPtr<AsyncPanZoomController> apzc = aTargetApzc;
  if (apzc && mEvents.Length() > 0) {
    const ScrollWheelInput& event = mEvents.ElementAt(0);
    apzc = apzc->BuildOverscrollHandoffChain()->FindFirstScrollable(event);
  }

  InputBlockState::SetConfirmedTargetApzc(apzc);
  return true;
}

void
WheelBlockState::Update(const ScrollWheelInput& aEvent)
{
  // We might not be in a transaction if the block never started in a
  // transaction - for example, if nothing was scrollable.
  if (!InTransaction()) {
    return;
  }

  // If we can't scroll in the direction of the wheel event, we don't update
  // the last move time. This allows us to timeout a transaction even if the
  // mouse isn't moving.
  //
  // We skip this check if the target is not yet confirmed, so that when it is
  // confirmed, we don't timeout the transaction.
  nsRefPtr<AsyncPanZoomController> apzc = GetTargetApzc();
  if (IsTargetConfirmed() && !apzc->CanScroll(aEvent)) {
    return;
  }

  // Update the time of the last known good event, and reset the mouse move
  // time to null. This will reset the delays on both the general transaction
  // timeout and the mouse-move-in-frame timeout.
  mLastEventTime = aEvent.mTimeStamp;
  mLastMouseMove = TimeStamp();
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
  TBS_LOG("%p dropping %" PRIuSIZE " events\n", this, mEvents.Length());
  mEvents.Clear();
}

void
WheelBlockState::HandleEvents()
{
  while (HasEvents()) {
    TBS_LOG("%p returning first of %" PRIuSIZE " events\n", this, mEvents.Length());
    ScrollWheelInput event = mEvents[0];
    mEvents.RemoveElementAt(0);
    DispatchEvent(event);
  }
}

bool
WheelBlockState::MustStayActive()
{
  return !mTransactionEnded;
}

const char*
WheelBlockState::Type()
{
  return "scroll wheel";
}

bool
WheelBlockState::ShouldAcceptNewEvent() const
{
  if (!InTransaction()) {
    // If we're not in a transaction, start a new one.
    return false;
  }

  nsRefPtr<AsyncPanZoomController> apzc = GetTargetApzc();
  if (apzc->IsDestroyed()) {
    return false;
  }

  return true;
}

bool
WheelBlockState::MaybeTimeout(const ScrollWheelInput& aEvent)
{
  MOZ_ASSERT(InTransaction());

  if (MaybeTimeout(aEvent.mTimeStamp)) {
    return true;
  }

  if (!mLastMouseMove.IsNull()) {
    // If there's a recent mouse movement, we can time out the transaction early.
    TimeDuration duration = TimeStamp::Now() - mLastMouseMove;
    if (duration.ToMilliseconds() >= gfxPrefs::MouseWheelIgnoreMoveDelayMs()) {
      TBS_LOG("%p wheel transaction timed out after mouse move\n", this);
      EndTransaction();
      return true;
    }
  }

  return false;
}

bool
WheelBlockState::MaybeTimeout(const TimeStamp& aTimeStamp)
{
  MOZ_ASSERT(InTransaction());

  // End the transaction if the event occurred > 1.5s after the most recently
  // seen wheel event.
  TimeDuration duration = aTimeStamp - mLastEventTime;
  if (duration.ToMilliseconds() < gfxPrefs::MouseWheelTransactionTimeoutMs()) {
    return false;
  }

  TBS_LOG("%p wheel transaction timed out\n", this);

  if (gfxPrefs::MouseScrollTestingEnabled()) {
    nsRefPtr<AsyncPanZoomController> apzc = GetTargetApzc();
    apzc->NotifyMozMouseScrollEvent(NS_LITERAL_STRING("MozMouseScrollTransactionTimeout"));
  }

  EndTransaction();
  return true;
}

void
WheelBlockState::OnMouseMove(const ScreenIntPoint& aPoint)
{
  MOZ_ASSERT(InTransaction());

  if (!GetTargetApzc()->Contains(aPoint)) {
    EndTransaction();
    return;
  }

  if (mLastMouseMove.IsNull()) {
    // If the cursor is moving inside the frame, and it is more than the
    // ignoremovedelay time since the last scroll operation, we record
    // this as the most recent mouse movement.
    TimeStamp now = TimeStamp::Now();
    TimeDuration duration = now - mLastEventTime;
    if (duration.ToMilliseconds() >= gfxPrefs::MouseWheelIgnoreMoveDelayMs()) {
      mLastMouseMove = now;
    }
  }
}

void
WheelBlockState::UpdateTargetApzc(const nsRefPtr<AsyncPanZoomController>& aTargetApzc)
{
  InputBlockState::UpdateTargetApzc(aTargetApzc);

  // If we found there was no target apzc, then we end the transaction.
  if (!GetTargetApzc()) {
    EndTransaction();
  }
}

bool
WheelBlockState::InTransaction() const
{
  // We consider a wheel block to be in a transaction if it has a confirmed
  // target and is the most recent wheel input block to be created.
  if (GetBlockId() != sLastWheelBlockId) {
    return false;
  }

  if (mTransactionEnded) {
    return false;
  }

  MOZ_ASSERT(GetTargetApzc());
  return true;
}

bool
WheelBlockState::AllowScrollHandoff() const
{
  // If we're in a wheel transaction, we do not allow overscroll handoff until
  // a new event ends the wheel transaction.
  return !IsTargetConfirmed() || !InTransaction();
}

void
WheelBlockState::EndTransaction()
{
  TBS_LOG("%p ending wheel transaction\n", this);
  mTransactionEnded = true;
}

PanGestureBlockState::PanGestureBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                                           bool aTargetConfirmed,
                                           const PanGestureInput& aInitialEvent)
  : CancelableBlockState(aTargetApzc, aTargetConfirmed)
  , mInterrupted(false)
  , mWaitingForContentResponse(false)
{
  if (aTargetConfirmed) {
    // Find the nearest APZC in the overscroll handoff chain that is scrollable.
    // If we get a content confirmation later that the apzc is different, then
    // content should have found a scrollable apzc, so we don't need to handle
    // that case.
    nsRefPtr<AsyncPanZoomController> apzc =
      mOverscrollHandoffChain->FindFirstScrollable(aInitialEvent);

    if (apzc && apzc != GetTargetApzc()) {
      UpdateTargetApzc(apzc);
    }
  }
}

bool
PanGestureBlockState::SetConfirmedTargetApzc(const nsRefPtr<AsyncPanZoomController>& aTargetApzc)
{
  // The APZC that we find via APZCCallbackHelpers may not be the same APZC
  // ESM or OverscrollHandoff would have computed. Make sure we get the right
  // one by looking for the first apzc the next pending event can scroll.
  nsRefPtr<AsyncPanZoomController> apzc = aTargetApzc;
  if (apzc && mEvents.Length() > 0) {
    const PanGestureInput& event = mEvents.ElementAt(0);
    nsRefPtr<AsyncPanZoomController> scrollableApzc =
      apzc->BuildOverscrollHandoffChain()->FindFirstScrollable(event);
    if (scrollableApzc) {
      apzc = scrollableApzc;
    }
  }

  InputBlockState::SetConfirmedTargetApzc(apzc);
  return true;
}

void
PanGestureBlockState::AddEvent(const PanGestureInput& aEvent)
{
  mEvents.AppendElement(aEvent);
}

bool
PanGestureBlockState::HasEvents() const
{
  return !mEvents.IsEmpty();
}

void
PanGestureBlockState::DropEvents()
{
  TBS_LOG("%p dropping %" PRIuSIZE " events\n", this, mEvents.Length());
  mEvents.Clear();
}

void
PanGestureBlockState::HandleEvents()
{
  while (HasEvents()) {
    TBS_LOG("%p returning first of %" PRIuSIZE " events\n", this, mEvents.Length());
    PanGestureInput event = mEvents[0];
    mEvents.RemoveElementAt(0);
    DispatchEvent(event);
  }
}

bool
PanGestureBlockState::MustStayActive()
{
  return !mInterrupted;
}

const char*
PanGestureBlockState::Type()
{
  return "pan gesture";
}

bool
PanGestureBlockState::SetContentResponse(bool aPreventDefault)
{
  if (aPreventDefault) {
    mInterrupted = true;
  }
  bool stateChanged = CancelableBlockState::SetContentResponse(aPreventDefault);
  if (mWaitingForContentResponse) {
    mWaitingForContentResponse = false;
    stateChanged = true;
  }
  return stateChanged;
}

bool
PanGestureBlockState::IsReadyForHandling() const
{
  if (!CancelableBlockState::IsReadyForHandling()) {
    return false;
  }
  return !mWaitingForContentResponse ||
         IsContentResponseTimerExpired();
}

bool
PanGestureBlockState::AllowScrollHandoff() const
{
  return false;
}

void
PanGestureBlockState::SetNeedsToWaitForContentResponse(bool aWaitForContentResponse)
{
  mWaitingForContentResponse = aWaitForContentResponse;
}

TouchBlockState::TouchBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                                 bool aTargetConfirmed, TouchCounter& aCounter)
  : CancelableBlockState(aTargetApzc, aTargetConfirmed)
  , mAllowedTouchBehaviorSet(false)
  , mDuringFastFling(false)
  , mSingleTapOccurred(false)
  , mTouchCounter(aCounter)
{
  TBS_LOG("Creating %p\n", this);
}

bool
TouchBlockState::SetAllowedTouchBehaviors(const nsTArray<TouchBehaviorFlags>& aBehaviors)
{
  if (mAllowedTouchBehaviorSet) {
    return false;
  }
  TBS_LOG("%p got allowed touch behaviours for %" PRIuSIZE " points\n", this, aBehaviors.Length());
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
    MOZ_ASSERT(aOther.mAllowedTouchBehaviorSet || aOther.IsContentResponseTimerExpired());
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

  if (!gfxPrefs::TouchActionEnabled()) {
    return true;
  }

  return mAllowedTouchBehaviorSet || IsContentResponseTimerExpired();
}

void
TouchBlockState::SetDuringFastFling()
{
  TBS_LOG("%p setting fast-motion flag\n", this);
  mDuringFastFling = true;
}

bool
TouchBlockState::IsDuringFastFling() const
{
  return mDuringFastFling;
}

bool
TouchBlockState::SetSingleTapOccurred()
{
  TBS_LOG("%p attempting to set single-tap occurred; disallowed=%d\n",
    this, mDuringFastFling);
  if (!mDuringFastFling) {
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
  TBS_LOG("%p dropping %" PRIuSIZE " events\n", this, mEvents.Length());
  mEvents.Clear();
}

void
TouchBlockState::HandleEvents()
{
  while (HasEvents()) {
    TBS_LOG("%p returning first of %" PRIuSIZE " events\n", this, mEvents.Length());
    MultiTouchInput event = mEvents[0];
    mEvents.RemoveElementAt(0);
    DispatchEvent(event);
  }
}

void
TouchBlockState::DispatchEvent(const InputData& aEvent) const
{
  MOZ_ASSERT(aEvent.mInputType == MULTITOUCH_INPUT);
  mTouchCounter.Update(aEvent.AsMultiTouchInput());
  CancelableBlockState::DispatchEvent(aEvent);
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
