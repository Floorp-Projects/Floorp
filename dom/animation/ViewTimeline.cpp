/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ViewTimeline.h"

#include "mozilla/dom/Animation.h"
#include "mozilla/dom/ElementInlines.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(ViewTimeline, ScrollTimeline, mSubject)
NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(ViewTimeline, ScrollTimeline)

/* static */
already_AddRefed<ViewTimeline> ViewTimeline::MakeNamed(
    Document* aDocument, Element* aSubject, PseudoStyleType aPseudoType,
    const StyleViewTimeline& aStyleTimeline) {
  MOZ_ASSERT(NS_IsMainThread());

  // 1. Lookup scroller. We have to find the nearest scroller from |aSubject|
  // and |aPseudoType|.
  auto [element, pseudo] = FindNearestScroller(aSubject, aPseudoType);
  auto scroller = Scroller::Nearest(const_cast<Element*>(element), pseudo);

  // 2. Create timeline.
  return MakeAndAddRef<ViewTimeline>(aDocument, scroller,
                                     aStyleTimeline.GetAxis(), aSubject,
                                     aPseudoType, aStyleTimeline.GetInset());
}

/* static */
already_AddRefed<ViewTimeline> ViewTimeline::MakeAnonymous(
    Document* aDocument, const NonOwningAnimationTarget& aTarget,
    StyleScrollAxis aAxis, const StyleViewTimelineInset& aInset) {
  // view() finds the nearest scroll container from the animation target.
  auto [element, pseudo] =
      FindNearestScroller(aTarget.mElement, aTarget.mPseudoType);
  Scroller scroller = Scroller::Nearest(const_cast<Element*>(element), pseudo);
  return MakeAndAddRef<ViewTimeline>(aDocument, scroller, aAxis,
                                     aTarget.mElement, aTarget.mPseudoType,
                                     aInset);
}

void ViewTimeline::ReplacePropertiesWith(Element* aSubjectElement,
                                         PseudoStyleType aPseudoType,
                                         const StyleViewTimeline& aNew) {
  mSubject = aSubjectElement;
  mSubjectPseudoType = aPseudoType;
  mAxis = aNew.GetAxis();
  // FIXME: Bug 1817073. We assume it is a non-animatable value for now.
  mInset = aNew.GetInset();

  for (auto* anim = mAnimationOrder.getFirst(); anim;
       anim = static_cast<LinkedListElement<Animation>*>(anim)->getNext()) {
    MOZ_ASSERT(anim->GetTimeline() == this);
    // Set this so we just PostUpdate() for this animation.
    anim->SetTimeline(this);
  }
}

