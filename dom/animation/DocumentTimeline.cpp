/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentTimeline.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/DocumentTimelineBinding.h"
#include "AnimationUtils.h"
#include "nsContentUtils.h"
#include "nsDOMMutationObserver.h"
#include "nsDOMNavigationTiming.h"
#include "nsPresContext.h"
#include "nsRefreshDriver.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(DocumentTimeline)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DocumentTimeline,
                                                AnimationTimeline)
  tmp->UnregisterFromRefreshDriver();
  if (tmp->isInList()) {
    tmp->remove();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DocumentTimeline,
                                                  AnimationTimeline)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(DocumentTimeline,
                                               AnimationTimeline)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DocumentTimeline)
NS_INTERFACE_MAP_END_INHERITING(AnimationTimeline)

NS_IMPL_ADDREF_INHERITED(DocumentTimeline, AnimationTimeline)
NS_IMPL_RELEASE_INHERITED(DocumentTimeline, AnimationTimeline)

JSObject* DocumentTimeline::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return DocumentTimeline_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<DocumentTimeline> DocumentTimeline::Constructor(
    const GlobalObject& aGlobal, const DocumentTimelineOptions& aOptions,
    ErrorResult& aRv) {
  Document* doc = AnimationUtils::GetCurrentRealmDocument(aGlobal.Context());
  if (!doc) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  TimeDuration originTime =
      TimeDuration::FromMilliseconds(aOptions.mOriginTime);

  if (originTime == TimeDuration::Forever() ||
      originTime == -TimeDuration::Forever()) {
    aRv.ThrowTypeError<dom::MSG_TIME_VALUE_OUT_OF_RANGE>("Origin time");
    return nullptr;
  }
  RefPtr<DocumentTimeline> timeline = new DocumentTimeline(doc, originTime);

  return timeline.forget();
}

Nullable<TimeDuration> DocumentTimeline::GetCurrentTimeAsDuration() const {
  return ToTimelineTime(GetCurrentTimeStamp());
}

TimeStamp DocumentTimeline::GetCurrentTimeStamp() const {
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  TimeStamp refreshTime =
      refreshDriver ? refreshDriver->MostRecentRefresh() : TimeStamp();

  // Always return the same object to benefit from return-value optimization.
  TimeStamp result =
      !refreshTime.IsNull() ? refreshTime : mLastRefreshDriverTime;

  nsDOMNavigationTiming* timing = mDocument->GetNavigationTiming();
  // If we don't have a refresh driver and we've never had one use the
  // timeline's zero time.
  // In addition, it's possible that our refresh driver's timestamp is behind
  // from the navigation start time because the refresh driver timestamp is
  // sent through an IPC call whereas the navigation time is set by calling
  // TimeStamp::Now() directly. In such cases we also use the timeline's zero
  // time.
  if (timing &&
      (result.IsNull() || result < timing->GetNavigationStartTimeStamp())) {
    result = timing->GetNavigationStartTimeStamp();
    // Also, let this time represent the current refresh time. This way
    // we'll save it as the last refresh time and skip looking up
    // navigation start time each time.
    refreshTime = result;
  }

  if (!refreshTime.IsNull()) {
    mLastRefreshDriverTime = refreshTime;
  }

  return result;
}

Nullable<TimeDuration> DocumentTimeline::ToTimelineTime(
    const TimeStamp& aTimeStamp) const {
  Nullable<TimeDuration> result;  // Initializes to null
  if (aTimeStamp.IsNull()) {
    return result;
  }

  nsDOMNavigationTiming* timing = mDocument->GetNavigationTiming();
  if (MOZ_UNLIKELY(!timing)) {
    return result;
  }

  result.SetValue(aTimeStamp - timing->GetNavigationStartTimeStamp() -
                  mOriginTime);
  return result;
}

void DocumentTimeline::NotifyAnimationUpdated(Animation& aAnimation) {
  AnimationTimeline::NotifyAnimationUpdated(aAnimation);

  if (!mIsObservingRefreshDriver) {
    nsRefreshDriver* refreshDriver = GetRefreshDriver();
    if (refreshDriver) {
      MOZ_ASSERT(isInList(),
                 "We should not register with the refresh driver if we are not"
                 " in the document's list of timelines");

      ObserveRefreshDriver(refreshDriver);
    }
  }
}

