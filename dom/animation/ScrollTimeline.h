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
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/TimingParams.h"
#include "mozilla/WritingModes.h"

class nsIScrollableFrame;

namespace mozilla {

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
 *   | Try schedule the scroll-linked animations, if there are any scroll
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
class ScrollTimeline final : public AnimationTimeline {
 public:
  struct Scroller {
    // FIXME: Support nearest and replace auto with root in Bug 1737918.
    enum class Type : uint8_t {
      // For auto. Should be scrolling element of the owner doc.
      Auto,
      // For any other specific elements.
      Other,
    };
    Type mType = Type::Auto;
    RefPtr<Element> mElement;

    // We use the owner doc of the animation target. This may be different from
    // |mDocument| after we implement ScrollTimeline interface for script.
    static Scroller Auto(const Document* aOwnerDoc) {
      // For auto, we use scrolling element as the default scroller.
      // However, it's mutable, and we would like to keep things simple, so
      // we always register the ScrollTimeline to the document element (i.e.
      // root element) because the content of the root scroll frame is the root
      // element.
      return {Type::Auto, aOwnerDoc->GetDocumentElement()};
    }

    explicit operator bool() const { return mElement; }
    bool operator==(const Scroller& aOther) const {
      return mType == aOther.mType && mElement == aOther.mElement;
    }
  };

  ScrollTimeline() = delete;
  ScrollTimeline(Document* aDocument, const Scroller& aScroller);

  // FIXME: Bug 1737918: Rewrite this because @scroll-timeline will be obsolete.
  static already_AddRefed<ScrollTimeline> FromRule(
      const RawServoScrollTimelineRule& aRule, Document* aDocument,
      const NonOwningAnimationTarget& aTarget);

  bool operator==(const ScrollTimeline& aOther) const {
    return mDocument == aOther.mDocument && mSource == aOther.mSource &&
           mDirection == aOther.mDirection;
  }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(ScrollTimeline,
                                                         AnimationTimeline)

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

  void ScheduleAnimations() {
    // FIXME: Bug 1737927: Need to check the animation mutation observers for
    // animations with scroll timelines.
    // nsAutoAnimationMutationBatch mb(mDocument);

    Tick();
  }

  static constexpr const TimingParams& GetTiming() { return sTiming; }

 protected:
  virtual ~ScrollTimeline() { Teardown(); }

 private:
  // Note: This function is required to be idempotent, as it can be called from
  // both cycleCollection::Unlink() and ~ScrollTimeline(). When modifying this
  // function, be sure to preserve this property.
  void Teardown() { UnregisterFromScrollSource(); }

  // Register/Unregister this scroll timeline to the element property.
  void RegisterWithScrollSource();
  void UnregisterFromScrollSource();

  const nsIScrollableFrame* GetScrollFrame() const;

  // A helper to get the physical orientation of this scroll-timeline.
  //
  // The spec defines auto, but there is a spec issue:
  // "ISSUE 5 Define these values." in this section. The DOM interface removed
  // auto and use block as default value, so we treat auto as block now.
  // https://drafts.csswg.org/scroll-animations-1/#descdef-scroll-timeline-orientation
  layers::ScrollDirection GetPhysicalOrientation(WritingMode aWM) const {
    return mDirection == StyleScrollDirection::Horizontal ||
                   (!aWM.IsVertical() &&
                    mDirection == StyleScrollDirection::Inline) ||
                   (aWM.IsVertical() &&
                    (mDirection == StyleScrollDirection::Block ||
                     mDirection == StyleScrollDirection::Auto))
               ? layers::ScrollDirection::eHorizontal
               : layers::ScrollDirection::eVertical;
  }

  RefPtr<Document> mDocument;

  // FIXME: Bug 1733260: new spec proposal uses a new way to define scroller,
  // and move the element-based offset into view-timeline, so here we only
  // implement the default behavior of scroll timeline:
  // 1. "source" is auto (use scrolling element), and
  // 2. "scroll-offsets" is none (i.e. always 0% ~ 100%).
  // So now we will only use the scroll direction from @scroll-timeline rule.
  Scroller mSource;
  StyleScrollDirection mDirection;

  // Note: it's unfortunate TimingParams cannot be a const variable because
  // we have to use StickyTimingDuration::FromMilliseconds() in its
  // constructor.
  static TimingParams sTiming;
};

/**
 *
 * A wrapper around a hashset of ScrollTimeline objects to handle
 * storing the set as a property of an element (i.e. source).
 * This makes use easier to look up a ScrollTimeline from the element.
 *
 * Note:
 * 1. "source" is the element which the ScrollTimeline hooks.
 *    Each ScrollTimeline hooks an dom::Element, and a dom::Element may be
 *    registered by multiple ScrollTimelines.
 * 2. Element holds the ScrollTimelineSet as an element property. Also, the
 *    owner document of this Element keeps a linked list of ScrollTimelines
 *    (instead of ScrollTimelineSet).
 */
class ScrollTimelineSet {
 public:
  using NonOwningScrollTimelineSet = HashSet<ScrollTimeline*>;

  ~ScrollTimelineSet() = default;

  static ScrollTimelineSet* GetScrollTimelineSet(Element* aElement);
  static ScrollTimelineSet* GetOrCreateScrollTimelineSet(Element* aElement);
  static void DestroyScrollTimelineSet(Element* aElement);

  void AddScrollTimeline(ScrollTimeline& aScrollTimeline) {
    Unused << mScrollTimelines.put(&aScrollTimeline);
  }
  void RemoveScrollTimeline(ScrollTimeline& aScrollTimeline) {
    mScrollTimelines.remove(&aScrollTimeline);
  }

  bool IsEmpty() const { return mScrollTimelines.empty(); }

  void ScheduleAnimations() const {
    for (auto iter = mScrollTimelines.iter(); !iter.done(); iter.next()) {
      iter.get()->ScheduleAnimations();
    }
  }

 private:
  ScrollTimelineSet() = default;

  // ScrollTimelineSet doesn't own ScrollTimeline. We let Animations own its
  // scroll timeline. (Note: one ScrollTimeline could be owned by multiple
  // associated Animations.)
  // The ScrollTimeline is generated only by CSS, so if all the associated
  // Animations are gone, we don't need the ScrollTimeline anymore, so
  // ScrollTimelineSet doesn't have to keep it for the source element.
  // FIXME: Bug 1676794: We may have to update here if it's possible to create
  // ScrollTimeline via script.
  NonOwningScrollTimelineSet mScrollTimelines;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ScrollTimeline_h
