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
#include "InputData.h"
#include "Axis.h"
#include "TaskThrottler.h"

#include "base/message_loop.h"

namespace mozilla {
namespace layers {

class CompositorParent;
class GestureEventListener;
class ContainerLayer;
class ViewTransform;

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
class AsyncPanZoomController {
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

  /**
   * Constant describing the tolerance in distance we use, multiplied by the
   * device DPI, before we start panning the screen. This is to prevent us from
   * accidentally processing taps as touch moves, and from very short/accidental
   * touches moving the screen.
   */
  static float GetTouchStartTolerance();

  AsyncPanZoomController(GeckoContentController* aController,
                         GestureBehavior aGestures = DEFAULT_GESTURES);
  ~AsyncPanZoomController();

  // --------------------------------------------------------------------------
  // These methods must only be called on the gecko thread.
  //

  /**
   * Read the various prefs and do any global initialization for all APZC instances.
   * This must be run on the gecko thread before any APZC instances are actually
   * used for anything meaningful.
   */
  static void InitializeGlobalState();

  // --------------------------------------------------------------------------
  // These methods must only be called on the controller/UI thread.
  //

  /**
   * General handler for incoming input events. Manipulates the frame metrics
   * based on what type of input it is. For example, a PinchGestureEvent will
   * cause scaling. This should only be called externally to this class.
   * HandleInputEvent() should be used internally.
   */
  nsEventStatus ReceiveInputEvent(const InputData& aEvent);

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
  nsEventStatus ReceiveInputEvent(const nsInputEvent& aEvent,
                                  nsInputEvent* aOutEvent);

  /**
   * Updates the composition bounds, i.e. the dimensions of the final size of
   * the frame this is tied to during composition onto, in device pixels. In
   * general, this will just be:
   * { x = 0, y = 0, width = surface.width, height = surface.height }, however
   * there is no hard requirement for this.
   */
  void UpdateCompositionBounds(const ScreenIntRect& aCompositionBounds);

  /**
   * We are scrolling a subframe, so disable our machinery until we hit
   * a touch end or a new touch start. This prevents us from accidentally
   * panning both the subframe and the parent frame.
   *
   * XXX/bug 775452: We should eventually be supporting async scrollable
   * subframes.
   */
  void CancelDefaultPanZoom();

  /**
   * We have found a scrollable subframe, so we need to delay the scrolling
   * gesture executed and let subframe do the scrolling first.
   */
  void DetectScrollableSubframe();

  /**
   * Kicks an animation to zoom to a rect. This may be either a zoom out or zoom
   * in. The actual animation is done on the compositor thread after being set
   * up.
   */
  void ZoomToRect(CSSRect aRect);

  /**
   * If we have touch listeners, this should always be called when we know
   * definitively whether or not content has preventDefaulted any touch events
   * that have come in. If |aPreventDefault| is true, any touch events in the
   * queue will be discarded.
   */
  void ContentReceivedTouch(bool aPreventDefault);

  /**
   * Updates any zoom constraints contained in the <meta name="viewport"> tag.
   * We try to obey everything it asks us elsewhere, but here we only handle
   * minimum-scale, maximum-scale, and user-scalable.
   */
  void UpdateZoomConstraints(bool aAllowZoom, float aMinScale, float aMaxScale);

  /**
   * Schedules a runnable to run on the controller/UI thread at some time
   * in the future.
   */
  void PostDelayedTask(Task* aTask, int aDelayMs);

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
                                      ViewTransform* aNewTransform,
                                      ScreenPoint& aScrollOffset);

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
   * Shut down the controller/UI thread state and prepare to be
   * deleted (which may happen from any thread).
   */
  void Destroy();

  /**
   * Sets the DPI of the device for use within panning and zooming logic. It is
   * a platform responsibility to set this on initialization of this class and
   * whenever it changes.
   */
  void SetDPI(int aDPI);

  /**
   * Gets the DPI of the device for use outside the panning and zooming logic.
   * It defaults to 72 if not set using SetDPI() at any point.
   */
  int GetDPI();

  /**
   * Recalculates the displayport. Ideally, this should paint an area bigger
   * than the composite-to dimensions so that when you scroll down, you don't
   * checkerboard immediately. This includes a bunch of logic, including
   * algorithms to bias painting in the direction of the velocity.
   */
  static const CSSRect CalculatePendingDisplayPort(
    const FrameMetrics& aFrameMetrics,
    const gfx::Point& aVelocity,
    const gfx::Point& aAcceleration,
    double aEstimatedPaintDuration);

  /**
   * Send an mozbrowserasyncscroll event.
   * *** The monitor must be held while calling this.
   */
  void SendAsyncScrollEvent();

