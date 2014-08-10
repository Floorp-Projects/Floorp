/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationPlayer_h
#define mozilla_dom_AnimationPlayer_h

#include <algorithm> // for std::max
#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "mozilla/StyleAnimationValue.h" // for StyleAnimationValue
#include "mozilla/TimeStamp.h" // for TimeStamp, TimeDuration
#include "mozilla/dom/Animation.h" // for Animation
#include "mozilla/dom/AnimationTimeline.h" // for AnimationTimeline
#include "nsCSSProperty.h" // for nsCSSProperty
#include "nsSMILKeySpline.h" // for nsSMILKeySpline
#include "nsStyleStruct.h" // for nsTimingFunction

// X11 has a #define for CurrentTime.
#ifdef CurrentTime
#undef CurrentTime
#endif

struct JSContext;

namespace mozilla {

struct ElementPropertyTransition;

class ComputedTimingFunction {
public:
  typedef nsTimingFunction::Type Type;
  void Init(const nsTimingFunction &aFunction);
  double GetValue(double aPortion) const;
  const nsSMILKeySpline* GetFunction() const {
    NS_ASSERTION(mType == nsTimingFunction::Function, "Type mismatch");
    return &mTimingFunction;
  }
  Type GetType() const { return mType; }
  uint32_t GetSteps() const { return mSteps; }
private:
  Type mType;
  nsSMILKeySpline mTimingFunction;
  uint32_t mSteps;
};

struct AnimationPropertySegment
{
  float mFromKey, mToKey;
  StyleAnimationValue mFromValue, mToValue;
  ComputedTimingFunction mTimingFunction;
};

struct AnimationProperty
{
  nsCSSProperty mProperty;
  InfallibleTArray<AnimationPropertySegment> mSegments;
};

/**
 * Input timing parameters.
 *
 * Eventually this will represent all the input timing parameters specified
 * by content but for now it encapsulates just the subset of those
 * parameters passed to GetPositionInIteration.
 */
struct AnimationTiming
{
  TimeDuration mIterationDuration;
  TimeDuration mDelay;
  float mIterationCount; // mozilla::PositiveInfinity<float>() means infinite
  uint8_t mDirection;
  uint8_t mFillMode;

  bool FillsForwards() const {
    return mFillMode == NS_STYLE_ANIMATION_FILL_MODE_BOTH ||
           mFillMode == NS_STYLE_ANIMATION_FILL_MODE_FORWARDS;
  }
  bool FillsBackwards() const {
    return mFillMode == NS_STYLE_ANIMATION_FILL_MODE_BOTH ||
           mFillMode == NS_STYLE_ANIMATION_FILL_MODE_BACKWARDS;
  }
};

/**
 * Stores the results of calculating the timing properties of an animation
 * at a given sample time.
 */
struct ComputedTiming
{
  ComputedTiming()
  : mTimeFraction(kNullTimeFraction)
  , mCurrentIteration(0)
  , mPhase(AnimationPhase_Null)
  { }

  static const double kNullTimeFraction;

  // The total duration of the animation including all iterations.
  // Will equal TimeDuration::Forever() if the animation repeats indefinitely.
  TimeDuration mActiveDuration;

  // Will be kNullTimeFraction if the animation is neither animating nor
  // filling at the sampled time.
  double mTimeFraction;

  // Zero-based iteration index (meaningless if mTimeFraction is
  // kNullTimeFraction).
  uint64_t mCurrentIteration;

  enum {
    // Not sampled (null sample time)
    AnimationPhase_Null,
    // Sampled prior to the start of the active interval
    AnimationPhase_Before,
    // Sampled within the active interval
    AnimationPhase_Active,
    // Sampled after (or at) the end of the active interval
    AnimationPhase_After
  } mPhase;
};

namespace dom {

class AnimationPlayer : public nsWrapperCache
{
protected:
  virtual ~AnimationPlayer() { }

public:
  explicit AnimationPlayer(AnimationTimeline* aTimeline)
    : mIsRunningOnCompositor(false)
    , mIsFinishedTransition(false)
    , mLastNotification(LAST_NOTIFICATION_NONE)
    , mTimeline(aTimeline)
  {
    SetIsDOMBinding();
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnimationPlayer)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AnimationPlayer)

