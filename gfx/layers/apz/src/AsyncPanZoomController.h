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
#include "mozilla/layers/AsyncPanZoomAnimation.h"
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
#include "APZUtils.h"
#include "Layers.h"                     // for Layer::ScrollDirection
#include "LayersTypes.h"
#include "mozilla/gfx/Matrix.h"
#include "nsIScrollableFrame.h"
#include "nsRegion.h"
#include "nsTArray.h"
#include "PotentialCheckerboardDurationTracker.h"

#include "base/message_loop.h"

namespace mozilla {

namespace ipc {

class SharedMemoryBasic;

} // namespace ipc

namespace layers {

class AsyncDragMetrics;
struct ScrollableLayerGuid;
class CompositorBridgeParent;
class GestureEventListener;
class PCompositorBridgeParent;
struct AsyncTransform;
class AsyncPanZoomAnimation;
class AndroidFlingAnimation;
class GenericFlingAnimation;
class InputBlockState;
class TouchBlockState;
class PanGestureBlockState;
class OverscrollHandoffChain;
class StateChangeNotificationBlocker;
class CheckerboardEvent;
class OverscrollEffectBase;
class WidgetOverscrollEffect;
class GenericOverscrollEffect;
class AndroidSpecificState;

// Base class for grouping platform-specific APZC state variables.
class PlatformSpecificStateBase {
public:
  virtual ~PlatformSpecificStateBase() {}
  virtual AndroidSpecificState* AsAndroidSpecificState() { return nullptr; }
};

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
   * Note: It's an abuse of the 'Coord' class to use it to represent a 2D
   *       distance, but it's the closest thing we currently have.
   */
  static ScreenCoord GetTouchStartTolerance();

  AsyncPanZoomController(uint64_t aLayersId,
                         APZCTreeManager* aTreeManager,
                         const RefPtr<InputQueue>& aInputQueue,
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
   * Kicks an animation to zoom to a rect. This may be either a zoom out or zoom
   * in. The actual animation is done on the compositor thread after being set
   * up.
   */
  void ZoomToRect(CSSRect aRect, const uint32_t aFlags);

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
  void PostDelayedTask(already_AddRefed<Runnable> aTask, int aDelayMs);

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
                       nsTArray<RefPtr<Runnable>>* aOutDeferredTasks);

  /**
   * A shadow layer update has arrived. |aScrollMetdata| is the new ScrollMetadata
   * for the container layer corresponding to this APZC.
   * |aIsFirstPaint| is a flag passed from the shadow
   * layers code indicating that the scroll metadata being sent with this call are
   * the initial metadata and the initial paint of the frame has just happened.
   */
  void NotifyLayersUpdated(const ScrollMetadata& aScrollMetadata, bool aIsFirstPaint,
                           bool aThisLayerTreeUpdated);

  /**
   * The platform implementation must set the compositor parent so that we can
   * request composites.
   */
  void SetCompositorBridgeParent(CompositorBridgeParent* aCompositorBridgeParent);

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
   * Returns the transform to take something from the coordinate space of the
   * last thing we know gecko painted, to the coordinate space of the last thing
   * we asked gecko to paint. In cases where that last request has not yet been
   * processed, this is needed to transform input events properly into a space
   * gecko will understand.
   */
  Matrix4x4 GetTransformToLastDispatchedPaint() const;

  /**
   * Returns the number of CSS pixels of checkerboard according to the metrics
   * in this APZC.
   */
  uint32_t GetCheckerboardMagnitude() const;

  /**
   * Report the number of CSSPixel-milliseconds of checkerboard to telemetry.
   */
  void ReportCheckerboard(const TimeStamp& aSampleTime);

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
  static const ScreenMargin CalculatePendingDisplayPort(
    const FrameMetrics& aFrameMetrics,
    const ParentLayerPoint& aVelocity);

  nsEventStatus HandleDragEvent(const MouseInput& aEvent,
                                const AsyncDragMetrics& aDragMetrics);

