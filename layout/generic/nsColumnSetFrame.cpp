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
 *   Robert O'Callahan <roc@ocallahan.org>
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

#include "nsHTMLContainerFrame.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsPresContext.h"
#include "nsHTMLParts.h"
#include "nsLayoutAtoms.h"
#include "nsStyleConsts.h"
#include "nsCOMPtr.h"
#include "nsReflowPath.h"

class nsColumnSetFrame : public nsHTMLContainerFrame {
public:
  nsColumnSetFrame();

  NS_IMETHOD SetInitialChildList(nsPresContext* aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD Reflow(nsPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
                               
  NS_IMETHOD  AppendFrames(nsPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  InsertFrames(nsPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  RemoveFrame(nsPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);

  virtual nsIFrame* GetContentInsertionFrame() {
    return GetFirstChild(nsnull)->GetContentInsertionFrame();
  }

  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const {
    return MakeFrameName(NS_LITERAL_STRING("ColumnSet"), aResult);
  }
#endif

protected:
  nscoord        mLastBalanceHeight;
  nsReflowStatus mLastFrameStatus;

  virtual PRIntn GetSkipSides() const;

  /**
   * These are the parameters that control the layout of columns.
   */
  struct ReflowConfig {
    PRInt32 mBalanceColCount;
    nscoord mColWidth;
    nscoord mExpectedWidthLeftOver;
    nscoord mColGap;
    nscoord mColMaxHeight;
  };
  
  /**
   * Similar to nsBlockFrame::DrainOverflowLines. Locate any columns not
   * handled by our prev-in-flow, and any columns sitting on our own
   * overflow list, and put them in our primary child list for reflowing.
   */
  void DrainOverflowColumns();

  /**
   * The basic reflow strategy is to call this function repeatedly to
   * obtain specific parameters that determine the layout of the
   * columns. This function will compute those parameters from the CSS
   * style. This function will also be responsible for implementing
   * the state machine that controls column balancing.
   */
  ReflowConfig ChooseColumnStrategy(const nsHTMLReflowState& aReflowState);

  /**
   * Reflow column children. Returns PR_TRUE iff the content that was reflowed 
   * fit into the mColMaxHeight.
   */
  PRBool ReflowChildren(nsHTMLReflowMetrics& aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowReason aReason,
                        nsReflowStatus& aStatus,
                        const ReflowConfig& aConfig,
                        PRBool aLastColumnUnbounded);
};

/**
 * Tracking issues:
 *
 * XXX cursor movement around the top and bottom of colums seems to make the editor
 * lose the caret.
 *
 * XXX should we support CSS columns applied to table elements?
 */
nsresult
NS_NewColumnSetFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRUint32 aStateFlags)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");

  nsColumnSetFrame* it = new (aPresShell) nsColumnSetFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // set the state flags (if any are provided)
  it->AddStateBits(aStateFlags);
  
  *aNewFrame = it;
  return NS_OK;
}

nsColumnSetFrame::nsColumnSetFrame()
  : nsHTMLContainerFrame(), mLastBalanceHeight(NS_INTRINSICSIZE),
    mLastFrameStatus(NS_FRAME_COMPLETE)
{
}

nsIAtom*
nsColumnSetFrame::GetType() const
{
  return nsLayoutAtoms::columnSetFrame;
}

NS_IMETHODIMP
nsColumnSetFrame::SetInitialChildList(nsPresContext* aPresContext,
                                   nsIAtom*        aListName,
                                   nsIFrame*       aChildList)
{
  NS_ASSERTION(!aListName, "Only default child list supported");
  NS_ASSERTION(aChildList && !aChildList->GetNextSibling(),
               "initial child list must have exactly one child");
  // Queue up the frames for the content frame
  return nsHTMLContainerFrame::SetInitialChildList(aPresContext, nsnull, aChildList);
}

static nscoord GetAvailableContentWidth(const nsHTMLReflowState& aReflowState) {
  if (aReflowState.availableWidth == NS_INTRINSICSIZE) {
    return NS_INTRINSICSIZE;
  }
  nscoord borderPaddingWidth =
    aReflowState.mComputedBorderPadding.left +
    aReflowState.mComputedBorderPadding.right;
  return PR_MAX(0, aReflowState.availableWidth - borderPaddingWidth);
}

static nscoord GetAvailableContentHeight(const nsHTMLReflowState& aReflowState) {
  if (aReflowState.availableHeight == NS_INTRINSICSIZE) {
    return NS_INTRINSICSIZE;
  }
  nscoord borderPaddingHeight =
    aReflowState.mComputedBorderPadding.top +
    aReflowState.mComputedBorderPadding.bottom;
  return PR_MAX(0, aReflowState.availableHeight - borderPaddingHeight);
}

nsColumnSetFrame::ReflowConfig
nsColumnSetFrame::ChooseColumnStrategy(const nsHTMLReflowState& aReflowState)
{
  const nsStyleColumn* colStyle = GetStyleColumn();
  nscoord availContentWidth = GetAvailableContentWidth(aReflowState);
  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE) {
    availContentWidth = aReflowState.mComputedWidth;
  }
  nscoord colHeight = GetAvailableContentHeight(aReflowState);
  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE) {
    colHeight = aReflowState.mComputedHeight;
  }

