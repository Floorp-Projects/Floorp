/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * code for managing absolutely positioned children of a rendering
 * object that is a containing block for them
 */

#include "nsCOMPtr.h"
#include "nsAbsoluteContainingBlock.h"
#include "nsContainerFrame.h"
#include "nsIPresShell.h"
#include "nsHTMLParts.h"
#include "nsPresContext.h"

#ifdef DEBUG
#include "nsBlockFrame.h"
#endif


nsresult
nsAbsoluteContainingBlock::FirstChild(const nsIFrame* aDelegatingFrame,
                                      nsIAtom*        aListName,
                                      nsIFrame**      aFirstChild) const
{
  NS_PRECONDITION(GetChildListName() == aListName, "unexpected child list name");
  *aFirstChild = mAbsoluteFrames.FirstChild();
  return NS_OK;
}

nsresult
nsAbsoluteContainingBlock::SetInitialChildList(nsIFrame*       aDelegatingFrame,
                                               nsIAtom*        aListName,
                                               nsIFrame*       aChildList)
{
  NS_PRECONDITION(GetChildListName() == aListName, "unexpected child list name");
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aChildList);
#endif
  mAbsoluteFrames.SetFrames(aChildList);
  return NS_OK;
}

nsresult
nsAbsoluteContainingBlock::AppendFrames(nsIFrame*      aDelegatingFrame,
                                        nsIAtom*       aListName,
                                        nsIFrame*      aFrameList)
{
  NS_ASSERTION(GetChildListName() == aListName, "unexpected child list");

  // Append the frames to our list of absolutely positioned frames
#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mAbsoluteFrames.AppendFrames(nsnull, aFrameList);

  aDelegatingFrame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
  // no damage to intrinsic widths, since absolutely positioned frames can't
  // change them
  return aDelegatingFrame->PresContext()->PresShell()->
    FrameNeedsReflow(aDelegatingFrame, nsIPresShell::eResize);
}

nsresult
nsAbsoluteContainingBlock::InsertFrames(nsIFrame*      aDelegatingFrame,
                                        nsIAtom*       aListName,
                                        nsIFrame*      aPrevFrame,
                                        nsIFrame*      aFrameList)
{
  NS_ASSERTION(GetChildListName() == aListName, "unexpected child list");
  NS_ASSERTION(!aPrevFrame || aPrevFrame->GetParent() == aDelegatingFrame,
               "inserting after sibling frame with different parent");

#ifdef NS_DEBUG
  nsFrame::VerifyDirtyBitSet(aFrameList);
#endif
  mAbsoluteFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);

  aDelegatingFrame->AddStateBits(NS_FRAME_HAS_DIRTY_CHILDREN);
  // no damage to intrinsic widths, since absolutely positioned frames can't
  // change them
  return aDelegatingFrame->PresContext()->PresShell()->
    FrameNeedsReflow(aDelegatingFrame, nsIPresShell::eResize);
}

nsresult
nsAbsoluteContainingBlock::RemoveFrame(nsIFrame*       aDelegatingFrame,
                                       nsIAtom*        aListName,
                                       nsIFrame*       aOldFrame)
{
  NS_ASSERTION(GetChildListName() == aListName, "unexpected child list");

  PRBool result = mAbsoluteFrames.DestroyFrame(aOldFrame);
  NS_ASSERTION(result, "didn't find frame to delete");
  // Because positioned frames aren't part of a flow, there's no additional
  // work to do, e.g. reflowing sibling frames. And because positioned frames
  // have a view, we don't need to repaint
  return result ? NS_OK : NS_ERROR_FAILURE;
}