  /**
   * Handler for events which should not be intercepted by the touch listener.
   */
  nsEventStatus HandleInputEvent(const InputData& aEvent,
                                 const ScreenToParentLayerMatrix4x4& aTransformToApzc);

  /**
   * Handler for gesture events.
   * Currently some gestures are detected in GestureEventListener that calls
   * APZC back through this handler in order to avoid recursive calls to
   * APZC::HandleInputEvent() which is supposed to do the work for
   * ReceiveInputEvent().
   */
  nsEventStatus HandleGestureEvent(const InputData& aEvent);

  /**
   * Handler for touch velocity.
   * Sometimes the touch move event will have a velocity even though no scrolling
   * is occurring such as when the toolbar is being hidden/shown in Fennec.
   * This function can be called to have the y axis' velocity queue updated.
   */
  void HandleTouchVelocity(uint32_t aTimesampMs, float aSpeedY);

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

  /**
   * Returns true if the tree manager of this APZC is the same as the one
   * passed in.
   */
  bool HasTreeManager(const APZCTreeManager* aTreeManager) const;

  void StartAnimation(AsyncPanZoomAnimation* aAnimation);

  /**
   * Cancels any currently running animation.
   * aFlags is a bit-field to provide specifics of how to cancel the animation.
   * See CancelAnimationFlags.
   */
  void CancelAnimation(CancelAnimationFlags aFlags = Default);

  /**
   * Adjusts the scroll position to compensate for a shift in the surface, such
   * that the content appears to remain visually in the same position. i.e. if
   * the surface moves up by 10 screenpixels, the scroll position should also
   * move up by 10 pixels so that what used to be at the top of the surface is
   * now 10 pixels down the surface.
   */
  void AdjustScrollForSurfaceShift(const ScreenPoint& aShift);

  /**
   * Clear any overscroll on this APZC.
   */
  void ClearOverscroll();

  /**
   * Returns whether this APZC is for an element marked with the 'scrollgrab'
   * attribute.
   */
  bool HasScrollgrab() const { return mScrollMetadata.GetHasScrollgrab(); }

  /**
   * Returns whether this APZC has room to be panned (in any direction).
   */
  bool IsPannable() const;

  /**
   * Returns true if the APZC has been flung with a velocity greater than the
   * stop-on-tap fling velocity threshold (which is pref-controlled).
   */
  bool IsFlingingFast() const;

  /**
   * Returns the identifier of the touch in the last touch event processed by
   * this APZC. This should only be called when the last touch event contained
   * only one touch.
   */
  int32_t GetLastTouchIdentifier() const;

  /**
   * Returns the matrix that transforms points from global screen space into
   * this APZC's ParentLayer space.
   * To respect the lock ordering, mMonitor must NOT be held when calling
   * this function (since this function acquires the tree lock).
   */
  ScreenToParentLayerMatrix4x4 GetTransformToThis() const;

  /**
   * Convert the vector |aVector|, rooted at the point |aAnchor|, from
   * this APZC's ParentLayer coordinates into screen coordinates.
   * The anchor is necessary because with 3D tranforms, the location of the
   * vector can affect the result of the transform.
   * To respect the lock ordering, mMonitor must NOT be held when calling
   * this function (since this function acquires the tree lock).
   */
  ScreenPoint ToScreenCoordinates(const ParentLayerPoint& aVector,
                                  const ParentLayerPoint& aAnchor) const;

  /**
   * Convert the vector |aVector|, rooted at the point |aAnchor|, from
   * screen coordinates into this APZC's ParentLayer coordinates.
   * The anchor is necessary because with 3D tranforms, the location of the
   * vector can affect the result of the transform.
   * To respect the lock ordering, mMonitor must NOT be held when calling
   * this function (since this function acquires the tree lock).
   */
  ParentLayerPoint ToParentLayerCoordinates(const ScreenPoint& aVector,
                                            const ScreenPoint& aAnchor) const;

  // Return whether or not a wheel event will be able to scroll in either
  // direction.
  bool CanScroll(const InputData& aEvent) const;

  // Return whether or not a scroll delta will be able to scroll in either
  // direction.
  bool CanScrollWithWheel(const ParentLayerPoint& aDelta) const;

