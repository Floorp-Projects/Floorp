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
#include "nsBodyFrame.h"
#include "nsReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsCSSLayout.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIPtr.h"
#include "nsIView.h"

NS_DEF_PTR(nsIStyleContext);

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
//#define   NOISY_STYLE
//#define NOISY_FLOW
#else
static const PRBool gsDebug = PR_FALSE;
#endif

/**
  */
nsTableCellFrame::nsTableCellFrame(nsIContent* aContent,
                                   nsIFrame*   aParentFrame)
  : nsContainerFrame(aContent, aParentFrame)
{
}

nsTableCellFrame::~nsTableCellFrame()
{
}

NS_METHOD nsTableCellFrame::Paint(nsIPresContext& aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  const nsRect& aDirtyRect)
{
  nsStyleDisplay* disp =
    (nsStyleDisplay*)mStyleContext->GetData(eStyleStruct_Display);

  if (disp->mVisible) {
    nsStyleColor* myColor =
      (nsStyleColor*)mStyleContext->GetData(eStyleStruct_Color);
    nsStyleSpacing* mySpacing =
      (nsStyleSpacing*)mStyleContext->GetData(eStyleStruct_Spacing);
    NS_ASSERTION(nsnull!=myColor, "bad style color");
    NS_ASSERTION(nsnull!=mySpacing, "bad style spacing");

    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, mRect, *myColor);

    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, mRect, *mySpacing, 0);
    /*
    printf("painting borders, size = %d %d %d %d\n", 
            myBorder->mSize.left, myBorder->mSize.top, 
            myBorder->mSize.right, myBorder->mSize.bottom);
    */
  }

  // for debug...
  if (nsIFrame::GetShowFrameBorders()) {
    aRenderingContext.SetColor(NS_RGB(0,128,128));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect);
  return NS_OK;
}

/**
*
* Align the cell's child frame within the cell
*
**/

void  nsTableCellFrame::VerticallyAlignChild(nsIPresContext* aPresContext)
{
  nsStyleSpacing* spacing =
      (nsStyleSpacing*)mStyleContext->GetData(eStyleStruct_Spacing);
  nsStyleText* textStyle =
      (nsStyleText*)mStyleContext->GetData(eStyleStruct_Text);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);
  
  nscoord topInset = borderPadding.top;
  nscoord bottomInset = borderPadding.bottom;
  PRUint8 verticalAlignFlags = NS_STYLE_VERTICAL_ALIGN_MIDDLE;
  if (textStyle->mVerticalAlign.GetUnit() == eStyleUnit_Enumerated) {
    verticalAlignFlags = textStyle->mVerticalAlign.GetIntValue();
  }

  nscoord       height = mRect.height;

  nsRect        kidRect;  
  mFirstChild->GetRect(kidRect);
  nscoord       childHeight = kidRect.height;
  

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

/** helper method to get the row span of this frame's content (which must be a cell) */
PRInt32 nsTableCellFrame::GetRowSpan()
{
  PRInt32 result = 0;
  nsTableCell *cellContent = (nsTableCell *)mContent;
  if (nsnull!=cellContent)
  {
    result = cellContent->GetRowSpan();
  }
  return result;
}

/** helper method to get the col span of this frame's content (which must be a cell) */
PRInt32 nsTableCellFrame::GetColSpan()
{
  PRInt32 result = 0;
  nsTableCell *cellContent = (nsTableCell *)mContent;
  if (nsnull!=cellContent)
  {
    result = cellContent->GetColSpan();
  }
  return result;
}

/** helper method to get the col index of this frame's content (which must be a cell) */
PRInt32 nsTableCellFrame::GetColIndex()
{
  PRInt32 result = 0;
  nsTableCell *cellContent = (nsTableCell *)mContent;
  if (nsnull!=cellContent)
  {
    result = cellContent->GetColIndex();
  }
  return result;
}

