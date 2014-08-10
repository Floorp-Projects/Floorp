/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationPlayer.h"
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

double
AnimationPlayer::StartTime() const
{
  Nullable<double> startTime = mTimeline->ToTimelineTime(mStartTime);
  return startTime.IsNull() ? 0.0 : startTime.Value();
}

double
AnimationPlayer::CurrentTime() const
{
  Nullable<TimeDuration> currentTime = GetCurrentTimeDuration();

  // The current time is currently only going to be null when don't have a
  // refresh driver (e.g. because we are in a display:none iframe).
  //
  // Web Animations says that in this case we should use a timeline time of
  // 0 (the "effective timeline time") and calculate the current time from that.
  // Doing that, however, requires storing the start time as an offset rather
  // than a timestamp so for now we just return 0.
  //
  // FIXME: Store player start time and pause start as offsets rather than
  // timestamps and return the appropriate current time when the timeline time
  // is null.
  if (currentTime.IsNull()) {
    return 0.0;
  }

  return currentTime.Value().ToMilliseconds();
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

bool
AnimationPlayer::IsCurrent() const
{
  return GetSource() && GetSource()->IsCurrent();
}

} // namespace dom
} // namespace mozilla