static void
AddFrameToChildBounds(nsIFrame* aKidFrame, nsRect* aChildBounds)
{
  NS_PRECONDITION(aKidFrame, "Must have kid frame");
  
  if (!aChildBounds) {
    return;
  }

  // Add in the child's bounds
  nsRect kidBounds = aKidFrame->GetRect();
  nsRect* kidOverflow = aKidFrame->GetOverflowAreaProperty();
  if (kidOverflow) {
    // Put it in the parent's coordinate system
    kidBounds = *kidOverflow + kidBounds.TopLeft();
  }
  aChildBounds->UnionRect(*aChildBounds, kidBounds);
}

nsresult
nsAbsoluteContainingBlock::Reflow(nsIFrame*                aDelegatingFrame,
                                  nsPresContext*          aPresContext,
                                  const nsHTMLReflowState& aReflowState,
                                  nscoord                  aContainingBlockWidth,
                                  nscoord                  aContainingBlockHeight,
                                  PRBool                   aCBWidthChanged,
                                  PRBool                   aCBHeightChanged,
                                  nsRect*                  aChildBounds)
{
  // Initialize OUT parameter
  if (aChildBounds)
    aChildBounds->SetRect(0, 0, 0, 0);

  PRBool reflowAll = aReflowState.ShouldReflowAllKids();

  nsIFrame* kidFrame;
  for (kidFrame = mAbsoluteFrames.FirstChild(); kidFrame; kidFrame = kidFrame->GetNextSibling()) {
    if (reflowAll ||
        (kidFrame->GetStateBits() &
           (NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN)) ||
        FrameDependsOnContainer(kidFrame, aCBWidthChanged, aCBHeightChanged)) {
      // Reflow the frame
      nsReflowStatus  kidStatus;
      ReflowAbsoluteFrame(aDelegatingFrame, aPresContext, aReflowState, aContainingBlockWidth,
                          aContainingBlockHeight, kidFrame, kidStatus);

    }
    AddFrameToChildBounds(kidFrame, aChildBounds);
  }
  return NS_OK;
}

static PRBool IsFixedPaddingSize(nsStyleUnit aUnit) {
  return aUnit == eStyleUnit_Coord || aUnit == eStyleUnit_Null;
}
static PRBool IsFixedMarginSize(nsStyleUnit aUnit) {
  return aUnit == eStyleUnit_Coord || aUnit == eStyleUnit_Null;
}
static PRBool IsFixedMaxSize(nsStyleUnit aUnit) {
  return aUnit == eStyleUnit_Null || aUnit == eStyleUnit_Coord;
}

