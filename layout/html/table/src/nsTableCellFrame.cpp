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
#include "nsTableCellFrame.h"
#include "nsTableColFrame.h"
#include "nsTableFrame.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLParts.h"
#include "nsHTMLValue.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsVoidArray.h"
#include "nsIPtr.h"
#include "nsIView.h"
#include "nsStyleUtil.h"
#include "nsLayoutAtoms.h"

NS_DEF_PTR(nsIStyleContext);


#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsDebugNT = PR_FALSE;
//#define   NOISY_STYLE
//#define NOISY_FLOW
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsDebugNT = PR_FALSE;
#endif

NS_IMETHODIMP
nsTableCellFrame::Init(nsIPresContext&  aPresContext,
                       nsIContent*      aContent,
                       nsIFrame*        aParent,
                       nsIStyleContext* aContext,
                       nsIFrame*        aPrevInFlow)
{
  nsresult  rv;
  
  // Let the base class do its initialization
  rv = nsHTMLContainerFrame::Init(aPresContext, aContent, aParent, aContext,
                                  aPrevInFlow);

  if (aPrevInFlow) {
    // Set the column index
    nsTableCellFrame* cellFrame = (nsTableCellFrame*)aPrevInFlow;
    PRInt32           baseColIndex;
    
    cellFrame->GetColIndex(baseColIndex);
    InitCellFrame(baseColIndex);
  }

  return rv;
}

void nsTableCellFrame::InitCellFrame(PRInt32 aColIndex)
{
  NS_PRECONDITION(0<=aColIndex, "bad col index arg");
  mColIndex = aColIndex;
  mBorderEdges.mOutsideEdge=PR_FALSE;
  nsTableFrame* tableFrame=nsnull;  // I should be checking my own style context, but border-collapse isn't inheriting correctly
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if ((NS_SUCCEEDED(rv)) && (nsnull!=tableFrame))
  {
    const nsStyleTable* tableStyle;
    tableFrame->GetStyleData(eStyleStruct_Table, ((const nsStyleStruct *&)tableStyle)); 
    if (NS_STYLE_BORDER_COLLAPSE==tableStyle->mBorderCollapse)
    {
      PRInt32 rowspan = GetRowSpan();
      PRInt32 i;
      for (i=0; i<rowspan; i++)
      {
        nsBorderEdge *borderToAdd = new nsBorderEdge();
        mBorderEdges.mEdges[NS_SIDE_LEFT].AppendElement(borderToAdd);
        borderToAdd = new nsBorderEdge();
        mBorderEdges.mEdges[NS_SIDE_RIGHT].AppendElement(borderToAdd);
      }
      PRInt32 colspan = GetColSpan();
      for (i=0; i<colspan; i++)
      {
        nsBorderEdge *borderToAdd = new nsBorderEdge();
        mBorderEdges.mEdges[NS_SIDE_TOP].AppendElement(borderToAdd);
        borderToAdd = new nsBorderEdge();
        mBorderEdges.mEdges[NS_SIDE_BOTTOM].AppendElement(borderToAdd);
      }
    }
    mCollapseOffset = nsPoint(0,0);
  }
}

void nsTableCellFrame::SetBorderEdgeLength(PRUint8 aSide, 
                                           PRInt32 aIndex, 
                                           nscoord aLength)
{
  if ((NS_SIDE_LEFT==aSide) || (NS_SIDE_RIGHT==aSide))
  {
    PRInt32 baseRowIndex;
    GetRowIndex(baseRowIndex);
    PRInt32 rowIndex = aIndex-baseRowIndex;
    nsBorderEdge *border = (nsBorderEdge *)(mBorderEdges.mEdges[aSide].ElementAt(rowIndex));
    border->mLength = aLength;
  }
  else {
    NS_ASSERTION(PR_FALSE, "bad arg aSide passed to SetBorderEdgeLength");
  }
}


NS_METHOD nsTableCellFrame::Paint(nsIPresContext& aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  const nsRect& aDirtyRect,
                                  nsFramePaintLayer aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    const nsStyleDisplay* disp =
      (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

    if (disp->mVisible) {
      const nsStyleColor* myColor =
        (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
      const nsStyleSpacing* mySpacing =
        (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
      NS_ASSERTION(nsnull!=myColor, "bad style color");
      NS_ASSERTION(nsnull!=mySpacing, "bad style spacing");

      const nsStyleTable* cellTableStyle;
      GetStyleData(eStyleStruct_Table, ((const nsStyleStruct *&)cellTableStyle)); 
      nsRect  rect(0, 0, mRect.width, mRect.height);

      nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *myColor, *mySpacing, 0, 0);
    
      // empty cells do not render their border
      PRBool renderBorder = PR_TRUE;
      if (PR_TRUE==GetContentEmpty())
      {
        if (NS_STYLE_TABLE_EMPTY_CELLS_HIDE==cellTableStyle->mEmptyCells)
          renderBorder=PR_FALSE;
      }
      if (PR_TRUE==renderBorder)
      {
        PRIntn skipSides = GetSkipSides();
        nsTableFrame* tableFrame=nsnull;  // I should be checking my own style context, but border-collapse isn't inheriting correctly
        nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
        if ((NS_SUCCEEDED(rv)) && (nsnull!=tableFrame))
        {
          const nsStyleTable* tableStyle;
          tableFrame->GetStyleData(eStyleStruct_Table, ((const nsStyleStruct *&)tableStyle)); 
          if (NS_STYLE_BORDER_SEPARATE==tableStyle->mBorderCollapse)
          {
            nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *mySpacing, mStyleContext, skipSides);
          }
          else
          {
            nsCSSRendering::PaintBorderEdges(aPresContext, aRenderingContext, this,
                                             aDirtyRect, rect, &mBorderEdges, mStyleContext, skipSides);
          }
        }
      }
    }
  }

  // for debug...
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0, 0, 128));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }


  // if the cell originates in a row and/or col that is collapsed, the bottom and/or
  // right portion of the cell is painted by translating the rendering context.
  // XXX What about content that can leak outside the cell?
  PRBool clipState;
  aRenderingContext.PushState();
  nsPoint offset = mCollapseOffset;
  if ((0 != offset.x) || (0 != offset.y)) {
    aRenderingContext.Translate(offset.x, offset.y);
  }
  aRenderingContext.SetClipRect(nsRect(-offset.x, -offset.y, mRect.width, mRect.height),
                                nsClipCombine_kIntersect, clipState);
  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
  aRenderingContext.PopState(clipState);
  
  return NS_OK;
}