  // Return whether or not there is room to scroll this APZC
  // in the given direction.
  bool CanScroll(Layer::ScrollDirection aDirection) const;

  void NotifyMozMouseScrollEvent(const nsString& aString) const;

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~AsyncPanZoomController();

  // Returns the cached current frame time.
  TimeStamp GetFrameTime() const;

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
   * Helper methods for handling scroll wheel events.
   */
  nsEventStatus OnScrollWheel(const ScrollWheelInput& aEvent);

  ParentLayerPoint GetScrollWheelDelta(const ScrollWheelInput& aEvent) const;

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
   * CompositorBridgeParent::ScheduleRenderOnCompositorThread().
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
   * Note: It's an abuse of the 'Coord' class to use it to represent a 2D
   *       distance, but it's the closest thing we currently have.
   */
  ScreenCoord PanDistance() const;

  /**
   * Gets the start point of the current touch.
   * Like PanDistance(), this only makes sense if a touch is currently
   * happening and OnTouchMove() or the equivalent for pan gestures is
   * being invoked.
   */
  ParentLayerPoint PanStart() const;

  /**
   * Gets a vector of the velocities of each axis.
   */
  const ParentLayerPoint GetVelocityVector() const;

  /**
   * Sets the velocities of each axis.
   */
  void SetVelocityVector(const ParentLayerPoint& aVelocityVector);

  /**
   * Gets the first touch point from a MultiTouchInput.  This gets only
   * the first one and assumes the rest are either missing or not relevant.
   */
  ParentLayerPoint GetFirstTouchPoint(const MultiTouchInput& aEvent);

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
   * work. This call will use the current metrics. If this function is called
   * from a non-main thread, it will redispatch itself to the main thread, and
   * use the latest metrics during the redispatch.
   */
  void RequestContentRepaint();

  /**
   * Send the provided metrics to Gecko to trigger a repaint. This function
   * may filter duplicate calls with the same metrics. This function must be
   * called on the main thread.
   */
  void RequestContentRepaint(const FrameMetrics& aFrameMetrics,
                             const ParentLayerPoint& aVelocity);

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
   * Convert ScreenPoint relative to the screen to LayoutDevicePoint relative
   * to the parent document. This excludes the transient compositor transform.
   * NOTE: This must be converted to LayoutDevicePoint relative to the child
   * document before sending over IPC to a child process.
   */
  bool ConvertToGecko(const ScreenIntPoint& aPoint, LayoutDevicePoint* aOut);

  enum AxisLockMode {
    FREE,     /* No locking at all */
    STANDARD, /* Default axis locking mode that remains locked until pan ends*/
    STICKY,   /* Allow lock to be broken, with hysteresis */
  };

  static AxisLockMode GetAxisLockMode();

  // Helper function for OnSingleTapUp(), OnSingleTapConfirmed(), and
  // OnLongPressUp().
  nsEventStatus GenerateSingleTap(GeckoContentController::TapType aType,
                                  const ScreenIntPoint& aPoint,
                                  mozilla::Modifiers aModifiers);

  // Common processing at the end of a touch block.
  void OnTouchEndOrCancel();

  uint64_t mLayersId;
  RefPtr<CompositorBridgeParent> mCompositorBridgeParent;

  /* Access to the following two fields is protected by the mRefPtrMonitor,
     since they are accessed on the UI thread but can be cleared on the
     compositor thread. */
  RefPtr<GeckoContentController> mGeckoContentController;
  RefPtr<GestureEventListener> mGestureEventListener;
  mutable Monitor mRefPtrMonitor;

  // This is a raw pointer to avoid introducing a reference cycle between
  // AsyncPanZoomController and APZCTreeManager. Since these objects don't
  // live on the main thread, we can't use the cycle collector with them.
  // The APZCTreeManager owns the lifetime of the APZCs, so nulling this
  // pointer out in Destroy() will prevent accessing deleted memory.
  Atomic<APZCTreeManager*> mTreeManager;

