/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_AsyncPanZoomController_h
#define mozilla_layers_AsyncPanZoomController_h

#include "CrossProcessMutex.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "mozilla/Monitor.h"
#include "mozilla/ReentrantMonitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Atomics.h"
#include "InputData.h"
#include "Axis.h"
#include "InputQueue.h"
#include "TaskThrottler.h"
#include "mozilla/gfx/Matrix.h"
#include "nsRegion.h"

#include "base/message_loop.h"

namespace mozilla {

namespace ipc {

class SharedMemoryBasic;

}

namespace layers {

struct ScrollableLayerGuid;
class CompositorParent;
class GestureEventListener;
class PCompositorParent;
struct ViewTransform;
class AsyncPanZoomAnimation;
class FlingAnimation;
class InputBlockState;
class TouchBlockState;
class OverscrollHandoffChain;
class StateChangeNotificationBlocker;

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
  typedef mozilla::gfx::Matrix4x4 Matrix4x4;
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
   * Note: this distance is in global screen coordinates.
   */
  static float GetTouchStartTolerance();

  AsyncPanZoomController(uint64_t aLayersId,
                         APZCTreeManager* aTreeManager,
                         const nsRefPtr<InputQueue>& aInputQueue,
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
   * See the documentation on APZCTreeManager::ReceiveInputEvent for info on
   * return values from this function.
   */
  nsEventStatus ReceiveInputEvent(const InputData& aEvent, uint64_t* aOutInputBlockId);

  /**
   * Kicks an animation to zoom to a rect. This may be either a zoom out or zoom
   * in. The actual animation is done on the compositor thread after being set
   * up.
   */
  void ZoomToRect(CSSRect aRect);

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

  /**
   * Advances any animations currently running to the given timestamp.
   * This may be called multiple times with the same timestamp.
   *
   * The return value indicates whether or not any currently running animation
   * should continue. If true, the compositor should schedule another composite.
   */
  bool AdvanceAnimations(const TimeStamp& aSampleTime);

  bool UpdateAnimation(const TimeStamp& aSampleTime,
                       Vector<Task*>* aOutDeferredTasks);

  /**
   * Query the transforms that should be applied to the layer corresponding
   * to this APZC due to asynchronous panning and zooming.
   * This function returns the async transform via the |aOutTransform|
   * out parameter.
   */
  void SampleContentTransformForFrame(ViewTransform* aOutTransform,
                                      ScreenPoint& aScrollOffset);

  /**
   * Return a visual effect that reflects this apzc's
   * overscrolled state, if any.
   */
  Matrix4x4 GetOverscrollTransform() const;

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
  bool IsDestroyed() const;

  /**
   * Returns the incremental transformation corresponding to the async pan/zoom
   * in progress. That is, when this transform is multiplied with the layer's
   * existing transform, it will make the layer appear with the desired pan/zoom
   * amount.
   */
  ViewTransform GetCurrentAsyncTransform() const;

  /**
   * Returns the part of the async transform that will remain once Gecko does a
   * repaint at the desired metrics. That is, in the steady state:
   * Matrix4x4(GetCurrentAsyncTransform()) === GetNontransientAsyncTransform()
   */
  Matrix4x4 GetNontransientAsyncTransform() const;

  /**
   * Returns the transform to take something from the coordinate space of the
   * last thing we know gecko painted, to the coordinate space of the last thing
   * we asked gecko to paint. In cases where that last request has not yet been
   * processed, this is needed to transform input events properly into a space
   * gecko will understand.
   */
  Matrix4x4 GetTransformToLastDispatchedPaint() const;

  /**
   * Returns whether or not the APZC is currently in a state of checkerboarding.
   * This is a simple computation based on the last-painted content and whether
   * the async transform has pushed it so far that it doesn't fully contain the
   * composition bounds.
   */
  bool IsCurrentlyCheckerboarding() const;

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
   * Cancels any currently running animation.
   */
  void CancelAnimation();

  /**
   * Clear any overscroll on this APZC.
   */
  void ClearOverscroll();

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
   * Returns whether this APZC is for an element marked with the 'scrollgrab'
   * attribute.
   */
  bool HasScrollgrab() const { return mFrameMetrics.GetHasScrollgrab(); }

  /**
   * Returns whether this APZC has room to be panned (in any direction).
   */
  bool IsPannable() const;

  /**
   * Returns true if the APZC has a velocity greater than the stop-on-tap
   * fling velocity threshold (which is pref-controlled).
   */
  bool IsMovingFast() const;

  /**
   * Returns the identifier of the touch in the last touch event processed by
   * this APZC. This should only be called when the last touch event contained
   * only one touch.
   */
  int32_t GetLastTouchIdentifier() const;

  /**
   * Convert the vector |aVector|, rooted at the point |aAnchor|, from
   * this APZC's local screen coordinates into global screen coordinates.
   * The anchor is necessary because with 3D tranforms, the location of the
   * vector can affect the result of the transform.
   * To respect the lock ordering, mMonitor must NOT be held when calling
   * this function (since this function acquires the tree lock).
   */
  void ToGlobalScreenCoordinates(ScreenPoint* aVector,
                                 const ScreenPoint& aAnchor) const;

  /**
   * Convert the vector |aVector|, rooted at the point |aAnchor|, from
   * global screen coordinates into this APZC's local screen coordinates .
   * The anchor is necessary because with 3D tranforms, the location of the
   * vector can affect the result of the transform.
   * To respect the lock ordering, mMonitor must NOT be held when calling
   * this function (since this function acquires the tree lock).
   */
  void ToLocalScreenCoordinates(ScreenPoint* aVector,
                                const ScreenPoint& aAnchor) const;

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
   * Helper methods for handling pan events.
   */
  nsEventStatus OnPanMayBegin(const PanGestureInput& aEvent);
  nsEventStatus OnPanCancelled(const PanGestureInput& aEvent);
  nsEventStatus OnPanBegin(const PanGestureInput& aEvent);
  nsEventStatus OnPan(const PanGestureInput& aEvent, bool aFingersOnTouchpad);
  nsEventStatus OnPanEnd(const PanGestureInput& aEvent);
  nsEventStatus OnPanMomentumStart(const PanGestureInput& aEvent);
  nsEventStatus OnPanMomentumEnd(const PanGestureInput& aEvent);

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
   * OnTouchMove() or the equivalent for pan gestures is being invoked).
   * Note: This function returns a distance in global screen coordinates,
   *       not the local screen coordinates of this APZC.
   */
  float PanDistance() const;

