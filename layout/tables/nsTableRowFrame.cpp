/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsTableRowFrame.h"
#include "nsIRenderingContext.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIHTMLAttributes.h"
#include "nsHTMLAtoms.h"
#include "nsIContent.h"
#include "nsTableFrame.h"
#include "nsTableCellFrame.h"
#include "nsIView.h"
#include "nsIReflowCommand.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLParts.h"
#include "nsTableColGroupFrame.h"
#include "nsTableColFrame.h"
#include "nsCOMPtr.h"
// the following header files are required for style optimizations that work only when the child content is really a cell
#include "nsIHTMLTableCellElement.h"
static NS_DEFINE_IID(kIHTMLTableCellElementIID, NS_IHTMLTABLECELLELEMENT_IID);
// end includes for style optimizations that require real content knowledge


struct nsTableCellReflowState : public nsHTMLReflowState
{
  nsTableCellReflowState(nsIPresContext*          aPresContext,
                         const nsHTMLReflowState& aParentReflowState,
                         nsIFrame*                aFrame,
                         const nsSize&            aAvailableSpace,
                         nsReflowReason           aReason);

  nsTableCellReflowState(nsIPresContext*          aPresContext,
                         const nsHTMLReflowState& aParentReflowState,
                         nsIFrame*                aFrame,
                         const nsSize&            aAvailableSpace);

  void FixUp(const nsSize& aAvailSpace);
};

nsTableCellReflowState::nsTableCellReflowState(nsIPresContext*          aPresContext,
                                               const nsHTMLReflowState& aParentRS,
                                               nsIFrame*                aFrame,
                                               const nsSize&            aAvailSpace,
                                               nsReflowReason           aReason)
  :nsHTMLReflowState(aPresContext, aParentRS, aFrame, aAvailSpace, aReason)
{
  FixUp(aAvailSpace);
}

nsTableCellReflowState::nsTableCellReflowState(nsIPresContext*          aPresContext,
                                               const nsHTMLReflowState& aParentRS,
                                               nsIFrame*                aFrame,
                                               const nsSize&            aAvailSpace)
  :nsHTMLReflowState(aPresContext, aParentRS, aFrame, aAvailSpace)
{
  FixUp(aAvailSpace);
}

void nsTableCellReflowState::FixUp(const nsSize& aAvailSpace)
{
  // fix the mComputed values during a pass 2 reflow since the cell can be a percentage base
  if (NS_UNCONSTRAINEDSIZE != aAvailSpace.width) {
    if (NS_UNCONSTRAINEDSIZE != mComputedWidth) {
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

// 'old' is old cached cell's desired size
// 'raw' is new cell's size without including style constraints from CalculateCellActualSize()
// 'new' is new cell's size including style constraints
static PRBool
TallestCellGotShorter(nscoord aOld,
                      nscoord aRaw,
                      nscoord aNew,
                      nscoord aTallest)
{
  // see if the cell got shorter and it may have been the tallest
  PRBool tallestCellGotShorter = PR_FALSE;
  if (aRaw < aOld) {
    if ((aRaw == aTallest) && (aNew < aTallest)) {
      tallestCellGotShorter = PR_TRUE;
    }
  }
  return tallestCellGotShorter;
}

/* ----------- nsTableRowpFrame ---------- */

nsTableRowFrame::nsTableRowFrame()
  : nsHTMLContainerFrame(),
    mAllBits(0)
{
  mBits.mMinRowSpan = 1;
  mBits.mRowIndex   = 0;
  ResetTallestCell(0);
}

NS_IMETHODIMP
nsTableRowFrame::Init(nsIPresContext*  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow)
{
  nsresult  rv;

  // Let the the base class do its initialization
  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);

  if (aPrevInFlow) {
    // Set the row index
    nsTableRowFrame* rowFrame = (nsTableRowFrame*)aPrevInFlow;
    
    SetRowIndex(rowFrame->GetRowIndex());
  }

  return rv;
}

// Helper function. It marks the table frame as dirty and generates
// a reflow command
nsresult
nsTableRowFrame::AddTableDirtyReflowCommand(nsIPresContext* aPresContext,
                                            nsIPresShell&   aPresShell,
                                            nsIFrame*       aTableFrame)
{
  nsFrameState      frameState;
  nsIFrame*         tableParentFrame;
  nsIReflowCommand* reflowCmd;
  nsresult          rv;

  // Mark the table frame as dirty
  aTableFrame->GetFrameState(&frameState);
  frameState |= NS_FRAME_IS_DIRTY;
  aTableFrame->SetFrameState(frameState);

  // Target the reflow comamnd at its parent frame
  aTableFrame->GetParent(&tableParentFrame);
  rv = NS_NewHTMLReflowCommand(&reflowCmd, tableParentFrame,
                               nsIReflowCommand::ReflowDirty);
  if (NS_SUCCEEDED(rv)) {
    // Add the reflow command
    rv = aPresShell.AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }

  return rv;
}

NS_IMETHODIMP
nsTableRowFrame::AppendFrames(nsIPresContext* aPresContext,
                              nsIPresShell&   aPresShell,
                              nsIAtom*        aListName,
                              nsIFrame*       aFrameList)
{
  // Append the frames
  mFrames.AppendFrames(nsnull, aFrameList);

  // Add the new cell frames to the table
  nsTableFrame *tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  for (nsIFrame* childFrame = aFrameList; childFrame; childFrame->GetNextSibling(&childFrame)) {
    const nsStyleDisplay *display;
    childFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)display));

    if (NS_STYLE_DISPLAY_TABLE_CELL == display->mDisplay) {
      // Add the cell to the cell map
      tableFrame->AppendCell(*aPresContext, (nsTableCellFrame&)*childFrame, GetRowIndex());
    }
  }

  tableFrame->InvalidateColumnWidths();

  // Reflow the new frames. They're already marked dirty, so generate a reflow
  // command that tells us to reflow our dirty child frames
  nsIReflowCommand* reflowCmd;

  if (NS_SUCCEEDED(NS_NewHTMLReflowCommand(&reflowCmd, this,
                                           nsIReflowCommand::ReflowDirty))) {
    aPresShell.AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsTableRowFrame::InsertFrames(nsIPresContext* aPresContext,
                              nsIPresShell&   aPresShell,
                              nsIAtom*        aListName,
                              nsIFrame*       aPrevFrame,
                              nsIFrame*       aFrameList)
{
  // Get the table frame
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  
  // gather the new frames (only those which are cells) into an array
  nsTableCellFrame* prevCellFrame = (nsTableCellFrame *)nsTableFrame::GetFrameAtOrBefore(aPresContext, this, aPrevFrame, nsLayoutAtoms::tableCellFrame);
  nsVoidArray cellChildren;
  for (nsIFrame* childFrame = aFrameList; childFrame; childFrame->GetNextSibling(&childFrame)) {
    nsIAtom* frameType;
    childFrame->GetFrameType(&frameType);
    if (nsLayoutAtoms::tableCellFrame == frameType) {
      cellChildren.AppendElement(childFrame);
    }
    NS_IF_RELEASE(frameType);
  }
  // insert the cells into the cell map
  PRInt32 colIndex = -1;
  if (prevCellFrame) {
    prevCellFrame->GetColIndex(colIndex);
  }
  tableFrame->InsertCells(*aPresContext, cellChildren, GetRowIndex(), colIndex);

  // Insert the frames in the frame list
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  
  // Because the number of columns may have changed invalidate the column widths
  tableFrame->InvalidateColumnWidths();

  // Reflow the new frames. They're already marked dirty, so generate a reflow
  // command that tells us to reflow our dirty child frames
  nsIReflowCommand* reflowCmd;

  if (NS_SUCCEEDED(NS_NewHTMLReflowCommand(&reflowCmd, this,
                                           nsIReflowCommand::ReflowDirty))) {
    aPresShell.AppendReflowCommand(reflowCmd);
    NS_RELEASE(reflowCmd);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTableRowFrame::RemoveFrame(nsIPresContext* aPresContext,
                             nsIPresShell&   aPresShell,
                             nsIAtom*        aListName,
                             nsIFrame*       aOldFrame)
{
  // Get the table frame
  nsTableFrame* tableFrame=nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (tableFrame) {
    nsIAtom* frameType;
    aOldFrame->GetFrameType(&frameType);
    if (nsLayoutAtoms::tableCellFrame == frameType) {
      nsTableCellFrame* cellFrame = (nsTableCellFrame*)aOldFrame;
      PRInt32 colIndex;
      cellFrame->GetColIndex(colIndex);
      tableFrame->RemoveCell(*aPresContext, cellFrame, GetRowIndex());

      // Remove the frame and destroy it
      mFrames.DestroyFrame(aPresContext, aOldFrame);

      // cells have possibly shifted into different columns. 
      tableFrame->InvalidateColumnWidths();

      // Because we haven't added any new frames we don't need to do a pass1
      // reflow. Just generate a reflow command so we reflow the table itself.
      // Target the row so that it gets a dirty reflow before a resize reflow
      // in case another cell gets added to the row during a reflow coallesce.
      nsIReflowCommand* reflowCmd;

      if (NS_SUCCEEDED(NS_NewHTMLReflowCommand(&reflowCmd, this,
                                               nsIReflowCommand::ReflowDirty))) {
        aPresShell.AppendReflowCommand(reflowCmd);
        NS_RELEASE(reflowCmd);
      }
    }
    NS_IF_RELEASE(frameType);
  }

  return NS_OK;
}

/**
 * Post-reflow hook. This is where the table row does its post-processing
 */
void
nsTableRowFrame::DidResize(nsIPresContext* aPresContext,
                           const nsHTMLReflowState& aReflowState)
{
  // Resize and re-align the cell frames based on our row height
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  if (!tableFrame) return;
  nscoord cellSpacingY = tableFrame->GetCellSpacingY();

  nsTableIterator iter(aPresContext, *this, eTableDIR);
  nsIFrame* cellFrame = iter.First();

  while (nsnull != cellFrame) {
    const nsStyleDisplay *kidDisplay;
    cellFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)kidDisplay));
    if (NS_STYLE_DISPLAY_TABLE_CELL == kidDisplay->mDisplay) {
      PRInt32 rowSpan = tableFrame->GetEffectiveRowSpan((nsTableCellFrame &)*cellFrame);
      nscoord cellHeight = mRect.height;
      // add in height of rows spanned beyond the 1st one
      nsIFrame* nextRow = nsnull;
      GetNextSibling(&nextRow);
      for (int i = 1; ((i < rowSpan) && nextRow);) {
        const nsStyleDisplay *nextDisplay;
        nextRow->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)nextDisplay));
        if (NS_STYLE_DISPLAY_TABLE_ROW == nextDisplay->mDisplay) { 
          nsRect rect;
          nextRow->GetRect(rect);
          cellHeight += rect.height;
          i++;
        }
        cellHeight += cellSpacingY;
        nextRow->GetNextSibling(&nextRow);
      }

      // resize the cell's height
      nsSize  cellFrameSize;
      cellFrame->GetSize(cellFrameSize);
      //if (cellFrameSize.height!=cellHeight)
      {
        // XXX If the cell frame has a view, then we need to resize
        // it as well. We would like to only do that if the cell's size
        // is changing. Why is the 'if' stmt above commented out?
        cellFrame->SizeTo(aPresContext, cellFrameSize.width, cellHeight);
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

        ((nsTableCellFrame *)cellFrame)->VerticallyAlignChild(aPresContext, aReflowState, mMaxCellAscent);

        /* if we're collapsing borders, notify the cell that the border edge length has changed */
        if (NS_STYLE_BORDER_COLLAPSE == tableFrame->GetBorderCollapseStyle()) {
          ((nsTableCellFrame *)(cellFrame))->SetBorderEdgeLength(NS_SIDE_LEFT,
                                                                 GetRowIndex(),
                                                                 cellHeight);
          ((nsTableCellFrame *)(cellFrame))->SetBorderEdgeLength(NS_SIDE_RIGHT,
                                                                 GetRowIndex(),
                                                                 cellHeight);
        }
      }
    }
      // Get the next cell
    cellFrame = iter.Next();
  }

  // Let our base class do the usual work
}

