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


NS_IMETHODIMP
nsTableCellFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aChildList)
{
  // Create body pseudo frame
  NS_NewBodyFrame(mFirstChild, NS_BODY_NO_AUTO_MARGINS);

  // Resolve style and set the style context
  nsIStyleContext* styleContext =
    aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::cellContentPseudo, mStyleContext);              // styleContext: ADDREF++
  mFirstChild->Init(aPresContext, mContent, this, styleContext);
  NS_RELEASE(styleContext);                                           // styleContext: ADDREF--

  // Set the geometric and content parent for each of the child frames
  for (nsIFrame* frame = aChildList; nsnull != frame; frame->GetNextSibling(frame)) {
    frame->SetGeometricParent(mFirstChild);
    frame->SetContentParent(mFirstChild);
  }

  // Queue up the frames for the block frame
  return mFirstChild->SetInitialChildList(aPresContext, nsnull, aChildList);
}

NS_METHOD nsTableCellFrame::Paint(nsIPresContext& aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  const nsRect& aDirtyRect)
{
  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

  if (disp->mVisible) {
    const nsStyleColor* myColor =
      (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    const nsStyleSpacing* mySpacing =
      (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    NS_ASSERTION(nsnull!=myColor, "bad style color");
    NS_ASSERTION(nsnull!=mySpacing, "bad style spacing");

    nsRect  rect(0, 0, mRect.width, mRect.height);

    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *myColor, 0, 0);
    
    //XXX: this could be calculated once and remembered
    // get border padding values
    //XXX: also check style for rule on rendering empty cells
    nsMargin borderPadding;
    const nsStyleSpacing* cellSpacing;
    GetStyleData(eStyleStruct_Spacing , ((const nsStyleStruct *&)cellSpacing));
    cellSpacing->CalcBorderPaddingFor(this, borderPadding);
    nscoord contentWidth = mPass1DesiredSize.width - (borderPadding.left+borderPadding.right);
    nscoord contentHeight = mPass1DesiredSize.height - (borderPadding.top+borderPadding.bottom);
    // empty cells do not render their border
    if (0<contentWidth || 0<contentHeight)
    {
      nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                  aDirtyRect, rect, *mySpacing, 0);
    }
  }

  // for debug...
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,128,128));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
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
    hc->GetAttribute(nsHTMLAtoms::rowspan, val); 
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
    hc->GetAttribute(nsHTMLAtoms::colspan, val); 
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
  // Nav4 hack for 0 width cells.  
  // Empty cells are assigned a width of 3px
  // see testcase "cellHeights.html"
  if (0==kidSize.width)
  {
    float p2t;
    aPresContext.GetScaledPixelsToTwips(p2t);
    kidSize.width=NSIntPixelsToTwips(3, p2t);
    if (nsnull!=aDesiredSize.maxElementSize && 0==pMaxElementSize->width)
          pMaxElementSize->width=NSIntPixelsToTwips(3, p2t);
    if (gsDebug) printf ("setting child width from 0 to %d for nav4 compatibility\n", NSIntPixelsToTwips(1, p2t));
  }
  // end Nav4 hack for 0 width cells
