/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
#include "nsIPtr.h"
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

NS_DEF_PTR(nsIStyleContext);

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsDebugIR = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsDebugIR = PR_FALSE;
#endif


/* ----------- RowReflowState ---------- */

struct RowReflowState {
  // Our reflow state
  const nsHTMLReflowState& reflowState;

  // The body's available size (computed from the body's parent)
  nsSize availSize;

  // the running x-offset
  nscoord x;

  // Height of tallest cell (excluding cells with rowspan > 1)
  nscoord maxCellHeight;    // just the height of the cell frame
  nscoord maxCellVertSpace; // the maximum MAX(cellheight + topMargin + bottomMargin)
  
  nsTableFrame *tableFrame;

  RowReflowState(const nsHTMLReflowState& aReflowState,
                 nsTableFrame*            aTableFrame)
    : reflowState(aReflowState)
  {
    availSize.width = reflowState.availableWidth;
    availSize.height = reflowState.availableHeight;
    maxCellHeight = 0;
    maxCellVertSpace = 0;
    tableFrame = aTableFrame;
    x=0;
  }
};




/* ----------- nsTableRowpFrame ---------- */

nsTableRowFrame::nsTableRowFrame()
  : nsHTMLContainerFrame(),
    mTallestCell(0),
    mCellMaxTopMargin(0),
    mCellMaxBottomMargin(0),
    mMinRowSpan(1),
    mInitializedChildren(PR_FALSE)
{
}

NS_IMETHODIMP
nsTableRowFrame::Init(nsIPresContext&  aPresContext,
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
nsTableRowFrame::AddTableDirtyReflowCommand(nsIPresContext& aPresContext,
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
nsTableRowFrame::AppendFrames(nsIPresContext& aPresContext,
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
      PRInt32 colIndex = tableFrame->AddCellToTable((nsTableCellFrame*)childFrame,
                                                    GetRowIndex());

      // Initialize the cell frame and give it its column index
      ((nsTableCellFrame*)childFrame)->InitCellFrame(colIndex);
    }
  }

  // See if any implicit column frames need to be created as a result of
  // adding the new rows
  PRBool  createdColFrames;
  tableFrame->EnsureColumns(aPresContext, createdColFrames);
  if (createdColFrames) {
    // We need to rebuild the column cache
    // XXX It would be nice if this could be done incrementally
    tableFrame->InvalidateColumnCache();
  }

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
nsTableRowFrame::InsertFrames(nsIPresContext& aPresContext,
                              nsIPresShell&   aPresShell,
                              nsIAtom*        aListName,
                              nsIFrame*       aPrevFrame,
                              nsIFrame*       aFrameList)
{
  // Get the table frame
  nsTableFrame* tableFrame = nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);
  
  // Insert the frames
  mFrames.InsertFrames(nsnull, aPrevFrame, aFrameList);
  
  // We need to rebuild the cell map, because currently we can't insert
  // new frames except at the end (append)
  tableFrame->InvalidateCellMap();

  // We should try and avoid doing a pass1 reflow on all the cells and just
  // do it for the newly added frames, but we need to add these frames to the
  // cell map before we reflow them
  tableFrame->InvalidateFirstPassCache();

  // Because the number of columns may have changed invalidate the column
  // cache. Note that this has the side effect of recomputing the column
  // widths, so we don't need to call InvalidateColumnWidths()
  tableFrame->InvalidateColumnCache();

  // Generate a reflow command so we reflow the table itself. This will
  // do a pass-1 reflow of all the rows including any rows we just added
  AddTableDirtyReflowCommand(aPresContext, aPresShell, tableFrame);
  return NS_OK;
}

NS_IMETHODIMP
nsTableRowFrame::RemoveFrame(nsIPresContext& aPresContext,
                             nsIPresShell&   aPresShell,
                             nsIAtom*        aListName,
                             nsIFrame*       aOldFrame)
{
  // Get the table frame
  nsTableFrame* tableFrame=nsnull;
  nsTableFrame::GetTableFrame(this, tableFrame);

#if 0
  // XXX Currently we can't incrementally remove cells from the cell map
  PRInt32 colIndex;
  aDeletedFrame->GetColIndex(colIndex);
  tableFrame->RemoveCellFromTable(aOldFrame, GetRowIndex());

  // Remove the frame and destroy it
  mFrames.DestroyFrame(aPresContext, (nsIFrame*)aOldFrame); 

  // XXX Reflow the row

#else
  // Remove the frame and destroy it
  mFrames.DestroyFrame(aPresContext, aOldFrame);

  // We need to rebuild the cell map, because currently we can't incrementally
  // remove rows
  tableFrame->InvalidateCellMap();

  // Because the number of columns may have changed invalidate the column
  // cache. Note that this has the side effect of recomputing the column
  // widths, so we don't need to call InvalidateColumnWidths()
  tableFrame->InvalidateColumnCache();

  // Because we haven't added any new frames we don't need to do a pass1
  // reflow. Just generate a reflow command so we reflow the table itself
  AddTableDirtyReflowCommand(aPresContext, aPresShell, tableFrame);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsTableRowFrame::InitChildren()
{
  if (gsDebug) printf("Row InitChildren: begin\n");
  nsTableFrame* table = nsnull;
  nsresult  result=NS_OK;

  // each child cell can only be added to the table one time.
  // for now, we remember globally whether we've added all or none
  if (PR_FALSE==mInitializedChildren)
  {
    if (gsDebug) printf("Row InitChildren: mInitializedChildren=PR_FALSE\n");
    result = nsTableFrame::GetTableFrame(this, table);
    if ((NS_OK==result) && (table != nsnull))
    {
      mInitializedChildren=PR_TRUE;
      PRInt32 rowIndex = table->GetNextAvailRowIndex();
      SetRowIndex(rowIndex);
      if (gsDebug) printf("Row InitChildren: set row index to %d\n", rowIndex);
      PRInt32   colIndex = 0;
      for (nsIFrame* kidFrame = mFrames.FirstChild(); nsnull != kidFrame; kidFrame->GetNextSibling(&kidFrame)) 
      {
        const nsStyleDisplay *kidDisplay;
        kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)kidDisplay));
        if (NS_STYLE_DISPLAY_TABLE_CELL == kidDisplay->mDisplay)
        {
          // add the cell frame to the table's cell map and get its col index
          PRInt32 colIndex;
          colIndex = table->AddCellToTable((nsTableCellFrame *)kidFrame, rowIndex);
          // what column does this cell belong to?
                    if (gsDebug) printf("Row InitChildren: cell given colIndex %d\n", colIndex);
          // this sets the frame's notion of it's column index
          ((nsTableCellFrame *)kidFrame)->InitCellFrame(colIndex);
                    if (gsDebug) printf("%p : set cell frame %p to col index = %d\n", this, kidFrame, colIndex);
        }
      }
    }
  }
  if (gsDebug) printf("Row InitChildren: end\n");
  return NS_OK;
}