PRIntn
nsTableCellFrame::GetSkipSides() const
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

void nsTableCellFrame::SetBorderEdge(PRUint8       aSide, 
                                     PRInt32       aRowIndex, 
                                     PRInt32       aColIndex, 
                                     nsBorderEdge *aBorder,
                                     nscoord       aOddAmountToAdd)
{
  nsBorderEdge *border = nsnull;
  switch (aSide)
  {
    case NS_SIDE_TOP:
    {
      PRInt32 baseColIndex;
      GetColIndex(baseColIndex);
      PRInt32 colIndex = aColIndex-baseColIndex;
      border = (nsBorderEdge *)(mBorderEdges.mEdges[aSide].ElementAt(colIndex));
      mBorderEdges.mMaxBorderWidth.top = PR_MAX(aBorder->mWidth+aOddAmountToAdd, mBorderEdges.mMaxBorderWidth.top);
      break;
    }

    case NS_SIDE_BOTTOM:
    {
      PRInt32 baseColIndex;
      GetColIndex(baseColIndex);
      PRInt32 colIndex = aColIndex-baseColIndex;
      border = (nsBorderEdge *)(mBorderEdges.mEdges[aSide].ElementAt(colIndex));
      mBorderEdges.mMaxBorderWidth.bottom = PR_MAX(aBorder->mWidth+aOddAmountToAdd, mBorderEdges.mMaxBorderWidth.bottom);
      break;
    }
  
    case NS_SIDE_LEFT:
    {
      PRInt32 baseRowIndex;
      GetRowIndex(baseRowIndex);
      PRInt32 rowIndex = aRowIndex-baseRowIndex;
      border = (nsBorderEdge *)(mBorderEdges.mEdges[aSide].ElementAt(rowIndex));
      mBorderEdges.mMaxBorderWidth.left = PR_MAX(aBorder->mWidth+aOddAmountToAdd, mBorderEdges.mMaxBorderWidth.left);
      break;
    }
      
    case NS_SIDE_RIGHT:  
    {
      PRInt32 baseRowIndex;
      GetRowIndex(baseRowIndex);
      PRInt32 rowIndex = aRowIndex-baseRowIndex;
      border = (nsBorderEdge *)(mBorderEdges.mEdges[aSide].ElementAt(rowIndex));
      mBorderEdges.mMaxBorderWidth.right = PR_MAX(aBorder->mWidth+aOddAmountToAdd, mBorderEdges.mMaxBorderWidth.right);
      break;
    }
  }
  if (nsnull!=border) {
    *border=*aBorder;
    border->mWidth += aOddAmountToAdd;
  }
  else {
    NS_ASSERTION(PR_FALSE, "bad border edge state");
  }
}

/**
  *
  * Align the cell's child frame within the cell
  *
  */
void  nsTableCellFrame::VerticallyAlignChild()
{
  const nsStyleSpacing* spacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  const nsStyleText* textStyle =
      (const nsStyleText*)mStyleContext->GetStyleData(eStyleStruct_Text);
  /* XXX: remove tableFrame when border-collapse inherits */
  nsTableFrame* tableFrame=nsnull;
  nsresult rv = nsTableFrame::GetTableFrame(this, tableFrame);
  nsMargin borderPadding;
  GetCellBorder (borderPadding, tableFrame);
  nsMargin padding;
  spacing->GetPadding(padding);
  borderPadding += padding;
  
  nscoord topInset = borderPadding.top;
  nscoord bottomInset = borderPadding.bottom;
  PRUint8 verticalAlignFlags = NS_STYLE_VERTICAL_ALIGN_MIDDLE;
  if (textStyle->mVerticalAlign.GetUnit() == eStyleUnit_Enumerated) {
    verticalAlignFlags = textStyle->mVerticalAlign.GetIntValue();
  }
  nscoord height = mRect.height;
  nsRect kidRect;  
  nsIFrame* firstKid = mFrames.FirstChild();
  firstKid->GetRect(kidRect);
  nscoord childHeight = kidRect.height;
  

  // Vertically align the child
  nscoord kidYTop = 0;
  switch (verticalAlignFlags) 
  {
    case NS_STYLE_VERTICAL_ALIGN_BASELINE:
    // Align the child's baseline at the max baseline
    //kidYTop = aMaxAscent - kidAscent;
    break;

    case NS_STYLE_VERTICAL_ALIGN_TOP:
    // Align the top of the child frame with the top of the box, 
    // minus the top padding
      kidYTop = topInset;
    break;

    case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
      kidYTop = height - childHeight - bottomInset;
    break;

    default:
    case NS_STYLE_VERTICAL_ALIGN_MIDDLE:
      kidYTop = height/2 - childHeight/2;
  }
  firstKid->MoveTo(kidRect.x, kidYTop);
}

