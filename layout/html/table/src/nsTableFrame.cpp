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
#include "nsTableFrame.h"
#include "nsTablePart.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsCellMap.h"
#include "nsTableContent.h"
#include "nsTableCell.h"
#include "nsTableCellFrame.h"
#include "nsTableCol.h"
#include "nsTableRowFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsColLayoutData.h"

#include "BasicTableLayoutStrategy.h"

#include "nsIPresContext.h"
#include "nsCSSRendering.h"
#include "nsStyleConsts.h"
#include "nsCellLayoutData.h"
#include "nsVoidArray.h"
#include "prinrval.h"
#include "nsIPtr.h"
#include "nsIView.h"
#include "nsHTMLAtoms.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsDebugCLD = PR_FALSE;
static PRBool gsTiming = PR_FALSE;
static PRBool gsDebugMBP = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
//#ifdef NOISY_STYLE
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsDebugCLD = PR_FALSE;
static const PRBool gsTiming = PR_FALSE;
static const PRBool gsDebugMBP = PR_FALSE;
#endif

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);
NS_DEF_PTR(nsTableContent);
NS_DEF_PTR(nsTableCol);
NS_DEF_PTR(nsTableCell);

/* ----------- InnerTableReflowState ---------- */

struct InnerTableReflowState {

  // The body's style molecule

  // The body's available size (computed from the body's parent)
  nsSize availSize;

  nscoord leftInset, topInset;

  // Margin tracking information
  nscoord prevMaxPosBottomMargin;
  nscoord prevMaxNegBottomMargin;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  // Running y-offset
  nscoord y;

  // Flag for whether we're dealing with the first interior row group
  PRBool firstRowGroup;

  // a list of the footers in this table frame, for quick access when inserting bodies
  nsVoidArray *footerList;

  // cache the total height of the footers for placing body rows
  nscoord footerHeight;

  InnerTableReflowState(nsIPresContext*  aPresContext,
                        const nsSize&    aMaxSize,
                        const nsMargin&  aBorderPadding)
  {
    prevMaxPosBottomMargin = 0;
    prevMaxNegBottomMargin = 0;
    y=0;  // border/padding/margin???

    unconstrainedWidth = PRBool(aMaxSize.width == NS_UNCONSTRAINEDSIZE);
    availSize.width = aMaxSize.width;
    if (!unconstrainedWidth) {
      availSize.width -= aBorderPadding.left + aBorderPadding.right;
    }
    leftInset = aBorderPadding.left;

    unconstrainedHeight = PRBool(aMaxSize.height == NS_UNCONSTRAINEDSIZE);
    availSize.height = aMaxSize.height;
    if (!unconstrainedHeight) {
      availSize.height -= aBorderPadding.top + aBorderPadding.bottom;
    }
    topInset = aBorderPadding.top;

    firstRowGroup = PR_TRUE;
    footerHeight = 0;
    footerList = nsnull;
  }

  ~InnerTableReflowState() {
    if (nsnull!=footerList)
      delete footerList;
  }
};



/* ----------- nsTableFrame ---------- */


nsTableFrame::nsTableFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsContainerFrame(aContent, aParentFrame),
    mColumnLayoutData(nsnull),
    mColumnWidths(nsnull),
    mTableLayoutStrategy(nsnull),
    mFirstPassValid(PR_FALSE),
    mPass(kPASS_UNDEFINED),
    mIsInvariantWidth(PR_FALSE)
{
}

/**
 * Method to delete all owned objects assoicated
 * with the ColumnLayoutObject instance variable
 */
void nsTableFrame::DeleteColumnLayoutData()
{
  if (nsnull!=mColumnLayoutData)
  {
    PRInt32 numCols = mColumnLayoutData->Count();
    for (PRInt32 i = 0; i<numCols; i++)
    {
      nsColLayoutData *colData = (nsColLayoutData *)(mColumnLayoutData->ElementAt(i));
      delete colData;
    }
    delete mColumnLayoutData;
    mColumnLayoutData = nsnull;
  }
}

nsTableFrame::~nsTableFrame()
{
  DeleteColumnLayoutData();
  if (nsnull!=mColumnWidths)
    delete [] mColumnWidths;
  if (nsnull!=mTableLayoutStrategy)
    delete mTableLayoutStrategy;
}



/**
* Lists the column layout data which turns
* around and lists the cell layout data.
* This is for debugging purposes only.
*/
void nsTableFrame::ListColumnLayoutData(FILE* out, PRInt32 aIndent) const
{
  // if this is a continuing frame, there will be no output
  if (nsnull!=mColumnLayoutData)
  {
    fprintf(out,"Column Layout Data \n");
    
    PRInt32 numCols = mColumnLayoutData->Count();
    for (PRInt32 i = 0; i<numCols; i++)
    {
      nsColLayoutData *colData = (nsColLayoutData *)(mColumnLayoutData->ElementAt(i));

      for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);
      fprintf(out,"Column Data [%d] \n",i);
      colData->List(out,aIndent+2);
    }
  }
}


/**
 * Find the CellLayoutData assocated with the TableCell
**/
nsCellLayoutData* nsTableFrame::FindCellLayoutData(nsTableCell* aCell) 
{
  if (aCell != nsnull)
  {
    if (nsnull!=mColumnLayoutData)
    {
      PRInt32 numCols = mColumnLayoutData->Count();
      for (PRInt32 i = 0; i<numCols; i++)
      {
        nsColLayoutData *colLayoutData = (nsColLayoutData *)(mColumnLayoutData->ElementAt(i));
        PRInt32 index = colLayoutData->IndexOf(aCell);
        if (index != -1)
          return colLayoutData->ElementAt(index);
      }
    }
  }
  return nsnull;
}


/**
 * For the TableCell in CellData, find the CellLayoutData assocated
 * and add it to the list
**/
void nsTableFrame::AppendLayoutData(nsVoidArray* aList, nsTableCell* aTableCell)
{

  if (aTableCell != nsnull)
  {
    nsCellLayoutData* layoutData = FindCellLayoutData(aTableCell);
    if (layoutData != nsnull)
      aList->AppendElement((void*)layoutData);
  }
}

void nsTableFrame::RecalcLayoutData()
{

  nsTablePart*  table = (nsTablePart*)mContent;
  nsCellMap*    cellMap = table->GetCellMap();
  PRInt32       colCount = cellMap->GetColCount();
  PRInt32       rowCount = cellMap->GetRowCount();
  PRInt32       row = 0;
  PRInt32       col = 0;

  nsTableCell*  above = nsnull;
  nsTableCell*  below = nsnull;
  nsTableCell*  left = nsnull;
  nsTableCell*  right = nsnull;


  PRInt32       edge = 0;
  nsVoidArray*  boundaryCells[4];

  for (edge = 0; edge < 4; edge++)
    boundaryCells[edge] = new nsVoidArray();


  if (colCount != 0 && rowCount != 0)
  {
    for (row = 0; row < rowCount; row++)
    {
      for (col = 0; col < colCount; col++)
      {
        nsTableCell*  cell = nsnull;
        CellData*     cellData = cellMap->GetCellAt(row,col);
        
        if (cellData)
          cell = cellData->mCell;
        
        if (!cell)
          continue;

        PRInt32       colSpan = cell->GetColSpan();
        PRInt32       rowSpan = cell->GetRowSpan();

        // clear the cells for all for edges
        for (edge = 0; edge < 4; edge++)
          boundaryCells[edge]->Clear();

    
        // Check to see if the cell data represents the top,left
        // corner of a a table cell

        // Check to see if cell the represents a top edge cell
        if (row == 0)
          above = nsnull;
        else
        {
          cellData = cellMap->GetCellAt(row-1,col);
          if (cellData != nsnull)
            above = cellData->mRealCell->mCell;

          // Does the cell data point to the same cell?
          // If it is, then continue
          if (above != nsnull && above == cell)
            continue;
        }           

        // Check to see if cell the represents a left edge cell
        if (col == 0)
          left = nsnull;
        else
        {
          cellData = cellMap->GetCellAt(row,col-1);
          if (cellData != nsnull)
            left = cellData->mRealCell->mCell;

          if (left != nsnull && left == cell)
            continue;
        }

        // If this is the top,left edged cell
        // Then add the cells on the for edges to the array
        
        // Do the top and bottom edge
        PRInt32   r,c;
        PRInt32   r1,r2;
        PRInt32   c1,c2;
        PRInt32   last;
        

        r1 = row - 1;
        r2 = row + rowSpan;
        c = col;
        last = col + colSpan -1;
        
        while (c <= last)
        {
          if (r1 != -1)
          {
            // Add top edge cells
            if (c != col)
            {
              cellData = cellMap->GetCellAt(r1,c);
              if ((cellData != nsnull) && (cellData->mCell != above))
              {
                above = cellData->mCell;
                if (above != nsnull)
                  AppendLayoutData(boundaryCells[NS_SIDE_TOP],above);
              }
            }
            else if (above != nsnull)
            {
              AppendLayoutData(boundaryCells[NS_SIDE_TOP],above);
            }
          }

          if (r2 < rowCount)
          {
            // Add bottom edge cells
            cellData = cellMap->GetCellAt(r2,c);
            if ((cellData != nsnull) && cellData->mCell != below)
            {
              below = cellData->mCell;
              if (below != nsnull)
                AppendLayoutData(boundaryCells[NS_SIDE_BOTTOM],below);
            }
          }
          c++;
        }

        // Do the left and right edge
        c1 = col - 1;
        c2 = col + colSpan;
        r = row ;
        last = row + rowSpan-1;
        
        while (r <= last)
        {
          // Add left edge cells
          if (c1 != -1)
          {
            if (r != row)
            {
              cellData = cellMap->GetCellAt(r,c1);
              if ((cellData != nsnull) && (cellData->mCell != left))
              {
                left = cellData->mCell;
                if (left != nsnull)
                  AppendLayoutData(boundaryCells[NS_SIDE_LEFT],left);
              }
            }
            else if (left != nsnull)
            {
              AppendLayoutData(boundaryCells[NS_SIDE_LEFT],left);
            }
          }

          if (c2 < colCount)
          {
            // Add right edge cells
            cellData = cellMap->GetCellAt(r,c2);
            if ((cellData != nsnull) && (cellData->mCell != right))
            {
              right = cellData->mCell;
              if (right != nsnull)
                AppendLayoutData(boundaryCells[NS_SIDE_RIGHT],right);
            }
          }
          r++;
        }
        
        nsCellLayoutData* cellLayoutData = FindCellLayoutData(cell); 
        if (cellLayoutData != nsnull)
          cellLayoutData->RecalcLayoutData(this,boundaryCells);
      }
    }
  }
  for (edge = 0; edge < 4; edge++)
    delete boundaryCells[edge];

}