/**
 * Post-reflow hook. This is where the table row does its post-processing
 */
void
nsTableRowFrame::DidResize(nsIPresContext& aPresContext,
                           const nsHTMLReflowState& aReflowState)
{
  if (gsDebug) printf("Row %p DidResize: begin mRect.h=%d, mCellMTM=%d, mCellMBM=%d\n",
                      this, mRect.height, mCellMaxTopMargin, mCellMaxBottomMargin);
  // Resize and re-align the cell frames based on our row height
  nscoord rowCellHeight = mRect.height - mCellMaxTopMargin - mCellMaxBottomMargin;
  if (gsDebug) printf("Row DidReflow: cellHeight=%d\n", rowCellHeight);
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  const nsStyleTable* tableStyle;
  tableFrame->GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);

  nsTableIterator iter(*this, eTableDIR);
  nsIFrame* cellFrame = iter.First();

  while (nsnull != cellFrame) {
    const nsStyleDisplay *kidDisplay;
    cellFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)kidDisplay));
    if (NS_STYLE_DISPLAY_TABLE_CELL == kidDisplay->mDisplay) {
      PRInt32 rowSpan = tableFrame->GetEffectiveRowSpan(mRowIndex, (nsTableCellFrame *)cellFrame);
      if (gsDebug) printf("Row DidReflow: cellFrame %p ", cellFrame);
      nscoord cellHeight = rowCellHeight;
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
        nextRow->GetNextSibling(&nextRow);
      }

      // resize the cell's height
      nsSize  cellFrameSize;
      cellFrame->GetSize(cellFrameSize);
      //if (cellFrameSize.height!=cellHeight)
      {
        cellFrame->SizeTo(cellFrameSize.width, cellHeight);
        if (gsDebug) printf("given height %d\n", cellHeight);
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
        ((nsTableCellFrame *)cellFrame)->VerticallyAlignChild();
        /* if we're collapsing borders, notify the cell that the border edge length has changed */
        if (NS_STYLE_BORDER_COLLAPSE==tableStyle->mBorderCollapse) {
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
  if (gsDebug) printf("Row DidResize: returning NS_OK---------------------------\n");
}

void nsTableRowFrame::ResetMaxChildHeight()
{
  if (gsDebug) printf("Row ResetMaxChildHeight\n");
  mTallestCell=0;
  mCellMaxTopMargin=0;
  mCellMaxBottomMargin=0;
}

void nsTableRowFrame::SetMaxChildHeight(nscoord aChildHeight, nscoord aTopMargin, nscoord aBottomMargin)
{
  if (gsDebug) printf("Row SetMaxChildHeight to %d\n", aChildHeight);
  if (mTallestCell<aChildHeight)
    mTallestCell = aChildHeight;

  if (mCellMaxTopMargin<aTopMargin)
    mCellMaxTopMargin = aTopMargin;

  if (mCellMaxBottomMargin<aBottomMargin)
    mCellMaxBottomMargin = aBottomMargin;
}

  /*
  const nsStyleColor* myColor =
    (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
  if (nsnull != myColor) {
    nsRect  rect(0, 0, mRect.width, mRect.height);
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *myColor);
  }
  */

PRBool IsFirstRow(nsTableFrame&    aTable,
                  nsTableRowFrame& aRow)
{
  nsIFrame* firstRowGroup = nsnull;
  aTable.FirstChild(nsnull, &firstRowGroup);
  nsIFrame* rowGroupFrame = nsnull;
  nsresult rv = aRow.GetParent(&rowGroupFrame);
  if (NS_SUCCEEDED(rv) && (rowGroupFrame == firstRowGroup)) {
    nsIFrame* firstRow;
    rowGroupFrame->FirstChild(nsnull, &firstRow);
    return (&aRow == firstRow);
  }
  return PR_FALSE;
}


NS_METHOD nsTableRowFrame::Paint(nsIPresContext& aPresContext,
                                 nsIRenderingContext& aRenderingContext,
                                 const nsRect& aDirtyRect,
                                 nsFramePaintLayer aWhichLayer)
{
  nsresult rv;
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    nsCompatibility mode;
    aPresContext.GetCompatibilityMode(&mode);
    if (eCompatibility_Standard == mode) {
      const nsStyleDisplay* disp =
        (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
      if (disp->mVisible) {
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
        nscoord halfCellSpacingY = 
          NSToCoordRound(((float)tableFrame->GetCellSpacingY()) / (float)2);
        // every row is short by the ending cell spacing X
        nsRect rect(0, 0, mRect.width + cellSpacingX, mRect.height);
        // first row may have gotten too much cell spacing Y
        if (tableFrame->GetRowCount() != 1) {
          if (IsFirstRow(*tableFrame, *this)) { 
            rect.height -= halfCellSpacingY;
          }
          else {
            rect.height += halfCellSpacingY;
            rect.y      -= halfCellSpacingY;
          }
        }

        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *spacing, 0, 0);
      }
    }
  }
  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,255,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

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
void nsTableRowFrame::PaintChildren(nsIPresContext&      aPresContext,
                                    nsIRenderingContext& aRenderingContext,
                                    const nsRect&        aDirtyRect,
                                    nsFramePaintLayer aWhichLayer)
{
  nsIFrame* kid = mFrames.FirstChild();
  while (nsnull != kid) {
    nsIView *pView;
     
    kid->GetView(&pView);
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
        if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) &&
            GetShowFrameBorders()) {
          aRenderingContext.SetColor(NS_RGB(255,0,0));
          aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
        }
        aRenderingContext.PopState(clipState);
      }
    }
    kid->GetNextSibling(&kid);
  }
}