// returns max-ascent amongst all cells that have 'vertical-align: baseline'
// *including* cells with rowspans
nscoord nsTableRowFrame::GetMaxCellAscent() const
{
  return mMaxCellAscent;
}

#if 0 // nobody uses this
// returns max-descent amongst all cells that have 'vertical-align: baseline'
// does *not* include cells with rowspans
nscoord nsTableRowFrame::GetMaxCellDescent() const
{
  return mMaxCellDescent;
}
#endif

/** returns the height of the tallest child in this row (ignoring any cell with rowspans) */
nscoord nsTableRowFrame::GetTallestCell() const
{
  return mTallestCell;
}

void 
nsTableRowFrame::ResetTallestCell(nscoord aRowStyleHeight)
{
  mTallestCell = (NS_UNCONSTRAINEDSIZE == aRowStyleHeight) ? 0 : aRowStyleHeight;
  mMaxCellAscent = 0;
  mMaxCellDescent = 0;
}

void
nsTableRowFrame::SetTallestCell(nscoord           aHeight,
                                nscoord           aAscent,
                                nscoord           aDescent,
                                nsTableFrame*     aTableFrame,
                                nsTableCellFrame* aCellFrame)
{
  NS_ASSERTION((aTableFrame && aCellFrame) , "invalid call");
  if (aHeight != NS_UNCONSTRAINEDSIZE) {
    if (!(aCellFrame->HasVerticalAlignBaseline())) { // only the cell's height matters
      if (mTallestCell < aHeight) {
        PRInt32 rowSpan = aTableFrame->GetEffectiveRowSpan(*aCellFrame);
        if (rowSpan == 1) {
          mTallestCell = aHeight;
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
      if (mTallestCell < mMaxCellAscent + mMaxCellDescent) {
        mTallestCell = mMaxCellAscent + mMaxCellDescent;
      }
    }
  }
}

void 
nsTableRowFrame::CalcTallestCell()
{
  nsTableFrame* tableFrame = nsnull;
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (NS_FAILED(rv)) return;

  nscoord cellSpacingX = tableFrame->GetCellSpacingX();
  ResetTallestCell(0);

  for (nsIFrame* kidFrame = mFrames.FirstChild(); kidFrame; kidFrame->GetNextSibling(&kidFrame)) {
    nsCOMPtr<nsIAtom> frameType;
    kidFrame->GetFrameType(getter_AddRefs(frameType));
    if (nsLayoutAtoms::tableCellFrame == frameType.get()) {
      nscoord availWidth = ((nsTableCellFrame *)kidFrame)->GetPriorAvailWidth();
      nsSize desSize = ((nsTableCellFrame *)kidFrame)->GetDesiredSize();
      CalculateCellActualSize(kidFrame, desSize.width, desSize.height, availWidth);
      // height may have changed, adjust descent to absorb any excess difference
      nscoord ascent = ((nsTableCellFrame *)kidFrame)->GetDesiredAscent();
      nscoord descent = desSize.height - ascent;
      SetTallestCell(desSize.height, ascent, descent, tableFrame, (nsTableCellFrame*)kidFrame);
    }
  }
}

static
PRBool IsFirstRow(nsIPresContext*  aPresContext,
                  nsTableFrame&    aTable,
                  nsTableRowFrame& aRow)
{
  nsIFrame* firstRowGroup = nsnull;
  aTable.FirstChild(aPresContext, nsnull, &firstRowGroup);
  nsIFrame* rowGroupFrame = nsnull;
  nsresult rv = aRow.GetParent(&rowGroupFrame);
  if (NS_SUCCEEDED(rv) && (rowGroupFrame == firstRowGroup)) {
    nsIFrame* firstRow;
    rowGroupFrame->FirstChild(aPresContext, nsnull, &firstRow);
    return (&aRow == firstRow);
  }
  return PR_FALSE;
}


NS_METHOD nsTableRowFrame::Paint(nsIPresContext* aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect& aDirtyRect,
                                 nsFramePaintLayer aWhichLayer)
{
  nsresult rv;
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    nsCompatibility mode;
    aPresContext->GetCompatibilityMode(&mode);
    if (eCompatibility_Standard == mode) {
      const nsStyleDisplay* disp =
        (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
      if (disp->IsVisibleOrCollapsed()) {
        const nsStyleSpacing* spacing =
          (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
        const nsStyleColor* color =
          (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
        nsTableFrame* tableFrame = nsnull;
        rv = nsTableFrame::GetTableFrame(this, tableFrame);
        if (NS_FAILED(rv) || (nsnull == tableFrame)) {
          return rv;
        }
        nscoord cellSpacingX = tableFrame->GetCellSpacingX();
        // every row is short by the ending cell spacing X
        nsRect rect(0, 0, mRect.width + cellSpacingX, mRect.height);

        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *spacing, 0, 0);
      }
    }
  }

#ifdef DEBUG
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,255,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
#endif

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
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

/** overloaded method from nsContainerFrame.  The difference is that 
  * we don't want to clip our children, so a cell can do a rowspan
  */
void nsTableRowFrame::PaintChildren(nsIPresContext*      aPresContext,
                                    nsIRenderingContext& aRenderingContext,
                                    const nsRect&        aDirtyRect,
                                    nsFramePaintLayer aWhichLayer)
{
  nsIFrame* kid = mFrames.FirstChild();
  while (nsnull != kid) {
    nsIView *pView;
     
    kid->GetView(aPresContext, &pView);
    if (nsnull == pView) {
      nsRect kidRect;
      kid->GetRect(kidRect);
      nsRect damageArea;
      PRBool overlap = damageArea.IntersectRect(aDirtyRect, kidRect);
      if (overlap) {
        PRBool clipState;
        // Translate damage area into kid's coordinate system
        nsRect kidDamageArea(damageArea.x - kidRect.x,
                             damageArea.y - kidRect.y,
                             damageArea.width, damageArea.height);
        aRenderingContext.PushState();
        aRenderingContext.Translate(kidRect.x, kidRect.y);
        kid->Paint(aPresContext, aRenderingContext, kidDamageArea,
                   aWhichLayer);
#ifdef DEBUG
        if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) &&
            GetShowFrameBorders()) {
          aRenderingContext.SetColor(NS_RGB(255,0,0));
          aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
        }
#endif
        aRenderingContext.PopState(clipState);
      }
    }
    kid->GetNextSibling(&kid);
  }
}

/* we overload this here because rows have children that can span outside of themselves.
 * so the default "get the child rect, see if it contains the event point" action isn't
 * sufficient.  We have to ask the row if it has a child that contains the point.
 */
NS_IMETHODIMP
nsTableRowFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                       const nsPoint& aPoint,
                                       nsFramePaintLayer aWhichLayer,
                                       nsIFrame** aFrame)
{
  // XXX This would not need to exist (except as a one-liner, to make this
  // frame work like a block frame) if rows with rowspan cells made the
  // the NS_FRAME_OUTSIDE_CHILDREN bit of mState set correctly (see
  // nsIFrame.h).

  // I imagine fixing this would help performance of GetFrameForPoint in
  // tables.  It may also fix problems with relative positioning.

  // This is basically copied from nsContainerFrame::GetFrameForPointUsing,
  // except for one bit removed

  nsIFrame *kid, *hit;
  nsPoint tmp;

  PRBool inThisFrame = mRect.Contains(aPoint);

  FirstChild(aPresContext, nsnull, &kid);
  *aFrame = nsnull;
  tmp.MoveTo(aPoint.x - mRect.x, aPoint.y - mRect.y);
  while (nsnull != kid) {
    nsresult rv = kid->GetFrameForPoint(aPresContext, tmp, aWhichLayer, &hit);

    if (NS_SUCCEEDED(rv) && hit) {
      *aFrame = hit;
    }
    kid->GetNextSibling(&kid);
  }

  if (*aFrame) {
    return NS_OK;
  }

  if ( inThisFrame && (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND)) {
    const nsStyleDisplay* disp = (const nsStyleDisplay*)
      mStyleContext->GetStyleData(eStyleStruct_Display);
    if (disp->IsVisible()) {
      *aFrame = this;
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

/* GetMinRowSpan is needed for deviant cases where every cell in a row has a rowspan > 1.
 * It sets mMinRowSpan, which is used in FixMinCellHeight and PlaceChild
 */
void nsTableRowFrame::GetMinRowSpan(nsTableFrame *aTableFrame)
{
  PRInt32 minRowSpan=-1;
  nsIFrame *frame=mFrames.FirstChild();
  while (nsnull!=frame)
  {
    const nsStyleDisplay *kidDisplay;
    frame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)kidDisplay));
    if (NS_STYLE_DISPLAY_TABLE_CELL == kidDisplay->mDisplay)
    {
      PRInt32 rowSpan = aTableFrame->GetEffectiveRowSpan((nsTableCellFrame &)*frame);
      if (-1==minRowSpan)
        minRowSpan = rowSpan;
      else if (minRowSpan>rowSpan)
        minRowSpan = rowSpan;
    }
    frame->GetNextSibling(&frame);
  }
  mBits.mMinRowSpan = unsigned(minRowSpan);
}

void nsTableRowFrame::FixMinCellHeight(nsTableFrame *aTableFrame)
{
  nsIFrame *frame=mFrames.FirstChild();
  while (nsnull!=frame)
  {
    const nsStyleDisplay *kidDisplay;
    frame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)kidDisplay));
    if (NS_STYLE_DISPLAY_TABLE_CELL == kidDisplay->mDisplay)
    {
      PRInt32 rowSpan = aTableFrame->GetEffectiveRowSpan((nsTableCellFrame &)*frame);
      if (PRInt32(mBits.mMinRowSpan) ==rowSpan)
      {
        nsRect rect;
        frame->GetRect(rect);
        if (rect.height > mTallestCell)
          mTallestCell = rect.height;
      }
    }
    frame->GetNextSibling(&frame);
  }
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame, and includes
// any left/top margin.
void nsTableRowFrame::PlaceChild(nsIPresContext*      aPresContext,
																 RowReflowState&      aReflowState,
																 nsIFrame*            aKidFrame,
                                 nsHTMLReflowMetrics& aDesiredSize,
                                 nscoord              aX,
                                 nscoord              aY,
																 nsSize*              aMaxElementSize,
																 nsSize*              aKidMaxElementSize)
{
  // Complete the reflow
  FinishReflowChild(aKidFrame, aPresContext, aDesiredSize, aX, aY, 0);

  // update the running total for the row width
  aReflowState.x += aDesiredSize.width;

  // Update the maximum element size
  PRInt32 rowSpan = aReflowState.tableFrame->GetEffectiveRowSpan((nsTableCellFrame&)*aKidFrame);
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width += aKidMaxElementSize->width;
    if (1 == rowSpan) {
      aMaxElementSize->height = PR_MAX(aMaxElementSize->height, aKidMaxElementSize->height);
    }
  }
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
  nscoord                specifiedHeight = 0;
  const nsStylePosition* position;
  
  // Get the height specified in the style information
  aCellFrame->GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);
  
  switch (position->mHeight.GetUnit()) {
  case eStyleUnit_Coord:
    specifiedHeight = position->mHeight.GetCoordValue();
    break;
  case eStyleUnit_Percent:
    {
      nsTableFrame* table = nsnull;
      nsTableFrame::GetTableFrame(this, table);
      if (table) {
        nscoord basis = table->GetPercentBasisForRows();
        if (basis > 0) {
          float percent = position->mHeight.GetPercentValue();
          specifiedHeight = NSToCoordRound(percent * ((float)basis));
        }
      }
      break;
    }
  case eStyleUnit_Inherit:
    // XXX for now, do nothing
  default:
  case eStyleUnit_Auto:
    break;
  }

  // If the specified height is greater than the desired height, then use the
  // specified height
  if (specifiedHeight > aDesiredHeight)
    aDesiredHeight = specifiedHeight;
 
  if (0 == aDesiredWidth) { // special Nav4 compatibility code for the width
    aDesiredWidth = aAvailWidth;
  } 

  return NS_OK;
}

