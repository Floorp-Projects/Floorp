/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AnimationTimeline.h"
#include "mozilla/dom/AnimationTimelineBinding.h"
#include "mozilla/TimeStamp.h"
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
  return ToTimelineTime(GetCurrentTimeStamp());
}

TimeStamp
AnimationTimeline::GetCurrentTimeStamp() const
{
  // Always return the same object to benefit from return-value optimization.
  TimeStamp result; // Initializes to null timestamp

  nsIPresShell* presShell = mDocument->GetShell();
  if (MOZ_UNLIKELY(!presShell)) {
    return result;
  }

  nsPresContext* presContext = presShell->GetPresContext();
  if (MOZ_UNLIKELY(!presContext)) {
    return result;
  }

  result = presContext->RefreshDriver()->MostRecentRefresh();
  return result;
}

Nullable<double>
AnimationTimeline::ToTimelineTime(const mozilla::TimeStamp& aTimeStamp) const
{
  Nullable<double> result; // Initializes to null
  if (aTimeStamp.IsNull()) {
    return result;
  }

  nsRefPtr<nsDOMNavigationTiming> timing = mDocument->GetNavigationTiming();
  if (MOZ_UNLIKELY(!timing)) {
    return result;
  }

  result.SetValue(timing->TimeStampToDOMHighRes(aTimeStamp));
  return result;
}

} // namespace dom
} // namespace mozilla
