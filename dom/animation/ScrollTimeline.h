/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ScrollTimeline_h
#define mozilla_dom_ScrollTimeline_h

#include "mozilla/dom/AnimationTimeline.h"
#include "mozilla/dom/Document.h"
#include "mozilla/HashTable.h"
#include "mozilla/PairHash.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/WritingModes.h"

#define PROGRESS_TIMELINE_DURATION_MILLISEC 100000

class nsIScrollableFrame;

namespace mozilla {
class ElementAnimationData;
struct NonOwningAnimationTarget;
namespace dom {
class Element;

/**
 * Implementation notes
 * --------------------
 *
 * ScrollTimelines do not observe refreshes the way DocumentTimelines do.
 * This is because the refresh driver keeps ticking while it has registered
 * refresh observers. For a DocumentTimeline, it's appropriate to keep the
 * refresh driver ticking as long as there are active animations, since the
 * animations need to be sampled on every frame. Scroll-linked animations,
 * however, only need to be sampled when scrolling has occurred, so keeping
 * the refresh driver ticking is wasteful.
 *
 * As a result, we schedule an animation restyle when
 * 1) there are any scroll offsets updated (from APZ or script), via
 *    nsIScrollableFrame, or
 * 2) there are any possible scroll range updated during the frame reflow.
 *
 * -------------
 * | Animation |
 * -------------
 *   ^
 *   | Call Animation::Tick() if there are any scroll updates.
 *   |
 * ------------------
 * | ScrollTimeline |
 * ------------------
 *   ^
 *   | Try schedule the scroll-driven animations, if there are any scroll
 *   | offsets changed or the scroll range changed [1].
 *   |
 * ----------------------
 * | nsIScrollableFrame |
 * ----------------------
 *
 * [1] nsIScrollableFrame uses its associated dom::Element to lookup the
 *     ScrollTimelineSet, and iterates the set to schedule the animations
 *     linked to the ScrollTimelines.
 */
class ScrollTimeline : public AnimationTimeline {
  template <typename T, typename... Args>
  friend already_AddRefed<T> mozilla::MakeAndAddRef(Args&&... aArgs);

 public:
  struct Scroller {
    // FIXME: Bug 1765211. Perhaps we only need root and a specific element.
    // This depends on how we fix this bug.
    enum class Type : uint8_t {
      Root,
      Nearest,
      Name,
      Self,
    };
    Type mType = Type::Root;
    RefPtr<Element> mElement;
    PseudoStyleType mPseudoType;

    // We use the owner doc of the animation target. This may be different from
    // |mDocument| after we implement ScrollTimeline interface for script.
    static Scroller Root(const Document* aOwnerDoc) {
      // For auto, we use scrolling element as the default scroller.
      // However, it's mutable, and we would like to keep things simple, so
      // we always register the ScrollTimeline to the document element (i.e.
      // root element) because the content of the root scroll frame is the root
      // element.
      return {Type::Root, aOwnerDoc->GetDocumentElement(),
              PseudoStyleType::NotPseudo};
    }

    static Scroller Nearest(Element* aElement, PseudoStyleType aPseudoType) {
      return {Type::Nearest, aElement, aPseudoType};
    }

    static Scroller Named(Element* aElement, PseudoStyleType aPseudoType) {
      return {Type::Name, aElement, aPseudoType};
    }

    static Scroller Self(Element* aElement, PseudoStyleType aPseudoType) {
      return {Type::Self, aElement, aPseudoType};
    }

    explicit operator bool() const { return mElement; }
    bool operator==(const Scroller& aOther) const {
      return mType == aOther.mType && mElement == aOther.mElement &&
             mPseudoType == aOther.mPseudoType;
    }
  };

  static already_AddRefed<ScrollTimeline> MakeAnonymous(
      Document* aDocument, const NonOwningAnimationTarget& aTarget,
      StyleScrollAxis aAxis, StyleScroller aScroller);

  // Note: |aReferfenceElement| is used as the scroller which specifies
  // scroll-timeline-name property.
  static already_AddRefed<ScrollTimeline> MakeNamed(
      Document* aDocument, Element* aReferenceElement,
      PseudoStyleType aPseudoType, const StyleScrollTimeline& aStyleTimeline);

  bool operator==(const ScrollTimeline& aOther) const {
    return mDocument == aOther.mDocument && mSource == aOther.mSource &&
           mAxis == aOther.mAxis;
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ScrollTimeline, AnimationTimeline)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    // FIXME: Bug 1676794: Implement ScrollTimeline interface.
    return nullptr;
  }

  // AnimationTimeline methods.
  Nullable<TimeDuration> GetCurrentTimeAsDuration() const override;
  bool TracksWallclockTime() const override { return false; }
  Nullable<TimeDuration> ToTimelineTime(
      const TimeStamp& aTimeStamp) const override {
    // It's unclear to us what should we do for this function now, so return
    // nullptr.
    return nullptr;
  }
  TimeStamp ToTimeStamp(const TimeDuration& aTimelineTime) const override {
    // It's unclear to us what should we do for this function now, so return
    // zero time.
    return {};
  }
  Document* GetDocument() const override { return mDocument; }
  bool IsMonotonicallyIncreasing() const override { return false; }
  bool IsScrollTimeline() const override { return true; }
  const ScrollTimeline* AsScrollTimeline() const override { return this; }
  bool IsViewTimeline() const override { return false; }