PRInt32 nsTableCellFrame::GetRowSpan()
{  
  PRInt32 rowSpan=1;
  nsIHTMLContent *hc=nsnull;
  nsresult rv = mContent->QueryInterface(kIHTMLContentIID, (void**) &hc);
  if (NS_OK==rv)
  {
    nsHTMLValue val;
    hc->GetHTMLAttribute(nsHTMLAtoms::rowspan, val); 
    if (eHTMLUnit_Integer == val.GetUnit()) { 
       rowSpan=val.GetIntValue(); 
    }
    NS_RELEASE(hc);
  }
  return rowSpan;
}

PRInt32 nsTableCellFrame::GetColSpan()
{  
  PRInt32 colSpan=1;
  nsIHTMLContent *hc=nsnull;
  nsresult rv = mContent->QueryInterface(kIHTMLContentIID, (void**) &hc);
  if (NS_OK==rv)
  {
    nsHTMLValue val;
    hc->GetHTMLAttribute(nsHTMLAtoms::colspan, val); 
    if (eHTMLUnit_Integer == val.GetUnit()) { 
       colSpan=val.GetIntValue(); 
    }
    NS_RELEASE(hc);
  }
  return colSpan;
}


void DebugCheckChildSize(nsIFrame* aChild, nsHTMLReflowMetrics& aMet, nsSize& aAvailSize)
{
  if (aMet.width > aAvailSize.width) {
    nsAutoString tmp;
    aChild->GetFrameName(tmp);
    printf("WARNING: cell %s content has desired width %d given avail width %d\n",
            tmp, aMet.width, aAvailSize.width);
  }
  if ((aMet.width < 0) || (aMet.width > 30000)) {
    printf("WARNING: cell content %X has large width %d \n", aChild, aMet.width);
  }
  if ((aMet.height < 0) || (aMet.height > 30000)) {
    printf("WARNING: cell content %X has large height %d \n", aChild, aMet.height);
  }
  if (aMet.maxElementSize) {
    nscoord tmp = aMet.maxElementSize->width;
    if ((tmp < 0) || (tmp > 30000)) {
      printf("WARNING: cell content %X has large max element width %d \n", aChild, tmp);
    }
    tmp = aMet.maxElementSize->height;
    if ((tmp < 0) || (tmp > 30000)) {
      printf("WARNING: cell content %X has large max element height %d \n", aChild, tmp);
    }
  }
}

/**
  */
