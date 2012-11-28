/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorParent.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Constants.h"
#include "mozilla/Util.h"
#include "mozilla/XPCOM.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPtr.h"
#include "AsyncPanZoomController.h"
#include "GestureEventListener.h"
#include "nsIThreadManager.h"
#include "nsThreadUtils.h"
#include "Layers.h"
#include "AnimationCommon.h"

using namespace mozilla::css;

namespace mozilla {
namespace layers {

const float AsyncPanZoomController::TOUCH_START_TOLERANCE = 1.0f/16.0f;

static const float EPSILON = 0.0001;

/**
 * Maximum amount of time while panning before sending a viewport change. This
 * will asynchronously repaint the page. It is also forced when panning stops.
 */
static const int32_t PAN_REPAINT_INTERVAL = 250;

/**
 * Maximum amount of time flinging before sending a viewport change. This will
 * asynchronously repaint the page.
 */
static const int32_t FLING_REPAINT_INTERVAL = 75;

/**
 * Minimum amount of speed along an axis before we begin painting far ahead by
 * adjusting the displayport.
 */
static const float MIN_SKATE_SPEED = 0.7f;

/**
 * Duration of a zoom to animation.
 */
static const TimeDuration ZOOM_TO_DURATION = TimeDuration::FromSeconds(0.25);

/**
 * Computed time function used for sampling frames of a zoom to animation.
 */
StaticAutoPtr<ComputedTimingFunction> gComputedTimingFunction;

/**
 * Maximum zoom amount, always used, even if a page asks for higher.
 */
static const double MAX_ZOOM = 8.0;

/**
 * Minimum zoom amount, always used, even if a page asks for lower.
 */
static const double MIN_ZOOM = 0.125;

/**
 * Amount of time before we timeout touch event listeners. For example, if
 * content is being unruly/slow and we don't get a response back within this
 * time, we will just pretend that content did not preventDefault any touch
 * events we dispatched to it.
 */
static const int TOUCH_LISTENER_TIMEOUT = 300;

/**
 * Number of samples to store of how long it took to paint after the previous
 * requests.
 */
static const int NUM_PAINT_DURATION_SAMPLES = 3;

AsyncPanZoomController::AsyncPanZoomController(GeckoContentController* aGeckoContentController,
                                               GestureBehavior aGestures)
  :  mGeckoContentController(aGeckoContentController),
     mTouchListenerTimeoutTask(nullptr),
     mX(this),
     mY(this),
     mAllowZoom(true),
     mMinZoom(MIN_ZOOM),
     mMaxZoom(MAX_ZOOM),
     mMonitor("AsyncPanZoomController"),
     mLastSampleTime(TimeStamp::Now()),
     mState(NOTHING),
     mDPI(72),
     mWaitingForContentToPaint(false),
     mDisableNextTouchBatch(false),
     mHandlingTouchQueue(false)
{
  if (aGestures == USE_GESTURE_DETECTOR) {
    mGestureEventListener = new GestureEventListener(this);
  }

  SetDPI(mDPI);

  if (!gComputedTimingFunction) {
    gComputedTimingFunction = new ComputedTimingFunction();
    gComputedTimingFunction->Init(
      nsTimingFunction(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE));
    ClearOnShutdown(&gComputedTimingFunction);
  }
}

AsyncPanZoomController::~AsyncPanZoomController() {

}

static gfx::Point
WidgetSpaceToCompensatedViewportSpace(const gfx::Point& aPoint,
                                      gfxFloat aCurrentZoom)
{
  // Transform the input point from local widget space to the content document
  // space that the user is seeing, from last composite.
  gfx::Point pt(aPoint);
  pt = pt / aCurrentZoom;

  // FIXME/bug 775451: this doesn't attempt to compensate for content transforms
  // in effect on the compositor.  The problem is that it's very hard for us to
  // know what content CSS pixel is at widget point 0,0 based on information
  // available here.  So we use this hacky implementation for now, which works
  // in quiescent states.

  return pt;
}

nsEventStatus
AsyncPanZoomController::ReceiveInputEvent(const nsInputEvent& aEvent,
                                          nsInputEvent* aOutEvent)
{
  gfxFloat currentResolution;
  gfx::Point currentScrollOffset, lastScrollOffset;
  {
    MonitorAutoLock monitor(mMonitor);
    currentResolution = CalculateResolution(mFrameMetrics).width;
    currentScrollOffset = gfx::Point(mFrameMetrics.mScrollOffset.x,
                                     mFrameMetrics.mScrollOffset.y);
    lastScrollOffset = gfx::Point(mLastContentPaintMetrics.mScrollOffset.x,
                                  mLastContentPaintMetrics.mScrollOffset.y);
  }

  nsEventStatus status;
  switch (aEvent.eventStructType) {
  case NS_TOUCH_EVENT: {
    MultiTouchInput event(static_cast<const nsTouchEvent&>(aEvent));
    status = ReceiveInputEvent(event);
    break;
  }
  case NS_MOUSE_EVENT: {
    MultiTouchInput event(static_cast<const nsMouseEvent&>(aEvent));
    status = ReceiveInputEvent(event);
    break;
  }
  default:
    status = nsEventStatus_eIgnore;
    break;
  }

  switch (aEvent.eventStructType) {
  case NS_TOUCH_EVENT: {
    nsTouchEvent* touchEvent = static_cast<nsTouchEvent*>(aOutEvent);
    const nsTArray<nsCOMPtr<nsIDOMTouch> >& touches = touchEvent->touches;
    for (uint32_t i = 0; i < touches.Length(); ++i) {
      nsIDOMTouch* touch = touches[i];
      if (touch) {
        gfx::Point refPoint = WidgetSpaceToCompensatedViewportSpace(
          gfx::Point(touch->mRefPoint.x, touch->mRefPoint.y),
          currentResolution);
        touch->mRefPoint = nsIntPoint(refPoint.x, refPoint.y);
      }
    }
    break;
  }
  default: {
    gfx::Point refPoint = WidgetSpaceToCompensatedViewportSpace(
      gfx::Point(aOutEvent->refPoint.x, aOutEvent->refPoint.y),
      currentResolution);
    aOutEvent->refPoint = nsIntPoint(refPoint.x, refPoint.y);
    break;
  }
  }

  return status;
}

nsEventStatus AsyncPanZoomController::ReceiveInputEvent(const InputData& aEvent) {
  // If we may have touch listeners, we enable the machinery that allows touch
  // listeners to preventDefault any touch inputs. This should not happen unless
  // there are actually touch listeners as it introduces potentially unbounded
  // lag because it causes a round-trip through content.  Usually, if content is
  // responding in a timely fashion, this only introduces a nearly constant few
  // hundred ms of lag.
  if (mFrameMetrics.mMayHaveTouchListeners && aEvent.mInputType == MULTITOUCH_INPUT &&
      (mState == NOTHING || mState == TOUCHING || mState == PANNING)) {
    const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
    if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_START) {
      SetState(WAITING_LISTENERS);
    }
  }

