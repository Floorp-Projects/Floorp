/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>                       // for fabsf, fabs, atan2
#include <stdint.h>                     // for uint32_t, uint64_t
#include <sys/types.h>                  // for int32_t
#include <algorithm>                    // for max, min
#include "AsyncPanZoomController.h"     // for AsyncPanZoomController, etc
#include "Axis.h"                       // for AxisX, AxisY, Axis, etc
#include "CheckerboardEvent.h"          // for CheckerboardEvent
#include "Compositor.h"                 // for Compositor
#include "FrameMetrics.h"               // for FrameMetrics, etc
#include "GestureEventListener.h"       // for GestureEventListener
#include "HitTestingTreeNode.h"         // for HitTestingTreeNode
#include "InputData.h"                  // for MultiTouchInput, etc
#include "InputBlockState.h"            // for InputBlockState, TouchBlockState
#include "InputQueue.h"                 // for InputQueue
#include "OverscrollHandoffState.h"     // for OverscrollHandoffState
#include "Units.h"                      // for CSSRect, CSSPoint, etc
#include "UnitTransforms.h"             // for TransformTo
#include "base/message_loop.h"          // for MessageLoop
#include "base/task.h"                  // for NewRunnableMethod, etc
#include "base/tracked.h"               // for FROM_HERE
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxTypes.h"                   // for gfxFloat
#include "LayersLogging.h"              // for print_stderr
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/BasicEvents.h"        // for Modifiers, MODIFIER_*
#include "mozilla/ClearOnShutdown.h"    // for ClearOnShutdown
#include "mozilla/ComputedTimingFunction.h" // for ComputedTimingFunction
#include "mozilla/EventForwards.h"      // for nsEventStatus_*
#include "mozilla/MouseEvents.h"        // for WidgetWheelEvent
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/ReentrantMonitor.h"   // for ReentrantMonitorAutoEnter, etc
#include "mozilla/StaticPtr.h"          // for StaticAutoPtr
#include "mozilla/Telemetry.h"          // for Telemetry
#include "mozilla/TimeStamp.h"          // for TimeDuration, TimeStamp
#include "mozilla/dom/CheckerboardReportService.h" // for CheckerboardEventStorage
             // note: CheckerboardReportService.h actually lives in gfx/layers/apz/util/
#include "mozilla/dom/Touch.h"          // for Touch
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/Point.h"          // for Point, RoundedToInt, etc
#include "mozilla/gfx/Rect.h"           // for RoundedIn
#include "mozilla/gfx/ScaleFactor.h"    // for ScaleFactor
#include "mozilla/layers/APZCTreeManager.h"  // for ScrollableLayerGuid
#include "mozilla/layers/APZThreadUtils.h"  // for AssertOnControllerThread, etc
#include "mozilla/layers/AsyncCompositionManager.h"  // for ViewTransform
#include "mozilla/layers/AxisPhysicsModel.h" // for AxisPhysicsModel
#include "mozilla/layers/AxisPhysicsMSDModel.h" // for AxisPhysicsMSDModel
#include "mozilla/layers/CompositorBridgeParent.h" // for CompositorBridgeParent
#include "mozilla/layers/LayerTransactionParent.h" // for LayerTransactionParent
#include "mozilla/layers/ScrollInputMethods.h" // for ScrollInputMethod
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "mozilla/unused.h"             // for unused
#include "mozilla/FloatingPoint.h"      // for FuzzyEquals*
#include "nsAlgorithm.h"                // for clamped
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_WARNING
#include "nsIDOMWindowUtils.h"          // for nsIDOMWindowUtils
#include "nsMathUtils.h"                // for NS_hypot
#include "nsPoint.h"                    // for nsIntPoint
#include "nsStyleConsts.h"
#include "nsStyleStruct.h"              // for nsTimingFunction
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "prsystem.h"                   // for PR_GetPhysicalMemorySize
#include "SharedMemoryBasic.h"          // for SharedMemoryBasic
#include "ScrollSnap.h"                 // for ScrollSnapUtils
#include "WheelScrollAnimation.h"

#define ENABLE_APZC_LOGGING 0
// #define ENABLE_APZC_LOGGING 1

#if ENABLE_APZC_LOGGING
#  define APZC_LOG(...) printf_stderr("APZC: " __VA_ARGS__)
#  define APZC_LOG_FM(fm, prefix, ...) \
    { std::stringstream ss; \
      ss << nsPrintfCString(prefix, __VA_ARGS__).get(); \
      AppendToString(ss, fm, ":", "", true); \
      APZC_LOG("%s\n", ss.str().c_str()); \
    }
#else
#  define APZC_LOG(...)
#  define APZC_LOG_FM(fm, prefix, ...)
#endif

namespace mozilla {
namespace layers {

typedef mozilla::layers::AllowedTouchBehavior AllowedTouchBehavior;
typedef GeckoContentController::APZStateChange APZStateChange;
typedef mozilla::gfx::Point Point;
typedef mozilla::gfx::Matrix4x4 Matrix4x4;
using mozilla::gfx::PointTyped;

/**
 * \page APZCPrefs APZ preferences
 *
 * The following prefs are used to control the behaviour of the APZC.
 * The default values are provided in gfxPrefs.h.
 *
 * \li\b apz.allow_checkerboarding
 * Pref that allows or disallows checkerboarding
 *
 * \li\b apz.allow_immediate_handoff
 * If set to true, scroll can be handed off from one APZC to another within
 * a single input block. If set to false, a single input block can only
 * scroll one APZC.
 *
 * \li\b apz.axis_lock.mode
 * The preferred axis locking style. See AxisLockMode for possible values.
 *
 * \li\b apz.axis_lock.lock_angle
 * Angle from axis within which we stay axis-locked.\n
 * Units: radians
 *
 * \li\b apz.axis_lock.breakout_threshold
 * Distance in inches the user must pan before axis lock can be broken.\n
 * Units: (real-world, i.e. screen) inches
 *
 * \li\b apz.axis_lock.breakout_angle
 * Angle at which axis lock can be broken.\n
 * Units: radians
 *
 * \li\b apz.axis_lock.direct_pan_angle
 * If the angle from an axis to the line drawn by a pan move is less than
 * this value, we can assume that panning can be done in the allowed direction
 * (horizontal or vertical).\n
 * Currently used only for touch-action css property stuff and was addded to
 * keep behaviour consistent with IE.\n
 * Units: radians
 *
 * \li\b apz.content_response_timeout
 * Amount of time before we timeout response from content. For example, if
 * content is being unruly/slow and we don't get a response back within this
 * time, we will just pretend that content did not preventDefault any touch
 * events we dispatched to it.\n
 * Units: milliseconds
 *
 * \li\b apz.danger_zone_x
 * \li\b apz.danger_zone_y
 * When drawing high-res tiles, we drop down to drawing low-res tiles
 * when we know we can't keep up with the scrolling. The way we determine
 * this is by checking if we are entering the "danger zone", which is the
 * boundary of the painted content. For example, if the painted content
 * goes from y=0...1000 and the visible portion is y=250...750 then
 * we're far from checkerboarding. If we get to y=490...990 though then we're
 * only 10 pixels away from showing checkerboarding so we are probably in
 * a state where we can't keep up with scrolling. The danger zone prefs specify
 * how wide this margin is; in the above example a y-axis danger zone of 10
 * pixels would make us drop to low-res at y=490...990.\n
 * This value is in layer pixels.
 *
 * \li\b apz.disable_for_scroll_linked_effects
 * Setting this pref to true will disable APZ scrolling on documents where
 * scroll-linked effects are detected. A scroll linked effect is detected if
 * positioning or transform properties are updated inside a scroll event
 * dispatch; we assume that such an update is in response to the scroll event
 * and is therefore a scroll-linked effect which will be laggy with APZ
 * scrolling.
 *
 * \li\b apz.displayport_expiry_ms
 * While a scrollable frame is scrolling async, we set a displayport on it
 * to make sure it is layerized. However this takes up memory, so once the
 * scrolling stops we want to remove the displayport. This pref controls how
 * long after scrolling stops the displayport is removed. A value of 0 will
 * disable the expiry behavior entirely.
 * Units: milliseconds
 *
 * \li\b apz.enlarge_displayport_when_clipped
 * Pref that enables enlarging of the displayport along one axis when the
 * generated displayport's size is beyond that of the scrollable rect on the
 * opposite axis.
 *
 * \li\b apz.fling_accel_interval_ms
 * The time that determines whether a second fling will be treated as
 * accelerated. If two flings are started within this interval, the second one
 * will be accelerated. Setting an interval of 0 means that acceleration will
 * be disabled.\n
 * Units: milliseconds
 *
 * \li\b apz.fling_accel_base_mult
 * \li\b apz.fling_accel_supplemental_mult
 * When applying an acceleration on a fling, the new computed velocity is
 * (new_fling_velocity * base_mult) + (old_velocity * supplemental_mult).
 * The base_mult and supplemental_mult multiplier values are controlled by
 * these prefs. Note that "old_velocity" here is the initial velocity of the
 * previous fling _after_ acceleration was applied to it (if applicable).
 *
 * \li\b apz.fling_curve_function_x1
 * \li\b apz.fling_curve_function_y1
 * \li\b apz.fling_curve_function_x2
 * \li\b apz.fling_curve_function_y2
 * \li\b apz.fling_curve_threshold_inches_per_ms
 * These five parameters define a Bezier curve function and threshold used to
 * increase the actual velocity relative to the user's finger velocity. When the
 * finger velocity is below the threshold (or if the threshold is not positive),
 * the velocity is used as-is. If the finger velocity exceeds the threshold
 * velocity, then the function defined by the curve is applied on the part of
 * the velocity that exceeds the threshold. Note that the upper bound of the
 * velocity is still specified by the \b apz.max_velocity_inches_per_ms pref, and
 * the function will smoothly curve the velocity from the threshold to the
 * max. In general the function parameters chosen should define an ease-out
 * curve in order to increase the velocity in this range, or an ease-in curve to
 * decrease the velocity. A straight-line curve is equivalent to disabling the
 * curve entirely by setting the threshold to -1. The max velocity pref must
 * also be set in order for the curving to take effect, as it defines the upper
 * bound of the velocity curve.\n
 * The points (x1, y1) and (x2, y2) used as the two intermediate control points
 * in the cubic bezier curve; the first and last points are (0,0) and (1,1).\n
 * Some example values for these prefs can be found at\n
 * http://mxr.mozilla.org/mozilla-central/source/layout/style/nsStyleStruct.cpp?rev=21282be9ad95#2462
 *
 * \li\b apz.fling_friction
 * Amount of friction applied during flings. This is used in the following
 * formula: v(t1) = v(t0) * (1 - f)^(t1 - t0), where v(t1) is the velocity
 * for a new sample, v(t0) is the velocity at the previous sample, f is the
 * value of this pref, and (t1 - t0) is the amount of time, in milliseconds,
 * that has elapsed between the two samples.
 *
 * \li\b apz.fling_stop_on_tap_threshold
 * When flinging, if the velocity is above this number, then a tap on the
 * screen will stop the fling without dispatching a tap to content. If the
 * velocity is below this threshold a tap will also be dispatched.
 * Note: when modifying this pref be sure to run the APZC gtests as some of
 * them depend on the value of this pref.\n
 * Units: screen pixels per millisecond
 *
 * \li\b apz.fling_stopped_threshold
 * When flinging, if the velocity goes below this number, we just stop the
 * animation completely. This is to prevent asymptotically approaching 0
 * velocity and rerendering unnecessarily.\n
 * Units: screen pixels per millisecond
 *
 * \li\b apz.max_velocity_inches_per_ms
 * Maximum velocity.  Velocity will be capped at this value if a faster fling
 * occurs.  Negative values indicate unlimited velocity.\n
 * Units: (real-world, i.e. screen) inches per millisecond
 *
 * \li\b apz.max_velocity_queue_size
 * Maximum size of velocity queue. The queue contains last N velocity records.
 * On touch end we calculate the average velocity in order to compensate
 * touch/mouse drivers misbehaviour.
 *
 * \li\b apz.min_skate_speed
 * Minimum amount of speed along an axis before we switch to "skate" multipliers
 * rather than using the "stationary" multipliers.\n
 * Units: CSS pixels per millisecond
 *
 * \li\b apz.overscroll.enabled
 * Pref that enables overscrolling. If this is disabled, excess scroll that
 * cannot be handed off is discarded.
 *
 * \li\b apz.overscroll.min_pan_distance_ratio
 * The minimum ratio of the pan distance along one axis to the pan distance
 * along the other axis needed to initiate overscroll along the first axis
 * during panning.
 *
 * \li\b apz.overscroll.stretch_factor
 * How much overscrolling can stretch content along an axis.
 * The maximum stretch along an axis is a factor of (1 + kStretchFactor).
 * (So if kStretchFactor is 0, you can't stretch at all; if kStretchFactor
 * is 1, you can stretch at most by a factor of 2).
 *
 * \li\b apz.overscroll.spring_stiffness
 * The stiffness of the spring used in the physics model for the overscroll
 * animation.
 *
 * \li\b apz.overscroll.spring_friction
 * The friction of the spring used in the physics model for the overscroll
 * animation.
 * Even though a realistic physics model would dictate that this be the same
 * as \b apz.fling_friction, we allow it to be set to be something different,
 * because in practice we want flings to skate smoothly (low friction), while
 * we want the overscroll bounce-back to oscillate few times (high friction).
 *
 * \li\b apz.overscroll.stop_distance_threshold
 * \li\b apz.overscroll.stop_velocity_threshold
 * Thresholds for stopping the overscroll animation. When both the distance
 * and the velocity fall below their thresholds, we stop oscillating.\n
 * Units: screen pixels (for distance)
 *        screen pixels per millisecond (for velocity)
 *
 * \li\b apz.paint_skipping.enabled
 * When APZ is scrolling and sending repaint requests to the main thread, often
 * the main thread doesn't actually need to do a repaint. This pref allows the
 * main thread to skip doing those repaints in cases where it doesn't need to.
 *
 * \li\b apz.record_checkerboarding
 * Whether or not to record detailed info on checkerboarding events.
 *
 * \li\b apz.test.logging_enabled
 * Enable logging of APZ test data (see bug 961289).
 *
 * \li\b apz.touch_move_tolerance
 * See the description for apz.touch_start_tolerance below. This is a similar
 * threshold, except it is used to suppress touchmove events from being delivered
 * to content for NON-scrollable frames (or more precisely, for APZCs where
 * ArePointerEventsConsumable returns false).\n
 * Units: (real-world, i.e. screen) inches
 *
 * \li\b apz.touch_start_tolerance
 * Constant describing the tolerance in distance we use, multiplied by the
 * device DPI, before we start panning the screen. This is to prevent us from
 * accidentally processing taps as touch moves, and from very short/accidental
 * touches moving the screen. touchmove events are also not delivered to content
 * within this distance on scrollable frames.\n
 * Units: (real-world, i.e. screen) inches
 *
 * \li\b apz.velocity_bias
 * How much to adjust the displayport in the direction of scrolling. This value
 * is multiplied by the velocity and added to the displayport offset.
 *
 * \li\b apz.velocity_relevance_time_ms
 * When computing a fling velocity from the most recently stored velocity
 * information, only velocities within the most X milliseconds are used.
 * This pref controls the value of X.\n
 * Units: ms
 *
 * \li\b apz.x_skate_size_multiplier
 * \li\b apz.y_skate_size_multiplier
 * The multiplier we apply to the displayport size if it is skating (current
 * velocity is above \b apz.min_skate_speed). We prefer to increase the size of
 * the Y axis because it is more natural in the case that a user is reading a
 * page page that scrolls up/down. Note that one, both or neither of these may be
 * used at any instant.\n
 * In general we want \b apz.[xy]_skate_size_multiplier to be smaller than the corresponding
 * stationary size multiplier because when panning fast we would like to paint
 * less and get faster, more predictable paint times. When panning slowly we
 * can afford to paint more even though it's slower.
 *
 * \li\b apz.x_stationary_size_multiplier
 * \li\b apz.y_stationary_size_multiplier
 * The multiplier we apply to the displayport size if it is not skating (see
 * documentation for the skate size multipliers above).
 *
 * \li\b apz.x_skate_highmem_adjust
 * \li\b apz.y_skate_highmem_adjust
 * On high memory systems, we adjust the displayport during skating
 * to be larger so we can reduce checkerboarding.
 *
 * \li\b apz.zoom_animation_duration_ms
 * This controls how long the zoom-to-rect animation takes.\n
 * Units: ms
 */

/**
 * Computed time function used for sampling frames of a zoom to animation.
 */
StaticAutoPtr<ComputedTimingFunction> gZoomAnimationFunction;

/**
 * Computed time function used for curving up velocity when it gets high.
 */
StaticAutoPtr<ComputedTimingFunction> gVelocityCurveFunction;

/**
 * The estimated duration of a paint for the purposes of calculating a new
 * displayport, in milliseconds.
 */
static const double kDefaultEstimatedPaintDurationMs = 50;

/**
 * Returns true if this is a high memory system and we can use
 * extra memory for a larger displayport to reduce checkerboarding.
 */
static bool gIsHighMemSystem = false;
static bool IsHighMemSystem()
{
  return gIsHighMemSystem;
}

/**
 * Maximum zoom amount, always used, even if a page asks for higher.
 */
static const CSSToParentLayerScale MAX_ZOOM(8.0f);

/**
 * Minimum zoom amount, always used, even if a page asks for lower.
 */
static const CSSToParentLayerScale MIN_ZOOM(0.125f);

/**
 * Is aAngle within the given threshold of the horizontal axis?
 * @param aAngle an angle in radians in the range [0, pi]
 * @param aThreshold an angle in radians in the range [0, pi/2]
 */
static bool IsCloseToHorizontal(float aAngle, float aThreshold)
{
  return (aAngle < aThreshold || aAngle > (M_PI - aThreshold));
}

// As above, but for the vertical axis.
static bool IsCloseToVertical(float aAngle, float aThreshold)
{
  return (fabs(aAngle - (M_PI / 2)) < aThreshold);
}

// Counter used to give each APZC a unique id
static uint32_t sAsyncPanZoomControllerCount = 0;

TimeStamp
AsyncPanZoomController::GetFrameTime() const
{
  APZCTreeManager* treeManagerLocal = GetApzcTreeManager();
  return treeManagerLocal ? treeManagerLocal->GetFrameTime() : TimeStamp::Now();
}

class MOZ_STACK_CLASS StateChangeNotificationBlocker {
public:
  explicit StateChangeNotificationBlocker(AsyncPanZoomController* aApzc)
    : mApzc(aApzc)
  {
    ReentrantMonitorAutoEnter lock(mApzc->mMonitor);
    mInitialState = mApzc->mState;
    mApzc->mNotificationBlockers++;
  }

  ~StateChangeNotificationBlocker()
  {
    AsyncPanZoomController::PanZoomState newState;
    {
      ReentrantMonitorAutoEnter lock(mApzc->mMonitor);
      mApzc->mNotificationBlockers--;
      newState = mApzc->mState;
    }
    mApzc->DispatchStateChangeNotification(mInitialState, newState);
  }

private:
  AsyncPanZoomController* mApzc;
  AsyncPanZoomController::PanZoomState mInitialState;
};

class FlingAnimation: public AsyncPanZoomAnimation {
public:
  FlingAnimation(AsyncPanZoomController& aApzc,
                 const RefPtr<const OverscrollHandoffChain>& aOverscrollHandoffChain,
                 bool aApplyAcceleration,
                 const RefPtr<const AsyncPanZoomController>& aScrolledApzc)
    : mApzc(aApzc)
    , mOverscrollHandoffChain(aOverscrollHandoffChain)
    , mScrolledApzc(aScrolledApzc)
  {
    MOZ_ASSERT(mOverscrollHandoffChain);
    TimeStamp now = aApzc.GetFrameTime();

    // Drop any velocity on axes where we don't have room to scroll anyways
    // (in this APZC, or an APZC further in the handoff chain).
    // This ensures that we don't take the 'overscroll' path in Sample()
    // on account of one axis which can't scroll having a velocity.
    if (!mOverscrollHandoffChain->CanScrollInDirection(&mApzc, Layer::HORIZONTAL)) {
      ReentrantMonitorAutoEnter lock(mApzc.mMonitor);
      mApzc.mX.SetVelocity(0);
    }
    if (!mOverscrollHandoffChain->CanScrollInDirection(&mApzc, Layer::VERTICAL)) {
      ReentrantMonitorAutoEnter lock(mApzc.mMonitor);
      mApzc.mY.SetVelocity(0);
    }

    ParentLayerPoint velocity = mApzc.GetVelocityVector();

    // If the last fling was very recent and in the same direction as this one,
    // boost the velocity to be the sum of the two. Check separate axes separately
    // because we could have two vertical flings with small horizontal components
    // on the opposite side of zero, and we still want the y-fling to get accelerated.
    // Note that the acceleration code is only applied on the APZC that initiates
    // the fling; the accelerated velocities are then handed off using the
    // normal DispatchFling codepath.
    if (aApplyAcceleration && !mApzc.mLastFlingTime.IsNull()
        && (now - mApzc.mLastFlingTime).ToMilliseconds() < gfxPrefs::APZFlingAccelInterval()) {
      if (SameDirection(velocity.x, mApzc.mLastFlingVelocity.x)) {
        velocity.x = Accelerate(velocity.x, mApzc.mLastFlingVelocity.x);
        APZC_LOG("%p Applying fling x-acceleration from %f to %f (delta %f)\n",
                 &mApzc, mApzc.mX.GetVelocity(), velocity.x, mApzc.mLastFlingVelocity.x);
        mApzc.mX.SetVelocity(velocity.x);
      }
      if (SameDirection(velocity.y, mApzc.mLastFlingVelocity.y)) {
        velocity.y = Accelerate(velocity.y, mApzc.mLastFlingVelocity.y);
        APZC_LOG("%p Applying fling y-acceleration from %f to %f (delta %f)\n",
                 &mApzc, mApzc.mY.GetVelocity(), velocity.y, mApzc.mLastFlingVelocity.y);
        mApzc.mY.SetVelocity(velocity.y);
      }
    }

    mApzc.mLastFlingTime = now;
    mApzc.mLastFlingVelocity = velocity;
  }