NS_METHOD nsTableCellFrame::Reflow(nsIPresContext& aPresContext,
                                   nsHTMLReflowMetrics& aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus& aStatus)
{
#ifdef NS_DEBUG
  //PreReflowCheck();
#endif

  nsresult rv = NS_OK;
  // this should probably be cached somewhere
  nsCompatibility compatMode;
  aPresContext.GetCompatibilityMode(&compatMode);

  // Initialize out parameter
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  aStatus = NS_FRAME_COMPLETE;
  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT)
    printf("%p nsTableCellFrame::Reflow: maxSize=%d,%d\n",
           this, aReflowState.availableWidth, aReflowState.availableHeight);

  nsSize availSize(aReflowState.availableWidth, aReflowState.availableHeight);
  nsSize maxElementSize;
  nsSize *pMaxElementSize = aDesiredSize.maxElementSize;
  if (NS_UNCONSTRAINEDSIZE==aReflowState.availableWidth)
    pMaxElementSize = &maxElementSize;
  nscoord x = 0;
  // SEC: what about ascent and decent???

  // Compute the insets (sum of border and padding)
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);

  /* XXX: remove tableFrame when border-collapse inherits */
  nsTableFrame* tableFrame=nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame);
  nsMargin borderPadding;
  spacing->GetPadding(borderPadding);
  nsMargin border;
  GetCellBorder(border, tableFrame);
  if ((NS_UNCONSTRAINEDSIZE == availSize.width) || !GetContentEmpty()) {
    borderPadding += border;
  }
  
  nscoord topInset    = borderPadding.top;
  nscoord rightInset  = borderPadding.right;
  nscoord bottomInset = borderPadding.bottom;
  nscoord leftInset   = borderPadding.left;

  // reduce available space by insets, if we're in a constrained situation
  if (NS_UNCONSTRAINEDSIZE!=availSize.width)
    availSize.width -= leftInset+rightInset;
  if (NS_UNCONSTRAINEDSIZE!=availSize.height)
    availSize.height -= topInset+bottomInset;

  if (eReflowReason_Incremental == aReflowState.reason) 
  {
    // We *must* do this otherwise incremental reflow that's
    // passing through will not work right.
    nsIFrame* next;
    aReflowState.reflowCommand->GetNext(next);

    // if it is a StyleChanged reflow targeted at this cell frame,
    // handle that here
    // first determine if this frame is the target or not
    nsIFrame *target=nsnull;
    rv = aReflowState.reflowCommand->GetTarget(target);
    if ((PR_TRUE==NS_SUCCEEDED(rv)) && (nsnull!=target))
    {
      if (this==target)
      {
        if (gsDebug) { printf("IR target is cell\n"); }
        nsIReflowCommand::ReflowType type;
        aReflowState.reflowCommand->GetType(type);
        if (nsIReflowCommand::StyleChanged==type)
        {
          nsresult rv = IR_StyleChanged(aPresContext, aDesiredSize, aReflowState, aStatus);
          aStatus = NS_FRAME_COMPLETE;
          return rv;
        }
        // BEGIN bug 4577 ---------------------------------------------------------
        // XXX: this code should be replaced with the assertion below
        //      the real fix belongs in the incr. reflow dispatch code
        //      see bug 4577
        else
        {
          nsIFrame *child;
          FirstChild(nsnull, &child);
          NS_ASSERTION(nsnull!=child, "cells can never have 0 children");
          aReflowState.reflowCommand->SetTarget(child);
        }
        // XXX: this else block should replace the one above 
        //      when the incr. relfow dispatch code is changed
        /*
        else {
          NS_ASSERTION(PR_FALSE, "table cell target of illegal incremental reflow type");
        }
        */
        // END bug 4577 -------------------------------------------------------
      }
      else if (gsDebug) { printf("IR target is cell content\n"); }
    }
    // if any of these conditions are not true, we just pass the reflow command down
  }

  // Try to reflow the child into the available space. It might not
  // fit or might need continuing.
  if (availSize.height < 0)
    availSize.height = 1;

  if (gsDebug==PR_TRUE)
    printf("  nsTableCellFrame::Reflow calling ReflowChild with availSize=%d,%d\n",
           availSize.width, availSize.height);
  nsHTMLReflowMetrics kidSize(pMaxElementSize);
  kidSize.width=kidSize.height=kidSize.ascent=kidSize.descent=0;
  SetPriorAvailWidth(aReflowState.availableWidth);
  nsIFrame* firstKid = mFrames.FirstChild();
  nsHTMLReflowState kidReflowState(aPresContext, aReflowState, firstKid,
                                   availSize);

  ReflowChild(firstKid, aPresContext, kidSize, kidReflowState, aStatus);
#ifdef NS_DEBUG
  DebugCheckChildSize(firstKid, kidSize, availSize);
