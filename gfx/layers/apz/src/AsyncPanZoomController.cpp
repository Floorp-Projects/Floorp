/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>                       // for fabsf, fabs, atan2
#include <stdint.h>                     // for uint32_t, uint64_t
#include <sys/types.h>                  // for int32_t
#include <algorithm>                    // for max, min
#include "AnimationCommon.h"            // for ComputedTimingFunction
#include "AsyncPanZoomController.h"     // for AsyncPanZoomController, etc
#include "Compositor.h"                 // for Compositor
#include "CompositorParent.h"           // for CompositorParent
#include "FrameMetrics.h"               // for FrameMetrics, etc
#include "GestureEventListener.h"       // for GestureEventListener
#include "InputData.h"                  // for MultiTouchInput, etc
#include "TouchBlockState.h"            // for TouchBlockState
#include "Units.h"                      // for CSSRect, CSSPoint, etc
#include "UnitTransforms.h"             // for TransformTo
#include "base/message_loop.h"          // for MessageLoop
#include "base/task.h"                  // for NewRunnableMethod, etc
#include "base/tracked.h"               // for FROM_HERE
#include "gfxPrefs.h"                   // for gfxPrefs
#include "gfxTypes.h"                   // for gfxFloat
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/BasicEvents.h"        // for Modifiers, MODIFIER_*
#include "mozilla/ClearOnShutdown.h"    // for ClearOnShutdown
#include "mozilla/Constants.h"          // for M_PI
#include "mozilla/EventForwards.h"      // for nsEventStatus_*
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/ReentrantMonitor.h"   // for ReentrantMonitorAutoEnter, etc
#include "mozilla/StaticPtr.h"          // for StaticAutoPtr
#include "mozilla/TimeStamp.h"          // for TimeDuration, TimeStamp
#include "mozilla/dom/Touch.h"          // for Touch
#include "mozilla/gfx/BasePoint.h"      // for BasePoint
#include "mozilla/gfx/BaseRect.h"       // for BaseRect
#include "mozilla/gfx/Point.h"          // for Point, RoundedToInt, etc
#include "mozilla/gfx/Rect.h"           // for RoundedIn
#include "mozilla/gfx/ScaleFactor.h"    // for ScaleFactor
#include "mozilla/layers/APZCTreeManager.h"  // for ScrollableLayerGuid
#include "mozilla/layers/AsyncCompositionManager.h"  // for ViewTransform
#include "mozilla/layers/Axis.h"        // for AxisX, AxisY, Axis, etc
#include "mozilla/layers/LayerTransactionParent.h" // for LayerTransactionParent
#include "mozilla/layers/PCompositorParent.h" // for PCompositorParent
#include "mozilla/layers/TaskThrottler.h"  // for TaskThrottler
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "mozilla/unused.h"             // for unused
#include "mozilla/FloatingPoint.h"      // for FuzzyEquals*
#include "nsAlgorithm.h"                // for clamped
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_WARNING
#include "nsIDOMWindowUtils.h"          // for nsIDOMWindowUtils
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsMathUtils.h"                // for NS_hypot
#include "nsPoint.h"                    // for nsIntPoint
#include "nsStyleConsts.h"
#include "nsStyleStruct.h"              // for nsTimingFunction
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "nsThreadUtils.h"              // for NS_IsMainThread
#include "SharedMemoryBasic.h"          // for SharedMemoryBasic

// #define APZC_ENABLE_RENDERTRACE

#define ENABLE_APZC_LOGGING 0
// #define ENABLE_APZC_LOGGING 1

#if ENABLE_APZC_LOGGING
#  define APZC_LOG(...) printf_stderr("APZC: " __VA_ARGS__)
#  define APZC_LOG_FM(fm, prefix, ...) \
    { std::stringstream ss; \
      ss << nsPrintfCString(prefix, __VA_ARGS__).get(); \
      AppendToString(ss, fm, ":", "", true); \
      APZC_LOG("%s", ss.str().c_str()); \
    }
#else
#  define APZC_LOG(...)
#  define APZC_LOG_FM(fm, prefix, ...)
#endif

// Static helper functions
namespace {

int32_t
WidgetModifiersToDOMModifiers(mozilla::Modifiers aModifiers)
{
  int32_t result = 0;
  if (aModifiers & mozilla::MODIFIER_SHIFT) {
    result |= nsIDOMWindowUtils::MODIFIER_SHIFT;
  }
  if (aModifiers & mozilla::MODIFIER_CONTROL) {
    result |= nsIDOMWindowUtils::MODIFIER_CONTROL;
  }
  if (aModifiers & mozilla::MODIFIER_ALT) {
    result |= nsIDOMWindowUtils::MODIFIER_ALT;
  }
  if (aModifiers & mozilla::MODIFIER_META) {
    result |= nsIDOMWindowUtils::MODIFIER_META;
  }
  if (aModifiers & mozilla::MODIFIER_ALTGRAPH) {
    result |= nsIDOMWindowUtils::MODIFIER_ALTGRAPH;
  }
  if (aModifiers & mozilla::MODIFIER_CAPSLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_CAPSLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_FN) {
    result |= nsIDOMWindowUtils::MODIFIER_FN;
  }
  if (aModifiers & mozilla::MODIFIER_NUMLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_NUMLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_SCROLLLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_SCROLLLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_SYMBOLLOCK) {
    result |= nsIDOMWindowUtils::MODIFIER_SYMBOLLOCK;
  }
  if (aModifiers & mozilla::MODIFIER_OS) {
    result |= nsIDOMWindowUtils::MODIFIER_OS;
  }
  return result;
}

}

