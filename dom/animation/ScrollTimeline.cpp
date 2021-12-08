/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollTimeline.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/Document.h"

namespace mozilla::dom {

// ---------------------------------
// Methods of ScrollTimeline
// ---------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(ScrollTimeline)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ScrollTimeline,
                                                AnimationTimeline)
  tmp->Teardown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ScrollTimeline,
                                                  AnimationTimeline)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ScrollTimeline,
                                               AnimationTimeline)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(ScrollTimeline,
                                               AnimationTimeline)

ScrollTimeline::ScrollTimeline(Document* aDocument, Element* aScroller)
    : AnimationTimeline(aDocument->GetParentObject()),
      mDocument(aDocument),
      // FIXME: Bug 1737918: We may have to udpate the constructor arguments
      // because this can be nearest, root, or a specific container. For now,
      // the input is a source element directly and it is the root element.
      //
      // FIXME: it seems we cannot use Document->GetScrollingElement() in the
      // caller (i.e. nsAnimationManager::BuildAnimation()) because it flushes
      // the layout, Perhaps we have to define a variant version, like
      // GetScrollingElementNoLayout(), which doesn't do layout. So this causes
      // a bug on quirks mode.
      mSource(aScroller),
      mDirection(StyleScrollDirection::Auto) {
  MOZ_ASSERT(aDocument);

  RegisterWithScrollSource();
}

void ScrollTimeline::RegisterWithScrollSource() {
  if (!mSource) {
    return;
  }

  if (ScrollTimelineSet* scrollTimelineSet =
          ScrollTimelineSet::GetOrCreateScrollTimelineSet(mSource)) {
    scrollTimelineSet->AddScrollTimeline(*this);
  }
}

void ScrollTimeline::UnregisterFromScrollSource() {
  if (!mSource) {
    return;
  }

  if (ScrollTimelineSet* scrollTimelineSet =
          ScrollTimelineSet::GetScrollTimelineSet(mSource)) {
    scrollTimelineSet->RemoveScrollTimeline(*this);
    if (scrollTimelineSet->IsEmpty()) {
      ScrollTimelineSet::DestroyScrollTimelineSet(mSource);
    }
  }
}

// ---------------------------------
// Methods of ScrollTimelineSet
// ---------------------------------

/* static */ ScrollTimelineSet* ScrollTimelineSet::GetScrollTimelineSet(
    Element* aElement) {
  MOZ_ASSERT(aElement);
  return static_cast<ScrollTimelineSet*>(
      aElement->GetProperty(nsGkAtoms::scrollTimelinesProperty));
}

/* static */ ScrollTimelineSet* ScrollTimelineSet::GetOrCreateScrollTimelineSet(
    Element* aElement) {
  MOZ_ASSERT(aElement);
  ScrollTimelineSet* scrollTimelineSet = GetScrollTimelineSet(aElement);
  if (scrollTimelineSet) {
    return scrollTimelineSet;
  }

  scrollTimelineSet = new ScrollTimelineSet();
  nsresult rv = aElement->SetProperty(
      nsGkAtoms::scrollTimelinesProperty, scrollTimelineSet,
      nsINode::DeleteProperty<ScrollTimelineSet>, true);
  if (NS_FAILED(rv)) {
    NS_WARNING("SetProperty failed");
    delete scrollTimelineSet;
    return nullptr;
  }
  return scrollTimelineSet;
}

/* static */ void ScrollTimelineSet::DestroyScrollTimelineSet(
    Element* aElement) {
  aElement->RemoveProperty(nsGkAtoms::scrollTimelinesProperty);
}

}  // namespace mozilla::dom