void nsTableCellFrame::CreatePsuedoFrame(nsIPresContext* aPresContext)
{
  // Do we have a prev-in-flow?
  if (nsnull == mPrevInFlow) {
    // No, create a column pseudo frame
    nsBodyFrame::NewFrame(&mFirstChild, mContent, this);
    mChildCount = 1;

    // Resolve style and set the style context
    nsIStyleContext* styleContext =
      aPresContext->ResolveStyleContextFor(mContent, this);             // styleContext: ADDREF++
    mFirstChild->SetStyleContext(aPresContext,styleContext);
    NS_RELEASE(styleContext);                                           // styleContext: ADDREF--
  } else {
    nsTableCellFrame* prevFrame = (nsTableCellFrame *)mPrevInFlow;

    nsIFrame* prevPseudoFrame = prevFrame->mFirstChild;
    NS_ASSERTION(prevFrame->ChildIsPseudoFrame(prevPseudoFrame), "bad previous pseudo-frame");

    // Create a continuing column
    nsIStyleContext* kidSC;
    prevPseudoFrame->GetStyleContext(aPresContext, kidSC);
    prevPseudoFrame->CreateContinuingFrame(aPresContext, this, kidSC, mFirstChild);
    NS_RELEASE(kidSC);
    mChildCount = 1;
  }
}

/*
 * Should this be performanced tuned? This is called
 * in resize/reflow. Maybe we should cache the table
 * frame in the table cell frame.
 *
 */
nsTableFrame* nsTableCellFrame::GetTableFrame()
{
  nsIFrame* frame = nsnull;
  nsresult  result;

  result = GetContentParent(frame);             // Get RowFrame
  
  if ((result == NS_OK) && (frame != nsnull))
    frame->GetContentParent(frame);             // Get RowGroupFrame
  
  if ((result == NS_OK) && (frame != nsnull))
   frame->GetContentParent(frame);              // Get TableFrame

  return (nsTableFrame*)frame;
}




/**
  */
