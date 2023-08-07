/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AnimationEventDispatcher.h"

#include "mozilla/EventDispatcher.h"
#include "nsPresContext.h"
#include "nsRefreshDriver.h"

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION_CLASS(AnimationEventDispatcher)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AnimationEventDispatcher)
  tmp->ClearEventQueue();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AnimationEventDispatcher)
  for (auto& info : tmp->mPendingEvents) {
    if (OwningAnimationTarget* target = info.GetOwningAnimationTarget()) {
      ImplCycleCollectionTraverse(
          cb, target->mElement,
          "mozilla::AnimationEventDispatcher.mPendingEvents.mTarget");
    } else {
      ImplCycleCollectionTraverse(
          cb, info.mData.as<AnimationEventInfo::WebAnimationData>().mEvent,
          "mozilla::AnimationEventDispatcher.mPendingEvents.mEvent");
    }
    ImplCycleCollectionTraverse(
        cb, info.mAnimation,
        "mozilla::AnimationEventDispatcher.mPendingEvents.mAnimation");
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void AnimationEventDispatcher::Disconnect() {
  if (mIsObserving) {
    MOZ_ASSERT(mPresContext && mPresContext->RefreshDriver(),
               "The pres context and the refresh driver should be still "
               "alive if we haven't disassociated from the refresh driver");
    mPresContext->RefreshDriver()->CancelPendingAnimationEvents(this);
    mIsObserving = false;
  }
  mPresContext = nullptr;
}

void AnimationEventDispatcher::QueueEvent(AnimationEventInfo&& aEvent) {
  mPendingEvents.AppendElement(std::move(aEvent));
  mIsSorted = false;
  ScheduleDispatch();
}

void AnimationEventDispatcher::QueueEvents(
    nsTArray<AnimationEventInfo>&& aEvents) {
  mPendingEvents.AppendElements(std::move(aEvents));
  mIsSorted = false;
  ScheduleDispatch();
}

void AnimationEventDispatcher::ScheduleDispatch() {
  MOZ_ASSERT(mPresContext, "The pres context should be valid");
  if (!mIsObserving) {
    mPresContext->RefreshDriver()->ScheduleAnimationEventDispatch(this);
    mIsObserving = true;
  }
}

void AnimationEventInfo::MaybeAddMarker() const {
  if (mData.is<CssAnimationData>()) {
    const auto& data = mData.as<CssAnimationData>();
    const EventMessage message = data.mMessage;
    if (message != eAnimationCancel && message != eAnimationEnd &&
        message != eAnimationIteration) {
      return;
    }
    nsAutoCString name;
    data.mAnimationName->ToUTF8String(name);
    const TimeStamp startTime = [&] {
      if (message == eAnimationIteration) {
        if (auto* effect = mAnimation->GetEffect()) {
          return mScheduledEventTimeStamp -
                 TimeDuration(effect->GetComputedTiming().mDuration);
        }
      }
      return mScheduledEventTimeStamp -
             TimeDuration::FromSeconds(data.mElapsedTime);
    }();

    nsCSSPropertyIDSet propertySet;
    nsAutoString target;
    if (dom::AnimationEffect* effect = mAnimation->GetEffect()) {
      if (dom::KeyframeEffect* keyFrameEffect = effect->AsKeyframeEffect()) {
        keyFrameEffect->GetTarget()->Describe(target, true);
        for (const AnimationProperty& property : keyFrameEffect->Properties()) {
          propertySet.AddProperty(property.mProperty);
        }
      }
    }
    PROFILER_MARKER(
        message == eAnimationIteration
            ? ProfilerString8View("CSS animation iteration")
            : ProfilerString8View("CSS animation"),
        DOM,
        MarkerOptions(
            MarkerTiming::Interval(startTime, mScheduledEventTimeStamp),
            mAnimation->GetOwner()
                ? MarkerInnerWindowId(mAnimation->GetOwner()->WindowID())
                : MarkerInnerWindowId::NoId()),
        CSSAnimationMarker, name, NS_ConvertUTF16toUTF8(target), propertySet);
    return;
  }

  if (!mData.is<CssTransitionData>()) {
    return;
  }

  const auto& data = mData.as<CssTransitionData>();
  const EventMessage message = data.mMessage;
  if (message != eTransitionEnd && message != eTransitionCancel) {
    return;
  }

  nsAutoString target;
  if (dom::AnimationEffect* effect = mAnimation->GetEffect()) {
    if (dom::KeyframeEffect* keyFrameEffect = effect->AsKeyframeEffect()) {
      keyFrameEffect->GetTarget()->Describe(target, true);
    }
  }
  PROFILER_MARKER(
      "CSS transition", DOM,
      MarkerOptions(
          MarkerTiming::Interval(
              mScheduledEventTimeStamp -
                  TimeDuration::FromSeconds(data.mElapsedTime),
              mScheduledEventTimeStamp),
          mAnimation->GetOwner()
              ? MarkerInnerWindowId(mAnimation->GetOwner()->WindowID())
              : MarkerInnerWindowId::NoId()),
      CSSTransitionMarker, NS_ConvertUTF16toUTF8(target), data.mPropertyId,
      message == eTransitionCancel);
}

}  // namespace mozilla
