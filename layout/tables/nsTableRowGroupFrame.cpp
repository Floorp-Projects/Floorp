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
#include "nsTableRowGroupFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableCellFrame.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIContent.h"
#include "nsIView.h"
#include "nsIPtr.h"
#include "nsIReflowCommand.h"
#include "nsHTMLIIDs.h"

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsDebugIR = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsDebugIR = PR_FALSE;
#endif

NS_DEF_PTR(nsIStyleContext);
NS_DEF_PTR(nsIContent);

/* ----------- RowGroupReflowState ---------- */

struct RowGroupReflowState {
  nsIPresContext& mPresContext;  // Our pres context
  const nsHTMLReflowState& reflowState;  // Our reflow state

  // The body's available size (computed from the body's parent)
  nsSize availSize;

  // Margin tracking information
  nscoord prevMaxPosBottomMargin;
  nscoord prevMaxNegBottomMargin;

  // Flags for whether the max size is unconstrained
  PRBool  unconstrainedWidth;
  PRBool  unconstrainedHeight;

  // Running y-offset
  nscoord y;

  // Flag used to set aMaxElementSize to my first row
  PRBool  firstRow;

  // Remember the height of the first row, because it's our maxElementHeight (plus header/footers)
  nscoord firstRowHeight;

  RowGroupReflowState(nsIPresContext&          aPresContext,
                      const nsHTMLReflowState& aReflowState)
    : mPresContext(aPresContext),
      reflowState(aReflowState)
  {
    availSize.width = reflowState.maxSize.width;
    availSize.height = reflowState.maxSize.height;
    prevMaxPosBottomMargin = 0;
    prevMaxNegBottomMargin = 0;
    y=0;  // border/padding/margin???
    unconstrainedWidth = PRBool(reflowState.maxSize.width == NS_UNCONSTRAINEDSIZE);
    unconstrainedHeight = PRBool(reflowState.maxSize.height == NS_UNCONSTRAINEDSIZE);
    firstRow = PR_TRUE;
    firstRowHeight=0;
  }

  ~RowGroupReflowState() {
  }
};




/* ----------- nsTableRowGroupFrame ---------- */

nsTableRowGroupFrame::nsTableRowGroupFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsContainerFrame(aContent, aParentFrame)
{
  aContent->GetTag(mType);       // mType: REFCNT++
}

nsTableRowGroupFrame::~nsTableRowGroupFrame()
{
  NS_IF_RELEASE(mType);              // mType: REFCNT--
}

NS_METHOD nsTableRowGroupFrame::GetRowGroupType(nsIAtom *& aType)
{
  NS_ADDREF(mType);
  aType=mType;
  return NS_OK;
}

