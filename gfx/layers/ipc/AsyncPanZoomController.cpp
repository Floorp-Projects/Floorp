/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
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
#include <algorithm>
#include "mozilla/layers/LayerManagerComposite.h"

using namespace mozilla::css;

namespace mozilla {
namespace layers {

/**
 * Constant describing the tolerance in distance we use, multiplied by the
 * device DPI, before we start panning the screen. This is to prevent us from
 * accidentally processing taps as touch moves, and from very short/accidental
 * touches moving the screen.
 */
static float gTouchStartTolerance = 1.0f/16.0f;

static const float EPSILON = 0.0001;

/**
 * Maximum amount of time while panning before sending a viewport change. This
 * will asynchronously repaint the page. It is also forced when panning stops.
 */
static int32_t gPanRepaintInterval = 250;

/**
 * Maximum amount of time flinging before sending a viewport change. This will
 * asynchronously repaint the page.
 */
static int32_t gFlingRepaintInterval = 75;

/**
 * Minimum amount of speed along an axis before we begin painting far ahead by
 * adjusting the displayport.
 */
static float gMinSkateSpeed = 0.7f;

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
static const float MAX_ZOOM = 8.0f;

/**
 * Minimum zoom amount, always used, even if a page asks for lower.
 */
static const float MIN_ZOOM = 0.125f;

/**
 * Amount of time before we timeout touch event listeners. For example, if
 * content is being unruly/slow and we don't get a response back within this
 * time, we will just pretend that content did not preventDefault any touch
 * events we dispatched to it.
 */
static int gTouchListenerTimeout = 300;

/**
 * Number of samples to store of how long it took to paint after the previous
 * requests.
 */
static int gNumPaintDurationSamples = 3;

/** The multiplier we apply to a dimension's length if it is skating. That is,
 * if it's going above sMinSkateSpeed. We prefer to increase the size of the
 * Y axis because it is more natural in the case that a user is reading a page
 * that scrolls up/down. Note that one, both or neither of these may be used
 * at any instant.
 */
static float gXSkateSizeMultiplier = 3.0f;
static float gYSkateSizeMultiplier = 3.5f;

/** The multiplier we apply to a dimension's length if it is stationary. We
 * prefer to increase the size of the Y axis because it is more natural in the
 * case that a user is reading a page that scrolls up/down. Note that one,
 * both or neither of these may be used at any instant.
 */
static float gXStationarySizeMultiplier = 1.5f;
static float gYStationarySizeMultiplier = 2.5f;

/**
 * The time period in ms that throttles mozbrowserasyncscroll event.
 * Default is 100ms if there is no "apzc.asyncscroll.throttle" in preference.
 */

static int gAsyncScrollThrottleTime = 100;
/**
 * The timeout in ms for mAsyncScrollTimeoutTask delay task.
 * Default is 300ms if there is no "apzc.asyncscroll.timeout" in preference.
 */
static int gAsyncScrollTimeout = 300;

static TimeStamp sFrameTime;

static TimeStamp
GetFrameTime() {
  if (sFrameTime.IsNull()) {
    return TimeStamp::Now();
  }
  return sFrameTime;
}

void
AsyncPanZoomController::SetFrameTime(const TimeStamp& aTime) {
  sFrameTime = aTime;
}

/*static*/ void
AsyncPanZoomController::InitializeGlobalState()
{
  MOZ_ASSERT(NS_IsMainThread());

  static bool sInitialized = false;
  if (sInitialized)
    return;
  sInitialized = true;

  Preferences::AddIntVarCache(&gPanRepaintInterval, "gfx.azpc.pan_repaint_interval", gPanRepaintInterval);
  Preferences::AddIntVarCache(&gFlingRepaintInterval, "gfx.azpc.fling_repaint_interval", gFlingRepaintInterval);
  Preferences::AddFloatVarCache(&gMinSkateSpeed, "gfx.azpc.min_skate_speed", gMinSkateSpeed);
  Preferences::AddIntVarCache(&gTouchListenerTimeout, "gfx.azpc.touch_listener_timeout", gTouchListenerTimeout);
  Preferences::AddIntVarCache(&gNumPaintDurationSamples, "gfx.azpc.num_paint_duration_samples", gNumPaintDurationSamples);
  Preferences::AddFloatVarCache(&gTouchStartTolerance, "gfx.azpc.touch_start_tolerance", gTouchStartTolerance);
  Preferences::AddFloatVarCache(&gXSkateSizeMultiplier, "gfx.azpc.x_skate_size_multiplier", gXSkateSizeMultiplier);
  Preferences::AddFloatVarCache(&gYSkateSizeMultiplier, "gfx.azpc.y_skate_size_multiplier", gYSkateSizeMultiplier);
  Preferences::AddFloatVarCache(&gXStationarySizeMultiplier, "gfx.azpc.x_stationary_size_multiplier", gXStationarySizeMultiplier);
  Preferences::AddFloatVarCache(&gYStationarySizeMultiplier, "gfx.azpc.y_stationary_size_multiplier", gYStationarySizeMultiplier);
  Preferences::AddIntVarCache(&gAsyncScrollThrottleTime, "apzc.asyncscroll.throttle", gAsyncScrollThrottleTime);
  Preferences::AddIntVarCache(&gAsyncScrollTimeout, "apzc.asyncscroll.timeout", gAsyncScrollTimeout);

  gComputedTimingFunction = new ComputedTimingFunction();
  gComputedTimingFunction->Init(
    nsTimingFunction(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE));
  ClearOnShutdown(&gComputedTimingFunction);
}

AsyncPanZoomController::AsyncPanZoomController(uint64_t aLayersId,
                                               GeckoContentController* aGeckoContentController,
                                               GestureBehavior aGestures)
  :  mLayersId(aLayersId),
     mPaintThrottler(GetFrameTime()),
     mGeckoContentController(aGeckoContentController),
     mRefPtrMonitor("RefPtrMonitor"),
     mMonitor("AsyncPanZoomController"),
     mTouchListenerTimeoutTask(nullptr),
     mX(this),
     mY(this),
     mAllowZoom(true),
     mMinZoom(MIN_ZOOM),
     mMaxZoom(MAX_ZOOM),
     mLastSampleTime(GetFrameTime()),
     mState(NOTHING),
     mLastAsyncScrollTime(GetFrameTime()),
     mLastAsyncScrollOffset(0, 0),
     mCurrentAsyncScrollOffset(0, 0),
     mAsyncScrollTimeoutTask(nullptr),
     mDPI(72),
     mDisableNextTouchBatch(false),
     mHandlingTouchQueue(false),
     mDelayPanning(false)
{
  MOZ_COUNT_CTOR(AsyncPanZoomController);

  if (aGestures == USE_GESTURE_DETECTOR) {
    mGestureEventListener = new GestureEventListener(this);
  }

  SetDPI(mDPI);
}

AsyncPanZoomController::~AsyncPanZoomController() {
  MOZ_COUNT_DTOR(AsyncPanZoomController);
}

already_AddRefed<GeckoContentController>
AsyncPanZoomController::GetGeckoContentController() {
  MonitorAutoLock lock(mRefPtrMonitor);
  nsRefPtr<GeckoContentController> controller = mGeckoContentController;
  return controller.forget();
}

already_AddRefed<GestureEventListener>
AsyncPanZoomController::GetGestureEventListener() {
  MonitorAutoLock lock(mRefPtrMonitor);
  nsRefPtr<GestureEventListener> listener = mGestureEventListener;
  return listener.forget();
}

void
AsyncPanZoomController::Destroy()
{
  { // scope the lock
    MonitorAutoLock lock(mRefPtrMonitor);
    mGeckoContentController = nullptr;
    mGestureEventListener = nullptr;
  }
  mPrevSibling = nullptr;
  mLastChild = nullptr;
}

/* static */float
AsyncPanZoomController::GetTouchStartTolerance()
{
  return gTouchStartTolerance;
}

static CSSPoint
WidgetSpaceToCompensatedViewportSpace(const ScreenPoint& aPoint,
                                      const CSSToScreenScale& aCurrentZoom)
{
  // Transform the input point from local widget space to the content document
  // space that the user is seeing, from last composite.
  // FIXME/bug 775451: this doesn't attempt to compensate for content transforms
  // in effect on the compositor.  The problem is that it's very hard for us to
  // know what content CSS pixel is at widget point 0,0 based on information
  // available here.  So we use this hacky implementation for now, which works
  // in quiescent states.

  return aPoint / aCurrentZoom;
}

nsEventStatus
AsyncPanZoomController::ReceiveInputEvent(const nsInputEvent& aEvent,
                                          nsInputEvent* aOutEvent)
{
  CSSToScreenScale currentResolution;
  {
    ReentrantMonitorAutoEnter lock(mMonitor);
    currentResolution = mFrameMetrics.CalculateResolution();
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
    const nsTArray< nsRefPtr<dom::Touch> >& touches = touchEvent->touches;
    for (uint32_t i = 0; i < touches.Length(); ++i) {
      nsIDOMTouch* touch = touches[i];
      if (touch) {
        CSSPoint refCSSPoint = WidgetSpaceToCompensatedViewportSpace(
          ScreenPoint::FromUnknownPoint(gfx::Point(
            touch->mRefPoint.x, touch->mRefPoint.y)),
          currentResolution);
        LayoutDevicePoint refPoint = refCSSPoint * mFrameMetrics.mDevPixelsPerCSSPixel;
        touch->mRefPoint = nsIntPoint(refPoint.x, refPoint.y);
      }
    }
    break;
  }
  default: {
    CSSPoint refCSSPoint = WidgetSpaceToCompensatedViewportSpace(
      ScreenPoint::FromUnknownPoint(gfx::Point(
        aOutEvent->refPoint.x, aOutEvent->refPoint.y)),
      currentResolution);
    LayoutDevicePoint refPoint = refCSSPoint * mFrameMetrics.mDevPixelsPerCSSPixel;
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

        PostDelayedTask(mTouchListenerTimeoutTask, gTouchListenerTimeout);
      }
    }
    return nsEventStatus_eConsumeNoDefault;
  }