void DocumentTimeline::MostRecentRefreshTimeUpdated() {
  MOZ_ASSERT(mIsObservingRefreshDriver);
  MOZ_ASSERT(GetRefreshDriver(),
             "Should be able to reach refresh driver from within WillRefresh");

  bool needsTicks = false;
  nsTArray<Animation*> animationsToRemove(mAnimations.Count());

  nsAutoAnimationMutationBatch mb(mDocument);

  for (Animation* animation = mAnimationOrder.getFirst(); animation;
       animation =
           static_cast<LinkedListElement<Animation>*>(animation)->getNext()) {
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

    if (!animation->NeedsTicks()) {
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

void DocumentTimeline::WillRefresh(mozilla::TimeStamp aTime) {
  MostRecentRefreshTimeUpdated();
}

void DocumentTimeline::NotifyTimerAdjusted(TimeStamp aTime) {
  MostRecentRefreshTimeUpdated();
}

void DocumentTimeline::ObserveRefreshDriver(nsRefreshDriver* aDriver) {
  MOZ_ASSERT(!mIsObservingRefreshDriver);
  // Set the mIsObservingRefreshDriver flag before calling AddRefreshObserver
  // since it might end up calling NotifyTimerAdjusted which calls
  // MostRecentRefreshTimeUpdated which has an assertion for
  // mIsObserveingRefreshDriver check.
  mIsObservingRefreshDriver = true;
  aDriver->AddRefreshObserver(this, FlushType::Style);
  aDriver->AddTimerAdjustmentObserver(this);
}

void DocumentTimeline::NotifyRefreshDriverCreated(nsRefreshDriver* aDriver) {
  MOZ_ASSERT(!mIsObservingRefreshDriver,
             "Timeline should not be observing the refresh driver before"
             " it is created");

  if (!mAnimationOrder.isEmpty()) {
    MOZ_ASSERT(isInList(),
               "We should not register with the refresh driver if we are not"
               " in the document's list of timelines");
    ObserveRefreshDriver(aDriver);
    // Although we have started observing the refresh driver, it's possible we
    // could perform a paint before the first refresh driver tick happens.  To
    // ensure we're in a consistent state in that case we run the first tick
    // manually.
    MostRecentRefreshTimeUpdated();
  }
}

void DocumentTimeline::DisconnectRefreshDriver(nsRefreshDriver* aDriver) {
  MOZ_ASSERT(mIsObservingRefreshDriver);

  aDriver->RemoveRefreshObserver(this, FlushType::Style);
  aDriver->RemoveTimerAdjustmentObserver(this);
  mIsObservingRefreshDriver = false;
}

void DocumentTimeline::NotifyRefreshDriverDestroying(nsRefreshDriver* aDriver) {
  if (!mIsObservingRefreshDriver) {
    return;
  }

  DisconnectRefreshDriver(aDriver);
}

void DocumentTimeline::RemoveAnimation(Animation* aAnimation) {
  AnimationTimeline::RemoveAnimation(aAnimation);

  if (mIsObservingRefreshDriver && mAnimations.IsEmpty()) {
    UnregisterFromRefreshDriver();
  }
}

TimeStamp DocumentTimeline::ToTimeStamp(
    const TimeDuration& aTimeDuration) const {
  TimeStamp result;
  nsDOMNavigationTiming* timing = mDocument->GetNavigationTiming();
  if (MOZ_UNLIKELY(!timing)) {
    return result;
  }

  result =
      timing->GetNavigationStartTimeStamp() + (aTimeDuration + mOriginTime);
  return result;
}

nsRefreshDriver* DocumentTimeline::GetRefreshDriver() const {
  nsPresContext* presContext = mDocument->GetPresContext();
  if (MOZ_UNLIKELY(!presContext)) {
    return nullptr;
  }

  return presContext->RefreshDriver();
}

void DocumentTimeline::UnregisterFromRefreshDriver() {
  if (!mIsObservingRefreshDriver) {
    return;
  }

  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  if (!refreshDriver) {
    return;
  }
  DisconnectRefreshDriver(refreshDriver);
}

}  // namespace dom
}  // namespace mozilla
