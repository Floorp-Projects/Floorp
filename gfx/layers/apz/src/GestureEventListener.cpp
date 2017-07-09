/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GestureEventListener.h"
#include <math.h>                       // for fabsf
#include <stddef.h>                     // for size_t
#include "AsyncPanZoomController.h"     // for AsyncPanZoomController
#include "InputBlockState.h"            // for TouchBlockState
#include "base/task.h"                  // for CancelableTask, etc
#include "gfxPrefs.h"                   // for gfxPrefs
#include "InputBlockState.h"            // for TouchBlockState
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
 * Amount of span or focus change needed to take us from the GESTURE_WAITING_PINCH
 * state to the GESTURE_PINCH state. This is measured as either a change in distance
 * between the fingers used to compute the span ratio, or the a change in
 * position of the focus point between the two fingers.
 */
static const float PINCH_START_THRESHOLD = 35.0f;

/**
 * Determines how fast a one touch pinch zooms in and out. The greater the
 * value, the faster it zooms.
 */
static const float ONE_TOUCH_PINCH_SPEED = 0.005f;

static bool sLongTapEnabled = true;

ParentLayerPoint GetCurrentFocus(const MultiTouchInput& aEvent)
{
  const ParentLayerPoint& firstTouch = aEvent.mTouches[0].mLocalScreenPoint;
  const ParentLayerPoint& secondTouch = aEvent.mTouches[1].mLocalScreenPoint;
  return (firstTouch + secondTouch) / 2;
}

ParentLayerCoord GetCurrentSpan(const MultiTouchInput& aEvent)
{
  const ParentLayerPoint& firstTouch = aEvent.mTouches[0].mLocalScreenPoint;
  const ParentLayerPoint& secondTouch = aEvent.mTouches[1].mLocalScreenPoint;
  ParentLayerPoint delta = secondTouch - firstTouch;
  return delta.Length();
}

ParentLayerCoord GestureEventListener::GetYSpanFromStartPoint()
{
  const ParentLayerPoint start = mTouchStartPosition;
  const ParentLayerPoint& current = mTouches[0].mLocalScreenPoint;
  return current.y - start.y;
}

TapGestureInput CreateTapEvent(const MultiTouchInput& aTouch, TapGestureInput::TapGestureType aType)
{
  return TapGestureInput(aType,
                         aTouch.mTime,
                         aTouch.mTimeStamp,
                         aTouch.mTouches[0].mScreenPoint,
                         aTouch.modifiers);
}

GestureEventListener::GestureEventListener(AsyncPanZoomController* aAsyncPanZoomController)
  : mAsyncPanZoomController(aAsyncPanZoomController),
    mState(GESTURE_NONE),
    mSpanChange(0.0f),
    mPreviousSpan(0.0f),
    mFocusChange(0.0f),
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
    // Cache every touch.
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
    // Update the screen points of the cached touches.
    for (size_t i = 0; i < aEvent.mTouches.Length(); i++) {
      for (size_t j = 0; j < mTouches.Length(); j++) {
        if (aEvent.mTouches[i].mIdentifier == mTouches[j].mIdentifier) {
          mTouches[j].mScreenPoint = aEvent.mTouches[i].mScreenPoint;
          mTouches[j].mLocalScreenPoint = aEvent.mTouches[i].mLocalScreenPoint;
        }
      }
    }
    rv = HandleInputTouchMove();
    break;
  case MultiTouchInput::MULTITOUCH_END:
    // Remove the cache of the touch that ended.
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

/* static */
void GestureEventListener::SetLongTapEnabled(bool aLongTapEnabled)
{
  sLongTapEnabled = aLongTapEnabled;
}

