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

namespace mozilla {
namespace css {
class AnimValuesStyleRule;
} // namespace css

class CSSAnimationPlayer;

namespace dom {

class AnimationPlayer : public nsWrapperCache
{
protected:
  virtual ~AnimationPlayer() { }

public:
  explicit AnimationPlayer(AnimationTimeline* aTimeline)
    : mTimeline(aTimeline)
    , mIsPaused(false)
    , mIsRunningOnCompositor(false)
    , mIsPreviousStateFinished(false)
  {
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnimationPlayer)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(AnimationPlayer)

  AnimationTimeline* GetParentObject() const { return mTimeline; }
  virtual JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  virtual CSSAnimationPlayer* AsCSSAnimationPlayer() { return nullptr; }

  // Temporary flags to control restyle behavior until bug 1073336
  // provides a better solution.
  enum UpdateFlags {
    eNoUpdate,
    eUpdateStyle
  };

  // AnimationPlayer methods
  Animation* GetSource() const { return mSource; }
  AnimationTimeline* Timeline() const { return mTimeline; }
  Nullable<double> GetStartTime() const;
  Nullable<TimeDuration> GetCurrentTime() const;
  AnimationPlayState PlayState() const;
  virtual void Play(UpdateFlags aUpdateFlags);
  virtual void Pause(UpdateFlags aUpdateFlags);
  bool IsRunningOnCompositor() const { return mIsRunningOnCompositor; }

  // Wrapper functions for AnimationPlayer DOM methods when called
  // from script. We often use the same methods internally and from
  // script but when called from script we perform extra steps such
  // as flushing style or converting the return type.
  Nullable<double> GetCurrentTimeAsDouble() const;
  AnimationPlayState PlayStateFromJS() const;
  void PlayFromJS();
  void PauseFromJS();

  void SetSource(Animation* aSource);
  void Tick();

  const nsString& Name() const {
    return mSource ? mSource->Name() : EmptyString();
  }

  bool IsPaused() const { return mIsPaused; }
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
  void FlushStyle() const;
  void MaybePostRestyle() const;
  StickyTimeDuration SourceContentEnd() const;

  nsRefPtr<AnimationTimeline> mTimeline;
  nsRefPtr<Animation> mSource;
  Nullable<TimeDuration> mHoldTime;  // Player timescale
  bool mIsPaused;
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
