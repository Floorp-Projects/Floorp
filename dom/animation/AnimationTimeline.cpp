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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AnimationTimeline, mDocument, mWindow)

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