/** returns the height of the tallest child in this row (ignoring any cell with rowspans) */
nscoord nsTableRowFrame::GetTallestChild() const
{
  return mTallestCell;
}

nscoord nsTableRowFrame::GetChildMaxTopMargin() const
{
  return mCellMaxTopMargin;
}

nscoord nsTableRowFrame::GetChildMaxBottomMargin() const
{
  return mCellMaxBottomMargin;
}

PRInt32 nsTableRowFrame::GetMaxColumns() const
{
  int sum = 0;
  nsIFrame *cell=mFrames.FirstChild();
  while (nsnull!=cell) 
  {
    const nsStyleDisplay *kidDisplay;
    cell->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)kidDisplay));
    if (NS_STYLE_DISPLAY_TABLE_CELL == kidDisplay->mDisplay)
    {
      sum += ((nsTableCellFrame *)cell)->GetColSpan();
    }
    cell->GetNextSibling(&cell);
  }
  return sum;
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
      PRInt32 rowSpan = aTableFrame->GetEffectiveRowSpan(mRowIndex, ((nsTableCellFrame *)frame));
      if (-1==minRowSpan)
        minRowSpan = rowSpan;
      else if (minRowSpan>rowSpan)
        minRowSpan = rowSpan;
    }
    frame->GetNextSibling(&frame);
  }
  mMinRowSpan = minRowSpan;
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
      PRInt32 rowSpan = aTableFrame->GetEffectiveRowSpan(mRowIndex, ((nsTableCellFrame *)frame));
      if (mMinRowSpan==rowSpan)
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
void nsTableRowFrame::PlaceChild(nsIPresContext&    aPresContext,
																 RowReflowState&    aReflowState,
																 nsIFrame*          aKidFrame,
																 const nsRect&      aKidRect,
																 nsSize*            aMaxElementSize,
																 nsSize*            aKidMaxElementSize)
{
  if (PR_TRUE==gsDebug)
    printf ("row: placing cell at %d, %d, %d, %d\n",
           aKidRect.x, aKidRect.y, aKidRect.width, aKidRect.height);

  // Place and size the child
  aKidFrame->SetRect(aKidRect);

  // update the running total for the row width
  aReflowState.x += aKidRect.width;

  // Update the maximum element size
  PRInt32 rowSpan = aReflowState.tableFrame->GetEffectiveRowSpan((nsTableCellFrame*)aKidFrame);
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width += aKidMaxElementSize->width;
    if (1 == rowSpan) {
      aMaxElementSize->height = PR_MAX(aMaxElementSize->height, aKidMaxElementSize->height);
    }
  }

// XXX this will be fixed when row spans deal with dead rows
#if 0
  // this accounts for cases where all cells in a row have a rowspan>1
  // XXX  "numColsInThisRow==numColsInTable" probably isn't the right metric to use here
  //      the point is to skip a cell who's span effects another row, maybe use cellmap?
  PRInt32 numColsInThisRow = GetMaxColumns();
  PRInt32 numColsInTable = aReflowState.tableFrame->GetColCount();
  if ((1==rowSpan) || (mMinRowSpan == rowSpan && numColsInThisRow==numColsInTable))
  {
    // Update maxCellHeight
    if (aKidRect.height > aReflowState.maxCellHeight)
      aReflowState.maxCellHeight = aKidRect.height;

    // Update maxCellVertSpace
    nsMargin margin;

    if (aReflowState.tableFrame->GetCellMarginData((nsTableCellFrame *)aKidFrame, margin) == NS_OK)
    {
      nscoord height = aKidRect.height + margin.top + margin.bottom;
  
      if (height > aReflowState.maxCellVertSpace)
        aReflowState.maxCellVertSpace = height;
    }
  }
#else
  if (1 == rowSpan) {
    // Update maxCellHeight
    if (aKidRect.height > aReflowState.maxCellHeight)
      aReflowState.maxCellHeight = aKidRect.height;

    // Update maxCellVertSpace
    nsMargin margin;

    if (aReflowState.tableFrame->GetCellMarginData((nsTableCellFrame *)aKidFrame, margin) == NS_OK) {
      nscoord height = aKidRect.height + margin.top + margin.bottom;
  
      if (height > aReflowState.maxCellVertSpace)
        aReflowState.maxCellVertSpace = height;
    }
  }
#endif
}

/**
 * Called for a resize reflow. Typically because the column widths have
 * changed. Reflows all the existing table cell frames unless aDirtyOnly
 * is PR_TRUE in which case only reflow the dirty frames
 */