NS_METHOD nsTableCellFrame::Reflow(nsIPresContext* aPresContext,
                                   nsReflowMetrics& aDesiredSize,
                                   const nsReflowState& aReflowState,
                                   nsReflowStatus& aStatus)
{
  NS_PRECONDITION(nsnull!=aPresContext, "bad arg");

#ifdef NS_DEBUG
  //PreReflowCheck();
#endif

  // Initialize out parameter
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = 0;
    aDesiredSize.maxElementSize->height = 0;
  }

  aStatus = NS_FRAME_COMPLETE;
  if (gsDebug==PR_TRUE)
    printf("nsTableCellFrame::ResizeReflow: maxSize=%d,%d\n",
           aReflowState.maxSize.width, aReflowState.maxSize.height);

  mFirstContentOffset = mLastContentOffset = 0;

  nsSize availSize(aReflowState.maxSize);
  nsSize maxElementSize;
  nsSize *pMaxElementSize = aDesiredSize.maxElementSize;
  if (NS_UNCONSTRAINEDSIZE==aReflowState.maxSize.width)
    pMaxElementSize = &maxElementSize;
  nscoord x = 0;
  // SEC: what about ascent and decent???

  // Compute the insets (sum of border and padding)
  nsStyleSpacing* spacing =
    (nsStyleSpacing*)mStyleContext->GetData(eStyleStruct_Spacing);

  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);

  nscoord topInset = borderPadding.top;
  nscoord rightInset = borderPadding.right;
  nscoord bottomInset = borderPadding.bottom;
  nscoord leftInset = borderPadding.left;

  // Get the margin information, available space should
  // be reduced accordingly
  nsMargin      margin(0,0,0,0);
  nsTableFrame* tableFrame = GetTableFrame();
  tableFrame->GetCellMarginData(this,margin);

  // reduce available space by insets

  if (NS_UNCONSTRAINEDSIZE!=availSize.width)
    availSize.width -= leftInset+rightInset+margin.left+margin.right;
  if (NS_UNCONSTRAINEDSIZE!=availSize.height)
    availSize.height -= topInset+bottomInset+margin.top+margin.bottom;

  mLastContentIsComplete = PR_TRUE;

  // get frame, creating one if needed
  if (nsnull==mFirstChild)
  {
    CreatePsuedoFrame(aPresContext);
  }

  // Determine the width we have to work with
  nscoord cellWidth = -1;
  if (NS_UNCONSTRAINEDSIZE!=availSize.width)
  {
    /*
    nsStylePosition* kidPosition = (nsStylePosition*)
      mStyleContext->GetData(eStyleStruct_Position);
    switch (kidPosition->mWidth.GetUnit()) {
      case eStyleUnit_Coord:
        cellWidth = kidPosition->mWidth.GetCoordValue();
        break;

      case eStyleUnit_Percent: 
      {
        nscoord tableWidth=0;
        const nsReflowState *tableReflowState = aReflowState.parentReflowState->parentReflowState->parentReflowState;
        nsTableFrame *tableFrame = (nsTableFrame *)(tableReflowState->frame);
        nsIStyleContextPtr tableSC;
        tableFrame->GetStyleContext(aPresContext, tableSC.AssignRef());
        PRBool tableIsAutoWidth = nsTableFrame::TableIsAutoWidth(tableFrame, tableSC, *tableReflowState, tableWidth);
        float percent = kidPosition->mWidth.GetPercentValue();
        cellWidth = (PRInt32)(tableWidth*percent);
        break;
      }

      case eStyleUnit_Inherit:
        // XXX for now, do nothing
      default:
      case eStyleUnit_Auto:
        break;
    }
    */
    if (-1!=cellWidth)
      availSize.width = cellWidth;
  }

  // Try to reflow the child into the available space. It might not
  // fit or might need continuing.
  if (availSize.height < 0)
    availSize.height = 1;
  nsSize maxKidElementSize;
  if (gsDebug==PR_TRUE)
    printf("  nsTableCellFrame::ResizeReflow calling ReflowChild with availSize=%d,%d\n",
           availSize.width, availSize.height);
  nsReflowMetrics kidSize(pMaxElementSize);
  kidSize.width=kidSize.height=kidSize.ascent=kidSize.descent=0;
  nsReflowState kidReflowState(mFirstChild, aReflowState, availSize);
  mFirstChild->WillReflow(*aPresContext);
  aStatus = ReflowChild(mFirstChild, aPresContext, kidSize, kidReflowState);

  if (gsDebug==PR_TRUE)
  {
    if (nsnull!=pMaxElementSize)
      printf("  nsTableCellFrame::ResizeReflow: child returned desiredSize=%d,%d,\
             and maxElementSize=%d,%d\n",
             kidSize.width, kidSize.height,
             pMaxElementSize->width, pMaxElementSize->height);
    else
      printf("  nsTableCellFrame::ResizeReflow: child returned desiredSize=%d,%d,\
             and maxElementSize=nsnull\n",
             kidSize.width, kidSize.height);
  }

  SetFirstContentOffset(mFirstChild);
  //if (gsDebug) printf("CELL: set first content offset to %d\n", GetFirstContentOffset()); //@@@
  SetLastContentOffset(mFirstChild);
  //if (gsDebug) printf("CELL: set last content offset to %d\n", GetLastContentOffset()); //@@@

  // Place the child
  mFirstChild->SetRect(nsRect(leftInset, topInset,
                           kidSize.width, kidSize.height));  
    
  if (NS_FRAME_IS_NOT_COMPLETE(aStatus)) {
    // If the child didn't finish layout then it means that it used
    // up all of our available space (or needs us to split).
    mLastContentIsComplete = PR_FALSE;
  }

  // Return our size and our result

  // first, compute the height
  // the height can be set w/o being restricted by aMaxSize.height
  nscoord cellHeight = kidSize.height;
  if (NS_UNCONSTRAINEDSIZE!=cellHeight)
    cellHeight += topInset + bottomInset;
  // next determine the cell's width
  cellWidth = kidSize.width;  // at this point, we've factored in the cell's style attributes
  if (NS_UNCONSTRAINEDSIZE!=cellWidth)
    cellWidth += leftInset + rightInset;

  // set the cell's desired size and max element size
  aDesiredSize.width = cellWidth;
  aDesiredSize.height = cellHeight;
  aDesiredSize.ascent = topInset;
  aDesiredSize.descent = bottomInset;
  if (nsnull!=aDesiredSize.maxElementSize)
  {
    *aDesiredSize.maxElementSize = *pMaxElementSize;
    aDesiredSize.maxElementSize->height += topInset + bottomInset;
    aDesiredSize.maxElementSize->width += leftInset + rightInset;
  }
  
  if (gsDebug==PR_TRUE)
    printf("  nsTableCellFrame::ResizeReflow returning aDesiredSize=%d,%d\n",
           aDesiredSize.width, aDesiredSize.height);