NS_METHOD nsTableRowGroupFrame::GetRowCount(PRInt32 &aCount)
{
  // init out-param
  aCount=0;

  // loop through children, adding one to aCount for every legit row
  nsIFrame *childFrame = mFirstChild;
  while (PR_TRUE)
  {
    if (nsnull==childFrame)
      break;
    const nsStyleDisplay *childDisplay;
    childFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
      aCount++;
    childFrame->GetNextSibling(childFrame);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTableRowGroupFrame::Init(nsIPresContext& aPresContext, nsIFrame* aChildList)
{
  mFirstChild = aChildList;
  return NS_OK;
}

NS_METHOD nsTableRowGroupFrame::Paint(nsIPresContext& aPresContext,
                                      nsIRenderingContext& aRenderingContext,
                                      const nsRect&        aDirtyRect)
{

  // for debug...
  /*
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(128,0,0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
  */

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
}

// aDirtyRect is in our coordinate system
// child rect's are also in our coordinate system
/** overloaded method from nsContainerFrame.  The difference is that 
  * we don't want to clip our children, so a cell can do a rowspan
  */
void nsTableRowGroupFrame::PaintChildren(nsIPresContext&      aPresContext,
                                         nsIRenderingContext& aRenderingContext,
                                         const nsRect&        aDirtyRect)
{
  nsIFrame* kid = mFirstChild;
  while (nsnull != kid) {
    nsIView *pView;
     
    kid->GetView(pView);
    if (nsnull == pView) {
      nsRect kidRect;
      kid->GetRect(kidRect);
      nsRect damageArea(aDirtyRect);
      // Translate damage area into kid's coordinate system
      nsRect kidDamageArea(damageArea.x - kidRect.x, damageArea.y - kidRect.y,
                           damageArea.width, damageArea.height);
      aRenderingContext.PushState();
      aRenderingContext.Translate(kidRect.x, kidRect.y);
      kid->Paint(aPresContext, aRenderingContext, kidDamageArea);
      if (nsIFrame::GetShowFrameBorders()) {
        aRenderingContext.SetColor(NS_RGB(255,0,0));
        aRenderingContext.DrawRect(0, 0, kidRect.width, kidRect.height);
      }
      aRenderingContext.PopState();
    }
    kid->GetNextSibling(kid);
  }
}

// Collapse child's top margin with previous bottom margin
nscoord nsTableRowGroupFrame::GetTopMarginFor(nsIPresContext*      aCX,
                                              RowGroupReflowState& aState,
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
void nsTableRowGroupFrame::PlaceChild( nsIPresContext*    aPresContext,
																			 RowGroupReflowState& aState,
																			 nsIFrame*          aKidFrame,
																			 const nsRect&      aKidRect,
																			 nsSize*            aMaxElementSize,
																			 nsSize&            aKidMaxElementSize)
{
  if (PR_TRUE==gsDebug)
    printf ("rowgroup %p: placing row at %d, %d, %d, %d\n",
            this, aKidRect.x, aKidRect.y, aKidRect.width, aKidRect.height);

  // Place and size the child
  aKidFrame->SetRect(aKidRect);

  // Adjust the running y-offset
  aState.y += aKidRect.height;

  // If our height is constrained then update the available height
  if (PR_FALSE == aState.unconstrainedHeight) {
    aState.availSize.height -= aKidRect.height;
  }

  // Update the maximum element size
  if (PR_TRUE==aState.firstRow)
  {
    aState.firstRow = PR_FALSE;
    aState.firstRowHeight = aKidRect.height;
    if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = aKidMaxElementSize.width;
      aMaxElementSize->height = aKidMaxElementSize.height;
    }
  }
  else if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = PR_MAX(aMaxElementSize->width, aKidMaxElementSize.width);
  }
  if (gsDebug && nsnull != aMaxElementSize)
    printf ("rowgroup %p: placing row %p with width = %d and MES= %d\n",
            this, aKidFrame, aKidRect.width, aMaxElementSize->width);
}

/**
 * Reflow the frames we've already created
 *
 * @param   aPresContext presentation context to use
 * @param   aState current inline state
 * @return  true if we successfully reflowed all the mapped children and false
 *            otherwise, e.g. we pushed children to the next in flow
 */
PRBool nsTableRowGroupFrame::ReflowMappedChildren( nsIPresContext*      aPresContext,
                                                   RowGroupReflowState& aState,
                                                   nsSize*              aMaxElementSize)
{
  NS_PRECONDITION(nsnull != mFirstChild, "no children");
  nsIFrame* prevKidFrame = nsnull;
  nsSize    kidMaxElementSize;
  nsSize*   pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  PRBool    result = PR_TRUE;
  PRInt32   debugCounter=0;
  for (nsIFrame*  kidFrame = mFirstChild; nsnull != kidFrame; ) {
    nsSize                  kidAvailSize(aState.availSize);
    if (0>=kidAvailSize.height)
      kidAvailSize.height = 1;      // XXX: HaCk - we don't handle negative heights yet
    nsHTMLReflowMetrics desiredSize(pKidMaxElementSize);
    desiredSize.width=desiredSize.height=desiredSize.ascent=desiredSize.descent=0;
    nsReflowStatus  status;

    // Get top margin for this kid
    nsIContentPtr kid;
     
    kidFrame->GetContent(kid.AssignRef());
    nsIStyleContextPtr kidSC;
     
    kidFrame->GetStyleContext(aPresContext, kidSC.AssignRef());
    const nsStyleSpacing* kidSpacing = (const nsStyleSpacing*)
      kidSC->GetStyleData(eStyleStruct_Spacing);
    nsMargin kidMargin;
    kidSpacing->CalcMarginFor(this, kidMargin);
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
    nsHTMLReflowState kidReflowState(*aPresContext, kidFrame,
                                     aState.reflowState, kidAvailSize);

    if (gsDebug) printf("%p RG reflowing child %d (frame=%p) with avail width = %d\n",
                        this, debugCounter, kidFrame, kidAvailSize.width);
    ReflowChild(kidFrame, *aPresContext, desiredSize, kidReflowState, status);
    if (gsDebug) printf("%p RG child %d (frame=%p) returned desired width = %d\n",
                        this, debugCounter, kidFrame, desiredSize.width);

    // Did the child fit?
    if ((kidFrame != mFirstChild) &&
        ((kidAvailSize.height <= 0) ||
         (desiredSize.height > kidAvailSize.height)))
    {
      // The child's height is too big to fit at all in our remaining space,
      // and it's not our first child.
      //
      // Note that if the width is too big that's okay and we allow the
      // child to extend horizontally outside of the reflow area
      PushChildren(kidFrame, prevKidFrame);
      result = PR_FALSE;
      break;
    }

    // Place the child after taking into account it's margin
    nsRect kidRect (kidMargin.left, aState.y, desiredSize.width, desiredSize.height);
    PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize,
               kidMaxElementSize);
    if (bottomMargin < 0) {
      aState.prevMaxNegBottomMargin = -bottomMargin;
    } else {
      aState.prevMaxPosBottomMargin = bottomMargin;
    }

		// Remember where we just were in case we end up pushing children
		prevKidFrame = kidFrame;

    /* Row groups should not create continuing frames for rows 
     * unless they absolutely have to!
     * check to see if this is absolutely necessary (with new params from troy)
     * otherwise PushChildren and bail.
     */
    // Special handling for incomplete children
    if (NS_FRAME_IS_NOT_COMPLETE(status)) {
      // XXX It's good to assume that we might still have room
      // even if the child didn't complete (floaters will want this)
      nsIFrame* kidNextInFlow;
       
      kidFrame->GetNextInFlow(kidNextInFlow);
      if (nsnull == kidNextInFlow) {
        // No the child isn't complete, and it doesn't have a next in flow so
        // create a continuing frame. This hooks the child into the flow.
        nsIFrame* continuingFrame;
        nsIStyleContext* kidSC;
        kidFrame->GetStyleContext(aPresContext, kidSC);
        kidFrame->CreateContinuingFrame(*aPresContext, this, kidSC,
                                        continuingFrame);
        NS_RELEASE(kidSC);

        // Insert the frame. We'll reflow it next pass through the loop
        nsIFrame* nextSib;
         
        kidFrame->GetNextSibling(nextSib);
        continuingFrame->SetNextSibling(nextSib);
        kidFrame->SetNextSibling(continuingFrame);
      }
      // We've used up all of our available space so push the remaining
      // children to the next-in-flow
      nsIFrame* nextSibling;
       
      kidFrame->GetNextSibling(nextSibling);
      if (nsnull != nextSibling) {
        PushChildren(nextSibling, kidFrame);
      }
      result = PR_FALSE;
      break;
    }

    // Add back in the left and right margins, because one row does not 
    // impact another row's width
    if (PR_FALSE == aState.unconstrainedWidth) {
      kidAvailSize.width += kidMargin.left + kidMargin.right;
    }

    // Get the next child
    kidFrame->GetNextSibling(kidFrame);
    debugCounter++;
  }

  // Update the child count
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
PRBool nsTableRowGroupFrame::PullUpChildren(nsIPresContext*      aPresContext,
                                            RowGroupReflowState& aState,
                                            nsSize*              aMaxElementSize)
{
  nsTableRowGroupFrame* nextInFlow = (nsTableRowGroupFrame*)mNextInFlow;
  nsSize         kidMaxElementSize;
  nsSize*        pKidMaxElementSize = (nsnull != aMaxElementSize) ? &kidMaxElementSize : nsnull;
  nsIFrame*      prevKidFrame = LastFrame(mFirstChild);
  PRBool         result = PR_TRUE;

  while (nsnull != nextInFlow) {
    nsHTMLReflowMetrics kidSize(pKidMaxElementSize);
    kidSize.width=kidSize.height=kidSize.ascent=kidSize.descent=0;
    nsReflowStatus    status;

    // Get the next child
    nsIFrame* kidFrame = nextInFlow->mFirstChild;

    // Any more child frames?
    if (nsnull == kidFrame) {
      // No. Any frames on its overflow list?
      if (nsnull != nextInFlow->mOverflowList) {
        // Move the overflow list to become the child list
//        NS_ABORT();
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
      break;
    }
    nsHTMLReflowState kidReflowState(*aPresContext, kidFrame,
                                     aState.reflowState, aState.availSize,
                                     eReflowReason_Resize);

    ReflowChild(kidFrame, *aPresContext, kidSize, kidReflowState, status);

    // Did the child fit?
    if ((kidSize.height > aState.availSize.height) && (nsnull != mFirstChild)) {
      // The child is too wide to fit in the available space, and it's
      // not our first child
      result = PR_FALSE;
      break;
    }

    // Place the child
    //aState.y += topMargin;
    nsRect kidRect (0, 0, kidSize.width, kidSize.height);
    //kidRect.x += kidMol->margin.left;
    kidRect.y += aState.y;
    PlaceChild(aPresContext, aState, kidFrame, kidRect, aMaxElementSize, *pKidMaxElementSize);

    // Remove the frame from its current parent
    kidFrame->GetNextSibling(nextInFlow->mFirstChild);

    // Link the frame into our list of children
    kidFrame->SetGeometricParent(this);
    nsIFrame* kidContentParent;

    kidFrame->GetContentParent(kidContentParent);
    if (nextInFlow == kidContentParent) {
      kidFrame->SetContentParent(this);
    }
    if (nsnull == prevKidFrame) {
      mFirstChild = kidFrame;
    } else {
      prevKidFrame->SetNextSibling(kidFrame);
    }
    kidFrame->SetNextSibling(nsnull);

    // Remember where we just were in case we end up pushing children
    prevKidFrame = kidFrame;

    // Is the child we just pulled up complete?
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
        kidFrame->CreateContinuingFrame(*aPresContext, this, kidSC,
                                        continuingFrame);
        NS_RELEASE(kidSC);
        NS_ASSERTION(nsnull != continuingFrame, "frame creation failed");

        // Add the continuing frame to our sibling list and then push
        // it to the next-in-flow. This ensures the next-in-flow's
        // content offsets and child count are set properly. Note that
        // we can safely assume that the continuation is complete so
        // we pass PR_TRUE into PushChidren
        kidFrame->SetNextSibling(continuingFrame);

        PushChildren(continuingFrame, kidFrame);
      }

      // If the child isn't complete then it means that we've used up
      // all of our available space.
      result = PR_FALSE;
      break;
    }
  }

  return result;
}