NS_METHOD nsTableRowFrame::ResizeReflow(nsIPresContext&      aPresContext,
                                        nsHTMLReflowMetrics& aDesiredSize,
                                        RowReflowState&      aReflowState,
                                        nsReflowStatus&      aStatus,
                                        PRBool               aDirtyOnly)
{
  aStatus = NS_FRAME_COMPLETE;
  if (nsnull == mFrames.FirstChild()) {
    return NS_OK;
  }

  nsresult rv=NS_OK;
  nsSize  kidMaxElementSize;
  nsSize* pKidMaxElementSize = (nsnull != aDesiredSize.maxElementSize) ?
                                  &kidMaxElementSize : nsnull;
  nscoord maxCellTopMargin = 0;
  nscoord maxCellBottomMargin = 0;
  nscoord cellSpacingX = aReflowState.tableFrame->GetCellSpacingX();
  PRInt32 cellColSpan=1;  // must be defined here so it's set properly for non-cell kids
  if (PR_TRUE==gsDebug) printf("Row %p: Resize Reflow\n", this);
  
  PRInt32 prevColIndex; // remember the col index of the previous cell to handle rowspans into this row

  nsTableIteration dir = (aReflowState.reflowState.availableWidth == NS_UNCONSTRAINEDSIZE)
                         ? eTableLTR : eTableDIR;
  nsTableIterator iter(*this, dir);
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
        cellColSpan = aReflowState.tableFrame->GetEffectiveColSpan(cellColIndex,
                                                                   ((nsTableCellFrame *)kidFrame));
        nsMargin kidMargin;
        aReflowState.tableFrame->GetCellMarginData((nsTableCellFrame *)kidFrame,kidMargin);
        if (kidMargin.top > maxCellTopMargin)
          maxCellTopMargin = kidMargin.top;
        if (kidMargin.bottom > maxCellBottomMargin)
          maxCellBottomMargin = kidMargin.bottom;
   
        // Compute the x-origin for the child, taking into account straddlers (cells from prior
        // rows with rowspans > 1)
        // if this cell is not immediately adjacent to the previous cell, factor in missing col info
        if (iter.IsLeftToRight()) {
          if (prevColIndex != (cellColIndex - 1)) { 
            for (PRInt32 colIndex = prevColIndex + 1; cellColIndex > colIndex; colIndex++) {
              aReflowState.x += aReflowState.tableFrame->GetColumnWidth(colIndex);
              if (aReflowState.tableFrame->GetNumCellsOriginatingIn(colIndex) > 0) {
                aReflowState.x += cellSpacingX;
              }
              if (PR_TRUE==gsDebug)
                printf("  Row: in loop, aReflowState.x set to %d from cellSpacing %d and col width %d\n", 
                       aReflowState.x, cellSpacingX, aReflowState.tableFrame->GetColumnWidth(colIndex));
            }
          }
        }
        else {
          if (prevColIndex != cellColIndex + cellColSpan) { 
            PRInt32 lastCol = cellColIndex + cellColSpan - 1;
            for (PRInt32 colIndex = prevColIndex - 1; colIndex > lastCol; colIndex--) {
              aReflowState.x += aReflowState.tableFrame->GetColumnWidth(colIndex);
              if (aReflowState.tableFrame->GetNumCellsOriginatingIn(colIndex) > 0) {
                aReflowState.x += cellSpacingX;
              }
              if (PR_TRUE==gsDebug)
                printf("  Row: in loop, aReflowState.x set to %d from cellSpacing %d and col width %d\n", 
                       aReflowState.x, cellSpacingX, aReflowState.tableFrame->GetColumnWidth(colIndex));
            }
          }
        }
  
        aReflowState.x += cellSpacingX;
        if (PR_TRUE==gsDebug) printf("  Row: past loop, aReflowState.x set to %d\n", aReflowState.x);
  
        // at this point, we know the column widths.  
        // so we get the avail width from the known column widths
        nscoord availWidth = 0;
        for (PRInt32 numColSpan=0; numColSpan<cellColSpan; numColSpan++) {
          availWidth += aReflowState.tableFrame->GetColumnWidth(cellColIndex+numColSpan);
          if ((numColSpan != 0) && (aReflowState.tableFrame->GetNumCellsOriginatingIn(cellColIndex + numColSpan) > 0)) {
            availWidth += cellSpacingX;
          }
          if (PR_TRUE==gsDebug) {
            printf("  Row: in loop, availWidth set to %d from colIndex %d width %d and cellSpacing\n", 
                    availWidth, cellColIndex, aReflowState.tableFrame->GetColumnWidth(cellColIndex+numColSpan));
          }
        }
        if (PR_TRUE==gsDebug) printf("  Row: availWidth for this cell is %d\n", availWidth);
        // remember the rightmost (ltr) or leftmost (rtl) column this cell spans into
        prevColIndex = (iter.IsLeftToRight()) ? cellColIndex + (cellColSpan - 1) : cellColIndex;
        nsHTMLReflowMetrics desiredSize(pKidMaxElementSize);
  
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
          nsSize  kidMaxElementSize(0,0);

          // If it's a dirty frame, then check whether it's the initial reflow
          nsReflowReason  reason = eReflowReason_Resize;
          if (aDirtyOnly) {
            if (frameState & NS_FRAME_FIRST_REFLOW) {
              // Newly inserted frame
              reason = eReflowReason_Initial;

              // Use an unconstrained width so we can get the child's maximum
              // width
              // XXX What about fixed layout tables?
              kidAvailSize.SizeTo(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
              desiredSize.maxElementSize=&kidMaxElementSize;
            }
          }
  
          // Reflow the child
          nsHTMLReflowState kidReflowState(aPresContext,
                                           aReflowState.reflowState, kidFrame,
                                           kidAvailSize,
                                           reason);
          if (gsDebug) printf ("Row %p RR: avail=%d\n", this, availWidth);
          nsReflowStatus status;
          rv = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState, status);
          if (gsDebug) printf ("Row %p RR: desired=%d\n", this, desiredSize.width);
#ifdef NS_DEBUG
          if (desiredSize.width > availWidth)
          {
            printf("WARNING: cell returned desired width %d given avail width %d\n",
                    desiredSize.width, availWidth);
          }
#endif

          if (eReflowReason_Initial == reason) {
            // Save the pass1 reflow information
            ((nsTableCellFrame *)kidFrame)->SetPass1DesiredSize(desiredSize);
            ((nsTableCellFrame *)kidFrame)->SetPass1MaxElementSize(kidMaxElementSize);
          }

          // If any of the cells are not complete, then we're not complete
          if (NS_FRAME_IS_NOT_COMPLETE(status)) {
            aStatus = NS_FRAME_NOT_COMPLETE;
          }
  
          if (gsDebug)
          {
            if (nsnull!=pKidMaxElementSize)
              printf("Row: reflow of cell returned result = %s with desired=%d,%d, min = %d,%d\n",
                      NS_FRAME_IS_COMPLETE(status)?"complete":"NOT complete", 
                      desiredSize.width, desiredSize.height, 
                      pKidMaxElementSize->width, pKidMaxElementSize->height);
            else
              printf("Row: reflow of cell returned result = %s with desired=%d,%d, min = nsnull\n",
                      NS_FRAME_IS_COMPLETE(status)?"complete":"NOT complete", 
                      desiredSize.width, desiredSize.height);
          }
        }
        else
        {
          nsSize priorSize = ((nsTableCellFrame *)kidFrame)->GetDesiredSize();
          desiredSize.width = priorSize.width;
          desiredSize.height = priorSize.height;
          if (nsnull != pKidMaxElementSize) 
            *pKidMaxElementSize = ((nsTableCellFrame *)kidFrame)->GetPass1MaxElementSize();
        }
  
        // Place the child after taking into account its margin and attributes
        nscoord specifiedHeight = 0;
        nscoord cellHeight = desiredSize.height;
        nsIStyleContextPtr kidSC;
        kidFrame->GetStyleContext(kidSC.AssignPtr());
        const nsStylePosition* kidPosition = (const nsStylePosition*)
          kidSC->GetStyleData(eStyleStruct_Position);
        switch (kidPosition->mHeight.GetUnit()) {
        case eStyleUnit_Coord:
          specifiedHeight = kidPosition->mHeight.GetCoordValue();
          break;
  
        case eStyleUnit_Inherit:
          // XXX for now, do nothing
        default:
        case eStyleUnit_Auto:
          break;
        }
        if (specifiedHeight>cellHeight)
          cellHeight = specifiedHeight;
  
        nscoord cellWidth = desiredSize.width;
        // begin special Nav4 compatibility code
        if (0==cellWidth)
        {
          cellWidth = availWidth;
        }
        // end special Nav4 compatibility code
  
        // Place the child
        nsRect kidRect (aReflowState.x, kidMargin.top, cellWidth, cellHeight);
  
        PlaceChild(aPresContext, aReflowState, kidFrame, kidRect, aDesiredSize.maxElementSize,
                   pKidMaxElementSize);
  
        if (PR_TRUE==gsDebug) printf("Row: past PlaceChild, aReflowState.x set to %d\n", aReflowState.x);
      }
      else
      {// it's an unknown frame type, give it a generic reflow and ignore the results
        nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState,
                                         kidFrame,
                                         nsSize(0,0), eReflowReason_Resize);
        nsHTMLReflowMetrics desiredSize(nsnull);
        if (PR_TRUE==gsDebug) printf("\nRow: Resize Reflow of unknown frame %p of type %d with reason=%d\n", 
                                       kidFrame, kidDisplay->mDisplay, eReflowReason_Resize);
        nsReflowStatus  status;
        ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState, status);
      }
    }

    kidFrame = iter.Next(); // Get the next child
    // if this was the last child, and it had a colspan>1, add in the cellSpacing for the colspan
    // if the last kid wasn't a colspan, then we still have the colspan of the last real cell
    if ((nsnull==kidFrame) && (cellColSpan>1))
      aReflowState.x += cellSpacingX;
  }

  SetMaxChildHeight(aReflowState.maxCellHeight,maxCellTopMargin, maxCellBottomMargin);  // remember height of tallest child who doesn't have a row span

  // Return our desired size. Note that our desired width is just whatever width
  // we were given by the row group frame
  aDesiredSize.width = aReflowState.x;
  aDesiredSize.height = aReflowState.maxCellVertSpace;  