  /* Utility functions that return a addrefed pointer to the corresponding fields. */
  already_AddRefed<GeckoContentController> GetGeckoContentController() const;
  already_AddRefed<GestureEventListener> GetGestureEventListener() const;

  // If we are sharing our frame metrics with content across processes
  bool mSharingFrameMetricsAcrossProcesses;
  /* Utility function to get the Compositor with which we share the FrameMetrics.
     This function is only callable from the compositor thread. */
  PCompositorBridgeParent* GetSharedFrameMetricsCompositor();

  PlatformSpecificStateBase* GetPlatformSpecificState();

protected:
  // Both |mFrameMetrics| and |mLastContentPaintMetrics| are protected by the
  // monitor. Do not read from or modify either of them without locking.
  ScrollMetadata mScrollMetadata;
  FrameMetrics& mFrameMetrics;  // for convenience, refers to mScrollMetadata.mMetrics

  // Protects |mFrameMetrics|, |mLastContentPaintMetrics|, and |mState|.
  // Before manipulating |mFrameMetrics| or |mLastContentPaintMetrics|, the
  // monitor should be held. When setting |mState|, either the SetState()
  // function can be used, or the monitor can be held and then |mState| updated.
  // IMPORTANT: See the note about lock ordering at the top of APZCTreeManager.h.
  // This is mutable to allow entering it from 'const' methods; doing otherwise
  // would significantly limit what methods could be 'const'.
  mutable ReentrantMonitor mMonitor;

private:
  // Metadata of the container layer corresponding to this APZC. This is
  // stored here so that it is accessible from the UI/controller thread.
  // These are the metrics at last content paint, the most recent
  // values we were notified of in NotifyLayersUpdate(). Since it represents
  // the Gecko state, it should be used as a basis for untransformation when
  // sending messages back to Gecko.
  ScrollMetadata mLastContentPaintMetadata;
  FrameMetrics& mLastContentPaintMetrics;  // for convenience, refers to mLastContentPaintMetadata.mMetrics
  // The last metrics used for a content repaint request.
  FrameMetrics mLastPaintRequestMetrics;
  // The metrics that we expect content to have. This is updated when we
  // request a content repaint, and when we receive a shadow layers update.
  // This allows us to transform events into Gecko's coordinate space.
  FrameMetrics mExpectedGeckoMetrics;

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

  // The last sample time at which we submitted a checkerboarding report.
  TimeStamp mLastCheckerboardReport;

  // Stores the previous focus point if there is a pinch gesture happening. Used
  // to allow panning by moving multiple fingers (thus moving the focus point).
  ParentLayerPoint mLastZoomFocus;

  RefPtr<AsyncPanZoomAnimation> mAnimation;

  UniquePtr<OverscrollEffectBase> mOverscrollEffect;

  // Groups state variables that are specific to a platform.
  // Initialized on first use.
  UniquePtr<PlatformSpecificStateBase> mPlatformSpecificState;

  friend class Axis;


  /* ===================================================================
   * The functions and members in this section are used to expose
   * the current async transform state to callers.
   */
public:
  /**
   * Allows callers to specify which type of async transform they want:
   * NORMAL provides the actual async transforms of the APZC, whereas
   * RESPECT_FORCE_DISABLE will provide empty async transforms if and only if
   * the metrics has the mForceDisableApz flag set. In general the latter should
   * only be used by call sites that are applying the transform to update
   * a layer's position.
   */
  enum AsyncMode {
    NORMAL,
    RESPECT_FORCE_DISABLE,
  };

  /**
   * Query the transforms that should be applied to the layer corresponding
   * to this APZC due to asynchronous panning and zooming.
   * This function returns the async transform via the |aOutTransform|
   * out parameter.
   */
  ParentLayerPoint GetCurrentAsyncScrollOffset(AsyncMode aMode) const;

  /**
   * Return a visual effect that reflects this apzc's
   * overscrolled state, if any.
   */
  AsyncTransformComponentMatrix GetOverscrollTransform(AsyncMode aMode) const;

