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


void nsTableCellFrame::InitCellFrame(PRInt32 aColIndex)
{
  NS_PRECONDITION(0<=aColIndex, "bad col index arg");
  mColIndex = aColIndex;
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
  }
}

void nsTableCellFrame::SetBorderEdgeLength(PRUint8 aSide, 
                                           PRInt32 aIndex, 
                                           nscoord aLength)
{
  if ((NS_SIDE_LEFT==aSide) || (NS_SIDE_RIGHT==aSide))
  {
    PRInt32 rowIndex = aIndex-GetRowIndex();
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
  if (eFramePaintLayer_Underlay == aWhichLayer) {
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
                                      aDirtyRect, rect, *myColor, 0, 0);
    
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
                                        aDirtyRect, rect, *mySpacing, skipSides);
          }
          else
          {
            nsCSSRendering::PaintBorderEdges(aPresContext, aRenderingContext, this,
                                             aDirtyRect, rect, &mBorderEdges, skipSides);
          }
        }
      }
    }
  }

  // for debug...
  if ((eFramePaintLayer_Overlay == aWhichLayer) && GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,128,128));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
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
      PRInt32 colIndex = aColIndex-GetColIndex();
      border = (nsBorderEdge *)(mBorderEdges.mEdges[aSide].ElementAt(colIndex));
      mBorderEdges.mMaxBorderWidth.top = PR_MAX(aBorder->mWidth, mBorderEdges.mMaxBorderWidth.top);
      break;
    }

    case NS_SIDE_BOTTOM:
    {
      PRInt32 colIndex = aColIndex-GetColIndex();
      border = (nsBorderEdge *)(mBorderEdges.mEdges[aSide].ElementAt(colIndex));
      mBorderEdges.mMaxBorderWidth.bottom = PR_MAX(aBorder->mWidth, mBorderEdges.mMaxBorderWidth.bottom);
      break;
    }
  
    case NS_SIDE_LEFT:
    {
      PRInt32 rowIndex = aRowIndex-GetRowIndex();
      border = (nsBorderEdge *)(mBorderEdges.mEdges[aSide].ElementAt(rowIndex));
      mBorderEdges.mMaxBorderWidth.left = PR_MAX(aBorder->mWidth, mBorderEdges.mMaxBorderWidth.left);
      break;
    }
      
    case NS_SIDE_RIGHT:  
    {
      PRInt32 rowIndex = aRowIndex-GetRowIndex();
      border = (nsBorderEdge *)(mBorderEdges.mEdges[aSide].ElementAt(rowIndex));
      mBorderEdges.mMaxBorderWidth.right = PR_MAX(aBorder->mWidth, mBorderEdges.mMaxBorderWidth.right);
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
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);
  
  nscoord topInset = borderPadding.top;
  nscoord bottomInset = borderPadding.bottom;
  PRUint8 verticalAlignFlags = NS_STYLE_VERTICAL_ALIGN_MIDDLE;
  if (textStyle->mVerticalAlign.GetUnit() == eStyleUnit_Enumerated) {
    verticalAlignFlags = textStyle->mVerticalAlign.GetIntValue();
  }
  nscoord height = mRect.height;
  nsRect kidRect;  
  mFirstChild->GetRect(kidRect);
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
  mFirstChild->MoveTo(kidRect.x, kidYTop);
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

  // Initialize out parameter
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  aStatus = NS_FRAME_COMPLETE;
  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT)
    printf("%p nsTableCellFrame::Reflow: maxSize=%d,%d\n",
           this, aReflowState.maxSize.width, aReflowState.maxSize.height);

  nsSize availSize(aReflowState.maxSize);
  nsSize maxElementSize;
  nsSize *pMaxElementSize = aDesiredSize.maxElementSize;
  if (NS_UNCONSTRAINEDSIZE==aReflowState.maxSize.width)
    pMaxElementSize = &maxElementSize;
  nscoord x = 0;
  // SEC: what about ascent and decent???

  // Compute the insets (sum of border and padding)
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);

  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);

  nscoord topInset = borderPadding.top;
  nscoord rightInset = borderPadding.right;
  nscoord bottomInset = borderPadding.bottom;
  nscoord leftInset = borderPadding.left;

  // reduce available space by insets, if we're in a constrained situation
  if (NS_UNCONSTRAINEDSIZE!=availSize.width)
    availSize.width -= leftInset+rightInset;
  if (NS_UNCONSTRAINEDSIZE!=availSize.height)
    availSize.height -= topInset+bottomInset;

  if (eReflowReason_Incremental == aReflowState.reason) 
  {
    // XXX We *must* do this otherwise incremental reflow that's
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
        nsIReflowCommand::ReflowType type;
        aReflowState.reflowCommand->GetType(type);
        if (nsIReflowCommand::StyleChanged==type)
        {
          nsresult rv = IR_StyleChanged(aPresContext, aDesiredSize, aReflowState, aStatus);
          aStatus = NS_FRAME_COMPLETE;
          return rv;
        }
      }
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
  SetPriorAvailWidth(aReflowState.maxSize.width);
  nsHTMLReflowState kidReflowState(aPresContext, mFirstChild, aReflowState,
                                   availSize);

  ReflowChild(mFirstChild, aPresContext, kidSize, kidReflowState, aStatus);