  /**
   * Handler for events which should not be intercepted by the touch listener.
   * Does the work for ReceiveInputEvent().
   */
  nsEventStatus HandleInputEvent(const InputData& aEvent);

  /**
   * Sync panning and zooming animation using a fixed frame time.
   * This will ensure that we animate the APZC correctly with other external
   * animations to the same timestamp.
   */
  static void SetFrameTime(const TimeStamp& aMilliseconds);

  /**
   * Transform and intersect aPoint with the layer tree returning the appropriate
   * AsyncPanZoomController for this point.
   * aRelativePointOut Return the point transformed into the layer coordinates
   * relative to the scroll origin for this layer.
   */
  static void GetAPZCAtPoint(const ContainerLayer& aLayerTree,
                             const ScreenIntPoint& aPoint,
                             AsyncPanZoomController** aApzcOut,
                             LayerIntPoint* aRelativePointOut);

  /**
   * Update mFrameMetrics.mScrollOffset to the given offset.
   * This is necessary in cases where a scroll is not caused by user
   * input (for example, a content scrollTo()).
   */
  void UpdateScrollOffset(const CSSPoint& aScrollOffset);

  /**
   * Cancels any currently running animation. Note that all this does is set the
   * state of the AsyncPanZoomController back to NOTHING, but it is the
   * animation's responsibility to check this before advancing.
   */
  void CancelAnimation();

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
  void ScrollBy(const CSSPoint& aOffset);

  /**
   * Scales the viewport by an amount (note that it multiplies this scale in to
   * the current scale, it doesn't set it to |aScale|). Also considers a focus
   * point so that the page zooms outward from that point.
   *
   * XXX: Fix focus point calculations.
   */
  void ScaleWithFocus(float aScale, const ScreenPoint& aFocus);

  /**
   * Schedules a composite on the compositor thread. Wrapper for
   * CompositorParent::ScheduleRenderOnCompositorThread().
   */
  void ScheduleComposite();

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
  const gfx::Point GetVelocityVector();

  /**
   * Gets a vector of the acceleration factors of each axis.
   */
  const gfx::Point GetAccelerationVector();

  /**
   * Gets a reference to the first SingleTouchData from a MultiTouchInput.  This
   * gets only the first one and assumes the rest are either missing or not
   * relevant.
   */
  SingleTouchData& GetFirstSingleTouch(const MultiTouchInput& aEvent);

  /**
   * Sets up anything needed for panning. This takes us out of the "TOUCHING"
   * state and starts actually panning us.
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
   * Attempts to enlarge the displayport along a single axis. Returns whether or
   * not the displayport was enlarged. This will fail in circumstances where the
   * velocity along that axis is not high enough to need any changes. The
   * displayport metrics are expected to be passed into |aDisplayPortOffset| and
   * |aDisplayPortLength|. If enlarged, these will be updated with the new
   * metrics.
   */
  static bool EnlargeDisplayPortAlongAxis(float aSkateSizeMultiplier,
                                          double aEstimatedPaintDuration,
                                          float aCompositionBounds,
                                          float aVelocity,
                                          float aAcceleration,
                                          float* aDisplayPortOffset,
                                          float* aDisplayPortLength);

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

  /**
   * Timeout function for touch listeners. This should be called on a timer
   * after we get our first touch event in a batch, under the condition that we
   * have touch listeners. If a notification comes indicating whether or not
   * content preventDefaulted a series of touch events before the timeout, the
   * timeout should be cancelled.
   */
  void TimeoutTouchListeners();

  /**
   * Utility function that sets the zoom and resolution simultaneously. This is
   * useful when we want to repaint at the current zoom level.
   *
   * *** The monitor must be held while calling this.
   */
  void SetZoomAndResolution(const ScreenToScreenScale& aZoom);

  /**
   * Timeout function for mozbrowserasyncscroll event. Because we throttle
   * mozbrowserasyncscroll events in some conditions, this function ensures
   * that the last mozbrowserasyncscroll event will be fired after a period of
   * time.
   */
  void FireAsyncScrollOnTimeout();

private:
  enum PanZoomState {
    NOTHING,        /* no touch-start events received */
    FLING,          /* all touches removed, but we're still scrolling page */
    TOUCHING,       /* one touch-start event received */
    PANNING,        /* panning the frame */
    PINCHING,       /* nth touch-start, where n > 1. this mode allows pan and zoom */
    ANIMATING_ZOOM, /* animated zoom to a new rect */
    WAITING_LISTENERS, /* a state halfway between NOTHING and TOUCHING - the user has
                    put a finger down, but we don't yet know if a touch listener has
                    prevented the default actions yet. we still need to abort animations. */
  };

  /**
   * Helper to set the current state. Holds the monitor before actually setting
   * it. If the monitor is already held by the current thread, it is safe to
   * instead use: |mState = NEWSTATE;|
   */
  void SetState(PanZoomState aState);

