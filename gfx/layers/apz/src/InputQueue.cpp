/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputQueue.h"

#include "AsyncPanZoomController.h"
#include "gfxPrefs.h"
#include "InputBlockState.h"
#include "LayersLogging.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "OverscrollHandoffState.h"
#include "QueuedInput.h"

#define INPQ_LOG(...)
// #define INPQ_LOG(...) printf_stderr("INPQ: " __VA_ARGS__)

namespace mozilla {
namespace layers {

InputQueue::InputQueue() = default;

InputQueue::~InputQueue() { mQueuedInputs.Clear(); }

nsEventStatus InputQueue::ReceiveInputEvent(
    const RefPtr<AsyncPanZoomController>& aTarget,
    TargetConfirmationFlags aFlags, const InputData& aEvent,
    uint64_t* aOutInputBlockId,
    const Maybe<nsTArray<TouchBehaviorFlags>>& aTouchBehaviors) {
  APZThreadUtils::AssertOnControllerThread();

  AutoRunImmediateTimeout timeoutRunner{this};

  switch (aEvent.mInputType) {
    case MULTITOUCH_INPUT: {
      const MultiTouchInput& event = aEvent.AsMultiTouchInput();
      return ReceiveTouchInput(aTarget, aFlags, event, aOutInputBlockId,
                               aTouchBehaviors);
    }

    case SCROLLWHEEL_INPUT: {
      const ScrollWheelInput& event = aEvent.AsScrollWheelInput();
      return ReceiveScrollWheelInput(aTarget, aFlags, event, aOutInputBlockId);
    }

    case PANGESTURE_INPUT: {
      const PanGestureInput& event = aEvent.AsPanGestureInput();
      return ReceivePanGestureInput(aTarget, aFlags, event, aOutInputBlockId);
    }

    case MOUSE_INPUT: {
      const MouseInput& event = aEvent.AsMouseInput();
      return ReceiveMouseInput(aTarget, aFlags, event, aOutInputBlockId);
    }

    case KEYBOARD_INPUT: {
      // Every keyboard input must have a confirmed target
      MOZ_ASSERT(aTarget && aFlags.mTargetConfirmed);

      const KeyboardInput& event = aEvent.AsKeyboardInput();
      return ReceiveKeyboardInput(aTarget, event, aOutInputBlockId);
    }

    default:
      // The return value for non-touch input is only used by tests, so just
      // pass through the return value for now. This can be changed later if
      // needed.
      // TODO (bug 1098430): we will eventually need to have smarter handling
      // for non-touch events as well.
      return aTarget->HandleInputEvent(aEvent, aTarget->GetTransformToThis());
  }
}

nsEventStatus InputQueue::ReceiveTouchInput(
    const RefPtr<AsyncPanZoomController>& aTarget,
    TargetConfirmationFlags aFlags, const MultiTouchInput& aEvent,
    uint64_t* aOutInputBlockId,
    const Maybe<nsTArray<TouchBehaviorFlags>>& aTouchBehaviors) {
  TouchBlockState* block = nullptr;
  if (aEvent.mType == MultiTouchInput::MULTITOUCH_START) {
    nsTArray<TouchBehaviorFlags> currentBehaviors;
    bool haveBehaviors = false;
    if (!gfxPrefs::TouchActionEnabled()) {
      haveBehaviors = true;
    } else if (mActiveTouchBlock) {
      haveBehaviors =
          mActiveTouchBlock->GetAllowedTouchBehaviors(currentBehaviors);
      // If the behaviours aren't set, but the main-thread response timer on
      // the block is expired we still treat it as though it has behaviors,
      // because in that case we still want to interrupt the fast-fling and
      // use the default behaviours.
      haveBehaviors |= mActiveTouchBlock->IsContentResponseTimerExpired();
    }

    block = StartNewTouchBlock(aTarget, aFlags, false);
    INPQ_LOG("started new touch block %p id %" PRIu64 " for target %p\n", block,
             block->GetBlockId(), aTarget.get());

    // XXX using the chain from |block| here may be wrong in cases where the
    // target isn't confirmed and the real target turns out to be something
    // else. For now assume this is rare enough that it's not an issue.
    if (mQueuedInputs.IsEmpty() && aEvent.mTouches.Length() == 1 &&
        block->GetOverscrollHandoffChain()->HasFastFlungApzc() &&
        haveBehaviors) {
      // If we're already in a fast fling, and a single finger goes down, then
      // we want special handling for the touch event, because it shouldn't get
      // delivered to content. Note that we don't set this flag when going
      // from a fast fling to a pinch state (i.e. second finger goes down while
      // the first finger is moving).
      block->SetDuringFastFling();
      block->SetConfirmedTargetApzc(
          aTarget, InputBlockState::TargetConfirmationState::eConfirmed,
          nullptr /* the block was just created so it has no events */,
          false /* not a scrollbar drag */);
      if (gfxPrefs::TouchActionEnabled()) {
        block->SetAllowedTouchBehaviors(currentBehaviors);
      }
      INPQ_LOG("block %p tagged as fast-motion\n", block);
    } else if (aTouchBehaviors) {
      // If this block isn't started during a fast-fling, and APZCTM has
      // provided touch behavior information, then put it on the block so
      // that the ArePointerEventsConsumable call below can use it.
      block->SetAllowedTouchBehaviors(*aTouchBehaviors);
    }

    CancelAnimationsForNewBlock(block);

    MaybeRequestContentResponse(aTarget, block);
  } else {
    // for touch inputs that don't start a block, APZCTM shouldn't be giving
    // us any touch behaviors.
    MOZ_ASSERT(aTouchBehaviors.isNothing());

    block = mActiveTouchBlock.get();
    if (!block) {
      NS_WARNING(
          "Received a non-start touch event while no touch blocks active!");
      return nsEventStatus_eIgnore;
    }

    INPQ_LOG("received new event in block %p\n", block);
  }

  if (aOutInputBlockId) {
    *aOutInputBlockId = block->GetBlockId();
  }

  // Note that the |aTarget| the APZCTM sent us may contradict the confirmed
  // target set on the block. In this case the confirmed target (which may be
  // null) should take priority. This is equivalent to just always using the
  // target (confirmed or not) from the block.
  RefPtr<AsyncPanZoomController> target = block->GetTargetApzc();

  nsEventStatus result = nsEventStatus_eIgnore;

  // XXX calling ArePointerEventsConsumable on |target| may be wrong here if
  // the target isn't confirmed and the real target turns out to be something
  // else. For now assume this is rare enough that it's not an issue.
  if (block->IsDuringFastFling()) {
    INPQ_LOG("dropping event due to block %p being in fast motion\n", block);
    result = nsEventStatus_eConsumeNoDefault;
  } else if (target && target->ArePointerEventsConsumable(block, aEvent)) {
    if (block->UpdateSlopState(aEvent, true)) {
      INPQ_LOG("dropping event due to block %p being in slop\n", block);
      result = nsEventStatus_eConsumeNoDefault;
    } else {
      result = nsEventStatus_eConsumeDoDefault;
    }
  } else if (block->UpdateSlopState(aEvent, false)) {
    INPQ_LOG("dropping event due to block %p being in mini-slop\n", block);
    result = nsEventStatus_eConsumeNoDefault;
  }
  mQueuedInputs.AppendElement(MakeUnique<QueuedInput>(aEvent, *block));
  ProcessQueue();
  return result;
}

nsEventStatus InputQueue::ReceiveMouseInput(
    const RefPtr<AsyncPanZoomController>& aTarget,
    TargetConfirmationFlags aFlags, const MouseInput& aEvent,
    uint64_t* aOutInputBlockId) {
  // On a new mouse down we can have a new target so we must force a new block
  // with a new target.
  bool newBlock = DragTracker::StartsDrag(aEvent);

  DragBlockState* block = newBlock ? nullptr : mActiveDragBlock.get();
  if (block && block->HasReceivedMouseUp()) {
    block = nullptr;
  }

  if (!block && mDragTracker.InDrag()) {
    // If there's no current drag block, but we're getting a move with a button
    // down, we need to start a new drag block because we're obviously already
    // in the middle of a drag (it probably got interrupted by something else).
    INPQ_LOG(
        "got a drag event outside a drag block, need to create a block to hold "
        "it\n");
    newBlock = true;
  }

  mDragTracker.Update(aEvent);

  if (!newBlock && !block) {
    // This input event is not in a drag block, so we're not doing anything
    // with it, return eIgnore.
    return nsEventStatus_eIgnore;
  }

  if (!block) {
    MOZ_ASSERT(newBlock);
    block = new DragBlockState(aTarget, aFlags, aEvent);

    INPQ_LOG("started new drag block %p id %" PRIu64
             " for %sconfirmed target %p\n",
             block, block->GetBlockId(), aFlags.mTargetConfirmed ? "" : "un",
             aTarget.get());

    mActiveDragBlock = block;

    CancelAnimationsForNewBlock(block);
    MaybeRequestContentResponse(aTarget, block);
  }

  if (aOutInputBlockId) {
    *aOutInputBlockId = block->GetBlockId();
  }

  mQueuedInputs.AppendElement(MakeUnique<QueuedInput>(aEvent, *block));
  ProcessQueue();

  if (DragTracker::EndsDrag(aEvent)) {
    block->MarkMouseUpReceived();
  }

  // The event is part of a drag block and could potentially cause
  // scrolling, so return DoDefault.
  return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus InputQueue::ReceiveScrollWheelInput(
    const RefPtr<AsyncPanZoomController>& aTarget,
    TargetConfirmationFlags aFlags, const ScrollWheelInput& aEvent,
    uint64_t* aOutInputBlockId) {
  WheelBlockState* block = mActiveWheelBlock.get();
  // If the block is not accepting new events we'll create a new input block
  // (and therefore a new wheel transaction).
  if (block &&
      (!block->ShouldAcceptNewEvent() || block->MaybeTimeout(aEvent))) {
    block = nullptr;
  }

  MOZ_ASSERT(!block || block->InTransaction());

  if (!block) {
    block = new WheelBlockState(aTarget, aFlags, aEvent);
    INPQ_LOG("started new scroll wheel block %p id %" PRIu64 " for target %p\n",
             block, block->GetBlockId(), aTarget.get());

    mActiveWheelBlock = block;

    CancelAnimationsForNewBlock(block, ExcludeWheel);
    MaybeRequestContentResponse(aTarget, block);
  } else {
    INPQ_LOG("received new event in block %p\n", block);
  }

  if (aOutInputBlockId) {
    *aOutInputBlockId = block->GetBlockId();
  }

  // Note that the |aTarget| the APZCTM sent us may contradict the confirmed
  // target set on the block. In this case the confirmed target (which may be
  // null) should take priority. This is equivalent to just always using the
  // target (confirmed or not) from the block, which is what
  // ProcessQueue() does.
  mQueuedInputs.AppendElement(MakeUnique<QueuedInput>(aEvent, *block));

  // The WheelBlockState needs to affix a counter to the event before we process
  // it. Note that the counter is affixed to the copy in the queue rather than
  // |aEvent|.
  block->Update(mQueuedInputs.LastElement()->Input()->AsScrollWheelInput());

  ProcessQueue();

  return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus InputQueue::ReceiveKeyboardInput(
    const RefPtr<AsyncPanZoomController>& aTarget, const KeyboardInput& aEvent,
    uint64_t* aOutInputBlockId) {
  KeyboardBlockState* block = mActiveKeyboardBlock.get();

  // If the block is targeting a different Apzc than this keyboard event then
  // we'll create a new input block
  if (block && block->GetTargetApzc() != aTarget) {
    block = nullptr;
  }

  if (!block) {
    block = new KeyboardBlockState(aTarget);
    INPQ_LOG("started new keyboard block %p id %" PRIu64 " for target %p\n",
             block, block->GetBlockId(), aTarget.get());

    mActiveKeyboardBlock = block;
  } else {
    INPQ_LOG("received new event in block %p\n", block);
  }

  if (aOutInputBlockId) {
    *aOutInputBlockId = block->GetBlockId();
  }

  mQueuedInputs.AppendElement(MakeUnique<QueuedInput>(aEvent, *block));

  ProcessQueue();

  // If APZ is allowing passive listeners then we must dispatch the event to
  // content, otherwise we can consume the event.
  return gfxPrefs::APZKeyboardPassiveListeners()
             ? nsEventStatus_eConsumeDoDefault
             : nsEventStatus_eConsumeNoDefault;
}

static bool CanScrollTargetHorizontally(const PanGestureInput& aInitialEvent,
                                        PanGestureBlockState* aBlock) {
  PanGestureInput horizontalComponent = aInitialEvent;
  horizontalComponent.mPanDisplacement.y = 0;
  ScrollDirections allowedScrollDirections;
  RefPtr<AsyncPanZoomController> horizontallyScrollableAPZC =
      aBlock->GetOverscrollHandoffChain()->FindFirstScrollable(
          horizontalComponent, &allowedScrollDirections);
  return horizontallyScrollableAPZC &&
         horizontallyScrollableAPZC == aBlock->GetTargetApzc() &&
         allowedScrollDirections.contains(ScrollDirection::eHorizontal);
}

nsEventStatus InputQueue::ReceivePanGestureInput(
    const RefPtr<AsyncPanZoomController>& aTarget,
    TargetConfirmationFlags aFlags, const PanGestureInput& aEvent,
    uint64_t* aOutInputBlockId) {
  if (aEvent.mType == PanGestureInput::PANGESTURE_MAYSTART ||
      aEvent.mType == PanGestureInput::PANGESTURE_CANCELLED) {
    // Ignore these events for now.
    return nsEventStatus_eConsumeDoDefault;
  }

  PanGestureBlockState* block = nullptr;
  if (aEvent.mType != PanGestureInput::PANGESTURE_START) {
    block = mActivePanGestureBlock.get();
  }

  PanGestureInput event = aEvent;
  nsEventStatus result = nsEventStatus_eConsumeDoDefault;

  if (!block || block->WasInterrupted()) {
    if (event.mType != PanGestureInput::PANGESTURE_START) {
      // Only PANGESTURE_START events are allowed to start a new pan gesture
      // block, but we really want to start a new block here, so we magically
      // turn this input into a PANGESTURE_START.
      INPQ_LOG(
          "transmogrifying pan input %d to PANGESTURE_START for new block\n",
          event.mType);
      event.mType = PanGestureInput::PANGESTURE_START;
    }
    block = new PanGestureBlockState(aTarget, aFlags, event);
    INPQ_LOG("started new pan gesture block %p id %" PRIu64 " for target %p\n",
             block, block->GetBlockId(), aTarget.get());

    if (aFlags.mTargetConfirmed &&
        event
            .mRequiresContentResponseIfCannotScrollHorizontallyInStartDirection &&
        !CanScrollTargetHorizontally(event, block)) {
      // This event may trigger a swipe gesture, depending on what our caller
      // wants to do it. We need to suspend handling of this block until we get
      // a content response which will tell us whether to proceed or abort the
      // block.
      block->SetNeedsToWaitForContentResponse(true);

      // Inform our caller that we haven't scrolled in response to the event
      // and that a swipe can be started from this event if desired.
      result = nsEventStatus_eIgnore;
    }

    mActivePanGestureBlock = block;

    CancelAnimationsForNewBlock(block);
    MaybeRequestContentResponse(aTarget, block);
  } else {
    INPQ_LOG("received new event in block %p\n", block);
  }

  if (aOutInputBlockId) {
    *aOutInputBlockId = block->GetBlockId();
  }

  // Note that the |aTarget| the APZCTM sent us may contradict the confirmed
  // target set on the block. In this case the confirmed target (which may be
  // null) should take priority. This is equivalent to just always using the
  // target (confirmed or not) from the block, which is what
  // ProcessQueue() does.
  mQueuedInputs.AppendElement(MakeUnique<QueuedInput>(event, *block));
  ProcessQueue();

  return result;
}

void InputQueue::CancelAnimationsForNewBlock(InputBlockState* aBlock,
                                             CancelAnimationFlags aExtraFlags) {
  // We want to cancel animations here as soon as possible (i.e. without waiting
  // for content responses) because a finger has gone down and we don't want to
  // keep moving the content under the finger. However, to prevent "future"
  // touchstart events from interfering with "past" animations (i.e. from a
  // previous touch block that is still being processed) we only do this
  // animation-cancellation if there are no older touch blocks still in the
  // queue.
  if (mQueuedInputs.IsEmpty()) {
    aBlock->GetOverscrollHandoffChain()->CancelAnimations(
        aExtraFlags | ExcludeOverscroll | ScrollSnap);
  }
}

void InputQueue::MaybeRequestContentResponse(
    const RefPtr<AsyncPanZoomController>& aTarget,
    CancelableBlockState* aBlock) {
  bool waitForMainThread = false;
  if (aBlock->IsTargetConfirmed()) {
    // Content won't prevent-default this, so we can just set the flag directly.
    INPQ_LOG("not waiting for content response on block %p\n", aBlock);
    aBlock->SetContentResponse(false);
  } else {
    waitForMainThread = true;
  }
  if (aBlock->AsTouchBlock() &&
      !aBlock->AsTouchBlock()->HasAllowedTouchBehaviors()) {
    INPQ_LOG("waiting for main thread touch-action info on block %p\n", aBlock);
    waitForMainThread = true;
  }
  if (waitForMainThread) {
    // We either don't know for sure if aTarget is the right APZC, or we may
    // need to wait to give content the opportunity to prevent-default the
    // touch events. Either way we schedule a timeout so the main thread stuff
    // can run.
    ScheduleMainThreadTimeout(aTarget, aBlock);
  }
}

uint64_t InputQueue::InjectNewTouchBlock(AsyncPanZoomController* aTarget) {
  AutoRunImmediateTimeout timeoutRunner{this};
  TouchBlockState* block =
      StartNewTouchBlock(aTarget, TargetConfirmationFlags{true},
                         /* aCopyPropertiesFromCurrent = */ true);
  INPQ_LOG("injecting new touch block %p with id %" PRIu64 " and target %p\n",
           block, block->GetBlockId(), aTarget);
  ScheduleMainThreadTimeout(aTarget, block);
  return block->GetBlockId();
}

TouchBlockState* InputQueue::StartNewTouchBlock(
    const RefPtr<AsyncPanZoomController>& aTarget,
    TargetConfirmationFlags aFlags, bool aCopyPropertiesFromCurrent) {
  TouchBlockState* newBlock =
      new TouchBlockState(aTarget, aFlags, mTouchCounter);
  if (aCopyPropertiesFromCurrent) {
    // We should never enter here without a current touch block, because this
    // codepath is invoked from the OnLongPress handler in
    // AsyncPanZoomController, which should bail out if there is no current
    // touch block.
    MOZ_ASSERT(GetCurrentTouchBlock());
    newBlock->CopyPropertiesFrom(*GetCurrentTouchBlock());
  }

  mActiveTouchBlock = newBlock;
  return newBlock;
}

InputBlockState* InputQueue::GetCurrentBlock() const {
  APZThreadUtils::AssertOnControllerThread();
  return mQueuedInputs.IsEmpty() ? nullptr : mQueuedInputs[0]->Block();
}

TouchBlockState* InputQueue::GetCurrentTouchBlock() const {
  InputBlockState* block = GetCurrentBlock();
  return block ? block->AsTouchBlock() : mActiveTouchBlock.get();
}

WheelBlockState* InputQueue::GetCurrentWheelBlock() const {
  InputBlockState* block = GetCurrentBlock();
  return block ? block->AsWheelBlock() : mActiveWheelBlock.get();
}

DragBlockState* InputQueue::GetCurrentDragBlock() const {
  InputBlockState* block = GetCurrentBlock();
  return block ? block->AsDragBlock() : mActiveDragBlock.get();
}

PanGestureBlockState* InputQueue::GetCurrentPanGestureBlock() const {
  InputBlockState* block = GetCurrentBlock();
  return block ? block->AsPanGestureBlock() : mActivePanGestureBlock.get();
}

KeyboardBlockState* InputQueue::GetCurrentKeyboardBlock() const {
  InputBlockState* block = GetCurrentBlock();
  return block ? block->AsKeyboardBlock() : mActiveKeyboardBlock.get();
}

WheelBlockState* InputQueue::GetActiveWheelTransaction() const {
  WheelBlockState* block = mActiveWheelBlock.get();
  if (!block || !block->InTransaction()) {
    return nullptr;
  }
  return block;
}

bool InputQueue::HasReadyTouchBlock() const {
  return !mQueuedInputs.IsEmpty() &&
         mQueuedInputs[0]->Block()->AsTouchBlock() &&
         mQueuedInputs[0]->Block()->AsTouchBlock()->IsReadyForHandling();
}

bool InputQueue::AllowScrollHandoff() const {
  if (GetCurrentWheelBlock()) {
    return GetCurrentWheelBlock()->AllowScrollHandoff();
  }
  if (GetCurrentPanGestureBlock()) {
    return GetCurrentPanGestureBlock()->AllowScrollHandoff();
  }
  if (GetCurrentKeyboardBlock()) {
    return GetCurrentKeyboardBlock()->AllowScrollHandoff();
  }
  return true;
}

bool InputQueue::IsDragOnScrollbar(bool aHitScrollbar) {
  if (!mDragTracker.InDrag()) {
    return false;
  }
  // Now that we know we are in a drag, get the info from the drag tracker.
  // We keep it in the tracker rather than the block because the block can get
  // interrupted by something else (like a wheel event) and then a new block
  // will get created without the info we want. The tracker will persist though.
  return mDragTracker.IsOnScrollbar(aHitScrollbar);
}

void InputQueue::ScheduleMainThreadTimeout(
    const RefPtr<AsyncPanZoomController>& aTarget,
    CancelableBlockState* aBlock) {
  INPQ_LOG("scheduling main thread timeout for target %p\n", aTarget.get());
  aBlock->StartContentResponseTimer();
  RefPtr<Runnable> timeoutTask = NewRunnableMethod<uint64_t>(
      "layers::InputQueue::MainThreadTimeout", this,
      &InputQueue::MainThreadTimeout, aBlock->GetBlockId());
  int32_t timeout = gfxPrefs::APZContentResponseTimeout();
  if (timeout == 0) {
    // If the timeout is zero, treat it as a request to ignore any main
    // thread confirmation and unconditionally use fallback behaviour for
    // when a timeout is reached. This codepath is used by tests that
    // want to exercise the fallback behaviour.
    // To ensure the fallback behaviour is used unconditionally, the timeout
    // is run right away instead of using PostDelayedTask(). However,
    // we can't run it right here, because MainThreadTimeout() expects that
    // the input block has at least one input event in mQueuedInputs, and
    // the event that triggered this call may not have been added to
    // mQueuedInputs yet.
    mImmediateTimeout = timeoutTask.forget();
  } else {
    aTarget->PostDelayedTask(timeoutTask.forget(), timeout);
  }
}

InputBlockState* InputQueue::FindBlockForId(uint64_t aInputBlockId,
                                            InputData** aOutFirstInput) {
  for (const auto& queuedInput : mQueuedInputs) {
    if (queuedInput->Block()->GetBlockId() == aInputBlockId) {
      if (aOutFirstInput) {
        *aOutFirstInput = queuedInput->Input();
      }
      return queuedInput->Block();
    }
  }

  InputBlockState* block = nullptr;
  if (mActiveTouchBlock && mActiveTouchBlock->GetBlockId() == aInputBlockId) {
    block = mActiveTouchBlock.get();
  } else if (mActiveWheelBlock &&
             mActiveWheelBlock->GetBlockId() == aInputBlockId) {
    block = mActiveWheelBlock.get();
  } else if (mActiveDragBlock &&
             mActiveDragBlock->GetBlockId() == aInputBlockId) {
    block = mActiveDragBlock.get();
  } else if (mActivePanGestureBlock &&
             mActivePanGestureBlock->GetBlockId() == aInputBlockId) {
    block = mActivePanGestureBlock.get();
  } else if (mActiveKeyboardBlock &&
             mActiveKeyboardBlock->GetBlockId() == aInputBlockId) {
    block = mActiveKeyboardBlock.get();
  }
  // Since we didn't encounter this block while iterating through mQueuedInputs,
  // it must have no events associated with it at the moment.
  if (aOutFirstInput) {
    *aOutFirstInput = nullptr;
  }
  return block;
}

void InputQueue::MainThreadTimeout(uint64_t aInputBlockId) {
  APZThreadUtils::AssertOnControllerThread();

  INPQ_LOG("got a main thread timeout; block=%" PRIu64 "\n", aInputBlockId);
  bool success = false;
  InputData* firstInput = nullptr;
  InputBlockState* inputBlock = FindBlockForId(aInputBlockId, &firstInput);
  if (inputBlock && inputBlock->AsCancelableBlock()) {
    CancelableBlockState* block = inputBlock->AsCancelableBlock();
    // time out the touch-listener response and also confirm the existing
    // target apzc in the case where the main thread doesn't get back to us
    // fast enough.
    success = block->TimeoutContentResponse();
    success |= block->SetConfirmedTargetApzc(
        block->GetTargetApzc(),
        InputBlockState::TargetConfirmationState::eTimedOut, firstInput,
        // This actually could be a scrollbar drag, but we pass
        // aForScrollbarDrag=false because for scrollbar drags,
        // SetConfirmedTargetApzc() will also be called by ConfirmDragBlock(),
        // and we pass aForScrollbarDrag=true there.
        false);
  } else if (inputBlock) {
    NS_WARNING("input block is not a cancelable block");
  }
  if (success) {
    ProcessQueue();
  }
}

void InputQueue::ContentReceivedInputBlock(uint64_t aInputBlockId,
                                           bool aPreventDefault) {
  APZThreadUtils::AssertOnControllerThread();

  INPQ_LOG("got a content response; block=%" PRIu64 "\n", aInputBlockId);
  bool success = false;
  InputBlockState* inputBlock = FindBlockForId(aInputBlockId, nullptr);
  if (inputBlock && inputBlock->AsCancelableBlock()) {
    CancelableBlockState* block = inputBlock->AsCancelableBlock();
    success = block->SetContentResponse(aPreventDefault);
    block->RecordContentResponseTime();
  } else if (inputBlock) {
    NS_WARNING("input block is not a cancelable block");
  }
  if (success) {
    ProcessQueue();
  }
}

void InputQueue::SetConfirmedTargetApzc(
    uint64_t aInputBlockId, const RefPtr<AsyncPanZoomController>& aTargetApzc) {
  APZThreadUtils::AssertOnControllerThread();

  INPQ_LOG("got a target apzc; block=%" PRIu64 " guid=%s\n", aInputBlockId,
           aTargetApzc ? Stringify(aTargetApzc->GetGuid()).c_str() : "");
  bool success = false;
  InputData* firstInput = nullptr;
  InputBlockState* inputBlock = FindBlockForId(aInputBlockId, &firstInput);
  if (inputBlock && inputBlock->AsCancelableBlock()) {
    CancelableBlockState* block = inputBlock->AsCancelableBlock();
    success = block->SetConfirmedTargetApzc(
        aTargetApzc, InputBlockState::TargetConfirmationState::eConfirmed,
        firstInput,
        // This actually could be a scrollbar drag, but we pass
        // aForScrollbarDrag=false because for scrollbar drags,
        // SetConfirmedTargetApzc() will also be called by ConfirmDragBlock(),
        // and we pass aForScrollbarDrag=true there.
        false);
    block->RecordContentResponseTime();
  } else if (inputBlock) {
    NS_WARNING("input block is not a cancelable block");
  }
  if (success) {
    ProcessQueue();
  }
}

void InputQueue::ConfirmDragBlock(
    uint64_t aInputBlockId, const RefPtr<AsyncPanZoomController>& aTargetApzc,
    const AsyncDragMetrics& aDragMetrics) {
  APZThreadUtils::AssertOnControllerThread();

  INPQ_LOG("got a target apzc; block=%" PRIu64 " guid=%s dragtarget=%" PRIu64
           "\n",
           aInputBlockId,
           aTargetApzc ? Stringify(aTargetApzc->GetGuid()).c_str() : "",
           aDragMetrics.mViewId);
  bool success = false;
  InputData* firstInput = nullptr;
  InputBlockState* inputBlock = FindBlockForId(aInputBlockId, &firstInput);
  if (inputBlock && inputBlock->AsDragBlock()) {
    DragBlockState* block = inputBlock->AsDragBlock();
    block->SetDragMetrics(aDragMetrics);
    success = block->SetConfirmedTargetApzc(
        aTargetApzc, InputBlockState::TargetConfirmationState::eConfirmed,
        firstInput,
        /* aForScrollbarDrag = */ true);
    block->RecordContentResponseTime();
  }
  if (success) {
    ProcessQueue();
  }
}

void InputQueue::SetAllowedTouchBehavior(
    uint64_t aInputBlockId, const nsTArray<TouchBehaviorFlags>& aBehaviors) {
  APZThreadUtils::AssertOnControllerThread();

  INPQ_LOG("got allowed touch behaviours; block=%" PRIu64 "\n", aInputBlockId);
  bool success = false;
  InputBlockState* inputBlock = FindBlockForId(aInputBlockId, nullptr);
  if (inputBlock && inputBlock->AsTouchBlock()) {
    TouchBlockState* block = inputBlock->AsTouchBlock();
    success = block->SetAllowedTouchBehaviors(aBehaviors);
    block->RecordContentResponseTime();
  } else if (inputBlock) {
    NS_WARNING("input block is not a touch block");
  }
  if (success) {
    ProcessQueue();
  }
}

void InputQueue::ProcessQueue() {
  APZThreadUtils::AssertOnControllerThread();

  while (!mQueuedInputs.IsEmpty()) {
    InputBlockState* curBlock = mQueuedInputs[0]->Block();
    CancelableBlockState* cancelable = curBlock->AsCancelableBlock();
    if (cancelable && !cancelable->IsReadyForHandling()) {
      break;
    }

    INPQ_LOG(
        "processing input from block %p; preventDefault %d shouldDropEvents %d "
        "target %p\n",
        curBlock, cancelable && cancelable->IsDefaultPrevented(),
        curBlock->ShouldDropEvents(), curBlock->GetTargetApzc().get());
    RefPtr<AsyncPanZoomController> target = curBlock->GetTargetApzc();
    // target may be null here if the initial target was unconfirmed and then
    // we later got a confirmed null target. in that case drop the events.
    if (target) {
      if (curBlock->ShouldDropEvents()) {
        if (curBlock->AsTouchBlock()) {
          target->ResetTouchInputState();
        }
      } else {
        UpdateActiveApzc(target);
        curBlock->DispatchEvent(*(mQueuedInputs[0]->Input()));
      }
    }
    mQueuedInputs.RemoveElementAt(0);
  }

  if (CanDiscardBlock(mActiveTouchBlock)) {
    mActiveTouchBlock = nullptr;
  }
  if (CanDiscardBlock(mActiveWheelBlock)) {
    mActiveWheelBlock = nullptr;
  }
  if (CanDiscardBlock(mActiveDragBlock)) {
    mActiveDragBlock = nullptr;
  }
  if (CanDiscardBlock(mActivePanGestureBlock)) {
    mActivePanGestureBlock = nullptr;
  }
  if (CanDiscardBlock(mActiveKeyboardBlock)) {
    mActiveKeyboardBlock = nullptr;
  }
}

bool InputQueue::CanDiscardBlock(InputBlockState* aBlock) {
  if (!aBlock ||
      (aBlock->AsCancelableBlock() &&
       !aBlock->AsCancelableBlock()->IsReadyForHandling()) ||
      aBlock->MustStayActive()) {
    return false;
  }
  InputData* firstInput = nullptr;
  FindBlockForId(aBlock->GetBlockId(), &firstInput);
  if (firstInput) {
    // The block has at least one input event still in the queue, so it's
    // not depleted
    return false;
  }
  return true;
}

void InputQueue::UpdateActiveApzc(
    const RefPtr<AsyncPanZoomController>& aNewActive) {
  if (mLastActiveApzc && mLastActiveApzc != aNewActive &&
      mTouchCounter.GetActiveTouchCount() > 0) {
    mLastActiveApzc->ResetTouchInputState();
  }
  mLastActiveApzc = aNewActive;
}

void InputQueue::Clear() {
  APZThreadUtils::AssertOnControllerThread();

  mQueuedInputs.Clear();
  mActiveTouchBlock = nullptr;
  mActiveWheelBlock = nullptr;
  mActiveDragBlock = nullptr;
  mActivePanGestureBlock = nullptr;
  mActiveKeyboardBlock = nullptr;
  mLastActiveApzc = nullptr;
}

InputQueue::AutoRunImmediateTimeout::AutoRunImmediateTimeout(InputQueue* aQueue)
    : mQueue(aQueue) {
  MOZ_ASSERT(!mQueue->mImmediateTimeout);
}

InputQueue::AutoRunImmediateTimeout::~AutoRunImmediateTimeout() {
  if (mQueue->mImmediateTimeout) {
    mQueue->mImmediateTimeout->Run();
    mQueue->mImmediateTimeout = nullptr;
  }
}

}  // namespace layers
}  // namespace mozilla