#endif
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

  // NAV4 compatibility: only add insets if cell content was not 0 width
  if (0!=cellWidth)
    cellWidth += leftInset + rightInset;    // factor in insets

  // set the cell's desired size and max element size
  aDesiredSize.width = cellWidth;
  aDesiredSize.height = cellHeight;
  aDesiredSize.ascent = topInset;
  aDesiredSize.descent = bottomInset;
  if (nsnull!=aDesiredSize.maxElementSize)
  {
    *aDesiredSize.maxElementSize = *pMaxElementSize;
    // NAV4 compatibility: only add insets if cell content was not 0 min height
    if (0!=pMaxElementSize->height)
      aDesiredSize.maxElementSize->height += topInset + bottomInset;
    // NAV4 compatibility: only add insets if cell content was not 0 min width
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
  aContinuingFrame = cf;
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
  aSpacingStyle.mBorderStyle[NS_SIDE_TOP] = NS_STYLE_BORDER_STYLE_INSET; 
  aSpacingStyle.mBorderStyle[NS_SIDE_LEFT] = NS_STYLE_BORDER_STYLE_INSET; 
  aSpacingStyle.mBorderStyle[NS_SIDE_BOTTOM] = NS_STYLE_BORDER_STYLE_INSET; 
  aSpacingStyle.mBorderStyle[NS_SIDE_RIGHT] = NS_STYLE_BORDER_STYLE_INSET; 
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

  aSpacingStyle.mBorderColor[NS_SIDE_TOP] = 
  aSpacingStyle.mBorderColor[NS_SIDE_LEFT] = 
  aSpacingStyle.mBorderColor[NS_SIDE_BOTTOM] = 
  aSpacingStyle.mBorderColor[NS_SIDE_RIGHT] = borderColor;

  //adjust the border style based on the table rules attribute
  const nsStyleTable* tableStyle;
  tableFrame->GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);

  switch (tableStyle->mRules)
  {
  case NS_STYLE_TABLE_RULES_NONE:
    aSpacingStyle.mBorderStyle[NS_SIDE_TOP] = NS_STYLE_BORDER_STYLE_NONE;
    aSpacingStyle.mBorderStyle[NS_SIDE_RIGHT] = NS_STYLE_BORDER_STYLE_NONE;
    aSpacingStyle.mBorderStyle[NS_SIDE_BOTTOM] = NS_STYLE_BORDER_STYLE_NONE;
    aSpacingStyle.mBorderStyle[NS_SIDE_LEFT] = NS_STYLE_BORDER_STYLE_NONE;
    break;
  case NS_STYLE_TABLE_RULES_GROUPS:
    // XXX: it depends on which cell this is!
    /*
    aSpacingStyle.mBorderStyle[NS_SIDE_TOP] = NS_STYLE_BORDER_STYLE_NONE;
    aSpacingStyle.mBorderStyle[NS_SIDE_RIGHT] = NS_STYLE_BORDER_STYLE_NONE;
    aSpacingStyle.mBorderStyle[NS_SIDE_BOTTOM] = NS_STYLE_BORDER_STYLE_NONE;
    aSpacingStyle.mBorderStyle[NS_SIDE_LEFT] = NS_STYLE_BORDER_STYLE_NONE;
    */
    break;
  case NS_STYLE_TABLE_RULES_COLS:
    aSpacingStyle.mBorderStyle[NS_SIDE_TOP] = NS_STYLE_BORDER_STYLE_NONE;
    aSpacingStyle.mBorderStyle[NS_SIDE_BOTTOM] = NS_STYLE_BORDER_STYLE_NONE;
    break;

  case NS_STYLE_TABLE_RULES_ROWS:
    aSpacingStyle.mBorderStyle[NS_SIDE_RIGHT] = NS_STYLE_BORDER_STYLE_NONE;
    aSpacingStyle.mBorderStyle[NS_SIDE_LEFT] = NS_STYLE_BORDER_STYLE_NONE;
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

  nscoord   padding = 0;
  nscoord   spacing = 0;
  nscoord   border  = 1;

  nsTableFrame* tableFrame;
  nsTableFrame::GetTableFrame(this, tableFrame);
  NS_ASSERTION(tableFrame,"Table must not be null");
  if (!tableFrame)
    return;

  // get the table frame style context, and from it get cellpadding, cellspacing, and border info
  const nsStyleTable* tableStyle;
  tableFrame->GetStyleData(eStyleStruct_Table, (const nsStyleStruct *&)tableStyle);
  const nsStyleSpacing* tableSpacingStyle;
  tableFrame->GetStyleData(eStyleStruct_Spacing,(const nsStyleStruct *&)tableSpacingStyle);
  nsStyleSpacing* spacingData = (nsStyleSpacing*)mStyleContext->GetMutableStyleData(eStyleStruct_Spacing);

  // check to see if cellpadding or cellspacing is defined
  if (tableStyle->mCellPadding.GetUnit() != eStyleUnit_Null || 
      tableStyle->mCellSpacing.GetUnit() != eStyleUnit_Null)
  {
    spacingData->mMargin.SetTop(tableStyle->mCellSpacing);
    spacingData->mMargin.SetLeft(tableStyle->mCellSpacing);
    spacingData->mMargin.SetBottom(tableStyle->mCellSpacing);
    spacingData->mMargin.SetRight(tableStyle->mCellSpacing);
    spacingData->mPadding.SetTop(tableStyle->mCellPadding);
    spacingData->mPadding.SetLeft(tableStyle->mCellPadding);
    spacingData->mPadding.SetBottom(tableStyle->mCellPadding);
    spacingData->mPadding.SetRight(tableStyle->mCellPadding); 
  }

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
  * Given a style context and an edge, find the padding
  *
  **/
nscoord nsTableCellFrame::GetPadding(nsIFrame* aFrame, PRUint8 aEdge) const
{
  nscoord result = 0;

  if (aFrame)
  {
    const nsStyleSpacing* spacing;
    aFrame->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)spacing);
    nsMargin  padding;
    spacing->CalcPaddingFor(aFrame, padding);
    switch (aEdge)
    {
      case NS_SIDE_TOP:
        result = padding.top;
      break;

      case NS_SIDE_RIGHT:
        result = padding.right;
      break;

      case NS_SIDE_BOTTOM:
        result = padding.bottom;
      break;

      case NS_SIDE_LEFT:
        result = padding.left;
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


/*
*
* Determine border style for two cells.
*
  1.If the adjacent elements are of the same type, the wider of the two borders is used. 
    "Wider" takes into account the border-style of 'none', so a "1px solid" border 
    will take precedence over a "20px none" border. 

  2.If there are two or more with the same width, but different style, 
    then the one with a style near the start of the following list will be drawn:

      'blank', 'double', 'solid', 'dashed', 'dotted', 'ridge', 'groove', 'none'

  3.If the borders are of the same width, the border on the element occurring first is used. 
  
    First is defined as aStyle for this method.

  NOTE: This assumes left-to-right, top-to-bottom bias. -- gpk

*
*/


nsIFrame* nsTableCellFrame::CompareCellBorders(nsIFrame* aFrame1,
                                               PRUint8 aEdge1,
                                               nsIFrame* aFrame2,
                                               PRUint8 aEdge2)
{
  PRInt32 width1 = GetBorderWidth(aFrame1,aEdge1);
  PRInt32 width2 = GetBorderWidth(aFrame2,aEdge2);

  nsIFrame* result = nsnull;
  
  if (width1 > width2)
    result =  aFrame1;
  else if (width1 < width2)
    result = aFrame2;
  else // width1 == width2
  {
    const nsStyleSpacing*  border1;
    const nsStyleSpacing*  border2;
    aFrame1->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)border1);
    aFrame2->GetStyleData(eStyleStruct_Spacing, (const nsStyleStruct*&)border2);
    if (border1->mBorderStyle[aEdge1] >= border2->mBorderStyle[aEdge2])
      result = aFrame1;
    else
      result = aFrame2;
  }
  return result;
}


