/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputQueue.h"

#include "AsyncPanZoomController.h"
#include "gfxPrefs.h"
#include "InputBlockState.h"
#include "OverscrollHandoffState.h"

#define INPQ_LOG(...)
// #define INPQ_LOG(...) printf_stderr("INPQ: " __VA_ARGS__)

namespace mozilla {
namespace layers {

InputQueue::InputQueue()
  : mTouchBlockBalance(0)
{
}

InputQueue::~InputQueue() {
  mTouchBlockQueue.Clear();
}

nsEventStatus
InputQueue::ReceiveInputEvent(const nsRefPtr<AsyncPanZoomController>& aTarget, const InputData& aEvent) {
  AsyncPanZoomController::AssertOnControllerThread();

  if (aEvent.mInputType != MULTITOUCH_INPUT) {
    aTarget->HandleInputEvent(aEvent);
    // The return value for non-touch input isn't really used, so just return
    // ConsumeDoDefault for now. This can be changed later if needed.
    return nsEventStatus_eConsumeDoDefault;
  }

  TouchBlockState* block = nullptr;
  if (aEvent.AsMultiTouchInput().mType == MultiTouchInput::MULTITOUCH_START) {
    block = StartNewTouchBlock(aTarget, false);
    INPQ_LOG("%p started new touch block %p for target %p\n", this, block, aTarget.get());

    // We want to cancel animations here as soon as possible (i.e. without waiting for
    // content responses) because a finger has gone down and we don't want to keep moving
    // the content under the finger. However, to prevent "future" touchstart events from
    // interfering with "past" animations (i.e. from a previous touch block that is still
    // being processed) we only do this animation-cancellation if there are no older
    // touch blocks still in the queue.
    if (block == CurrentTouchBlock()) {
      if (block->GetOverscrollHandoffChain()->HasFastMovingApzc()) {
        // If we're already in a fast fling, then we want the touch event to stop the fling
        // and to disallow the touch event from being used as part of a fling.
        block->DisallowSingleTap();
      }
      block->GetOverscrollHandoffChain()->CancelAnimations();
    }

    if (aTarget->NeedToWaitForContent()) {
      // Content may intercept the touch events and prevent-default them. So we schedule
      // a timeout to give content time to do that.
      ScheduleContentResponseTimeout(aTarget);
    } else {
      // Content won't prevent-default this, so we can just pretend like we scheduled
      // a timeout and it expired. Note that we will still receive a ContentReceivedTouch
      // callback for this block, and so we need to make sure we adjust the touch balance.
      INPQ_LOG("%p not waiting for content response on block %p\n", this, block);
      mTouchBlockBalance++;
      block->TimeoutContentResponse();
    }
  } else if (mTouchBlockQueue.IsEmpty()) {
    NS_WARNING("Received a non-start touch event while no touch blocks active!");
  } else {
    // this touch is part of the most-recently created block
    block = mTouchBlockQueue.LastElement().get();
    INPQ_LOG("%p received new event in block %p\n", this, block);
  }

  if (!block) {
    return nsEventStatus_eIgnore;
  }

  nsEventStatus result = aTarget->ArePointerEventsConsumable(block, aEvent.AsMultiTouchInput().mTouches.Length())
      ? nsEventStatus_eConsumeDoDefault
      : nsEventStatus_eIgnore;

  if (block == CurrentTouchBlock() && block->IsReadyForHandling()) {
    INPQ_LOG("%p's current touch block is ready with preventdefault %d\n",
        this, block->IsDefaultPrevented());
    if (block->IsDefaultPrevented()) {
      return result;
    }
    aTarget->HandleInputEvent(aEvent);
    return result;
  }

  // Otherwise, add it to the queue for the touch block
  block->AddEvent(aEvent.AsMultiTouchInput());
  return result;
}

void
InputQueue::InjectNewTouchBlock(AsyncPanZoomController* aTarget)
{
  StartNewTouchBlock(aTarget, true);
  ScheduleContentResponseTimeout(aTarget);
}

TouchBlockState*
InputQueue::StartNewTouchBlock(const nsRefPtr<AsyncPanZoomController>& aTarget, bool aCopyAllowedTouchBehaviorFromCurrent)
{
  TouchBlockState* newBlock = new TouchBlockState(aTarget);
  if (gfxPrefs::TouchActionEnabled() && aCopyAllowedTouchBehaviorFromCurrent) {
    newBlock->CopyAllowedTouchBehaviorsFrom(*CurrentTouchBlock());
  }

  // We're going to start a new block, so clear out any depleted blocks at the head of the queue.
  // See corresponding comment in ProcessPendingInputBlocks.
  while (!mTouchBlockQueue.IsEmpty()) {
    if (mTouchBlockQueue[0]->IsReadyForHandling() && !mTouchBlockQueue[0]->HasEvents()) {
      INPQ_LOG("%p discarding depleted touch block %p\n", this, mTouchBlockQueue[0].get());
      mTouchBlockQueue.RemoveElementAt(0);
    } else {
      break;
    }
  }

  // Add the new block to the queue.
  mTouchBlockQueue.AppendElement(newBlock);
  return newBlock;
}

TouchBlockState*
InputQueue::CurrentTouchBlock() const
{
  AsyncPanZoomController::AssertOnControllerThread();

  MOZ_ASSERT(!mTouchBlockQueue.IsEmpty());
  return mTouchBlockQueue[0].get();
}

bool
InputQueue::HasReadyTouchBlock() const
{
  return !mTouchBlockQueue.IsEmpty() && mTouchBlockQueue[0]->IsReadyForHandling();
}

void
InputQueue::ScheduleContentResponseTimeout(const nsRefPtr<AsyncPanZoomController>& aTarget) {
  INPQ_LOG("%p scheduling content response timeout for target %p\n", this, aTarget.get());
  aTarget->PostDelayedTask(
    NewRunnableMethod(this, &InputQueue::ContentResponseTimeout),
    gfxPrefs::APZContentResponseTimeout());
}

void
InputQueue::ContentResponseTimeout() {
  AsyncPanZoomController::AssertOnControllerThread();

  mTouchBlockBalance++;
  INPQ_LOG("%p got a content response timeout; balance %d\n", this, mTouchBlockBalance);
  if (mTouchBlockBalance > 0) {
    // Find the first touch block in the queue that hasn't already received
    // the content response timeout callback, and notify it.
    bool found = false;
    for (size_t i = 0; i < mTouchBlockQueue.Length(); i++) {
      if (mTouchBlockQueue[i]->TimeoutContentResponse()) {
        found = true;
        break;
      }
    }
    if (found) {
      ProcessPendingInputBlocks();
    } else {
      NS_WARNING("INPQ received more ContentResponseTimeout calls than it has unprocessed touch blocks\n");
    }
  }
}

void
InputQueue::ContentReceivedTouch(bool aPreventDefault) {
  AsyncPanZoomController::AssertOnControllerThread();

  mTouchBlockBalance--;
  INPQ_LOG("%p got a content response; balance %d\n", this, mTouchBlockBalance);
  if (mTouchBlockBalance < 0) {
    // Find the first touch block in the queue that hasn't already received
    // its response from content, and notify it.
    bool found = false;
    for (size_t i = 0; i < mTouchBlockQueue.Length(); i++) {
      if (mTouchBlockQueue[i]->SetContentResponse(aPreventDefault)) {
        found = true;
        break;
      }
    }
    if (found) {
      ProcessPendingInputBlocks();
    } else {
      NS_WARNING("INPQ received more ContentReceivedTouch calls than it has unprocessed touch blocks\n");
    }
  }
}

void
InputQueue::SetAllowedTouchBehavior(const nsTArray<TouchBehaviorFlags>& aBehaviors) {
  AsyncPanZoomController::AssertOnControllerThread();

  bool found = false;
  for (size_t i = 0; i < mTouchBlockQueue.Length(); i++) {
    if (mTouchBlockQueue[i]->SetAllowedTouchBehaviors(aBehaviors)) {
      found = true;
      break;
    }
  }
  if (found) {
    ProcessPendingInputBlocks();
  } else {
    NS_WARNING("INPQ received more SetAllowedTouchBehavior calls than it has unprocessed touch blocks\n");
  }
}

void
InputQueue::ProcessPendingInputBlocks() {
  AsyncPanZoomController::AssertOnControllerThread();

  while (true) {
    TouchBlockState* curBlock = CurrentTouchBlock();
    if (!curBlock->IsReadyForHandling()) {
      break;
    }

    INPQ_LOG("%p processing input block %p; preventDefault %d target %p\n",
        this, curBlock, curBlock->IsDefaultPrevented(),
        curBlock->GetTargetApzc().get());
    nsRefPtr<AsyncPanZoomController> target = curBlock->GetTargetApzc();
    if (curBlock->IsDefaultPrevented()) {
      curBlock->DropEvents();
      target->ResetInputState();
    } else {
      while (curBlock->HasEvents()) {
        target->HandleInputEvent(curBlock->RemoveFirstEvent());
      }
    }
    MOZ_ASSERT(!curBlock->HasEvents());

    if (mTouchBlockQueue.Length() == 1) {
      // If |curBlock| is the only touch block in the queue, then it is still
      // active and we cannot remove it yet. We only know that a touch block is
      // over when we start the next one. This block will be removed by the code
      // in StartNewTouchBlock, where new touch blocks are added.
      break;
    }

    // If we get here, we know there are more touch blocks in the queue after
    // |curBlock|, so we can remove |curBlock| and try to process the next one.
    INPQ_LOG("%p discarding depleted touch block %p\n", this, curBlock);
    mTouchBlockQueue.RemoveElementAt(0);
  }
}

} // namespace layers
} // namespace mozilla