  /**
   * Gets the start point of the current touch.
   * Like PanDistance(), this only makes sense if a touch is currently
   * happening and OnTouchMove() or the equivalent for pan gestures is
   * being invoked.
   * Unlikely PanDistance(), this function returns a point in local screen
   * coordinates.
   */
  ScreenPoint PanStart() const;

  /**
   * Gets a vector of the velocities of each axis.
   */
  const ScreenPoint GetVelocityVector() const;

  /**
   * Gets the first touch point from a MultiTouchInput.  This gets only
   * the first one and assumes the rest are either missing or not relevant.
   */
  ScreenPoint GetFirstTouchScreenPoint(const MultiTouchInput& aEvent);

  /**
   * Sets the panning state basing on the pan direction angle and current touch-action value.
   */
  void HandlePanningWithTouchAction(double angle);

  /**
   * Sets the panning state ignoring the touch action value.
   */
  void HandlePanning(double angle);

  /**
   * Update the panning state and axis locks.
   * Note: |aDelta| is expected to be in global screen coordinates.
   */
  void HandlePanningUpdate(const ScreenPoint& aDelta);

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
   * metrics.  (Helper function used by RequestContentRepaint.) If aThrottled
   * is set to false, the repaint request is sent directly without going through
   * the paint throttler. In particular, the GeckoContentController::RequestContentRepaint
   * function will be invoked before this function returns.
   */
  void RequestContentRepaint(FrameMetrics& aFrameMetrics, bool aThrottled = true);

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
   * Gets the pointer to the apzc tree manager. All the access to tree manager
   * should be made via this method and not via private variable since this method
   * ensures that no lock is set.
   */
  APZCTreeManager* GetApzcTreeManager() const;