/**
  * Given a List of cell layout data, compare the edges to see which has the
  * border with the highest precidence. 
  *
  **/

nsIFrame* nsTableCellFrame::FindHighestPrecedentBorder(nsVoidArray* aList,
                                                       PRUint8 aEdge)
{
  nsIFrame* result = nsnull;
  PRInt32 index = 0;
  PRInt32 count = 0;


  NS_ASSERTION(aList,"a List must be valid");
  count = aList->Count();
  if (count)
  {
    nsIFrame* frame1;
    nsIFrame* frame2;
    
    frame1 = (nsIFrame*)(aList->ElementAt(index++));
    while (index < count)
    {
      frame2 = (nsIFrame*)(aList->ElementAt(index++));
      if (GetMargin(frame2,aEdge) == 0) {
        frame1 = CompareCellBorders(frame1, aEdge, frame2, aEdge);
      }
    }
    if ((nsnull != frame1) && (GetMargin(frame1, aEdge) != 0))
      result = frame1;
  }
  return result;
}
  



nsIFrame* nsTableCellFrame::FindInnerBorder(nsVoidArray*  aList, PRUint8 aEdge)
{
  nsIFrame* result = nsnull;
  PRUint8   opposite = GetOpposingEdge(aEdge);

  if (GetMargin(this, aEdge) == 0)
  {
    nsIFrame* altFrame = FindHighestPrecedentBorder(aList,opposite);
    if (nsnull != altFrame)
      result = CompareCellBorders(this, aEdge, altFrame, opposite);
    else
      result = this;
  }

  return result;
}