// Calculates the available width for the table cell based on the known
// column widths taking into account column spans and column spacing
nscoord
nsTableRowFrame::CalculateCellAvailableWidth(nsTableFrame* aTableFrame,
                                             nsIFrame*     aCellFrame,
                                             PRInt32       aCellColIndex,
                                             PRInt32       aNumColSpans,
                                             nscoord       aCellSpacingX)
{
  nscoord availWidth = 0;
  for (PRInt32 index = 0; index < aNumColSpans; index++) {
    // Add in the width of this column
    availWidth += aTableFrame->GetColumnWidth(aCellColIndex + index);

    // If the cell spans columns, then for all columns except the first column
    // add in the column spacing
    if ((index != 0) && (aTableFrame->GetNumCellsOriginatingInCol(aCellColIndex + index) > 0)) {
      availWidth += aCellSpacingX;
    }
  }

  return availWidth;
}

/**
 * Called for a resize reflow. Typically because the column widths have
 * changed. Reflows all the existing table cell frames unless aDirtyOnly
 * is PR_TRUE in which case only reflow the dirty frames
 */
NS_METHOD nsTableRowFrame::ResizeReflow(nsIPresContext*      aPresContext,
                                        nsHTMLReflowMetrics& aDesiredSize,
                                        RowReflowState&      aReflowState,
                                        nsReflowStatus&      aStatus,
                                        PRBool               aDirtyOnly)
{
  aStatus = NS_FRAME_COMPLETE;
  if (nsnull == mFrames.FirstChild()) {
    return NS_OK;
  }

  nsresult rv = NS_OK;
  ResetTallestCell(aReflowState.reflowState.mComputedHeight);

  nsSize  localKidMaxElementSize(0,0);
  nsSize* kidMaxElementSize = (aDesiredSize.maxElementSize) ? &localKidMaxElementSize : nsnull;

  nscoord cellSpacingX = aReflowState.tableFrame->GetCellSpacingX();
  PRInt32 cellColSpan=1;  // must be defined here so it's set properly for non-cell kids
  
  PRInt32 prevColIndex; // remember the col index of the previous cell to handle rowspans into this row

  nsTableIteration dir = (aReflowState.reflowState.availableWidth == NS_UNCONSTRAINEDSIZE)
                         ? eTableLTR : eTableDIR;
  nsTableIterator iter(aPresContext, *this, dir);
  if (iter.IsLeftToRight()) {
    prevColIndex = -1;
  }
  else {
    nsTableFrame* tableFrame = nsnull;
    rv = nsTableFrame::GetTableFrame(this, tableFrame);
    if (NS_FAILED(rv) || (nsnull == tableFrame)) {
      return rv;
    }
    prevColIndex = tableFrame->GetColCount();
  }

  // Reflow each of our existing cell frames
  nsIFrame* kidFrame = iter.First();
  while (nsnull != kidFrame) {
    // Get the frame state bits
    nsFrameState  frameState;
    kidFrame->GetFrameState(&frameState);
    
    // See if we should only reflow the dirty child frames
    PRBool  doReflowChild = PR_TRUE;
    if (aDirtyOnly) {
      if ((frameState & NS_FRAME_IS_DIRTY) == 0) {
        doReflowChild = PR_FALSE;
      }
    }

    // Reflow the child frame
    if (doReflowChild) {
      const nsStyleDisplay *kidDisplay;
      kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)kidDisplay));
      if (NS_STYLE_DISPLAY_TABLE_CELL == kidDisplay->mDisplay) {
        PRInt32 cellColIndex;
        ((nsTableCellFrame *)kidFrame)->GetColIndex(cellColIndex);
        cellColSpan = aReflowState.tableFrame->GetEffectiveColSpan((nsTableCellFrame &)*kidFrame);
   
        // Compute the x-origin for the child, taking into account straddlers (cells from prior
        // rows with rowspans > 1)
        // if this cell is not immediately adjacent to the previous cell, factor in missing col info
        if (iter.IsLeftToRight()) {
          if (prevColIndex != (cellColIndex - 1)) { 
            for (PRInt32 colIndex = prevColIndex + 1; cellColIndex > colIndex; colIndex++) {
              aReflowState.x += aReflowState.tableFrame->GetColumnWidth(colIndex);
              if (aReflowState.tableFrame->GetNumCellsOriginatingInCol(colIndex) > 0) {
                aReflowState.x += cellSpacingX;
              }
            }
          }
        }
        else {
          if (prevColIndex != cellColIndex + cellColSpan) { 
            PRInt32 lastCol = cellColIndex + cellColSpan - 1;
            for (PRInt32 colIndex = prevColIndex - 1; colIndex > lastCol; colIndex--) {
              aReflowState.x += aReflowState.tableFrame->GetColumnWidth(colIndex);
              if (aReflowState.tableFrame->GetNumCellsOriginatingInCol(colIndex) > 0) {
                aReflowState.x += cellSpacingX;
              }
            }
          }
        }
  
        aReflowState.x += cellSpacingX;
  
        // Calculate the available width for the table cell using the known
        // column widths
        nscoord availWidth;
        if (aDirtyOnly && (frameState & NS_FRAME_FIRST_REFLOW)) {
          // This is the initial reflow for the cell and so we do an unconstrained
          // reflow.
          // Note: don't assume that we have known column widths. If we don't, then
          // CalculateCellAvailableWidth() may assert if called now...
          availWidth = NS_UNCONSTRAINEDSIZE;
        } else {
          availWidth = CalculateCellAvailableWidth(aReflowState.tableFrame,
                                                   kidFrame, cellColIndex,
                                                   cellColSpan, cellSpacingX);
          if (aReflowState.x + availWidth > aReflowState.reflowState.mComputedWidth)
            availWidth -= (aReflowState.x + availWidth - aReflowState.reflowState.mComputedWidth);
        }
        
        // remember the rightmost (ltr) or leftmost (rtl) column this cell spans into
        prevColIndex = (iter.IsLeftToRight()) ? cellColIndex + (cellColSpan - 1) : cellColIndex;
        nsHTMLReflowMetrics desiredSize(kidMaxElementSize);
  
        // If the available width is the same as last time we reflowed the cell,
        // then just use the previous desired size and max element size.
        // if we need the max-element-size we don't need to reflow.
        // we just grab it from the cell frame which remembers it (see the else clause below).
        // Note: we can't do that optimization if our height is constrained or the
        // cell frame has a next-in-flow
        nsIFrame* kidNextInFlow;
        kidFrame->GetNextInFlow(&kidNextInFlow);
        if ((aReflowState.reflowState.availableHeight != NS_UNCONSTRAINEDSIZE) ||
            (availWidth != ((nsTableCellFrame *)kidFrame)->GetPriorAvailWidth()) ||
            (nsnull != kidNextInFlow))
        {
          // Reflow the cell to fit the available height
          nsSize  kidAvailSize(availWidth, aReflowState.reflowState.availableHeight);

          // If it's a dirty frame, then check whether it's the initial reflow
          nsReflowReason  reason = eReflowReason_Resize;
          if (aDirtyOnly) {
            if (frameState & NS_FRAME_FIRST_REFLOW) {
              // Newly inserted frame
              reason = eReflowReason_Initial;

              // Use an unconstrained width so we can get the child's maximum width
              // XXX What about fixed layout tables?
              kidAvailSize.SizeTo(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
              // request to get the max element size if not already so
              if (!kidMaxElementSize) {
                kidMaxElementSize = &localKidMaxElementSize;
                desiredSize.maxElementSize = kidMaxElementSize;
              }
            }
          }
  
          // Reflow the child
          nsTableCellReflowState kidReflowState(aPresContext, aReflowState.reflowState, kidFrame,
                                                kidAvailSize, reason);

          nsReflowStatus status;
          rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState,
                           aReflowState.x, 0, 0, status);
#ifdef NS_DEBUG_karnaze
          if (desiredSize.width > availWidth)
          {
            printf("WARNING: cell returned desired width %d given avail width %d\n",
                    desiredSize.width, availWidth);
          }
#endif

          if (eReflowReason_Initial == reason) {
            nscoord oldMaxWidth = ((nsTableCellFrame *)kidFrame)->GetMaximumWidth();
            // Save the pass1 reflow information
            ((nsTableCellFrame *)kidFrame)->SetMaximumWidth(desiredSize.width);
            // invalidate the table's max width if the cell's max width changes
            if (oldMaxWidth != desiredSize.mMaximumWidth) {
              aReflowState.tableFrame->InvalidateMaximumWidth();
            }
            if (kidMaxElementSize) {
              ((nsTableCellFrame *)kidFrame)->SetPass1MaxElementSize(*kidMaxElementSize);
            }
          }

          // If any of the cells are not complete, then we're not complete
          if (NS_FRAME_IS_NOT_COMPLETE(status)) {
            aStatus = NS_FRAME_NOT_COMPLETE;
          }
        }
        else
        {
          nsSize priorSize = ((nsTableCellFrame *)kidFrame)->GetDesiredSize();
          desiredSize.width = priorSize.width;
          desiredSize.height = priorSize.height;
          if (kidMaxElementSize) 
            *kidMaxElementSize = ((nsTableCellFrame *)kidFrame)->GetPass1MaxElementSize();

          // Because we may have moved the frame we need to make sure any views are
          // positioned properly. We have to do this, because any one of our parent
          // frames could have moved and we have no way of knowing...
          nsIView*  view;
          kidFrame->GetView(aPresContext, &view);
          if (view) {
            nsContainerFrame::PositionFrameView(aPresContext, kidFrame, view);
          } else {
            nsContainerFrame::PositionChildViews(aPresContext, kidFrame);
          }
        }
  
        // Calculate the cell's actual size given its pass2 size. This function
        // takes into account the specified height (in the style), and any special
        // logic needed for backwards compatibility
        CalculateCellActualSize(kidFrame, desiredSize.width, 
                                desiredSize.height, availWidth);

        // height may have changed, adjust descent to absorb any excess difference
        nscoord ascent = ((nsTableCellFrame *)kidFrame)->GetDesiredAscent();
        nscoord descent = desiredSize.height - ascent;
        SetTallestCell(desiredSize.height, ascent, descent, aReflowState.tableFrame, (nsTableCellFrame*)kidFrame);

        // Place the child
        PlaceChild(aPresContext, aReflowState, kidFrame, desiredSize,
                   aReflowState.x, 0,
                   aDesiredSize.maxElementSize, kidMaxElementSize);
  
      }
      else
      {// it's an unknown frame type, give it a generic reflow and ignore the results
        nsTableCellReflowState kidReflowState(aPresContext, aReflowState.reflowState,
                                              kidFrame, nsSize(0,0), eReflowReason_Resize);
        nsHTMLReflowMetrics desiredSize(nsnull);
        nsReflowStatus  status;
        ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState, 0, 0, 0, status);
        kidFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
      }
    }

    kidFrame = iter.Next(); // Get the next child
    // if this was the last child, and it had a colspan>1, add in the cellSpacing for the colspan
    // if the last kid wasn't a colspan, then we still have the colspan of the last real cell
    if ((nsnull==kidFrame) && (cellColSpan>1))
      aReflowState.x += cellSpacingX;
  }

  // Return our desired size. Note that our desired width is just whatever width
  // we were given by the row group frame
  aDesiredSize.width  = aReflowState.x;
  aDesiredSize.height = GetTallestCell();  
