/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_InputBlockState_h
#define mozilla_layers_InputBlockState_h

#include "InputData.h"                      // for MultiTouchInput
#include "mozilla/gfx/Matrix.h"             // for Matrix4x4
#include "nsAutoPtr.h"                      // for nsRefPtr
#include "nsTArray.h"                       // for nsTArray

namespace mozilla {
namespace layers {

class AsyncPanZoomController;
class OverscrollHandoffChain;
class CancelableBlockState;
class TouchBlockState;
class WheelBlockState;
class PanGestureBlockState;

/**
 * A base class that stores state common to various input blocks.
 * Currently, it just stores the overscroll handoff chain.
 * Note that the InputBlockState constructor acquires the tree lock, so callers
 * from inside AsyncPanZoomController should ensure that the APZC lock is not
 * held.
 */
class InputBlockState
{
public:
  static const uint64_t NO_BLOCK_ID = 0;

  explicit InputBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                           bool aTargetConfirmed);
  virtual ~InputBlockState()
  {}

  virtual bool SetConfirmedTargetApzc(const nsRefPtr<AsyncPanZoomController>& aTargetApzc);
  const nsRefPtr<AsyncPanZoomController>& GetTargetApzc() const;
  const nsRefPtr<const OverscrollHandoffChain>& GetOverscrollHandoffChain() const;
  uint64_t GetBlockId() const;

  bool IsTargetConfirmed() const;

protected:
  virtual void UpdateTargetApzc(const nsRefPtr<AsyncPanZoomController>& aTargetApzc);

private:
  nsRefPtr<AsyncPanZoomController> mTargetApzc;
  bool mTargetConfirmed;
  const uint64_t mBlockId;
protected:
  nsRefPtr<const OverscrollHandoffChain> mOverscrollHandoffChain;

  // Used to transform events from global screen space to |mTargetApzc|'s
  // screen space. It's cached at the beginning of the input block so that
  // all events in the block are in the same coordinate space.
  gfx::Matrix4x4 mTransformToApzc;
};

/**
 * This class represents a set of events that can be cancelled by web content
 * via event listeners.
 *
 * Each cancelable input block can be cancelled by web content, and
 * this information is stored in the mPreventDefault flag. Because web
 * content runs on the Gecko main thread, we cannot always wait for web content's
 * response. Instead, there is a timeout that sets this flag in the case
 * where web content doesn't respond in time. The mContentResponded
 * and mContentResponseTimerExpired flags indicate which of these scenarios
 * occurred.
 */
class CancelableBlockState : public InputBlockState
{
public:
  CancelableBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                       bool aTargetConfirmed);

  virtual TouchBlockState *AsTouchBlock() {
    return nullptr;
  }
  virtual WheelBlockState *AsWheelBlock() {
    return nullptr;
  }
  virtual PanGestureBlockState *AsPanGestureBlock() {
    return nullptr;
  }

  /**
   * Record whether or not content cancelled this block of events.
   * @param aPreventDefault true iff the block is cancelled.
   * @return false if this block has already received a response from
   *         web content, true if not.
   */
  virtual bool SetContentResponse(bool aPreventDefault);

  /**
   * Record that content didn't respond in time.
   * @return false if this block already timed out, true if not.
   */
  bool TimeoutContentResponse();

  /**
   * Checks if the content response timer has already expired.
   */
  bool IsContentResponseTimerExpired() const;

  /**
   * @return true iff web content cancelled this block of events.
   */
  bool IsDefaultPrevented() const;

  /**
   * Process the given event using this input block's target apzc.
   * This input block must not have pending events, and its apzc must not be
   * nullptr.
   */
  void DispatchImmediate(const InputData& aEvent) const;

  /**
   * Dispatch the event to the target APZC. Mostly this is a hook for
   * subclasses to do any per-event processing they need to.
   */
  virtual void DispatchEvent(const InputData& aEvent) const;

  /**
   * @return true iff this block has received all the information needed
   *         to properly dispatch the events in the block.
   */
  virtual bool IsReadyForHandling() const;

  /**
   * Returns whether or not this block has pending events.
   */
  virtual bool HasEvents() const = 0;

