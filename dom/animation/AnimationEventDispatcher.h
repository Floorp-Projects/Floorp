/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AnimationEventDispatcher_h
#define mozilla_AnimationEventDispatcher_h

#include <algorithm>  // For <std::stable_sort>
#include "mozilla/AnimationComparator.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Variant.h"
#include "mozilla/dom/AnimationEffect.h"
#include "mozilla/dom/AnimationPlaybackEvent.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/ProfilerMarkers.h"
#include "nsCSSProps.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPresContext.h"

class nsRefreshDriver;

namespace geckoprofiler::markers {

using namespace mozilla;

struct CSSAnimationMarker {
  static constexpr Span<const char> MarkerTypeName() {
    return MakeStringSpan("CSSAnimation");
  }
  static void StreamJSONMarkerData(baseprofiler::SpliceableJSONWriter& aWriter,
                                   const nsCString& aName,
                                   const nsCString& aTarget,
                                   nsCSSPropertyIDSet aPropertySet) {
    aWriter.StringProperty("Name", aName);
    aWriter.StringProperty("Target", aTarget);
    nsAutoCString properties;
    nsAutoCString oncompositor;
    for (nsCSSPropertyID property : aPropertySet) {
      if (!properties.IsEmpty()) {
        properties.AppendLiteral(", ");
        oncompositor.AppendLiteral(", ");
      }
      properties.Append(nsCSSProps::GetStringValue(property));
      oncompositor.Append(nsCSSProps::PropHasFlags(
                              property, CSSPropFlags::CanAnimateOnCompositor)
                              ? "true"
                              : "false");
    }

    aWriter.StringProperty("properties", properties);
    aWriter.StringProperty("oncompositor", oncompositor);
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
                                   nsCSSPropertyID aProperty, bool aCanceled) {
    aWriter.StringProperty("Target", aTarget);
    aWriter.StringProperty("property", nsCSSProps::GetStringValue(aProperty));
    aWriter.BoolProperty("oncompositor",
                         nsCSSProps::PropHasFlags(
                             aProperty, CSSPropFlags::CanAnimateOnCompositor));
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

struct AnimationEventInfo {
  RefPtr<dom::EventTarget> mTarget;
  RefPtr<dom::Animation> mAnimation;
  TimeStamp mScheduledEventTimeStamp;

  typedef Variant<InternalTransitionEvent, InternalAnimationEvent,
                  RefPtr<dom::AnimationPlaybackEvent>>
      EventVariant;
  EventVariant mEvent;

  // For CSS animation events
  AnimationEventInfo(nsAtom* aAnimationName,
                     const NonOwningAnimationTarget& aTarget,
                     EventMessage aMessage, double aElapsedTime,
                     const TimeStamp& aScheduledEventTimeStamp,
                     dom::Animation* aAnimation)
      : mTarget(aTarget.mElement),
        mAnimation(aAnimation),
        mScheduledEventTimeStamp(aScheduledEventTimeStamp),
        mEvent(EventVariant(InternalAnimationEvent(true, aMessage))) {
    InternalAnimationEvent& event = mEvent.as<InternalAnimationEvent>();

    aAnimationName->ToString(event.mAnimationName);
    // XXX Looks like nobody initialize WidgetEvent::time
    event.mElapsedTime = aElapsedTime;
    event.mPseudoElement =
        nsCSSPseudoElements::PseudoTypeAsString(aTarget.mPseudoType);

    if ((aMessage == eAnimationCancel || aMessage == eAnimationEnd ||
         aMessage == eAnimationIteration) &&
        profiler_thread_is_being_profiled_for_markers()) {
      nsAutoCString name;
      aAnimationName->ToUTF8String(name);

      const TimeStamp startTime = [&] {
        if (aMessage == eAnimationIteration) {
          if (auto* effect = aAnimation->GetEffect()) {
            return aScheduledEventTimeStamp -
                   TimeDuration(effect->GetComputedTiming().mDuration);
          }
        }
        return aScheduledEventTimeStamp -
               TimeDuration::FromSeconds(aElapsedTime);
      }();

      nsCSSPropertyIDSet propertySet;
      nsAutoString target;
      if (dom::AnimationEffect* effect = aAnimation->GetEffect()) {
        if (dom::KeyframeEffect* keyFrameEffect = effect->AsKeyframeEffect()) {
          keyFrameEffect->GetTarget()->Describe(target, true);
          for (const AnimationProperty& property :
               keyFrameEffect->Properties()) {
            propertySet.AddProperty(property.mProperty);
          }
        }
      }

      PROFILER_MARKER(
          aMessage == eAnimationIteration
              ? ProfilerString8View("CSS animation iteration")
              : ProfilerString8View("CSS animation"),
          DOM,
          MarkerOptions(
              MarkerTiming::Interval(startTime, aScheduledEventTimeStamp),
              aAnimation->GetOwner()
                  ? MarkerInnerWindowId(aAnimation->GetOwner()->WindowID())
                  : MarkerInnerWindowId::NoId()),
          CSSAnimationMarker, name, NS_ConvertUTF16toUTF8(target), propertySet);
    }
  }

  // For CSS transition events
  AnimationEventInfo(nsCSSPropertyID aProperty,
                     const NonOwningAnimationTarget& aTarget,
                     EventMessage aMessage, double aElapsedTime,
                     const TimeStamp& aScheduledEventTimeStamp,
                     dom::Animation* aAnimation)
      : mTarget(aTarget.mElement),
        mAnimation(aAnimation),
        mScheduledEventTimeStamp(aScheduledEventTimeStamp),
        mEvent(EventVariant(InternalTransitionEvent(true, aMessage))) {
    InternalTransitionEvent& event = mEvent.as<InternalTransitionEvent>();

    event.mPropertyName =
        NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(aProperty));
    // XXX Looks like nobody initialize WidgetEvent::time
    event.mElapsedTime = aElapsedTime;
    event.mPseudoElement =
        nsCSSPseudoElements::PseudoTypeAsString(aTarget.mPseudoType);

    if ((aMessage == eTransitionEnd || aMessage == eTransitionCancel) &&
        profiler_thread_is_being_profiled_for_markers()) {
      nsAutoString target;
      if (dom::AnimationEffect* effect = aAnimation->GetEffect()) {
        if (dom::KeyframeEffect* keyFrameEffect = effect->AsKeyframeEffect()) {
          keyFrameEffect->GetTarget()->Describe(target, true);
        }
      }
      PROFILER_MARKER(
          "CSS transition", DOM,
          MarkerOptions(
              MarkerTiming::Interval(
                  aScheduledEventTimeStamp -
                      TimeDuration::FromSeconds(aElapsedTime),
                  aScheduledEventTimeStamp),
              aAnimation->GetOwner()
                  ? MarkerInnerWindowId(aAnimation->GetOwner()->WindowID())
                  : MarkerInnerWindowId::NoId()),
          CSSTransitionMarker, NS_ConvertUTF16toUTF8(target), aProperty,
          aMessage == eTransitionCancel);
    }
  }

  // For web animation events
  AnimationEventInfo(const nsAString& aName,
                     RefPtr<dom::AnimationPlaybackEvent>&& aEvent,
                     TimeStamp&& aScheduledEventTimeStamp,
                     dom::Animation* aAnimation)
      : mTarget(aAnimation),
        mAnimation(aAnimation),
        mScheduledEventTimeStamp(std::move(aScheduledEventTimeStamp)),
        mEvent(std::move(aEvent)) {}

  AnimationEventInfo(const AnimationEventInfo& aOther) = delete;
  AnimationEventInfo& operator=(const AnimationEventInfo& aOther) = delete;
  AnimationEventInfo(AnimationEventInfo&& aOther) = default;
  AnimationEventInfo& operator=(AnimationEventInfo&& aOther) = default;

  bool IsWebAnimationEvent() const {
    return mEvent.is<RefPtr<dom::AnimationPlaybackEvent>>();
  }

#ifdef DEBUG
  bool IsStale() const {
    const WidgetEvent* widgetEvent = AsWidgetEvent();
    return widgetEvent->mFlags.mIsBeingDispatched ||
           widgetEvent->mFlags.mDispatchedAtLeastOnce;
  }

  const WidgetEvent* AsWidgetEvent() const {
    return const_cast<AnimationEventInfo*>(this)->AsWidgetEvent();
  }
#endif

  WidgetEvent* AsWidgetEvent() {
    if (mEvent.is<InternalTransitionEvent>()) {
      return &mEvent.as<InternalTransitionEvent>();
    }
    if (mEvent.is<InternalAnimationEvent>()) {
      return &mEvent.as<InternalAnimationEvent>();
    }
    if (mEvent.is<RefPtr<dom::AnimationPlaybackEvent>>()) {
      return mEvent.as<RefPtr<dom::AnimationPlaybackEvent>>()->WidgetEventPtr();
    }

    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unexpected event type");
    return nullptr;
  }

  // TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Dispatch(nsPresContext* aPresContext) {
    RefPtr<dom::EventTarget> target = mTarget;
    if (mEvent.is<RefPtr<dom::AnimationPlaybackEvent>>()) {
      auto playbackEvent = mEvent.as<RefPtr<dom::AnimationPlaybackEvent>>();
      EventDispatcher::DispatchDOMEvent(target, nullptr /* WidgetEvent */,
                                        playbackEvent, aPresContext,
                                        nullptr /* nsEventStatus */);
      return;
    }

    MOZ_ASSERT(mEvent.is<InternalTransitionEvent>() ||
               mEvent.is<InternalAnimationEvent>());

    if (mEvent.is<InternalTransitionEvent>() && target->IsNode()) {
      nsPIDOMWindowInner* inner =
          target->AsNode()->OwnerDoc()->GetInnerWindow();
      if (inner && !inner->HasTransitionEventListeners()) {
        MOZ_ASSERT(AsWidgetEvent()->mMessage == eTransitionStart ||
                   AsWidgetEvent()->mMessage == eTransitionRun ||
                   AsWidgetEvent()->mMessage == eTransitionEnd ||
                   AsWidgetEvent()->mMessage == eTransitionCancel);
        return;
      }
    }

    EventDispatcher::Dispatch(target, aPresContext, AsWidgetEvent());
  }
};

class AnimationEventDispatcher final {
 public:
  explicit AnimationEventDispatcher(nsPresContext* aPresContext)
      : mPresContext(aPresContext), mIsSorted(true), mIsObserving(false) {}

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnimationEventDispatcher)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(AnimationEventDispatcher)