  /**
   * Advances a fling by an interpolated amount based on the passed in |aDelta|.
   * This should be called whenever sampling the content transform for this
   * frame. Returns true if the fling animation should be advanced by one frame,
   * or false if there is no fling or the fling has ended.
   */
  virtual bool DoSample(FrameMetrics& aFrameMetrics,
                        const TimeDuration& aDelta) override
  {
    float friction = gfxPrefs::APZFlingFriction();
    float threshold = gfxPrefs::APZFlingStoppedThreshold();

    bool shouldContinueFlingX = mApzc.mX.FlingApplyFrictionOrCancel(aDelta, friction, threshold),
         shouldContinueFlingY = mApzc.mY.FlingApplyFrictionOrCancel(aDelta, friction, threshold);
    // If we shouldn't continue the fling, let's just stop and repaint.
    if (!shouldContinueFlingX && !shouldContinueFlingY) {
      APZC_LOG("%p ending fling animation. overscrolled=%d\n", &mApzc, mApzc.IsOverscrolled());
      // This APZC or an APZC further down the handoff chain may be be overscrolled.
      // Start a snap-back animation on the overscrolled APZC.
      // Note:
      //   This needs to be a deferred task even though it can safely run
      //   while holding mMonitor, because otherwise, if the overscrolled APZC
      //   is this one, then the SetState(NOTHING) in UpdateAnimation will
      //   stomp on the SetState(SNAP_BACK) it does.
      if (!mDeferredTasks.append(NewRunnableMethod(mOverscrollHandoffChain.get(),
                                                   &OverscrollHandoffChain::SnapBackOverscrolledApzc,
                                                   &mApzc))) {
        MOZ_CRASH();
      }
      return false;
    }

    // AdjustDisplacement() zeroes out the Axis velocity if we're in overscroll.
    // Since we need to hand off the velocity to the tree manager in such a case,
    // we save it here. Would be ParentLayerVector instead of ParentLayerPoint
    // if we had vector classes.
    ParentLayerPoint velocity = mApzc.GetVelocityVector();

    ParentLayerPoint offset = velocity * aDelta.ToMilliseconds();

    // Ordinarily we might need to do a ScheduleComposite if either of
    // the following AdjustDisplacement calls returns true, but this
    // is already running as part of a FlingAnimation, so we'll be compositing
    // per frame of animation anyway.
    ParentLayerPoint overscroll;
    ParentLayerPoint adjustedOffset;
    mApzc.mX.AdjustDisplacement(offset.x, adjustedOffset.x, overscroll.x);
    mApzc.mY.AdjustDisplacement(offset.y, adjustedOffset.y, overscroll.y);

    aFrameMetrics.ScrollBy(adjustedOffset / aFrameMetrics.GetZoom());

    // The fling may have caused us to reach the end of our scroll range.
    if (!IsZero(overscroll)) {
      // Hand off the fling to the next APZC in the overscroll handoff chain.

      // We may have reached the end of the scroll range along one axis but
      // not the other. In such a case we only want to hand off the relevant
      // component of the fling.
      if (FuzzyEqualsAdditive(overscroll.x, 0.0f, COORDINATE_EPSILON)) {
        velocity.x = 0;
      } else if (FuzzyEqualsAdditive(overscroll.y, 0.0f, COORDINATE_EPSILON)) {
        velocity.y = 0;
      }

      // To hand off the fling, we attempt to find a target APZC and start a new
      // fling with the same velocity on that APZC. For simplicity, the actual
      // overscroll of the current sample is discarded rather than being handed
      // off. The compositor should sample animations sufficiently frequently
      // that this is not noticeable. The target APZC is chosen by seeing if
      // there is an APZC further in the handoff chain which is pannable; if
      // there isn't, we take the new fling ourselves, entering an overscrolled
      // state.
      // Note: APZC is holding mMonitor, so directly calling
      // HandleFlingOverscroll() (which acquires the tree lock) would violate
      // the lock ordering. Instead we schedule HandleFlingOverscroll() to be
      // called after mMonitor is released.
      APZC_LOG("%p fling went into overscroll, handing off with velocity %s\n", &mApzc, Stringify(velocity).c_str());
      if (!mDeferredTasks.append(NewRunnableMethod(&mApzc,
                                                   &AsyncPanZoomController::HandleFlingOverscroll,
                                                   velocity,
                                                   mOverscrollHandoffChain,
                                                   mScrolledApzc))) {
        MOZ_CRASH();
      }

      // If there is a remaining velocity on this APZC, continue this fling
      // as well. (This fling and the handed-off fling will run concurrently.)
      // Note that AdjustDisplacement() will have zeroed out the velocity
      // along the axes where we're overscrolled.
      return !IsZero(mApzc.GetVelocityVector());
    }

    return true;
  }

private:
  static bool SameDirection(float aVelocity1, float aVelocity2)
  {
    return (aVelocity1 == 0.0f)
        || (aVelocity2 == 0.0f)
        || (IsNegative(aVelocity1) == IsNegative(aVelocity2));
  }

  static float Accelerate(float aBase, float aSupplemental)
  {
    return (aBase * gfxPrefs::APZFlingAccelBaseMultiplier())
         + (aSupplemental * gfxPrefs::APZFlingAccelSupplementalMultiplier());
  }

  AsyncPanZoomController& mApzc;
  RefPtr<const OverscrollHandoffChain> mOverscrollHandoffChain;
  RefPtr<const AsyncPanZoomController> mScrolledApzc;
};

class ZoomAnimation: public AsyncPanZoomAnimation {
public:
  ZoomAnimation(CSSPoint aStartOffset, CSSToParentLayerScale2D aStartZoom,
                CSSPoint aEndOffset, CSSToParentLayerScale2D aEndZoom)
    : mTotalDuration(TimeDuration::FromMilliseconds(gfxPrefs::APZZoomAnimationDuration()))
    , mStartOffset(aStartOffset)
    , mStartZoom(aStartZoom)
    , mEndOffset(aEndOffset)
    , mEndZoom(aEndZoom)
  {}

  virtual bool DoSample(FrameMetrics& aFrameMetrics,
                        const TimeDuration& aDelta) override
  {
    mDuration += aDelta;
    double animPosition = mDuration / mTotalDuration;

    if (animPosition >= 1.0) {
      aFrameMetrics.SetZoom(mEndZoom);
      aFrameMetrics.SetScrollOffset(mEndOffset);
      return false;
    }

    // Sample the zoom at the current time point.  The sampled zoom
    // will affect the final computed resolution.
    float sampledPosition =
      gZoomAnimationFunction->GetValue(animPosition,
        ComputedTimingFunction::BeforeFlag::Unset);

    // We scale the scrollOffset linearly with sampledPosition, so the zoom
    // needs to scale inversely to match.
    aFrameMetrics.SetZoom(CSSToParentLayerScale2D(
      1 / (sampledPosition / mEndZoom.xScale + (1 - sampledPosition) / mStartZoom.xScale),
      1 / (sampledPosition / mEndZoom.yScale + (1 - sampledPosition) / mStartZoom.yScale)));

    aFrameMetrics.SetScrollOffset(CSSPoint::FromUnknownPoint(gfx::Point(
      mEndOffset.x * sampledPosition + mStartOffset.x * (1 - sampledPosition),
      mEndOffset.y * sampledPosition + mStartOffset.y * (1 - sampledPosition)
    )));

    return true;
  }

  virtual bool WantsRepaints() override
  {
    return false;
  }

private:
  TimeDuration mDuration;
  const TimeDuration mTotalDuration;

  // Old metrics from before we started a zoom animation. This is only valid
  // when we are in the "ANIMATED_ZOOM" state. This is used so that we can
  // interpolate between the start and end frames. We only use the
  // |mViewportScrollOffset| and |mResolution| fields on this.
  CSSPoint mStartOffset;
  CSSToParentLayerScale2D mStartZoom;

  // Target metrics for a zoom to animation. This is only valid when we are in
  // the "ANIMATED_ZOOM" state. We only use the |mViewportScrollOffset| and
  // |mResolution| fields on this.
  CSSPoint mEndOffset;
  CSSToParentLayerScale2D mEndZoom;
};

class OverscrollAnimation: public AsyncPanZoomAnimation {
public:
  explicit OverscrollAnimation(AsyncPanZoomController& aApzc, const ParentLayerPoint& aVelocity)
    : mApzc(aApzc)
  {
    mApzc.mX.StartOverscrollAnimation(aVelocity.x);
    mApzc.mY.StartOverscrollAnimation(aVelocity.y);
  }
  ~OverscrollAnimation()
  {
    mApzc.mX.EndOverscrollAnimation();
    mApzc.mY.EndOverscrollAnimation();
  }

  virtual bool DoSample(FrameMetrics& aFrameMetrics,
                        const TimeDuration& aDelta) override
  {
    // Can't inline these variables due to short-circuit evaluation.
    bool continueX = mApzc.mX.SampleOverscrollAnimation(aDelta);
    bool continueY = mApzc.mY.SampleOverscrollAnimation(aDelta);
    if (!continueX && !continueY) {
      // If we got into overscroll from a fling, that fling did not request a
      // fling snap to avoid a resulting scrollTo from cancelling the overscroll
      // animation too early. We do still want to request a fling snap, though,
      // in case the end of the axis at which we're overscrolled is not a valid
      // snap point, so we request one now. If there are no snap points, this will
      // do nothing. If there are snap points, we'll get a scrollTo that snaps us
      // back to the nearest valid snap point.
      // The scroll snapping is done in a deferred task, otherwise the state
      // change to NOTHING caused by the overscroll animation ending would
      // clobber a possible state change to SMOOTH_SCROLL in ScrollSnap().
      if (!mDeferredTasks.append(NewRunnableMethod(&mApzc,
                                                   &AsyncPanZoomController::ScrollSnap))) {
        MOZ_CRASH();
      }
      return false;
    }
    return true;
  }

  virtual bool WantsRepaints() override
  {
    return false;
  }

private:
  AsyncPanZoomController& mApzc;
};

class SmoothScrollAnimation : public AsyncPanZoomAnimation {
public:
  SmoothScrollAnimation(AsyncPanZoomController& aApzc,
                        const nsPoint &aInitialPosition,
                        const nsPoint &aInitialVelocity,
                        const nsPoint& aDestination, double aSpringConstant,
                        double aDampingRatio)
   : mApzc(aApzc)
   , mXAxisModel(aInitialPosition.x, aDestination.x, aInitialVelocity.x,
                 aSpringConstant, aDampingRatio)
   , mYAxisModel(aInitialPosition.y, aDestination.y, aInitialVelocity.y,
                 aSpringConstant, aDampingRatio)
  {
  }

  /**
   * Advances a smooth scroll simulation based on the time passed in |aDelta|.
   * This should be called whenever sampling the content transform for this
   * frame. Returns true if the smooth scroll should be advanced by one frame,
   * or false if the smooth scroll has ended.
   */
  bool DoSample(FrameMetrics& aFrameMetrics, const TimeDuration& aDelta) override {
    nsPoint oneParentLayerPixel =
      CSSPoint::ToAppUnits(ParentLayerPoint(1, 1) / aFrameMetrics.GetZoom());
    if (mXAxisModel.IsFinished(oneParentLayerPixel.x) &&
        mYAxisModel.IsFinished(oneParentLayerPixel.y)) {
      // Set the scroll offset to the exact destination. If we allow the scroll
      // offset to end up being a bit off from the destination, we can get
      // artefacts like "scroll to the next snap point in this direction"
      // scrolling to the snap point we're already supposed to be at.
      aFrameMetrics.SetScrollOffset(
          aFrameMetrics.CalculateScrollRange().ClampPoint(
              CSSPoint::FromAppUnits(nsPoint(mXAxisModel.GetDestination(),
                                             mYAxisModel.GetDestination()))));
      return false;
    }

    mXAxisModel.Simulate(aDelta);
    mYAxisModel.Simulate(aDelta);

    CSSPoint position = CSSPoint::FromAppUnits(nsPoint(mXAxisModel.GetPosition(),
                                                       mYAxisModel.GetPosition()));
    CSSPoint css_velocity = CSSPoint::FromAppUnits(nsPoint(mXAxisModel.GetVelocity(),
                                                           mYAxisModel.GetVelocity()));

    // Convert from points/second to points/ms
    ParentLayerPoint velocity = ParentLayerPoint(css_velocity.x, css_velocity.y) / 1000.0f;

    // Keep the velocity updated for the Axis class so that any animations
    // chained off of the smooth scroll will inherit it.
    if (mXAxisModel.IsFinished(oneParentLayerPixel.x)) {
      mApzc.mX.SetVelocity(0);
    } else {
      mApzc.mX.SetVelocity(velocity.x);
    }
    if (mYAxisModel.IsFinished(oneParentLayerPixel.y)) {
      mApzc.mY.SetVelocity(0);
    } else {
      mApzc.mY.SetVelocity(velocity.y);
    }
    // If we overscroll, hand off to a fling animation that will complete the
    // spring back.
    CSSToParentLayerScale2D zoom = aFrameMetrics.GetZoom();
    ParentLayerPoint displacement = (position - aFrameMetrics.GetScrollOffset()) * zoom;

    ParentLayerPoint overscroll;
    ParentLayerPoint adjustedOffset;
    mApzc.mX.AdjustDisplacement(displacement.x, adjustedOffset.x, overscroll.x);
    mApzc.mY.AdjustDisplacement(displacement.y, adjustedOffset.y, overscroll.y);

    aFrameMetrics.ScrollBy(adjustedOffset / zoom);

    // The smooth scroll may have caused us to reach the end of our scroll range.
    // This can happen if either the layout.css.scroll-behavior.damping-ratio
    // preference is set to less than 1 (underdamped) or if a smooth scroll
    // inherits velocity from a fling gesture.
    if (!IsZero(overscroll)) {
      // Hand off a fling with the remaining momentum to the next APZC in the
      // overscroll handoff chain.

      // We may have reached the end of the scroll range along one axis but
      // not the other. In such a case we only want to hand off the relevant
      // component of the fling.
      if (FuzzyEqualsAdditive(overscroll.x, 0.0f, COORDINATE_EPSILON)) {
        velocity.x = 0;
      } else if (FuzzyEqualsAdditive(overscroll.y, 0.0f, COORDINATE_EPSILON)) {
        velocity.y = 0;
      }

      // To hand off the fling, we attempt to find a target APZC and start a new
      // fling with the same velocity on that APZC. For simplicity, the actual
      // overscroll of the current sample is discarded rather than being handed
      // off. The compositor should sample animations sufficiently frequently
      // that this is not noticeable. The target APZC is chosen by seeing if
      // there is an APZC further in the handoff chain which is pannable; if
      // there isn't, we take the new fling ourselves, entering an overscrolled
      // state.
      // Note: APZC is holding mMonitor, so directly calling
      // HandleSmoothScrollOverscroll() (which acquires the tree lock) would violate
      // the lock ordering. Instead we schedule HandleSmoothScrollOverscroll() to be
      // called after mMonitor is released.
      if (!mDeferredTasks.append(NewRunnableMethod(&mApzc,
                                                   &AsyncPanZoomController::HandleSmoothScrollOverscroll,
                                                   velocity))) {
        MOZ_CRASH();
      }

      return false;
    }

    return true;
  }

  void SetDestination(const nsPoint& aNewDestination) {
    mXAxisModel.SetDestination(static_cast<int32_t>(aNewDestination.x));
    mYAxisModel.SetDestination(static_cast<int32_t>(aNewDestination.y));
  }

  CSSPoint GetDestination() const {
    return CSSPoint::FromAppUnits(
        nsPoint(mXAxisModel.GetDestination(), mYAxisModel.GetDestination()));
  }

