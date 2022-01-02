/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Animation_h
#define mozilla_dom_Animation_h

#include "X11UndefineNone.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/AnimationPerformanceWarning.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/EffectCompositor.h"  // For EffectCompositor::CascadeLevel
#include "mozilla/LinkedList.h"
#include "mozilla/Maybe.h"
#include "mozilla/PostRestyleMode.h"
#include "mozilla/StickyTimeDuration.h"
#include "mozilla/TimeStamp.h"             // for TimeStamp, TimeDuration
#include "mozilla/dom/AnimationBinding.h"  // for AnimationPlayState
#include "mozilla/dom/AnimationTimeline.h"

struct JSContext;
class nsCSSPropertyIDSet;
class nsIFrame;
class nsIGlobalObject;

namespace mozilla {

struct AnimationRule;
class MicroTaskRunnable;

namespace dom {

class AnimationEffect;
class AsyncFinishNotification;
class CSSAnimation;
class CSSTransition;
class Document;
class Promise;

class Animation : public DOMEventTargetHelper,
                  public LinkedListElement<Animation> {
 protected:
  virtual ~Animation();

 public:
  explicit Animation(nsIGlobalObject* aGlobal);

  // Constructs a copy of |aOther| with a new effect and timeline.
  // This is only intended to be used while making a static clone of a document
  // during printing, and does not assume that |aOther| is in the same document
  // as any of the other arguments.
  static already_AddRefed<Animation> ClonePausedAnimation(
      nsIGlobalObject* aGlobal, const Animation& aOther,
      AnimationEffect& aEffect, AnimationTimeline& aTimeline);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(Animation, DOMEventTargetHelper)

  nsIGlobalObject* GetParentObject() const { return GetOwnerGlobal(); }

  /**
   * Utility function to get the target (pseudo-)element associated with an
   * animation.
   */
  NonOwningAnimationTarget GetTargetForAnimation() const;

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
  enum class LimitBehavior { AutoRewind, Continue };

  // Animation interface methods
  static already_AddRefed<Animation> Constructor(
      const GlobalObject& aGlobal, AnimationEffect* aEffect,
      const Optional<AnimationTimeline*>& aTimeline, ErrorResult& aRv);

  void GetId(nsAString& aResult) const { aResult = mId; }
  void SetId(const nsAString& aId);

  AnimationEffect* GetEffect() const { return mEffect; }
  virtual void SetEffect(AnimationEffect* aEffect);
  void SetEffectNoUpdate(AnimationEffect* aEffect);

  AnimationTimeline* GetTimeline() const { return mTimeline; }
  // Animation.timeline setter is supported only on Nightly.
  void SetTimeline(AnimationTimeline* aTimeline);
  void SetTimelineNoUpdate(AnimationTimeline* aTimeline);

  Nullable<TimeDuration> GetStartTime() const { return mStartTime; }
  Nullable<double> GetStartTimeAsDouble() const;
  void SetStartTime(const Nullable<TimeDuration>& aNewStartTime);
  virtual void SetStartTimeAsDouble(const Nullable<double>& aStartTime);

  // This is deliberately _not_ called GetCurrentTime since that would clash
  // with a macro defined in winbase.h
  Nullable<TimeDuration> GetCurrentTimeAsDuration() const {
    return GetCurrentTimeForHoldTime(mHoldTime);
  }
  Nullable<double> GetCurrentTimeAsDouble() const;
  void SetCurrentTime(const TimeDuration& aNewCurrentTime);
  void SetCurrentTimeAsDouble(const Nullable<double>& aCurrentTime,
                              ErrorResult& aRv);

  double PlaybackRate() const { return mPlaybackRate; }
  void SetPlaybackRate(double aPlaybackRate);

  AnimationPlayState PlayState() const;
  virtual AnimationPlayState PlayStateFromJS() const { return PlayState(); }

  bool Pending() const { return mPendingState != PendingState::NotPending; }
  virtual bool PendingFromJS() const { return Pending(); }
  AnimationReplaceState ReplaceState() const { return mReplaceState; }

  virtual Promise* GetReady(ErrorResult& aRv);
  Promise* GetFinished(ErrorResult& aRv);

  IMPL_EVENT_HANDLER(finish);
  IMPL_EVENT_HANDLER(cancel);
  IMPL_EVENT_HANDLER(remove);

  void Cancel(PostRestyleMode aPostRestyle = PostRestyleMode::IfNeeded);

  void Finish(ErrorResult& aRv);

  void Play(ErrorResult& aRv, LimitBehavior aLimitBehavior);
  virtual void PlayFromJS(ErrorResult& aRv) {
    Play(aRv, LimitBehavior::AutoRewind);
  }

  void Pause(ErrorResult& aRv);
  virtual void PauseFromJS(ErrorResult& aRv) { Pause(aRv); }

  void UpdatePlaybackRate(double aPlaybackRate);
  virtual void Reverse(ErrorResult& aRv);

  void Persist();
  MOZ_CAN_RUN_SCRIPT void CommitStyles(ErrorResult& aRv);

  bool IsRunningOnCompositor() const;

  virtual void Tick();
  bool NeedsTicks() const {
    return Pending() ||
           (PlayState() == AnimationPlayState::Running &&
            // An animation with a zero playback rate doesn't need ticks even if
            // it is running since it effectively behaves as if it is paused.
            //
            // It's important we return false in this case since a zero playback
            // rate animation in the before or after phase that doesn't fill
            // won't be relevant and hence won't be returned by GetAnimations().
            // We don't want its timeline to keep it alive (which would happen
            // if we return true) since otherwise it will effectively be leaked.
            PlaybackRate() != 0.0);
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
   * are marked as being ready to start by calling TriggerOnNextTick.
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
   * When TriggerOnNextTick is called, we store the ready time but we don't
   * apply it until the next tick. In the meantime, GetStartTime() will return
   * null.
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
   * As with the start time, we should use the pending playback rate when
   * producing layer animations.
   */
  double CurrentOrPendingPlaybackRate() const {
    return mPendingPlaybackRate.valueOr(mPlaybackRate);
  }
  bool HasPendingPlaybackRate() const { return mPendingPlaybackRate.isSome(); }

  /**
   * The following relationship from the definition of the 'current time' is
   * re-used in many algorithms so we extract it here into a static method that
   * can be re-used:
   *
   *   current time = (timeline time - start time) * playback rate
   *
   * As per https://drafts.csswg.org/web-animations-1/#current-time
   */
  static TimeDuration CurrentTimeFromTimelineTime(
      const TimeDuration& aTimelineTime, const TimeDuration& aStartTime,
      float aPlaybackRate) {
    return (aTimelineTime - aStartTime).MultDouble(aPlaybackRate);
  }

  /**
   * As with calculating the current time, we often need to calculate a start
   * time from a current time. The following method simply inverts the current
   * time relationship.
   *
   * In each case where this is used, the desired behavior for playbackRate ==
   * 0 is to return the specified timeline time (often referred to as the ready
   * time).
   */
  static TimeDuration StartTimeFromTimelineTime(
      const TimeDuration& aTimelineTime, const TimeDuration& aCurrentTime,
      float aPlaybackRate) {
    TimeDuration result = aTimelineTime;
    if (aPlaybackRate == 0) {
      return result;
    }

    result -= aCurrentTime.MultDouble(1.0 / aPlaybackRate);
    return result;
  }

  /**
   * Converts a time in the timescale of this Animation's currentTime, to a
   * TimeStamp. Returns a null TimeStamp if the conversion cannot be performed
   * because of the current state of this Animation (e.g. it has no timeline, a
   * zero playbackRate, an unresolved start time etc.) or the value of the time
   * passed-in (e.g. an infinite time).
   */
  TimeStamp AnimationTimeToTimeStamp(const StickyTimeDuration& aTime) const;

  // Converts an AnimationEvent's elapsedTime value to an equivalent TimeStamp
  // that can be used to sort events by when they occurred.
  TimeStamp ElapsedTimeToTimeStamp(
      const StickyTimeDuration& aElapsedTime) const;

  bool IsPausedOrPausing() const {
    return PlayState() == AnimationPlayState::Paused;
  }

  bool HasCurrentEffect() const;
  bool IsInEffect() const;

  bool IsPlaying() const {
    return mPlaybackRate != 0.0 && mTimeline &&
           !mTimeline->GetCurrentTimeAsDuration().IsNull() &&
           PlayState() == AnimationPlayState::Running;
  }

  bool ShouldBeSynchronizedWithMainThread(
      const nsCSSPropertyIDSet& aPropertySet, const nsIFrame* aFrame,
      AnimationPerformanceWarning::Type& aPerformanceWarning /* out */) const;

  bool IsRelevant() const { return mIsRelevant; }
  void UpdateRelevance();

  // https://drafts.csswg.org/web-animations-1/#replaceable-animation
  bool IsReplaceable() const;

  /**
   * Returns true if this Animation satisfies the requirements for being
   * removed when it is replaced.
   *
   * Returning true does not imply this animation _should_ be removed.
   * Determining that depends on the other effects in the same EffectSet to
   * which this animation's effect, if any, contributes.
   */
  bool IsRemovable() const;

  /**
   * Make this animation's target effect no-longer part of the effect stack
   * while preserving its timing information.
   */
  void Remove();

  /**
   * Returns true if this Animation has a lower composite order than aOther.
   */
  bool HasLowerCompositeOrderThan(const Animation& aOther) const;

  /**
   * Returns the level at which the effect(s) associated with this Animation
   * are applied to the CSS cascade.
   */
  virtual EffectCompositor::CascadeLevel CascadeLevel() const {
    return EffectCompositor::CascadeLevel::Animations;
  }

  /**
   * Returns true if this animation does not currently need to update
   * style on the main thread (e.g. because it is empty, or is
   * running on the compositor).
   */
  bool CanThrottle() const;

  /**
   * Updates various bits of state that we need to update as the result of
   * running ComposeStyle().
   * See the comment of KeyframeEffect::WillComposeStyle for more detail.
   */
  void WillComposeStyle();

  /**
   * Updates |aComposeResult| with the animation values of this animation's
   * effect, if any.
   * Any properties contained in |aPropertiesToSkip| will not be added or
   * updated in |aComposeResult|.
   */
  void ComposeStyle(RawServoAnimationValueMap& aComposeResult,
                    const nsCSSPropertyIDSet& aPropertiesToSkip);

  void NotifyEffectTimingUpdated();
  void NotifyEffectPropertiesUpdated();
  void NotifyEffectTargetUpdated();
  void NotifyGeometricAnimationsStartingThisFrame();

  /**
   * Reschedule pending pause or pending play tasks when updating the target
   * effect.
   *
   * If we are pending, we will either be registered in the pending animation
   * tracker and have a null pending ready time, or, after our effect has been
   * painted, we will be removed from the tracker and assigned a pending ready
   * time.
   *
   * When the target effect is updated, we'll typically need to repaint so for
   * the latter case where we already have a pending ready time, clear it and
   * put ourselves back in the pending animation tracker.
   */
  void ReschedulePendingTasks();

  /**
   * Used by subclasses to synchronously queue a cancel event in situations
   * where the Animation may have been cancelled.
   *
   * We need to do this synchronously because after a CSS animation/transition
   * is canceled, it will be released by its owning element and may not still
   * exist when we would normally go to queue events on the next tick.
   */
  virtual void MaybeQueueCancelEvent(const StickyTimeDuration& aActiveTime){};

  int32_t& CachedChildIndexRef() { return mCachedChildIndex; }

  void SetPartialPrerendered(uint64_t aIdOnCompositor) {
    mIdOnCompositor = aIdOnCompositor;
    mIsPartialPrerendered = true;
  }
  bool IsPartialPrerendered() const { return mIsPartialPrerendered; }
  uint64_t IdOnCompositor() const { return mIdOnCompositor; }
  /**
   * Needs to be called when the pre-rendered animation is going to no longer
   * run on the compositor.
   */
  void ResetPartialPrerendered() {
    MOZ_ASSERT(mIsPartialPrerendered);
    mIsPartialPrerendered = false;
    mIdOnCompositor = 0;
  }
  /**
   * Called via NotifyJankedAnimations IPC call from the compositor to update
   * pre-rendered area on the main-thread.
   */
  void UpdatePartialPrerendered() {
    ResetPartialPrerendered();
    PostUpdate();
  }

 protected:
  void SilentlySetCurrentTime(const TimeDuration& aNewCurrentTime);
  void CancelNoUpdate();
  void PlayNoUpdate(ErrorResult& aRv, LimitBehavior aLimitBehavior);
  void ResumeAt(const TimeDuration& aReadyTime);
  void PauseAt(const TimeDuration& aReadyTime);
  void FinishPendingAt(const TimeDuration& aReadyTime) {
    if (mPendingState == PendingState::PlayPending) {
      ResumeAt(aReadyTime);
    } else if (mPendingState == PendingState::PausePending) {
      PauseAt(aReadyTime);
    } else {
      MOZ_ASSERT_UNREACHABLE(
          "Can't finish pending if we're not in a pending state");
    }
  }
  void ApplyPendingPlaybackRate() {
    if (mPendingPlaybackRate) {
      mPlaybackRate = *mPendingPlaybackRate;
      mPendingPlaybackRate.reset();
    }
  }

  /**
   * Finishing behavior depends on if changes to timing occurred due
   * to a seek or regular playback.
   */
  enum class SeekFlag { NoSeek, DidSeek };

  enum class SyncNotifyFlag { Sync, Async };

  virtual void UpdateTiming(SeekFlag aSeekFlag, SyncNotifyFlag aSyncNotifyFlag);
  void UpdateFinishedState(SeekFlag aSeekFlag, SyncNotifyFlag aSyncNotifyFlag);
  void UpdateEffect(PostRestyleMode aPostRestyle);
  /**
   * Flush all pending styles other than throttled animation styles (e.g.
   * animations running on the compositor).
   */
  void FlushUnanimatedStyle() const;
  void PostUpdate();
  void ResetFinishedPromise();
  void MaybeResolveFinishedPromise();
  void DoFinishNotification(SyncNotifyFlag aSyncNotifyFlag);
  friend class AsyncFinishNotification;
  void DoFinishNotificationImmediately(MicroTaskRunnable* aAsync = nullptr);
  void QueuePlaybackEvent(const nsAString& aName,
                          TimeStamp&& aScheduledEventTime);

  /**
   * Remove this animation from the pending animation tracker and reset
   * mPendingState as necessary. The caller is responsible for resolving or
   * aborting the mReady promise as necessary.
   */
  void CancelPendingTasks();

  /**
   * Performs the same steps as CancelPendingTasks and also rejects and
   * recreates the ready promise if the animation was pending.
   */
  void ResetPendingTasks();

  /**
   * Returns true if this animation is not only play-pending, but has
   * yet to be given a pending ready time. This roughly corresponds to
   * animations that are waiting to be painted (since we set the pending
   * ready time at the end of painting). Identifying such animations is
   * useful because in some cases animations that are painted together
   * may need to be synchronized.
   *
   * We don't, however, want to include animations with a fixed start time such
   * as animations that are simply having their playbackRate updated or which
   * are resuming from an aborted pause.
   */
  bool IsNewlyStarted() const {
    return mPendingState == PendingState::PlayPending &&
           mPendingReadyTime.IsNull() && mStartTime.IsNull();
  }
  bool IsPossiblyOrphanedPendingAnimation() const;
  StickyTimeDuration EffectEnd() const;

  Nullable<TimeDuration> GetCurrentTimeForHoldTime(
      const Nullable<TimeDuration>& aHoldTime) const;
  Nullable<TimeDuration> GetUnconstrainedCurrentTime() const {
    return GetCurrentTimeForHoldTime(Nullable<TimeDuration>());
  }

  void ScheduleReplacementCheck();
  void MaybeScheduleReplacementCheck();

  // Earlier side of the elapsed time range reported in CSS Animations and CSS
  // Transitions events.
  //
  // https://drafts.csswg.org/css-animations-2/#interval-start
  // https://drafts.csswg.org/css-transitions-2/#interval-start
  StickyTimeDuration IntervalStartTime(
      const StickyTimeDuration& aActiveDuration) const;

  // Later side of the elapsed time range reported in CSS Animations and CSS
  // Transitions events.
  //
  // https://drafts.csswg.org/css-animations-2/#interval-end
  // https://drafts.csswg.org/css-transitions-2/#interval-end
  StickyTimeDuration IntervalEndTime(
      const StickyTimeDuration& aActiveDuration) const;

  TimeStamp GetTimelineCurrentTimeAsTimeStamp() const {
    return mTimeline ? mTimeline->GetCurrentTimeAsTimeStamp() : TimeStamp();
  }

  Document* GetRenderedDocument() const;
  Document* GetTimelineDocument() const;

  RefPtr<AnimationTimeline> mTimeline;
  RefPtr<AnimationEffect> mEffect;
  // The beginning of the delay period.
  Nullable<TimeDuration> mStartTime;            // Timeline timescale
  Nullable<TimeDuration> mHoldTime;             // Animation timescale
  Nullable<TimeDuration> mPendingReadyTime;     // Timeline timescale
  Nullable<TimeDuration> mPreviousCurrentTime;  // Animation timescale
  double mPlaybackRate = 1.0;
  Maybe<double> mPendingPlaybackRate;

  // A Promise that is replaced on each call to Play()
  // and fulfilled when Play() is successfully completed.
  // This object is lazily created by GetReady.
  // See http://drafts.csswg.org/web-animations/#current-ready-promise
  RefPtr<Promise> mReady;

  // A Promise that is resolved when we reach the end of the effect, or
  // 0 when playing backwards. The Promise is replaced if the animation is
  // finished but then a state change makes it not finished.
  // This object is lazily created by GetFinished.
  // See http://drafts.csswg.org/web-animations/#current-finished-promise
  RefPtr<Promise> mFinished;

  static uint64_t sNextAnimationIndex;

  // The relative position of this animation within the global animation list.
  //
  // Note that subclasses such as CSSTransition and CSSAnimation may repurpose
  // this member to implement their own brand of sorting. As a result, it is
  // possible for two different objects to have the same index.
  uint64_t mAnimationIndex;

  // While ordering Animation objects for event dispatch, the index of the
  // target node in its parent may be cached in mCachedChildIndex.
  int32_t mCachedChildIndex = -1;

  // Indicates if the animation is in the pending state (and what state it is
  // waiting to enter when it finished pending). We use this rather than
  // checking if this animation is tracked by a PendingAnimationTracker because
  // the animation will continue to be pending even after it has been removed
  // from the PendingAnimationTracker while it is waiting for the next tick
  // (see TriggerOnNextTick for details).
  enum class PendingState : uint8_t { NotPending, PlayPending, PausePending };
  PendingState mPendingState = PendingState::NotPending;

  // Handling of this animation's target effect when filling while finished.
  AnimationReplaceState mReplaceState = AnimationReplaceState::Active;

  bool mFinishedAtLastComposeStyle = false;
  bool mWasReplaceableAtLastTick = false;

  // Indicates that the animation should be exposed in an element's
  // getAnimations() list.
  bool mIsRelevant = false;

  // True if mFinished is resolved or would be resolved if mFinished has
  // yet to be created. This is not set when mFinished is rejected since
  // in that case mFinished is immediately reset to represent a new current
  // finished promise.
  bool mFinishedIsResolved = false;

  // True if this animation was triggered at the same time as one or more
  // geometric animations and hence we should run any transform animations on
  // the main thread.
  bool mSyncWithGeometricAnimations = false;

  RefPtr<MicroTaskRunnable> mFinishNotificationTask;

  nsString mId;

 private:
  // The id for this animaiton on the compositor.
  uint64_t mIdOnCompositor = 0;
  bool mIsPartialPrerendered = false;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_Animation_h
