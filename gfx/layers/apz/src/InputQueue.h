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
   * return values from this function, including |aOutInputBlockId|.
   */
  nsEventStatus ReceiveInputEvent(const nsRefPtr<AsyncPanZoomController>& aTarget,
                                  bool aTargetConfirmed,
                                  const InputData& aEvent,
                                  uint64_t* aOutInputBlockId);
  /**
   * This function should be invoked to notify the InputQueue when web content
   * decides whether or not it wants to cancel a block of events. The block
   * id to which this applies should be provided in |aInputBlockId|.
   */
  void ContentReceivedTouch(uint64_t aInputBlockId, bool aPreventDefault);
  /**
   * This function should be invoked to notify the InputQueue once the target
   * APZC to handle an input block has been confirmed. In practice this should
   * generally be decidable upon receipt of the input event, but in some cases
   * we may need to query the layout engine to know for sure. The input block
   * this applies to should be specified via the |aInputBlockId| parameter.
   */
  void SetConfirmedTargetApzc(uint64_t aInputBlockId, const nsRefPtr<AsyncPanZoomController>& aTargetApzc);
  /**
   * This function should be invoked to notify the InputQueue of the touch-
   * action properties for the different touch points in an input block. The
   * input block this applies to should be specified by the |aInputBlockId|
   * parameter. If touch-action is not enabled on the platform, this function
   * does nothing and need not be called.
   */
  void SetAllowedTouchBehavior(uint64_t aInputBlockId, const nsTArray<TouchBehaviorFlags>& aBehaviors);
  /**
   * Adds a new touch block at the end of the input queue that has the same
   * allowed touch behaviour flags as the the touch block currently being
   * processed. This should only be called when processing of a touch block
   * triggers the creation of a new touch block. Returns the input block id
   * of the the newly-created block.
   */
  uint64_t InjectNewTouchBlock(AsyncPanZoomController* aTarget);
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
  TouchBlockState* StartNewTouchBlock(const nsRefPtr<AsyncPanZoomController>& aTarget,
                                      bool aTargetConfirmed,
                                      bool aCopyAllowedTouchBehaviorFromCurrent);
  void ScheduleMainThreadTimeout(const nsRefPtr<AsyncPanZoomController>& aTarget, uint64_t aInputBlockId);
  void MainThreadTimeout(const uint64_t& aInputBlockId);
  void ProcessPendingInputBlocks();

private:
  // The queue of touch blocks that have not yet been processed.
  // This member must only be accessed on the controller/UI thread.
  nsTArray<UniquePtr<TouchBlockState>> mTouchBlockQueue;
};

}
}

#endif // mozilla_layers_InputQueue_h
