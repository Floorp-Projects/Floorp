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
#include "nsTableRowFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsHTMLAtoms.h"
#include "nsIContent.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsIView.h"
#include "nsReflowPath.h"
#include "nsCSSRendering.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLParts.h"
#include "nsTableColGroupFrame.h"
#include "nsTableColFrame.h"
#include "nsCOMPtr.h"


struct nsTableCellReflowState : public nsHTMLReflowState
{
  nsTableCellReflowState(nsPresContext*          aPresContext,
                         const nsHTMLReflowState& aParentReflowState,
                         nsIFrame*                aFrame,
                         const nsSize&            aAvailableSpace,
                         nsReflowReason           aReason);

  void FixUp(const nsSize& aAvailSpace,
             PRBool        aResetComputedWidth);
};

nsTableCellReflowState::nsTableCellReflowState(nsPresContext*          aPresContext,
                                               const nsHTMLReflowState& aParentRS,
                                               nsIFrame*                aFrame,
                                               const nsSize&            aAvailSpace,
                                               nsReflowReason           aReason)
  :nsHTMLReflowState(aPresContext, aParentRS, aFrame, aAvailSpace, aReason)
{
}

void nsTableCellReflowState::FixUp(const nsSize& aAvailSpace,
                                   PRBool        aResetComputedWidth)
{
  // fix the mComputed values during a pass 2 reflow since the cell can be a percentage base
  if (NS_UNCONSTRAINEDSIZE != aAvailSpace.width) {
    if (aResetComputedWidth) {
      mComputedWidth = NS_UNCONSTRAINEDSIZE;
    }
    else if (NS_UNCONSTRAINEDSIZE != mComputedWidth) {
      mComputedWidth = aAvailSpace.width - mComputedBorderPadding.left - mComputedBorderPadding.right;
      mComputedWidth = PR_MAX(0, mComputedWidth);
    }
    if (NS_UNCONSTRAINEDSIZE != mComputedHeight) {
      if (NS_UNCONSTRAINEDSIZE != aAvailSpace.height) {
        mComputedHeight = aAvailSpace.height - mComputedBorderPadding.top - mComputedBorderPadding.bottom;
        mComputedHeight = PR_MAX(0, mComputedHeight);
      }
    }
  }
}

void
nsTableRowFrame::InitChildReflowState(nsPresContext&         aPresContext,
                                      const nsSize&           aAvailSize,
                                      PRBool                  aBorderCollapse,
                                      float                   aPixelsToTwips,
                                      nsTableCellReflowState& aReflowState,
                                      PRBool                  aResetComputedWidth)
{
  nsMargin collapseBorder;
  nsMargin* pCollapseBorder = nsnull;
  if (aBorderCollapse) {
    // we only reflow cells, so don't need to check frame type
    nsBCTableCellFrame* bcCellFrame = (nsBCTableCellFrame*)aReflowState.frame;
    if (bcCellFrame) {
      pCollapseBorder = bcCellFrame->GetBorderWidth(aPixelsToTwips, collapseBorder);
    }
  }
  aReflowState.Init(&aPresContext, -1, -1, pCollapseBorder);
  aReflowState.FixUp(aAvailSize, aResetComputedWidth);
}

void 
nsTableRowFrame::SetFixedHeight(nscoord aValue)
{
  nscoord height = PR_MAX(0, aValue);
  if (HasFixedHeight()) {
    if (height > mStyleFixedHeight) {
      mStyleFixedHeight = height;
    }
  }
  else {
    mStyleFixedHeight = height;
    if (height > 0) {
      SetHasFixedHeight(PR_TRUE);
    }
  }
}

void 
nsTableRowFrame::SetPctHeight(float  aPctValue,
                              PRBool aForce)
{
  nscoord height = PR_MAX(0, NSToCoordRound(aPctValue * 100.0f));
  if (HasPctHeight()) {
    if ((height > mStylePctHeight) || aForce) {
      mStylePctHeight = height;
    }
  }
  else {
    mStylePctHeight = height;
    if (height > 0.0f) {
      SetHasPctHeight(PR_TRUE);
    }
  }
}

// 'old' is old cached cell's desired size
// 'new' is new cell's size including style constraints
static PRBool
TallestCellGotShorter(nscoord aOld,
                      nscoord aNew,
                      nscoord aTallest)
{
  return ((aNew < aOld) && (aOld == aTallest));
}

/* ----------- nsTableRowFrame ---------- */

nsTableRowFrame::nsTableRowFrame()
  : nsHTMLContainerFrame()
{
  mBits.mRowIndex = mBits.mFirstInserted = 0;
  ResetHeight(0);
#ifdef DEBUG_TABLE_REFLOW_TIMING
  mTimer = new nsReflowTimer(this);
#endif
}

nsTableRowFrame::~nsTableRowFrame()
{
#ifdef DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflowDone(this);
#endif
}

NS_IMETHODIMP
nsTableRowFrame::Init(nsPresContext*  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsStyleContext*  aContext,
                      nsIFrame*        aPrevInFlow)
{
  nsresult  rv;

  // Let the the base class do its initialization
  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);

  // record that children that are ignorable whitespace should be excluded 
  mState |= NS_FRAME_EXCLUDE_IGNORABLE_WHITESPACE;

  if (aPrevInFlow) {
    // Set the row index
    nsTableRowFrame* rowFrame = (nsTableRowFrame*)aPrevInFlow;
    
    SetRowIndex(rowFrame->GetRowIndex());
  }

  return rv;
}


NS_IMETHODIMP
nsTableRowFrame::AppendFrames(nsPresContext* aPresContext,
                              nsIPresShell&   aPresShell,
                              nsIAtom*        aListName,
                              nsIFrame*       aFrameList)
{
  // Append the frames
  mFrames.AppendFrames(nsnull, aFrameList);

  // Add the new cell frames to the table
  nsTableFrame *tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  for (nsIFrame* childFrame = aFrameList; childFrame;
       childFrame = childFrame->GetNextSibling()) {
    if (IS_TABLE_CELL(childFrame->GetType())) {
      // Add the cell to the cell map
      tableFrame->AppendCell(*aPresContext, (nsTableCellFrame&)*childFrame, GetRowIndex());
      // XXX this could be optimized with some effort
      tableFrame->SetNeedStrategyInit(PR_TRUE);
    }
  }

  // Reflow the new frames. They're already marked dirty, so generate a reflow
  // command that tells us to reflow our dirty child frames
  tableFrame->AppendDirtyReflowCommand(&aPresShell, this);

  return NS_OK;
}


NS_IMETHODIMP
nsTableRowFrame::InsertFrames(nsPresContext* aPresContext,
                              nsIPresShell&   aPresShell,
                              nsIAtom*        aListName,
                              nsIFrame*       aPrevFrame,
                              nsIFrame*       aFrameList)
{
  // Get the table frame
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  
  // gather the new frames (only those which are cells) into an array
  nsIAtom* cellFrameType = (tableFrame->IsBorderCollapse()) ? nsLayoutAtoms::bcTableCellFrame : nsLayoutAtoms::tableCellFrame;
  nsTableCellFrame* prevCellFrame = (nsTableCellFrame *)nsTableFrame::GetFrameAtOrBefore(this, aPrevFrame, cellFrameType);
  nsVoidArray cellChildren;
  for (nsIFrame* childFrame = aFrameList; childFrame;
       childFrame = childFrame->GetNextSibling()) {
    if (IS_TABLE_CELL(childFrame->GetType())) {
      cellChildren.AppendElement(childFrame);
      // XXX this could be optimized with some effort
      tableFrame->SetNeedStrategyInit(PR_TRUE);
    }
  }
  // insert the cells into the cell map
  PRInt32 colIndex = -1;
  if (prevCellFrame) {
    prevCellFrame->GetColIndex(colIndex);
  }
  tableFrame->InsertCells(*aPresContext, cellChildren, GetRowIndex(), colIndex);

  // Insert the frames in the frame list
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  
  // Reflow the new frames. They're already marked dirty, so generate a reflow
  // command that tells us to reflow our dirty child frames
  tableFrame->AppendDirtyReflowCommand(&aPresShell, this);

  return NS_OK;
}