  nscoord colGap = 0;
  switch (colStyle->mColumnGap.GetUnit()) {
    case eStyleUnit_Coord:
      colGap = colStyle->mColumnGap.GetCoordValue();
      break;
    case eStyleUnit_Percent:
      if (availContentWidth != NS_INTRINSICSIZE) {
        colGap = NSToCoordRound(colStyle->mColumnGap.GetPercentValue()*availContentWidth);
      }
      break;
    default:
      NS_NOTREACHED("Unknown gap type");
      break;
  }

  PRInt32 numColumns = colStyle->mColumnCount;

  nscoord colWidth = NS_INTRINSICSIZE;
  if (colStyle->mColumnWidth.GetUnit() == eStyleUnit_Coord) {
    colWidth = colStyle->mColumnWidth.GetCoordValue();

    // Reduce column count if necesary to make columns fit in the
    // available width. Compute max number of columns that fit in
    // availContentWidth, satisfying colGap*(maxColumns - 1) +
    // colWidth*maxColumns <= availContentWidth
    if (availContentWidth != NS_INTRINSICSIZE && colWidth + colGap > 0
        && numColumns > 0) {
      // This expression uses truncated rounding, which is what we
      // want
      PRInt32 maxColumns = (availContentWidth + colGap)/(colGap + colWidth);
      numColumns = PR_MAX(1, PR_MIN(numColumns, maxColumns));
    }
  } else if (numColumns > 0 && availContentWidth != NS_INTRINSICSIZE) {
    nscoord widthMinusGaps = availContentWidth - colGap*(numColumns - 1);
    colWidth = widthMinusGaps/numColumns;
  }
  // Take care of the situation where there's only one column but it's
  // still too wide
  colWidth = PR_MAX(1, PR_MIN(colWidth, availContentWidth));

  nscoord expectedWidthLeftOver = 0;

  if (colWidth != NS_INTRINSICSIZE && availContentWidth != NS_INTRINSICSIZE) {
    // distribute leftover space

    // First, determine how many columns will be showing if the column
    // count is auto
    if (numColumns <= 0) {
      // choose so that colGap*(nominalColumnCount - 1) +
      // colWidth*nominalColumnCount is nearly availContentWidth
      // make sure to round down
      numColumns = (availContentWidth + colGap)/(colGap + colWidth);
    }

    // Compute extra space and divide it among the columns
    nscoord extraSpace = availContentWidth - (colWidth*numColumns + colGap*(numColumns - 1));
    nscoord extraToColumns = extraSpace/numColumns;
    colWidth += extraToColumns;
    expectedWidthLeftOver = extraSpace - (extraToColumns*numColumns);
  }

