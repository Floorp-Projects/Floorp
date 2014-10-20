/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationPlayer.h"
#include "AnimationUtils.h"
#include "mozilla/dom/AnimationPlayerBinding.h"
#include "nsLayoutUtils.h" // For PostRestyleEvent (remove after bug 1073336)

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationPlayer, mTimeline, mSource)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AnimationPlayer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AnimationPlayer, Release)

JSObject*
AnimationPlayer::WrapObject(JSContext* aCx)
{
  return dom::AnimationPlayerBinding::Wrap(aCx, this);
}

Nullable<double>
AnimationPlayer::GetStartTime() const
{
  return AnimationUtils::TimeDurationToDouble(mStartTime);
}

Nullable<TimeDuration>
AnimationPlayer::GetCurrentTime() const
{
  Nullable<TimeDuration> result;
  if (!mHoldTime.IsNull()) {
    result = mHoldTime;
  } else {
    Nullable<TimeDuration> timelineTime = mTimeline->GetCurrentTime();
    if (!timelineTime.IsNull() && !mStartTime.IsNull()) {
      result.SetValue(timelineTime.Value() - mStartTime.Value());
    }
  }
  return result;
}

AnimationPlayState
AnimationPlayer::PlayState() const
{
  Nullable<TimeDuration> currentTime = GetCurrentTime();
  if (currentTime.IsNull()) {
    return AnimationPlayState::Idle;
  }

  if (mIsPaused) {
    return AnimationPlayState::Paused;
  }

  if (currentTime.Value() >= SourceContentEnd()) {
    return AnimationPlayState::Finished;
  }

  return AnimationPlayState::Running;
}

void
AnimationPlayer::Play(UpdateFlags aFlags)
{
  // FIXME: When we implement finishing behavior (bug 1074630) we should
  // not return early if mIsPaused is false since we may still need to seek.
  // (However, we will need to pass a flag so that when we start playing due to
  //  a change in animation-play-state we *don't* trigger finishing behavior.)
  if (!mIsPaused) {
    return;
  }
  mIsPaused = false;

  Nullable<TimeDuration> timelineTime = mTimeline->GetCurrentTime();
  if (timelineTime.IsNull()) {
    // FIXME: We should just sit in the pending state in this case.
    // We will introduce the pending state in Bug 927349.
    return;
  }

  // Update start time to an appropriate offset from the current timeline time
  MOZ_ASSERT(!mHoldTime.IsNull(), "Hold time should not be null when paused");
  mStartTime.SetValue(timelineTime.Value() - mHoldTime.Value());
  mHoldTime.SetNull();

  if (aFlags == eUpdateStyle) {
    MaybePostRestyle();
  }
}

void
AnimationPlayer::Pause(UpdateFlags aFlags)
{
  if (mIsPaused) {
    return;
  }
  mIsPaused = true;
  mIsRunningOnCompositor = false;

  // Bug 927349 - check for null result here and go to pending state
  mHoldTime = GetCurrentTime();
  mStartTime.SetNull();

  if (aFlags == eUpdateStyle) {
    MaybePostRestyle();
  }
}

Nullable<double>
AnimationPlayer::GetCurrentTimeAsDouble() const
{
  return AnimationUtils::TimeDurationToDouble(GetCurrentTime());
}

AnimationPlayState
AnimationPlayer::PlayStateFromJS() const
{
  // FIXME: Once we introduce CSSTransitionPlayer, this should move to an
  // override of PlayStateFromJS in CSSAnimationPlayer and CSSTransitionPlayer
  // and we should skip it in the general case.
  FlushStyle();

  return PlayState();
}

void
AnimationPlayer::PlayFromJS()
{
  // Flush style to ensure that any properties controlling animation state
  // (e.g. animation-play-state) are fully updated before we proceed.
  //
  // Note that this might trigger PlayFromStyle()/PauseFromStyle() on this
  // object.
  //
  // FIXME: Once we introduce CSSTransitionPlayer, this should move to an
  // override of PlayFromJS in CSSAnimationPlayer and CSSTransitionPlayer and
  // we should skip it in the general case.
  FlushStyle();

  Play(eUpdateStyle);
}

void
AnimationPlayer::PauseFromJS()
{
  Pause(eUpdateStyle);
}

void
AnimationPlayer::SetSource(Animation* aSource)
{
  if (mSource) {
    mSource->SetParentTime(Nullable<TimeDuration>());
  }
  mSource = aSource;
  if (mSource) {
    mSource->SetParentTime(GetCurrentTime());
  }
}

void
AnimationPlayer::Tick()
{
  if (mSource) {
    mSource->SetParentTime(GetCurrentTime());
  }
}

bool
AnimationPlayer::IsRunning() const
{
  if (IsPaused() || !GetSource() || GetSource()->IsFinishedTransition()) {
    return false;
  }

  ComputedTiming computedTiming = GetSource()->GetComputedTiming();
  return computedTiming.mPhase == ComputedTiming::AnimationPhase_Active;
}

bool
AnimationPlayer::CanThrottle() const
{
  if (!mSource ||
      mSource->IsFinishedTransition() ||
      mSource->Properties().IsEmpty()) {
    return true;
  }

  if (!mIsRunningOnCompositor) {
    return false;
  }

  if (PlayState() != AnimationPlayState::Finished) {
    // Unfinished animations can be throttled.
    return true;
  }

  // The animation has finished but, if this is the first sample since
  // finishing, we need an unthrottled sample so we can apply the correct
  // end-of-animation behavior on the main thread (either removing the
  // animation style or applying the fill mode).
  return mIsPreviousStateFinished;
}

void
AnimationPlayer::ComposeStyle(nsRefPtr<css::AnimValuesStyleRule>& aStyleRule,
                              nsCSSPropertySet& aSetProperties,
                              bool& aNeedsRefreshes)
{
  if (!mSource || mSource->IsFinishedTransition()) {
    return;
  }

  AnimationPlayState playState = PlayState();
  if (playState == AnimationPlayState::Running) {
    aNeedsRefreshes = true;
  }

  mSource->ComposeStyle(aStyleRule, aSetProperties);

  mIsPreviousStateFinished = (playState == AnimationPlayState::Finished);
}

void
AnimationPlayer::FlushStyle() const
{
  if (mSource && mSource->GetTarget()) {
    nsIDocument* doc = mSource->GetTarget()->GetComposedDoc();
    if (doc) {
      doc->FlushPendingNotifications(Flush_Style);
    }
  }
}

void
AnimationPlayer::MaybePostRestyle() const
{
  if (!mSource || !mSource->GetTarget())
    return;

  // FIXME: This is a bit heavy-handed but in bug 1073336 we hope to
  // introduce a better means for players to update style.
  nsLayoutUtils::PostRestyleEvent(mSource->GetTarget(),
                                  eRestyle_Self,
                                  nsChangeHint_AllReflowHints);
}

StickyTimeDuration
AnimationPlayer::SourceContentEnd() const
{
  if (!mSource) {
    return StickyTimeDuration(0);
  }

  return mSource->Timing().mDelay
         + mSource->GetComputedTiming().mActiveDuration;
}

} // namespace dom
} // namespace mozilla