NS_IMETHODIMP
nsTableRowFrame::RemoveFrame(nsPresContext* aPresContext,
                             nsIPresShell&   aPresShell,
                             nsIAtom*        aListName,
                             nsIFrame*       aOldFrame)
{
  // Get the table frame
  nsTableFrame* tableFrame=nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (tableFrame) {
    if (IS_TABLE_CELL(aOldFrame->GetType())) {
      nsTableCellFrame* cellFrame = (nsTableCellFrame*)aOldFrame;
      PRInt32 colIndex;
      cellFrame->GetColIndex(colIndex);
      // remove the cell from the cell map
      tableFrame->RemoveCell(*aPresContext, cellFrame, GetRowIndex());
      // XXX this could be optimized with some effort
      tableFrame->SetNeedStrategyInit(PR_TRUE);

      // Remove the frame and destroy it
      mFrames.DestroyFrame(aPresContext, aOldFrame);

      // XXX This could probably be optimized with much effort
      tableFrame->SetNeedStrategyInit(PR_TRUE);
      // Generate a reflow command so we reflow the table itself.
      // Target the row so that it gets a dirty reflow before a resize reflow
      // in case another cell gets added to the row during a reflow coallesce.
      tableFrame->AppendDirtyReflowCommand(&aPresShell, this);
    }
  }

  return NS_OK;
}

nscoord 
GetHeightOfRowsSpannedBelowFirst(nsTableCellFrame& aTableCellFrame,
                                 nsTableFrame&     aTableFrame)
{
  nscoord height = 0;
  nscoord cellSpacingY = aTableFrame.GetCellSpacingY();
  PRInt32 rowSpan = aTableFrame.GetEffectiveRowSpan(aTableCellFrame);
  // add in height of rows spanned beyond the 1st one
  nsIFrame* nextRow = aTableCellFrame.GetParent()->GetNextSibling();
  for (PRInt32 rowX = 1; ((rowX < rowSpan) && nextRow);) {
    if (nsLayoutAtoms::tableRowFrame == nextRow->GetType()) {
      height += nextRow->GetSize().height;
      rowX++;
    }
    height += cellSpacingY;
    nextRow = nextRow->GetNextSibling();
  }
  return height;
}

nsTableCellFrame*  
nsTableRowFrame::GetFirstCell() 
{
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (IS_TABLE_CELL(childFrame->GetType())) {
      return (nsTableCellFrame*)childFrame;
    }
    childFrame = childFrame->GetNextSibling();
  }
  return nsnull;
}

/**
 * Post-reflow hook. This is where the table row does its post-processing
 */
void
nsTableRowFrame::DidResize(nsPresContext*          aPresContext,
                           const nsHTMLReflowState& aReflowState)
{
  // Resize and re-align the cell frames based on our row height
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (!tableFrame) return;
  
  nsTableIterator iter(*this, eTableDIR);
  nsIFrame* childFrame = iter.First();
  
  nsHTMLReflowMetrics desiredSize(PR_FALSE);
  desiredSize.width = mRect.width;
  desiredSize.height = mRect.height;
  desiredSize.mOverflowArea = nsRect(0, 0, desiredSize.width,
                                      desiredSize.height);

  while (childFrame) {
    if (IS_TABLE_CELL(childFrame->GetType())) {
      nsTableCellFrame* cellFrame = (nsTableCellFrame*)childFrame;
      nscoord cellHeight = mRect.height + GetHeightOfRowsSpannedBelowFirst(*cellFrame, *tableFrame);

      // resize the cell's height
      //if (cellFrameSize.height!=cellHeight)
      {
        // XXX If the cell frame has a view, then we need to resize
        // it as well. We would like to only do that if the cell's size
        // is changing. Why is the 'if' stmt above commented out?
        cellFrame->SetSize(nsSize(cellFrame->GetSize().width, cellHeight));
        // realign cell content based on the new height
        /*nsHTMLReflowMetrics desiredSize(nsnull);
        nsHTMLReflowState kidReflowState(aPresContext, aReflowState,
                                         cellFrame,
                                         nsSize(cellFrameSize.width, cellHeight),
                                         eReflowReason_Resize);*/
        //XXX: the following reflow is necessary for any content of the cell
        //     whose height is a percent of the cell's height (maybe indirectly.)
        //     But some content crashes when this reflow is issued, to be investigated
        //XXX nsReflowStatus status;
        //ReflowChild(cellFrame, aPresContext, desiredSize, kidReflowState, status);

        cellFrame->VerticallyAlignChild(aPresContext, aReflowState, mMaxCellAscent);
        ConsiderChildOverflow(aPresContext, desiredSize.mOverflowArea, cellFrame);
      }
    }
    // Get the next child
    childFrame = iter.Next();
  }
  FinishAndStoreOverflow(&desiredSize);
  if (HasView()) {
    nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, this, GetView(), &desiredSize.mOverflowArea, 0);
  }
  // Let our base class do the usual work
}

// returns max-ascent amongst all cells that have 'vertical-align: baseline'
// *including* cells with rowspans
nscoord nsTableRowFrame::GetMaxCellAscent() const
{
  return mMaxCellAscent;
}

nscoord
nsTableRowFrame::GetHeight(nscoord aPctBasis) const
{
  nscoord height = 0;
  if ((aPctBasis > 0) && HasPctHeight()) {
    height = NSToCoordRound(GetPctHeight() * (float)aPctBasis);
  }
  if (HasFixedHeight()) {
    height = PR_MAX(height, GetFixedHeight());
  }
  return PR_MAX(height, GetContentHeight());
}

void 
nsTableRowFrame::ResetHeight(nscoord aFixedHeight)
{
  SetHasFixedHeight(PR_FALSE);
  SetHasPctHeight(PR_FALSE);
  SetFixedHeight(0);
  SetPctHeight(0);
  SetContentHeight(0);

  if (aFixedHeight > 0) {
    SetFixedHeight(aFixedHeight);
  }

  mMaxCellAscent = 0;
  mMaxCellDescent = 0;
}

void
nsTableRowFrame::UpdateHeight(nscoord           aHeight,
                              nscoord           aAscent,
                              nscoord           aDescent,
                              nsTableFrame*     aTableFrame,
                              nsTableCellFrame* aCellFrame)
{
  if (!aTableFrame || !aCellFrame) {
    NS_ASSERTION(PR_FALSE , "invalid call");
    return;
  }

  if (aHeight != NS_UNCONSTRAINEDSIZE) {
    if (!(aCellFrame->HasVerticalAlignBaseline())) { // only the cell's height matters
      if (GetHeight() < aHeight) {
        PRInt32 rowSpan = aTableFrame->GetEffectiveRowSpan(*aCellFrame);
        if (rowSpan == 1) {
          SetContentHeight(aHeight);
        }
      }
    }
    else { // the alignment on the baseline can change the height
      NS_ASSERTION((aAscent != NS_UNCONSTRAINEDSIZE) && (aDescent != NS_UNCONSTRAINEDSIZE), "invalid call");
      // see if this is a long ascender
      if (mMaxCellAscent < aAscent) {
        mMaxCellAscent = aAscent;
      }
      // see if this is a long descender and without rowspan
      if (mMaxCellDescent < aDescent) {
        PRInt32 rowSpan = aTableFrame->GetEffectiveRowSpan(*aCellFrame);
        if (rowSpan == 1) {
          mMaxCellDescent = aDescent;
        }
      }
      // keep the tallest height in sync
      if (GetHeight() < mMaxCellAscent + mMaxCellDescent) {
        SetContentHeight(mMaxCellAscent + mMaxCellDescent);
      }
    }
  }
}