/**
  */
void nsTableRowGroupFrame::ShrinkWrapChildren(nsIPresContext* aPresContext, 
                                              nsHTMLReflowMetrics& aDesiredSize)
{
  // iterate children and for each row get its height
  PRBool atLeastOneRowSpanningCell = PR_FALSE;
  nscoord topInnerMargin = 0;
  nscoord bottomInnerMargin = 0;
  PRInt32 numRows;
  GetRowCount(numRows);
  PRInt32 *rowHeights = new PRInt32[numRows];
  nsCRT::memset (rowHeights, 0, numRows*sizeof(PRInt32));

  /* Step 1:  get the height of the tallest cell in the row and save it for
   *          pass 2
   */
  nsIFrame* rowFrame = mFirstChild;
  PRInt32 rowIndex = 0;
  while (nsnull != rowFrame)
  {
    const nsStyleDisplay *childDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      // get the height of the tallest cell in the row (excluding cells that span rows)
      nscoord maxCellHeight       = ((nsTableRowFrame*)rowFrame)->GetTallestChild();
      nscoord maxCellTopMargin    = ((nsTableRowFrame*)rowFrame)->GetChildMaxTopMargin();
      nscoord maxCellBottomMargin = ((nsTableRowFrame*)rowFrame)->GetChildMaxBottomMargin();
      nscoord maxRowHeight = maxCellHeight + maxCellTopMargin + maxCellBottomMargin;

      // save the row height for pass 2 below
      rowHeights[rowIndex] = maxRowHeight;

      // Update top and bottom inner margin if applicable
      if (0 == rowIndex) {
        topInnerMargin = maxCellTopMargin;
      }
      if ((rowIndex + 1) == numRows) {
        bottomInnerMargin = maxCellBottomMargin;
      }
      rowIndex++;
    }
    // Get the next row
    rowFrame->GetNextSibling(rowFrame);
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
  PRInt32 rowGroupHeight = 0;
  rowFrame = mFirstChild;
  rowIndex = 0;
  while (nsnull != rowFrame)
  {
    const nsStyleDisplay *childDisplay;
    rowFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      // check this row for a cell with rowspans
      nsIFrame *cellFrame;
      rowFrame->FirstChild(cellFrame);
      while (nsnull != cellFrame)
      {
        const nsStyleDisplay *childDisplay;
        cellFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
        if (NS_STYLE_DISPLAY_TABLE_CELL == childDisplay->mDisplay)
        {
          PRInt32 rowSpan = ((nsTableFrame*)mGeometricParent)->
                                GetEffectiveRowSpan(rowIndex,(nsTableCellFrame*)cellFrame);
          if (rowSpan > 1)
          { // found a cell with rowspan > 1, determine its height
            nscoord heightOfRowsSpanned = 0;
            PRInt32 i;
            for ( i = 0; i < rowSpan; i++)
              heightOfRowsSpanned += rowHeights[rowIndex + i];
        
            heightOfRowsSpanned -= topInnerMargin + bottomInnerMargin;

            /* if the cell height fits in the rows, expand the spanning cell's height and slap it in */
            nsSize  cellFrameSize;
            cellFrame->GetSize(cellFrameSize);
            if (heightOfRowsSpanned > cellFrameSize.height)
            {
              cellFrame->SizeTo(cellFrameSize.width, heightOfRowsSpanned);
              // Realign cell content based on new height
              ((nsTableCellFrame*)cellFrame)->VerticallyAlignChild(aPresContext);
            }
            /* otherwise, distribute the excess height to the rows effected.
             * push all subsequent rows down by the total change in height of all the rows above it
             */
            else
            {
              PRInt32 excessHeight = cellFrameSize.height - heightOfRowsSpanned;
              PRInt32 excessHeightPerRow = excessHeight/rowSpan;

              // for every row starting at the row with the spanning cell...
              nsTableRowFrame *rowFrameToBeResized = (nsTableRowFrame *)rowFrame;
              for (i = rowIndex; i < numRows; i++)
              {
                // if the row is within the spanned range, resize the row
                if (i < (rowIndex + rowSpan))
                {
                  // update the row height
                  rowHeights[i] += excessHeightPerRow;

                  // adjust the height of the row
                  nsSize  rowFrameSize;
                  rowFrameToBeResized->GetSize(rowFrameSize);
                  rowFrameToBeResized->SizeTo(rowFrameSize.width, rowHeights[i]);
                }

                // if we're dealing with a row below the row containing the spanning cell, 
                // push that row down by the amount we've expanded the cell heights by
                if ((i >= rowIndex) && (i != 0))
                {
                  nsRect rowRect;
               
                  rowFrameToBeResized->GetRect(rowRect);
                  nscoord delta = excessHeightPerRow * (i - rowIndex);
                  if (delta > excessHeight)
                    delta = excessHeight;
                  rowFrameToBeResized->MoveTo(rowRect.x, rowRect.y + delta);
                }

                // Get the next row frame
                rowFrameToBeResized->GetNextSibling((nsIFrame*&)rowFrameToBeResized);
              }
            }
          }
        }
        // Get the next row child (cell frame)
        cellFrame->GetNextSibling(cellFrame);
      }
      // Update the running row group height
      rowGroupHeight += rowHeights[rowIndex];
      rowIndex++;
    }
    // Get the next rowgroup child (row frame)
    rowFrame->GetNextSibling(rowFrame);
  }

  // Adjust our desired size
  aDesiredSize.height = rowGroupHeight;

  // cleanup
  delete []rowHeights;
}

