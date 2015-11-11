/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_GestureEventListener_h
#define mozilla_layers_GestureEventListener_h

#include "InputData.h"                  // for MultiTouchInput, etc
#include "Units.h"
#include "mozilla/EventForwards.h"      // for nsEventStatus
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsISupportsImpl.h"
#include "nsTArray.h"                   // for nsTArray

class CancelableTask;

namespace mozilla {
namespace layers {

class AsyncPanZoomController;

/**
 * Platform-non-specific, generalized gesture event listener. This class
 * intercepts all touches events on their way to AsyncPanZoomController and
 * determines whether or not they are part of a gesture.
 *
 * For example, seeing that two fingers are on the screen means that the user
 * wants to do a pinch gesture, so we don't forward the touches along to
 * AsyncPanZoomController since it will think that they are just trying to pan
 * the screen. Instead, we generate a PinchGestureInput and send that. If the
 * touch event is not part of a gesture, we just return nsEventStatus_eIgnore
 * and AsyncPanZoomController is expected to handle it.
 *
 * Android doesn't use this class because it has its own built-in gesture event
 * listeners that should generally be preferred.
 */
class GestureEventListener final {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GestureEventListener)

  explicit GestureEventListener(AsyncPanZoomController* aAsyncPanZoomController);

  // --------------------------------------------------------------------------
  // These methods must only be called on the controller/UI thread.
  //

  /**
   * General input handler for a touch event. If the touch event is not a part
   * of a gesture, then we pass it along to AsyncPanZoomController. Otherwise,
   * it gets consumed here and never forwarded along.
   */
  nsEventStatus HandleInputEvent(const MultiTouchInput& aEvent);

  /**
   * Returns the identifier of the touch in the last touch event processed by
   * this GestureEventListener. This should only be called when the last touch
   * event contained only one touch.
   */
  int32_t GetLastTouchIdentifier() const;

  /**
   * Function used to disable long tap gestures.
   *
   * On slow running tests, drags and touch events can be misinterpreted
   * as a long tap. This allows tests to disable long tap gesture detection.
   */
  static void SetLongTapEnabled(bool aLongTapEnabled);

private:
  // Private destructor, to discourage deletion outside of Release():
  ~GestureEventListener();

  /**
   * States of GEL finite-state machine.
   */
  enum GestureState {
    // This is the initial and final state of any gesture.
    // In this state there's no gesture going on, and we don't think we're
    // about to enter one.
    // Allowed next states: GESTURE_FIRST_SINGLE_TOUCH_DOWN, GESTURE_MULTI_TOUCH_DOWN.
    GESTURE_NONE,

    // A touch start with a single touch point has just happened.
    // After having gotten into this state we start timers for MAX_TAP_TIME and
    // gfxPrefs::UiClickHoldContextMenusDelay().
    // Allowed next states: GESTURE_MULTI_TOUCH_DOWN, GESTURE_NONE,
    //                      GESTURE_FIRST_SINGLE_TOUCH_UP, GESTURE_LONG_TOUCH_DOWN,
    //                      GESTURE_FIRST_SINGLE_TOUCH_MAX_TAP_DOWN.
    GESTURE_FIRST_SINGLE_TOUCH_DOWN,

    // While in GESTURE_FIRST_SINGLE_TOUCH_DOWN state a MAX_TAP_TIME timer got
    // triggered. Now we'll trigger either a single tap if a user lifts her
    // finger or a long tap if gfxPrefs::UiClickHoldContextMenusDelay() happens
    // first.
    // Allowed next states: GESTURE_MULTI_TOUCH_DOWN, GESTURE_NONE,
    //                      GESTURE_LONG_TOUCH_DOWN.
    GESTURE_FIRST_SINGLE_TOUCH_MAX_TAP_DOWN,

    // A user put her finger down and lifted it up quickly enough.
    // After having gotten into this state we clear the timer for MAX_TAP_TIME.
    // Allowed next states: GESTURE_SECOND_SINGLE_TOUCH_DOWN, GESTURE_NONE,
    //                      GESTURE_MULTI_TOUCH_DOWN.
    GESTURE_FIRST_SINGLE_TOUCH_UP,

    // A user put down her finger again right after a single tap thus the
    // gesture can't be a single tap, but rather a double tap. But we're
    // still not sure about that until the user lifts her finger again.
    // Allowed next states: GESTURE_MULTI_TOUCH_DOWN, GESTURE_NONE.
    GESTURE_SECOND_SINGLE_TOUCH_DOWN,