  void Disconnect();

  void QueueEvent(AnimationEventInfo&& aEvent);
  void QueueEvents(nsTArray<AnimationEventInfo>&& aEvents);

  // This will call SortEvents automatically if it has not already been
  // called.
  void DispatchEvents() {
    mIsObserving = false;
    if (!mPresContext || mPendingEvents.IsEmpty()) {
      return;
    }

    SortEvents();

    EventArray events = std::move(mPendingEvents);
    // mIsSorted will be set to true by SortEvents above, and we leave it
    // that way since mPendingEvents is now empty
    for (AnimationEventInfo& info : events) {
      MOZ_ASSERT(!info.IsStale(), "The event shouldn't be stale");
      info.Dispatch(mPresContext);

      // Bail out if our mPresContext was nullified due to destroying the pres
      // context.
      if (!mPresContext) {
        break;
      }
    }
  }

  void ClearEventQueue() {
    mPendingEvents.Clear();
    mIsSorted = true;
  }
  bool HasQueuedEvents() const { return !mPendingEvents.IsEmpty(); }

 private:
#ifndef DEBUG
  ~AnimationEventDispatcher() = default;
#else
  ~AnimationEventDispatcher() {
    MOZ_ASSERT(!mIsObserving,
               "AnimationEventDispatcher should have disassociated from "
               "nsRefreshDriver");
  }
#endif

