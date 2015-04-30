/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GestureEventListener.h"
#include <math.h>                       // for fabsf
#include <stddef.h>                     // for size_t
#include "AsyncPanZoomController.h"     // for AsyncPanZoomController
#include "base/task.h"                  // for CancelableTask, etc
#include "gfxPrefs.h"                   // for gfxPrefs
#include "mozilla/SizePrintfMacros.h"   // for PRIuSIZE
#include "nsDebug.h"                    // for NS_WARNING
#include "nsMathUtils.h"                // for NS_hypot

#define GEL_LOG(...)
// #define GEL_LOG(...) printf_stderr("GEL: " __VA_ARGS__)

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

ParentLayerPoint GetCurrentFocus(const MultiTouchInput& aEvent)
{
  const ParentLayerPoint& firstTouch = aEvent.mTouches[0].mLocalScreenPoint;
  const ParentLayerPoint& secondTouch = aEvent.mTouches[1].mLocalScreenPoint;
  return (firstTouch + secondTouch) / 2;
}

float GetCurrentSpan(const MultiTouchInput& aEvent)
{
  const ParentLayerPoint& firstTouch = aEvent.mTouches[0].mLocalScreenPoint;
  const ParentLayerPoint& secondTouch = aEvent.mTouches[1].mLocalScreenPoint;
  ParentLayerPoint delta = secondTouch - firstTouch;
  return delta.Length();
}

TapGestureInput CreateTapEvent(const MultiTouchInput& aTouch, TapGestureInput::TapGestureType aType)
{
  return TapGestureInput(aType,
                         aTouch.mTime,
                         aTouch.mTimeStamp,
                         // Use mLocalScreenPoint as this goes directly to APZC
                         // without being transformed in APZCTreeManager.
                         aTouch.mTouches[0].mLocalScreenPoint,
                         aTouch.modifiers);
}

GestureEventListener::GestureEventListener(AsyncPanZoomController* aAsyncPanZoomController)
  : mAsyncPanZoomController(aAsyncPanZoomController),
    mState(GESTURE_NONE),
    mSpanChange(0.0f),
    mPreviousSpan(0.0f),
    mLastTouchInput(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0),
    mLastTapInput(MultiTouchInput::MULTITOUCH_START, 0, TimeStamp(), 0),
    mLongTapTimeoutTask(nullptr),
    mMaxTapTimeoutTask(nullptr)
{
}

GestureEventListener::~GestureEventListener()
{
}

nsEventStatus GestureEventListener::HandleInputEvent(const MultiTouchInput& aEvent)
{
  GEL_LOG("Receiving event type %d with %" PRIuSIZE " touches in state %d\n", aEvent.mType, aEvent.mTouches.Length(), mState);

  nsEventStatus rv = nsEventStatus_eIgnore;

  // Cache the current event since it may become the single or long tap that we
  // send.
  mLastTouchInput = aEvent;

  switch (aEvent.mType) {
  case MultiTouchInput::MULTITOUCH_START:
    mTouches.Clear();
    for (size_t i = 0; i < aEvent.mTouches.Length(); i++) {
      mTouches.AppendElement(aEvent.mTouches[i]);
    }

    if (aEvent.mTouches.Length() == 1) {
      rv = HandleInputTouchSingleStart();
    } else {
      rv = HandleInputTouchMultiStart();
    }
    break;
  case MultiTouchInput::MULTITOUCH_MOVE:
    rv = HandleInputTouchMove();
    break;
  case MultiTouchInput::MULTITOUCH_END:
    for (size_t i = 0; i < aEvent.mTouches.Length(); i++) {
      for (size_t j = 0; j < mTouches.Length(); j++) {
        if (aEvent.mTouches[i].mIdentifier == mTouches[j].mIdentifier) {
          mTouches.RemoveElementAt(j);
          break;
        }
      }
    }

    rv = HandleInputTouchEnd();
    break;
  case MultiTouchInput::MULTITOUCH_CANCEL:
    mTouches.Clear();
    rv = HandleInputTouchCancel();
    break;
  }

  return rv;
}

int32_t GestureEventListener::GetLastTouchIdentifier() const
{
  if (mTouches.Length() != 1) {
    NS_WARNING("GetLastTouchIdentifier() called when last touch event "
               "did not have one touch");
  }
  return mTouches.IsEmpty() ? -1 : mTouches[0].mIdentifier;
}