    // A long touch has happened, but the user still keeps her finger down.
    // We'll trigger a "long tap up" event when the finger is up.
    // Allowed next states: GESTURE_NONE, GESTURE_MULTI_TOUCH_DOWN.
    GESTURE_LONG_TOUCH_DOWN,

    // We have detected that two or more fingers are on the screen, but there
    // hasn't been enough movement yet to make us start actually zooming the
    // screen.
    // Allowed next states: GESTURE_PINCH, GESTURE_NONE
    GESTURE_MULTI_TOUCH_DOWN,

    // There are two or more fingers on the screen, and the user has already
    // pinched enough for us to start zooming the screen.
    // Allowed next states: GESTURE_NONE
    GESTURE_PINCH
  };

  /**
   * These HandleInput* functions comprise input alphabet of the GEL
   * finite-state machine triggering state transitions.
   */
  nsEventStatus HandleInputTouchSingleStart();
  nsEventStatus HandleInputTouchMultiStart();
  nsEventStatus HandleInputTouchEnd();
  nsEventStatus HandleInputTouchMove();
  nsEventStatus HandleInputTouchCancel();
  void HandleInputTimeoutLongTap();
  void HandleInputTimeoutMaxTap();

  void TriggerSingleTapConfirmedEvent();

  bool MoveDistanceIsLarge();

  /**
   * Do actual state transition and reset substates.
   */
  void SetState(GestureState aState);

  RefPtr<AsyncPanZoomController> mAsyncPanZoomController;

  /**
   * Array containing all active touches. When a touch happens it, gets added to
   * this array, even if we choose not to handle it. When it ends, we remove it.
   * We need to maintain this array in order to detect the end of the
   * "multitouch" states because touch start events contain all current touches,
   * but touch end events contain only those touches that have gone.
   */
  nsTArray<SingleTouchData> mTouches;

  /**
   * Current state we're dealing with.
   */
  GestureState mState;

  /**
   * Total change in span since we detected a pinch gesture. Only used when we
   * are in the |GESTURE_WAITING_PINCH| state and need to know how far zoomed
   * out we are compared to our original pinch span. Note that this does _not_
   * continue to be updated once we jump into the |GESTURE_PINCH| state.
   */
  float mSpanChange;

  /**
   * Previous span calculated for the purposes of setting inside a
   * PinchGestureInput.
   */
  float mPreviousSpan;

  /**
   * Cached copy of the last touch input.
   */
  MultiTouchInput mLastTouchInput;

  /**
   * Cached copy of the last tap gesture input.
   * In the situation when we have a tap followed by a pinch we lose info
   * about tap since we keep only last input and to dispatch it correctly
   * we save last tap copy into this variable.
   * For more info see bug 947892.
   */
  MultiTouchInput mLastTapInput;

  /**
   * Position of the last touch starting. This is only valid during an attempt
   * to determine if a touch is a tap. If a touch point moves away from
   * mTouchStartPosition to the distance greater than
   * AsyncPanZoomController::GetTouchStartTolerance() while in
   * GESTURE_FIRST_SINGLE_TOUCH_DOWN, GESTURE_FIRST_SINGLE_TOUCH_MAX_TAP_DOWN
   * or GESTURE_SECOND_SINGLE_TOUCH_DOWN then we're certain the gesture is
   * not tap.
   */
  ParentLayerPoint mTouchStartPosition;

  /**
   * Task used to timeout a long tap. This gets posted to the UI thread such
   * that it runs a time when a single tap happens. We cache it so that
   * we can cancel it if any other touch event happens.
   *
   * The task is supposed to be non-null if in GESTURE_FIRST_SINGLE_TOUCH_DOWN
   * and GESTURE_FIRST_SINGLE_TOUCH_MAX_TAP_DOWN states.
   *
   * CancelLongTapTimeoutTask: Cancel the mLongTapTimeoutTask and also set
   * it to null.
   */
  CancelableTask *mLongTapTimeoutTask;
  void CancelLongTapTimeoutTask();
  void CreateLongTapTimeoutTask();

  /**
   * Task used to timeout a single tap or a double tap.
   *
   * The task is supposed to be non-null if in GESTURE_FIRST_SINGLE_TOUCH_DOWN,
   * GESTURE_FIRST_SINGLE_TOUCH_UP and GESTURE_SECOND_SINGLE_TOUCH_DOWN states.
   *
   * CancelMaxTapTimeoutTask: Cancel the mMaxTapTimeoutTask and also set
   * it to null.
   */
  CancelableTask *mMaxTapTimeoutTask;
  void CancelMaxTapTimeoutTask();
  void CreateMaxTapTimeoutTask();
};

} // namespace layers
} // namespace mozilla

#endif