/* SEC: TODO: adjust the rect for captions */
NS_METHOD nsTableFrame::Paint(nsIPresContext& aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect& aDirtyRect)
{
  // table paint code is concerned primarily with borders and bg color
  nsStyleDisplay* disp =
    (nsStyleDisplay*)mStyleContext->GetData(eStyleStruct_Display);

  if (disp->mVisible) {
    nsStyleColor* myColor =
      (nsStyleColor*)mStyleContext->GetData(eStyleStruct_Color);
    nsStyleSpacing* mySpacing =
      (nsStyleSpacing*)mStyleContext->GetData(eStyleStruct_Spacing);
    NS_ASSERTION(nsnull != myColor, "null style color");
    NS_ASSERTION(nsnull != mySpacing, "null style spacing");
    if (nsnull!=mySpacing)
    {
      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, mRect, *myColor);
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, mRect, *mySpacing, 0);
    }
  }

  // for debug...
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,128,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
}

PRBool nsTableFrame::NeedsReflow(const nsSize& aMaxSize)
{
  PRBool result = PR_TRUE;
  if (PR_TRUE==mIsInvariantWidth)
    result = PR_FALSE;
  // TODO: other cases...
  return result;
}

// SEC: TODO need to worry about continuing frames prev/next in flow for splitting across pages.
// SEC: TODO need to keep "first pass done" state, update it when ContentChanged notifications come in

/* overview:
  if mFirstPassValid is false, this is our first time through since content was last changed
    set pass to 1
    do pass 1
      get min/max info for all cells in an infinite space
      do column balancing
    set mFirstPassValid to true
    do pass 2
  if pass is 1,
    set pass to 2
    use column widths to ResizeReflow cells
    shrinkWrap Cells in each row to tallest, realigning contents within the cell
*/

/** Layout the entire inner table.
  */
NS_METHOD nsTableFrame::Reflow(nsIPresContext* aPresContext,
                               nsReflowMetrics& aDesiredSize,
                               const nsReflowState& aReflowState,
                               nsReflowStatus& aStatus)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  if (gsDebug==PR_TRUE) 
  {
    printf("-----------------------------------------------------------------\n");
    printf("nsTableFrame::Reflow: maxSize=%d,%d\n",
                               aReflowState.maxSize.width, aReflowState.maxSize.height);
  }

#ifdef NS_DEBUG
  PreReflowCheck();
#endif

  aStatus = NS_FRAME_COMPLETE;

  PRIntervalTime startTime;
  if (gsTiming) {
    startTime = PR_IntervalNow();
  }

  if (PR_TRUE==gsDebug) 
    printf ("*** tableframe reflow\t\t%p\n", this);

  if (PR_TRUE==NeedsReflow(aReflowState.maxSize))
  {
    if (PR_FALSE==IsFirstPassValid())
    { // we treat the table as if we've never seen the layout data before
      mPass = kPASS_FIRST;
      aStatus = ResizeReflowPass1(aPresContext, aDesiredSize,
                                  aReflowState.maxSize, aDesiredSize.maxElementSize);
      // check result
    }
    mPass = kPASS_SECOND;

    // assign column widths, and assign aMaxElementSize->width
    BalanceColumnWidths(aPresContext, aReflowState.maxSize, aDesiredSize.maxElementSize);

    // assign table width
    SetTableWidth(aPresContext);

    aStatus = ResizeReflowPass2(aPresContext, aDesiredSize, aReflowState.maxSize,
                                aDesiredSize.maxElementSize, 0, 0);

    if (gsTiming) {
      PRIntervalTime endTime = PR_IntervalNow();
      printf("Table reflow took %ld ticks for frame %d\n",
             endTime-startTime, this);/* XXX need to use LL_* macros! */
    }

    mPass = kPASS_UNDEFINED;
  }
  else
  {
    // set aDesiredSize and aMaxElementSize
  }

#ifdef NS_DEBUG
  PostReflowCheck(aStatus);
#endif  

  return NS_OK;
}

/** the first of 2 reflow passes
  * lay out the captions and row groups in an infinite space (NS_UNCONSTRAINEDSIZE)
  * cache the results for each caption and cell.
  * if successful, set mFirstPassValid=PR_TRUE, so we know we can skip this step 
  * next time.  mFirstPassValid is set to PR_FALSE when content is changed.
  * NOTE: should never get called on a continuing frame!  All cached pass1 state
  *       is stored in the inner table first-in-flow.
  */
nsReflowStatus nsTableFrame::ResizeReflowPass1(nsIPresContext* aPresContext,
                                               nsReflowMetrics& aDesiredSize,
                                               const nsSize& aMaxSize,
                                               nsSize* aMaxElementSize)
{
  NS_ASSERTION(nsnull!=aPresContext, "bad pres context param");
  NS_ASSERTION(nsnull==mPrevInFlow, "illegal call, cannot call pass 1 on a continuing frame.");

  if (gsDebug==PR_TRUE) printf("nsTableFrame::ResizeReflow Pass1: maxSize=%d,%d\n",
                               aMaxSize.width, aMaxSize.height);
  if (PR_TRUE==gsDebug) printf ("*** tableframe reflow pass1\t\t%d\n", this); 
  nsReflowStatus result = NS_FRAME_COMPLETE;

  mChildCount = 0;
  mFirstContentOffset = mLastContentOffset = 0;

  nsIContent* c = mContent;
  NS_ASSERTION(nsnull != c, "null content");
  nsTablePart* table = (nsTablePart*)c;

  // Ensure the cell map
  table->EnsureCellMap();
  
  nsSize availSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE); // availSize is the space available at any given time in the process
  nsSize maxSize(0, 0);       // maxSize is the size of the largest child so far in the process
  nsSize kidMaxSize(0,0);
  nsSize* pKidMaxSize = (nsnull != aMaxElementSize) ? &kidMaxSize : nsnull;
  nsReflowMetrics kidSize(pKidMaxSize);
  nscoord y = 0;
  nscoord maxAscent = 0;
  nscoord maxDescent = 0;
  PRInt32 kidIndex = 0;
  PRInt32 lastIndex = c->ChildCount();
  PRInt32 contentOffset=0;
  nsIFrame* prevKidFrame = nsnull;/* XXX incremental reflow! */

  // Compute the insets (sum of border and padding)
  nsStyleSpacing* spacing =
    (nsStyleSpacing*)mStyleContext->GetData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);
  nscoord topInset = borderPadding.top;
  nscoord rightInset = borderPadding.right;
  nscoord bottomInset = borderPadding.bottom;
  nscoord leftInset = borderPadding.left;

  /* assumes that Table's children are in the following order:
   *  Captions
   *  ColGroups, in order
   *  THead, in order
   *  TFoot, in order
   *  TBody, in order
   */
  for (;;) {
    nsIContentPtr kid = c->ChildAt(kidIndex);   // kid: REFCNT++
    if (kid.IsNull()) {
      result = NS_FRAME_COMPLETE;
      break;
    }

    mLastContentIsComplete = PR_TRUE;

    const PRInt32 contentType = ((nsTableContent *)(nsIContent*)kid)->GetType();
    if (contentType==nsITableContent::kTableRowGroupType)
    {
      // Resolve style
      nsIStyleContextPtr kidStyleContext =
        aPresContext->ResolveStyleContextFor(kid, this);
      NS_ASSERTION(kidStyleContext.IsNotNull(), "null style context for kid");

      // SEC: TODO:  when content is appended or deleted, be sure to clear out the frame hierarchy!!!!

      // get next frame, creating one if needed
      nsIFrame* kidFrame=nsnull;
      if (nsnull!=prevKidFrame)
        prevKidFrame->GetNextSibling(kidFrame);  // no need to check for an error, just see if it returned null...
      else
        ChildAt(0, kidFrame);

      // if this is the first time, allocate the caption frame
      if (nsnull==kidFrame)
      {
        nsIContentDelegate* kidDel;
        kidDel = kid->GetDelegate(aPresContext);
        nsresult rv = kidDel->CreateFrame(aPresContext, kid,
                                          this, kidStyleContext, kidFrame);
        NS_RELEASE(kidDel);
      }

      nsSize maxKidElementSize;
      nsReflowState kidReflowState(eReflowReason_Resize, availSize);
      result = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState);

      // Place the child since some of it's content fit in us.
      if (gsDebug) {
        if (nsnull != pKidMaxSize) {
          printf ("reflow of row group returned desired=%d,%d, max-element=%d,%d\n",
                  kidSize.width, kidSize.height, pKidMaxSize->width, pKidMaxSize->height);
        } else {
          printf ("reflow of row group returned desired=%d,%d\n", kidSize.width, kidSize.height);
        }
      }
      PRInt32 yCoord = y;
      if (NS_UNCONSTRAINEDSIZE!=yCoord)
        yCoord+= topInset;
      kidFrame->SetRect(nsRect(leftInset, yCoord,
                               kidSize.width, kidSize.height));
      if (NS_UNCONSTRAINEDSIZE==kidSize.height)
        y = NS_UNCONSTRAINEDSIZE;
      else
        y += kidSize.height;
      if (nsnull != pKidMaxSize) {
        if (pKidMaxSize->width > maxSize.width) {
          maxSize.width = pKidMaxSize->width;
        }
        if (pKidMaxSize->height > maxSize.height) {
          maxSize.height = pKidMaxSize->height;
        }
      }

      // Link child frame into the list of children
      if (nsnull != prevKidFrame) {
        prevKidFrame->SetNextSibling(kidFrame);
      } else {
        // Our first child (**blush**)
        mFirstChild = kidFrame;
        SetFirstContentOffset(kidFrame);
        if (gsDebug) printf("INNER: set first content offset to %d\n", GetFirstContentOffset()); //@@@
      }
      prevKidFrame = kidFrame;
      mChildCount++;

      if (NS_FRAME_IS_NOT_COMPLETE(result)) {
        // If the child didn't finish layout then it means that it used
        // up all of our available space (or needs us to split).
        mLastContentIsComplete = PR_FALSE;
        break;
      }
    }
    contentOffset++;
    kidIndex++;
    if (NS_FRAME_IS_NOT_COMPLETE(result)) {
      // If the child didn't finish layout then it means that it used
      // up all of our available space (or needs us to split).
      mLastContentIsComplete = PR_FALSE;
      break;
    }
  }

  // Recalculate Layout Dependencies
  RecalcLayoutData();

  if (nsnull != prevKidFrame) {
    NS_ASSERTION(IsLastChild(prevKidFrame), "unexpected last child");
                                           // can't use SetLastContentOffset here
    mLastContentOffset = contentOffset-1;    // takes into account colGroup frame we're not using
    if (gsDebug) printf("INNER: set last content offset to %d\n", GetLastContentOffset()); //@@@
  }

  mFirstPassValid = PR_TRUE;

  return result;
}

