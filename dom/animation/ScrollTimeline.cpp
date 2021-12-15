/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScrollTimeline.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/AnimationTarget.h"
#include "mozilla/PresShell.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"

#define SCROLL_TIMELINE_DURATION_MILLISEC 100000

namespace mozilla::dom {

// ---------------------------------
// Methods of ScrollTimeline
// ---------------------------------

NS_IMPL_CYCLE_COLLECTION_CLASS(ScrollTimeline)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ScrollTimeline,
                                                AnimationTimeline)
  tmp->Teardown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mSource.mElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ScrollTimeline,
                                                  AnimationTimeline)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mSource.mElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(ScrollTimeline,
                                               AnimationTimeline)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(ScrollTimeline,
                                               AnimationTimeline)

TimingParams ScrollTimeline::sTiming;

ScrollTimeline::ScrollTimeline(Document* aDocument, const Scroller& aScroller)
    : AnimationTimeline(aDocument->GetParentObject()),
      mDocument(aDocument),
      // FIXME: Bug 1737918: We may have to udpate the constructor arguments
      // because this can be nearest, root, or a specific container. For now,
      // the input is a source element directly and it is the root element.
      mSource(aScroller),
      mDirection(StyleScrollDirection::Auto) {
  MOZ_ASSERT(aDocument);

  // Use default values except for |mDuration| and |mFill|.
  // Use a fixed duration defined in SCROLL_TIMELINE_DURATIONMILLISEC, and use
  // FillMode::Both to make sure the animation is in effect at 100%.
  sTiming = TimingParams(SCROLL_TIMELINE_DURATION_MILLISEC, 0.0,
                         std::numeric_limits<float>::infinity(),
                         PlaybackDirection::Alternate, FillMode::Both);

  RegisterWithScrollSource();
}

already_AddRefed<ScrollTimeline> ScrollTimeline::FromRule(
    const RawServoScrollTimelineRule& aRule, Document* aDocument,
    const NonOwningAnimationTarget& aTarget) {
  // FIXME: Use ScrollingElement in the next patch.
  RefPtr<ScrollTimeline> timeline = new ScrollTimeline(
      aDocument, Scroller::Auto(aTarget.mElement->OwnerDoc()));

  // FIXME: Bug 1737918: applying new spec update.
  // Note: If the rules changes after we build the scroll-timeline rule, we
  // rebuild the animtions, so does the timeline object (because now we create
  // the scroll-timeline for each animation).
  // FIXME: Bug 1738135: If the scroll timeline is hold by Element, we have to
  // update it once the rule is changed.
  timeline->mDirection = Servo_ScrollTimelineRule_GetOrientation(&aRule);
  return timeline.forget();
}

Nullable<TimeDuration> ScrollTimeline::GetCurrentTimeAsDuration() const {
  const nsIFrame* frame =
      mSource ? mSource.mElement->GetPrimaryFrame() : nullptr;
  const nsIScrollableFrame* scrollFrame = GetScrollFrame();
  if (!frame || !scrollFrame) {
    return nullptr;
  }

  const auto orientation = GetPhysicalOrientation(frame->GetWritingMode());
  // If this orientation is not ready for scrolling (i.e. the scroll range is
  // not larger than or equal to one device pixel), we make it 100%.
  if (!scrollFrame->GetAvailableScrollingDirections().contains(orientation)) {
    return TimeDuration::FromMilliseconds(SCROLL_TIMELINE_DURATION_MILLISEC);
  }

  const nsPoint& scrollOffset = scrollFrame->GetScrollPosition();
  const nsRect& scrollRange = scrollFrame->GetScrollRange();
  const bool isHorizontal = orientation == layers::ScrollDirection::eHorizontal;

  // Note: For RTL, scrollOffset.x or scrollOffset.y may be negative, e.g. the
  // range of its value is [0, -range], so we have to use the absolute value.
  double position = std::abs(isHorizontal ? scrollOffset.x : scrollOffset.y);
  double range = isHorizontal ? scrollRange.width : scrollRange.height;
  MOZ_ASSERT(range > 0.0);
  // Use the definition of interval progress to compute the progress.
  // Note: We simplify the scroll offsets to [0%, 100%], so offset weight and
  // offset index are ignored here.
  // https://drafts.csswg.org/scroll-animations-1/#progress-calculation-algorithm
  double progress = position / range;
  return TimeDuration::FromMilliseconds(progress *
                                        SCROLL_TIMELINE_DURATION_MILLISEC);
}

void ScrollTimeline::RegisterWithScrollSource() {
  if (!mSource) {
    return;
  }

  if (ScrollTimelineSet* scrollTimelineSet =
          ScrollTimelineSet::GetOrCreateScrollTimelineSet(mSource.mElement)) {
    scrollTimelineSet->AddScrollTimeline(*this);
  }
}

void ScrollTimeline::UnregisterFromScrollSource() {
  if (!mSource) {
    return;
  }

  if (ScrollTimelineSet* scrollTimelineSet =
          ScrollTimelineSet::GetScrollTimelineSet(mSource.mElement)) {
    scrollTimelineSet->RemoveScrollTimeline(*this);
    if (scrollTimelineSet->IsEmpty()) {
      ScrollTimelineSet::DestroyScrollTimelineSet(mSource.mElement);
    }
  }
}

const nsIScrollableFrame* ScrollTimeline::GetScrollFrame() const {
  if (!mSource) {
    return nullptr;
  }

  switch (mSource.mType) {
    case Scroller::Type::Auto:
      if (const PresShell* presShell =
              mSource.mElement->OwnerDoc()->GetPresShell()) {
        return presShell->GetRootScrollFrameAsScrollable();
      }
      break;
    case Scroller::Type::Other:
    default:
      return nsLayoutUtils::FindScrollableFrameFor(mSource.mElement);
  }
  return nullptr;
}

// ---------------------------------
// Methods of ScrollTimelineSet
// ---------------------------------

/* static */ ScrollTimelineSet* ScrollTimelineSet::GetScrollTimelineSet(
    Element* aElement) {
  return aElement ? static_cast<ScrollTimelineSet*>(aElement->GetProperty(
                        nsGkAtoms::scrollTimelinesProperty))
                  : nullptr;
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