  /**
   * Throw away all the events in this input block.
   */
  virtual void DropEvents() = 0;

  /**
   * Process all events using this input block's target apzc, leaving this
   * block depleted. This input block's apzc must not be nullptr.
   */
  virtual void HandleEvents() = 0;

  /**
   * Return true if this input block must stay active if it would otherwise
   * be removed as the last item in the pending queue.
   */
  virtual bool MustStayActive() = 0;

  /**
   * Return a descriptive name for the block kind.
   */
  virtual const char* Type() = 0;

private:
  bool mPreventDefault;
  bool mContentResponded;
  bool mContentResponseTimerExpired;
};

/**
 * A single block of wheel events.
 */
class WheelBlockState : public CancelableBlockState
{
public:
  WheelBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                  bool aTargetConfirmed,
                  const ScrollWheelInput& aEvent);

  bool SetContentResponse(bool aPreventDefault) override;
  bool IsReadyForHandling() const override;
  bool HasEvents() const override;
  void DropEvents() override;
  void HandleEvents() override;
  bool MustStayActive() override;
  const char* Type() override;
  bool SetConfirmedTargetApzc(const nsRefPtr<AsyncPanZoomController>& aTargetApzc) override;

  void AddEvent(const ScrollWheelInput& aEvent);

  WheelBlockState *AsWheelBlock() override {
    return this;
  }

  /**
   * Determine whether this wheel block is accepting new events.
   */
  bool ShouldAcceptNewEvent() const;

  /**
   * Call to check whether a wheel event will cause the current transaction to
   * timeout.
   */
  bool MaybeTimeout(const ScrollWheelInput& aEvent);

  /**
   * Called from APZCTM when a mouse move or drag+drop event occurs, before
   * the event has been processed.
   */
  void OnMouseMove(const ScreenIntPoint& aPoint);

  /**
   * Returns whether or not the block is participating in a wheel transaction.
   * This means that the block is the most recent input block to be created,
   * and no events have occurred that would require scrolling a different
   * frame.
   *
   * @return True if in a transaction, false otherwise.
   */
  bool InTransaction() const;

  /**
   * Mark the block as no longer participating in a wheel transaction. This
   * will force future wheel events to begin a new input block.
   */
  void EndTransaction();

  /**
   * @return Whether or not overscrolling is prevented for this wheel block.
   */
  bool AllowScrollHandoff() const;

  /**
   * Called to check and possibly end the transaction due to a timeout.
   *
   * @return True if the transaction ended, false otherwise.
   */
  bool MaybeTimeout(const TimeStamp& aTimeStamp);

  /**
   * Update the wheel transaction state for a new event.
   */
  void Update(const ScrollWheelInput& aEvent);

protected:
  void UpdateTargetApzc(const nsRefPtr<AsyncPanZoomController>& aTargetApzc) override;

private:
  nsTArray<ScrollWheelInput> mEvents;
  TimeStamp mLastEventTime;
  TimeStamp mLastMouseMove;
  bool mTransactionEnded;
};

/**
 * A single block of pan gesture events.
 */
class PanGestureBlockState : public CancelableBlockState
{
public:
  PanGestureBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                       bool aTargetConfirmed,
                       const PanGestureInput& aEvent);

  bool SetContentResponse(bool aPreventDefault) override;
  bool HasEvents() const override;
  void DropEvents() override;
  void HandleEvents() override;
  bool MustStayActive() override;
  const char* Type() override;
  bool SetConfirmedTargetApzc(const nsRefPtr<AsyncPanZoomController>& aTargetApzc) override;

  void AddEvent(const PanGestureInput& aEvent);

  PanGestureBlockState *AsPanGestureBlock() override {
    return this;
  }

  /**
   * @return Whether or not overscrolling is prevented for this block.
   */
  bool AllowScrollHandoff() const;

  bool WasInterrupted() const { return mInterrupted; }

private:
  nsTArray<PanGestureInput> mEvents;
  bool mInterrupted;
};