#ifdef DEBUG
  nscoord overAllocated = aDesiredSize.width - aReflowState.reflowState.availableWidth;
  if (overAllocated > 0) {
    float p2t;
    aPresContext.GetScaledPixelsToTwips(&p2t);
    if (overAllocated > p2t) {
      printf("row over allocated by %d twips", overAllocated);
    }
  }
#endif

  if (gsDebug)
    printf("Row: RR -- row %p width = %d from maxSize %d\n", 
           this, aDesiredSize.width, aReflowState.reflowState.availableWidth);
  
  return rv;
}

/**
 * Called for the initial reflow. Creates each table cell frame, and
 * reflows it to gets its minimum and maximum sizes
 */
NS_METHOD
nsTableRowFrame::InitialReflow(nsIPresContext&      aPresContext,
                               nsHTMLReflowMetrics& aDesiredSize,
                               RowReflowState&      aReflowState,
                               nsReflowStatus&      aStatus,
                               nsTableCellFrame *   aStartFrame,
                               PRBool               aDoSiblings)
{
  // Place our children, one at a time, until we are out of children
  nsSize    kidMaxElementSize(0,0);
  nscoord   maxTopMargin = 0;
  nscoord   maxBottomMargin = 0;
  nscoord   x = 0;
  PRBool    tableLayoutStrategy=NS_STYLE_TABLE_LAYOUT_AUTO; 
  nsTableFrame* table = aReflowState.tableFrame;
  nsresult  rv = NS_OK;
  const nsStyleTable* tableStyle;
  table->GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  tableLayoutStrategy = tableStyle->mLayoutStrategy;

  nsIFrame* kidFrame;
  if (nsnull==aStartFrame)
    kidFrame = mFrames.FirstChild();
  else
    kidFrame = aStartFrame;

  for ( ; nsnull != kidFrame; kidFrame->GetNextSibling(&kidFrame)) 
  {
    const nsStyleDisplay *kidDisplay;
    kidFrame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)kidDisplay));
    if (NS_STYLE_DISPLAY_TABLE_CELL == kidDisplay->mDisplay)
    {
      // Get the child's margins
      nsMargin  margin;
      nscoord   topMargin = 0;
      if (aReflowState.tableFrame->GetCellMarginData((nsTableCellFrame *)kidFrame, margin) == NS_OK)
      {
        topMargin = margin.top;
      }
      maxTopMargin = PR_MAX(margin.top, maxTopMargin);
      maxBottomMargin = PR_MAX(margin.bottom, maxBottomMargin);

      // get border padding values
      nsMargin borderPadding;
      const nsStyleSpacing* cellSpacingStyle;
      kidFrame->GetStyleData(eStyleStruct_Spacing , ((const nsStyleStruct *&)cellSpacingStyle));
      cellSpacingStyle->CalcBorderPaddingFor(kidFrame, borderPadding);

      // For the initial reflow always allow the child to be as high as it
      // wants. The default available width is also unconstrained so we can
      // get the child's maximum width
      nsSize  kidAvailSize;
      nsHTMLReflowMetrics kidSize(nsnull);
      if (NS_STYLE_TABLE_LAYOUT_AUTO==tableLayoutStrategy)
      {
        kidAvailSize.SizeTo(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
        kidSize.maxElementSize=&kidMaxElementSize;
      }
      else
      {
        PRInt32 colIndex;
        ((nsTableCellFrame *)kidFrame)->GetColIndex(colIndex);
        kidAvailSize.SizeTo(table->GetColumnWidth(colIndex), NS_UNCONSTRAINEDSIZE); 
      }

      nsHTMLReflowState kidReflowState(aPresContext,
                                       aReflowState.reflowState,
                                       kidFrame, kidAvailSize,
                                       eReflowReason_Initial);

      if (gsDebug) printf ("%p InitR: avail=%d\n", this, kidAvailSize.width);
      rv = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState, aStatus);
      if (gsDebug) 
        printf ("TR %p for cell %p Initial Reflow: desired=%d, MES=%d\n", 
               this, kidFrame, kidSize.width, kidMaxElementSize.width);

      // the following signals bugs in the content frames.  
      if (kidMaxElementSize.width > kidSize.width) {
        printf("WARNING - table cell content max element width %d greater than desired width %d\n",
          kidMaxElementSize.width, kidSize.width);
        kidSize.width = kidMaxElementSize.width;
      }
      if (kidMaxElementSize.height > kidSize.height) {
        printf("Warning - table cell content max element height %d greater than desired height %d\n",
          kidMaxElementSize.height, kidSize.height);
        kidSize.height = kidMaxElementSize.height;
      }

      ((nsTableCellFrame *)kidFrame)->SetPass1DesiredSize(kidSize);
      ((nsTableCellFrame *)kidFrame)->SetPass1MaxElementSize(kidMaxElementSize);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "unexpected child reflow status");

      if (gsDebug) 
      {
        printf("reflow of cell returned result = %s with desired=%d,%d, min = %d,%d\n",
               NS_FRAME_IS_COMPLETE(aStatus)?"complete":"NOT complete", 
               kidSize.width, kidSize.height, 
               kidMaxElementSize.width, kidMaxElementSize.height);
      }

      // Place the child
      x += margin.left;
      nsRect kidRect(x, topMargin, kidSize.width, kidSize.height);
      PlaceChild(aPresContext, aReflowState, kidFrame, kidRect, aDesiredSize.maxElementSize,
                 &kidMaxElementSize);
      x += kidSize.width + margin.right;
    }
    else
    {// it's an unknown frame type, give it a generic reflow and ignore the results
      nsHTMLReflowState kidReflowState(aPresContext, aReflowState.reflowState,
                                       kidFrame, nsSize(0,0), eReflowReason_Initial);
      nsHTMLReflowMetrics desiredSize(nsnull);
      if (PR_TRUE==gsDebug) printf("\nTIF : Reflow Pass 2 of unknown frame %p of type %d with reason=%d\n", 
                                     kidFrame, kidDisplay->mDisplay, eReflowReason_Initial);
      ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState, aStatus);
    }
    if (PR_FALSE==aDoSiblings)
      break;
  }

  SetMaxChildHeight(aReflowState.maxCellHeight, maxTopMargin, maxBottomMargin);  // remember height of tallest child who doesn't have a row span

  // Return our desired size
  aDesiredSize.width = x;
  aDesiredSize.height = aReflowState.maxCellVertSpace;   

  return rv;
}

