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
#include "nsCellLayoutData.h"
#include "nsBodyFrame.h"
#include "nsIReflowCommand.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsCSSRendering.h"
#include "nsIContent.h"
#include "nsIContentDelegate.h"
#include "nsCSSLayout.h"
#include "nsHTMLValue.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLIIDs.h"
#include "nsIPtr.h"
#include "nsIView.h"
// dependancy on content
#include "nsHTMLTagContent.h"

NS_DEF_PTR(nsIStyleContext);

const nsIID kTableCellFrameCID = NS_TABLECELLFRAME_CID;

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsDebugNT = PR_FALSE;
//#define   NOISY_STYLE
//#define NOISY_FLOW
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsDebugNT = PR_FALSE;
#endif

/**
  */
nsTableCellFrame::nsTableCellFrame(nsIContent* aContent,
                                   nsIFrame*   aParentFrame)
  : nsContainerFrame(aContent, aParentFrame),
  mCellLayoutData(nsnull)
{
  mRowSpan=1;
  mColSpan=1;
  mColIndex=0;
}

nsTableCellFrame::~nsTableCellFrame()
{
  if (nsnull!=mCellLayoutData)
    delete mCellLayoutData;
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

    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                    aDirtyRect, mRect, *myColor);

    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                aDirtyRect, mRect, *mySpacing, 0);
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
  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT)
    printf("%p nsTableCellFrame::Reflow: maxSize=%d,%d\n",
           this, aReflowState.maxSize.width, aReflowState.maxSize.height);

  mFirstContentOffset = mLastContentOffset = 0;

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

  // Get the margin information, available space should
  // be reduced accordingly
  nsMargin      margin(0,0,0,0);
  nsTableFrame* tableFrame = GetTableFrame();
  tableFrame->GetCellMarginData(this,margin);

  mLastContentIsComplete = PR_TRUE;

  // get frame, creating one if needed
  if (nsnull==mFirstChild)
  {
    CreatePsuedoFrame(aPresContext);
  }

  // reduce available space by insets

  if (NS_UNCONSTRAINEDSIZE!=availSize.width)
    availSize.width -= leftInset+rightInset+margin.left+margin.right;
  if (NS_UNCONSTRAINEDSIZE!=availSize.height)
    availSize.height -= topInset+bottomInset+margin.top+margin.bottom;

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
  mFirstChild->MoveTo(leftInset, topInset);
  aStatus = ReflowChild(mFirstChild, aPresContext, kidSize, kidReflowState);

  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT)
  {
    if (nsnull!=pMaxElementSize)
      printf("  %p cellFrame child returned desiredSize=%d,%d,\
             and maxElementSize=%d,%d\n",
             this, kidSize.width, kidSize.height,
             pMaxElementSize->width, pMaxElementSize->height);
    else
      printf("  %p cellFrame child returned desiredSize=%d,%d,\
             and maxElementSize=nsnull\n",
             this, kidSize.width, kidSize.height);
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
  {
    cellHeight += topInset + bottomInset;
  }
  if (PR_TRUE==gsDebugNT)
    printf("  %p cellFrame height set to %d from kidSize=%d and insets %d,%d\n",
             this, cellHeight, kidSize.height, topInset, bottomInset);
  // next determine the cell's width
  nscoord cellWidth = kidSize.width;  // at this point, we've factored in the cell's style attributes
  if (NS_UNCONSTRAINEDSIZE!=cellWidth)
    cellWidth += leftInset + rightInset;
  // Nav4 hack for 0 width cells.  If the cell has any content, it must have a desired width of at least 1
  /*
  if (0==cellWidth)
  {
    PRInt32 childCount;
    mFirstChild->ChildCount(childCount);
    if (0!=childCount)
    {
      nsIFrame *grandChild;
      mFirstChild->FirstChild(grandChild);
      grandChild->ChildCount(childCount);
      if (0!=childCount)
      {
        cellWidth=1;
        if (nsnull!=aDesiredSize.maxElementSize && 0==pMaxElementSize->width)
          pMaxElementSize->width=1;
      }
    }
  }
  */
  // end Nav4 hack for 0 width cells

  // set the cell's desired size and max element size
  aDesiredSize.width = cellWidth;
  aDesiredSize.height = cellHeight;
  aDesiredSize.ascent = topInset;
  aDesiredSize.descent = bottomInset;
  if (nsnull!=aDesiredSize.maxElementSize)
  {
    *aDesiredSize.maxElementSize = *pMaxElementSize;
    if (NS_UNCONSTRAINEDSIZE != aDesiredSize.maxElementSize->height)
      aDesiredSize.maxElementSize->height += topInset + bottomInset;
    if (NS_UNCONSTRAINEDSIZE != aDesiredSize.maxElementSize->width)
      aDesiredSize.maxElementSize->width += leftInset + rightInset;
  }
  
  if (PR_TRUE==gsDebug || PR_TRUE==gsDebugNT)
    printf("  %p cellFrame returning aDesiredSize=%d,%d\n",
           this, aDesiredSize.width, aDesiredSize.height);

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

  aSpacingStyle.mBorderStyle[NS_SIDE_TOP] = NS_STYLE_BORDER_STYLE_SOLID; 
  aSpacingStyle.mBorderStyle[NS_SIDE_LEFT] = NS_STYLE_BORDER_STYLE_SOLID; 
  aSpacingStyle.mBorderStyle[NS_SIDE_BOTTOM] = NS_STYLE_BORDER_STYLE_SOLID; 
  aSpacingStyle.mBorderStyle[NS_SIDE_RIGHT] = NS_STYLE_BORDER_STYLE_SOLID; 
  
  nsTableFrame*     tableFrame = GetTableFrame();
  nsIStyleContext*  styleContext = nsnull;
  
  tableFrame->GetStyleContext(aPresContext,styleContext);
  
  const nsStyleColor*   colorData = (const nsStyleColor*)styleContext->GetStyleData(eStyleStruct_Color);

   // Look until we find a style context with a NON-transparent background color
  while (styleContext)
  {
    if ((colorData->mBackgroundFlags & NS_STYLE_BG_COLOR_TRANSPARENT)!=0)
    {
      nsIStyleContext* temp = styleContext;
      styleContext = styleContext->GetParent();
      NS_RELEASE(temp);
      colorData = (const nsStyleColor*)styleContext->GetStyleData(eStyleStruct_Color);
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

  nscoord   padding = 0;
  nscoord   spacing = 0;
  nscoord   border  = 1;

  float     p2t = aPresContext->GetPixelsToTwips();

  nsTableFrame*  tableFrame = GetTableFrame();
  tableFrame->GetGeometricParent((nsIFrame *&)tableFrame); // get the outer frame
  NS_ASSERTION(tableFrame,"Table Must not be null");
  if (!tableFrame)
    return;

  // get the table frame style context, and from it get cellpadding, cellspacing, and border info
  nsIStyleContextPtr tableSC;
  tableFrame->GetStyleContext(aPresContext, tableSC.AssignRef());
  nsStyleTable* tableStyle = (nsStyleTable*)tableSC->GetStyleData(eStyleStruct_Table);
  nsStyleSpacing* tableSpacingStyle = (nsStyleSpacing*)tableSC->GetStyleData(eStyleStruct_Spacing);
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
  if (tableSpacingStyle->mBorder.GetTopUnit() == eStyleUnit_Coord)
  {
    nsStyleCoord borderWidth;
    tableSpacingStyle->mBorder.GetTop(borderWidth);
    if (0!=borderWidth.GetCoordValue())
    {
      // in HTML, cell borders are always 1 pixel by default
      border = (nscoord)(1*(aPresContext->GetPixelsToTwips()));
      MapHTMLBorderStyle(aPresContext, *spacingData, border);
    }
  }
  
}

void nsTableCellFrame::MapTextAttributes(nsIPresContext* aPresContext)
{
  nsHTMLValue value;

  ((nsHTMLTagContent*)mContent)->GetAttribute(nsHTMLAtoms::align, value);
  if (value.GetUnit() == eHTMLUnit_Enumerated) 
  {
    nsStyleText* text = (nsStyleText*)mStyleContext->GetMutableStyleData(eStyleStruct_Text);
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


NS_METHOD
nsTableCellFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kTableCellFrameCID)) {
    *aInstancePtr = (void*) (this);
    return NS_OK;
  }
  return nsContainerFrame::QueryInterface(aIID, aInstancePtr);
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