#endif

  // 0 dimensioned cells need to be treated specially in Standard/NavQuirks mode 
  // see testcase "cellHeight.html"
  if (NS_UNCONSTRAINEDSIZE == kidReflowState.availableWidth) {
    if ((0 == kidSize.width) || (0 == kidSize.height)) {
    //if ((0 == kidSize.width) && (0 == kidSize.height)) { // XXX why was this &&
      SetContentEmpty(PR_TRUE);
      // need to reduce the insets by border if the cell is empty
      leftInset   -= border.left;
      rightInset  -= border.right;
      topInset    -= border.top;
      bottomInset -= border.bottom;
    }
    else 
      SetContentEmpty(PR_FALSE);
  }
  if (0 == kidSize.width) {
    if (NS_UNCONSTRAINEDSIZE == kidReflowState.availableWidth) {
      const nsStylePosition* pos;
      GetStyleData(eStyleStruct_Position, ((const nsStyleStruct *&)pos));
      if ((pos->mWidth.GetUnit() != eStyleUnit_Coord)   &&
          (pos->mWidth.GetUnit() != eStyleUnit_Percent)) {
        float p2t;
        aPresContext.GetScaledPixelsToTwips(&p2t);
        PRInt32 pixWidth = (eCompatibility_Standard == compatMode) ? 1 : 3;
        kidSize.width = NSIntPixelsToTwips(pixWidth, p2t);
        if ((nsnull != aDesiredSize.maxElementSize) && (0 == pMaxElementSize->width))
          pMaxElementSize->width = kidSize.width;
        if (eCompatibility_NavQuirks == compatMode) {
          if (gsDebug) printf ("setting initial child width from 0 to %d for nav4 compatibility\n", kidSize.width);
        } 
      }
    }
    else  // empty content has to be forced to the assigned width for resize or incremental reflow
      kidSize.width = kidReflowState.availableWidth;
  }
  if (0 == kidSize.height) {
    const nsStylePosition* pos;
    GetStyleData(eStyleStruct_Position, ((const nsStyleStruct *&)pos));
    if ((pos->mHeight.GetUnit() != eStyleUnit_Coord) &&
        (pos->mHeight.GetUnit() != eStyleUnit_Percent)) {
      float p2t;
      aPresContext.GetScaledPixelsToTwips(&p2t);
      // Standard mode should probably be 0 pixels high instead of 1
      PRInt32 pixHeight = (eCompatibility_Standard == compatMode) ? 1 : 2;
      kidSize.height = NSIntPixelsToTwips(pixHeight, p2t);
      if ((nsnull != aDesiredSize.maxElementSize) && (0 == pMaxElementSize->height))
        pMaxElementSize->height = kidSize.height;
      if (eCompatibility_NavQuirks == compatMode) {
        if (gsDebug) printf ("setting child height from 0 to %d for nav4 compatibility\n", kidSize.height);
      }
    }
  }
  // end 0 dimensioned cells

  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT)
  {
    if (nsnull!=pMaxElementSize)
      printf("  %p cellFrame child returned desiredSize=%d,%d, and maxElementSize=%d,%d\n",
             this, kidSize.width, kidSize.height,
             pMaxElementSize->width, pMaxElementSize->height);
    else
      printf("  %p cellFrame child returned desiredSize=%d,%d, and maxElementSize=nsnull\n",
             this, kidSize.width, kidSize.height);
  }

  // Place the child
  //////////////////////////////// HACK //////////////////////////////
  kidSize.width = PR_MIN(kidSize.width, availSize.width);
  ///////////////////////////// END HACK /////////////////////////////
  firstKid->SetRect(nsRect(leftInset, topInset,
                           kidSize.width, kidSize.height));  
    
  // Return our size and our result

  // first, compute the height which can be set w/o being restricted by aMaxSize.height
  nscoord cellHeight = kidSize.height + topInset + bottomInset;
  if (PR_TRUE==gsDebugNT)
    printf("  %p cellFrame height set to %d from kidSize=%d and insets %d,%d\n",
             this, cellHeight, kidSize.height, topInset, bottomInset);
  // next determine the cell's width
  nscoord cellWidth = kidSize.width;      // at this point, we've factored in the cell's style attributes

  // add the insets into the cell width (in both the constrained-width and unconstrained-width cases
  cellWidth += leftInset + rightInset;    // factor in border and padding

  // set the cell's desired size and max element size
  aDesiredSize.width = cellWidth;
  aDesiredSize.height = cellHeight;
  aDesiredSize.ascent = topInset;
  aDesiredSize.descent = bottomInset;
  if (nsnull!=aDesiredSize.maxElementSize)
  {
    *aDesiredSize.maxElementSize = *pMaxElementSize;
    if (0!=pMaxElementSize->height)
      aDesiredSize.maxElementSize->height += topInset + bottomInset;
    if (0!=pMaxElementSize->width)
      aDesiredSize.maxElementSize->width += leftInset + rightInset;
  }
  // remember my desired size for this reflow
  SetDesiredSize(aDesiredSize);

  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT)
    printf("  %p cellFrame returning aDesiredSize=%d,%d\n",
           this, aDesiredSize.width, aDesiredSize.height);

  aDesiredSize.ascent=aDesiredSize.height;
  aDesiredSize.descent=0;

  // if we got this far with an incremental reflow, the reflow was targeted
  // at the cell's content.  We should only have to redo pass1 for this cell
  // then rebalance columns. The pass1 is handled by the cell's parent row.
  // So here all we have to do is tell the table to rebalance.
  if (eReflowReason_Incremental == aReflowState.reason) 
  {
    nsTableFrame* tableFrame=nsnull;
    rv = nsTableFrame::GetTableFrame(this, tableFrame);
    if ((NS_SUCCEEDED(rv)) && (nsnull!=tableFrame))
    {
      tableFrame->InvalidateColumnWidths();
    }
  }
   

#ifdef NS_DEBUG
  //PostReflowCheck(result);
#endif

  return NS_OK;
}

NS_METHOD nsTableCellFrame::IR_StyleChanged(nsIPresContext&          aPresContext,
                                            nsHTMLReflowMetrics&     aDesiredSize,
                                            const nsHTMLReflowState& aReflowState,
                                            nsReflowStatus&          aStatus)
{
  if (PR_TRUE==gsDebug) printf("Cell IR: IR_StyleChanged for frame %p\n", this);
  nsresult rv = NS_OK;
  // we presume that all the easy optimizations were done in the nsHTMLStyleSheet before we were called here
  // XXX: we can optimize this when we know which style attribute changed
  DidSetStyleContext(&aPresContext);
  nsTableFrame* tableFrame=nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if ((NS_SUCCEEDED(rv)) && (nsnull!=tableFrame))
  {
    tableFrame->InvalidateCellMap();
    tableFrame->InvalidateFirstPassCache();
  }

  return rv;
}

/**
  *
  * Update the border style to map to the HTML border style
  *
  */
void nsTableCellFrame::MapHTMLBorderStyle(nsIPresContext* aPresContext, 
                                          nsStyleSpacing& aSpacingStyle, 
                                          nscoord aBorderWidth,
                                          nsTableFrame *aTableFrame)
{
  nsStyleCoord  width;
  width.SetCoordValue(aBorderWidth);
  aSpacingStyle.mBorder.SetTop(width);
  aSpacingStyle.mBorder.SetLeft(width);
  aSpacingStyle.mBorder.SetBottom(width);
  aSpacingStyle.mBorder.SetRight(width);

  // XXX This should come from the default style sheet (ua.css), and
  // not be hardcoded. Using solid causes the borders not to show up...
#if 1

  aSpacingStyle.SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_BG_INSET);
  aSpacingStyle.SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_BG_INSET);
  aSpacingStyle.SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_BG_INSET);
  aSpacingStyle.SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_BG_INSET);