PRBool
nsAbsoluteContainingBlock::FrameDependsOnContainer(nsIFrame* f,
                                                   PRBool aCBWidthChanged,
                                                   PRBool aCBHeightChanged)
{
  const nsStylePosition* pos = f->GetStylePosition();
  // See if f's position might have changed because it depends on a
  // placeholder's position
  // This can happen in the following cases:
  // 1) Vertical positioning.  "top" must be auto and "bottom" must be auto
  //    (otherwise the vertical position is completely determined by
  //    whichever of them is not auto and the height).
  // 2) Horizontal positioning.  "left" must be auto and "right" must be auto
  //    (otherwise the horizontal position is completely determined by
  //    whichever of them is not auto and the width).
  // See nsHTMLReflowState::InitAbsoluteConstraints -- these are the
  // only cases when we call CalculateHypotheticalBox().
  if ((pos->mOffset.GetTopUnit() == eStyleUnit_Auto &&
       pos->mOffset.GetBottomUnit() == eStyleUnit_Auto) ||
      (pos->mOffset.GetLeftUnit() == eStyleUnit_Auto &&
       pos->mOffset.GetRightUnit() == eStyleUnit_Auto)) {
    return PR_TRUE;
  }
  if (!aCBWidthChanged && !aCBHeightChanged) {
    // skip getting style data
    return PR_FALSE;
  }
  const nsStylePadding* padding = f->GetStylePadding();
  const nsStyleMargin* margin = f->GetStyleMargin();
  if (aCBWidthChanged) {
    // See if f's width might have changed.
    // If border-left, border-right, padding-left, padding-right,
    // width, min-width, and max-width are all lengths, 'none', or enumerated,
    // then our frame width does not depend on the parent width.
    // Note that borders never depend on the parent width
    if (pos->mWidth.GetUnit() != eStyleUnit_Coord ||
        pos->mMinWidth.GetUnit() != eStyleUnit_Coord ||
        !IsFixedMaxSize(pos->mMaxWidth.GetUnit()) ||
        !IsFixedPaddingSize(padding->mPadding.GetLeftUnit()) ||
        !IsFixedPaddingSize(padding->mPadding.GetRightUnit())) {
      return PR_TRUE;
    }

    // See if f's position might have changed. If we're RTL then the
    // rules are slightly different. We'll assume percentage or auto
    // margins will always induce a dependency on the size
    if (!IsFixedMarginSize(margin->mMargin.GetLeftUnit()) ||
        !IsFixedMarginSize(margin->mMargin.GetRightUnit())) {
      return PR_TRUE;
    }
    if (f->GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL) {
      // Note that even if 'left' is a length, our position can
      // still depend on the containing block width, because if
      // 'right' is also a length we will discard 'left' and be
      // positioned relative to the containing block right edge.
      // 'left' length and 'right' auto is the only combination
      // we can be sure of.
      if (pos->mOffset.GetLeftUnit() != eStyleUnit_Coord ||
          pos->mOffset.GetRightUnit() != eStyleUnit_Auto) {
        return PR_TRUE;
      }
    } else {
      if (pos->mOffset.GetLeftUnit() != eStyleUnit_Coord) {
        return PR_TRUE;
      }
    }
  }
  if (aCBHeightChanged) {
    // See if f's height might have changed.
    // If border-top, border-bottom, padding-top, padding-bottom,
    // min-height, and max-height are all lengths or 'none',
    // and height is a length or height and bottom are auto and top is not auto,
    // then our frame height does not depend on the parent height.
    // Note that borders never depend on the parent height
    if (!(pos->mHeight.GetUnit() == eStyleUnit_Coord ||
          (pos->mHeight.GetUnit() == eStyleUnit_Auto &&
           pos->mOffset.GetBottomUnit() == eStyleUnit_Auto &&
           pos->mOffset.GetTopUnit() != eStyleUnit_Auto)) ||
        pos->mMinHeight.GetUnit() != eStyleUnit_Coord ||
        !IsFixedMaxSize(pos->mMaxHeight.GetUnit()) ||
        !IsFixedPaddingSize(padding->mPadding.GetTopUnit()) ||
        !IsFixedPaddingSize(padding->mPadding.GetBottomUnit())) { 
      return PR_TRUE;
    }
      
    // See if f's position might have changed.
    if (!IsFixedMarginSize(margin->mMargin.GetTopUnit()) ||
        !IsFixedMarginSize(margin->mMargin.GetBottomUnit())) {
      return PR_TRUE;
    }
    if (pos->mOffset.GetTopUnit() != eStyleUnit_Coord) {
      return PR_TRUE;
    }
  }
  return PR_FALSE;
}

void
nsAbsoluteContainingBlock::DestroyFrames(nsIFrame* aDelegatingFrame)
{
  mAbsoluteFrames.DestroyFrames();
}

// XXX Optimize the case where it's a resize reflow and the absolutely
// positioned child has the exact same size and position and skip the
// reflow...

// When bug 154892 is checked in, make sure that when 
// GetChildListName() == nsGkAtoms::fixedList, the height is unconstrained.
// since we don't allow replicated frames to split.