nscoord
nsTableRowFrame::CalcHeight(const nsHTMLReflowState& aReflowState)
{
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (!tableFrame) return 0;

  nscoord computedHeight = (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedHeight)
                            ? 0 : aReflowState.mComputedHeight;
  ResetHeight(computedHeight);

  const nsStylePosition* position = GetStylePosition();
  if (eStyleUnit_Coord == position->mHeight.GetUnit()) {
    SetFixedHeight(position->mHeight.GetCoordValue());
  }
  else if (eStyleUnit_Percent == position->mHeight.GetUnit()) {
    SetPctHeight(position->mHeight.GetPercentValue());
  }

  for (nsIFrame* kidFrame = mFrames.FirstChild(); kidFrame;
       kidFrame = kidFrame->GetNextSibling()) {
    if (IS_TABLE_CELL(kidFrame->GetType())) {
      nscoord availWidth = ((nsTableCellFrame *)kidFrame)->GetPriorAvailWidth();
      nsSize desSize = ((nsTableCellFrame *)kidFrame)->GetDesiredSize();
      if ((NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight) && !mPrevInFlow) {
        CalculateCellActualSize(kidFrame, desSize.width, desSize.height, availWidth);
      }
      // height may have changed, adjust descent to absorb any excess difference
      nscoord ascent = ((nsTableCellFrame *)kidFrame)->GetDesiredAscent();
      nscoord descent = desSize.height - ascent;
      UpdateHeight(desSize.height, ascent, descent, tableFrame, (nsTableCellFrame*)kidFrame);
    }
  }
  return GetHeight();
}

NS_METHOD nsTableRowFrame::Paint(nsPresContext*      aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect&        aDirtyRect,
                                 nsFramePaintLayer    aWhichLayer,
                                 PRUint32             aFlags)
{
  PRBool isVisible;
  if (NS_SUCCEEDED(IsVisibleForPainting(aPresContext, aRenderingContext, PR_FALSE, &isVisible)) && !isVisible) {
    return NS_OK;
  }

#ifdef DEBUG
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,255,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
#endif

  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer &&
      //direct (not table-called) background paint
      !(aFlags & (NS_PAINT_FLAG_TABLE_BG_PAINT | NS_PAINT_FLAG_TABLE_CELL_BG_PASS))) {
    nsTableFrame* tableFrame;
    nsTableFrame::GetTableFrame(this, tableFrame);
    NS_ASSERTION(tableFrame, "null table frame");

    TableBackgroundPainter painter(tableFrame,
                                   TableBackgroundPainter::eOrigin_TableRow,
                                   aPresContext, aRenderingContext,
                                   aDirtyRect);
    nsresult rv = painter.PaintRow(this);
    if (NS_FAILED(rv)) return rv;
    aFlags |= NS_PAINT_FLAG_TABLE_BG_PAINT;
  }

  PRUint8 overflow = GetStyleDisplay()->mOverflow;
  PRBool clip = overflow == NS_STYLE_OVERFLOW_CLIP ||
                overflow == NS_STYLE_OVERFLOW_HIDDEN;
  if (clip) {
    aRenderingContext.PushState();
    SetOverflowClipRect(aRenderingContext);
  }
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect,
                aWhichLayer, aFlags);
  if (clip)
    aRenderingContext.PopState();
  return NS_OK;

}

PRIntn
nsTableRowFrame::GetSkipSides() const
{
  PRIntn skip = 0;
  if (nsnull != mPrevInFlow) {
    skip |= 1 << NS_SIDE_TOP;
  }
  if (nsnull != mNextInFlow) {
    skip |= 1 << NS_SIDE_BOTTOM;
  }
  return skip;
}


/* we overload this here because rows have children that can span outside of themselves.
 * so the default "get the child rect, see if it contains the event point" action isn't
 * sufficient.  We have to ask the row if it has a child that contains the point.
 */