nsresult nsTableRowGroupFrame::AdjustSiblingsAfterReflow(nsIPresContext&      aPresContext,
                                                         RowGroupReflowState& aState,
                                                         nsIFrame*            aKidFrame,
                                                         nscoord              aDeltaY)
{
  nsIFrame* lastKidFrame = aKidFrame;

  if (aDeltaY != 0) {
    // Move the frames that follow aKidFrame by aDeltaY
    nsIFrame* kidFrame;

    aKidFrame->GetNextSibling(kidFrame);
    while (nsnull != kidFrame) {
      nsPoint origin;
  
      // XXX We can't just slide the child if it has a next-in-flow
      kidFrame->GetOrigin(origin);
      origin.y += aDeltaY;
  
      // XXX We need to send move notifications to the frame...
      nsIHTMLReflow* htmlReflow;
      if (NS_OK == kidFrame->QueryInterface(kIHTMLReflowIID, (void**)&htmlReflow)) {
        htmlReflow->WillReflow(aPresContext);
      }
      kidFrame->MoveTo(origin.x, origin.y);

      // Get the next frame
      lastKidFrame = kidFrame;
      kidFrame->GetNextSibling(kidFrame);
    }

  } else {
    // Get the last frame
    lastKidFrame = LastFrame(mFirstChild);
  }

  // XXX Deal with cells that have rowspans.

  // Update our running y-offset to reflect the bottommost child
  nsRect  rect;
  lastKidFrame->GetRect(rect);
  aState.y = rect.YMost();

  return NS_OK;
}