nsEventStatus GestureEventListener::HandleInputTouchSingleStart()
{
  switch (mState) {
  case GESTURE_NONE:
    SetState(GESTURE_FIRST_SINGLE_TOUCH_DOWN);
    mTouchStartPosition = mLastTouchInput.mTouches[0].mLocalScreenPoint;

    if (sLongTapEnabled) {
      CreateLongTapTimeoutTask();
    }
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
  case GESTURE_SECOND_SINGLE_TOUCH_DOWN:
    // Cancel wait for double tap
    CancelMaxTapTimeoutTask();
    MOZ_ASSERT(mSingleTapSent.isSome());
    if (!mSingleTapSent.value()) {
      TriggerSingleTapConfirmedEvent();
    }
    mSingleTapSent = Nothing();
    SetState(GESTURE_MULTI_TOUCH_DOWN);
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
  case GESTURE_FIRST_SINGLE_TOUCH_MAX_TAP_DOWN: {
    // If we move too much, bail out of the tap.
    if (MoveDistanceIsLarge()) {
      CancelLongTapTimeoutTask();
      CancelMaxTapTimeoutTask();
      mSingleTapSent = Nothing();
      SetState(GESTURE_NONE);
    }
    break;
  }

  // The user has performed a double tap, but not lifted her finger.
  case GESTURE_SECOND_SINGLE_TOUCH_DOWN: {
    // If touch has moved noticeably (within MAX_TAP_TIME), change state.
    if (MoveDistanceIsLarge()) {
      CancelLongTapTimeoutTask();
      CancelMaxTapTimeoutTask();
      mSingleTapSent = Nothing();
      if (!gfxPrefs::APZOneTouchPinchEnabled()) {
        // If the one-touch-pinch feature is disabled, bail out of the double-
        // tap gesture instead.
        SetState(GESTURE_NONE);
        break;
      }

      SetState(GESTURE_ONE_TOUCH_PINCH);

      ParentLayerCoord currentSpan = 1.0f;
      ParentLayerPoint currentFocus = mTouchStartPosition;

      PinchGestureInput pinchEvent(PinchGestureInput::PINCHGESTURE_START,
                                   mLastTouchInput.mTime,
                                   mLastTouchInput.mTimeStamp,
                                   currentFocus,
                                   currentSpan,
                                   currentSpan,
                                   mLastTouchInput.modifiers);

      rv = mAsyncPanZoomController->HandleGestureEvent(pinchEvent);

      mPreviousSpan = currentSpan;
      mPreviousFocus = currentFocus;
    }
    break;
  }

  case GESTURE_MULTI_TOUCH_DOWN: {
    if (mLastTouchInput.mTouches.Length() < 2) {
      NS_WARNING("Wrong input: less than 2 moving points in GESTURE_MULTI_TOUCH_DOWN state");
      break;
    }

    ParentLayerCoord currentSpan = GetCurrentSpan(mLastTouchInput);
    ParentLayerPoint currentFocus = GetCurrentFocus(mLastTouchInput);

    mSpanChange += fabsf(currentSpan - mPreviousSpan);
    mFocusChange += (currentFocus - mPreviousFocus).Length();
    if (mSpanChange > PINCH_START_THRESHOLD ||
        mFocusChange > PINCH_START_THRESHOLD) {
      SetState(GESTURE_PINCH);
      PinchGestureInput pinchEvent(PinchGestureInput::PINCHGESTURE_START,
                                   mLastTouchInput.mTime,
                                   mLastTouchInput.mTimeStamp,
                                   currentFocus,
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
    mPreviousFocus = currentFocus;
    break;
  }

  case GESTURE_PINCH: {
    if (mLastTouchInput.mTouches.Length() < 2) {
      NS_WARNING("Wrong input: less than 2 moving points in GESTURE_PINCH state");
      // Prevent APZC::OnTouchMove() from handling this wrong input
      rv = nsEventStatus_eConsumeNoDefault;
      break;
    }

    ParentLayerCoord currentSpan = GetCurrentSpan(mLastTouchInput);

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

  case GESTURE_ONE_TOUCH_PINCH: {
    ParentLayerCoord currentSpan = GetYSpanFromStartPoint();
    float effectiveSpan = 1.0f + (fabsf(currentSpan.value) * ONE_TOUCH_PINCH_SPEED);
    ParentLayerPoint currentFocus = mTouchStartPosition;

    // Invert zoom.
    if (currentSpan.value < 0) {
      effectiveSpan = 1.0f / effectiveSpan;
    }

    PinchGestureInput pinchEvent(PinchGestureInput::PINCHGESTURE_SCALE,
                                 mLastTouchInput.mTime,
                                 mLastTouchInput.mTimeStamp,
                                 currentFocus,
                                 effectiveSpan,
                                 mPreviousSpan,
                                 mLastTouchInput.modifiers);

    rv = mAsyncPanZoomController->HandleGestureEvent(pinchEvent);
    mPreviousSpan = effectiveSpan;

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
    mSingleTapSent = Some(tapupStatus != nsEventStatus_eIgnore);
    SetState(GESTURE_FIRST_SINGLE_TOUCH_UP);
    CreateMaxTapTimeoutTask();
    break;
  }

  case GESTURE_SECOND_SINGLE_TOUCH_DOWN: {
    CancelMaxTapTimeoutTask();
    MOZ_ASSERT(mSingleTapSent.isSome());
    mAsyncPanZoomController->HandleGestureEvent(
        CreateTapEvent(mLastTouchInput,
            mSingleTapSent.value() ? TapGestureInput::TAPGESTURE_SECOND
                                   : TapGestureInput::TAPGESTURE_DOUBLE));
    mSingleTapSent = Nothing();
    SetState(GESTURE_NONE);
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
      ParentLayerPoint point(-1, -1);
      if (mTouches.Length() == 1) {
        // As user still keeps one finger down the event's focus point should
        // contain meaningful data.
        point = mTouches[0].mLocalScreenPoint;
      }
      PinchGestureInput pinchEvent(PinchGestureInput::PINCHGESTURE_END,
                                   mLastTouchInput.mTime,
                                   mLastTouchInput.mTimeStamp,
                                   point,
                                   1.0f,
                                   1.0f,
                                   mLastTouchInput.modifiers);
      mAsyncPanZoomController->HandleGestureEvent(pinchEvent);
    }

    rv = nsEventStatus_eConsumeNoDefault;

    break;

  case GESTURE_ONE_TOUCH_PINCH: {
    SetState(GESTURE_NONE);
    ParentLayerPoint point(-1, -1);
    PinchGestureInput pinchEvent(PinchGestureInput::PINCHGESTURE_END,
                                 mLastTouchInput.mTime,
                                 mLastTouchInput.mTimeStamp,
                                 point,
                                 1.0f,
                                 1.0f,
                                 mLastTouchInput.modifiers);
    mAsyncPanZoomController->HandleGestureEvent(pinchEvent);

    rv = nsEventStatus_eConsumeNoDefault;

    break;
  }

  default:
    NS_WARNING("Unhandled state upon touch end");
    SetState(GESTURE_NONE);
    break;
  }

  return rv;
}

nsEventStatus GestureEventListener::HandleInputTouchCancel()
{
  mSingleTapSent = Nothing();
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
    MOZ_FALLTHROUGH;
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

void GestureEventListener::HandleInputTimeoutMaxTap(bool aDuringFastFling)
{
  GEL_LOG("Running max-tap timeout task in state %d\n", mState);

  mMaxTapTimeoutTask = nullptr;

  if (mState == GESTURE_FIRST_SINGLE_TOUCH_DOWN) {
    SetState(GESTURE_FIRST_SINGLE_TOUCH_MAX_TAP_DOWN);
  } else if (mState == GESTURE_FIRST_SINGLE_TOUCH_UP ||
             mState == GESTURE_SECOND_SINGLE_TOUCH_DOWN) {
    MOZ_ASSERT(mSingleTapSent.isSome());
    if (!aDuringFastFling && !mSingleTapSent.value()) {
      TriggerSingleTapConfirmedEvent();
    }
    mSingleTapSent = Nothing();
    SetState(GESTURE_NONE);
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
    mFocusChange = 0.0f;
  } else if (mState == GESTURE_MULTI_TOUCH_DOWN) {
    mPreviousSpan = GetCurrentSpan(mLastTouchInput);
    mPreviousFocus = GetCurrentFocus(mLastTouchInput);
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
  RefPtr<CancelableRunnable> task = NewCancelableRunnableMethod(
    "layers::GestureEventListener::HandleInputTimeoutLongTap",
    this,
    &GestureEventListener::HandleInputTimeoutLongTap);

  mLongTapTimeoutTask = task;
  mAsyncPanZoomController->PostDelayedTask(
    task.forget(),
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

  TouchBlockState* block = mAsyncPanZoomController->GetInputQueue()->GetCurrentTouchBlock();
  MOZ_ASSERT(block);
  RefPtr<CancelableRunnable> task = NewCancelableRunnableMethod<bool>(
    "layers::GestureEventListener::HandleInputTimeoutMaxTap",
    this,
    &GestureEventListener::HandleInputTimeoutMaxTap,
    block->IsDuringFastFling());

  mMaxTapTimeoutTask = task;
  mAsyncPanZoomController->PostDelayedTask(
    task.forget(),
    MAX_TAP_TIME);
}

} // namespace layers
} // namespace mozilla
