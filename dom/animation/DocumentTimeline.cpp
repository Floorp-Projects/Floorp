/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentTimeline.h"
#include "mozilla/dom/DocumentTimelineBinding.h"
#include "AnimationUtils.h"
#include "nsContentUtils.h"
#include "nsDOMMutationObserver.h"
#include "nsDOMNavigationTiming.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsRefreshDriver.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(DocumentTimeline)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DocumentTimeline,
                                                AnimationTimeline)
  tmp->UnregisterFromRefreshDriver();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DocumentTimeline,
                                                  AnimationTimeline)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

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

/* static */ already_AddRefed<DocumentTimeline>
DocumentTimeline::Constructor(const GlobalObject& aGlobal,
                              const DocumentTimelineOptions& aOptions,
                              ErrorResult& aRv)
{
  nsIDocument* doc = AnimationUtils::GetCurrentRealmDocument(aGlobal.Context());
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  TimeDuration originTime =
    TimeDuration::FromMilliseconds(aOptions.mOriginTime);

  if (originTime == TimeDuration::Forever() ||
      originTime == -TimeDuration::Forever()) {
    aRv.ThrowTypeError<dom::MSG_TIME_VALUE_OUT_OF_RANGE>(
      NS_LITERAL_STRING("Origin time"));
    return nullptr;
  }
  RefPtr<DocumentTimeline> timeline = new DocumentTimeline(doc, originTime);

  return timeline.forget();
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
    RefPtr<nsDOMNavigationTiming> timing = mDocument->GetNavigationTiming();
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

  RefPtr<nsDOMNavigationTiming> timing = mDocument->GetNavigationTiming();
  if (MOZ_UNLIKELY(!timing)) {
    return result;
  }

  result.SetValue(aTimeStamp
                  - timing->GetNavigationStartTimeStamp()
                  - mOriginTime);
  return result;
}

void
DocumentTimeline::NotifyAnimationUpdated(Animation& aAnimation)
{
  AnimationTimeline::NotifyAnimationUpdated(aAnimation);

  if (!mIsObservingRefreshDriver) {
    nsRefreshDriver* refreshDriver = GetRefreshDriver();
    if (refreshDriver) {
      refreshDriver->AddRefreshObserver(this, Flush_Style);
      mIsObservingRefreshDriver = true;
    }
  }
}

void
DocumentTimeline::WillRefresh(mozilla::TimeStamp aTime)
{
  MOZ_ASSERT(mIsObservingRefreshDriver);
  MOZ_ASSERT(GetRefreshDriver(),
             "Should be able to reach refresh driver from within WillRefresh");

  bool needsTicks = false;
  nsTArray<Animation*> animationsToRemove(mAnimations.Count());

  nsAutoAnimationMutationBatch mb(mDocument);

  for (Animation* animation = mAnimationOrder.getFirst(); animation;
       animation = animation->getNext()) {
    // Skip any animations that are longer need associated with this timeline.
    if (animation->GetTimeline() != this) {
      // If animation has some other timeline, it better not be also in the
      // animation list of this timeline object!
      MOZ_ASSERT(!animation->GetTimeline());
      animationsToRemove.AppendElement(animation);
      continue;
    }

    needsTicks |= animation->NeedsTicks();
    // Even if |animation| doesn't need future ticks, we should still
    // Tick it this time around since it might just need a one-off tick in
    // order to dispatch events.
    animation->Tick();

    if (!animation->IsRelevant() && !animation->NeedsTicks()) {
      animationsToRemove.AppendElement(animation);
    }
  }

  for (Animation* animation : animationsToRemove) {
    RemoveAnimation(animation);
  }

  if (!needsTicks) {
    // We already assert that GetRefreshDriver() is non-null at the beginning
    // of this function but we check it again here to be sure that ticking
    // animations does not have any side effects that cause us to lose the
    // connection with the refresh driver, such as triggering the destruction
    // of mDocument's PresShell.
    MOZ_ASSERT(GetRefreshDriver(),
               "Refresh driver should still be valid at end of WillRefresh");
    UnregisterFromRefreshDriver();
  }
}

void
DocumentTimeline::NotifyRefreshDriverCreated(nsRefreshDriver* aDriver)
{
  MOZ_ASSERT(!mIsObservingRefreshDriver,
             "Timeline should not be observing the refresh driver before"
             " it is created");

  if (!mAnimationOrder.isEmpty()) {
    aDriver->AddRefreshObserver(this, Flush_Style);
    mIsObservingRefreshDriver = true;
  }
}

void
DocumentTimeline::NotifyRefreshDriverDestroying(nsRefreshDriver* aDriver)
{
  if (!mIsObservingRefreshDriver) {
    return;
  }

  aDriver->RemoveRefreshObserver(this, Flush_Style);
  mIsObservingRefreshDriver = false;
}

void
DocumentTimeline::RemoveAnimation(Animation* aAnimation)
{
  AnimationTimeline::RemoveAnimation(aAnimation);

  if (mIsObservingRefreshDriver && mAnimations.IsEmpty()) {
    UnregisterFromRefreshDriver();
  }
}

TimeStamp
DocumentTimeline::ToTimeStamp(const TimeDuration& aTimeDuration) const
{
  TimeStamp result;
  RefPtr<nsDOMNavigationTiming> timing = mDocument->GetNavigationTiming();
  if (MOZ_UNLIKELY(!timing)) {
    return result;
  }

  result =
    timing->GetNavigationStartTimeStamp() + (aTimeDuration + mOriginTime);
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

void
DocumentTimeline::UnregisterFromRefreshDriver()
{
  if (!mIsObservingRefreshDriver) {
    return;
  }

  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (!refreshDriver) {
    return;
  }

  refreshDriver->RemoveRefreshObserver(this, Flush_Style);
  mIsObservingRefreshDriver = false;
}

} // namespace dom
} // namespace mozilla