  // NOTE that the non-balancing behavior for non-auto computed height
  // is not in the CSS3 columns draft as of 18 January 2001
  if (aReflowState.mComputedHeight == NS_INTRINSICSIZE) {
    // Balancing!
    if (numColumns <= 0) {
      // Hmm, auto column count, column width or available width is unknown,
      // and balancing is required. Let's just use one column then.
      numColumns = 1;
    }
    colHeight = PR_MIN(mLastBalanceHeight, GetAvailableContentHeight(aReflowState));
  } else {
    // No balancing, so don't limit the column count
    numColumns = PR_INT32_MAX;
  }

#ifdef DEBUG_roc
  printf("*** nsColumnSetFrame::ChooseColumnStrategy: numColumns=%d, colWidth=%d, expectedWidthLeftOver=%d, colHeight=%d, colGap=%d\n",
         numColumns, colWidth, expectedWidthLeftOver, colHeight, colGap);
#endif
  ReflowConfig config = { numColumns, colWidth, expectedWidthLeftOver, colGap, colHeight };
  return config;
}

// XXX copied from nsBlockFrame, should this be moved to nsContainerFrame?
static void
PlaceFrameView(nsIFrame* aFrame)
{
  if (aFrame->HasView())
    nsContainerFrame::PositionFrameView(aFrame->GetPresContext(), aFrame);
  else
    nsContainerFrame::PositionChildViews(aFrame->GetPresContext(), aFrame);
}

static void MoveChildTo(nsIFrame* aParent, nsIFrame* aChild, nsPoint aOrigin) {
  if (aChild->GetPosition() == aOrigin) {
    return;
  }
  
  nsRect* overflowArea = aChild->GetOverflowAreaProperty(PR_FALSE);
  nsRect r = overflowArea ? *overflowArea : nsRect(nsPoint(0, 0), aChild->GetSize());
  r += aChild->GetPosition();
  aParent->Invalidate(r);
  r -= aChild->GetPosition();
  aChild->SetPosition(aOrigin);
  r += aOrigin;
  aParent->Invalidate(r);
  PlaceFrameView(aChild);
}