  AnimationTimeline* GetParentObject() const { return mTimeline; }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // AnimationPlayer methods
  Animation* GetSource() const { return nullptr; }
  AnimationTimeline* Timeline() const { return mTimeline; }
  double StartTime() const;
  double CurrentTime() const;

  // FIXME: If we succeed in moving transition-specific code to a type of
  // AnimationEffect (as per the Web Animations API) we should remove these
  // virtual methods.
  virtual ElementPropertyTransition* AsTransition() { return nullptr; }
  virtual const ElementPropertyTransition* AsTransition() const {
    return nullptr;
  }

  bool IsPaused() const {
    return mPlayState == NS_STYLE_ANIMATION_PLAY_STATE_PAUSED;
  }

  // After transitions finish they need to be retained for one throttle-able
  // cycle (for reasons see explanation in nsTransitionManager.cpp). In the
  // meantime, however, they should be ignored.
  bool IsFinishedTransition() const {
    return mIsFinishedTransition;
  }
  void SetFinishedTransition() {
    MOZ_ASSERT(AsTransition(),
               "Calling SetFinishedTransition but it's not a transition");
    mIsFinishedTransition = true;
  }

  bool HasAnimationOfProperty(nsCSSProperty aProperty) const;
  bool IsRunning() const;
  bool IsCurrent() const;

  // Return the duration since the start of the delay period, taking into
  // account the pause state.  May be negative.
  // Returns a null value if the timeline associated with this object has a
  // current timestamp that is null or if the start time of this object is
  // null.
  Nullable<TimeDuration> GetLocalTime() const {
    const TimeStamp& timelineTime = mTimeline->GetCurrentTimeStamp();
    // FIXME: In order to support arbitrary timelines we will need to fix
    // the pause logic to handle the timeline time going backwards.
    MOZ_ASSERT(timelineTime.IsNull() || !IsPaused() ||
               timelineTime >= mPauseStart,
               "if paused, any non-null value of aTime must be at least"
               " mPauseStart");

    Nullable<TimeDuration> result; // Initializes to null
    if (!timelineTime.IsNull() && !mStartTime.IsNull()) {
      result.SetValue((IsPaused() ? mPauseStart : timelineTime) - mStartTime);
    }
    return result;
  }

  // Return the duration from the start the active interval to the point where
  // the animation begins playback. This is zero unless the animation has
  // a negative delay in which case it is the absolute value of the delay.
  // This is used for setting the elapsedTime member of AnimationEvents.
  TimeDuration InitialAdvance() const {
    return std::max(TimeDuration(), mTiming.mDelay * -1);
  }

  // This function takes as input the timing parameters of an animation and
  // returns the computed timing at the specified local time.
  //
  // The local time may be null in which case only static parameters such as the
  // active duration are calculated. All other members of the returned object
  // are given a null/initial value.
  //
  // This function returns ComputedTiming::kNullTimeFraction for the
  // mTimeFraction member of the return value if the animation should not be
  // run (because it is not currently active and is not filling at this time).
  static ComputedTiming
  GetComputedTimingAt(const Nullable<TimeDuration>& aLocalTime,
                      const AnimationTiming& aTiming);

  // Shortcut for that gets the computed timing using the current local time as
  // calculated from the timeline time.
  ComputedTiming GetComputedTiming(const AnimationTiming& aTiming) const {
    return GetComputedTimingAt(GetLocalTime(), aTiming);
  }

  // Return the duration of the active interval for the given timing parameters.
  static TimeDuration ActiveDuration(const AnimationTiming& aTiming);

  nsString mName;
  AnimationTiming mTiming;
  // The beginning of the delay period.
  TimeStamp mStartTime;
  TimeStamp mPauseStart;
  uint8_t mPlayState;
  bool mIsRunningOnCompositor;
  // A flag to mark transitions that have finished and are due to
  // be removed on the next throttle-able cycle.
  bool mIsFinishedTransition;

  enum {
    LAST_NOTIFICATION_NONE = uint64_t(-1),
    LAST_NOTIFICATION_END = uint64_t(-2)
  };
  // One of the above constants, or an integer for the iteration
  // whose start we last notified on.
  uint64_t mLastNotification;

  InfallibleTArray<AnimationProperty> mProperties;

  nsRefPtr<AnimationTimeline> mTimeline;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationPlayer_h
