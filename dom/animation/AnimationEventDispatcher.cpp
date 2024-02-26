/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AnimationEventDispatcher.h"

#include "mozilla/EventDispatcher.h"
#include "nsPresContext.h"
#include "nsRefreshDriver.h"
#include "nsCSSProps.h"
#include "mozilla/dom/AnimationEffect.h"

using namespace mozilla;

namespace geckoprofiler::markers {

struct CSSAnimationMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("CSSAnimation");
  }
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const nsCString& aName,
                                   const nsCString& aTarget,
                                   const nsCString& aProperties,
                                   const nsCString& aOnCompositor) {
    aWriter.StringProperty("Name", aName);
    aWriter.StringProperty("Target", aTarget);
    aWriter.StringProperty("properties", aProperties);
    aWriter.StringProperty("oncompositor", aOnCompositor);
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
    schema.AddKeyFormatSearchable("Name", MS::Format::String,
                                  MS::Searchable::Searchable);
    schema.AddKeyLabelFormat("properties", "Animated Properties",
                             MS::Format::String);
    schema.AddKeyLabelFormat("oncompositor", "Can Run on Compositor",
                             MS::Format::String);
    schema.AddKeyFormat("Target", MS::Format::String);
    schema.SetChartLabel("{marker.data.Name}");
    schema.SetTableLabel(
        "{marker.name} - {marker.data.Name}: {marker.data.properties}");
    return schema;
  }
};

struct CSSTransitionMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("CSSTransition");
  }
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const nsCString& aTarget,
                                   const nsCString& aProperty,
                                   bool aOnCompositor, bool aCanceled) {
    aWriter.StringProperty("Target", aTarget);
    aWriter.StringProperty("property", aProperty);
    aWriter.BoolProperty("oncompositor", aOnCompositor);
    if (aCanceled) {
      aWriter.BoolProperty("Canceled", aCanceled);
    }
  }
  static MarkerSchema MarkerTypeDisplay() {
    using MS = MarkerSchema;
    MS schema{MS::Location::MarkerChart, MS::Location::MarkerTable};
    schema.AddKeyLabelFormat("property", "Animated Property",
                             MS::Format::String);
    schema.AddKeyLabelFormat("oncompositor", "Can Run on Compositor",
                             MS::Format::String);
    schema.AddKeyFormat("Canceled", MS::Format::String);
    schema.AddKeyFormat("Target", MS::Format::String);
    schema.SetChartLabel("{marker.data.property}");
    schema.SetTableLabel("{marker.name} - {marker.data.property}");
    return schema;
  }
};

}  // namespace geckoprofiler::markers

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

    AnimatedPropertyIDSet propertySet;
    nsAutoString target;
    if (dom::AnimationEffect* effect = mAnimation->GetEffect()) {
      if (dom::KeyframeEffect* keyFrameEffect = effect->AsKeyframeEffect()) {
        keyFrameEffect->GetTarget()->Describe(target, true);
        for (const AnimationProperty& property : keyFrameEffect->Properties()) {
          propertySet.AddProperty(property.mProperty);
        }
      }
    }
    nsAutoCString properties;
    nsAutoCString oncompositor;
    for (const AnimatedPropertyID& property : propertySet) {
      if (!properties.IsEmpty()) {
        properties.AppendLiteral(", ");
        oncompositor.AppendLiteral(", ");
      }
      nsAutoCString prop;
      property.ToString(prop);
      properties.Append(prop);
      oncompositor.Append(
          !property.IsCustom() &&
                  nsCSSProps::PropHasFlags(property.mID,
                                           CSSPropFlags::CanAnimateOnCompositor)
              ? "true"
              : "false");
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
        CSSAnimationMarker, name, NS_ConvertUTF16toUTF8(target), properties,
        oncompositor);
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
  nsAutoCString property;
  data.mProperty.ToString(property);

  // FIXME: This doesn't _really_ reflect whether the animation is actually run
  // in the compositor. The effect has that information and we should use it
  // probably.
  const bool onCompositor =
      !data.mProperty.IsCustom() &&
      nsCSSProps::PropHasFlags(data.mProperty.mID,
                               CSSPropFlags::CanAnimateOnCompositor);
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
      CSSTransitionMarker, NS_ConvertUTF16toUTF8(target), property,
      onCompositor, message == eTransitionCancel);
}

}  // namespace mozilla
