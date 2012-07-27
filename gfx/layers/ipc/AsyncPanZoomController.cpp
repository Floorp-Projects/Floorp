/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorParent.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/Util.h"
#include "mozilla/XPCOM.h"
#include "mozilla/Monitor.h"
#include "AsyncPanZoomController.h"
#include "GestureEventListener.h"
#include "nsIThreadManager.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const float EPSILON = 0.0001;

/**
 * Maximum amount of time while panning before sending a viewport change. This
 * will asynchronously repaint the page. It is also forced when panning stops.
 */
static const PRInt32 PAN_REPAINT_INTERVAL = 250;

/**
 * Maximum amount of time flinging before sending a viewport change. This will
 * asynchronously repaint the page.
 */
static const PRInt32 FLING_REPAINT_INTERVAL = 75;

/**
 * Minimum amount of speed along an axis before we begin painting far ahead by
 * adjusting the displayport.
 */
static const float MIN_SKATE_SPEED = 0.5f;

/**
 * Angle from axis within which we stay axis-locked.
 */
static const float AXIS_LOCK_ANGLE = M_PI / 6.0;

AsyncPanZoomController::AsyncPanZoomController(GeckoContentController* aGeckoContentController,
                                               GestureBehavior aGestures)
  :  mGeckoContentController(aGeckoContentController),
     mX(this),
     mY(this),
     mMonitor("AsyncPanZoomController"),
     mLastSampleTime(TimeStamp::Now()),
     mState(NOTHING),
     mDPI(72),
     mContentPainterStatus(CONTENT_IDLE)
{
  if (aGestures == USE_GESTURE_DETECTOR) {
    mGestureEventListener = new GestureEventListener(this);
  }

  SetDPI(mDPI);
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
AsyncPanZoomController::HandleInputEvent(const nsInputEvent& aEvent,
                                         nsInputEvent* aOutEvent)
{
  float currentZoom;
  gfx::Point currentScrollOffset, lastScrollOffset;
  {
    MonitorAutoLock monitor(mMonitor);
    currentZoom = mFrameMetrics.mResolution.width;
    currentScrollOffset = gfx::Point(mFrameMetrics.mViewportScrollOffset.x,
                                     mFrameMetrics.mViewportScrollOffset.y);
    lastScrollOffset = gfx::Point(mLastContentPaintMetrics.mViewportScrollOffset.x,
                                  mLastContentPaintMetrics.mViewportScrollOffset.y);
  }

  nsEventStatus status;
  switch (aEvent.eventStructType) {
  case NS_TOUCH_EVENT: {
    MultiTouchInput event(static_cast<const nsTouchEvent&>(aEvent));
    status = HandleInputEvent(event);
    break;
  }
  case NS_MOUSE_EVENT: {
    MultiTouchInput event(static_cast<const nsMouseEvent&>(aEvent));
    status = HandleInputEvent(event);
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
    for (PRUint32 i = 0; i < touches.Length(); ++i) {
      nsIDOMTouch* touch = touches[i];
      if (touch) {
        gfx::Point refPoint = WidgetSpaceToCompensatedViewportSpace(
          gfx::Point(touch->mRefPoint.x, touch->mRefPoint.y),
          currentZoom);
        touch->mRefPoint = nsIntPoint(refPoint.x, refPoint.y);
      }
    }
    break;
  }
  default: {
    gfx::Point refPoint = WidgetSpaceToCompensatedViewportSpace(
      gfx::Point(aOutEvent->refPoint.x, aOutEvent->refPoint.y),
      currentZoom);
    aOutEvent->refPoint = nsIntPoint(refPoint.x, refPoint.y);
    break;
  }
  }

  return status;
}

nsEventStatus AsyncPanZoomController::HandleInputEvent(const InputData& aEvent) {
  nsEventStatus rv = nsEventStatus_eIgnore;

  if (mGestureEventListener) {
    nsEventStatus rv = mGestureEventListener->HandleInputEvent(aEvent);
    if (rv == nsEventStatus_eConsumeNoDefault)
      return rv;
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
  PRInt32 xPos = point.x, yPos = point.y;

  switch (mState) {
    case FLING:
      CancelAnimation();
      // Fall through.
    case NOTHING:
      mX.StartTouch(xPos);
      mY.StartTouch(yPos);
      mState = TOUCHING;
      break;
    case TOUCHING:
    case PANNING:
    case PINCHING:
      NS_WARNING("Received impossible touch in OnTouchStart");
      break;
    default:
      NS_WARNING("Unhandled case in OnTouchStart");
      break;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchMove(const MultiTouchInput& aEvent) {
  switch (mState) {
    case FLING:
    case NOTHING:
      // May happen if the user double-taps and drags without lifting after the
      // second tap. Ignore the move if this happens.
      return nsEventStatus_eIgnore;

    case TOUCHING: {
      float panThreshold = 1.0f/2.0f * mDPI;
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
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchEnd(const MultiTouchInput& aEvent) {
  switch (mState) {
  case FLING:
    // Should never happen.
    NS_WARNING("Received impossible touch end in OnTouchEnd.");
    // Fall through.
  case NOTHING:
    // May happen if the user double-taps and drags without lifting after the
    // second tap. Ignore if this happens.
    return nsEventStatus_eIgnore;

  case TOUCHING:
    mState = NOTHING;
    return nsEventStatus_eIgnore;

  case PANNING:
    {
      MonitorAutoLock monitor(mMonitor);
      ScheduleComposite();
      RequestContentRepaint();
    }
    mState = FLING;
    return nsEventStatus_eConsumeNoDefault;
  case PINCHING:
    mState = NOTHING;
    // Scale gesture listener should have handled this.
    NS_WARNING("Gesture listener should have handled pinching in OnTouchEnd.");
    return nsEventStatus_eIgnore;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchCancel(const MultiTouchInput& aEvent) {
  mState = NOTHING;
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnScaleBegin(const PinchGestureInput& aEvent) {
  mState = PINCHING;
  mLastZoomFocus = aEvent.mFocusPoint;

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnScale(const PinchGestureInput& aEvent) {
  float prevSpan = aEvent.mPreviousSpan;
  if (fabsf(prevSpan) <= EPSILON || fabsf(aEvent.mCurrentSpan) <= EPSILON) {
    // We're still handling it; we've just decided to throw this event away.
    return nsEventStatus_eConsumeNoDefault;
  }

  float spanRatio = aEvent.mCurrentSpan / aEvent.mPreviousSpan;

  {
    MonitorAutoLock monitor(mMonitor);

    float scale = mFrameMetrics.mResolution.width;

    nsIntPoint focusPoint = aEvent.mFocusPoint;
    PRInt32 xFocusChange = (mLastZoomFocus.x - focusPoint.x) / scale, yFocusChange = (mLastZoomFocus.y - focusPoint.y) / scale;
    // If displacing by the change in focus point will take us off page bounds,
    // then reduce the displacement such that it doesn't.
    if (mX.DisplacementWillOverscroll(xFocusChange) != Axis::OVERSCROLL_NONE) {
      xFocusChange -= mX.DisplacementWillOverscrollAmount(xFocusChange);
    }
    if (mY.DisplacementWillOverscroll(yFocusChange) != Axis::OVERSCROLL_NONE) {
      yFocusChange -= mY.DisplacementWillOverscrollAmount(yFocusChange);
    }
    ScrollBy(nsIntPoint(xFocusChange, yFocusChange));

    // When we zoom in with focus, we can zoom too much towards the boundaries
    // that we actually go over them. These are the needed displacements along
    // either axis such that we don't overscroll the boundaries when zooming.
    PRInt32 neededDisplacementX = 0, neededDisplacementY = 0;

    // Only do the scaling if we won't go over 8x zoom in or out.
    bool doScale = (scale < 8.0f && spanRatio > 1.0f) || (scale > 0.125f && spanRatio < 1.0f);

    // If this zoom will take it over 8x zoom in either direction, but it's not
    // already there, then normalize it.
    if (scale * spanRatio > 8.0f) {
      spanRatio = scale / 8.0f;
    } else if (scale * spanRatio < 0.125f) {
      spanRatio = scale / 0.125f;
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
      ScaleWithFocus(scale * spanRatio,
                     focusPoint);

      if (neededDisplacementX != 0 || neededDisplacementY != 0) {
        ScrollBy(nsIntPoint(neededDisplacementX, neededDisplacementY));
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
  mState = PANNING;
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
  // XXX: Implement this.
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnSingleTapUp(const TapGestureInput& aEvent) {
  // XXX: Implement this.
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnSingleTapConfirmed(const TapGestureInput& aEvent) {
  // XXX: Implement this.
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnDoubleTap(const TapGestureInput& aEvent) {
  // XXX: Implement this.
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnCancelTap(const TapGestureInput& aEvent) {
  // XXX: Implement this.
  return nsEventStatus_eIgnore;
}

float AsyncPanZoomController::PanDistance() {
  return NS_hypot(mX.PanDistance(), mY.PanDistance()) * mFrameMetrics.mResolution.width;
}

const nsPoint AsyncPanZoomController::GetVelocityVector() {
  return nsPoint(
    mX.GetVelocity(),
    mY.GetVelocity()
  );
}

void AsyncPanZoomController::StartPanning(const MultiTouchInput& aEvent) {
  SingleTouchData& touch = GetFirstSingleTouch(aEvent);

  float dx = mX.PanDistance(),
        dy = mY.PanDistance();

  double angle = atan2(dy, dx); // range [-pi, pi]
  angle = fabs(angle); // range [0, pi]

  mX.StartTouch(touch.mScreenPoint.x);
  mY.StartTouch(touch.mScreenPoint.y);
  mState = PANNING;

  if (angle < AXIS_LOCK_ANGLE || angle > (M_PI - AXIS_LOCK_ANGLE)) {
    mY.LockPanning();
  } else if (fabsf(angle - M_PI / 2) < AXIS_LOCK_ANGLE) {
    mX.LockPanning();
  }
}

void AsyncPanZoomController::UpdateWithTouchAtDevicePoint(const MultiTouchInput& aEvent) {
  SingleTouchData& touch = GetFirstSingleTouch(aEvent);
  nsIntPoint point = touch.mScreenPoint;
  PRInt32 xPos = point.x, yPos = point.y;
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
    float inverseScale = 1 / mFrameMetrics.mResolution.width;

    PRInt32 xDisplacement = mX.GetDisplacementForDuration(inverseScale, timeDelta);
    PRInt32 yDisplacement = mY.GetDisplacementForDuration(inverseScale, timeDelta);
    if (!xDisplacement && !yDisplacement) {
      return;
    }

    ScrollBy(nsIntPoint(xDisplacement, yDisplacement));
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
    RequestContentRepaint();
    mState = NOTHING;
    return false;
  }

  // We want to inversely scale it because when you're zoomed further in, a
  // larger swipe should move you a shorter distance.
  float inverseScale = 1 / mFrameMetrics.mResolution.width;

  ScrollBy(nsIntPoint(
    mX.GetDisplacementForDuration(inverseScale, aDelta),
    mY.GetDisplacementForDuration(inverseScale, aDelta)
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

void AsyncPanZoomController::ScrollBy(const nsIntPoint& aOffset) {
  nsIntPoint newOffset(mFrameMetrics.mViewportScrollOffset.x + aOffset.x,
                       mFrameMetrics.mViewportScrollOffset.y + aOffset.y);
  FrameMetrics metrics(mFrameMetrics);
  metrics.mViewportScrollOffset = newOffset;
  mFrameMetrics = metrics;
}

void AsyncPanZoomController::SetPageRect(const gfx::Rect& aCSSPageRect) {
  FrameMetrics metrics = mFrameMetrics;
  gfx::Rect pageSize = aCSSPageRect;
  float scale = mFrameMetrics.mResolution.width;

  // The page rect is the css page rect scaled by the current zoom.
  pageSize.ScaleRoundOut(scale);

  // Round the page rect so we don't get any truncation, then get the nsIntRect
  // from this.
  metrics.mContentRect = nsIntRect(pageSize.x, pageSize.y, pageSize.width, pageSize.height);
  metrics.mCSSContentRect = aCSSPageRect;

  mFrameMetrics = metrics;
}

void AsyncPanZoomController::ScaleWithFocus(float aScale, const nsIntPoint& aFocus) {
  FrameMetrics metrics(mFrameMetrics);

  // Don't set the scale to the inputted value, but rather multiply it in.
  float scaleFactor = aScale / metrics.mResolution.width;

  metrics.mResolution.width = metrics.mResolution.height = aScale;

  // Force a recalculation of the page rect based on the new zoom and the
  // current CSS page rect (which is unchanged since it's not affected by zoom).
  SetPageRect(mFrameMetrics.mCSSContentRect);

  nsIntPoint scrollOffset = metrics.mViewportScrollOffset;

  scrollOffset.x += aFocus.x * (scaleFactor - 1.0f);
  scrollOffset.y += aFocus.y * (scaleFactor - 1.0f);

  metrics.mViewportScrollOffset = scrollOffset;

  mFrameMetrics = metrics;
}

const nsIntRect AsyncPanZoomController::CalculatePendingDisplayPort() {
  float scale = mFrameMetrics.mResolution.width;
  nsIntRect viewport = mFrameMetrics.mViewport;
  viewport.ScaleRoundIn(1 / scale);

  nsIntPoint scrollOffset = mFrameMetrics.mViewportScrollOffset;
  nsPoint velocity = GetVelocityVector();

  // The displayport is relative to the current scroll offset. Here's a little
  // diagram to make it easier to see:
  //
  //       - - - -
  //       |     |
  //    *************
  //    *  |     |  *
  // - -*- @------ -*- -
  // |  *  |=====|  *  |
  //    *  |=====|  *
  // |  *  |=====|  *  |
  // - -*- ------- -*- -
  //    *  |     |  *
  //    *************
  //       |     |
  //       - - - -
  //
  // The full --- area with === inside it is the actual viewport rect, the *** area
  // is the displayport, and the - - - area is an imaginary additional page on all 4
  // borders of the actual page. Notice that the displayport intersects half-way with
  // each of the imaginary extra pages. The @ symbol at the top left of the
  // viewport marks the current scroll offset. From the @ symbol to the far left
  // and far top, it is clear that this distance is 1/4 of the displayport's
  // height/width dimension.
  const float STATIONARY_SIZE_MULTIPLIER = 2.0f;
  const float SKATE_SIZE_MULTIPLIER = 3.0f;
  gfx::Rect displayPort(0, 0,
                        viewport.width * STATIONARY_SIZE_MULTIPLIER,
                        viewport.height * STATIONARY_SIZE_MULTIPLIER);

  // Iff there's motion along only one axis of movement, and it's above a
  // threshold, then we want to paint a larger area in the direction of that
  // motion so that it's less likely to checkerboard. Also note that the other
  // axis doesn't need its displayport enlarged beyond the viewport dimension,
  // since it is impossible for it to checkerboard along that axis until motion
  // begins on it.
  if (fabsf(velocity.x) > MIN_SKATE_SPEED && fabsf(velocity.y) < MIN_SKATE_SPEED) {
    displayPort.height = viewport.height;
    displayPort.width = viewport.width * SKATE_SIZE_MULTIPLIER;
    displayPort.x = velocity.x > 0 ? 0 : viewport.width - displayPort.width;
  } else if (fabsf(velocity.x) < MIN_SKATE_SPEED && fabsf(velocity.y) > MIN_SKATE_SPEED) {
    displayPort.width = viewport.width;
    displayPort.height = viewport.height * SKATE_SIZE_MULTIPLIER;
    displayPort.y = velocity.y > 0 ? 0 : viewport.height - displayPort.height;
  } else {
    displayPort.x = -displayPort.width / 4;
    displayPort.y = -displayPort.height / 4;
  }

  gfx::Rect shiftedDisplayPort = displayPort;
  shiftedDisplayPort.MoveBy(scrollOffset.x, scrollOffset.y);
  displayPort = shiftedDisplayPort.Intersect(mFrameMetrics.mCSSContentRect);
  displayPort.MoveBy(-scrollOffset.x, -scrollOffset.y);

  // Round the displayport so we don't get any truncation, then get the nsIntRect
  // from this.
  displayPort.Round();
  return nsIntRect(displayPort.x, displayPort.y, displayPort.width, displayPort.height);
}

void AsyncPanZoomController::SetDPI(int aDPI) {
  mDPI = aDPI;
}

void AsyncPanZoomController::ScheduleComposite() {
  if (mCompositorParent) {
    mCompositorParent->ScheduleRenderOnCompositorThread();
  }
}

void AsyncPanZoomController::RequestContentRepaint() {
  mFrameMetrics.mDisplayPort = CalculatePendingDisplayPort();

  // If we're trying to paint what we already think is painted, discard this
  // request since it's a pointless paint.
  nsIntRect oldDisplayPort = mLastPaintRequestMetrics.mDisplayPort,
            newDisplayPort = mFrameMetrics.mDisplayPort;
  oldDisplayPort.MoveBy(mLastPaintRequestMetrics.mViewportScrollOffset);
  newDisplayPort.MoveBy(mFrameMetrics.mViewportScrollOffset);

  if (oldDisplayPort.IsEqualEdges(newDisplayPort) &&
      mFrameMetrics.mResolution.width == mLastPaintRequestMetrics.mResolution.width) {
    return;
  }

  if (mContentPainterStatus == CONTENT_IDLE) {
    mContentPainterStatus = CONTENT_PAINTING;
    mLastPaintRequestMetrics = mFrameMetrics;
    mGeckoContentController->RequestContentRepaint(mFrameMetrics);
  } else {
    mContentPainterStatus = CONTENT_PAINTING_AND_PAINT_PENDING;
  }
}

bool AsyncPanZoomController::SampleContentTransformForFrame(const TimeStamp& aSampleTime,
                                                            const FrameMetrics& aFrame,
                                                            const gfx3DMatrix& aCurrentTransform,
                                                            gfx3DMatrix* aNewTransform) {
  // The eventual return value of this function. The compositor needs to know
  // whether or not to advance by a frame as soon as it can. For example, if a
  // fling is happening, it has to keep compositing so that the animation is
  // smooth. If an animation frame is requested, it is the compositor's
  // responsibility to schedule a composite.
  bool requestAnimationFrame = false;

  // Scales on the root layer, on what's currently painted.
  float rootScaleX = aCurrentTransform.GetXScale(),
        rootScaleY = aCurrentTransform.GetYScale();

  nsIntPoint metricsScrollOffset(0, 0);
  nsIntPoint scrollOffset;
  float localScaleX, localScaleY;

  {
    MonitorAutoLock mon(mMonitor);

    // If a fling is currently happening, apply it now. We can pull the updated
    // metrics afterwards.
    requestAnimationFrame = requestAnimationFrame || DoFling(aSampleTime - mLastSampleTime);

    // Current local transform; this is not what's painted but rather what PZC has
    // transformed due to touches like panning or pinching. Eventually, the root
    // layer transform will become this during runtime, but we must wait for Gecko
    // to repaint.
    localScaleX = mFrameMetrics.mResolution.width;
    localScaleY = mFrameMetrics.mResolution.height;

    if (aFrame.IsScrollable()) {
      metricsScrollOffset = aFrame.mViewportScrollOffset;
    }

    scrollOffset = mFrameMetrics.mViewportScrollOffset;
  }

  nsIntPoint scrollCompensation(
    (scrollOffset.x / rootScaleX - metricsScrollOffset.x) * localScaleX,
    (scrollOffset.y / rootScaleY - metricsScrollOffset.y) * localScaleY);

  ViewTransform treeTransform(-scrollCompensation, localScaleX, localScaleY);
  *aNewTransform = gfx3DMatrix(treeTransform) * aCurrentTransform;

  mLastSampleTime = aSampleTime;

  return requestAnimationFrame;
}

void AsyncPanZoomController::NotifyLayersUpdated(const FrameMetrics& aViewportFrame, bool aIsFirstPaint) {
  MonitorAutoLock monitor(mMonitor);

  mLastContentPaintMetrics = aViewportFrame;

  if (mContentPainterStatus != CONTENT_IDLE) {
    if (mContentPainterStatus == CONTENT_PAINTING_AND_PAINT_PENDING) {
      mContentPainterStatus = CONTENT_IDLE;
      RequestContentRepaint();
    } else {
      mContentPainterStatus = CONTENT_IDLE;
    }
  }

  if (aIsFirstPaint || mFrameMetrics.IsDefault()) {
    mContentPainterStatus = CONTENT_IDLE;

    mX.StopTouch();
    mY.StopTouch();
    mFrameMetrics = aViewportFrame;
    mFrameMetrics.mResolution.width = 1 / mFrameMetrics.mResolution.width;
    mFrameMetrics.mResolution.height = 1 / mFrameMetrics.mResolution.height;
    SetPageRect(mFrameMetrics.mCSSContentRect);

    // Bug 776413/fixme: Request a repaint as soon as a page is loaded so that
    // we get a larger displayport. This is very bad because we're wasting a
    // paint and not initializating the displayport correctly.
    RequestContentRepaint();
  } else if (!mFrameMetrics.mContentRect.IsEqualEdges(aViewportFrame.mContentRect)) {
    mFrameMetrics.mCSSContentRect = aViewportFrame.mCSSContentRect;
    SetPageRect(mFrameMetrics.mCSSContentRect);
  }
}

const FrameMetrics& AsyncPanZoomController::GetFrameMetrics() {
  mMonitor.AssertCurrentThreadOwns();
  return mFrameMetrics;
}

void AsyncPanZoomController::UpdateViewportSize(int aWidth, int aHeight) {
  MonitorAutoLock mon(mMonitor);
  FrameMetrics metrics = GetFrameMetrics();
  metrics.mViewport = nsIntRect(0, 0, aWidth, aHeight);
  mFrameMetrics = metrics;
}

}
}