nsEventStatus GestureEventListener::HandleInputTouchSingleStart()
{
  switch (mState) {
  case GESTURE_NONE:
    SetState(GESTURE_FIRST_SINGLE_TOUCH_DOWN);
    mTouchStartPosition = mLastTouchInput.mTouches[0].mLocalScreenPoint;

    CreateLongTapTimeoutTask();
    CreateMaxTapTimeoutTask();
    break;
  case GESTURE_FIRST_SINGLE_TOUCH_UP:
    SetState(GESTURE_SECOND_SINGLE_TOUCH_DOWN);
    break;
  default:
    NS_WARNING("Unhandled state upon single touch start");
    SetState(GESTURE_NONE);
    break;
  }

  return nsEventStatus_eIgnore;
}

nsEventStatus GestureEventListener::HandleInputTouchMultiStart()
{
  nsEventStatus rv = nsEventStatus_eIgnore;

  switch (mState) {
  case GESTURE_NONE:
    SetState(GESTURE_MULTI_TOUCH_DOWN);
    break;
  case GESTURE_FIRST_SINGLE_TOUCH_DOWN:
    CancelLongTapTimeoutTask();
    CancelMaxTapTimeoutTask();
    SetState(GESTURE_MULTI_TOUCH_DOWN);
    // Prevent APZC::OnTouchStart() from handling MULTITOUCH_START event
    rv = nsEventStatus_eConsumeNoDefault;
    break;
  case GESTURE_FIRST_SINGLE_TOUCH_MAX_TAP_DOWN:
    CancelLongTapTimeoutTask();
    SetState(GESTURE_MULTI_TOUCH_DOWN);
    // Prevent APZC::OnTouchStart() from handling MULTITOUCH_START event
    rv = nsEventStatus_eConsumeNoDefault;
    break;
  case GESTURE_FIRST_SINGLE_TOUCH_UP:
    // Cancel wait for double tap
    CancelMaxTapTimeoutTask();
    SetState(GESTURE_MULTI_TOUCH_DOWN);
    TriggerSingleTapConfirmedEvent();
    // Prevent APZC::OnTouchStart() from handling MULTITOUCH_START event
    rv = nsEventStatus_eConsumeNoDefault;
    break;
  case GESTURE_SECOND_SINGLE_TOUCH_DOWN:
    // Cancel wait for single tap
    CancelMaxTapTimeoutTask();
    SetState(GESTURE_MULTI_TOUCH_DOWN);
    TriggerSingleTapConfirmedEvent();
    // Prevent APZC::OnTouchStart() from handling MULTITOUCH_START event
    rv = nsEventStatus_eConsumeNoDefault;
    break;
  case GESTURE_LONG_TOUCH_DOWN:
    SetState(GESTURE_MULTI_TOUCH_DOWN);
    break;
  case GESTURE_MULTI_TOUCH_DOWN:
  case GESTURE_PINCH:
    // Prevent APZC::OnTouchStart() from handling MULTITOUCH_START event
    rv = nsEventStatus_eConsumeNoDefault;
    break;
  default:
    NS_WARNING("Unhandled state upon multitouch start");
    SetState(GESTURE_NONE);
    break;
  }

  return rv;
}

bool GestureEventListener::MoveDistanceIsLarge()
{
  const ParentLayerPoint start = mLastTouchInput.mTouches[0].mLocalScreenPoint;
  ParentLayerPoint delta = start - mTouchStartPosition;
  ScreenPoint screenDelta = mAsyncPanZoomController->ToScreenCoordinates(delta, start);
  return (screenDelta.Length() > AsyncPanZoomController::GetTouchStartTolerance());
}

