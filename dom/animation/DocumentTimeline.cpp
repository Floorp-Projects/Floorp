/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentTimeline.h"
#include "mozilla/dom/DocumentTimelineBinding.h"
#include "AnimationUtils.h"
#include "nsContentUtils.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsRefreshDriver.h"
#include "nsDOMNavigationTiming.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(DocumentTimeline, AnimationTimeline,
                                   mDocument)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(DocumentTimeline,
                                               AnimationTimeline)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DocumentTimeline)
NS_INTERFACE_MAP_END_INHERITING(AnimationTimeline)

NS_IMPL_ADDREF_INHERITED(DocumentTimeline, AnimationTimeline)
NS_IMPL_RELEASE_INHERITED(DocumentTimeline, AnimationTimeline)

JSObject*
DocumentTimeline::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DocumentTimelineBinding::Wrap(aCx, this, aGivenProto);
}

Nullable<TimeDuration>
DocumentTimeline::GetCurrentTime() const
{
  return ToTimelineTime(GetCurrentTimeStamp());
}

TimeStamp
DocumentTimeline::GetCurrentTimeStamp() const
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
DocumentTimeline::ToTimelineTime(const TimeStamp& aTimeStamp) const
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
DocumentTimeline::ToTimeStamp(const TimeDuration& aTimeDuration) const
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
DocumentTimeline::GetRefreshDriver() const
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
