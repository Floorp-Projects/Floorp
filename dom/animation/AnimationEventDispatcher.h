/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AnimationEventDispatcher_h
#define mozilla_AnimationEventDispatcher_h

#include "mozilla/AnimationComparator.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/Variant.h"
#include "mozilla/dom/AnimationPlaybackEvent.h"
#include "mozilla/dom/KeyframeEffect.h"
#include "mozilla/ProfilerMarkers.h"
#include "nsCycleCollectionParticipant.h"
#include "nsPresContext.h"

class nsRefreshDriver;

namespace mozilla {

struct AnimationEventInfo {
  struct CssAnimationOrTransitionData {
    OwningAnimationTarget mTarget;
    const EventMessage mMessage;
    const double mElapsedTime;
    // The transition generation or animation relative position in the global
    // animation list. We use this information to determine the order of
    // cancelled transitions or animations. (i.e. We override the animation
    // index of the cancelled transitions/animations because their animation
    // indexes have been changed.)
    const uint64_t mAnimationIndex;
    // FIXME(emilio): is this needed? This preserves behavior from before
    // bug 1847200, but it's unclear what the timeStamp of the event should be.
    // See also https://github.com/w3c/csswg-drafts/issues/9167
    const TimeStamp mEventEnqueueTimeStamp{TimeStamp::Now()};
  };

  struct CssAnimationData : public CssAnimationOrTransitionData {
    const RefPtr<nsAtom> mAnimationName;
  };

  struct CssTransitionData : public CssAnimationOrTransitionData {
    // For transition events only.
    const AnimatedPropertyID mProperty;
  };

  struct WebAnimationData {
    const RefPtr<nsAtom> mOnEvent;
    const dom::Nullable<double> mCurrentTime;
    const dom::Nullable<double> mTimelineTime;
    const TimeStamp mEventEnqueueTimeStamp{TimeStamp::Now()};
  };

  using Data = Variant<CssAnimationData, CssTransitionData, WebAnimationData>;

  RefPtr<dom::Animation> mAnimation;
  TimeStamp mScheduledEventTimeStamp;
  Data mData;

  OwningAnimationTarget* GetOwningAnimationTarget() {
    if (mData.is<CssAnimationData>()) {
      return &mData.as<CssAnimationData>().mTarget;
    }
    if (mData.is<CssTransitionData>()) {
      return &mData.as<CssTransitionData>().mTarget;
    }
    return nullptr;
  }

  // Return the event context if the event is animationcancel or
  // transitioncancel.
  Maybe<dom::Animation::EventContext> GetEventContext() const {
    if (mData.is<CssAnimationData>()) {
      const auto& data = mData.as<CssAnimationData>();
      return data.mMessage == eAnimationCancel
                 ? Some(dom::Animation::EventContext{
                       NonOwningAnimationTarget(data.mTarget),
                       data.mAnimationIndex})
                 : Nothing();
    }

    if (mData.is<CssTransitionData>()) {
      const auto& data = mData.as<CssTransitionData>();
      return data.mMessage == eTransitionCancel
                 ? Some(dom::Animation::EventContext{
                       NonOwningAnimationTarget(data.mTarget),
                       data.mAnimationIndex})
                 : Nothing();
    }

    return Nothing();
  }

  void MaybeAddMarker() const;

  // For CSS animation events
  AnimationEventInfo(RefPtr<nsAtom> aAnimationName,
                     const NonOwningAnimationTarget& aTarget,
                     EventMessage aMessage, double aElapsedTime,
                     uint64_t aAnimationIndex,
                     const TimeStamp& aScheduledEventTimeStamp,
                     dom::Animation* aAnimation)
      : mAnimation(aAnimation),
        mScheduledEventTimeStamp(aScheduledEventTimeStamp),
        mData(CssAnimationData{
            {OwningAnimationTarget(aTarget.mElement, aTarget.mPseudoType),
             aMessage, aElapsedTime, aAnimationIndex},
            std::move(aAnimationName)}) {
    if (profiler_thread_is_being_profiled_for_markers()) {
      MaybeAddMarker();
    }
  }

  // For CSS transition events
  AnimationEventInfo(const AnimatedPropertyID& aProperty,
                     const NonOwningAnimationTarget& aTarget,
                     EventMessage aMessage, double aElapsedTime,
                     uint64_t aTransitionGeneration,
                     const TimeStamp& aScheduledEventTimeStamp,
                     dom::Animation* aAnimation)
      : mAnimation(aAnimation),
        mScheduledEventTimeStamp(aScheduledEventTimeStamp),
        mData(CssTransitionData{
            {OwningAnimationTarget(aTarget.mElement, aTarget.mPseudoType),
             aMessage, aElapsedTime, aTransitionGeneration},
            aProperty}) {
    if (profiler_thread_is_being_profiled_for_markers()) {
      MaybeAddMarker();
    }
  }