  /**
   * Returns the incremental transformation corresponding to the async pan/zoom
   * in progress. That is, when this transform is multiplied with the layer's
   * existing transform, it will make the layer appear with the desired pan/zoom
   * amount.
   */
  AsyncTransform GetCurrentAsyncTransform(AsyncMode aMode) const;

  /**
   * Returns the same transform as GetCurrentAsyncTransform(), but includes
   * any transform due to axis over-scroll.
   */
  AsyncTransformComponentMatrix GetCurrentAsyncTransformWithOverscroll(AsyncMode aMode) const;


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

    PAN_MOMENTUM,             /* like PANNING, but controlled by momentum PanGestureInput events */

    PINCHING,                 /* nth touch-start, where n > 1. this mode allows pan and zoom */
    ANIMATING_ZOOM,           /* animated zoom to a new rect */
    OVERSCROLL_ANIMATION,     /* Spring-based animation used to relieve overscroll once
                                 the finger is lifted. */
    SMOOTH_SCROLL,            /* Smooth scrolling to destination. Used by
                                 CSSOM-View smooth scroll-behavior */
    WHEEL_SCROLL              /* Smooth scrolling to a destination for a wheel event. */
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

  /* ===================================================================
   * The functions and members in this section are used to manage
   * blocks of touch events and the state needed to deal with content
   * listeners.
   */
public:
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
   * Clear internal state relating to touch input handling.
   */
  void ResetTouchInputState();

  /**
   * Gets a ref to the input queue that is shared across the entire tree manager.
   */
  const RefPtr<InputQueue>& GetInputQueue() const;

private:
  void CancelAnimationAndGestureState();

  RefPtr<InputQueue> mInputQueue;
  CancelableBlockState* CurrentInputBlock() const;
  TouchBlockState* CurrentTouchBlock() const;
  bool HasReadyTouchBlock() const;

  PanGestureBlockState* CurrentPanGestureBlock() const;

private:
  /* ===================================================================
   * The functions and members in this section are used to manage
   * fling animations, smooth scroll animations, and overscroll
   * during a fling or smooth scroll.
   */
public:
  /**
   * Attempt a fling with the velocity specified in |aHandoffState|.
   * If we are not pannable, the fling is handed off to the next APZC in
   * the handoff chain via mTreeManager->DispatchFling().
   * Returns true iff. the entire velocity of the fling was consumed by
   * this APZC. |aHandoffState.mVelocity| is modified to contain any
   * unused, residual velocity.
   * |aHandoffState.mIsHandoff| should be true iff. the fling was handed off
   * from a previous APZC, and determines whether acceleration is applied
   * to the fling.
   */
  bool AttemptFling(FlingHandoffState& aHandoffState);

private:
  friend class AndroidFlingAnimation;
  friend class GenericFlingAnimation;
  friend class OverscrollAnimation;
  friend class SmoothScrollAnimation;
  friend class WheelScrollAnimation;

  friend class GenericOverscrollEffect;
  friend class WidgetOverscrollEffect;

  // The initial velocity of the most recent fling.
  ParentLayerPoint mLastFlingVelocity;
  // The time at which the most recent fling started.
  TimeStamp mLastFlingTime;
  // Indicates if the repaint-during-pinch timer is currently set
  bool mPinchPaintTimerSet;

  // Deal with overscroll resulting from a fling animation. This is only ever
  // called on APZC instances that were actually performing a fling.
  // The overscroll is handled by trying to hand the fling off to an APZC
  // later in the handoff chain, or if there are no takers, continuing the
  // fling and entering an overscrolled state.
  void HandleFlingOverscroll(const ParentLayerPoint& aVelocity,
                             const RefPtr<const OverscrollHandoffChain>& aOverscrollHandoffChain,
                             const RefPtr<const AsyncPanZoomController>& aScrolledApzc);

  void HandleSmoothScrollOverscroll(const ParentLayerPoint& aVelocity);

  // Helper function used by AttemptFling().
  void AcceptFling(FlingHandoffState& aHandoffState);

  // Start an overscroll animation with the given initial velocity.
  void StartOverscrollAnimation(const ParentLayerPoint& aVelocity);