nsresult
nsAbsoluteContainingBlock::ReflowAbsoluteFrame(nsIFrame*                aDelegatingFrame,
                                               nsPresContext*          aPresContext,
                                               const nsHTMLReflowState& aReflowState,
                                               nscoord                  aContainingBlockWidth,
                                               nscoord                  aContainingBlockHeight,
                                               nsIFrame*                aKidFrame,
                                               nsReflowStatus&          aStatus)
{
#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout,nsBlockFrame::gNoiseIndent);
    printf("abs pos ");
    if (nsnull != aKidFrame) {
      nsIFrameDebug*  frameDebug;
      if (NS_SUCCEEDED(CallQueryInterface(aKidFrame, &frameDebug))) {
        nsAutoString name;
        frameDebug->GetFrameName(name);
        printf("%s ", NS_LossyConvertUTF16toASCII(name).get());
      }
    }

    char width[16];
    char height[16];
    PrettyUC(aReflowState.availableWidth, width);
    PrettyUC(aReflowState.availableHeight, height);
    printf(" a=%s,%s ", width, height);
    PrettyUC(aReflowState.ComputedWidth(), width);
    PrettyUC(aReflowState.mComputedHeight, height);
    printf("c=%s,%s \n", width, height);
  }
  AutoNoisyIndenter indent(nsBlockFrame::gNoisy);
