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
#include "nsCSSProperty.h" // for nsCSSProperty

// X11 has a #define for CurrentTime.
#ifdef CurrentTime
#undef CurrentTime
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

class AnimationPlayer : public nsWrapperCache
{
protected:
  virtual ~AnimationPlayer() { }

public:
  explicit AnimationPlayer(AnimationTimeline* aTimeline)
    : mTimeline(aTimeline)
    , mIsRunningOnCompositor(false)
    , mIsPreviousStateFinished(false)
  {
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnimationPlayer)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AnimationPlayer)

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

  // The beginning of the delay period.
  Nullable<TimeDuration> mStartTime; // Timeline timescale

protected:
  void DoPlay();
  void DoPause();

  void FlushStyle() const;
  void PostUpdate();
  StickyTimeDuration SourceContentEnd() const;

  nsIDocument* GetRenderedDocument() const;
  nsPresContext* GetPresContext() const;
  virtual css::CommonAnimationManager* GetAnimationManager() const = 0;
  AnimationPlayerCollection* GetCollection() const;

  nsRefPtr<AnimationTimeline> mTimeline;
  nsRefPtr<Animation> mSource;
  Nullable<TimeDuration> mHoldTime;  // Player timescale
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