PRBool
nsColumnSetFrame::ReflowChildren(nsHTMLReflowMetrics&     aDesiredSize,
                              const nsHTMLReflowState& aReflowState,
                              nsReflowReason           aKidReason,
                              nsReflowStatus&          aStatus,
                              const ReflowConfig&      aConfig,
                              PRBool                   aUnboundedLastColumn) {
  PRBool allFit = PR_TRUE;
  PRBool RTL = GetStyleVisibility()->mDirection == NS_STYLE_DIRECTION_RTL;
  PRBool shrinkingHeightOnly = aKidReason == eReflowReason_Resize &&
    mLastBalanceHeight > aConfig.mColMaxHeight;
  
#ifdef DEBUG_roc
  printf("*** Doing column reflow pass: mLastBalanceHeight=%d, mColMaxHeight=%d, RTL=%d\n, mBalanceColCount=%d, mColWidth=%d, mColGap=%d\n",
         mLastBalanceHeight, aConfig.mColMaxHeight, RTL, aConfig.mBalanceColCount,
         aConfig.mColWidth, aConfig.mColGap);
#endif

  DrainOverflowColumns();
  
  if (mLastBalanceHeight != aConfig.mColMaxHeight) {
    mLastBalanceHeight = aConfig.mColMaxHeight;
    // XXX Seems like this could fire if incremental reflow pushed the column set
    // down so we reflow incrementally with a different available height.
    // We need a way to do an incremental reflow and be sure availableHeight
    // changes are taken account of! Right now I think block frames with absolute
    // children might exit early.
    NS_ASSERTION(aKidReason != eReflowReason_Incremental,
                 "incremental reflow should not have changed the balance height");
  }

  // get our border and padding
  const nsMargin &borderPadding = aReflowState.mComputedBorderPadding;
  
  nsRect contentRect(0, 0, 0, 0);
  aDesiredSize.mMaxElementWidth = 0;
  nsRect overflowRect(0, 0, 0, 0);
  
  nsIFrame* child = mFrames.FirstChild();
  nsPoint childOrigin = nsPoint(borderPadding.left, borderPadding.top);
  // For RTL, figure out where the last column's left edge should be. Since the
  // columns might not fill the frame exactly, we need to account for the
  // slop. Otherwise we'll waste time moving the columns by some tiny
  // amount unnecessarily.
  nscoord targetX = borderPadding.left;
  if (RTL) {
    nscoord availWidth = aReflowState.availableWidth;
    if (aReflowState.mComputedWidth != NS_INTRINSICSIZE) {
      availWidth = aReflowState.mComputedWidth;
    }
    if (availWidth != NS_INTRINSICSIZE) {
      childOrigin.x += availWidth - aConfig.mColWidth;
      targetX += aConfig.mExpectedWidthLeftOver;
#ifdef DEBUG_roc
      printf("*** childOrigin.x = %d\n", childOrigin.x);
#endif
    }
  }
  int columnCount = 0;
  PRBool reflowNext = PR_FALSE;

  while (child) {
    // Try to skip reflowing the child. We can't skip if the child is dirty. We also can't
    // skip if the next column is dirty, because the next column's first line(s)
    // might be pullable back to this column.
    PRBool skipIncremental = aKidReason == eReflowReason_Incremental
      && !(child->GetStateBits() & NS_FRAME_IS_DIRTY)
      && (!child->GetNextSibling()
          || !(child->GetNextSibling()->GetStateBits() & NS_FRAME_IS_DIRTY))
      && !aDesiredSize.mComputeMEW;
    PRBool skipResizeHeightShrink = shrinkingHeightOnly
      && child->GetSize().height <= aConfig.mColMaxHeight;
    if (!reflowNext && (skipIncremental || skipResizeHeightShrink)) {
      // This child does not need to be reflowed, but we may need to move it
      MoveChildTo(this, child, childOrigin);
      
      // If this is the last frame then make sure we get the right status
      if (child->GetNextSibling()) {
        aStatus = NS_FRAME_NOT_COMPLETE;
      } else {
        aStatus = mLastFrameStatus;
      }
#ifdef DEBUG_roc
      printf("*** Skipping child #%d %p (incremental %d, resize height shrink %d): status = %d\n",
             columnCount, (void*)child, skipIncremental, skipResizeHeightShrink, aStatus);
#endif
    } else {
      nsSize availSize(aConfig.mColWidth, aConfig.mColMaxHeight);
      
      if (aUnboundedLastColumn && columnCount == aConfig.mBalanceColCount - 1) {
        availSize.height = GetAvailableContentHeight(aReflowState);
      }
  
      nsReflowReason tmpReason = aKidReason;
      if (reflowNext && aKidReason == eReflowReason_Incremental
          && !(child->GetStateBits() & NS_FRAME_IS_DIRTY)) {
        // If this frame was not being incrementally reflowed but was
        // just reflowed because the previous frame wants us to
        // reflow, then force this child to reflow its dirty lines!
        // XXX what we should really do here is add the child block to
        // the incremental reflow path! Currently I think if there's an
        // incremental reflow targeted only at the absolute frames of a
        // column, then the column will be dirty BUT reflowing it will
        // not reflow any lines affected by the prev-in-flow!
        tmpReason = eReflowReason_Dirty;
      }

      nsHTMLReflowState kidReflowState(GetPresContext(), aReflowState, child,
                                       availSize, tmpReason);
                                       
#ifdef DEBUG_roc
      printf("*** Reflowing child #%d %p: reason = %d, availHeight=%d\n",
             columnCount, (void*)child, tmpReason, availSize.height);
#endif

      // Note if the column's next in flow is not being changed by this incremental reflow.
      // This may allow the current column to avoid trying to pull lines from the next column.
      if (child->GetNextSibling() && aKidReason == eReflowReason_Incremental &&
        !(child->GetNextSibling()->GetStateBits() & NS_FRAME_IS_DIRTY)) {
        kidReflowState.mFlags.mNextInFlowUntouched = PR_TRUE;
      }
    
      nsHTMLReflowMetrics kidDesiredSize(aDesiredSize.mComputeMEW, aDesiredSize.mFlags);

      // XXX it would be cool to consult the space manager for the
      // previous block to figure out the region of floats from the
      // previous column that extend into this column, and subtract
      // that region from the new space manager.  So you could stick a
      // really big float in the first column and text in following
      // columns would flow around it.

      // Reflow the frame
      ReflowChild(child, GetPresContext(), kidDesiredSize, kidReflowState,
                  childOrigin.x + kidReflowState.mComputedMargin.left,
                  childOrigin.y + kidReflowState.mComputedMargin.top,
                  0, aStatus);

      if (kidDesiredSize.height > aConfig.mColMaxHeight) {
        allFit = PR_FALSE;
      }
      
      reflowNext = (aStatus & NS_FRAME_REFLOW_NEXTINFLOW) != 0;
    
#ifdef DEBUG_roc
      printf("*** Reflowed child #%d %p: status = %d, desiredSize=%d,%d\n",
             columnCount, (void*)child, aStatus, kidDesiredSize.width, kidDesiredSize.height);
#endif

      NS_FRAME_TRACE_REFLOW_OUT("Column::Reflow", aStatus);

      FinishReflowChild(child, GetPresContext(), &kidReflowState, 
                        kidDesiredSize, childOrigin.x, childOrigin.y, 0);

      if (aDesiredSize.mComputeMEW) {
        aDesiredSize.mMaxElementWidth = PR_MAX(aDesiredSize.mMaxElementWidth,
                                               kidDesiredSize.mMaxElementWidth);
      }
    }

    contentRect.UnionRect(contentRect, child->GetRect());

    ConsiderChildOverflow(GetPresContext(), overflowRect, child);

    // Build a continuation column if necessary
    nsIFrame* kidNextInFlow = child->GetNextInFlow();

    if (NS_FRAME_IS_COMPLETE(aStatus) && !NS_FRAME_IS_TRUNCATED(aStatus)) {
      NS_ASSERTION(!kidNextInFlow, "next in flow should have been deleted");
      break;
    } else {
      ++columnCount;
      // Make sure that the column has a next-in-flow. If not, we must
      // create one to hold the overflowing stuff, even if we're just
      // going to put it on our overflow list and let *our*
      // next in flow handle it.
      if (!kidNextInFlow) {
        NS_ASSERTION(aStatus & NS_FRAME_REFLOW_NEXTINFLOW,
                     "We have to create a continuation, but the block doesn't want us to reflow it?");

        // We need to create a continuing column
        nsIFrame* continuation;
        nsresult rv = CreateNextInFlow(GetPresContext(), this, child, continuation);
        
        if (NS_FAILED(rv)) {
          NS_NOTREACHED("Couldn't create continuation");
          break;
        }
        
        continuation->AddStateBits(NS_BLOCK_SPACE_MGR);

        // Do an initial reflow if we're going to reflow this thing.
        aKidReason = eReflowReason_Initial;
      }
        
      if (columnCount >= aConfig.mBalanceColCount) {
        // No more columns allowed here. Stop.
        aStatus |= NS_FRAME_REFLOW_NEXTINFLOW;
        
        // Move any of our leftover columns to our overflow list. Our
        // next-in-flow will eventually pick them up.
        nsIFrame* continuationColumns = child->GetNextSibling();
        if (continuationColumns) {
          SetOverflowFrames(GetPresContext(), continuationColumns);
          child->SetNextSibling(nsnull);
        }
        break;
      }
    }

    // Advance to the next column
    child = child->GetNextSibling();

    if (child) {
      if (!RTL) {
        childOrigin.x += aConfig.mColWidth + aConfig.mColGap;
      } else {
        childOrigin.x -= aConfig.mColWidth + aConfig.mColGap;
      }
      
#ifdef DEBUG_roc
      printf("*** NEXT CHILD ORIGIN.x = %d\n", childOrigin.x);
#endif
    }
  }
  
  // If we're doing RTL, we need to make sure our last column is at the left-hand side of the frame.
  if (RTL && childOrigin.x != targetX) {
    overflowRect = nsRect(0, 0, 0, 0);
    contentRect = nsRect(0, 0, 0, 0);
    PRInt32 deltaX = targetX - childOrigin.x;
#ifdef DEBUG_roc
    printf("*** CHILDORIGIN.x = %d, targetX = %d, DELTAX = %d\n", childOrigin.x, targetX, deltaX);
#endif
    for (child = mFrames.FirstChild(); child; child = child->GetNextSibling()) {
      MoveChildTo(this, child, child->GetPosition() + nsPoint(deltaX, 0));
      ConsiderChildOverflow(GetPresContext(), overflowRect, child);
      contentRect.UnionRect(contentRect, child->GetRect());
    }
  }

  mLastFrameStatus = aStatus;
  
  // contentRect included the borderPadding.left,borderPadding.top of the child rects
  contentRect -= nsPoint(borderPadding.left, borderPadding.top);
  
  nsSize contentSize = nsSize(contentRect.XMost(), contentRect.YMost());

  // Apply computed and min/max values
  if (aReflowState.mComputedHeight != NS_INTRINSICSIZE) {
    contentSize.height = aReflowState.mComputedHeight;
  } else {
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMaxHeight) {
      contentSize.height = PR_MIN(aReflowState.mComputedMaxHeight, contentSize.height);
    }
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMinHeight) {
      contentSize.height = PR_MAX(aReflowState.mComputedMinHeight, contentSize.height);
    }
  }
  if (aReflowState.mComputedWidth != NS_INTRINSICSIZE) {
    contentSize.width = aReflowState.mComputedWidth;
  } else {
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMaxWidth) {
      contentSize.width = PR_MIN(aReflowState.mComputedMaxWidth, contentSize.width);
    }
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedMinWidth) {
      contentSize.width = PR_MAX(aReflowState.mComputedMinWidth, contentSize.width);
    }
  }
    
  aDesiredSize.height = borderPadding.top + contentSize.height +
    borderPadding.bottom;
  aDesiredSize.width = contentSize.width + borderPadding.left + borderPadding.right;
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;
  aDesiredSize.mMaximumWidth = aDesiredSize.width;
  if (aDesiredSize.mComputeMEW) {
    // add in padding.
    aDesiredSize.mMaxElementWidth += borderPadding.left + borderPadding.right;
  }
  overflowRect.UnionRect(overflowRect, nsRect(0, 0, aDesiredSize.width, aDesiredSize.height));
  aDesiredSize.mOverflowArea = overflowRect;
  
