/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AnimationEventDispatcher_h
#define mozilla_AnimationEventDispatcher_h

#include <algorithm> // For <std::stable_sort>
#include "mozilla/AnimationComparator.h"
#include "mozilla/Assertions.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/Variant.h"
#include "nsCSSProps.h"
#include "nsCycleCollectionParticipant.h"

class nsPresContext;
class nsRefreshDriver;

namespace mozilla {

struct AnimationEventInfo
{
  RefPtr<dom::EventTarget> mTarget;
  RefPtr<dom::Animation> mAnimation;
  TimeStamp mTimeStamp;

  typedef Variant<InternalTransitionEvent, InternalAnimationEvent> EventVariant;
  EventVariant mEvent;

  // For CSS animation events
  AnimationEventInfo(nsAtom* aAnimationName,
                     const NonOwningAnimationTarget& aTarget,
                     EventMessage aMessage,
                     double aElapsedTime,
                     const TimeStamp& aTimeStamp,
                     dom::Animation* aAnimation)
    : mTarget(aTarget.mElement)
    , mAnimation(aAnimation)
    , mTimeStamp(aTimeStamp)
    , mEvent(EventVariant(InternalAnimationEvent(true, aMessage)))
  {
    InternalAnimationEvent& event = mEvent.as<InternalAnimationEvent>();

    aAnimationName->ToString(event.mAnimationName);
    // XXX Looks like nobody initialize WidgetEvent::time
    event.mElapsedTime = aElapsedTime;
    event.mPseudoElement =
      nsCSSPseudoElements::PseudoTypeAsString(aTarget.mPseudoType);
  }

  // For CSS transition events
  AnimationEventInfo(nsCSSPropertyID aProperty,
                     const NonOwningAnimationTarget& aTarget,
                     EventMessage aMessage,
                     double aElapsedTime,
                     const TimeStamp& aTimeStamp,
                     dom::Animation* aAnimation)
    : mTarget(aTarget.mElement)
    , mAnimation(aAnimation)
    , mTimeStamp(aTimeStamp)
    , mEvent(EventVariant(InternalTransitionEvent(true, aMessage)))
  {
    InternalTransitionEvent& event = mEvent.as<InternalTransitionEvent>();

    event.mPropertyName =
      NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(aProperty));
    // XXX Looks like nobody initialize WidgetEvent::time
    event.mElapsedTime = aElapsedTime;
    event.mPseudoElement =
      nsCSSPseudoElements::PseudoTypeAsString(aTarget.mPseudoType);
  }

  AnimationEventInfo(const AnimationEventInfo& aOther) = delete;
  AnimationEventInfo& operator=(const AnimationEventInfo& aOther) = delete;
  AnimationEventInfo(AnimationEventInfo&& aOther) = default;
  AnimationEventInfo& operator=(AnimationEventInfo&& aOther) = default;

  WidgetEvent* AsWidgetEvent()
  {
    if (mEvent.is<InternalTransitionEvent>()) {
      return &mEvent.as<InternalTransitionEvent>();
    }
    if (mEvent.is<InternalAnimationEvent>()) {
      return &mEvent.as<InternalAnimationEvent>();
    }

    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unexpected event type");
    return nullptr;
  }
};

class AnimationEventDispatcher final
{
public:
  explicit AnimationEventDispatcher(nsPresContext* aPresContext)
    : mPresContext(aPresContext)
    , mIsSorted(true)
    , mIsObserving(false)
  {
  }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(AnimationEventDispatcher)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(AnimationEventDispatcher)

  void Disconnect();

  void QueueEvent(AnimationEventInfo&& aEvent);
  void QueueEvents(nsTArray<AnimationEventInfo>&& aEvents);

  // This will call SortEvents automatically if it has not already been
  // called.
  void DispatchEvents()
  {
    mIsObserving = false;
    if (!mPresContext || mPendingEvents.IsEmpty()) {
      return;
    }

    SortEvents();

    EventArray events;
    mPendingEvents.SwapElements(events);
    // mIsSorted will be set to true by SortEvents above, and we leave it
    // that way since mPendingEvents is now empty
    for (AnimationEventInfo& info : events) {
      MOZ_ASSERT(!info.AsWidgetEvent()->mFlags.mIsBeingDispatched &&
                 !info.AsWidgetEvent()->mFlags.mDispatchedAtLeastOnce,
                 "The WidgetEvent should be fresh");
      EventDispatcher::Dispatch(info.mTarget,
                                mPresContext,
                                info.AsWidgetEvent());

      // Bail out if our mPresContext was nullified due to destroying the pres
      // context.
      if (!mPresContext) {
        break;
      }
    }
  }

  void ClearEventQueue()
  {
    mPendingEvents.Clear();
    mIsSorted = true;
  }
  bool HasQueuedEvents() const { return !mPendingEvents.IsEmpty(); }

private:
#ifndef DEBUG
  ~AnimationEventDispatcher() = default;
#else
  ~AnimationEventDispatcher()
  {
    MOZ_ASSERT(!mIsObserving,
               "AnimationEventDispatcher should have disassociated from "
               "nsRefreshDriver");
  }
#endif

  class AnimationEventInfoLessThan
  {
  public:
    bool operator()(const AnimationEventInfo& a, const AnimationEventInfo& b) const
    {
      if (a.mTimeStamp != b.mTimeStamp) {
        // Null timestamps sort first
        if (a.mTimeStamp.IsNull() || b.mTimeStamp.IsNull()) {
          return a.mTimeStamp.IsNull();
        } else {
          return a.mTimeStamp < b.mTimeStamp;
        }
      }

      AnimationPtrComparator<RefPtr<dom::Animation>> comparator;
      return comparator.LessThan(a.mAnimation, b.mAnimation);
    }
  };

  // Sort all pending CSS animation/transition events by scheduled event time
  // and composite order.
  // https://drafts.csswg.org/web-animations/#update-animations-and-send-events
  void SortEvents()
  {
    if (mIsSorted) {
      return;
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

} // namespace mozilla

#endif // mozilla_AnimationEventDispatcher_h