#ifdef DEBUG_karnaze
  nscoord overAllocated = aDesiredSize.width - aReflowState.reflowState.availableWidth;
  if (overAllocated > 0) {
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    if (overAllocated > p2t) {
      printf("row over allocated by %d\n twips", overAllocated);
    }
  }
#endif

  return rv;
}

/**
 * Called for the initial reflow. Creates each table cell frame, and
 * reflows it to gets its minimum and maximum sizes
 */
NS_METHOD
nsTableRowFrame::InitialReflow(nsIPresContext*      aPresContext,
                               nsHTMLReflowMetrics& aDesiredSize,
                               RowReflowState&      aReflowState,
                               nsReflowStatus&      aStatus,
                               nsTableCellFrame *   aStartFrame,
                               PRBool               aDoSiblings)
{
  nsresult  rv = NS_OK;
  ResetTallestCell(aReflowState.reflowState.mComputedHeight);

  // Place our children, one at a time, until we are out of children
  nsSize    kidMaxElementSize(0,0);
  nscoord   x = 0;
  nsTableFrame* table = aReflowState.tableFrame;
  PRBool    isAutoLayout = table->IsAutoLayout(&aReflowState.reflowState);
  nscoord   cellSpacingX = table->GetCellSpacingX();

  nsIFrame* kidFrame;
  if (nsnull==aStartFrame)
    kidFrame = mFrames.FirstChild();
  else
    kidFrame = aStartFrame;

  for ( ; nsnull != kidFrame; kidFrame->GetNextSibling(&kidFrame)) {
    const nsStyleDisplay *kidDisplay;
    kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)kidDisplay));
    if (NS_STYLE_DISPLAY_TABLE_CELL == kidDisplay->mDisplay) {
      // For the initial reflow always allow the child to be as high as it
      // wants. The default available width is also unconstrained so we can
      // get the child's maximum width
      nsSize  kidAvailSize;
      nsHTMLReflowMetrics kidSize(nsnull);
      if (isAutoLayout) {
        kidAvailSize.SizeTo(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
        kidSize.maxElementSize=&kidMaxElementSize;
      }
      else {
        PRInt32 colIndex;
        ((nsTableCellFrame *)kidFrame)->GetColIndex(colIndex);
        kidAvailSize.SizeTo(table->GetColumnWidth(colIndex), NS_UNCONSTRAINEDSIZE); 
      }

      nsTableCellReflowState kidReflowState(aPresContext, aReflowState.reflowState,
                                            kidFrame, kidAvailSize, eReflowReason_Initial);

      rv = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState,
                       x + cellSpacingX, 0, 0, aStatus);

      // the following signals bugs in the content frames.  
      if (kidMaxElementSize.width > kidSize.width) {
#ifdef DEBUG_karnaze
        printf("WARNING - table cell content max element width %d greater than desired width %d\n",
          kidMaxElementSize.width, kidSize.width);
#endif
        kidSize.width = kidMaxElementSize.width;
      }
      if (kidMaxElementSize.height > kidSize.height) {
#ifdef DEBUG_karnaze
        printf("Warning - table cell content max element height %d greater than desired height %d\n",
          kidMaxElementSize.height, kidSize.height);
#endif
        kidSize.height = kidMaxElementSize.height;
      }

      ((nsTableCellFrame *)kidFrame)->SetMaximumWidth(kidSize.width);
      ((nsTableCellFrame *)kidFrame)->SetPass1MaxElementSize(kidMaxElementSize);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "unexpected child reflow status");

      // Place the child
      x += cellSpacingX;
      // XXX do we need to call CalculateCellActualSize?
      PlaceChild(aPresContext, aReflowState, kidFrame, kidSize, x, 0,
                 aDesiredSize.maxElementSize, &kidMaxElementSize);
      SetTallestCell(aDesiredSize.height, aDesiredSize.ascent, aDesiredSize.descent, aReflowState.tableFrame, (nsTableCellFrame*)kidFrame);
      x += kidSize.width + cellSpacingX;
    }
    else
    {// it's an unknown frame type, give it a generic reflow and ignore the results
      nsTableCellReflowState kidReflowState(aPresContext, aReflowState.reflowState,
                                            kidFrame, nsSize(0,0), eReflowReason_Initial);
      nsHTMLReflowMetrics desiredSize(nsnull);
      ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState, 0, 0, 0, aStatus);
      kidFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
    }
    if (PR_FALSE==aDoSiblings)
      break;
  }

  // Return our desired size
  aDesiredSize.width  = x;
  aDesiredSize.height = GetTallestCell();  

  return rv;
}