NS_IMETHODIMP
nsTableRowFrame::GetFrameForPoint(nsPresContext*        aPresContext,
                                       const nsPoint&    aPoint,
                                       nsFramePaintLayer aWhichLayer,
                                       nsIFrame**        aFrame)
{
  // XXX This would not need to exist (except as a one-liner, to make this
  // frame work like a block frame) if rows with rowspan cells made the
  // the NS_FRAME_OUTSIDE_CHILDREN bit of mState set correctly (see
  // nsIFrame.h).

  // XXXldb Do we need this anymore?

  // I imagine fixing this would help performance of GetFrameForPoint in
  // tables.  It may also fix problems with relative positioning.

  // This is basically copied from nsContainerFrame::GetFrameForPointUsing,
  // except for one bit removed

  nsIFrame *hit;
  nsPoint tmp;

  nsIFrame* kid = GetFirstChild(nsnull);
  *aFrame = nsnull;
  tmp.MoveTo(aPoint.x - mRect.x, aPoint.y - mRect.y);
  while (nsnull != kid) {
    nsresult rv = kid->GetFrameForPoint(aPresContext, tmp, aWhichLayer, &hit);

    if (NS_SUCCEEDED(rv) && hit) {
      *aFrame = hit;
    }
    kid = kid->GetNextSibling();
  }

  if (*aFrame) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

// Calculate the cell's actual size given its pass2 desired width and height.
// Takes into account the specified height (in the style), and any special logic
// needed for backwards compatibility.
// Modifies the desired width and height that are passed in.
nsresult
nsTableRowFrame::CalculateCellActualSize(nsIFrame* aCellFrame,
                                         nscoord&  aDesiredWidth,
                                         nscoord&  aDesiredHeight,
                                         nscoord   aAvailWidth)
{
  nscoord specifiedHeight = 0;
  
  // Get the height specified in the style information
  const nsStylePosition* position = aCellFrame->GetStylePosition();

  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (!tableFrame) return NS_ERROR_NULL_POINTER;
  
  PRInt32 rowSpan = tableFrame->GetEffectiveRowSpan((nsTableCellFrame&)*aCellFrame);
  
  switch (position->mHeight.GetUnit()) {
    case eStyleUnit_Coord:
      specifiedHeight = position->mHeight.GetCoordValue();
      if (1 == rowSpan) 
        SetFixedHeight(specifiedHeight);
      break;
    case eStyleUnit_Percent: {
      if (1 == rowSpan) 
        SetPctHeight(position->mHeight.GetPercentValue());
      // pct heights are handled when all of the cells are finished, so don't set specifiedHeight 
      break;
    }
    case eStyleUnit_Auto:
    default:
      break;
  }

  // If the specified height is greater than the desired height, then use the specified height
  if (specifiedHeight > aDesiredHeight)
    aDesiredHeight = specifiedHeight;
 
  if ((0 == aDesiredWidth) && (NS_UNCONSTRAINEDSIZE != aAvailWidth)) { // special Nav4 compatibility code for the width
    aDesiredWidth = aAvailWidth;
  } 

  return NS_OK;
}

// Calculates the available width for the table cell based on the known
// column widths taking into account column spans and column spacing
static void 
CalcAvailWidth(nsTableFrame&     aTableFrame,
               nscoord           aTableComputedWidth,
               float             aPixelToTwips,
               nsTableCellFrame& aCellFrame,
               nscoord           aCellSpacingX,
               nscoord&          aColAvailWidth,
               nscoord&          aCellAvailWidth)
{
  aColAvailWidth = aCellAvailWidth = 0;
  PRInt32 colIndex;
  aCellFrame.GetColIndex(colIndex);
  PRInt32 colspan = aTableFrame.GetEffectiveColSpan(aCellFrame);
  nscoord cellSpacing = 0;

  for (PRInt32 spanX = 0; spanX < colspan; spanX++) {
    nscoord colWidth = aTableFrame.GetColumnWidth(colIndex + spanX);
    if (colWidth > 0) {
      aColAvailWidth += colWidth;
    }
    if ((spanX > 0) && (aTableFrame.GetNumCellsOriginatingInCol(colIndex + spanX) > 0)) {
      cellSpacing += aCellSpacingX;
    }
  }
  if (aColAvailWidth > 0) {
    aColAvailWidth += cellSpacing;
  } 
  aCellAvailWidth = aColAvailWidth;

  // for a cell with a colspan > 1, use its fix width (if set) as the avail width 
  // if this is its initial reflow
  if ((aCellFrame.GetStateBits() & NS_FRAME_FIRST_REFLOW)
      && (aTableFrame.GetEffectiveColSpan(aCellFrame) > 1)) {
    // see if the cell has a style width specified
    const nsStylePosition* cellPosition = aCellFrame.GetStylePosition();
    if (eStyleUnit_Coord == cellPosition->mWidth.GetUnit()) {
      // need to add padding into fixed width
      nsMargin borderPadding(0,0,0,0);
      if (NS_UNCONSTRAINEDSIZE != aTableComputedWidth) {
        borderPadding = nsTableFrame::GetBorderPadding(nsSize(aTableComputedWidth, 0), 
                                                       aPixelToTwips,  &aCellFrame);
      }
      nscoord fixWidth = cellPosition->mWidth.GetCoordValue() + borderPadding.left + borderPadding.right;
      aCellAvailWidth = PR_MIN(aColAvailWidth, fixWidth);
    }
  }
}

nscoord
GetSpaceBetween(PRInt32       aPrevColIndex,
                PRInt32       aColIndex,
                PRInt32       aColSpan,
                nsTableFrame& aTableFrame,
                nscoord       aCellSpacingX,
                PRBool        aIsLeftToRight)
{
  nscoord space = 0;
  PRInt32 colX;
  if (aIsLeftToRight) {
    for (colX = aPrevColIndex + 1; aColIndex > colX; colX++) {
      space += aTableFrame.GetColumnWidth(colX);
      if (aTableFrame.GetNumCellsOriginatingInCol(colX) > 0) {
        space += aCellSpacingX;
      }
    }
  } 
  else {
    PRInt32 lastCol = aColIndex + aColSpan - 1;
    for (colX = aPrevColIndex - 1; colX > lastCol; colX--) {
      space += aTableFrame.GetColumnWidth(colX);
      if (aTableFrame.GetNumCellsOriginatingInCol(colX) > 0) {
        space += aCellSpacingX;
      }
    }
  }
  return space;
}

static nscoord
GetComputedWidth(const nsHTMLReflowState& aReflowState,
                 nsTableFrame&            aTableFrame)
{
  const nsHTMLReflowState* parentReflow = aReflowState.parentReflowState;
  nscoord computedWidth = 0;
  while (parentReflow) {
    if (parentReflow->frame == &aTableFrame) {
      computedWidth = parentReflow->mComputedWidth;
      break;
    }
    parentReflow = parentReflow->parentReflowState;
  }
  return computedWidth;
}

// subtract the heights of aRow's prev in flows from the unpaginated height
static
nscoord CalcHeightFromUnpaginatedHeight(nsPresContext*  aPresContext,
                                        nsTableRowFrame& aRow)
{
  nscoord height = 0;
  nsTableRowFrame* firstInFlow = (nsTableRowFrame*)aRow.GetFirstInFlow();
  if (!firstInFlow) ABORT1(0);
  if (firstInFlow->HasUnpaginatedHeight()) {
    height = firstInFlow->GetUnpaginatedHeight(aPresContext);
    for (nsIFrame* prevInFlow = aRow.GetPrevInFlow(); prevInFlow;
         prevInFlow->GetPrevInFlow(&prevInFlow)) {
      height -= prevInFlow->GetSize().height;
    }
  }
  return PR_MAX(height, 0);
}

// Called for a dirty or resize reflow. Reflows all the existing table cell 
// frames unless aDirtyOnly is PR_TRUE in which case only reflow the dirty frames

NS_METHOD 
nsTableRowFrame::ReflowChildren(nsPresContext*          aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsTableFrame&            aTableFrame,
                                nsReflowStatus&          aStatus,
                                PRBool                   aDirtyOnly)
{
  aStatus = NS_FRAME_COMPLETE;

  GET_PIXELS_TO_TWIPS(aPresContext, p2t);
  PRBool borderCollapse = (((nsTableFrame*)aTableFrame.GetFirstInFlow())->IsBorderCollapse());

  nsIFrame* tablePrevInFlow;
  aTableFrame.GetPrevInFlow(&tablePrevInFlow);
  PRBool isPaginated = aPresContext->IsPaginated();

  nsresult rv = NS_OK;

  nscoord cellSpacingX = aTableFrame.GetCellSpacingX();
  PRInt32 cellColSpan = 1;  // must be defined here so it's set properly for non-cell kids
  
  nsTableIteration dir = (aReflowState.availableWidth == NS_UNCONSTRAINEDSIZE)
                         ? eTableLTR : eTableDIR;
  nsTableIterator iter(*this, dir);
  // remember the col index of the previous cell to handle rowspans into this row
  PRInt32 firstPrevColIndex = (iter.IsLeftToRight()) ? -1 : aTableFrame.GetColCount();
  PRInt32 prevColIndex  = firstPrevColIndex;
  nscoord x = 0; // running total of children x offset

  PRBool isAutoLayout = aTableFrame.IsAutoLayout();
  PRBool needToNotifyTable = PR_TRUE;
  nscoord paginatedHeight = 0;
  // If the incremental reflow command is a StyleChanged reflow and
  // it's target is the current frame, then make sure we send
  // StyleChange reflow reasons down to the children so that they
  // don't over-optimize their reflow.

  PRBool notifyStyleChange = PR_FALSE;
  if (eReflowReason_Incremental == aReflowState.reason) {
    nsHTMLReflowCommand* command = aReflowState.path->mReflowCommand;
    if (command) {
      nsReflowType type;
      command->GetType(type);
      if (eReflowType_StyleChanged == type) {
        notifyStyleChange = PR_TRUE;
      }
    }
  }
  // Reflow each of our existing cell frames
  nsIFrame* kidFrame = iter.First();
  while (kidFrame) {
    nsIAtom* frameType = kidFrame->GetType();

    // See if we should only reflow the dirty child frames
    PRBool doReflowChild = PR_TRUE;
    if (aDirtyOnly && ((kidFrame->GetStateBits() & NS_FRAME_IS_DIRTY) == 0)) {
      doReflowChild = PR_FALSE;
    }
    else if ((NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight) && IS_TABLE_CELL(frameType)) {
      // We don't reflow a rowspan >1 cell here with a constrained height. 
      // That happens in nsTableRowGroupFrame::SplitSpanningCells.
      if (aTableFrame.GetEffectiveRowSpan((nsTableCellFrame&)*kidFrame) > 1) {
        doReflowChild = PR_FALSE;
      }
    }
    if (aReflowState.mFlags.mSpecialHeightReflow) {
      if (!isPaginated && (IS_TABLE_CELL(frameType) &&
                           !((nsTableCellFrame*)kidFrame)->NeedSpecialReflow())) {
        kidFrame = iter.Next(); 
        continue;
      }
    }

    // Reflow the child frame
    if (doReflowChild) {
      if (IS_TABLE_CELL(frameType)) {
        nsTableCellFrame* cellFrame = (nsTableCellFrame*)kidFrame;
        PRInt32 cellColIndex;
        cellFrame->GetColIndex(cellColIndex);
        cellColSpan = aTableFrame.GetEffectiveColSpan(*cellFrame);

        // If the adjacent cell is in a prior row (because of a rowspan) add in the space
        if ((iter.IsLeftToRight() && (prevColIndex != (cellColIndex - 1))) ||
            (!iter.IsLeftToRight() && (prevColIndex != cellColIndex + cellColSpan))) {
          x += GetSpaceBetween(prevColIndex, cellColIndex, cellColSpan, aTableFrame, 
                               cellSpacingX, iter.IsLeftToRight());
        }
        // Calculate the available width for the table cell using the known column widths
        nscoord availColWidth, availCellWidth;
        CalcAvailWidth(aTableFrame, GetComputedWidth(aReflowState, aTableFrame), p2t,
                       *cellFrame, cellSpacingX, availColWidth, availCellWidth);
        if (0 == availColWidth)  availColWidth  = NS_UNCONSTRAINEDSIZE;
        if (0 == availCellWidth) availCellWidth = NS_UNCONSTRAINEDSIZE;

        // remember the rightmost (ltr) or leftmost (rtl) column this cell spans into
        prevColIndex = (iter.IsLeftToRight()) ? cellColIndex + (cellColSpan - 1) : cellColIndex;
        // always request MEW. Since we may turn MEW on for selected cells during incremental
        // reflow, we need to request MEW *now* so that those incremental reflows will be
        // able to build on existing MEW data in the children.
        nsHTMLReflowMetrics desiredSize(PR_TRUE);
  
        // If the avail width is not the same as last time we reflowed the cell or
        // the cell wants to be bigger than what was available last time or
        // it is a style change reflow or we are printing, then we must reflow the
        // cell. Otherwise we can skip the reflow.
        nsIFrame* kidNextInFlow;
        kidFrame->GetNextInFlow(&kidNextInFlow);
        nsSize cellDesiredSize = cellFrame->GetDesiredSize();
        if ((availCellWidth != cellFrame->GetPriorAvailWidth())       ||
            (cellDesiredSize.width > cellFrame->GetPriorAvailWidth()) ||
            (eReflowReason_StyleChange == aReflowState.reason)        ||
            isPaginated                                               ||
            (cellFrame->NeedPass2Reflow() &&
             NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth)     ||
            (aReflowState.mFlags.mSpecialHeightReflow && cellFrame->NeedSpecialReflow()) ||
            (!aReflowState.mFlags.mSpecialHeightReflow && cellFrame->HadSpecialReflow()) ||
            HasPctHeight()                                            ||
            notifyStyleChange) {
          // Reflow the cell to fit the available width, height
          nsSize  kidAvailSize(availColWidth, aReflowState.availableHeight);
          nsReflowReason reason = eReflowReason_Resize;
          PRBool cellToWatch = PR_FALSE;
          // If it's a dirty frame, then check whether it's the initial reflow
          if (kidFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW) {
            reason = eReflowReason_Initial;
            cellToWatch = PR_TRUE;
          }
          else if (eReflowReason_StyleChange == aReflowState.reason) {
            reason = eReflowReason_StyleChange;
            cellToWatch = PR_TRUE;
          }
          else if (notifyStyleChange) {
            reason = eReflowReason_StyleChange;
            cellToWatch = PR_TRUE;
          }
          if (cellToWatch) {
            cellFrame->DidSetStyleContext(aPresContext); // XXX check this
            if (!tablePrevInFlow && isAutoLayout) {
              // request the maximum width if availWidth is constrained
              // XXX we could just do this always, but blocks have some problems
              if (NS_UNCONSTRAINEDSIZE != availCellWidth) {
                desiredSize.mFlags |= NS_REFLOW_CALC_MAX_WIDTH; 
              }
            }
            else {
              cellToWatch = PR_FALSE;
            }
          }
  
          nscoord oldMaxWidth     = cellFrame->GetMaximumWidth();
          nscoord oldMaxElemWidth = cellFrame->GetPass1MaxElementWidth();

          // Reflow the child
          nsTableCellReflowState kidReflowState(aPresContext, aReflowState, 
                                                kidFrame, kidAvailSize, reason);
          InitChildReflowState(*aPresContext, kidAvailSize, borderCollapse, p2t, kidReflowState);

          nsReflowStatus status;
          rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState,
                           x, 0, 0, status);

          if (cellToWatch) { 
            nscoord maxWidth = (NS_UNCONSTRAINEDSIZE == availCellWidth) 
                                ? desiredSize.width : desiredSize.mMaximumWidth;
            // save the max element width and max width
            if (desiredSize.mComputeMEW) {
              cellFrame->SetPass1MaxElementWidth(desiredSize.width, desiredSize.mMaxElementWidth);
              if (desiredSize.mMaxElementWidth > desiredSize.width) {
                NS_ASSERTION(PR_FALSE, "max element width exceeded desired width");
                desiredSize.width = desiredSize.mMaxElementWidth;
              }
            }
            cellFrame->SetMaximumWidth(maxWidth);
          }

          // allow the table to determine if/how the table needs to be rebalanced
          if (cellToWatch && needToNotifyTable) {
            needToNotifyTable = !aTableFrame.CellChangedWidth(*cellFrame, oldMaxWidth, oldMaxElemWidth);
          }

          // If any of the cells are not complete, then we're not complete
          if (NS_FRAME_IS_NOT_COMPLETE(status)) {
            aStatus = NS_FRAME_NOT_COMPLETE;
          }
        }
        else { 
          desiredSize.width = cellDesiredSize.width;
          desiredSize.height = cellDesiredSize.height;
          nsRect *overflowArea =
            cellFrame->GetOverflowAreaProperty();
          if (overflowArea)
            desiredSize.mOverflowArea = *overflowArea;
          else
            desiredSize.mOverflowArea.SetRect(0, 0, cellDesiredSize.width,
                                              cellDesiredSize.height);
          
          // if we are in a floated table, our position is not yet established, so we cannot reposition our views
          // the containing glock will do this for us after positioning the table
          if (!aTableFrame.GetStyleDisplay()->IsFloating()) {
            // Because we may have moved the frame we need to make sure any views are
            // positioned properly. We have to do this, because any one of our parent
            // frames could have moved and we have no way of knowing...
            nsTableFrame::RePositionViews(aPresContext, kidFrame);
          }
        }
        
        if (NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight) {
          if (!mPrevInFlow) {
            // Calculate the cell's actual size given its pass2 size. This function
            // takes into account the specified height (in the style), and any special
            // logic needed for backwards compatibility
            CalculateCellActualSize(kidFrame, desiredSize.width, 
                                    desiredSize.height, availCellWidth);
          }
          // height may have changed, adjust descent to absorb any excess difference
          nscoord ascent = cellFrame->GetDesiredAscent();
          nscoord descent = desiredSize.height - ascent;
          UpdateHeight(desiredSize.height, ascent, descent, &aTableFrame, cellFrame);
        }
        else {
          paginatedHeight = PR_MAX(paginatedHeight, desiredSize.height);
          PRInt32 rowSpan = aTableFrame.GetEffectiveRowSpan((nsTableCellFrame&)*kidFrame);
          if (1 == rowSpan) {
            SetContentHeight(paginatedHeight);
          }
        }

        // Place the child
        if (NS_UNCONSTRAINEDSIZE != availColWidth) {
          desiredSize.width = PR_MAX(availCellWidth, availColWidth);
        }

        FinishReflowChild(kidFrame, aPresContext, nsnull, desiredSize, x, 0, 0);
        
        x += desiredSize.width;  
      }
      else {// it's an unknown frame type, give it a generic reflow and ignore the results
        nsTableCellReflowState kidReflowState(aPresContext, aReflowState,
                                              kidFrame, nsSize(0,0), eReflowReason_Resize);
        InitChildReflowState(*aPresContext, nsSize(0,0), PR_FALSE, p2t, kidReflowState);
        nsHTMLReflowMetrics desiredSize(PR_FALSE);
        nsReflowStatus  status;
        ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState, 0, 0, 0, status);
        kidFrame->DidReflow(aPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);
      }
    }
    else if (IS_TABLE_CELL(frameType)) {
      // we need to account for the cell's width even if it isn't reflowed
      x += kidFrame->GetSize().width;
    }
    ConsiderChildOverflow(aPresContext, aDesiredSize.mOverflowArea, kidFrame);
    kidFrame = iter.Next(); // Get the next child
    x += cellSpacingX;
  }

  // just set our width to what was available. The table will calculate the width and not use our value.
  aDesiredSize.width = aReflowState.availableWidth;

  if (aReflowState.mFlags.mSpecialHeightReflow) {
    aDesiredSize.height = mRect.height;
  }
  else if (NS_UNCONSTRAINEDSIZE == aReflowState.availableHeight) {
    aDesiredSize.height = CalcHeight(aReflowState);
    if (mPrevInFlow) {
      nscoord height = CalcHeightFromUnpaginatedHeight(aPresContext, *this);
      aDesiredSize.height = PR_MAX(aDesiredSize.height, height);
    }
    else {
      if (isPaginated && HasStyleHeight()) {
        // set the unpaginated height so next in flows can try to honor it
        SetHasUnpaginatedHeight(PR_TRUE);
        SetUnpaginatedHeight(aPresContext, aDesiredSize.height);
      }
      if (isPaginated && HasUnpaginatedHeight()) {
        aDesiredSize.height = PR_MAX(aDesiredSize.height, GetUnpaginatedHeight(aPresContext));
      }
    }
  }
  else { // constrained height, paginated
    aDesiredSize.height = paginatedHeight;
    if (aDesiredSize.height <= aReflowState.availableHeight) {
      nscoord height = CalcHeightFromUnpaginatedHeight(aPresContext, *this);
      aDesiredSize.height = PR_MAX(aDesiredSize.height, height);
      aDesiredSize.height = PR_MIN(aDesiredSize.height, aReflowState.availableHeight);
    }
  }
  nsRect rowRect(0, 0, aDesiredSize.width, aDesiredSize.height);
  aDesiredSize.mOverflowArea.UnionRect(aDesiredSize.mOverflowArea, rowRect);
  FinishAndStoreOverflow(&aDesiredSize);
  return rv;
}