  void SmoothScrollTo(const CSSPoint& aDestination);

  // Returns whether overscroll is allowed during an event.
  bool AllowScrollHandoffInCurrentBlock() const;

  // Invoked by the pinch repaint timer.
  void DoDelayedRequestContentRepaint();

  /* ===================================================================
   * The functions and members in this section are used to make ancestor chains
   * out of APZC instances. These chains can only be walked or manipulated
   * while holding the lock in the associated APZCTreeManager instance.
   */
public:
  void SetParent(AsyncPanZoomController* aParent) {
    mParent = aParent;
  }

  AsyncPanZoomController* GetParent() const {
    return mParent;
  }

  /* Returns true if there is no APZC higher in the tree with the same
   * layers id.
   */
  bool HasNoParentWithSameLayersId() const {
    return !mParent || (mParent->mLayersId != mLayersId);
  }

  bool IsRootForLayersId() const {
    ReentrantMonitorAutoEnter lock(mMonitor);
    return mScrollMetadata.IsLayersIdRoot();
  }

  bool IsRootContent() const {
    ReentrantMonitorAutoEnter lock(mMonitor);
    return mFrameMetrics.IsRootContent();
  }

private:
  // |mTreeManager| belongs in this section but it's declaration is a bit
  // further above due to initialization-order constraints.

  RefPtr<AsyncPanZoomController> mParent;


