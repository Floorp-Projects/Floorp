/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * compute sticky positioning, both during reflow and when the scrolling
 * container scrolls
 */

#include "StickyScrollContainer.h"
#include "nsIFrame.h"
#include "nsIScrollableFrame.h"
#include "nsLayoutUtils.h"
#include "RestyleTracker.h"

using namespace mozilla::css;

namespace mozilla {

void DestroyStickyScrollContainer(void* aPropertyValue)
{
  delete static_cast<StickyScrollContainer*>(aPropertyValue);
}

NS_DECLARE_FRAME_PROPERTY(StickyScrollContainerProperty,
                          DestroyStickyScrollContainer)

StickyScrollContainer::StickyScrollContainer(nsIScrollableFrame* aScrollFrame)
  : mScrollFrame(aScrollFrame)
  , mScrollPosition()
{
  mScrollFrame->AddScrollPositionListener(this);
}

StickyScrollContainer::~StickyScrollContainer()
{
  mScrollFrame->RemoveScrollPositionListener(this);
}

// static
StickyScrollContainer*
StickyScrollContainer::GetStickyScrollContainerForFrame(nsIFrame* aFrame)
{
  nsIScrollableFrame* scrollFrame =
    nsLayoutUtils::GetNearestScrollableFrame(aFrame->GetParent(),
      nsLayoutUtils::SCROLLABLE_SAME_DOC |
      nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);
  if (!scrollFrame) {
    // We might not find any, for instance in the case of
    // <html style="position: fixed">
    return nullptr;
  }
  FrameProperties props = static_cast<nsIFrame*>(do_QueryFrame(scrollFrame))->
    Properties();
  StickyScrollContainer* s = static_cast<StickyScrollContainer*>
    (props.Get(StickyScrollContainerProperty()));
  if (!s) {
    s = new StickyScrollContainer(scrollFrame);
    props.Set(StickyScrollContainerProperty(), s);
  }
  return s;
}

// static
StickyScrollContainer*
StickyScrollContainer::GetStickyScrollContainerForScrollFrame(nsIFrame* aFrame)
{
  FrameProperties props = aFrame->Properties();
  return static_cast<StickyScrollContainer*>
    (props.Get(StickyScrollContainerProperty()));
}

static nscoord
ComputeStickySideOffset(Side aSide, const nsStyleSides& aOffset,
                        nscoord aPercentBasis)
{
  if (eStyleUnit_Auto == aOffset.GetUnit(aSide)) {
    return NS_AUTOOFFSET;
  } else {
    return nsLayoutUtils::ComputeCBDependentValue(aPercentBasis,
                                                  aOffset.Get(aSide));
  }
}

// static
void
StickyScrollContainer::ComputeStickyOffsets(nsIFrame* aFrame)
{
  nsIScrollableFrame* scrollableFrame =
    nsLayoutUtils::GetNearestScrollableFrame(aFrame->GetParent(),
      nsLayoutUtils::SCROLLABLE_SAME_DOC |
      nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);

  if (!scrollableFrame) {
    // Bail.
    return;
  }

  nsSize scrollContainerSize = scrollableFrame->GetScrolledFrame()->
    GetContentRectRelativeToSelf().Size();

  nsMargin computedOffsets;
  const nsStylePosition* position = aFrame->StylePosition();

  computedOffsets.left   = ComputeStickySideOffset(eSideLeft, position->mOffset,
                                                   scrollContainerSize.width);
  computedOffsets.right  = ComputeStickySideOffset(eSideRight, position->mOffset,
                                                   scrollContainerSize.width);
  computedOffsets.top    = ComputeStickySideOffset(eSideTop, position->mOffset,
                                                   scrollContainerSize.height);
  computedOffsets.bottom = ComputeStickySideOffset(eSideBottom, position->mOffset,
                                                   scrollContainerSize.height);

  // Store the offset
  FrameProperties props = aFrame->Properties();
  nsMargin* offsets = static_cast<nsMargin*>
    (props.Get(nsIFrame::ComputedStickyOffsetProperty()));
  if (offsets) {
    *offsets = computedOffsets;
  } else {
    props.Set(nsIFrame::ComputedStickyOffsetProperty(),
              new nsMargin(computedOffsets));
  }
}

void
StickyScrollContainer::ComputeStickyLimits(nsIFrame* aFrame, nsRect* aStick,
                                           nsRect* aContain) const
{
  aStick->SetRect(nscoord_MIN/2, nscoord_MIN/2, nscoord_MAX, nscoord_MAX);
  aContain->SetRect(nscoord_MIN/2, nscoord_MIN/2, nscoord_MAX, nscoord_MAX);

  const nsMargin* computedOffsets = static_cast<nsMargin*>(
    aFrame->Properties().Get(nsIFrame::ComputedStickyOffsetProperty()));
  if (!computedOffsets) {
    // We haven't reflowed the scroll frame yet, so offsets haven't been
    // computed. Bail.
    return;
  }

  nsIFrame* scrolledFrame = mScrollFrame->GetScrolledFrame();
  nsIFrame* cbFrame = aFrame->GetContainingBlock();
  NS_ASSERTION(cbFrame == scrolledFrame ||
    nsLayoutUtils::IsProperAncestorFrame(scrolledFrame, cbFrame),
    "Scroll frame should be an ancestor of the containing block");

  nsRect rect = aFrame->GetRect();
  nsMargin margin = aFrame->GetUsedMargin();

  // Containing block limits
  if (cbFrame != scrolledFrame) {
    nsMargin cbBorderPadding = cbFrame->GetUsedBorderAndPadding();
    aContain->SetRect(nsPoint(cbBorderPadding.left, cbBorderPadding.top) -
                      aFrame->GetParent()->GetOffsetTo(cbFrame),
                      cbFrame->GetContentRectRelativeToSelf().Size() -
                      rect.Size());
    aContain->Deflate(margin);
  }

  nsMargin sfPadding = scrolledFrame->GetUsedPadding();
  nsPoint sfOffset = aFrame->GetParent()->GetOffsetTo(scrolledFrame);

  // Top
  if (computedOffsets->top != NS_AUTOOFFSET) {
    aStick->SetTopEdge(mScrollPosition.y + sfPadding.top +
                       computedOffsets->top - sfOffset.y);
  }

  nsSize sfSize = scrolledFrame->GetContentRectRelativeToSelf().Size();

  // Bottom
  if (computedOffsets->bottom != NS_AUTOOFFSET &&
      (computedOffsets->top == NS_AUTOOFFSET ||
       rect.height <= sfSize.height - computedOffsets->TopBottom())) {
    aStick->SetBottomEdge(mScrollPosition.y + sfPadding.top + sfSize.height -
                          computedOffsets->bottom - rect.height - sfOffset.y);
  }

  uint8_t direction = cbFrame->StyleVisibility()->mDirection;

  // Left
  if (computedOffsets->left != NS_AUTOOFFSET &&
      (computedOffsets->right == NS_AUTOOFFSET ||
       direction == NS_STYLE_DIRECTION_LTR ||
       rect.width <= sfSize.width - computedOffsets->LeftRight())) {
    aStick->SetLeftEdge(mScrollPosition.x + sfPadding.left +
                        computedOffsets->left - sfOffset.x);
  }

  // Right
  if (computedOffsets->right != NS_AUTOOFFSET &&
      (computedOffsets->left == NS_AUTOOFFSET ||
       direction == NS_STYLE_DIRECTION_RTL ||
       rect.width <= sfSize.width - computedOffsets->LeftRight())) {
    aStick->SetRightEdge(mScrollPosition.x + sfPadding.left + sfSize.width -
                         computedOffsets->right - rect.width - sfOffset.x);
  }
}

nsPoint
StickyScrollContainer::ComputePosition(nsIFrame* aFrame) const
{
  nsRect stick;
  nsRect contain;
  ComputeStickyLimits(aFrame, &stick, &contain);

  nsPoint position = aFrame->GetNormalPosition();

  // For each sticky direction (top, bottom, left, right), move the frame along
  // the appropriate axis, based on the scroll position, but limit this to keep
  // the element's margin box within the containing block.
  position.y = std::max(position.y, std::min(stick.y, contain.YMost()));
  position.y = std::min(position.y, std::max(stick.YMost(), contain.y));
  position.x = std::max(position.x, std::min(stick.x, contain.XMost()));
  position.x = std::min(position.x, std::max(stick.XMost(), contain.x));

  return position;
}

void
StickyScrollContainer::UpdatePositions(nsPoint aScrollPosition,
                                       nsIFrame* aSubtreeRoot)
{
  NS_ASSERTION(!aSubtreeRoot || aSubtreeRoot == do_QueryFrame(mScrollFrame),
    "If reflowing, should be reflowing the scroll frame");
  mScrollPosition = aScrollPosition;

  OverflowChangedTracker oct;
  oct.SetSubtreeRoot(aSubtreeRoot);
  for (nsTArray<nsIFrame*>::size_type i = 0; i < mFrames.Length(); i++) {
    nsIFrame* f = mFrames[i];
    if (aSubtreeRoot) {
      // Reflowing the scroll frame, so recompute offsets.
      ComputeStickyOffsets(f);
    }
    f->SetPosition(ComputePosition(f));
    oct.AddFrame(f);
  }
  oct.Flush();
}

void
StickyScrollContainer::ScrollPositionWillChange(nscoord aX, nscoord aY)
{
}

void
StickyScrollContainer::ScrollPositionDidChange(nscoord aX, nscoord aY)
{
  UpdatePositions(nsPoint(aX, aY), nullptr);
}

} // namespace mozilla
