/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ElementAnimationData_h
#define mozilla_ElementAnimationData_h

#include "mozilla/UniquePtr.h"
#include "mozilla/PseudoStyleType.h"

class nsCycleCollectionTraversalCallback;

namespace mozilla {
enum class PseudoStyleType : uint8_t;
class EffectSet;
template <typename Animation>
class AnimationCollection;
template <typename TimelineType>
class TimelineCollection;
namespace dom {
class Element;
class CSSAnimation;
class CSSTransition;
class ProgressTimelineScheduler;
class ScrollTimeline;
class ViewTimeline;
}  // namespace dom
using CSSAnimationCollection = AnimationCollection<dom::CSSAnimation>;
using CSSTransitionCollection = AnimationCollection<dom::CSSTransition>;
using ScrollTimelineCollection = TimelineCollection<dom::ScrollTimeline>;
using ViewTimelineCollection = TimelineCollection<dom::ViewTimeline>;

// The animation data for a given element (and its pseudo-elements).
class ElementAnimationData {
  struct PerElementOrPseudoData {
    UniquePtr<EffectSet> mEffectSet;
    UniquePtr<CSSAnimationCollection> mAnimations;
    UniquePtr<CSSTransitionCollection> mTransitions;

    // Note: scroll-timeline-name is applied to elements which could be
    // scroll containers, or replaced elements. view-timeline-name is applied to
    // all elements. However, the named timeline is referenceable in
    // animation-timeline by the tree order scope.
    // Spec: https://drafts.csswg.org/scroll-animations-1/#timeline-scope.
    //
    // So it should be fine to create timeline objects only on the elements and
    // pseudo elements which support animations.
    UniquePtr<ScrollTimelineCollection> mScrollTimelines;
    UniquePtr<ViewTimelineCollection> mViewTimelines;

    // This is different from |mScrollTimelines|. We use this to schedule all
    // scroll-driven animations (which use anonymous/named scroll timelines or
    // anonymous/name view timelines) for a specific scroll source (which is the
    // element with ScrollContainerFrame).
    //
    // TimelineCollection owns and manages the named progress timeline generated
    // by specifying scroll-timeline-name property and view-timeline-name
    // property on this element. However, the anonymous progress timelines (e.g.
    // animation-timeline:scroll()) are owned by Animation objects only.
    //
    // Note:
    // 1. For named scroll timelines. The element which specifies
    //    scroll-timeline-name is the scroll source. However, for named view
    //    timelines, the element which specifies view-timeline-name may not be
    //    the scroll source because we use its nearest scroll container as the
    //    scroll source.
    // 2. For anonymous progress timelines, we don't keep their timeline obejcts
    //    in TimelineCollection.
    // So, per 1) and 2), we use |mProgressTimelineScheduler| for the scroll
    // source element to schedule scroll-driven animations.
    UniquePtr<dom::ProgressTimelineScheduler> mProgressTimelineScheduler;

    PerElementOrPseudoData();
    ~PerElementOrPseudoData();

    EffectSet& DoEnsureEffectSet();
    CSSTransitionCollection& DoEnsureTransitions(dom::Element&,
                                                 PseudoStyleType);
    CSSAnimationCollection& DoEnsureAnimations(dom::Element&, PseudoStyleType);
    ScrollTimelineCollection& DoEnsureScrollTimelines(dom::Element&,
                                                      PseudoStyleType);
    ViewTimelineCollection& DoEnsureViewTimelines(dom::Element&,
                                                  PseudoStyleType);
    dom::ProgressTimelineScheduler& DoEnsureProgressTimelineScheduler(
        dom::Element&, PseudoStyleType);

    void DoClearEffectSet();
    void DoClearTransitions();
    void DoClearAnimations();
    void DoClearScrollTimelines();
    void DoClearViewTimelines();
    void DoClearProgressTimelineScheduler();

    void Traverse(nsCycleCollectionTraversalCallback&);
  };

  PerElementOrPseudoData mElementData;

  // TODO(emilio): Maybe this should be a hash map eventually, once we allow
  // animating all pseudo-elements.
  PerElementOrPseudoData mBeforeData;
  PerElementOrPseudoData mAfterData;
  PerElementOrPseudoData mMarkerData;

  const PerElementOrPseudoData& DataFor(PseudoStyleType aType) const {
    switch (aType) {
      case PseudoStyleType::NotPseudo:
        break;
      case PseudoStyleType::before:
        return mBeforeData;
      case PseudoStyleType::after:
        return mAfterData;
      case PseudoStyleType::marker:
        return mMarkerData;
      default:
        MOZ_ASSERT_UNREACHABLE(
            "Should not try to get animation effects for "
            "a pseudo other that :before, :after or ::marker");
        break;
    }
    return mElementData;
  }

