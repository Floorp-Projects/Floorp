/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"
#include "base/thread.h"

#include "GestureEventListener.h"
#include "AsyncPanZoomController.h"

#include "mozilla/Preferences.h"

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
    mLastTouchInput(MultiTouchInput::MULTITOUCH_START, 0)
{
}

GestureEventListener::~GestureEventListener()
{
}

nsEventStatus GestureEventListener::HandleInputEvent(const InputData& aEvent)
{
  if (aEvent.mInputType != MULTITOUCH_INPUT) {
    return nsEventStatus_eIgnore;
  }

  const MultiTouchInput& event = static_cast<const MultiTouchInput&>(aEvent);

  // Cache the current event since it may become the single or long tap that we
  // send.
  mLastTouchInput = event;

  switch (event.mType)
  {
  case MultiTouchInput::MULTITOUCH_START:
  case MultiTouchInput::MULTITOUCH_ENTER: {
    for (size_t i = 0; i < event.mTouches.Length(); i++) {
      bool foundAlreadyExistingTouch = false;
      for (size_t j = 0; j < mTouches.Length(); j++) {
        if (mTouches[j].mIdentifier == event.mTouches[i].mIdentifier) {
          foundAlreadyExistingTouch = true;
        }
      }

      NS_WARN_IF_FALSE(!foundAlreadyExistingTouch, "Tried to add a touch that already exists");

      // If we didn't find a touch in our list that matches this, then add it.
      // If it already existed, we don't want to add it twice because that
      // messes with our touch move/end code.
      if (!foundAlreadyExistingTouch) {
        mTouches.AppendElement(event.mTouches[i]);
      }
    }

    size_t length = mTouches.Length();
    if (length == 1) {
      mTapStartTime = event.mTime;
      mTouchStartPosition = event.mTouches[0].mScreenPoint;
      if (mState == GESTURE_NONE) {
        mState = GESTURE_WAITING_SINGLE_TAP;

        mLongTapTimeoutTask =
          NewRunnableMethod(this, &GestureEventListener::TimeoutLongTap);

        mAsyncPanZoomController->PostDelayedTask(
          mLongTapTimeoutTask,
          Preferences::GetInt("ui.click_hold_context_menus.delay", 500));
      }
    } else if (length == 2) {
      // Another finger has been added; it can't be a tap anymore.
      HandleTapCancel(event);
    }

    break;
  }
  case MultiTouchInput::MULTITOUCH_MOVE: {
    // If we move too much, bail out of the tap.
    ScreenIntPoint delta = event.mTouches[0].mScreenPoint - mTouchStartPosition;
    if (mTouches.Length() == 1 &&
        NS_hypot(delta.x, delta.y) >
          mAsyncPanZoomController->GetDPI() * mAsyncPanZoomController->GetTouchStartTolerance())
    {
      HandleTapCancel(event);
    }

    bool foundAlreadyExistingTouch = false;
    for (size_t i = 0; i < mTouches.Length(); i++) {
      for (size_t j = 0; j < event.mTouches.Length(); j++) {
        if (mTouches[i].mIdentifier == event.mTouches[j].mIdentifier) {
          foundAlreadyExistingTouch = true;
          mTouches[i] = event.mTouches[j];
        }
      }
    }

    NS_WARN_IF_FALSE(foundAlreadyExistingTouch, "Touch moved, but not in list");

    break;
  }
  case MultiTouchInput::MULTITOUCH_END:
  case MultiTouchInput::MULTITOUCH_LEAVE: {
    bool foundAlreadyExistingTouch = false;
    for (size_t i = 0; i < event.mTouches.Length() && !foundAlreadyExistingTouch; i++) {
      for (size_t j = 0; j < mTouches.Length() && !foundAlreadyExistingTouch; j++) {
        if (event.mTouches[i].mIdentifier == mTouches[j].mIdentifier) {
          foundAlreadyExistingTouch = true;
          mTouches.RemoveElementAt(j);
        }
      }
    }

    NS_WARN_IF_FALSE(foundAlreadyExistingTouch, "Touch ended, but not in list");

    if (event.mTime - mTapStartTime <= MAX_TAP_TIME) {
      if (mState == GESTURE_WAITING_DOUBLE_TAP &&
          event.mTime - mLastTapEndTime > MAX_TAP_TIME) {
        // mDoubleTapTimeoutTask wasn't scheduled in time. We need to run the
        // task synchronously to confirm the last tap.
        CancelDoubleTapTimeoutTask();
        TimeoutDoubleTap();

        // Change the state so we can proceed to process the current tap.
        mState = GESTURE_WAITING_SINGLE_TAP;
      }

      if (mState == GESTURE_WAITING_DOUBLE_TAP) {
        CancelDoubleTapTimeoutTask();
        // We were waiting for a double tap and it has arrived.
        HandleDoubleTap(event);
        mState = GESTURE_NONE;
      } else if (mState == GESTURE_WAITING_SINGLE_TAP) {
        CancelLongTapTimeoutTask();
        HandleSingleTapUpEvent(event);

        // We were not waiting for anything but a single tap has happened that
        // may turn into a double tap. Wait a while and if it doesn't turn into
        // a double tap, send a single tap instead.
        mState = GESTURE_WAITING_DOUBLE_TAP;

        mDoubleTapTimeoutTask =
          NewRunnableMethod(this, &GestureEventListener::TimeoutDoubleTap);

        mAsyncPanZoomController->PostDelayedTask(
          mDoubleTapTimeoutTask,
          MAX_TAP_TIME);
      }

      mLastTapEndTime = event.mTime;
    }

    if (mState == GESTURE_WAITING_SINGLE_TAP) {
      mState = GESTURE_NONE;
    }

    if (!mTouches.Length()) {
      mSpanChange = 0.0f;
    }

    break;
  }
  case MultiTouchInput::MULTITOUCH_CANCEL:
    // This gets called if there's a touch that has to bail for weird reasons
    // like pinching and then moving away from the window that the pinch was
    // started in without letting go of the screen.
    HandlePinchGestureEvent(event, true);
    break;
  }

  return HandlePinchGestureEvent(event, false);
}