/** the second of 2 reflow passes
  */
nsReflowStatus nsTableFrame::ResizeReflowPass2(nsIPresContext* aPresContext,
                                               nsReflowMetrics& aDesiredSize,
                                               const nsSize& aMaxSize,
                                               nsSize* aMaxElementSize,
                                               PRInt32 aMinCaptionWidth,
                                               PRInt32 mMaxCaptionWidth)
{
  if (PR_TRUE==gsDebug) printf ("***tableframe reflow pass2\t\t%d\n", this);
  if (gsDebug==PR_TRUE)
    printf("nsTableFrame::ResizeReflow Pass2: maxSize=%d,%d\n",
           aMaxSize.width, aMaxSize.height);

  nsReflowStatus result = NS_FRAME_COMPLETE;

  // now that we've computed the column  width information, reflow all children
  nsIContent* c = mContent;
  NS_ASSERTION(nsnull != c, "null kid");
  nsSize kidMaxSize(0,0);

  PRInt32 kidIndex = 0;
  PRInt32 lastIndex = c->ChildCount();
  nsIFrame* prevKidFrame = nsnull;/* XXX incremental reflow! */

#ifdef NS_DEBUG
  //PreReflowCheck();
#endif

  // Initialize out parameter
  if (nsnull != aMaxElementSize) {
    aMaxElementSize->width = 0;
    aMaxElementSize->height = 0;
  }

  PRBool        reflowMappedOK = PR_TRUE;
  nsReflowStatus  status = NS_FRAME_COMPLETE;

  // Check for an overflow list
  MoveOverflowToChildList();

  nsStyleSpacing* mySpacing = (nsStyleSpacing*)
    mStyleContext->GetData(eStyleStruct_Spacing);
  nsMargin myBorderPadding;
  mySpacing->CalcBorderPaddingFor(this, myBorderPadding);

  InnerTableReflowState state(aPresContext, aMaxSize, myBorderPadding);

  // Reflow the existing frames
  if (nsnull != mFirstChild) {
    reflowMappedOK = ReflowMappedChildren(aPresContext, state, aMaxElementSize);
    if (PR_FALSE == reflowMappedOK) {
      status = NS_FRAME_NOT_COMPLETE;
    }
  }

  // Did we successfully reflow our mapped children?
  if (PR_TRUE == reflowMappedOK) {
    // Any space left?
    if (state.availSize.height <= 0) {
      // No space left. Don't try to pull-up children or reflow unmapped
      if (NextChildOffset() < mContent->ChildCount()) {
        status = NS_FRAME_NOT_COMPLETE;
      }
    } else if (NextChildOffset() < mContent->ChildCount()) {
      // Try and pull-up some children from a next-in-flow
      if (PullUpChildren(aPresContext, state, aMaxElementSize)) {
        // If we still have unmapped children then create some new frames
        if (NextChildOffset() < mContent->ChildCount()) {
          status = ReflowUnmappedChildren(aPresContext, state, aMaxElementSize);
        }
      } else {
        // We were unable to pull-up all the existing frames from the
        // next in flow
        status = NS_FRAME_NOT_COMPLETE;
      }
    }
  }

  // Return our size and our status

  if (NS_FRAME_IS_NOT_COMPLETE(status)) {
    // Don't forget to add in the bottom margin from our last child.
    // Only add it in if there's room for it.
    nscoord margin = state.prevMaxPosBottomMargin -
      state.prevMaxNegBottomMargin;
    if (state.availSize.height >= margin) {
      state.y += margin;
    }
  }

  // Return our desired rect
  //NS_ASSERTION(0<state.y, "illegal height after reflow");
  aDesiredSize.width = aMaxSize.width;
  aDesiredSize.height = state.y;

  // shrink wrap rows to height of tallest cell in that row
  ShrinkWrapChildren(aPresContext, aDesiredSize, aMaxElementSize);

  if (gsDebug==PR_TRUE) 
  {
    if (nsnull!=aMaxElementSize)
      printf("Reflow complete, returning aDesiredSize = %d,%d and aMaxElementSize=%d,%d\n",
              aDesiredSize.width, aDesiredSize.height, 
              aMaxElementSize->width, aMaxElementSize->height);
    else
      printf("Reflow complete, returning aDesiredSize = %d,%d and NSNULL aMaxElementSize\n",
              aDesiredSize.width, aDesiredSize.height);
  }

  // SEC: assign our real width and height based on this reflow step and return

  mPass = kPASS_UNDEFINED;  // we're no longer in-process

#ifdef NS_DEBUG
  //PostReflowCheck(status);
#endif

  return status;

}

// Collapse child's top margin with previous bottom margin
nscoord nsTableFrame::GetTopMarginFor(nsIPresContext*      aCX,
                                      InnerTableReflowState& aState,
                                      const nsMargin& aKidMargin)
{
  nscoord margin;
  nscoord maxNegTopMargin = 0;
  nscoord maxPosTopMargin = 0;
  if ((margin = aKidMargin.top) < 0) {
    maxNegTopMargin = -margin;
  } else {
    maxPosTopMargin = margin;
  }

  nscoord maxPos = PR_MAX(aState.prevMaxPosBottomMargin, maxPosTopMargin);
  nscoord maxNeg = PR_MAX(aState.prevMaxNegBottomMargin, maxNegTopMargin);
  margin = maxPos - maxNeg;

  return margin;
}

// Position and size aKidFrame and update our reflow state. The origin of
// aKidRect is relative to the upper-left origin of our frame, and includes
// any left/top margin.
void nsTableFrame::PlaceChild(nsIPresContext*    aPresContext,
                              InnerTableReflowState& aState,
                              nsIFrame*          aKidFrame,
                              const nsRect&      aKidRect,
                              nsSize*            aMaxElementSize,
                              nsSize&            aKidMaxElementSize)
{
  if (PR_TRUE==gsDebug)
    printf ("table: placing row group at %d, %d, %d, %d\n",
           aKidRect.x, aKidRect.y, aKidRect.width, aKidRect.height);
  // Place and size the child
  aKidFrame->SetRect(aKidRect);

  // Adjust the running y-offset
  aState.y += aKidRect.height;

  // If our height is constrained then update the available height
  if (PR_FALSE == aState.unconstrainedHeight) {
    aState.availSize.height -= aKidRect.height;
  }

  // If this is a footer row group, add it to the list of footer row groups
  nsIAtom * tFootTag = NS_NewAtom(nsTablePart::kRowGroupFootTagString);  // tFootTag: REFCNT++
  nsIAtom *kidType=nsnull;
  ((nsTableRowGroupFrame *)aKidFrame)->GetRowGroupType(kidType);
  if (tFootTag==kidType)
  {
    if (nsnull==aState.footerList)
      aState.footerList = new nsVoidArray();
    aState.footerList->AppendElement((void *)aKidFrame);
    aState.footerHeight += aKidRect.height;
  }
  // else if this is a body row group, push down all the footer row groups
  else
  {
    // don't bother unless there are footers to push down
    if (nsnull!=aState.footerList  &&  0!=aState.footerList->Count())
    {
      nsPoint origin;
      aKidFrame->GetOrigin(origin);
      origin.y -= aState.footerHeight;
      aKidFrame->MoveTo(origin.x, origin.y);
      nsIAtom * tBodyTag = NS_NewAtom(nsTablePart::kRowGroupBodyTagString);  // tBodyTag: REFCNT++
      if (tBodyTag==kidType)
      {
        PRInt32 numFooters = aState.footerList->Count();
        for (PRInt32 footerIndex = 0; footerIndex < numFooters; footerIndex++)
        {
          nsTableRowGroupFrame * footer = (nsTableRowGroupFrame *)(aState.footerList->ElementAt(footerIndex));
          NS_ASSERTION(nsnull!=footer, "bad footer list in table inner frame.");
          if (nsnull!=footer)
          {
            footer->GetOrigin(origin);
            origin.y += aKidRect.height;
            footer->MoveTo(origin.x, origin.y);
          }
        }
      }
      NS_RELEASE(tBodyTag);
    }
  }
  NS_RELEASE(tFootTag);

  // Update the maximum element size
  if (PR_TRUE==aState.firstRowGroup)
  {
    aState.firstRowGroup = PR_FALSE;
    if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = aKidMaxElementSize.width;
      aMaxElementSize->height = aKidMaxElementSize.height;
    }
  }
}

/**
 * Reflow the frames we've already created
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  true if we successfully reflowed all the mapped children and false
 *            otherwise, e.g. we pushed children to the next in flow
 */