#endif
  
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  nsIStyleContext*  styleContext = nsnull;
  
  tableFrame->GetStyleContext(&styleContext);
  
  const nsStyleColor* colorData = 
      nsStyleUtil::FindNonTransparentBackground(styleContext);
  NS_IF_RELEASE(styleContext);

  // Yaahoo, we found a style context which has a background color 
  nscolor borderColor = 0xFFC0C0C0;

  if (colorData != nsnull)
    borderColor = colorData->mBackgroundColor;

  // if the border color is white, then shift to grey
  if (borderColor == 0xFFFFFFFF)
    borderColor = 0xFFC0C0C0;
  aSpacingStyle.SetBorderColor(NS_SIDE_TOP, borderColor);
  aSpacingStyle.SetBorderColor(NS_SIDE_LEFT, borderColor);
  aSpacingStyle.SetBorderColor(NS_SIDE_BOTTOM, borderColor);
  aSpacingStyle.SetBorderColor(NS_SIDE_RIGHT, borderColor);

  //adjust the border style based on the table rules attribute
  const nsStyleTable* tableStyle;
  tableFrame->GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);

  switch (tableStyle->mRules)
  {
  case NS_STYLE_TABLE_RULES_NONE:
    aSpacingStyle.SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
    aSpacingStyle.SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
    aSpacingStyle.SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
    aSpacingStyle.SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
    break;
  case NS_STYLE_TABLE_RULES_GROUPS:
#ifdef NS_DEBUG
    printf("warning: NS_STYLE_TABLE_RULES_GROUPS used but not yet implemented.\n");
#endif
    // XXX: it depends on which cell this is!
    /*
    for h-sides, walk up content parent list to row and see what "index in parent" row is.
    if it is 0 or last, then draw the rule.  otherwise, don't.  Probably just compare
    against FirstChild and LastChild.
    for v-sides, do the same thing only for col and colgroup.
    */
    /*
    aSpacingStyle.mBorderStyle[NS_SIDE_TOP] = NS_STYLE_BORDER_STYLE_NONE;
    aSpacingStyle.mBorderStyle[NS_SIDE_RIGHT] = NS_STYLE_BORDER_STYLE_NONE;
    aSpacingStyle.mBorderStyle[NS_SIDE_BOTTOM] = NS_STYLE_BORDER_STYLE_NONE;
    aSpacingStyle.mBorderStyle[NS_SIDE_LEFT] = NS_STYLE_BORDER_STYLE_NONE;
    */
    break;
  case NS_STYLE_TABLE_RULES_COLS:
	aSpacingStyle.SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_NONE);
    aSpacingStyle.SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_NONE);
    break;

  case NS_STYLE_TABLE_RULES_ROWS:
    aSpacingStyle.SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_NONE);
    aSpacingStyle.SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_NONE);
    break;

  // do nothing for "ALL" or for any illegal value
  }
}


PRBool nsTableCellFrame::ConvertToPixelValue(nsHTMLValue& aValue, PRInt32 aDefault, PRInt32& aResult)
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

void nsTableCellFrame::MapBorderMarginPadding(nsIPresContext* aPresContext)
{
  // Check to see if the table has either cell padding or 
  // Cell spacing defined for the table. If true, then
  // this setting overrides any specific border, margin or 
  // padding information in the cell. If these attributes
  // are not defined, the the cells attributes are used

  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  NS_ASSERTION(tableFrame,"Table must not be null");
  if (!tableFrame)
    return;

  nscoord   padding = tableFrame->GetCellPadding();
  nscoord   spacingX = tableFrame->GetCellSpacingX();
  nscoord   spacingY = tableFrame->GetCellSpacingY();
  nscoord   border  = 1;


  // get the table frame style context, and from it get cellpadding, cellspacing, and border info
  const nsStyleTable* tableStyle;
  tableFrame->GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  const nsStyleSpacing* tableSpacingStyle;
  tableFrame->GetStyleData(eStyleStruct_Spacing,(const nsStyleStruct *&)tableSpacingStyle);
  nsStyleSpacing* spacingData = (nsStyleSpacing*)mStyleContext->GetMutableStyleData(eStyleStruct_Spacing);

  spacingData->mMargin.SetTop(spacingY);
  spacingData->mMargin.SetLeft(spacingX);
  spacingData->mMargin.SetBottom(spacingY);
  spacingData->mMargin.SetRight(spacingX);
  spacingData->mPadding.SetTop(padding);
  spacingData->mPadding.SetLeft(padding);
  spacingData->mPadding.SetBottom(padding);
  spacingData->mPadding.SetRight(padding); 

  // get border information from the table
  if (tableStyle->mRules!= NS_STYLE_TABLE_RULES_NONE)
  {
    // XXX: need to get border width here
    // in HTML, cell borders are always 1 pixel by default
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    border = NSIntPixelsToTwips(1, p2t);
    MapHTMLBorderStyle(aPresContext, *spacingData, border, tableFrame);
  }

  MapVAlignAttribute(aPresContext, tableFrame);
  MapHAlignAttribute(aPresContext, tableFrame);
  
}