  if (mState == WAITING_LISTENERS || mHandlingTouchQueue) {
    if (aEvent.mInputType == MULTITOUCH_INPUT) {
      const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
      mTouchQueue.AppendElement(multiTouchInput);

      if (!mTouchListenerTimeoutTask) {
        mTouchListenerTimeoutTask =
          NewRunnableMethod(this, &AsyncPanZoomController::TimeoutTouchListeners);

        MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          mTouchListenerTimeoutTask,
          TOUCH_LISTENER_TIMEOUT);
      }
    }
    return nsEventStatus_eConsumeNoDefault;
  }

  return HandleInputEvent(aEvent);
}

nsEventStatus AsyncPanZoomController::HandleInputEvent(const InputData& aEvent) {
  nsEventStatus rv = nsEventStatus_eIgnore;

  if (mGestureEventListener && !mDisableNextTouchBatch) {
    rv = mGestureEventListener->HandleInputEvent(aEvent);
    if (rv == nsEventStatus_eConsumeNoDefault)
      return rv;
  }

  if (mDelayPanning && aEvent.mInputType == MULTITOUCH_INPUT) {
    const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
    if (multiTouchInput.mType == MultiTouchInput::MULTITOUCH_MOVE) {
      // Let BrowserElementScrolling perform panning gesture first.
      SetState(WAITING_LISTENERS);
      mTouchQueue.AppendElement(multiTouchInput);

      if (!mTouchListenerTimeoutTask) {
        mTouchListenerTimeoutTask =
          NewRunnableMethod(this, &AsyncPanZoomController::TimeoutTouchListeners);

        MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          mTouchListenerTimeoutTask,
          TOUCH_LISTENER_TIMEOUT);
      }
      return nsEventStatus_eConsumeNoDefault;
    }
  }

  switch (aEvent.mInputType) {
  case MULTITOUCH_INPUT: {
    const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();
    switch (multiTouchInput.mType) {
      case MultiTouchInput::MULTITOUCH_START: rv = OnTouchStart(multiTouchInput); break;
      case MultiTouchInput::MULTITOUCH_MOVE: rv = OnTouchMove(multiTouchInput); break;
      case MultiTouchInput::MULTITOUCH_END: rv = OnTouchEnd(multiTouchInput); break;
      case MultiTouchInput::MULTITOUCH_CANCEL: rv = OnTouchCancel(multiTouchInput); break;
      default: NS_WARNING("Unhandled multitouch"); break;
    }
    break;
  }
  case PINCHGESTURE_INPUT: {
    const PinchGestureInput& pinchGestureInput = aEvent.AsPinchGestureInput();
    switch (pinchGestureInput.mType) {
      case PinchGestureInput::PINCHGESTURE_START: rv = OnScaleBegin(pinchGestureInput); break;
      case PinchGestureInput::PINCHGESTURE_SCALE: rv = OnScale(pinchGestureInput); break;
      case PinchGestureInput::PINCHGESTURE_END: rv = OnScaleEnd(pinchGestureInput); break;
      default: NS_WARNING("Unhandled pinch gesture"); break;
    }
    break;
  }
  case TAPGESTURE_INPUT: {
    const TapGestureInput& tapGestureInput = aEvent.AsTapGestureInput();
    switch (tapGestureInput.mType) {
      case TapGestureInput::TAPGESTURE_LONG: rv = OnLongPress(tapGestureInput); break;
      case TapGestureInput::TAPGESTURE_UP: rv = OnSingleTapUp(tapGestureInput); break;
      case TapGestureInput::TAPGESTURE_CONFIRMED: rv = OnSingleTapConfirmed(tapGestureInput); break;
      case TapGestureInput::TAPGESTURE_DOUBLE: rv = OnDoubleTap(tapGestureInput); break;
      case TapGestureInput::TAPGESTURE_CANCEL: rv = OnCancelTap(tapGestureInput); break;
      default: NS_WARNING("Unhandled tap gesture"); break;
    }
    break;
  }
  default: NS_WARNING("Unhandled input event"); break;
  }

  mLastEventTime = aEvent.mTime;
  return rv;
}