nsEventStatus GestureEventListener::HandleInputTouchMove()
{
  nsEventStatus rv = nsEventStatus_eIgnore;

  switch (mState) {
  case GESTURE_NONE:
    // Ignore this input signal as the corresponding events get handled by APZC
    break;

  case GESTURE_LONG_TOUCH_DOWN:
    if (MoveDistanceIsLarge()) {
      // So that we don't fire a long-tap-up if the user moves around after a
      // long-tap
      SetState(GESTURE_NONE);
    }
    break;

  case GESTURE_FIRST_SINGLE_TOUCH_DOWN:
  case GESTURE_FIRST_SINGLE_TOUCH_MAX_TAP_DOWN:
  case GESTURE_SECOND_SINGLE_TOUCH_DOWN: {
    // If we move too much, bail out of the tap.
    if (MoveDistanceIsLarge()) {
      CancelLongTapTimeoutTask();
      CancelMaxTapTimeoutTask();
      SetState(GESTURE_NONE);
    }
    break;
  }

  case GESTURE_MULTI_TOUCH_DOWN: {
    if (mLastTouchInput.mTouches.Length() < 2) {
      NS_WARNING("Wrong input: less than 2 moving points in GESTURE_MULTI_TOUCH_DOWN state");
      break;
    }

    float currentSpan = GetCurrentSpan(mLastTouchInput);

    mSpanChange += fabsf(currentSpan - mPreviousSpan);
    if (mSpanChange > PINCH_START_THRESHOLD) {
      SetState(GESTURE_PINCH);
      PinchGestureInput pinchEvent(PinchGestureInput::PINCHGESTURE_START,
                                   mLastTouchInput.mTime,
                                   mLastTouchInput.mTimeStamp,
                                   GetCurrentFocus(mLastTouchInput),
                                   currentSpan,
                                   currentSpan,
                                   mLastTouchInput.modifiers);

      rv = mAsyncPanZoomController->HandleGestureEvent(pinchEvent);
    } else {
      // Prevent APZC::OnTouchMove from processing a move event when two
      // touches are active
      rv = nsEventStatus_eConsumeNoDefault;
    }

    mPreviousSpan = currentSpan;
    break;
  }

  case GESTURE_PINCH: {
    if (mLastTouchInput.mTouches.Length() < 2) {
      NS_WARNING("Wrong input: less than 2 moving points in GESTURE_PINCH state");
      // Prevent APZC::OnTouchMove() from handling this wrong input
      rv = nsEventStatus_eConsumeNoDefault;
      break;
    }

    float currentSpan = GetCurrentSpan(mLastTouchInput);

    PinchGestureInput pinchEvent(PinchGestureInput::PINCHGESTURE_SCALE,
                                 mLastTouchInput.mTime,
                                 mLastTouchInput.mTimeStamp,
                                 GetCurrentFocus(mLastTouchInput),
                                 currentSpan,
                                 mPreviousSpan,
                                 mLastTouchInput.modifiers);

    rv = mAsyncPanZoomController->HandleGestureEvent(pinchEvent);
    mPreviousSpan = currentSpan;

    break;
  }

  default:
    NS_WARNING("Unhandled state upon touch move");
    SetState(GESTURE_NONE);
    break;
  }

  return rv;
}

nsEventStatus GestureEventListener::HandleInputTouchEnd()
{
  // We intentionally do not pass apzc return statuses up since
  // it may cause apzc stay in the touching state even after
  // gestures are completed (please see Bug 1013378 for reference).

  nsEventStatus rv = nsEventStatus_eIgnore;

  switch (mState) {
  case GESTURE_NONE:
    // GEL doesn't have a dedicated state for PANNING handled in APZC thus ignore.
    break;

  case GESTURE_FIRST_SINGLE_TOUCH_DOWN: {
    CancelLongTapTimeoutTask();
    CancelMaxTapTimeoutTask();
    nsEventStatus tapupStatus = mAsyncPanZoomController->HandleGestureEvent(
        CreateTapEvent(mLastTouchInput, TapGestureInput::TAPGESTURE_UP));
    if (tapupStatus == nsEventStatus_eIgnore) {
      SetState(GESTURE_FIRST_SINGLE_TOUCH_UP);
      CreateMaxTapTimeoutTask();
    } else {
      // We sent the tapup into content without waiting for a double tap
      SetState(GESTURE_NONE);
    }
    break;
  }

  case GESTURE_SECOND_SINGLE_TOUCH_DOWN: {
    CancelMaxTapTimeoutTask();
    SetState(GESTURE_NONE);
    mAsyncPanZoomController->HandleGestureEvent(
        CreateTapEvent(mLastTouchInput, TapGestureInput::TAPGESTURE_DOUBLE));
    break;
  }

  case GESTURE_FIRST_SINGLE_TOUCH_MAX_TAP_DOWN:
    CancelLongTapTimeoutTask();
    SetState(GESTURE_NONE);
    TriggerSingleTapConfirmedEvent();
    break;

  case GESTURE_LONG_TOUCH_DOWN: {
    SetState(GESTURE_NONE);
    mAsyncPanZoomController->HandleGestureEvent(
        CreateTapEvent(mLastTouchInput, TapGestureInput::TAPGESTURE_LONG_UP));
    break;
  }

  case GESTURE_MULTI_TOUCH_DOWN:
    if (mTouches.Length() < 2) {
      SetState(GESTURE_NONE);
    }
    break;

  case GESTURE_PINCH:
    if (mTouches.Length() < 2) {
      SetState(GESTURE_NONE);
      PinchGestureInput pinchEvent(PinchGestureInput::PINCHGESTURE_END,
                                   mLastTouchInput.mTime,
                                   mLastTouchInput.mTimeStamp,
                                   ScreenPoint(),
                                   1.0f,
                                   1.0f,
                                   mLastTouchInput.modifiers);
      mAsyncPanZoomController->HandleGestureEvent(pinchEvent);
    }

    rv = nsEventStatus_eConsumeNoDefault;

    break;

  default:
    NS_WARNING("Unhandled state upon touch end");
    SetState(GESTURE_NONE);
    break;
  }

  return rv;
}