/* XXX: this code will not work properly until the style and layout code has been updated 
 * as outlined in Bugzilla bug report 1802 and 915 */
void nsTableCellFrame::MapVAlignAttribute(nsIPresContext* aPresContext, nsTableFrame *aTableFrame)
{
  const nsStyleText* textStyle;
  GetStyleData(eStyleStruct_Text,(const nsStyleStruct *&)textStyle);
  // check if valign is set on the cell
  //   this condition will also be true if we inherited valign from the row or rowgroup
  if (textStyle->mVerticalAlign.GetUnit() == eStyleUnit_Enumerated)
  {
    return; // valign is already set on this cell
  }

  // check if valign is set on the cell's COL (or COLGROUP by inheritance)
  nsTableColFrame *colFrame;
  PRInt32 colIndex;
  GetColIndex(colIndex);
  aTableFrame->GetColumnFrame(colIndex, colFrame);
  const nsStyleText* colTextStyle;
  colFrame->GetStyleData(eStyleStruct_Text,(const nsStyleStruct *&)colTextStyle);
  if (colTextStyle->mVerticalAlign.GetUnit() == eStyleUnit_Enumerated)
  {
    nsStyleText* mutableTextStyle = (nsStyleText*)mStyleContext->GetMutableStyleData(eStyleStruct_Text);
    mutableTextStyle->mVerticalAlign.SetIntValue(colTextStyle->mVerticalAlign.GetIntValue(), eStyleUnit_Enumerated);
    return; // valign set from COL info
  }

  // otherwise, set the vertical align attribute to the HTML default
  nsStyleText* mutableTextStyle = (nsStyleText*)mStyleContext->GetMutableStyleData(eStyleStruct_Text);
  mutableTextStyle->mVerticalAlign.SetIntValue(NS_STYLE_VERTICAL_ALIGN_MIDDLE, eStyleUnit_Enumerated);
}

/* XXX: this code will not work properly until the style and layout code has been updated 
 * as outlined in Bugzilla bug report 1802 and 915.
 * In particular, mTextAlign has to be an nsStyleCoord, not just an int */
void nsTableCellFrame::MapHAlignAttribute(nsIPresContext* aPresContext, nsTableFrame *aTableFrame)
{
#if 0
  const nsStyleText* textStyle;
  GetStyleData(eStyleStruct_Text,(const nsStyleStruct *&)textStyle);
  // check if halign is set on the cell
  //   cells do not inherited halign from the row or rowgroup
  if (textStyle->mTextAlign.GetUnit() == eStyleUnit_Enumerated)
  {
    if (textStyle->mTextAlign.GetIntValue() != NS_STYLE_TEXT_ALIGN_DEFAULT)
      return; // valign is already set on this cell
  }

  // check if halign is set on the cell's ROW (or ROWGROUP by inheritance)
  nsIFrame *rowFrame;
  aTableFrame->GetContentParent(rowFrame);
  const nsStyleText* rowTextStyle;
  rowFrame->GetStyleData(eStyleStruct_Text,(const nsStyleStruct *&)rowTextStyle);
  if (rowTextStyle->mTextAlign.GetUnit() == eStyleUnit_Enumerated)
  {
    nsStyleText* mutableTextStyle = (nsStyleText*)mStyleContext->GetMutableStyleData(eStyleStruct_Text);
    mutableTextStyle->mTextAlign.SetIntValue(rowTextStyle->mTextAlign.GetIntValue(), eStyleUnit_Enumerated);
    return; // halign set from ROW info
  }

  // check if halign is set on the cell's COL (or COLGROUP by inheritance)
  nsTableColFrame *colFrame;
  PRInt32 colIndex;
  GetColIndex(colIndex);
  aTableFrame->GetColumnFrame(colIndex, colFrame);
  const nsStyleText* colTextStyle;
  colFrame->GetStyleData(eStyleStruct_Text,(const nsStyleStruct *&)colTextStyle);
  if (colTextStyle->mTextAlign.GetUnit() == eStyleUnit_Enumerated)
  {
    nsStyleText* mutableTextStyle = (nsStyleText*)mStyleContext->GetMutableStyleData(eStyleStruct_Text);
    mutableTextStyle->mTextAlign.SetIntValue(colTextStyle->mTextAlign.GetIntValue(), eStyleUnit_Enumerated);
    return; // halign set from COL info
  }

  // otherwise, set the vertical align attribute to the HTML default (center for TH, left for TD and all others)
  nsStyleText* mutableTextStyle = (nsStyleText*)mStyleContext->GetMutableStyleData(eStyleStruct_Text);
  nsIAtom *tag=nsnull;
  if (nsnull!=mContent)
    mContent->GetTag(tag);
  if (nsHTMLAtoms::th==tag)
    mutableTextStyle->mVerticalAlign.SetIntValue(NS_STYLE_TEXT_ALIGN_CENTER, eStyleUnit_Enumerated);
  else
    mutableTextStyle->mVerticalAlign.SetIntValue(NS_STYLE_TEXT_ALIGN_LEFT, eStyleUnit_Enumerated);
#endif
}  