#ifdef DEBUG_roc
  printf("*** DONE PASS feasible=%d\n", allFit && NS_FRAME_IS_COMPLETE(aStatus)
         && !NS_FRAME_IS_TRUNCATED(aStatus));
#endif
  return allFit && NS_FRAME_IS_COMPLETE(aStatus)
    && !NS_FRAME_IS_TRUNCATED(aStatus);
}

static nscoord ComputeSumOfChildHeights(nsIFrame* aFrame) {
  nscoord totalHeight = 0;
  for (nsIFrame* f = aFrame->GetFirstChild(nsnull); f; f = f->GetNextSibling()) {
    // individual columns don't have borders or padding so this is a
    // reasonable way to get their content height
    totalHeight += f->GetSize().height;
  }
  return totalHeight;
}

void
nsColumnSetFrame::DrainOverflowColumns()
{
  // First grab the prev-in-flows overflows and reparent them to this
  // frame.
  nsColumnSetFrame* prev = NS_STATIC_CAST(nsColumnSetFrame*, mPrevInFlow);
  if (prev) {
    nsIFrame* overflows = prev->GetOverflowFrames(GetPresContext(), PR_TRUE);
    if (overflows) {
      // Make all the frames on the overflow list mine
      nsIFrame* lastFrame = nsnull;
      for (nsIFrame* f = overflows; f; f = f->GetNextSibling()) {
        f->SetParent(this);

        // When pushing and pulling frames we need to check for whether any
        // views need to be reparented
        nsHTMLContainerFrame::ReparentFrameView(GetPresContext(), f, prev, this);

        // Get the next frame
        lastFrame = f;
      }

      NS_ASSERTION(lastFrame, "overflow list was created with no frames");
      lastFrame->SetNextSibling(mFrames.FirstChild());
      
      mFrames.SetFrames(overflows);
    }
  }
  
  // Now pull back our own overflows and append them to our children.
  // We don't need to reparent them since we're already their parent.
  nsIFrame* overflows = GetOverflowFrames(GetPresContext(), PR_TRUE);
  if (overflows) {
    mFrames.AppendFrames(this, overflows);
  }
}