  /* ===================================================================
   * The functions and members in this section are used for scrolling,
   * including handing off scroll to another APZC, and overscrolling.
   */
public:
  FrameMetrics::ViewID GetScrollHandoffParentId() const {
    return mScrollMetadata.GetScrollParentId();
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
   * aStartPoint and aEndPoint are modified depending on how much of the
   * scroll gesture was consumed by APZCs in the handoff chain.
   */
  bool AttemptScroll(ParentLayerPoint& aStartPoint,
                     ParentLayerPoint& aEndPoint,
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
   *     |RefPtr<const OverscrollHandoffChain>|). This ensures the chain is
   *     kept alive. Note that queueing a task that uses the chain as an
   *     argument constitutes storing, as the task may outlive its queuer.
   *   - When passing the chain to a function that will store it, pass it as
   *     |const RefPtr<const OverscrollHandoffChain>&|. This allows the
   *     function to copy it into the |RefPtr<const OverscrollHandoffChain>|
   *     that will store it, while avoiding an unnecessary copy (and thus
   *     AddRef() and Release()) when passing it.
   */
  RefPtr<const OverscrollHandoffChain> BuildOverscrollHandoffChain();

private:
  /**
   * A helper function for calling APZCTreeManager::DispatchScroll().
   * Guards against the case where the APZC is being concurrently destroyed
   * (and thus mTreeManager is being nulled out).
   */
  void CallDispatchScroll(ParentLayerPoint& aStartPoint,
                          ParentLayerPoint& aEndPoint,
                          OverscrollHandoffState& aOverscrollHandoffState);

  /**
   * A helper function for overscrolling during panning. This is a wrapper
   * around OverscrollBy() that also implements restrictions on entering
   * overscroll based on the pan angle.
   */
  void OverscrollForPanning(ParentLayerPoint& aOverscroll,
                            const ScreenPoint& aPanDistance);

  /**
   * Try to overscroll by 'aOverscroll'.
   * If we are pannable on a particular axis, that component of 'aOverscroll'
   * is transferred to any existing overscroll.
   */
  void OverscrollBy(ParentLayerPoint& aOverscroll);


  /* ===================================================================
   * The functions and members in this section are used to maintain the
   * area that this APZC instance is responsible for. This is used when
   * hit-testing to see which APZC instance should handle touch events.
   */
public:
  void SetAncestorTransform(const Matrix4x4& aTransformToLayer) {
    mAncestorTransform = aTransformToLayer;
  }

  Matrix4x4 GetAncestorTransform() const {
    return mAncestorTransform;
  }

  // Returns whether or not this apzc contains the given screen point within
  // its composition bounds.
  bool Contains(const ScreenIntPoint& aPoint) const;

  bool IsOverscrolled() const {
    return mX.IsOverscrolled() || mY.IsOverscrolled();
  }

  bool IsInPanningState() const;

private:
  /* This is the cumulative CSS transform for all the layers from (and including)
   * the parent APZC down to (but excluding) this one, and excluding any
   * perspective transforms. */
  Matrix4x4 mAncestorTransform;


  /* ===================================================================
   * The functions and members in this section are used for sharing the
   * FrameMetrics across processes for the progressive tiling code.
   */
private:
  /* Unique id assigned to each APZC. Used with ViewID to uniquely identify
   * shared FrameMeterics used in progressive tile painting. */
  const uint32_t mAPZCId;

  RefPtr<ipc::SharedMemoryBasic> mSharedFrameMetricsBuffer;
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
   * Set an extra offset for testing async scrolling.
   */
  void SetTestAsyncScrollOffset(const CSSPoint& aPoint)
  {
    mTestAsyncScrollOffset = aPoint;
  }
  /**
   * Set an extra offset for testing async scrolling.
   */
  void SetTestAsyncZoom(const LayerToParentLayerScale& aZoom)
  {
    mTestAsyncZoom = aZoom;
  }

  void MarkAsyncTransformAppliedToContent()
  {
    mAsyncTransformAppliedToContent = true;
  }

  bool GetAsyncTransformAppliedToContent() const
  {
    return mAsyncTransformAppliedToContent;
  }

  uint64_t GetLayersId() const
  {
    return mLayersId;
  }

private:
  // Extra offset to add to the async scroll position for testing
  CSSPoint mTestAsyncScrollOffset;
  // Extra zoom to include in the aync zoom for testing
  LayerToParentLayerScale mTestAsyncZoom;
  // Flag to track whether or not the APZ transform is not used. This
  // flag is recomputed for every composition frame.
  bool mAsyncTransformAppliedToContent;


  /* ===================================================================
   * The functions and members in this section are used for checkerboard
   * recording.
   */
private:
  // Mutex protecting mCheckerboardEvent
  Mutex mCheckerboardEventLock;
  // This is created when this APZC instance is first included as part of a
  // composite. If a checkerboard event takes place, this is destroyed at the
  // end of the event, and a new one is created on the next composite.
  UniquePtr<CheckerboardEvent> mCheckerboardEvent;
  // This is used to track the total amount of time that we could reasonably
  // be checkerboarding. Combined with other info, this allows us to meaningfully
  // say how frequently users actually encounter checkerboarding.
  PotentialCheckerboardDurationTracker mPotentialCheckerboardTracker;


  /* ===================================================================
   * The functions in this section are used for CSS scroll snapping.
   */

  // If |aEvent| should trigger scroll snapping, adjust |aDelta| to reflect
  // the snapping (that is, make it a delta that will take us to the desired
  // snap point). The delta is interpreted as being relative to
  // |aStartPosition|, and if a target snap point is found, |aStartPosition|
  // is also updated, to the value of the snap point.
  // Returns true iff. a target snap point was found.
  bool MaybeAdjustDeltaForScrollSnapping(const ScrollWheelInput& aEvent,
                                         ParentLayerPoint& aDelta,
                                         CSSPoint& aStartPosition);

  // Snap to a snap position nearby the current scroll position, if appropriate.
  void ScrollSnap();

  // Snap to a snap position nearby the destination predicted based on the
  // current velocity, if appropriate.
  void ScrollSnapToDestination();

  // Snap to a snap position nearby the provided destination, if appropriate.
  void ScrollSnapNear(const CSSPoint& aDestination);

  // Find a snap point near |aDestination| that we should snap to.
  // Returns the snap point if one was found, or an empty Maybe otherwise.
  // |aUnit| affects the snapping behaviour (see ScrollSnapUtils::
  // GetSnapPointForDestination). It should generally be determined by the
  // type of event that's triggering the scroll.
  Maybe<CSSPoint> FindSnapPointNear(const CSSPoint& aDestination,
                                    nsIScrollableFrame::ScrollUnit aUnit);
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_PanZoomController_h
