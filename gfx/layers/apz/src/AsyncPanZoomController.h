/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AsyncPanZoomController_h
#define mozilla_layers_AsyncPanZoomController_h

#include "CrossProcessMutex.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Monitor.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Atomics.h"
#include "InputData.h"
#include "Axis.h"
#include "TaskThrottler.h"
#include "gfx3DMatrix.h"

#include "base/message_loop.h"

namespace mozilla {

namespace ipc {

class SharedMemoryBasic;

}

namespace layers {

struct ScrollableLayerGuid;
class CompositorParent;
class GestureEventListener;
class ContainerLayer;
class PCompositorParent;
class ViewTransform;
class APZCTreeManager;
class AsyncPanZoomAnimation;
class FlingAnimation;

/**
 * Controller for all panning and zooming logic. Any time a user input is
 * detected and it must be processed in some way to affect what the user sees,
 * it goes through here. Listens for any input event from InputData and can
 * optionally handle WidgetGUIEvent-derived touch events, but this must be done
 * on the main thread. Note that this class completely cross-platform.
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
  typedef uint32_t TouchBehaviorFlags;

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

  AsyncPanZoomController(uint64_t aLayersId,
                         APZCTreeManager* aTreeManager,
                         GeckoContentController* aController,
                         GestureBehavior aGestures = DEFAULT_GESTURES);

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
   */
  void UpdateZoomConstraints(const ZoomConstraints& aConstraints);

  /**
   * Return the zoom constraints last set for this APZC (in the constructor
   * or in UpdateZoomConstraints()).
   */
  ZoomConstraints GetZoomConstraints() const;

  /**
   * Schedules a runnable to run on the controller/UI thread at some time
   * in the future.
   */
  void PostDelayedTask(Task* aTask, int aDelayMs);

  // --------------------------------------------------------------------------
  // These methods must only be called on the compositor thread.
  //

  bool UpdateAnimation(const TimeStamp& aSampleTime,
                       Vector<Task*>* aOutDeferredTasks);

  /**
   * The compositor calls this when it's about to draw pannable/zoomable content
   * and is setting up transforms for compositing the layer tree. This is not
   * idempotent. For example, a fling transform can be applied each time this is
   * called (though not necessarily). |aSampleTime| is the time that this is
   * sampled at; this is used for interpolating animations. Calling this sets a
   * new transform in |aNewTransform| which should be multiplied to the transform
   * in the shadow layer corresponding to this APZC.
   *
   * Return value indicates whether or not any currently running animation
   * should continue. That is, if true, the compositor should schedule another
   * composite.
   */
  bool SampleContentTransformForFrame(const TimeStamp& aSampleTime,
                                      ViewTransform* aNewTransform,
                                      ScreenPoint& aScrollOffset);

  /**
   * A shadow layer update has arrived. |aLayerMetrics| is the new FrameMetrics
   * for the container layer corresponding to this APZC.
   * |aIsFirstPaint| is a flag passed from the shadow
   * layers code indicating that the frame metrics being sent with this call are
   * the initial metrics and the initial paint of the frame has just happened.
   */
  void NotifyLayersUpdated(const FrameMetrics& aLayerMetrics, bool aIsFirstPaint);

  /**
   * The platform implementation must set the compositor parent so that we can
   * request composites.
   */
  void SetCompositorParent(CompositorParent* aCompositorParent);

  /**
   * Inform this APZC that it will be sharing its FrameMetrics with a cross-process
   * compositor so that the associated content process can access it. This is only
   * relevant when progressive painting is enabled.
   */
  void ShareFrameMetricsAcrossProcesses();

  // --------------------------------------------------------------------------
  // These methods can be called from any thread.
  //

  /**
   * Shut down the controller/UI thread state and prepare to be
   * deleted (which may happen from any thread).
   */
  void Destroy();

  /**
   * Returns true if Destroy() has already been called on this APZC instance.
   */
  bool IsDestroyed();

  /**
   * Returns the incremental transformation corresponding to the async pan/zoom
   * in progress. That is, when this transform is multiplied with the layer's
   * existing transform, it will make the layer appear with the desired pan/zoom
   * amount.
   */
  ViewTransform GetCurrentAsyncTransform();