  return HandleInputEvent(aEvent);
}

nsEventStatus AsyncPanZoomController::HandleInputEvent(const InputData& aEvent) {
  nsEventStatus rv = nsEventStatus_eIgnore;

  nsRefPtr<GestureEventListener> listener = GetGestureEventListener();
  if (listener && !mDisableNextTouchBatch) {
    rv = listener->HandleInputEvent(aEvent);
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

        PostDelayedTask(mTouchListenerTimeoutTask, gTouchListenerTimeout);
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

  ScreenIntPoint point = touch.mScreenPoint;

  switch (mState) {
    case ANIMATING_ZOOM:
      // We just interrupted a double-tap animation, so force a redraw in case
      // this touchstart is just a tap that doesn't end up triggering a redraw.
      {
        ReentrantMonitorAutoEnter lock(mMonitor);
        // Bring the resolution back in sync with the zoom.
        SetZoomAndResolution(mFrameMetrics.mZoom);
        RequestContentRepaint();
        ScheduleComposite();
      }
      // Fall through.
    case FLING:
      CancelAnimation();
      // Fall through.
    case NOTHING:
      mX.StartTouch(point.x);
      mY.StartTouch(point.y);
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
      float panThreshold = gTouchStartTolerance * mDPI;
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

  {
    ReentrantMonitorAutoEnter lock(mMonitor);
    SendAsyncScrollEvent();
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
      ReentrantMonitorAutoEnter lock(mMonitor);
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
    ReentrantMonitorAutoEnter lock(mMonitor);

    CSSToScreenScale resolution = mFrameMetrics.CalculateResolution();
    gfxFloat userZoom = mFrameMetrics.mZoom.scale;
    ScreenPoint focusPoint = aEvent.mFocusPoint;

    CSSPoint focusChange = (mLastZoomFocus - focusPoint) / resolution;
    // If displacing by the change in focus point will take us off page bounds,
    // then reduce the displacement such that it doesn't.
    if (mX.DisplacementWillOverscroll(focusChange.x) != Axis::OVERSCROLL_NONE) {
      focusChange.x -= mX.DisplacementWillOverscrollAmount(focusChange.x);
    }
    if (mY.DisplacementWillOverscroll(focusChange.y) != Axis::OVERSCROLL_NONE) {
      focusChange.y -= mY.DisplacementWillOverscrollAmount(focusChange.y);
    }
    ScrollBy(focusChange);

    // When we zoom in with focus, we can zoom too much towards the boundaries
    // that we actually go over them. These are the needed displacements along
    // either axis such that we don't overscroll the boundaries when zooming.
    gfx::Point neededDisplacement;

    float maxZoom = mMaxZoom / mFrameMetrics.CalculateIntrinsicScale().scale;
    float minZoom = mMinZoom / mFrameMetrics.CalculateIntrinsicScale().scale;

    bool doScale = (spanRatio > 1.0 && userZoom < maxZoom) ||
                   (spanRatio < 1.0 && userZoom > minZoom);

    if (doScale) {
      if (userZoom * spanRatio > maxZoom) {
        spanRatio = maxZoom / userZoom;
      } else if (userZoom * spanRatio < minZoom) {
        spanRatio = minZoom / userZoom;
      }

      switch (mX.ScaleWillOverscroll(spanRatio, focusPoint.x))
      {
        case Axis::OVERSCROLL_NONE:
          break;
        case Axis::OVERSCROLL_MINUS:
        case Axis::OVERSCROLL_PLUS:
          neededDisplacement.x = -mX.ScaleWillOverscrollAmount(spanRatio, focusPoint.x);
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
          neededDisplacement.y = -mY.ScaleWillOverscrollAmount(spanRatio, focusPoint.y);
          break;
        case Axis::OVERSCROLL_BOTH:
          doScale = false;
          break;
      }
    }

    if (doScale) {
      ScaleWithFocus(userZoom * spanRatio, focusPoint);

      if (neededDisplacement != gfx::Point()) {
        ScrollBy(CSSPoint::FromUnknownPoint(neededDisplacement));
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
    ReentrantMonitorAutoEnter lock(mMonitor);
    ScheduleComposite();
    RequestContentRepaint();
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnLongPress(const TapGestureInput& aEvent) {
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    ReentrantMonitorAutoEnter lock(mMonitor);

    CSSToScreenScale resolution = mFrameMetrics.CalculateResolution();
    CSSPoint point = WidgetSpaceToCompensatedViewportSpace(aEvent.mPoint, resolution);
    controller->HandleLongTap(gfx::RoundedToInt(point));
    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnSingleTapUp(const TapGestureInput& aEvent) {
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnSingleTapConfirmed(const TapGestureInput& aEvent) {
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    ReentrantMonitorAutoEnter lock(mMonitor);

    CSSToScreenScale resolution = mFrameMetrics.CalculateResolution();
    CSSPoint point = WidgetSpaceToCompensatedViewportSpace(aEvent.mPoint, resolution);
    controller->HandleSingleTap(gfx::RoundedToInt(point));
    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnDoubleTap(const TapGestureInput& aEvent) {
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    ReentrantMonitorAutoEnter lock(mMonitor);

    if (mAllowZoom) {
      CSSToScreenScale resolution = mFrameMetrics.CalculateResolution();
      CSSPoint point = WidgetSpaceToCompensatedViewportSpace(aEvent.mPoint, resolution);
      controller->HandleDoubleTap(gfx::RoundedToInt(point));
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
  ReentrantMonitorAutoEnter lock(mMonitor);
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
  ScreenIntPoint point = touch.mScreenPoint;
  TimeDuration timeDelta = TimeDuration().FromMilliseconds(aEvent.mTime - mLastEventTime);

  // Probably a duplicate event, just throw it away.
  if (timeDelta.ToMilliseconds() <= EPSILON) {
    return;
  }

  mX.UpdateWithTouchAtDevicePoint(point.x, timeDelta);
  mY.UpdateWithTouchAtDevicePoint(point.y, timeDelta);
}

void AsyncPanZoomController::TrackTouch(const MultiTouchInput& aEvent) {
  TimeDuration timeDelta = TimeDuration().FromMilliseconds(aEvent.mTime - mLastEventTime);

  // Probably a duplicate event, just throw it away.
  if (timeDelta.ToMilliseconds() <= EPSILON) {
    return;
  }

  UpdateWithTouchAtDevicePoint(aEvent);

  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    // We want to inversely scale it because when you're zoomed further in, a
    // larger swipe should move you a shorter distance.
    ScreenToCSSScale inverseResolution = mFrameMetrics.CalculateResolution().Inverse();

    gfx::Point displacement(mX.GetDisplacementForDuration(inverseResolution.scale,
                                                          timeDelta),
                            mY.GetDisplacementForDuration(inverseResolution.scale,
                                                          timeDelta));
    if (fabs(displacement.x) <= EPSILON && fabs(displacement.y) <= EPSILON) {
      return;
    }

    ScrollBy(CSSPoint::FromUnknownPoint(displacement));
    ScheduleComposite();

    TimeDuration timePaintDelta = mPaintThrottler.TimeSinceLastRequest(GetFrameTime());
    if (timePaintDelta.ToMilliseconds() > gPanRepaintInterval) {
      RequestContentRepaint();
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

  bool shouldContinueFlingX = mX.FlingApplyFrictionOrCancel(aDelta),
       shouldContinueFlingY = mY.FlingApplyFrictionOrCancel(aDelta);
  // If we shouldn't continue the fling, let's just stop and repaint.
  if (!shouldContinueFlingX && !shouldContinueFlingY) {
    // Bring the resolution back in sync with the zoom, in case we scaled down
    // the zoom while accelerating.
    SetZoomAndResolution(mFrameMetrics.mZoom);
    SendAsyncScrollEvent();
    RequestContentRepaint();
    mState = NOTHING;
    return false;
  }

  // We want to inversely scale it because when you're zoomed further in, a
  // larger swipe should move you a shorter distance.
  ScreenToCSSScale inverseResolution = mFrameMetrics.CalculateResolution().Inverse();

  ScrollBy(CSSPoint::FromUnknownPoint(gfx::Point(
    mX.GetDisplacementForDuration(inverseResolution.scale, aDelta),
    mY.GetDisplacementForDuration(inverseResolution.scale, aDelta)
  )));
  TimeDuration timePaintDelta = mPaintThrottler.TimeSinceLastRequest(GetFrameTime());
  if (timePaintDelta.ToMilliseconds() > gFlingRepaintInterval) {
    RequestContentRepaint();
  }

  return true;
}

void AsyncPanZoomController::CancelAnimation() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  mState = NOTHING;
}

void AsyncPanZoomController::SetCompositorParent(CompositorParent* aCompositorParent) {
  mCompositorParent = aCompositorParent;
}

void AsyncPanZoomController::ScrollBy(const CSSPoint& aOffset) {
  mFrameMetrics.mScrollOffset += aOffset;
}

void AsyncPanZoomController::ScaleWithFocus(float aZoom,
                                            const ScreenPoint& aFocus) {
  float zoomFactor = aZoom / mFrameMetrics.mZoom.scale;
  CSSToScreenScale resolution = mFrameMetrics.CalculateResolution();

  SetZoomAndResolution(ScreenToScreenScale(aZoom));

  // If the new scale is very small, we risk multiplying in huge rounding
  // errors, so don't bother adjusting the scroll offset.
  if (resolution.scale >= 0.01f) {
    mFrameMetrics.mScrollOffset.x +=
      aFocus.x * (zoomFactor - 1.0) / resolution.scale;
    mFrameMetrics.mScrollOffset.y +=
      aFocus.y * (zoomFactor - 1.0) / resolution.scale;
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
  if (fabsf(aVelocity) > gMinSkateSpeed) {
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

const CSSRect AsyncPanZoomController::CalculatePendingDisplayPort(
  const FrameMetrics& aFrameMetrics,
  const gfx::Point& aVelocity,
  const gfx::Point& aAcceleration,
  double aEstimatedPaintDuration)
{
  // If we don't get an estimated paint duration, we probably don't have any
  // data. In this case, we're dealing with either a stationary frame or a first
  // paint. In either of these cases, we can just assume it'll take 1 second to
  // paint. Getting this correct is not important anyways since it's only really
  // useful when accelerating, which can't be happening at this point.
  double estimatedPaintDuration =
    aEstimatedPaintDuration > EPSILON ? aEstimatedPaintDuration : 1.0;

  CSSToScreenScale resolution = aFrameMetrics.CalculateResolution();
  CSSIntRect compositionBounds = gfx::RoundedIn(aFrameMetrics.mCompositionBounds / resolution);
  CSSRect scrollableRect = aFrameMetrics.mScrollableRect;

  // Ensure the scrollableRect is at least as big as the compositionBounds
  // because the scrollableRect can be smaller if the content is not large
  // and the scrollableRect hasn't been updated yet.
  // We move the scrollableRect up because we don't know if we can move it
  // down. i.e. we know that scrollableRect can go back as far as zero.
  // but we don't know how much further ahead it can go.
  if (scrollableRect.width < compositionBounds.width) {
      scrollableRect.x = std::max(0.f,
                                  scrollableRect.x - (compositionBounds.width - scrollableRect.width));
      scrollableRect.width = compositionBounds.width;
  }
  if (scrollableRect.height < compositionBounds.height) {
      scrollableRect.y = std::max(0.f,
                                  scrollableRect.y - (compositionBounds.height - scrollableRect.height));
      scrollableRect.height = compositionBounds.height;
  }

  CSSPoint scrollOffset = aFrameMetrics.mScrollOffset;

  CSSRect displayPort = CSSRect(compositionBounds);
  displayPort.MoveTo(0, 0);
  displayPort.Scale(gXStationarySizeMultiplier, gYStationarySizeMultiplier);

  // If there's motion along an axis of movement, and it's above a threshold,
  // then we want to paint a larger area in the direction of that motion so that
  // it's less likely to checkerboard.
  bool enlargedX = EnlargeDisplayPortAlongAxis(
    gXSkateSizeMultiplier, estimatedPaintDuration,
    compositionBounds.width, aVelocity.x, aAcceleration.x,
    &displayPort.x, &displayPort.width);
  bool enlargedY = EnlargeDisplayPortAlongAxis(
    gYSkateSizeMultiplier, estimatedPaintDuration,
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

  CSSRect shiftedDisplayPort = displayPort + scrollOffset;
  return scrollableRect.ClampRect(shiftedDisplayPort) - scrollOffset;
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
  mFrameMetrics.mDisplayPort =
    CalculatePendingDisplayPort(mFrameMetrics,
                                GetVelocityVector(),
                                GetAccelerationVector(),
                                mPaintThrottler.AverageDuration().ToSeconds());

  // If we're trying to paint what we already think is painted, discard this
  // request since it's a pointless paint.
  CSSRect oldDisplayPort = mLastPaintRequestMetrics.mDisplayPort
                         + mLastPaintRequestMetrics.mScrollOffset;
  CSSRect newDisplayPort = mFrameMetrics.mDisplayPort
                         + mFrameMetrics.mScrollOffset;

  if (fabsf(oldDisplayPort.x - newDisplayPort.x) < EPSILON &&
      fabsf(oldDisplayPort.y - newDisplayPort.y) < EPSILON &&
      fabsf(oldDisplayPort.width - newDisplayPort.width) < EPSILON &&
      fabsf(oldDisplayPort.height - newDisplayPort.height) < EPSILON &&
      fabsf(mLastPaintRequestMetrics.mScrollOffset.x -
            mFrameMetrics.mScrollOffset.x) < EPSILON &&
      fabsf(mLastPaintRequestMetrics.mScrollOffset.y -
            mFrameMetrics.mScrollOffset.y) < EPSILON &&
      mFrameMetrics.mResolution == mLastPaintRequestMetrics.mResolution) {
    return;
  }

  SendAsyncScrollEvent();

  // Cache the zoom since we're temporarily changing it for
  // acceleration-scaled painting.
  ScreenToScreenScale actualZoom = mFrameMetrics.mZoom;
  // Calculate the factor of acceleration based on the faster of the two axes.
  float accelerationFactor =
    clamped(std::max(mX.GetAccelerationFactor(), mY.GetAccelerationFactor()),
            MIN_ZOOM / 2.0f, MAX_ZOOM);
  // Scale down the resolution a bit based on acceleration.
  mFrameMetrics.mZoom.scale /= accelerationFactor;

  // This message is compressed, so fire whether or not we already have a paint
  // queued up. We need to know whether or not a paint was requested anyways,
  // for the purposes of content calling window.scrollTo().
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    mPaintThrottler.PostTask(
      FROM_HERE,
      NewRunnableMethod(controller.get(),
                        &GeckoContentController::RequestContentRepaint,
                        mFrameMetrics),
      GetFrameTime());
  }
  mFrameMetrics.mPresShellId = mLastContentPaintMetrics.mPresShellId;
  mLastPaintRequestMetrics = mFrameMetrics;

  // Set the zoom back to what it was for the purpose of logic control.
  mFrameMetrics.mZoom = actualZoom;
}

void
AsyncPanZoomController::FireAsyncScrollOnTimeout()
{
  if (mCurrentAsyncScrollOffset != mLastAsyncScrollOffset) {
    ReentrantMonitorAutoEnter lock(mMonitor);
    SendAsyncScrollEvent();
  }
  mAsyncScrollTimeoutTask = nullptr;
}

bool AsyncPanZoomController::SampleContentTransformForFrame(const TimeStamp& aSampleTime,
                                                            ViewTransform* aNewTransform,
                                                            ScreenPoint& aScrollOffset) {
  // The eventual return value of this function. The compositor needs to know
  // whether or not to advance by a frame as soon as it can. For example, if a
  // fling is happening, it has to keep compositing so that the animation is
  // smooth. If an animation frame is requested, it is the compositor's
  // responsibility to schedule a composite.
  bool requestAnimationFrame = false;

  {
    ReentrantMonitorAutoEnter lock(mMonitor);

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

      ScreenToScreenScale startZoom = mStartZoomToMetrics.mZoom;
      ScreenToScreenScale endZoom = mEndZoomToMetrics.mZoom;
      mFrameMetrics.mZoom = ScreenToScreenScale(endZoom.scale * sampledPosition +
                                                startZoom.scale * (1 - sampledPosition));

      mFrameMetrics.mScrollOffset = CSSPoint::FromUnknownPoint(gfx::Point(
        mEndZoomToMetrics.mScrollOffset.x * sampledPosition +
          mStartZoomToMetrics.mScrollOffset.x * (1 - sampledPosition),
        mEndZoomToMetrics.mScrollOffset.y * sampledPosition +
          mStartZoomToMetrics.mScrollOffset.y * (1 - sampledPosition)
      ));

      requestAnimationFrame = true;

      if (aSampleTime - mAnimationStartTime >= ZOOM_TO_DURATION) {
        // Bring the resolution in sync with the zoom.
        SetZoomAndResolution(mFrameMetrics.mZoom);
        mState = NOTHING;
        SendAsyncScrollEvent();
        RequestContentRepaint();
      }

      break;
    }
    default:
      break;
    }

    aScrollOffset = mFrameMetrics.mScrollOffset * mFrameMetrics.CalculateResolution();
    *aNewTransform = GetCurrentAsyncTransform();

    mCurrentAsyncScrollOffset = mFrameMetrics.mScrollOffset;
  }

  // Cancel the mAsyncScrollTimeoutTask because we will fire a
  // mozbrowserasyncscroll event or renew the mAsyncScrollTimeoutTask again.
  if (mAsyncScrollTimeoutTask) {
    mAsyncScrollTimeoutTask->Cancel();
    mAsyncScrollTimeoutTask = nullptr;
  }
  // Fire the mozbrowserasyncscroll event immediately if it's been
  // sAsyncScrollThrottleTime ms since the last time we fired the event and the
  // current scroll offset is different than the mLastAsyncScrollOffset we sent
  // with the last event.
  // Otherwise, start a timer to fire the event sAsyncScrollTimeout ms from now.
  TimeDuration delta = aSampleTime - mLastAsyncScrollTime;
  if (delta.ToMilliseconds() > gAsyncScrollThrottleTime &&
      mCurrentAsyncScrollOffset != mLastAsyncScrollOffset) {
    ReentrantMonitorAutoEnter lock(mMonitor);
    mLastAsyncScrollTime = aSampleTime;
    mLastAsyncScrollOffset = mCurrentAsyncScrollOffset;
    SendAsyncScrollEvent();
  }
  else {
    mAsyncScrollTimeoutTask =
      NewRunnableMethod(this, &AsyncPanZoomController::FireAsyncScrollOnTimeout);
    MessageLoop::current()->PostDelayedTask(FROM_HERE,
                                            mAsyncScrollTimeoutTask,
                                            gAsyncScrollTimeout);
  }

  mLastSampleTime = aSampleTime;

  return requestAnimationFrame;
}

ViewTransform AsyncPanZoomController::GetCurrentAsyncTransform() {
  ReentrantMonitorAutoEnter lock(mMonitor);

  LayerPoint metricsScrollOffset;
  if (mLastContentPaintMetrics.IsScrollable()) {
    metricsScrollOffset = mLastContentPaintMetrics.GetScrollOffsetInLayerPixels();
  }
  CSSToScreenScale localScale = mFrameMetrics.CalculateResolution();
  LayerPoint translation = mFrameMetrics.GetScrollOffsetInLayerPixels() - metricsScrollOffset;
  return ViewTransform(-translation, localScale / mLastContentPaintMetrics.mDevPixelsPerCSSPixel);
}

void AsyncPanZoomController::NotifyLayersUpdated(const FrameMetrics& aLayerMetrics, bool aIsFirstPaint) {
  ReentrantMonitorAutoEnter lock(mMonitor);

  mLastContentPaintMetrics = aLayerMetrics;

  mFrameMetrics.mMayHaveTouchListeners = aLayerMetrics.mMayHaveTouchListeners;

  // TODO: Once a mechanism for calling UpdateScrollOffset() when content does
  //       a scrollTo() is implemented for B2G (bug 895905), this block can be removed.
#ifndef MOZ_WIDGET_ANDROID
  if (!mPaintThrottler.IsOutstanding()) {
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
      mFrameMetrics.mScrollOffset = aLayerMetrics.mScrollOffset;
      break;
    // Don't clobber if we're in other states.
    default:
      break;
    }
  }
#endif

  mPaintThrottler.TaskComplete(GetFrameTime());
  bool needContentRepaint = false;
  if (aLayerMetrics.mCompositionBounds.width == mFrameMetrics.mCompositionBounds.width &&
      aLayerMetrics.mCompositionBounds.height == mFrameMetrics.mCompositionBounds.height) {
    // Remote content has sync'd up to the composition geometry
    // change, so we can accept the viewport it's calculated.
    CSSToScreenScale previousResolution = mFrameMetrics.CalculateResolution();
    mFrameMetrics.mViewport = aLayerMetrics.mViewport;
    CSSToScreenScale newResolution = mFrameMetrics.CalculateResolution();
    needContentRepaint |= (previousResolution != newResolution);
  }

  if (aIsFirstPaint || mFrameMetrics.IsDefault()) {
    mPaintThrottler.ClearHistory();
    mPaintThrottler.SetMaxDurations(gNumPaintDurationSamples);

    mX.CancelTouch();
    mY.CancelTouch();

    // XXX If this is the very first time we're getting a layers update we need to
    // trigger another repaint, or the B2G browser shows stale content. This needs
    // to be investigated and fixed.
    needContentRepaint |= (mFrameMetrics.IsDefault() && !aLayerMetrics.IsDefault());

    mFrameMetrics = aLayerMetrics;
    mState = NOTHING;
  } else if (!mFrameMetrics.mScrollableRect.IsEqualEdges(aLayerMetrics.mScrollableRect)) {
    mFrameMetrics.mScrollableRect = aLayerMetrics.mScrollableRect;
  }

  if (needContentRepaint) {
    RequestContentRepaint();
  }
}

const FrameMetrics& AsyncPanZoomController::GetFrameMetrics() {
  mMonitor.AssertCurrentThreadIn();
  return mFrameMetrics;
}

void AsyncPanZoomController::UpdateCompositionBounds(const ScreenIntRect& aCompositionBounds) {
  ReentrantMonitorAutoEnter lock(mMonitor);

  ScreenIntRect oldCompositionBounds = mFrameMetrics.mCompositionBounds;
  mFrameMetrics.mCompositionBounds = aCompositionBounds;

  // If the window had 0 dimensions before, or does now, we don't want to
  // repaint or update the zoom since we'll run into rendering issues and/or
  // divide-by-zero. This manifests itself as the screen flashing. If the page
  // has gone out of view, the buffer will be cleared elsewhere anyways.
  if (aCompositionBounds.width && aCompositionBounds.height &&
      oldCompositionBounds.width && oldCompositionBounds.height) {
    SetZoomAndResolution(mFrameMetrics.mZoom);

    // Repaint on a rotation so that our new resolution gets properly updated.
    RequestContentRepaint();
  }
}

void AsyncPanZoomController::CancelDefaultPanZoom() {
  mDisableNextTouchBatch = true;
  nsRefPtr<GestureEventListener> listener = GetGestureEventListener();
  if (listener) {
    listener->CancelGesture();
  }
}

void AsyncPanZoomController::DetectScrollableSubframe() {
  mDelayPanning = true;
}

void AsyncPanZoomController::ZoomToRect(CSSRect aRect) {
  SetState(ANIMATING_ZOOM);

  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    ScreenIntRect compositionBounds = mFrameMetrics.mCompositionBounds;
    CSSRect cssPageRect = mFrameMetrics.mScrollableRect;
    CSSPoint scrollOffset = mFrameMetrics.mScrollOffset;
    float currentZoom = mFrameMetrics.mZoom.scale;
    float targetZoom;
    float intrinsicScale = mFrameMetrics.CalculateIntrinsicScale().scale;

    // The minimum zoom to prevent over-zoom-out.
    // If the zoom factor is lower than this (i.e. we are zoomed more into the page),
    // then the CSS content rect, in layers pixels, will be smaller than the
    // composition bounds. If this happens, we can't fill the target composited
    // area with this frame.
    float localMinZoom = std::max(mMinZoom,
                         std::max(compositionBounds.width / cssPageRect.width,
                                  compositionBounds.height / cssPageRect.height))
                         / intrinsicScale;
    float localMaxZoom = mMaxZoom / intrinsicScale;

    if (!aRect.IsEmpty()) {
      // Intersect the zoom-to-rect to the CSS rect to make sure it fits.
      aRect = aRect.Intersect(cssPageRect);
      float targetResolution =
        std::min(compositionBounds.width / aRect.width,
                 compositionBounds.height / aRect.height);
      targetZoom = targetResolution / intrinsicScale;
    }
    // 1. If the rect is empty, request received from browserElementScrolling.js
    // 2. currentZoom is equal to mMaxZoom and user still double-tapping it
    // 3. currentZoom is equal to localMinZoom and user still double-tapping it
    // Treat these three cases as a request to zoom out as much as possible.
    if (aRect.IsEmpty() ||
        (currentZoom == localMaxZoom && targetZoom >= localMaxZoom) ||
        (currentZoom == localMinZoom && targetZoom <= localMinZoom)) {
      CSSRect compositedRect = mFrameMetrics.CalculateCompositedRectInCssPixels();
      float y = scrollOffset.y;
      float newHeight =
        cssPageRect.width * (compositedRect.height / compositedRect.width);
      float dh = compositedRect.height - newHeight;

      aRect = CSSRect(0.0f,
                           y + dh/2,
                           cssPageRect.width,
                           newHeight);
      aRect = aRect.Intersect(cssPageRect);
      float targetResolution =
        std::min(compositionBounds.width / aRect.width,
                 compositionBounds.height / aRect.height);
      targetZoom = targetResolution / intrinsicScale;
    }

    targetZoom = clamped(targetZoom, localMinZoom, localMaxZoom);
    mEndZoomToMetrics.mZoom = ScreenToScreenScale(targetZoom);

    // Adjust the zoomToRect to a sensible position to prevent overscrolling.
    FrameMetrics metricsAfterZoom = mFrameMetrics;
    metricsAfterZoom.mZoom = mEndZoomToMetrics.mZoom;
    CSSRect rectAfterZoom = metricsAfterZoom.CalculateCompositedRectInCssPixels();

    // If either of these conditions are met, the page will be
    // overscrolled after zoomed
    if (aRect.y + rectAfterZoom.height > cssPageRect.height) {
      aRect.y = cssPageRect.height - rectAfterZoom.height;
      aRect.y = aRect.y > 0 ? aRect.y : 0;
    }
    if (aRect.x + rectAfterZoom.width > cssPageRect.width) {
      aRect.x = cssPageRect.width - rectAfterZoom.width;
      aRect.x = aRect.x > 0 ? aRect.x : 0;
    }

    mStartZoomToMetrics = mFrameMetrics;
    mEndZoomToMetrics.mScrollOffset = aRect.TopLeft();

    mAnimationStartTime = GetFrameTime();

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
      if (!aPreventDefault && mTouchQueue[0].mType == MultiTouchInput::MULTITOUCH_MOVE) {
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

void AsyncPanZoomController::SetState(PanZoomState aNewState) {

  PanZoomState oldState;

  // Intentional scoping for mutex
  {
    ReentrantMonitorAutoEnter lock(mMonitor);
    oldState = mState;
    mState = aNewState;
  }

  if (mGeckoContentController) {
    if (oldState == PANNING && aNewState != PANNING) {
      mGeckoContentController->HandlePanEnd();
    } else if (oldState != PANNING && aNewState == PANNING) {
      mGeckoContentController->HandlePanBegin();
    }
  }
}

void AsyncPanZoomController::TimeoutTouchListeners() {
  mTouchListenerTimeoutTask = nullptr;
  ContentReceivedTouch(false);
}

void AsyncPanZoomController::SetZoomAndResolution(const ScreenToScreenScale& aZoom) {
  mMonitor.AssertCurrentThreadIn();
  mFrameMetrics.mZoom = aZoom;
  CSSToScreenScale resolution = mFrameMetrics.CalculateResolution();
  // We use ScreenToLayerScale(1) below in order to ask gecko to render
  // what's currently visible on the screen. This is effectively turning
  // the async zoom amount into the gecko zoom amount.
  mFrameMetrics.mResolution = resolution / mFrameMetrics.mDevPixelsPerCSSPixel * ScreenToLayerScale(1);
}

void AsyncPanZoomController::UpdateZoomConstraints(bool aAllowZoom,
                                                   float aMinZoom,
                                                   float aMaxZoom) {
  mAllowZoom = aAllowZoom;
  mMinZoom = std::max(MIN_ZOOM, aMinZoom);
  mMaxZoom = std::min(MAX_ZOOM, aMaxZoom);
}

void AsyncPanZoomController::PostDelayedTask(Task* aTask, int aDelayMs) {
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    controller->PostDelayedTask(aTask, aDelayMs);
  }
}

void AsyncPanZoomController::SendAsyncScrollEvent() {
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (!controller) {
    return;
  }

  FrameMetrics::ViewID scrollId;
  CSSRect contentRect;
  CSSSize scrollableSize;
  {
    // XXX bug 890932 - there should be a lock here. but it causes a deadlock.
    scrollId = mFrameMetrics.mScrollId;
    scrollableSize = mFrameMetrics.mScrollableRect.Size();
    contentRect = mFrameMetrics.CalculateCompositedRectInCssPixels();
    contentRect.MoveTo(mCurrentAsyncScrollOffset);
  }

  controller->SendAsyncScrollDOMEvent(scrollId, contentRect, scrollableSize);
}

void AsyncPanZoomController::UpdateScrollOffset(const CSSPoint& aScrollOffset)
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  mFrameMetrics.mScrollOffset = aScrollOffset;
}

bool AsyncPanZoomController::Matches(const ScrollableLayerGuid& aGuid)
{
  // TODO: also check the presShellId and mScrollId, once those are
  // fully propagated everywhere in RenderFrameParent and AndroidJNI.
  return aGuid.mLayersId == mLayersId;
}

}
}
