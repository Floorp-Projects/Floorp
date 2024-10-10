/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocumentTimeline.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/DocumentTimelineBinding.h"
#include "AnimationUtils.h"
#include "nsContentUtils.h"
#include "nsDOMMutationObserver.h"
#include "nsDOMNavigationTiming.h"
#include "nsPresContext.h"
#include "nsRefreshDriver.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(DocumentTimeline)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DocumentTimeline,
                                                AnimationTimeline)
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

DocumentTimeline::DocumentTimeline(Document* aDocument,
                                   const TimeDuration& aOriginTime)
    : AnimationTimeline(aDocument->GetParentObject(),
                        aDocument->GetScopeObject()->GetRTPCallerType()),
      mDocument(aDocument),
      mOriginTime(aOriginTime) {
  if (mDocument) {
    mDocument->Timelines().insertBack(this);
  }
  // Ensure mLastRefreshDriverTime is valid.
  UpdateLastRefreshDriverTime();
}

DocumentTimeline::~DocumentTimeline() {
  if (isInList()) {
    remove();
  }
}

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

bool DocumentTimeline::TracksWallclockTime() const {
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  return !refreshDriver || !refreshDriver->IsTestControllingRefreshesEnabled();
}

TimeStamp DocumentTimeline::GetCurrentTimeStamp() const {
  nsRefreshDriver* refreshDriver = GetRefreshDriver();
  return refreshDriver ? refreshDriver->MostRecentRefresh()
                       : mLastRefreshDriverTime;
}

void DocumentTimeline::UpdateLastRefreshDriverTime() {
  TimeStamp result = [&] {
    if (auto* rd = GetRefreshDriver()) {
      return rd->MostRecentRefresh();
    };
    return mLastRefreshDriverTime;
  }();

  if (nsDOMNavigationTiming* timing = mDocument->GetNavigationTiming()) {
    // If we don't have a refresh driver and we've never had one use the
    // timeline's zero time.
    // In addition, it's possible that our refresh driver's timestamp is behind
    // from the navigation start time because the refresh driver timestamp is
    // sent through an IPC call whereas the navigation time is set by calling
    // TimeStamp::Now() directly. In such cases we also use the timeline's zero
    // time.
    // Also, let this time represent the current refresh time. This way we'll
    // save it as the last refresh time and skip looking up navigation start
    // time each time.
    if (result.IsNull() || result < timing->GetNavigationStartTimeStamp()) {
      result = timing->GetNavigationStartTimeStamp();
    }
  }

  if (!result.IsNull()) {
    mLastRefreshDriverTime = result;
  }
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

  if (!mAnimationOrder.isEmpty()) {
    if (nsRefreshDriver* refreshDriver = GetRefreshDriver()) {
      MOZ_ASSERT(isInList(),
                 "We should not register with the refresh driver if we are not"
                 " in the document's list of timelines");
      refreshDriver->EnsureAnimationUpdate();
    }
  }
}

void DocumentTimeline::TriggerAllPendingAnimationsNow() {
  AutoTArray<RefPtr<Animation>, 32> animationsToTrigger;
  for (Animation* animation : mAnimationOrder) {
    animationsToTrigger.AppendElement(animation);
  }

  for (Animation* animation : animationsToTrigger) {
    animation->TryTriggerNow();
  }
}

void DocumentTimeline::WillRefresh() {
  if (!mDocument->GetPresShell()) {
    // If we're not displayed, don't tick animations.
    return;
  }
  UpdateLastRefreshDriverTime();
  if (mAnimationOrder.isEmpty()) {
    return;
  }
  nsAutoAnimationMutationBatch mb(mDocument);

  TickState state;
  bool ticked = Tick(state);
  if (!ticked) {
    return;
  }
  // We already assert that GetRefreshDriver() is non-null at the beginning
  // of this function but we check it again here to be sure that ticking
  // animations does not have any side effects that cause us to lose the
  // connection with the refresh driver, such as triggering the destruction
  // of mDocument's PresShell.
  if (nsRefreshDriver* refreshDriver = GetRefreshDriver()) {
    refreshDriver->EnsureAnimationUpdate();
  }
}

void DocumentTimeline::RemoveAnimation(Animation* aAnimation) {
  AnimationTimeline::RemoveAnimation(aAnimation);
}

void DocumentTimeline::NotifyAnimationContentVisibilityChanged(
    Animation* aAnimation, bool aIsVisible) {
  AnimationTimeline::NotifyAnimationContentVisibilityChanged(aAnimation,
                                                             aIsVisible);

  if (nsRefreshDriver* refreshDriver = GetRefreshDriver()) {
    MOZ_ASSERT(isInList(),
               "We should not register with the refresh driver if we are not"
               " in the document's list of timelines");
    refreshDriver->EnsureAnimationUpdate();
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

}  // namespace mozilla::dom