PRBool nsTableFrame::ReflowMappedChildren( nsIPresContext*      aPresContext,
                                           InnerTableReflowState& aState,
                                           nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": reflow mapped (childCount=%d) [%d,%d,%c]\n",
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableFrame* flow = (nsTableFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  NS_PRECONDITION(nsnull != mFirstChild, "no children");

  PRInt32   childCount = 0;
  nsIFrame* prevKidFrame = nsnull;

  // Remember our original mLastContentIsComplete so that if we end up
  // having to push children, we have the correct value to hand to
  // PushChildren.
  PRBool    originalLastContentIsComplete = mLastContentIsComplete;

  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  PRBool    result = PR_TRUE;

  for (nsIFrame*  kidFrame = mFirstChild; nsnull != kidFrame; ) {
    nsSize            kidAvailSize(aState.availSize);
    nsReflowMetrics   desiredSize(pKidMaxElementSize);
    nsReflowStatus    status;

    // Get top margin for this kid
    nsIContentPtr kid;

    kidFrame->GetContent(kid.AssignRef());
    if (((nsTableContent *)(nsIContent*)kid)->GetType() == nsITableContent::kTableRowGroupType)
    { // skip children that are not row groups
      nsIStyleContextPtr kidSC;

      kidFrame->GetStyleContext(aPresContext, kidSC.AssignRef());
      nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
        kidSC->GetData(eStyleStruct_Spacing);
      nsMargin kidMargin;
      kidSpacing->CalcMarginFor(kidFrame, kidMargin);

      nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidMargin);
      nscoord bottomMargin = kidMargin.bottom;

      // Figure out the amount of available size for the child (subtract
      // off the top margin we are going to apply to it)
      if (PR_FALSE == aState.unconstrainedHeight) {
        kidAvailSize.height -= topMargin;
      }
      // Subtract off for left and right margin
      if (PR_FALSE == aState.unconstrainedWidth) {
        kidAvailSize.width -= kidMargin.left + kidMargin.right;
      }

      // Reflow the child into the available space
      nsReflowState kidReflowState(eReflowReason_Resize, kidAvailSize);
      status = ReflowChild(kidFrame, aPresContext, desiredSize, kidReflowState);

      // Did the child fit?
      if ((kidFrame != mFirstChild) && (desiredSize.height > kidAvailSize.height))
      {
        // The child is too wide to fit in the available space, and it's
        // not our first child

        // Since we are giving the next-in-flow our last child, we
        // give it our original mLastContentIsComplete too (in case we
        // are pushing into an empty next-in-flow)
        PushChildren(kidFrame, prevKidFrame, originalLastContentIsComplete);
        SetLastContentOffset(prevKidFrame);

        result = PR_FALSE;
        break;
      }

      // Place the child after taking into account it's margin
      aState.y += topMargin;
      nsRect kidRect (0, 0, desiredSize.width, desiredSize.height);
      kidRect.x += aState.leftInset + kidMargin.left;
      kidRect.y += aState.topInset + aState.y ;
      PlaceChild(aPresContext, aState, kidFrame, kidRect,
                 aMaxElementSize, kidMaxElementSize);
      if (bottomMargin < 0) {
        aState.prevMaxNegBottomMargin = -bottomMargin;
      } else {
        aState.prevMaxPosBottomMargin = bottomMargin;
      }
      childCount++;

      // Remember where we just were in case we end up pushing children
      prevKidFrame = kidFrame;

      // Update mLastContentIsComplete now that this kid fits
      mLastContentIsComplete = PRBool(NS_FRAME_IS_COMPLETE(status));

      // Special handling for incomplete children
      if (NS_FRAME_IS_NOT_COMPLETE(status)) {
        nsIFrame* kidNextInFlow;
         
        kidFrame->GetNextInFlow(kidNextInFlow);
        PRBool lastContentIsComplete = mLastContentIsComplete;
        if (nsnull == kidNextInFlow) {
          // The child doesn't have a next-in-flow so create a continuing
          // frame. This hooks the child into the flow
          nsIFrame* continuingFrame;
           
          nsIStyleContext* kidSC;
          kidFrame->GetStyleContext(aPresContext, kidSC);
          kidFrame->CreateContinuingFrame(aPresContext, this, kidSC, continuingFrame);
          NS_RELEASE(kidSC);
          NS_ASSERTION(nsnull != continuingFrame, "frame creation failed");

          // Add the continuing frame to the sibling list
          nsIFrame* nextSib;
           
          kidFrame->GetNextSibling(nextSib);
          continuingFrame->SetNextSibling(nextSib);
          kidFrame->SetNextSibling(continuingFrame);
          if (nsnull == nextSib) {
            // Assume that the continuation frame we just created is
            // complete, for now. It will get reflowed by our
            // next-in-flow (we are going to push it now)
            lastContentIsComplete = PR_TRUE;
          }
        }
        // We've used up all of our available space so push the remaining
        // children to the next-in-flow
        nsIFrame* nextSibling;
         
        kidFrame->GetNextSibling(nextSibling);
        if (nsnull != nextSibling) {
          PushChildren(nextSibling, kidFrame, lastContentIsComplete);
          SetLastContentOffset(prevKidFrame);
        }
        result = PR_FALSE;
        break;
      }
    }

    // Get the next child
    kidFrame->GetNextSibling(kidFrame);

    // XXX talk with troy about checking for available space here
  }

  // Update the child count
  mChildCount = childCount;
#ifdef NS_DEBUG
  NS_POSTCONDITION(LengthOf(mFirstChild) == mChildCount, "bad child count");

  nsIFrame* lastChild;
  PRInt32   lastIndexInParent;

  LastChild(lastChild);
  lastChild->GetContentIndex(lastIndexInParent);
  NS_POSTCONDITION(lastIndexInParent == mLastContentOffset, "bad last content offset");
#endif

#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": reflow mapped %sok (childCount=%d) [%d,%d,%c]\n",
         (result ? "" : "NOT "),
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableFrame* flow = (nsTableFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  return result;
}

/**
 * Try and pull-up frames from our next-in-flow
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  true if we successfully pulled-up all the children and false
 *            otherwise, e.g. child didn't fit
 */
PRBool nsTableFrame::PullUpChildren(nsIPresContext*      aPresContext,
                                    InnerTableReflowState& aState,
                                    nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
#ifdef NOISY
  ListTag(stdout);
  printf(": pullup (childCount=%d) [%d,%d,%c]\n",
         mChildCount,
         mFirstContentOffset, mLastContentOffset,
         (mLastContentIsComplete ? 'T' : 'F'));
#ifdef NOISY_FLOW
  {
    nsTableFrame* flow = (nsTableFrame*) mNextInFlow;
    while (flow != 0) {
      printf("  %p: [%d,%d,%c]\n",
             flow, flow->mFirstContentOffset, flow->mLastContentOffset,
             (flow->mLastContentIsComplete ? 'T' : 'F'));
      flow = (nsTableFrame*) flow->mNextInFlow;
    }
  }
#endif
#endif
  nsTableFrame* nextInFlow = (nsTableFrame*)mNextInFlow;
  nsSize         kidMaxElementSize;
  nsSize*        pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
#ifdef NS_DEBUG
  PRInt32        kidIndex = NextChildOffset();
#endif
  nsIFrame*      prevKidFrame;
   
  LastChild(prevKidFrame);

  // This will hold the prevKidFrame's mLastContentIsComplete
  // status. If we have to push the frame that follows prevKidFrame
  // then this will become our mLastContentIsComplete state. Since
  // prevKidFrame is initially our last frame, it's completion status
  // is our mLastContentIsComplete value.
  PRBool        prevLastContentIsComplete = mLastContentIsComplete;

  PRBool        result = PR_TRUE;

  while (nsnull != nextInFlow) {
    nsReflowMetrics kidSize(pKidMaxElementSize);
    nsReflowStatus  status;

    // Get the next child
    nsIFrame* kidFrame = nextInFlow->mFirstChild;

    // Any more child frames?
    if (nsnull == kidFrame) {
      // No. Any frames on its overflow list?
      if (nsnull != nextInFlow->mOverflowList) {
        // Move the overflow list to become the child list
        nextInFlow->AppendChildren(nextInFlow->mOverflowList);
        nextInFlow->mOverflowList = nsnull;
        kidFrame = nextInFlow->mFirstChild;
      } else {
        // We've pulled up all the children, so move to the next-in-flow.
        nextInFlow->GetNextInFlow((nsIFrame*&)nextInFlow);
        continue;
      }
    }

    // See if the child fits in the available space. If it fits or
    // it's splittable then reflow it. The reason we can't just move
    // it is that we still need ascent/descent information
    nsSize            kidFrameSize;
    nsSplittableType  kidIsSplittable;

    kidFrame->GetSize(kidFrameSize);
    kidFrame->IsSplittable(kidIsSplittable);
    if ((kidFrameSize.height > aState.availSize.height) &&
        NS_FRAME_IS_NOT_SPLITTABLE(kidIsSplittable)) {
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }
    nsReflowState kidReflowState(eReflowReason_Resize, aState.availSize);
    status = ReflowChild(kidFrame, aPresContext, kidSize, kidReflowState);

    // Did the child fit?
    if ((kidSize.height > aState.availSize.height) && (nsnull != mFirstChild)) {
      // The child is too wide to fit in the available space, and it's
      // not our first child
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }

    // Advance y by the topMargin between children. Zero out the
    // topMargin in case this frame is continued because
    // continuations do not have a top margin. Update the prev
    // bottom margin state in the body reflow state so that we can
    // apply the bottom margin when we hit the next child (or
    // finish).
    //aState.y += topMargin;
    nsRect kidRect (0, 0, kidSize.width, kidSize.height);
    //kidRect.x += kidMol->margin.left;
    kidRect.y += aState.y;
    PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize, *pKidMaxElementSize);

    // Remove the frame from its current parent
    kidFrame->GetNextSibling(nextInFlow->mFirstChild);
    nextInFlow->mChildCount--;
    // Update the next-in-flows first content offset
    if (nsnull != nextInFlow->mFirstChild) {
      nextInFlow->SetFirstContentOffset(nextInFlow->mFirstChild);
    }

    // Link the frame into our list of children
    kidFrame->SetGeometricParent(this);
    nsIFrame* contentParent;

    kidFrame->GetContentParent(contentParent);
    if (nextInFlow == contentParent) {
      kidFrame->SetContentParent(this);
    }
    if (nsnull == prevKidFrame) {
      mFirstChild = kidFrame;
      SetFirstContentOffset(kidFrame);
    } else {
      prevKidFrame->SetNextSibling(kidFrame);
    }
    kidFrame->SetNextSibling(nsnull);
    mChildCount++;

    // Remember where we just were in case we end up pushing children
    prevKidFrame = kidFrame;
    prevLastContentIsComplete = mLastContentIsComplete;

    // Is the child we just pulled up complete?
    mLastContentIsComplete = PRBool(NS_FRAME_IS_COMPLETE(status));
    if (NS_FRAME_IS_NOT_COMPLETE(status)) {
      // No the child isn't complete
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      if (nsnull == kidNextInFlow) {
        // The child doesn't have a next-in-flow so create a
        // continuing frame. The creation appends it to the flow and
        // prepares it for reflow.
        nsIFrame* continuingFrame;

        nsIStyleContext* kidSC;
        kidFrame->GetStyleContext(aPresContext, kidSC);
        kidFrame->CreateContinuingFrame(aPresContext, this, kidSC, continuingFrame);
        NS_RELEASE(kidSC);
        NS_ASSERTION(nsnull != continuingFrame, "frame creation failed");

        // Add the continuing frame to our sibling list and then push
        // it to the next-in-flow. This ensures the next-in-flow's
        // content offsets and child count are set properly. Note that
        // we can safely assume that the continuation is complete so
        // we pass PR_TRUE into PushChidren in case our next-in-flow
        // was just drained and now needs to know it's
        // mLastContentIsComplete state.
        kidFrame->SetNextSibling(continuingFrame);

        PushChildren(continuingFrame, kidFrame, PR_TRUE);

        // After we push the continuation frame we don't need to fuss
        // with mLastContentIsComplete beause the continuation frame
        // is no longer on *our* list.
      }

      // If the child isn't complete then it means that we've used up
      // all of our available space.
      result = PR_FALSE;
      break;
    }
  }

  // Update our last content offset
  if (nsnull != prevKidFrame) {
    NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
    SetLastContentOffset(prevKidFrame);
  }

  // We need to make sure the first content offset is correct for any empty
  // next-in-flow frames (frames where we pulled up all the child frames)
  nextInFlow = (nsTableFrame*)mNextInFlow;
  if ((nsnull != nextInFlow) && (nsnull == nextInFlow->mFirstChild)) {
    // We have at least one empty frame. Did we succesfully pull up all the
    // child frames?
    if (PR_FALSE == result) {
      // No, so we need to adjust the first content offset of all the empty
      // frames
      AdjustOffsetOfEmptyNextInFlows();
#ifdef NS_DEBUG
    } else {
      // Yes, we successfully pulled up all the child frames which means all
      // the next-in-flows must be empty. Do a sanity check
      while (nsnull != nextInFlow) {
        NS_ASSERTION(nsnull == nextInFlow->mFirstChild, "non-empty next-in-flow");
        nextInFlow->GetNextInFlow((nsIFrame*&)nextInFlow);
      }
#endif
    }
  }

#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  return result;
}