/** Layout the entire row group.
  * This method stacks rows vertically according to HTML 4.0 rules.
  * Rows are responsible for layout of their children.
  */
NS_METHOD
nsTableRowGroupFrame::Reflow(nsIPresContext&          aPresContext,
                             nsHTMLReflowMetrics&     aDesiredSize,
                             const nsHTMLReflowState& aReflowState,
                             nsReflowStatus&          aStatus)
{
  nsresult rv=NS_OK;
  if (gsDebug==PR_TRUE)
    printf("nsTableRowGroupFrame::Reflow - aMaxSize = %d, %d\n",
            aReflowState.maxSize.width, aReflowState.maxSize.height);

  // Initialize out parameter
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  RowGroupReflowState state(aPresContext, aReflowState);

  if (eReflowReason_Incremental == aReflowState.reason) {
    rv = IncrementalReflow(aPresContext, aDesiredSize, aReflowState, aStatus);
  } else {
    PRBool reflowMappedOK = PR_TRUE;
  
    aStatus = NS_FRAME_COMPLETE;
  
    // Check for an overflow list
    MoveOverflowToChildList();
  
    // Reflow the existing frames
    if (nsnull != mFirstChild) {
      reflowMappedOK = ReflowMappedChildren(&aPresContext, state, aDesiredSize.maxElementSize);
      if (PR_FALSE == reflowMappedOK) {
        aStatus = NS_FRAME_NOT_COMPLETE;
      }
    }
  
    // Did we successfully reflow our mapped children?
    if (PR_TRUE == reflowMappedOK) {
      // Any space left?
      PRInt32 numKids;
      mContent->ChildCount(numKids);
      if (state.availSize.height > 0) {
        // Try and pull-up some children from a next-in-flow
        if (!PullUpChildren(&aPresContext, state, aDesiredSize.maxElementSize)) {
          // We were unable to pull-up all the existing frames from the
          // next in flow
          aStatus = NS_FRAME_NOT_COMPLETE;
        }
      }
    }
  
    if (NS_FRAME_IS_COMPLETE(aStatus)) {
      // Don't forget to add in the bottom margin from our last child.
      // Only add it in if there's room for it.
      nscoord margin = state.prevMaxPosBottomMargin -
        state.prevMaxNegBottomMargin;
      if (state.availSize.height >= margin) {
        state.y += margin;
      }
    }
  
    // Return our desired rect
    //NS_ASSERTION(0<state.firstRowHeight, "illegal firstRowHeight after reflow");
    //NS_ASSERTION(0<state.y, "illegal height after reflow");
    aDesiredSize.width = aReflowState.maxSize.width;
    aDesiredSize.height = state.y;

    // shrink wrap rows to height of tallest cell in that row
    ShrinkWrapChildren(&aPresContext, aDesiredSize);
  }

  if (gsDebug==PR_TRUE) 
  {
    if (nsnull!=aDesiredSize.maxElementSize)
      printf("nsTableRowGroupFrame %p returning: %s with aDesiredSize=%d,%d, aMES=%d,%d\n",
              this, NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
              aDesiredSize.width, aDesiredSize.height,
              aDesiredSize.maxElementSize->width, aDesiredSize.maxElementSize->height);
    else
      printf("nsTableRowGroupFrame %p returning: %s with aDesiredSize=%d,%d, aMES=NSNULL\n", 
             this, NS_FRAME_IS_COMPLETE(aStatus)?"Complete":"Not Complete",
             aDesiredSize.width, aDesiredSize.height);
  }

  return rv;

}