  /**
   * Returns the part of the async transform that will remain once Gecko does a
   * repaint at the desired metrics. That is, in the steady state:
   * gfx3DMatrix(GetCurrentAsyncTransform()) === GetNontransientAsyncTransform()
   */
  gfx3DMatrix GetNontransientAsyncTransform();

  /**
   * Returns the transform to take something from the coordinate space of the
   * last thing we know gecko painted, to the coordinate space of the last thing
   * we asked gecko to paint. In cases where that last request has not yet been
   * processed, this is needed to transform input events properly into a space
   * gecko will understand.
   */
  gfx3DMatrix GetTransformToLastDispatchedPaint();

  /**
   * Recalculates the displayport. Ideally, this should paint an area bigger
   * than the composite-to dimensions so that when you scroll down, you don't
   * checkerboard immediately. This includes a bunch of logic, including
   * algorithms to bias painting in the direction of the velocity.
   */
  static const LayerMargin CalculatePendingDisplayPort(
    const FrameMetrics& aFrameMetrics,
    const ScreenPoint& aVelocity,
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
   * Handler for gesture events.
   * Currently some gestures are detected in GestureEventListener that calls
   * APZC back through this handler in order to avoid recursive calls to
   * APZC::HandleInputEvent() which is supposed to do the work for
   * ReceiveInputEvent().
   */
  nsEventStatus HandleGestureEvent(const InputData& aEvent);

  /**
   * Populates the provided object (if non-null) with the scrollable guid of this apzc.
   */
  void GetGuid(ScrollableLayerGuid* aGuidOut) const;

  /**
   * Returns the scrollable guid of this apzc.
   */
  ScrollableLayerGuid GetGuid() const;

  /**
   * Returns true if this APZC instance is for the layer identified by the guid.
   */
  bool Matches(const ScrollableLayerGuid& aGuid);

  void StartAnimation(AsyncPanZoomAnimation* aAnimation);

  /**
   * Cancels any currently running animation. Note that all this does is set the
   * state of the AsyncPanZoomController back to NOTHING, but it is the
   * animation's responsibility to check this before advancing.
   */
  void CancelAnimation();

  /**
   * Returns allowed touch behavior for the given point on the scrollable layer.
   * Internally performs a kind of hit testing based on the regions constructed
   * on the main thread and attached to the current scrollable layer. Each of such regions
   * contains info about allowed touch behavior. If regions info isn't enough it returns
   * UNKNOWN value and we should switch to the fallback approach - asking content.
   * TODO: for now it's only a stub and returns hardcoded magic value. As soon as bug 928833
   * is done we should integrate its logic here.
   */
  TouchBehaviorFlags GetAllowedTouchBehavior(ScreenIntPoint& aPoint);

  /**
   * Sets allowed touch behavior for current touch session.
   * This method is invoked by the APZCTreeManager which in its turn invoked by
   * the widget after performing touch-action values retrieving.
   * Must be called after receiving the TOUCH_START even that started the
   * touch session.
   */
  void SetAllowedTouchBehavior(const nsTArray<TouchBehaviorFlags>& aBehaviors);

  /**
   * Returns whether this APZC is for an element marked with the 'scrollgrab'
   * attribute.
   */
  bool HasScrollgrab() const { return mFrameMetrics.mHasScrollgrab; }

  /**
   * Returns whether this APZC has room to be panned (in any direction).
   */
  bool IsPannable() const;

protected:
  // Protected destructor, to discourage deletion outside of Release():
  ~AsyncPanZoomController();

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
   * Helper methods for long press gestures.
   */
  nsEventStatus OnLongPress(const TapGestureInput& aEvent);
  nsEventStatus OnLongPressUp(const TapGestureInput& aEvent);

  /**
   * Helper method for single tap gestures.
   */
  nsEventStatus OnSingleTapUp(const TapGestureInput& aEvent);

  /**
   * Helper method for a single tap confirmed.
   */
  nsEventStatus OnSingleTapConfirmed(const TapGestureInput& aEvent);

  /**
   * Helper method for double taps.
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
   * point so that the page zooms inward/outward from that point.
   */
  void ScaleWithFocus(float aScale,
                      const CSSPoint& aFocus);

  /**
   * Schedules a composite on the compositor thread. Wrapper for
   * CompositorParent::ScheduleRenderOnCompositorThread().
   */
  void ScheduleComposite();

  /**
   * Schedules a composite, and if enough time has elapsed since the last
   * paint, a paint.
   */
  void ScheduleCompositeAndMaybeRepaint();

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
  const ScreenPoint GetVelocityVector();

  /**
   * Gets a reference to the first touch point from a MultiTouchInput.  This
   * gets only the first one and assumes the rest are either missing or not
   * relevant.
   */
  ScreenIntPoint& GetFirstTouchScreenPoint(const MultiTouchInput& aEvent);

  /**
   * Sets the panning state basing on the pan direction angle and current touch-action value.
   */
  void HandlePanningWithTouchAction(double angle, TouchBehaviorFlags value);

  /**
   * Sets the panning state ignoring the touch action value.
   */
  void HandlePanning(double angle);

  /**
   * Sets up anything needed for panning. This takes us out of the "TOUCHING"
   * state and starts actually panning us.
   */
  nsEventStatus StartPanning(const MultiTouchInput& aStartPoint);

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
   * Utility function to send updated FrameMetrics to Gecko so that it can paint
   * the displayport area. Calls into GeckoContentController to do the actual
   * work. Note that only one paint request can be active at a time. If a paint
   * request is made while a paint is currently happening, it gets queued up. If
   * a new paint request arrives before a paint is completed, the old request
   * gets discarded.
   */
  void RequestContentRepaint();

  /**
   * Tell the paint throttler to request a content repaint with the given
   * metrics.  (Helper function used by RequestContentRepaint.)
   */
  void RequestContentRepaint(FrameMetrics& aFrameMetrics);

  /**
   * Actually send the next pending paint request to gecko.
   */
  void DispatchRepaintRequest(const FrameMetrics& aFrameMetrics);

  /**
   * Gets the current frame metrics. This is *not* the Gecko copy stored in the
   * layers code.
   */
  const FrameMetrics& GetFrameMetrics() const;

  /**
   * Sets the timer for content response to a series of touch events, if it
   * hasn't been already. This is to prevent us from batching up touch events
   * indefinitely in the case that content doesn't respond with whether or not
   * it wants to preventDefault. When the timer is fired, the touch event queue
   * will be flushed.
   */
  void SetContentResponseTimer();

  /**
   * Timeout function for content response. This should be called on a timer
   * after we get our first touch event in a batch, under the condition that we
   * waiting for response from content. If a notification comes indicating whether or not
   * content preventDefaulted a series of touch events and touch behavior values are
   * set before the timeout, the timeout should be cancelled.
   */
  void TimeoutContentResponse();

  /**
   * Timeout function for mozbrowserasyncscroll event. Because we throttle
   * mozbrowserasyncscroll events in some conditions, this function ensures
   * that the last mozbrowserasyncscroll event will be fired after a period of
   * time.
   */
  void FireAsyncScrollOnTimeout();

private:
  enum PanZoomState {
    NOTHING,                  /* no touch-start events received */
    FLING,                    /* all touches removed, but we're still scrolling page */
    TOUCHING,                 /* one touch-start event received */

    PANNING,                  /* panning the frame */
    PANNING_LOCKED_X,         /* touch-start followed by move (i.e. panning with axis lock) X axis */
    PANNING_LOCKED_Y,         /* as above for Y axis */

    CROSS_SLIDING_X,          /* Panning disabled while user does a horizontal gesture
                                 on a vertically-scrollable view. This used for the
                                 Windows Metro "cross-slide" gesture. */
    CROSS_SLIDING_Y,          /* as above for Y axis */

    PINCHING,                 /* nth touch-start, where n > 1. this mode allows pan and zoom */
    ANIMATING_ZOOM,           /* animated zoom to a new rect */
    WAITING_CONTENT_RESPONSE, /* a state halfway between NOTHING and TOUCHING - the user has
                                 put a finger down, but we don't yet know if a touch listener has
                                 prevented the default actions yet and the allowed touch behavior
                                 was not set yet. we still need to abort animations. */
    SNAP_BACK,                /* snap-back animation to relieve overscroll */
  };

  // State related to a single touch block. Does not persist across touch blocks.
  struct TouchBlockState {

    TouchBlockState()
      :  mAllowedTouchBehaviorSet(false),
         mPreventDefault(false),
         mPreventDefaultSet(false),
         mSingleTapOccurred(false)
    {}

    // Values of allowed touch behavior for touch points of this touch block.
    // Since there are maybe a few current active touch points per time (multitouch case)
    // and each touch point should have its own value of allowed touch behavior- we're
    // keeping an array of allowed touch behavior values, not the single value.
    nsTArray<TouchBehaviorFlags> mAllowedTouchBehaviors;

    // Specifies whether mAllowedTouchBehaviors is set for this touch events block.
    bool mAllowedTouchBehaviorSet;

    // Flag used to specify that content prevented the default behavior of this
    // touch events block.
    bool mPreventDefault;

    // Specifies whether mPreventDefault property is set for this touch events block.
    bool mPreventDefaultSet;

    // Specifies whether a single tap event was generated during this touch block.
    bool mSingleTapOccurred;
  };

  /*
   * Returns whether current touch behavior values allow pinch-zooming.
   */
  bool TouchActionAllowPinchZoom();

  /*
   * Returns whether current touch behavior values allow double-tap-zooming.
   */
  bool TouchActionAllowDoubleTapZoom();

  /*
   * Returns allowed touch behavior from the mAllowedTouchBehavior array.
   * In case apzc didn't receive touch behavior values within the timeout
   * it returns default value.
   */
  TouchBehaviorFlags GetTouchBehavior(uint32_t touchIndex);

  /**
   * To move from the WAITING_CONTENT_RESPONSE state to TOUCHING one we need two
   * conditions set: get content listeners response (whether they called preventDefault)
   * and get allowed touch behaviors.
   * This method checks both conditions and changes (or not changes) state
   * appropriately.
   */
  void CheckContentResponse();

  /**
   * Helper to set the current state. Holds the monitor before actually setting
   * it and fires content controller events based on state changes. Always set
   * the state using this call, do not set it directly.
   */
  void SetState(PanZoomState aState);

  /**
   * Convert ScreenPoint relative to this APZC to CSSPoint relative
   * to the parent document. This excludes the transient compositor transform.
   * NOTE: This must be converted to CSSPoint relative to the child
   * document before sending over IPC.
   */
  bool ConvertToGecko(const ScreenPoint& aPoint, CSSPoint* aOut);

  /**
   * Internal helpers for checking general state of this apzc.
   */
  bool IsTransformingState(PanZoomState aState);
  bool IsPanningState(PanZoomState mState);

  enum AxisLockMode {
    FREE,     /* No locking at all */
    STANDARD, /* Default axis locking mode that remains locked until pan ends*/
    STICKY,   /* Allow lock to be broken, with hysteresis */
  };

  static AxisLockMode GetAxisLockMode();

  // Convert a point from local screen coordinates to parent layer coordinates.
  // This is a common operation as inputs from the tree manager are in screen
  // coordinates but the composition bounds is in parent layer coordinates.
  ParentLayerPoint ToParentLayerCoords(const ScreenPoint& aPoint);

  // Update mFrameMetrics.mTransformScale. This should be called whenever
  // our CSS transform or the non-transient part of our async transform
  // changes, as it corresponds to the scale portion of those transforms.
  void UpdateTransformScale();

  // Helper function for OnSingleTapUp() and OnSingleTapConfirmed().
  nsEventStatus GenerateSingleTap(const ScreenIntPoint& aPoint, mozilla::Modifiers aModifiers);

  // Common processing at the end of a touch block.
  void OnTouchEndOrCancel();

  uint64_t mLayersId;
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

  // If we are sharing our frame metrics with content across processes
  bool mSharingFrameMetricsAcrossProcesses;
  /* Utility function to get the Compositor with which we share the FrameMetrics.
     This function is only callable from the compositor thread. */
  PCompositorParent* GetSharedFrameMetricsCompositor();

protected:
  // Both |mFrameMetrics| and |mLastContentPaintMetrics| are protected by the
  // monitor. Do not read from or modify either of them without locking.
  FrameMetrics mFrameMetrics;

  // Protects |mFrameMetrics|, |mLastContentPaintMetrics|, and |mState|.
  // Before manipulating |mFrameMetrics| or |mLastContentPaintMetrics|, the
  // monitor should be held. When setting |mState|, either the SetState()
  // function can be used, or the monitor can be held and then |mState| updated.
  // IMPORTANT: See the note about lock ordering at the top of APZCTreeManager.h.
  // This is mutable to allow entering it from 'const' methods; doing otherwise
  // would significantly limit what methods could be 'const'.
  mutable ReentrantMonitor mMonitor;

  // Specifies whether we should use touch-action css property. Initialized from
  // the preferences. This property (in comparison with the global one) simplifies
  // testing apzc with (and without) touch-action property enabled concurrently
  // (e.g. with the gtest framework).
  bool mTouchActionPropertyEnabled;

private:
  // Metrics of the container layer corresponding to this APZC. This is
  // stored here so that it is accessible from the UI/controller thread.
  // These are the metrics at last content paint, the most recent
  // values we were notified of in NotifyLayersUpdate(). Since it represents
  // the Gecko state, it should be used as a basis for untransformation when
  // sending messages back to Gecko.
  FrameMetrics mLastContentPaintMetrics;
  // The last metrics that we requested a paint for. These are used to make sure
  // that we're not requesting a paint of the same thing that's already drawn.
  // If we don't do this check, we don't get a ShadowLayersUpdated back.
  FrameMetrics mLastPaintRequestMetrics;
  // The last metrics that we actually sent to Gecko. This allows us to transform
  // inputs into a coordinate space that Gecko knows about. This assumes the pipe
  // through which input events and repaint requests are sent to Gecko operates
  // in a FIFO manner.
  FrameMetrics mLastDispatchedPaintMetrics;

  nsTArray<MultiTouchInput> mTouchQueue;

  CancelableTask* mContentResponseTimeoutTask;

  AxisX mX;
  AxisY mY;

  // This flag is set to true when we are in a axis-locked pan as a result of
  // the touch-action CSS property.
  bool mPanDirRestricted;

  // Most up-to-date constraints on zooming. These should always be reasonable
  // values; for example, allowing a min zoom of 0.0 can cause very bad things
  // to happen.
  ZoomConstraints mZoomConstraints;

  // The last time the compositor has sampled the content transform for this
  // frame.
  TimeStamp mLastSampleTime;
  // The last time a touch event came through on the UI thread.
  uint32_t mLastEventTime;

  // Stores the previous focus point if there is a pinch gesture happening. Used
  // to allow panning by moving multiple fingers (thus moving the focus point).
  ParentLayerPoint mLastZoomFocus;

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

  // Flag used to determine whether or not we should try to enter the
  // WAITING_LISTENERS state. This is used in the case that we are processing a
  // queued up event block. If set, this means that we are handling this queue
  // and we don't want to queue the events back up again.
  bool mHandlingTouchQueue;

  // Stores information about the current touch block.
  TouchBlockState mTouchBlockState;

  nsRefPtr<AsyncPanZoomAnimation> mAnimation;

  friend class Axis;


  /* ===================================================================
   * The functions and members in this section are used to manage
   * fling animations and handling overscroll during a fling.
   */
public:
  /**
   * Take over a fling with the given velocity from another APZC. Used for
   * during overscroll handoff for a fling. If we are not pannable, calls
   * mTreeManager->HandOffFling() to hand the fling off further.
   * Returns true iff. any APZC (whether this one or one further in the handoff
   * chain accepted the fling).
   */
  bool TakeOverFling(ScreenPoint aVelocity);

private:
  friend class FlingAnimation;
  friend class OverscrollSnapBackAnimation;
  // The initial velocity of the most recent fling.
  ScreenPoint mLastFlingVelocity;
  // The time at which the most recent fling started.
  TimeStamp mLastFlingTime;

  // Deal with overscroll resulting from a fling animation. This is only ever
  // called on APZC instances that were actually performing a fling.
  // The overscroll is handled by trying to hand the fling off to an APZC
  // later in the handoff chain, or if there are no takers, continuing the
  // fling and entering an overscrolled state.
  void HandleFlingOverscroll(const ScreenPoint& aVelocity);

  // Helper function used by TakeOverFling() and HandleFlingOverscroll().
  void AcceptFling(const ScreenPoint& aVelocity, bool aAllowOverscroll);

  // Start a snap-back animation to relieve overscroll.
  void StartSnapBack();


  /* ===================================================================
   * The functions and members in this section are used to build a tree
   * structure out of APZC instances. This tree can only be walked or
   * manipulated while holding the lock in the associated APZCTreeManager
   * instance.
   */
public:
  void SetLastChild(AsyncPanZoomController* child) {
    mLastChild = child;
    if (child) {
      child->mParent = this;
    }
  }

  void SetPrevSibling(AsyncPanZoomController* sibling) {
    mPrevSibling = sibling;
    if (sibling) {
      sibling->mParent = mParent;
    }
  }

  // Make this APZC the root of the APZC tree. Clears the parent pointer.
  void MakeRoot() {
    mParent = nullptr;
  }

  AsyncPanZoomController* GetLastChild() const { return mLastChild; }
  AsyncPanZoomController* GetPrevSibling() const { return mPrevSibling; }
  AsyncPanZoomController* GetParent() const { return mParent; }

  /* Returns true if there is no APZC higher in the tree with the same
   * layers id.
   */
  bool IsRootForLayersId() const {
    return !mParent || (mParent->mLayersId != mLayersId);
  }

private:
  // This is a raw pointer to avoid introducing a reference cycle between
  // AsyncPanZoomController and APZCTreeManager. Since these objects don't
  // live on the main thread, we can't use the cycle collector with them.
  // The APZCTreeManager owns the lifetime of the APZCs, so nulling this
  // pointer out in Destroy() will prevent accessing deleted memory.
  Atomic<APZCTreeManager*> mTreeManager;

  nsRefPtr<AsyncPanZoomController> mLastChild;
  nsRefPtr<AsyncPanZoomController> mPrevSibling;
  nsRefPtr<AsyncPanZoomController> mParent;


  /* ===================================================================
   * The functions and members in this section are used for scrolling,
   * including handing off scroll to another APZC, and overscrolling.
   */
public:
  void SetScrollHandoffParentId(FrameMetrics::ViewID aScrollParentId) {
    mScrollParentId = aScrollParentId;
  }

  FrameMetrics::ViewID GetScrollHandoffParentId() const {
    return mScrollParentId;
  }

  /**
   * Attempt to scroll in response to a touch-move from |aStartPoint| to
   * |aEndPoint|, which are in our (transformed) screen coordinates.
   * Due to overscroll handling, there may not actually have been a touch-move
   * at these points, but this function will scroll as if there had been.
   * If this attempt causes overscroll (i.e. the layer cannot be scrolled
   * by the entire amount requested), the overscroll is passed back to the
   * tree manager via APZCTreeManager::DispatchScroll(). If the tree manager
   * does not find an APZC further in the handoff chain to accept the
   * overscroll, and this APZC is pannable, this APZC enters an overscrolled
   * state.
   * |aOverscrollHandoffChainIndex| is used by the tree manager to keep track
   * of which APZC to hand off the overscroll to; this function increments it
   * and passes it on to APZCTreeManager::DispatchScroll() in the event of
   * overscroll.
   * Returns true iff. this APZC, or an APZC further down the
   * handoff chain, accepted the scroll (possibly entering an overscrolled
   * state). If this return false, the caller APZC knows that it should enter
   * an overscrolled state itself if it can.
   */
  bool AttemptScroll(const ScreenPoint& aStartPoint, const ScreenPoint& aEndPoint,
                     uint32_t aOverscrollHandoffChainIndex = 0);

  void FlushRepaintForOverscrollHandoff();

private:
  FrameMetrics::ViewID mScrollParentId;

  /**
   * A helper function for calling APZCTreeManager::DispatchScroll().
   * Guards against the case where the APZC is being concurrently destroyed
   * (and thus mTreeManager is being nulled out).
   */
  bool CallDispatchScroll(const ScreenPoint& aStartPoint, const ScreenPoint& aEndPoint,
                          uint32_t aOverscrollHandoffChainIndex);

  /**
   * Try to overscroll by 'aOverscroll'.
   * If we are pannable, 'aOverscroll' is added to any existing overscroll,
   * and the function returns true.
   * Otherwise, nothing happens and the function return false.
   */
  bool OverscrollBy(const CSSPoint& aOverscroll);


  /* ===================================================================
   * The functions and members in this section are used to maintain the
   * area that this APZC instance is responsible for. This is used when
   * hit-testing to see which APZC instance should handle touch events.
   */
public:
  void SetLayerHitTestData(const ParentLayerRect& aRect, const gfx3DMatrix& aTransformToLayer,
                           const gfx3DMatrix& aTransformForLayer) {
    mVisibleRect = aRect;
    mAncestorTransform = aTransformToLayer;
    mCSSTransform = aTransformForLayer;
    UpdateTransformScale();
  }

  gfx3DMatrix GetAncestorTransform() const {
    return mAncestorTransform;
  }

  gfx3DMatrix GetCSSTransform() const {
    return mCSSTransform;
  }

  bool VisibleRegionContains(const ParentLayerPoint& aPoint) const {
    return mVisibleRect.Contains(aPoint);
  }

  bool IsOverscrolled() const {
    return mX.IsOverscrolled() || mY.IsOverscrolled();
  }

private:
  /* This is the visible region of the layer that this APZC corresponds to, in
   * that layer's screen pixels (the same coordinate system in which this APZC
   * receives events in ReceiveInputEvent()). */
  ParentLayerRect mVisibleRect;
  /* This is the cumulative CSS transform for all the layers between the parent
   * APZC and this one (not inclusive) */
  gfx3DMatrix mAncestorTransform;
  /* This is the CSS transform for this APZC's layer. */
  gfx3DMatrix mCSSTransform;


  /* ===================================================================
   * The functions and members in this section are used for sharing the
   * FrameMetrics across processes for the progressive tiling code.
   */
private:
  /* Unique id assigned to each APZC. Used with ViewID to uniquely identify
   * shared FrameMeterics used in progressive tile painting. */
  const uint32_t mAPZCId;

  ipc::SharedMemoryBasic* mSharedFrameMetricsBuffer;
  CrossProcessMutex* mSharedLock;
  /**
   * Called when ever mFrameMetrics is updated so that if it is being
   * shared with the content process the shared FrameMetrics may be updated.
   */
  void UpdateSharedCompositorFrameMetrics();
  /**
   * Create a shared memory buffer for containing the FrameMetrics and
   * a CrossProcessMutex that may be shared with the content process
   * for use in progressive tiled update calculations.
   */
  void ShareCompositorFrameMetrics();


  /* ===================================================================
   * The functions and members in this section are used for testing
   * purposes only.
   */
public:
  /**
   * Sync panning and zooming animation using a fixed frame time.
   * This will ensure that we animate the APZC correctly with other external
   * animations to the same timestamp.
   */
  static void SetFrameTime(const TimeStamp& aMilliseconds);
  /**
   * In the gtest environment everything runs on one thread, so we
   * shouldn't assert that we're on a particular thread. This enables
   * that behaviour.
   */
  static void SetThreadAssertionsEnabled(bool aEnabled);
  static bool GetThreadAssertionsEnabled();
  /**
   * Set an extra offset for testing async scrolling.
   */
  void SetTestAsyncScrollOffset(const CSSPoint& aPoint)
  {
    mTestAsyncScrollOffset = aPoint;
  }

private:
  // Extra offset to add in SampleContentTransformForFrame for testing
  CSSPoint mTestAsyncScrollOffset;
};

class AsyncPanZoomAnimation {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncPanZoomAnimation)

public:
  AsyncPanZoomAnimation(const TimeDuration& aRepaintInterval =
                        TimeDuration::Forever())
    : mRepaintInterval(aRepaintInterval)
  { }

  virtual bool Sample(FrameMetrics& aFrameMetrics,
                      const TimeDuration& aDelta) = 0;

  /**
   * Get the deferred tasks in |mDeferredTasks|. See |mDeferredTasks|
   * for more information.
   * Clears |mDeferredTasks|.
   */
  Vector<Task*> TakeDeferredTasks() {
    Vector<Task*> result;
    mDeferredTasks.swap(result);
    return result;
  }

  /**
   * Specifies how frequently (at most) we want to do repaints during the
   * animation sequence. TimeDuration::Forever() will cause it to only repaint
   * at the end of the animation.
   */
  TimeDuration mRepaintInterval;

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~AsyncPanZoomAnimation()
  { }

  /**
   * Tasks scheduled for execution after the APZC's mMonitor is released.
   * Derived classes can add tasks here in Sample(), and the APZC can call
   * ExecuteDeferredTasks() to execute them.
   */
  Vector<Task*> mDeferredTasks;
};

}
}

#endif // mozilla_layers_PanZoomController_h
