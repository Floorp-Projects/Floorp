/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GestureEventListener.h"
#include <math.h>                       // for fabsf
#include <stddef.h>                     // for size_t
#include "AsyncPanZoomController.h"     // for AsyncPanZoomController
#include "mozilla/layers/APZCTreeManager.h"  // for APZCTreeManager
#include "base/task.h"                  // for CancelableTask, etc
#include "gfxPrefs.h"                   // for gfxPrefs
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/mozalloc.h"           // for operator new
#include "nsDebug.h"                    // for NS_WARN_IF_FALSE
#include "nsMathUtils.h"                // for NS_hypot

namespace mozilla {
namespace layers {

/**
 * Maximum time for a touch on the screen and corresponding lift of the finger
 * to be considered a tap. This also applies to double taps, except that it is
 * used twice.
 */
static const uint32_t MAX_TAP_TIME = 300;

/**
 * Amount of change in span needed to take us from the GESTURE_WAITING_PINCH
 * state to the GESTURE_PINCH state. This is measured as a change in distance
 * between the fingers used to compute the span ratio. Note that it is a
 * distance, not a displacement.
 */
static const float PINCH_START_THRESHOLD = 35.0f;

GestureEventListener::GestureEventListener(AsyncPanZoomController* aAsyncPanZoomController)
  : mAsyncPanZoomController(aAsyncPanZoomController),
    mState(GESTURE_NONE),
    mSpanChange(0.0f),
    mTapStartTime(0),
    mLastTapEndTime(0),
    mLastTouchInput(MultiTouchInput::MULTITOUCH_START, 0, 0)
{
}

GestureEventListener::~GestureEventListener()
{
}

nsEventStatus GestureEventListener::HandleInputEvent(const MultiTouchInput& aEvent)
{
  // Cache the current event since it may become the single or long tap that we
  // send.
  mLastTouchInput = aEvent;

  switch (aEvent.mType)
  {
  case MultiTouchInput::MULTITOUCH_START:
  case MultiTouchInput::MULTITOUCH_ENTER: {
    for (size_t i = 0; i < aEvent.mTouches.Length(); i++) {
      bool foundAlreadyExistingTouch = false;
      for (size_t j = 0; j < mTouches.Length(); j++) {
        if (mTouches[j].mIdentifier == aEvent.mTouches[i].mIdentifier) {
          foundAlreadyExistingTouch = true;
          break;
        }
      }

      // If we didn't find a touch in our list that matches this, then add it.
      if (!foundAlreadyExistingTouch) {
        mTouches.AppendElement(aEvent.mTouches[i]);
      }
    }

    size_t length = mTouches.Length();
    if (length == 1) {
      mTapStartTime = aEvent.mTime;
      mTouchStartPosition = aEvent.mTouches[0].mScreenPoint;
      if (mState == GESTURE_NONE) {
        mState = GESTURE_WAITING_SINGLE_TAP;

        mLongTapTimeoutTask =
          NewRunnableMethod(this, &GestureEventListener::TimeoutLongTap);

        mAsyncPanZoomController->PostDelayedTask(
          mLongTapTimeoutTask,
          gfxPrefs::UiClickHoldContextMenusDelay());
      }
    } else if (length == 2) {
      // Another finger has been added; it can't be a tap anymore.
      HandleTapCancel(aEvent);
    }

    break;
  }
  case MultiTouchInput::MULTITOUCH_MOVE: {
    // If we move too much, bail out of the tap.
    ScreenIntPoint delta = aEvent.mTouches[0].mScreenPoint - mTouchStartPosition;
    if (mTouches.Length() == 1 &&
        NS_hypot(delta.x, delta.y) > AsyncPanZoomController::GetTouchStartTolerance())
    {
      HandleTapCancel(aEvent);
    }

    size_t eventTouchesMatched = 0;
    for (size_t i = 0; i < mTouches.Length(); i++) {
      bool isTouchRemoved = true;
      for (size_t j = 0; j < aEvent.mTouches.Length(); j++) {
        if (mTouches[i].mIdentifier == aEvent.mTouches[j].mIdentifier) {
          eventTouchesMatched++;
          isTouchRemoved = false;
          mTouches[i] = aEvent.mTouches[j];
        }
      }
      if (isTouchRemoved) {
        // this touch point was lifted, so remove it from our list
        mTouches.RemoveElementAt(i);
        i--;
      }
    }

    NS_WARN_IF_FALSE(eventTouchesMatched == aEvent.mTouches.Length(), "Touch moved, but not in list");

    break;
  }
  case MultiTouchInput::MULTITOUCH_END:
  case MultiTouchInput::MULTITOUCH_LEAVE: {
    for (size_t i = 0; i < aEvent.mTouches.Length(); i++) {
      bool foundAlreadyExistingTouch = false;
      for (size_t j = 0; j < mTouches.Length() && !foundAlreadyExistingTouch; j++) {
        if (aEvent.mTouches[i].mIdentifier == mTouches[j].mIdentifier) {
          foundAlreadyExistingTouch = true;
          mTouches.RemoveElementAt(j);
        }
      }
      NS_WARN_IF_FALSE(foundAlreadyExistingTouch, "Touch ended, but not in list");
    }

    if (mState == GESTURE_WAITING_DOUBLE_TAP) {
      CancelDoubleTapTimeoutTask();
      if (mTapStartTime - mLastTapEndTime > MAX_TAP_TIME ||
          aEvent.mTime - mTapStartTime > MAX_TAP_TIME) {
        // Either the time between taps or the last tap took too long
        // confirm previous tap and handle current tap seperately
        TimeoutDoubleTap();
        mState = GESTURE_WAITING_SINGLE_TAP;
      } else {
        // We were waiting for a double tap and it has arrived.
        HandleDoubleTap(aEvent);
        mState = GESTURE_NONE;
      }
    }

    if (mState == GESTURE_LONG_TAP_UP) {
      HandleLongTapUpEvent(aEvent);
      mState = GESTURE_NONE;
    } else if (mState == GESTURE_WAITING_SINGLE_TAP &&
        aEvent.mTime - mTapStartTime > MAX_TAP_TIME) {
      // Extended taps are immediately dispatched as single taps
      CancelLongTapTimeoutTask();
      HandleSingleTapConfirmedEvent(aEvent);
      mState = GESTURE_NONE;
    } else if (mState == GESTURE_WAITING_SINGLE_TAP) {
      CancelLongTapTimeoutTask();
      nsEventStatus tapupEvent = HandleSingleTapUpEvent(aEvent);

      if (tapupEvent == nsEventStatus_eIgnore) {
        // We were not waiting for anything but a single tap has happened that
        // may turn into a double tap. Wait a while and if it doesn't turn into
        // a double tap, send a single tap instead.
        mState = GESTURE_WAITING_DOUBLE_TAP;

        mDoubleTapTimeoutTask =
          NewRunnableMethod(this, &GestureEventListener::TimeoutDoubleTap);

        mAsyncPanZoomController->PostDelayedTask(
          mDoubleTapTimeoutTask,
          MAX_TAP_TIME);

      } else if (tapupEvent == nsEventStatus_eConsumeNoDefault) {
        // We sent the tapup into content without waiting for a double tap
        mState = GESTURE_NONE;
      }
    }

    mLastTapEndTime = aEvent.mTime;

    if (!mTouches.Length()) {
      mSpanChange = 0.0f;
    }

    break;
  }
  case MultiTouchInput::MULTITOUCH_CANCEL:
    // FIXME: we should probably clear a bunch of gesture state here
    break;
  }

  return HandlePinchGestureEvent(aEvent);
}

nsEventStatus GestureEventListener::HandlePinchGestureEvent(const MultiTouchInput& aEvent)
{
  nsEventStatus rv = nsEventStatus_eIgnore;

  if (aEvent.mType == MultiTouchInput::MULTITOUCH_CANCEL) {
    mTouches.Clear();
    mState = GESTURE_NONE;
    return rv;
  }

  if (mTouches.Length() > 1) {
    const ScreenIntPoint& firstTouch = mTouches[0].mScreenPoint,
                         secondTouch = mTouches[1].mScreenPoint;
    ScreenPoint focusPoint = ScreenPoint(firstTouch + secondTouch) / 2;
    ScreenIntPoint delta = secondTouch - firstTouch;
    float currentSpan = float(NS_hypot(delta.x, delta.y));

    switch (mState) {
    case GESTURE_NONE:
      mPreviousSpan = currentSpan;
      mState = GESTURE_WAITING_PINCH;
      // Deliberately fall through. If the user pinched and took their fingers
      // off the screen such that they still had 1 left on it, we want there to
      // be no resistance. We should only reset |mSpanChange| once all fingers
      // are off the screen.
    case GESTURE_WAITING_PINCH: {
      mSpanChange += fabsf(currentSpan - mPreviousSpan);
      if (mSpanChange > PINCH_START_THRESHOLD) {
        PinchGestureInput pinchEvent(PinchGestureInput::PINCHGESTURE_START,
                                     aEvent.mTime,
                                     focusPoint,
                                     currentSpan,
                                     currentSpan,
                                     aEvent.modifiers);

        mAsyncPanZoomController->HandleInputEvent(pinchEvent);

        mState = GESTURE_PINCH;
      }

      break;
    }
    case GESTURE_PINCH: {
      PinchGestureInput pinchEvent(PinchGestureInput::PINCHGESTURE_SCALE,
                                   aEvent.mTime,
                                   focusPoint,
                                   currentSpan,
                                   mPreviousSpan,
                                   aEvent.modifiers);

      mAsyncPanZoomController->HandleInputEvent(pinchEvent);
      break;
    }
    default:
      // What?
      break;
    }

    mPreviousSpan = currentSpan;

    rv = nsEventStatus_eConsumeNoDefault;
  } else if (mState == GESTURE_PINCH) {
    PinchGestureInput pinchEvent(PinchGestureInput::PINCHGESTURE_END,
                                 aEvent.mTime,
                                 ScreenPoint(),
                                 1.0f,
                                 1.0f,
                                 aEvent.modifiers);
    mAsyncPanZoomController->HandleInputEvent(pinchEvent);

    mState = GESTURE_NONE;

    // If the user left a finger on the screen, spoof a touch start event and
    // send it to APZC so that they can continue panning from that point.
    if (mTouches.Length() == 1) {
      MultiTouchInput touchEvent(MultiTouchInput::MULTITOUCH_START,
                                 aEvent.mTime,
                                 aEvent.modifiers);
      touchEvent.mTouches.AppendElement(mTouches[0]);
      mAsyncPanZoomController->HandleInputEvent(touchEvent);

      // The spoofed touch start will get back to GEL and make us enter the
      // GESTURE_WAITING_SINGLE_TAP state, but this isn't a new touch, so there
      // is no condition under which this touch should turn into any tap.
      mState = GESTURE_NONE;
    }

    rv = nsEventStatus_eConsumeNoDefault;
  } else if (mState == GESTURE_WAITING_PINCH) {
    mState = GESTURE_NONE;
  }

  return rv;
}

nsEventStatus GestureEventListener::HandleSingleTapUpEvent(const MultiTouchInput& aEvent)
{
  TapGestureInput tapEvent(TapGestureInput::TAPGESTURE_UP, aEvent.mTime,
      aEvent.mTouches[0].mScreenPoint, aEvent.modifiers);
  return mAsyncPanZoomController->HandleInputEvent(tapEvent);
}

nsEventStatus GestureEventListener::HandleSingleTapConfirmedEvent(const MultiTouchInput& aEvent)
{
  TapGestureInput tapEvent(TapGestureInput::TAPGESTURE_CONFIRMED, aEvent.mTime,
      aEvent.mTouches[0].mScreenPoint, aEvent.modifiers);
  return mAsyncPanZoomController->HandleInputEvent(tapEvent);
}

nsEventStatus GestureEventListener::HandleLongTapEvent(const MultiTouchInput& aEvent)
{
  TapGestureInput tapEvent(TapGestureInput::TAPGESTURE_LONG, aEvent.mTime,
      aEvent.mTouches[0].mScreenPoint, aEvent.modifiers);
  return mAsyncPanZoomController->HandleInputEvent(tapEvent);
}

nsEventStatus GestureEventListener::HandleLongTapUpEvent(const MultiTouchInput& aEvent)
{
  TapGestureInput tapEvent(TapGestureInput::TAPGESTURE_LONG_UP, aEvent.mTime,
      aEvent.mTouches[0].mScreenPoint, aEvent.modifiers);
  return mAsyncPanZoomController->HandleInputEvent(tapEvent);
}

nsEventStatus GestureEventListener::HandleTapCancel(const MultiTouchInput& aEvent)
{
  mTapStartTime = 0;

  switch (mState)
  {
  case GESTURE_WAITING_SINGLE_TAP:
    CancelLongTapTimeoutTask();
    mState = GESTURE_NONE;
    break;

  case GESTURE_WAITING_DOUBLE_TAP:
  case GESTURE_LONG_TAP_UP:
    mState = GESTURE_NONE;
    break;
  default:
    break;
  }

  return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus GestureEventListener::HandleDoubleTap(const MultiTouchInput& aEvent)
{
  TapGestureInput tapEvent(TapGestureInput::TAPGESTURE_DOUBLE, aEvent.mTime,
      aEvent.mTouches[0].mScreenPoint, aEvent.modifiers);
  return mAsyncPanZoomController->HandleInputEvent(tapEvent);
}

void GestureEventListener::TimeoutDoubleTap()
{
  mDoubleTapTimeoutTask = nullptr;
  // If we haven't gotten another tap by now, reset the state and treat it as a
  // single tap. It couldn't have been a double tap.
  if (mState == GESTURE_WAITING_DOUBLE_TAP) {
    mState = GESTURE_NONE;

    HandleSingleTapConfirmedEvent(mLastTouchInput);
  }
}

void GestureEventListener::CancelDoubleTapTimeoutTask() {
  if (mDoubleTapTimeoutTask) {
    mDoubleTapTimeoutTask->Cancel();
    mDoubleTapTimeoutTask = nullptr;
  }
}

void GestureEventListener::TimeoutLongTap()
{
  mLongTapTimeoutTask = nullptr;
  // If the tap has not been released, this is a long press.
  if (mState == GESTURE_WAITING_SINGLE_TAP) {
    mState = GESTURE_LONG_TAP_UP;

    HandleLongTapEvent(mLastTouchInput);
  }
}

void GestureEventListener::CancelLongTapTimeoutTask() {
  if (mLongTapTimeoutTask) {
    mLongTapTimeoutTask->Cancel();
    mLongTapTimeoutTask = nullptr;
  }
}

AsyncPanZoomController* GestureEventListener::GetAsyncPanZoomController() {
  return mAsyncPanZoomController;
}

void GestureEventListener::CancelGesture() {
  mTouches.Clear();
  mState = GESTURE_NONE;
}

}
}