NS_METHOD nsTableRowGroupFrame::IncrementalReflow(nsIPresContext& aPresContext,
                                                  nsHTMLReflowMetrics& aDesiredSize,
                                                  const nsHTMLReflowState& aReflowState,
                                                  nsReflowStatus& aStatus)
{
  if (PR_TRUE==gsDebugIR) printf("\nTRGF IR: IncrementalReflow\n");
  nsresult  rv = NS_OK;

  // create an inner table reflow state
  RowGroupReflowState state(aPresContext, aReflowState);

  // determine if this frame is the target or not
  nsIFrame *target=nsnull;
  rv = aReflowState.reflowCommand->GetTarget(target);
  if ((PR_TRUE==NS_SUCCEEDED(rv)) && (nsnull!=target))
  {
    if (this==target)
      rv = IR_TargetIsMe(aPresContext, aDesiredSize, state, aStatus);
    else
    {
      // Get the next frame in the reflow chain
      nsIFrame* nextFrame;
      aReflowState.reflowCommand->GetNext(nextFrame);

      // Recover our reflow state
      //RecoverState(state, nextFrame);
      rv = IR_TargetIsChild(aPresContext, aDesiredSize, state, aStatus, nextFrame);
    }
  }
  return rv;
}

NS_METHOD nsTableRowGroupFrame::IR_TargetIsMe(nsIPresContext&      aPresContext,
                                              nsHTMLReflowMetrics& aDesiredSize,
                                              RowGroupReflowState& aReflowState,
                                              nsReflowStatus&      aStatus)
{
  nsresult rv = NS_FRAME_COMPLETE;
  nsIReflowCommand::ReflowType type;
  aReflowState.reflowState.reflowCommand->GetType(type);
  nsIFrame *objectFrame;
  aReflowState.reflowState.reflowCommand->GetChildFrame(objectFrame); 
  const nsStyleDisplay *childDisplay;
  objectFrame->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
  if (PR_TRUE==gsDebugIR) printf("TRGF IR: IncrementalReflow_TargetIsMe with type=%d\n", type);
  switch (type)
  {
  case nsIReflowCommand::FrameInserted :
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      rv = IR_RowInserted(aPresContext, aDesiredSize, aReflowState, aStatus, 
                          objectFrame, PR_FALSE);
    }
    else
    {
      rv = IR_UnknownFrameInserted(aPresContext, aDesiredSize, aReflowState, aStatus, 
                                   objectFrame, PR_FALSE);
    }
    break;
  
  case nsIReflowCommand::FrameAppended :
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      rv = IR_RowAppended(aPresContext, aDesiredSize, aReflowState, aStatus, objectFrame);
    }
    else
    { // no optimization to be done for Unknown frame types, so just reuse the Inserted method
      rv = IR_UnknownFrameInserted(aPresContext, aDesiredSize, aReflowState, aStatus, 
                                   objectFrame, PR_FALSE);
    }
    break;

  /*
  case nsIReflowCommand::FrameReplaced :

  */

  case nsIReflowCommand::FrameRemoved :
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
    {
      rv = IR_RowRemoved(aPresContext, aDesiredSize, aReflowState, aStatus, 
                         objectFrame);
    }
    else
    {
      rv = IR_UnknownFrameRemoved(aPresContext, aDesiredSize, aReflowState, aStatus, 
                                  objectFrame);
    }
    break;

  case nsIReflowCommand::StyleChanged :
    NS_NOTYETIMPLEMENTED("unimplemented reflow command type");
    rv = NS_ERROR_NOT_IMPLEMENTED;
    if (PR_TRUE==gsDebugIR) printf("TRGF IR: StyleChanged not implemented.\n");
    break;

  case nsIReflowCommand::ContentChanged :
    NS_ASSERTION(PR_FALSE, "illegal reflow type: ContentChanged");
    rv = NS_ERROR_ILLEGAL_VALUE;
    break;
  
  case nsIReflowCommand::PullupReflow:
  case nsIReflowCommand::PushReflow:
  case nsIReflowCommand::CheckPullupReflow :
  case nsIReflowCommand::UserDefined :
    NS_NOTYETIMPLEMENTED("unimplemented reflow command type");
    rv = NS_ERROR_NOT_IMPLEMENTED;
    if (PR_TRUE==gsDebugIR) printf("TRGF IR: reflow command not implemented.\n");
    break;
  }

  return rv;
}

NS_METHOD nsTableRowGroupFrame::IR_RowInserted(nsIPresContext&      aPresContext,
                                               nsHTMLReflowMetrics& aDesiredSize,
                                               RowGroupReflowState& aReflowState,
                                               nsReflowStatus&      aStatus,
                                               nsIFrame *           aInsertedFrame,
                                               PRBool               aReplace)
{
  nsresult rv=NS_OK;
  // inserting the rowgroup only effects reflow if the rowgroup includes at least one row
  return rv;
}

