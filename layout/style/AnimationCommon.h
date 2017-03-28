/* vim: set shiftwidth=2 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_css_AnimationCommon_h
#define mozilla_css_AnimationCommon_h

#include <algorithm> // For <std::stable_sort>
#include "mozilla/AnimationCollection.h"
#include "mozilla/AnimationComparator.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/LinkedList.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Animation.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/Attributes.h" // For MOZ_NON_OWNING_REF
#include "mozilla/Assertions.h"
#include "mozilla/TimingParams.h"
#include "nsContentUtils.h"
#include "nsCSSPseudoElements.h"
#include "nsCycleCollectionParticipant.h"

class nsIFrame;
class nsPresContext;

namespace mozilla {
enum class CSSPseudoElementType : uint8_t;

namespace dom {
class Element;
}

template <class AnimationType>
class CommonAnimationManager {
public:
  explicit CommonAnimationManager(nsPresContext *aPresContext)
    : mPresContext(aPresContext)
  {
  }

  // NOTE:  This can return null after Disconnect().
  nsPresContext* PresContext() const { return mPresContext; }

  /**
   * Notify the manager that the pres context is going away.
   */
  void Disconnect()
  {
    // Content nodes might outlive the transition or animation manager.
    RemoveAllElementCollections();

    mPresContext = nullptr;
  }

  /**
   * Stop animations on the element. This method takes the real element
   * rather than the element for the generated content for animations on
   * ::before and ::after.
   */
  void StopAnimationsForElement(dom::Element* aElement,
                                CSSPseudoElementType aPseudoType)
  {
    MOZ_ASSERT(aElement);
    AnimationCollection<AnimationType>* collection =
      AnimationCollection<AnimationType>::GetAnimationCollection(aElement,
                                                                 aPseudoType);
    if (!collection) {
      return;
    }

    nsAutoAnimationMutationBatch mb(aElement->OwnerDoc());
    collection->Destroy();
  }

protected:
  virtual ~CommonAnimationManager()
  {
    MOZ_ASSERT(!mPresContext, "Disconnect should have been called");
  }

  void AddElementCollection(AnimationCollection<AnimationType>* aCollection)
  {
    mElementCollections.insertBack(aCollection);
  }
  void RemoveAllElementCollections()
  {
    while (AnimationCollection<AnimationType>* head =
           mElementCollections.getFirst()) {
      head->Destroy(); // Note: this removes 'head' from mElementCollections.
    }
  }

  LinkedList<AnimationCollection<AnimationType>> mElementCollections;
  nsPresContext *mPresContext; // weak (non-null from ctor to Disconnect)
};

/**
 * Utility class for referencing the element that created a CSS animation or
 * transition. It is non-owning (i.e. it uses a raw pointer) since it is only
 * expected to be set by the owned animation while it actually being managed
 * by the owning element.
 *
 * This class also abstracts the comparison of an element/pseudo-class pair
 * for the sake of composite ordering since this logic is common to both CSS
 * animations and transitions.
 *
 * (We call this OwningElementRef instead of just OwningElement so that we can
 * call the getter on CSSAnimation/CSSTransition OwningElement() without
 * clashing with this object's contructor.)
 */
class OwningElementRef final
{
public:
  OwningElementRef() = default;

  explicit OwningElementRef(const NonOwningAnimationTarget& aTarget)
    : mTarget(aTarget)
  { }

  OwningElementRef(dom::Element& aElement,
                   CSSPseudoElementType aPseudoType)
    : mTarget(&aElement, aPseudoType)
  { }

  bool Equals(const OwningElementRef& aOther) const
  {
    return mTarget == aOther.mTarget;
  }

  bool LessThan(const OwningElementRef& aOther) const
  {
    MOZ_ASSERT(mTarget.mElement && aOther.mTarget.mElement,
               "Elements to compare should not be null");

    if (mTarget.mElement != aOther.mTarget.mElement) {
      return nsContentUtils::PositionIsBefore(mTarget.mElement,
                                              aOther.mTarget.mElement);
    }

    return mTarget.mPseudoType == CSSPseudoElementType::NotPseudo ||
          (mTarget.mPseudoType == CSSPseudoElementType::before &&
           aOther.mTarget.mPseudoType == CSSPseudoElementType::after);
  }

  bool IsSet() const { return !!mTarget.mElement; }

