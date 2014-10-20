/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationPlayer.h"
#include "AnimationUtils.h"
#include "mozilla/dom/AnimationPlayerBinding.h"

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
AnimationPlayer::Play()
{
  // TODO
}

void
AnimationPlayer::Pause()
{
  // TODO
}

void
AnimationPlayer::PlayFromJS()
{
  // TODO (flush styles etc.)

  Play();
}

void
AnimationPlayer::PauseFromJS()
{
  Pause();

  // TODO (flush styles etc.)
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

} // namespace dom
} // namespace mozilla