/**
 * Create new frames for content we haven't yet mapped
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  frComplete if all content has been mapped and frNotComplete
 *            if we should be continued
 */
nsReflowStatus
nsTableFrame::ReflowUnmappedChildren(nsIPresContext*      aPresContext,
                                     InnerTableReflowState& aState,
                                     nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  nsIFrame*    kidPrevInFlow = nsnull;
  nsReflowStatus result = NS_FRAME_NOT_COMPLETE;

  // If we have no children and we have a prev-in-flow then we need to pick
  // up where it left off. If we have children, e.g. we're being resized, then
  // our content offset should already be set correctly...
  if ((nsnull == mFirstChild) && (nsnull != mPrevInFlow)) {
    nsTableFrame* prev = (nsTableFrame*)mPrevInFlow;
    NS_ASSERTION(prev->mLastContentOffset >= prev->mFirstContentOffset, "bad prevInFlow");

    mFirstContentOffset = prev->NextChildOffset();
    if (!prev->mLastContentIsComplete) {
      // Our prev-in-flow's last child is not complete
      prev->LastChild(kidPrevInFlow);
    }
  }
  mLastContentIsComplete = PR_TRUE;

  // Place our children, one at a time until we are out of children
  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  PRInt32   kidIndex = NextChildOffset();
  nsIFrame* prevKidFrame;

  LastChild(prevKidFrame);
  for (;;) {
    // Get the next content object
    nsIContentPtr kid = mContent->ChildAt(kidIndex);
    if (kid.IsNull()) {
      result = NS_FRAME_COMPLETE;
      break;
    }

    // Make sure we still have room left
    if (aState.availSize.height <= 0) {
      // Note: return status was set to frNotComplete above...
      break;
    }

    // Resolve style for the child
    nsIStyleContextPtr kidStyleContext =
      aPresContext->ResolveStyleContextFor(kid, this);

    // Figure out how we should treat the child
    nsIFrame*        kidFrame;

    // Create a child frame
    if (nsnull == kidPrevInFlow) {
      nsIContentDelegate* kidDel = nsnull;
      kidDel = kid->GetDelegate(aPresContext);
      nsresult rv = kidDel->CreateFrame(aPresContext, kid, this,
                                        kidStyleContext, kidFrame);
      NS_RELEASE(kidDel);
    } else {
      kidPrevInFlow->CreateContinuingFrame(aPresContext, this, kidStyleContext,
                                           kidFrame);
    }

    // Try to reflow the child into the available space. It might not
    // fit or might need continuing.
    nsReflowMetrics kidSize(pKidMaxElementSize);
    nsReflowState   kidReflowState(eReflowReason_Initial, aState.availSize);
    nsReflowStatus status = ReflowChild(kidFrame,aPresContext, kidSize, kidReflowState);

    // Did the child fit?
    if ((kidSize.height > aState.availSize.height) && (nsnull != mFirstChild)) {
      // The child is too wide to fit in the available space, and it's
      // not our first child. Add the frame to our overflow list
      NS_ASSERTION(nsnull == mOverflowList, "bad overflow list");
      mOverflowList = kidFrame;
      prevKidFrame->SetNextSibling(nsnull);
      break;
    }

    // Advance y by the topMargin between children. Zero out the
    // topMargin in case this frame is continued because
    // continuations do not have a top margin. Update the prev
    // bottom margin state in the body reflow state so that we can
    // apply the bottom margin when we hit the next child (or
    // finish).
    //aState.y += topMargin;
    nsRect kidRect (0, 0, kidSize.width, kidSize.height);
    kidRect.y += aState.y;
    PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize, *pKidMaxElementSize);

    // Link child frame into the list of children
    if (nsnull != prevKidFrame) {
      prevKidFrame->SetNextSibling(kidFrame);
    } else {
      mFirstChild = kidFrame;  // our first child
      SetFirstContentOffset(kidFrame);
    }
    prevKidFrame = kidFrame;
    mChildCount++;
    kidIndex++;

    // Did the child complete?
    if (NS_FRAME_IS_NOT_COMPLETE(status)) {
      // If the child isn't complete then it means that we've used up
      // all of our available space
      mLastContentIsComplete = PR_FALSE;
      break;
    }
    kidPrevInFlow = nsnull;
  }

  // Update the content mapping
  NS_ASSERTION(IsLastChild(prevKidFrame), "bad last child");
  SetLastContentOffset(prevKidFrame);
#ifdef NS_DEBUG
  PRInt32 len = LengthOf(mFirstChild);
  NS_ASSERTION(len == mChildCount, "bad child count");
#endif
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  return result;
}


/**
  Now I've got all the cells laid out in an infinite space.
  For each column, use the min size for each cell in that column
  along with the attributes of the table, column group, and column
  to assign widths to each column.
  */
// use the cell map to determine which cell is in which column.
void nsTableFrame::BalanceColumnWidths(nsIPresContext* aPresContext, 
                                       const nsSize& aMaxSize, 
                                       nsSize* aMaxElementSize)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");

  if (gsDebug)
    printf ("BalanceColumnWidths...\n");

  nsVoidArray *columnLayoutData = GetColumnLayoutData();
  PRInt32 numCols = columnLayoutData->Count();
  if (nsnull==mColumnWidths)
  {
    mColumnWidths = new PRInt32[numCols];
    // SEC should be a memset
    for (PRInt32 i = 0; i<numCols; i++)
      mColumnWidths[i] = 0;
  }

  // need to track min and max table widths
  PRInt32 minTableWidth = 0;
  PRInt32 maxTableWidth = 0;
  PRInt32 totalFixedWidth = 0;

  nsStyleSpacing* spacing =
    (nsStyleSpacing*)mStyleContext->GetData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);

  // need to figure out the overall table width constraint
  // default case, get 100% of available space
  PRInt32 maxWidth;
  nsStylePosition* position =
    (nsStylePosition*)mStyleContext->GetData(eStyleStruct_Position);
  switch (position->mWidth.GetUnit()) {
  case eStyleUnit_Coord:
    maxWidth = position->mWidth.GetCoordValue();
    break;
  case eStyleUnit_Percent:
  case eStyleUnit_Proportional:
    // XXX for now these fall through

  default:
  case eStyleUnit_Auto:
  case eStyleUnit_Inherit:
    maxWidth = aMaxSize.width;
    break;
  }

  // now, if maxWidth is not NS_UNCONSTRAINED, subtract out my border
  // and padding
  if (NS_UNCONSTRAINEDSIZE!=maxWidth)
  {
    maxWidth -= borderPadding.left + borderPadding.right;
    if (0>maxWidth)  // nonsense style specification
      maxWidth = 0;
  }

  if (gsDebug) printf ("  maxWidth=%d from aMaxSize=%d,%d\n", maxWidth, aMaxSize.width, aMaxSize.height);

  // based on the compatibility mode, create a table layout strategy
  if (nsnull==mTableLayoutStrategy)
  { // TODO:  build a different strategy based on the compatibility mode
    mTableLayoutStrategy = new BasicTableLayoutStrategy(this);
  }
  mTableLayoutStrategy->BalanceColumnWidths(aPresContext, mStyleContext,
                                            maxWidth, numCols,
                                            totalFixedWidth, minTableWidth, maxTableWidth,
                                            aMaxElementSize);

}

/**
  sum the width of each column
  add in table insets
  set rect
  */