// Recover the reflow state to what it should be if aKidFrame is about
// to be reflowed
//
// The things in the RowReflowState object we need to restore are:
// - maxCellHeight
// - maxVertCellSpace
// - x
NS_METHOD nsTableRowFrame::RecoverState(nsIPresContext& aPresContext,
                                        RowReflowState& aReflowState,
                                        nsIFrame*       aKidFrame,
                                        nscoord&        aMaxCellTopMargin,
                                        nscoord&        aMaxCellBottomMargin)
{
  aMaxCellTopMargin = aMaxCellBottomMargin = 0;

  // Walk the list of children looking for aKidFrame. While we're at
  // it get the maxCellHeight and maxVertCellSpace for all the
  // frames except aKidFrame
  for (nsIFrame* frame = mFrames.FirstChild(); nsnull != frame;) 
  {
    const nsStyleDisplay *kidDisplay;
    frame->GetStyleData(eStyleStruct_Display, ((const nsStyleStruct *&)kidDisplay));
    if (NS_STYLE_DISPLAY_TABLE_CELL == kidDisplay->mDisplay)
    {
      if (frame != aKidFrame) {
        // Update the max top and bottom margins
        nsMargin       kidMargin;
        aReflowState.tableFrame->GetCellMarginData((nsTableCellFrame *)frame, kidMargin);
        if (kidMargin.top > aMaxCellTopMargin)
          aMaxCellTopMargin = kidMargin.top;
        if (kidMargin.bottom > aMaxCellBottomMargin)
          aMaxCellBottomMargin = kidMargin.bottom;

        PRInt32 rowSpan = aReflowState.tableFrame->GetEffectiveRowSpan(mRowIndex, ((nsTableCellFrame *)frame));
        if (mMinRowSpan == rowSpan) {
          // Get the cell's desired height the last time it was reflowed
          nsSize  desiredSize = ((nsTableCellFrame *)frame)->GetDesiredSize();

          // See if it has a specified height that overrides the desired size
          nscoord specifiedHeight = 0;
          nsIStyleContextPtr kidSC;
          frame->GetStyleContext(kidSC.AssignPtr());
          const nsStylePosition* kidPosition = (const nsStylePosition*)
            kidSC->GetStyleData(eStyleStruct_Position);
          switch (kidPosition->mHeight.GetUnit()) {
          case eStyleUnit_Coord:
            specifiedHeight = kidPosition->mHeight.GetCoordValue();
            break;
      
          case eStyleUnit_Inherit:
            // XXX for now, do nothing
          default:
          case eStyleUnit_Auto:
            break;
          }
          if (specifiedHeight > desiredSize.height)
            desiredSize.height = specifiedHeight;
        
          // Update maxCellHeight
          if (desiredSize.height > aReflowState.maxCellHeight) {
            aReflowState.maxCellHeight = desiredSize.height;
          }
  
          // Update maxCellVertHeight
          nsMargin margin;
    
          if (aReflowState.tableFrame->GetCellMarginData((nsTableCellFrame *)frame, margin) == NS_OK)
          {
            nscoord height = desiredSize.height + margin.top + margin.bottom;
            if (height > aReflowState.maxCellVertSpace) {
              aReflowState.maxCellVertSpace = height;
            }
          }
        }

        // XXX We also need to recover the max element size if requested by the
        // caller...
        //
        // We should be using GetReflowMetrics() to get information from the
        // table cell, and that will include the max element size...
      }
    }

    frame->GetNextSibling(&frame);
  }

  // Update the running x-offset based on the frame's current x-origin
  nsPoint origin;
  aKidFrame->GetOrigin(origin);
  aReflowState.x = origin.x;

  return NS_OK;
}



