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

namespace mozilla {

NS_DECLARE_FRAME_PROPERTY(StickyScrollContainerProperty,
                          DeleteValue<StickyScrollContainer>)

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
void
StickyScrollContainer::NotifyReparentedFrameAcrossScrollFrameBoundary(nsIFrame* aFrame,
                                                                      nsIFrame* aOldParent)
{
  nsIScrollableFrame* oldScrollFrame =
    nsLayoutUtils::GetNearestScrollableFrame(aOldParent,
      nsLayoutUtils::SCROLLABLE_SAME_DOC |
      nsLayoutUtils::SCROLLABLE_INCLUDE_HIDDEN);
  if (!oldScrollFrame) {
    // XXX maybe aFrame has sticky descendants that can be sticky now, but
    // we aren't going to handle that.
    return;
  }
  FrameProperties props = static_cast<nsIFrame*>(do_QueryFrame(oldScrollFrame))->
    Properties();
  StickyScrollContainer* oldSSC = static_cast<StickyScrollContainer*>
    (props.Get(StickyScrollContainerProperty()));
  if (!oldSSC) {
    // aOldParent had no sticky descendants, so aFrame doesn't have any sticky
    // descendants, and we're done here.
    return;
  }

  auto i = oldSSC->mFrames.Length();
  while (i-- > 0) {
    nsIFrame* f = oldSSC->mFrames[i];
    StickyScrollContainer* newSSC = GetStickyScrollContainerForFrame(f);
    if (newSSC != oldSSC) {
      oldSSC->RemoveFrame(f);
      if (newSSC) {
        newSSC->AddFrame(f);
      }
    }
  }
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
    (props.Get(nsIFrame::ComputedOffsetProperty()));
  if (offsets) {
    *offsets = computedOffsets;
  } else {
    props.Set(nsIFrame::ComputedOffsetProperty(),
              new nsMargin(computedOffsets));
  }
}

void
StickyScrollContainer::ComputeStickyLimits(nsIFrame* aFrame, nsRect* aStick,
                                           nsRect* aContain) const
{
  NS_ASSERTION(nsLayoutUtils::IsFirstContinuationOrIBSplitSibling(aFrame),
               "Can't sticky position individual continuations");

  aStick->SetRect(nscoord_MIN/2, nscoord_MIN/2, nscoord_MAX, nscoord_MAX);
  aContain->SetRect(nscoord_MIN/2, nscoord_MIN/2, nscoord_MAX, nscoord_MAX);

  const nsMargin* computedOffsets = static_cast<nsMargin*>(
    aFrame->Properties().Get(nsIFrame::ComputedOffsetProperty()));
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

  nsRect rect =
    nsLayoutUtils::GetAllInFlowRectsUnion(aFrame, aFrame->GetParent());

  // Containing block limits for the position of aFrame relative to its parent.
  // The margin box of the sticky element stays within the content box of the
  // contaning-block element.
  if (cbFrame != scrolledFrame) {
    *aContain = nsLayoutUtils::
      GetAllInFlowRectsUnion(cbFrame, aFrame->GetParent(),
                             nsLayoutUtils::RECTS_USE_CONTENT_BOX);
    nsRect marginRect = nsLayoutUtils::
      GetAllInFlowRectsUnion(aFrame, aFrame->GetParent(),
                             nsLayoutUtils::RECTS_USE_MARGIN_BOX);

    // Deflate aContain by the difference between the union of aFrame's
    // continuations' margin boxes and the union of their border boxes, so that
    // by keeping aFrame within aContain, we keep the union of the margin boxes
    // within the containing block's content box.
    aContain->Deflate(marginRect - rect);

    // Deflate aContain by the border-box size, to form a constraint on the
    // upper-left corner of aFrame and continuations.
    aContain->Deflate(nsMargin(0, rect.width, rect.height, 0));
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

  // These limits are for the bounding box of aFrame's continuations. Convert
  // to limits for aFrame itself.
  nsPoint frameOffset = aFrame->GetPosition() - rect.TopLeft();
  aStick->MoveBy(frameOffset);
  aContain->MoveBy(frameOffset);
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
StickyScrollContainer::GetScrollRanges(nsIFrame* aFrame, nsRect* aOuter,
                                       nsRect* aInner) const
{
  // We need to use the first in flow; ComputeStickyLimits requires
  // this, at the very least because its call to
  // nsLayoutUtils::GetAllInFlowRectsUnion requires it.
  nsIFrame *firstCont =
    nsLayoutUtils::FirstContinuationOrIBSplitSibling(aFrame);

  nsRect stick;
  nsRect contain;
  ComputeStickyLimits(firstCont, &stick, &contain);

  aOuter->SetRect(nscoord_MIN/2, nscoord_MIN/2, nscoord_MAX, nscoord_MAX);
  aInner->SetRect(nscoord_MIN/2, nscoord_MIN/2, nscoord_MAX, nscoord_MAX);

  const nsPoint normalPosition = aFrame->GetNormalPosition();

  // Bottom and top
  if (stick.YMost() != nscoord_MAX/2) {
    aOuter->SetTopEdge(contain.y - stick.YMost());
    aInner->SetTopEdge(normalPosition.y - stick.YMost());
  }

  if (stick.y != nscoord_MIN/2) {
    aInner->SetBottomEdge(normalPosition.y - stick.y);
    aOuter->SetBottomEdge(contain.YMost() - stick.y);
  }

  // Right and left
  if (stick.XMost() != nscoord_MAX/2) {
    aOuter->SetLeftEdge(contain.x - stick.XMost());
    aInner->SetLeftEdge(normalPosition.x - stick.XMost());
  }

  if (stick.x != nscoord_MIN/2) {
    aInner->SetRightEdge(normalPosition.x - stick.x);
    aOuter->SetRightEdge(contain.XMost() - stick.x);
  }
}

void
StickyScrollContainer::PositionContinuations(nsIFrame* aFrame)
{
  NS_ASSERTION(nsLayoutUtils::IsFirstContinuationOrIBSplitSibling(aFrame),
               "Should be starting from the first continuation");
  nsPoint translation = ComputePosition(aFrame) - aFrame->GetNormalPosition();

  // Move all continuation frames by the same amount.
  for (nsIFrame* cont = aFrame; cont;
       cont = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
    cont->SetPosition(cont->GetNormalPosition() + translation);
  }
}

void
StickyScrollContainer::UpdatePositions(nsPoint aScrollPosition,
                                       nsIFrame* aSubtreeRoot)
{
#ifdef DEBUG
  {
    nsIFrame* scrollFrameAsFrame = do_QueryFrame(mScrollFrame);
    NS_ASSERTION(!aSubtreeRoot || aSubtreeRoot == scrollFrameAsFrame,
                 "If reflowing, should be reflowing the scroll frame");
  }
#endif
  mScrollPosition = aScrollPosition;

  OverflowChangedTracker oct;
  oct.SetSubtreeRoot(aSubtreeRoot);
  for (nsTArray<nsIFrame*>::size_type i = 0; i < mFrames.Length(); i++) {
    nsIFrame* f = mFrames[i];
    if (!nsLayoutUtils::IsFirstContinuationOrIBSplitSibling(f)) {
      // This frame was added in nsFrame::Init before we knew it wasn't
      // the first ib-split-sibling.
      mFrames.RemoveElementAt(i);
      --i;
      continue;
    }

    if (aSubtreeRoot) {
      // Reflowing the scroll frame, so recompute offsets.
      ComputeStickyOffsets(f);
    }
    // mFrames will only contain first continuations, because we filter in
    // nsIFrame::Init.
    PositionContinuations(f);

    f = f->GetParent();
    if (f != aSubtreeRoot) {
      for (nsIFrame* cont = f; cont;
           cont = nsLayoutUtils::GetNextContinuationOrIBSplitSibling(cont)) {
        oct.AddFrame(cont, OverflowChangedTracker::CHILDREN_CHANGED);
      }
    }
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
