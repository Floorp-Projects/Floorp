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
#include "mozilla/dom/AnimationTimeline.h" // for AnimationTimeline
#include "nsCSSProperty.h" // for nsCSSProperty

// X11 has a #define for CurrentTime.
#ifdef CurrentTime
#undef CurrentTime
#endif

struct JSContext;

namespace mozilla {

class CSSAnimationPlayer;

namespace dom {

class AnimationPlayer : public nsWrapperCache
{
protected:
  virtual ~AnimationPlayer() { }

public:
  explicit AnimationPlayer(AnimationTimeline* aTimeline)
    : mIsPaused(false)
    , mIsRunningOnCompositor(false)
    , mTimeline(aTimeline)
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
  Nullable<double> GetCurrentTime() const;
  void Play(UpdateFlags aUpdateFlags);
  void Pause(UpdateFlags aUpdateFlags);
  bool IsRunningOnCompositor() const { return mIsRunningOnCompositor; }

  // Wrapper functions for performing extra steps such as flushing
  // style when calling from JS.
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

  // Return the duration since the start time of the player, taking into
  // account the pause state.  May be negative or null.
  Nullable<TimeDuration> GetCurrentTimeDuration() const;

  // The beginning of the delay period.
  Nullable<TimeDuration> mStartTime; // Timeline timescale
  Nullable<TimeDuration> mHoldTime;  // Player timescale
  bool mIsPaused;
  bool mIsRunningOnCompositor;

  nsRefPtr<AnimationTimeline> mTimeline;
  nsRefPtr<Animation> mSource;

protected:
  void FlushStyle() const;
  void MaybePostRestyle() const;
};

} // namespace dom

class CSSAnimationPlayer MOZ_FINAL : public dom::AnimationPlayer
{
public:
 explicit CSSAnimationPlayer(dom::AnimationTimeline* aTimeline)
    : dom::AnimationPlayer(aTimeline)
  {
  }

  virtual CSSAnimationPlayer*
  AsCSSAnimationPlayer() MOZ_OVERRIDE { return this; }

protected:
  virtual ~CSSAnimationPlayer() { }
};

} // namespace mozilla

#endif // mozilla_dom_AnimationPlayer_h