NS_METHOD nsTableRowGroupFrame::IR_DidAppendRow(nsTableRowFrame *aRowFrame)
{
  nsresult rv=NS_OK;
  /* need to make space in the cell map.  Remeber that row spans can't cross row groups
     once the space is made, tell the row to initizalize its children.  
     it will automatically find the row to initialize into.
     but this is tough because a cell in aInsertedFrame could have a rowspan
     which must be respected if a subsequent row is appended.
  */
  rv = aRowFrame->InitChildren();
  return rv;
}

// since we know we're doing an append here, we can optimize
NS_METHOD nsTableRowGroupFrame::IR_RowAppended(nsIPresContext&      aPresContext,
                                               nsHTMLReflowMetrics& aDesiredSize,
                                               RowGroupReflowState& aReflowState,
                                               nsReflowStatus&      aStatus,
                                               nsIFrame *           aAppendedFrame)
{
  nsresult rv=NS_OK;
  // hook aAppendedFrame into the child list
  nsIFrame *lastChild = mFirstChild;
  nsIFrame *nextChild = lastChild;
  nsIFrame *lastRow = nsnull;
  while (nsnull!=nextChild)
  {
    // remember the last child that is really a row
    const nsStyleDisplay *childDisplay;
    nextChild->GetStyleData(eStyleStruct_Display, ((nsStyleStruct *&)childDisplay));
    if (NS_STYLE_DISPLAY_TABLE_ROW == childDisplay->mDisplay)
      lastRow = nextChild;
    lastChild = nextChild;
    nextChild->GetNextSibling(nextChild);
  }
  if (nsnull==lastChild)
    mFirstChild = aAppendedFrame;
  else
    lastChild->SetNextSibling(aAppendedFrame);

  nsTableFrame *tableFrame=nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if (NS_FAILED(rv) || nsnull==tableFrame)
    return rv;
  tableFrame->InvalidateFirstPassCache();
  // the table will see that it's cached info is bogus and rebuild the cell map,
  // and do a reflow

#if 0
  // find the row index of the new row
  PRInt32 newRowIndex=-1;
  if (nsnull!=

  lastChild=mFirstChild;
  nextChild=lastChild;
  while (nsnull!=nextChild)
  {
  }
  // account for the cells in aAppendedFrame
  nsresult rv = DidAppendRow((nsTableRowFrame*)aAppendedFrame, newRowIndex);

  // need to increment the row index of all subsequent rows
#endif 

  return rv;
}

NS_METHOD nsTableRowGroupFrame::IR_RowRemoved(nsIPresContext&      aPresContext,
                                              nsHTMLReflowMetrics& aDesiredSize,
                                              RowGroupReflowState& aReflowState,
                                              nsReflowStatus&      aStatus,
                                              nsIFrame *           aDeletedFrame)
{
  nsresult rv;
  return rv;
}

//XXX: handle aReplace
NS_METHOD nsTableRowGroupFrame::IR_UnknownFrameInserted(nsIPresContext&      aPresContext,
                                                        nsHTMLReflowMetrics& aDesiredSize,
                                                        RowGroupReflowState& aReflowState,
                                                        nsReflowStatus&      aStatus,
                                                        nsIFrame *           aInsertedFrame,
                                                        PRBool               aReplace)
{
  nsIReflowCommand::ReflowType type;
  aReflowState.reflowState.reflowCommand->GetType(type);
  // we have a generic frame that gets inserted but doesn't effect reflow
  // hook it up then ignore it
  if (nsIReflowCommand::FrameAppended==type)
  { // frameAppended reflow -- find the last child and make aInsertedFrame its next sibling
    if (PR_TRUE==gsDebugIR) printf("TRGF IR: FrameAppended adding unknown frame type.\n");
    nsIFrame *lastChild=mFirstChild;
    nsIFrame *nextChild=mFirstChild;
    while (nsnull!=nextChild)
    {
      lastChild=nextChild;
      nextChild->GetNextSibling(nextChild);
    }
    if (nsnull==lastChild)
      mFirstChild = aInsertedFrame;
    else
      lastChild->SetNextSibling(aInsertedFrame);
  }
  else
  { // frameInserted reflow -- hook up aInsertedFrame as prevSibling's next sibling, 
    // and be sure to hook in aInsertedFrame's nextSibling (from prevSibling)
    if (PR_TRUE==gsDebugIR) printf("TRGF IR: FrameInserted adding unknown frame type.\n");
    nsIFrame *prevSibling=nsnull;
    nsresult rv = aReflowState.reflowState.reflowCommand->GetPrevSiblingFrame(prevSibling);
    if (NS_SUCCEEDED(rv) && (nsnull!=prevSibling))
    {
      nsIFrame *nextSibling=nsnull;
      prevSibling->GetNextSibling(nextSibling);
      prevSibling->SetNextSibling(aInsertedFrame);
      aInsertedFrame->SetNextSibling(nextSibling);
    }
    else
    {
      nsIFrame *nextSibling=nsnull;
      if (nsnull!=mFirstChild)
        mFirstChild->GetNextSibling(nextSibling);
      mFirstChild = aInsertedFrame;
      aInsertedFrame->SetNextSibling(nextSibling);
    }
  }
  return NS_FRAME_COMPLETE;
}

NS_METHOD nsTableRowGroupFrame::IR_UnknownFrameRemoved(nsIPresContext&      aPresContext,
                                                       nsHTMLReflowMetrics& aDesiredSize,
                                                       RowGroupReflowState& aReflowState,
                                                       nsReflowStatus&      aStatus,
                                                       nsIFrame *           aRemovedFrame)
{
  // we have a generic frame that gets removed but doesn't effect reflow
  // unhook it then ignore it
  if (PR_TRUE==gsDebugIR) printf("TRGF IR: FrameRemoved removing unknown frame type.\n");
  nsIFrame *prevChild=nsnull;
  nsIFrame *nextChild=mFirstChild;
  while (nextChild!=aRemovedFrame)
  {
    prevChild=nextChild;
    nextChild->GetNextSibling(nextChild);
  }  
  nextChild=nsnull;
  aRemovedFrame->GetNextSibling(nextChild);
  if (nsnull==prevChild)  // objectFrame was first child
    mFirstChild = nextChild;
  else
    prevChild->SetNextSibling(nextChild);
  return NS_FRAME_COMPLETE;
}

NS_METHOD nsTableRowGroupFrame::IR_TargetIsChild(nsIPresContext&      aPresContext,
                                                 nsHTMLReflowMetrics& aDesiredSize,
                                                 RowGroupReflowState& aReflowState,
                                                 nsReflowStatus&      aStatus,
                                                 nsIFrame *           aNextFrame)

{
  nsresult rv;
  if (PR_TRUE==gsDebugIR) printf("\nTRGF IR: IR_TargetIsChild\n");
  // XXX Recover state

  // Remember the old rect
  nsRect  oldKidRect;
  aNextFrame->GetRect(oldKidRect);

  // Pass along the reflow command
  // XXX Correctly compute the available space...
  nsHTMLReflowState   kidReflowState(aPresContext, aNextFrame, aReflowState.reflowState, aReflowState.reflowState.maxSize);
  nsHTMLReflowMetrics desiredSize(nsnull);

  rv = ReflowChild(aNextFrame, aPresContext, desiredSize, kidReflowState, aStatus);

  // Resize the row frame
  nsRect  kidRect;
  aNextFrame->GetRect(kidRect);
  aNextFrame->SizeTo(desiredSize.width, desiredSize.height);

  // Adjust the frames that follow...
  AdjustSiblingsAfterReflow(aPresContext, aReflowState, aNextFrame, desiredSize.height -
                            oldKidRect.height);

  // Return of desired size
  aDesiredSize.width = aReflowState.reflowState.maxSize.width;
  aDesiredSize.height = aReflowState.y;

  return rv;
}

NS_METHOD
nsTableRowGroupFrame::CreateContinuingFrame(nsIPresContext&  aPresContext,
                                            nsIFrame*        aParent,
                                            nsIStyleContext* aStyleContext,
                                            nsIFrame*&       aContinuingFrame)
{
  nsTableRowGroupFrame* cf = new nsTableRowGroupFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aPresContext, aParent, aStyleContext, cf);
  if (PR_TRUE==gsDebug) printf("nsTableRowGroupFrame::CCF parent = %p, this=%p, cf=%p\n", aParent, this, cf);
  aContinuingFrame = cf;
  return NS_OK;
}

