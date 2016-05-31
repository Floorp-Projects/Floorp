/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Animation_h
#define mozilla_dom_Animation_h

#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "mozilla/EffectCompositor.h" // For EffectCompositor::CascadeLevel
#include "mozilla/LinkedList.h"
#include "mozilla/TimeStamp.h" // for TimeStamp, TimeDuration
#include "mozilla/dom/AnimationBinding.h" // for AnimationPlayState
#include "mozilla/dom/AnimationTimeline.h" // for AnimationTimeline
#include "mozilla/DOMEventTargetHelper.h" // for DOMEventTargetHelper
#include "mozilla/dom/KeyframeEffect.h" // for KeyframeEffectReadOnly
#include "mozilla/dom/Promise.h" // for Promise
#include "nsCSSProperty.h" // for nsCSSProperty
#include "nsIGlobalObject.h"

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

class AnimValuesStyleRule;

namespace dom {

class CSSAnimation;
class CSSTransition;

class Animation
  : public DOMEventTargetHelper
  , public LinkedListElement<Animation>
{
protected:
  virtual ~Animation() {}

public:
  explicit Animation(nsIGlobalObject* aGlobal)
    : DOMEventTargetHelper(aGlobal)
    , mPlaybackRate(1.0)
    , mPendingState(PendingState::NotPending)
    , mAnimationIndex(sNextAnimationIndex++)
    , mFinishedAtLastComposeStyle(false)
    , mIsRelevant(false)
    , mFinishedIsResolved(false)
  {
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Animation,
                                           DOMEventTargetHelper)

  AnimationTimeline* GetParentObject() const { return mTimeline; }
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  virtual CSSAnimation* AsCSSAnimation() { return nullptr; }
  virtual const CSSAnimation* AsCSSAnimation() const { return nullptr; }
  virtual CSSTransition* AsCSSTransition() { return nullptr; }
  virtual const CSSTransition* AsCSSTransition() const { return nullptr; }

  /**
   * Flag to pass to Play to indicate whether or not it should automatically
   * rewind the current time to the start point if the animation is finished.
   * For regular calls to play() from script we should do this, but when a CSS
   * animation's animation-play-state changes we shouldn't rewind the animation.
   */
  enum class LimitBehavior {
    AutoRewind,
    Continue
  };

  // Animation interface methods
  static already_AddRefed<Animation>
  Constructor(const GlobalObject& aGlobal,
              KeyframeEffectReadOnly* aEffect,
              const Optional<AnimationTimeline*>& aTimeline,
              ErrorResult& aRv);
  void GetId(nsAString& aResult) const { aResult = mId; }
  void SetId(const nsAString& aId);
  KeyframeEffectReadOnly* GetEffect() const { return mEffect; }
  void SetEffect(KeyframeEffectReadOnly* aEffect);
  AnimationTimeline* GetTimeline() const { return mTimeline; }
  void SetTimeline(AnimationTimeline* aTimeline);
  Nullable<TimeDuration> GetStartTime() const { return mStartTime; }
  void SetStartTime(const Nullable<TimeDuration>& aNewStartTime);
  Nullable<TimeDuration> GetCurrentTime() const;
  void SetCurrentTime(const TimeDuration& aNewCurrentTime);
  double PlaybackRate() const { return mPlaybackRate; }
  void SetPlaybackRate(double aPlaybackRate);
  AnimationPlayState PlayState() const;
  virtual Promise* GetReady(ErrorResult& aRv);
  virtual Promise* GetFinished(ErrorResult& aRv);
  void Cancel();
  virtual void Finish(ErrorResult& aRv);
  virtual void Play(ErrorResult& aRv, LimitBehavior aLimitBehavior);
  virtual void Pause(ErrorResult& aRv);
  virtual void Reverse(ErrorResult& aRv);
  bool IsRunningOnCompositor() const;
  IMPL_EVENT_HANDLER(finish);
  IMPL_EVENT_HANDLER(cancel);

  // Wrapper functions for Animation DOM methods when called
  // from script.
  //
  // We often use the same methods internally and from script but when called
  // from script we (or one of our subclasses) perform extra steps such as
  // flushing style or converting the return type.
  Nullable<double> GetStartTimeAsDouble() const;
  void SetStartTimeAsDouble(const Nullable<double>& aStartTime);
  Nullable<double> GetCurrentTimeAsDouble() const;
  void SetCurrentTimeAsDouble(const Nullable<double>& aCurrentTime,
                              ErrorResult& aRv);
  virtual AnimationPlayState PlayStateFromJS() const { return PlayState(); }
  virtual void PlayFromJS(ErrorResult& aRv)
  {
    Play(aRv, LimitBehavior::AutoRewind);
  }
  /**
   * PauseFromJS is currently only here for symmetry with PlayFromJS but
   * in future we will likely have to flush style in
   * CSSAnimation::PauseFromJS so we leave it for now.
   */
  void PauseFromJS(ErrorResult& aRv) { Pause(aRv); }

  // Wrapper functions for Animation DOM methods when called from style.

  virtual void CancelFromStyle() { CancelNoUpdate(); }

  virtual void Tick();
  bool NeedsTicks() const
  {
    AnimationPlayState playState = PlayState();
    return playState == AnimationPlayState::Running ||
           playState == AnimationPlayState::Pending;
  }

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
  /**
   * Testing only: Start or pause a pending animation using the current
   * timeline time. This is used to support existing tests that expect
   * animations to begin immediately. Ideally we would rewrite the those tests
   * and get rid of this method, but there are a lot of them.
   *
   * As with TriggerOnNextTick, the caller of this method is responsible for
   * removing the animation from any PendingAnimationTracker it may have been
   * added to.
   */
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

  /**
   * Converts a time in the timescale of this Animation's currentTime, to a
   * TimeStamp. Returns a null TimeStamp if the conversion cannot be performed
   * because of the current state of this Animation (e.g. it has no timeline, a
   * zero playbackRate, an unresolved start time etc.) or the value of the time
   * passed-in (e.g. an infinite time).
   */
  TimeStamp AnimationTimeToTimeStamp(const StickyTimeDuration& aTime) const;

  bool IsPausedOrPausing() const
  {
    return PlayState() == AnimationPlayState::Paused ||
           mPendingState == PendingState::PausePending;
  }

  bool HasInPlayEffect() const
  {
    return GetEffect() && GetEffect()->IsInPlay();
  }
  bool HasCurrentEffect() const
  {
    return GetEffect() && GetEffect()->IsCurrent();
  }
  bool IsInEffect() const
  {
    return GetEffect() && GetEffect()->IsInEffect();
  }

  /**
   * "Playing" is different to "running". An animation in its delay phase is
   * still running but we only consider it playing when it is in its active
   * interval. This definition is used for fetching the animations that are
   * candidates for running on the compositor (since we don't ship animations
   * to the compositor when they are in their delay phase or paused including
   * being effectively paused due to having a zero playback rate).
   */
  bool IsPlaying() const
  {
    // We need to have an effect in its active interval, and
    // be either running or waiting to run with a non-zero playback rate.
    return HasInPlayEffect() &&
           mPlaybackRate != 0.0 &&
           (PlayState() == AnimationPlayState::Running ||
            mPendingState == PendingState::PlayPending);
  }
  bool IsRelevant() const { return mIsRelevant; }
  void UpdateRelevance();

  /**
   * Returns true if this Animation has a lower composite order than aOther.
   */
  bool HasLowerCompositeOrderThan(const Animation& aOther) const;

   /**
   * Returns the level at which the effect(s) associated with this Animation
   * are applied to the CSS cascade.
   */
  virtual EffectCompositor::CascadeLevel CascadeLevel() const
  {
    return EffectCompositor::CascadeLevel::Animations;
  }

  /**
   * Returns true if this animation does not currently need to update
   * style on the main thread (e.g. because it is empty, or is
   * running on the compositor).
   */
  bool CanThrottle() const;
  /**
   * Updates |aStyleRule| with the animation values of this animation's effect,
   * if any.
   * Any properties already contained in |aSetProperties| are not changed. Any
   * properties that are changed are added to |aSetProperties|.
   */
  void ComposeStyle(RefPtr<AnimValuesStyleRule>& aStyleRule,
                    nsCSSPropertySet& aSetProperties);

  void NotifyEffectTimingUpdated();

protected:
  void SilentlySetCurrentTime(const TimeDuration& aNewCurrentTime);
  void SilentlySetPlaybackRate(double aPlaybackRate);
  void CancelNoUpdate();
  void PlayNoUpdate(ErrorResult& aRv, LimitBehavior aLimitBehavior);
  void PauseNoUpdate(ErrorResult& aRv);
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

  /**
   * Finishing behavior depends on if changes to timing occurred due
   * to a seek or regular playback.
   */
  enum class SeekFlag {
    NoSeek,
    DidSeek
  };

  enum class SyncNotifyFlag {
    Sync,
    Async
  };

  virtual void UpdateTiming(SeekFlag aSeekFlag,
                            SyncNotifyFlag aSyncNotifyFlag);
  void UpdateFinishedState(SeekFlag aSeekFlag,
                           SyncNotifyFlag aSyncNotifyFlag);
  void UpdateEffect();
  void FlushStyle() const;
  void PostUpdate();
  void ResetFinishedPromise();
  void MaybeResolveFinishedPromise();
  void DoFinishNotification(SyncNotifyFlag aSyncNotifyFlag);
  void DoFinishNotificationImmediately();
  void DispatchPlaybackEvent(const nsAString& aName);

  /**
   * Remove this animation from the pending animation tracker and reset
   * mPendingState as necessary. The caller is responsible for resolving or
   * aborting the mReady promise as necessary.
   */
  void CancelPendingTasks();

  bool IsPossiblyOrphanedPendingAnimation() const;
  StickyTimeDuration EffectEnd() const;

  nsIDocument* GetRenderedDocument() const;
  nsPresContext* GetPresContext() const;

  RefPtr<AnimationTimeline> mTimeline;
  RefPtr<KeyframeEffectReadOnly> mEffect;
  // The beginning of the delay period.
  Nullable<TimeDuration> mStartTime; // Timeline timescale
  Nullable<TimeDuration> mHoldTime;  // Animation timescale
  Nullable<TimeDuration> mPendingReadyTime; // Timeline timescale
  Nullable<TimeDuration> mPreviousCurrentTime; // Animation timescale
  double mPlaybackRate;

  // A Promise that is replaced on each call to Play()
  // and fulfilled when Play() is successfully completed.
  // This object is lazily created by GetReady.
  // See http://w3c.github.io/web-animations/#current-ready-promise
  RefPtr<Promise> mReady;

  // A Promise that is resolved when we reach the end of the effect, or
  // 0 when playing backwards. The Promise is replaced if the animation is
  // finished but then a state change makes it not finished.
  // This object is lazily created by GetFinished.
  // See http://w3c.github.io/web-animations/#current-finished-promise
  RefPtr<Promise> mFinished;

  // Indicates if the animation is in the pending state (and what state it is
  // waiting to enter when it finished pending). We use this rather than
  // checking if this animation is tracked by a PendingAnimationTracker because
  // the animation will continue to be pending even after it has been removed
  // from the PendingAnimationTracker while it is waiting for the next tick
  // (see TriggerOnNextTick for details).
  enum class PendingState { NotPending, PlayPending, PausePending };
  PendingState mPendingState;

  static uint64_t sNextAnimationIndex;

  // The relative position of this animation within the global animation list.
  // This is kNoIndex while the animation is in the idle state and is updated
  // each time the animation transitions out of the idle state.
  //
  // Note that subclasses such as CSSTransition and CSSAnimation may repurpose
  // this member to implement their own brand of sorting. As a result, it is
  // possible for two different objects to have the same index.
  uint64_t mAnimationIndex;

  bool mFinishedAtLastComposeStyle;
  // Indicates that the animation should be exposed in an element's
  // getAnimations() list.
  bool mIsRelevant;

  nsRevocableEventPtr<nsRunnableMethod<Animation>> mFinishNotificationTask;
  // True if mFinished is resolved or would be resolved if mFinished has
  // yet to be created. This is not set when mFinished is rejected since
  // in that case mFinished is immediately reset to represent a new current
  // finished promise.
  bool mFinishedIsResolved;

  nsString mId;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Animation_h
