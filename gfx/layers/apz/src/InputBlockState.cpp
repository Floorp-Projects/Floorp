/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputBlockState.h"

#include "APZUtils.h"
#include "AsyncPanZoomController.h"  // for AsyncPanZoomController
#include "ScrollAnimationPhysics.h"  // for kScrollSeriesTimeoutMs

#include "mozilla/MouseEvents.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/StaticPrefs_mousewheel.h"
#include "mozilla/StaticPrefs_test.h"
#include "mozilla/Telemetry.h"  // for Telemetry
#include "mozilla/ToString.h"
#include "mozilla/layers/IAPZCTreeManager.h"  // for AllowedTouchBehavior
#include "OverscrollHandoffState.h"
#include "QueuedInput.h"

static mozilla::LazyLogModule sApzIbsLog("apz.inputstate");
#define TBS_LOG(...) MOZ_LOG(sApzIbsLog, LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
namespace layers {

static uint64_t sBlockCounter = InputBlockState::NO_BLOCK_ID + 1;

InputBlockState::InputBlockState(
    const RefPtr<AsyncPanZoomController>& aTargetApzc,
    TargetConfirmationFlags aFlags)
    : mTargetApzc(aTargetApzc),
      mRequiresTargetConfirmation(aFlags.mRequiresTargetConfirmation),
      mBlockId(sBlockCounter++),
      mTransformToApzc(aTargetApzc->GetTransformToThis()) {
  // We should never be constructed with a nullptr target.
  MOZ_ASSERT(mTargetApzc);
  mOverscrollHandoffChain = mTargetApzc->BuildOverscrollHandoffChain();
  // If a new block starts on a scrollthumb and we have APZ scrollbar
  // dragging enabled, defer confirmation until we get the drag metrics
  // for the thumb.
  bool startingDrag = StaticPrefs::apz_drag_enabled() && aFlags.mHitScrollThumb;
  mTargetConfirmed = aFlags.mTargetConfirmed && !startingDrag
                         ? TargetConfirmationState::eConfirmed
                         : TargetConfirmationState::eUnconfirmed;
}

bool InputBlockState::SetConfirmedTargetApzc(
    const RefPtr<AsyncPanZoomController>& aTargetApzc,
    TargetConfirmationState aState, InputData* aFirstInput,
    bool aForScrollbarDrag) {
  MOZ_ASSERT(aState == TargetConfirmationState::eConfirmed ||
             aState == TargetConfirmationState::eTimedOut);

  if (mTargetConfirmed == TargetConfirmationState::eTimedOut &&
      aState == TargetConfirmationState::eConfirmed) {
    // The main thread finally responded. We had already timed out the
    // confirmation, but we want to update the state internally so that we
    // can record the time for telemetry purposes.
    mTargetConfirmed = TargetConfirmationState::eTimedOutAndMainThreadResponded;
  }
  // Sometimes, bugs in compositor hit testing can lead to APZ confirming
  // a different target than the main thread. If this happens for a drag
  // block created for a scrollbar drag, the consequences can be fairly
  // user-unfriendly, such as the scrollbar not being draggable at all,
  // or it scrolling the contents of the wrong scrollframe. In debug
  // builds, we assert in this situation, so that the
  // underlying compositor hit testing bug can be fixed. In release builds,
  // however, we just silently accept the main thread's confirmed target,
  // which will produce the expected behaviour (apart from drag events
  // received so far being dropped).
  if (AsDragBlock() && aForScrollbarDrag &&
      mTargetConfirmed == TargetConfirmationState::eConfirmed &&
      aState == TargetConfirmationState::eConfirmed && mTargetApzc &&
      aTargetApzc && mTargetApzc->GetGuid() != aTargetApzc->GetGuid()) {
    MOZ_ASSERT(false,
               "APZ and main thread confirmed scrollbar drag block with "
               "different targets");
    UpdateTargetApzc(aTargetApzc);
    return true;
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

  TBS_LOG("%p replacing unconfirmed target %p with real target %p\n", this,
          mTargetApzc.get(), aTargetApzc.get());

  UpdateTargetApzc(aTargetApzc);
  return true;
}

void InputBlockState::UpdateTargetApzc(
    const RefPtr<AsyncPanZoomController>& aTargetApzc) {
  // note that aTargetApzc MAY be null here.
  mTargetApzc = aTargetApzc;
  mTransformToApzc = aTargetApzc ? aTargetApzc->GetTransformToThis()
                                 : ScreenToParentLayerMatrix4x4();
  mOverscrollHandoffChain =
      (mTargetApzc ? mTargetApzc->BuildOverscrollHandoffChain() : nullptr);
}

const RefPtr<AsyncPanZoomController>& InputBlockState::GetTargetApzc() const {
  return mTargetApzc;
}

const RefPtr<const OverscrollHandoffChain>&
InputBlockState::GetOverscrollHandoffChain() const {
  return mOverscrollHandoffChain;
}

uint64_t InputBlockState::GetBlockId() const { return mBlockId; }

bool InputBlockState::IsTargetConfirmed() const {
  return mTargetConfirmed != TargetConfirmationState::eUnconfirmed;
}

bool InputBlockState::HasReceivedRealConfirmedTarget() const {
  return mTargetConfirmed == TargetConfirmationState::eConfirmed ||
         mTargetConfirmed ==
             TargetConfirmationState::eTimedOutAndMainThreadResponded;
}

bool InputBlockState::ShouldDropEvents() const {
  return mRequiresTargetConfirmation &&
         (mTargetConfirmed != TargetConfirmationState::eConfirmed);
}

bool InputBlockState::IsDownchainOf(AsyncPanZoomController* aA,
                                    AsyncPanZoomController* aB) const {
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

void InputBlockState::SetScrolledApzc(AsyncPanZoomController* aApzc) {
  // An input block should only have one scrolled APZC.
  MOZ_ASSERT(!mScrolledApzc || (StaticPrefs::apz_allow_immediate_handoff()
                                    ? IsDownchainOf(mScrolledApzc, aApzc)
                                    : mScrolledApzc == aApzc));

  mScrolledApzc = aApzc;
}

AsyncPanZoomController* InputBlockState::GetScrolledApzc() const {
  return mScrolledApzc;
}

bool InputBlockState::IsDownchainOfScrolledApzc(
    AsyncPanZoomController* aApzc) const {
  MOZ_ASSERT(aApzc && mScrolledApzc);

  return IsDownchainOf(mScrolledApzc, aApzc);
}

void InputBlockState::DispatchEvent(const InputData& aEvent) const {
  GetTargetApzc()->HandleInputEvent(aEvent, mTransformToApzc);
}

CancelableBlockState::CancelableBlockState(
    const RefPtr<AsyncPanZoomController>& aTargetApzc,
    TargetConfirmationFlags aFlags)
    : InputBlockState(aTargetApzc, aFlags),
      mPreventDefault(false),
      mContentResponded(false),
      mContentResponseTimerExpired(false) {}

bool CancelableBlockState::SetContentResponse(bool aPreventDefault) {
  if (mContentResponded) {
    return false;
  }
  TBS_LOG("%p got content response %d with timer expired %d\n", this,
          aPreventDefault, mContentResponseTimerExpired);
  mPreventDefault = aPreventDefault;
  mContentResponded = true;
  return true;
}

bool CancelableBlockState::TimeoutContentResponse() {
  if (mContentResponseTimerExpired) {
    return false;
  }
  TBS_LOG("%p got content timer expired with response received %d\n", this,
          mContentResponded);
  if (!mContentResponded) {
    mPreventDefault = false;
  }
  mContentResponseTimerExpired = true;
  return true;
}

bool CancelableBlockState::IsContentResponseTimerExpired() const {
  return mContentResponseTimerExpired;
}

bool CancelableBlockState::IsDefaultPrevented() const {
  MOZ_ASSERT(mContentResponded || mContentResponseTimerExpired);
  return mPreventDefault;
}

bool CancelableBlockState::HasReceivedAllContentNotifications() const {
  return HasReceivedRealConfirmedTarget() && mContentResponded;
}

bool CancelableBlockState::IsReadyForHandling() const {
  if (!IsTargetConfirmed()) {
    return false;
  }
  return mContentResponded || mContentResponseTimerExpired;
}

bool CancelableBlockState::ShouldDropEvents() const {
  return InputBlockState::ShouldDropEvents() || IsDefaultPrevented();
}

DragBlockState::DragBlockState(
    const RefPtr<AsyncPanZoomController>& aTargetApzc,
    TargetConfirmationFlags aFlags, const MouseInput& aInitialEvent)
    : CancelableBlockState(aTargetApzc, aFlags), mReceivedMouseUp(false) {}

bool DragBlockState::HasReceivedMouseUp() { return mReceivedMouseUp; }

void DragBlockState::MarkMouseUpReceived() { mReceivedMouseUp = true; }

void DragBlockState::SetInitialThumbPos(CSSCoord aThumbPos) {
  mInitialThumbPos = aThumbPos;
}

void DragBlockState::SetDragMetrics(const AsyncDragMetrics& aDragMetrics) {
  mDragMetrics = aDragMetrics;
}

void DragBlockState::DispatchEvent(const InputData& aEvent) const {
  MouseInput mouseInput = aEvent.AsMouseInput();
  if (!mouseInput.TransformToLocal(mTransformToApzc)) {
    return;
  }

  GetTargetApzc()->HandleDragEvent(mouseInput, mDragMetrics, mInitialThumbPos);
}

bool DragBlockState::MustStayActive() { return !mReceivedMouseUp; }

const char* DragBlockState::Type() { return "drag"; }
// This is used to track the current wheel transaction.
static uint64_t sLastWheelBlockId = InputBlockState::NO_BLOCK_ID;

WheelBlockState::WheelBlockState(
    const RefPtr<AsyncPanZoomController>& aTargetApzc,
    TargetConfirmationFlags aFlags, const ScrollWheelInput& aInitialEvent)
    : CancelableBlockState(aTargetApzc, aFlags),
      mScrollSeriesCounter(0),
      mTransactionEnded(false) {
  sLastWheelBlockId = GetBlockId();

  if (aFlags.mTargetConfirmed) {
    // Find the nearest APZC in the overscroll handoff chain that is scrollable.
    // If we get a content confirmation later that the apzc is different, then
    // content should have found a scrollable apzc, so we don't need to handle
    // that case.
    RefPtr<AsyncPanZoomController> apzc =
        mOverscrollHandoffChain->FindFirstScrollable(aInitialEvent,
                                                     &mAllowedScrollDirections);

    if (apzc) {
      if (apzc != GetTargetApzc()) {
        UpdateTargetApzc(apzc);
      }
    } else if (!mOverscrollHandoffChain->CanBePanned(
                   mOverscrollHandoffChain->GetApzcAtIndex(0))) {
      // If there's absolutely nothing scrollable start a transaction and mark
      // this as such to we know to store our EventTime.
      mIsScrollable = false;
    } else {
      // Scrollable, but not in this direction.
      EndTransaction();
    }
  }
}

bool WheelBlockState::SetContentResponse(bool aPreventDefault) {
  if (aPreventDefault) {
    EndTransaction();
  }
  return CancelableBlockState::SetContentResponse(aPreventDefault);
}

bool WheelBlockState::SetConfirmedTargetApzc(
    const RefPtr<AsyncPanZoomController>& aTargetApzc,
    TargetConfirmationState aState, InputData* aFirstInput,
    bool aForScrollbarDrag) {
  // The APZC that we find via APZCCallbackHelpers may not be the same APZC
  // ESM or OverscrollHandoff would have computed. Make sure we get the right
  // one by looking for the first apzc the next pending event can scroll.
  RefPtr<AsyncPanZoomController> apzc = aTargetApzc;
  if (apzc && aFirstInput) {
    apzc = apzc->BuildOverscrollHandoffChain()->FindFirstScrollable(
        *aFirstInput, &mAllowedScrollDirections);
  }

  InputBlockState::SetConfirmedTargetApzc(apzc, aState, aFirstInput,
                                          aForScrollbarDrag);
  return true;
}

void WheelBlockState::Update(ScrollWheelInput& aEvent) {
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
      (aEvent.mTimeStamp - mLastEventTime).ToMilliseconds() >
          kScrollSeriesTimeoutMs) {
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
  if (mIsScrollable && IsTargetConfirmed() && !apzc->CanScroll(aEvent)) {
    return;
  }

  // Update the time of the last known good event, and reset the mouse move
  // time to null. This will reset the delays on both the general transaction
  // timeout and the mouse-move-in-frame timeout.
  mLastEventTime = aEvent.mTimeStamp;
  mLastMouseMove = TimeStamp();
}

bool WheelBlockState::MustStayActive() { return !mTransactionEnded; }

const char* WheelBlockState::Type() { return "scroll wheel"; }

bool WheelBlockState::ShouldAcceptNewEvent() const {
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

bool WheelBlockState::MaybeTimeout(const ScrollWheelInput& aEvent) {
  MOZ_ASSERT(InTransaction());

  if (MaybeTimeout(aEvent.mTimeStamp)) {
    return true;
  }

  if (!mLastMouseMove.IsNull()) {
    // If there's a recent mouse movement, we can time out the transaction
    // early.
    TimeDuration duration = TimeStamp::Now() - mLastMouseMove;
    if (duration.ToMilliseconds() >=
        StaticPrefs::mousewheel_transaction_ignoremovedelay()) {
      TBS_LOG("%p wheel transaction timed out after mouse move\n", this);
      EndTransaction();
      return true;
    }
  }

  return false;
}

bool WheelBlockState::MaybeTimeout(const TimeStamp& aTimeStamp) {
  MOZ_ASSERT(InTransaction());

  // End the transaction if the event occurred > 1.5s after the most recently
  // seen wheel event.
  TimeDuration duration = aTimeStamp - mLastEventTime;
  if (duration.ToMilliseconds() <
      StaticPrefs::mousewheel_transaction_timeout()) {
    return false;
  }

  TBS_LOG("%p wheel transaction timed out\n", this);

  if (StaticPrefs::test_mousescroll()) {
    RefPtr<AsyncPanZoomController> apzc = GetTargetApzc();
    apzc->NotifyMozMouseScrollEvent(u"MozMouseScrollTransactionTimeout"_ns);
  }

  EndTransaction();
  return true;
}

void WheelBlockState::OnMouseMove(
    const ScreenIntPoint& aPoint,
    const Maybe<ScrollableLayerGuid>& aTargetGuid) {
  MOZ_ASSERT(InTransaction());

  if (!GetTargetApzc()->Contains(aPoint) ||
      // If the mouse moved over to a different APZC, `mIsScrollable`
      // may no longer be false and needs to be recomputed.
      (!mIsScrollable && aTargetGuid.isSome() &&
       aTargetGuid.value() != GetTargetApzc()->GetGuid())) {
    EndTransaction();
    return;
  }

  if (mLastMouseMove.IsNull()) {
    // If the cursor is moving inside the frame, and it is more than the
    // ignoremovedelay time since the last scroll operation, we record
    // this as the most recent mouse movement.
    TimeStamp now = TimeStamp::Now();
    TimeDuration duration = now - mLastEventTime;
    if (duration.ToMilliseconds() >=
        StaticPrefs::mousewheel_transaction_ignoremovedelay()) {
      mLastMouseMove = now;
    }
  }
}

void WheelBlockState::UpdateTargetApzc(
    const RefPtr<AsyncPanZoomController>& aTargetApzc) {
  InputBlockState::UpdateTargetApzc(aTargetApzc);

  // If we found there was no target apzc, then we end the transaction.
  if (!GetTargetApzc()) {
    EndTransaction();
  }
}

bool WheelBlockState::InTransaction() const {
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

bool WheelBlockState::AllowScrollHandoff() const {
  // If we're in a wheel transaction, we do not allow overscroll handoff until
  // a new event ends the wheel transaction.
  return !IsTargetConfirmed() || !InTransaction();
}

void WheelBlockState::EndTransaction() {
  TBS_LOG("%p ending wheel transaction\n", this);
  mTransactionEnded = true;
}

PanGestureBlockState::PanGestureBlockState(
    const RefPtr<AsyncPanZoomController>& aTargetApzc,
    TargetConfirmationFlags aFlags, const PanGestureInput& aInitialEvent)
    : CancelableBlockState(aTargetApzc, aFlags),
      mInterrupted(false),
      mWaitingForContentResponse(false) {
  if (aFlags.mTargetConfirmed) {
    // Find the nearest APZC in the overscroll handoff chain that is scrollable.
    // If we get a content confirmation later that the apzc is different, then
    // content should have found a scrollable apzc, so we don't need to handle
    // that case.
    RefPtr<AsyncPanZoomController> apzc =
        mOverscrollHandoffChain->FindFirstScrollable(aInitialEvent,
                                                     &mAllowedScrollDirections);

    if (apzc && apzc != GetTargetApzc()) {
      UpdateTargetApzc(apzc);
    }
  }
}

bool PanGestureBlockState::SetConfirmedTargetApzc(
    const RefPtr<AsyncPanZoomController>& aTargetApzc,
    TargetConfirmationState aState, InputData* aFirstInput,
    bool aForScrollbarDrag) {
  // The APZC that we find via APZCCallbackHelpers may not be the same APZC
  // ESM or OverscrollHandoff would have computed. Make sure we get the right
  // one by looking for the first apzc the next pending event can scroll.
  RefPtr<AsyncPanZoomController> apzc = aTargetApzc;
  if (apzc && aFirstInput) {
    RefPtr<AsyncPanZoomController> scrollableApzc =
        apzc->BuildOverscrollHandoffChain()->FindFirstScrollable(
            *aFirstInput, &mAllowedScrollDirections);
    if (scrollableApzc) {
      apzc = scrollableApzc;
    }
  }

  InputBlockState::SetConfirmedTargetApzc(apzc, aState, aFirstInput,
                                          aForScrollbarDrag);
  return true;
}

bool PanGestureBlockState::MustStayActive() { return !mInterrupted; }

const char* PanGestureBlockState::Type() { return "pan gesture"; }

bool PanGestureBlockState::SetContentResponse(bool aPreventDefault) {
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

bool PanGestureBlockState::HasReceivedAllContentNotifications() const {
  return CancelableBlockState::HasReceivedAllContentNotifications() &&
         !mWaitingForContentResponse;
}

bool PanGestureBlockState::IsReadyForHandling() const {
  if (!CancelableBlockState::IsReadyForHandling()) {
    return false;
  }
  return !mWaitingForContentResponse || IsContentResponseTimerExpired();
}

bool PanGestureBlockState::AllowScrollHandoff() const { return false; }

void PanGestureBlockState::SetNeedsToWaitForContentResponse(
    bool aWaitForContentResponse) {
  mWaitingForContentResponse = aWaitForContentResponse;
}

PinchGestureBlockState::PinchGestureBlockState(
    const RefPtr<AsyncPanZoomController>& aTargetApzc,
    TargetConfirmationFlags aFlags)
    : CancelableBlockState(aTargetApzc, aFlags),
      mInterrupted(false),
      mWaitingForContentResponse(false) {}

bool PinchGestureBlockState::MustStayActive() { return true; }

const char* PinchGestureBlockState::Type() { return "pinch gesture"; }

bool PinchGestureBlockState::SetContentResponse(bool aPreventDefault) {
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

bool PinchGestureBlockState::HasReceivedAllContentNotifications() const {
  return CancelableBlockState::HasReceivedAllContentNotifications() &&
         !mWaitingForContentResponse;
}

bool PinchGestureBlockState::IsReadyForHandling() const {
  if (!CancelableBlockState::IsReadyForHandling()) {
    return false;
  }
  return !mWaitingForContentResponse || IsContentResponseTimerExpired();
}

void PinchGestureBlockState::SetNeedsToWaitForContentResponse(
    bool aWaitForContentResponse) {
  mWaitingForContentResponse = aWaitForContentResponse;
}

TouchBlockState::TouchBlockState(
    const RefPtr<AsyncPanZoomController>& aTargetApzc,
    TargetConfirmationFlags aFlags, TouchCounter& aCounter)
    : CancelableBlockState(aTargetApzc, aFlags),
      mAllowedTouchBehaviorSet(false),
      mDuringFastFling(false),
      mSingleTapOccurred(false),
      mInSlop(false),
      mTouchCounter(aCounter),
      mStartTime(GetTargetApzc()->GetFrameTime().Time()) {
  TBS_LOG("Creating %p\n", this);
}

bool TouchBlockState::SetAllowedTouchBehaviors(
    const nsTArray<TouchBehaviorFlags>& aBehaviors) {
  if (mAllowedTouchBehaviorSet) {
    return false;
  }
  TBS_LOG("%p got allowed touch behaviours for %zu points\n", this,
          aBehaviors.Length());
  mAllowedTouchBehaviors.AppendElements(aBehaviors);
  mAllowedTouchBehaviorSet = true;
  return true;
}

bool TouchBlockState::GetAllowedTouchBehaviors(
    nsTArray<TouchBehaviorFlags>& aOutBehaviors) const {
  if (!mAllowedTouchBehaviorSet) {
    return false;
  }
  aOutBehaviors.AppendElements(mAllowedTouchBehaviors);
  return true;
}

bool TouchBlockState::HasAllowedTouchBehaviors() const {
  return mAllowedTouchBehaviorSet;
}

void TouchBlockState::CopyPropertiesFrom(const TouchBlockState& aOther) {
  TBS_LOG("%p copying properties from %p\n", this, &aOther);
  MOZ_ASSERT(aOther.mAllowedTouchBehaviorSet ||
             aOther.IsContentResponseTimerExpired());
  SetAllowedTouchBehaviors(aOther.mAllowedTouchBehaviors);
  mTransformToApzc = aOther.mTransformToApzc;
}

bool TouchBlockState::HasReceivedAllContentNotifications() const {
  return CancelableBlockState::HasReceivedAllContentNotifications()
         // See comment in TouchBlockState::IsReadyforHandling()
         && mAllowedTouchBehaviorSet;
}

bool TouchBlockState::IsReadyForHandling() const {
  if (!CancelableBlockState::IsReadyForHandling()) {
    return false;
  }

  return mAllowedTouchBehaviorSet || IsContentResponseTimerExpired();
}

void TouchBlockState::SetDuringFastFling() {
  TBS_LOG("%p setting fast-motion flag\n", this);
  mDuringFastFling = true;
}

bool TouchBlockState::IsDuringFastFling() const { return mDuringFastFling; }

void TouchBlockState::SetSingleTapOccurred() {
  TBS_LOG("%p setting single-tap-occurred flag\n", this);
  mSingleTapOccurred = true;
}

bool TouchBlockState::SingleTapOccurred() const { return mSingleTapOccurred; }

bool TouchBlockState::MustStayActive() { return true; }

const char* TouchBlockState::Type() { return "touch"; }

TimeDuration TouchBlockState::GetTimeSinceBlockStart() const {
  return GetTargetApzc()->GetFrameTime().Time() - mStartTime;
}

void TouchBlockState::DispatchEvent(const InputData& aEvent) const {
  MOZ_ASSERT(aEvent.mInputType == MULTITOUCH_INPUT);
  mTouchCounter.Update(aEvent.AsMultiTouchInput());
  CancelableBlockState::DispatchEvent(aEvent);
}

bool TouchBlockState::TouchActionAllowsPinchZoom() const {
  // Pointer events specification requires that all touch points allow zoom.
  for (auto& behavior : mAllowedTouchBehaviors) {
    if (!(behavior & AllowedTouchBehavior::PINCH_ZOOM)) {
      return false;
    }
  }
  return true;
}

bool TouchBlockState::TouchActionAllowsDoubleTapZoom() const {
  for (auto& behavior : mAllowedTouchBehaviors) {
    if (!(behavior & AllowedTouchBehavior::DOUBLE_TAP_ZOOM)) {
      return false;
    }
  }
  return true;
}

bool TouchBlockState::TouchActionAllowsPanningX() const {
  if (mAllowedTouchBehaviors.IsEmpty()) {
    // Default to allowed
    return true;
  }
  TouchBehaviorFlags flags = mAllowedTouchBehaviors[0];
  return (flags & AllowedTouchBehavior::HORIZONTAL_PAN);
}

bool TouchBlockState::TouchActionAllowsPanningY() const {
  if (mAllowedTouchBehaviors.IsEmpty()) {
    // Default to allowed
    return true;
  }
  TouchBehaviorFlags flags = mAllowedTouchBehaviors[0];
  return (flags & AllowedTouchBehavior::VERTICAL_PAN);
}

bool TouchBlockState::TouchActionAllowsPanningXY() const {
  if (mAllowedTouchBehaviors.IsEmpty()) {
    // Default to allowed
    return true;
  }
  TouchBehaviorFlags flags = mAllowedTouchBehaviors[0];
  return (flags & AllowedTouchBehavior::HORIZONTAL_PAN) &&
         (flags & AllowedTouchBehavior::VERTICAL_PAN);
}

bool TouchBlockState::UpdateSlopState(const MultiTouchInput& aInput,
                                      bool aApzcCanConsumeEvents) {
  if (aInput.mType == MultiTouchInput::MULTITOUCH_START) {
    // this is by definition the first event in this block. If it's the first
    // touch, then we enter a slop state.
    mInSlop = (aInput.mTouches.Length() == 1);
    if (mInSlop) {
      mSlopOrigin = aInput.mTouches[0].mScreenPoint;
      TBS_LOG("%p entering slop with origin %s\n", this,
              ToString(mSlopOrigin).c_str());
    }
    return false;
  }
  if (mInSlop) {
    ScreenCoord threshold = 0;
    // If the target was confirmed to null then the threshold doesn't
    // matter anyway since the events will never be processed.
    if (const RefPtr<AsyncPanZoomController>& apzc = GetTargetApzc()) {
      threshold = aApzcCanConsumeEvents ? apzc->GetTouchStartTolerance()
                                        : apzc->GetTouchMoveTolerance();
    }
    bool stayInSlop =
        (aInput.mType == MultiTouchInput::MULTITOUCH_MOVE) &&
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

bool TouchBlockState::IsInSlop() const { return mInSlop; }

Maybe<ScrollDirection> TouchBlockState::GetBestGuessPanDirection(
    const MultiTouchInput& aInput) {
  if (aInput.mType != MultiTouchInput::MULTITOUCH_MOVE ||
      aInput.mTouches.Length() != 1) {
    return Nothing();
  }
  ScreenPoint vector = aInput.mTouches[0].mScreenPoint - mSlopOrigin;
  double angle = atan2(vector.y, vector.x);  // range [-pi, pi]
  angle = fabs(angle);                       // range [0, pi]

  double angleThreshold = TouchActionAllowsPanningXY()
                              ? StaticPrefs::apz_axis_lock_lock_angle()
                              : StaticPrefs::apz_axis_lock_direct_pan_angle();
  if (apz::IsCloseToHorizontal(angle, angleThreshold)) {
    return Some(ScrollDirection::eHorizontal);
  }
  if (apz::IsCloseToVertical(angle, angleThreshold)) {
    return Some(ScrollDirection::eVertical);
  }
  return Nothing();
}

uint32_t TouchBlockState::GetActiveTouchCount() const {
  return mTouchCounter.GetActiveTouchCount();
}

KeyboardBlockState::KeyboardBlockState(
    const RefPtr<AsyncPanZoomController>& aTargetApzc)
    : InputBlockState(aTargetApzc, TargetConfirmationFlags{true}) {}

}  // namespace layers
}  // namespace mozilla