namespace mozilla {
namespace layers {

typedef mozilla::layers::AllowedTouchBehavior AllowedTouchBehavior;
typedef GeckoContentController::APZStateChange APZStateChange;

/*
 * The following prefs are used to control the behaviour of the APZC.
 * The default values are provided in gfxPrefs.h.
 *
 * "apz.allow-checkerboarding"
 * Pref that allows or disallows checkerboarding
 *
 * "apz.asyncscroll.throttle"
 * The time period that throttles mozbrowserasyncscroll event.
 * Units: milliseconds
 *
 * "apz.asyncscroll.timeout"
 * The timeout for mAsyncScrollTimeoutTask delay task.
 * Units: milliseconds
 *
 * "apz.axis_lock_mode"
 * The preferred axis locking style. See AxisLockMode for possible values.
 *
 * "apz.content_response_timeout"
 * Amount of time before we timeout response from content. For example, if
 * content is being unruly/slow and we don't get a response back within this
 * time, we will just pretend that content did not preventDefault any touch
 * events we dispatched to it.
 * Units: milliseconds
 *
 * "apz.cross_slide_enabled"
 * Pref that enables integration with the Metro "cross-slide" gesture.
 *
 * "apz.danger_zone_x"
 * "apz.danger_zone_y"
 * When drawing high-res tiles, we drop down to drawing low-res tiles
 * when we know we can't keep up with the scrolling. The way we determine
 * this is by checking if we are entering the "danger zone", which is the
 * boundary of the painted content. For example, if the painted content
 * goes from y=0...1000 and the visible portion is y=250...750 then
 * we're far from checkerboarding. If we get to y=490...990 though then we're
 * only 10 pixels away from showing checkerboarding so we are probably in
 * a state where we can't keep up with scrolling. The danger zone prefs specify
 * how wide this margin is; in the above example a y-axis danger zone of 10
 * pixels would make us drop to low-res at y=490...990.
 * This value is in layer pixels.
 *
 * "apz.enlarge_displayport_when_clipped"
 * Pref that enables enlarging of the displayport along one axis when the
 * generated displayport's size is beyond that of the scrollable rect on the
 * opposite axis.
 *
 * "apz.fling_accel_interval_ms"
 * The time that determines whether a second fling will be treated as
 * accelerated. If two flings are started within this interval, the second one
 * will be accelerated. Setting an interval of 0 means that acceleration will
 * be disabled.
 * Units: milliseconds
 *
 * "apz.fling_accel_base_mult"
 * "apz.fling_accel_supplemental_mult"
 * When applying an acceleration on a fling, the new computed velocity is
 * (new_fling_velocity * base_mult) + (old_velocity * supplemental_mult).
 * The base_mult and supplemental_mult multiplier values are controlled by
 * these prefs. Note that "old_velocity" here is the initial velocity of the
 * previous fling _after_ acceleration was applied to it (if applicable).
 *
 * "apz.fling_friction"
 * Amount of friction applied during flings.
 *
 * "apz.fling_repaint_interval"
 * Maximum amount of time flinging before sending a viewport change. This will
 * asynchronously repaint the page.
 * Units: milliseconds
 *
 * "apz.fling_stop_on_tap_threshold"
 * When flinging, if the velocity is above this number, then a tap on the
 * screen will stop the fling without dispatching a tap to content. If the
 * velocity is below this threshold a tap will also be dispatched.
 * Note: when modifying this pref be sure to run the APZC gtests as some of
 * them depend on the value of this pref.
 * Units: screen pixels per millisecond
 *
 * "apz.fling_stopped_threshold"
 * When flinging, if the velocity goes below this number, we just stop the
 * animation completely. This is to prevent asymptotically approaching 0
 * velocity and rerendering unnecessarily.
 * Units: screen pixels per millisecond
 *
 * "apz.max_velocity_inches_per_ms"
 * Maximum velocity.  Velocity will be capped at this value if a faster fling
 * occurs.  Negative values indicate unlimited velocity.
 * Units: (real-world, i.e. screen) inches per millisecond
 *
 * "apz.max_velocity_queue_size"
 * Maximum size of velocity queue. The queue contains last N velocity records.
 * On touch end we calculate the average velocity in order to compensate
 * touch/mouse drivers misbehaviour.
 *
 * "apz.min_skate_speed"
 * Minimum amount of speed along an axis before we switch to "skate" multipliers
 * rather than using the "stationary" multipliers.
 * Units: CSS pixels per millisecond
 *
 * "apz.num_paint_duration_samples"
 * Number of samples to store of how long it took to paint after the previous
 *
 * "apz.overscroll.enabled"
 * Pref that enables overscrolling. If this is disabled, excess scroll that
 * cannot be handed off is discarded.
 *
 * "apz.overscroll.fling_friction"
 * Amount of friction applied during flings when in overscroll.
 *
 * "apz.overscroll.fling_stopped_threshold"
 * When flinging in an overscrolled state, if the velocity goes below this
 * number, we stop the fling.
 * Units: screen pixels per millisecond
 *
 * "apz.overscroll.clamping"
 * The maximum proportion of the composition bounds which can become blank
 * as a result of overscroll, along the axis of overscroll.
 *
 * "apz.overscroll.z_effect"
 * The fraction of "apz.overscroll.clamping" which can become blank as a result
 * of overscroll, along the axis opposite to the axis of overscroll. Called
 * "z_effect" because the shrinking that brings about the blank space on the
 * opposite axis creates the effect of the page moving away from you along a
 * "z" axis.
 *
 * "apz.overscroll.snap_back.spring_stiffness"
 * The stiffness of the spring used in the physics model for the overscroll
 * snap-back animation.
 *
 * "apz.overscroll.snap_back.spring_damping"
 * The friction of the spring used in the physics model for the overscroll
 * snap-back animation.
 *
 * "apz.overscroll.snap_back.mass"
 * The mass of the page in the physics model for the overscroll snap-back
 * animation.
 *
 * "apz.pan_repaint_interval"
 * Maximum amount of time while panning before sending a viewport change. This
 * will asynchronously repaint the page. It is also forced when panning stops.
 *
 * "apz.test.logging_enabled"
 * Enable logging of APZ test data (see bug 961289).
 *
 * "apz.touch_start_tolerance"
 * Constant describing the tolerance in distance we use, multiplied by the
 * device DPI, before we start panning the screen. This is to prevent us from
 * accidentally processing taps as touch moves, and from very short/accidental
 * touches moving the screen.
 * Units: (real-world, i.e. screen) inches
 *
 * "apz.use_paint_duration"
 * Whether or not to use the estimated paint duration as a factor when projecting
 * the displayport in the direction of scrolling. If this value is set to false,
 * a constant 50ms paint time is used; the projection can be scaled as desired
 * using the apz.velocity_bias pref below.
 *
 * "apz.velocity_bias"
 * How much to adjust the displayport in the direction of scrolling. This value
 * is multiplied by the velocity and added to the displayport offset.
 *
 * "apz.velocity_relevance_time_ms"
 * When computing a fling velocity from the most recently stored velocity
 * information, only velocities within the most X milliseconds are used.
 * This pref controls the value of X.
 * Units: ms
 *
 * "apz.x_skate_size_multiplier", "apz.y_skate_size_multiplier"
 * The multiplier we apply to the displayport size if it is skating (current
 * velocity is above apz.min_skate_speed). We prefer to increase the size of the
 * Y axis because it is more natural in the case that a user is reading a page
 * that scrolls up/down. Note that one, both or neither of these may be used
 * at any instant.
 * In general we want apz.[xy]_skate_size_multiplier to be smaller than the corresponding
 * stationary size multiplier because when panning fast we would like to paint
 * less and get faster, more predictable paint times. When panning slowly we
 * can afford to paint more even though it's slower.
 *
 * "apz.x_stationary_size_multiplier", "apz.y_stationary_size_multiplier"
 * The multiplier we apply to the displayport size if it is not skating (see
 * documentation for the skate size multipliers above).
 *
 * "apz.zoom_animation_duration_ms"
 * This controls how long the zoom-to-rect animation takes.
 * Units: ms
 */

/**
 * Angle from axis within which we stay axis-locked
 */
static const double AXIS_LOCK_ANGLE = M_PI / 6.0; // 30 degrees

/**
 * The distance in inches the user must pan before axis lock can be broken
 */
static const float AXIS_BREAKOUT_THRESHOLD = 1.0f/32.0f;

/**
 * The angle at which axis lock can be broken
 */
static const double AXIS_BREAKOUT_ANGLE = M_PI / 8.0; // 22.5 degrees

/**
 * Angle from axis to the line drawn by pan move.
 * If angle is less than this value we can assume that panning
 * can be done in allowed direction (horizontal or vertical).
 * Currently used only for touch-action css property stuff and was
 * added to keep behavior consistent with IE.
 */
static const double ALLOWED_DIRECT_PAN_ANGLE = M_PI / 3.0; // 60 degrees

/**
 * Computed time function used for sampling frames of a zoom to animation.
 */
StaticAutoPtr<css::ComputedTimingFunction> gComputedTimingFunction;

/**
 * Maximum zoom amount, always used, even if a page asks for higher.
 */
static const CSSToScreenScale MAX_ZOOM(8.0f);

/**
 * Minimum zoom amount, always used, even if a page asks for lower.
 */
static const CSSToScreenScale MIN_ZOOM(0.125f);

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

template <typename Units>
static bool IsZero(const gfx::PointTyped<Units>& aPoint)
{
  return FuzzyEqualsMultiplicative(aPoint.x, 0.0f)
      && FuzzyEqualsMultiplicative(aPoint.y, 0.0f);
}

static inline void LogRendertraceRect(const ScrollableLayerGuid& aGuid, const char* aDesc, const char* aColor, const CSSRect& aRect)
{
#ifdef APZC_ENABLE_RENDERTRACE
  static const TimeStamp sRenderStart = TimeStamp::Now();
  TimeDuration delta = TimeStamp::Now() - sRenderStart;
  printf_stderr("(%llu,%lu,%llu)%s RENDERTRACE %f rect %s %f %f %f %f\n",
    aGuid.mLayersId, aGuid.mPresShellId, aGuid.mScrollId,
    aDesc, delta.ToMilliseconds(), aColor,
    aRect.x, aRect.y, aRect.width, aRect.height);
#endif
}

static TimeStamp sFrameTime;
static bool sThreadAssertionsEnabled = true;

// Counter used to give each APZC a unique id
static uint32_t sAsyncPanZoomControllerCount = 0;

static TimeStamp
GetFrameTime() {
  if (sFrameTime.IsNull()) {
    return TimeStamp::Now();
  }
  return sFrameTime;
}

static PRThread* sControllerThread;

static void
AssertOnControllerThread() {
  if (!AsyncPanZoomController::GetThreadAssertionsEnabled()) {
    return;
  }

  static bool sControllerThreadDetermined = false;
  if (!sControllerThreadDetermined) {
    // Technically this may not actually pick up the correct controller thread,
    // if the first call to this method happens from a non-controller thread.
    // If the assertion below fires, it is possible that it is because
    // sControllerThread is not actually the controller thread.
    sControllerThread = PR_GetCurrentThread();
    sControllerThreadDetermined = true;
  }
  MOZ_ASSERT(sControllerThread == PR_GetCurrentThread());
}

class FlingAnimation: public AsyncPanZoomAnimation {
public:
  FlingAnimation(AsyncPanZoomController& aApzc,
                 bool aApplyAcceleration,
                 bool aAllowOverscroll)
    : AsyncPanZoomAnimation(TimeDuration::FromMilliseconds(gfxPrefs::APZFlingRepaintInterval()))
    , mApzc(aApzc)
    , mAllowOverscroll(aAllowOverscroll)
  {
    TimeStamp now = GetFrameTime();
    ScreenPoint velocity(mApzc.mX.GetVelocity(), mApzc.mY.GetVelocity());

    // If the last fling was very recent and in the same direction as this one,
    // boost the velocity to be the sum of the two. Check separate axes separately
    // because we could have two vertical flings with small horizontal components
    // on the opposite side of zero, and we still want the y-fling to get accelerated.
    // Note that the acceleration code is only applied on the APZC that receives the
    // actual touch event; the accelerated velocities are then handed off using the
    // normal HandOffFling codepath.
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
  virtual bool Sample(FrameMetrics& aFrameMetrics,
                      const TimeDuration& aDelta) MOZ_OVERRIDE
  {
    // If the fling is handed off to our APZC from a child, on the first call to
    // Sample() aDelta might be negative because it's computed as the sample time
    // from SampleContentTransformForFrame() minus our APZC's mLastSampleTime
    // which is the time the child handed off the fling from its call to
    // SampleContentTransformForFrame() with the same sample time. If we allow
    // the negative aDelta to be processed, it will yield a displacement in the
    // direction opposite to the fling, which can cause us to overscroll and
    // hand off the fling to _our_ parent, which effectively kills the fling.
    if (aDelta.ToMilliseconds() <= 0) {
      return true;
    }

    bool overscrolled = mApzc.IsOverscrolled();
    float friction = overscrolled ? gfxPrefs::APZOverscrollFlingFriction()
                                  : gfxPrefs::APZFlingFriction();
    float threshold = overscrolled ? gfxPrefs::APZOverscrollFlingStoppedThreshold()
                                   : gfxPrefs::APZFlingStoppedThreshold();

    bool shouldContinueFlingX = mApzc.mX.FlingApplyFrictionOrCancel(aDelta, friction, threshold),
         shouldContinueFlingY = mApzc.mY.FlingApplyFrictionOrCancel(aDelta, friction, threshold);
    // If we shouldn't continue the fling, let's just stop and repaint.
    if (!shouldContinueFlingX && !shouldContinueFlingY) {
      APZC_LOG("%p ending fling animation. overscrolled=%d\n", &mApzc, mApzc.IsOverscrolled());
      // This APZC or an APZC further down the handoff chain may be be overscrolled.
      // Start a snap-back animation on the overscrolled APZC.
      mDeferredTasks.append(NewRunnableMethod(&mApzc,
          &AsyncPanZoomController::CallSnapBackOverscrolledApzc));
      return false;
    }

    // AdjustDisplacement() zeroes out the Axis velocity if we're in overscroll.
    // Since we need to hand off the velocity to the tree manager in such a case,
    // we save it here. Would be ScreenVector instead of ScreenPoint if we had
    // vector classes.
    ScreenPoint velocity(mApzc.mX.GetVelocity(), mApzc.mY.GetVelocity());

    ScreenPoint offset = velocity * aDelta.ToMilliseconds();

    // Inversely scale the offset by the resolution (when you're zoomed further in,
    // the same swipe should move you a shorter distance).
    CSSPoint cssOffset = offset / aFrameMetrics.GetZoom();

    // Ordinarily we might need to do a ScheduleComposite if either of
    // the following AdjustDisplacement calls returns true, but this
    // is already running as part of a FlingAnimation, so we'll be compositing
    // per frame of animation anyway.
    CSSPoint overscroll;
    CSSPoint adjustedOffset;
    mApzc.mX.AdjustDisplacement(cssOffset.x, adjustedOffset.x, overscroll.x);
    mApzc.mY.AdjustDisplacement(cssOffset.y, adjustedOffset.y, overscroll.y);

    aFrameMetrics.ScrollBy(adjustedOffset);

    // The fling may have caused us to reach the end of our scroll range.
    if (!IsZero(overscroll)) {
      if (mAllowOverscroll) {
        // If this is a fling that allows overscroll, then go into overscroll.

        mApzc.OverscrollBy(overscroll);

        // Restore the velocity of the fling, which was zeroed out by
        // AdjustDisplacement().
        mApzc.mX.SetVelocity(velocity.x);
        mApzc.mY.SetVelocity(velocity.y);

      } else {
        // Otherwise, hand off the fling to the next APZC in the overscroll
        // handoff chain.

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
        mDeferredTasks.append(NewRunnableMethod(&mApzc,
                                                &AsyncPanZoomController::HandleFlingOverscroll,
                                                velocity));
      }
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
  bool mAllowOverscroll;
};

class ZoomAnimation: public AsyncPanZoomAnimation {
public:
  ZoomAnimation(CSSPoint aStartOffset, CSSToScreenScale aStartZoom,
                CSSPoint aEndOffset, CSSToScreenScale aEndZoom)
    : mTotalDuration(TimeDuration::FromMilliseconds(gfxPrefs::APZZoomAnimationDuration()))
    , mStartOffset(aStartOffset)
    , mStartZoom(aStartZoom)
    , mEndOffset(aEndOffset)
    , mEndZoom(aEndZoom)
  {}

  virtual bool Sample(FrameMetrics& aFrameMetrics,
                      const TimeDuration& aDelta) MOZ_OVERRIDE
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
    double sampledPosition = gComputedTimingFunction->GetValue(animPosition);

    // We scale the scrollOffset linearly with sampledPosition, so the zoom
    // needs to scale inversely to match.
    aFrameMetrics.SetZoom(CSSToScreenScale(1 /
      (sampledPosition / mEndZoom.scale +
      (1 - sampledPosition) / mStartZoom.scale)));

    aFrameMetrics.SetScrollOffset(CSSPoint::FromUnknownPoint(gfx::Point(
      mEndOffset.x * sampledPosition + mStartOffset.x * (1 - sampledPosition),
      mEndOffset.y * sampledPosition + mStartOffset.y * (1 - sampledPosition)
    )));

    return true;
  }

private:
  TimeDuration mDuration;
  const TimeDuration mTotalDuration;

  // Old metrics from before we started a zoom animation. This is only valid
  // when we are in the "ANIMATED_ZOOM" state. This is used so that we can
  // interpolate between the start and end frames. We only use the
  // |mViewportScrollOffset| and |mResolution| fields on this.
  CSSPoint mStartOffset;
  CSSToScreenScale mStartZoom;

  // Target metrics for a zoom to animation. This is only valid when we are in
  // the "ANIMATED_ZOOM" state. We only use the |mViewportScrollOffset| and
  // |mResolution| fields on this.
  CSSPoint mEndOffset;
  CSSToScreenScale mEndZoom;
};

class OverscrollSnapBackAnimation: public AsyncPanZoomAnimation {
public:
  OverscrollSnapBackAnimation(AsyncPanZoomController& aApzc)
    : mApzc(aApzc)
  {
    // Make sure the initial velocity is zero. This is normally the case
    // since we've just stopped a fling, but in some corner cases involving
    // handoff it could not be.
    mApzc.mX.SetVelocity(0);
    mApzc.mY.SetVelocity(0);
  }

  virtual bool Sample(FrameMetrics& aFrameMetrics,
                      const TimeDuration& aDelta) MOZ_OVERRIDE
  {
    // Can't inline these variables due to short-circuit evaluation.
    bool continueX = mApzc.mX.SampleSnapBack(aDelta);
    bool continueY = mApzc.mY.SampleSnapBack(aDelta);
    return continueX || continueY;
  }
private:
  AsyncPanZoomController& mApzc;
};

void
AsyncPanZoomController::SetFrameTime(const TimeStamp& aTime) {
  sFrameTime = aTime;
}

void
AsyncPanZoomController::SetThreadAssertionsEnabled(bool aEnabled) {
  sThreadAssertionsEnabled = aEnabled;
}

bool
AsyncPanZoomController::GetThreadAssertionsEnabled() {
  return sThreadAssertionsEnabled;
}

/*static*/ void
AsyncPanZoomController::InitializeGlobalState()
{
  MOZ_ASSERT(NS_IsMainThread());

  static bool sInitialized = false;
  if (sInitialized)
    return;
  sInitialized = true;

  gComputedTimingFunction = new css::ComputedTimingFunction();
  gComputedTimingFunction->Init(
    nsTimingFunction(NS_STYLE_TRANSITION_TIMING_FUNCTION_EASE));
  ClearOnShutdown(&gComputedTimingFunction);
}

AsyncPanZoomController::AsyncPanZoomController(uint64_t aLayersId,
                                               APZCTreeManager* aTreeManager,
                                               GeckoContentController* aGeckoContentController,
                                               GestureBehavior aGestures)
  :  mLayersId(aLayersId),
     mPaintThrottler(GetFrameTime()),
     mGeckoContentController(aGeckoContentController),
     mRefPtrMonitor("RefPtrMonitor"),
     mSharingFrameMetricsAcrossProcesses(false),
     mMonitor("AsyncPanZoomController"),
     mState(NOTHING),
     mX(MOZ_THIS_IN_INITIALIZER_LIST()),
     mY(MOZ_THIS_IN_INITIALIZER_LIST()),
     mPanDirRestricted(false),
     mZoomConstraints(false, false, MIN_ZOOM, MAX_ZOOM),
     mLastSampleTime(GetFrameTime()),
     mLastAsyncScrollTime(GetFrameTime()),
     mLastAsyncScrollOffset(0, 0),
     mCurrentAsyncScrollOffset(0, 0),
     mAsyncScrollTimeoutTask(nullptr),
     mTouchBlockBalance(0),
     mTreeManager(aTreeManager),
     mScrollParentId(FrameMetrics::NULL_SCROLL_ID),
     mAPZCId(sAsyncPanZoomControllerCount++),
     mSharedLock(nullptr)
{
  MOZ_COUNT_CTOR(AsyncPanZoomController);

  if (aGestures == USE_GESTURE_DETECTOR) {
    mGestureEventListener = new GestureEventListener(this);
  }
}

AsyncPanZoomController::~AsyncPanZoomController() {
  MOZ_COUNT_DTOR(AsyncPanZoomController);
}

PCompositorParent*
AsyncPanZoomController::GetSharedFrameMetricsCompositor()
{
  if (GetThreadAssertionsEnabled()) {
    Compositor::AssertOnCompositorThread();
  }

  if (mSharingFrameMetricsAcrossProcesses) {
    const CompositorParent::LayerTreeState* state = CompositorParent::GetIndirectShadowTree(mLayersId);
    // |state| may be null here if the CrossProcessCompositorParent has already been destroyed.
    return state ? state->mCrossProcessParent : nullptr;
  }
  return mCompositorParent.get();
}

already_AddRefed<GeckoContentController>
AsyncPanZoomController::GetGeckoContentController() const {
  MonitorAutoLock lock(mRefPtrMonitor);
  nsRefPtr<GeckoContentController> controller = mGeckoContentController;
  return controller.forget();
}

already_AddRefed<GestureEventListener>
AsyncPanZoomController::GetGestureEventListener() const {
  MonitorAutoLock lock(mRefPtrMonitor);
  nsRefPtr<GestureEventListener> listener = mGestureEventListener;
  return listener.forget();
}

void
AsyncPanZoomController::Destroy()
{
  CancelAnimation();

  mTouchBlockQueue.Clear();

  { // scope the lock
    MonitorAutoLock lock(mRefPtrMonitor);
    mGeckoContentController = nullptr;
    mGestureEventListener = nullptr;
  }
  mPrevSibling = nullptr;
  mLastChild = nullptr;
  mParent = nullptr;
  mTreeManager = nullptr;

  PCompositorParent* compositor = GetSharedFrameMetricsCompositor();
  // Only send the release message if the SharedFrameMetrics has been created.
  if (compositor && mSharedFrameMetricsBuffer) {
    unused << compositor->SendReleaseSharedCompositorFrameMetrics(mFrameMetrics.GetScrollId(), mAPZCId);
  }

  { // scope the lock
    ReentrantMonitorAutoEnter lock(mMonitor);
    mSharedFrameMetricsBuffer = nullptr;
    delete mSharedLock;
    mSharedLock = nullptr;
  }
}

bool
AsyncPanZoomController::IsDestroyed()
{
  return mTreeManager == nullptr;
}

/* static */float
AsyncPanZoomController::GetTouchStartTolerance()
{
  return (gfxPrefs::APZTouchStartTolerance() * APZCTreeManager::GetDPI());
}

/* static */AsyncPanZoomController::AxisLockMode AsyncPanZoomController::GetAxisLockMode()
{
  return static_cast<AxisLockMode>(gfxPrefs::APZAxisLockMode());
}

void
AsyncPanZoomController::CancelAnimationForHandoffChain()
{
  APZCTreeManager* treeManagerLocal = mTreeManager;
  if (treeManagerLocal && treeManagerLocal->CancelAnimationsForOverscrollHandoffChain()) {
    return;
  }
  NS_WARNING("Overscroll handoff chain was empty in CancelAnimationForHandoffChain! This should not be the case.");
  // Graceful handling of error condition
  CancelAnimation();
}

nsEventStatus AsyncPanZoomController::ReceiveInputEvent(const InputData& aEvent) {
  AssertOnControllerThread();

  if (aEvent.mInputType != MULTITOUCH_INPUT) {
    return HandleInputEvent(aEvent);
  }

  TouchBlockState* block = nullptr;
  if (aEvent.AsMultiTouchInput().mType == MultiTouchInput::MULTITOUCH_START) {
    block = StartNewTouchBlock(false);
    APZC_LOG("%p started new touch block %p\n", this, block);

    // We want to cancel animations here as soon as possible (i.e. without waiting for
    // content responses) because a finger has gone down and we don't want to keep moving
    // the content under the finger. However, to prevent "future" touchstart events from
    // interfering with "past" animations (i.e. from a previous touch block that is still
    // being processed) we only do this animation-cancellation if there are no older
    // touch blocks still in the queue.
    if (block == CurrentTouchBlock()) {
      if (GetVelocityVector().Length() > gfxPrefs::APZFlingStopOnTapThreshold()) {
        // If we're already in a fast fling, then we want the touch event to stop the fling
        // and to disallow the touch event from being used as part of a fling.
        block->DisallowSingleTap();
      }
      CancelAnimationForHandoffChain();
    }

    if (mFrameMetrics.mMayHaveTouchListeners || mFrameMetrics.mMayHaveTouchCaret) {
      // Content may intercept the touch events and prevent-default them. So we schedule
      // a timeout to give content time to do that.
      ScheduleContentResponseTimeout();
    } else {
      // Content won't prevent-default this, so we can just pretend like we scheduled
      // a timeout and it expired. Note that we will still receive a ContentReceivedTouch
      // callback for this block, and so we need to make sure we adjust the touch balance.
      APZC_LOG("%p not waiting for content response on block %p\n", this, block);
      mTouchBlockBalance++;
      block->TimeoutContentResponse();
    }
  } else if (mTouchBlockQueue.IsEmpty()) {
    NS_WARNING("Received a non-start touch event while no touch blocks active!");
  } else {
    // this touch is part of the most-recently created block
    block = mTouchBlockQueue.LastElement().get();
    APZC_LOG("%p received new event in block %p\n", this, block);
  }

  if (!block) {
    return nsEventStatus_eIgnore;
  }

  if (block == CurrentTouchBlock() && block->IsReadyForHandling()) {
    APZC_LOG("%p's current touch block is ready with preventdefault %d\n",
        this, block->IsDefaultPrevented());
    if (block->IsDefaultPrevented()) {
      return nsEventStatus_eIgnore;
    }
    return HandleInputEvent(aEvent);
  }

  // Otherwise, add it to the queue for the touch block
  block->AddEvent(aEvent.AsMultiTouchInput());
  return nsEventStatus_eConsumeDoDefault;
}

nsEventStatus AsyncPanZoomController::HandleInputEvent(const InputData& aEvent) {
  AssertOnControllerThread();

  nsEventStatus rv = nsEventStatus_eIgnore;

  switch (aEvent.mInputType) {
  case MULTITOUCH_INPUT: {
    const MultiTouchInput& multiTouchInput = aEvent.AsMultiTouchInput();

    nsRefPtr<GestureEventListener> listener = GetGestureEventListener();
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
    const PanGestureInput& panGestureInput = aEvent.AsPanGestureInput();
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
  default: NS_WARNING("Unhandled input event"); break;
  }

  return rv;
}

nsEventStatus AsyncPanZoomController::HandleGestureEvent(const InputData& aEvent)
{
  AssertOnControllerThread();

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

nsEventStatus AsyncPanZoomController::OnTouchStart(const MultiTouchInput& aEvent) {
  APZC_LOG("%p got a touch-start in state %d\n", this, mState);
  mPanDirRestricted = false;
  ScreenIntPoint point = GetFirstTouchScreenPoint(aEvent);

  switch (mState) {
    case FLING:
    case ANIMATING_ZOOM:
      CancelAnimationForHandoffChain();
      // Fall through.
    case NOTHING: {
      mX.StartTouch(point.x, aEvent.mTime);
      mY.StartTouch(point.y, aEvent.mTime);
      APZCTreeManager* treeManagerLocal = mTreeManager;
      nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
      if (treeManagerLocal && controller) {
        bool touchCanBePan = treeManagerLocal->CanBePanned(this);
        controller->NotifyAPZStateChange(
            GetGuid(), APZStateChange::StartTouch, touchCanBePan);
      }
      SetState(TOUCHING);
      break;
    }
    case TOUCHING:
    case PANNING:
    case PANNING_LOCKED_X:
    case PANNING_LOCKED_Y:
    case CROSS_SLIDING_X:
    case CROSS_SLIDING_Y:
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
    case NOTHING:
    case ANIMATING_ZOOM:
      // May happen if the user double-taps and drags without lifting after the
      // second tap. Ignore the move if this happens.
      return nsEventStatus_eIgnore;

    case CROSS_SLIDING_X:
    case CROSS_SLIDING_Y:
      // While cross-sliding, we don't want to consume any touchmove events for
      // panning or zooming, and let the caller handle them instead.
      return nsEventStatus_eIgnore;

    case TOUCHING: {
      float panThreshold = GetTouchStartTolerance();
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
      TrackTouch(aEvent);
      return nsEventStatus_eConsumeNoDefault;

    case PINCHING:
      // The scale gesture listener should have handled this.
      NS_WARNING("Gesture listener should have handled pinching in OnTouchMove.");
      return nsEventStatus_eIgnore;

    case SNAP_BACK:  // Should not receive a touch-move in the SNAP_BACK state
                     // as touch blocks that begin in an overscrolled state
                     // are ignored.
      NS_WARNING("Received impossible touch in OnTouchMove");
      break;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchEnd(const MultiTouchInput& aEvent) {
  APZC_LOG("%p got a touch-end in state %d\n", this, mState);

  OnTouchEndOrCancel();

  // In case no touch behavior triggered previously we can avoid sending
  // scroll events or requesting content repaint. This condition is added
  // to make tests consistent - in case touch-action is NONE (and therefore
  // no pans/zooms can be performed) we expected neither scroll or repaint
  // events.
  if (mState != NOTHING) {
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
  case CROSS_SLIDING_X:
  case CROSS_SLIDING_Y:
    SetState(NOTHING);
    return nsEventStatus_eIgnore;

  case PANNING:
  case PANNING_LOCKED_X:
  case PANNING_LOCKED_Y:
    {
      // Make a local copy of the tree manager pointer and check if it's not
      // null before calling FlushRepaintsForOverscrollHandoffChain().
      // This is necessary because Destroy(), which nulls out mTreeManager,
      // could be called concurrently.
      APZCTreeManager* treeManagerLocal = mTreeManager;
      if (treeManagerLocal) {
        if (!treeManagerLocal->FlushRepaintsForOverscrollHandoffChain()) {
          NS_WARNING("Overscroll handoff chain was empty during panning! This should not be the case.");
          // Graceful handling of error condition
          FlushRepaintForOverscrollHandoff();
        }
      }
    }
    mX.EndTouch(aEvent.mTime);
    mY.EndTouch(aEvent.mTime);
    SetState(FLING);
    APZC_LOG("%p starting a fling animation\n", this);
    StartAnimation(new FlingAnimation(*this,
                                      true  /* apply acceleration */,
                                      false /* allow overscroll */));
    return nsEventStatus_eConsumeNoDefault;

  case PINCHING:
    SetState(NOTHING);
    // Scale gesture listener should have handled this.
    NS_WARNING("Gesture listener should have handled pinching in OnTouchEnd.");
    return nsEventStatus_eIgnore;

  case SNAP_BACK:  // Should not receive a touch-move in the SNAP_BACK state
                   // as touch blocks that begin in an overscrolled state
                   // are ignored.
    NS_WARNING("Received impossible touch in OnTouchEnd");
    break;
  }

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnTouchCancel(const MultiTouchInput& aEvent) {
  APZC_LOG("%p got a touch-cancel in state %d\n", this, mState);
  OnTouchEndOrCancel();
  CancelAnimation();
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnScaleBegin(const PinchGestureInput& aEvent) {
  APZC_LOG("%p got a scale-begin in state %d\n", this, mState);

  // Note that there may not be a touch block at this point, if we received the
  // PinchGestureEvent directly from widget code without any touch events.
  if (HasReadyTouchBlock() && !CurrentTouchBlock()->TouchActionAllowsPinchZoom()) {
    return nsEventStatus_eIgnore;
  }

  if (!mZoomConstraints.mAllowZoom) {
    return nsEventStatus_eConsumeNoDefault;
  }

  SetState(PINCHING);
  mLastZoomFocus = ToParentLayerCoords(aEvent.mFocusPoint) - mFrameMetrics.mCompositionBounds.TopLeft();

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

  float prevSpan = aEvent.mPreviousSpan;
  if (fabsf(prevSpan) <= EPSILON || fabsf(aEvent.mCurrentSpan) <= EPSILON) {
    // We're still handling it; we've just decided to throw this event away.
    return nsEventStatus_eConsumeNoDefault;
  }

  float spanRatio = aEvent.mCurrentSpan / aEvent.mPreviousSpan;

  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    CSSToParentLayerScale userZoom = mFrameMetrics.GetZoomToParent();
    ParentLayerPoint focusPoint = ToParentLayerCoords(aEvent.mFocusPoint) - mFrameMetrics.mCompositionBounds.TopLeft();
    CSSPoint cssFocusPoint = focusPoint / userZoom;

    CSSPoint focusChange = (mLastZoomFocus - focusPoint) / userZoom;
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
    CSSPoint neededDisplacement;

    CSSToParentLayerScale realMinZoom = mZoomConstraints.mMinZoom * mFrameMetrics.mTransformScale;
    CSSToParentLayerScale realMaxZoom = mZoomConstraints.mMaxZoom * mFrameMetrics.mTransformScale;
    realMinZoom.scale = std::max(realMinZoom.scale,
                                 mFrameMetrics.mCompositionBounds.width / mFrameMetrics.mScrollableRect.width);
    realMinZoom.scale = std::max(realMinZoom.scale,
                                 mFrameMetrics.mCompositionBounds.height / mFrameMetrics.mScrollableRect.height);
    if (realMaxZoom < realMinZoom) {
      realMaxZoom = realMinZoom;
    }

    bool doScale = (spanRatio > 1.0 && userZoom < realMaxZoom) ||
                   (spanRatio < 1.0 && userZoom > realMinZoom);

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

    // We can get into a situation where we are overscrolled at the end of a
    // pinch if we go into overscroll with a two-finger pan, and then turn
    // that into a pinch by increasing the span sufficiently. In such a case,
    // there is no snap-back animation to get us out of overscroll, so we need
    // to get out of it somehow.
    mX.ClearOverscroll();
    mY.ClearOverscroll();

    ScheduleComposite();
    RequestContentRepaint();
    UpdateSharedCompositorFrameMetrics();
  }

  return nsEventStatus_eConsumeNoDefault;
}

bool
AsyncPanZoomController::ConvertToGecko(const ScreenPoint& aPoint, CSSPoint* aOut)
{
  APZCTreeManager* treeManagerLocal = mTreeManager;
  if (treeManagerLocal) {
    gfx3DMatrix transformToApzc;
    gfx3DMatrix transformToGecko;
    treeManagerLocal->GetInputTransforms(this, transformToApzc, transformToGecko);
    gfxPoint result = transformToGecko.Transform(gfxPoint(aPoint.x, aPoint.y));
    // NOTE: This isn't *quite* LayoutDevicePoint, we just don't have a name
    // for this coordinate space and it maps the closest to LayoutDevicePoint.
    LayoutDevicePoint layoutPoint = LayoutDevicePoint(result.x, result.y);
    { // scoped lock to access mFrameMetrics
      ReentrantMonitorAutoEnter lock(mMonitor);
      *aOut = layoutPoint / mFrameMetrics.mDevPixelsPerCSSPixel;
    }
    return true;
  }
  return false;
}

nsEventStatus AsyncPanZoomController::OnPanMayBegin(const PanGestureInput& aEvent) {
  APZC_LOG("%p got a pan-maybegin in state %d\n", this, mState);

  mX.StartTouch(aEvent.mPanStartPoint.x, aEvent.mTime);
  mY.StartTouch(aEvent.mPanStartPoint.y, aEvent.mTime);
  CancelAnimationForHandoffChain();

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnPanCancelled(const PanGestureInput& aEvent) {
  APZC_LOG("%p got a pan-cancelled in state %d\n", this, mState);

  mX.CancelTouch();
  mY.CancelTouch();

  return nsEventStatus_eConsumeNoDefault;
}


nsEventStatus AsyncPanZoomController::OnPanBegin(const PanGestureInput& aEvent) {
  APZC_LOG("%p got a pan-begin in state %d\n", this, mState);

  mX.StartTouch(aEvent.mPanStartPoint.x, aEvent.mTime);
  mY.StartTouch(aEvent.mPanStartPoint.y, aEvent.mTime);

  if (GetAxisLockMode() == FREE) {
    SetState(PANNING);
    return nsEventStatus_eConsumeNoDefault;
  }

  float dx = aEvent.mPanDisplacement.x, dy = aEvent.mPanDisplacement.y;
  double angle = atan2(dy, dx); // range [-pi, pi]
  angle = fabs(angle); // range [0, pi]

  HandlePanning(angle);

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnPan(const PanGestureInput& aEvent, bool aFingersOnTouchpad) {
  APZC_LOG("%p got a pan-pan in state %d\n", this, mState);

  // We need to update the axis velocity in order to get a useful display port
  // size and position. We need to do so even if this is a momentum pan (i.e.
  // aFingersOnTouchpad == false); in that case the "with touch" part is not
  // really appropriate, so we may want to rethink this at some point.
  mX.UpdateWithTouchAtDevicePoint(aEvent.mPanStartPoint.x, aEvent.mTime);
  mY.UpdateWithTouchAtDevicePoint(aEvent.mPanStartPoint.y, aEvent.mTime);

  HandlePanningUpdate(aEvent.mPanDisplacement.x, aEvent.mPanDisplacement.y);

  CallDispatchScroll(aEvent.mPanStartPoint, aEvent.mPanStartPoint + aEvent.mPanDisplacement, 0);

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnPanEnd(const PanGestureInput& aEvent) {
  APZC_LOG("%p got a pan-end in state %d\n", this, mState);

  mX.EndTouch(aEvent.mTime);
  mY.EndTouch(aEvent.mTime);
  RequestContentRepaint();

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnPanMomentumStart(const PanGestureInput& aEvent) {
  APZC_LOG("%p got a pan-momentumstart in state %d\n", this, mState);

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnPanMomentumEnd(const PanGestureInput& aEvent) {
  APZC_LOG("%p got a pan-momentumend in state %d\n", this, mState);

  // We need to reset the velocity to zero. We don't really have a "touch"
  // here because the touch has already ended long before the momentum
  // animation started, but I guess it doesn't really matter for now.
  mX.CancelTouch();
  mY.CancelTouch();

  RequestContentRepaint();

  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus AsyncPanZoomController::OnLongPress(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a long-press in state %d\n", this, mState);
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    int32_t modifiers = WidgetModifiersToDOMModifiers(aEvent.modifiers);
    CSSPoint geckoScreenPoint;
    if (ConvertToGecko(aEvent.mPoint, &geckoScreenPoint)) {
      StartNewTouchBlock(true);
      ScheduleContentResponseTimeout();
      controller->HandleLongTap(geckoScreenPoint, modifiers, GetGuid());
      return nsEventStatus_eConsumeNoDefault;
    }
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::OnLongPressUp(const TapGestureInput& aEvent) {
  APZC_LOG("%p got a long-tap-up in state %d\n", this, mState);
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    int32_t modifiers = WidgetModifiersToDOMModifiers(aEvent.modifiers);
    CSSPoint geckoScreenPoint;
    if (ConvertToGecko(aEvent.mPoint, &geckoScreenPoint)) {
      controller->HandleLongTapUp(geckoScreenPoint, modifiers, GetGuid());
      return nsEventStatus_eConsumeNoDefault;
    }
  }
  return nsEventStatus_eIgnore;
}

nsEventStatus AsyncPanZoomController::GenerateSingleTap(const ScreenIntPoint& aPoint, mozilla::Modifiers aModifiers) {
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    CSSPoint geckoScreenPoint;
    if (ConvertToGecko(aPoint, &geckoScreenPoint)) {
      if (!CurrentTouchBlock()->SetSingleTapOccurred()) {
        return nsEventStatus_eIgnore;
      }
      // Because this may be being running as part of APZCTreeManager::ReceiveInputEvent,
      // calling controller->HandleSingleTap directly might mean that content receives
      // the single tap message before the corresponding touch-up. To avoid that we
      // schedule the singletap message to run on the next spin of the event loop.
      // See bug 965381 for the issue this was causing.
      controller->PostDelayedTask(
        NewRunnableMethod(controller.get(), &GeckoContentController::HandleSingleTap,
                          geckoScreenPoint, WidgetModifiersToDOMModifiers(aModifiers),
                          GetGuid()),
        0);
      return nsEventStatus_eConsumeNoDefault;
    }
  }
  return nsEventStatus_eIgnore;
}

void AsyncPanZoomController::OnTouchEndOrCancel() {
  if (nsRefPtr<GeckoContentController> controller = GetGeckoContentController()) {
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
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    if (mZoomConstraints.mAllowDoubleTapZoom && CurrentTouchBlock()->TouchActionAllowsDoubleTapZoom()) {
      int32_t modifiers = WidgetModifiersToDOMModifiers(aEvent.modifiers);
      CSSPoint geckoScreenPoint;
      if (ConvertToGecko(aEvent.mPoint, &geckoScreenPoint)) {
        controller->HandleDoubleTap(geckoScreenPoint, modifiers, GetGuid());
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

float AsyncPanZoomController::PanDistance() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  return NS_hypot(mX.PanDistance(), mY.PanDistance());
}

const ScreenPoint AsyncPanZoomController::GetVelocityVector() {
  return ScreenPoint(mX.GetVelocity(), mY.GetVelocity());
}

void AsyncPanZoomController::HandlePanningWithTouchAction(double aAngle) {
  // Handling of cross sliding will need to be added in this method after touch-action released
  // enabled by default.
  if (CurrentTouchBlock()->TouchActionAllowsPanningXY()) {
    if (mX.CanScrollNow() && mY.CanScrollNow()) {
      if (IsCloseToHorizontal(aAngle, AXIS_LOCK_ANGLE)) {
        mY.SetAxisLocked(true);
        SetState(PANNING_LOCKED_X);
      } else if (IsCloseToVertical(aAngle, AXIS_LOCK_ANGLE)) {
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
    if (IsCloseToHorizontal(aAngle, ALLOWED_DIRECT_PAN_ANGLE)) {
      mY.SetAxisLocked(true);
      SetState(PANNING_LOCKED_X);
      mPanDirRestricted = true;
    } else {
      // Don't treat these touches as pan/zoom movements since 'touch-action' value
      // requires it.
      SetState(NOTHING);
    }
  } else if (CurrentTouchBlock()->TouchActionAllowsPanningY()) {
    if (IsCloseToVertical(aAngle, ALLOWED_DIRECT_PAN_ANGLE)) {
      mX.SetAxisLocked(true);
      SetState(PANNING_LOCKED_Y);
      mPanDirRestricted = true;
    } else {
      SetState(NOTHING);
    }
  } else {
    SetState(NOTHING);
  }
}

void AsyncPanZoomController::HandlePanning(double aAngle) {
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (!gfxPrefs::APZCrossSlideEnabled() && (!mX.CanScrollNow() || !mY.CanScrollNow())) {
    SetState(PANNING);
  } else if (IsCloseToHorizontal(aAngle, AXIS_LOCK_ANGLE)) {
    mY.SetAxisLocked(true);
    if (mX.CanScrollNow()) {
      SetState(PANNING_LOCKED_X);
    } else {
      SetState(CROSS_SLIDING_X);
      mX.SetAxisLocked(true);
    }
  } else if (IsCloseToVertical(aAngle, AXIS_LOCK_ANGLE)) {
    mX.SetAxisLocked(true);
    if (mY.CanScrollNow()) {
      SetState(PANNING_LOCKED_Y);
    } else {
      SetState(CROSS_SLIDING_Y);
      mY.SetAxisLocked(true);
    }
  } else {
    SetState(PANNING);
  }
}

void AsyncPanZoomController::HandlePanningUpdate(float aDX, float aDY) {
  // If we're axis-locked, check if the user is trying to break the lock
  if (GetAxisLockMode() == STICKY && !mPanDirRestricted) {

    double angle = atan2(aDY, aDX); // range [-pi, pi]
    angle = fabs(angle); // range [0, pi]

    float breakThreshold = AXIS_BREAKOUT_THRESHOLD * APZCTreeManager::GetDPI();

    if (fabs(aDX) > breakThreshold || fabs(aDY) > breakThreshold) {
      if (mState == PANNING_LOCKED_X || mState == CROSS_SLIDING_X) {
        if (!IsCloseToHorizontal(angle, AXIS_BREAKOUT_ANGLE)) {
          mY.SetAxisLocked(false);
          SetState(PANNING);
        }
      } else if (mState == PANNING_LOCKED_Y || mState == CROSS_SLIDING_Y) {
        if (!IsCloseToVertical(angle, AXIS_BREAKOUT_ANGLE)) {
          mX.SetAxisLocked(false);
          SetState(PANNING);
        }
      }
    }
  }
}

nsEventStatus AsyncPanZoomController::StartPanning(const MultiTouchInput& aEvent) {
  ReentrantMonitorAutoEnter lock(mMonitor);

  ScreenIntPoint point = GetFirstTouchScreenPoint(aEvent);
  float dx = mX.PanDistance(point.x);
  float dy = mY.PanDistance(point.y);

  // When the touch move breaks through the pan threshold, reposition the touch down origin
  // so the page won't jump when we start panning.
  mX.StartTouch(point.x, aEvent.mTime);
  mY.StartTouch(point.y, aEvent.mTime);

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

  if (IsPanningState(mState)) {
    if (nsRefPtr<GeckoContentController> controller = GetGeckoContentController()) {
      controller->NotifyAPZStateChange(GetGuid(), APZStateChange::StartPanning);
    }
    return nsEventStatus_eConsumeNoDefault;
  }
  // Don't consume an event that didn't trigger a panning.
  return nsEventStatus_eIgnore;
}

void AsyncPanZoomController::UpdateWithTouchAtDevicePoint(const MultiTouchInput& aEvent) {
  ScreenIntPoint point = GetFirstTouchScreenPoint(aEvent);
  mX.UpdateWithTouchAtDevicePoint(point.x, aEvent.mTime);
  mY.UpdateWithTouchAtDevicePoint(point.y, aEvent.mTime);
}

bool AsyncPanZoomController::AttemptScroll(const ScreenPoint& aStartPoint,
                                           const ScreenPoint& aEndPoint,
                                           uint32_t aOverscrollHandoffChainIndex) {

  // "start - end" rather than "end - start" because e.g. moving your finger
  // down (*positive* direction along y axis) causes the vertical scroll offset
  // to *decrease* as the page follows your finger.
  ScreenPoint displacement = aStartPoint - aEndPoint;

  ScreenPoint overscroll;  // will be used outside monitor block
  CSSPoint cssOverscroll;  // ditto
  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    CSSToScreenScale zoom = mFrameMetrics.GetZoom();

    // Inversely scale the offset by the resolution (when you're zoomed further in,
    // the same swipe should move you a shorter distance).
    CSSPoint cssDisplacement = displacement / zoom;

    CSSPoint adjustedDisplacement;
    bool xChanged = mX.AdjustDisplacement(cssDisplacement.x, adjustedDisplacement.x, cssOverscroll.x);
    bool yChanged = mY.AdjustDisplacement(cssDisplacement.y, adjustedDisplacement.y, cssOverscroll.y);
    if (xChanged || yChanged) {
      ScheduleComposite();
    }

    overscroll = cssOverscroll * zoom;

    if (!IsZero(adjustedDisplacement)) {
      ScrollBy(adjustedDisplacement);
      ScheduleCompositeAndMaybeRepaint();
      UpdateSharedCompositorFrameMetrics();
    }
  }

  // If we consumed the entire displacement as a normal scroll, great.
  if (IsZero(overscroll)) {
    return true;
  }

  // If there is overscroll, first try to hand it off to an APZC later
  // in the handoff chain to consume (either as a normal scroll or as
  // overscroll).
  // Note: "+ overscroll" rather than "- overscroll" because "overscroll"
  // is what's left of "displacement", and "displacement" is "start - end".
  if (CallDispatchScroll(aEndPoint + overscroll, aEndPoint, aOverscrollHandoffChainIndex + 1)) {
    return true;
  }

  // If there is no APZC later in the handoff chain that accepted the
  // overscroll, try to accept it ourselves. We only accept it if we
  // are pannable.
  APZC_LOG("%p taking overscroll during panning\n", this);
  return OverscrollBy(cssOverscroll);
}

bool AsyncPanZoomController::OverscrollBy(const CSSPoint& aOverscroll) {
  if (!gfxPrefs::APZOverscrollEnabled()) {
    return false;
  }

  ReentrantMonitorAutoEnter lock(mMonitor);
  // Do not go into overscroll in a direction in which we have no room to
  // scroll to begin with.
  bool xCanScroll = mX.CanScroll();
  bool yCanScroll = mY.CanScroll();
  if (xCanScroll) {
    mX.OverscrollBy(aOverscroll.x);
  }
  if (yCanScroll) {
    mY.OverscrollBy(aOverscroll.y);
  }
  if (xCanScroll || yCanScroll) {
    ScheduleComposite();
    return true;
  }
  // TODO(botond): If one of the x- or y-overscroll was not accepted, we
  // may want to propagate that one to an APZC earlier in the handoff chain.
  return false;
}

void AsyncPanZoomController::AcceptFling(const ScreenPoint& aVelocity,
                                         bool aAllowOverscroll) {
  // We may have a pre-existing velocity for whatever reason (for example,
  // a previously handed off fling). We don't want to clobber that.
  mX.SetVelocity(mX.GetVelocity() + aVelocity.x);
  mY.SetVelocity(mY.GetVelocity() + aVelocity.y);
  SetState(FLING);
  StartAnimation(new FlingAnimation(*this, false /* no acceleration */,
                                    aAllowOverscroll));
}

bool AsyncPanZoomController::TakeOverFling(ScreenPoint aVelocity) {
  // If we are pannable, take over the fling ourselves.
  if (IsPannable()) {
    AcceptFling(aVelocity, false /* do not allow overscroll */);
    return true;
  }

  // Otherwise, hand the fling back to the tree manager to pass on to the
  // next APZC in the handoff chain. Had we started a fling animation in this
  // APZC, we would have done this hand-off on its first frame anyways, but
  // doing it here allows the tree manager to tell the previous APZC to enter
  // an overscroll fling if nothing further in the chain wants the fling.
  APZCTreeManager* treeManagerLocal = mTreeManager;
  return treeManagerLocal
      && treeManagerLocal->HandOffFling(this, aVelocity);
}

void AsyncPanZoomController::HandleFlingOverscroll(const ScreenPoint& aVelocity) {
  APZCTreeManager* treeManagerLocal = mTreeManager;
  if (!(treeManagerLocal && treeManagerLocal->HandOffFling(this, aVelocity))) {
    // No one wanted the fling, so we enter into an overscroll fling ourselves.
    if (IsPannable()) {
      AcceptFling(aVelocity, true /* allow overscroll */);
    }
  }
}

void AsyncPanZoomController::StartSnapBack() {
  SetState(SNAP_BACK);
  StartAnimation(new OverscrollSnapBackAnimation(*this));
}

bool AsyncPanZoomController::CallDispatchScroll(const ScreenPoint& aStartPoint, const ScreenPoint& aEndPoint,
                                                uint32_t aOverscrollHandoffChainIndex) {
  // Make a local copy of the tree manager pointer and check if it's not
  // null before calling DispatchScroll(). This is necessary because
  // Destroy(), which nulls out mTreeManager, could be called concurrently.
  APZCTreeManager* treeManagerLocal = mTreeManager;
  return treeManagerLocal
      && treeManagerLocal->DispatchScroll(this, aStartPoint, aEndPoint,
                                          aOverscrollHandoffChainIndex);
}

void AsyncPanZoomController::CallSnapBackOverscrolledApzc() {
  APZCTreeManager* treeManagerLocal = mTreeManager;
  if (treeManagerLocal) {
    treeManagerLocal->SnapBackOverscrolledApzc(this);
  }
}

void AsyncPanZoomController::TrackTouch(const MultiTouchInput& aEvent) {
  ScreenIntPoint prevTouchPoint(mX.GetPos(), mY.GetPos());
  ScreenIntPoint touchPoint = GetFirstTouchScreenPoint(aEvent);

  float dx = mX.PanDistance(touchPoint.x);
  float dy = mY.PanDistance(touchPoint.y);
  HandlePanningUpdate(dx, dy);

  UpdateWithTouchAtDevicePoint(aEvent);

  if (prevTouchPoint != touchPoint) {
    CallDispatchScroll(prevTouchPoint, touchPoint, 0);
  }
}

ScreenIntPoint& AsyncPanZoomController::GetFirstTouchScreenPoint(const MultiTouchInput& aEvent) {
  return ((SingleTouchData&)aEvent.mTouches[0]).mScreenPoint;
}

void AsyncPanZoomController::StartAnimation(AsyncPanZoomAnimation* aAnimation)
{
  ReentrantMonitorAutoEnter lock(mMonitor);
  mAnimation = aAnimation;
  mLastSampleTime = GetFrameTime();
  ScheduleComposite();
}

void AsyncPanZoomController::CancelAnimation() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  APZC_LOG("%p running CancelAnimation in state %d\n", this, mState);
  SetState(NOTHING);
  mAnimation = nullptr;
  // Since there is no animation in progress now the axes should
  // have no velocity either.
  mX.SetVelocity(0);
  mY.SetVelocity(0);
  // Setting the state to nothing and cancelling the animation can
  // preempt normal mechanisms for relieving overscroll, so we need to clear
  // overscroll here.
  if (mX.IsOverscrolled() || mY.IsOverscrolled()) {
    mX.ClearOverscroll();
    mY.ClearOverscroll();
    RequestContentRepaint();
    ScheduleComposite();
    UpdateSharedCompositorFrameMetrics();
  }
}

void AsyncPanZoomController::SetCompositorParent(CompositorParent* aCompositorParent) {
  mCompositorParent = aCompositorParent;
}

void AsyncPanZoomController::ShareFrameMetricsAcrossProcesses() {
  mSharingFrameMetricsAcrossProcesses = true;
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
  float xMultiplier = fabsf(aVelocity.x) < gfxPrefs::APZMinSkateSpeed()
                        ? gfxPrefs::APZXStationarySizeMultiplier()
                        : gfxPrefs::APZXSkateSizeMultiplier();
  float yMultiplier = fabsf(aVelocity.y) < gfxPrefs::APZMinSkateSpeed()
                        ? gfxPrefs::APZYStationarySizeMultiplier()
                        : gfxPrefs::APZYSkateSizeMultiplier();

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
  float xSlack = std::max(0.0f, aDisplayPortSize.width - aScrollableRect.width);
  float ySlack = std::max(0.0f, aDisplayPortSize.height - aScrollableRect.height);

  if (ySlack > 0) {
    // Reassign wasted y-axis displayport to the x-axis
    aDisplayPortSize.height -= ySlack;
    float xExtra = ySlack * aDisplayPortSize.width / aDisplayPortSize.height;
    aDisplayPortSize.width += xExtra;
  } else if (xSlack > 0) {
    // Reassign wasted x-axis displayport to the y-axis
    aDisplayPortSize.width -= xSlack;
    float yExtra = xSlack * aDisplayPortSize.height / aDisplayPortSize.width;
    aDisplayPortSize.height += yExtra;
  }
}

/* static */
const LayerMargin AsyncPanZoomController::CalculatePendingDisplayPort(
  const FrameMetrics& aFrameMetrics,
  const ScreenPoint& aVelocity,
  double aEstimatedPaintDuration)
{
  CSSSize compositionSize = aFrameMetrics.CalculateBoundedCompositedSizeInCssPixels();
  CSSPoint velocity = aVelocity / aFrameMetrics.GetZoom();
  CSSPoint scrollOffset = aFrameMetrics.GetScrollOffset();
  CSSRect scrollableRect = aFrameMetrics.GetExpandedScrollableRect();

  // Calculate the displayport size based on how fast we're moving along each axis.
  CSSSize displayPortSize = CalculateDisplayPortSize(compositionSize, velocity);

  if (gfxPrefs::APZEnlargeDisplayPortWhenClipped()) {
    RedistributeDisplayPortExcess(displayPortSize, scrollableRect);
  }

  // Offset the displayport, depending on how fast we're moving and the
  // estimated time it takes to paint, to try to minimise checkerboarding.
  float estimatedPaintDurationMillis = (float)(aEstimatedPaintDuration * 1000.0);
  float paintFactor = (gfxPrefs::APZUsePaintDuration() ? estimatedPaintDurationMillis : 50.0f);
  CSSRect displayPort = CSSRect(scrollOffset + (velocity * paintFactor * gfxPrefs::APZVelocityBias()),
                                displayPortSize);

  // Re-center the displayport based on its expansion over the composition size.
  displayPort.MoveBy((compositionSize.width - displayPort.width)/2.0f,
                     (compositionSize.height - displayPort.height)/2.0f);

  // Make sure the displayport remains within the scrollable rect.
  displayPort = displayPort.ForceInside(scrollableRect) - scrollOffset;

  APZC_LOG_FM(aFrameMetrics,
    "Calculated displayport as (%f %f %f %f) from velocity (%f %f) paint time %f metrics",
    displayPort.x, displayPort.y, displayPort.width, displayPort.height,
    aVelocity.x, aVelocity.y, (float)estimatedPaintDurationMillis);

  CSSMargin cssMargins;
  cssMargins.left = -displayPort.x;
  cssMargins.top = -displayPort.y;
  cssMargins.right = displayPort.width - compositionSize.width - cssMargins.left;
  cssMargins.bottom = displayPort.height - compositionSize.height - cssMargins.top;

  LayerMargin layerMargins = cssMargins * aFrameMetrics.LayersPixelsPerCSSPixel();

  return layerMargins;
}

void AsyncPanZoomController::ScheduleComposite() {
  if (mCompositorParent) {
    mCompositorParent->ScheduleRenderOnCompositorThread();
  }
}

void AsyncPanZoomController::ScheduleCompositeAndMaybeRepaint() {
  ScheduleComposite();

  TimeDuration timePaintDelta = mPaintThrottler.TimeSinceLastRequest(GetFrameTime());
  if (timePaintDelta.ToMilliseconds() > gfxPrefs::APZPanRepaintInterval()) {
    RequestContentRepaint();
  }
}

void AsyncPanZoomController::FlushRepaintForOverscrollHandoff() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  RequestContentRepaint();
  UpdateSharedCompositorFrameMetrics();
}

bool AsyncPanZoomController::SnapBackIfOverscrolled() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  if (IsOverscrolled()) {
    APZC_LOG("%p is overscrolled, starting snap-back\n", this);
    StartSnapBack();
    return true;
  }
  return false;
}

bool AsyncPanZoomController::IsPannable() const {
  ReentrantMonitorAutoEnter lock(mMonitor);
  return mX.CanScroll() || mY.CanScroll();
}

int32_t AsyncPanZoomController::GetLastTouchIdentifier() const {
  nsRefPtr<GestureEventListener> listener = GetGestureEventListener();
  return listener ? listener->GetLastTouchIdentifier() : -1;
}

void AsyncPanZoomController::RequestContentRepaint() {
  RequestContentRepaint(mFrameMetrics);
}

void AsyncPanZoomController::RequestContentRepaint(FrameMetrics& aFrameMetrics) {
  aFrameMetrics.SetDisplayPortMargins(
    CalculatePendingDisplayPort(aFrameMetrics,
                                GetVelocityVector(),
                                mPaintThrottler.AverageDuration().ToSeconds()));
  aFrameMetrics.SetUseDisplayPortMargins();

  // If we're trying to paint what we already think is painted, discard this
  // request since it's a pointless paint.
  LayerMargin marginDelta = mLastPaintRequestMetrics.GetDisplayPortMargins()
                          - aFrameMetrics.GetDisplayPortMargins();
  if (fabsf(marginDelta.left) < EPSILON &&
      fabsf(marginDelta.top) < EPSILON &&
      fabsf(marginDelta.right) < EPSILON &&
      fabsf(marginDelta.bottom) < EPSILON &&
      fabsf(mLastPaintRequestMetrics.GetScrollOffset().x -
            aFrameMetrics.GetScrollOffset().x) < EPSILON &&
      fabsf(mLastPaintRequestMetrics.GetScrollOffset().y -
            aFrameMetrics.GetScrollOffset().y) < EPSILON &&
      aFrameMetrics.GetZoom() == mLastPaintRequestMetrics.GetZoom() &&
      fabsf(aFrameMetrics.mViewport.width - mLastPaintRequestMetrics.mViewport.width) < EPSILON &&
      fabsf(aFrameMetrics.mViewport.height - mLastPaintRequestMetrics.mViewport.height) < EPSILON) {
    return;
  }

  SendAsyncScrollEvent();
  mPaintThrottler.PostTask(
    FROM_HERE,
    UniquePtr<CancelableTask>(NewRunnableMethod(this,
                      &AsyncPanZoomController::DispatchRepaintRequest,
                      aFrameMetrics)),
    GetFrameTime());

  aFrameMetrics.SetPresShellId(mLastContentPaintMetrics.GetPresShellId());
  mLastPaintRequestMetrics = aFrameMetrics;
}

/*static*/ CSSRect
GetDisplayPortRect(const FrameMetrics& aFrameMetrics)
{
  // This computation is based on what happens in CalculatePendingDisplayPort. If that
  // changes then this might need to change too
  CSSRect baseRect(aFrameMetrics.GetScrollOffset(),
                   aFrameMetrics.CalculateBoundedCompositedSizeInCssPixels());
  baseRect.Inflate(aFrameMetrics.GetDisplayPortMargins() / aFrameMetrics.LayersPixelsPerCSSPixel());
  return baseRect;
}

void
AsyncPanZoomController::DispatchRepaintRequest(const FrameMetrics& aFrameMetrics) {
  nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
  if (controller) {
    APZC_LOG_FM(aFrameMetrics, "%p requesting content repaint", this);
    LogRendertraceRect(GetGuid(), "requested displayport", "yellow", GetDisplayPortRect(aFrameMetrics));

    controller->RequestContentRepaint(aFrameMetrics);
    mLastDispatchedPaintMetrics = aFrameMetrics;
  }
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

bool AsyncPanZoomController::UpdateAnimation(const TimeStamp& aSampleTime,
                                             Vector<Task*>* aOutDeferredTasks)
{
  if (mAnimation) {
    bool continueAnimation = mAnimation->Sample(mFrameMetrics, aSampleTime - mLastSampleTime);
    *aOutDeferredTasks = mAnimation->TakeDeferredTasks();
    if (continueAnimation) {
      if (mPaintThrottler.TimeSinceLastRequest(aSampleTime) >
          mAnimation->mRepaintInterval) {
        RequestContentRepaint();
      }
    } else {
      mAnimation = nullptr;
      SetState(NOTHING);
      SendAsyncScrollEvent();
      RequestContentRepaint();
    }
    UpdateSharedCompositorFrameMetrics();
    mLastSampleTime = aSampleTime;
    return true;
  }
  return false;
}

void AsyncPanZoomController::GetOverscrollTransform(ViewTransform* aTransform) const {
  // The overscroll effect is a combination of a translation in
  // the direction of overscroll, and shrinking in both directions.
  // With the effect applied, we can think of the composited region as being
  // made up of the following subregions.
  //  (1) The shrunk content (or a portion of it) that used to fill the
  //      composited region.
  //  (2) The space created along the axis that has overscroll. This space is
  //      blank, filled by the background color of the overscrolled content.
  //      TODO(botond): Implement handling of background color.
  //  (3) The space created along the other axis. There may or may not be
  //      content available to fill this space, depending on our scroll
  //      position along this axis. If there is content, it's shown from the
  //      displayport. (TODO: Currently we don't take any measures to ensure
  //      that the displayport is large enough to have this content. Perhaps
  //      we should.) Otherwise, these spaces are also blank like (2).
  // To illustrate, for the case where we are overscrolling past the top of
  // the page, these regions are (1) the bottom-centre region, (2) the top
  // regions, and (3) the bottom-left and bottom-right regions of the
  // composited area, respectively.

  // The maximum proportion of the composition length which can become blank
  // space along an axis as we overscroll along that axis.
  const float kClamping = gfxPrefs::APZOverscrollClamping();

  // The proportion of the composition length which will become blank space
  // along each axis as a result of overscroll along that axis. Since
  // Axis::ApplyResistance() keeps the magnitude of the overscroll in the range
  // [0, GetCompositionLength()], these scale factors should be in the range
  // [0, kClamping].
  float spacePropX = kClamping * fabsf(mX.GetOverscroll()) / mX.GetCompositionLength();
  float spacePropY = kClamping * fabsf(mY.GetOverscroll()) / mY.GetCompositionLength();

  // The fraction of the proportions above which will become blank space along
  // the _opposite_ axis as we zoom out as a result of overscroll along an axis.
  // This creates a 3D effect, as in the layer were moving backward along a "z"
  // axis.
  const float kZEffect = gfxPrefs::APZOverscrollZEffect();

  // The translation to apply for overscroll along the x axis.
  CSSPoint translationX;
  if (mX.IsOverscrolled()) {
    // Keep the content centred vertically as we zoom out.
    translationX.y = (spacePropX * kZEffect * mY.GetCompositionLength()) / 2;

    if (mX.GetOverscroll() < 0) {
      // Overscroll on left.
      translationX.x = spacePropX * mX.GetCompositionLength();
    } else {
      // Overscroll on right.
      // Note that zooming out already moves the content at the right edge
      // of the composition bounds to the left, but since the zooming is
      // dampened by kZEffect, it doesn't take us as far as we want to go.
      translationX.x = - (spacePropX * (1 - kZEffect) * mX.GetCompositionLength());
    }
  }

  // The translation to apply for overscroll along the y axis.
  CSSPoint translationY;
  if (mY.IsOverscrolled()) {
    // Keep the content centred horizontally as we zoom out.
    translationY.x = (spacePropY * kZEffect * mX.GetCompositionLength()) / 2;

    if (mY.GetOverscroll() < 0) {
      // Overscroll at top.
      translationY.y = spacePropY * mY.GetCompositionLength();
    } else {
      // Overscroll at bottom.
      // Note that zooming out already moves the content at the bottom edge
      // of the composition bounds up, but since the zooming is
      // dampened by kZEffect, it doesn't take us as far as we want to go.
      translationY.y = - (spacePropY * (1 - kZEffect) * mY.GetCompositionLength());
    }
  }

  // Combine the transformations along the two axes.
  float spaceProp = sqrtf(spacePropX * spacePropX + spacePropY * spacePropY);
  CSSPoint translation = translationX + translationY;

  float scale = 1 - (kZEffect * spaceProp);

  // In a ViewTransform, the translation is applied before the scale. We want
  // to apply our translation after our scale, so we compensate for that here.
  translation.x /= scale;
  translation.y /= scale;

  // Finally, apply the transformations.
  aTransform->mScale.scale *= scale;
  aTransform->mTranslation += translation * mFrameMetrics.LayersPixelsPerCSSPixel();
}

bool AsyncPanZoomController::SampleContentTransformForFrame(const TimeStamp& aSampleTime,
                                                            ViewTransform* aOutTransform,
                                                            ScreenPoint& aScrollOffset,
                                                            ViewTransform* aOutOverscrollTransform) {
  // The eventual return value of this function. The compositor needs to know
  // whether or not to advance by a frame as soon as it can. For example, if a
  // fling is happening, it has to keep compositing so that the animation is
  // smooth. If an animation frame is requested, it is the compositor's
  // responsibility to schedule a composite.
  bool requestAnimationFrame = false;
  Vector<Task*> deferredTasks;

  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    requestAnimationFrame = UpdateAnimation(aSampleTime, &deferredTasks);

    aScrollOffset = mFrameMetrics.GetScrollOffset() * mFrameMetrics.GetZoom();
    *aOutTransform = GetCurrentAsyncTransform();

    // If we are overscrolled, we would like the compositor to apply an
    // additional transform that produces an overscroll effect.
    if (aOutOverscrollTransform && IsOverscrolled()) {
      GetOverscrollTransform(aOutOverscrollTransform);

      // Since the caller will apply aOverscrollTransform after aNewTransform,
      // aOverscrollTransform's translation will be not be scaled by
      // aNewTransform's scale. Since the resulting transform is then
      // multiplied by the CSS transform, which cancels out the non-transient
      // part of aNewTransform->mScale, this results in an overscroll
      // translation whose magnitude varies with the zoom. To avoid this,
      // we adjust for that here.
      aOutOverscrollTransform->mTranslation.x *= aOutTransform->mScale.scale;
      aOutOverscrollTransform->mTranslation.y *= aOutTransform->mScale.scale;
    }

    LogRendertraceRect(GetGuid(), "viewport", "red",
      CSSRect(mFrameMetrics.GetScrollOffset(),
              mFrameMetrics.CalculateCompositedSizeInCssPixels()));

    mCurrentAsyncScrollOffset = mFrameMetrics.GetScrollOffset();
  }

  // Execute any deferred tasks queued up by mAnimation's Sample() (called by
  // UpdateAnimation()). This needs to be done after the monitor is released
  // since the tasks are allowed to call APZCTreeManager methods which can grab
  // the tree lock.
  for (uint32_t i = 0; i < deferredTasks.length(); ++i) {
    deferredTasks[i]->Run();
    delete deferredTasks[i];
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
  if (delta.ToMilliseconds() > gfxPrefs::APZAsyncScrollThrottleTime() &&
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
                                            gfxPrefs::APZAsyncScrollTimeout());
  }

  return requestAnimationFrame;
}

ViewTransform AsyncPanZoomController::GetCurrentAsyncTransform() {
  ReentrantMonitorAutoEnter lock(mMonitor);

  CSSPoint lastPaintScrollOffset;
  if (mLastContentPaintMetrics.IsScrollable()) {
    lastPaintScrollOffset = mLastContentPaintMetrics.GetScrollOffset();
  }

  CSSPoint currentScrollOffset = mFrameMetrics.GetScrollOffset() +
    mTestAsyncScrollOffset;

  // If checkerboarding has been disallowed, clamp the scroll position to stay
  // within rendered content.
  if (!gfxPrefs::APZAllowCheckerboarding() &&
      !mLastContentPaintMetrics.mDisplayPort.IsEmpty()) {
    CSSSize compositedSize = mLastContentPaintMetrics.CalculateCompositedSizeInCssPixels();
    CSSPoint maxScrollOffset = lastPaintScrollOffset +
      CSSPoint(mLastContentPaintMetrics.mDisplayPort.XMost() - compositedSize.width,
               mLastContentPaintMetrics.mDisplayPort.YMost() - compositedSize.height);
    CSSPoint minScrollOffset = lastPaintScrollOffset + mLastContentPaintMetrics.mDisplayPort.TopLeft();

    if (minScrollOffset.x < maxScrollOffset.x) {
      currentScrollOffset.x = clamped(currentScrollOffset.x, minScrollOffset.x, maxScrollOffset.x);
    }
    if (minScrollOffset.y < maxScrollOffset.y) {
      currentScrollOffset.y = clamped(currentScrollOffset.y, minScrollOffset.y, maxScrollOffset.y);
    }
  }

  LayerPoint translation = (currentScrollOffset - lastPaintScrollOffset)
                         * mLastContentPaintMetrics.LayersPixelsPerCSSPixel();

  return ViewTransform(-translation,
                       mFrameMetrics.GetZoom()
                     / mLastContentPaintMetrics.mDevPixelsPerCSSPixel
                     / mFrameMetrics.GetParentResolution());
}

gfx3DMatrix AsyncPanZoomController::GetNontransientAsyncTransform() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  return gfx3DMatrix::ScalingMatrix(mLastContentPaintMetrics.mResolution.scale,
                                    mLastContentPaintMetrics.mResolution.scale,
                                    1.0f);
}

gfx3DMatrix AsyncPanZoomController::GetTransformToLastDispatchedPaint() {
  ReentrantMonitorAutoEnter lock(mMonitor);
  LayerPoint scrollChange = (mLastContentPaintMetrics.GetScrollOffset() - mLastDispatchedPaintMetrics.GetScrollOffset())
                          * mLastContentPaintMetrics.LayersPixelsPerCSSPixel();
  float zoomChange = mLastContentPaintMetrics.GetZoom().scale / mLastDispatchedPaintMetrics.GetZoom().scale;
  return gfx3DMatrix::Translation(scrollChange.x, scrollChange.y, 0) *
         gfx3DMatrix::ScalingMatrix(zoomChange, zoomChange, 1);
}

void AsyncPanZoomController::NotifyLayersUpdated(const FrameMetrics& aLayerMetrics, bool aIsFirstPaint) {
  ReentrantMonitorAutoEnter lock(mMonitor);
  bool isDefault = mFrameMetrics.IsDefault();

  mLastContentPaintMetrics = aLayerMetrics;
  UpdateTransformScale();

  mFrameMetrics.mMayHaveTouchListeners = aLayerMetrics.mMayHaveTouchListeners;
  mFrameMetrics.mMayHaveTouchCaret = aLayerMetrics.mMayHaveTouchCaret;
  APZC_LOG_FM(aLayerMetrics, "%p got a NotifyLayersUpdated with aIsFirstPaint=%d", this, aIsFirstPaint);

  LogRendertraceRect(GetGuid(), "page", "brown", aLayerMetrics.mScrollableRect);
  LogRendertraceRect(GetGuid(), "painted displayport", "lightgreen",
    aLayerMetrics.mDisplayPort + aLayerMetrics.GetScrollOffset());
  if (!aLayerMetrics.mCriticalDisplayPort.IsEmpty()) {
    LogRendertraceRect(GetGuid(), "painted critical displayport", "darkgreen",
      aLayerMetrics.mCriticalDisplayPort + aLayerMetrics.GetScrollOffset());
  }

  mPaintThrottler.TaskComplete(GetFrameTime());
  bool needContentRepaint = false;
  if (FuzzyEqualsAdditive(aLayerMetrics.mCompositionBounds.width, mFrameMetrics.mCompositionBounds.width) &&
      FuzzyEqualsAdditive(aLayerMetrics.mCompositionBounds.height, mFrameMetrics.mCompositionBounds.height)) {
    // Remote content has sync'd up to the composition geometry
    // change, so we can accept the viewport it's calculated.
    if (mFrameMetrics.mViewport.width != aLayerMetrics.mViewport.width ||
        mFrameMetrics.mViewport.height != aLayerMetrics.mViewport.height) {
      needContentRepaint = true;
    }
    mFrameMetrics.mViewport = aLayerMetrics.mViewport;
  }

  // If the layers update was not triggered by our own repaint request, then
  // we want to take the new scroll offset. Check the scroll generation as well
  // to filter duplicate calls to NotifyLayersUpdated with the same scroll offset
  // update message.
  bool scrollOffsetUpdated = aLayerMetrics.GetScrollOffsetUpdated()
        && (aLayerMetrics.GetScrollGeneration() != mFrameMetrics.GetScrollGeneration());

  if (aIsFirstPaint || isDefault) {
    // Initialize our internal state to something sane when the content
    // that was just painted is something we knew nothing about previously
    mPaintThrottler.ClearHistory();
    mPaintThrottler.SetMaxDurations(gfxPrefs::APZNumPaintDurationSamples());

    mX.CancelTouch();
    mY.CancelTouch();
    SetState(NOTHING);

    mFrameMetrics = aLayerMetrics;
    mLastDispatchedPaintMetrics = aLayerMetrics;
    ShareCompositorFrameMetrics();

    if (mFrameMetrics.GetDisplayPortMargins() != LayerMargin()) {
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

    if (FuzzyEqualsAdditive(mFrameMetrics.mCompositionBounds.width, aLayerMetrics.mCompositionBounds.width) &&
        mFrameMetrics.mDevPixelsPerCSSPixel == aLayerMetrics.mDevPixelsPerCSSPixel) {
      float parentResolutionChange = aLayerMetrics.GetParentResolution().scale
                                   / mFrameMetrics.GetParentResolution().scale;
      mFrameMetrics.ZoomBy(parentResolutionChange);
    } else {
      // Take the new zoom as either device scale or composition width or both
      // got changed (e.g. due to orientation change).
      mFrameMetrics.SetZoom(aLayerMetrics.GetZoom());
      mFrameMetrics.mDevPixelsPerCSSPixel.scale = aLayerMetrics.mDevPixelsPerCSSPixel.scale;
    }
    if (!mFrameMetrics.mScrollableRect.IsEqualEdges(aLayerMetrics.mScrollableRect)) {
      mFrameMetrics.mScrollableRect = aLayerMetrics.mScrollableRect;
      needContentRepaint = true;
    }
    mFrameMetrics.mCompositionBounds = aLayerMetrics.mCompositionBounds;
    mFrameMetrics.SetRootCompositionSize(aLayerMetrics.GetRootCompositionSize());
    mFrameMetrics.mResolution = aLayerMetrics.mResolution;
    mFrameMetrics.mCumulativeResolution = aLayerMetrics.mCumulativeResolution;
    mFrameMetrics.SetHasScrollgrab(aLayerMetrics.GetHasScrollgrab());

    if (scrollOffsetUpdated) {
      APZC_LOG("%p updating scroll offset from (%f, %f) to (%f, %f)\n", this,
        mFrameMetrics.GetScrollOffset().x, mFrameMetrics.GetScrollOffset().y,
        aLayerMetrics.GetScrollOffset().x, aLayerMetrics.GetScrollOffset().y);

      mFrameMetrics.CopyScrollInfoFrom(aLayerMetrics);

      // Cancel the animation (which will also trigger a repaint request)
      // after we update the scroll offset above. Otherwise we can be left
      // in a state where things are out of sync.
      CancelAnimation();

      // Because of the scroll offset update, any inflight paint requests are
      // going to be ignored by layout, and so mLastDispatchedPaintMetrics
      // becomes incorrect for the purposes of calculating the LD transform. To
      // correct this we need to update mLastDispatchedPaintMetrics to be the
      // last thing we know was painted by Gecko.
      mLastDispatchedPaintMetrics = aLayerMetrics;
    }
  }

  if (scrollOffsetUpdated) {
    // Once layout issues a scroll offset update, it becomes impervious to
    // scroll offset updates from APZ until we acknowledge the update it sent.
    // This prevents APZ updates from clobbering scroll updates from other
    // more "legitimate" sources like content scripts.
    nsRefPtr<GeckoContentController> controller = GetGeckoContentController();
    if (controller) {
      APZC_LOG("%p sending scroll update acknowledgement with gen %lu\n", this, aLayerMetrics.GetScrollGeneration());
      controller->AcknowledgeScrollUpdate(aLayerMetrics.GetScrollId(),
                                          aLayerMetrics.GetScrollGeneration());
    }
  }

  if (needContentRepaint) {
    RequestContentRepaint();
  }
  UpdateSharedCompositorFrameMetrics();
}

const FrameMetrics& AsyncPanZoomController::GetFrameMetrics() const {
  mMonitor.AssertCurrentThreadIn();
  return mFrameMetrics;
}

void AsyncPanZoomController::ZoomToRect(CSSRect aRect) {
  if (!aRect.IsFinite()) {
    NS_WARNING("ZoomToRect got called with a non-finite rect; ignoring...\n");
    return;
  }

  SetState(ANIMATING_ZOOM);

  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    ParentLayerRect compositionBounds = mFrameMetrics.mCompositionBounds;
    CSSRect cssPageRect = mFrameMetrics.mScrollableRect;
    CSSPoint scrollOffset = mFrameMetrics.GetScrollOffset();
    CSSToParentLayerScale currentZoom = mFrameMetrics.GetZoomToParent();
    CSSToParentLayerScale targetZoom;

    // The minimum zoom to prevent over-zoom-out.
    // If the zoom factor is lower than this (i.e. we are zoomed more into the page),
    // then the CSS content rect, in layers pixels, will be smaller than the
    // composition bounds. If this happens, we can't fill the target composited
    // area with this frame.
    CSSToParentLayerScale localMinZoom(std::max((mZoomConstraints.mMinZoom * mFrameMetrics.mTransformScale).scale,
                                       std::max(compositionBounds.width / cssPageRect.width,
                                                compositionBounds.height / cssPageRect.height)));
    CSSToParentLayerScale localMaxZoom = mZoomConstraints.mMaxZoom * mFrameMetrics.mTransformScale;

    if (!aRect.IsEmpty()) {
      // Intersect the zoom-to-rect to the CSS rect to make sure it fits.
      aRect = aRect.Intersect(cssPageRect);
      targetZoom = CSSToParentLayerScale(std::min(compositionBounds.width / aRect.width,
                                                  compositionBounds.height / aRect.height));
    }
    // 1. If the rect is empty, request received from browserElementScrolling.js
    // 2. currentZoom is equal to mZoomConstraints.mMaxZoom and user still double-tapping it
    // 3. currentZoom is equal to localMinZoom and user still double-tapping it
    // Treat these three cases as a request to zoom out as much as possible.
    if (aRect.IsEmpty() ||
        (currentZoom == localMaxZoom && targetZoom >= localMaxZoom) ||
        (currentZoom == localMinZoom && targetZoom <= localMinZoom)) {
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
    endZoomToMetrics.SetZoom(targetZoom / mFrameMetrics.mTransformScale);

    // Adjust the zoomToRect to a sensible position to prevent overscrolling.
    CSSSize sizeAfterZoom = endZoomToMetrics.CalculateCompositedSizeInCssPixels();

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
    endZoomToMetrics.SetDisplayPortMargins(
      CalculatePendingDisplayPort(endZoomToMetrics,
                                  ScreenPoint(0,0),
                                  0));
    endZoomToMetrics.SetUseDisplayPortMargins();

    StartAnimation(new ZoomAnimation(
        mFrameMetrics.GetScrollOffset(),
        mFrameMetrics.GetZoom(),
        endZoomToMetrics.GetScrollOffset(),
        endZoomToMetrics.GetZoom()));

    // Schedule a repaint now, so the new displayport will be painted before the
    // animation finishes.
    RequestContentRepaint(endZoomToMetrics);
  }
}

void
AsyncPanZoomController::ScheduleContentResponseTimeout() {
  APZC_LOG("%p scheduling content response timeout\n", this);
  PostDelayedTask(
    NewRunnableMethod(this, &AsyncPanZoomController::ContentResponseTimeout),
    gfxPrefs::APZContentResponseTimeout());
}

void
AsyncPanZoomController::ContentResponseTimeout() {
  AssertOnControllerThread();

  mTouchBlockBalance++;
  APZC_LOG("%p got a content response timeout; balance %d\n", this, mTouchBlockBalance);
  if (mTouchBlockBalance > 0) {
    // Find the first touch block in the queue that hasn't already received
    // the content response timeout callback, and notify it.
    bool found = false;
    for (size_t i = 0; i < mTouchBlockQueue.Length(); i++) {
      if (mTouchBlockQueue[i]->TimeoutContentResponse()) {
        found = true;
        break;
      }
    }
    if (found) {
      ProcessPendingInputBlocks();
    } else {
      NS_WARNING("APZC received more ContentResponseTimeout calls than it has unprocessed touch blocks\n");
    }
  }
}

void
AsyncPanZoomController::ContentReceivedTouch(bool aPreventDefault) {
  AssertOnControllerThread();

  mTouchBlockBalance--;
  APZC_LOG("%p got a content response; balance %d\n", this, mTouchBlockBalance);
  if (mTouchBlockBalance < 0) {
    // Find the first touch block in the queue that hasn't already received
    // its response from content, and notify it.
    bool found = false;
    for (size_t i = 0; i < mTouchBlockQueue.Length(); i++) {
      if (mTouchBlockQueue[i]->SetContentResponse(aPreventDefault)) {
        found = true;
        break;
      }
    }
    if (found) {
      ProcessPendingInputBlocks();
    } else {
      NS_WARNING("APZC received more ContentReceivedTouch calls than it has unprocessed touch blocks\n");
    }
  }
}

void
AsyncPanZoomController::SetAllowedTouchBehavior(const nsTArray<TouchBehaviorFlags>& aBehaviors) {
  AssertOnControllerThread();

  bool found = false;
  for (size_t i = 0; i < mTouchBlockQueue.Length(); i++) {
    if (mTouchBlockQueue[i]->SetAllowedTouchBehaviors(aBehaviors)) {
      found = true;
      break;
    }
  }
  if (found) {
    ProcessPendingInputBlocks();
  } else {
    NS_WARNING("APZC received more SetAllowedTouchBehavior calls than it has unprocessed touch blocks\n");
  }
}

void
AsyncPanZoomController::ProcessPendingInputBlocks() {
  AssertOnControllerThread();

  while (true) {
    TouchBlockState* curBlock = CurrentTouchBlock();
    if (!curBlock->IsReadyForHandling()) {
      break;
    }

    APZC_LOG("%p processing input block %p; preventDefault %d\n",
        this, curBlock, curBlock->IsDefaultPrevented());
    if (curBlock->IsDefaultPrevented()) {
      SetState(NOTHING);
      curBlock->DropEvents();
    } else {
      while (curBlock->HasEvents()) {
        HandleInputEvent(curBlock->RemoveFirstEvent());
      }
    }
    MOZ_ASSERT(!curBlock->HasEvents());

    if (mTouchBlockQueue.Length() == 1) {
      // If |curBlock| is the only touch block in the queue, then it is still
      // active and we cannot remove it yet. We only know that a touch block is
      // over when we start the next one. This block will be removed by the code
      // in StartNewTouchBlock, where new touch blocks are added.
      break;
    }

    // If we get here, we know there are more touch blocks in the queue after
    // |curBlock|, so we can remove |curBlock| and try to process the next one.
    APZC_LOG("%p discarding depleted touch block %p\n", this, curBlock);
    mTouchBlockQueue.RemoveElementAt(0);
  }
}

TouchBlockState*
AsyncPanZoomController::StartNewTouchBlock(bool aCopyAllowedTouchBehaviorFromCurrent)
{
  TouchBlockState* newBlock = new TouchBlockState();
  if (gfxPrefs::TouchActionEnabled() && aCopyAllowedTouchBehaviorFromCurrent) {
    newBlock->CopyAllowedTouchBehaviorsFrom(*CurrentTouchBlock());
  }

  // We're going to start a new block, so clear out any depleted blocks at the head of the queue.
  // See corresponding comment in ProcessPendingInputBlocks.
  while (!mTouchBlockQueue.IsEmpty()) {
    if (mTouchBlockQueue[0]->IsReadyForHandling() && !mTouchBlockQueue[0]->HasEvents()) {
      APZC_LOG("%p discarding depleted touch block %p\n", this, mTouchBlockQueue[0].get());
      mTouchBlockQueue.RemoveElementAt(0);
    } else {
      break;
    }
  }

  // Add the new block to the queue.
  mTouchBlockQueue.AppendElement(newBlock);
  return newBlock;
}

TouchBlockState*
AsyncPanZoomController::CurrentTouchBlock()
{
  AssertOnControllerThread();

  MOZ_ASSERT(!mTouchBlockQueue.IsEmpty());
  return mTouchBlockQueue[0].get();
}

bool
AsyncPanZoomController::HasReadyTouchBlock()
{
  return !mTouchBlockQueue.IsEmpty() && mTouchBlockQueue[0]->IsReadyForHandling();
}

AsyncPanZoomController::TouchBehaviorFlags
AsyncPanZoomController::GetAllowedTouchBehavior(ScreenIntPoint& aPoint) {
  // Here we need to perform a hit testing over the touch-action regions attached to the
  // layer associated with current apzc.
  // Currently they are in progress, for more info see bug 928833.
  return AllowedTouchBehavior::UNKNOWN;
}

void AsyncPanZoomController::SetState(PanZoomState aNewState) {

  PanZoomState oldState;

  // Intentional scoping for mutex
  {
    ReentrantMonitorAutoEnter lock(mMonitor);
    oldState = mState;
    mState = aNewState;
  }

  if (nsRefPtr<GeckoContentController> controller = GetGeckoContentController()) {
    if (!IsTransformingState(oldState) && IsTransformingState(aNewState)) {
      controller->NotifyAPZStateChange(
          GetGuid(), APZStateChange::TransformBegin);
    } else if (IsTransformingState(oldState) && !IsTransformingState(aNewState)) {
      controller->NotifyAPZStateChange(
          GetGuid(), APZStateChange::TransformEnd);
    }
  }
}

bool AsyncPanZoomController::IsTransformingState(PanZoomState aState) {
  return !(aState == NOTHING || aState == TOUCHING);
}

bool AsyncPanZoomController::IsPanningState(PanZoomState aState) {
  return (aState == PANNING || aState == PANNING_LOCKED_X || aState == PANNING_LOCKED_Y);
}

void AsyncPanZoomController::UpdateZoomConstraints(const ZoomConstraints& aConstraints) {
  APZC_LOG("%p updating zoom constraints to %d %d %f %f\n", this, aConstraints.mAllowZoom,
    aConstraints.mAllowDoubleTapZoom, aConstraints.mMinZoom.scale, aConstraints.mMaxZoom.scale);
  if (IsNaN(aConstraints.mMinZoom.scale) || IsNaN(aConstraints.mMaxZoom.scale)) {
    NS_WARNING("APZC received zoom constraints with NaN values; dropping...\n");
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

  bool isRoot;
  CSSRect contentRect;
  CSSSize scrollableSize;
  {
    ReentrantMonitorAutoEnter lock(mMonitor);

    isRoot = mFrameMetrics.mIsRoot;
    scrollableSize = mFrameMetrics.mScrollableRect.Size();
    contentRect = mFrameMetrics.CalculateCompositedRectInCssPixels();
    contentRect.MoveTo(mCurrentAsyncScrollOffset);
  }

  controller->SendAsyncScrollDOMEvent(isRoot, contentRect, scrollableSize);
}

bool AsyncPanZoomController::Matches(const ScrollableLayerGuid& aGuid)
{
  return aGuid == GetGuid();
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

  if (frame && mSharedLock && gfxPrefs::UseProgressiveTilePainting()) {
    mSharedLock->Lock();
    *frame = mFrameMetrics;
    mSharedLock->Unlock();
  }
}

void AsyncPanZoomController::ShareCompositorFrameMetrics() {

  PCompositorParent* compositor = GetSharedFrameMetricsCompositor();

  // Only create the shared memory buffer if it hasn't already been created,
  // we are using progressive tile painting, and we have a
  // compositor to pass the shared memory back to the content process/thread.
  if (!mSharedFrameMetricsBuffer && compositor && gfxPrefs::UseProgressiveTilePainting()) {

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
      base::ProcessHandle processHandle = compositor->OtherProcess();
      ipc::SharedMemoryBasic::Handle mem = ipc::SharedMemoryBasic::NULLHandle();

      // Get the shared memory handle to share with the content process
      mSharedFrameMetricsBuffer->ShareToProcess(processHandle, &mem);

      // Get the cross process mutex handle to share with the content process
      mSharedLock = new CrossProcessMutex("AsyncPanZoomControlLock");
      CrossProcessMutexHandle handle = mSharedLock->ShareToProcess(processHandle);

      // Send the shared memory handle and cross process handle to the content
      // process by an asynchronous ipc call. Include the APZC unique ID
      // so the content process know which APZC sent this shared FrameMetrics.
      if (!compositor->SendSharedCompositorFrameMetrics(mem, handle, mAPZCId)) {
        APZC_LOG("%p failed to share FrameMetrics with content process.", this);
      }
    }
  }
}

ParentLayerPoint AsyncPanZoomController::ToParentLayerCoords(const ScreenPoint& aPoint)
{
  return TransformTo<ParentLayerPixel>(GetNontransientAsyncTransform() * GetCSSTransform(), aPoint);
}

void AsyncPanZoomController::UpdateTransformScale()
{
  gfx3DMatrix nontransientTransforms = GetNontransientAsyncTransform() * GetCSSTransform();
  if (!FuzzyEqualsMultiplicative(nontransientTransforms.GetXScale(), nontransientTransforms.GetYScale())) {
    NS_WARNING("The x- and y-scales of the nontransient transforms should be equal");
  }
  mFrameMetrics.mTransformScale.scale = nontransientTransforms.GetXScale();
}

}
}