// Recover the reflow state to what it should be if aKidFrame is about
// to be reflowed
//
// The things in the RowReflowState object we need to restore are:
// - max-element size
// - x
NS_METHOD nsTableRowFrame::RecoverState(nsIPresContext* aPresContext,
                                        RowReflowState& aReflowState,
                                        nsIFrame*       aKidFrame,
                                        nsSize*         aMaxElementSize)
{
  // Initialize OUT parameters
  if (aMaxElementSize) {
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }

  // Walk the child frames (except aKidFrame) and find the table cell frames
  for (nsIFrame* frame = mFrames.FirstChild(); frame; frame->GetNextSibling(&frame)) {
    if (frame != aKidFrame) {
      // See if the frame is a table cell frame
      const nsStyleDisplay *kidDisplay;
      frame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)kidDisplay));

      if (NS_STYLE_DISPLAY_TABLE_CELL == kidDisplay->mDisplay) {
        // Recover the max element size if requested,
        // ignoring the height of cells that span rows
        if (aMaxElementSize) {
          nsSize  kidMaxElementSize = ((nsTableCellFrame *)frame)->GetPass1MaxElementSize();
          aMaxElementSize->width += kidMaxElementSize.width;

          PRInt32 rowSpan = aReflowState.tableFrame->GetEffectiveRowSpan((nsTableCellFrame &)*frame);
          if (1 == rowSpan) {
            aMaxElementSize->height = PR_MAX(aMaxElementSize->height, kidMaxElementSize.height);
          }
        }
      }
    }
  }

  // Update the running x-offset based on the frame's current x-origin
  nsPoint origin;
  aKidFrame->GetOrigin(origin);
  aReflowState.x = origin.x;

  return NS_OK;
}