void nsTableFrame::SetTableWidth(nsIPresContext* aPresContext)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");

  if (gsDebug==PR_TRUE) printf ("SetTableWidth...");
  PRInt32 tableWidth = 0;
  nsVoidArray *columnLayoutData = GetColumnLayoutData();
  PRInt32 numCols = columnLayoutData->Count();
  for (PRInt32 i = 0; i<numCols; i++)
  {
    tableWidth += mColumnWidths[i];
    if (gsDebug==PR_TRUE) printf (" += %d ", mColumnWidths[i]);
  }

  // Compute the insets (sum of border and padding)
  nsStyleSpacing* spacing =
    (nsStyleSpacing*)mStyleContext->GetData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);

  nscoord rightInset = borderPadding.right;
  nscoord leftInset = borderPadding.left;
  tableWidth += (leftInset + rightInset);
  nsRect tableSize = mRect;
  tableSize.width = tableWidth;
  if (gsDebug==PR_TRUE)
  {
    printf ("setting table rect to %d, %d after adding insets %d, %d\n", 
            tableSize.width, tableSize.height, rightInset, leftInset);
  }
  SetRect(tableSize);
}

/**
  */
void nsTableFrame::ShrinkWrapChildren(nsIPresContext* aPresContext, 
                                      nsReflowMetrics& aDesiredSize,
                                      nsSize* aMaxElementSize)
{
#ifdef NS_DEBUG
  PRBool gsDebugWas = gsDebug;
  //gsDebug = PR_TRUE;  // turn on debug in this method
#endif
  // iterate children, tell all row groups to ShrinkWrap
  PRBool atLeastOneRowSpanningCell = PR_FALSE;

  PRInt32 rowIndex;
  PRInt32 tableHeight = 0;

  nsStyleSpacing* spacing = (nsStyleSpacing*)
    mStyleContext->GetData(eStyleStruct_Spacing);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);

  tableHeight += borderPadding.top + borderPadding.bottom;

  PRInt32 childCount = mChildCount;
  nsIFrame * kidFrame;
  for (PRInt32 i = 0; i < childCount; i++)
  {
    PRInt32 childHeight=0;
    // for every child that is a rowFrame, set the row frame height  = sum of row heights

    if (0==i)
      ChildAt(i, kidFrame); // frames are not ref counted
    else
      kidFrame->GetNextSibling(kidFrame);
    NS_ASSERTION(nsnull != kidFrame, "bad kid frame");
    nsTableContentPtr kid;
     
    kidFrame->GetContent((nsIContent*&)(kid.AssignRef()));  // kid: REFCNT++
    NS_ASSERTION(kid.IsNotNull(), "bad kid");

    nscoord topInnerMargin = 0;
    nscoord bottomInnerMargin = 0;

    if (kid->GetType() == nsITableContent::kTableRowGroupType)
    {
      /* Step 1:  set the row height to the height of the tallest cell,
       *          and resize all cells in that row to that height (except cells with rowspan>1)
       */
      PRInt32 rowGroupHeight = 0;
      nsTableRowGroupFrame * rowGroupFrame = (nsTableRowGroupFrame *)kidFrame;
      PRInt32 numRows;
      rowGroupFrame->ChildCount(numRows);
      PRInt32 *rowHeights = new PRInt32[numRows];
      if (gsDebug==PR_TRUE) printf("Height Step 1...\n");
      for (rowIndex = 0; rowIndex < numRows; rowIndex++)
      {
        // get the height of the tallest cell in the row (excluding cells that span rows)
        nsTableRowFrame *rowFrame;
         
        rowGroupFrame->ChildAt(rowIndex, (nsIFrame*&)rowFrame);
        NS_ASSERTION(nsnull != rowFrame, "bad row frame");

        nscoord maxCellHeight       = rowFrame->GetTallestChild();
        nscoord maxCellTopMargin    = rowFrame->GetChildMaxTopMargin();
        nscoord maxCellBottomMargin = rowFrame->GetChildMaxBottomMargin();
        nscoord maxRowHeight = maxCellHeight + maxCellTopMargin + maxCellBottomMargin;

        rowHeights[rowIndex] = maxRowHeight;
    
        if (rowIndex == 0)
          topInnerMargin = maxCellTopMargin;
        if (rowIndex+1 == numRows)
          bottomInnerMargin = maxCellBottomMargin;


        nsSize  rowFrameSize;

        rowFrame->GetSize(rowFrameSize);
        rowFrame->SizeTo(rowFrameSize.width, maxRowHeight);
        rowGroupHeight += maxRowHeight;
        // resize all the cells based on the rowHeight
        PRInt32 numCells;

        rowFrame->ChildCount(numCells);
        for (PRInt32 cellIndex = 0; cellIndex < numCells; cellIndex++)
        {
          nsTableCellFrame *cellFrame;

          rowFrame->ChildAt(cellIndex, (nsIFrame*&)cellFrame);
          PRInt32 rowSpan = cellFrame->GetRowSpan();
          if (1==rowSpan)
          {
            if (gsDebug==PR_TRUE) printf("  setting cell[%d,%d] height to %d\n", rowIndex, cellIndex, rowHeights[rowIndex]);

            nsSize  cellFrameSize;

            cellFrame->GetSize(cellFrameSize);
            cellFrame->SizeTo(cellFrameSize.width, maxCellHeight);
            // Realign cell content based on new height
            cellFrame->VerticallyAlignChild(aPresContext);
          }
          else
          {
            if (gsDebug==PR_TRUE) printf("  skipping cell[%d,%d]\n", rowIndex, cellIndex);
            atLeastOneRowSpanningCell = PR_TRUE;
          }
        }
      }

      /* Step 2:  now account for cells that span rows.
       *          a spanning cell's height is the sum of the heights of the rows it spans,
       *          or it's own desired height, whichever is greater.
       *          If the cell's desired height is the larger value, resize the rows and contained
       *          cells by an equal percentage of the additional space.
       */
      /* TODO
       * 1. optimization, if (PR_TRUE==atLeastOneRowSpanningCell) ... otherwise skip this step entirely
       * 2. find cases where spanning cells effect other spanning cells that began in rows above themselves.
       *    I think in this case, we have to make another pass through step 2.
       *    There should be a "rational" check to terminate that kind of loop after n passes, probably 3 or 4.
       */
      if (gsDebug==PR_TRUE) printf("Height Step 2...\n");
      rowGroupHeight=0;
      nsTableRowFrame *rowFrame=nsnull;
      for (rowIndex = 0; rowIndex < numRows; rowIndex++)
      {
        // get the next row
        if (0==rowIndex)
          rowGroupFrame->ChildAt(rowIndex, (nsIFrame*&)rowFrame);
        else
          rowFrame->GetNextSibling((nsIFrame*&)rowFrame);
        PRInt32 numCells;
        rowFrame->ChildCount(numCells);
        // check this row for a cell with rowspans
        for (PRInt32 cellIndex = 0; cellIndex < numCells; cellIndex++)
        {
          // get the next cell
          nsTableCellFrame *cellFrame;
          rowFrame->ChildAt(cellIndex, (nsIFrame*&)cellFrame);
          PRInt32 rowSpan = cellFrame->GetRowSpan();
          if (1<rowSpan)
          { // found a cell with rowspan > 1, determine it's height
            if (gsDebug==PR_TRUE) printf("  cell[%d,%d] has a rowspan = %d\n", rowIndex, cellIndex, rowSpan);
            
            nscoord heightOfRowsSpanned = 0;
            for (PRInt32 i=0; i<rowSpan; i++)
              heightOfRowsSpanned += rowHeights[i+rowIndex];
            
            heightOfRowsSpanned -= topInnerMargin + bottomInnerMargin;

            /* if the cell height fits in the rows, expand the spanning cell's height and slap it in */
            nsSize  cellFrameSize;
            cellFrame->GetSize(cellFrameSize);
            if (heightOfRowsSpanned>cellFrameSize.height)
            {
              if (gsDebug==PR_TRUE) printf("  cell[%d,%d] fits, setting height to %d\n", rowIndex, cellIndex, heightOfRowsSpanned);
              cellFrame->SizeTo(cellFrameSize.width, heightOfRowsSpanned);
              // Realign cell content based on new height
              cellFrame->VerticallyAlignChild(aPresContext);
            }
            /* otherwise, distribute the excess height to the rows effected, and to the cells in those rows
             * push all subsequent rows down by the total change in height of all the rows above it
             */
            else
            {
              PRInt32 excessHeight = cellFrameSize.height - heightOfRowsSpanned;
              PRInt32 excessHeightPerRow = excessHeight/rowSpan;
              if (gsDebug==PR_TRUE) printf("  cell[%d,%d] does not fit, excessHeight = %d, excessHeightPerRow = %d\n", 
                      rowIndex, cellIndex, excessHeight, excessHeightPerRow);
              // for every row starting at the row with the spanning cell...
              for (i=rowIndex; i<numRows; i++)
              {
                nsTableRowFrame *rowFrameToBeResized;

                rowGroupFrame->ChildAt(i, (nsIFrame*&)rowFrameToBeResized);
                // if the row is within the spanned range, resize the row and it's cells
                if (i<rowIndex+rowSpan)
                {
                  rowHeights[i] += excessHeightPerRow;
                  if (gsDebug==PR_TRUE) printf("    rowHeight[%d] set to %d\n", i, rowHeights[i]);

                  nsSize  rowFrameSize;

                  rowFrameToBeResized->GetSize(rowFrameSize);
                  rowFrameToBeResized->SizeTo(rowFrameSize.width, rowHeights[i]);
                  PRInt32 cellCount;

                  rowFrameToBeResized->ChildCount(cellCount);
                  for (PRInt32 j=0; j<cellCount; j++)
                  {
                    if (i==rowIndex && j==cellIndex)
                    {
                      if (gsDebug==PR_TRUE) printf("      cell[%d, %d] skipping self\n", i, j);
                      continue; // don't do math on myself, only the other cells I effect
                    }
                    nsTableCellFrame *frame;

                    rowFrameToBeResized->ChildAt(j, (nsIFrame*&)frame);
                    if (frame->GetRowSpan()==1)
                    {
                      nsSize  frameSize;

                      frame->GetSize(frameSize);
                      if (gsDebug==PR_TRUE) printf("      cell[%d, %d] set height to %d\n", i, j, frameSize.height+excessHeightPerRow);
                      frame->SizeTo(frameSize.width, frameSize.height+excessHeightPerRow);
                      // Realign cell content based on new height
                      frame->VerticallyAlignChild(aPresContext);
                    }
                  }
                }
                // if we're dealing with a row below the row containing the spanning cell, 
                // push that row down by the amount we've expanded the cell heights by
                if (i>=rowIndex && i!=0)
                {
                  nsRect rowRect;
                   
                  rowFrameToBeResized->GetRect(rowRect);
                  nscoord delta = excessHeightPerRow*(i-rowIndex);
                  if (delta > excessHeight)
                    delta = excessHeight;
                  rowFrameToBeResized->MoveTo(rowRect.x, rowRect.y + delta);
                  if (gsDebug==PR_TRUE) printf("      row %d (%p) moved by %d to y-offset %d\n",  
                                                i, rowFrameToBeResized, delta, rowRect.y + delta);
                }
              }
            }
          }
        }
        rowGroupHeight += rowHeights[rowIndex];
      }
      if (gsDebug==PR_TRUE) printf("row group height set to %d\n", rowGroupHeight);

      nsSize  rowGroupFrameSize;

      rowGroupFrame->GetSize(rowGroupFrameSize);
      rowGroupFrame->SizeTo(rowGroupFrameSize.width, rowGroupHeight);
      tableHeight += rowGroupHeight;
    }
  }
  if (0!=tableHeight)
  {
    if (gsDebug==PR_TRUE) printf("table desired height set to %d\n", tableHeight);
    aDesiredSize.height = tableHeight;
  }
