/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AnimationPlayer_h
#define mozilla_dom_AnimationPlayer_h

#include "nsWrapperCache.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"
#include "mozilla/TimeStamp.h" // for TimeStamp, TimeDuration
#include "mozilla/dom/Animation.h" // for Animation
#include "mozilla/dom/AnimationPlayerBinding.h" // for AnimationPlayState
#include "mozilla/dom/AnimationTimeline.h" // for AnimationTimeline
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

class AnimationPlayer : public nsISupports,
                        public nsWrapperCache
{
protected:
  virtual ~AnimationPlayer() { }

public:
  explicit AnimationPlayer(AnimationTimeline* aTimeline)
    : mTimeline(aTimeline)
    , mIsPending(false)
    , mIsRunningOnCompositor(false)
    , mIsPreviousStateFinished(false)
  {
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(AnimationPlayer)

  AnimationTimeline* GetParentObject() const { return mTimeline; }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual CSSAnimationPlayer* AsCSSAnimationPlayer() { return nullptr; }
  virtual CSSTransitionPlayer* AsCSSTransitionPlayer() { return nullptr; }

  // AnimationPlayer methods
  Animation* GetSource() const { return mSource; }
  AnimationTimeline* Timeline() const { return mTimeline; }
  Nullable<TimeDuration> GetStartTime() const { return mStartTime; }
  Nullable<TimeDuration> GetCurrentTime() const;
  AnimationPlayState PlayState() const;
  virtual Promise* GetReady(ErrorResult& aRv);
  virtual void Play();
  virtual void Pause();
  bool IsRunningOnCompositor() const { return mIsRunningOnCompositor; }

  // Wrapper functions for AnimationPlayer DOM methods when called
  // from script. We often use the same methods internally and from
  // script but when called from script we (or one of our subclasses) perform
  // extra steps such as flushing style or converting the return type.
  Nullable<double> GetStartTimeAsDouble() const;
  Nullable<double> GetCurrentTimeAsDouble() const;
  virtual AnimationPlayState PlayStateFromJS() const { return PlayState(); }
  virtual void PlayFromJS() { Play(); }
  // PauseFromJS is currently only here for symmetry with PlayFromJS but
  // in future we will likely have to flush style in
  // CSSAnimationPlayer::PauseFromJS so we leave it for now.
  void PauseFromJS() { Pause(); }

  void SetSource(Animation* aSource);
  void Tick();

  /**
   * Typically, when a player is played, it does not start immediately but is
   * added to a table of pending players on the document of its source content.
   * In the meantime it sets its hold time to the time from which playback
   * should begin.
   *
   * When the document finishes painting, any pending players in its table
   * are marked as being ready to start by calling StartOnNextTick.
   * The moment when the paint completed is also recorded, converted to a
   * timeline time, and passed to StartOnTick. This is so that when these
   * players do start, they can be timed from the point when painting
   * completed.
   *
   * After calling StartOnNextTick, players remain in the pending state until
   * the next refresh driver tick. At that time they transition out of the
   * pending state using the time passed to StartOnNextTick as the effective
   * time at which they resumed.
   *
   * This approach means that any setup time required for performing the
   * initial paint of an animation such as layerization is not deducted from
   * the running time of the animation. Without this we can easily drop the
   * first few frames of an animation, or, on slower devices, the whole
   * animation.
   *
   * Furthermore:
   *
   * - Starting the player immediately when painting finishes is problematic
   *   because the start time of the player will be ahead of its timeline
   *   (since the timeline time is based on the refresh driver time).
   *   That's a problem because the player is playing but its timing suggests
   *   it starts in the future. We could update the timeline to match the start
   *   time of the player but then we'd also have to update the timing and style
   *   of all animations connected to that timeline or else be stuck in an
   *   inconsistent state until the next refresh driver tick.
   *
   * - If we simply use the refresh driver time on its next tick, the lag
   *   between triggering an animation and its effective start is unacceptably
   *   long.
   *
   * Note that the caller of this method is responsible for removing the player
   * from any PendingPlayerTracker it may have been added to.
   */
  void StartOnNextTick(const Nullable<TimeDuration>& aReadyTime);

  // Testing only: Start a pending player using the current timeline time.
  // This is used to support existing tests that expect animations to begin
  // immediately. Ideally we would rewrite the those tests and get rid of this
  // method, but there are a lot of them.
  //
  // As with StartOnNextTick, the caller of this method is responsible for
  // removing the player from any PendingPlayerTracker it may have been added
  // to.
  void StartNow();

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

  const nsString& Name() const {
    return mSource ? mSource->Name() : EmptyString();
  }

  bool IsPaused() const { return PlayState() == AnimationPlayState::Paused; }
  bool IsRunning() const;

  bool HasCurrentSource() const {
    return GetSource() && GetSource()->IsCurrent();
  }
  bool HasInEffectSource() const {
    return GetSource() && GetSource()->IsInEffect();
  }

  void SetIsRunningOnCompositor() { mIsRunningOnCompositor = true; }
  void ClearIsRunningOnCompositor() { mIsRunningOnCompositor = false; }

  // Returns true if this animation does not currently need to update
  // style on the main thread (e.g. because it is empty, or is
  // running on the compositor).
  bool CanThrottle() const;

  // Updates |aStyleRule| with the animation values of this player's source
  // content, if any.
  // Any properties already contained in |aSetProperties| are not changed. Any
  // properties that are changed are added to |aSetProperties|.
  // |aNeedsRefreshes| will be set to true if this player expects to update
  // the style rule on the next refresh driver tick as well (because it
  // is running and has source content to sample).
  void ComposeStyle(nsRefPtr<css::AnimValuesStyleRule>& aStyleRule,
                    nsCSSPropertySet& aSetProperties,
                    bool& aNeedsRefreshes);

protected:
  void DoPlay();
  void DoPause();
  void ResumeAt(const TimeDuration& aResumeTime);

  void UpdateSourceContent();
  void FlushStyle() const;
  void PostUpdate();
  // Remove this player from the pending player tracker and resets mIsPending
  // as necessary. The caller is responsible for resolving or aborting the
  // mReady promise as necessary.
  void CancelPendingPlay();

  bool IsPossiblyOrphanedPendingPlayer() const;
  StickyTimeDuration SourceContentEnd() const;

  nsIDocument* GetRenderedDocument() const;
  nsPresContext* GetPresContext() const;
  virtual css::CommonAnimationManager* GetAnimationManager() const = 0;
  AnimationPlayerCollection* GetCollection() const;

  nsRefPtr<AnimationTimeline> mTimeline;
  nsRefPtr<Animation> mSource;
  // The beginning of the delay period.
  Nullable<TimeDuration> mStartTime; // Timeline timescale
  Nullable<TimeDuration> mHoldTime;  // Player timescale
  Nullable<TimeDuration> mPendingReadyTime; // Timeline timescale

  // A Promise that is replaced on each call to Play() (and in future Pause())
  // and fulfilled when Play() is successfully completed.
  // This object is lazily created by GetReady.
  nsRefPtr<Promise> mReady;

  // Indicates if the player is in the pending state. We use this rather
  // than checking if this player is tracked by a PendingPlayerTracker.
  // This is because the PendingPlayerTracker is associated with the source
  // content's document but we need to know if we're pending even if the
  // source content loses association with its document.
  bool mIsPending;
  bool mIsRunningOnCompositor;
  // Indicates whether we were in the finished state during our
  // most recent unthrottled sample (our last ComposeStyle call).
  // FIXME: When we implement the finished promise (bug 1074630) we can
  // probably remove this and check if the promise has been settled yet
  // or not instead.
  bool mIsPreviousStateFinished; // Spec calls this "previous finished state"
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AnimationPlayer_h