  /**
   * Timeout function for mozbrowserasyncscroll event. Because we throttle
   * mozbrowserasyncscroll events in some conditions, this function ensures
   * that the last mozbrowserasyncscroll event will be fired after a period of
   * time.
   */
  void FireAsyncScrollOnTimeout();

  /**
   * Convert ScreenPoint relative to this APZC to CSSPoint relative
   * to the parent document. This excludes the transient compositor transform.
   * NOTE: This must be converted to CSSPoint relative to the child
   * document before sending over IPC.
   */
  bool ConvertToGecko(const ScreenPoint& aPoint, CSSPoint* aOut);

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
  mutable Monitor mRefPtrMonitor;

  /* Utility functions that return a addrefed pointer to the corresponding fields. */
  already_AddRefed<GeckoContentController> GetGeckoContentController() const;
  already_AddRefed<GestureEventListener> GetGestureEventListener() const;
  const nsRefPtr<InputQueue>& GetInputQueue() const;

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

  // Stores the previous focus point if there is a pinch gesture happening. Used
  // to allow panning by moving multiple fingers (thus moving the focus point).
  ParentLayerPoint mLastZoomFocus;

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

  nsRefPtr<AsyncPanZoomAnimation> mAnimation;

  friend class Axis;



  /* ===================================================================
   * The functions and members in this section are used to manage
   * the state that tracks what this APZC is doing with the input events.
   */
protected:
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
    SNAP_BACK,                /* snap-back animation to relieve overscroll */
    SMOOTH_SCROLL,            /* Smooth scrolling to destination. Used by
                                 CSSOM-View smooth scroll-behavior */
  };

  // This is in theory protected by |mMonitor|; that is, it should be held whenever
  // this is updated. In practice though... see bug 897017.
  PanZoomState mState;

private:
  friend class StateChangeNotificationBlocker;
  /**
   * A counter of how many StateChangeNotificationBlockers are active.
   * A non-zero count will prevent state change notifications from
   * being dispatched. Only code that holds mMonitor should touch this.
   */
  int mNotificationBlockers;

  /**
   * Helper to set the current state. Holds the monitor before actually setting
   * it and fires content controller events based on state changes. Always set
   * the state using this call, do not set it directly.
   */
  void SetState(PanZoomState aState);
  /**
   * Fire content controller notifications about state changes, assuming no
   * StateChangeNotificationBlocker has been activated.
   */
  void DispatchStateChangeNotification(PanZoomState aOldState, PanZoomState aNewState);
  /**
   * Internal helpers for checking general state of this apzc.
   */
  static bool IsTransformingState(PanZoomState aState);
  bool IsInPanningState() const;



  /* ===================================================================
   * The functions and members in this section are used to manage
   * blocks of touch events and the state needed to deal with content
   * listeners.
   */
public:
  /**
   * See InputQueue::ContentReceivedTouch
   */
  void ContentReceivedTouch(uint64_t aInputBlockId, bool aPreventDefault);

  /**
   * See InputQueue::SetAllowedTouchBehavior
   */
  void SetAllowedTouchBehavior(uint64_t aInputBlockId, const nsTArray<TouchBehaviorFlags>& aBehaviors);

  /**
   * Flush a repaint request if one is needed, without throttling it with the
   * paint throttler.
   */
  void FlushRepaintForNewInputBlock();

  /**
   * Given the number of touch points in an input event and touch block they
   * belong to, check if the event can result in a panning/zooming behavior.
   * This is primarily used to figure out when to dispatch the pointercancel
   * event for the pointer events spec.
   */
  bool ArePointerEventsConsumable(TouchBlockState* aBlock, uint32_t aTouchPoints);

  /**
   * Return true if there are are touch listeners registered on content
   * scrolled by this APZC.
   */
  bool NeedToWaitForContent() const;

  /**
   * Clear internal state relating to input handling.
   */
  void ResetInputState();

private:
  nsRefPtr<InputQueue> mInputQueue;
  TouchBlockState* CurrentTouchBlock();
  bool HasReadyTouchBlock();


  /* ===================================================================
   * The functions and members in this section are used to manage
   * pan gestures.
   */

private:
  UniquePtr<InputBlockState> mPanGestureState;