NS_IMETHODIMP 
nsColumnSetFrame::Reflow(nsPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aDesiredSize,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsColumnSetFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);

  // Initialize OUT parameter
  aStatus = NS_FRAME_COMPLETE;

  //------------ Handle Incremental Reflow -----------------
  nsReflowReason kidReason = aReflowState.reason;

  if ( aReflowState.reason == eReflowReason_Incremental ) {
    nsHTMLReflowCommand *command = aReflowState.path->mReflowCommand;
    
    // Dirty any frames on the incremental reflow path
    nsReflowPath *path = aReflowState.path;
    nsReflowPath::iterator iter = path->FirstChild();
    nsReflowPath::iterator end = path->EndChildren();
    for ( ; iter != end; ++iter) {
      (*iter)->AddStateBits(NS_FRAME_IS_DIRTY);
    }

    // See if it's targeted at us
    if (command) {
      nsReflowType reflowType;
      command->GetType(reflowType);
      
      switch (reflowType) {
        
      case eReflowType_StyleChanged:
        kidReason = eReflowReason_StyleChange;
        break;
        
        // if its a dirty type then reflow us with a dirty reflow
      case eReflowType_ReflowDirty:
        kidReason = eReflowReason_Dirty;
        break;

      default:
        NS_ERROR("Unexpected Reflow Type");
      }
    }
  }

  ReflowConfig config = ChooseColumnStrategy(aReflowState);
  PRBool isBalancing = config.mBalanceColCount < PR_INT32_MAX;
  
  // If balancing, then we allow the last column to grow to unbounded
  // height during the first reflow. This gives us a way to estimate
  // what the average column height should be, because we can measure
  // the heights of all the columns and sum them up. But don't do this
  // if we have a next in flow because we don't want to suck all its
  // content back here and then have to push it out again!
  nsIFrame* nextInFlow = GetNextInFlow();
  PRBool unboundedLastColumn = isBalancing && nextInFlow;
  PRBool feasible = ReflowChildren(aDesiredSize, aReflowState, kidReason,
    aStatus, config, unboundedLastColumn);

  if (isBalancing) {
    nscoord availableContentHeight = GetAvailableContentHeight(aReflowState);
  
    // Termination of the algorithm below is guaranteed because
    // knownFeasibleHeight - knownInfeasibleHeight decreases in every
    // iteration.
    nscoord knownFeasibleHeight = NS_INTRINSICSIZE;
    nscoord knownInfeasibleHeight = 0;

    while (1) {
      nscoord maxHeight = 0;
      for (nsIFrame* f = mFrames.FirstChild(); f; f = f->GetNextSibling()) {
        maxHeight = PR_MAX(maxHeight, f->GetSize().height);
      }

      // Record what we learned from the last reflow
      if (feasible) {
        // maxHeight is feasible (and always maxHeight <=
        // mLastBalanceHeight)
        knownFeasibleHeight = PR_MIN(knownFeasibleHeight, maxHeight);

        // Furthermore, no height less than the height of the last
        // column can ever be feasible.
        if (mFrames.GetLength() == config.mBalanceColCount) {
          knownInfeasibleHeight = PR_MAX(knownInfeasibleHeight,
                                         mFrames.LastChild()->GetSize().height - 1);
        }
      } else {
        knownInfeasibleHeight = PR_MAX(knownInfeasibleHeight, mLastBalanceHeight);

        if (unboundedLastColumn) {
          // The last column is unbounded, so all content got reflowed, so the
          // maxHeight is feasible.
          knownFeasibleHeight = PR_MIN(knownFeasibleHeight, maxHeight);
        }
      }

#ifdef DEBUG_roc
      printf("*** nsColumnSetFrame::Reflow balancing knownInfeasible=%d knownFeasible=%d\n",
             knownInfeasibleHeight, knownFeasibleHeight);
#endif

      if (knownInfeasibleHeight >= knownFeasibleHeight - 1) {
        // knownFeasibleHeight is where we want to be
        break;
      }

      if (knownInfeasibleHeight >= availableContentHeight) {
        break;
      }

      nscoord nextGuess = (knownFeasibleHeight + knownInfeasibleHeight)/2;
      // The constant of 600 twips is arbitrary. It's about two line-heights.
      if (knownFeasibleHeight - nextGuess < 600) {
        // We're close to our target, so just try shrinking just the
        // minimum amount that will cause one of our columns to break
        // differently.
        nextGuess = knownFeasibleHeight - 1;
      } else if (unboundedLastColumn) {
        // Make a guess by dividing that into N columns. Add some slop
        // to try to make it on the feasible side.  The constant of
        // 600 twips is arbitrary. It's about two line-heights.
        nextGuess = ComputeSumOfChildHeights(this)/config.mBalanceColCount + 600;
        // Sanitize it
        nextGuess = PR_MIN(PR_MAX(nextGuess, knownInfeasibleHeight + 1),
                           knownFeasibleHeight - 1);
      } else if (knownFeasibleHeight == NS_INTRINSICSIZE) {
        // This can happen when we had a next-in-flow so we didn't
        // want to do an unbounded height measuring step. Let's just increase
        // from the infeasible height by some reasonable amount.
        nextGuess = knownInfeasibleHeight*2 + 600;
      }
      // Don't bother guessing more than our height constraint.
      nextGuess = PR_MIN(availableContentHeight, nextGuess);

#ifdef DEBUG_roc
      printf("*** nsColumnSetFrame::Reflow balancing choosing next guess=%d\n", nextGuess);
#endif

      config.mColMaxHeight = nextGuess;
      
      unboundedLastColumn = PR_FALSE;
      feasible = ReflowChildren(aDesiredSize, aReflowState,
                                eReflowReason_Resize, aStatus, config, PR_FALSE);
    }

    if (!feasible) {
      // We may need to reflow one more time at the feasible height to
      // get a valid layout.
      PRBool skip = PR_FALSE;
      if (knownInfeasibleHeight >= availableContentHeight) {
        config.mColMaxHeight = availableContentHeight;
        if (mLastBalanceHeight == availableContentHeight) {
          skip = PR_TRUE;
        }
      } else {
        config.mColMaxHeight = knownFeasibleHeight;
      }
      if (!skip) {
        ReflowChildren(aDesiredSize, aReflowState,
                       eReflowReason_Resize, aStatus, config, PR_FALSE);
      }
    }
  }

  CheckInvalidateSizeChange(GetPresContext(), aDesiredSize, aReflowState);

  FinishAndStoreOverflow(&aDesiredSize);

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  return NS_OK;
}

PRIntn
nsColumnSetFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsColumnSetFrame::AppendFrames(nsPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aFrameList)
{
  NS_NOTREACHED("AppendFrames not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsColumnSetFrame::InsertFrames(nsPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aPrevFrame,
                      nsIFrame*       aFrameList)
{
  NS_NOTREACHED("InsertFrames not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsColumnSetFrame::RemoveFrame(nsPresContext* aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame)
{
  NS_NOTREACHED("RemoveFrame not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}