NS_METHOD nsTableRowFrame::IncrementalReflow(nsIPresContext&       aPresContext,
                                             nsHTMLReflowMetrics&  aDesiredSize,
                                             RowReflowState&       aReflowState,
                                             nsReflowStatus&       aStatus)
{
  if (PR_TRUE==gsDebugIR) printf("\nTRF IR: IncrementalReflow\n");
  nsresult  rv = NS_OK;

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

NS_METHOD nsTableRowFrame::IR_TargetIsMe(nsIPresContext&      aPresContext,
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
  if (PR_TRUE==gsDebugIR) printf("TRF IR: IncrementalReflow_TargetIsMe with type=%d\n", type);
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
    if (PR_TRUE==gsDebugIR) printf("TRF IR: unexpected reflow command not implemented.\n");
    break;
  }

  return rv;
}

NS_METHOD nsTableRowFrame::IR_StyleChanged(nsIPresContext&      aPresContext,
                                           nsHTMLReflowMetrics& aDesiredSize,
                                           RowReflowState&      aReflowState,
                                           nsReflowStatus&      aStatus)
{
  if (PR_TRUE==gsDebugIR) printf("Row: IR_StyleChanged for frame %p\n", this);
  nsresult rv = NS_OK;
  // we presume that all the easy optimizations were done in the nsHTMLStyleSheet before we were called here
  // XXX: we can optimize this when we know which style attribute changed
  aReflowState.tableFrame->InvalidateFirstPassCache();
  return rv;
}

NS_METHOD nsTableRowFrame::IR_TargetIsChild(nsIPresContext&      aPresContext,
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
    nscoord maxCellTopMargin, maxCellBottomMargin;
    RecoverState(aPresContext, aReflowState, aNextFrame, maxCellTopMargin, maxCellBottomMargin);

    // Get the frame's margins
    nsMargin  kidMargin;
    aReflowState.tableFrame->GetCellMarginData((nsTableCellFrame *)aNextFrame, kidMargin);
    if (kidMargin.top > maxCellTopMargin)
      maxCellTopMargin = kidMargin.top;
    if (kidMargin.bottom > maxCellBottomMargin)
      maxCellBottomMargin = kidMargin.bottom;

    // At this point, we know the column widths. Get the available width
    // from the known column widths
    PRInt32 cellColIndex;
    ((nsTableCellFrame *)aNextFrame)->GetColIndex(cellColIndex);
    PRInt32 cellColSpan = aReflowState.tableFrame->GetEffectiveColSpan(cellColIndex,
                                                                       ((nsTableCellFrame *)aNextFrame));
    nscoord availWidth = 0;
    for (PRInt32 numColSpan = 0; numColSpan < cellColSpan; numColSpan++)
    {
      availWidth += aReflowState.tableFrame->GetColumnWidth(cellColIndex+numColSpan);
      if (0<numColSpan)
      {
        availWidth += kidMargin.right;
        if (0!=cellColIndex)
          availWidth += kidMargin.left;
      }
    }

    // Always let the cell be as high as it wants. We ignore the height that's
    // passed in and always place the entire row. Let the row group decide
    // whether we fit or wehther the entire row is pushed
    nsSize  kidAvailSize(availWidth, NS_UNCONSTRAINEDSIZE);

    // Pass along the reflow command
    nsSize          kidMaxElementSize;
    nsHTMLReflowMetrics desiredSize(&kidMaxElementSize);
    nsHTMLReflowState kidReflowState(aPresContext,
                                     aReflowState.reflowState,
                                     aNextFrame, kidAvailSize);

    // Unfortunately we need to reflow the child several times.
    // The first time is for the incremental reflow command. We can't pass in
    // a max width of NS_UNCONSTRAINEDSIZE, because the max width must match
    // the width of the previous reflow...
    rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState, aStatus);

    // Now do the regular pass 1 reflow and gather the max width and max element
    // size.
    // XXX It would be nice if we could skip this step and the next step if the
    // column width isn't dependent on the max cell width...
    kidReflowState.reason = eReflowReason_Initial;
    kidReflowState.reflowCommand = nsnull;
    kidReflowState.availableWidth = NS_UNCONSTRAINEDSIZE;
    rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState, aStatus);
    if (gsDebug) 
        printf ("TR %p for cell %p Incremental Reflow: desired=%d, MES=%d\n", 
               this, aNextFrame, desiredSize.width, kidMaxElementSize.width);
    // Update the cell layout data.
    //XXX: this is a hack, shouldn't it be the case that a min size is 
    //     never larger than a desired size?
    if (kidMaxElementSize.width>desiredSize.width)
      desiredSize.width = kidMaxElementSize.width;
    if (kidMaxElementSize.height>desiredSize.height)
      desiredSize.height = kidMaxElementSize.height;
    ((nsTableCellFrame *)aNextFrame)->SetPass1DesiredSize(desiredSize);
    ((nsTableCellFrame *)aNextFrame)->SetPass1MaxElementSize(kidMaxElementSize);
    // Now reflow the cell again this time constraining the width
    // XXX Ignore for now the possibility that the column width has changed...
    kidReflowState.availableWidth = availWidth;
    rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState, aStatus);
  
    // Place the child after taking into account it's margin and attributes
    // XXX We need to ask the table (or the table layout strategy) if the column
    // widths have changed. If so, we just bail and return a status indicating
    // what happened and let the table reflow all the table cells...
    nscoord specifiedHeight = 0;
    nscoord cellHeight = desiredSize.height;
    nsIStyleContextPtr kidSC;
    aNextFrame->GetStyleContext(kidSC.AssignPtr());
    const nsStylePosition* kidPosition = (const nsStylePosition*)
      kidSC->GetStyleData(eStyleStruct_Position);
    switch (kidPosition->mHeight.GetUnit()) {
    case eStyleUnit_Coord:
      specifiedHeight = kidPosition->mHeight.GetCoordValue();
      break;

    case eStyleUnit_Inherit:
      // XXX for now, do nothing
    default:
    case eStyleUnit_Auto:
      break;
    }
    if (specifiedHeight>cellHeight)
      cellHeight = specifiedHeight;

    nscoord cellWidth = desiredSize.width;
    // begin special Nav4 compatibility code
    if (0==cellWidth)
    {
      cellWidth = aReflowState.tableFrame->GetColumnWidth(cellColIndex);
    }
    // end special Nav4 compatibility code

    // Now place the child
    nsRect kidRect (aReflowState.x, kidMargin.top, cellWidth, cellHeight);

    PlaceChild(aPresContext, aReflowState, aNextFrame, kidRect, aDesiredSize.maxElementSize,
               &kidMaxElementSize);

    SetMaxChildHeight(aReflowState.maxCellHeight, maxCellTopMargin, maxCellBottomMargin);

    // Return our desired size. Note that our desired width is just whatever width
    // we were given by the row group frame
    aDesiredSize.width = aReflowState.availSize.width;
    aDesiredSize.height = aReflowState.maxCellVertSpace;   

    if (gsDebug)
      printf("incr -- row %p width = %d MES=%d from maxSize %d\n", 
             this, aDesiredSize.width, 
             aDesiredSize.maxElementSize ? aDesiredSize.maxElementSize->width : -1,
             aReflowState.reflowState.availableWidth);
  }
  else
  { // pass reflow to unknown frame child
    // aDesiredSize does not change
  }
  return rv;
}


