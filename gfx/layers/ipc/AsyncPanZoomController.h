/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AsyncPanZoomController_h
#define mozilla_layers_AsyncPanZoomController_h

#include "GeckoContentController.h"
#include "mozilla/Attributes.h"
#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "InputData.h"
#include "Axis.h"

namespace mozilla {
namespace layers {

class CompositorParent;
class GestureEventListener;
class ContainerLayer;

/**
 * Controller for all panning and zooming logic. Any time a user input is
 * detected and it must be processed in some way to affect what the user sees,
 * it goes through here. Listens for any input event from InputData and can
 * optionally handle nsGUIEvent-derived touch events, but this must be done on
 * the main thread. Note that this class completely cross-platform.
 *
 * Input events originate on the UI thread of the platform that this runs on,
 * and are then sent to this class. This class processes the event in some way;
 * for example, a touch move will usually lead to a panning of content (though
 * of course there are exceptions, such as if content preventDefaults the event,
 * or if the target frame is not scrollable). The compositor interacts with this
 * class by locking it and querying it for the current transform matrix based on
 * the panning and zooming logic that was invoked on the UI thread.
 *
 * Currently, each outer DOM window (i.e. a website in a tab, but not any
 * subframes) has its own AsyncPanZoomController. In the future, to support
 * asynchronously scrolled subframes, we want to have one AsyncPanZoomController
 * per frame.
 */
class AsyncPanZoomController MOZ_FINAL {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncPanZoomController)

  typedef mozilla::MonitorAutoLock MonitorAutoLock;

public:
  enum GestureBehavior {
    // The platform code is responsible for forwarding gesture events here. We
    // will not attempt to generate gesture events from MultiTouchInputs.
    DEFAULT_GESTURES,
    // An instance of GestureEventListener is used to detect gestures. This is
    // handled completely internally within this class.
    USE_GESTURE_DETECTOR
  };

  AsyncPanZoomController(GeckoContentController* aController,
                         GestureBehavior aGestures = DEFAULT_GESTURES);
  ~AsyncPanZoomController();

  // --------------------------------------------------------------------------
  // These methods must only be called on the controller/UI thread.
  //

  /**
   * General handler for incoming input events. Manipulates the frame metrics
   * basde on what type of input it is. For example, a PinchGestureEvent will
   * cause scaling.
   */

  nsEventStatus HandleInputEvent(const InputData& aEvent);

  /**
   * Special handler for nsInputEvents. Also sets |aOutEvent| (which is assumed
   * to be an already-existing instance of an nsInputEvent which may be an
   * nsTouchEvent) to have its touch points in DOM space. This is so that the
   * touches can be passed through the DOM and content can handle them.
   *
   * NOTE: Be careful of invoking the nsInputEvent variant. This can only be
   * called on the main thread. See widget/InputData.h for more information on
   * why we have InputData and nsInputEvent separated.
   */
  nsEventStatus HandleInputEvent(const nsInputEvent& aEvent,
                                 nsInputEvent* aOutEvent);

  /**
   * Updates the viewport size, i.e. the dimensions of the frame (not
   * necessarily the screen) content will actually be rendered onto in device
   * pixels for example, a subframe will not take the entire screen, but we
   * still want to know how big it is in device pixels. Ideally we want to be
   * using CSS pixels everywhere inside here, but in this case we need to know
   * how large of a displayport to set so we use these dimensions plus some
   * extra.
   *
   * XXX: Use nsIntRect instead.
   */
  void UpdateViewportSize(int aWidth, int aHeight);

  // --------------------------------------------------------------------------
  // These methods must only be called on the compositor thread.
  //

  /**
   * The compositor calls this when it's about to draw pannable/zoomable content
   * and is setting up transforms for compositing the layer tree. This is not
   * idempotent. For example, a fling transform can be applied each time this is
   * called (though not necessarily). |aSampleTime| is the time that this is
   * sampled at; this is used for interpolating animations. Calling this sets a
   * new transform in |aNewTransform| which should be applied directly to the
   * shadow layer of the frame (do not multiply it in as the code already does
   * this internally with |aLayer|'s transform).
   *
   * Return value indicates whether or not any currently running animation
   * should continue. That is, if true, the compositor should schedule another
   * composite.
   */
  bool SampleContentTransformForFrame(const TimeStamp& aSampleTime,
                                      ContainerLayer* aLayer,
                                      gfx3DMatrix* aNewTransform);

  /**
   * A shadow layer update has arrived. |aViewportFrame| is the new FrameMetrics
   * for the top-level frame. |aIsFirstPaint| is a flag passed from the shadow
   * layers code indicating that the frame metrics being sent with this call are
   * the initial metrics and the initial paint of the frame has just happened.
   */
  void NotifyLayersUpdated(const FrameMetrics& aViewportFrame, bool aIsFirstPaint);