  SmoothScrollAnimation* AsSmoothScrollAnimation() override {
    return this;
  }

private:
  AsyncPanZoomController& mApzc;
  AxisPhysicsMSDModel mXAxisModel, mYAxisModel;
};

/*static*/ void
AsyncPanZoomController::InitializeGlobalState()
{
  MOZ_ASSERT(NS_IsMainThread());

  static bool sInitialized = false;
  if (sInitialized)
    return;
  sInitialized = true;

  gZoomAnimationFunction = new ComputedTimingFunction();
  gZoomAnimationFunction->Init(
    nsTimingFunction(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE));
  ClearOnShutdown(&gZoomAnimationFunction);
  gVelocityCurveFunction = new ComputedTimingFunction();
  gVelocityCurveFunction->Init(
    nsTimingFunction(gfxPrefs::APZCurveFunctionX1(),
                     gfxPrefs::APZCurveFunctionY2(),
                     gfxPrefs::APZCurveFunctionX2(),
                     gfxPrefs::APZCurveFunctionY2()));
  ClearOnShutdown(&gVelocityCurveFunction);

  uint64_t sysmem = PR_GetPhysicalMemorySize();
  uint64_t threshold = 1LL << 32; // 4 GB in bytes
  gIsHighMemSystem = sysmem >= threshold;
}

AsyncPanZoomController::AsyncPanZoomController(uint64_t aLayersId,
                                               APZCTreeManager* aTreeManager,
                                               const RefPtr<InputQueue>& aInputQueue,
                                               GeckoContentController* aGeckoContentController,
                                               GestureBehavior aGestures)
  :  mLayersId(aLayersId),
     mGeckoContentController(aGeckoContentController),
     mRefPtrMonitor("RefPtrMonitor"),
     // mTreeManager must be initialized before GetFrameTime() is called
     mTreeManager(aTreeManager),
     mSharingFrameMetricsAcrossProcesses(false),
     mFrameMetrics(mScrollMetadata.GetMetrics()),
     mMonitor("AsyncPanZoomController"),
     mX(this),
     mY(this),
     mPanDirRestricted(false),
     mZoomConstraints(false, false, MIN_ZOOM, MAX_ZOOM),
     mLastSampleTime(GetFrameTime()),
     mLastCheckerboardReport(GetFrameTime()),
     mState(NOTHING),
     mNotificationBlockers(0),
     mInputQueue(aInputQueue),
     mAPZCId(sAsyncPanZoomControllerCount++),
     mSharedLock(nullptr),
     mAsyncTransformAppliedToContent(false),
     mCheckerboardEventLock("APZCBELock")
{
  if (aGestures == USE_GESTURE_DETECTOR) {
    mGestureEventListener = new GestureEventListener(this);
  }
}

AsyncPanZoomController::~AsyncPanZoomController()
{
  MOZ_ASSERT(IsDestroyed());
}

PCompositorBridgeParent*
AsyncPanZoomController::GetSharedFrameMetricsCompositor()
{
  APZThreadUtils::AssertOnCompositorThread();

  if (mSharingFrameMetricsAcrossProcesses) {
    // |state| may be null here if the CrossProcessCompositorBridgeParent has already been destroyed.
    if (const CompositorBridgeParent::LayerTreeState* state = CompositorBridgeParent::GetIndirectShadowTree(mLayersId)) {
      return state->CrossProcessPCompositorBridge();
    }
    return nullptr;
  }
  return mCompositorBridgeParent.get();
}

already_AddRefed<GeckoContentController>
AsyncPanZoomController::GetGeckoContentController() const {
  MonitorAutoLock lock(mRefPtrMonitor);
  RefPtr<GeckoContentController> controller = mGeckoContentController;
  return controller.forget();
}

already_AddRefed<GestureEventListener>
AsyncPanZoomController::GetGestureEventListener() const {
  MonitorAutoLock lock(mRefPtrMonitor);
  RefPtr<GestureEventListener> listener = mGestureEventListener;
  return listener.forget();
}

const RefPtr<InputQueue>&
AsyncPanZoomController::GetInputQueue() const {
  return mInputQueue;
}

void
AsyncPanZoomController::Destroy()
{
  APZThreadUtils::AssertOnCompositorThread();

  CancelAnimation(CancelAnimationFlags::ScrollSnap);

  { // scope the lock
    MonitorAutoLock lock(mRefPtrMonitor);
    mGeckoContentController = nullptr;
    mGestureEventListener = nullptr;
  }
  mParent = nullptr;
  mTreeManager = nullptr;

  PCompositorBridgeParent* compositor = GetSharedFrameMetricsCompositor();
  // Only send the release message if the SharedFrameMetrics has been created.
  if (compositor && mSharedFrameMetricsBuffer) {
    Unused << compositor->SendReleaseSharedCompositorFrameMetrics(mFrameMetrics.GetScrollId(), mAPZCId);
  }

  { // scope the lock
    ReentrantMonitorAutoEnter lock(mMonitor);
    mSharedFrameMetricsBuffer = nullptr;
    delete mSharedLock;
    mSharedLock = nullptr;
  }
}

bool
AsyncPanZoomController::IsDestroyed() const
{
  return mTreeManager == nullptr;
}

/* static */ScreenCoord
AsyncPanZoomController::GetTouchStartTolerance()
{
  return (gfxPrefs::APZTouchStartTolerance() * APZCTreeManager::GetDPI());
}

/* static */AsyncPanZoomController::AxisLockMode AsyncPanZoomController::GetAxisLockMode()
{
  return static_cast<AxisLockMode>(gfxPrefs::APZAxisLockMode());
}

bool
AsyncPanZoomController::ArePointerEventsConsumable(TouchBlockState* aBlock, uint32_t aTouchPoints) {
  if (aTouchPoints == 0) {
    // Cant' do anything with zero touch points
    return false;
  }

  // This logic is simplified, erring on the side of returning true
  // if we're not sure. It's safer to pretend that we can consume the
  // event and then not be able to than vice-versa.
  // We could probably enhance this logic to determine things like "we're
  // not pannable, so we can only zoom in, and the zoom is already maxed
  // out, so we're not zoomable either" but no need for that at this point.

  bool pannable = aBlock->GetOverscrollHandoffChain()->CanBePanned(this);
  bool zoomable = mZoomConstraints.mAllowZoom;

  pannable &= (aBlock->TouchActionAllowsPanningX() || aBlock->TouchActionAllowsPanningY());
  zoomable &= (aBlock->TouchActionAllowsPinchZoom());

  // XXX once we fix bug 1031443, consumable should be assigned
  // pannable || zoomable if aTouchPoints > 1.
  bool consumable = (aTouchPoints == 1 ? pannable : zoomable);
  if (!consumable) {
    return false;
  }

  return true;
}

template <typename T>
static float GetAxisStart(AsyncDragMetrics::DragDirection aDir, T aValue) {
  if (aDir == AsyncDragMetrics::HORIZONTAL) {
    return aValue.x;
  } else {
    return aValue.y;
  }
}

template <typename T>
static float GetAxisEnd(AsyncDragMetrics::DragDirection aDir, T aValue) {
  if (aDir == AsyncDragMetrics::HORIZONTAL) {
    return aValue.x + aValue.width;
  } else {
    return aValue.y + aValue.height;
  }
}

template <typename T>
static float GetAxisSize(AsyncDragMetrics::DragDirection aDir, T aValue) {
  if (aDir == AsyncDragMetrics::HORIZONTAL) {
    return aValue.width;
  } else {
    return aValue.height;
  }
}

template <typename T>
static float GetAxisScale(AsyncDragMetrics::DragDirection aDir, T aValue) {
  if (aDir == AsyncDragMetrics::HORIZONTAL) {
    return aValue.xScale;
  } else {
    return aValue.yScale;
  }
}

nsEventStatus AsyncPanZoomController::HandleDragEvent(const MouseInput& aEvent,
                                                      const AsyncDragMetrics& aDragMetrics)
{
  if (!gfxPrefs::APZDragEnabled()) {
    return nsEventStatus_eIgnore;
  }

  if (!GetApzcTreeManager()) {
    return nsEventStatus_eConsumeNoDefault;
  }

  RefPtr<HitTestingTreeNode> node =
    GetApzcTreeManager()->FindScrollNode(aDragMetrics);
  if (!node) {
    return nsEventStatus_eConsumeNoDefault;
  }

  mozilla::Telemetry::Accumulate(mozilla::Telemetry::SCROLL_INPUT_METHODS,
      (uint32_t) ScrollInputMethod::ApzScrollbarDrag);

  ReentrantMonitorAutoEnter lock(mMonitor);
  CSSPoint scrollFramePoint = aEvent.mLocalOrigin / GetFrameMetrics().GetZoom();
  // The scrollbar can be transformed with the frame but the pres shell
  // resolution is only applied to the scroll frame.
  CSSPoint scrollbarPoint = scrollFramePoint * mFrameMetrics.GetPresShellResolution();
  CSSRect cssCompositionBound = mFrameMetrics.CalculateCompositedRectInCssPixels();

  float mousePosition = GetAxisStart(aDragMetrics.mDirection, scrollbarPoint) -
                        aDragMetrics.mScrollbarDragOffset -
                        GetAxisStart(aDragMetrics.mDirection, cssCompositionBound) -
                        GetAxisStart(aDragMetrics.mDirection, aDragMetrics.mScrollTrack);

  float scrollMax = GetAxisEnd(aDragMetrics.mDirection, aDragMetrics.mScrollTrack);
  scrollMax -= node->GetScrollSize() /
               GetAxisScale(aDragMetrics.mDirection, mFrameMetrics.GetZoom()) *
               mFrameMetrics.GetPresShellResolution();

  float scrollPercent = mousePosition / scrollMax;

  float minScrollPosition =
    GetAxisStart(aDragMetrics.mDirection, mFrameMetrics.GetScrollableRect().TopLeft());
  float maxScrollPosition =
    GetAxisSize(aDragMetrics.mDirection, mFrameMetrics.GetScrollableRect()) -
    GetAxisSize(aDragMetrics.mDirection, mFrameMetrics.GetCompositionBounds());
  float scrollPosition = scrollPercent * maxScrollPosition;

  scrollPosition = std::max(scrollPosition, minScrollPosition);
  scrollPosition = std::min(scrollPosition, maxScrollPosition);

  CSSPoint scrollOffset = mFrameMetrics.GetScrollOffset();
  if (aDragMetrics.mDirection == AsyncDragMetrics::HORIZONTAL) {
    scrollOffset.x = scrollPosition;
  } else {
    scrollOffset.y = scrollPosition;
  }
  mFrameMetrics.SetScrollOffset(scrollOffset);
  ScheduleCompositeAndMaybeRepaint();
  UpdateSharedCompositorFrameMetrics();

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::HandleInputEvent(const InputData& aEvent,
                                                       const ScreenToParentLayerMatrix4x4& aTransformToApzc) {
  APZThreadUtils::AssertOnControllerThread();

  nsEventStatus rv = nsEventStatus_eIgnore;

  switch (aEvent.mInputType) {
  case MULTITOUCH_INPUT: {
    MultiTouchInput multiTouchInput = aEvent.AsMultiTouchInput();
    if (!multiTouchInput.TransformToLocal(aTransformToApzc)) {
      return rv;
    }

    RefPtr<GestureEventListener> listener = GetGestureEventListener();
    if (listener) {
      rv = listener->HandleInputEvent(multiTouchInput);
      if (rv == nsEventStatus_eConsumeNoDefault) {
        return rv;
      }
    }

    switch (multiTouchInput.mType) {
      case MultiTouchInput::MULTITOUCH_START: rv = OnTouchStart(multiTouchInput); break;
      case MultiTouchInput::MULTITOUCH_MOVE: rv = OnTouchMove(multiTouchInput); break;
      case MultiTouchInput::MULTITOUCH_END: rv = OnTouchEnd(multiTouchInput); break;
      case MultiTouchInput::MULTITOUCH_CANCEL: rv = OnTouchCancel(multiTouchInput); break;
      default: NS_WARNING("Unhandled multitouch"); break;
    }
    break;
  }
  case PANGESTURE_INPUT: {
    PanGestureInput panGestureInput = aEvent.AsPanGestureInput();
    if (!panGestureInput.TransformToLocal(aTransformToApzc)) {
      return rv;
    }

    switch (panGestureInput.mType) {
      case PanGestureInput::PANGESTURE_MAYSTART: rv = OnPanMayBegin(panGestureInput); break;
      case PanGestureInput::PANGESTURE_CANCELLED: rv = OnPanCancelled(panGestureInput); break;
      case PanGestureInput::PANGESTURE_START: rv = OnPanBegin(panGestureInput); break;
      case PanGestureInput::PANGESTURE_PAN: rv = OnPan(panGestureInput, true); break;
      case PanGestureInput::PANGESTURE_END: rv = OnPanEnd(panGestureInput); break;
      case PanGestureInput::PANGESTURE_MOMENTUMSTART: rv = OnPanMomentumStart(panGestureInput); break;
      case PanGestureInput::PANGESTURE_MOMENTUMPAN: rv = OnPan(panGestureInput, false); break;
      case PanGestureInput::PANGESTURE_MOMENTUMEND: rv = OnPanMomentumEnd(panGestureInput); break;
      default: NS_WARNING("Unhandled pan gesture"); break;
    }
    break;
  }
  case MOUSE_INPUT: {
    MouseInput mouseInput = aEvent.AsMouseInput();
    if (!mouseInput.TransformToLocal(aTransformToApzc)) {
      return rv;
    }

    // TODO Need to implement blocks to properly handle this.
    //rv = HandleDragEvent(mouseInput, dragMetrics);
    break;
  }
  case SCROLLWHEEL_INPUT: {
    ScrollWheelInput scrollInput = aEvent.AsScrollWheelInput();
    if (!scrollInput.TransformToLocal(aTransformToApzc)) { 
      return rv;
    }

    rv = OnScrollWheel(scrollInput);
    break;
  }
  case PINCHGESTURE_INPUT: {
    PinchGestureInput pinchInput = aEvent.AsPinchGestureInput();
    if (!pinchInput.TransformToLocal(aTransformToApzc)) { 
      return rv;
    }

    rv = HandleGestureEvent(pinchInput);
    break;
  }
  case TAPGESTURE_INPUT: {
    TapGestureInput tapInput = aEvent.AsTapGestureInput();
    if (!tapInput.TransformToLocal(aTransformToApzc)) { 
      return rv;
    }

    rv = HandleGestureEvent(tapInput);
    break;
  }
  default: NS_WARNING("Unhandled input event type"); break;
  }

  return rv;
}

nsEventStatus AsyncPanZoomController::HandleGestureEvent(const InputData& aEvent)
{
  APZThreadUtils::AssertOnControllerThread();

  nsEventStatus rv = nsEventStatus_eIgnore;

  switch (aEvent.mInputType) {
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
      case TapGestureInput::TAPGESTURE_LONG_UP: rv = OnLongPressUp(tapGestureInput); break;
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

  return rv;
}

void AsyncPanZoomController::HandleTouchVelocity(uint32_t aTimesampMs, float aSpeedY)
{
  mY.HandleTouchVelocity(aTimesampMs, aSpeedY);
}

nsEventStatus AsyncPanZoomController::OnTouchStart(const MultiTouchInput& aEvent) {
  APZC_LOG("%p got a touch-start in state %d\n", this, mState);
  mPanDirRestricted = false;
  ParentLayerPoint point = GetFirstTouchPoint(aEvent);

  switch (mState) {
    case FLING:
    case ANIMATING_ZOOM:
    case SMOOTH_SCROLL:
    case OVERSCROLL_ANIMATION:
    case WHEEL_SCROLL:
    case PAN_MOMENTUM:
      CurrentTouchBlock()->GetOverscrollHandoffChain()->CancelAnimations(ExcludeOverscroll);
      MOZ_FALLTHROUGH;
    case NOTHING: {
      mX.StartTouch(point.x, aEvent.mTime);
      mY.StartTouch(point.y, aEvent.mTime);
      if (RefPtr<GeckoContentController> controller = GetGeckoContentController()) {
        controller->NotifyAPZStateChange(
            GetGuid(), APZStateChange::StartTouch,
            CurrentTouchBlock()->GetOverscrollHandoffChain()->CanBePanned(this));
      }
      SetState(TOUCHING);
      break;
    }
    case TOUCHING:
    case PANNING:
    case PANNING_LOCKED_X:
    case PANNING_LOCKED_Y:
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
  APZC_LOG("%p got a touch-move in state %d\n", this, mState);
  switch (mState) {
    case FLING:
    case SMOOTH_SCROLL:
    case NOTHING:
    case ANIMATING_ZOOM:
      // May happen if the user double-taps and drags without lifting after the
      // second tap. Ignore the move if this happens.
      return nsEventStatus_eIgnore;

    case TOUCHING: {
      ScreenCoord panThreshold = GetTouchStartTolerance();
      UpdateWithTouchAtDevicePoint(aEvent);

      if (PanDistance() < panThreshold) {
        return nsEventStatus_eIgnore;
      }

      if (gfxPrefs::TouchActionEnabled() && CurrentTouchBlock()->TouchActionAllowsPanningXY()) {
        // User tries to trigger a touch behavior. If allowed touch behavior is vertical pan
        // + horizontal pan (touch-action value is equal to AUTO) we can return ConsumeNoDefault
        // status immediately to trigger cancel event further. It should happen independent of
        // the parent type (whether it is scrolling or not).
        StartPanning(aEvent);
        return nsEventStatus_eConsumeNoDefault;
      }

      return StartPanning(aEvent);
    }

    case PANNING:
    case PANNING_LOCKED_X:
    case PANNING_LOCKED_Y:
    case PAN_MOMENTUM:
      TrackTouch(aEvent);
      return nsEventStatus_eConsumeNoDefault;

    case PINCHING:
      // The scale gesture listener should have handled this.
      NS_WARNING("Gesture listener should have handled pinching in OnTouchMove.");
      return nsEventStatus_eIgnore;

    case WHEEL_SCROLL:
    case OVERSCROLL_ANIMATION:
      // Should not receive a touch-move in the OVERSCROLL_ANIMATION state
      // as touch blocks that begin in an overscrolled state cancel the
      // animation. The same is true for wheel scroll animations.
      NS_WARNING("Received impossible touch in OnTouchMove");
      break;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchEnd(const MultiTouchInput& aEvent) {
  APZC_LOG("%p got a touch-end in state %d\n", this, mState);

  RefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    controller->SetScrollingRootContent(false);
  }

  OnTouchEndOrCancel();

  // In case no touch behavior triggered previously we can avoid sending
  // scroll events or requesting content repaint. This condition is added
  // to make tests consistent - in case touch-action is NONE (and therefore
  // no pans/zooms can be performed) we expected neither scroll or repaint
  // events.
  if (mState != NOTHING) {
    ReentrantMonitorAutoEnter lock(mMonitor);
  }

  switch (mState) {
  case FLING:
    // Should never happen.
    NS_WARNING("Received impossible touch end in OnTouchEnd.");
    MOZ_FALLTHROUGH;
  case ANIMATING_ZOOM:
  case SMOOTH_SCROLL:
  case NOTHING:
    // May happen if the user double-taps and drags without lifting after the
    // second tap. Ignore if this happens.
    return nsEventStatus_eIgnore;

  case TOUCHING:
    // We may have some velocity stored on the axis from move events
    // that were not big enough to trigger scrolling. Clear that out.
    mX.SetVelocity(0);
    mY.SetVelocity(0);
    APZC_LOG("%p still has %u touch points active\n", this,
        CurrentTouchBlock()->GetActiveTouchCount());
    // In cases where the user is panning, then taps the second finger without
    // entering a pinch, we will arrive here when the second finger is lifted.
    // However the first finger is still down so we want to remain in state
    // TOUCHING.
    if (CurrentTouchBlock()->GetActiveTouchCount() == 0) {
      // It's possible we may be overscrolled if the user tapped during a
      // previous overscroll pan. Make sure to snap back in this situation.
      // An ancestor APZC could be overscrolled instead of this APZC, so
      // walk the handoff chain as well.
      CurrentTouchBlock()->GetOverscrollHandoffChain()->SnapBackOverscrolledApzc(this);
      // SnapBackOverscrolledApzc() will put any APZC it causes to snap back
      // into the OVERSCROLL_ANIMATION state. If that's not us, since we're
      // done TOUCHING enter the NOTHING state.
      if (mState != OVERSCROLL_ANIMATION) {
        SetState(NOTHING);
      }
    }
    return nsEventStatus_eIgnore;

  case PANNING:
  case PANNING_LOCKED_X:
  case PANNING_LOCKED_Y:
  case PAN_MOMENTUM:
  {
    CurrentTouchBlock()->GetOverscrollHandoffChain()->FlushRepaints();
    mX.EndTouch(aEvent.mTime);
    mY.EndTouch(aEvent.mTime);
    ParentLayerPoint flingVelocity = GetVelocityVector();
    // Clear our velocities; if DispatchFling() gives the fling to us,
    // the fling velocity gets *added* to our existing velocity in
    // AcceptFling().
    mX.SetVelocity(0);
    mY.SetVelocity(0);
    // Clear our state so that we don't stay in the PANNING state
    // if DispatchFling() gives the fling to somone else. However,
    // don't send the state change notification until we've determined
    // what our final state is to avoid notification churn.
    StateChangeNotificationBlocker blocker(this);
    SetState(NOTHING);
    APZC_LOG("%p starting a fling animation\n", this);
    // Make a local copy of the tree manager pointer and check that it's not
    // null before calling DispatchFling(). This is necessary because Destroy(),
    // which nulls out mTreeManager, could be called concurrently.
    if (APZCTreeManager* treeManagerLocal = GetApzcTreeManager()) {
      FlingHandoffState handoffState{flingVelocity,
                                     CurrentTouchBlock()->GetOverscrollHandoffChain(),
                                     false /* not handoff */,
                                     CurrentTouchBlock()->GetScrolledApzc()};
      treeManagerLocal->DispatchFling(this, handoffState);
    }
    return nsEventStatus_eConsumeNoDefault;
  }
  case PINCHING:
    SetState(NOTHING);
    // Scale gesture listener should have handled this.
    NS_WARNING("Gesture listener should have handled pinching in OnTouchEnd.");
    return nsEventStatus_eIgnore;

  case WHEEL_SCROLL:
  case OVERSCROLL_ANIMATION:
    // Should not receive a touch-end in the OVERSCROLL_ANIMATION state
    // as touch blocks that begin in an overscrolled state cancel the
    // animation. The same is true for WHEEL_SCROLL.
    NS_WARNING("Received impossible touch in OnTouchEnd");
    break;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchCancel(const MultiTouchInput& aEvent) {
  APZC_LOG("%p got a touch-cancel in state %d\n", this, mState);
  OnTouchEndOrCancel();
  CancelAnimationAndGestureState();
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnScaleBegin(const PinchGestureInput& aEvent) {
  APZC_LOG("%p got a scale-begin in state %d\n", this, mState);

  // Note that there may not be a touch block at this point, if we received the
  // PinchGestureEvent directly from widget code without any touch events.
  if (HasReadyTouchBlock() && !CurrentTouchBlock()->TouchActionAllowsPinchZoom()) {
    return nsEventStatus_eIgnore;
  }

  SetState(PINCHING);
  mX.SetVelocity(0);
  mY.SetVelocity(0);
  mLastZoomFocus = aEvent.mLocalFocusPoint - mFrameMetrics.GetCompositionBounds().TopLeft();

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnScale(const PinchGestureInput& aEvent) {
  APZC_LOG("%p got a scale in state %d\n", this, mState);

  if (HasReadyTouchBlock() && !CurrentTouchBlock()->TouchActionAllowsPinchZoom()) {
    return nsEventStatus_eIgnore;
  }

  if (mState != PINCHING) {
    return nsEventStatus_eConsumeNoDefault;
  }

  // Only the root APZC is zoomable, and the root APZC is not allowed to have
  // different x and y scales. If it did, the calculations in this function
  // would have to be adjusted (as e.g. it would no longer be valid to take
  // the minimum or maximum of the ratios of the widths and heights of the
  // page rect and the composition bounds).
  MOZ_ASSERT(mFrameMetrics.IsRootContent());
  MOZ_ASSERT(mFrameMetrics.GetZoom().AreScalesSame());

  float prevSpan = aEvent.mPreviousSpan;
  if (fabsf(prevSpan) <= EPSILON || fabsf(aEvent.mCurrentSpan) <= EPSILON) {
    // We're still handling it; we've just decided to throw this event away.
    return nsEventStatus_eConsumeNoDefault;
  }

  float spanRatio = aEvent.mCurrentSpan / aEvent.mPreviousSpan;

  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    CSSToParentLayerScale userZoom = mFrameMetrics.GetZoom().ToScaleFactor();
    ParentLayerPoint focusPoint = aEvent.mLocalFocusPoint - mFrameMetrics.GetCompositionBounds().TopLeft();
    CSSPoint cssFocusPoint = focusPoint / mFrameMetrics.GetZoom();

    ParentLayerPoint focusChange = mLastZoomFocus - focusPoint;
    // If displacing by the change in focus point will take us off page bounds,
    // then reduce the displacement such that it doesn't.
    focusChange.x -= mX.DisplacementWillOverscrollAmount(focusChange.x);
    focusChange.y -= mY.DisplacementWillOverscrollAmount(focusChange.y);
    ScrollBy(focusChange / userZoom);

    // When we zoom in with focus, we can zoom too much towards the boundaries
    // that we actually go over them. These are the needed displacements along
    // either axis such that we don't overscroll the boundaries when zooming.
    CSSPoint neededDisplacement;

    CSSToParentLayerScale realMinZoom = mZoomConstraints.mMinZoom;
    CSSToParentLayerScale realMaxZoom = mZoomConstraints.mMaxZoom;
    realMinZoom.scale = std::max(realMinZoom.scale,
                                 mFrameMetrics.GetCompositionBounds().width / mFrameMetrics.GetScrollableRect().width);
    realMinZoom.scale = std::max(realMinZoom.scale,
                                 mFrameMetrics.GetCompositionBounds().height / mFrameMetrics.GetScrollableRect().height);
    if (realMaxZoom < realMinZoom) {
      realMaxZoom = realMinZoom;
    }

    bool doScale = (spanRatio > 1.0 && userZoom < realMaxZoom) ||
                   (spanRatio < 1.0 && userZoom > realMinZoom);

    if (!mZoomConstraints.mAllowZoom) {
      doScale = false;
    }

    if (doScale) {
      spanRatio = clamped(spanRatio,
                          realMinZoom.scale / userZoom.scale,
                          realMaxZoom.scale / userZoom.scale);

      // Note that the spanRatio here should never put us into OVERSCROLL_BOTH because
      // up above we clamped it.
      neededDisplacement.x = -mX.ScaleWillOverscrollAmount(spanRatio, cssFocusPoint.x);
      neededDisplacement.y = -mY.ScaleWillOverscrollAmount(spanRatio, cssFocusPoint.y);

      ScaleWithFocus(spanRatio, cssFocusPoint);

      if (neededDisplacement != CSSPoint()) {
        ScrollBy(neededDisplacement);
      }

      ScheduleComposite();
      // We don't want to redraw on every scale, so don't use
      // RequestContentRepaint()
      UpdateSharedCompositorFrameMetrics();
    }

    mLastZoomFocus = focusPoint;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnScaleEnd(const PinchGestureInput& aEvent) {
  APZC_LOG("%p got a scale-end in state %d\n", this, mState);

  if (HasReadyTouchBlock() && !CurrentTouchBlock()->TouchActionAllowsPinchZoom()) {
    return nsEventStatus_eIgnore;
  }

  SetState(NOTHING);

  {
    ReentrantMonitorAutoEnter lock(mMonitor);
    ScheduleComposite();
    RequestContentRepaint();
    UpdateSharedCompositorFrameMetrics();
  }

  // Non-negative focus point would indicate that one finger is still down
  if (aEvent.mFocusPoint.x != -1 && aEvent.mFocusPoint.y != -1) {
    mPanDirRestricted = false;
    mX.StartTouch(aEvent.mFocusPoint.x, aEvent.mTime);
    mY.StartTouch(aEvent.mFocusPoint.y, aEvent.mTime);
    SetState(TOUCHING);
  } else {
    // Otherwise, handle the fingers being lifted.
    ReentrantMonitorAutoEnter lock(mMonitor);

    // We can get into a situation where we are overscrolled at the end of a
    // pinch if we go into overscroll with a two-finger pan, and then turn
    // that into a pinch by increasing the span sufficiently. In such a case,
    // there is no snap-back animation to get us out of overscroll, so we need
    // to get out of it somehow.
    // Moreover, in cases of scroll handoff, the overscroll can be on an APZC
    // further up in the handoff chain rather than on the current APZC, so
    // we need to clear overscroll along the entire handoff chain.
    if (HasReadyTouchBlock()) {
      CurrentTouchBlock()->GetOverscrollHandoffChain()->ClearOverscroll();
    } else {
      ClearOverscroll();
    }
    // Along with clearing the overscroll, we also want to snap to the nearest
    // snap point as appropriate.
    ScrollSnap();
  }

  return nsEventStatus_eConsumeNoDefault;
}

bool
AsyncPanZoomController::ConvertToGecko(const ScreenIntPoint& aPoint, CSSPoint* aOut)
{
  if (APZCTreeManager* treeManagerLocal = GetApzcTreeManager()) {
    ScreenToScreenMatrix4x4 transformScreenToGecko =
        treeManagerLocal->GetScreenToApzcTransform(this)
      * treeManagerLocal->GetApzcToGeckoTransform(this);
    
    Maybe<ScreenIntPoint> layoutPoint = UntransformBy(
        transformScreenToGecko, aPoint);
    if (!layoutPoint) {
      return false;
    }

    { // scoped lock to access mFrameMetrics
      ReentrantMonitorAutoEnter lock(mMonitor);
      // NOTE: This isn't *quite* LayoutDevicePoint, we just don't have a name
      // for this coordinate space and it maps the closest to LayoutDevicePoint.
      *aOut = LayoutDevicePoint(ViewAs<LayoutDevicePixel>(*layoutPoint,
                  PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent))
            / mFrameMetrics.GetDevPixelsPerCSSPixel();
    }
    return true;
  }
  return false;
}

static bool
AllowsScrollingMoreThanOnePage(double aMultiplier)
{
  const int32_t kMinAllowPageScroll =
    EventStateManager::MIN_MULTIPLIER_VALUE_ALLOWING_OVER_ONE_PAGE_SCROLL;
  return Abs(aMultiplier) >= kMinAllowPageScroll;
}

ParentLayerPoint
AsyncPanZoomController::GetScrollWheelDelta(const ScrollWheelInput& aEvent) const
{
  ParentLayerSize scrollAmount;
  ParentLayerSize pageScrollSize;
  bool isRootContent = false;

  {
    // Grab the lock to access the frame metrics.
    ReentrantMonitorAutoEnter lock(mMonitor);
    LayoutDeviceIntSize scrollAmountLD = mFrameMetrics.GetLineScrollAmount();
    LayoutDeviceIntSize pageScrollSizeLD = mFrameMetrics.GetPageScrollAmount();
    isRootContent = mFrameMetrics.IsRootContent();
    scrollAmount = scrollAmountLD /
      mFrameMetrics.GetDevPixelsPerCSSPixel() * mFrameMetrics.GetZoom();
    pageScrollSize = pageScrollSizeLD /
      mFrameMetrics.GetDevPixelsPerCSSPixel() * mFrameMetrics.GetZoom();
  }

  ParentLayerPoint delta;
  switch (aEvent.mDeltaType) {
    case ScrollWheelInput::SCROLLDELTA_LINE: {
      delta.x = aEvent.mDeltaX * scrollAmount.width;
      delta.y = aEvent.mDeltaY * scrollAmount.height;
      break;
    }
    case ScrollWheelInput::SCROLLDELTA_PAGE: {
      delta.x = aEvent.mDeltaX * pageScrollSize.width;
      delta.y = aEvent.mDeltaY * pageScrollSize.height;
      break;
    }
    case ScrollWheelInput::SCROLLDELTA_PIXEL: {
      delta = ToParentLayerCoordinates(ScreenPoint(aEvent.mDeltaX, aEvent.mDeltaY), aEvent.mOrigin);
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected scroll delta type");
  }

  // Apply user-set multipliers.
  delta.x *= aEvent.mUserDeltaMultiplierX;
  delta.y *= aEvent.mUserDeltaMultiplierY;

  // For the conditions under which we allow system scroll overrides, see
  // EventStateManager::DeltaAccumulator::ComputeScrollAmountForDefaultAction
  // and WheelTransaction::OverrideSystemScrollSpeed.
  if (isRootContent &&
      gfxPrefs::MouseWheelHasRootScrollDeltaOverride() &&
      !aEvent.IsCustomizedByUserPrefs() &&
      aEvent.mDeltaType == ScrollWheelInput::SCROLLDELTA_LINE &&
      aEvent.mAllowToOverrideSystemScrollSpeed) {
    delta.x = WidgetWheelEvent::ComputeOverriddenDelta(delta.x, false);
    delta.y = WidgetWheelEvent::ComputeOverriddenDelta(delta.y, true);
  }

  // If this is a line scroll, and this event was part of a scroll series, then
  // it might need extra acceleration. See WheelHandlingHelper.cpp.
  if (aEvent.mDeltaType == ScrollWheelInput::SCROLLDELTA_LINE &&
      aEvent.mScrollSeriesNumber > 0)
  {
    int32_t start = gfxPrefs::MouseWheelAccelerationStart();
    if (start >= 0 && aEvent.mScrollSeriesNumber >= uint32_t(start)) {
      int32_t factor = gfxPrefs::MouseWheelAccelerationFactor();
      if (factor > 0) {
        delta.x = ComputeAcceleratedWheelDelta(delta.x, aEvent.mScrollSeriesNumber, factor);
        delta.y = ComputeAcceleratedWheelDelta(delta.y, aEvent.mScrollSeriesNumber, factor);
      }
    }
  }

  // We shouldn't scroll more than one page at once except when the
  // user preference is large.
  if (!AllowsScrollingMoreThanOnePage(aEvent.mUserDeltaMultiplierX) &&
      Abs(delta.x) > pageScrollSize.width) {
    delta.x = (delta.x >= 0)
              ? pageScrollSize.width
              : -pageScrollSize.width;
  }
  if (!AllowsScrollingMoreThanOnePage(aEvent.mUserDeltaMultiplierY) &&
      Abs(delta.y) > pageScrollSize.height) {
    delta.y = (delta.y >= 0)
              ? pageScrollSize.height
              : -pageScrollSize.height;
  }

  return delta;
}

// Return whether or not the underlying layer can be scrolled on either axis.
bool
AsyncPanZoomController::CanScroll(const InputData& aEvent) const
{
  ParentLayerPoint delta;
  if (aEvent.mInputType == SCROLLWHEEL_INPUT) {
    delta = GetScrollWheelDelta(aEvent.AsScrollWheelInput());
  } else if (aEvent.mInputType == PANGESTURE_INPUT) {
    const PanGestureInput& panInput = aEvent.AsPanGestureInput();
    delta = ToParentLayerCoordinates(panInput.mPanDisplacement, panInput.mPanStartPoint);
  }
  if (!delta.x && !delta.y) {
    return false;
  }

  return CanScrollWithWheel(delta);
}

bool
AsyncPanZoomController::CanScrollWithWheel(const ParentLayerPoint& aDelta) const
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (mX.CanScroll(aDelta.x)) {
    return true;
  }
  if (mY.CanScroll(aDelta.y) && mFrameMetrics.AllowVerticalScrollWithWheel()) {
    return true;
  }
  return false;
}

bool
AsyncPanZoomController::CanScroll(Layer::ScrollDirection aDirection) const
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  switch (aDirection) {
  case Layer::HORIZONTAL: return mX.CanScroll();
  case Layer::VERTICAL:   return mY.CanScroll();
  default:                MOZ_ASSERT(false); return false;
  }
}

bool
AsyncPanZoomController::AllowScrollHandoffInCurrentBlock() const
{
  bool result = mInputQueue->AllowScrollHandoff();
  if (!gfxPrefs::APZAllowImmediateHandoff()) {
    if (InputBlockState* currentBlock = CurrentInputBlock()) {
      // Do not allow handoff beyond the first APZC to scroll.
      if (currentBlock->GetScrolledApzc() == this) {
        result = false;
      }
    }
  }
  return result;
}

static ScrollInputMethod
ScrollInputMethodForWheelDeltaType(ScrollWheelInput::ScrollDeltaType aDeltaType)
{
  switch (aDeltaType) {
    case ScrollWheelInput::SCROLLDELTA_LINE: {
      return ScrollInputMethod::ApzWheelLine;
    }
    case ScrollWheelInput::SCROLLDELTA_PAGE: {
      return ScrollInputMethod::ApzWheelPage;
    }
    case ScrollWheelInput::SCROLLDELTA_PIXEL: {
      return ScrollInputMethod::ApzWheelPixel;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected scroll delta type");
      return ScrollInputMethod::ApzWheelLine;
  }
}

nsEventStatus AsyncPanZoomController::OnScrollWheel(const ScrollWheelInput& aEvent)
{
  ParentLayerPoint delta = GetScrollWheelDelta(aEvent);
  APZC_LOG("%p got a scroll-wheel with delta %s\n", this, Stringify(delta).c_str());

  if ((delta.x || delta.y) && !CanScrollWithWheel(delta)) {
    // We can't scroll this apz anymore, so we simply drop the event.
    if (mInputQueue->GetCurrentWheelTransaction() &&
        gfxPrefs::MouseScrollTestingEnabled()) {
      if (RefPtr<GeckoContentController> controller = GetGeckoContentController()) {
        controller->NotifyMozMouseScrollEvent(
          mFrameMetrics.GetScrollId(),
          NS_LITERAL_STRING("MozMouseScrollFailed"));
      }
    }
    return nsEventStatus_eConsumeNoDefault;
  }

  if (delta.x == 0 && delta.y == 0) {
    // Avoid spurious state changes and unnecessary work
    return nsEventStatus_eIgnore;
  }

  mozilla::Telemetry::Accumulate(mozilla::Telemetry::SCROLL_INPUT_METHODS,
      (uint32_t) ScrollInputMethodForWheelDeltaType(aEvent.mDeltaType));


  switch (aEvent.mScrollMode) {
    case ScrollWheelInput::SCROLLMODE_INSTANT: {

      // Wheel events from "clicky" mouse wheels trigger scroll snapping to the
      // next snap point. Check for this, and adjust the delta to take into
      // account the snap point.
      CSSPoint startPosition = mFrameMetrics.GetScrollOffset();
      MaybeAdjustDeltaForScrollSnapping(aEvent, delta, startPosition);

      ScreenPoint distance = ToScreenCoordinates(
        ParentLayerPoint(fabs(delta.x), fabs(delta.y)), aEvent.mLocalOrigin);

      CancelAnimation();

      OverscrollHandoffState handoffState(
          *mInputQueue->CurrentWheelBlock()->GetOverscrollHandoffChain(),
          distance,
          ScrollSource::Wheel);
      ParentLayerPoint startPoint = aEvent.mLocalOrigin;
      ParentLayerPoint endPoint = aEvent.mLocalOrigin - delta;
      CallDispatchScroll(startPoint, endPoint, handoffState);

      SetState(NOTHING);

      // The calls above handle their own locking; moreover,
      // ToScreenCoordinates() and CallDispatchScroll() can grab the tree lock.
      ReentrantMonitorAutoEnter lock(mMonitor);
      RequestContentRepaint();

      break;
    }

    case ScrollWheelInput::SCROLLMODE_SMOOTH: {
      // The lock must be held across the entire update operation, so the
      // compositor doesn't end the animation before we get a chance to
      // update it.
      ReentrantMonitorAutoEnter lock(mMonitor);

      // Perform scroll snapping if appropriate.
      CSSPoint startPosition = mFrameMetrics.GetScrollOffset();
      // If we're already in a wheel scroll or smooth scroll animation,
      // the delta is applied to its destination, not to the current
      // scroll position. Take this into account when finding a snap point.
      if (mState == WHEEL_SCROLL) {
        startPosition = mAnimation->AsWheelScrollAnimation()->GetDestination();
      } else if (mState == SMOOTH_SCROLL) {
        startPosition = mAnimation->AsSmoothScrollAnimation()->GetDestination();
      }
      if (MaybeAdjustDeltaForScrollSnapping(aEvent, delta, startPosition)) {
        // If we're scroll snapping, use a smooth scroll animation to get
        // the desired physics. Note that SmoothScrollTo() will re-use an
        // existing smooth scroll animation if there is one.
        SmoothScrollTo(startPosition);
        break;
      }

      // Otherwise, use a wheel scroll animation, also reusing one if possible.
      if (mState != WHEEL_SCROLL) {
        CancelAnimation();
        SetState(WHEEL_SCROLL);

        nsPoint initialPosition = CSSPoint::ToAppUnits(mFrameMetrics.GetScrollOffset());
        StartAnimation(new WheelScrollAnimation(
          *this, initialPosition, aEvent.mDeltaType));
      }

      nsPoint deltaInAppUnits =
        CSSPoint::ToAppUnits(delta / mFrameMetrics.GetZoom());
      // Cast velocity from ParentLayerPoints/ms to CSSPoints/ms then convert to
      // appunits/second
      nsPoint velocity =
        CSSPoint::ToAppUnits(CSSPoint(mX.GetVelocity(), mY.GetVelocity())) * 1000.0f;

      WheelScrollAnimation* animation = mAnimation->AsWheelScrollAnimation();
      animation->Update(aEvent.mTimeStamp, deltaInAppUnits, nsSize(velocity.x, velocity.y));
      break;
    }
  }

  return nsEventStatus_eConsumeNoDefault;
}

void
AsyncPanZoomController::NotifyMozMouseScrollEvent(const nsString& aString) const
{
  RefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (!controller) {
    return;
  }

  controller->NotifyMozMouseScrollEvent(mFrameMetrics.GetScrollId(), aString);
}

nsEventStatus AsyncPanZoomController::OnPanMayBegin(const PanGestureInput& aEvent) {
  APZC_LOG("%p got a pan-maybegin in state %d\n", this, mState);

  mX.StartTouch(aEvent.mLocalPanStartPoint.x, aEvent.mTime);
  mY.StartTouch(aEvent.mLocalPanStartPoint.y, aEvent.mTime);
  CurrentPanGestureBlock()->GetOverscrollHandoffChain()->CancelAnimations();

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnPanCancelled(const PanGestureInput& aEvent) {
  APZC_LOG("%p got a pan-cancelled in state %d\n", this, mState);

  mX.CancelGesture();
  mY.CancelGesture();

  return nsEventStatus_eConsumeNoDefault;
}


nsEventStatus AsyncPanZoomController::OnPanBegin(const PanGestureInput& aEvent) {
  APZC_LOG("%p got a pan-begin in state %d\n", this, mState);

  if (mState == SMOOTH_SCROLL) {
    // SMOOTH_SCROLL scrolls are cancelled by pan gestures.
    CancelAnimation();
  }

  mX.StartTouch(aEvent.mLocalPanStartPoint.x, aEvent.mTime);
  mY.StartTouch(aEvent.mLocalPanStartPoint.y, aEvent.mTime);

  if (GetAxisLockMode() == FREE) {
    SetState(PANNING);
    return nsEventStatus_eConsumeNoDefault;
  }

  float dx = aEvent.mPanDisplacement.x, dy = aEvent.mPanDisplacement.y;

  if (dx || dy) {
    double angle = atan2(dy, dx); // range [-pi, pi]
    angle = fabs(angle); // range [0, pi]
    HandlePanning(angle);
  } else {
    SetState(PANNING);
  }

  // Call into OnPan in order to process any delta included in this event.
  OnPan(aEvent, true);

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnPan(const PanGestureInput& aEvent, bool aFingersOnTouchpad) {
  APZC_LOG("%p got a pan-pan in state %d\n", this, mState);

  if (mState == SMOOTH_SCROLL) {
    if (!aFingersOnTouchpad) {
      // When a SMOOTH_SCROLL scroll is being processed on a frame, mouse
      // wheel and trackpad momentum scroll position updates will not cancel the
      // SMOOTH_SCROLL scroll animations, enabling scripts that depend on
      // them to be responsive without forcing the user to wait for the momentum
      // scrolling to completely stop.
      return nsEventStatus_eConsumeNoDefault;
    }

    // SMOOTH_SCROLL scrolls are cancelled by pan gestures.
    CancelAnimation();
  }

  if (mState == NOTHING) {
    // This event block was interrupted by something else. If the user's fingers
    // are still on on the touchpad we want to resume scrolling, otherwise we
    // ignore the rest of the scroll gesture.
    if (!aFingersOnTouchpad) {
      return nsEventStatus_eConsumeNoDefault;
    }
    // Resume / restart the pan.
    // PanBegin will call back into this function with mState == PANNING.
    return OnPanBegin(aEvent);
  }

  // We need to update the axis velocity in order to get a useful display port
  // size and position. We need to do so even if this is a momentum pan (i.e.
  // aFingersOnTouchpad == false); in that case the "with touch" part is not
  // really appropriate, so we may want to rethink this at some point.
  mX.UpdateWithTouchAtDevicePoint(aEvent.mLocalPanStartPoint.x, aEvent.mLocalPanDisplacement.x, aEvent.mTime);
  mY.UpdateWithTouchAtDevicePoint(aEvent.mLocalPanStartPoint.y, aEvent.mLocalPanDisplacement.y, aEvent.mTime);

  HandlePanningUpdate(aEvent.mPanDisplacement);

  mozilla::Telemetry::Accumulate(mozilla::Telemetry::SCROLL_INPUT_METHODS,
      (uint32_t) ScrollInputMethod::ApzPanGesture);

  ScreenPoint panDistance(fabs(aEvent.mPanDisplacement.x), fabs(aEvent.mPanDisplacement.y));
  OverscrollHandoffState handoffState(
      *CurrentPanGestureBlock()->GetOverscrollHandoffChain(),
      panDistance,
      ScrollSource::Wheel);

  // Create fake "touch" positions that will result in the desired scroll motion.
  // Note that the pan displacement describes the change in scroll position:
  // positive displacement values mean that the scroll position increases.
  // However, an increase in scroll position means that the scrolled contents
  // are moved to the left / upwards. Since our simulated "touches" determine
  // the motion of the scrolled contents, not of the scroll position, they need
  // to move in the opposite direction of the pan displacement.
  ParentLayerPoint startPoint = aEvent.mLocalPanStartPoint;
  ParentLayerPoint endPoint = aEvent.mLocalPanStartPoint - aEvent.mLocalPanDisplacement;
  CallDispatchScroll(startPoint, endPoint, handoffState);

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnPanEnd(const PanGestureInput& aEvent) {
  APZC_LOG("%p got a pan-end in state %d\n", this, mState);

  // Call into OnPan in order to process any delta included in this event.
  OnPan(aEvent, true);

  mX.EndTouch(aEvent.mTime);
  mY.EndTouch(aEvent.mTime);

  // Drop any velocity on axes where we don't have room to scroll anyways
  // (in this APZC, or an APZC further in the handoff chain).
  // This ensures that we don't enlarge the display port unnecessarily.
  RefPtr<const OverscrollHandoffChain> overscrollHandoffChain =
    CurrentPanGestureBlock()->GetOverscrollHandoffChain();
  if (!overscrollHandoffChain->CanScrollInDirection(this, Layer::HORIZONTAL)) {
    mX.SetVelocity(0);
  }
  if (!overscrollHandoffChain->CanScrollInDirection(this, Layer::VERTICAL)) {
    mY.SetVelocity(0);
  }

  SetState(NOTHING);
  RequestContentRepaint();

  if (!aEvent.mFollowedByMomentum) {
    ScrollSnap();
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnPanMomentumStart(const PanGestureInput& aEvent) {
  APZC_LOG("%p got a pan-momentumstart in state %d\n", this, mState);

  if (mState == SMOOTH_SCROLL) {
    // SMOOTH_SCROLL scrolls are cancelled by pan gestures.
    CancelAnimation();
  }

  SetState(PAN_MOMENTUM);
  ScrollSnapToDestination();

  // Call into OnPan in order to process any delta included in this event.
  OnPan(aEvent, false);

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnPanMomentumEnd(const PanGestureInput& aEvent) {
  APZC_LOG("%p got a pan-momentumend in state %d\n", this, mState);

  // Call into OnPan in order to process any delta included in this event.
  OnPan(aEvent, false);

  // We need to reset the velocity to zero. We don't really have a "touch"
  // here because the touch has already ended long before the momentum
  // animation started, but I guess it doesn't really matter for now.
  mX.CancelGesture();
  mY.CancelGesture();
  SetState(NOTHING);

  RequestContentRepaint();

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnLongPress(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a long-press in state %d\n", this, mState);
  RefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    CSSPoint geckoScreenPoint;
    if (ConvertToGecko(aEvent.mPoint, &geckoScreenPoint)) {
      CancelableBlockState* block = CurrentInputBlock();
      MOZ_ASSERT(block);
      if (!block->AsTouchBlock()) {
        APZC_LOG("%p dropping long-press because some non-touch block interrupted it\n", this);
        return nsEventStatus_eIgnore;
      }
      if (block->AsTouchBlock()->IsDuringFastFling()) {
        APZC_LOG("%p dropping long-press because of fast fling\n", this);
        return nsEventStatus_eIgnore;
      }
      uint64_t blockId = GetInputQueue()->InjectNewTouchBlock(this);
      controller->HandleLongTap(geckoScreenPoint, aEvent.modifiers, GetGuid(), blockId);
      return nsEventStatus_eConsumeNoDefault;
    }
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnLongPressUp(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a long-tap-up in state %d\n", this, mState);
  return GenerateSingleTap(aEvent.mPoint, aEvent.modifiers);
}

nsEventStatus AsyncPanZoomController::GenerateSingleTap(const ScreenIntPoint& aPoint, mozilla::Modifiers aModifiers) {
  RefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    CSSPoint geckoScreenPoint;
    if (ConvertToGecko(aPoint, &geckoScreenPoint)) {
      CancelableBlockState* block = CurrentInputBlock();
      MOZ_ASSERT(block);
      TouchBlockState* touch = block->AsTouchBlock();
      // |block| may be a non-touch block in the case where this function is
      // invoked by GestureEventListener on a timeout. In that case we already
      // verified that the single tap is allowed so we let it through.
      // XXX there is a bug here that in such a case the touch block that
      // generated this tap will not get its mSingleTapOccurred flag set.
      // See https://bugzilla.mozilla.org/show_bug.cgi?id=1256344#c6
      if (touch) {
        if (touch->IsDuringFastFling()) {
          APZC_LOG("%p dropping single-tap because it was during a fast-fling\n", this);
          return nsEventStatus_eIgnore;
        }
        touch->SetSingleTapOccurred();
      }
      // Because this may be being running as part of APZCTreeManager::ReceiveInputEvent,
      // calling controller->HandleSingleTap directly might mean that content receives
      // the single tap message before the corresponding touch-up. To avoid that we
      // schedule the singletap message to run on the next spin of the event loop.
      // See bug 965381 for the issue this was causing.
      controller->PostDelayedTask(
        NewRunnableMethod(controller.get(), &GeckoContentController::HandleSingleTap,
                          geckoScreenPoint, aModifiers,
                          GetGuid()),
        0);
      return nsEventStatus_eConsumeNoDefault;
    }
  }
  return nsEventStatus_eIgnore;
}

void AsyncPanZoomController::OnTouchEndOrCancel() {
  if (RefPtr<GeckoContentController> controller = GetGeckoContentController()) {
    controller->NotifyAPZStateChange(
        GetGuid(), APZStateChange::EndTouch, CurrentTouchBlock()->SingleTapOccurred());
  }
}

nsEventStatus AsyncPanZoomController::OnSingleTapUp(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a single-tap-up in state %d\n", this, mState);
  // If mZoomConstraints.mAllowDoubleTapZoom is true we wait for a call to OnSingleTapConfirmed before
  // sending event to content
  if (!(mZoomConstraints.mAllowDoubleTapZoom && CurrentTouchBlock()->TouchActionAllowsDoubleTapZoom())) {
    return GenerateSingleTap(aEvent.mPoint, aEvent.modifiers);
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnSingleTapConfirmed(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a single-tap-confirmed in state %d\n", this, mState);
  return GenerateSingleTap(aEvent.mPoint, aEvent.modifiers);
}

nsEventStatus AsyncPanZoomController::OnDoubleTap(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a double-tap in state %d\n", this, mState);
  RefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    if (mZoomConstraints.mAllowDoubleTapZoom && CurrentTouchBlock()->TouchActionAllowsDoubleTapZoom()) {
      CSSPoint geckoScreenPoint;
      if (ConvertToGecko(aEvent.mPoint, &geckoScreenPoint)) {
        controller->HandleDoubleTap(geckoScreenPoint, aEvent.modifiers, GetGuid());
      }
    }
    return nsEventStatus_eConsumeNoDefault;
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnCancelTap(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a cancel-tap in state %d\n", this, mState);
  // XXX: Implement this.
  return nsEventStatus_eIgnore;
}


ScreenToParentLayerMatrix4x4 AsyncPanZoomController::GetTransformToThis() const {
  if (APZCTreeManager* treeManagerLocal = GetApzcTreeManager()) {
    return treeManagerLocal->GetScreenToApzcTransform(this);
  }
  return ScreenToParentLayerMatrix4x4();
}

ScreenPoint AsyncPanZoomController::ToScreenCoordinates(const ParentLayerPoint& aVector,
                                                        const ParentLayerPoint& aAnchor) const {
  return TransformVector(GetTransformToThis().Inverse(), aVector, aAnchor);
}

// TODO: figure out a good way to check the w-coordinate is positive and return the result
ParentLayerPoint AsyncPanZoomController::ToParentLayerCoordinates(const ScreenPoint& aVector,
                                                                  const ScreenPoint& aAnchor) const {
  return TransformVector(GetTransformToThis(), aVector, aAnchor);
}

bool AsyncPanZoomController::Contains(const ScreenIntPoint& aPoint) const
{
  ScreenToParentLayerMatrix4x4 transformToThis = GetTransformToThis();
  Maybe<ParentLayerIntPoint> point = UntransformBy(transformToThis, aPoint);
  if (!point) {
    return false;
  }

  ParentLayerIntRect cb;
  {
    ReentrantMonitorAutoEnter lock(mMonitor);
    GetFrameMetrics().GetCompositionBounds().ToIntRect(&cb);
  }
  return cb.Contains(*point);
}

ScreenCoord AsyncPanZoomController::PanDistance() const {
  ParentLayerPoint panVector;
  ParentLayerPoint panStart;
  {
    ReentrantMonitorAutoEnter lock(mMonitor);
    panVector = ParentLayerPoint(mX.PanDistance(), mY.PanDistance());
    panStart = PanStart();
  }
  return ToScreenCoordinates(panVector, panStart).Length();
}

ParentLayerPoint AsyncPanZoomController::PanStart() const {
  return ParentLayerPoint(mX.PanStart(), mY.PanStart());
}

const ParentLayerPoint AsyncPanZoomController::GetVelocityVector() const {
  return ParentLayerPoint(mX.GetVelocity(), mY.GetVelocity());
}

void AsyncPanZoomController::SetVelocityVector(const ParentLayerPoint& aVelocityVector) {
  mX.SetVelocity(aVelocityVector.x);
  mY.SetVelocity(aVelocityVector.y);
}

void AsyncPanZoomController::HandlePanningWithTouchAction(double aAngle) {
  // Handling of cross sliding will need to be added in this method after touch-action released
  // enabled by default.
  if (CurrentTouchBlock()->TouchActionAllowsPanningXY()) {
    if (mX.CanScrollNow() && mY.CanScrollNow()) {
      if (IsCloseToHorizontal(aAngle, gfxPrefs::APZAxisLockAngle())) {
        mY.SetAxisLocked(true);
        SetState(PANNING_LOCKED_X);
      } else if (IsCloseToVertical(aAngle, gfxPrefs::APZAxisLockAngle())) {
        mX.SetAxisLocked(true);
        SetState(PANNING_LOCKED_Y);
      } else {
        SetState(PANNING);
      }
    } else if (mX.CanScrollNow() || mY.CanScrollNow()) {
      SetState(PANNING);
    } else {
      SetState(NOTHING);
    }
  } else if (CurrentTouchBlock()->TouchActionAllowsPanningX()) {
    // Using bigger angle for panning to keep behavior consistent
    // with IE.
    if (IsCloseToHorizontal(aAngle, gfxPrefs::APZAllowedDirectPanAngle())) {
      mY.SetAxisLocked(true);
      SetState(PANNING_LOCKED_X);
      mPanDirRestricted = true;
    } else {
      // Don't treat these touches as pan/zoom movements since 'touch-action' value
      // requires it.
      SetState(NOTHING);
    }
  } else if (CurrentTouchBlock()->TouchActionAllowsPanningY()) {
    if (IsCloseToVertical(aAngle, gfxPrefs::APZAllowedDirectPanAngle())) {
      mX.SetAxisLocked(true);
      SetState(PANNING_LOCKED_Y);
      mPanDirRestricted = true;
    } else {
      SetState(NOTHING);
    }
  } else {
    SetState(NOTHING);
  }
  if (!IsInPanningState()) {
    // If we didn't enter a panning state because touch-action disallowed it,
    // make sure to clear any leftover velocity from the pre-threshold
    // touchmoves.
    mX.SetVelocity(0);
    mY.SetVelocity(0);
  }
}

void AsyncPanZoomController::HandlePanning(double aAngle) {
  ReentrantMonitorAutoEnter lock(mMonitor);
  RefPtr<const OverscrollHandoffChain> overscrollHandoffChain =
    CurrentInputBlock()->GetOverscrollHandoffChain();
  bool canScrollHorizontal = !mX.IsAxisLocked() &&
    overscrollHandoffChain->CanScrollInDirection(this, Layer::HORIZONTAL);
  bool canScrollVertical = !mY.IsAxisLocked() &&
    overscrollHandoffChain->CanScrollInDirection(this, Layer::VERTICAL);

  if (!canScrollHorizontal || !canScrollVertical) {
    SetState(PANNING);
  } else if (IsCloseToHorizontal(aAngle, gfxPrefs::APZAxisLockAngle())) {
    mY.SetAxisLocked(true);
    if (canScrollHorizontal) {
      SetState(PANNING_LOCKED_X);
    }
  } else if (IsCloseToVertical(aAngle, gfxPrefs::APZAxisLockAngle())) {
    mX.SetAxisLocked(true);
    if (canScrollVertical) {
      SetState(PANNING_LOCKED_Y);
    }
  } else {
    SetState(PANNING);
  }
}

void AsyncPanZoomController::HandlePanningUpdate(const ScreenPoint& aPanDistance) {
  // If we're axis-locked, check if the user is trying to break the lock
  if (GetAxisLockMode() == STICKY && !mPanDirRestricted) {

    double angle = atan2(aPanDistance.y, aPanDistance.x); // range [-pi, pi]
    angle = fabs(angle); // range [0, pi]

    float breakThreshold = gfxPrefs::APZAxisBreakoutThreshold() * APZCTreeManager::GetDPI();

    if (fabs(aPanDistance.x) > breakThreshold || fabs(aPanDistance.y) > breakThreshold) {
      if (mState == PANNING_LOCKED_X) {
        if (!IsCloseToHorizontal(angle, gfxPrefs::APZAxisBreakoutAngle())) {
          mY.SetAxisLocked(false);
          SetState(PANNING);
        }
      } else if (mState == PANNING_LOCKED_Y) {
        if (!IsCloseToVertical(angle, gfxPrefs::APZAxisBreakoutAngle())) {
          mX.SetAxisLocked(false);
          SetState(PANNING);
        }
      }
    }
  }
}

nsEventStatus AsyncPanZoomController::StartPanning(const MultiTouchInput& aEvent) {
  ReentrantMonitorAutoEnter lock(mMonitor);

  ParentLayerPoint point = GetFirstTouchPoint(aEvent);
  float dx = mX.PanDistance(point.x);
  float dy = mY.PanDistance(point.y);

  double angle = atan2(dy, dx); // range [-pi, pi]
  angle = fabs(angle); // range [0, pi]

  if (gfxPrefs::TouchActionEnabled()) {
    HandlePanningWithTouchAction(angle);
  } else {
    if (GetAxisLockMode() == FREE) {
      SetState(PANNING);
    } else {
      HandlePanning(angle);
    }
  }

  if (IsInPanningState()) {
    if (RefPtr<GeckoContentController> controller = GetGeckoContentController()) {
      controller->NotifyAPZStateChange(GetGuid(), APZStateChange::StartPanning);
    }
    return nsEventStatus_eConsumeNoDefault;
  }
  // Don't consume an event that didn't trigger a panning.
  return nsEventStatus_eIgnore;
}

void AsyncPanZoomController::UpdateWithTouchAtDevicePoint(const MultiTouchInput& aEvent) {
  ParentLayerPoint point = GetFirstTouchPoint(aEvent);
  mX.UpdateWithTouchAtDevicePoint(point.x, 0, aEvent.mTime);
  mY.UpdateWithTouchAtDevicePoint(point.y, 0, aEvent.mTime);
}

bool AsyncPanZoomController::AttemptScroll(ParentLayerPoint& aStartPoint,
                                           ParentLayerPoint& aEndPoint,
                                           OverscrollHandoffState& aOverscrollHandoffState) {

  // "start - end" rather than "end - start" because e.g. moving your finger
  // down (*positive* direction along y axis) causes the vertical scroll offset
  // to *decrease* as the page follows your finger.
  ParentLayerPoint displacement = aStartPoint - aEndPoint;

  ParentLayerPoint overscroll;  // will be used outside monitor block

  // If the direction of panning is reversed within the same input block,
  // a later event in the block could potentially scroll an APZC earlier
  // in the handoff chain, than an earlier event in the block (because
  // the earlier APZC was scrolled to its extent in the original direction).
  // We want to disallow this.
  bool scrollThisApzc = false;
  if (InputBlockState* block = CurrentInputBlock()) {
    scrollThisApzc = !block->GetScrolledApzc() || block->IsDownchainOfScrolledApzc(this);
  }

  if (scrollThisApzc) {
    ReentrantMonitorAutoEnter lock(mMonitor);

    ParentLayerPoint adjustedDisplacement;
    bool forceVerticalOverscroll =
      (aOverscrollHandoffState.mScrollSource == ScrollSource::Wheel &&
       !mFrameMetrics.AllowVerticalScrollWithWheel());
    bool yChanged = mY.AdjustDisplacement(displacement.y, adjustedDisplacement.y, overscroll.y,
                                          forceVerticalOverscroll);
    bool xChanged = mX.AdjustDisplacement(displacement.x, adjustedDisplacement.x, overscroll.x);

    if (xChanged || yChanged) {
      ScheduleComposite();
    }

    if (!IsZero(adjustedDisplacement)) {
      ScrollBy(adjustedDisplacement / mFrameMetrics.GetZoom());
      if (CancelableBlockState* block = CurrentInputBlock()) {
        if (block->AsTouchBlock() && (block->GetScrolledApzc() != this)) {
          RefPtr<GeckoContentController> controller = GetGeckoContentController();
          if (controller) {
            controller->SetScrollingRootContent(IsRootContent());
          }
        }
        block->SetScrolledApzc(this);
      }
      ScheduleCompositeAndMaybeRepaint();
      UpdateSharedCompositorFrameMetrics();
    }

    // Adjust the start point to reflect the consumed portion of the scroll.
    aStartPoint = aEndPoint + overscroll;
  } else {
    overscroll = displacement;
  }

  // If we consumed the entire displacement as a normal scroll, great.
  if (IsZero(overscroll)) {
    return true;
  }

  if (AllowScrollHandoffInCurrentBlock()) {
    // If there is overscroll, first try to hand it off to an APZC later
    // in the handoff chain to consume (either as a normal scroll or as
    // overscroll).
    // Note: "+ overscroll" rather than "- overscroll" because "overscroll"
    // is what's left of "displacement", and "displacement" is "start - end".
    ++aOverscrollHandoffState.mChainIndex;
    CallDispatchScroll(aStartPoint, aEndPoint, aOverscrollHandoffState);

    overscroll = aStartPoint - aEndPoint;
    if (IsZero(overscroll)) {
      return true;
    }
  }

  // If there is no APZC later in the handoff chain that accepted the
  // overscroll, try to accept it ourselves. We only accept it if we
  // are pannable.
  APZC_LOG("%p taking overscroll during panning\n", this);
  OverscrollForPanning(overscroll, aOverscrollHandoffState.mPanDistance);
  aStartPoint = aEndPoint + overscroll;

  return IsZero(overscroll);
}

void AsyncPanZoomController::OverscrollForPanning(ParentLayerPoint& aOverscroll,
                                                  const ScreenPoint& aPanDistance) {
  // Only allow entering overscroll along an axis if the pan distance along
  // that axis is greater than the pan distance along the other axis by a
  // configurable factor. If we are already overscrolled, don't check this.
  if (!IsOverscrolled()) {
    if (aPanDistance.x < gfxPrefs::APZMinPanDistanceRatio() * aPanDistance.y) {
      aOverscroll.x = 0;
    }
    if (aPanDistance.y < gfxPrefs::APZMinPanDistanceRatio() * aPanDistance.x) {
      aOverscroll.y = 0;
    }
  }

  OverscrollBy(aOverscroll);
}

void AsyncPanZoomController::OverscrollBy(ParentLayerPoint& aOverscroll) {
  if (!gfxPrefs::APZOverscrollEnabled()) {
    return;
  }

  ReentrantMonitorAutoEnter lock(mMonitor);
  // Do not go into overscroll in a direction in which we have no room to
  // scroll to begin with.
  bool xCanScroll = mX.CanScroll();
  bool yCanScroll = mY.CanScroll();
  bool xConsumed = FuzzyEqualsAdditive(aOverscroll.x, 0.0f, COORDINATE_EPSILON);
  bool yConsumed = FuzzyEqualsAdditive(aOverscroll.y, 0.0f, COORDINATE_EPSILON);

#if defined(MOZ_ANDROID_APZ)
  RefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller && ((xCanScroll && !xConsumed) || (yCanScroll && !yConsumed))) {
    controller->UpdateOverscrollOffset(aOverscroll.x, aOverscroll.y);
    aOverscroll.x = aOverscroll.y = 0.0f;
  }
#else
  if (xCanScroll && !xConsumed) {
    mX.OverscrollBy(aOverscroll.x);
    aOverscroll.x = 0;
    xConsumed = true;
  }

  if (yCanScroll && !yConsumed) {
    mY.OverscrollBy(aOverscroll.y);
    aOverscroll.y = 0;
    yConsumed = true;
  }

  if ((xCanScroll && xConsumed) || (yCanScroll && yConsumed)) {
    ScheduleComposite();
  }
#endif
}

RefPtr<const OverscrollHandoffChain> AsyncPanZoomController::BuildOverscrollHandoffChain() {
  if (APZCTreeManager* treeManagerLocal = GetApzcTreeManager()) {
    return treeManagerLocal->BuildOverscrollHandoffChain(this);
  }

  // This APZC IsDestroyed(). To avoid callers having to special-case this
  // scenario, just build a 1-element chain containing ourselves.
  OverscrollHandoffChain* result = new OverscrollHandoffChain;
  result->Add(this);
  return result;
}

void AsyncPanZoomController::AcceptFling(FlingHandoffState& aHandoffState) {
  ReentrantMonitorAutoEnter lock(mMonitor);

  // We may have a pre-existing velocity for whatever reason (for example,
  // a previously handed off fling). We don't want to clobber that.
  APZC_LOG("%p accepting fling with velocity %s\n", this,
           Stringify(aHandoffState.mVelocity).c_str());
  if (mX.CanScroll()) {
    mX.SetVelocity(mX.GetVelocity() + aHandoffState.mVelocity.x);
    aHandoffState.mVelocity.x = 0;
  }
  if (mY.CanScroll()) {
    mY.SetVelocity(mY.GetVelocity() + aHandoffState.mVelocity.y);
    aHandoffState.mVelocity.y = 0;
  }

  // If there's a scroll snap point near the predicted fling destination,
  // scroll there using a smooth scroll animation. Otherwise, start a
  // fling animation.
  ScrollSnapToDestination();
  if (mState != SMOOTH_SCROLL) {
    SetState(FLING);
    FlingAnimation *fling = new FlingAnimation(*this,
        aHandoffState.mChain,
        !aHandoffState.mIsHandoff,  // only apply acceleration if this is an initial fling
        aHandoffState.mScrolledApzc);
    StartAnimation(fling);
  }
}

bool AsyncPanZoomController::AttemptFling(FlingHandoffState& aHandoffState) {
  // If we are pannable, take over the fling ourselves.
  if (IsPannable()) {
    AcceptFling(aHandoffState);
    return true;
  }

  return false;
}

void AsyncPanZoomController::HandleFlingOverscroll(const ParentLayerPoint& aVelocity,
                                                   const RefPtr<const OverscrollHandoffChain>& aOverscrollHandoffChain,
                                                   const RefPtr<const AsyncPanZoomController>& aScrolledApzc) {
  APZCTreeManager* treeManagerLocal = GetApzcTreeManager();
  if (treeManagerLocal) {
    FlingHandoffState handoffState{aVelocity,
                                   aOverscrollHandoffChain,
                                   true /* handoff */,
                                   aScrolledApzc};
    treeManagerLocal->DispatchFling(this, handoffState);
    if (!IsZero(handoffState.mVelocity) && IsPannable() && gfxPrefs::APZOverscrollEnabled()) {
#if defined(MOZ_ANDROID_APZ)
      RefPtr<GeckoContentController> controller = GetGeckoContentController();
      if (controller) {
        controller->UpdateOverscrollVelocity(handoffState.mVelocity.x, handoffState.mVelocity.y);
      }
#else
      StartOverscrollAnimation(handoffState.mVelocity);
#endif
    }
  }
}

void AsyncPanZoomController::HandleSmoothScrollOverscroll(const ParentLayerPoint& aVelocity) {
  // We must call BuildOverscrollHandoffChain from this deferred callback
  // function in order to avoid a deadlock when acquiring the tree lock.
  HandleFlingOverscroll(aVelocity, BuildOverscrollHandoffChain(), nullptr);
}

void AsyncPanZoomController::SmoothScrollTo(const CSSPoint& aDestination) {
  if (mState == SMOOTH_SCROLL && mAnimation) {
    APZC_LOG("%p updating destination on existing animation\n", this);
    RefPtr<SmoothScrollAnimation> animation(
      static_cast<SmoothScrollAnimation*>(mAnimation.get()));
    animation->SetDestination(CSSPoint::ToAppUnits(aDestination));
  } else {
    CancelAnimation();
    SetState(SMOOTH_SCROLL);
    nsPoint initialPosition = CSSPoint::ToAppUnits(mFrameMetrics.GetScrollOffset());
    // Cast velocity from ParentLayerPoints/ms to CSSPoints/ms then convert to
    // appunits/second
    nsPoint initialVelocity = CSSPoint::ToAppUnits(CSSPoint(mX.GetVelocity(),
                                                            mY.GetVelocity())) * 1000.0f;
    nsPoint destination = CSSPoint::ToAppUnits(aDestination);

    StartAnimation(new SmoothScrollAnimation(*this,
                                             initialPosition, initialVelocity,
                                             destination,
                                             gfxPrefs::ScrollBehaviorSpringConstant(),
                                             gfxPrefs::ScrollBehaviorDampingRatio()));
  }
}

void AsyncPanZoomController::StartOverscrollAnimation(const ParentLayerPoint& aVelocity) {
  SetState(OVERSCROLL_ANIMATION);
  StartAnimation(new OverscrollAnimation(*this, aVelocity));
}

void AsyncPanZoomController::CallDispatchScroll(ParentLayerPoint& aStartPoint,
                                                ParentLayerPoint& aEndPoint,
                                                OverscrollHandoffState& aOverscrollHandoffState) {
  // Make a local copy of the tree manager pointer and check if it's not
  // null before calling DispatchScroll(). This is necessary because
  // Destroy(), which nulls out mTreeManager, could be called concurrently.
  APZCTreeManager* treeManagerLocal = GetApzcTreeManager();
  if (!treeManagerLocal) {
    return;
  }
  treeManagerLocal->DispatchScroll(this,
                                   aStartPoint, aEndPoint,
                                   aOverscrollHandoffState);
}

void AsyncPanZoomController::TrackTouch(const MultiTouchInput& aEvent) {
  ParentLayerPoint prevTouchPoint(mX.GetPos(), mY.GetPos());
  ParentLayerPoint touchPoint = GetFirstTouchPoint(aEvent);

  ScreenPoint panDistance = ToScreenCoordinates(
      ParentLayerPoint(mX.PanDistance(touchPoint.x),
                       mY.PanDistance(touchPoint.y)),
      PanStart());
  HandlePanningUpdate(panDistance);

  UpdateWithTouchAtDevicePoint(aEvent);

  if (prevTouchPoint != touchPoint) {
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::SCROLL_INPUT_METHODS,
        (uint32_t) ScrollInputMethod::ApzTouch);
    OverscrollHandoffState handoffState(
        *CurrentTouchBlock()->GetOverscrollHandoffChain(),
        panDistance,
        ScrollSource::Touch);
    CallDispatchScroll(prevTouchPoint, touchPoint, handoffState);
  }
}

ParentLayerPoint AsyncPanZoomController::GetFirstTouchPoint(const MultiTouchInput& aEvent) {
  return ((SingleTouchData&)aEvent.mTouches[0]).mLocalScreenPoint;
}

void AsyncPanZoomController::StartAnimation(AsyncPanZoomAnimation* aAnimation)
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  mAnimation = aAnimation;
  mLastSampleTime = GetFrameTime();
  ScheduleComposite();
}

void AsyncPanZoomController::CancelAnimation(CancelAnimationFlags aFlags) {
  ReentrantMonitorAutoEnter lock(mMonitor);
  APZC_LOG("%p running CancelAnimation in state %d\n", this, mState);
  SetState(NOTHING);
  mAnimation = nullptr;
  // Since there is no animation in progress now the axes should
  // have no velocity either. If we are dropping the velocity from a non-zero
  // value we should trigger a repaint as the displayport margins are dependent
  // on the velocity and the last repaint request might not have good margins
  // any more.
  bool repaint = !IsZero(GetVelocityVector());
  mX.SetVelocity(0);
  mY.SetVelocity(0);
  mX.SetAxisLocked(false);
  mY.SetAxisLocked(false);
  // Setting the state to nothing and cancelling the animation can
  // preempt normal mechanisms for relieving overscroll, so we need to clear
  // overscroll here.
  if (!(aFlags & ExcludeOverscroll) && IsOverscrolled()) {
    ClearOverscroll();
    repaint = true;
  }
  // Similar to relieving overscroll, we also need to snap to any snap points
  // if appropriate.
  if (aFlags & CancelAnimationFlags::ScrollSnap) {
    ScrollSnap();
  }
  if (repaint) {
    RequestContentRepaint();
    ScheduleComposite();
    UpdateSharedCompositorFrameMetrics();
  }
}

void AsyncPanZoomController::ClearOverscroll() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  mX.ClearOverscroll();
  mY.ClearOverscroll();
}

void AsyncPanZoomController::SetCompositorBridgeParent(CompositorBridgeParent* aCompositorBridgeParent) {
  mCompositorBridgeParent = aCompositorBridgeParent;
}

void AsyncPanZoomController::ShareFrameMetricsAcrossProcesses() {
  mSharingFrameMetricsAcrossProcesses = true;
}

void AsyncPanZoomController::AdjustScrollForSurfaceShift(const ScreenPoint& aShift)
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  CSSPoint adjustment =
    ViewAs<ParentLayerPixel>(aShift, PixelCastJustification::ScreenIsParentLayerForRoot)
    / mFrameMetrics.GetZoom();
  APZC_LOG("%p adjusting scroll position by %s for surface shift\n",
    this, Stringify(adjustment).c_str());
  CSSPoint scrollOffset = mFrameMetrics.GetScrollOffset();
  scrollOffset.y = mY.ClampOriginToScrollableRect(scrollOffset.y + adjustment.y);
  scrollOffset.x = mX.ClampOriginToScrollableRect(scrollOffset.x + adjustment.x);
  mFrameMetrics.SetScrollOffset(scrollOffset);
  ScheduleCompositeAndMaybeRepaint();
  UpdateSharedCompositorFrameMetrics();
}

void AsyncPanZoomController::ScrollBy(const CSSPoint& aOffset) {
  mFrameMetrics.ScrollBy(aOffset);
}

void AsyncPanZoomController::ScaleWithFocus(float aScale,
                                            const CSSPoint& aFocus) {
  mFrameMetrics.ZoomBy(aScale);
  // We want to adjust the scroll offset such that the CSS point represented by aFocus remains
  // at the same position on the screen before and after the change in zoom. The below code
  // accomplishes this; see https://bugzilla.mozilla.org/show_bug.cgi?id=923431#c6 for an
  // in-depth explanation of how.
  mFrameMetrics.SetScrollOffset((mFrameMetrics.GetScrollOffset() + aFocus) - (aFocus / aScale));
}

/**
 * Enlarges the displayport along both axes based on the velocity.
 */
static CSSSize
CalculateDisplayPortSize(const CSSSize& aCompositionSize,
                         const CSSPoint& aVelocity)
{
  bool xIsStationarySpeed = fabsf(aVelocity.x) < gfxPrefs::APZMinSkateSpeed();
  bool yIsStationarySpeed = fabsf(aVelocity.y) < gfxPrefs::APZMinSkateSpeed();
  float xMultiplier = xIsStationarySpeed
                        ? gfxPrefs::APZXStationarySizeMultiplier()
                        : gfxPrefs::APZXSkateSizeMultiplier();
  float yMultiplier = yIsStationarySpeed
                        ? gfxPrefs::APZYStationarySizeMultiplier()
                        : gfxPrefs::APZYSkateSizeMultiplier();

  if (IsHighMemSystem() && !xIsStationarySpeed) {
    xMultiplier += gfxPrefs::APZXSkateHighMemAdjust();
  }

  if (IsHighMemSystem() && !yIsStationarySpeed) {
    yMultiplier += gfxPrefs::APZYSkateHighMemAdjust();
  }

  // Ensure that it is at least as large as the visible area inflated by the
  // danger zone. If this is not the case then the "AboutToCheckerboard"
  // function in TiledContentClient.cpp will return true even in the stable
  // state.
  float xSize = std::max(aCompositionSize.width * xMultiplier,
                         aCompositionSize.width + (2 * gfxPrefs::APZDangerZoneX()));
  float ySize = std::max(aCompositionSize.height * yMultiplier,
                         aCompositionSize.height + (2 * gfxPrefs::APZDangerZoneY()));

  return CSSSize(xSize, ySize);
}

/**
 * Attempts to redistribute any area in the displayport that would get clipped
 * by the scrollable rect, or be inaccessible due to disabled scrolling, to the
 * other axis, while maintaining total displayport area.
 */
static void
RedistributeDisplayPortExcess(CSSSize& aDisplayPortSize,
                              const CSSRect& aScrollableRect)
{
  // As aDisplayPortSize.height * aDisplayPortSize.width does not change,
  // we are just scaling by the ratio and its inverse.
  if (aDisplayPortSize.height > aScrollableRect.height) {
    aDisplayPortSize.width *= (aDisplayPortSize.height / aScrollableRect.height);
    aDisplayPortSize.height = aScrollableRect.height;
  } else if (aDisplayPortSize.width > aScrollableRect.width) {
    aDisplayPortSize.height *= (aDisplayPortSize.width / aScrollableRect.width);
    aDisplayPortSize.width = aScrollableRect.width;
  }
}

/* static */
const ScreenMargin AsyncPanZoomController::CalculatePendingDisplayPort(
  const FrameMetrics& aFrameMetrics,
  const ParentLayerPoint& aVelocity)
{
  if (aFrameMetrics.IsScrollInfoLayer()) {
    // Don't compute margins. Since we can't asynchronously scroll this frame,
    // we don't want to paint anything more than the composition bounds.
    return ScreenMargin();
  }

  CSSSize compositionSize = aFrameMetrics.CalculateBoundedCompositedSizeInCssPixels();
  CSSPoint velocity = aVelocity / aFrameMetrics.GetZoom();
  CSSRect scrollableRect = aFrameMetrics.GetExpandedScrollableRect();

  // Calculate the displayport size based on how fast we're moving along each axis.
  CSSSize displayPortSize = CalculateDisplayPortSize(compositionSize, velocity);

  if (gfxPrefs::APZEnlargeDisplayPortWhenClipped()) {
    RedistributeDisplayPortExcess(displayPortSize, scrollableRect);
  }

  // We calculate a "displayport" here which is relative to the scroll offset.
  // Note that the scroll offset we have here in the APZ code may not be the
  // same as the base rect that gets used on the layout side when the displayport
  // margins are actually applied, so it is important to only consider the
  // displayport as margins relative to a scroll offset rather than relative to
  // something more unchanging like the scrollable rect origin.

  // Center the displayport based on its expansion over the composition size.
  CSSRect displayPort((compositionSize.width - displayPortSize.width) / 2.0f,
                      (compositionSize.height - displayPortSize.height) / 2.0f,
                      displayPortSize.width, displayPortSize.height);

  // Offset the displayport, depending on how fast we're moving and the
  // estimated time it takes to paint, to try to minimise checkerboarding.
  float paintFactor = kDefaultEstimatedPaintDurationMs;
  displayPort.MoveBy(velocity * paintFactor * gfxPrefs::APZVelocityBias());

  APZC_LOG_FM(aFrameMetrics,
    "Calculated displayport as (%f %f %f %f) from velocity %s paint time %f metrics",
    displayPort.x, displayPort.y, displayPort.width, displayPort.height,
    ToString(aVelocity).c_str(), paintFactor);

  CSSMargin cssMargins;
  cssMargins.left = -displayPort.x;
  cssMargins.top = -displayPort.y;
  cssMargins.right = displayPort.width - compositionSize.width - cssMargins.left;
  cssMargins.bottom = displayPort.height - compositionSize.height - cssMargins.top;

  return cssMargins * aFrameMetrics.DisplayportPixelsPerCSSPixel();
}

void AsyncPanZoomController::ScheduleComposite() {
  if (mCompositorBridgeParent) {
    mCompositorBridgeParent->ScheduleRenderOnCompositorThread();
  }
}

void AsyncPanZoomController::ScheduleCompositeAndMaybeRepaint() {
  ScheduleComposite();
  RequestContentRepaint();
}

void AsyncPanZoomController::FlushRepaintForOverscrollHandoff() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  RequestContentRepaint();
  UpdateSharedCompositorFrameMetrics();
}

void AsyncPanZoomController::FlushRepaintForNewInputBlock() {
  APZC_LOG("%p flushing repaint for new input block\n", this);

  ReentrantMonitorAutoEnter lock(mMonitor);
  RequestContentRepaint();
  UpdateSharedCompositorFrameMetrics();
}

bool AsyncPanZoomController::SnapBackIfOverscrolled() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  // It's possible that we're already in the middle of an overscroll
  // animation - if so, don't start a new one.
  if (IsOverscrolled() && mState != OVERSCROLL_ANIMATION) {
    APZC_LOG("%p is overscrolled, starting snap-back\n", this);
    StartOverscrollAnimation(ParentLayerPoint(0, 0));
    return true;
  }
  // If we don't kick off an overscroll animation, we still need to ask the
  // main thread to snap to any nearby snap points, assuming we haven't already
  // done so when we started this fling
  if (mState != FLING) {
    ScrollSnap();
  }
  return false;
}

bool AsyncPanZoomController::IsFlingingFast() const {
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (mState == FLING &&
      GetVelocityVector().Length() > gfxPrefs::APZFlingStopOnTapThreshold()) {
    APZC_LOG("%p is moving fast\n", this);
    return true;
  }
  return false;
}

bool AsyncPanZoomController::IsPannable() const {
  ReentrantMonitorAutoEnter lock(mMonitor);
  return mX.CanScroll() || mY.CanScroll();
}

int32_t AsyncPanZoomController::GetLastTouchIdentifier() const {
  RefPtr<GestureEventListener> listener = GetGestureEventListener();
  return listener ? listener->GetLastTouchIdentifier() : -1;
}

void AsyncPanZoomController::RequestContentRepaint() {
  // Reinvoke this method on the main thread if it's not there already. It's
  // important to do this before the call to CalculatePendingDisplayPort, so
  // that CalculatePendingDisplayPort uses the most recent available version of
  // mFrameMetrics, just before the paint request is dispatched to content.
  if (!NS_IsMainThread()) {
    // use the local variable to resolve the function overload.
    auto func = static_cast<void (AsyncPanZoomController::*)()>
        (&AsyncPanZoomController::RequestContentRepaint);
    NS_DispatchToMainThread(NS_NewRunnableMethod(this, func));
    return;
  }

  MOZ_ASSERT(NS_IsMainThread());

  ReentrantMonitorAutoEnter lock(mMonitor);
  ParentLayerPoint velocity = GetVelocityVector();
  mFrameMetrics.SetDisplayPortMargins(CalculatePendingDisplayPort(mFrameMetrics, velocity));
  mFrameMetrics.SetUseDisplayPortMargins(true);
  mFrameMetrics.SetPaintRequestTime(TimeStamp::Now());
  RequestContentRepaint(mFrameMetrics, velocity);
}

/*static*/ CSSRect
GetDisplayPortRect(const FrameMetrics& aFrameMetrics)
{
  // This computation is based on what happens in CalculatePendingDisplayPort. If that
  // changes then this might need to change too
  CSSRect baseRect(aFrameMetrics.GetScrollOffset(),
                   aFrameMetrics.CalculateBoundedCompositedSizeInCssPixels());
  baseRect.Inflate(aFrameMetrics.GetDisplayPortMargins() / aFrameMetrics.DisplayportPixelsPerCSSPixel());
  return baseRect;
}

void
AsyncPanZoomController::RequestContentRepaint(const FrameMetrics& aFrameMetrics,
                                              const ParentLayerPoint& aVelocity)
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we're trying to paint what we already think is painted, discard this
  // request since it's a pointless paint.
  ScreenMargin marginDelta = (mLastPaintRequestMetrics.GetDisplayPortMargins()
                           - aFrameMetrics.GetDisplayPortMargins());
  if (fabsf(marginDelta.left) < EPSILON &&
      fabsf(marginDelta.top) < EPSILON &&
      fabsf(marginDelta.right) < EPSILON &&
      fabsf(marginDelta.bottom) < EPSILON &&
      fabsf(mLastPaintRequestMetrics.GetScrollOffset().x -
            aFrameMetrics.GetScrollOffset().x) < EPSILON &&
      fabsf(mLastPaintRequestMetrics.GetScrollOffset().y -
            aFrameMetrics.GetScrollOffset().y) < EPSILON &&
      aFrameMetrics.GetPresShellResolution() == mLastPaintRequestMetrics.GetPresShellResolution() &&
      aFrameMetrics.GetZoom() == mLastPaintRequestMetrics.GetZoom() &&
      fabsf(aFrameMetrics.GetViewport().width -
            mLastPaintRequestMetrics.GetViewport().width) < EPSILON &&
      fabsf(aFrameMetrics.GetViewport().height -
            mLastPaintRequestMetrics.GetViewport().height) < EPSILON &&
      aFrameMetrics.GetScrollGeneration() == mLastPaintRequestMetrics.GetScrollGeneration()) {
    return;
  }

  RefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (!controller) {
    return;
  }

  APZC_LOG_FM(aFrameMetrics, "%p requesting content repaint", this);
  { // scope lock
    MutexAutoLock lock(mCheckerboardEventLock);
    if (mCheckerboardEvent && mCheckerboardEvent->IsRecordingTrace()) {
      std::stringstream info;
      info << " velocity " << aVelocity;
      std::string str = info.str();
      mCheckerboardEvent->UpdateRendertraceProperty(
          CheckerboardEvent::RequestedDisplayPort, GetDisplayPortRect(aFrameMetrics),
          str);
    }
  }

  controller->RequestContentRepaint(aFrameMetrics);
  mExpectedGeckoMetrics = aFrameMetrics;
  mLastPaintRequestMetrics = aFrameMetrics;
}

bool AsyncPanZoomController::UpdateAnimation(const TimeStamp& aSampleTime,
                                             Vector<Task*>* aOutDeferredTasks)
{
  APZThreadUtils::AssertOnCompositorThread();

  // This function may get called multiple with the same sample time, because
  // there may be multiple layers with this APZC, and each layer invokes this
  // function during composition. However we only want to do one animation step
  // per composition so we need to deduplicate these calls first.
  if (mLastSampleTime == aSampleTime) {
    return false;
  }
  TimeDuration sampleTimeDelta = aSampleTime - mLastSampleTime;
  mLastSampleTime = aSampleTime;

  if (mAnimation) {
    bool continueAnimation = mAnimation->Sample(mFrameMetrics, sampleTimeDelta);
    bool wantsRepaints = mAnimation->WantsRepaints();
    *aOutDeferredTasks = mAnimation->TakeDeferredTasks();
    if (!continueAnimation) {
      mAnimation = nullptr;
      SetState(NOTHING);
    }
    // Request a repaint at the end of the animation in case something such as a
    // call to NotifyLayersUpdated was invoked during the animation and Gecko's
    // current state is some intermediate point of the animation.
    if (!continueAnimation || wantsRepaints) {
      RequestContentRepaint();
    }
    UpdateSharedCompositorFrameMetrics();
    return true;
  }
  return false;
}

AsyncTransformComponentMatrix
AsyncPanZoomController::GetOverscrollTransform(AsyncMode aMode) const
{
  ReentrantMonitorAutoEnter lock(mMonitor);

  if (aMode == RESPECT_FORCE_DISABLE && mFrameMetrics.IsApzForceDisabled()) {
    return AsyncTransformComponentMatrix();
  }

  if (!IsOverscrolled()) {
    return AsyncTransformComponentMatrix();
  }

  // The overscroll effect is a uniform stretch along the overscrolled axis,
  // with the edge of the content where we have reached the end of the
  // scrollable area pinned into place.

  // The kStretchFactor parameter determines how much overscroll can stretch the
  // content.
  const float kStretchFactor = gfxPrefs::APZOverscrollStretchFactor();

  // Compute the amount of the stretch along each axis. The stretch is
  // proportional to the amount by which we are overscrolled along that axis.
  ParentLayerSize compositionSize(mX.GetCompositionLength(), mY.GetCompositionLength());
  float scaleX = 1 + kStretchFactor * fabsf(mX.GetOverscroll()) / mX.GetCompositionLength();
  float scaleY = 1 + kStretchFactor * fabsf(mY.GetOverscroll()) / mY.GetCompositionLength();

  // The scale is applied relative to the origin of the composition bounds, i.e.
  // it keeps the top-left corner of the content in place. This is fine if we
  // are overscrolling at the top or on the left, but if we are overscrolling
  // at the bottom or on the right, we want the bottom or right edge of the
  // content to stay in place instead, so we add a translation to compensate.
  ParentLayerPoint translation;
  bool overscrolledOnRight = mX.GetOverscroll() > 0;
  if (overscrolledOnRight) {
    ParentLayerCoord overscrolledCompositionWidth = scaleX * compositionSize.width;
    ParentLayerCoord extraCompositionWidth = overscrolledCompositionWidth - compositionSize.width;
    translation.x = -extraCompositionWidth;
  }
  bool overscrolledAtBottom = mY.GetOverscroll() > 0;
  if (overscrolledAtBottom) {
    ParentLayerCoord overscrolledCompositionHeight = scaleY * compositionSize.height;
    ParentLayerCoord extraCompositionHeight = overscrolledCompositionHeight - compositionSize.height;
    translation.y = -extraCompositionHeight;
  }

  // Combine the transformations into a matrix.
  return AsyncTransformComponentMatrix::Scaling(scaleX, scaleY, 1)
                    .PostTranslate(translation.x, translation.y, 0);
}

bool AsyncPanZoomController::AdvanceAnimations(const TimeStamp& aSampleTime)
{
  APZThreadUtils::AssertOnCompositorThread();

  // Don't send any state-change notifications until the end of the function,
  // because we may go through some intermediate states while we finish
  // animations and start new ones.
  StateChangeNotificationBlocker blocker(this);

  // The eventual return value of this function. The compositor needs to know
  // whether or not to advance by a frame as soon as it can. For example, if a
  // fling is happening, it has to keep compositing so that the animation is
  // smooth. If an animation frame is requested, it is the compositor's
  // responsibility to schedule a composite.
  mAsyncTransformAppliedToContent = false;
  bool requestAnimationFrame = false;
  Vector<Task*> deferredTasks;

  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    requestAnimationFrame = UpdateAnimation(aSampleTime, &deferredTasks);

    { // scope lock
      MutexAutoLock lock(mCheckerboardEventLock);
      if (mCheckerboardEvent) {
        mCheckerboardEvent->UpdateRendertraceProperty(
            CheckerboardEvent::UserVisible,
            CSSRect(mFrameMetrics.GetScrollOffset(),
                    mFrameMetrics.CalculateCompositedSizeInCssPixels()));
      }
    }
  }

  // Execute any deferred tasks queued up by mAnimation's Sample() (called by
  // UpdateAnimation()). This needs to be done after the monitor is released
  // since the tasks are allowed to call APZCTreeManager methods which can grab
  // the tree lock.
  for (uint32_t i = 0; i < deferredTasks.length(); ++i) {
    deferredTasks[i]->Run();
    delete deferredTasks[i];
  }

  // One of the deferred tasks may have started a new animation. In this case,
  // we want to ask the compositor to schedule a new composite.
  requestAnimationFrame |= (mAnimation != nullptr);

  return requestAnimationFrame;
}

ParentLayerPoint
AsyncPanZoomController::GetCurrentAsyncScrollOffset(AsyncMode aMode) const
{
  ReentrantMonitorAutoEnter lock(mMonitor);

  if (aMode == RESPECT_FORCE_DISABLE && mFrameMetrics.IsApzForceDisabled()) {
    return mLastContentPaintMetrics.GetScrollOffset() * mLastContentPaintMetrics.GetZoom();
  }

  return (mFrameMetrics.GetScrollOffset() + mTestAsyncScrollOffset)
      * mFrameMetrics.GetZoom() * mTestAsyncZoom.scale;
}

AsyncTransform
AsyncPanZoomController::GetCurrentAsyncTransform(AsyncMode aMode) const
{
  ReentrantMonitorAutoEnter lock(mMonitor);

  if (aMode == RESPECT_FORCE_DISABLE && mFrameMetrics.IsApzForceDisabled()) {
    return AsyncTransform();
  }

  CSSPoint lastPaintScrollOffset;
  if (mLastContentPaintMetrics.IsScrollable()) {
    lastPaintScrollOffset = mLastContentPaintMetrics.GetScrollOffset();
  }

  CSSPoint currentScrollOffset = mFrameMetrics.GetScrollOffset() +
    mTestAsyncScrollOffset;

  // If checkerboarding has been disallowed, clamp the scroll position to stay
  // within rendered content.
  if (!gfxPrefs::APZAllowCheckerboarding() &&
      !mLastContentPaintMetrics.GetDisplayPort().IsEmpty()) {
    CSSSize compositedSize = mLastContentPaintMetrics.CalculateCompositedSizeInCssPixels();
    CSSPoint maxScrollOffset = lastPaintScrollOffset +
      CSSPoint(mLastContentPaintMetrics.GetDisplayPort().XMost() - compositedSize.width,
               mLastContentPaintMetrics.GetDisplayPort().YMost() - compositedSize.height);
    CSSPoint minScrollOffset = lastPaintScrollOffset + mLastContentPaintMetrics.GetDisplayPort().TopLeft();

    if (minScrollOffset.x < maxScrollOffset.x) {
      currentScrollOffset.x = clamped(currentScrollOffset.x, minScrollOffset.x, maxScrollOffset.x);
    }
    if (minScrollOffset.y < maxScrollOffset.y) {
      currentScrollOffset.y = clamped(currentScrollOffset.y, minScrollOffset.y, maxScrollOffset.y);
    }
  }

  ParentLayerPoint translation = (currentScrollOffset - lastPaintScrollOffset)
                               * mFrameMetrics.GetZoom() * mTestAsyncZoom.scale;

  return AsyncTransform(
    LayerToParentLayerScale(mFrameMetrics.GetAsyncZoom().scale * mTestAsyncZoom.scale),
    -translation);
}

AsyncTransformComponentMatrix
AsyncPanZoomController::GetCurrentAsyncTransformWithOverscroll(AsyncMode aMode) const
{
  return AsyncTransformComponentMatrix(GetCurrentAsyncTransform(aMode))
       * GetOverscrollTransform(aMode);
}

Matrix4x4 AsyncPanZoomController::GetTransformToLastDispatchedPaint() const {
  ReentrantMonitorAutoEnter lock(mMonitor);

  LayerPoint scrollChange =
    (mLastContentPaintMetrics.GetScrollOffset() - mExpectedGeckoMetrics.GetScrollOffset())
    * mLastContentPaintMetrics.GetDevPixelsPerCSSPixel()
    * mLastContentPaintMetrics.GetCumulativeResolution();

  // We're interested in the async zoom change. Factor out the content scale
  // that may change when dragging the window to a monitor with a different
  // content scale.
  LayoutDeviceToParentLayerScale2D lastContentZoom =
    mLastContentPaintMetrics.GetZoom() / mLastContentPaintMetrics.GetDevPixelsPerCSSPixel();
  LayoutDeviceToParentLayerScale2D lastDispatchedZoom =
    mExpectedGeckoMetrics.GetZoom() / mExpectedGeckoMetrics.GetDevPixelsPerCSSPixel();
  gfxSize zoomChange = lastContentZoom / lastDispatchedZoom;

  return Matrix4x4::Translation(scrollChange.x, scrollChange.y, 0).
           PostScale(zoomChange.width, zoomChange.height, 1);
}

uint32_t
AsyncPanZoomController::GetCheckerboardMagnitude() const
{
  ReentrantMonitorAutoEnter lock(mMonitor);

  CSSPoint currentScrollOffset = mFrameMetrics.GetScrollOffset() + mTestAsyncScrollOffset;
  CSSRect painted = mLastContentPaintMetrics.GetDisplayPort() + mLastContentPaintMetrics.GetScrollOffset();
  CSSRect visible = CSSRect(currentScrollOffset, mFrameMetrics.CalculateCompositedSizeInCssPixels());

  CSSIntRegion checkerboard;
  // Round so as to minimize checkerboarding; if we're only showing fractional
  // pixels of checkerboarding it's not really worth counting
  checkerboard.Sub(RoundedIn(visible), RoundedOut(painted));
  return checkerboard.Area();
}

void
AsyncPanZoomController::ReportCheckerboard(const TimeStamp& aSampleTime)
{
  if (mLastCheckerboardReport == aSampleTime) {
    // This function will get called multiple times for each APZC on a single
    // composite (once for each layer it is attached to). Only report the
    // checkerboard once per composite though.
    return;
  }
  mLastCheckerboardReport = aSampleTime;

  bool recordTrace = gfxPrefs::APZRecordCheckerboarding();
  bool forTelemetry = Telemetry::CanRecordExtended();
  uint32_t magnitude = GetCheckerboardMagnitude();

  MutexAutoLock lock(mCheckerboardEventLock);
  if (!mCheckerboardEvent && (recordTrace || forTelemetry)) {
    mCheckerboardEvent = MakeUnique<CheckerboardEvent>(recordTrace);
  }
  mPotentialCheckerboardTracker.InTransform(IsTransformingState(mState));
  if (magnitude) {
    mPotentialCheckerboardTracker.CheckerboardSeen();
  }
  if (mCheckerboardEvent && mCheckerboardEvent->RecordFrameInfo(magnitude)) {
    // This checkerboard event is done. Report some metrics to telemetry.
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::CHECKERBOARD_SEVERITY,
      mCheckerboardEvent->GetSeverity());
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::CHECKERBOARD_PEAK,
      mCheckerboardEvent->GetPeak());
    mozilla::Telemetry::Accumulate(mozilla::Telemetry::CHECKERBOARD_DURATION,
      (uint32_t)mCheckerboardEvent->GetDuration().ToMilliseconds());

    mPotentialCheckerboardTracker.CheckerboardDone();

    if (recordTrace) {
      // if the pref is enabled, also send it to the storage class. it may be
      // chosen for public display on about:checkerboard, the hall of fame for
      // checkerboard events.
      uint32_t severity = mCheckerboardEvent->GetSeverity();
      std::string log = mCheckerboardEvent->GetLog();
      NS_DispatchToMainThread(NS_NewRunnableFunction([severity, log]() {
          RefPtr<CheckerboardEventStorage> storage = CheckerboardEventStorage::GetInstance();
          storage->ReportCheckerboard(severity, log);
      }));
    }
    mCheckerboardEvent = nullptr;
  }
}

bool AsyncPanZoomController::IsCurrentlyCheckerboarding() const {
  ReentrantMonitorAutoEnter lock(mMonitor);

  if (!gfxPrefs::APZAllowCheckerboarding()) {
    return false;
  }

  CSSPoint currentScrollOffset = mFrameMetrics.GetScrollOffset() + mTestAsyncScrollOffset;
  CSSRect painted = mLastContentPaintMetrics.GetDisplayPort() + mLastContentPaintMetrics.GetScrollOffset();
  painted.Inflate(CSSMargin::FromAppUnits(nsMargin(1, 1, 1, 1)));   // fuzz for rounding error
  CSSRect visible = CSSRect(currentScrollOffset, mFrameMetrics.CalculateCompositedSizeInCssPixels());
  if (painted.Contains(visible)) {
    return false;
  }
  APZC_LOG_FM(mFrameMetrics, "%p is currently checkerboarding (painted %s visble %s)",
    this, Stringify(painted).c_str(), Stringify(visible).c_str());
  return true;
}

void AsyncPanZoomController::NotifyLayersUpdated(const ScrollMetadata& aScrollMetadata,
                                                 bool aIsFirstPaint,
                                                 bool aThisLayerTreeUpdated)
{
  APZThreadUtils::AssertOnCompositorThread();

  ReentrantMonitorAutoEnter lock(mMonitor);
  bool isDefault = mScrollMetadata.IsDefault();

  const FrameMetrics& aLayerMetrics = aScrollMetadata.GetMetrics();

  if ((aLayerMetrics == mLastContentPaintMetrics) && !isDefault) {
    // No new information here, skip it.
    APZC_LOG("%p NotifyLayersUpdated short-circuit\n", this);
    return;
  }
  if (aLayerMetrics.GetScrollUpdateType() != FrameMetrics::ScrollOffsetUpdateType::ePending) {
    mLastContentPaintMetrics = aLayerMetrics;
  }

  mFrameMetrics.SetScrollParentId(aLayerMetrics.GetScrollParentId());
  APZC_LOG_FM(aLayerMetrics, "%p got a NotifyLayersUpdated with aIsFirstPaint=%d, aThisLayerTreeUpdated=%d",
    this, aIsFirstPaint, aThisLayerTreeUpdated);

  { // scope lock
    MutexAutoLock lock(mCheckerboardEventLock);
    if (mCheckerboardEvent && mCheckerboardEvent->IsRecordingTrace()) {
      std::string str;
      if (aThisLayerTreeUpdated) {
        if (!aLayerMetrics.GetPaintRequestTime().IsNull()) {
          // Note that we might get the paint request time as non-null, but with
          // aThisLayerTreeUpdated false. That can happen if we get a layer transaction
          // from a different process right after we get the layer transaction with
          // aThisLayerTreeUpdated == true. In this case we want to ignore the
          // paint request time because it was already dumped in the previous layer
          // transaction.
          TimeDuration paintTime = TimeStamp::Now() - aLayerMetrics.GetPaintRequestTime();
          std::stringstream info;
          info << " painttime " << paintTime.ToMilliseconds();
          str = info.str();
        } else {
          // This might be indicative of a wasted paint particularly if it happens
          // during a checkerboard event.
          str = " (this layertree updated)";
        }
      }
      mCheckerboardEvent->UpdateRendertraceProperty(
          CheckerboardEvent::Page, aLayerMetrics.GetScrollableRect());
      mCheckerboardEvent->UpdateRendertraceProperty(
          CheckerboardEvent::PaintedDisplayPort,
          aLayerMetrics.GetDisplayPort() + aLayerMetrics.GetScrollOffset(),
          str);
      if (!aLayerMetrics.GetCriticalDisplayPort().IsEmpty()) {
        mCheckerboardEvent->UpdateRendertraceProperty(
            CheckerboardEvent::PaintedCriticalDisplayPort,
            aLayerMetrics.GetCriticalDisplayPort() + aLayerMetrics.GetScrollOffset());
      }
    }
  }

  bool needContentRepaint = false;
  bool viewportUpdated = false;
  if (FuzzyEqualsAdditive(aLayerMetrics.GetCompositionBounds().width, mFrameMetrics.GetCompositionBounds().width) &&
      FuzzyEqualsAdditive(aLayerMetrics.GetCompositionBounds().height, mFrameMetrics.GetCompositionBounds().height)) {
    // Remote content has sync'd up to the composition geometry
    // change, so we can accept the viewport it's calculated.
    if (mFrameMetrics.GetViewport().width != aLayerMetrics.GetViewport().width ||
        mFrameMetrics.GetViewport().height != aLayerMetrics.GetViewport().height) {
      needContentRepaint = true;
      viewportUpdated = true;
    }
    mFrameMetrics.SetViewport(aLayerMetrics.GetViewport());
  }

  // If the layers update was not triggered by our own repaint request, then
  // we want to take the new scroll offset. Check the scroll generation as well
  // to filter duplicate calls to NotifyLayersUpdated with the same scroll offset
  // update message.
  bool scrollOffsetUpdated = aLayerMetrics.GetScrollOffsetUpdated()
        && (aLayerMetrics.GetScrollGeneration() != mFrameMetrics.GetScrollGeneration());

  bool smoothScrollRequested = aLayerMetrics.GetDoSmoothScroll()
       && (aLayerMetrics.GetScrollGeneration() != mFrameMetrics.GetScrollGeneration());

  // TODO if we're in a drag and scrollOffsetUpdated is set then we want to
  // ignore it

  if ((aIsFirstPaint && aThisLayerTreeUpdated) || isDefault) {
    // Initialize our internal state to something sane when the content
    // that was just painted is something we knew nothing about previously
    CancelAnimation();

    mScrollMetadata = aScrollMetadata;
    if (scrollOffsetUpdated) {
      AcknowledgeScrollUpdate();
    }
    mExpectedGeckoMetrics = aLayerMetrics;
    ShareCompositorFrameMetrics();

    if (mFrameMetrics.GetDisplayPortMargins() != ScreenMargin()) {
      // A non-zero display port margin here indicates a displayport has
      // been set by a previous APZC for the content at this guid. The
      // scrollable rect may have changed since then, making the margins
      // wrong, so we need to calculate a new display port.
      APZC_LOG("%p detected non-empty margins which probably need updating\n", this);
      needContentRepaint = true;
    }
  } else {
    // If we're not taking the aLayerMetrics wholesale we still need to pull
    // in some things into our local mFrameMetrics because these things are
    // determined by Gecko and our copy in mFrameMetrics may be stale.

    if (FuzzyEqualsAdditive(mFrameMetrics.GetCompositionBounds().width, aLayerMetrics.GetCompositionBounds().width) &&
        mFrameMetrics.GetDevPixelsPerCSSPixel() == aLayerMetrics.GetDevPixelsPerCSSPixel() &&
        !viewportUpdated) {
      // Any change to the pres shell resolution was requested by APZ and is
      // already included in our zoom; however, other components of the
      // cumulative resolution (a parent document's pres-shell resolution, or
      // the css-driven resolution) may have changed, and we need to update
      // our zoom to reflect that. Note that we can't just take
      // aLayerMetrics.mZoom because the APZ may have additional async zoom
      // since the repaint request.
      gfxSize totalResolutionChange = aLayerMetrics.GetCumulativeResolution()
                                    / mFrameMetrics.GetCumulativeResolution();
      float presShellResolutionChange = aLayerMetrics.GetPresShellResolution()
                                      / mFrameMetrics.GetPresShellResolution();
      if (presShellResolutionChange != 1.0f) {
        needContentRepaint = true;
      }
      mFrameMetrics.ZoomBy(totalResolutionChange / presShellResolutionChange);
    } else {
      // Take the new zoom as either device scale or composition width or
      // viewport size got changed (e.g. due to orientation change, or content
      // changing the meta-viewport tag).
      mFrameMetrics.SetZoom(aLayerMetrics.GetZoom());
      mFrameMetrics.SetDevPixelsPerCSSPixel(aLayerMetrics.GetDevPixelsPerCSSPixel());
    }
    if (!mFrameMetrics.GetScrollableRect().IsEqualEdges(aLayerMetrics.GetScrollableRect())) {
      mFrameMetrics.SetScrollableRect(aLayerMetrics.GetScrollableRect());
      needContentRepaint = true;
    }
    mFrameMetrics.SetCompositionBounds(aLayerMetrics.GetCompositionBounds());
    mFrameMetrics.SetRootCompositionSize(aLayerMetrics.GetRootCompositionSize());
    mFrameMetrics.SetPresShellResolution(aLayerMetrics.GetPresShellResolution());
    mFrameMetrics.SetCumulativeResolution(aLayerMetrics.GetCumulativeResolution());
    mFrameMetrics.SetHasScrollgrab(aLayerMetrics.GetHasScrollgrab());
    mFrameMetrics.SetLineScrollAmount(aLayerMetrics.GetLineScrollAmount());
    mFrameMetrics.SetPageScrollAmount(aLayerMetrics.GetPageScrollAmount());
    mScrollMetadata.SetSnapInfo(ScrollSnapInfo(aScrollMetadata.GetSnapInfo()));
    mScrollMetadata.SetClipRect(aScrollMetadata.GetClipRect());
    mScrollMetadata.SetMaskLayerIndex(aScrollMetadata.GetMaskLayerIndex());
    mFrameMetrics.SetIsLayersIdRoot(aLayerMetrics.IsLayersIdRoot());
    mFrameMetrics.SetUsesContainerScrolling(aLayerMetrics.UsesContainerScrolling());
    mFrameMetrics.SetIsScrollInfoLayer(aLayerMetrics.IsScrollInfoLayer());
    mFrameMetrics.SetForceDisableApz(aLayerMetrics.IsApzForceDisabled());

    if (scrollOffsetUpdated) {
      APZC_LOG("%p updating scroll offset from %s to %s\n", this,
        ToString(mFrameMetrics.GetScrollOffset()).c_str(),
        ToString(aLayerMetrics.GetScrollOffset()).c_str());

      // Send an acknowledgement with the new scroll generation so that any
      // repaint requests later in this function go through.
      // Because of the scroll generation update, any inflight paint requests are
      // going to be ignored by layout, and so mExpectedGeckoMetrics
      // becomes incorrect for the purposes of calculating the LD transform. To
      // correct this we need to update mExpectedGeckoMetrics to be the
      // last thing we know was painted by Gecko.
      mFrameMetrics.CopyScrollInfoFrom(aLayerMetrics);
      AcknowledgeScrollUpdate();
      mExpectedGeckoMetrics = aLayerMetrics;

      // Cancel the animation (which might also trigger a repaint request)
      // after we update the scroll offset above. Otherwise we can be left
      // in a state where things are out of sync.
      CancelAnimation();

      // Since the scroll offset has changed, we need to recompute the
      // displayport margins and send them to layout. Otherwise there might be
      // scenarios where for example we scroll from the top of a page (where the
      // top displayport margin is zero) to the bottom of a page, which will
      // result in a displayport that doesn't extend upwards at all.
      // Note that even if the CancelAnimation call above requested a repaint
      // this is fine because we already have repaint request deduplication.
      needContentRepaint = true;
    }
  }

  if (smoothScrollRequested) {
    // A smooth scroll has been requested for animation on the compositor
    // thread.  This flag will be reset by the main thread when it receives
    // the scroll update acknowledgement.

    APZC_LOG("%p smooth scrolling from %s to %s in state %d\n", this,
      Stringify(mFrameMetrics.GetScrollOffset()).c_str(),
      Stringify(aLayerMetrics.GetSmoothScrollOffset()).c_str(),
      mState);

    // See comment on the similar code in the |if (scrollOffsetUpdated)| block
    // above.
    mFrameMetrics.CopySmoothScrollInfoFrom(aLayerMetrics);
    AcknowledgeScrollUpdate();
    mExpectedGeckoMetrics = aLayerMetrics;

    SmoothScrollTo(mFrameMetrics.GetSmoothScrollOffset());
  }

  if (needContentRepaint) {
    RequestContentRepaint();
  }
  UpdateSharedCompositorFrameMetrics();
}

void
AsyncPanZoomController::AcknowledgeScrollUpdate() const
{
  // Once layout issues a scroll offset update, it becomes impervious to
  // scroll offset updates from APZ until we acknowledge the update it sent.
  // This prevents APZ updates from clobbering scroll updates from other
  // more "legitimate" sources like content scripts.
  RefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    APZC_LOG("%p sending scroll update acknowledgement with gen %u\n", this, mFrameMetrics.GetScrollGeneration());
    controller->AcknowledgeScrollUpdate(mFrameMetrics.GetScrollId(),
                                        mFrameMetrics.GetScrollGeneration());
  }
}

const FrameMetrics& AsyncPanZoomController::GetFrameMetrics() const {
  mMonitor.AssertCurrentThreadIn();
  return mFrameMetrics;
}

APZCTreeManager* AsyncPanZoomController::GetApzcTreeManager() const {
  mMonitor.AssertNotCurrentThreadIn();
  return mTreeManager;
}

void AsyncPanZoomController::ZoomToRect(CSSRect aRect, const uint32_t aFlags) {
  if (!aRect.IsFinite()) {
    NS_WARNING("ZoomToRect got called with a non-finite rect; ignoring...");
    return;
  }

  // Only the root APZC is zoomable, and the root APZC is not allowed to have
  // different x and y scales. If it did, the calculations in this function
  // would have to be adjusted (as e.g. it would no longer be valid to take
  // the minimum or maximum of the ratios of the widths and heights of the
  // page rect and the composition bounds).
  MOZ_ASSERT(mFrameMetrics.IsRootContent());
  MOZ_ASSERT(mFrameMetrics.GetZoom().AreScalesSame());

  SetState(ANIMATING_ZOOM);

  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    ParentLayerRect compositionBounds = mFrameMetrics.GetCompositionBounds();
    CSSRect cssPageRect = mFrameMetrics.GetScrollableRect();
    CSSPoint scrollOffset = mFrameMetrics.GetScrollOffset();
    CSSToParentLayerScale currentZoom = mFrameMetrics.GetZoom().ToScaleFactor();
    CSSToParentLayerScale targetZoom;

    // The minimum zoom to prevent over-zoom-out.
    // If the zoom factor is lower than this (i.e. we are zoomed more into the page),
    // then the CSS content rect, in layers pixels, will be smaller than the
    // composition bounds. If this happens, we can't fill the target composited
    // area with this frame.
    CSSToParentLayerScale localMinZoom(std::max(mZoomConstraints.mMinZoom.scale,
                                       std::max(compositionBounds.width / cssPageRect.width,
                                                compositionBounds.height / cssPageRect.height)));
    CSSToParentLayerScale localMaxZoom = mZoomConstraints.mMaxZoom;

    if (!aRect.IsEmpty()) {
      // Intersect the zoom-to-rect to the CSS rect to make sure it fits.
      aRect = aRect.Intersect(cssPageRect);
      targetZoom = CSSToParentLayerScale(std::min(compositionBounds.width / aRect.width,
                                                  compositionBounds.height / aRect.height));
    }

    // 1. If the rect is empty, the content-side logic for handling a double-tap
    //    requested that we zoom out.
    // 2. currentZoom is equal to mZoomConstraints.mMaxZoom and user still double-tapping it
    // 3. currentZoom is equal to localMinZoom and user still double-tapping it
    // Treat these three cases as a request to zoom out as much as possible.
    bool zoomOut;
    if (aFlags & DISABLE_ZOOM_OUT) {
      zoomOut = false;
    } else {
      zoomOut = aRect.IsEmpty() ||
        (currentZoom == localMaxZoom && targetZoom >= localMaxZoom) ||
        (currentZoom == localMinZoom && targetZoom <= localMinZoom);
    }

    if (zoomOut) {
      CSSSize compositedSize = mFrameMetrics.CalculateCompositedSizeInCssPixels();
      float y = scrollOffset.y;
      float newHeight =
        cssPageRect.width * (compositedSize.height / compositedSize.width);
      float dh = compositedSize.height - newHeight;

      aRect = CSSRect(0.0f,
                      y + dh/2,
                      cssPageRect.width,
                      newHeight);
      aRect = aRect.Intersect(cssPageRect);
      targetZoom = CSSToParentLayerScale(std::min(compositionBounds.width / aRect.width,
                                                  compositionBounds.height / aRect.height));
    }

    targetZoom.scale = clamped(targetZoom.scale, localMinZoom.scale, localMaxZoom.scale);
    FrameMetrics endZoomToMetrics = mFrameMetrics;
    if (aFlags & PAN_INTO_VIEW_ONLY) {
      targetZoom = currentZoom;
    } else if(aFlags & ONLY_ZOOM_TO_DEFAULT_SCALE) {
      CSSToParentLayerScale zoomAtDefaultScale =
        mFrameMetrics.GetDevPixelsPerCSSPixel() * LayoutDeviceToParentLayerScale(1.0);
      if (targetZoom.scale > zoomAtDefaultScale.scale) {
        // Only change the zoom if we are less than the default zoom
        if (currentZoom.scale < zoomAtDefaultScale.scale) {
          targetZoom = zoomAtDefaultScale;
        } else {
          targetZoom = currentZoom;
        }
      }
    }
    endZoomToMetrics.SetZoom(CSSToParentLayerScale2D(targetZoom));

    // Adjust the zoomToRect to a sensible position to prevent overscrolling.
    CSSSize sizeAfterZoom = endZoomToMetrics.CalculateCompositedSizeInCssPixels();

    // Vertically center the zoomed element in the screen.
    if (!zoomOut && (sizeAfterZoom.height > aRect.height)) {
      aRect.y -= (sizeAfterZoom.height - aRect.height) * 0.5f;
      if (aRect.y < 0.0f) {
        aRect.y = 0.0f;
      }
    }

    // If either of these conditions are met, the page will be
    // overscrolled after zoomed
    if (aRect.y + sizeAfterZoom.height > cssPageRect.height) {
      aRect.y = cssPageRect.height - sizeAfterZoom.height;
      aRect.y = aRect.y > 0 ? aRect.y : 0;
    }
    if (aRect.x + sizeAfterZoom.width > cssPageRect.width) {
      aRect.x = cssPageRect.width - sizeAfterZoom.width;
      aRect.x = aRect.x > 0 ? aRect.x : 0;
    }

    endZoomToMetrics.SetScrollOffset(aRect.TopLeft());

    StartAnimation(new ZoomAnimation(
        mFrameMetrics.GetScrollOffset(),
        mFrameMetrics.GetZoom(),
        endZoomToMetrics.GetScrollOffset(),
        endZoomToMetrics.GetZoom()));

    // Schedule a repaint now, so the new displayport will be painted before the
    // animation finishes.
    ParentLayerPoint velocity(0, 0);
    endZoomToMetrics.SetDisplayPortMargins(
      CalculatePendingDisplayPort(endZoomToMetrics, velocity));
    endZoomToMetrics.SetUseDisplayPortMargins(true);
    endZoomToMetrics.SetPaintRequestTime(TimeStamp::Now());
    if (NS_IsMainThread()) {
      RequestContentRepaint(endZoomToMetrics, velocity);
    } else {
      // use a local var to resolve the function overload
      auto func = static_cast<void (AsyncPanZoomController::*)(const FrameMetrics&, const ParentLayerPoint&)>
          (&AsyncPanZoomController::RequestContentRepaint);
      NS_DispatchToMainThread(
          NS_NewRunnableMethodWithArgs<FrameMetrics, ParentLayerPoint>(
              this, func, endZoomToMetrics, velocity));
    }
  }
}

CancelableBlockState*
AsyncPanZoomController::CurrentInputBlock() const
{
  return GetInputQueue()->CurrentBlock();
}

TouchBlockState*
AsyncPanZoomController::CurrentTouchBlock() const
{
  return GetInputQueue()->CurrentTouchBlock();
}

PanGestureBlockState*
AsyncPanZoomController::CurrentPanGestureBlock() const
{
  return GetInputQueue()->CurrentPanGestureBlock();
}

void
AsyncPanZoomController::ResetTouchInputState()
{
  MultiTouchInput cancel(MultiTouchInput::MULTITOUCH_CANCEL, 0, TimeStamp::Now(), 0);
  RefPtr<GestureEventListener> listener = GetGestureEventListener();
  if (listener) {
    listener->HandleInputEvent(cancel);
  }
  CancelAnimationAndGestureState();
  // Clear overscroll along the entire handoff chain, in case an APZC
  // later in the chain is overscrolled.
  if (TouchBlockState* block = CurrentTouchBlock()) {
    block->GetOverscrollHandoffChain()->ClearOverscroll();
  }
}

void
AsyncPanZoomController::CancelAnimationAndGestureState()
{
  mX.CancelGesture();
  mY.CancelGesture();
  CancelAnimation(CancelAnimationFlags::ScrollSnap);
}

bool
AsyncPanZoomController::HasReadyTouchBlock() const
{
  return GetInputQueue()->HasReadyTouchBlock();
}

void AsyncPanZoomController::SetState(PanZoomState aNewState)
{
  PanZoomState oldState;

  // Intentional scoping for mutex
  {
    ReentrantMonitorAutoEnter lock(mMonitor);
    APZC_LOG("%p changing from state %d to %d\n", this, mState, aNewState);
    oldState = mState;
    mState = aNewState;
  }

  DispatchStateChangeNotification(oldState, aNewState);
}

void AsyncPanZoomController::DispatchStateChangeNotification(PanZoomState aOldState,
                                                             PanZoomState aNewState)
{
  { // scope the lock
    ReentrantMonitorAutoEnter lock(mMonitor);
    if (mNotificationBlockers > 0) {
      return;
    }
  }

  if (RefPtr<GeckoContentController> controller = GetGeckoContentController()) {
    if (!IsTransformingState(aOldState) && IsTransformingState(aNewState)) {
      controller->NotifyAPZStateChange(
          GetGuid(), APZStateChange::TransformBegin);
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
      // Let the compositor know about scroll state changes so it can manage
      // windowed plugins.
      if (mCompositorBridgeParent) {
        mCompositorBridgeParent->ScheduleHideAllPluginWindows();
      }
#endif
    } else if (IsTransformingState(aOldState) && !IsTransformingState(aNewState)) {
      controller->NotifyAPZStateChange(
          GetGuid(), APZStateChange::TransformEnd);
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
      if (mCompositorBridgeParent) {
        mCompositorBridgeParent->ScheduleShowAllPluginWindows();
      }
#endif
    }
  }
}

bool AsyncPanZoomController::IsTransformingState(PanZoomState aState) {
  return !(aState == NOTHING || aState == TOUCHING);
}

bool AsyncPanZoomController::IsInPanningState() const {
  return (mState == PANNING || mState == PANNING_LOCKED_X || mState == PANNING_LOCKED_Y);
}

void AsyncPanZoomController::UpdateZoomConstraints(const ZoomConstraints& aConstraints) {
  APZC_LOG("%p updating zoom constraints to %d %d %f %f\n", this, aConstraints.mAllowZoom,
    aConstraints.mAllowDoubleTapZoom, aConstraints.mMinZoom.scale, aConstraints.mMaxZoom.scale);
  if (IsNaN(aConstraints.mMinZoom.scale) || IsNaN(aConstraints.mMaxZoom.scale)) {
    NS_WARNING("APZC received zoom constraints with NaN values; dropping...");
    return;
  }
  // inf float values and other bad cases should be sanitized by the code below.
  mZoomConstraints.mAllowZoom = aConstraints.mAllowZoom;
  mZoomConstraints.mAllowDoubleTapZoom = aConstraints.mAllowDoubleTapZoom;
  mZoomConstraints.mMinZoom = (MIN_ZOOM > aConstraints.mMinZoom ? MIN_ZOOM : aConstraints.mMinZoom);
  mZoomConstraints.mMaxZoom = (MAX_ZOOM > aConstraints.mMaxZoom ? aConstraints.mMaxZoom : MAX_ZOOM);
  if (mZoomConstraints.mMaxZoom < mZoomConstraints.mMinZoom) {
    mZoomConstraints.mMaxZoom = mZoomConstraints.mMinZoom;
  }
}

ZoomConstraints
AsyncPanZoomController::GetZoomConstraints() const
{
  return mZoomConstraints;
}


void AsyncPanZoomController::PostDelayedTask(Task* aTask, int aDelayMs) {
  APZThreadUtils::AssertOnControllerThread();
  RefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    controller->PostDelayedTask(aTask, aDelayMs);
  }
}

bool AsyncPanZoomController::Matches(const ScrollableLayerGuid& aGuid)
{
  return aGuid == GetGuid();
}

bool AsyncPanZoomController::HasTreeManager(const APZCTreeManager* aTreeManager) const
{
  return GetApzcTreeManager() == aTreeManager;
}

void AsyncPanZoomController::GetGuid(ScrollableLayerGuid* aGuidOut) const
{
  if (aGuidOut) {
    *aGuidOut = GetGuid();
  }
}

ScrollableLayerGuid AsyncPanZoomController::GetGuid() const
{
  return ScrollableLayerGuid(mLayersId, mFrameMetrics);
}

void AsyncPanZoomController::UpdateSharedCompositorFrameMetrics()
{
  mMonitor.AssertCurrentThreadIn();

  FrameMetrics* frame = mSharedFrameMetricsBuffer ?
      static_cast<FrameMetrics*>(mSharedFrameMetricsBuffer->memory()) : nullptr;

  if (frame && mSharedLock && gfxPlatform::GetPlatform()->UseProgressivePaint()) {
    mSharedLock->Lock();
    *frame = mFrameMetrics.MakePODObject();
    mSharedLock->Unlock();
  }
}

void AsyncPanZoomController::ShareCompositorFrameMetrics() {

  PCompositorBridgeParent* compositor = GetSharedFrameMetricsCompositor();

  // Only create the shared memory buffer if it hasn't already been created,
  // we are using progressive tile painting, and we have a
  // compositor to pass the shared memory back to the content process/thread.
  if (!mSharedFrameMetricsBuffer && compositor && gfxPlatform::GetPlatform()->UseProgressivePaint()) {

    // Create shared memory and initialize it with the current FrameMetrics value
    mSharedFrameMetricsBuffer = new ipc::SharedMemoryBasic;
    FrameMetrics* frame = nullptr;
    mSharedFrameMetricsBuffer->Create(sizeof(FrameMetrics));
    mSharedFrameMetricsBuffer->Map(sizeof(FrameMetrics));
    frame = static_cast<FrameMetrics*>(mSharedFrameMetricsBuffer->memory());

    if (frame) {

      { // scope the monitor, only needed to copy the FrameMetrics.
        ReentrantMonitorAutoEnter lock(mMonitor);
        *frame = mFrameMetrics;
      }

      // Get the process id of the content process
      base::ProcessId otherPid = compositor->OtherPid();
      ipc::SharedMemoryBasic::Handle mem = ipc::SharedMemoryBasic::NULLHandle();

      // Get the shared memory handle to share with the content process
      mSharedFrameMetricsBuffer->ShareToProcess(otherPid, &mem);

      // Get the cross process mutex handle to share with the content process
      mSharedLock = new CrossProcessMutex("AsyncPanZoomControlLock");
      CrossProcessMutexHandle handle = mSharedLock->ShareToProcess(otherPid);

      // Send the shared memory handle and cross process handle to the content
      // process by an asynchronous ipc call. Include the APZC unique ID
      // so the content process know which APZC sent this shared FrameMetrics.
      if (!compositor->SendSharedCompositorFrameMetrics(mem, handle, mLayersId, mAPZCId)) {
        APZC_LOG("%p failed to share FrameMetrics with content process.", this);
      }
    }
  }
}

Maybe<CSSPoint> AsyncPanZoomController::FindSnapPointNear(
    const CSSPoint& aDestination, nsIScrollableFrame::ScrollUnit aUnit) {
  mMonitor.AssertCurrentThreadIn();
  APZC_LOG("%p scroll snapping near %s\n", this, Stringify(aDestination).c_str());
  CSSRect scrollRange = mFrameMetrics.CalculateScrollRange();
  if (Maybe<nsPoint> snapPoint = ScrollSnapUtils::GetSnapPointForDestination(
          mScrollMetadata.GetSnapInfo(),
          aUnit,
          CSSSize::ToAppUnits(mFrameMetrics.CalculateCompositedSizeInCssPixels()),
          CSSRect::ToAppUnits(scrollRange),
          CSSPoint::ToAppUnits(mFrameMetrics.GetScrollOffset()),
          CSSPoint::ToAppUnits(aDestination))) {
    CSSPoint cssSnapPoint = CSSPoint::FromAppUnits(snapPoint.ref());
    // GetSnapPointForDestination() can produce a destination that's outside
    // of the scroll frame's scroll range. Clamp it here (this matches the
    // behaviour of the main-thread code path, which clamps it in
    // nsGfxScrollFrame::ScrollTo()).
    return Some(scrollRange.ClampPoint(cssSnapPoint));
  }
  return Nothing();
}

void AsyncPanZoomController::ScrollSnapNear(const CSSPoint& aDestination) {
  if (Maybe<CSSPoint> snapPoint =
        FindSnapPointNear(aDestination, nsIScrollableFrame::DEVICE_PIXELS)) {
    SmoothScrollTo(*snapPoint);
  }
}

void AsyncPanZoomController::ScrollSnap() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  ScrollSnapNear(mFrameMetrics.GetScrollOffset());
}

void AsyncPanZoomController::ScrollSnapToDestination() {
  ReentrantMonitorAutoEnter lock(mMonitor);

  float friction = gfxPrefs::APZFlingFriction();
  ParentLayerPoint velocity(mX.GetVelocity(), mY.GetVelocity());
  ParentLayerPoint predictedDelta;
  // "-velocity / log(1.0 - friction)" is the integral of the deceleration
  // curve modeled for flings in the "Axis" class.
  if (velocity.x != 0.0f) {
    predictedDelta.x = -velocity.x / log(1.0 - friction);
  }
  if (velocity.y != 0.0f) {
    predictedDelta.y = -velocity.y / log(1.0 - friction);
  }
  CSSPoint predictedDestination = mFrameMetrics.GetScrollOffset() + predictedDelta / mFrameMetrics.GetZoom();

  // If the fling will overscroll, don't scroll snap, because then the user
  // user would not see any overscroll animation.
  bool flingWillOverscroll = IsOverscrolled() && ((velocity.x * mX.GetOverscroll() >= 0) ||
                                                  (velocity.y * mY.GetOverscroll() >= 0));
  if (!flingWillOverscroll) {
    APZC_LOG("%p fling snapping.  friction: %f velocity: %f, %f "
             "predictedDelta: %f, %f position: %f, %f "
             "predictedDestination: %f, %f\n",
             this, friction, velocity.x, velocity.y, (float)predictedDelta.x,
             (float)predictedDelta.y, (float)mFrameMetrics.GetScrollOffset().x,
             (float)mFrameMetrics.GetScrollOffset().y,
             (float)predictedDestination.x, (float)predictedDestination.y);

    ScrollSnapNear(predictedDestination);
  }
}

bool AsyncPanZoomController::MaybeAdjustDeltaForScrollSnapping(
    const ScrollWheelInput& aEvent,
    ParentLayerPoint& aDelta,
    CSSPoint& aStartPosition)
{
  // Don't scroll snap for pixel scrolls. This matches the main thread
  // behaviour in EventStateManager::DoScrollText().
  if (aEvent.mDeltaType == ScrollWheelInput::SCROLLDELTA_PIXEL) {
    return false;
  }

  ReentrantMonitorAutoEnter lock(mMonitor);
  CSSToParentLayerScale2D zoom = mFrameMetrics.GetZoom();
  CSSPoint destination = mFrameMetrics.CalculateScrollRange().ClampPoint(
      aStartPosition + (aDelta / zoom));
  nsIScrollableFrame::ScrollUnit unit =
      ScrollWheelInput::ScrollUnitForDeltaType(aEvent.mDeltaType);

  if (Maybe<CSSPoint> snapPoint = FindSnapPointNear(destination, unit)) {
    aDelta = (*snapPoint - aStartPosition) * zoom;
    aStartPosition = *snapPoint;
    return true;
  }
  return false;
}

} // namespace layers
} // namespace mozilla