#endif // DEBUG

  // Store position and overflow rect so taht we can invalidate the correct
  // area if the position changes
  nsRect oldOverflowRect(aKidFrame->GetOverflowRect() +
                         aKidFrame->GetPosition());
  nsRect oldRect = aKidFrame->GetRect();

  nsresult  rv;
  // Get the border values
  const nsMargin& border = aReflowState.mStyleBorder->GetBorder();

  nscoord availWidth = aContainingBlockWidth;
  if (availWidth == -1) {
    NS_ASSERTION(aReflowState.ComputedWidth() != NS_UNCONSTRAINEDSIZE,
                 "Must have a useful width _somewhere_");
    availWidth =
      aReflowState.ComputedWidth() + aReflowState.mComputedPadding.LeftRight();
  }
    
  nsHTMLReflowMetrics kidDesiredSize;
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, aKidFrame,
                                   nsSize(availWidth, NS_UNCONSTRAINEDSIZE),
                                   aContainingBlockWidth,
                                   aContainingBlockHeight);

  // Send the WillReflow() notification and position the frame
  aKidFrame->WillReflow(aPresContext);

  // XXXldb We can simplify this if we come up with a better way to
  // position views.
  nscoord x;
  if (NS_AUTOOFFSET == kidReflowState.mComputedOffsets.left) {
    // Just use the current x-offset
    x = aKidFrame->GetPosition().x;
  } else {
    x = border.left + kidReflowState.mComputedOffsets.left + kidReflowState.mComputedMargin.left;
  }
  aKidFrame->SetPosition(nsPoint(x, border.top +
                                    kidReflowState.mComputedOffsets.top +
                                    kidReflowState.mComputedMargin.top));

  // Do the reflow
  rv = aKidFrame->Reflow(aPresContext, kidDesiredSize, kidReflowState, aStatus);

  // If we're solving for 'left' or 'top', then compute it now that we know the
  // width/height
  if ((NS_AUTOOFFSET == kidReflowState.mComputedOffsets.left) ||
      (NS_AUTOOFFSET == kidReflowState.mComputedOffsets.top)) {
    if (-1 == aContainingBlockWidth) {
      // Get the containing block width/height
      kidReflowState.ComputeContainingBlockRectangle(aPresContext,
                                                     &aReflowState,
                                                     aContainingBlockWidth,
                                                     aContainingBlockHeight);
    }

    if (NS_AUTOOFFSET == kidReflowState.mComputedOffsets.left) {
      NS_ASSERTION(NS_AUTOOFFSET != kidReflowState.mComputedOffsets.right,
                   "Can't solve for both left and right");
      kidReflowState.mComputedOffsets.left = aContainingBlockWidth -
                                             kidReflowState.mComputedOffsets.right -
                                             kidReflowState.mComputedMargin.right -
                                             kidDesiredSize.width -
                                             kidReflowState.mComputedMargin.left;
    }
    if (NS_AUTOOFFSET == kidReflowState.mComputedOffsets.top) {
      kidReflowState.mComputedOffsets.top = aContainingBlockHeight -
                                            kidReflowState.mComputedOffsets.bottom -
                                            kidReflowState.mComputedMargin.bottom -
                                            kidDesiredSize.height -
                                            kidReflowState.mComputedMargin.top;
    }
  }

  // Position the child relative to our padding edge
  nsRect  rect(border.left + kidReflowState.mComputedOffsets.left + kidReflowState.mComputedMargin.left,
               border.top + kidReflowState.mComputedOffsets.top + kidReflowState.mComputedMargin.top,
               kidDesiredSize.width, kidDesiredSize.height);
  aKidFrame->SetRect(rect);

  // Size and position the view and set its opacity, visibility, content
  // transparency, and clip
  nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, aKidFrame,
                                             aKidFrame->GetView(),
                                             &kidDesiredSize.mOverflowArea);

  if (oldRect.TopLeft() != rect.TopLeft() || 
      (aDelegatingFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW) ||
      ((aKidFrame->GetStyleDisplay()->mClipFlags & NS_STYLE_CLIP_RECT) && 
       (kidDesiredSize.mOverflowArea != oldOverflowRect))) {
    // The frame moved; we have to invalidate the whole frame
    // because the children may have moved after they were reflowed
    // We also have to invalidate when we're clipping and the overflow
    // changes; the style code doesn't know how to deal with this case
    // XXX This could be optimized in some cases, especially clipping changes
    aKidFrame->GetParent()->Invalidate(oldOverflowRect);
    aKidFrame->GetParent()->Invalidate(kidDesiredSize.mOverflowArea +
                                       rect.TopLeft());
    nsContainerFrame::PositionChildViews(aKidFrame);
  } else if (oldRect.Size() != rect.Size()) {
    // Invalidate the area where the frame changed size.
    nscoord innerWidth = PR_MIN(oldRect.width, rect.width);
    nscoord innerHeight = PR_MIN(oldRect.height, rect.height);
    nscoord outerWidth = PR_MAX(oldRect.width, rect.width);
    nscoord outerHeight = PR_MAX(oldRect.height, rect.height);
    aKidFrame->GetParent()->Invalidate(
        nsRect(rect.x + innerWidth, rect.y, outerWidth - innerWidth, outerHeight));
    // Invalidate the horizontal strip
    aKidFrame->GetParent()->Invalidate(
        nsRect(rect.x, rect.y + innerHeight, outerWidth, outerHeight - innerHeight));
  }
  aKidFrame->DidReflow(aPresContext, &kidReflowState, NS_FRAME_REFLOW_FINISHED);

#ifdef DEBUG
  if (nsBlockFrame::gNoisyReflow) {
    nsFrame::IndentBy(stdout,nsBlockFrame::gNoiseIndent - 1);
    printf("abs pos ");
    if (nsnull != aKidFrame) {
      nsIFrameDebug*  frameDebug;
      if (NS_SUCCEEDED(CallQueryInterface(aKidFrame, &frameDebug))) {
        nsAutoString name;
        frameDebug->GetFrameName(name);
        printf("%s ", NS_LossyConvertUTF16toASCII(name).get());
      }
    }
    printf("%p rect=%d,%d,%d,%d", aKidFrame, rect.x, rect.y, rect.width, rect.height);
    printf("\n");
  }
#endif

  return rv;
}

#ifdef DEBUG
 void nsAbsoluteContainingBlock::PrettyUC(nscoord aSize,
                        char*   aBuf)
{
  if (NS_UNCONSTRAINEDSIZE == aSize) {
    strcpy(aBuf, "UC");
  }
  else {
    if((PRInt32)0xdeadbeef == aSize)
    {
      strcpy(aBuf, "deadbeef");
    }
    else {
      sprintf(aBuf, "%d", aSize);
    }
  }
}
#endif