#ifdef NS_DEBUG
  if (kidSize.width > availSize.width)
  {
    printf("WARNING: cell ");
    nsAutoString tmp;
    mFirstChild->GetFrameName(tmp);
    fputs(tmp, stdout);
    printf(" content returned desired width %d given avail width %d\n",
            kidSize.width, availSize.width);
  }
#endif

  // Nav4 hack for 0 dimensioned cells.  
  // Empty cells are assigned a width and height of 4px
  // see testcase "cellHeights.html"
  if (eReflowReason_Initial == aReflowState.reason)
  {
    if ((0==kidSize.width) && (0==kidSize.height))
      SetContentEmpty(PR_TRUE);
    else
      SetContentEmpty(PR_FALSE);
  }
  if (0==kidSize.width)
  {
    float p2t;
    aPresContext.GetScaledPixelsToTwips(p2t);
    kidSize.width=NSIntPixelsToTwips(4, p2t);
    if (nsnull!=aDesiredSize.maxElementSize && 0==pMaxElementSize->width)
          pMaxElementSize->width=NSIntPixelsToTwips(4, p2t);
    if (gsDebug) printf ("setting child width from 0 to %d for nav4 compatibility\n", NSIntPixelsToTwips(4, p2t));
  }
  if (0==kidSize.height)
  {
    float p2t;
    aPresContext.GetScaledPixelsToTwips(p2t);
    kidSize.height=NSIntPixelsToTwips(4, p2t);
    if (nsnull!=aDesiredSize.maxElementSize && 0==pMaxElementSize->width)
          pMaxElementSize->height=NSIntPixelsToTwips(4, p2t);
    if (gsDebug) printf ("setting child height from 0 to %d for nav4 compatibility\n", NSIntPixelsToTwips(4, p2t));
  }
  // end Nav4 hack for 0 dimensioned cells

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
  mFirstChild->SetRect(nsRect(leftInset, topInset,
                           kidSize.width, kidSize.height));  
    
  // Return our size and our result

  // first, compute the height
  // the height can be set w/o being restricted by aMaxSize.height
  nscoord cellHeight = kidSize.height;
  // NAV4 compatibility: only add insets if cell content was not 0 height
  if (0!=kidSize.height)
    cellHeight += topInset + bottomInset;
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
  nsTableFrame* tableFrame=nsnull;
  rv = nsTableFrame::GetTableFrame(this, tableFrame);
  if ((NS_SUCCEEDED(rv)) && (nsnull!=tableFrame))
  {
    tableFrame->InvalidateCellMap();
    tableFrame->InvalidateFirstPassCache();
  }

  return rv;
}

NS_METHOD
nsTableCellFrame::CreateContinuingFrame(nsIPresContext&  aPresContext,
                                        nsIFrame*        aParent,
                                        nsIStyleContext* aStyleContext,
                                        nsIFrame*&       aContinuingFrame)
{
  nsTableCellFrame* cf = new nsTableCellFrame;
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  cf->Init(aPresContext, mContent, aParent, aStyleContext);
  cf->AppendToFlow(this);
  cf->InitCellFrame(GetColIndex());
  aContinuingFrame = cf;

  // Create a continuing body frame
  nsIFrame*         childList;
  nsIStyleContext*  kidSC;

  mFirstChild->GetStyleContext(kidSC);
  mFirstChild->CreateContinuingFrame(aPresContext, cf, kidSC, childList);
  NS_RELEASE(kidSC);
  cf->SetInitialChildList(aPresContext, nsnull, childList);
  return NS_OK;
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

  aSpacingStyle.SetBorderStyle(NS_SIDE_TOP, NS_STYLE_BORDER_STYLE_INSET);
  aSpacingStyle.SetBorderStyle(NS_SIDE_LEFT, NS_STYLE_BORDER_STYLE_INSET);
  aSpacingStyle.SetBorderStyle(NS_SIDE_BOTTOM, NS_STYLE_BORDER_STYLE_INSET);
  aSpacingStyle.SetBorderStyle(NS_SIDE_RIGHT, NS_STYLE_BORDER_STYLE_INSET);

#endif
  
  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  nsIStyleContext*  styleContext = nsnull;
  
  tableFrame->GetStyleContext(styleContext);
  
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
    aPresContext->GetScaledPixelsToTwips(p2t);
    border = NSIntPixelsToTwips(1, p2t);
    MapHTMLBorderStyle(aPresContext, *spacingData, border, tableFrame);
  }

  // TODO: map the align attributes here
  
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


