/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_InputQueue_h
#define mozilla_layers_InputQueue_h

#include "mozilla/EventForwards.h"
#include "mozilla/UniquePtr.h"
#include "nsAutoPtr.h"
#include "nsTArray.h"

namespace mozilla {

class InputData;

namespace layers {

class AsyncPanZoomController;
class OverscrollHandoffChain;
class TouchBlockState;

/**
 * This class stores incoming input events, separated into "input blocks", until
 * they are ready for handling. Currently input blocks are only created from
 * touch input.
 */
class InputQueue {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(InputQueue)

public:
  typedef uint32_t TouchBehaviorFlags;

public:
  InputQueue();

  /**
   * Notifies the InputQueue of a new incoming input event. The APZC that the
   * input event was targeted to should be provided in the |aTarget| parameter.
   * See the documentation on APZCTreeManager::ReceiveInputEvent for info on
   * return values from this function.
   */
  nsEventStatus ReceiveInputEvent(const nsRefPtr<AsyncPanZoomController>& aTarget, const InputData& aEvent);
  /**
   * This function should be invoked to notify the InputQueue when web content
   * decides whether or not it wants to cancel a block of events. This
   * automatically gets applied to the next block of events that has not yet
   * been responded to. This function MUST be invoked exactly once for each
   * touch block, after the touch-start event that creates the block is sent to
   * ReceiveInputEvent.
   */
  void ContentReceivedTouch(bool aPreventDefault);
  /**
   * This function should be invoked to notify the InputQueue of the touch-
   * action properties for the different touch points in an input block. This
   * automatically gets applied to the next block of events that has not yet
   * received a touch behaviour notification. This function MUST be invoked
   * exactly once for each touch block, after the touch-start event that creates
   * the block is sent to ReceiveInputEvent. If touch-action is not enabled on
   * the platform, this function does nothing and need not be called.
   */
  void SetAllowedTouchBehavior(const nsTArray<TouchBehaviorFlags>& aBehaviors);
  /**
   * Adds a new touch block at the end of the input queue that has the same
   * allowed touch behaviour flags as the the touch block currently being
   * processed. This should only be called when processing of a touch block
   * triggers the creation of a new touch block.
   */
  void InjectNewTouchBlock(AsyncPanZoomController* aTarget);
  /**
   * Returns the touch block at the head of the queue.
   */
  TouchBlockState* CurrentTouchBlock() const;
  /**
   * Returns true iff the touch block at the head of the queue is ready for
   * handling.
   */
  bool HasReadyTouchBlock() const;

private:
  ~InputQueue();
  TouchBlockState* StartNewTouchBlock(const nsRefPtr<AsyncPanZoomController>& aTarget, bool aCopyAllowedTouchBehaviorFromCurrent);
  void ScheduleContentResponseTimeout(const nsRefPtr<AsyncPanZoomController>& aTarget);
  void ContentResponseTimeout();
  void ProcessPendingInputBlocks();

private:
  // The queue of touch blocks that have not yet been processed.
  // This member must only be accessed on the controller/UI thread.
  nsTArray<UniquePtr<TouchBlockState>> mTouchBlockQueue;

  // This variable requires some explanation. Strap yourself in.
  //
  // For each block of events, we do two things: (1) send the events to gecko and expect
  // exactly one call to ContentReceivedTouch in return, and (2) kick off a timeout
  // that triggers in case we don't hear from web content in a timely fashion.
  // Since events are constantly coming in, we need to be able to handle more than one
  // block of input events sitting in the queue.
  //
  // There are ordering restrictions on events that we can take advantage of, and that
  // we need to abide by. Blocks of events in the queue will always be in the order that
  // the user generated them. Responses we get from content will be in the same order as
  // as the blocks of events in the queue. The timeout callbacks that have been posted
  // will also fire in the same order as the blocks of events in the queue.
  // HOWEVER, we may get multiple responses from content interleaved with multiple
  // timeout expirations, and that interleaving is not predictable.
  //
  // Therefore, we need to make sure that for each block of events, we process the queued
  // events exactly once, either when we get the response from content, or when the
  // timeout expires (whichever happens first). There is no way to associate the timeout
  // or response from content with a particular block of events other than via ordering.
  //
  // So, what we do to accomplish this is to track a "touch block balance", which is the
  // number of timeout expirations that have fired, minus the number of content responses
  // that have been received. (Think "balance" as in teeter-totter balance). This
  // value is:
  // - zero when we are in a state where the next content response we expect to receive
  //   and the next timeout expiration we expect to fire both correspond to the next
  //   unprocessed block of events in the queue.
  // - negative when we are in a state where we have received more content responses than
  //   timeout expirations. This means that the next content repsonse we receive will
  //   correspond to the first unprocessed block, but the next n timeout expirations need
  //   to be ignored as they are for blocks we have already processed. (n is the absolute
  //   value of the balance.)
  // - positive when we are in a state where we have received more timeout expirations
  //   than content responses. This means that the next timeout expiration that we will
  //   receive will correspond to the first unprocessed block, but the next n content
  //   responses need to be ignored as they are for blocks we have already processed.
  //   (n is the absolute value of the balance.)
  //
  // Note that each touch block internally carries flags that indicate whether or not it
  // has received a content response and/or timeout expiration. However, we cannot rely
  // on that alone to deliver these notifications to the right input block, because
  // once an input block has been processed, it can potentially be removed from the queue.
  // Therefore the information in that block is lost. An alternative approach would
  // be to keep around those blocks until they have received both the content response
  // and timeout expiration, but that involves a higher level of memory usage.
  //
  // This member must only be accessed on the controller/UI thread.
  int32_t mTouchBlockBalance;
};

}
}

#endif // mozilla_layers_InputQueue_h