nsEventStatus AsyncPanZoomController::OnTouchStart(const MultiTouchInput& aEvent) {
  SingleTouchData& touch = GetFirstSingleTouch(aEvent);

  nsIntPoint point = touch.mScreenPoint;
  int32_t xPos = point.x, yPos = point.y;

  switch (mState) {
    case ANIMATING_ZOOM:
      // We just interrupted a double-tap animation, so force a redraw in case
      // this touchstart is just a tap that doesn't end up triggering a redraw.
      {
        MonitorAutoLock monitor(mMonitor);
        // Bring the resolution back in sync with the zoom.
        SetZoomAndResolution(mFrameMetrics.mZoom.width);
        RequestContentRepaint();
        ScheduleComposite();
      }
      // Fall through.
    case FLING:
      CancelAnimation();
      // Fall through.
    case NOTHING:
      mX.StartTouch(xPos);
      mY.StartTouch(yPos);
      SetState(TOUCHING);
      break;
    case TOUCHING:
    case PANNING:
    case PINCHING:
    case WAITING_LISTENERS:
      NS_WARNING("Received impossible touch in OnTouchStart");
      break;
    default:
      NS_WARNING("Unhandled case in OnTouchStart");
      break;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchMove(const MultiTouchInput& aEvent) {
  if (mDisableNextTouchBatch) {
    return nsEventStatus_eIgnore;
  }

  switch (mState) {
    case FLING:
    case NOTHING:
    case ANIMATING_ZOOM:
      // May happen if the user double-taps and drags without lifting after the
      // second tap. Ignore the move if this happens.
      return nsEventStatus_eIgnore;

    case TOUCHING: {
      float panThreshold = TOUCH_START_TOLERANCE * mDPI;
      UpdateWithTouchAtDevicePoint(aEvent);

      if (PanDistance() < panThreshold) {
        return nsEventStatus_eIgnore;
      }

      StartPanning(aEvent);

      return nsEventStatus_eConsumeNoDefault;
    }

    case PANNING:
      TrackTouch(aEvent);
      return nsEventStatus_eConsumeNoDefault;

    case PINCHING:
      // The scale gesture listener should have handled this.
      NS_WARNING("Gesture listener should have handled pinching in OnTouchMove.");
      return nsEventStatus_eIgnore;

    case WAITING_LISTENERS:
      NS_WARNING("Received impossible touch in OnTouchMove");
      break;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchEnd(const MultiTouchInput& aEvent) {
  if (mDisableNextTouchBatch) {
    mDisableNextTouchBatch = false;
    return nsEventStatus_eIgnore;
  }

  switch (mState) {
  case FLING:
    // Should never happen.
    NS_WARNING("Received impossible touch end in OnTouchEnd.");
    // Fall through.
  case ANIMATING_ZOOM:
  case NOTHING:
    // May happen if the user double-taps and drags without lifting after the
    // second tap. Ignore if this happens.
    return nsEventStatus_eIgnore;

  case TOUCHING:
    SetState(NOTHING);
    return nsEventStatus_eIgnore;

  case PANNING:
    {
      MonitorAutoLock monitor(mMonitor);
      ScheduleComposite();
      RequestContentRepaint();
    }
    mX.EndTouch();
    mY.EndTouch();
    SetState(FLING);
    return nsEventStatus_eConsumeNoDefault;

  case PINCHING:
    SetState(NOTHING);
    // Scale gesture listener should have handled this.
    NS_WARNING("Gesture listener should have handled pinching in OnTouchEnd.");
    return nsEventStatus_eIgnore;

  case WAITING_LISTENERS:
    NS_WARNING("Received impossible touch in OnTouchEnd");
    break;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchCancel(const MultiTouchInput& aEvent) {
  SetState(NOTHING);
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnScaleBegin(const PinchGestureInput& aEvent) {
  if (!mAllowZoom) {
    return nsEventStatus_eConsumeNoDefault;
  }

  SetState(PINCHING);
  mLastZoomFocus = aEvent.mFocusPoint;

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnScale(const PinchGestureInput& aEvent) {
  if (mState != PINCHING) {
    return nsEventStatus_eConsumeNoDefault;
  }

  float prevSpan = aEvent.mPreviousSpan;
  if (fabsf(prevSpan) <= EPSILON || fabsf(aEvent.mCurrentSpan) <= EPSILON) {
    // We're still handling it; we've just decided to throw this event away.
    return nsEventStatus_eConsumeNoDefault;
  }

  float spanRatio = aEvent.mCurrentSpan / aEvent.mPreviousSpan;

  {
    MonitorAutoLock monitor(mMonitor);

    gfxFloat resolution = CalculateResolution(mFrameMetrics).width;
    gfxFloat userZoom = mFrameMetrics.mZoom.width;
    nsIntPoint focusPoint = aEvent.mFocusPoint;
    gfxFloat xFocusChange = (mLastZoomFocus.x - focusPoint.x) / resolution;
    gfxFloat yFocusChange = (mLastZoomFocus.y - focusPoint.y) / resolution;
    // If displacing by the change in focus point will take us off page bounds,
    // then reduce the displacement such that it doesn't.
    if (mX.DisplacementWillOverscroll(xFocusChange) != Axis::OVERSCROLL_NONE) {
      xFocusChange -= mX.DisplacementWillOverscrollAmount(xFocusChange);
    }
    if (mY.DisplacementWillOverscroll(yFocusChange) != Axis::OVERSCROLL_NONE) {
      yFocusChange -= mY.DisplacementWillOverscrollAmount(yFocusChange);
    }
    ScrollBy(gfx::Point(xFocusChange, yFocusChange));

    // When we zoom in with focus, we can zoom too much towards the boundaries
    // that we actually go over them. These are the needed displacements along
    // either axis such that we don't overscroll the boundaries when zooming.
    gfxFloat neededDisplacementX = 0, neededDisplacementY = 0;

    // Only do the scaling if we won't go over 8x zoom in or out.
    bool doScale = (spanRatio > 1.0 && userZoom < mMaxZoom) ||
                   (spanRatio < 1.0 && userZoom > mMinZoom);

    // If this zoom will take it over 8x zoom in either direction, but it's not
    // already there, then normalize it.
    if (userZoom * spanRatio > mMaxZoom) {
      spanRatio = userZoom / mMaxZoom;
    } else if (userZoom * spanRatio < mMinZoom) {
      spanRatio = userZoom / mMinZoom;
    }

    if (doScale) {
      switch (mX.ScaleWillOverscroll(spanRatio, focusPoint.x))
      {
        case Axis::OVERSCROLL_NONE:
          break;
        case Axis::OVERSCROLL_MINUS:
        case Axis::OVERSCROLL_PLUS:
          neededDisplacementX = -mX.ScaleWillOverscrollAmount(spanRatio, focusPoint.x);
          break;
        case Axis::OVERSCROLL_BOTH:
          // If scaling this way will make us overscroll in both directions, then
          // we must already be at the maximum zoomed out amount. In this case, we
          // don't want to allow this scaling to go through and instead clamp it
          // here.
          doScale = false;
          break;
      }
    }

    if (doScale) {
      switch (mY.ScaleWillOverscroll(spanRatio, focusPoint.y))
      {
        case Axis::OVERSCROLL_NONE:
          break;
        case Axis::OVERSCROLL_MINUS:
        case Axis::OVERSCROLL_PLUS:
          neededDisplacementY = -mY.ScaleWillOverscrollAmount(spanRatio, focusPoint.y);
          break;
        case Axis::OVERSCROLL_BOTH:
          doScale = false;
          break;
      }
    }

    if (doScale) {
      ScaleWithFocus(userZoom * spanRatio, focusPoint);

      if (neededDisplacementX != 0 || neededDisplacementY != 0) {
        ScrollBy(gfx::Point(neededDisplacementX, neededDisplacementY));
      }

      ScheduleComposite();
      // We don't want to redraw on every scale, so don't use
      // RequestContentRepaint()
    }

    mLastZoomFocus = focusPoint;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnScaleEnd(const PinchGestureInput& aEvent) {
  SetState(PANNING);
  mX.StartTouch(aEvent.mFocusPoint.x);
  mY.StartTouch(aEvent.mFocusPoint.y);
  {
    MonitorAutoLock monitor(mMonitor);
    ScheduleComposite();
    RequestContentRepaint();
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnLongPress(const TapGestureInput& aEvent) {
  if (mGeckoContentController) {
    MonitorAutoLock monitor(mMonitor);

    gfxFloat resolution = CalculateResolution(mFrameMetrics).width;
    gfx::Point point = WidgetSpaceToCompensatedViewportSpace(
      gfx::Point(aEvent.mPoint.x, aEvent.mPoint.y),
      resolution);
    mGeckoContentController->HandleLongTap(nsIntPoint(NS_lround(point.x),
                                                      NS_lround(point.y)));
    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnSingleTapUp(const TapGestureInput& aEvent) {
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnSingleTapConfirmed(const TapGestureInput& aEvent) {
  if (mGeckoContentController) {
    MonitorAutoLock monitor(mMonitor);

    gfxFloat resolution = CalculateResolution(mFrameMetrics).width;
    gfx::Point point = WidgetSpaceToCompensatedViewportSpace(
      gfx::Point(aEvent.mPoint.x, aEvent.mPoint.y),
      resolution);
    mGeckoContentController->HandleSingleTap(nsIntPoint(NS_lround(point.x),
                                                        NS_lround(point.y)));
    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnDoubleTap(const TapGestureInput& aEvent) {
  if (mGeckoContentController) {
    MonitorAutoLock monitor(mMonitor);

    if (mAllowZoom) {
      gfxFloat resolution = CalculateResolution(mFrameMetrics).width;
      gfx::Point point = WidgetSpaceToCompensatedViewportSpace(
        gfx::Point(aEvent.mPoint.x, aEvent.mPoint.y),
        resolution);
      mGeckoContentController->HandleDoubleTap(nsIntPoint(NS_lround(point.x),
                                                          NS_lround(point.y)));
    }

    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnCancelTap(const TapGestureInput& aEvent) {
  // XXX: Implement this.
  return nsEventStatus_eIgnore;
}

float AsyncPanZoomController::PanDistance() {
  MonitorAutoLock monitor(mMonitor);
  return NS_hypot(mX.PanDistance(), mY.PanDistance());
}

const gfx::Point AsyncPanZoomController::GetVelocityVector() {
  return gfx::Point(mX.GetVelocity(), mY.GetVelocity());
}

const gfx::Point AsyncPanZoomController::GetAccelerationVector() {
  return gfx::Point(mX.GetAccelerationFactor(), mY.GetAccelerationFactor());
}

void AsyncPanZoomController::StartPanning(const MultiTouchInput& aEvent) {
  float dx = mX.PanDistance(),
        dy = mY.PanDistance();

  double angle = atan2(dy, dx); // range [-pi, pi]
  angle = fabs(angle); // range [0, pi]

  SetState(PANNING);
}

void AsyncPanZoomController::UpdateWithTouchAtDevicePoint(const MultiTouchInput& aEvent) {
  SingleTouchData& touch = GetFirstSingleTouch(aEvent);
  nsIntPoint point = touch.mScreenPoint;
  int32_t xPos = point.x, yPos = point.y;
  TimeDuration timeDelta = TimeDuration().FromMilliseconds(aEvent.mTime - mLastEventTime);

  // Probably a duplicate event, just throw it away.
  if (timeDelta.ToMilliseconds() <= EPSILON) {
    return;
  }

  mX.UpdateWithTouchAtDevicePoint(xPos, timeDelta);
  mY.UpdateWithTouchAtDevicePoint(yPos, timeDelta);
}

void AsyncPanZoomController::TrackTouch(const MultiTouchInput& aEvent) {
  TimeDuration timeDelta = TimeDuration().FromMilliseconds(aEvent.mTime - mLastEventTime);

  // Probably a duplicate event, just throw it away.
  if (timeDelta.ToMilliseconds() <= EPSILON) {
    return;
  }

  UpdateWithTouchAtDevicePoint(aEvent);

  {
    MonitorAutoLock monitor(mMonitor);

    // We want to inversely scale it because when you're zoomed further in, a
    // larger swipe should move you a shorter distance.
    gfxFloat inverseResolution = 1 / CalculateResolution(mFrameMetrics).width;

    int32_t xDisplacement = mX.GetDisplacementForDuration(inverseResolution,
                                                          timeDelta);
    int32_t yDisplacement = mY.GetDisplacementForDuration(inverseResolution,
                                                          timeDelta);
    if (!xDisplacement && !yDisplacement) {
      return;
    }

    ScrollBy(gfx::Point(xDisplacement, yDisplacement));
    ScheduleComposite();

    RequestContentRepaint();
  }
}

SingleTouchData& AsyncPanZoomController::GetFirstSingleTouch(const MultiTouchInput& aEvent) {
  return (SingleTouchData&)aEvent.mTouches[0];
}

bool AsyncPanZoomController::DoFling(const TimeDuration& aDelta) {
  if (mState != FLING) {
    return false;
  }

  bool shouldContinueFlingX = mX.FlingApplyFrictionOrCancel(aDelta),
       shouldContinueFlingY = mY.FlingApplyFrictionOrCancel(aDelta);
  // If we shouldn't continue the fling, let's just stop and repaint.
  if (!shouldContinueFlingX && !shouldContinueFlingY) {
    // Bring the resolution back in sync with the zoom, in case we scaled down
    // the zoom while accelerating.
    SetZoomAndResolution(mFrameMetrics.mZoom.width);
    RequestContentRepaint();
    mState = NOTHING;
    return false;
  }

  // We want to inversely scale it because when you're zoomed further in, a
  // larger swipe should move you a shorter distance.
  gfxFloat inverseResolution = 1 / CalculateResolution(mFrameMetrics).width;

  ScrollBy(gfx::Point(
    mX.GetDisplacementForDuration(inverseResolution, aDelta),
    mY.GetDisplacementForDuration(inverseResolution, aDelta)
  ));
  RequestContentRepaint();

  return true;
}

void AsyncPanZoomController::CancelAnimation() {
  mState = NOTHING;
}

void AsyncPanZoomController::SetCompositorParent(CompositorParent* aCompositorParent) {
  mCompositorParent = aCompositorParent;
}

void AsyncPanZoomController::ScrollBy(const gfx::Point& aOffset) {
  gfx::Point newOffset(mFrameMetrics.mScrollOffset.x + aOffset.x,
                       mFrameMetrics.mScrollOffset.y + aOffset.y);
  FrameMetrics metrics(mFrameMetrics);
  metrics.mScrollOffset = newOffset;
  mFrameMetrics = metrics;
}

void AsyncPanZoomController::SetPageRect(const gfx::Rect& aCSSPageRect) {
  FrameMetrics metrics = mFrameMetrics;
  gfx::Rect pageSize = aCSSPageRect;
  gfxFloat resolution = CalculateResolution(mFrameMetrics).width;

  // The page rect is the css page rect scaled by the current zoom.
  pageSize.ScaleInverseRoundOut(resolution);

  // Round the page rect so we don't get any truncation, then get the nsIntRect
  // from this.
  metrics.mContentRect = nsIntRect(pageSize.x, pageSize.y,
                                   pageSize.width, pageSize.height);
  metrics.mScrollableRect = aCSSPageRect;

  mFrameMetrics = metrics;
}

void AsyncPanZoomController::ScaleWithFocus(float aZoom,
                                            const nsIntPoint& aFocus) {
  float zoomFactor = aZoom / mFrameMetrics.mZoom.width;
  gfxFloat resolution = CalculateResolution(mFrameMetrics).width;

  SetZoomAndResolution(aZoom);

  // Force a recalculation of the page rect based on the new zoom and the
  // current CSS page rect (which is unchanged since it's not affected by zoom).
  SetPageRect(mFrameMetrics.mScrollableRect);

  // If the new scale is very small, we risk multiplying in huge rounding
  // errors, so don't bother adjusting the scroll offset.
  if (resolution >= 0.01f) {
    mFrameMetrics.mScrollOffset.x +=
      gfxFloat(aFocus.x) * (zoomFactor - 1.0) / resolution;
    mFrameMetrics.mScrollOffset.y +=
      gfxFloat(aFocus.y) * (zoomFactor - 1.0) / resolution;
  }
}

bool AsyncPanZoomController::EnlargeDisplayPortAlongAxis(float aSkateSizeMultiplier,
                                                         double aEstimatedPaintDuration,
                                                         float aCompositionBounds,
                                                         float aVelocity,
                                                         float aAcceleration,
                                                         float* aDisplayPortOffset,
                                                         float* aDisplayPortLength)
{
  if (fabsf(aVelocity) > MIN_SKATE_SPEED) {
    // Enlarge the area we paint.
    *aDisplayPortLength = aCompositionBounds * aSkateSizeMultiplier;
    // Position the area we paint such that all of the excess that extends past
    // the screen is on the side towards the velocity.
    *aDisplayPortOffset = aVelocity > 0 ? 0 : aCompositionBounds - *aDisplayPortLength;

    // Only compensate for acceleration when we actually have any. Otherwise
    // we'll overcompensate when a user is just panning around without flinging.
    if (aAcceleration > 1.01f) {
      // Compensate for acceleration and how long we expect a paint to take. We
      // try to predict where the viewport will be when painting has finished.
      *aDisplayPortOffset +=
        fabsf(aAcceleration) * aVelocity * aCompositionBounds * aEstimatedPaintDuration;
      // If our velocity is in the negative direction of the axis, we have to
      // compensate for the fact that our scroll offset is the top-left position
      // of the viewport. In this case, let's make it relative to the
      // bottom-right. That way, we'll always be growing the displayport upwards
      // and to the left when skating negatively.
      *aDisplayPortOffset -= aVelocity < 0 ? aCompositionBounds : 0;
    }
    return true;
  }
  return false;
}

const gfx::Rect AsyncPanZoomController::CalculatePendingDisplayPort(
  const FrameMetrics& aFrameMetrics,
  const gfx::Point& aVelocity,
  const gfx::Point& aAcceleration,
  double aEstimatedPaintDuration)
{
  // The multiplier we apply to a dimension's length if it is skating. That is,
  // if it's going above MIN_SKATE_SPEED. We prefer to increase the size of the
  // Y axis because it is more natural in the case that a user is reading a page
  // that scrolls up/down. Note that one, both or neither of these may be used
  // at any instant.
  const float X_SKATE_SIZE_MULTIPLIER = 3.0f;
  const float Y_SKATE_SIZE_MULTIPLIER = 3.5f;

  // The multiplier we apply to a dimension's length if it is stationary. We
  // prefer to increase the size of the Y axis because it is more natural in the
  // case that a user is reading a page that scrolls up/down. Note that one,
  // both or neither of these may be used at any instant.
  const float X_STATIONARY_SIZE_MULTIPLIER = 1.5f;
  const float Y_STATIONARY_SIZE_MULTIPLIER = 2.5f;

  // If we don't get an estimated paint duration, we probably don't have any
  // data. In this case, we're dealing with either a stationary frame or a first
  // paint. In either of these cases, we can just assume it'll take 1 second to
  // paint. Getting this correct is not important anyways since it's only really
  // useful when accelerating, which can't be happening at this point.
  double estimatedPaintDuration =
    aEstimatedPaintDuration > EPSILON ? aEstimatedPaintDuration : 1.0;

  gfxFloat resolution = CalculateResolution(aFrameMetrics).width;
  nsIntRect compositionBounds = aFrameMetrics.mCompositionBounds;
  compositionBounds.ScaleInverseRoundIn(resolution);
  const gfx::Rect& scrollableRect = aFrameMetrics.mScrollableRect;

  gfx::Point scrollOffset = aFrameMetrics.mScrollOffset;

  gfx::Rect displayPort(0, 0,
                        compositionBounds.width * X_STATIONARY_SIZE_MULTIPLIER,
                        compositionBounds.height * Y_STATIONARY_SIZE_MULTIPLIER);

  // If there's motion along an axis of movement, and it's above a threshold,
  // then we want to paint a larger area in the direction of that motion so that
  // it's less likely to checkerboard.
  bool enlargedX = EnlargeDisplayPortAlongAxis(
    X_SKATE_SIZE_MULTIPLIER, estimatedPaintDuration,
    compositionBounds.width, aVelocity.x, aAcceleration.x,
    &displayPort.x, &displayPort.width);
  bool enlargedY = EnlargeDisplayPortAlongAxis(
    Y_SKATE_SIZE_MULTIPLIER, estimatedPaintDuration,
    compositionBounds.height, aVelocity.y, aAcceleration.y,
    &displayPort.y, &displayPort.height);

  if (!enlargedX && !enlargedY) {
    // Position the x and y such that the screen falls in the middle of the displayport.
    displayPort.x = -(displayPort.width - compositionBounds.width) / 2;
    displayPort.y = -(displayPort.height - compositionBounds.height) / 2;
  } else if (!enlargedX) {
    displayPort.width = compositionBounds.width;
  } else if (!enlargedY) {
    displayPort.height = compositionBounds.height;
  }

  // If we go over the bounds when trying to predict where we will be when this
  // paint finishes, move it back into the range of the CSS content rect.
  // FIXME/bug 780395: Generalize this. This code is pretty hacky as it will
  // probably not work at all for RTL content. This is not intended to be
  // incredibly accurate; it'll just prevent the entire displayport from being
  // outside the content rect (which causes bad things to happen).
  if (scrollOffset.x + compositionBounds.width > scrollableRect.width) {
    scrollOffset.x -= compositionBounds.width + scrollOffset.x - scrollableRect.width;
  } else if (scrollOffset.x < scrollableRect.x) {
    scrollOffset.x = scrollableRect.x;
  }
  if (scrollOffset.y + compositionBounds.height > scrollableRect.height) {
    scrollOffset.y -= compositionBounds.height + scrollOffset.y - scrollableRect.height;
  } else if (scrollOffset.y < scrollableRect.y) {
    scrollOffset.y = scrollableRect.y;
  }

  gfx::Rect shiftedDisplayPort = displayPort;
  shiftedDisplayPort.MoveBy(scrollOffset.x, scrollOffset.y);
  displayPort = shiftedDisplayPort.Intersect(aFrameMetrics.mScrollableRect);
  displayPort.MoveBy(-scrollOffset.x, -scrollOffset.y);

  return displayPort;
}

/*static*/ gfxSize
AsyncPanZoomController::CalculateIntrinsicScale(const FrameMetrics& aMetrics)
{
  gfxFloat intrinsicScale = (gfxFloat(aMetrics.mCompositionBounds.width) / 
                             gfxFloat(aMetrics.mViewport.width));
  return gfxSize(intrinsicScale, intrinsicScale);
}

/*static*/ gfxSize
AsyncPanZoomController::CalculateResolution(const FrameMetrics& aMetrics)
{
  gfxSize intrinsicScale = CalculateIntrinsicScale(aMetrics);
  gfxSize userZoom = aMetrics.mZoom;
  return gfxSize(intrinsicScale.width * userZoom.width,
                 intrinsicScale.height * userZoom.height);
}

/*static*/ gfx::Rect
AsyncPanZoomController::CalculateCompositedRectInCssPixels(const FrameMetrics& aMetrics)
{
  gfxSize resolution = CalculateResolution(aMetrics);
  gfx::Rect rect(aMetrics.mCompositionBounds.x,
                 aMetrics.mCompositionBounds.y,
                 aMetrics.mCompositionBounds.width,
                 aMetrics.mCompositionBounds.height);
  rect.ScaleInverseRoundIn(resolution.width, resolution.height);
  return rect;
}

void AsyncPanZoomController::SetDPI(int aDPI) {
  mDPI = aDPI;
}

int AsyncPanZoomController::GetDPI() {
  return mDPI;
}

void AsyncPanZoomController::ScheduleComposite() {
  if (mCompositorParent) {
    mCompositorParent->ScheduleRenderOnCompositorThread();
  }
}

void AsyncPanZoomController::RequestContentRepaint() {
  mPreviousPaintStartTime = TimeStamp::Now();

  double estimatedPaintSum = 0.0;
  for (uint32_t i = 0; i < mPreviousPaintDurations.Length(); i++) {
    estimatedPaintSum += mPreviousPaintDurations[i].ToSeconds();
  }

  double estimatedPaintDuration = 0.0;
  if (estimatedPaintSum > EPSILON) {
    estimatedPaintDuration = estimatedPaintSum / mPreviousPaintDurations.Length();
  }

  mFrameMetrics.mDisplayPort =
    CalculatePendingDisplayPort(mFrameMetrics,
                                GetVelocityVector(),
                                GetAccelerationVector(),
                                estimatedPaintDuration);

  gfx::Point oldScrollOffset = mLastPaintRequestMetrics.mScrollOffset,
             newScrollOffset = mFrameMetrics.mScrollOffset;

  // If we're trying to paint what we already think is painted, discard this
  // request since it's a pointless paint.
  gfx::Rect oldDisplayPort = mLastPaintRequestMetrics.mDisplayPort;
  gfx::Rect newDisplayPort = mFrameMetrics.mDisplayPort;

  oldDisplayPort.MoveBy(oldScrollOffset.x, oldScrollOffset.y);
  newDisplayPort.MoveBy(newScrollOffset.x, newScrollOffset.y);

  if (fabsf(oldDisplayPort.x - newDisplayPort.x) < EPSILON &&
      fabsf(oldDisplayPort.y - newDisplayPort.y) < EPSILON &&
      fabsf(oldDisplayPort.width - newDisplayPort.width) < EPSILON &&
      fabsf(oldDisplayPort.height - newDisplayPort.height) < EPSILON &&
      mFrameMetrics.mResolution.width == mLastPaintRequestMetrics.mResolution.width) {
    return;
  }

  // Cache the zoom since we're temporarily changing it for
  // acceleration-scaled painting.
  gfxFloat actualZoom = mFrameMetrics.mZoom.width;
  // Calculate the factor of acceleration based on the faster of the two axes.
  float accelerationFactor =
    clamped(NS_MAX(mX.GetAccelerationFactor(), mY.GetAccelerationFactor()),
            float(MIN_ZOOM) / 2.0f, float(MAX_ZOOM));
  // Scale down the resolution a bit based on acceleration.
  mFrameMetrics.mZoom.width = mFrameMetrics.mZoom.height =
                              actualZoom / accelerationFactor;

  // This message is compressed, so fire whether or not we already have a paint
  // queued up. We need to know whether or not a paint was requested anyways,
  // for the purposes of content calling window.scrollTo().
  mPaintThrottler.PostTask(
    FROM_HERE,
    NewRunnableMethod(mGeckoContentController.get(),
                      &GeckoContentController::RequestContentRepaint,
                      mFrameMetrics));
  mLastPaintRequestMetrics = mFrameMetrics;
  mWaitingForContentToPaint = true;

  // Set the zoom back to what it was for the purpose of logic control.
  mFrameMetrics.mZoom = gfxSize(actualZoom, actualZoom);
}

bool AsyncPanZoomController::SampleContentTransformForFrame(const TimeStamp& aSampleTime,
                                                            ContainerLayer* aLayer,
                                                            gfx3DMatrix* aNewTransform) {
  // The eventual return value of this function. The compositor needs to know
  // whether or not to advance by a frame as soon as it can. For example, if a
  // fling is happening, it has to keep compositing so that the animation is
  // smooth. If an animation frame is requested, it is the compositor's
  // responsibility to schedule a composite.
  bool requestAnimationFrame = false;

  const gfx3DMatrix& currentTransform = aLayer->GetTransform();

  // Scales on the root layer, on what's currently painted.
  float rootScaleX = currentTransform.GetXScale(),
        rootScaleY = currentTransform.GetYScale();

  gfx::Point metricsScrollOffset(0, 0);
  gfx::Point scrollOffset;
  float localScaleX, localScaleY;
  const FrameMetrics& frame = aLayer->GetFrameMetrics();
  {
    MonitorAutoLock mon(mMonitor);

    switch (mState) {
    case FLING:
      // If a fling is currently happening, apply it now. We can pull
      // the updated metrics afterwards.
      requestAnimationFrame |= DoFling(aSampleTime - mLastSampleTime);
      break;
    case ANIMATING_ZOOM: {
      double animPosition = (aSampleTime - mAnimationStartTime) / ZOOM_TO_DURATION;
      if (animPosition > 1.0) {
        animPosition = 1.0;
      }
      // Sample the zoom at the current time point.  The sampled zoom
      // will affect the final computed resolution.
      double sampledPosition = gComputedTimingFunction->GetValue(animPosition);

      gfxFloat startZoom = mStartZoomToMetrics.mZoom.width;
      gfxFloat endZoom = mEndZoomToMetrics.mZoom.width;
      gfxFloat sampledZoom = (endZoom * sampledPosition +
                              startZoom * (1 - sampledPosition));
      mFrameMetrics.mZoom = gfxSize(sampledZoom, sampledZoom);

      mFrameMetrics.mScrollOffset = gfx::Point(
        mEndZoomToMetrics.mScrollOffset.x * sampledPosition +
          mStartZoomToMetrics.mScrollOffset.x * (1 - sampledPosition),
        mEndZoomToMetrics.mScrollOffset.y * sampledPosition +
          mStartZoomToMetrics.mScrollOffset.y * (1 - sampledPosition)
      );

      requestAnimationFrame = true;

      if (aSampleTime - mAnimationStartTime >= ZOOM_TO_DURATION) {
        // Bring the resolution in sync with the zoom.
        SetZoomAndResolution(mFrameMetrics.mZoom.width);
        mState = NOTHING;
        RequestContentRepaint();
      }

      break;
    }
    default:
      break;
    }

    // Current local transform; this is not what's painted but rather
    // what PZC has transformed due to touches like panning or
    // pinching. Eventually, the root layer transform will become this
    // during runtime, but we must wait for Gecko to repaint.
    gfxSize localScale = CalculateResolution(mFrameMetrics);
    localScaleX = localScale.width;
    localScaleY = localScale.height;

    if (frame.IsScrollable()) {
      metricsScrollOffset = frame.GetScrollOffsetInLayerPixels();
    }

    scrollOffset = mFrameMetrics.mScrollOffset;
  }

  nsIntPoint scrollCompensation(
    NS_lround((scrollOffset.x / rootScaleX - metricsScrollOffset.x) * localScaleX),
    NS_lround((scrollOffset.y / rootScaleY - metricsScrollOffset.y) * localScaleY));

  ViewTransform treeTransform(-scrollCompensation, localScaleX, localScaleY);
  *aNewTransform = gfx3DMatrix(treeTransform) * currentTransform;

  // The transform already takes the resolution scale into account.  Since we
  // will apply the resolution scale again when computing the effective
  // transform, we must apply the inverse resolution scale here.
  aNewTransform->Scale(1.0f/aLayer->GetPreXScale(),
                       1.0f/aLayer->GetPreYScale(),
                       1);
  aNewTransform->ScalePost(1.0f/aLayer->GetPostXScale(),
                           1.0f/aLayer->GetPostYScale(),
                           1);

  mLastSampleTime = aSampleTime;

  return requestAnimationFrame;
}

void AsyncPanZoomController::NotifyLayersUpdated(const FrameMetrics& aViewportFrame, bool aIsFirstPaint) {
  MonitorAutoLock monitor(mMonitor);

  mPaintThrottler.TaskComplete();

  mLastContentPaintMetrics = aViewportFrame;

  mFrameMetrics.mMayHaveTouchListeners = aViewportFrame.mMayHaveTouchListeners;
  if (mWaitingForContentToPaint) {
    // Remove the oldest sample we have if adding a new sample takes us over our
    // desired number of samples.
    if (mPreviousPaintDurations.Length() >= NUM_PAINT_DURATION_SAMPLES) {
      mPreviousPaintDurations.RemoveElementAt(0);
    }

    mPreviousPaintDurations.AppendElement(
      TimeStamp::Now() - mPreviousPaintStartTime);
  } else {
    // No paint was requested, but we got one anyways. One possible cause of this
    // is that content could have fired a scrollTo(). In this case, we should take
    // the new scroll offset. Document/viewport changes are handled elsewhere.
    // Also note that, since NotifyLayersUpdated() is called whenever there's a
    // layers update, we didn't necessarily get a new scroll offset, but we're
    // updating our local copy of it anyways just in case.
    switch (mState) {
    case NOTHING:
    case FLING:
    case TOUCHING:
    case WAITING_LISTENERS:
      mFrameMetrics.mScrollOffset = aViewportFrame.mScrollOffset;
      break;
    // Don't clobber if we're in other states.
    default:
      break;
    }
  }

  mWaitingForContentToPaint = false;
  bool needContentRepaint = false;
  if (aViewportFrame.mCompositionBounds.width == mFrameMetrics.mCompositionBounds.width &&
      aViewportFrame.mCompositionBounds.height == mFrameMetrics.mCompositionBounds.height) {
    // Remote content has sync'd up to the composition geometry
    // change, so we can accept the viewport it's calculated.
    gfxSize previousResolution = CalculateResolution(mFrameMetrics);
    mFrameMetrics.mViewport = aViewportFrame.mViewport;
    gfxSize newResolution = CalculateResolution(mFrameMetrics);
    needContentRepaint |= (previousResolution != newResolution);
  }

  if (aIsFirstPaint || mFrameMetrics.IsDefault()) {
    mPreviousPaintDurations.Clear();

    mX.CancelTouch();
    mY.CancelTouch();

    mFrameMetrics = aViewportFrame;

    SetPageRect(mFrameMetrics.mScrollableRect);

    mState = NOTHING;
  } else if (!mFrameMetrics.mScrollableRect.IsEqualEdges(aViewportFrame.mScrollableRect)) {
    mFrameMetrics.mScrollableRect = aViewportFrame.mScrollableRect;
    SetPageRect(mFrameMetrics.mScrollableRect);
  }

  if (needContentRepaint) {
    RequestContentRepaint();
  }
}

const FrameMetrics& AsyncPanZoomController::GetFrameMetrics() {
  mMonitor.AssertCurrentThreadOwns();
  return mFrameMetrics;
}

void AsyncPanZoomController::UpdateCompositionBounds(const nsIntRect& aCompositionBounds) {
  MonitorAutoLock mon(mMonitor);

  nsIntRect oldCompositionBounds = mFrameMetrics.mCompositionBounds;
  mFrameMetrics.mCompositionBounds = aCompositionBounds;

  // If the window had 0 dimensions before, or does now, we don't want to
  // repaint or update the zoom since we'll run into rendering issues and/or
  // divide-by-zero. This manifests itself as the screen flashing. If the page
  // has gone out of view, the buffer will be cleared elsewhere anyways.
  if (aCompositionBounds.width && aCompositionBounds.height &&
      oldCompositionBounds.width && oldCompositionBounds.height) {
    SetZoomAndResolution(mFrameMetrics.mZoom.width);

    // Repaint on a rotation so that our new resolution gets properly updated.
    RequestContentRepaint();
  }
}

void AsyncPanZoomController::CancelDefaultPanZoom() {
  mDisableNextTouchBatch = true;
  if (mGestureEventListener) {
    mGestureEventListener->CancelGesture();
  }
}

void AsyncPanZoomController::DetectScrollableSubframe() {
  mDelayPanning = true;
}

void AsyncPanZoomController::ZoomToRect(const gfxRect& aRect) {
  gfx::Rect zoomToRect(gfx::Rect(aRect.x, aRect.y, aRect.width, aRect.height));

  SetState(ANIMATING_ZOOM);

  {
    MonitorAutoLock mon(mMonitor);

    nsIntRect compositionBounds = mFrameMetrics.mCompositionBounds;
    gfx::Rect cssPageRect = mFrameMetrics.mScrollableRect;
    gfx::Point scrollOffset = mFrameMetrics.mScrollOffset;
    gfxSize resolution = CalculateResolution(mFrameMetrics);

    // If the rect is empty, treat it as a request to zoom out to the full page
    // size.
    if (zoomToRect.IsEmpty()) {
      // composition bounds in CSS coordinates
      nsIntRect cssCompositionBounds = compositionBounds;
      cssCompositionBounds.ScaleInverseRoundIn(resolution.width,
                                               resolution.height);
      cssCompositionBounds.MoveBy(scrollOffset.x, scrollOffset.y);

      float y = mFrameMetrics.mScrollOffset.y;
      float newHeight =
        cssCompositionBounds.height * cssPageRect.width / cssCompositionBounds.width;
      float dh = cssCompositionBounds.height - newHeight;

      zoomToRect = gfx::Rect(0.0f,
                             y + dh/2,
                             cssPageRect.width,
                             y + dh/2 + newHeight);
    }

    gfxFloat targetResolution =
      NS_MIN(compositionBounds.width / zoomToRect.width,
             compositionBounds.height / zoomToRect.height);

    // Recalculate the zoom to rect using the new dimensions.
    zoomToRect.width = compositionBounds.width / targetResolution;
    zoomToRect.height = compositionBounds.height / targetResolution;

    // Clamp the zoom to rect to the CSS rect to make sure it fits.
    zoomToRect = zoomToRect.Intersect(cssPageRect);

    // Do one final recalculation to get the resolution.
    targetResolution = NS_MAX(compositionBounds.width / zoomToRect.width,
                              compositionBounds.height / zoomToRect.height);
    float targetZoom = float(targetResolution / resolution.width) * mFrameMetrics.mZoom.width;

    // If current zoom is equal to mMaxZoom,
    // user still double-tapping it, just zoom-out to the full page size
    if (mFrameMetrics.mZoom.width == mMaxZoom && targetZoom >= mMaxZoom) {
      nsIntRect cssCompositionBounds = compositionBounds;
      cssCompositionBounds.ScaleInverseRoundIn(resolution.width,
                                               resolution.height);
      cssCompositionBounds.MoveBy(scrollOffset.x, scrollOffset.y);

      float y = mFrameMetrics.mScrollOffset.y;
      float newHeight =
        cssCompositionBounds.height * cssPageRect.width / cssCompositionBounds.width;
      float dh = cssCompositionBounds.height - newHeight;

      zoomToRect = gfx::Rect(0.0f,
                             y + dh/2,
                             cssPageRect.width,
                             y + dh/2 + newHeight);

      zoomToRect = zoomToRect.Intersect(cssPageRect);
      // assign 1 to targetZoom is a shortcut
      targetZoom = 1;
    }

    gfxFloat targetFinalZoom = clamped(targetZoom, mMinZoom, mMaxZoom);
    mEndZoomToMetrics.mZoom = gfxSize(targetFinalZoom, targetFinalZoom);

    mStartZoomToMetrics = mFrameMetrics;
    mEndZoomToMetrics.mScrollOffset =
      gfx::Point(zoomToRect.x, zoomToRect.y);

    mAnimationStartTime = TimeStamp::Now();

    ScheduleComposite();
  }
}

void AsyncPanZoomController::ContentReceivedTouch(bool aPreventDefault) {
  if (!mFrameMetrics.mMayHaveTouchListeners && !mDelayPanning) {
    mTouchQueue.Clear();
    return;
  }

  if (mTouchListenerTimeoutTask) {
    mTouchListenerTimeoutTask->Cancel();
    mTouchListenerTimeoutTask = nullptr;
  }

  if (mState == WAITING_LISTENERS) {
    if (!aPreventDefault) {
      // Delayed scrolling gesture is pending at TOUCHING state.
      if (mDelayPanning) {
        SetState(TOUCHING);
      } else {
        SetState(NOTHING);
      }
    }

    mHandlingTouchQueue = true;

    while (!mTouchQueue.IsEmpty()) {
      // we need to reset mDelayPanning before handling scrolling gesture.
      if (mTouchQueue[0].mType == MultiTouchInput::MULTITOUCH_MOVE) {
        mDelayPanning = false;
      }
      if (!aPreventDefault) {
        HandleInputEvent(mTouchQueue[0]);
      }

      if (mTouchQueue[0].mType == MultiTouchInput::MULTITOUCH_END ||
          mTouchQueue[0].mType == MultiTouchInput::MULTITOUCH_CANCEL) {
        mTouchQueue.RemoveElementAt(0);
        break;
      }

      mTouchQueue.RemoveElementAt(0);
    }

    mHandlingTouchQueue = false;
  }
}

void AsyncPanZoomController::SetState(PanZoomState aState) {
  MonitorAutoLock monitor(mMonitor);
  mState = aState;
}

void AsyncPanZoomController::TimeoutTouchListeners() {
  ContentReceivedTouch(false);
}

void AsyncPanZoomController::SetZoomAndResolution(float aZoom) {
  mMonitor.AssertCurrentThreadOwns();
  mFrameMetrics.mZoom = gfxSize(aZoom, aZoom);
  mFrameMetrics.mResolution = CalculateResolution(mFrameMetrics);
}

void AsyncPanZoomController::UpdateZoomConstraints(bool aAllowZoom,
                                                   float aMinZoom,
                                                   float aMaxZoom) {
  mAllowZoom = aAllowZoom;
  mMinZoom = aMinZoom;
  mMaxZoom = aMaxZoom;
}

}
}