NS_METHOD nsTableRowFrame::IncrementalReflow(nsPresContext*          aPresContext,
                                             nsHTMLReflowMetrics&     aDesiredSize,
                                             const nsHTMLReflowState& aReflowState,
                                             nsTableFrame&            aTableFrame,
                                             nsReflowStatus&          aStatus)
{
  CalcHeight(aReflowState); // need to recalculate it based on last reflow sizes
 
  // the row is a target if its path has a reflow command
  nsHTMLReflowCommand* command = aReflowState.path->mReflowCommand;
  if (command)
    IR_TargetIsMe(aPresContext, aDesiredSize, aReflowState, aTableFrame, aStatus);

  // see if the chidren are targets as well
  nsReflowPath::iterator iter = aReflowState.path->FirstChild();
  nsReflowPath::iterator end  = aReflowState.path->EndChildren();
  for (; iter != end; ++iter)
    IR_TargetIsChild(aPresContext, aDesiredSize, aReflowState, aTableFrame, aStatus, *iter);

  return NS_OK;
}

NS_METHOD 
nsTableRowFrame::IR_TargetIsMe(nsPresContext*          aPresContext,
                               nsHTMLReflowMetrics&     aDesiredSize,
                               const nsHTMLReflowState& aReflowState,
                               nsTableFrame&            aTableFrame,
                               nsReflowStatus&          aStatus)
{
  nsresult rv = NS_FRAME_COMPLETE;

  nsReflowType type;
  aReflowState.path->mReflowCommand->GetType(type);
  switch (type) {
    case eReflowType_ReflowDirty: 
      // Reflow the dirty child frames. Typically this is newly added frames.
      rv = ReflowChildren(aPresContext, aDesiredSize, aReflowState, aTableFrame, aStatus, PR_TRUE);
      break;
    case eReflowType_StyleChanged :
      rv = IR_StyleChanged(aPresContext, aDesiredSize, aReflowState, aTableFrame, aStatus);
      break;
    case eReflowType_ContentChanged :
      NS_ASSERTION(PR_FALSE, "illegal reflow type: ContentChanged");
      rv = NS_ERROR_ILLEGAL_VALUE;
      break;
    default:
      NS_NOTYETIMPLEMENTED("unexpected reflow command type");
      rv = NS_ERROR_NOT_IMPLEMENTED;
      break;
  }

  return rv;
}