/* ----- global methods ----- */

nsresult 
NS_NewTableRowGroupFrame(nsIContent* aContent,
                         nsIFrame*   aParentFrame,
                         nsIFrame*&  aResult)
{
  nsIFrame* it = new nsTableRowGroupFrame(aContent, aParentFrame);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = it;
  return NS_OK;
}

/* ----- debugging methods ----- */
NS_METHOD nsTableRowGroupFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const
{
  // if a filter is present, only output this frame if the filter says we should
  // since this could be any "tag" with the right display type, we'll
  // just pretend it's a tbody
  if (nsnull==aFilter)
    return nsContainerFrame::List(out, aIndent, aFilter);

  nsAutoString tagString("tbody");
  PRBool outputMe = aFilter->OutputTag(&tagString);
  if (PR_TRUE==outputMe)
  {
    // Indent
    for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);

    // Output the tag and rect
    nsIAtom* tag;
    mContent->GetTag(tag);
    if (tag != nsnull) {
      nsAutoString buf;
      tag->ToString(buf);
      fputs(buf, out);
      NS_RELEASE(tag);
    }

    fprintf(out, "(%d)", ContentIndexInContainer(this));
    out << mRect;
    if (0 != mState) {
      fprintf(out, " [state=%08x]", mState);
    }
    fputs("\n", out);
  }

  // Output the children
  if (nsnull != mFirstChild) {
    if (PR_TRUE==outputMe)
    {
      if (0 != mState) {
        fprintf(out, " [state=%08x]\n", mState);
      }
    }
    for (nsIFrame* child = mFirstChild; child; child->GetNextSibling(child)) {
      child->List(out, aIndent + 1, aFilter);
    }
  } else {
    if (PR_TRUE==outputMe)
    {
      if (0 != mState) {
        fprintf(out, " [state=%08x]\n", mState);
      }
    }
  }


  return NS_OK;
}