nsEventStatus GestureEventListener::HandleInputTouchCancel()
{
  SetState(GESTURE_NONE);
  CancelMaxTapTimeoutTask();
  CancelLongTapTimeoutTask();
  return nsEventStatus_eIgnore;
}

void GestureEventListener::HandleInputTimeoutLongTap()
{
  GEL_LOG("Running long-tap timeout task in state %d\n", mState);

  mLongTapTimeoutTask = nullptr;

  switch (mState) {
  case GESTURE_FIRST_SINGLE_TOUCH_DOWN:
    // just in case MAX_TAP_TIME > ContextMenuDelay cancel MAX_TAP timer
    // and fall through
    CancelMaxTapTimeoutTask();
  case GESTURE_FIRST_SINGLE_TOUCH_MAX_TAP_DOWN: {
    SetState(GESTURE_LONG_TOUCH_DOWN);
    mAsyncPanZoomController->HandleGestureEvent(
        CreateTapEvent(mLastTouchInput, TapGestureInput::TAPGESTURE_LONG));
    break;
  }
  default:
    NS_WARNING("Unhandled state upon long tap timeout");
    SetState(GESTURE_NONE);
    break;
  }
}

void GestureEventListener::HandleInputTimeoutMaxTap()
{
  GEL_LOG("Running max-tap timeout task in state %d\n", mState);

  mMaxTapTimeoutTask = nullptr;

  if (mState == GESTURE_FIRST_SINGLE_TOUCH_DOWN) {
    SetState(GESTURE_FIRST_SINGLE_TOUCH_MAX_TAP_DOWN);
  } else if (mState == GESTURE_FIRST_SINGLE_TOUCH_UP ||
             mState == GESTURE_SECOND_SINGLE_TOUCH_DOWN) {
    SetState(GESTURE_NONE);
    TriggerSingleTapConfirmedEvent();
  } else {
    NS_WARNING("Unhandled state upon MAX_TAP timeout");
    SetState(GESTURE_NONE);
  }
}

void GestureEventListener::TriggerSingleTapConfirmedEvent()
{
  mAsyncPanZoomController->HandleGestureEvent(
      CreateTapEvent(mLastTapInput, TapGestureInput::TAPGESTURE_CONFIRMED));
}

void GestureEventListener::SetState(GestureState aState)
{
  mState = aState;

  if (mState == GESTURE_NONE) {
    mSpanChange = 0.0f;
    mPreviousSpan = 0.0f;
  } else if (mState == GESTURE_MULTI_TOUCH_DOWN) {
    mPreviousSpan = GetCurrentSpan(mLastTouchInput);
  }
}

void GestureEventListener::CancelLongTapTimeoutTask()
{
  if (mState == GESTURE_SECOND_SINGLE_TOUCH_DOWN) {
    // being in this state means the task has been canceled already
    return;
  }

  if (mLongTapTimeoutTask) {
    mLongTapTimeoutTask->Cancel();
    mLongTapTimeoutTask = nullptr;
  }
}

void GestureEventListener::CreateLongTapTimeoutTask()
{
  mLongTapTimeoutTask =
    NewRunnableMethod(this, &GestureEventListener::HandleInputTimeoutLongTap);

  mAsyncPanZoomController->PostDelayedTask(
    mLongTapTimeoutTask,
    gfxPrefs::UiClickHoldContextMenusDelay());
}

void GestureEventListener::CancelMaxTapTimeoutTask()
{
  if (mState == GESTURE_FIRST_SINGLE_TOUCH_MAX_TAP_DOWN) {
    // being in this state means the timer has just been triggered
    return;
  }

  if (mMaxTapTimeoutTask) {
    mMaxTapTimeoutTask->Cancel();
    mMaxTapTimeoutTask = nullptr;
  }
}

void GestureEventListener::CreateMaxTapTimeoutTask()
{
  mLastTapInput = mLastTouchInput;

  mMaxTapTimeoutTask =
    NewRunnableMethod(this, &GestureEventListener::HandleInputTimeoutMaxTap);

  mAsyncPanZoomController->PostDelayedTask(
    mMaxTapTimeoutTask,
    MAX_TAP_TIME);
}

}
}