// Subclass hook for style post processing
NS_METHOD nsTableCellFrame::DidSetStyleContext(nsIPresContext* aPresContext)
{
#ifdef NOISY_STYLE
  printf("nsTableCellFrame::DidSetStyleContext \n");
#endif

  MapBorderMarginPadding(aPresContext);
  mStyleContext->RecalcAutomaticData(aPresContext);
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(nsTableCellFrame, nsHTMLContainerFrame, nsITableCellLayout)

/* ----- global methods ----- */

nsresult 
NS_NewTableCellFrame(nsIFrame*& aResult)
{
  nsIFrame* it = new nsTableCellFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = it;
  return NS_OK;
}



/* ----- methods from CellLayoutData ----- */

/**
  * Given a frame and an edge, find the margin
  *
  **/
nscoord nsTableCellFrame::GetMargin(nsIFrame* aFrame, PRUint8 aEdge) const
{
  nscoord result = 0;

  if (aFrame)
  {
    const nsStyleSpacing* spacing;
    aFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)spacing);
    nsMargin  margin;
    spacing->CalcMarginFor(aFrame, margin);
    switch (aEdge)
    {
      case NS_SIDE_TOP:
        result = margin.top;
      break;

      case NS_SIDE_RIGHT:
        result = margin.right;
      break;

      case NS_SIDE_BOTTOM:
        result = margin.bottom;
      break;

      case NS_SIDE_LEFT:
        result = margin.left;
      break;

    }
  }
  return result;
}


/**
  * Given an Edge, find the opposing edge (top<-->bottom, left<-->right)
  *
  **/
PRUint8 nsTableCellFrame::GetOpposingEdge(PRUint8 aEdge)
{
   PRUint8 result;

   switch (aEdge)
    {
   case NS_SIDE_LEFT:
        result = NS_SIDE_RIGHT;
        break;

   case NS_SIDE_RIGHT:
        result = NS_SIDE_LEFT;
        break;

   case NS_SIDE_TOP:
        result = NS_SIDE_BOTTOM;
        break;

   case NS_SIDE_BOTTOM:
        result = NS_SIDE_TOP;
        break;

      default:
        result = NS_SIDE_TOP;
     }
  return result;
}




/**
  * Given a List of cell layout data, compare the edges to see which has the
  * border with the highest precidence. 
  *
  **/
nscoord nsTableCellFrame::FindLargestMargin(nsVoidArray* aList,PRUint8 aEdge)
{
  nscoord result = 0;
  PRInt32 index = 0;
  PRInt32 count = 0;


  NS_ASSERTION(aList,"a List must be valid");
  count = aList->Count();
  if (count)
  {
    nsIFrame* frame;
    
    nscoord value = 0;
    while (index < count)
    {
      frame = (nsIFrame*)(aList->ElementAt(index++));
      value = GetMargin(frame, aEdge);
      if (value > result)
        result = value;
    }
  }
  return result;
}


void nsTableCellFrame::GetCellBorder(nsMargin &aBorder, nsTableFrame *aTableFrame)
{
  aBorder.left = aBorder.right = aBorder.top = aBorder.bottom = 0;
  if (nsnull==aTableFrame) {
    return;
  }

  if (NS_STYLE_BORDER_COLLAPSE==aTableFrame->GetBorderCollapseStyle()) {
    aBorder = mBorderEdges.mMaxBorderWidth;
  } else {
    const nsStyleSpacing* spacing;
    GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)spacing);
    spacing->GetBorder(aBorder);
  }
}

void nsTableCellFrame::CalculateMargins(nsTableFrame*     aTableFrame,
                                        nsVoidArray*      aBoundaryCells[4])
{ 
  // By default the margin is just the margin found in the 
  // table cells style 
  const nsStyleSpacing* spacing;
  GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)spacing);
  spacing->CalcMarginFor(this, mMargin);

  // Left and Top Margins are collapsed with their neightbors
  // Right and Bottom Margins are simple left as they are
  nscoord value;

  // The left and top sides margins are the difference between
  // their inherint value and the value of the margin of the 
  // object to the left or right of them.

  value = FindLargestMargin(aBoundaryCells[NS_SIDE_LEFT],NS_SIDE_RIGHT);
  if (value > mMargin.left)
    mMargin.left = 0;
  else
    mMargin.left -= value;

  value = FindLargestMargin(aBoundaryCells[NS_SIDE_TOP],NS_SIDE_BOTTOM);
  if (value > mMargin.top)
    mMargin.top = 0;
  else
    mMargin.top -= value;
}


void nsTableCellFrame::RecalcLayoutData(nsTableFrame* aTableFrame,
                                        nsVoidArray* aBoundaryCells[4])

{
  CalculateBorders(aTableFrame, aBoundaryCells);
  CalculateMargins(aTableFrame, aBoundaryCells);
  mCalculated = NS_OK;
}

NS_IMETHODIMP
nsTableCellFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::tableCellFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

NS_IMETHODIMP
nsTableCellFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("TableCell", aResult);
}

void nsTableCellFrame::SetCollapseOffsetX(nscoord aXOffset)
{
  mCollapseOffset.x = aXOffset;
}

void nsTableCellFrame::SetCollapseOffsetY(nscoord aYOffset)
{
  mCollapseOffset.y = aYOffset;
}

nsPoint nsTableCellFrame::GetCollapseOffset()
{
  return mCollapseOffset;
}