  class AnimationEventInfoLessThan {
   public:
    bool operator()(const AnimationEventInfo& a,
                    const AnimationEventInfo& b) const {
      if (a.mScheduledEventTimeStamp != b.mScheduledEventTimeStamp) {
        // Null timestamps sort first
        if (a.mScheduledEventTimeStamp.IsNull() ||
            b.mScheduledEventTimeStamp.IsNull()) {
          return a.mScheduledEventTimeStamp.IsNull();
        } else {
          return a.mScheduledEventTimeStamp < b.mScheduledEventTimeStamp;
        }
      }

      // Events in the Web Animations spec are prior to CSS events.
      if (a.IsWebAnimationEvent() != b.IsWebAnimationEvent()) {
        return a.IsWebAnimationEvent();
      }

      AnimationPtrComparator<RefPtr<dom::Animation>> comparator;
      return comparator.LessThan(a.mAnimation, b.mAnimation);
    }
  };

  // Sort all pending CSS animation/transition events by scheduled event time
  // and composite order.
  // https://drafts.csswg.org/web-animations/#update-animations-and-send-events
  void SortEvents() {
    if (mIsSorted) {
      return;
    }

    for (auto& pending : mPendingEvents) {
      pending.mAnimation->CachedChildIndexRef().reset();
    }

    // FIXME: Replace with mPendingEvents.StableSort when bug 1147091 is
    // fixed.
    std::stable_sort(mPendingEvents.begin(), mPendingEvents.end(),
                     AnimationEventInfoLessThan());
    mIsSorted = true;
  }
  void ScheduleDispatch();

  nsPresContext* mPresContext;
  typedef nsTArray<AnimationEventInfo> EventArray;
  EventArray mPendingEvents;
  bool mIsSorted;
  bool mIsObserving;
};

}  // namespace mozilla

#endif  // mozilla_AnimationEventDispatcher_h
