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

namespace mozilla {

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

  // The beginning of the delay period.
  Nullable<TimeDuration> mStartTime; // Timeline timescale

  nsRefPtr<AnimationTimeline> mTimeline;
  nsRefPtr<Animation> mSource;

protected:
  void FlushStyle() const;
  void MaybePostRestyle() const;
  StickyTimeDuration SourceContentEnd() const;

  Nullable<TimeDuration> mHoldTime;  // Player timescale
  bool mIsPaused;
  bool mIsRunningOnCompositor;
};

} // namespace dom

class CSSAnimationPlayer MOZ_FINAL : public dom::AnimationPlayer
{
public:
 explicit CSSAnimationPlayer(dom::AnimationTimeline* aTimeline)
    : dom::AnimationPlayer(aTimeline)
    , mIsStylePaused(false)
    , mPauseShouldStick(false)
  {
  }

  virtual CSSAnimationPlayer*
  AsCSSAnimationPlayer() MOZ_OVERRIDE { return this; }

  virtual void Play(UpdateFlags aUpdateFlags) MOZ_OVERRIDE;
  virtual void Pause(UpdateFlags aUpdateFlags) MOZ_OVERRIDE;

  void PlayFromStyle();
  void PauseFromStyle();

  bool IsStylePaused() const { return mIsStylePaused; }

protected:
  virtual ~CSSAnimationPlayer() { }

  // When combining animation-play-state with play() / pause() the following
  // behavior applies:
  // 1. pause() is sticky and always overrides the underlying
  //    animation-play-state
  // 2. If animation-play-state is 'paused', play() will temporarily override
  //    it until animation-play-state next becomes 'running'.
  // 3. Calls to play() trigger finishing behavior but setting the
  //    animation-play-state to 'running' does not.
  //
  // This leads to five distinct states:
  //
  // A. Running
  // B. Running and temporarily overriding animation-play-state: paused
  // C. Paused and sticky overriding animation-play-state: running
  // D. Paused and sticky overriding animation-play-state: paused
  // E. Paused by animation-play-state
  //
  // C and D may seem redundant but they differ in how to respond to the
  // sequence: call play(), set animation-play-state: paused.
  //
  // C will transition to A then E leaving the animation paused.
  // D will transition to B then B leaving the animation running.
  //
  // A state transition chart is as follows:
  //
  //             A | B | C | D | E
  //   ---------------------------
  //   play()    A | B | A | B | B
  //   pause()   C | D | C | D | D
  //   'running' A | A | C | C | A
  //   'paused'  E | B | D | D | E
  //
  // The base class, AnimationPlayer already provides a boolean value,
  // mIsPaused which gives us two states. To this we add a further two booleans
  // to represent the states as follows.
  //
  // A. Running
  //    (!mIsPaused; !mIsStylePaused; !mPauseShouldStick)
  // B. Running and temporarily overriding animation-play-state: paused
  //    (!mIsPaused; mIsStylePaused; !mPauseShouldStick)
  // C. Paused and sticky overriding animation-play-state: running
  //    (mIsPaused; !mIsStylePaused; mPauseShouldStick)
  // D. Paused and sticky overriding animation-play-state: paused
  //    (mIsPaused; mIsStylePaused; mPauseShouldStick)
  // E. Paused by animation-play-state
  //    (mIsPaused; mIsStylePaused; !mPauseShouldStick)
  //
  // (That leaves 3 combinations of the boolean values that we never set because
  // they don't represent valid states.)
  bool mIsStylePaused;
  bool mPauseShouldStick;
};

} // namespace mozilla

#endif // mozilla_dom_AnimationPlayer_h