  Nullable<TimeDuration> TimelineDuration() const override {
    // We are using this magic number for progress-based timeline duration
    // because we don't support percentage for duration.
    return TimeDuration::FromMilliseconds(PROGRESS_TIMELINE_DURATION_MILLISEC);
  }

  void ScheduleAnimations() {
    // FIXME: Bug 1737927: Need to check the animation mutation observers for
    // animations with scroll timelines.
    // nsAutoAnimationMutationBatch mb(mDocument);
    TickState state;
    Tick(state);
    // TODO: Do we need to synchronize scroll animations?
  }

  // If the source of a ScrollTimeline is an element whose principal box does
  // not exist or is not a scroll container, then its phase is the timeline
  // inactive phase. It is otherwise in the active phase. This returns true if
  // the timeline is in active phase.
  // https://drafts.csswg.org/web-animations-1/#inactive-timeline
  // Note: This function is called only for compositor animations, so we must
  // have the primary frame (principal box) for the source element if it exists.
  bool IsActive() const { return GetScrollFrame(); }

  Element* SourceElement() const {
    MOZ_ASSERT(mSource);
    return mSource.mElement;
  }

  // A helper to get the physical orientation of this scroll-timeline.
  layers::ScrollDirection Axis() const;

  StyleOverflow SourceScrollStyle() const;

  bool APZIsActiveForSource() const;

  bool ScrollingDirectionIsAvailable() const;

  void ReplacePropertiesWith(const Element* aReferenceElement,
                             PseudoStyleType aPseudoType,
                             const StyleScrollTimeline& aNew);

  void NotifyAnimationContentVisibilityChanged(Animation* aAnimation,
                                               bool aIsVisible) override;

 protected:
  virtual ~ScrollTimeline() { Teardown(); }
  ScrollTimeline() = delete;
  ScrollTimeline(Document* aDocument, const Scroller& aScroller,
                 StyleScrollAxis aAxis);

  struct ScrollOffsets {
    nscoord mStart = 0;
    nscoord mEnd = 0;
  };
  virtual Maybe<ScrollOffsets> ComputeOffsets(
      const nsIScrollableFrame* aScrollFrame,
      layers::ScrollDirection aOrientation) const;

  // Note: This function is required to be idempotent, as it can be called from
  // both cycleCollection::Unlink() and ~ScrollTimeline(). When modifying this
  // function, be sure to preserve this property.
  void Teardown() { UnregisterFromScrollSource(); }

  // Register this scroll timeline to the element property.
  void RegisterWithScrollSource();

  // Unregister this scroll timeline from the element property.
  void UnregisterFromScrollSource();

  const nsIScrollableFrame* GetScrollFrame() const;

  static std::pair<const Element*, PseudoStyleType> FindNearestScroller(
      Element* aSubject, PseudoStyleType aPseudoType);

  RefPtr<Document> mDocument;

  // FIXME: Bug 1765211: We may have to update the source element once the
  // overflow property of the scroll-container is updated when we are using
  // nearest scroller.
  Scroller mSource;
  StyleScrollAxis mAxis;
};

/**
 * A wrapper around a hashset of ScrollTimeline objects to handle the scheduling
 * of scroll driven animations. This is used for all kinds of progress
 * timelines, i.e. anonymous/named scroll timelines and anonymous/named view
 * timelines. And this object is owned by the scroll source (See
 * ElementAnimationData and nsGfxScrollFrame for the usage).
 */
class ProgressTimelineScheduler {
 public:
  ProgressTimelineScheduler() { MOZ_COUNT_CTOR(ProgressTimelineScheduler); }
  ~ProgressTimelineScheduler() { MOZ_COUNT_DTOR(ProgressTimelineScheduler); }

  static ProgressTimelineScheduler* Get(const Element* aElement,
                                        PseudoStyleType aPseudoType);
  static ProgressTimelineScheduler& Ensure(Element* aElement,
                                           PseudoStyleType aPseudoType);
  static void Destroy(const Element* aElement, PseudoStyleType aPseudoType);

  void AddTimeline(ScrollTimeline* aScrollTimeline) {
    Unused << mTimelines.put(aScrollTimeline);
  }
  void RemoveTimeline(ScrollTimeline* aScrollTimeline) {
    mTimelines.remove(aScrollTimeline);
  }

  bool IsEmpty() const { return mTimelines.empty(); }

  void ScheduleAnimations() const {
    for (auto iter = mTimelines.iter(); !iter.done(); iter.next()) {
      iter.get()->ScheduleAnimations();
    }
  }

 private:
  // We let Animations own its scroll timeline or view timeline if it is
  // anonymous. For named progress timelines, they are created and destroyed by
  // TimelineCollection.
  HashSet<ScrollTimeline*> mTimelines;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ScrollTimeline_h