  /* ===================================================================
   * The functions and members in this section are used to manage
   * fling animations, smooth scroll animations, and overscroll
   * during a fling or smooth scroll.
   */
public:
  /**
   * Attempt a fling with the given velocity. If we are not pannable, tehe fling
   * is handed off to the next APZC in the handoff chain via
   * mTreeManager->DspatchFling(). Returns true iff. any APZC (whether this
   * one or one further in the handoff chain) accepted the fling.
   * |aHandoff| should be true iff. the fling was handed off from a previous
   *            APZC, and determines whether acceleration is applied to the
   *            fling.
   */
  bool AttemptFling(ScreenPoint aVelocity,
                    const nsRefPtr<const OverscrollHandoffChain>& aOverscrollHandoffChain,
                    bool aHandoff);

private:
  friend class FlingAnimation;
  friend class OverscrollSnapBackAnimation;
  friend class SmoothScrollAnimation;
  // The initial velocity of the most recent fling.
  ScreenPoint mLastFlingVelocity;
  // The time at which the most recent fling started.
  TimeStamp mLastFlingTime;

  // Deal with overscroll resulting from a fling animation. This is only ever
  // called on APZC instances that were actually performing a fling.
  // The overscroll is handled by trying to hand the fling off to an APZC
  // later in the handoff chain, or if there are no takers, continuing the
  // fling and entering an overscrolled state.
  void HandleFlingOverscroll(const ScreenPoint& aVelocity,
                             const nsRefPtr<const OverscrollHandoffChain>& aOverscrollHandoffChain);

  void HandleSmoothScrollOverscroll(const ScreenPoint& aVelocity);

  // Helper function used by TakeOverFling() and HandleFlingOverscroll().
  void AcceptFling(const ScreenPoint& aVelocity,
                   const nsRefPtr<const OverscrollHandoffChain>& aOverscrollHandoffChain,
                   bool aHandoff,
                   bool aAllowOverscroll);

  // Start a snap-back animation to relieve overscroll.
  void StartSnapBack();

  void StartSmoothScroll();

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