#ifdef NS_DEBUG
  //PostReflowCheck(result);
#endif

  return NS_OK;
}

NS_METHOD
nsTableCellFrame::CreateContinuingFrame(nsIPresContext*  aPresContext,
                                        nsIFrame*        aParent,
                                        nsIStyleContext* aStyleContext,
                                        nsIFrame*&       aContinuingFrame)
{
  nsTableCellFrame* cf = new nsTableCellFrame(mContent, aParent);
  if (nsnull == cf) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  PrepareContinuingFrame(aPresContext, aParent, aStyleContext, cf);
  aContinuingFrame = cf;
  return NS_OK;
}


/**
  *
  * Update the border style to map to the HTML border style
  *
  */
void nsTableCellFrame::MapHTMLBorderStyle(nsIPresContext* aPresContext, nsStyleSpacing& aSpacingStyle, nscoord aBorderWidth)
{
  nsStyleCoord  width;
  width.SetCoordValue(aBorderWidth);
  aSpacingStyle.mBorder.SetTop(width);
  aSpacingStyle.mBorder.SetLeft(width);
  aSpacingStyle.mBorder.SetBottom(width);
  aSpacingStyle.mBorder.SetRight(width);

  aSpacingStyle.mBorderStyle[NS_SIDE_TOP] = NS_STYLE_BORDER_STYLE_INSET; 
  aSpacingStyle.mBorderStyle[NS_SIDE_LEFT] = NS_STYLE_BORDER_STYLE_INSET; 
  aSpacingStyle.mBorderStyle[NS_SIDE_BOTTOM] = NS_STYLE_BORDER_STYLE_INSET; 
  aSpacingStyle.mBorderStyle[NS_SIDE_RIGHT] = NS_STYLE_BORDER_STYLE_INSET; 
  
  nsTableFrame*     tableFrame = GetTableFrame();
  nsIStyleContext*  styleContext = nsnull;
  
  tableFrame->GetStyleContext(aPresContext,styleContext);
  
  nsStyleColor*   colorData = (nsStyleColor*)styleContext->GetData(eStyleStruct_Color);

   // Look until we find a style context with a NON-transparent background color
  while (styleContext)
  {
    if ((colorData->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)!=0)
    {
      nsIStyleContext* temp = styleContext;
      styleContext = styleContext->GetParent();
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
    borderColor = colorData->mBackgroundColor;

  // if the border color is white, then shift to grey
  if (borderColor == 0xFFFFFFFF)
    borderColor = 0xFFC0C0C0;


  aSpacingStyle.mBorderColor[NS_SIDE_TOP] = 
  aSpacingStyle.mBorderColor[NS_SIDE_LEFT] = 
  aSpacingStyle.mBorderColor[NS_SIDE_BOTTOM] = 
  aSpacingStyle.mBorderColor[NS_SIDE_RIGHT] = borderColor;
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
  
  nsHTMLValue padding_value;
  nsHTMLValue spacing_value;
  nsHTMLValue border_value;

  nsContentAttr padding_result;
  nsContentAttr spacing_result;
  nsContentAttr border_result;

  nscoord   padding = 0;
  nscoord   spacing = 0;
  nscoord   border  = 1;

  float     p2t = aPresContext->GetPixelsToTwips();

  nsTablePart*  table = ((nsTableContent*)mContent)->GetTable();

  NS_ASSERTION(table,"Table Must not be null");
  if (!table)
    return;

  padding_result = table->GetAttribute(nsHTMLAtoms::cellpadding,padding_value);
  spacing_result = table->GetAttribute(nsHTMLAtoms::cellspacing,spacing_value);
  border_result = table->GetAttribute(nsHTMLAtoms::border,border_value);

  nsStyleSpacing* spacingData = (nsStyleSpacing*)mStyleContext->GetData(eStyleStruct_Spacing);

  // check to see if cellpadding or cellspacing is defined
  if (spacing_result == eContentAttr_HasValue || padding_result == eContentAttr_HasValue)
  {
  
    PRInt32 value;
    nsStyleCoord  padding(0);
    nsStyleCoord  spacing(0);

    if (padding_result == eContentAttr_HasValue && ConvertToPixelValue(padding_value,0,value))
      padding.SetCoordValue((nscoord)(p2t*(float)value)); 
    
    if (spacing_result == eContentAttr_HasValue && ConvertToPixelValue(spacing_value,0,value))
      spacing.SetCoordValue((nscoord)(p2t*(float)value)); 

    spacingData->mMargin.SetTop(spacing);
    spacingData->mMargin.SetLeft(spacing);
    spacingData->mMargin.SetBottom(spacing);
    spacingData->mMargin.SetRight(spacing);
    spacingData->mPadding.SetTop(padding);
    spacingData->mPadding.SetLeft(padding);
    spacingData->mPadding.SetBottom(padding);
    spacingData->mPadding.SetRight(padding); 

  }
  if (border_result == eContentAttr_HasValue)
  {
    PRInt32 intValue = 0;

    if (ConvertToPixelValue(border_value,1,intValue))
    {    
      if (intValue > 0)
        intValue = 1;
      border = nscoord(p2t*(float)intValue); 
    }
  }
  MapHTMLBorderStyle(aPresContext, *spacingData,border);
}

void nsTableCellFrame::MapTextAttributes(nsIPresContext* aPresContext)
{
  nsHTMLValue value;

  ((nsTableCell*)mContent)->GetAttribute(nsHTMLAtoms::align, value);
  if (value.GetUnit() == eHTMLUnit_Enumerated) 
  {
    nsStyleText* text = (nsStyleText*)mStyleContext->GetData(eStyleStruct_Text);
    text->mTextAlign = value.GetIntValue();
  }
}



// Subclass hook for style post processing
NS_METHOD nsTableCellFrame::DidSetStyleContext(nsIPresContext* aPresContext)
{
#ifdef NOISY_STYLE
  printf("nsTableCellFrame::DidSetStyleContext \n");
#endif

  MapTextAttributes(aPresContext);
  MapBorderMarginPadding(aPresContext);
  mStyleContext->RecalcAutomaticData(aPresContext);
  return NS_OK;
}



/* ----- static methods ----- */

nsresult nsTableCellFrame::NewFrame(nsIFrame** aInstancePtrResult,
                                    nsIContent* aContent,
                                    nsIFrame*   aParent)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIFrame* it = new nsTableCellFrame(aContent, aParent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aInstancePtrResult = it;
  return NS_OK;
}


// For Debugging ONLY
NS_METHOD nsTableCellFrame::MoveTo(nscoord aX, nscoord aY)
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

NS_METHOD nsTableCellFrame::SizeTo(nscoord aWidth, nscoord aHeight)
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