  // For web animation events
  AnimationEventInfo(nsAtom* aOnEvent,
                     const dom::Nullable<double>& aCurrentTime,
                     const dom::Nullable<double>& aTimelineTime,
                     TimeStamp&& aScheduledEventTimeStamp,
                     dom::Animation* aAnimation)
      : mAnimation(aAnimation),
        mScheduledEventTimeStamp(std::move(aScheduledEventTimeStamp)),
        mData(WebAnimationData{RefPtr{aOnEvent}, aCurrentTime, aTimelineTime}) {
  }

  AnimationEventInfo(const AnimationEventInfo& aOther) = delete;
  AnimationEventInfo& operator=(const AnimationEventInfo& aOther) = delete;

  AnimationEventInfo(AnimationEventInfo&& aOther) = default;
  AnimationEventInfo& operator=(AnimationEventInfo&& aOther) = default;

  bool operator<(const AnimationEventInfo& aOther) const {
    if (this->mScheduledEventTimeStamp != aOther.mScheduledEventTimeStamp) {
      // Null timestamps sort first
      if (this->mScheduledEventTimeStamp.IsNull() ||
          aOther.mScheduledEventTimeStamp.IsNull()) {
        return this->mScheduledEventTimeStamp.IsNull();
      }
      return this->mScheduledEventTimeStamp < aOther.mScheduledEventTimeStamp;
    }

    // Events in the Web Animations spec are prior to CSS events.
    if (this->IsWebAnimationEvent() != aOther.IsWebAnimationEvent()) {
      return this->IsWebAnimationEvent();
    }

    return mAnimation->HasLowerCompositeOrderThan(
        GetEventContext(), *aOther.mAnimation, aOther.GetEventContext());
  }

  bool IsWebAnimationEvent() const { return mData.is<WebAnimationData>(); }

  // TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Dispatch(nsPresContext* aPresContext) {
    if (mData.is<WebAnimationData>()) {
      const auto& data = mData.as<WebAnimationData>();
      EventListenerManager* elm = mAnimation->GetExistingListenerManager();
      if (!elm || !elm->HasListenersFor(data.mOnEvent)) {
        return;
      }

      dom::AnimationPlaybackEventInit init;
      init.mCurrentTime = data.mCurrentTime;
      init.mTimelineTime = data.mTimelineTime;
      MOZ_ASSERT(nsDependentAtomString(data.mOnEvent).Find(u"on"_ns) == 0,
                 "mOnEvent atom should start with 'on'!");
      RefPtr<dom::AnimationPlaybackEvent> event =
          dom::AnimationPlaybackEvent::Constructor(
              mAnimation, Substring(nsDependentAtomString(data.mOnEvent), 2),
              init);
      event->SetTrusted(true);
      event->WidgetEventPtr()->AssignEventTime(
          WidgetEventTime(data.mEventEnqueueTimeStamp));
      RefPtr target = mAnimation;
      EventDispatcher::DispatchDOMEvent(target, nullptr /* WidgetEvent */,
                                        event, aPresContext,
                                        nullptr /* nsEventStatus */);
      return;
    }

    if (mData.is<CssTransitionData>()) {
      const auto& data = mData.as<CssTransitionData>();
      nsPIDOMWindowInner* win =
          data.mTarget.mElement->OwnerDoc()->GetInnerWindow();
      if (win && !win->HasTransitionEventListeners()) {
        MOZ_ASSERT(data.mMessage == eTransitionStart ||
                   data.mMessage == eTransitionRun ||
                   data.mMessage == eTransitionEnd ||
                   data.mMessage == eTransitionCancel);
        return;
      }

      InternalTransitionEvent event(true, data.mMessage);
      data.mProperty.ToString(event.mPropertyName);
      event.mElapsedTime = data.mElapsedTime;
      event.mPseudoElement =
          nsCSSPseudoElements::PseudoTypeAsString(data.mTarget.mPseudoType);
      event.AssignEventTime(WidgetEventTime(data.mEventEnqueueTimeStamp));
      RefPtr target = data.mTarget.mElement;
      EventDispatcher::Dispatch(target, aPresContext, &event);
      return;
    }

    const auto& data = mData.as<CssAnimationData>();
    InternalAnimationEvent event(true, data.mMessage);
    data.mAnimationName->ToString(event.mAnimationName);
    event.mElapsedTime = data.mElapsedTime;
    event.mPseudoElement =
        nsCSSPseudoElements::PseudoTypeAsString(data.mTarget.mPseudoType);
    event.AssignEventTime(WidgetEventTime(data.mEventEnqueueTimeStamp));
    RefPtr target = data.mTarget.mElement;
    EventDispatcher::Dispatch(target, aPresContext, &event);
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

    mPendingEvents.StableSort();
    mIsSorted = true;
  }
  void ScheduleDispatch();

  nsPresContext* mPresContext;
  using EventArray = nsTArray<AnimationEventInfo>;
  EventArray mPendingEvents;
  bool mIsSorted;
  bool mIsObserving;
};

}  // namespace mozilla

#endif  // mozilla_AnimationEventDispatcher_h
