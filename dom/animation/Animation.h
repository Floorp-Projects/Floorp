/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Animation_h
#define mozilla_dom_Animation_h

#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h" // for TimeStamp, TimeDuration
#include "mozilla/dom/AnimationBinding.h" // for AnimationPlayState
#include "mozilla/dom/DocumentTimeline.h" // for DocumentTimeline
#include "mozilla/dom/KeyframeEffect.h" // for KeyframeEffectReadonly
#include "mozilla/dom/Promise.h" // for Promise
#include "nsCSSProperty.h" // for nsCSSProperty

// X11 has a #define for CurrentTime.
#ifdef CurrentTime
#undef CurrentTime
#endif

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount().
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

struct JSContext;
class nsCSSPropertySet;
class nsIDocument;
class nsPresContext;

namespace mozilla {
struct AnimationPlayerCollection;
namespace css {
class AnimValuesStyleRule;
class CommonAnimationManager;
} // namespace css

class CSSAnimationPlayer;
class CSSTransitionPlayer;

namespace dom {

class Animation
  : public nsISupports
  , public nsWrapperCache
{
protected:
  virtual ~Animation() {}

public:
  explicit Animation(DocumentTimeline* aTimeline)
    : mTimeline(aTimeline)
    , mPlaybackRate(1.0)
    , mPendingState(PendingState::NotPending)
    , mIsRunningOnCompositor(false)
    , mIsPreviousStateFinished(false)
    , mFinishedAtLastComposeStyle(false)
    , mIsRelevant(false)
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(Animation)

  DocumentTimeline* GetParentObject() const { return mTimeline; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  virtual CSSAnimationPlayer* AsCSSAnimationPlayer() { return nullptr; }
  virtual CSSTransitionPlayer* AsCSSTransitionPlayer() { return nullptr; }

  // Flag to pass to DoPlay to indicate that it should not carry out finishing
  // behavior (reset the current time to the beginning of the active duration).
  enum LimitBehavior {
    AutoRewind = 0,
    Continue = 1
  };

  // Animation methods
  KeyframeEffectReadonly* GetEffect() const { return mEffect; }
  DocumentTimeline* Timeline() const { return mTimeline; }
  Nullable<TimeDuration> GetStartTime() const { return mStartTime; }
  void SetStartTime(const Nullable<TimeDuration>& aNewStartTime);
  Nullable<TimeDuration> GetCurrentTime() const;
  void SilentlySetCurrentTime(const TimeDuration& aNewCurrentTime);
  void SetCurrentTime(const TimeDuration& aNewCurrentTime);
  double PlaybackRate() const { return mPlaybackRate; }
  void SetPlaybackRate(double aPlaybackRate);
  void SilentlySetPlaybackRate(double aPlaybackRate);
  AnimationPlayState PlayState() const;
  virtual Promise* GetReady(ErrorResult& aRv);
  virtual Promise* GetFinished(ErrorResult& aRv);
  virtual void Play(LimitBehavior aLimitBehavior);
  virtual void Pause();
  bool IsRunningOnCompositor() const { return mIsRunningOnCompositor; }

  // Wrapper functions for Animation DOM methods when called
  // from script. We often use the same methods internally and from
  // script but when called from script we (or one of our subclasses) perform
  // extra steps such as flushing style or converting the return type.
  Nullable<double> GetStartTimeAsDouble() const;
  void SetStartTimeAsDouble(const Nullable<double>& aStartTime);
  Nullable<double> GetCurrentTimeAsDouble() const;
  void SetCurrentTimeAsDouble(const Nullable<double>& aCurrentTime,
                              ErrorResult& aRv);
  virtual AnimationPlayState PlayStateFromJS() const { return PlayState(); }
  virtual void PlayFromJS() { Play(LimitBehavior::AutoRewind); }
  // PauseFromJS is currently only here for symmetry with PlayFromJS but
  // in future we will likely have to flush style in
  // CSSAnimationPlayer::PauseFromJS so we leave it for now.
  void PauseFromJS() { Pause(); }

  void SetEffect(KeyframeEffectReadonly* aEffect);
  void Tick();

  /**
   * Set the time to use for starting or pausing a pending animation.
   *
   * Typically, when an animation is played, it does not start immediately but
   * is added to a table of pending animations on the document of its effect.
   * In the meantime it sets its hold time to the time from which playback
   * should begin.
   *
   * When the document finishes painting, any pending animations in its table
   * are marked as being ready to start by calling StartOnNextTick.
   * The moment when the paint completed is also recorded, converted to a
   * timeline time, and passed to StartOnTick. This is so that when these
   * animations do start, they can be timed from the point when painting
   * completed.
   *
   * After calling TriggerOnNextTick, animations remain in the pending state
   * until the next refresh driver tick. At that time they transition out of
   * the pending state using the time passed to TriggerOnNextTick as the
   * effective time at which they resumed.
   *
   * This approach means that any setup time required for performing the
   * initial paint of an animation such as layerization is not deducted from
   * the running time of the animation. Without this we can easily drop the
   * first few frames of an animation, or, on slower devices, the whole
   * animation.
   *
   * Furthermore:
   *
   * - Starting the animation immediately when painting finishes is problematic
   *   because the start time of the animation will be ahead of its timeline
   *   (since the timeline time is based on the refresh driver time).
   *   That's a problem because the animation is playing but its timing
   *   suggests it starts in the future. We could update the timeline to match
   *   the start time of the animation but then we'd also have to update the
   *   timing and style of all animations connected to that timeline or else be
   *   stuck in an inconsistent state until the next refresh driver tick.
   *
   * - If we simply use the refresh driver time on its next tick, the lag
   *   between triggering an animation and its effective start is unacceptably
   *   long.
   *
   * For pausing, we apply the same asynchronous approach. This is so that we
   * synchronize with animations that are running on the compositor. Otherwise
   * if the main thread lags behind the compositor there will be a noticeable
   * jump backwards when the main thread takes over. Even though main thread
   * animations could be paused immediately, we do it asynchronously for
   * consistency and so that animations paused together end up in step.
   *
   * Note that the caller of this method is responsible for removing the
   * animation from any PendingAnimationTracker it may have been added to.
   */
  void TriggerOnNextTick(const Nullable<TimeDuration>& aReadyTime);

  // Testing only: Start or pause a pending animation using the current timeline
  // time. This is used to support existing tests that expect animations to
  // begin immediately. Ideally we would rewrite the those tests and get rid of
  // this method, but there are a lot of them.
  //
  // As with TriggerOnNextTick, the caller of this method is responsible for
  // removing the animation from any PendingAnimationTracker it may have been
  // added to.
  void TriggerNow();

  /**
   * When StartOnNextTick is called, we store the ready time but we don't apply
   * it until the next tick. In the meantime, GetStartTime() will return null.
   *
   * However, if we build layer animations again before the next tick, we
   * should initialize them with the start time that GetStartTime() will return
   * on the next tick.
   *
   * If we were to simply set the start time of layer animations to null, their
   * start time would be updated to the current wallclock time when rendering
   * finishes, thus making them out of sync with the start time stored here.
   * This, in turn, will make the animation jump backwards when we build
   * animations on the next tick and apply the start time stored here.
   *
   * This method returns the start time, if resolved. Otherwise, if we have
   * a pending ready time, it returns the corresponding start time. If neither
   * of those are available, it returns null.
   */
  Nullable<TimeDuration> GetCurrentOrPendingStartTime() const;

  void Cancel();

  const nsString& Name() const
  {
    return mEffect ? mEffect->Name() : EmptyString();
  }

  bool IsPausedOrPausing() const
  {
    return PlayState() == AnimationPlayState::Paused ||
           mPendingState == PendingState::PausePending;
  }

  bool HasInPlayEffect() const
  {
    return GetEffect() && GetEffect()->IsInPlay(*this);
  }
  bool HasCurrentEffect() const
  {
    return GetEffect() && GetEffect()->IsCurrent(*this);
  }
  bool IsInEffect() const
  {
    return GetEffect() && GetEffect()->IsInEffect();
  }

  /**
   * "Playing" is different to "running". An animation in its delay phase is
   * still running but we only consider it playing when it is in its active
   * interval. This definition is used for fetching the animations that are
   * are candidates for running on the compositor (since we don't ship
   * animations to the compositor when they are in their delay phase or
   * paused).
   */
  bool IsPlaying() const
  {
    // We need to have an effect in its active interval, and
    // be either running or waiting to run.
    return HasInPlayEffect() &&
           (PlayState() == AnimationPlayState::Running ||
            mPendingState == PendingState::PlayPending);
  }

  bool IsRelevant() const { return mIsRelevant; }
  void UpdateRelevance();

  void SetIsRunningOnCompositor() { mIsRunningOnCompositor = true; }
  void ClearIsRunningOnCompositor() { mIsRunningOnCompositor = false; }

  // Returns true if this animation does not currently need to update
  // style on the main thread (e.g. because it is empty, or is
  // running on the compositor).
  bool CanThrottle() const;

  // Updates |aStyleRule| with the animation values of this animation's effect,
  // if any.
  // Any properties already contained in |aSetProperties| are not changed. Any
  // properties that are changed are added to |aSetProperties|.
  // |aNeedsRefreshes| will be set to true if this animation expects to update
  // the style rule on the next refresh driver tick as well (because it
  // is running and has an effect to sample).
  void ComposeStyle(nsRefPtr<css::AnimValuesStyleRule>& aStyleRule,
                    nsCSSPropertySet& aSetProperties,
                    bool& aNeedsRefreshes);

protected:
  void DoPlay(LimitBehavior aLimitBehavior);
  void DoPause();
  void ResumeAt(const TimeDuration& aReadyTime);
  void PauseAt(const TimeDuration& aReadyTime);
  void FinishPendingAt(const TimeDuration& aReadyTime)
  {
    if (mPendingState == PendingState::PlayPending) {
      ResumeAt(aReadyTime);
    } else if (mPendingState == PendingState::PausePending) {
      PauseAt(aReadyTime);
    } else {
      NS_NOTREACHED("Can't finish pending if we're not in a pending state");
    }
  }

  void UpdateTiming();
  void UpdateFinishedState(bool aSeekFlag = false);
  void UpdateEffect();
  void FlushStyle() const;
  void PostUpdate();
  /**
   * Remove this animation from the pending animation tracker and reset
   * mPendingState as necessary. The caller is responsible for resolving or
   * aborting the mReady promise as necessary.
   */
  void CancelPendingTasks();

  bool IsFinished() const;

  bool IsPossiblyOrphanedPendingAnimation() const;
  StickyTimeDuration EffectEnd() const;

  nsIDocument* GetRenderedDocument() const;
  nsPresContext* GetPresContext() const;
  virtual css::CommonAnimationManager* GetAnimationManager() const = 0;
  AnimationPlayerCollection* GetCollection() const;

  nsRefPtr<DocumentTimeline> mTimeline;
  nsRefPtr<KeyframeEffectReadonly> mEffect;
  // The beginning of the delay period.
  Nullable<TimeDuration> mStartTime; // Timeline timescale
  Nullable<TimeDuration> mHoldTime;  // Animation timescale
  Nullable<TimeDuration> mPendingReadyTime; // Timeline timescale
  Nullable<TimeDuration> mPreviousCurrentTime; // Animation timescale
  double mPlaybackRate;

  // A Promise that is replaced on each call to Play() (and in future Pause())
  // and fulfilled when Play() is successfully completed.
  // This object is lazily created by GetReady.
  // See http://w3c.github.io/web-animations/#current-ready-promise
  nsRefPtr<Promise> mReady;

  // A Promise that is resolved when we reach the end of the effect, or
  // 0 when playing backwards. The Promise is replaced if the animation is
  // finished but then a state change makes it not finished.
  // This object is lazily created by GetFinished.
  // See http://w3c.github.io/web-animations/#current-finished-promise
  nsRefPtr<Promise> mFinished;

  // Indicates if the animation is in the pending state (and what state it is
  // waiting to enter when it finished pending). We use this rather than
  // checking if this animation is tracked by a PendingAnimationTracker because
  // the animation will continue to be pending even after it has been removed
  // from the PendingAnimationTracker while it is waiting for the next tick
  // (see TriggerOnNextTick for details).
  enum class PendingState { NotPending, PlayPending, PausePending };
  PendingState mPendingState;

  bool mIsRunningOnCompositor;
  // Indicates whether we were in the finished state during our
  // most recent unthrottled sample (our last ComposeStyle call).
  bool mIsPreviousStateFinished; // Spec calls this "previous finished state"
  bool mFinishedAtLastComposeStyle;
  // Indicates that the animation should be exposed in an element's
  // getAnimations() list.
  bool mIsRelevant;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Animation_h