#ifdef NS_DEBUG
  gsDebug = gsDebugWas;
#endif
}

void nsTableFrame::VerticallyAlignChildren(nsIPresContext* aPresContext,
                                          nscoord* aAscents,
                                          nscoord aMaxAscent,
                                          nscoord aMaxHeight)
{
}

nsVoidArray * nsTableFrame::GetColumnLayoutData()
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  NS_ASSERTION(nsnull!=firstInFlow->mColumnLayoutData, "illegal state -- no column layout data");
  return firstInFlow->mColumnLayoutData;
}

/** Associate aData with the cell at (aRow,aCol)
  * @return PR_TRUE if the data was successfully associated with a Cell
  *         PR_FALSE if there was an error, such as aRow or aCol being invalid
  */
PRBool nsTableFrame::SetCellLayoutData(nsCellLayoutData * aData, nsTableCell *aCell)
{
  NS_ASSERTION(nsnull != aData, "bad arg");
  NS_ASSERTION(nsnull != aCell, "bad arg");

  PRBool result = PR_TRUE;

  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  if (this!=firstInFlow)
    result = firstInFlow->SetCellLayoutData(aData, aCell);
  else
  {
    if (kPASS_FIRST==GetReflowPass())
    {
      if (nsnull==mColumnLayoutData)
      {
        mColumnLayoutData = new nsVoidArray();
        NS_ASSERTION(nsnull != mColumnLayoutData, "bad alloc");
        nsTablePart * tablePart = (nsTablePart *)mContent;
        PRInt32 cols = tablePart->GetMaxColumns();
        PRInt32 tableKidCount = tablePart->ChildCount();
        for (PRInt32 i=0; i<tableKidCount; i++)
        {
          nsTableContentPtr tableKid = (nsTableContent *)tablePart->ChildAt(i);
          NS_ASSERTION(tableKid.IsNotNull(), "bad kid");
          const int contentType = tableKid->GetType();
          if (contentType == nsITableContent::kTableColGroupType)
          {
            PRInt32 colsInGroup = tableKid->ChildCount();
            for (PRInt32 j=0; j<colsInGroup; j++)
            {
              nsTableColPtr col = (nsTableCol *)tableKid->ChildAt(j);
              NS_ASSERTION(col.IsNotNull(), "bad content");
              nsColLayoutData *colData = new nsColLayoutData(col);
              mColumnLayoutData->AppendElement((void *)colData);
            }
          }
          // can't have col groups after row groups, so stop if you find a row group
          else if (contentType == nsITableContent::kTableRowGroupType)
          {
            break;
          }
        }
      }

    // create cell layout data objects for the passed in data, one per column spanned
    // for now, divide width equally between spanned columns

      PRInt32 firstColIndex = aCell->GetColIndex();
      nsTableRow *row = aCell->GetRow();            // row: ADDREF++
      PRInt32 rowIndex = row->GetRowIndex();
      NS_RELEASE(row);                              // row: ADDREF--
      PRInt32 colSpan = aCell->GetColSpan();
      nsColLayoutData * colData = (nsColLayoutData *)(mColumnLayoutData->ElementAt(firstColIndex));
      nsVoidArray *col = colData->GetCells();
      if (gsDebugCLD) printf ("     ~ SetCellLayoutData with row = %d, firstCol = %d, colSpan = %d, colData = %ld, col=%ld\n", 
                           rowIndex, firstColIndex, colSpan, colData, col);
      for (PRInt32 i=0; i<colSpan; i++)
      {
        nsSize * cellSize = aData->GetMaxElementSize();
        nsSize partialCellSize(*cellSize);
        partialCellSize.width = (cellSize->width)/colSpan;
        // This method will copy the nsReflowMetrics pointed at by aData->GetDesiredSize()
        nsCellLayoutData * kidLayoutData = new nsCellLayoutData(aData->GetCellFrame(),
                                                                aData->GetDesiredSize(),
                                                                &partialCellSize);
        PRInt32 numCells = col->Count();
        if (gsDebugCLD) printf ("     ~ before setting data, number of cells in this column = %d\n", numCells);
        if ((numCells==0) || numCells<i+rowIndex+1)
        {
          if (gsDebugCLD) printf ("     ~ appending  %d\n", i+rowIndex);
          col->AppendElement((void *)kidLayoutData);
        }
        else
        {
          if (gsDebugCLD) printf ("     ~ replacing  %d + rowIndex = %d\n", i, i+rowIndex);
          nsCellLayoutData* data = (nsCellLayoutData*)col->ElementAt(i+rowIndex);
          col->ReplaceElementAt((void *)kidLayoutData, i+rowIndex);
          if (data != nsnull)
            delete data;
        }
        if (gsDebugCLD) printf ("     ~ after setting data, number of cells in this column = %d\n", col->Count());
      }
    }
    else
      result = PR_FALSE;
  }

  return result;
}

/** Get the layout data associated with the cell at (aRow,aCol)
  * @return nsnull if there was an error, such as aRow or aCol being invalid
  *         otherwise, the data is returned.
  */
nsCellLayoutData * nsTableFrame::GetCellLayoutData(nsTableCell *aCell)
{
  NS_ASSERTION(nsnull != aCell, "bad arg");

  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  nsCellLayoutData *result = nsnull;
  if (this!=firstInFlow)
    result = firstInFlow->GetCellLayoutData(aCell);
  else
  {
    if (nsnull!=mColumnLayoutData)
    {
      PRInt32 firstColIndex = aCell->GetColIndex();
      nsColLayoutData * colData = (nsColLayoutData *)(mColumnLayoutData->ElementAt(firstColIndex));
      nsVoidArray *cells = colData->GetCells();
      PRInt32 count = cells->Count();
      for (PRInt32 i=0; i<count; i++)
      {
        nsCellLayoutData * data = (nsCellLayoutData *)(cells->ElementAt(i));
        nsTableCellPtr cell;
         
        data->GetCellFrame()->GetContent((nsIContent*&)(cell.AssignRef()));  // cell: REFCNT++
        if (cell == aCell)
        {
          result = data;
          break;
        }
      }
    }
  }
  return result;
}



PRInt32 nsTableFrame::GetReflowPass() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return firstInFlow->mPass;
}

void nsTableFrame::SetReflowPass(PRInt32 aReflowPass)
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  firstInFlow->mPass = aReflowPass;
}

PRBool nsTableFrame::IsFirstPassValid() const
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  return firstInFlow->mFirstPassValid;
}

NS_METHOD
nsTableFrame::CreateContinuingFrame(nsIPresContext*  aPresContext,
                                    nsIFrame*        aParent,
                                    nsIStyleContext* aStyleContext,
                                    nsIFrame*&       aContinuingFrame)
{
  nsTableFrame* cf = new nsTableFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aPresContext, aParent, aStyleContext, cf);
  if (PR_TRUE==gsDebug) printf("nsTableFrame::CCF parent = %p, this=%p, cf=%p\n", aParent, this, cf);
  // set my width, because all frames in a table flow are the same width
  // code in nsTableOuterFrame depends on this being set
  cf->SetRect(nsRect(0, 0, mRect.width, 0));
  // add headers and footers to cf
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  nsIFrame * rg = nsnull;
  firstInFlow->ChildAt(0, rg);
  NS_ASSERTION (nsnull!=rg, "previous frame has no children");
  nsIAtom * tHeadTag = NS_NewAtom(nsTablePart::kRowGroupHeadTagString);  // tHeadTag: REFCNT++
  nsIAtom * tFootTag = NS_NewAtom(nsTablePart::kRowGroupFootTagString);  // tFootTag: REFCNT++
  PRInt32 index = 0;
  nsIFrame * bodyRowGroupFromOverflow = mOverflowList;
  nsIFrame * lastSib = nsnull;
  for ( ; nsnull!=rg; index++)
  {
    nsIContent *content = nsnull;
    rg->GetContent(content);                                              // content: REFCNT++
    NS_ASSERTION(nsnull!=content, "bad frame, returned null content.");
    nsIAtom * rgTag = content->GetTag();
    // if we've found a header or a footer, replicate it
    if (tHeadTag==rgTag || tFootTag==rgTag)
    {
      printf("found a head or foot in continuing frame\n");
      // Resolve style for the child
      nsIStyleContext* kidStyleContext =
        aPresContext->ResolveStyleContextFor(content, cf);               // kidStyleContext: REFCNT++
      nsIContentDelegate* kidDel = nsnull;
      kidDel = content->GetDelegate(aPresContext);                       // kidDel: REFCNT++
      nsIFrame* duplicateFrame;
      nsresult rv = kidDel->CreateFrame(aPresContext, content, cf,
                                        kidStyleContext, duplicateFrame);
      NS_RELEASE(kidDel);                                                // kidDel: REFCNT--
      NS_RELEASE(kidStyleContext);                                       // kidStyleContenxt: REFCNT--
      
      if (nsnull==lastSib)
      {
        mOverflowList = duplicateFrame;
      }
      else
      {
        lastSib->SetNextSibling(duplicateFrame);
      }
      duplicateFrame->SetNextSibling(bodyRowGroupFromOverflow);
      lastSib = duplicateFrame;
    }
    NS_RELEASE(content);                                                 // content: REFCNT--
    // get the next row group
    rg->GetNextSibling(rg);
  }
  NS_RELEASE(tFootTag);                                                  // tHeadTag: REFCNT --
  NS_RELEASE(tHeadTag);                                                  // tFootTag: REFCNT --
  aContinuingFrame = cf;
  return NS_OK;
}