Maybe<ScrollTimeline::ScrollOffsets> ViewTimeline::ComputeOffsets(
    const nsIScrollableFrame* aScrollFrame,
    layers::ScrollDirection aOrientation) const {
  MOZ_ASSERT(mSubject);
  MOZ_ASSERT(aScrollFrame);

  const Element* subjectElement =
      AnimationUtils::GetElementForRestyle(mSubject, mSubjectPseudoType);
  const nsIFrame* subject = subjectElement->GetPrimaryFrame();
  if (!subject) {
    // No principal box of the subject, so we cannot compute the offset. This
    // may happen when we clear all animation collections during unbinding from
    // the tree.
    return Nothing();
  }

  // In order to get the distance between the subject and the scrollport
  // properly, we use the position based on the domain of the scrolled frame,
  // instead of the scrollable frame.
  const nsIFrame* scrolledFrame = aScrollFrame->GetScrolledFrame();
  MOZ_ASSERT(scrolledFrame);
  const nsRect subjectRect(subject->GetOffsetTo(scrolledFrame),
                           subject->GetSize());

  // Use scrollport size (i.e. padding box size - scrollbar size), which is used
  // for calculating the view progress visibility range.
  // https://drafts.csswg.org/scroll-animations/#view-progress-visibility-range
  const nsRect scrollPort = aScrollFrame->GetScrollPortRect();

  // Adjuct the positions and sizes based on the physical axis.
  nscoord subjectPosition = subjectRect.y;
  nscoord subjectSize = subjectRect.height;
  nscoord scrollPortSize = scrollPort.height;
  if (aOrientation == layers::ScrollDirection::eHorizontal) {
    // |subjectPosition| should be the position of the start border edge of the
    // subject, so for R-L case, we have to use XMost() as the start border
    // edge of the subject, and compute its position by using the x-most side of
    // the scrolled frame as the origin on the horizontal axis.
    subjectPosition = scrolledFrame->GetWritingMode().IsPhysicalRTL()
                          ? scrolledFrame->GetSize().width - subjectRect.XMost()
                          : subjectRect.x;
    subjectSize = subjectRect.width;
    scrollPortSize = scrollPort.width;
  }

  // |sideInsets.mEnd| is used to adjust the start offset, and
  // |sideInsets.mStart| is used to adjust the end offset. This is because
  // |sideInsets.mStart| refers to logical start side [1] of the source box
  // (i.e. the box of the scrollport), where as |startOffset| refers to the
  // start of the timeline, and similarly for end side/offset. [1]
  // https://drafts.csswg.org/css-writing-modes-4/#css-start
  const auto sideInsets = ComputeInsets(aScrollFrame, aOrientation);

  // Basically, we are computing the "cover" timeline range name, which
  // represents the full range of the view progress timeline.
  // https://drafts.csswg.org/scroll-animations-1/#valdef-animation-timeline-range-cover

  // Note: `subjectPosition - scrollPortSize` means the distance between the
  // start border edge of the subject and the end edge of the scrollport.
  nscoord startOffset = subjectPosition - scrollPortSize + sideInsets.mEnd;
  // Note: `subjectPosition + subjectSize` means the position of the end border
  // edge of the subject. When it touches the start edge of the scrollport, it
  // is 100%.
  nscoord endOffset = subjectPosition + subjectSize - sideInsets.mStart;
  return Some(ScrollOffsets{startOffset, endOffset});
}

ScrollTimeline::ScrollOffsets ViewTimeline::ComputeInsets(
    const nsIScrollableFrame* aScrollFrame,
    layers::ScrollDirection aOrientation) const {
  // If view-timeline-inset is auto, it indicates to use the value of
  // scroll-padding. We use logical dimension to map that start/end offset to
  // the corresponding scroll-padding-{inline|block}-{start|end} values.
  const WritingMode wm = aScrollFrame->GetScrolledFrame()->GetWritingMode();
  const auto& scrollPadding =
      LogicalMargin(wm, aScrollFrame->GetScrollPadding());
  const bool isBlockAxis =
      mAxis == StyleScrollAxis::Block ||
      (mAxis == StyleScrollAxis::Horizontal && wm.IsVertical()) ||
      (mAxis == StyleScrollAxis::Vertical && !wm.IsVertical());

  // The percentages of view-timelne-inset is relative to the corresponding
  // dimension of the relevant scrollport.
  // https://drafts.csswg.org/scroll-animations-1/#view-timeline-inset
  const nsRect scrollPort = aScrollFrame->GetScrollPortRect();
  const nscoord percentageBasis =
      aOrientation == layers::ScrollDirection::eHorizontal ? scrollPort.width
                                                           : scrollPort.height;

  nscoord startInset =
      mInset.start.IsAuto()
          ? (isBlockAxis ? scrollPadding.BStart(wm) : scrollPadding.IStart(wm))
          : mInset.start.AsLengthPercentage().Resolve(percentageBasis);
  nscoord endInset =
      mInset.end.IsAuto()
          ? (isBlockAxis ? scrollPadding.BEnd(wm) : scrollPadding.IEnd(wm))
          : mInset.end.AsLengthPercentage().Resolve(percentageBasis);
  return {startInset, endInset};
}

}  // namespace mozilla::dom