NS_METHOD nsTableRowFrame::IncrementalReflow(nsIPresContext*       aPresContext,
                                             nsHTMLReflowMetrics&  aDesiredSize,
                                             RowReflowState&       aReflowState,
                                             nsReflowStatus&       aStatus)
{
  nsresult  rv = NS_OK;
  CalcTallestCell(); // need to recalculate it based on last reflow sizes
 
  // determine if this frame is the target or not
  nsIFrame *target=nsnull;
  rv = aReflowState.reflowState.reflowCommand->GetTarget(target);
  if ((PR_TRUE==NS_SUCCEEDED(rv)) && (nsnull!=target))
  {
    if (this==target)
      rv = IR_TargetIsMe(aPresContext, aDesiredSize, aReflowState, aStatus);
    else
    {
      // Get the next frame in the reflow chain
      nsIFrame* nextFrame;
      aReflowState.reflowState.reflowCommand->GetNext(nextFrame);
      rv = IR_TargetIsChild(aPresContext, aDesiredSize, aReflowState, aStatus, nextFrame);
    }
  }
  return rv;
}

NS_METHOD nsTableRowFrame::IR_TargetIsMe(nsIPresContext*      aPresContext,
                                         nsHTMLReflowMetrics& aDesiredSize,
                                         RowReflowState&      aReflowState,
                                         nsReflowStatus&      aStatus)
{
  nsresult rv = NS_FRAME_COMPLETE;
  nsIReflowCommand::ReflowType type;
  aReflowState.reflowState.reflowCommand->GetType(type);
  nsIFrame *objectFrame;
  aReflowState.reflowState.reflowCommand->GetChildFrame(objectFrame); 
  const nsStyleDisplay *childDisplay=nsnull;
  if (nsnull!=objectFrame)
    objectFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
  switch (type)
  {
  case nsIReflowCommand::ReflowDirty:
  {
    // Reflow the dirty child frames. Typically this is newly added frames.
    // XXX What we really should do is do a pass-1 reflow of newly added
    // frames (only if necessary, i.e., the table isn't fixed layout), then
    // see if column widtsh changed and decide whether to do the pass-2 reflow
    // of just the dirty rows or have the table rebalance column widths and
    // do a pass-2 reflow of all rows
    rv = ResizeReflow(aPresContext, aDesiredSize, aReflowState, aStatus, PR_TRUE);

    // If any column widths have to change due to this, rebalance column widths.
    // XXX need to calculate this, but for now just do it
    aReflowState.tableFrame->InvalidateColumnWidths();  
    break;
  }

  case nsIReflowCommand::StyleChanged :
    rv = IR_StyleChanged(aPresContext, aDesiredSize, aReflowState, aStatus);
    break;

  case nsIReflowCommand::ContentChanged :
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

NS_METHOD nsTableRowFrame::IR_StyleChanged(nsIPresContext*      aPresContext,
                                           nsHTMLReflowMetrics& aDesiredSize,
                                           RowReflowState&      aReflowState,
                                           nsReflowStatus&      aStatus)
{
  nsresult rv = NS_OK;
  // we presume that all the easy optimizations were done in the nsHTMLStyleSheet before we were called here
  // XXX: we can optimize this when we know which style attribute changed
  aReflowState.tableFrame->InvalidateFirstPassCache();
  return rv;
}

NS_METHOD nsTableRowFrame::IR_TargetIsChild(nsIPresContext*      aPresContext,
                                            nsHTMLReflowMetrics& aDesiredSize,
                                            RowReflowState&      aReflowState,
                                            nsReflowStatus&      aStatus,
                                            nsIFrame *           aNextFrame)

{
  nsresult  rv;

  const nsStyleDisplay *childDisplay;
  aNextFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)childDisplay));
  if (NS_STYLE_DISPLAY_TABLE_CELL == childDisplay->mDisplay)
  {
    // Recover our reflow state
    RecoverState(aPresContext, aReflowState, aNextFrame, aDesiredSize.maxElementSize);

    // At this point, we know the column widths. Compute the cell available width
    PRInt32 cellColIndex;
    ((nsTableCellFrame *)aNextFrame)->GetColIndex(cellColIndex);
    PRInt32 cellColSpan = aReflowState.tableFrame->GetEffectiveColSpan((nsTableCellFrame &)*aNextFrame);
    nscoord cellSpacingX = aReflowState.tableFrame->GetCellSpacingX();

    nscoord cellAvailWidth = CalculateCellAvailableWidth(aReflowState.tableFrame, aNextFrame,
                                                         cellColIndex, cellColSpan, cellSpacingX);

    // Always let the cell be as high as it wants. We ignore the height that's
    // passed in and always place the entire row. Let the row group decide
    // whether we fit or wehther the entire row is pushed
    nsSize  kidAvailSize(cellAvailWidth, NS_UNCONSTRAINEDSIZE);

    // Pass along the reflow command
    nsSize              kidMaxElementSize;
    // Unless this is a fixed-layout table, then have the cell incrementally
    // update its maximum width. XXX should not skip if the cell is in the 1st row
    nsHTMLReflowMetrics cellMet(&kidMaxElementSize, aReflowState.tableFrame->IsAutoLayout() ?
                                NS_REFLOW_CALC_MAX_WIDTH : 0);
    nsTableCellReflowState kidRS(aPresContext, aReflowState.reflowState,
                                 aNextFrame, kidAvailSize);

    // Remember the current desired size, we'll need it later
    nsSize  oldCellMinSize      = ((nsTableCellFrame*)aNextFrame)->GetPass1MaxElementSize();
    nscoord oldCellMaximumWidth = ((nsTableCellFrame*)aNextFrame)->GetMaximumWidth();
    nsSize  oldCellDesSize      = ((nsTableCellFrame*)aNextFrame)->GetDesiredSize();
    nscoord oldCellDesAscent    = ((nsTableCellFrame*)aNextFrame)->GetDesiredAscent();
    nscoord oldCellDesDescent   = oldCellDesSize.height - oldCellDesAscent;
    
    // Reflow the cell passing it the incremental reflow command. We can't pass
    // in a max width of NS_UNCONSTRAINEDSIZE, because the max width must match
    // the width of the previous reflow...
    rv = ReflowChild(aNextFrame, aPresContext, cellMet, kidRS,
                     aReflowState.x, 0, 0, aStatus);
    nsSize initCellDesSize(cellMet.width, cellMet.height);
    nscoord initCellDesAscent = cellMet.ascent;
    nscoord initCellDesDescent = cellMet.descent;
    
    // Update the cell layout data.. If the cell's maximum width changed,
    // then inform the table that its maximum width needs to be recomputed
    ((nsTableCellFrame *)aNextFrame)->SetPass1MaxElementSize(kidMaxElementSize);
    if (cellMet.mFlags & NS_REFLOW_CALC_MAX_WIDTH) {
      // If the cell is not auto-width, then the natural width is the
      // desired width. XXX what about Pct width?
      PRBool  isAutoWidth = eStyleUnit_Auto == kidRS.mStylePosition->mWidth.GetUnit();
      
      if (!isAutoWidth) {
        // Reset the natural width to be the desired width
        // XXX This isn't the best thing to do, but if we don't then the table
        // layout strategy will compute a different maximum content width than we
        // computed for the initial reflow. That's because the table layout
        // strategy doesn't check whether the cell is auto-width...
        cellMet.mMaximumWidth = PR_MIN(cellMet.mMaximumWidth, cellMet.width);
      }
      ((nsTableCellFrame *)aNextFrame)->SetMaximumWidth(cellMet.mMaximumWidth);
      if (oldCellMaximumWidth != cellMet.mMaximumWidth) {
        aReflowState.tableFrame->InvalidateMaximumWidth();
      }
    }

    // Now that we know the minimum and preferred widths see if the column
    // widths need to be rebalanced
    if (!aReflowState.tableFrame->ColumnsAreValidFor(*(nsTableCellFrame*)aNextFrame,
                                                     oldCellMinSize.width,
                                                     oldCellMaximumWidth)) {
      // The column widths need to be rebalanced. Tell the table to rebalance
      // the column widths
      aReflowState.tableFrame->InvalidateColumnWidths();
    } 

    // Calculate the cell's actual size given its pass2 size. This function
    // takes into account the specified height (in the style), and any special
    // logic needed for backwards compatibility
    CalculateCellActualSize(aNextFrame, cellMet.width, cellMet.height, cellAvailWidth);

    // height may have changed, adjust descent to absorb any excess difference
    cellMet.descent = cellMet.height - cellMet.ascent;

    // if the cell got shorter and it may have been the tallest, recalc the tallest cell
    PRBool tallestCellGotShorter = PR_FALSE;
    PRBool hasVerticalAlignBaseline = ((nsTableCellFrame*)aNextFrame)->HasVerticalAlignBaseline();
    if (!hasVerticalAlignBaseline) { 
      // only the height matters
      tallestCellGotShorter = 
        TallestCellGotShorter(oldCellDesSize.height, initCellDesSize.height, 
          cellMet.height, mTallestCell);
    }
    else {
      // the ascent matters
      tallestCellGotShorter = 
        TallestCellGotShorter(oldCellDesAscent, initCellDesAscent, 
          cellMet.ascent, mMaxCellAscent);
      // the descent of cells without rowspan also matters
      if (!tallestCellGotShorter) {
        PRInt32 rowSpan = aReflowState.tableFrame->GetEffectiveRowSpan((nsTableCellFrame&)*aNextFrame);
        if (rowSpan == 1) {
         tallestCellGotShorter = 
           TallestCellGotShorter(oldCellDesAscent, initCellDesDescent, 
             cellMet.descent, mMaxCellDescent);
        }
      }
    }
    if (tallestCellGotShorter) {
      CalcTallestCell();
    }
    else {
      SetTallestCell(cellMet.height, cellMet.ascent, cellMet.descent, aReflowState.tableFrame, (nsTableCellFrame*)aNextFrame);
    }

    // if the cell's desired size didn't changed, our height is unchanged
    aDesiredSize.mNothingChanged = PR_FALSE;
    PRInt32 rowSpan = aReflowState.tableFrame->GetEffectiveRowSpan((nsTableCellFrame&)*aNextFrame);
    if (initCellDesSize.width == oldCellDesSize.width) {
      if (!hasVerticalAlignBaseline) { // only the cell's height matters
        if (initCellDesSize.height == oldCellDesSize.height) {
          aDesiredSize.mNothingChanged = PR_TRUE;
        }
      }
      else { // cell's ascent and cell's descent matter
        if (initCellDesAscent == oldCellDesAscent) {
          if ((rowSpan == 1) && (initCellDesDescent == oldCellDesDescent)) {
            aDesiredSize.mNothingChanged = PR_TRUE;
          }
        }
      }
    }
    aDesiredSize.height = (aDesiredSize.mNothingChanged) ? mRect.height : GetTallestCell();
    cellMet.height = (rowSpan == 1) ? aDesiredSize.height : PR_MAX(aDesiredSize.height, cellMet.height);

    // Now place the child
    PlaceChild(aPresContext, aReflowState, aNextFrame, cellMet, aReflowState.x,
               0, aDesiredSize.maxElementSize, &kidMaxElementSize);


    // Return our desired size. Note that our desired width is just whatever width
    // we were given by the row group frame
    aDesiredSize.width  = aReflowState.availSize.width;
    if (!aDesiredSize.mNothingChanged) {
      if (aDesiredSize.height == mRect.height) { // our height didn't change
        ((nsTableCellFrame *)aNextFrame)->VerticallyAlignChild(aPresContext, aReflowState.reflowState, mMaxCellAscent);
        nsRect dirtyRect;
        aNextFrame->GetRect(dirtyRect);
        dirtyRect.height = mRect.height;
        Invalidate(aPresContext, dirtyRect);
      }
    }
  }
  else
  { // pass reflow to unknown frame child
    // aDesiredSize does not change
  }

  // When returning whether we're complete we need to look at each of our cell
  // frames. If any of them has a continuing frame, then we're not complete. We
  // can't just return the status of the cell frame we just reflowed...
  aStatus = NS_FRAME_COMPLETE;
  if (mNextInFlow) {
    for (nsIFrame* cell = mFrames.FirstChild(); cell; cell->GetNextSibling(&cell)) {
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
nsTableRowFrame::Reflow(nsIPresContext*          aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTableRowFrame", aReflowState.reason);
  if (nsDebugTable::gRflRow) nsTableFrame::DebugReflow("TR::Rfl en", this, &aReflowState, nsnull);
  nsresult rv = NS_OK;

  // Initialize 'out' parameters (aStatus set below, undefined if rv returns an error)
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  // Create a reflow state
  nsTableFrame *tableFrame=nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (NS_FAILED(rv) || nsnull==tableFrame)
    return rv;
  RowReflowState state(aReflowState, tableFrame);

  // Do the reflow
  // XXX If the width is unconstrained, then treat it like we would treat the
  // initial reflow instead. This needs to be cleaned up
  nsReflowReason  reason = aReflowState.reason;
  if (NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) {
    reason = eReflowReason_Initial;
  }

  switch (reason) {
  case eReflowReason_Initial:
    rv = InitialReflow(aPresContext, aDesiredSize, state, aStatus, nsnull, PR_TRUE);
    if (!tableFrame->IsAutoLayout())
    { // this resize reflow is necessary to place the cells correctly in the case of rowspans and colspans.  
      // It is very efficient.  It does not actually need to pass a reflow down to the cells.
      nsSize  availSpace(aReflowState.availableWidth, aReflowState.availableHeight);
      nsHTMLReflowState  resizeReflowState(aPresContext,
                                           (const nsHTMLReflowState&)(*(aReflowState.parentReflowState)),
                                           (nsIFrame *)this,
                                           availSpace,
                                           eReflowReason_Resize);
      RowReflowState rowResizeReflowState(resizeReflowState, tableFrame);
      rv = ResizeReflow(aPresContext, aDesiredSize, rowResizeReflowState, aStatus);
    }
    //GetMinRowSpan(tableFrame);
    //FixMinCellHeight(tableFrame);
    aStatus = NS_FRAME_COMPLETE;
    break;

  case eReflowReason_Resize:
  case eReflowReason_StyleChange:
    rv = ResizeReflow(aPresContext, aDesiredSize, state, aStatus);
    break;

  case eReflowReason_Incremental:
    rv = IncrementalReflow(aPresContext, aDesiredSize, state, aStatus);
    break;
  }

  // If we computed our max element element size, then cache it so we can return
  // it later when asked
  if (aDesiredSize.maxElementSize) {
    mMaxElementSize = *aDesiredSize.maxElementSize;
  }

  if (nsDebugTable::gRflRow) nsTableFrame::DebugReflow("TR::Rfl ex", this, nsnull, &aDesiredSize, aStatus);
  return rv;
}

/* we overload this here because rows have children that can span outside of themselves.
 * so the default "get the child rect, see if it contains the event point" action isn't
 * sufficient.  We have to ask the row if it has a child that contains the point.
 */
PRBool nsTableRowFrame::Contains(nsIPresContext* aPresContext, const nsPoint& aPoint)
{
  PRBool result = PR_FALSE;
  // first, check the row rect and see if the point is in their
  if (mRect.Contains(aPoint)) {
    result = PR_TRUE;
  }
  // if that fails, check the cells, they might span outside the row rect
  else {
    nsIFrame* kid;
    FirstChild(aPresContext, nsnull, &kid);
    while (nsnull != kid) {
      nsRect kidRect;
      kid->GetRect(kidRect);
      nsPoint point(aPoint);
      point.MoveBy(-mRect.x, -mRect.y); // offset the point to check by the row container
      if (kidRect.Contains(point)) {
        result = PR_TRUE;
        break;
      }
      kid->GetNextSibling(&kid);
    }
  }
  return result;
}

/**
 * This function is called by the row group frame's SplitRowGroup() code when
 * pushing a row frame that has cell frames that span into it. The cell frame
 * should be reflowed with the specified height
 */
void nsTableRowFrame::ReflowCellFrame(nsIPresContext*          aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsTableCellFrame*        aCellFrame,
                                      nscoord                  aAvailableHeight,
                                      nsReflowStatus&          aStatus)
{
  // Reflow the cell frame with the specified height. Use the existing width
  nsSize  cellSize;
  aCellFrame->GetSize(cellSize);
  
  nsSize  availSize(cellSize.width, aAvailableHeight);
  nsTableCellReflowState cellReflowState(aPresContext, aReflowState, aCellFrame, availSize,
                                         eReflowReason_Resize);
  nsHTMLReflowMetrics desiredSize(nsnull);

  ReflowChild(aCellFrame, aPresContext, desiredSize, cellReflowState,
              0, 0, NS_FRAME_NO_MOVE_FRAME, aStatus);
  aCellFrame->SizeTo(aPresContext, cellSize.width, aAvailableHeight);
  // XXX What happens if this cell has 'vertical-align: baseline' ?
  // XXX Why is it assumed that the cell's ascent hasn't changed ?
  aCellFrame->VerticallyAlignChild(aPresContext, aReflowState, mMaxCellAscent);
  aCellFrame->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
}

/**
 * This function is called by the row group frame's SplitRowGroup() code when
 * it creates a continuing cell frame and wants to insert it into the row's
 * child list
 */
void nsTableRowFrame::InsertCellFrame(nsTableCellFrame* aFrame,
                                      nsTableCellFrame* aPrevSibling)
{
  mFrames.InsertFrame(nsnull, aPrevSibling, aFrame);
}

NS_IMETHODIMP
nsTableRowFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::tableRowFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
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
nsTableRowFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("TableRow", aResult);
}

NS_IMETHODIMP
nsTableRowFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  PRUint32 sum = sizeof(*this);
  *aResult = sum;
  return NS_OK;
}
#endif
