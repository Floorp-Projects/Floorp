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

Nullable<double>
AnimationTimeline::GetCurrentTime() const
{
  return AnimationUtils::TimeDurationToDouble(GetCurrentTimeDuration());
}

TimeStamp
AnimationTimeline::GetCurrentTimeStamp() const
{
  // Always return the same object to benefit from return-value optimization.
  TimeStamp result = mLastCurrentTime;

  // If we've never been sampled, initialize the current time to the timeline's
  // zero time since that is the time we'll use if we don't have a refresh
  // driver.
  if (result.IsNull()) {
    nsRefPtr<nsDOMNavigationTiming> timing = mDocument->GetNavigationTiming();
    if (!timing) {
      return result;
    }
    result = timing->GetNavigationStartTimeStamp();
  }

  nsIPresShell* presShell = mDocument->GetShell();
  if (MOZ_UNLIKELY(!presShell)) {
    return result;
  }

  nsPresContext* presContext = presShell->GetPresContext();
  if (MOZ_UNLIKELY(!presContext)) {
    return result;
  }

  result = presContext->RefreshDriver()->MostRecentRefresh();
  // FIXME: We would like to assert that:
  //   mLastCurrentTime.IsNull() || result >= mLastCurrentTime
  // but due to bug 1043078 this will not be the case when the refresh driver
  // is restored from test control.
  mLastCurrentTime = result;
  return result;
}

Nullable<TimeDuration>
AnimationTimeline::GetCurrentTimeDuration() const
{
  return ToTimelineTime(GetCurrentTimeStamp());
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

} // namespace dom
} // namespace mozilla