NS_METHOD
nsTableCellFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  return nsContainerFrame::QueryInterface(aIID, aInstancePtr);
}

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
  * Given a style context and an edge, find the border width
  *
  **/
nscoord nsTableCellFrame::GetBorderWidth(nsIFrame* aFrame, PRUint8 aEdge) const
{
  nscoord result = 0;

  if (aFrame)
  {
    const nsStyleSpacing* spacing;
    aFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)spacing);
    nsMargin  border;
    spacing->CalcBorderFor(aFrame, border);
    switch (aEdge)
    {
      case NS_SIDE_TOP:
        result = border.top;
      break;

      case NS_SIDE_RIGHT:
        result = border.right;
      break;

      case NS_SIDE_BOTTOM:
        result = border.bottom;
      break;

      case NS_SIDE_LEFT:
        result = border.left;
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


/* ----- debugging methods ----- */
NS_METHOD nsTableCellFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const
{
  // if a filter is present, only output this frame if the filter says we should
  // since this could be any "tag" with the right display type, we'll
  // just pretend it's a cell
  if (nsnull==aFilter)
    return nsContainerFrame::List(out, aIndent, aFilter);

  nsAutoString tagString("td");
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

#if 0       //QQQ
void nsTableCellFrame::List(FILE* out, PRInt32 aIndent, nsIListFilter *aFilter) const
{
// if a filter is present, only output this frame if the filter says we should
  nsIAtom* tag;
  nsAutoString tagString;
  mContent->GetTag(tag);
  if (tag != nsnull) 
    tag->ToString(tagString);
  if ((nsnull==aFilter) || (PR_TRUE==aFilter->OutputTag(&tagString)))
  {
    PRInt32 indent;

    nsIContent* cell;

    this->GetContent(cell);
    if (cell != nsnull)
    {
      /*
      for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
      fprintf(out,"RowSpan = %d ColSpan = %d \n",cell->GetRowSpan(),cell->GetColSpan());
      */
      for (indent = aIndent; --indent >= 0; ) fputs("  ", out);
      fprintf(out,"Margin -- Top: %d Left: %d Bottom: %d Right: %d \n",  
                  NSTwipsToIntPoints(mMargin.top),
                  NSTwipsToIntPoints(mMargin.left),
                  NSTwipsToIntPoints(mMargin.bottom),
                  NSTwipsToIntPoints(mMargin.right));


      for (indent = aIndent; --indent >= 0; ) fputs("  ", out);

      nscoord top,left,bottom,right;
    
      top = (mBorderFrame[NS_SIDE_TOP] ? GetBorderWidth((nsIFrame*)mBorderFrame[NS_SIDE_TOP], NS_SIDE_TOP) : 0);
      left = (mBorderFrame[NS_SIDE_LEFT] ? GetBorderWidth((nsIFrame*)mBorderFrame[NS_SIDE_LEFT], NS_SIDE_LEFT) : 0);
      bottom = (mBorderFrame[NS_SIDE_BOTTOM] ? GetBorderWidth((nsIFrame*)mBorderFrame[NS_SIDE_BOTTOM], NS_SIDE_BOTTOM) : 0);
      right = (mBorderFrame[NS_SIDE_RIGHT] ? GetBorderWidth((nsIFrame*)mBorderFrame[NS_SIDE_RIGHT], NS_SIDE_RIGHT) : 0);


      fprintf(out,"Border -- Top: %d Left: %d Bottom: %d Right: %d \n",  
                  NSTwipsToIntPoints(top),
                  NSTwipsToIntPoints(left),
                  NSTwipsToIntPoints(bottom),
                  NSTwipsToIntPoints(right));



      cell->List(out,aIndent);
      NS_RELEASE(cell);
    }
  }

  // Output the children
  if (nsnull != mFirstChild) {
    if (PR_TRUE==outputMe)
    {
      if (0 != mState) {
        fprintf(out, " [state=%08x]", mState);
      }
    }
    for (nsIFrame* child = mFirstChild; child; NextChild(child, child)) {
      child->List(out, aIndent + 1, aFilter);
    }
    if (PR_TRUE==outputMe)
    {
      for (PRInt32 i = aIndent; --i >= 0; ) fputs("  ", out);
    }
  } else {
    if (PR_TRUE==outputMe)
    {
      if (0 != mState) {
        fprintf(out, " [state=%08x]", mState);
      }
    }
  }
}

#endif

NS_IMETHODIMP
nsTableCellFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("TableCell", aResult);
}