  /**
   * The platform implementation must set the compositor parent so that we can
   * request composites.
   */
  void SetCompositorParent(CompositorParent* aCompositorParent);

  // --------------------------------------------------------------------------
  // These methods can be called from any thread.
  //

  /**
   * Sets the CSS page rect, and calculates a new page rect based on the zoom
   * level of the current metrics and the passed in CSS page rect.
   */
  void SetPageRect(const gfx::Rect& aCSSPageRect);

  /**
   * Sets the DPI of the device for use within panning and zooming logic. It is
   * a platform responsibility to set this on initialization of this class and
   * whenever it changes.
   */
  void SetDPI(int aDPI);

protected:
  /**
   * Helper method for touches beginning. Sets everything up for panning and any
   * multitouch gestures.
   */
  nsEventStatus OnTouchStart(const MultiTouchInput& aEvent);

  /**
   * Helper method for touches moving. Does any transforms needed when panning.
   */
  nsEventStatus OnTouchMove(const MultiTouchInput& aEvent);

  /**
   * Helper method for touches ending. Redraws the screen if necessary and does
   * any cleanup after a touch has ended.
   */
  nsEventStatus OnTouchEnd(const MultiTouchInput& aEvent);

  /**
   * Helper method for touches being cancelled. Treated roughly the same as a
   * touch ending (OnTouchEnd()).
   */
  nsEventStatus OnTouchCancel(const MultiTouchInput& aEvent);

  /**
   * Helper method for scales beginning. Distinct from the OnTouch* handlers in
   * that this implies some outside implementation has determined that the user
   * is pinching.
   */
  nsEventStatus OnScaleBegin(const PinchGestureInput& aEvent);

  /**
   * Helper method for scaling. As the user moves their fingers when pinching,
   * this changes the scale of the page.
   */
  nsEventStatus OnScale(const PinchGestureInput& aEvent);

  /**
   * Helper method for scales ending. Redraws the screen if necessary and does
   * any cleanup after a scale has ended.
   */
  nsEventStatus OnScaleEnd(const PinchGestureInput& aEvent);

  /**
   * Helper method for long press gestures.
   *
   * XXX: Implement this.
   */
  nsEventStatus OnLongPress(const TapGestureInput& aEvent);

  /**
   * Helper method for single tap gestures.
   *
   * XXX: Implement this.
   */
  nsEventStatus OnSingleTapUp(const TapGestureInput& aEvent);

  /**
   * Helper method for a single tap confirmed.
   *
   * XXX: Implement this.
   */
  nsEventStatus OnSingleTapConfirmed(const TapGestureInput& aEvent);

  /**
   * Helper method for double taps.
   *
   * XXX: Implement this.
   */
  nsEventStatus OnDoubleTap(const TapGestureInput& aEvent);

  /**
   * Helper method to cancel any gesture currently going to Gecko. Used
   * primarily when a user taps the screen over some clickable content but then
   * pans down instead of letting go (i.e. to cancel a previous touch so that a
   * new one can properly take effect.
   */
  nsEventStatus OnCancelTap(const TapGestureInput& aEvent);

  /**
   * Scrolls the viewport by an X,Y offset.
   */
  void ScrollBy(const nsIntPoint& aOffset);

  /**
   * Scales the viewport by an amount (note that it multiplies this scale in to
   * the current scale, it doesn't set it to |aScale|). Also considers a focus
   * point so that the page zooms outward from that point.
   *
   * XXX: Fix focus point calculations.
   */
  void ScaleWithFocus(float aScale, const nsIntPoint& aFocus);

  /**
   * Schedules a composite on the compositor thread. Wrapper for
   * CompositorParent::ScheduleRenderOnCompositorThread().
   */
  void ScheduleComposite();

  /**
   * Cancels any currently running animation. Note that all this does is set the
   * state of the AsyncPanZoomController back to NOTHING, but it is the
   * animation's responsibility to check this before advancing.
   */
  void CancelAnimation();

  /**
   * Gets the displacement of the current touch since it began. That is, it is
   * the distance between the current position and the initial position of the
   * current touch (this only makes sense if a touch is currently happening and
   * OnTouchMove() is being invoked).
   */
  float PanDistance();

  /**
   * Gets a vector of the velocities of each axis.
   */
  const nsPoint GetVelocityVector();

  /**
   * Gets a reference to the first SingleTouchData from a MultiTouchInput.  This
   * gets only the first one and assumes the rest are either missing or not
   * relevant.
   */
  SingleTouchData& GetFirstSingleTouch(const MultiTouchInput& aEvent);

