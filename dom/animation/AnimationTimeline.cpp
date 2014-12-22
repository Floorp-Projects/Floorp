/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationTimeline.h"
#include "mozilla/dom/AnimationTimelineBinding.h"
#include "AnimationUtils.h"
#include "nsContentUtils.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsRefreshDriver.h"
#include "nsDOMNavigationTiming.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationTimeline, mDocument)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AnimationTimeline, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AnimationTimeline, Release)

JSObject*
AnimationTimeline::WrapObject(JSContext* aCx)
{
  return AnimationTimelineBinding::Wrap(aCx, this);
}

Nullable<TimeDuration>
AnimationTimeline::GetCurrentTime() const
{
  return ToTimelineTime(GetCurrentTimeStamp());
}

Nullable<double>
AnimationTimeline::GetCurrentTimeAsDouble() const
{
  return AnimationUtils::TimeDurationToDouble(GetCurrentTime());
}

void
AnimationTimeline::FastForward(const TimeStamp& aTimeStamp)
{
  // If we have already been fast-forwarded to an equally or more
  // recent time, ignore this call.
  if (!mFastForwardTime.IsNull() && aTimeStamp <= mFastForwardTime) {
    return;
  }

  // If the refresh driver is under test control then its values have little
  // connection to TimeStamp values and it doesn't make sense to fast-forward
  // the timeline to a TimeStamp value.
  //
  // Furthermore, when the refresh driver is under test control,
  // nsDOMWindowUtils::AdvanceTimeAndRefresh automatically starts any
  // pending animation players so we don't need to fast-forward the timeline
  // anyway.
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (refreshDriver && refreshDriver->IsTestControllingRefreshesEnabled()) {
    return;
  }

  // Bug 1113413: If the refresh driver has just been restored from test
  // control it's possible that aTimeStamp could be before the most recent
  // refresh.
  if (refreshDriver &&
      aTimeStamp < refreshDriver->MostRecentRefresh()) {
    mFastForwardTime = refreshDriver->MostRecentRefresh();
    return;
  }

  // FIXME: For all animations attached to this timeline, we should mark
  // their target elements as needing restyling. Otherwise, tasks that run
  // in between now and the next refresh driver tick might see inconsistencies
  // between the timing of an animation and the computed style of its target.

  mFastForwardTime = aTimeStamp;
}

TimeStamp
AnimationTimeline::GetCurrentTimeStamp() const
{
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  TimeStamp refreshTime = refreshDriver
                          ? refreshDriver->MostRecentRefresh()
                          : TimeStamp();

  // Always return the same object to benefit from return-value optimization.
  TimeStamp result = !refreshTime.IsNull()
                     ? refreshTime
                     : mLastRefreshDriverTime;

  // If we don't have a refresh driver and we've never had one use the
  // timeline's zero time.
  if (result.IsNull()) {
    nsRefPtr<nsDOMNavigationTiming> timing = mDocument->GetNavigationTiming();
    if (timing) {
      result = timing->GetNavigationStartTimeStamp();
      // Also, let this time represent the current refresh time. This way
      // we'll save it as the last refresh time and skip looking up
      // navigation timing each time.
      refreshTime = result;
    }
  }

  // The timeline may have been fast-forwarded to account for animations
  // that begin playing between ticks of the refresh driver. If so, we should
  // use the fast-forward time unless we've already gone past that time.
  //
  // (If the refresh driver were ever to go backwards then we would need to
  //  ignore the fast-forward time in that case to prevent the timeline getting
  //  "stuck" until the refresh driver caught up. However, the only time the
  //  refresh driver goes backwards is when it is restored from test control
  //  and FastForward makes sure we don't set the fast foward time when we
  //  are under test control.)
  MOZ_ASSERT(refreshTime.IsNull() || mLastRefreshDriverTime.IsNull() ||
             refreshTime >= mLastRefreshDriverTime ||
             mFastForwardTime.IsNull(),
             "The refresh driver time should not go backwards when the"
             " fast-forward time is set");

  // We need to check if mFastForwardTime is ahead of the refresh driver
  // time. This is because mFastForwardTime can still be set after the next
  // refresh driver tick since we don't clear mFastForwardTime on a call to
  // Tick() as we aren't currently guaranteed to get only one call to Tick()
  // per refresh-driver tick.
  if (result.IsNull() ||
       (!mFastForwardTime.IsNull() && mFastForwardTime > result)) {
    result = mFastForwardTime;
  } else {
    // Make sure we continue to ignore the fast-forward time.
    mFastForwardTime = TimeStamp();
  }

  if (!refreshTime.IsNull()) {
    mLastRefreshDriverTime = refreshTime;
  }

  return result;
}

Nullable<TimeDuration>
AnimationTimeline::ToTimelineTime(const TimeStamp& aTimeStamp) const
{
  Nullable<TimeDuration> result; // Initializes to null
  if (aTimeStamp.IsNull()) {
    return result;
  }

  nsRefPtr<nsDOMNavigationTiming> timing = mDocument->GetNavigationTiming();
  if (MOZ_UNLIKELY(!timing)) {
    return result;
  }

  result.SetValue(aTimeStamp - timing->GetNavigationStartTimeStamp());
  return result;
}

TimeStamp
AnimationTimeline::ToTimeStamp(const TimeDuration& aTimeDuration) const
{
  TimeStamp result;
  nsRefPtr<nsDOMNavigationTiming> timing = mDocument->GetNavigationTiming();
  if (MOZ_UNLIKELY(!timing)) {
    return result;
  }

  result = timing->GetNavigationStartTimeStamp() + aTimeDuration;
  return result;
}

nsRefreshDriver*
AnimationTimeline::GetRefreshDriver() const
{
  nsIPresShell* presShell = mDocument->GetShell();
  if (MOZ_UNLIKELY(!presShell)) {
    return nullptr;
  }

  nsPresContext* presContext = presShell->GetPresContext();
  if (MOZ_UNLIKELY(!presContext)) {
    return nullptr;
  }

  return presContext->RefreshDriver();
}

} // namespace dom
} // namespace mozilla