  nsRefPtr<CompositorParent> mCompositorParent;
  TaskThrottler mPaintThrottler;

  /* Access to the following two fields is protected by the mRefPtrMonitor,
     since they are accessed on the UI thread but can be cleared on the
     compositor thread. */
  nsRefPtr<GeckoContentController> mGeckoContentController;
  nsRefPtr<GestureEventListener> mGestureEventListener;
  Monitor mRefPtrMonitor;

  /* Utility functions that return a addrefed pointer to the corresponding fields. */
  already_AddRefed<GeckoContentController> GetGeckoContentController();
  already_AddRefed<GestureEventListener> GetGestureEventListener();

protected:
  // Both |mFrameMetrics| and |mLastContentPaintMetrics| are protected by the
  // monitor. Do not read from or modify either of them without locking.
  FrameMetrics mFrameMetrics;

  // Protects |mFrameMetrics|, |mLastContentPaintMetrics|, |mState| and
  // |mMetaViewportInfo|. Before manipulating |mFrameMetrics| or
  // |mLastContentPaintMetrics|, the monitor should be held. When setting
  // |mState|, either the SetState() function can be used, or the monitor can be
  // held and then |mState| updated.  |mMetaViewportInfo| should be updated
  // using UpdateMetaViewport().
  Monitor mMonitor;

private:
  // These are the metrics at last content paint, the most recent
  // values we were notified of in NotifyLayersUpdate().
  FrameMetrics mLastContentPaintMetrics;
  // The last metrics that we requested a paint for. These are used to make sure
  // that we're not requesting a paint of the same thing that's already drawn.
  // If we don't do this check, we don't get a ShadowLayersUpdated back.
  FrameMetrics mLastPaintRequestMetrics;

  // Old metrics from before we started a zoom animation. This is only valid
  // when we are in the "ANIMATED_ZOOM" state. This is used so that we can
  // interpolate between the start and end frames. We only use the
  // |mViewportScrollOffset| and |mResolution| fields on this.
  FrameMetrics mStartZoomToMetrics;
  // Target metrics for a zoom to animation. This is only valid when we are in
  // the "ANIMATED_ZOOM" state. We only use the |mViewportScrollOffset| and
  // |mResolution| fields on this.
  FrameMetrics mEndZoomToMetrics;

  nsTArray<MultiTouchInput> mTouchQueue;

  CancelableTask* mTouchListenerTimeoutTask;

  AxisX mX;
  AxisY mY;

  // Most up-to-date constraints on zooming. These should always be reasonable
  // values; for example, allowing a min zoom of 0.0 can cause very bad things
  // to happen.
  bool mAllowZoom;
  float mMinZoom;
  float mMaxZoom;

  // The last time the compositor has sampled the content transform for this
  // frame.
  TimeStamp mLastSampleTime;
  // The last time a touch event came through on the UI thread.
  uint32_t mLastEventTime;

  // Start time of an animation. This is used for a zoom to animation to mark
  // the beginning.
  TimeStamp mAnimationStartTime;

  // Stores the previous focus point if there is a pinch gesture happening. Used
  // to allow panning by moving multiple fingers (thus moving the focus point).
  ScreenPoint mLastZoomFocus;

  // Stores the state of panning and zooming this frame. This is protected by
  // |mMonitor|; that is, it should be held whenever this is updated.
  PanZoomState mState;

  // The last time and offset we fire the mozbrowserasyncscroll event when
  // compositor has sampled the content transform for this frame.
  TimeStamp mLastAsyncScrollTime;
  CSSPoint mLastAsyncScrollOffset;

  // The current offset drawn on the screen, it may not be sent since we have
  // throttling policy for mozbrowserasyncscroll event.
  CSSPoint mCurrentAsyncScrollOffset;

  // The delay task triggered by the throttling mozbrowserasyncscroll event
  // ensures the last mozbrowserasyncscroll event is always been fired.
  CancelableTask* mAsyncScrollTimeoutTask;

  int mDPI;

  // Flag used to determine whether or not we should disable handling of the
  // next batch of touch events. This is used for sync scrolling of subframes.
  bool mDisableNextTouchBatch;

  // Flag used to determine whether or not we should try to enter the
  // WAITING_LISTENERS state. This is used in the case that we are processing a
  // queued up event block. If set, this means that we are handling this queue
  // and we don't want to queue the events back up again.
  bool mHandlingTouchQueue;

  // Flag used to determine whether or not we should try scrolling by
  // BrowserElementScrolling first.  If set, we delay delivering
  // touchmove events to GestureListener until BrowserElementScrolling
  // decides whether it wants to handle panning for this touch series.
  bool mDelayPanning;

  friend class Axis;
};

}
}

#endif // mozilla_layers_PanZoomController_h