nsEventStatus GestureEventListener::HandlePinchGestureEvent(const MultiTouchInput& aEvent, bool aClearTouches)
{
  nsEventStatus rv = nsEventStatus_eIgnore;

  if (mTouches.Length() > 1 && !aClearTouches) {
    const ScreenIntPoint& firstTouch = mTouches[0].mScreenPoint,
                         secondTouch = mTouches[mTouches.Length() - 1].mScreenPoint;
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
                                     currentSpan);

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
                                   mPreviousSpan);

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
                                 mTouches[0].mScreenPoint,
                                 1.0f,
                                 1.0f);

    mAsyncPanZoomController->HandleInputEvent(pinchEvent);

    mState = GESTURE_NONE;

    rv = nsEventStatus_eConsumeNoDefault;
  }

  if (aClearTouches) {
    mTouches.Clear();
  }

  return rv;
}

nsEventStatus GestureEventListener::HandleSingleTapUpEvent(const MultiTouchInput& aEvent)
{
  TapGestureInput tapEvent(TapGestureInput::TAPGESTURE_UP, aEvent.mTime, aEvent.mTouches[0].mScreenPoint);
  return mAsyncPanZoomController->HandleInputEvent(tapEvent);
}

nsEventStatus GestureEventListener::HandleSingleTapConfirmedEvent(const MultiTouchInput& aEvent)
{
  TapGestureInput tapEvent(TapGestureInput::TAPGESTURE_CONFIRMED, aEvent.mTime, aEvent.mTouches[0].mScreenPoint);
  return mAsyncPanZoomController->HandleInputEvent(tapEvent);
}

nsEventStatus GestureEventListener::HandleLongTapEvent(const MultiTouchInput& aEvent)
{
  TapGestureInput tapEvent(TapGestureInput::TAPGESTURE_LONG, aEvent.mTime, aEvent.mTouches[0].mScreenPoint);
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
    mState = GESTURE_NONE;
    break;
  default:
    break;
  }

  return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus GestureEventListener::HandleDoubleTap(const MultiTouchInput& aEvent)
{
  TapGestureInput tapEvent(TapGestureInput::TAPGESTURE_DOUBLE, aEvent.mTime, aEvent.mTouches[0].mScreenPoint);
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
    mState = GESTURE_NONE;

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
