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

AsyncPanZoomController::AsyncPanZoomController(GeckoContentController* aGeckoContentController,
                                               GestureBehavior aGestures)
  :  mGeckoContentController(aGeckoContentController),
     mX(this),
     mY(this),
     mMonitor("AsyncPanZoomController"),
     mLastSampleTime(TimeStamp::Now()),
     mState(NOTHING),
     mDPI(72)
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
  SingleTouchData& touch = GetFirstSingleTouch(aEvent);
  nsIntPoint point = touch.mScreenPoint;
  PRInt32 xPos = point.x, yPos = point.y;

  switch (mState) {
    case FLING:
    case NOTHING:
      // May happen if the user double-taps and drags without lifting after the
      // second tap. Ignore the move if this happens.
      return nsEventStatus_eIgnore;

    case TOUCHING: {
      float panThreshold = 1.0f/16.0f * mDPI;
      if (PanDistance(aEvent) < panThreshold) {
        return nsEventStatus_eIgnore;
      }
      mLastRepaint = aEvent.mTime;
      mX.StartTouch(xPos);
      mY.StartTouch(yPos);
      mState = PANNING;
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
    mLastSampleTime = TimeStamp::Now();
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

float AsyncPanZoomController::PanDistance(const MultiTouchInput& aEvent) {
  SingleTouchData& touch = GetFirstSingleTouch(aEvent);
  nsIntPoint point = touch.mScreenPoint;
  PRInt32 xPos = point.x, yPos = point.y;
  mX.UpdateWithTouchAtDevicePoint(xPos, 0);
  mY.UpdateWithTouchAtDevicePoint(yPos, 0);
  return NS_hypot(mX.PanDistance(), mY.PanDistance()) * mFrameMetrics.mResolution.width;
}

const nsPoint AsyncPanZoomController::GetVelocityVector() {
  return nsPoint(
    mX.GetVelocity(),
    mY.GetVelocity()
  );
}

void AsyncPanZoomController::TrackTouch(const MultiTouchInput& aEvent) {
  SingleTouchData& touch = GetFirstSingleTouch(aEvent);
  nsIntPoint point = touch.mScreenPoint;
  PRInt32 xPos = point.x, yPos = point.y, timeDelta = aEvent.mTime - mLastEventTime;

  // Probably a duplicate event, just throw it away.
  if (!timeDelta) {
    return;
  }

  {
    MonitorAutoLock monitor(mMonitor);
    mX.UpdateWithTouchAtDevicePoint(xPos, timeDelta);
    mY.UpdateWithTouchAtDevicePoint(yPos, timeDelta);

    // We want to inversely scale it because when you're zoomed further in, a
    // larger swipe should move you a shorter distance.
    float inverseScale = 1 / mFrameMetrics.mResolution.width;

    PRInt32 xDisplacement = mX.UpdateAndGetDisplacement(inverseScale);
    PRInt32 yDisplacement = mY.UpdateAndGetDisplacement(inverseScale);
    if (!xDisplacement && !yDisplacement) {
      return;
    }

    ScrollBy(nsIntPoint(xDisplacement, yDisplacement));
    ScheduleComposite();

    if (aEvent.mTime - mLastRepaint >= PAN_REPAINT_INTERVAL) {
      RequestContentRepaint();
      mLastRepaint = aEvent.mTime;
    }
  }
}

SingleTouchData& AsyncPanZoomController::GetFirstSingleTouch(const MultiTouchInput& aEvent) {
  return (SingleTouchData&)aEvent.mTouches[0];
}

bool AsyncPanZoomController::DoFling(const TimeDuration& aDelta) {
  if (mState != FLING) {
    return false;
  }

  if (!mX.FlingApplyFrictionOrCancel(aDelta) && !mY.FlingApplyFrictionOrCancel(aDelta)) {
    RequestContentRepaint();
    mState = NOTHING;
    return false;
  }

  // We want to inversely scale it because when you're zoomed further in, a
  // larger swipe should move you a shorter distance.
  float inverseScale = 1 / mFrameMetrics.mResolution.width;

  ScrollBy(nsIntPoint(
    mX.UpdateAndGetDisplacement(inverseScale),
    mY.UpdateAndGetDisplacement(inverseScale)
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

  const float SIZE_MULTIPLIER = 2.0f;
  nsIntPoint scrollOffset = mFrameMetrics.mViewportScrollOffset;
  gfx::Rect contentRect = mFrameMetrics.mCSSContentRect;

  // Paint a larger portion of the screen than just what we can see. This makes
  // it less likely that we'll checkerboard when panning around and Gecko hasn't
  // repainted yet.
  float desiredWidth = viewport.width * SIZE_MULTIPLIER,
        desiredHeight = viewport.height * SIZE_MULTIPLIER;

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
  gfx::Rect displayPort(-desiredWidth / 4, -desiredHeight / 4, desiredWidth, desiredHeight);

  // Check if the desired boundaries go over the CSS page rect along the top or
  // left. If they do, shift them to the right or down.
  float oldDisplayPortX = displayPort.x, oldDisplayPortY = displayPort.y;
  if (displayPort.X() + scrollOffset.x < contentRect.X()) {
    displayPort.x = contentRect.X() - scrollOffset.x;
  }
  if (displayPort.Y() + scrollOffset.y < contentRect.Y()) {
    displayPort.y = contentRect.Y() - scrollOffset.y;
  }

  // We don't need to paint the extra area that was going to overlap with the
  // content rect. Subtract out this extra width or height.
  displayPort.width -= displayPort.x - oldDisplayPortX;
  displayPort.height -= displayPort.y - oldDisplayPortY;

  // Check if the desired boundaries go over the CSS page rect along the right
  // or bottom. If they do, subtract out some height or width such that they
  // perfectly align with the end of the CSS page rect.
  if (displayPort.XMost() + scrollOffset.x > contentRect.XMost()) {
    displayPort.width = NS_MAX(0.0f, contentRect.XMost() - (displayPort.X() + scrollOffset.x));
  }
  if (displayPort.YMost() + scrollOffset.y > contentRect.YMost()) {
    displayPort.height = NS_MAX(0.0f, contentRect.YMost() - (displayPort.Y() + scrollOffset.y));
  }

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
  mGeckoContentController->RequestContentRepaint(mFrameMetrics);
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

  if (aIsFirstPaint || mFrameMetrics.IsDefault()) {
    mX.StopTouch();
    mY.StopTouch();
    mFrameMetrics = aViewportFrame;
    mFrameMetrics.mResolution.width = 1 / mFrameMetrics.mResolution.width;
    mFrameMetrics.mResolution.height = 1 / mFrameMetrics.mResolution.height;
    SetPageRect(mFrameMetrics.mCSSContentRect);
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
