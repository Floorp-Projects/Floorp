/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
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

#define INPQ_LOG(...)
// #define INPQ_LOG(...) printf_stderr("INPQ: " __VA_ARGS__)

namespace mozilla {
namespace layers {

InputQueue::InputQueue()
{
}

InputQueue::~InputQueue() {
  mInputBlockQueue.Clear();
}

nsEventStatus
InputQueue::ReceiveInputEvent(const RefPtr<AsyncPanZoomController>& aTarget,
                              bool aTargetConfirmed,
                              const InputData& aEvent,
                              uint64_t* aOutInputBlockId) {
  APZThreadUtils::AssertOnControllerThread();

  switch (aEvent.mInputType) {
    case MULTITOUCH_INPUT: {
      const MultiTouchInput& event = aEvent.AsMultiTouchInput();
      return ReceiveTouchInput(aTarget, aTargetConfirmed, event, aOutInputBlockId);
    }

    case SCROLLWHEEL_INPUT: {
      const ScrollWheelInput& event = aEvent.AsScrollWheelInput();
      return ReceiveScrollWheelInput(aTarget, aTargetConfirmed, event, aOutInputBlockId);
    }

    case PANGESTURE_INPUT: {
      const PanGestureInput& event = aEvent.AsPanGestureInput();
      return ReceivePanGestureInput(aTarget, aTargetConfirmed, event, aOutInputBlockId);
    }

    case MOUSE_INPUT: {
      const MouseInput& event = aEvent.AsMouseInput();
      return ReceiveMouseInput(aTarget, aTargetConfirmed, event, aOutInputBlockId);
    }

    default:
      // The return value for non-touch input is only used by tests, so just pass
      // through the return value for now. This can be changed later if needed.
      // TODO (bug 1098430): we will eventually need to have smarter handling for
      // non-touch events as well.
      return aTarget->HandleInputEvent(aEvent, aTarget->GetTransformToThis());
  }
}

bool
InputQueue::MaybeHandleCurrentBlock(CancelableBlockState *block,
                                    const InputData& aEvent) {
  if (block == CurrentBlock() && block->IsReadyForHandling()) {
    const RefPtr<AsyncPanZoomController>& target = block->GetTargetApzc();
    INPQ_LOG("current block is ready with target %p preventdefault %d\n",
        target.get(), block->IsDefaultPrevented());
    if (!target || block->IsDefaultPrevented()) {
      return true;
    }
    UpdateActiveApzc(block->GetTargetApzc());
    block->DispatchImmediate(aEvent);
    return true;
  }
  return false;
}

nsEventStatus
InputQueue::ReceiveTouchInput(const RefPtr<AsyncPanZoomController>& aTarget,
                              bool aTargetConfirmed,
                              const MultiTouchInput& aEvent,
                              uint64_t* aOutInputBlockId) {
  TouchBlockState* block = nullptr;
  if (aEvent.mType == MultiTouchInput::MULTITOUCH_START) {
    nsTArray<TouchBehaviorFlags> currentBehaviors;
    bool haveBehaviors = false;
    if (!gfxPrefs::TouchActionEnabled()) {
      haveBehaviors = true;
    } else if (!mInputBlockQueue.IsEmpty() && CurrentBlock()->AsTouchBlock()) {
      haveBehaviors = CurrentTouchBlock()->GetAllowedTouchBehaviors(currentBehaviors);
      // If the behaviours aren't set, but the main-thread response timer on
      // the block is expired we still treat it as though it has behaviors,
      // because in that case we still want to interrupt the fast-fling and
      // use the default behaviours.
      haveBehaviors |= CurrentTouchBlock()->IsContentResponseTimerExpired();
    }

    block = StartNewTouchBlock(aTarget, aTargetConfirmed, false);
    INPQ_LOG("started new touch block %p id %" PRIu64 " for target %p\n",
        block, block->GetBlockId(), aTarget.get());

    // XXX using the chain from |block| here may be wrong in cases where the
    // target isn't confirmed and the real target turns out to be something
    // else. For now assume this is rare enough that it's not an issue.
    if (block == CurrentBlock() &&
        aEvent.mTouches.Length() == 1 &&
        block->GetOverscrollHandoffChain()->HasFastFlungApzc() &&
        haveBehaviors) {
      // If we're already in a fast fling, and a single finger goes down, then
      // we want special handling for the touch event, because it shouldn't get
      // delivered to content. Note that we don't set this flag when going
      // from a fast fling to a pinch state (i.e. second finger goes down while
      // the first finger is moving).
      block->SetDuringFastFling();
      block->SetConfirmedTargetApzc(aTarget,
          InputBlockState::TargetConfirmationState::eConfirmed);
      if (gfxPrefs::TouchActionEnabled()) {
        block->SetAllowedTouchBehaviors(currentBehaviors);
      }
      INPQ_LOG("block %p tagged as fast-motion\n", block);
    }

    CancelAnimationsForNewBlock(block);

    MaybeRequestContentResponse(aTarget, block);
  } else {
    if (!mInputBlockQueue.IsEmpty()) {
      block = mInputBlockQueue.LastElement().get()->AsTouchBlock();
    }

    if (!block) {
      NS_WARNING("Received a non-start touch event while no touch blocks active!");
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
  } else if (target && target->ArePointerEventsConsumable(block, aEvent.AsMultiTouchInput().mTouches.Length())) {
    if (block->UpdateSlopState(aEvent.AsMultiTouchInput(), true)) {
      INPQ_LOG("dropping event due to block %p being in slop\n", block);
      result = nsEventStatus_eConsumeNoDefault;
    } else {
      result = nsEventStatus_eConsumeDoDefault;
    }
  } else if (block->UpdateSlopState(aEvent.AsMultiTouchInput(), false)) {
    INPQ_LOG("dropping event due to block %p being in mini-slop\n", block);
    result = nsEventStatus_eConsumeNoDefault;
  }
  if (!MaybeHandleCurrentBlock(block, aEvent)) {
    block->AddEvent(aEvent.AsMultiTouchInput());
  }
  return result;
}

nsEventStatus
InputQueue::ReceiveMouseInput(const RefPtr<AsyncPanZoomController>& aTarget,
                              bool aTargetConfirmed,
                              const MouseInput& aEvent,
                              uint64_t* aOutInputBlockId) {
  // On a new mouse down we can have a new target so we must force a new block
  // with a new target.
  bool newBlock = DragTracker::StartsDrag(aEvent);

  DragBlockState* block = nullptr;
  if (!newBlock && !mInputBlockQueue.IsEmpty()) {
    block = mInputBlockQueue.LastElement()->AsDragBlock();
  }

  if (block && block->HasReceivedMouseUp()) {
    block = nullptr;
  }

  if (!block && mDragTracker.InDrag()) {
    // If there's no current drag block, but we're getting a move with a button
    // down, we need to start a new drag block because we're obviously already
    // in the middle of a drag (it probably got interrupted by something else).
    INPQ_LOG("got a drag event outside a drag block, need to create a block to hold it\n");
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
    block = new DragBlockState(aTarget, aTargetConfirmed, aEvent);

    INPQ_LOG("started new drag block %p id %" PRIu64 " for %sconfirmed target %p\n",
        block, block->GetBlockId(), aTargetConfirmed ? "" : "un", aTarget.get());

    SweepDepletedBlocks();
    mInputBlockQueue.AppendElement(block);

    CancelAnimationsForNewBlock(block);
    MaybeRequestContentResponse(aTarget, block);
  }

  if (aOutInputBlockId) {
    *aOutInputBlockId = block->GetBlockId();
  }

  if (!MaybeHandleCurrentBlock(block, aEvent)) {
    block->AddEvent(aEvent.AsMouseInput());
  }

  if (DragTracker::EndsDrag(aEvent)) {
    block->MarkMouseUpReceived();
  }

  // The event is part of a drag block and could potentially cause
  // scrolling, so return DoDefault.
  return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus
InputQueue::ReceiveScrollWheelInput(const RefPtr<AsyncPanZoomController>& aTarget,
                                    bool aTargetConfirmed,
                                    const ScrollWheelInput& aEvent,
                                    uint64_t* aOutInputBlockId) {
  WheelBlockState* block = nullptr;
  if (!mInputBlockQueue.IsEmpty()) {
    block = mInputBlockQueue.LastElement()->AsWheelBlock();

    // If the block is not accepting new events we'll create a new input block
    // (and therefore a new wheel transaction).
    if (block &&
        (!block->ShouldAcceptNewEvent() ||
         block->MaybeTimeout(aEvent)))
    {
      block = nullptr;
    }
  }

  MOZ_ASSERT(!block || block->InTransaction());

  if (!block) {
    block = new WheelBlockState(aTarget, aTargetConfirmed, aEvent);
    INPQ_LOG("started new scroll wheel block %p id %" PRIu64 " for target %p\n",
        block, block->GetBlockId(), aTarget.get());

    SweepDepletedBlocks();
    mInputBlockQueue.AppendElement(block);

    CancelAnimationsForNewBlock(block);
    MaybeRequestContentResponse(aTarget, block);
  } else {
    INPQ_LOG("received new event in block %p\n", block);
  }

  if (aOutInputBlockId) {
    *aOutInputBlockId = block->GetBlockId();
  }

  // Copy the event, since WheelBlockState needs to affix a counter.
  ScrollWheelInput event(aEvent);
  block->Update(event);

  // Note that the |aTarget| the APZCTM sent us may contradict the confirmed
  // target set on the block. In this case the confirmed target (which may be
  // null) should take priority. This is equivalent to just always using the
  // target (confirmed or not) from the block, which is what
  // MaybeHandleCurrentBlock() does.
  if (!MaybeHandleCurrentBlock(block, event)) {
    block->AddEvent(event);
  }

  return nsEventStatus_eConsumeDoDefault;
}

static bool
CanScrollTargetHorizontally(const PanGestureInput& aInitialEvent,
                            PanGestureBlockState* aBlock)
{
  PanGestureInput horizontalComponent = aInitialEvent;
  horizontalComponent.mPanDisplacement.y = 0;
  RefPtr<AsyncPanZoomController> horizontallyScrollableAPZC =
    aBlock->GetOverscrollHandoffChain()->FindFirstScrollable(horizontalComponent);
  return horizontallyScrollableAPZC && horizontallyScrollableAPZC == aBlock->GetTargetApzc();
}

nsEventStatus
InputQueue::ReceivePanGestureInput(const RefPtr<AsyncPanZoomController>& aTarget,
                                   bool aTargetConfirmed,
                                   const PanGestureInput& aEvent,
                                   uint64_t* aOutInputBlockId) {
  if (aEvent.mType == PanGestureInput::PANGESTURE_MAYSTART ||
      aEvent.mType == PanGestureInput::PANGESTURE_CANCELLED) {
    // Ignore these events for now.
    return nsEventStatus_eConsumeDoDefault;
  }

  PanGestureBlockState* block = nullptr;
  if (!mInputBlockQueue.IsEmpty() &&
      aEvent.mType != PanGestureInput::PANGESTURE_START) {
    block = mInputBlockQueue.LastElement()->AsPanGestureBlock();
  }

  PanGestureInput event = aEvent;
  nsEventStatus result = nsEventStatus_eConsumeDoDefault;

  if (!block || block->WasInterrupted()) {
    if (event.mType != PanGestureInput::PANGESTURE_START) {
      // Only PANGESTURE_START events are allowed to start a new pan gesture
      // block, but we really want to start a new block here, so we magically
      // turn this input into a PANGESTURE_START.
      INPQ_LOG("transmogrifying pan input %d to PANGESTURE_START for new block\n",
          event.mType);
      event.mType = PanGestureInput::PANGESTURE_START;
    }
    block = new PanGestureBlockState(aTarget, aTargetConfirmed, event);
    INPQ_LOG("started new pan gesture block %p id %" PRIu64 " for target %p\n",
        block, block->GetBlockId(), aTarget.get());

    if (aTargetConfirmed &&
        event.mRequiresContentResponseIfCannotScrollHorizontallyInStartDirection &&
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

    SweepDepletedBlocks();
    mInputBlockQueue.AppendElement(block);

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
  // MaybeHandleCurrentBlock() does.
  if (!MaybeHandleCurrentBlock(block, event)) {
    block->AddEvent(event.AsPanGestureInput());
  }

  return result;
}

void
InputQueue::CancelAnimationsForNewBlock(CancelableBlockState* aBlock)
{
  // We want to cancel animations here as soon as possible (i.e. without waiting for
  // content responses) because a finger has gone down and we don't want to keep moving
  // the content under the finger. However, to prevent "future" touchstart events from
  // interfering with "past" animations (i.e. from a previous touch block that is still
  // being processed) we only do this animation-cancellation if there are no older
  // touch blocks still in the queue.
  if (aBlock == CurrentBlock()) {
    aBlock->GetOverscrollHandoffChain()->CancelAnimations(ExcludeOverscroll | ScrollSnap);
  }
}

void
InputQueue::MaybeRequestContentResponse(const RefPtr<AsyncPanZoomController>& aTarget,
                                        CancelableBlockState* aBlock)
{
  bool waitForMainThread = false;
  if (aBlock->IsTargetConfirmed()) {
    // Content won't prevent-default this, so we can just set the flag directly.
    INPQ_LOG("not waiting for content response on block %p\n", aBlock);
    aBlock->SetContentResponse(false);
  } else {
    waitForMainThread = true;
  }
  if (aBlock->AsTouchBlock() && gfxPrefs::TouchActionEnabled()) {
    // waitForMainThread is set to true unconditionally here, but if the APZCTM
    // has the touch-action behaviours for this block, it will set it
    // immediately after we unwind out of this ReceiveInputEvent call. So even
    // though we are scheduling the main-thread timeout, we might end up not
    // waiting.
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

uint64_t
InputQueue::InjectNewTouchBlock(AsyncPanZoomController* aTarget)
{
  TouchBlockState* block = StartNewTouchBlock(aTarget,
    /* aTargetConfirmed = */ true,
    /* aCopyPropertiesFromCurrent = */ true);
  INPQ_LOG("injecting new touch block %p with id %" PRIu64 " and target %p\n",
    block, block->GetBlockId(), aTarget);
  ScheduleMainThreadTimeout(aTarget, block);
  return block->GetBlockId();
}

void
InputQueue::SweepDepletedBlocks()
{
  // We're going to start a new block, so clear out any depleted blocks at the head of the queue.
  // See corresponding comment in ProcessInputBlocks.
  while (!mInputBlockQueue.IsEmpty()) {
    CancelableBlockState* block = mInputBlockQueue[0].get();
    if (!block->IsReadyForHandling() || block->HasEvents()) {
      break;
    }

    INPQ_LOG("discarding depleted %s block %p\n", block->Type(), block);
    mInputBlockQueue.RemoveElementAt(0);
  }
}

TouchBlockState*
InputQueue::StartNewTouchBlock(const RefPtr<AsyncPanZoomController>& aTarget,
                               bool aTargetConfirmed,
                               bool aCopyPropertiesFromCurrent)
{
  TouchBlockState* newBlock = new TouchBlockState(aTarget, aTargetConfirmed,
      mTouchCounter);
  if (aCopyPropertiesFromCurrent) {
    newBlock->CopyPropertiesFrom(*CurrentTouchBlock());
  }

  SweepDepletedBlocks();

  // Add the new block to the queue.
  mInputBlockQueue.AppendElement(newBlock);
  return newBlock;
}

CancelableBlockState*
InputQueue::CurrentBlock() const
{
  APZThreadUtils::AssertOnControllerThread();

  MOZ_ASSERT(!mInputBlockQueue.IsEmpty());
  return mInputBlockQueue[0].get();
}

TouchBlockState*
InputQueue::CurrentTouchBlock() const
{
  TouchBlockState* block = CurrentBlock()->AsTouchBlock();
  MOZ_ASSERT(block);
  return block;
}

WheelBlockState*
InputQueue::CurrentWheelBlock() const
{
  WheelBlockState* block = CurrentBlock()->AsWheelBlock();
  MOZ_ASSERT(block);
  return block;
}

DragBlockState*
InputQueue::CurrentDragBlock() const
{
  DragBlockState* block = CurrentBlock()->AsDragBlock();
  MOZ_ASSERT(block);
  return block;
}

PanGestureBlockState*
InputQueue::CurrentPanGestureBlock() const
{
  PanGestureBlockState* block = CurrentBlock()->AsPanGestureBlock();
  MOZ_ASSERT(block);
  return block;
}

WheelBlockState*
InputQueue::GetCurrentWheelTransaction() const
{
  if (mInputBlockQueue.IsEmpty()) {
    return nullptr;
  }
  WheelBlockState* block = CurrentBlock()->AsWheelBlock();
  if (!block || !block->InTransaction()) {
    return nullptr;
  }
  return block;
}

bool
InputQueue::HasReadyTouchBlock() const
{
  return !mInputBlockQueue.IsEmpty() &&
         mInputBlockQueue[0]->AsTouchBlock() &&
         mInputBlockQueue[0]->IsReadyForHandling();
}

bool
InputQueue::AllowScrollHandoff() const
{
  MOZ_ASSERT(CurrentBlock());
  if (CurrentBlock()->AsWheelBlock()) {
    return CurrentBlock()->AsWheelBlock()->AllowScrollHandoff();
  }
  if (CurrentBlock()->AsPanGestureBlock()) {
    return CurrentBlock()->AsPanGestureBlock()->AllowScrollHandoff();
  }
  return true;
}

bool
InputQueue::IsDragOnScrollbar(bool aHitScrollbar)
{
  if (!mDragTracker.InDrag()) {
    return false;
  }
  // Now that we know we are in a drag, get the info from the drag tracker.
  // We keep it in the tracker rather than the block because the block can get
  // interrupted by something else (like a wheel event) and then a new block
  // will get created without the info we want. The tracker will persist though.
  return mDragTracker.IsOnScrollbar(aHitScrollbar);
}

void
InputQueue::ScheduleMainThreadTimeout(const RefPtr<AsyncPanZoomController>& aTarget,
                                      CancelableBlockState* aBlock) {
  INPQ_LOG("scheduling main thread timeout for target %p\n", aTarget.get());
  aBlock->StartContentResponseTimer();
  aTarget->PostDelayedTask(NewRunnableMethod<uint64_t>(this,
                                                       &InputQueue::MainThreadTimeout,
                                                       aBlock->GetBlockId()),
                           gfxPrefs::APZContentResponseTimeout());
}

void
InputQueue::MainThreadTimeout(const uint64_t& aInputBlockId) {
  APZThreadUtils::AssertOnControllerThread();

  INPQ_LOG("got a main thread timeout; block=%" PRIu64 "\n", aInputBlockId);
  bool success = false;
  for (size_t i = 0; i < mInputBlockQueue.Length(); i++) {
    if (mInputBlockQueue[i]->GetBlockId() == aInputBlockId) {
      // time out the touch-listener response and also confirm the existing
      // target apzc in the case where the main thread doesn't get back to us
      // fast enough.
      success = mInputBlockQueue[i]->TimeoutContentResponse();
      success |= mInputBlockQueue[i]->SetConfirmedTargetApzc(
          mInputBlockQueue[i]->GetTargetApzc(),
          InputBlockState::TargetConfirmationState::eTimedOut);
      break;
    }
  }
  if (success) {
    ProcessInputBlocks();
  }
}

void
InputQueue::ContentReceivedInputBlock(uint64_t aInputBlockId, bool aPreventDefault) {
  APZThreadUtils::AssertOnControllerThread();

  INPQ_LOG("got a content response; block=%" PRIu64 "\n", aInputBlockId);
  bool success = false;
  for (size_t i = 0; i < mInputBlockQueue.Length(); i++) {
    CancelableBlockState* block = mInputBlockQueue[i].get();
    if (block->GetBlockId() == aInputBlockId) {
      success = block->SetContentResponse(aPreventDefault);
      block->RecordContentResponseTime();
      break;
    }
  }
  if (success) {
    ProcessInputBlocks();
  }
}

void
InputQueue::SetConfirmedTargetApzc(uint64_t aInputBlockId, const RefPtr<AsyncPanZoomController>& aTargetApzc) {
  APZThreadUtils::AssertOnControllerThread();

  INPQ_LOG("got a target apzc; block=%" PRIu64 " guid=%s\n",
    aInputBlockId, aTargetApzc ? Stringify(aTargetApzc->GetGuid()).c_str() : "");
  bool success = false;
  for (size_t i = 0; i < mInputBlockQueue.Length(); i++) {
    CancelableBlockState* block = mInputBlockQueue[i].get();
    if (block->GetBlockId() == aInputBlockId) {
      success = block->SetConfirmedTargetApzc(aTargetApzc,
          InputBlockState::TargetConfirmationState::eConfirmed);
      block->RecordContentResponseTime();
      break;
    }
  }
  if (success) {
    ProcessInputBlocks();
  }
}

void
InputQueue::ConfirmDragBlock(uint64_t aInputBlockId, const RefPtr<AsyncPanZoomController>& aTargetApzc,
                                   const AsyncDragMetrics& aDragMetrics)
{
  APZThreadUtils::AssertOnControllerThread();

  INPQ_LOG("got a target apzc; block=%" PRIu64 " guid=%s\n",
    aInputBlockId, aTargetApzc ? Stringify(aTargetApzc->GetGuid()).c_str() : "");
  bool success = false;
  for (size_t i = 0; i < mInputBlockQueue.Length(); i++) {
    DragBlockState* block = mInputBlockQueue[i]->AsDragBlock();
    if (block && block->GetBlockId() == aInputBlockId) {
      block->SetDragMetrics(aDragMetrics);
      success = block->SetConfirmedTargetApzc(aTargetApzc,
          InputBlockState::TargetConfirmationState::eConfirmed);
      block->RecordContentResponseTime();
      break;
    }
  }
  if (success) {
    ProcessInputBlocks();
  }
}

void
InputQueue::SetAllowedTouchBehavior(uint64_t aInputBlockId, const nsTArray<TouchBehaviorFlags>& aBehaviors) {
  APZThreadUtils::AssertOnControllerThread();

  INPQ_LOG("got allowed touch behaviours; block=%" PRIu64 "\n", aInputBlockId);
  bool success = false;
  for (size_t i = 0; i < mInputBlockQueue.Length(); i++) {
    if (mInputBlockQueue[i]->GetBlockId() == aInputBlockId) {
      TouchBlockState *block = mInputBlockQueue[i]->AsTouchBlock();
      if (block) {
        success = block->SetAllowedTouchBehaviors(aBehaviors);
        block->RecordContentResponseTime();
      } else {
        NS_WARNING("input block is not a touch block");
      }
      break;
    }
  }
  if (success) {
    ProcessInputBlocks();
  }
}

void
InputQueue::ProcessInputBlocks() {
  APZThreadUtils::AssertOnControllerThread();

  do {
    CancelableBlockState* curBlock = CurrentBlock();
    if (!curBlock->IsReadyForHandling()) {
      break;
    }

    INPQ_LOG("processing input block %p; preventDefault %d target %p\n",
        curBlock, curBlock->IsDefaultPrevented(),
        curBlock->GetTargetApzc().get());
    RefPtr<AsyncPanZoomController> target = curBlock->GetTargetApzc();
    // target may be null here if the initial target was unconfirmed and then
    // we later got a confirmed null target. in that case drop the events.
    if (!target) {
      curBlock->DropEvents();
    } else if (curBlock->IsDefaultPrevented()) {
      curBlock->DropEvents();
      if (curBlock->AsTouchBlock()) {
        target->ResetTouchInputState();
      }
    } else {
      UpdateActiveApzc(curBlock->GetTargetApzc());
      curBlock->HandleEvents();
    }
    MOZ_ASSERT(!curBlock->HasEvents());

    if (mInputBlockQueue.Length() == 1 && curBlock->MustStayActive()) {
      // Some types of blocks (e.g. touch blocks) accumulate events until the
      // next input block is started. Therefore we cannot remove the block from
      // the queue until we have started another block. This block will be
      // removed by SweepDeletedBlocks() whenever a new block is added.
      break;
    }

    // If we get here, we know there are more touch blocks in the queue after
    // |curBlock|, so we can remove |curBlock| and try to process the next one.
    INPQ_LOG("discarding processed %s block %p\n", curBlock->Type(), curBlock);
    mInputBlockQueue.RemoveElementAt(0);
  } while (!mInputBlockQueue.IsEmpty());
}

void
InputQueue::UpdateActiveApzc(const RefPtr<AsyncPanZoomController>& aNewActive) {
  if (mLastActiveApzc && mLastActiveApzc != aNewActive
      && mTouchCounter.GetActiveTouchCount() > 0) {
    mLastActiveApzc->ResetTouchInputState();
  }
  mLastActiveApzc = aNewActive;
}

void
InputQueue::Clear()
{
  APZThreadUtils::AssertOnControllerThread();

  mInputBlockQueue.Clear();
  mLastActiveApzc = nullptr;
}

} // namespace layers
} // namespace mozilla