  void GetElement(dom::Element*& aElement,
                  CSSPseudoElementType& aPseudoType) const
  {
    aElement = mTarget.mElement;
    aPseudoType = mTarget.mPseudoType;
  }

private:
  NonOwningAnimationTarget mTarget;
};

template <class EventInfo>
class DelayedEventDispatcher
{
public:
  DelayedEventDispatcher() : mIsSorted(true) { }

  void QueueEvent(EventInfo&& aEventInfo)
  {
    mPendingEvents.AppendElement(Forward<EventInfo>(aEventInfo));
    mIsSorted = false;
  }

  // This is exposed as a separate method so that when we are dispatching
  // *both* transition events and animation events we can sort both lists
  // once using the current state of the document before beginning any
  // dispatch.
  void SortEvents()
  {
    if (mIsSorted) {
      return;
    }

    // FIXME: Replace with mPendingEvents.StableSort when bug 1147091 is
    // fixed.
    std::stable_sort(mPendingEvents.begin(), mPendingEvents.end(),
                     EventInfoLessThan());
    mIsSorted = true;
  }

  // Takes a reference to the owning manager's pres context so it can
  // detect if the pres context is destroyed while dispatching one of
  // the events.
  //
  // This will call SortEvents automatically if it has not already been
  // called.
  void DispatchEvents(nsPresContext* const & aPresContext)
  {
    if (!aPresContext || mPendingEvents.IsEmpty()) {
      return;
    }

    SortEvents();

    EventArray events;
    mPendingEvents.SwapElements(events);
    // mIsSorted will be set to true by SortEvents above, and we leave it
    // that way since mPendingEvents is now empty
    for (EventInfo& info : events) {
      EventDispatcher::Dispatch(info.mElement, aPresContext, &info.mEvent);

      if (!aPresContext) {
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

  // Methods for supporting cycle-collection
  void Traverse(nsCycleCollectionTraversalCallback* aCallback,
                const char* aName)
  {
    for (EventInfo& info : mPendingEvents) {
      ImplCycleCollectionTraverse(*aCallback, info.mElement, aName);
      ImplCycleCollectionTraverse(*aCallback, info.mAnimation, aName);
    }
  }
  void Unlink() { ClearEventQueue(); }

protected:
  class EventInfoLessThan
  {
  public:
    bool operator()(const EventInfo& a, const EventInfo& b) const
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

  typedef nsTArray<EventInfo> EventArray;
  EventArray mPendingEvents;
  bool mIsSorted;
};

template <class EventInfo>
inline void
ImplCycleCollectionUnlink(DelayedEventDispatcher<EventInfo>& aField)
{
  aField.Unlink();
}

template <class EventInfo>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            DelayedEventDispatcher<EventInfo>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  aField.Traverse(&aCallback, aName);
}

// Return the TransitionPhase or AnimationPhase to use when the animation
// doesn't have a target effect.
template <typename PhaseType>
PhaseType GetAnimationPhaseWithoutEffect(const dom::Animation& aAnimation)
{
  MOZ_ASSERT(!aAnimation.GetEffect(),
             "Should only be called when we do not have an effect");

  Nullable<TimeDuration> currentTime = aAnimation.GetCurrentTime();
  if (currentTime.IsNull()) {
    return PhaseType::Idle;
  }

  // If we don't have a target effect, the duration will be zero so the phase is
  // 'before' if the current time is less than zero.
  return currentTime.Value() < TimeDuration()
         ? PhaseType::Before
         : PhaseType::After;
};

inline TimingParams
TimingParamsFromCSSParams(float aDuration, float aDelay,
                          float aIterationCount,
                          dom::PlaybackDirection aDirection,
                          dom::FillMode aFillMode)
{
  MOZ_ASSERT(aIterationCount >= 0.0 && !IsNaN(aIterationCount),
             "aIterations should be nonnegative & finite, as ensured by "
             "CSSParser");

  TimingParams timing;
  timing.mDuration.emplace(StickyTimeDuration::FromMilliseconds(aDuration));
  timing.mDelay = TimeDuration::FromMilliseconds(aDelay);
  timing.mIterations = aIterationCount;
  timing.mDirection = aDirection;
  timing.mFill = aFillMode;

  return timing;
}

} // namespace mozilla

#endif /* !defined(mozilla_css_AnimationCommon_h) */