/*
*
* FindRelevantBorder recursively searches up the frame hierarchy for the border
* style that is applicable to the cell. If at any point the frame has a margin
* or the parent frame has padding, then the outer frame for this object takes
* presendence over the inner frame. 

1.Borders on 'table' elements take precedence over borders on any other table elements. 
2.Borders on 'row-groups' take precedence over borders on 'rows', 
  and likewise borders on 'column-groups' take precedence over borders on 'columns'. 
3.Borders on any other type of table element take precedence over 'table-cell' elements. 
 
*
* NOTE: This method assumes that the table cell potentially shares a border.
* It should not be called for internal cells
*
* NOTE: COLUMNS AND COLGROUPS NEED TO BE FIGURED INTO THE ALGORITHM -- GPK!!!
* 
*
*/
nsIFrame* nsTableCellFrame::FindOuterBorder( nsTableFrame* aTableFrame,
                                             PRUint8 aEdge)
{
  nsIFrame* frame = this;   // By default, return our frame 
  PRBool    done = PR_FALSE;
    

  // The table frame is the outer most frame we test against
  while (done == PR_FALSE)
  {
    done = PR_TRUE; // where done unless the frame's margin is zero
                    // and the parent's padding is zero

    nscoord margin = GetMargin(frame,aEdge);

    // if the margin for this style is zero then check to see if the paddding
    // for the parent frame is also zero
    if (margin == 0)
    { 
      nsIFrame* parentFrame;

      frame->GetGeometricParent(parentFrame);

      // if the padding for the parent style is zero just
      // recursively call this routine
      PRInt32 padding = GetPadding(parentFrame,aEdge);
      if ((nsnull != parentFrame) && (padding == 0))
      {
        frame = parentFrame;
        // If this frame represents the table frame then
        // the table style is used
        done = PRBool(frame != (nsIFrame*)aTableFrame);
        continue;
      }

    }
  }
  return frame;
}



/*

  Border Resolution
  1.Borders on 'table' elements take precedence over borders on any other table elements. 
  2.Borders on 'row-groups' take precedence over borders on 'rows', and likewise borders on 'column-groups' take
    precedence over borders on 'columns'. 
  3.Borders on any other type of table element take precedence over 'table-cell' elements. 
  4.If the adjacent elements are of the same type, the wider of the two borders is used. "Wider" takes into account
    the border-style of 'none', so a "1px solid" border will take precedence over a "20px none" border. 
  5.If the borders are of the same width, the border on the element occurring first is used. 


  How to compare
  1.Those of the one or two cells that have an edge here. 
    Less than two can occur at the edge of the table, but also
    at the edges of "holes" (unoccupied grid cells).
  2.Those of the columns that have an edge here.
  3.Those of the column groups that have an edge here.
  4.Those of the rows that have an edge here.
  5.Those of the row groups that have an edge here.
  6.Those of the table, if this is the edge of the table.

*
* @param aIsFirst -- TRUE if this is the first cell in the row
* @param aIsLast  -- TRUE if this is the last cell in the row
* @param aIsTop -- TRUE if this is the top cell in the column
* @param aIsBottom  -- TRUE if this is the last cell in the column
*/

nsIFrame* nsTableCellFrame::FindBorderFrame(nsTableFrame*    aTableFrame,
                                            nsVoidArray*     aList,
                                            PRUint8          aEdge)
{
  nsIFrame*  frame = nsnull;

  if (aList && aList->Count() == 0)
    frame = FindOuterBorder(aTableFrame, aEdge);
  else
    frame = FindInnerBorder(aList, aEdge);

  if (! frame) 
    frame = this;

  return frame;
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