  AsyncPanZoomController* GetFirstChild() const {
    AsyncPanZoomController* child = GetLastChild();
    while (child && child->GetPrevSibling()) {
      child = child->GetPrevSibling();
    }
    return child;
  }

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
  FrameMetrics::ViewID GetScrollHandoffParentId() const {
    return mFrameMetrics.GetScrollParentId();
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
   * |aOverscrollHandoffChain| and |aOverscrollHandoffChainIndex| are used by
   * the tree manager to keep track of which APZC to hand off the overscroll
   * to; this function increments the chain and the index and passes it on to
   * APZCTreeManager::DispatchScroll() in the event of overscroll.
   * Returns true iff. this APZC, or an APZC further down the
   * handoff chain, accepted the scroll (possibly entering an overscrolled
   * state). If this returns false, the caller APZC knows that it should enter
   * an overscrolled state itself if it can.
   */
  bool AttemptScroll(const ScreenPoint& aStartPoint, const ScreenPoint& aEndPoint,
                     OverscrollHandoffState& aOverscrollHandoffState);

  void FlushRepaintForOverscrollHandoff();

  /**
   * If overscrolled, start a snap-back animation and return true.
   * Otherwise return false.
   */
  bool SnapBackIfOverscrolled();

  /**
   * Build the chain of APZCs along which scroll will be handed off when
   * this APZC receives input events.
   *
   * Notes on lifetime and const-correctness:
   *   - The returned handoff chain is |const|, to indicate that it cannot be
   *     changed after being built.
   *   - When passing the chain to a function that uses it without storing it,
   *     pass it by reference-to-const (as in |const OverscrollHandoffChain&|).
   *   - When storing the chain, store it by RefPtr-to-const (as in
   *     |nsRefPtr<const OverscrollHandoffChain>|). This ensures the chain is
   *     kept alive. Note that queueing a task that uses the chain as an
   *     argument constitutes storing, as the task may outlive its queuer.
   *   - When passing the chain to a function that will store it, pass it as
   *     |const nsRefPtr<const OverscrollHandoffChain>&|. This allows the
   *     function to copy it into the |nsRefPtr<const OverscrollHandoffChain>|
   *     that will store it, while avoiding an unnecessary copy (and thus
   *     AddRef() and Release()) when passing it.
   */
  nsRefPtr<const OverscrollHandoffChain> BuildOverscrollHandoffChain();

private:
  /**
   * A helper function for calling APZCTreeManager::DispatchScroll().
   * Guards against the case where the APZC is being concurrently destroyed
   * (and thus mTreeManager is being nulled out).
   */
  bool CallDispatchScroll(const ScreenPoint& aStartPoint,
                          const ScreenPoint& aEndPoint,
                          OverscrollHandoffState& aOverscrollHandoffState);

  /**
   * A helper function for overscrolling during panning. This is a wrapper
   * around OverscrollBy() that also implements restrictions on entering
   * overscroll based on the pan angle.
   */
  bool OverscrollForPanning(ScreenPoint aOverscroll,
                            const ScreenPoint& aPanDistance);

  /**
   * Try to overscroll by 'aOverscroll'.
   * If we are pannable, 'aOverscroll' is added to any existing overscroll,
   * and the function returns true.
   * Otherwise, nothing happens and the function return false.
   */
  bool OverscrollBy(const ScreenPoint& aOverscroll);


  /* ===================================================================
   * The functions and members in this section are used to maintain the
   * area that this APZC instance is responsible for. This is used when
   * hit-testing to see which APZC instance should handle touch events.
   */
public:
  void SetLayerHitTestData(const nsIntRegion& aRegion, const Matrix4x4& aTransformToLayer) {
    mVisibleRegion = aRegion;
    mAncestorTransform = aTransformToLayer;
  }

  void AddHitTestRegion(const nsIntRegion& aRegion) {
    mVisibleRegion.OrWith(aRegion);
  }

  Matrix4x4 GetAncestorTransform() const {
    return mAncestorTransform;
  }

  bool VisibleRegionContains(const ParentLayerPoint& aPoint) const {
    ParentLayerIntPoint point = RoundedToInt(aPoint);
    return mVisibleRegion.Contains(point.x, point.y);
  }

  bool IsOverscrolled() const {
    return mX.IsOverscrolled() || mY.IsOverscrolled();
  }

private:
  /* This is the union of the visible regions of the layers that this APZC
   * corresponds to, in the screen pixels of those layers. (This is the same
   * coordinate system in which this APZC receives events in
   * ReceiveInputEvent()). */
  nsIntRegion mVisibleRegion;
  /* This is the cumulative CSS transform for all the layers from (and including)
   * the parent APZC down to (but excluding) this one. */
  Matrix4x4 mAncestorTransform;


  /* ===================================================================
   * The functions and members in this section are used for sharing the
   * FrameMetrics across processes for the progressive tiling code.
   */
private:
  /* Unique id assigned to each APZC. Used with ViewID to uniquely identify
   * shared FrameMeterics used in progressive tile painting. */
  const uint32_t mAPZCId;

  nsRefPtr<ipc::SharedMemoryBasic> mSharedFrameMetricsBuffer;
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
   * and assertion purposes only.
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
   * This can be used to assert that the current thread is the
   * controller/UI thread (on which input events are received).
   * This does nothing if thread assertions are disabled.
   */
  static void AssertOnControllerThread();
  /**
   * This can be used to assert that the current thread is the
   * compositor thread (which applies the async transform).
   * This does nothing if thread assertions are disabled.
   */
  static void AssertOnCompositorThread();
  /**
   * Set an extra offset for testing async scrolling.
   */
  void SetTestAsyncScrollOffset(const CSSPoint& aPoint)
  {
    mTestAsyncScrollOffset = aPoint;
  }

  void MarkAsyncTransformAppliedToContent()
  {
    mAsyncTransformAppliedToContent = true;
  }

  bool GetAsyncTransformAppliedToContent() const
  {
    return mAsyncTransformAppliedToContent;
  }

private:
  // Extra offset to add in SampleContentTransformForFrame for testing
  CSSPoint mTestAsyncScrollOffset;
  // Flag to track whether or not the APZ transform is not used. This
  // flag is recomputed for every composition frame.
  bool mAsyncTransformAppliedToContent;
};

class AsyncPanZoomAnimation {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncPanZoomAnimation)

public:
  explicit AsyncPanZoomAnimation(const TimeDuration& aRepaintInterval =
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