  /**
   * Sets up anything needed for panning. This may lock one of the axes if the
   * angle of movement is heavily skewed towards it.
   */
  void StartPanning(const MultiTouchInput& aStartPoint);

  /**
   * Wrapper for Axis::UpdateWithTouchAtDevicePoint(). Calls this function for
   * both axes and factors in the time delta from the last update.
   */
  void UpdateWithTouchAtDevicePoint(const MultiTouchInput& aEvent);

  /**
   * Does any panning required due to a new touch event.
   */
  void TrackTouch(const MultiTouchInput& aEvent);

  /**
   * Recalculates the displayport. Ideally, this should paint an area bigger
   * than the actual screen. The viewport refers to the size of the screen,
   * while the displayport is the area actually painted by Gecko. We paint
   * a larger area than the screen so that when you scroll down, you don't
   * checkerboard immediately.
   */
  const nsIntRect CalculatePendingDisplayPort();

  /**
   * Utility function to send updated FrameMetrics to Gecko so that it can paint
   * the displayport area. Calls into GeckoContentController to do the actual
   * work. Note that only one paint request can be active at a time. If a paint
   * request is made while a paint is currently happening, it gets queued up. If
   * a new paint request arrives before a paint is completed, the old request
   * gets discarded.
   */
  void RequestContentRepaint();

  /**
   * Advances a fling by an interpolated amount based on the passed in |aDelta|.
   * This should be called whenever sampling the content transform for this
   * frame. Returns true if the fling animation should be advanced by one frame,
   * or false if there is no fling or the fling has ended.
   */
  bool DoFling(const TimeDuration& aDelta);

  /**
   * Gets the current frame metrics. This is *not* the Gecko copy stored in the
   * layers code.
   */
  const FrameMetrics& GetFrameMetrics();

private:
  enum PanZoomState {
    NOTHING,        /* no touch-start events received */
    FLING,          /* all touches removed, but we're still scrolling page */
    TOUCHING,       /* one touch-start event received */
    PANNING,        /* panning without axis lock */
    PINCHING,       /* nth touch-start, where n > 1. this mode allows pan and zoom */
  };

  enum ContentPainterStatus {
    // A paint may be happening, but it is not due to any action taken by this
    // thread. For example, content could be invalidating itself, but
    // AsyncPanZoomController has nothing to do with that.
    CONTENT_IDLE,
    // Set every time we dispatch a request for a repaint. When a
    // ShadowLayersUpdate arrives and the metrics of this frame have changed, we
    // toggle this off and assume that the paint has completed.
    CONTENT_PAINTING,
    // Set when we have a new displayport in the pipeline that we want to paint.
    // When a ShadowLayersUpdate comes in, we dispatch a new repaint using
    // mFrameMetrics.mDisplayPort (the most recent request) if this is toggled.
    // This is distinct from CONTENT_PAINTING in that it signals that a repaint
    // is happening, whereas this signals that we want to repaint as soon as the
    // previous paint finishes. When the request is eventually made, it will use
    // the most up-to-date metrics.
   CONTENT_PAINTING_AND_PAINT_PENDING
  };

  nsRefPtr<CompositorParent> mCompositorParent;
  nsRefPtr<GeckoContentController> mGeckoContentController;
  nsRefPtr<GestureEventListener> mGestureEventListener;

  // Both |mFrameMetrics| and |mLastContentPaintMetrics| are protected by the
  // monitor. Do not read from or modify either of them without locking.
  FrameMetrics mFrameMetrics;
  // These are the metrics at last content paint, the most recent
  // values we were notified of in NotifyLayersUpdate().
  FrameMetrics mLastContentPaintMetrics;
  // The last metrics that we requested a paint for. These are used to make sure
  // that we're not requesting a paint of the same thing that's already drawn.
  // If we don't do this check, we don't get a ShadowLayersUpdated back.
  FrameMetrics mLastPaintRequestMetrics;

  AxisX mX;
  AxisY mY;

  Monitor mMonitor;

  // The last time the compositor has sampled the content transform for this
  // frame.
  TimeStamp mLastSampleTime;
  // The last time a touch event came through on the UI thread.
  PRInt32 mLastEventTime;

  // Stores the previous focus point if there is a pinch gesture happening. Used
  // to allow panning by moving multiple fingers (thus moving the focus point).
  nsIntPoint mLastZoomFocus;
  PanZoomState mState;
  int mDPI;

  // Stores the current paint status of the frame that we're managing. Repaints
  // may be triggered by other things (like content doing things), in which case
  // this status will not be updated. It is only changed when this class
  // requests a repaint.
  ContentPainterStatus mContentPainterStatus;

  friend class Axis;
};

}
}

#endif // mozilla_layers_PanZoomController_h
