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
#include "nsIPresContext.h"
#include "nsCSSRendering.h"
#include "nsStyleConsts.h"
#include "nsCellLayoutData.h"
#include "nsVoidArray.h"
#include "prinrval.h"
#include "nsIPtr.h"


#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsDebugCLD = PR_FALSE;
static PRBool gsTiming = PR_FALSE;
static PRBool gsDebugMBP = PR_FALSE;
//#define NOISY
//#define NOISY_FLOW
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsDebugCLD = PR_FALSE;
static const PRBool gsTiming = PR_FALSE;
static const PRBool gsDebugMBP = PR_FALSE;
#endif

#ifndef max
#define max(x, y) ((x) > (y) ? (x) : (y))
#endif

static NS_DEFINE_IID(kStyleBorderSID, NS_STYLEBORDER_SID);
static NS_DEFINE_IID(kStyleColorSID, NS_STYLECOLOR_SID);
static NS_DEFINE_IID(kStylePositionSID, NS_STYLEPOSITION_SID);
static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);

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
                        nsStyleSpacing* aStyleSpacing)
  {
    prevMaxPosBottomMargin = 0;
    prevMaxNegBottomMargin = 0;
    y=0;  // border/padding/margin???

    unconstrainedWidth = PRBool(aMaxSize.width == NS_UNCONSTRAINEDSIZE);
    availSize.width = aMaxSize.width;
    if (!unconstrainedWidth) {
      availSize.width -= aStyleSpacing->mBorderPadding.left +
        aStyleSpacing->mBorderPadding.right;
    }
    leftInset = aStyleSpacing->mBorderPadding.left;

    unconstrainedHeight = PRBool(aMaxSize.height == NS_UNCONSTRAINEDSIZE);
    availSize.height = aMaxSize.height;
    if (!unconstrainedHeight) {
      availSize.height -= aStyleSpacing->mBorderPadding.top +
        aStyleSpacing->mBorderPadding.bottom;
    }
    topInset = aStyleSpacing->mBorderPadding.top;

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


nsTableFrame::nsTableFrame(nsIContent* aContent,
                           PRInt32 aIndexInParent,
                           nsIFrame* aParentFrame)
  : nsContainerFrame(aContent, aIndexInParent, aParentFrame),
    mColumnLayoutData(nsnull),
    mColumnWidths(nsnull),
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
        CellData*     cellData = cellMap->GetCellAt(row,col);
        nsTableCell*  cell = cellData->mCell;
        
        if (!cell)
          continue;

        PRInt32       colSpan = cell->GetColSpan();
        PRInt32       rowSpan = cell->GetRowSpan();

        // clear the cells for all for edges
        for (edge = 0; edge < 4; edge++)
          boundaryCells[edge]->Clear();

    
        // Check to see if the cell data represents the top,left
        // corner of a a table cell

        // Check to see if cell the represents a left edge cell
        if (row == 0)
          above = nsnull;
        else
        {
          cellData = cellMap->GetCellAt(row-1,col);
          if (cellData != nsnull)
            above = cellData->mCell;

          // Does the cell data point to the same cell?
          // If it is, then continue
          if (above != nsnull && above == cell)
            continue;
        }           

        // Check to see if cell the represents a top edge cell
        if (col == 0)
          left = nsnull;
        else
        {
          cellData = cellMap->GetCellAt(row,col-1);
          if (cellData != nsnull)
            left = cellData->mCell;

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
              if (cellData->mCell != above)
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
            if (cellData->mCell != below)
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
              if (cellData->mCell != left)
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
            if (cellData->mCell != right)
            {
              right = cellData->mCell;
              if (right != nsnull)
                AppendLayoutData(boundaryCells[NS_SIDE_RIGHT],right);
            }
          }
          r++;
        }
        
        nsCellLayoutData* cellLayoutData = FindCellLayoutData(cell); 
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
  nsStyleColor* myColor =
    (nsStyleColor*)mStyleContext->GetData(kStyleColorSID);
  nsStyleBorder* myBorder =
    (nsStyleBorder*)mStyleContext->GetData(kStyleBorderSID);
  NS_ASSERTION(nsnull != myColor, "null style color");
  NS_ASSERTION(nsnull != myBorder, "null style border");
  if (nsnull!=myBorder)
  {
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, mRect, *myColor);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, mRect, *myBorder, 0);
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
NS_METHOD nsTableFrame::ResizeReflow(nsIPresContext* aPresContext,
                                     nsReflowMetrics& aDesiredSize,
                                     const nsSize& aMaxSize,
                                     nsSize* aMaxElementSize,
                                     ReflowStatus& aStatus)
{
  NS_PRECONDITION(nsnull != aPresContext, "null arg");
  if (gsDebug==PR_TRUE) 
  {
    printf("-----------------------------------------------------------------\n");
    printf("nsTableFrame::ResizeReflow: maxSize=%d,%d\n",
                               aMaxSize.width, aMaxSize.height);
  }

#ifdef NS_DEBUG
  PreReflowCheck();
#endif

  aStatus = frComplete;

  PRIntervalTime startTime;
  if (gsTiming) {
    startTime = PR_IntervalNow();
  }

  if (PR_TRUE==gsDebug) 
    printf ("*** tableframe reflow\t\t%p\n", this);

  if (PR_TRUE==NeedsReflow(aMaxSize))
  {
    if (PR_FALSE==IsFirstPassValid())
    { // we treat the table as if we've never seen the layout data before
      mPass = kPASS_FIRST;
      aStatus = ResizeReflowPass1(aPresContext, aDesiredSize,
                                  aMaxSize, aMaxElementSize);
      // check result
    }
    mPass = kPASS_SECOND;

    // assign column widths, and assign aMaxElementSize->width
    BalanceColumnWidths(aPresContext, aMaxSize, aMaxElementSize);

    // assign table width
    SetTableWidth(aPresContext);

    aStatus = ResizeReflowPass2(aPresContext, aDesiredSize, aMaxSize,
                                aMaxElementSize, 0, 0);

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
nsIFrame::ReflowStatus nsTableFrame::ResizeReflowPass1(nsIPresContext* aPresContext,
                                                       nsReflowMetrics& aDesiredSize,
                                                       const nsSize& aMaxSize,
                                                       nsSize* aMaxElementSize)
{
  NS_ASSERTION(nsnull!=aPresContext, "bad pres context param");
  NS_ASSERTION(nsnull==mPrevInFlow, "illegal call, cannot call pass 1 on a continuing frame.");

  if (gsDebug==PR_TRUE) printf("nsTableFrame::ResizeReflow Pass1: maxSize=%d,%d\n",
                               aMaxSize.width, aMaxSize.height);
  if (PR_TRUE==gsDebug) printf ("*** tableframe reflow pass1\t\t%d\n", this); 
  ReflowStatus result = frComplete;

  mChildCount = 0;
  mFirstContentOffset = mLastContentOffset = 0;

  nsIContent* c = mContent;
  NS_ASSERTION(nsnull != c, "null content");
  ((nsTablePart *)c)->GetMaxColumns();  // as a side effect, does important initialization

  nsSize availSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE); // availSize is the space available at any given time in the process
  nsSize maxSize(0, 0);       // maxSize is the size of the largest child so far in the process
  nsSize kidMaxSize(0,0);
  nsSize* pKidMaxSize = (nsnull != aMaxElementSize) ? &kidMaxSize : nsnull;
  nsReflowMetrics kidSize;
  nscoord y = 0;
  nscoord maxAscent = 0;
  nscoord maxDescent = 0;
  PRInt32 kidIndex = 0;
  PRInt32 lastIndex = c->ChildCount();
  PRInt32 contentOffset=0;
  nsIFrame* prevKidFrame = nsnull;/* XXX incremental reflow! */

  // Compute the insets (sum of border and padding)
  nsStyleSpacing* spacing =
    (nsStyleSpacing*)mStyleContext->GetData(kStyleSpacingSID);
  nscoord topInset = spacing->mBorderPadding.top;
  nscoord rightInset = spacing->mBorderPadding.right;
  nscoord bottomInset =spacing->mBorderPadding.bottom;
  nscoord leftInset = spacing->mBorderPadding.left;

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
      result = frComplete;
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
        kidFrame = kidDel->CreateFrame(aPresContext, kid, contentOffset, this);
        NS_RELEASE(kidDel);
        kidFrame->SetStyleContext(kidStyleContext);
      }

      nsSize maxKidElementSize;
      result = ReflowChild(kidFrame, aPresContext, kidSize, availSize, pKidMaxSize);

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

      if (frNotComplete == result) {
        // If the child didn't finish layout then it means that it used
        // up all of our available space (or needs us to split).
        mLastContentIsComplete = PR_FALSE;
        break;
      }
    }
    contentOffset++;
    kidIndex++;
    if (frNotComplete == result) {
      // If the child didn't finish layout then it means that it used
      // up all of our available space (or needs us to split).
      mLastContentIsComplete = PR_FALSE;
      break;
    }
  }

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
nsIFrame::ReflowStatus nsTableFrame::ResizeReflowPass2(nsIPresContext* aPresContext,
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

  ReflowStatus result = frComplete;

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
  ReflowStatus  status = frComplete;

  // Check for an overflow list
  MoveOverflowToChildList();

  nsStyleSpacing* mySpacing = (nsStyleSpacing*)
    mStyleContext->GetData(kStyleSpacingSID);
  InnerTableReflowState state(aPresContext, aMaxSize, mySpacing);

  // Reflow the existing frames
  if (nsnull != mFirstChild) {
    reflowMappedOK = ReflowMappedChildren(aPresContext, state, aMaxElementSize);
    if (PR_FALSE == reflowMappedOK) {
      status = frNotComplete;
    }
  }

  // Did we successfully reflow our mapped children?
  if (PR_TRUE == reflowMappedOK) {
    // Any space left?
    if (state.availSize.height <= 0) {
      // No space left. Don't try to pull-up children or reflow unmapped
      if (NextChildOffset() < mContent->ChildCount()) {
        status = frNotComplete;
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
        status = frNotComplete;
      }
    }
  }

  // Return our size and our status

  if (frComplete == status) {
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
                                      nsStyleSpacing* aKidSpacing)
{
  nscoord margin;
  nscoord maxNegTopMargin = 0;
  nscoord maxPosTopMargin = 0;
  if ((margin = aKidSpacing->mMargin.top) < 0) {
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
    nsSize                  kidAvailSize(aState.availSize);
    nsReflowMetrics         desiredSize;
    nsIFrame::ReflowStatus  status;

    // Get top margin for this kid
    nsIContentPtr kid;

    kidFrame->GetContent(kid.AssignRef());
    if (((nsTableContent *)(nsIContent*)kid)->GetType() == nsITableContent::kTableRowGroupType)
    { // skip children that are not row groups
      nsIStyleContextPtr kidSC;

      kidFrame->GetStyleContext(aPresContext, kidSC.AssignRef());
      nsStyleSpacing* kidSpacing = (nsStyleSpacing*)
        kidSC->GetData(kStyleSpacingSID);
      nscoord topMargin = GetTopMarginFor(aPresContext, aState, kidSpacing);
      nscoord bottomMargin = kidSpacing->mMargin.bottom;

      // Figure out the amount of available size for the child (subtract
      // off the top margin we are going to apply to it)
      if (PR_FALSE == aState.unconstrainedHeight) {
        kidAvailSize.height -= topMargin;
      }
      // Subtract off for left and right margin
      if (PR_FALSE == aState.unconstrainedWidth) {
        kidAvailSize.width -= kidSpacing->mMargin.left +
          kidSpacing->mMargin.right;
      }

      // Reflow the child into the available space
      status = ReflowChild(kidFrame, aPresContext, desiredSize,
                           kidAvailSize, pKidMaxElementSize);

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
      kidRect.x += aState.leftInset + kidSpacing->mMargin.left;
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
      mLastContentIsComplete = PRBool(status == frComplete);

      // Special handling for incomplete children
      if (frNotComplete == status) {
        nsIFrame* kidNextInFlow;
         
        kidFrame->GetNextInFlow(kidNextInFlow);
        PRBool lastContentIsComplete = mLastContentIsComplete;
        if (nsnull == kidNextInFlow) {
          // The child doesn't have a next-in-flow so create a continuing
          // frame. This hooks the child into the flow
          nsIFrame* continuingFrame;
           
          kidFrame->CreateContinuingFrame(aPresContext, this, continuingFrame);
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
  lastChild->GetIndexInParent(lastIndexInParent);
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
    nsReflowMetrics kidSize;
    ReflowStatus    status;

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
    nsSize          kidFrameSize;
    SplittableType  kidIsSplittable;

    kidFrame->GetSize(kidFrameSize);
    kidFrame->IsSplittable(kidIsSplittable);
    if ((kidFrameSize.height > aState.availSize.height) &&
        (kidIsSplittable == frNotSplittable)) {
      result = PR_FALSE;
      mLastContentIsComplete = prevLastContentIsComplete;
      break;
    }
    status = ReflowChild(kidFrame, aPresContext, kidSize, aState.availSize,
                         pKidMaxElementSize);

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
    mLastContentIsComplete = PRBool(status == frComplete);
    if (frNotComplete == status) {
      // No the child isn't complete
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      if (nsnull == kidNextInFlow) {
        // The child doesn't have a next-in-flow so create a
        // continuing frame. The creation appends it to the flow and
        // prepares it for reflow.
        nsIFrame* continuingFrame;

        kidFrame->CreateContinuingFrame(aPresContext, this, continuingFrame);
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
nsIFrame::ReflowStatus
nsTableFrame::ReflowUnmappedChildren(nsIPresContext*      aPresContext,
                                     InnerTableReflowState& aState,
                                     nsSize*              aMaxElementSize)
{
#ifdef NS_DEBUG
  VerifyLastIsComplete();
#endif
  nsIFrame*    kidPrevInFlow = nsnull;
  ReflowStatus result = frNotComplete;

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
      result = frComplete;
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
      kidFrame = kidDel->CreateFrame(aPresContext, kid, kidIndex, this);
      NS_RELEASE(kidDel);
      kidFrame->SetStyleContext(kidStyleContext);
    } else {
      kidPrevInFlow->CreateContinuingFrame(aPresContext, this, kidFrame);
    }

    // Try to reflow the child into the available space. It might not
    // fit or might need continuing.
    nsReflowMetrics kidSize;
    ReflowStatus status = ReflowChild(kidFrame,aPresContext, kidSize,
                                      aState.availSize, pKidMaxElementSize);

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
    if (frNotComplete == status) {
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
    (nsStyleSpacing*)mStyleContext->GetData(kStyleSpacingSID);

  // need to figure out the overall table width constraint
  // default case, get 100% of available space
  PRInt32 maxWidth;
  nsStylePosition* position =
    (nsStylePosition*)mStyleContext->GetData(kStylePositionSID);
  switch (position->mWidthFlags) {
  case NS_STYLE_POSITION_VALUE_LENGTH:
    maxWidth = position->mWidth;
    break;
  case NS_STYLE_POSITION_VALUE_PERCENT:
  case NS_STYLE_POSITION_VALUE_PROPORTIONAL:
    // XXX for now these fall through

  default:
  case NS_STYLE_POSITION_VALUE_AUTO:
  case NS_STYLE_POSITION_VALUE_INHERIT:
    maxWidth = aMaxSize.width;
    break;
  }

  // now, if maxWidth is not NS_UNCONSTRAINED, subtract out my border
  // and padding
  if (NS_UNCONSTRAINEDSIZE!=maxWidth)
  {
    maxWidth -= spacing->mBorderPadding.left + spacing->mBorderPadding.right;
    if (0>maxWidth)  // nonsense style specification
      maxWidth = 0;
  }

  if (gsDebug) printf ("  maxWidth=%d from aMaxSize=%d,%d\n", maxWidth, aMaxSize.width, aMaxSize.height);

  // Step 1 - assign the width of all fixed-width columns
  AssignFixedColumnWidths(aPresContext, maxWidth, numCols,
                          totalFixedWidth, minTableWidth, maxTableWidth);

  if (nsnull!=aMaxElementSize)
  {
    aMaxElementSize->width = minTableWidth;
    if (gsDebug) printf("  setting aMaxElementSize->width = %d\n", aMaxElementSize->width);
  }
  else
  {
    if (gsDebug) printf("  nsnull aMaxElementSize\n");
  }

  // Step 2 - assign the width of all proportional-width columns in the remaining space
  PRInt32 availWidth=maxWidth - totalFixedWidth;
  if (gsDebug==PR_TRUE) printf ("Step 2...\n  availWidth = %d\n", availWidth);
  if (TableIsAutoWidth())
  {
    if (gsDebug==PR_TRUE) printf ("  calling BalanceProportionalColumnsForAutoWidthTable\n");
    BalanceProportionalColumnsForAutoWidthTable(aPresContext,
                                                availWidth, maxWidth,
                                                minTableWidth, maxTableWidth);
  }
  else
  {
    if (gsDebug==PR_TRUE) printf ("  calling BalanceProportionalColumnsForSpecifiedWidthTable\n");
    BalanceProportionalColumnsForSpecifiedWidthTable(aPresContext,
                                                     availWidth, maxWidth,
                                                     minTableWidth, maxTableWidth);
  }

/*
STEP 1
for every col
  if the col has a fixed width
    set the width to max(fixed width, maxElementSize)
    if the cell spans columns, divide the cell width between the columns
  else skip it for now

take borders and padding into account

STEP 2
determine the min and max size for the table width
if col is proportionately specified
  if (col width specified to 0)
    col width = minColWidth
  else if (minTableWidth >= aMaxSize.width)
    set col widths to min, install a hor. scroll bar
  else if (maxTableWidth <= aMaxSize.width)
    set each col to its max size
  else
    W = aMaxSize.width -  minTableWidth
    D = maxTableWidth - minTableWidth
    for each col
      d = maxColWidth - minColWidth
      col width = minColWidth + ((d*W)/D)

STEP 3
if there is space left over
  for every col
    if col is proportionately specified
      add space to col width until it is that proportion of the table width
      do this non-destructively in case there isn't enough space
  if there isn't enough space as determined in the prior step,
    add space in proportion to the proportionate width attribute
  */

}

// Step 1 - assign the width of all fixed-width columns, 
//          and calculate min/max table width
PRBool nsTableFrame::AssignFixedColumnWidths(nsIPresContext* aPresContext,
                                             PRInt32 maxWidth, 
                                             PRInt32 aNumCols,
                                             PRInt32 &aTotalFixedWidth,
                                             PRInt32 &aMinTableWidth,
                                             PRInt32 &aMaxTableWidth)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");

  if (gsDebug==PR_TRUE) printf ("  AssignFixedColumnWidths\n");
  for (PRInt32 colIndex = 0; colIndex<aNumCols; colIndex++)
  { 
    nsVoidArray *columnLayoutData = GetColumnLayoutData();
    nsColLayoutData * colData = (nsColLayoutData *)(columnLayoutData->ElementAt(colIndex));
    NS_ASSERTION(nsnull != colData, "bad column data");
    nsTableColPtr col = colData->GetCol();    // col: ADDREF++
    NS_ASSERTION(col.IsNotNull(), "bad col");

    // need to track min/max column width for setting min/max table widths
    PRInt32 minColWidth = 0;
    PRInt32 maxColWidth = 0;
    nsVoidArray *cells = colData->GetCells();
    PRInt32 numCells = cells->Count();
    if (gsDebug==PR_TRUE) printf ("  for column %d  numCells = %d\n", colIndex, numCells);

#if XXX_need_access_to_column_frame_help
    // Get the columns's style
    nsIStyleContextPtr colSC;
    colFrame->GetStyleContext(aPresContext, colSC.AssignRef());
    nsStylePosition* colPosition = (nsStylePosition*)
      colSC->GetData(kStylePositionSID);

    // Get column width if it has one
    PRBool haveColWidth = PR_FALSE;
    nscoord colWidth;
    switch (colPosition->mWidthFlags) {
    default:
    case NS_STYLE_POSITION_VALUE_AUTO:
    case NS_STYLE_POSITION_VALUE_INHERIT:
      break;

    case NS_STYLE_POSITION_VALUE_LENGTH:
      haveColWidth = PR_TRUE;
      colWidth = colPosition->mWidth;
      break;

    case NS_STYLE_POSITION_VALUE_PCT:
    case NS_STYLE_POSITION_VALUE_PROPORTIONAL:
      //XXX haveColWidth = PR_TRUE;
      //XXX colWidth = colPosition->mWidthPCT * something;
      break;
    }
#endif

    // Scan the column
    for (PRInt32 cellIndex = 0; cellIndex<numCells; cellIndex++)
    {
      nsCellLayoutData * data = (nsCellLayoutData *)(cells->ElementAt(cellIndex));
      NS_ASSERTION(nsnull != data, "bad data");
      nsSize * cellMinSize = data->GetMaxElementSize();
      nsReflowMetrics * cellDesiredSize = data->GetDesiredSize();
      NS_ASSERTION(nsnull != cellDesiredSize, "bad cellDesiredSize");
      if (gsDebug==PR_TRUE) 
        printf ("    for cell %d  min = %d,%d  and  des = %d,%d\n", cellIndex, cellMinSize->width, cellMinSize->height,
                cellDesiredSize->width, cellDesiredSize->height);

      PRBool haveCellWidth = PR_FALSE;
      nscoord cellWidth;

      /*
       * The first cell in a column (in row 0) has special standing.
       * if the first cell has a width specification, it overrides the
       * COL width
       */
      if (0==cellIndex)
      {
        nsCellLayoutData * data = (nsCellLayoutData *)(cells->ElementAt(0));
        nsTableCellFrame *cellFrame = data->GetCellFrame();
        nsTableCellPtr cell;
        cellFrame->GetContent((nsIContent*&)(cell.AssignRef()));          // cell: REFCNT++

        // Get the cell's style
        nsIStyleContextPtr cellSC;
        cellFrame->GetStyleContext(aPresContext, cellSC.AssignRef());
        nsStylePosition* cellPosition = (nsStylePosition*)
          cellSC->GetData(kStylePositionSID);
        switch (cellPosition->mWidthFlags) {
        case NS_STYLE_POSITION_VALUE_LENGTH:
          haveCellWidth = PR_TRUE;
          cellWidth = cellPosition->mWidth;
          break;

        case NS_STYLE_POSITION_VALUE_PERCENT:
        case NS_STYLE_POSITION_VALUE_PROPORTIONAL:
          // XXX write me when pct/proportional are supported
          // XXX haveCellWidth = PR_TRUE;
          // XXX cellWidth = cellPosition->mWidth;
          break;

        default:
        case NS_STYLE_POSITION_VALUE_INHERIT:
        case NS_STYLE_POSITION_VALUE_AUTO:
          break;
        }
      }

#if XXX_need_access_to_column_frame_help
      switch (colPosition->mWidthFlags) {
      default:
      case NS_STYLE_POSITION_VALUE_AUTO:
      case NS_STYLE_POSITION_VALUE_INHERIT:
        break;

      case NS_STYLE_POSITION_VALUE_LENGTH:
        {
          // This col has a fixed width, so set the cell's width to the
          // larger of (specified width, largest max_element_size of the
          // cells in the column)
          PRInt32 widthForThisCell = max(cellMinSize->width, colPosition->mWidth);
          if (mColumnWidths[colIndex] < widthForThisCell)
          {
            if (gsDebug) printf ("    setting fixed width to %d\n",widthForThisCell);
            mColumnWidths[colIndex] = widthForThisCell;
            maxColWidth = widthForThisCell;
          }
        }
        break;

      case NS_STYLE_POSITION_VALUE_PCT:
      case NS_STYLE_POSITION_VALUE_PROPORTIONAL:
        // XXX write me when pct/proportional are supported
        break;
      }
#endif

      // regardless of the width specification, keep track of the
      // min/max column widths
      /* TODO: must distribute COLSPAN'd cell widths between columns,
       *       either here or in the subsequent Balance***ColumnWidth
       *       routines
       */
      if (minColWidth < cellMinSize->width)
        minColWidth = cellMinSize->width;
      if (maxColWidth < cellDesiredSize->width)
        maxColWidth = cellDesiredSize->width;
      if (gsDebug) {
        printf ("    after cell %d, minColWidth = %d and maxColWidth = %d\n",
                cellIndex, minColWidth, maxColWidth);
      }
      /* take colspan into account? */
      /*
      PRInt32 colSpan = col->GetColSpan();
      cellIndex += colSpan-1;
      */
    }

#if 0
    // if the col is fixed-width, expand the col to the specified
    // fixed width if necessary
    if (colStyle->fixedWidth > mColumnWidths[colIndex])
      mColumnWidths[colIndex] = colStyle->fixedWidth;

    // keep a running total of the amount of space taken up by all
    // fixed-width columns
    aTotalFixedWidth += mColumnWidths[colIndex];
    if (gsDebug) {
      printf ("    after col %d, aTotalFixedWidth = %d\n",
              colIndex, aTotalFixedWidth);
    }
#endif

    // add col[i] metrics to the running totals for the table min/max width
    if (NS_UNCONSTRAINEDSIZE!=aMinTableWidth)
      aMinTableWidth += minColWidth;  // SEC: insets!
    if (aMinTableWidth<=0) 
      aMinTableWidth = NS_UNCONSTRAINEDSIZE; // handle overflow
    if (NS_UNCONSTRAINEDSIZE!=aMaxTableWidth)
      aMaxTableWidth += maxColWidth;  // SEC: insets!
    if (aMaxTableWidth<=0) 
      aMaxTableWidth = NS_UNCONSTRAINEDSIZE; // handle overflow
    if (gsDebug==PR_TRUE) 
      printf ("  after this col, minTableWidth = %d and maxTableWidth = %d\n", aMinTableWidth, aMaxTableWidth);
  
  } // end Step 1 for fixed-width columns
  return PR_TRUE;
}

PRBool nsTableFrame::BalanceProportionalColumnsForSpecifiedWidthTable(nsIPresContext* aPresContext,
                                                                      PRInt32 aAvailWidth,
                                                                      PRInt32 aMaxWidth,
                                                                      PRInt32 aMinTableWidth, 
                                                                      PRInt32 aMaxTableWidth)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");

  PRBool result = PR_TRUE;

  if (NS_UNCONSTRAINEDSIZE==aMaxWidth)
  { // the max width of the table fits comfortably in the available space
    if (gsDebug) printf ("  * table laying out in NS_UNCONSTRAINEDSIZE, calling BalanceColumnsTableFits\n");
    result = BalanceColumnsTableFits(aPresContext, aAvailWidth);
  }
  else if (aMinTableWidth > aMaxWidth)
  { // the table doesn't fit in the available space
    if (gsDebug) printf ("  * min table does not fit, calling SetColumnsToMinWidth\n");
    result = SetColumnsToMinWidth(aPresContext);
  }
  else if (aMaxTableWidth <= aMaxWidth)
  { // the max width of the table fits comfortably in the available space
    if (gsDebug) printf ("  * table desired size fits, calling BalanceColumnsTableFits\n");
    result = BalanceColumnsTableFits(aPresContext, aAvailWidth);
  }
  else
  { // the table fits somewhere between its min and desired size
    if (gsDebug) printf ("  * table desired size does not fit, calling BalanceColumnsHTML4Constrained\n");
    result = BalanceColumnsHTML4Constrained(aPresContext, aAvailWidth,
                                            aMaxWidth, aMinTableWidth, aMaxTableWidth);
  }
  return result;
}

PRBool nsTableFrame::BalanceProportionalColumnsForAutoWidthTable( nsIPresContext* aPresContext,
                                                                  PRInt32 aAvailWidth,
                                                                  PRInt32 aMaxWidth,
                                                                  PRInt32 aMinTableWidth, 
                                                                  PRInt32 aMaxTableWidth)
{
  PRBool result = PR_TRUE;

  if (NS_UNCONSTRAINEDSIZE==aMaxWidth)
  { // the max width of the table fits comfortably in the available space
    if (gsDebug) printf ("  * table laying out in NS_UNCONSTRAINEDSIZE, calling BalanceColumnsTableFits\n");
    result = BalanceColumnsTableFits(aPresContext, aAvailWidth);
  }
  else if (aMinTableWidth > aMaxWidth)
  { // the table doesn't fit in the available space
    if (gsDebug) printf ("  * min table does not fit, calling SetColumnsToMinWidth\n");
    result = SetColumnsToMinWidth(aPresContext);
  }
  else if (aMaxTableWidth <= aMaxWidth)
  { // the max width of the table fits comfortably in the available space
    if (gsDebug) printf ("  * table desired size fits, calling BalanceColumnsTableFits\n");
    result = BalanceColumnsTableFits(aPresContext, aAvailWidth);
  }
  else
  { // the table fits somewhere between its min and desired size
    if (gsDebug) printf ("  * table desired size does not fit, calling BalanceColumnsHTML4Constrained\n");
    result = BalanceColumnsHTML4Constrained(aPresContext, aAvailWidth,
                                            aMaxWidth, aMinTableWidth, aMaxTableWidth);
  }
  return result;
}

PRBool nsTableFrame::SetColumnsToMinWidth(nsIPresContext* aPresContext)
{
  PRBool result = PR_TRUE;
  nsVoidArray *columnLayoutData = GetColumnLayoutData();
  PRInt32 numCols = columnLayoutData->Count();
  for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
  { 
    nsColLayoutData * colData = (nsColLayoutData *)(columnLayoutData->ElementAt(colIndex));
    nsTableColPtr col = colData->GetCol();  // col: ADDREF++
    nsVoidArray *cells = colData->GetCells();
    PRInt32 minColWidth = 0;
    PRInt32 maxColWidth = 0;
    PRInt32 numCells = cells->Count();
    if (gsDebug==PR_TRUE) printf ("  for col %d\n", colIndex);

    // XXX need column frame to ask this question
    nsStylePosition* colPosition = nsnull;

    if (PR_TRUE==IsProportionalWidth(colPosition))
    {
      for (PRInt32 cellIndex = 0; cellIndex<numCells; cellIndex++)
      { // this col has proportional width, so determine its width requirements
        nsCellLayoutData * data = (nsCellLayoutData *)(cells->ElementAt(cellIndex));
        NS_ASSERTION(nsnull != data, "bad data");
        nsSize * cellMinSize = data->GetMaxElementSize();
        NS_ASSERTION(nsnull != cellMinSize, "bad cellMinSize");
        nsReflowMetrics * cellDesiredSize = data->GetDesiredSize();
        NS_ASSERTION(nsnull != cellDesiredSize, "bad cellDesiredSize");
        if (minColWidth < cellMinSize->width)
          minColWidth = cellMinSize->width;
        if (maxColWidth < cellDesiredSize->width)
          maxColWidth = cellDesiredSize->width;
        /*
        if (gsDebug==PR_TRUE) 
          printf ("    after cell %d, minColWidth = %d and maxColWidth = %d\n", 
                  cellIndex, minColWidth, maxColWidth);
        */
      }

      if (gsDebug==PR_TRUE) 
      {
        printf ("  for determining width of col %d %s:\n",colIndex, IsProportionalWidth(colPosition)? "(P)":"(A)");
        printf ("    minColWidth = %d and maxColWidth = %d\n", minColWidth, maxColWidth);
      }

      // XXX BUG: why are we asking this again? this if is already in a
      // IsProportionalWidth == PR_TRUE case!
      if (PR_TRUE==IsProportionalWidth(colPosition))
      { // this col has proportional width, so set its width based on the table width
        mColumnWidths[colIndex] = minColWidth;
        if (gsDebug==PR_TRUE) 
          printf ("  2: col %d, set to width = %d\n", colIndex, mColumnWidths[colIndex]);
      }
    }
  }
  return result;
}

PRBool nsTableFrame::BalanceColumnsTableFits(nsIPresContext* aPresContext, 
                                             PRInt32 aAvailWidth)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");

  PRBool result = PR_TRUE;
  nsVoidArray *columnLayoutData = GetColumnLayoutData();
  PRInt32 numCols = columnLayoutData->Count();
  for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
  { 
    nsColLayoutData * colData = (nsColLayoutData *)(columnLayoutData->ElementAt(colIndex));
    nsTableColPtr col = colData->GetCol();  // col: ADDREF++
    nsVoidArray *cells = colData->GetCells();
    PRInt32 minColWidth = 0;
    PRInt32 maxColWidth = 0;
    PRInt32 numCells = cells->Count();
    if (gsDebug==PR_TRUE) printf ("  for col %d\n", colIndex);
    /* TODO:  must distribute COLSPAN'd cell widths between columns, either here 
     *        or in the prior Balance***ColumnWidth routines 
     */

    // XXX Need columnFrame to ask the style question
    nsStylePosition* colPosition = nsnull;

    if (PR_TRUE==IsProportionalWidth(colPosition))
    {
      for (PRInt32 cellIndex = 0; cellIndex<numCells; cellIndex++)
      { // this col has proportional width, so determine its width requirements
        nsCellLayoutData * data = (nsCellLayoutData *)(cells->ElementAt(cellIndex));
        NS_ASSERTION(nsnull != data, "bad data");
        nsSize * cellMinSize = data->GetMaxElementSize();
        NS_ASSERTION(nsnull != cellMinSize, "bad cellMinSize");
        nsReflowMetrics * cellDesiredSize = data->GetDesiredSize();
        NS_ASSERTION(nsnull != cellDesiredSize, "bad cellDesiredSize");
        if (minColWidth < cellMinSize->width)
          minColWidth = cellMinSize->width;
        if (maxColWidth < cellDesiredSize->width)
          maxColWidth = cellDesiredSize->width;
        if (gsDebug==PR_TRUE) 
          printf ("    after cell %d, minColWidth = %d and maxColWidth = %d\n", 
                  cellIndex, minColWidth, maxColWidth);
      }

      if (gsDebug==PR_TRUE) 
      {
        printf ("  for determining width of col %d %s:\n",colIndex, IsProportionalWidth(colPosition)? "(P)":"(A)");
        printf ("    minColWidth = %d and maxColWidth = %d\n", minColWidth, maxColWidth);
        printf ("    aAvailWidth = %d\n", aAvailWidth);
      }

      // XXX BUG: why are we asking this again? this if is already in a
      // IsProportionalWidth == PR_TRUE case!
      if (PR_TRUE==IsProportionalWidth(colPosition))
      { // this col has proportional width, so set its width based on the table width
#if XXX_bug_kipp_about_this
        if (0==colStyle->proportionalWidth)
        { // col width is specified to be the minimum
          mColumnWidths[colIndex] = minColWidth;
          if (gsDebug==PR_TRUE) 
            printf ("  3 (0): col %d set to min width = %d because style set proportionalWidth=0\n", 
                    colIndex, mColumnWidths[colIndex]);
        }
        else // BUG? else? other code below has the else
#endif
        if (PR_TRUE==AutoColumnWidths())
        {  // give each remaining column it's desired width
           // if there is width left over, we'll factor that in after this loop is complete
          mColumnWidths[colIndex] = maxColWidth;
          if (gsDebug==PR_TRUE) 
            printf ("  3a: col %d with availWidth %d, set to width = %d\n", 
                    colIndex, aAvailWidth, mColumnWidths[colIndex]);
        }
        else
        {  // give each remaining column an equal percentage of the remaining space
          PRInt32 percentage = -1;
          if (NS_UNCONSTRAINEDSIZE==aAvailWidth)
          {
            mColumnWidths[colIndex] = maxColWidth;
          }
          else
          {
#if XXX_bug_kipp_about_this
            percentage = colStyle->proportionalWidth;
            if (-1==percentage)
#endif
              percentage = 100/numCols;
            mColumnWidths[colIndex] = (percentage*aAvailWidth)/100;
            // if the column was computed to be too small, enlarge the column
            if (mColumnWidths[colIndex] <= minColWidth)
              mColumnWidths[colIndex] = minColWidth;
          }
          if (gsDebug==PR_TRUE) 
            printf ("  3b: col %d given %d percent of availWidth %d, set to width = %d\n", 
                    colIndex, percentage, aAvailWidth, mColumnWidths[colIndex]);
        }
      }
    }
  }
  return result;
}

PRBool nsTableFrame::BalanceColumnsHTML4Constrained(nsIPresContext* aPresContext,
                                                    PRInt32 aAvailWidth,
                                                    PRInt32 aMaxWidth,
                                                    PRInt32 aMinTableWidth, 
                                                    PRInt32 aMaxTableWidth)
{
  NS_ASSERTION(nsnull==mPrevInFlow, "never ever call me on a continuing frame!");

  PRBool result = PR_TRUE;
  PRInt32 maxOfAllMinColWidths = 0;
  nsVoidArray *columnLayoutData = GetColumnLayoutData();
  PRInt32 numCols = columnLayoutData->Count();
  for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
  { 
    nsColLayoutData * colData = (nsColLayoutData *)(columnLayoutData->ElementAt(colIndex));
    nsTableColPtr col = colData->GetCol();  // col: ADDREF++

#if XXX_bug_kipp_about_this
    // XXX BUG: mStyleContext is for the table frame not for the column.
    nsStyleMolecule* colStyle =
      (nsStyleMolecule*)mStyleContext->GetData(kStyleMoleculeSID);
#else
    nsStylePosition* colPosition = nsnull;
#endif

    nsVoidArray *cells = colData->GetCells();
    PRInt32 minColWidth = 0;
    PRInt32 maxColWidth = 0;
    PRInt32 numCells = cells->Count();
    if (gsDebug==PR_TRUE) printf ("  for col %d\n", colIndex);

    if (PR_TRUE==IsProportionalWidth(colPosition))
    {
      for (PRInt32 cellIndex = 0; cellIndex<numCells; cellIndex++)
      { // this col has proportional width, so determine its width requirements
        nsCellLayoutData * data = (nsCellLayoutData *)(cells->ElementAt(cellIndex));
        NS_ASSERTION(nsnull != data, "bad data");
        nsSize * cellMinSize = data->GetMaxElementSize();
        NS_ASSERTION(nsnull != cellMinSize, "bad cellMinSize");
        nsReflowMetrics * cellDesiredSize = data->GetDesiredSize();
        NS_ASSERTION(nsnull != cellDesiredSize, "bad cellDesiredSize");
        if (minColWidth < cellMinSize->width)
          minColWidth = cellMinSize->width;
        if (maxColWidth < cellDesiredSize->width)
          maxColWidth = cellDesiredSize->width;
        /*
        if (gsDebug==PR_TRUE) 
          printf ("    after cell %d, minColWidth = %d and maxColWidth = %d\n", 
                  cellIndex, minColWidth, maxColWidth);
        */
      }

      if (gsDebug==PR_TRUE) 
      {
        printf ("  for determining width of col %d %s:\n",colIndex, IsProportionalWidth(colPosition)? "(P)":"(A)");
        printf ("    minTableWidth = %d and maxTableWidth = %d\n", aMinTableWidth, aMaxTableWidth);
        printf ("    minColWidth = %d and maxColWidth = %d\n", minColWidth, maxColWidth);
        printf ("    aAvailWidth = %d\n", aAvailWidth);
      }

      // XXX BUG: why are we asking this again? this if is already in a
      // IsProportionalWidth == PR_TRUE case!
      if (PR_TRUE==IsProportionalWidth(colPosition))
      { // this col has proportional width, so set its width based on the table width
        // the table fits in the space somewhere between its min and max size
        // so dole out the available space appropriately
#if XXX_bug_kipp_about_this
        if (0==colStyle->proportionalWidth)
        { // col width is specified to be the minimum
          mColumnWidths[colIndex] = minColWidth;
          if (gsDebug==PR_TRUE) 
            printf ("  4 (0): col %d set to min width = %d because style set proportionalWidth=0\n", 
                    colIndex, mColumnWidths[colIndex]);
        }
        else
#endif
          if (AutoColumnWidths())
        {
          PRInt32 W = aMaxWidth - aMinTableWidth;
          PRInt32 D = aMaxTableWidth - aMinTableWidth;
          PRInt32 d = maxColWidth - minColWidth;
          mColumnWidths[colIndex] = minColWidth + ((d*W)/D);
          if (gsDebug==PR_TRUE) 
            printf ("  4 auto-width:  col %d  W=%d  D=%d  d=%d, set to width = %d\n", 
                    colIndex, W, D, d, mColumnWidths[colIndex]);
        }
        else
        {  // give each remaining column an equal percentage of the remaining space
#if XXX_bug_kipp_about_this
          PRInt32 percentage = colStyle->proportionalWidth;
          if (-1==percentage)
#endif
            PRInt32 percentage = 100/numCols;
          mColumnWidths[colIndex] = (percentage*aAvailWidth)/100;
          // if the column was computed to be too small, enlarge the column
          if (mColumnWidths[colIndex] <= minColWidth)
          {
            mColumnWidths[colIndex] = minColWidth;
            if (maxOfAllMinColWidths < minColWidth)
              maxOfAllMinColWidths = minColWidth;
          }
          if (gsDebug==PR_TRUE) 
          {
            printf ("  4 equal width: col %d given %d percent of availWidth %d, set to width = %d\n", 
                    colIndex, percentage, aAvailWidth, mColumnWidths[colIndex]);
            if (0!=maxOfAllMinColWidths)
              printf("   and setting maxOfAllMins to %d\n", maxOfAllMinColWidths);
          }
        }
      }
    }
  }

  // post-process if necessary

  // if columns have equal width, and some column's content couldn't squeeze into the computed size, 
  // then expand every column to the min size of the column with the largest min size
  if (!AutoColumnWidths() && 0!=maxOfAllMinColWidths)
  {
    if (gsDebug==PR_TRUE) printf("  EqualColWidths specified, so setting all col widths to %d\n", maxOfAllMinColWidths);
    for (PRInt32 colIndex = 0; colIndex<numCols; colIndex++)
      mColumnWidths[colIndex] = maxOfAllMinColWidths;
  }

  return result;
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
    (nsStyleSpacing*)mStyleContext->GetData(kStyleSpacingSID);
  nscoord rightInset = spacing->mBorderPadding.right;
  nscoord leftInset = spacing->mBorderPadding.left;
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

  PRInt32 tableHeight = 0;

  nsStyleSpacing* spacing = (nsStyleSpacing*)
    mStyleContext->GetData(kStyleSpacingSID);
  tableHeight +=
    spacing->mBorderPadding.top + spacing->mBorderPadding.bottom;

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
      for (PRInt32 rowIndex = 0; rowIndex < numRows; rowIndex++)
      {
        // get the height of the tallest cell in the row (excluding cells that span rows)
        nsTableRowFrame *rowFrame;
         
        rowGroupFrame->ChildAt(rowIndex, (nsIFrame*&)rowFrame);
        NS_ASSERTION(nsnull != rowFrame, "bad row frame");
        rowHeights[rowIndex] = rowFrame->GetTallestChild();

        nsSize  rowFrameSize;

        rowFrame->GetSize(rowFrameSize);
        rowFrame->SizeTo(rowFrameSize.width, rowHeights[rowIndex]);
        rowGroupHeight += rowHeights[rowIndex];
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
            cellFrame->SizeTo(cellFrameSize.width, rowHeights[rowIndex]);
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
            PRInt32 heightOfRowsSpanned = 0;
            for (PRInt32 i=0; i<rowSpan; i++)
              heightOfRowsSpanned += rowHeights[i+rowIndex];

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

// XXX Kipp wonders: what does this really mean? Are you really asking
// "Is it fixed width"? If so, then VALUE_PCT may be wrong and the
// name of the method should be changed.

PRBool nsTableFrame::IsProportionalWidth(nsStylePosition* aStylePosition)
{
  PRBool result = PR_FALSE;
  if (nsnull == aStylePosition) {
    // Assume NS_STYLE_POSITION_VALUE_AUTO when no style is available
    result = PR_TRUE;
  }
  else {
    switch (aStylePosition->mWidthFlags) {
    case NS_STYLE_POSITION_VALUE_LENGTH:
    case NS_STYLE_POSITION_VALUE_PERCENT:
      break;

    default:
    case NS_STYLE_POSITION_VALUE_AUTO:
    case NS_STYLE_POSITION_VALUE_INHERIT:
    case NS_STYLE_POSITION_VALUE_PROPORTIONAL:
      result = PR_TRUE;
      break;
    }
  }
  return result;
}

/**
  */
NS_METHOD nsTableFrame::IncrementalReflow(nsIPresContext*  aCX,
                                          nsReflowMetrics& aDesiredSize,
                                          const nsSize&    aMaxSize,
                                          nsReflowCommand& aReflowCommand,
                                          ReflowStatus&    aStatus)
{
  NS_ASSERTION(nsnull != aCX, "bad arg");
  if (gsDebug==PR_TRUE) printf ("nsTableFrame::IncrementalReflow: maxSize=%d,%d\n",
                                aMaxSize.width, aMaxSize.height);

  // mFirstPassValid needs to be set somewhere in response to change notifications.

  aDesiredSize.width = mRect.width;
  aDesiredSize.height = mRect.height;
  aStatus = frComplete;
  return NS_OK;
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

//ZZZ TOTAL HACK
PRBool isTableAutoWidth = PR_TRUE;
PRBool isAutoColumnWidths = PR_TRUE;

PRBool nsTableFrame::TableIsAutoWidth()
{ // ZZZ: TOTAL HACK
  return isTableAutoWidth; 
}

PRBool nsTableFrame::AutoColumnWidths()
{ // ZZZ: TOTAL HACK
  return isAutoColumnWidths;
}

NS_METHOD nsTableFrame::CreateContinuingFrame(nsIPresContext* aPresContext,
                                              nsIFrame*       aParent,
                                              nsIFrame*&      aContinuingFrame)
{
  nsTableFrame* cf = new nsTableFrame(mContent, mIndexInParent, aParent);
  PrepareContinuingFrame(aPresContext, aParent, cf);
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
      nsIFrame * duplicateFrame = kidDel->CreateFrame(aPresContext, content, index, cf);
      NS_RELEASE(kidDel);                                                // kidDel: REFCNT--
      duplicateFrame->SetStyleContext(kidStyleContext);
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


/* ---------- static methods ---------- */

nsresult nsTableFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                nsIContent* aContent,
                                PRInt32     aIndexInParent,
                                nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableFrame(aContent, aIndexInParent, aParent);
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
          (nsStyleMolecule*)mStyleContext->GetData(kStyleMoleculeSID);
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




