/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_GestureEventListener_h
#define mozilla_layers_GestureEventListener_h

#include "mozilla/RefPtr.h"
#include "InputData.h"
#include "Axis.h"

namespace mozilla {
namespace layers {

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
   * Returns the AsyncPanZoomController stored on this class and used for
   * callbacks.
   */
  AsyncPanZoomController* GetAsyncPanZoomController();

protected:
  enum GestureState {
    NoGesture = 0,
    InPinchGesture
  };

  /**
   * Maximum time for a touch on the screen and corresponding lift of the finger
   * to be considered a tap.
   */
  enum { MAX_TAP_TIME = 500 };

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
   * Attempts to handle a tap event cancellation. This happens when we think
   * something was a tap but it actually wasn't. In general, this will not
   * attempt to block the touch event from being passed along to
   * AsyncPanZoomController since APZC needs to know about touches ending (and
   * we only know if a touch was a tap once it ends).
   */
  nsEventStatus HandleTapCancel(const MultiTouchInput& aEvent);

  nsRefPtr<AsyncPanZoomController> mAsyncPanZoomController;

  /**
   * Array containing all active touches. When a touch happens it, gets added to
   * this array, even if we choose not to handle it. When it ends, we remove it.
   */
  nsTArray<SingleTouchData> mTouches;
  GestureState mState;

  /**
   * Previous span calculated for the purposes of setting inside a
   * PinchGestureInput.
   */
  float mPreviousSpan;

  /**
   * Stores the time a touch started, used for detecting a tap gesture. Only
   * valid when there's exactly one touch in mTouches.
   */
  PRUint64 mTouchStartTime;
};

}
}

#endif
