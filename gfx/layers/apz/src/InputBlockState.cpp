/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputBlockState.h"
#include "AsyncPanZoomController.h"         // for AsyncPanZoomController
#include "AsyncScrollBase.h"                // for kScrollSeriesTimeoutMs
#include "gfxPrefs.h"                       // for gfxPrefs
#include "mozilla/MouseEvents.h"
#include "mozilla/SizePrintfMacros.h"       // for PRIuSIZE
#include "mozilla/Telemetry.h"              // for Telemetry
#include "mozilla/layers/APZCTreeManager.h" // for AllowedTouchBehavior
#include "OverscrollHandoffState.h"

#define TBS_LOG(...)
// #define TBS_LOG(...) printf_stderr("TBS: " __VA_ARGS__)

namespace mozilla {
namespace layers {

static uint64_t sBlockCounter = InputBlockState::NO_BLOCK_ID + 1;

InputBlockState::InputBlockState(const RefPtr<AsyncPanZoomController>& aTargetApzc,
                                 bool aTargetConfirmed)
  : mTargetApzc(aTargetApzc)
  , mTargetConfirmed(aTargetConfirmed ? TargetConfirmationState::eConfirmed
                                      : TargetConfirmationState::eUnconfirmed)
  , mBlockId(sBlockCounter++)
  , mTransformToApzc(aTargetApzc->GetTransformToThis())
{
  // We should never be constructed with a nullptr target.
  MOZ_ASSERT(mTargetApzc);
  mOverscrollHandoffChain = mTargetApzc->BuildOverscrollHandoffChain();
}

bool
InputBlockState::SetConfirmedTargetApzc(const RefPtr<AsyncPanZoomController>& aTargetApzc,
                                        TargetConfirmationState aState)
{
  MOZ_ASSERT(aState == TargetConfirmationState::eConfirmed
          || aState == TargetConfirmationState::eTimedOut);

  if (mTargetConfirmed == TargetConfirmationState::eTimedOut &&
      aState == TargetConfirmationState::eConfirmed) {
    // The main thread finally responded. We had already timed out the
    // confirmation, but we want to update the state internally so that we
    // can record the time for telemetry purposes.
    mTargetConfirmed = TargetConfirmationState::eTimedOutAndMainThreadResponded;
  }
  if (mTargetConfirmed != TargetConfirmationState::eUnconfirmed) {
    return false;
  }
  mTargetConfirmed = aState;

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
InputBlockState::UpdateTargetApzc(const RefPtr<AsyncPanZoomController>& aTargetApzc)
{
  // note that aTargetApzc MAY be null here.
  mTargetApzc = aTargetApzc;
  mTransformToApzc = aTargetApzc ? aTargetApzc->GetTransformToThis() : ScreenToParentLayerMatrix4x4();
  mOverscrollHandoffChain = (mTargetApzc ? mTargetApzc->BuildOverscrollHandoffChain() : nullptr);
}

const RefPtr<AsyncPanZoomController>&
InputBlockState::GetTargetApzc() const
{
  return mTargetApzc;
}

const RefPtr<const OverscrollHandoffChain>&
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
  return mTargetConfirmed != TargetConfirmationState::eUnconfirmed;
}

bool
InputBlockState::HasReceivedRealConfirmedTarget() const
{
  return mTargetConfirmed == TargetConfirmationState::eConfirmed ||
         mTargetConfirmed == TargetConfirmationState::eTimedOutAndMainThreadResponded;
}

bool
InputBlockState::IsDownchainOf(AsyncPanZoomController* aA, AsyncPanZoomController* aB) const
{
  if (aA == aB) {
    return true;
  }

  bool seenA = false;
  for (size_t i = 0; i < mOverscrollHandoffChain->Length(); ++i) {
    AsyncPanZoomController* apzc = mOverscrollHandoffChain->GetApzcAtIndex(i);
    if (apzc == aB) {
      return seenA;
    }
    if (apzc == aA) {
      seenA = true;
    }
  }
  return false;
}


void
InputBlockState::SetScrolledApzc(AsyncPanZoomController* aApzc)
{
  // An input block should only have one scrolled APZC.
  MOZ_ASSERT(!mScrolledApzc || (gfxPrefs::APZAllowImmediateHandoff() ? IsDownchainOf(mScrolledApzc, aApzc) : mScrolledApzc == aApzc));

  mScrolledApzc = aApzc;
}

AsyncPanZoomController*
InputBlockState::GetScrolledApzc() const
{
  return mScrolledApzc;
}

bool
InputBlockState::IsDownchainOfScrolledApzc(AsyncPanZoomController* aApzc) const
{
  MOZ_ASSERT(aApzc && mScrolledApzc);

  return IsDownchainOf(mScrolledApzc, aApzc);
}

CancelableBlockState::CancelableBlockState(const RefPtr<AsyncPanZoomController>& aTargetApzc,
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
  mPreventDefault = aPreventDefault;
  mContentResponded = true;
  return true;
}

void
CancelableBlockState::StartContentResponseTimer()
{
  MOZ_ASSERT(mContentResponseTimer.IsNull());
  mContentResponseTimer = TimeStamp::Now();
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
CancelableBlockState::HasReceivedAllContentNotifications() const
{
  return HasReceivedRealConfirmedTarget() && mContentResponded;
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

void
CancelableBlockState::RecordContentResponseTime()
{
  if (!mContentResponseTimer) {
    // We might get responses from content even though we didn't wait for them.
    // In that case, ignore the time on them, because they're not relevant for
    // tuning our timeout value. Also this function might get called multiple
    // times on the same input block, so we should only record the time from the
    // first successful call.
    return;
  }
  if (!HasReceivedAllContentNotifications()) {
    // Not done yet, we'll get called again
    return;
  }
  mozilla::Telemetry::Accumulate(mozilla::Telemetry::CONTENT_RESPONSE_DURATION,
    (uint32_t)(TimeStamp::Now() - mContentResponseTimer).ToMilliseconds());
  mContentResponseTimer = TimeStamp();
}

DragBlockState::DragBlockState(const RefPtr<AsyncPanZoomController>& aTargetApzc,
                               bool aTargetConfirmed,
                               const MouseInput& aInitialEvent)
  : CancelableBlockState(aTargetApzc, aTargetConfirmed)
  , mReceivedMouseUp(false)
{
}

bool
DragBlockState::HasReceivedMouseUp()
{
  return mReceivedMouseUp;
}

void
DragBlockState::MarkMouseUpReceived()
{
  mReceivedMouseUp = true;
}

void
DragBlockState::SetDragMetrics(const AsyncDragMetrics& aDragMetrics)
{
  mDragMetrics = aDragMetrics;
}

void
DragBlockState::DispatchEvent(const InputData& aEvent) const
{
  MouseInput mouseInput = aEvent.AsMouseInput();
  if (!mouseInput.TransformToLocal(mTransformToApzc)) {
    return;
  }

  GetTargetApzc()->HandleDragEvent(mouseInput, mDragMetrics);
}

void
DragBlockState::AddEvent(const MouseInput& aEvent)
{
  mEvents.AppendElement(aEvent);
}

bool
DragBlockState::HasEvents() const
{
  return !mEvents.IsEmpty();
}

void
DragBlockState::DropEvents()
{
  TBS_LOG("%p dropping %" PRIuSIZE " events\n", this, mEvents.Length());
  mEvents.Clear();
}

void
DragBlockState::HandleEvents()
{
  while (HasEvents()) {
    TBS_LOG("%p returning first of %" PRIuSIZE " events\n", this, mEvents.Length());
    MouseInput event = mEvents[0];
    mEvents.RemoveElementAt(0);
    DispatchEvent(event);
  }
}

bool
DragBlockState::MustStayActive()
{
  return !mReceivedMouseUp;
}

const char*
DragBlockState::Type()
{
  return "drag";
}
// This is used to track the current wheel transaction.
static uint64_t sLastWheelBlockId = InputBlockState::NO_BLOCK_ID;

WheelBlockState::WheelBlockState(const RefPtr<AsyncPanZoomController>& aTargetApzc,
                                 bool aTargetConfirmed,
                                 const ScrollWheelInput& aInitialEvent)
  : CancelableBlockState(aTargetApzc, aTargetConfirmed)
  , mScrollSeriesCounter(0)
  , mTransactionEnded(false)
{
  sLastWheelBlockId = GetBlockId();

  if (aTargetConfirmed) {
    // Find the nearest APZC in the overscroll handoff chain that is scrollable.
    // If we get a content confirmation later that the apzc is different, then
    // content should have found a scrollable apzc, so we don't need to handle
    // that case.
    RefPtr<AsyncPanZoomController> apzc =
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
WheelBlockState::SetConfirmedTargetApzc(const RefPtr<AsyncPanZoomController>& aTargetApzc,
                                        TargetConfirmationState aState)
{
  // The APZC that we find via APZCCallbackHelpers may not be the same APZC
  // ESM or OverscrollHandoff would have computed. Make sure we get the right
  // one by looking for the first apzc the next pending event can scroll.
  RefPtr<AsyncPanZoomController> apzc = aTargetApzc;
  if (apzc && mEvents.Length() > 0) {
    const ScrollWheelInput& event = mEvents.ElementAt(0);
    apzc = apzc->BuildOverscrollHandoffChain()->FindFirstScrollable(event);
  }

  InputBlockState::SetConfirmedTargetApzc(apzc, aState);
  return true;
}

void
WheelBlockState::Update(ScrollWheelInput& aEvent)
{
  // We might not be in a transaction if the block never started in a
  // transaction - for example, if nothing was scrollable.
  if (!InTransaction()) {
    return;
  }

  // The current "scroll series" is a like a sub-transaction. It has a separate
  // timeout of 80ms. Since we need to compute wheel deltas at different phases
  // of a transaction (for example, when it is updated, and later when the
  // event action is taken), we affix the scroll series counter to the event.
  // This makes GetScrollWheelDelta() consistent.
  if (!mLastEventTime.IsNull() &&
      (aEvent.mTimeStamp - mLastEventTime).ToMilliseconds() > kScrollSeriesTimeoutMs)
  {
    mScrollSeriesCounter = 0;
  }
  aEvent.mScrollSeriesNumber = ++mScrollSeriesCounter;

  // If we can't scroll in the direction of the wheel event, we don't update
  // the last move time. This allows us to timeout a transaction even if the
  // mouse isn't moving.
  //
  // We skip this check if the target is not yet confirmed, so that when it is
  // confirmed, we don't timeout the transaction.
  RefPtr<AsyncPanZoomController> apzc = GetTargetApzc();
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

  RefPtr<AsyncPanZoomController> apzc = GetTargetApzc();
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
    RefPtr<AsyncPanZoomController> apzc = GetTargetApzc();
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
WheelBlockState::UpdateTargetApzc(const RefPtr<AsyncPanZoomController>& aTargetApzc)
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

PanGestureBlockState::PanGestureBlockState(const RefPtr<AsyncPanZoomController>& aTargetApzc,
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
    RefPtr<AsyncPanZoomController> apzc =
      mOverscrollHandoffChain->FindFirstScrollable(aInitialEvent);

    if (apzc && apzc != GetTargetApzc()) {
      UpdateTargetApzc(apzc);
    }
  }
}

bool
PanGestureBlockState::SetConfirmedTargetApzc(const RefPtr<AsyncPanZoomController>& aTargetApzc,
                                             TargetConfirmationState aState)
{
  // The APZC that we find via APZCCallbackHelpers may not be the same APZC
  // ESM or OverscrollHandoff would have computed. Make sure we get the right
  // one by looking for the first apzc the next pending event can scroll.
  RefPtr<AsyncPanZoomController> apzc = aTargetApzc;
  if (apzc && mEvents.Length() > 0) {
    const PanGestureInput& event = mEvents.ElementAt(0);
    RefPtr<AsyncPanZoomController> scrollableApzc =
      apzc->BuildOverscrollHandoffChain()->FindFirstScrollable(event);
    if (scrollableApzc) {
      apzc = scrollableApzc;
    }
  }

  InputBlockState::SetConfirmedTargetApzc(apzc, aState);
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
    TBS_LOG("%p setting interrupted flag\n", this);
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
PanGestureBlockState::HasReceivedAllContentNotifications() const
{
  return CancelableBlockState::HasReceivedAllContentNotifications()
      && !mWaitingForContentResponse;
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

TouchBlockState::TouchBlockState(const RefPtr<AsyncPanZoomController>& aTargetApzc,
                                 bool aTargetConfirmed, TouchCounter& aCounter)
  : CancelableBlockState(aTargetApzc, aTargetConfirmed)
  , mAllowedTouchBehaviorSet(false)
  , mDuringFastFling(false)
  , mSingleTapOccurred(false)
  , mInSlop(false)
  , mTouchCounter(aCounter)
{
  TBS_LOG("Creating %p\n", this);
  if (!gfxPrefs::TouchActionEnabled()) {
    mAllowedTouchBehaviorSet = true;
  }
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
TouchBlockState::HasReceivedAllContentNotifications() const
{
  return CancelableBlockState::HasReceivedAllContentNotifications()
      // See comment in TouchBlockState::IsReadyforHandling()
      && (!gfxPrefs::TouchActionEnabled() || mAllowedTouchBehaviorSet);
}

bool
TouchBlockState::IsReadyForHandling() const
{
  if (!CancelableBlockState::IsReadyForHandling()) {
    return false;
  }

  if (!gfxPrefs::TouchActionEnabled()) {
    // If TouchActionEnabled() was false when this block was created, then
    // mAllowedTouchBehaviorSet is guaranteed to the true. However, the pref
    // may have been flipped to false after the block was created. In that case,
    // we should eventually get the touch-behaviour notification, or expire the
    // content response timeout, but we don't really need to wait for those,
    // since we don't care about the touch-behaviour values any more.
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

void
TouchBlockState::SetSingleTapOccurred()
{
  TBS_LOG("%p setting single-tap-occurred flag\n", this);
  mSingleTapOccurred = true;
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

bool
TouchBlockState::UpdateSlopState(const MultiTouchInput& aInput,
                                 bool aApzcCanConsumeEvents)
{
  if (aInput.mType == MultiTouchInput::MULTITOUCH_START) {
    // this is by definition the first event in this block. If it's the first
    // touch, then we enter a slop state.
    mInSlop = (aInput.mTouches.Length() == 1);
    if (mInSlop) {
      mSlopOrigin = aInput.mTouches[0].mScreenPoint;
      TBS_LOG("%p entering slop with origin %s\n", this, Stringify(mSlopOrigin).c_str());
    }
    return false;
  }
  if (mInSlop) {
    ScreenCoord threshold = aApzcCanConsumeEvents
        ? AsyncPanZoomController::GetTouchStartTolerance()
        : ScreenCoord(gfxPrefs::APZTouchMoveTolerance() * APZCTreeManager::GetDPI());
    bool stayInSlop = (aInput.mType == MultiTouchInput::MULTITOUCH_MOVE) &&
        (aInput.mTouches.Length() == 1) &&
        ((aInput.mTouches[0].mScreenPoint - mSlopOrigin).Length() < threshold);
    if (!stayInSlop) {
      // we're out of the slop zone, and will stay out for the remainder of
      // this block
      TBS_LOG("%p exiting slop\n", this);
      mInSlop = false;
    }
  }
  return mInSlop;
}

uint32_t
TouchBlockState::GetActiveTouchCount() const
{
  return mTouchCounter.GetActiveTouchCount();
}

} // namespace layers
} // namespace mozilla