NS_METHOD 
nsTableRowFrame::IR_StyleChanged(nsPresContext*          aPresContext,
                                 nsHTMLReflowMetrics&     aDesiredSize,
                                 const nsHTMLReflowState& aReflowState,
                                 nsTableFrame&            aTableFrame,
                                 nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  // we presume that all the easy optimizations were done in the nsHTMLStyleSheet before we were called here
  // XXX: we can optimize this when we know which style attribute changed
  aTableFrame.SetNeedStrategyInit(PR_TRUE);
  rv = ReflowChildren(aPresContext, aDesiredSize, aReflowState, aTableFrame, aStatus, PR_FALSE);
  return rv;
}

NS_METHOD 
nsTableRowFrame::IR_TargetIsChild(nsPresContext*          aPresContext,
                                  nsHTMLReflowMetrics&     aDesiredSize,
                                  const nsHTMLReflowState& aReflowState,
                                  nsTableFrame&            aTableFrame,
                                  nsReflowStatus&          aStatus,
                                  nsIFrame*                aNextFrame)

{
  if (!aNextFrame) return NS_ERROR_NULL_POINTER;
  nsresult rv = NS_OK;

  GET_PIXELS_TO_TWIPS(aPresContext, p2t);
  PRBool isAutoLayout = aTableFrame.IsAutoLayout();
  const nsStyleDisplay* childDisplay = aNextFrame->GetStyleDisplay();
  if (NS_STYLE_DISPLAY_TABLE_CELL == childDisplay->mDisplay) {
    nsTableCellFrame* cellFrame = (nsTableCellFrame*)aNextFrame;
    // Get the x coord of the cell
    nsPoint cellOrigin = cellFrame->GetPosition();

    // At this point, we know the column widths. Compute the cell available width
    PRInt32 cellColIndex;
    cellFrame->GetColIndex(cellColIndex);
    nscoord cellSpacingX = aTableFrame.GetCellSpacingX();

    nscoord colAvailWidth, cellAvailWidth;
    CalcAvailWidth(aTableFrame, GetComputedWidth(aReflowState, aTableFrame), p2t,
                   *cellFrame, cellSpacingX, colAvailWidth, cellAvailWidth);

    // Always let the cell be as high as it wants. We ignore the height that's
    // passed in and always place the entire row. Let the row group decide
    // whether we fit or wehther the entire row is pushed
    nsSize  cellAvailSize(cellAvailWidth, NS_UNCONSTRAINEDSIZE);

    // Pass along the reflow command
    // Unless this is a fixed-layout table, then have the cell incrementally
    // update its maximum width. 
    nsHTMLReflowMetrics cellMet(PR_TRUE,
                                isAutoLayout ? NS_REFLOW_CALC_MAX_WIDTH : 0);
    GET_PIXELS_TO_TWIPS(aPresContext, p2t);
    nsTableCellReflowState kidRS(aPresContext, aReflowState, aNextFrame, cellAvailSize, 
                                 aReflowState.reason);
    // If the table will intialize the strategy (and balance) or balance, make the computed 
    // width unconstrained. This avoids having the cell block compute a bogus max width 
    // which will bias the balancing. Leave the avail width alone, since it is a best guess.
    // After the table balances, the cell will get reflowed with the correct computed width.
    PRBool resetComputedWidth = aTableFrame.NeedStrategyInit() || aTableFrame.NeedStrategyBalance();
    if (resetComputedWidth)
      cellFrame->SetNeedPass2Reflow(PR_TRUE);
    InitChildReflowState(*aPresContext, cellAvailSize, aTableFrame.IsBorderCollapse(), 
                         p2t, kidRS, resetComputedWidth); 

    // Remember the current desired size, we'll need it later
    nscoord oldCellMinWidth     = cellFrame->GetPass1MaxElementWidth();
    nscoord oldCellMaximumWidth = cellFrame->GetMaximumWidth();
    nsSize  oldCellDesSize      = cellFrame->GetDesiredSize();
    nscoord oldCellDesAscent    = cellFrame->GetDesiredAscent();
    nscoord oldCellDesDescent   = oldCellDesSize.height - oldCellDesAscent;
    
    // Reflow the cell passing it the incremental reflow command. We can't pass
    // in a max width of NS_UNCONSTRAINEDSIZE, because the max width must match
    // the width of the previous reflow...
    rv = ReflowChild(aNextFrame, aPresContext, cellMet, kidRS,
                     cellOrigin.x, 0, 0, aStatus);
    nsSize initCellDesSize(cellMet.width, cellMet.height);
    nscoord initCellDesAscent = cellMet.ascent;
    nscoord initCellDesDescent = cellMet.descent;
    
    // cache the max-elem and maximum widths
    cellFrame->SetPass1MaxElementWidth(cellMet.width, cellMet.mMaxElementWidth);
    cellFrame->SetMaximumWidth(cellMet.mMaximumWidth);

    // Calculate the cell's actual size given its pass2 size. This function
    // takes into account the specified height (in the style), and any special
    // logic needed for backwards compatibility
    CalculateCellActualSize(aNextFrame, cellMet.width, cellMet.height, cellAvailWidth);

    // height may have changed, adjust descent to absorb any excess difference
    cellMet.descent = cellMet.height - cellMet.ascent;

    // if the cell got shorter and it may have been the tallest, recalc the tallest cell
    PRBool tallestCellGotShorter = PR_FALSE;
    PRBool hasVerticalAlignBaseline = cellFrame->HasVerticalAlignBaseline();
    if (!hasVerticalAlignBaseline) { 
      // only the height matters
      tallestCellGotShorter = 
        TallestCellGotShorter(oldCellDesSize.height, cellMet.height, GetHeight());
    }
    else {
      // the ascent matters
      tallestCellGotShorter = 
        TallestCellGotShorter(oldCellDesAscent, cellMet.ascent, mMaxCellAscent);
      // the descent of cells without rowspan also matters
      if (!tallestCellGotShorter) {
        PRInt32 rowSpan = aTableFrame.GetEffectiveRowSpan(*cellFrame);
        if (rowSpan == 1) {
         tallestCellGotShorter = 
           TallestCellGotShorter(oldCellDesDescent, cellMet.descent, mMaxCellDescent);
        }
      }
    }
    if (tallestCellGotShorter) {
      CalcHeight(aReflowState);
    }
    else {
      UpdateHeight(cellMet.height, cellMet.ascent, cellMet.descent, &aTableFrame, cellFrame);
    }

    // if the cell's desired size didn't changed, our height is unchanged
    aDesiredSize.mNothingChanged = PR_FALSE;
    PRInt32 rowSpan = aTableFrame.GetEffectiveRowSpan(*cellFrame);
    if ((initCellDesSize.width  == oldCellDesSize.width)  &&
        (initCellDesSize.height == oldCellDesSize.height) &&
        (oldCellMaximumWidth    == cellMet.mMaximumWidth)) {
      if (!hasVerticalAlignBaseline) { // only the cell's height matters
        aDesiredSize.mNothingChanged = PR_TRUE;
      }
      else { // cell's ascent and cell's descent matter
        if (initCellDesAscent == oldCellDesAscent) {
          if ((rowSpan == 1) && (initCellDesDescent == oldCellDesDescent)) {
            aDesiredSize.mNothingChanged = PR_TRUE;
          }
        }
      }
    }
    aDesiredSize.height = (aDesiredSize.mNothingChanged) ? mRect.height : GetHeight();
    if (1 == rowSpan) {
      cellMet.height = aDesiredSize.height;
    }
    else {
      nscoord heightOfRows = aDesiredSize.height + GetHeightOfRowsSpannedBelowFirst(*cellFrame, aTableFrame); 
      cellMet.height = PR_MAX(cellMet.height, heightOfRows); 
      // XXX need to check what happens when this height differs from height of the cell's previous mRect.height
    }

    // Now place the child
    cellMet.width = colAvailWidth;
    FinishReflowChild(aNextFrame, aPresContext, nsnull, cellMet, cellOrigin.x, 0, 0);

    // Notify the table if the cell width changed so it can decide whether to rebalance
    if (!aDesiredSize.mNothingChanged) {
      aTableFrame.CellChangedWidth(*cellFrame, oldCellMinWidth, oldCellMaximumWidth); 
    } 

    // Return our desired size. Note that our desired width is just whatever width
    // we were given by the row group frame
    aDesiredSize.width  = aReflowState.availableWidth;
    if (!aDesiredSize.mNothingChanged) {
      if (aDesiredSize.height == mRect.height) { // our height didn't change
        cellFrame->VerticallyAlignChild(aPresContext, aReflowState, mMaxCellAscent);
        nsRect dirtyRect = cellFrame->GetRect();
        dirtyRect.height = mRect.height;
        ConsiderChildOverflow(aPresContext, aDesiredSize.mOverflowArea, cellFrame);
        dirtyRect.UnionRect(dirtyRect, aDesiredSize.mOverflowArea);
        Invalidate(dirtyRect);
      }
    }
    else { // we dont realign vertical but we need to update the overflow area
      nsIFrame* cellKidFrame = cellFrame->GetFirstChild(nsnull);
      if (cellKidFrame) {
        cellFrame->ConsiderChildOverflow(aPresContext, cellMet.mOverflowArea, cellKidFrame);
        cellFrame->FinishAndStoreOverflow(&cellMet);
        if (cellFrame->HasView()) {
          nsContainerFrame::SyncFrameViewAfterReflow(aPresContext, cellFrame, cellFrame->GetView(), &cellMet.mOverflowArea, 0);
        }
      }
    }
  }
  else
  { // pass reflow to unknown frame child
    // aDesiredSize does not change
  }
  
  // recover the overflow area
  aDesiredSize.mOverflowArea = nsRect(0, 0, aDesiredSize.width, aDesiredSize.height);
  for (nsIFrame* cell = mFrames.FirstChild(); cell; cell = cell->GetNextSibling()) {
    ConsiderChildOverflow(aPresContext, aDesiredSize.mOverflowArea, cell);
  }
  FinishAndStoreOverflow(&aDesiredSize);
  // When returning whether we're complete we need to look at each of our cell
  // frames. If any of them has a continuing frame, then we're not complete. We
  // can't just return the status of the cell frame we just reflowed...
  aStatus = NS_FRAME_COMPLETE;
  if (mNextInFlow) {
    for (nsIFrame* cell = mFrames.FirstChild(); cell;
         cell = cell->GetNextSibling()) {
      nsIFrame* contFrame;
  
      cell->GetNextInFlow(&contFrame);
      if (contFrame) {
        aStatus =  NS_FRAME_NOT_COMPLETE;
        break;
      }
    }
  }
  return rv;
}


