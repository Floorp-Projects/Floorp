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

Nullable<double>
AnimationPlayer::GetCurrentTime() const
{
  return AnimationUtils::TimeDurationToDouble(GetCurrentTimeDuration());
}

void
AnimationPlayer::Play(UpdateFlags aFlags)
{
  // FIXME: When we implement finishing behavior (bug 1074630) we should
  // not return early if mIsPaused is false since we may still need to seek.
  if (!mIsPaused) {
    return;
  }
  mIsPaused = false;

  Nullable<TimeDuration> timelineTime = mTimeline->GetCurrentTimeDuration();
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
  mHoldTime = GetCurrentTimeDuration();
  mStartTime.SetNull();

  if (aFlags == eUpdateStyle) {
    MaybePostRestyle();
  }
}

void
AnimationPlayer::PlayFromJS()
{
  // TODO (flush styles etc.)

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
    mSource->SetParentTime(GetCurrentTimeDuration());
  }
}

void
AnimationPlayer::Tick()
{
  if (mSource) {
    mSource->SetParentTime(GetCurrentTimeDuration());
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

Nullable<TimeDuration>
AnimationPlayer::GetCurrentTimeDuration() const
{
  Nullable<TimeDuration> result;
  if (!mHoldTime.IsNull()) {
    result = mHoldTime;
  } else {
    Nullable<TimeDuration> timelineTime = mTimeline->GetCurrentTimeDuration();
    if (!timelineTime.IsNull() && !mStartTime.IsNull()) {
      result.SetValue(timelineTime.Value() - mStartTime.Value());
    }
  }
  return result;
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

} // namespace dom
} // namespace mozilla
