/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_GestureEventListener_h
#define mozilla_layers_GestureEventListener_h

#include <stdint.h>                     // for uint64_t
#include "InputData.h"                  // for MultiTouchInput, etc
#include "Units.h"                      // for ScreenIntPoint
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsEvent.h"                    // for nsEventStatus
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
class GestureEventListener {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GestureEventListener)

  GestureEventListener(AsyncPanZoomController* aAsyncPanZoomController);
  ~GestureEventListener();

  // --------------------------------------------------------------------------
  // These methods must only be called on the controller/UI thread.
  //

  /**
   * General input handler for a touch event. If the touch event is not a part
   * of a gesture, then we pass it along to AsyncPanZoomController. Otherwise,
   * it gets consumed here and never forwarded along.
   */
  nsEventStatus HandleInputEvent(const InputData& aEvent);

  /**
   * Cancels any currently active gesture. May not properly handle situations
   * that require extra work at the gesture's end, like a pinch which only
   * requests a repaint once it has ended.
   */
  void CancelGesture();

  /**
   * Returns the AsyncPanZoomController stored on this class and used for
   * callbacks.
   */
  AsyncPanZoomController* GetAsyncPanZoomController();

protected:
  enum GestureState {
    // There's no gesture going on, and we don't think we're about to enter one.
    GESTURE_NONE,
    // We have detected that two or more fingers are on the screen, but there
    // hasn't been enough movement yet to make us start actually zooming the
    // screen.
    GESTURE_WAITING_PINCH,
    // There are two or more fingers on the screen, and the user has already
    // pinched enough for us to start zooming the screen.
    GESTURE_PINCH,
    // A touch start has happened and it may turn into a tap. We use this
    // because, if we put down two fingers and then lift them very quickly, this
    // may be mistaken for a tap.
    GESTURE_WAITING_SINGLE_TAP,
    // A single tap has happened for sure, and we're waiting for a second tap.
    GESTURE_WAITING_DOUBLE_TAP
  };

  /**
   * Attempts to handle the event as a pinch event. If it is not a pinch event,
   * then we simply tell the next consumer to consume the event instead.
   *
   * |aClearTouches| marks whether or not to terminate any pinch currently
   * happening.
   */
  nsEventStatus HandlePinchGestureEvent(const MultiTouchInput& aEvent, bool aClearTouches);

  /**
   * Attempts to handle the event as a single tap event, which highlights links
   * before opening them. In general, this will not attempt to block the touch
   * event from being passed along to AsyncPanZoomController since APZC needs to
   * know about touches ending (and we only know if a touch was a tap once it
   * ends).
   */
  nsEventStatus HandleSingleTapUpEvent(const MultiTouchInput& aEvent);

  /**
   * Attempts to handle a single tap confirmation. This is what will actually
   * open links, etc. In general, this will not attempt to block the touch event
   * from being passed along to AsyncPanZoomController since APZC needs to know
   * about touches ending (and we only know if a touch was a tap once it ends).
   */
  nsEventStatus HandleSingleTapConfirmedEvent(const MultiTouchInput& aEvent);

  /**
   * Attempts to handle a long tap confirmation. This is what will use
   * for context menu.
   */
  nsEventStatus HandleLongTapEvent(const MultiTouchInput& aEvent);

  /**
   * Attempts to handle a tap event cancellation. This happens when we think
   * something was a tap but it actually wasn't. In general, this will not
   * attempt to block the touch event from being passed along to
   * AsyncPanZoomController since APZC needs to know about touches ending (and
   * we only know if a touch was a tap once it ends).
   */
  nsEventStatus HandleTapCancel(const MultiTouchInput& aEvent);

  /**
   * Attempts to handle a double tap. This happens when we get two single taps
   * within a short time. In general, this will not attempt to block the touch
   * event from being passed along to AsyncPanZoomController since APZC needs to
   * know about touches ending (and we only know if a touch was a double tap
   * once it ends).
   */
  nsEventStatus HandleDoubleTap(const MultiTouchInput& aEvent);

  /**
   * Times out a single tap we think may be turned into a double tap. This will
   * also send a single tap if we're still in the "GESTURE_WAITING_DOUBLE_TAP"
   * state when this is called. This should be called a short time after a
   * single tap is detected, and the delay on it should be enough that the user
   * has time to tap again (to make a double tap).
   */
  void TimeoutDoubleTap();
  /**
   * Times out a long tap. This should be called a 'long' time after a single
   * tap is detected.
   */
  void TimeoutLongTap();

  nsRefPtr<AsyncPanZoomController> mAsyncPanZoomController;

  /**
   * Array containing all active touches. When a touch happens it, gets added to
   * this array, even if we choose not to handle it. When it ends, we remove it.
   */
  nsTArray<SingleTouchData> mTouches;

  /**
   * Current gesture we're dealing with.
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
   * Stores the time a touch started, used for detecting a tap gesture. Only
   * valid when there's exactly one touch in mTouches. This is the time that the
   * first touch was inserted into the array. This is a uint64_t because it is
   * initialized from interactions with InputData, which stores its timestamps as
   * a uint64_t.
   */
  uint64_t mTapStartTime;

  /**
   * Stores the time the last tap ends (finger leaves the screen). This is used
   * when mDoubleTapTimeoutTask cannot be scheduled in time and consecutive
   * taps are falsely regarded as double taps.
   */
  uint64_t mLastTapEndTime;

  /**
   * Cached copy of the last touch input, only valid when in the
   * "GESTURE_WAITING_DOUBLE_TAP" state. This is used to forward along to
   * AsyncPanZoomController if a single tap needs to be sent (since it is sent
   * shortly after the user actually taps, since we need to wait for a double
   * tap).
   */
  MultiTouchInput mLastTouchInput;

  /**
   * Task used to timeout a double tap. This gets posted to the UI thread such
   * that it runs a short time after a single tap happens. We cache it so that
   * we can cancel it if a double tap actually comes in.
   * CancelDoubleTapTimeoutTask: Cancel the mDoubleTapTimeoutTask and also set
   * it to null.
   */
  CancelableTask *mDoubleTapTimeoutTask;
  inline void CancelDoubleTapTimeoutTask();

  /**
   * Task used to timeout a long tap. This gets posted to the UI thread such
   * that it runs a time when a single tap happens. We cache it so that
   * we can cancel it if any other touch event happens.
   * CancelLongTapTimeoutTask: Cancel the mLongTapTimeoutTask and also set
   * it to null.
   */
  CancelableTask *mLongTapTimeoutTask;
  inline void CancelLongTapTimeoutTask();

  /**
   * Position of the last touch starting. This is only valid during an attempt
   * to determine if a touch is a tap. This means that it is used in both the
   * "GESTURE_WAITING_SINGLE_TAP" and "GESTURE_WAITING_DOUBLE_TAP" states.
   */
  ScreenIntPoint mTouchStartPosition;
};

}
}

#endif