/** Layout the entire row.
  * This method stacks cells horizontally according to HTML 4.0 rules.
  */
NS_METHOD
nsTableRowFrame::Reflow(nsPresContext*          aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableRowFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aReflowState);
#endif
  nsresult rv = NS_OK;

  nsTableFrame* tableFrame = nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (!tableFrame) return NS_ERROR_NULL_POINTER;

  const nsStyleVisibility* rowVis = GetStyleVisibility();
  PRBool collapseRow = (NS_STYLE_VISIBILITY_COLLAPSE == rowVis->mVisible);
  if (collapseRow) {
    tableFrame->SetNeedToCollapseRows(PR_TRUE);
  }

  // see if a special height reflow needs to occur due to having a pct height
  if (!NeedSpecialReflow()) 
    nsTableFrame::CheckRequestSpecialHeightReflow(aReflowState);

  switch (aReflowState.reason) {
  case eReflowReason_Initial:
    rv = ReflowChildren(aPresContext, aDesiredSize, aReflowState, *tableFrame, aStatus, PR_FALSE);
    break;

  case eReflowReason_Resize:
  case eReflowReason_StyleChange: 
  case eReflowReason_Dirty:
    rv = ReflowChildren(aPresContext, aDesiredSize, aReflowState, *tableFrame, aStatus, PR_FALSE);
    break;

  case eReflowReason_Incremental:
    rv = IncrementalReflow(aPresContext, aDesiredSize, aReflowState, *tableFrame, aStatus);
    break;
  default:
    NS_ASSERTION(PR_FALSE , "we should handle this reflow reason");
    rv = NS_ERROR_NOT_IMPLEMENTED;
    break;
  }

  // just set our width to what was available. The table will calculate the width and not use our value.
  aDesiredSize.width = aReflowState.availableWidth;

  if (aReflowState.mFlags.mSpecialHeightReflow) {
    SetNeedSpecialReflow(PR_FALSE);
  }