  PerElementOrPseudoData& DataFor(PseudoStyleType aType) {
    const auto& data =
        const_cast<const ElementAnimationData*>(this)->DataFor(aType);
    return const_cast<PerElementOrPseudoData&>(data);
  }

 public:
  void Traverse(nsCycleCollectionTraversalCallback&);

  void ClearAllAnimationCollections();

  EffectSet* GetEffectSetFor(PseudoStyleType aType) const {
    return DataFor(aType).mEffectSet.get();
  }

  void ClearEffectSetFor(PseudoStyleType aType) {
    auto& data = DataFor(aType);
    if (data.mEffectSet) {
      data.DoClearEffectSet();
    }
  }

  EffectSet& EnsureEffectSetFor(PseudoStyleType aType) {
    auto& data = DataFor(aType);
    if (auto* set = data.mEffectSet.get()) {
      return *set;
    }
    return data.DoEnsureEffectSet();
  }

  CSSTransitionCollection* GetTransitionCollection(PseudoStyleType aType) {
    return DataFor(aType).mTransitions.get();
  }

  void ClearTransitionCollectionFor(PseudoStyleType aType) {
    auto& data = DataFor(aType);
    if (data.mTransitions) {
      data.DoClearTransitions();
    }
  }

  CSSTransitionCollection& EnsureTransitionCollection(dom::Element& aOwner,
                                                      PseudoStyleType aType) {
    auto& data = DataFor(aType);
    if (auto* collection = data.mTransitions.get()) {
      return *collection;
    }
    return data.DoEnsureTransitions(aOwner, aType);
  }

  CSSAnimationCollection* GetAnimationCollection(PseudoStyleType aType) {
    return DataFor(aType).mAnimations.get();
  }

  void ClearAnimationCollectionFor(PseudoStyleType aType) {
    auto& data = DataFor(aType);
    if (data.mAnimations) {
      data.DoClearAnimations();
    }
  }

  CSSAnimationCollection& EnsureAnimationCollection(dom::Element& aOwner,
                                                    PseudoStyleType aType) {
    auto& data = DataFor(aType);
    if (auto* collection = data.mAnimations.get()) {
      return *collection;
    }
    return data.DoEnsureAnimations(aOwner, aType);
  }

  ScrollTimelineCollection* GetScrollTimelineCollection(PseudoStyleType aType) {
    return DataFor(aType).mScrollTimelines.get();
  }

  void ClearScrollTimelineCollectionFor(PseudoStyleType aType) {
    auto& data = DataFor(aType);
    if (data.mScrollTimelines) {
      data.DoClearScrollTimelines();
    }
  }

  ScrollTimelineCollection& EnsureScrollTimelineCollection(
      dom::Element& aOwner, PseudoStyleType aType) {
    auto& data = DataFor(aType);
    if (auto* collection = data.mScrollTimelines.get()) {
      return *collection;
    }
    return data.DoEnsureScrollTimelines(aOwner, aType);
  }

  ViewTimelineCollection* GetViewTimelineCollection(PseudoStyleType aType) {
    return DataFor(aType).mViewTimelines.get();
  }

  void ClearViewTimelineCollectionFor(PseudoStyleType aType) {
    auto& data = DataFor(aType);
    if (data.mViewTimelines) {
      data.DoClearViewTimelines();
    }
  }

  ViewTimelineCollection& EnsureViewTimelineCollection(dom::Element& aOwner,
                                                       PseudoStyleType aType) {
    auto& data = DataFor(aType);
    if (auto* collection = data.mViewTimelines.get()) {
      return *collection;
    }
    return data.DoEnsureViewTimelines(aOwner, aType);
  }

  dom::ProgressTimelineScheduler* GetProgressTimelineScheduler(
      PseudoStyleType aType) {
    return DataFor(aType).mProgressTimelineScheduler.get();
  }

  void ClearProgressTimelineScheduler(PseudoStyleType aType) {
    auto& data = DataFor(aType);
    if (data.mProgressTimelineScheduler) {
      data.DoClearProgressTimelineScheduler();
    }
  }

  dom::ProgressTimelineScheduler& EnsureProgressTimelineScheduler(
      dom::Element& aOwner, PseudoStyleType aType) {
    auto& data = DataFor(aType);
    if (auto* collection = data.mProgressTimelineScheduler.get()) {
      return *collection;
    }
    return data.DoEnsureProgressTimelineScheduler(aOwner, aType);
  }

  ElementAnimationData() = default;
};

}  // namespace mozilla

#endif
