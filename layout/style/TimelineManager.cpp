/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TimelineManager.h"

#include "mozilla/ElementAnimationData.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScrollTimeline.h"
#include "mozilla/dom/ViewTimeline.h"
#include "nsPresContext.h"

namespace mozilla {
using dom::Element;
using dom::ScrollTimeline;
using dom::ViewTimeline;

template <typename TimelineType>
void TryDestroyTimeline(Element* aElement, PseudoStyleType aPseudoType) {
  auto* collection =
      TimelineCollection<TimelineType>::Get(aElement, aPseudoType);
  if (!collection) {
    return;
  }
  collection->Destroy();
}

void TimelineManager::UpdateTimelines(Element* aElement,
                                      PseudoStyleType aPseudoType,
                                      const ComputedStyle* aComputedStyle,
                                      ProgressTimelineType aType) {
  MOZ_ASSERT(
      aElement->IsInComposedDoc(),
      "No need to update timelines that are not attached to the document tree");

  // If we are in a display:none subtree we will have no computed values.
  // However, if we are on the root of display:none subtree, the computed values
  // might not have been cleared yet. In either case, since CSS animations
  // should not run in display:none subtrees, so we don't need timeline, either.
  const bool shouldDestroyTimelines =
      !aComputedStyle ||
      aComputedStyle->StyleDisplay()->mDisplay == StyleDisplay::None;

  switch (aType) {
    case ProgressTimelineType::Scroll:
      if (shouldDestroyTimelines) {
        TryDestroyTimeline<ScrollTimeline>(aElement, aPseudoType);
        return;
      }
      DoUpdateTimelines<StyleScrollTimeline, ScrollTimeline>(
          mPresContext, aElement, aPseudoType,
          aComputedStyle->StyleUIReset()->mScrollTimelines,
          aComputedStyle->StyleUIReset()->mScrollTimelineNameCount);
      break;

    case ProgressTimelineType::View:
      if (shouldDestroyTimelines) {
        TryDestroyTimeline<ViewTimeline>(aElement, aPseudoType);
        return;
      }
      DoUpdateTimelines<StyleViewTimeline, ViewTimeline>(
          mPresContext, aElement, aPseudoType,
          aComputedStyle->StyleUIReset()->mViewTimelines,
          aComputedStyle->StyleUIReset()->mViewTimelineNameCount);
      break;
  }
}

template <typename TimelineType>
static already_AddRefed<TimelineType> PopExistingTimeline(
    nsAtom* aName, TimelineCollection<TimelineType>* aCollection) {
  if (!aCollection) {
    return nullptr;
  }
  return aCollection->Extract(aName);
}

template <typename StyleType, typename TimelineType>
static auto BuildTimelines(nsPresContext* aPresContext, Element* aElement,
                           PseudoStyleType aPseudoType,
                           const nsStyleAutoArray<StyleType>& aTimelines,
                           size_t aTimelineCount,
                           TimelineCollection<TimelineType>* aCollection) {
  typename TimelineCollection<TimelineType>::TimelineMap result;
  // If multiple timelines are attempting to modify the same property, then the
  // timeline closest to the end of the list of names wins.
  // The spec doesn't mention this specifically for scroll-timeline-name and
  // view-timeline-name, so we follow the same rule with animation-name.
  for (size_t idx = 0; idx < aTimelineCount; ++idx) {
    const StyleType& timeline = aTimelines[idx];
    if (timeline.GetName() == nsGkAtoms::_empty) {
      continue;
    }

    RefPtr<TimelineType> dest =
        PopExistingTimeline(timeline.GetName(), aCollection);
    if (dest) {
      dest->ReplacePropertiesWith(aElement, aPseudoType, timeline);
    } else {
      dest = TimelineType::MakeNamed(aPresContext->Document(), aElement,
                                     aPseudoType, timeline);
    }
    MOZ_ASSERT(dest);

    // Override the previous one if it is duplicated.
    Unused << result.InsertOrUpdate(timeline.GetName(), dest);
  }
  return result;
}

template <typename TimelineType>
static TimelineCollection<TimelineType>& EnsureTimelineCollection(
    Element& aElement, PseudoStyleType aPseudoType);

template <>
ScrollTimelineCollection& EnsureTimelineCollection<ScrollTimeline>(
    Element& aElement, PseudoStyleType aPseudoType) {
  return aElement.EnsureAnimationData().EnsureScrollTimelineCollection(
      aElement, aPseudoType);
}

template <>
ViewTimelineCollection& EnsureTimelineCollection<ViewTimeline>(
    Element& aElement, PseudoStyleType aPseudoType) {
  return aElement.EnsureAnimationData().EnsureViewTimelineCollection(
      aElement, aPseudoType);
}

template <typename StyleType, typename TimelineType>
void TimelineManager::DoUpdateTimelines(
    nsPresContext* aPresContext, Element* aElement, PseudoStyleType aPseudoType,
    const nsStyleAutoArray<StyleType>& aStyleTimelines, size_t aTimelineCount) {
  auto* collection =
      TimelineCollection<TimelineType>::Get(aElement, aPseudoType);
  if (!collection && aTimelineCount == 1 &&
      aStyleTimelines[0].GetName() == nsGkAtoms::_empty) {
    return;
  }

  // We create a new timeline list based on its computed style and the existing
  // timelines.
  auto newTimelines = BuildTimelines<StyleType, TimelineType>(
      aPresContext, aElement, aPseudoType, aStyleTimelines, aTimelineCount,
      collection);

  if (newTimelines.IsEmpty()) {
    if (collection) {
      collection->Destroy();
    }
    return;
  }

  if (!collection) {
    collection =
        &EnsureTimelineCollection<TimelineType>(*aElement, aPseudoType);
    if (!collection->isInList()) {
      AddTimelineCollection(collection);
    }
  }

  // Replace unused timeline with new ones.
  collection->Swap(newTimelines);

  // FIXME: Bug 1774060. We may have to restyle the animations which use the
  // dropped timelines. Or rely on restyling the subtree and the following
  // siblings when mutating {scroll|view}-timeline-name.
}

void TimelineManager::UpdateHiddenByContentVisibilityForAnimations() {
  for (auto* scrollTimelineCollection : mScrollTimelineCollections) {
    for (ScrollTimeline* timeline :
         scrollTimelineCollection->Timelines().Values()) {
      timeline->UpdateHiddenByContentVisibility();
    }
  }

  for (auto* viewTimelineCollection : mViewTimelineCollections) {
    for (ViewTimeline* timeline :
         viewTimelineCollection->Timelines().Values()) {
      timeline->UpdateHiddenByContentVisibility();
    }
  }
}

}  // namespace mozilla