PRInt32 nsTableFrame::GetColumnWidth(PRInt32 aColIndex)
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  PRInt32 result = 0;
  if (this!=firstInFlow)
    result = firstInFlow->GetColumnWidth(aColIndex);
  else
  {
    NS_ASSERTION(nsnull!=mColumnWidths, "illegal state");
#ifdef DEBUG
    nsVoidArray *cld = GetColumnLayoutData();
    NS_ASSERTION(nsnull!=cld, "no column layout data");
    PRInt32 numCols = cld->Count();
    NS_ASSERTION (numCols > aColIndex, "bad arg, col index out of bounds");
#endif
    if (nsnull!=mColumnWidths)
     result = mColumnWidths[aColIndex];
  }

  //printf("GET_COL_WIDTH: %p, FIF=%p getting col %d and returning %d\n", this, firstInFlow, aColIndex, result);

  // XXX hack
#if 0
  if (result <= 0) {
    result = 100;
  }
#endif
  return result;
}

void  nsTableFrame::SetColumnWidth(PRInt32 aColIndex, PRInt32 aWidth)
{
  nsTableFrame * firstInFlow = (nsTableFrame *)GetFirstInFlow();
  NS_ASSERTION(nsnull!=firstInFlow, "illegal state -- no first in flow");
  //printf("SET_COL_WIDTH: %p, FIF=%p setting col %d to %d\n", this, firstInFlow, aColIndex, aWidth);
  if (this!=firstInFlow)
    firstInFlow->SetColumnWidth(aColIndex, aWidth);
  else
  {
    NS_ASSERTION(nsnull!=mColumnWidths, "illegal state");
    if (nsnull!=mColumnWidths)
      mColumnWidths[aColIndex] = aWidth;
  }
}




/**
  *
  * Update the border style to map to the HTML border style
  *
  */
void nsTableFrame::MapHTMLBorderStyle(nsStyleSpacing& aSpacingStyle, nscoord aBorderWidth)
{
  nsStyleCoord  width;
  width.SetCoordValue(aBorderWidth);
  aSpacingStyle.mBorder.SetTop(width);
  aSpacingStyle.mBorder.SetLeft(width);
  aSpacingStyle.mBorder.SetBottom(width);
  aSpacingStyle.mBorder.SetRight(width);

  aSpacingStyle.mBorderStyle[NS_SIDE_TOP] = NS_STYLE_BORDER_STYLE_OUTSET; 
  aSpacingStyle.mBorderStyle[NS_SIDE_LEFT] = NS_STYLE_BORDER_STYLE_OUTSET; 
  aSpacingStyle.mBorderStyle[NS_SIDE_BOTTOM] = NS_STYLE_BORDER_STYLE_OUTSET; 
  aSpacingStyle.mBorderStyle[NS_SIDE_RIGHT] = NS_STYLE_BORDER_STYLE_OUTSET; 
  

  nsIStyleContext* styleContext = mStyleContext; 
  nsStyleColor*   colorData = (nsStyleColor*)styleContext->GetData(eStyleStruct_Color);

  // Look until we find a style context with a NON-transparent background color
  while (styleContext)
  {
    if ((colorData->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)!=0)
    {
      nsIStyleContext* temp = styleContext;
      styleContext = styleContext->GetParent();
      if (temp != mStyleContext)
        NS_RELEASE(temp);
      colorData = (nsStyleColor*)styleContext->GetData(eStyleStruct_Color);
    }
    else
    {
      break;
    }
  }

  // Yaahoo, we found a style context which has a background color 
  
  nscolor borderColor = 0xFFC0C0C0;

  if (styleContext != nsnull)
  {
    borderColor = colorData->mBackgroundColor;
    if (styleContext != mStyleContext)
      NS_RELEASE(styleContext);
  }

  // if the border color is white, then shift to grey
  if (borderColor == 0xFFFFFFFF)
    borderColor = 0xFFC0C0C0;

  aSpacingStyle.mBorderColor[NS_SIDE_TOP]     = borderColor;
  aSpacingStyle.mBorderColor[NS_SIDE_LEFT]    = borderColor;
  aSpacingStyle.mBorderColor[NS_SIDE_BOTTOM]  = borderColor;
  aSpacingStyle.mBorderColor[NS_SIDE_RIGHT]   = borderColor;

}



PRBool nsTableFrame::ConvertToPixelValue(nsHTMLValue& aValue, PRInt32 aDefault, PRInt32& aResult)
{
  PRInt32 result = 0;

  if (aValue.GetUnit() == eHTMLUnit_Pixel)
    aResult = aValue.GetPixelValue();
  else if (aValue.GetUnit() == eHTMLUnit_Empty)
    aResult = aDefault;
  else
  {
    NS_ERROR("Unit must be pixel or empty");
    return PR_FALSE;
  }
  return PR_TRUE;
}

void nsTableFrame::MapBorderMarginPadding(nsIPresContext* aPresContext)
{
  // Check to see if the table has either cell padding or 
  // Cell spacing defined for the table. If true, then
  // this setting overrides any specific border, margin or 
  // padding information in the cell. If these attributes
  // are not defined, the the cells attributes are used
  
  nsHTMLValue padding_value;
  nsHTMLValue spacing_value;
  nsHTMLValue border_value;


  nsContentAttr border_result;

  nscoord   padding = 0;
  nscoord   spacing = 0;
  nscoord   border  = 1;

  float     p2t = aPresContext->GetPixelsToTwips();

  nsTablePart*  table = (nsTablePart*)mContent;

  NS_ASSERTION(table,"Table Must not be null");
  if (!table)
    return;

  nsStyleSpacing* spacingData = (nsStyleSpacing*)mStyleContext->GetData(eStyleStruct_Spacing);

  border_result = table->GetAttribute(nsHTMLAtoms::border,border_value);
  if (border_result == eContentAttr_HasValue)
  {
    PRInt32 intValue = 0;

    if (ConvertToPixelValue(border_value,1,intValue))
      border = nscoord(p2t*(float)intValue); 
  }
  MapHTMLBorderStyle(*spacingData,border);
}




// Subclass hook for style post processing
NS_METHOD nsTableFrame::DidSetStyleContext(nsIPresContext* aPresContext)
{
#ifdef NOISY_STYLE
  printf("nsTableFrame::DidSetStyleContext \n");
#endif
  MapBorderMarginPadding(aPresContext);
  return NS_OK;
}

NS_METHOD nsTableFrame::GetCellMarginData(nsIFrame* aKidFrame, nsMargin& aMargin)
{
  nsresult result = NS_ERROR_NOT_INITIALIZED;

  nsIContent* content = nsnull;
  aKidFrame->GetContent(content);

  if (nsnull != content)
  {
    nsTableCell*      cell = (nsTableCell*)content;

    nsCellLayoutData* layoutData = GetCellLayoutData(cell);
    if (layoutData)
      result = layoutData->GetMargin(aMargin);
    NS_RELEASE(content);
  }

  return result;
}

/* ---------- static methods ---------- */

nsresult nsTableFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                nsIContent* aContent,
                                nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}

/* valuable code not yet used anywhere

    // since the table is a specified width, we need to expand the columns to fill the table
    nsVoidArray *columnLayoutData = GetColumnLayoutData();
    PRInt32 numCols = columnLayoutData->Count();
    PRInt32 spaceUsed = 0;
    for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
      spaceUsed += mColumnWidths[colIndex];
    PRInt32 spaceRemaining = spaceUsed - aMaxWidth;
    PRInt32 additionalSpaceAdded = 0;
    if (0<spaceRemaining)
    {
      for (colIndex = 0; colIndex<numCols; colIndex++)
      {
        nsColLayoutData * colData = (nsColLayoutData *)(columnLayoutData->ElementAt(colIndex));
        nsTableColPtr col = colData->GetCol();  // col: ADDREF++
        nsStyleMolecule* colStyle =
          (nsStyleMolecule*)mStyleContext->GetData(eStyleStruct_Molecule);
        if (PR_TRUE==IsProportionalWidth(colStyle))
        {
          PRInt32 percentage = (100*mColumnWidths[colIndex]) / aMaxWidth;
          PRInt32 additionalSpace = (spaceRemaining*percentage)/100;
          mColumnWidths[colIndex] += additionalSpace;
          additionalSpaceAdded += additionalSpace;
        }
      }
      if (spaceUsed+additionalSpaceAdded < aMaxTableWidth)
        mColumnWidths[numCols-1] += (aMaxTableWidth - (spaceUsed+additionalSpaceAdded));
    }
*/

// For Debugging ONLY
NS_METHOD nsTableFrame::MoveTo(nscoord aX, nscoord aY)
{
  if ((aX != mRect.x) || (aY != mRect.y)) {
    mRect.x = aX;
    mRect.y = aY;

    nsIView* view;
    GetView(view);

    // Let the view know
    if (nsnull != view) {
      // Position view relative to it's parent, not relative to our
      // parent frame (our parent frame may not have a view).
      nsIView* parentWithView;
      nsPoint origin;
      GetOffsetFromView(origin, parentWithView);
      view->SetPosition(origin.x, origin.y);
      NS_IF_RELEASE(parentWithView);
    }
  }

  return NS_OK;
}

NS_METHOD nsTableFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  mRect.width = aWidth;
  mRect.height = aHeight;

  nsIView* view;
  GetView(view);

  // Let the view know
  if (nsnull != view) {
    view->SetDimensions(aWidth, aHeight);
  }
  return NS_OK;
}