#if defined DEBUG_TABLE_REFLOW_TIMING
  nsTableFrame::DebugReflow(this, (nsHTMLReflowState&)aReflowState, &aDesiredSize, aStatus);
#endif
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return rv;
}

/**
 * This function is called by the row group frame's SplitRowGroup() code when
 * pushing a row frame that has cell frames that span into it. The cell frame
 * should be reflowed with the specified height
 */
nscoord 
nsTableRowFrame::ReflowCellFrame(nsPresContext*          aPresContext,
                                 const nsHTMLReflowState& aReflowState,
                                 nsTableCellFrame*        aCellFrame,
                                 nscoord                  aAvailableHeight,
                                 nsReflowStatus&          aStatus)
{
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame); if (!tableFrame) ABORT1(0);

  // Reflow the cell frame with the specified height. Use the existing width
  nsSize cellSize = aCellFrame->GetSize();
  
  nsSize  availSize(cellSize.width, aAvailableHeight);
  PRBool borderCollapse = ((nsTableFrame*)tableFrame->GetFirstInFlow())->IsBorderCollapse();
  GET_PIXELS_TO_TWIPS(aPresContext, p2t);
  nsTableCellReflowState cellReflowState(aPresContext, aReflowState, aCellFrame, availSize,
                                         eReflowReason_Resize);
  InitChildReflowState(*aPresContext, availSize, borderCollapse, p2t, cellReflowState);

  nsHTMLReflowMetrics desiredSize(PR_FALSE);

  ReflowChild(aCellFrame, aPresContext, desiredSize, cellReflowState,
              0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
  PRBool fullyComplete = NS_FRAME_IS_COMPLETE(aStatus) && !NS_FRAME_IS_TRUNCATED(aStatus);

  aCellFrame->SetSize(
    nsSize(cellSize.width, fullyComplete ? aAvailableHeight : desiredSize.height));

  // XXX What happens if this cell has 'vertical-align: baseline' ?
  // XXX Why is it assumed that the cell's ascent hasn't changed ?
  if (fullyComplete) {
    aCellFrame->VerticallyAlignChild(aPresContext, aReflowState, mMaxCellAscent);
  }
  aCellFrame->DidReflow(aPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);

  return desiredSize.height;
}

/**
 * These 3 functions are called by the row group frame's SplitRowGroup() code when
 * it creates a continuing cell frame and wants to insert it into the row's child list
 */
void 
nsTableRowFrame::InsertCellFrame(nsTableCellFrame* aFrame,
                                 nsTableCellFrame* aPrevSibling)
{
  mFrames.InsertFrame(nsnull, aPrevSibling, aFrame);
  aFrame->SetParent(this);
}

void 
nsTableRowFrame::InsertCellFrame(nsTableCellFrame* aFrame,
                                 PRInt32           aColIndex)
{
  // Find the cell frame where col index < aColIndex
  nsTableCellFrame* priorCell = nsnull;
  for (nsIFrame* child = mFrames.FirstChild(); child;
       child = child->GetNextSibling()) {
    if (!IS_TABLE_CELL(child->GetType())) {
      nsTableCellFrame* cellFrame = (nsTableCellFrame*)child;
      PRInt32 colIndex;
      cellFrame->GetColIndex(colIndex);
      if (colIndex < aColIndex) {
        priorCell = cellFrame;
      }
      else break;
    }
  }
  InsertCellFrame(aFrame, priorCell);
}

void 
nsTableRowFrame::RemoveCellFrame(nsTableCellFrame* aFrame)
{
  if (!mFrames.RemoveFrame(aFrame))
    NS_ASSERTION(PR_FALSE, "frame not in list");
}

nsIAtom*
nsTableRowFrame::GetType() const
{
  return nsLayoutAtoms::tableRowFrame;
}

nsTableRowFrame*  
nsTableRowFrame::GetNextRow() const
{
  nsIFrame* childFrame = GetNextSibling();
  while (childFrame) {
    if (nsLayoutAtoms::tableRowFrame == childFrame->GetType()) {
      return (nsTableRowFrame*)childFrame;
    }
    childFrame = childFrame->GetNextSibling();
  }
  return nsnull;
}

void 
nsTableRowFrame::SetUnpaginatedHeight(nsPresContext* aPresContext,
                                      nscoord         aValue)
{
  NS_ASSERTION(!mPrevInFlow, "program error");
  // Get the property 
  nscoord* value = (nscoord*)nsTableFrame::GetProperty(aPresContext, this, nsLayoutAtoms::rowUnpaginatedHeightProperty, PR_TRUE);
  if (value) {
    *value = aValue;
  }
}

nscoord
nsTableRowFrame::GetUnpaginatedHeight(nsPresContext* aPresContext)
{
  // See if the property is set
  nscoord* value = (nscoord*)nsTableFrame::GetProperty(aPresContext, GetFirstInFlow(), nsLayoutAtoms::rowUnpaginatedHeightProperty);
  if (value) 
    return *value;
  else 
    return 0;
}

void nsTableRowFrame::SetContinuousBCBorderWidth(PRUint8     aForSide,
                                                 BCPixelSize aPixelValue)
{
  switch (aForSide) {
    case NS_SIDE_RIGHT:
      mRightContBorderWidth = aPixelValue;
      return;
    case NS_SIDE_TOP:
      mTopContBorderWidth = aPixelValue;
      return;
    case NS_SIDE_LEFT:
      mLeftContBorderWidth = aPixelValue;
      return;
    default:
      NS_ERROR("invalid NS_SIDE arg");
  }
}

/* ----- global methods ----- */

nsresult 
NS_NewTableRowFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTableRowFrame* it = new (aPresShell) nsTableRowFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsTableRowFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("TableRow"), aResult);
}
#endif