/** Layout the entire row.
  * This method stacks cells horizontally according to HTML 4.0 rules.
  */
NS_METHOD
nsTableRowFrame::Reflow(nsIPresContext&          aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  nsresult rv=NS_OK;
  if (gsDebug==PR_TRUE)
    printf("nsTableRowFrame::Reflow - aMaxSize = %d, %d\n",
            aReflowState.availableWidth, aReflowState.availableHeight);

  // Initialize 'out' parameters (aStatus set below, undefined if rv returns an error)
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  // Initialize our internal data
  ResetMaxChildHeight();

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
    if (PR_FALSE==tableFrame->RequiresPass1Layout())
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
    GetMinRowSpan(tableFrame);
    FixMinCellHeight(tableFrame);
    aStatus = NS_FRAME_COMPLETE;
    break;

  case eReflowReason_Resize:
    rv = ResizeReflow(aPresContext, aDesiredSize, state, aStatus);
    break;

  case eReflowReason_Incremental:
    rv = IncrementalReflow(aPresContext, aDesiredSize, state, aStatus);
    break;
  }

  if (gsDebug==PR_TRUE) 
  {
    if (nsnull!=aDesiredSize.maxElementSize)
      printf("%p: Row::RR returning: %s with aDesiredSize=%d,%d, aMES=%d,%d\n",
              this, NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
              aDesiredSize.width, aDesiredSize.height,
              aDesiredSize.maxElementSize->width, aDesiredSize.maxElementSize->height);
    else
      printf("%p: Row::RR returning: %s with aDesiredSize=%d,%d, aMES=NSNULL\n", 
             this, NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
             aDesiredSize.width, aDesiredSize.height);
  }

  return rv;
}

/* we overload this here because rows have children that can span outside of themselves.
 * so the default "get the child rect, see if it contains the event point" action isn't
 * sufficient.  We have to ask the row if it has a child that contains the point.
 */
PRBool nsTableRowFrame::Contains(const nsPoint& aPoint)
{
  PRBool result = PR_FALSE;
  // first, check the row rect and see if the point is in their
  if (mRect.Contains(aPoint)) {
    result = PR_TRUE;
  }
  // if that fails, check the cells, they might span outside the row rect
  else {
    nsIFrame* kid;
    FirstChild(nsnull, &kid);
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
void nsTableRowFrame::ReflowCellFrame(nsIPresContext&          aPresContext,
                                      const nsHTMLReflowState& aReflowState,
                                      nsTableCellFrame*        aCellFrame,
                                      nscoord                  aAvailableHeight,
                                      nsReflowStatus&          aStatus)
{
  // Reflow the cell frame with the specified height. Use the existing width
  nsSize  cellSize;
  aCellFrame->GetSize(cellSize);
  
  nsSize  availSize(cellSize.width, aAvailableHeight);
  nsHTMLReflowState cellReflowState(aPresContext, aReflowState, aCellFrame, availSize,
                                    eReflowReason_Resize);
  nsHTMLReflowMetrics desiredSize(nsnull);

  ReflowChild(aCellFrame, aPresContext, desiredSize, cellReflowState, aStatus);
  aCellFrame->SizeTo(cellSize.width, aAvailableHeight);
  aCellFrame->VerticallyAlignChild();
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
NS_NewTableRowFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTableRowFrame* it = new nsTableRowFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

NS_IMETHODIMP
nsTableRowFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("TableRow", aResult);
}



