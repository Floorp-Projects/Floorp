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
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Variant.h"
#include "mozilla/dom/AnimationPlaybackEvent.h"
#include "mozilla/ProfilerMarkers.h"
#include "nsCSSProps.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPresContext.h"

class nsRefreshDriver;

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

#ifdef MOZ_GECKO_PROFILER
    if (aMessage == eAnimationCancel && profiler_can_accept_markers()) {
      nsCString markerText;
      aAnimationName->ToUTF8String(markerText);
      PROFILER_MARKER_TEXT(
          "CSS animation", DOM,
          MarkerTiming::Interval(aScheduledEventTimeStamp -
                                     TimeDuration::FromSeconds(aElapsedTime),
                                 aScheduledEventTimeStamp),
          markerText);
    }
#endif
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

#ifdef MOZ_GECKO_PROFILER
    if ((aMessage == eTransitionEnd || aMessage == eTransitionCancel) &&
        profiler_can_accept_markers()) {
      nsCString markerText;
      markerText.Assign(nsCSSProps::GetStringValue(aProperty));
      if (aMessage == eTransitionCancel) {
        markerText.AppendLiteral(" (canceled)");
      }
      PROFILER_MARKER_TEXT(
          "CSS transition", DOM,
          MarkerTiming::Interval(aScheduledEventTimeStamp -
                                     TimeDuration::FromSeconds(aElapsedTime),
                                 aScheduledEventTimeStamp),
          markerText);
    }
#endif
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

  void Dispatch(nsPresContext* aPresContext) {
    if (mEvent.is<RefPtr<dom::AnimationPlaybackEvent>>()) {
      EventDispatcher::DispatchDOMEvent(
          mTarget, nullptr /* WidgetEvent */,
          mEvent.as<RefPtr<dom::AnimationPlaybackEvent>>(), aPresContext,
          nullptr /* nsEventStatus */);
      return;
    }

    MOZ_ASSERT(mEvent.is<InternalTransitionEvent>() ||
               mEvent.is<InternalAnimationEvent>());

    EventDispatcher::Dispatch(mTarget, aPresContext, AsWidgetEvent());
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
      pending.mAnimation->CachedChildIndexRef() = -1;
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