/**
 * This class represents a single touch block. A touch block is
 * a set of touch events that can be cancelled by web content via
 * touch event listeners.
 *
 * Every touch-start event creates a new touch block. In this case, the
 * touch block consists of the touch-start, followed by all touch events
 * up to but not including the next touch-start (except in the case where
 * a long-tap happens, see below). Note that in particular we cannot know
 * when a touch block ends until the next one is started. Most touch
 * blocks are created by receipt of a touch-start event.
 *
 * Every long-tap event also creates a new touch block, since it can also
 * be consumed by web content. In this case, when the long-tap event is
 * dispatched to web content, a new touch block is started to hold the remaining
 * touch events, up to but not including the next touch start (or long-tap).
 *
 * Additionally, if touch-action is enabled, each touch block should
 * have a set of allowed touch behavior flags; one for each touch point.
 * This also requires running code on the Gecko main thread, and so may
 * be populated with some latency. The mAllowedTouchBehaviorSet and
 * mAllowedTouchBehaviors variables track this information.
 */
class TouchBlockState : public CancelableBlockState
{
public:
  explicit TouchBlockState(const nsRefPtr<AsyncPanZoomController>& aTargetApzc,
                           bool aTargetConfirmed, TouchCounter& aTouchCounter);

  TouchBlockState *AsTouchBlock() override {
    return this;
  }

  /**
   * Set the allowed touch behavior flags for this block.
   * @return false if this block already has these flags set, true if not.
   */
  bool SetAllowedTouchBehaviors(const nsTArray<TouchBehaviorFlags>& aBehaviors);
  /**
   * If the allowed touch behaviors have been set, populate them into
   * |aOutBehaviors| and return true. Else, return false.
   */
  bool GetAllowedTouchBehaviors(nsTArray<TouchBehaviorFlags>& aOutBehaviors) const;

  /**
   * Copy various properties from another block.
   */
  void CopyPropertiesFrom(const TouchBlockState& aOther);

  /**
   * @return true iff this block has received all the information needed
   *         to properly dispatch the events in the block.
   */
  bool IsReadyForHandling() const override;

  /**
   * Sets a flag that indicates this input block occurred while the APZ was
   * in a state of fast flinging. This affects gestures that may be produced
   * from input events in this block.
   */
  void SetDuringFastFling();
  /**
   * @return true iff SetDuringFastFling was called on this block.
   */
  bool IsDuringFastFling() const;
  /**
   * Set the single-tap-occurred flag that indicates that this touch block
   * triggered a single tap event.
   * @return true if the flag was set. This may not happen if, for example,
   *         SetDuringFastFling was previously called.
   */
  bool SetSingleTapOccurred();
  /**
   * @return true iff the single-tap-occurred flag is set on this block.
   */
  bool SingleTapOccurred() const;

  /**
   * Add a new touch event to the queue of events in this input block.
   */
  void AddEvent(const MultiTouchInput& aEvent);

  /**
   * @return false iff touch-action is enabled and the allowed touch behaviors for
   *         this touch block do not allow pinch-zooming.
   */
  bool TouchActionAllowsPinchZoom() const;
  /**
   * @return false iff touch-action is enabled and the allowed touch behaviors for
   *         this touch block do not allow double-tap zooming.
   */
  bool TouchActionAllowsDoubleTapZoom() const;
  /**
   * @return false iff touch-action is enabled and the allowed touch behaviors for
   *         the first touch point do not allow panning in the specified direction(s).
   */
  bool TouchActionAllowsPanningX() const;
  bool TouchActionAllowsPanningY() const;
  bool TouchActionAllowsPanningXY() const;

  bool HasEvents() const override;
  void DropEvents() override;
  void HandleEvents() override;
  void DispatchEvent(const InputData& aEvent) const override;
  bool MustStayActive() override;
  const char* Type() override;

private:
  nsTArray<TouchBehaviorFlags> mAllowedTouchBehaviors;
  bool mAllowedTouchBehaviorSet;
  bool mDuringFastFling;
  bool mSingleTapOccurred;
  nsTArray<MultiTouchInput> mEvents;
  // A reference to the InputQueue's touch counter
  TouchCounter& mTouchCounter;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_InputBlockState_h
