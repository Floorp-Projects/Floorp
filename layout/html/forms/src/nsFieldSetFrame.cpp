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

// YY need to pass isMultiple before create called

//#include "nsFormControlFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsLegendFrame.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLFieldSetElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsCSSRendering.h"
//#include "nsIDOMHTMLCollection.h"
#include "nsIContent.h"
//#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLParts.h"
//#include "nsIRadioButton.h"
//#include "nsWidgetsCID.h"
//#include "nsSize.h"
#include "nsHTMLAtoms.h"
//#include "nsIView.h"
//#include "nsIListWidget.h"
//#include "nsIComboBox.h"
//#include "nsIListBox.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsStyleUtil.h"
#include "nsFont.h"

static NS_DEFINE_IID(kLegendFrameCID, NS_LEGEND_FRAME_CID);
static NS_DEFINE_IID(kIDOMHTMLFieldSetElementIID, NS_IDOMHTMLFIELDSETELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLLegendElementIID, NS_IDOMHTMLLEGENDELEMENT_IID);
 
class nsLegendFrame;

class nsFieldSetFrame : public nsHTMLContainerFrame {
public:

  nsFieldSetFrame();

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD Reflow(nsIPresContext& aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
                               
  NS_METHOD Paint(nsIPresContext& aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect& aDirtyRect,
                  nsFramePaintLayer aWhichLayer);

  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("FieldSet", aResult);
  }

protected:

  virtual PRIntn GetSkipSides() const;
  //virtual void GetDesiredSize(nsIPresContext* aPresContext,
  //                            const nsReflowState& aReflowState,
  //                            nsReflowMetrics& aDesiredLayoutSize,
  //                            nsSize& aDesiredWidgetSize);

  nsIFrame* mLegendFrame;
  nsIFrame* mContentFrame;
  nsRect    mTopBorderGap;
  PRInt32   mTopBorderOffset;
  PRBool    mInline;
};

nsresult
NS_NewFieldSetFrame(nsIFrame*& aResult)
{
  aResult = new nsFieldSetFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsFieldSetFrame::nsFieldSetFrame()
  : nsHTMLContainerFrame()
{
  mContentFrame       = nsnull;
  mLegendFrame     = nsnull;
  mTopBorderGap    = nsRect(0,0,0,0);
  mTopBorderOffset = 0;
  mInline          = PR_TRUE;
}

NS_IMETHODIMP
nsFieldSetFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aChildList)
{
  // cache our display type
  const nsStyleDisplay* styleDisplay;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
  mInline = (NS_STYLE_DISPLAY_BLOCK != styleDisplay->mDisplay);

  PRUint8 flags = (mInline) ? NS_BLOCK_SHRINK_WRAP : 0;
  NS_NewAreaFrame(mContentFrame, flags);
  mFrames.SetFrames(mContentFrame);

  // Resolve style and initialize the frame
  nsIStyleContext* styleContext;
  aPresContext.ResolvePseudoStyleContextFor(mContent, 
                                            nsHTMLAtoms::fieldsetContentPseudo,
                                            mStyleContext, PR_FALSE,
                                            &styleContext);
  mFrames.FirstChild()->Init(aPresContext, mContent, this, styleContext, nsnull);
  NS_RELEASE(styleContext);                                           

  nsIFrame* newChildList = aChildList;

  // XXX this just tosses any extra frames passed in onto the floor;
  // this is a memory leak!!!
  mFrames.FirstChild()->SetNextSibling(nsnull);

  // Set the geometric and content parent for each of the child frames. 
  // The legend is handled differently and removed from aChildList
  nsIFrame* lastFrame = nsnull;
  for (nsIFrame* frame = aChildList; nsnull != frame;) {
    nsIFrame* legendFrame = nsnull;
    nsresult result = frame->QueryInterface(kLegendFrameCID, (void**)&legendFrame);
    if ((NS_OK == result) && legendFrame) {
      nsIFrame* nextFrame;
      frame->GetNextSibling(&nextFrame);
      if (lastFrame) {
        lastFrame->SetNextSibling(nextFrame);
      } else {
        newChildList = nextFrame;
      }
      frame->SetParent(this);
      mFrames.FirstChild()->SetNextSibling(frame);
      mLegendFrame = frame;
      mLegendFrame->SetNextSibling(nsnull);
      frame = nextFrame;
     } else {
      frame->SetParent(mFrames.FirstChild());
      frame->GetNextSibling(&frame);
    }
    lastFrame = frame;
  }

  // Queue up the frames for the content frame
  return mFrames.FirstChild()->SetInitialChildList(aPresContext, nsnull, newChildList);
}

// this is identical to nsHTMLContainerFrame::Paint except for the background and border. 
NS_IMETHODIMP
nsFieldSetFrame::Paint(nsIPresContext& aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect& aDirtyRect,
                       nsFramePaintLayer aWhichLayer)
{
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    // Paint our background and border
    const nsStyleDisplay* disp =
      (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

    if (disp->mVisible && mRect.width && mRect.height) {
      PRIntn skipSides = GetSkipSides();
      const nsStyleColor* color =
        (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
      const nsStyleSpacing* spacing =
        (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);

//XXX      nsRect backgroundRect(0, 0, mRect.width, mRect.height);
      // XXX our parent doesn't account for top and bottom margins yet, if we are inline
      if (mInline) {
        nsMargin margin;
        spacing->CalcMarginFor(this, margin); 
        nsRect rect(0, mTopBorderOffset, mRect.width, mRect.height - margin.top - 
                    margin.bottom - mTopBorderOffset);
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *spacing, 0, 0);
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *spacing, mStyleContext, skipSides, &mTopBorderGap);
      } else {
        nsRect rect(0, mTopBorderOffset, mRect.width, mRect.height - mTopBorderOffset);
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *spacing, 0, 0);
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *spacing, mStyleContext, skipSides, &mTopBorderGap);
      } 
    }
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    nsIView* view;
    GetView(&view);
    if (nsnull != view) {
      aRenderingContext.SetColor(NS_RGB(0,0,255));
    }
    else {
      aRenderingContext.SetColor(NS_RGB(255,0,0));
    }
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
  return NS_OK;
}

// XXX a hack until the reflow state does this correctly
// XXX when it gets fixed, leave in the printf statements or add an assertion
static
void FieldSetHack(nsHTMLReflowState& aReflowState, char* aMessage, PRBool aUseMax)
{
  if (aReflowState.computedWidth == 0) {
    aReflowState.computedWidth = aReflowState.availableWidth;
  }
  if ((aReflowState.computedWidth != NS_INTRINSICSIZE) &&
      (aReflowState.availableWidth > 0)) {
//    printf("BUG - %s has a computed width = %d, available width = %d \n", 
//      aMessage, aReflowState.computedWidth, aReflowState.availableWidth);
    if (aUseMax) {
      aReflowState.computedWidth = PR_MAX(aReflowState.computedWidth,aReflowState.availableWidth);
    } else {
      aReflowState.computedWidth = PR_MIN(aReflowState.computedWidth,aReflowState.availableWidth);
    }
  }
  if (aReflowState.computedHeight == 0) {
    aReflowState.computedHeight = aReflowState.availableHeight;
  }
  if ((aReflowState.computedHeight != NS_INTRINSICSIZE) &&
      (aReflowState.availableHeight > 0)) {
//    printf("BUG - %s has a computed height = %d, available height = %d \n", 
//      aMessage, aReflowState.computedHeight, aReflowState.availableHeight);
    if (aUseMax) {
      aReflowState.computedHeight = PR_MAX(aReflowState.computedHeight,aReflowState.availableHeight);
    } else {
      aReflowState.computedHeight = PR_MIN(aReflowState.computedHeight,aReflowState.availableHeight);
    }
  }
}


// XXX currently only supports legend align=left,center,right
#define DESIRED_LEGEND_OFFSET 10
NS_IMETHODIMP 
nsFieldSetFrame::Reflow(nsIPresContext&          aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  // XXX remove the following when the reflow state is fixed
  FieldSetHack((nsHTMLReflowState&)aReflowState, "fieldset", PR_TRUE);
  // availSize could have unconstrained values, don't perform any addition on them
  nsSize availSize(aReflowState.computedWidth, aReflowState.computedHeight);
  
  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);
  const PRInt32 desiredLegendOffset = NSIntPixelsToTwips(DESIRED_LEGEND_OFFSET, p2t);

  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);

  nsMargin margin;
  spacing->CalcMarginFor(this, margin); 

  nsMargin border;
  nsMargin padding;
  spacing->CalcBorderFor(this, border);
  spacing->CalcPaddingFor(this, padding);
  nsMargin borderPadding = border + padding;

  nsMargin legendMargin;
  if (mLegendFrame) {
    nsIStyleContext* legendSC = nsnull;
    mLegendFrame->GetStyleContext(&legendSC);
    if (legendSC) {
      const nsStyleSpacing* legendSpacing =
        (const nsStyleSpacing*)legendSC->GetStyleData(eStyleStruct_Spacing);
      legendSpacing->CalcMarginFor(mLegendFrame, legendMargin);
      NS_RELEASE(legendSC);
    }
  }

  // reduce available space by border, padding, etc. if we're in a constrained situation
  nscoord horTaken = borderPadding.left + borderPadding.right + legendMargin.left + legendMargin.right;
  nscoord verTaken = padding.top + borderPadding.bottom + legendMargin.top + legendMargin.bottom;

  if (NS_INTRINSICSIZE != availSize.width) {
    availSize.width -= horTaken;
    availSize.width = PR_MAX(availSize.width,0);
  }
  if (NS_AUTOHEIGHT != availSize.height) {
    // XXX this assumes that the legend is taller than the top border width
    availSize.height -= verTaken;
    availSize.height = PR_MAX(availSize.height,0);
  }

  nsSize maxElementSize(0,0);

  // Try to reflow the legend into the available space. It might not fit
  nsSize legendSize(0,0);
  if (mLegendFrame) {
    nsHTMLReflowState legendReflowState(aPresContext, aReflowState,
                                        mLegendFrame, availSize);
    // XXX remove when reflow state is fixed
    FieldSetHack((nsHTMLReflowState&)legendReflowState, "fieldset's legend", PR_FALSE);
    ReflowChild(mLegendFrame, aPresContext, aDesiredSize, legendReflowState, aStatus);

    legendSize.width  = aDesiredSize.width;
    legendSize.height = aDesiredSize.height;

    // The legend didn't fit
    if ((NS_INTRINSICSIZE != availSize.width) && 
        (availSize.width < aDesiredSize.width + legendMargin.left + legendMargin.right)) {
      availSize.width = aDesiredSize.width + horTaken;
    } 
    if (NS_AUTOHEIGHT != availSize.height) {
      if (availSize.height < aDesiredSize.height) {
        availSize.height = 0;
      } else {
        availSize.height -= aDesiredSize.height;
      }
    }
    if (aDesiredSize.maxElementSize) {
      maxElementSize.width  = aDesiredSize.maxElementSize->width;
      maxElementSize.height = aDesiredSize.maxElementSize->height;
    }
  }

  PRBool needAnotherLegendReflow = PR_FALSE;

  // Try to reflow the area frame into the available space. It might not fit
  nsHTMLReflowState contentReflowState(aPresContext, aReflowState,
                                       mContentFrame, availSize);
  // XXX remove when reflow state is fixed
  FieldSetHack(contentReflowState, "fieldset's area", PR_FALSE);

  nscoord contentTopOffset = (legendSize.height > border.top) 
                                ? legendSize.height + padding.top
                                : border.top + padding.top;
  ReflowChild(mContentFrame, aPresContext, aDesiredSize, contentReflowState, aStatus);

  nsSize contentSize(aDesiredSize.width, aDesiredSize.height);

  // The content didn't fit
  if ((NS_UNCONSTRAINEDSIZE != availSize.width) && (availSize.width < aDesiredSize.width)) {
    needAnotherLegendReflow = PR_TRUE;
    availSize.width = contentSize.width + borderPadding.left + borderPadding.right;
  }
  if ((NS_UNCONSTRAINEDSIZE != availSize.height) && (availSize.height < aDesiredSize.height)) {
    needAnotherLegendReflow = PR_TRUE;
  }
  if (aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = PR_MAX(aDesiredSize.maxElementSize->width,maxElementSize.width);
    maxElementSize.height += aDesiredSize.maxElementSize->height;
  }

  // need to reflow the legend a 2nd time
  if (needAnotherLegendReflow && mLegendFrame) {
    nsHTMLReflowState legendReflowState(aPresContext, aReflowState,
                                        mLegendFrame, availSize);
    // XXX remove when reflow state is fixed
    FieldSetHack(legendReflowState, "fieldset's legend frame", PR_FALSE);
    ReflowChild(mLegendFrame, aPresContext, aDesiredSize, legendReflowState, aStatus);

    legendSize.width  = aDesiredSize.width;
    legendSize.height = aDesiredSize.height;
  }

  nscoord legendWidth = legendSize.width + border.left + border.right;
  nscoord contentWidth = contentSize.width + borderPadding.left + borderPadding.right;

  PRBool legendOffsetFits = PR_TRUE; // can the legend be offset by the 10 pixels

  aDesiredSize.width = (legendWidth > contentWidth) ? legendWidth : contentWidth;

  // if we are constrained and the child is smaller, use the constrained values
  //if ((NS_INTRINSICSIZE != aReflowState.computedWidth) &&
  //    (aReflowState.computedWidth > aDesiredSize.width)) {
  //  aDesiredSize.width = aReflowState.computedWidth;
  //}
  
  if ((NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth) && 
      ( (legendWidth + (2 * desiredLegendOffset)) > aDesiredSize.width) ) {
    legendOffsetFits = PR_FALSE;
  }

  // Place the legend 
  nsRect legendRect(0, 0, 0, 0);
  contentTopOffset = border.top + padding.top;
  if (mLegendFrame) {
    nscoord legendTopOffset;
    if (legendSize.height > border.top) {
      legendTopOffset = 0;
    } else {
      legendTopOffset = (border.top - legendSize.height) / 2;
    }
    contentTopOffset = legendTopOffset + legendSize.height + padding.top;

    nscoord legendLeftOffset = 0;
    PRInt32 align = ((nsLegendFrame*)mLegendFrame)->GetAlign();

    // if there isn't room for the 10 pixel left/right offset, align center
    if (!legendOffsetFits) {
      align = NS_STYLE_TEXT_ALIGN_CENTER;
    }

    switch(align) {
    case NS_STYLE_TEXT_ALIGN_LEFT:
      legendLeftOffset = border.left + desiredLegendOffset;
      break;
    case NS_STYLE_TEXT_ALIGN_RIGHT:
      legendLeftOffset = aDesiredSize.width - border.right - desiredLegendOffset - legendSize.width;
      break;
    // XXX The spec specifies center and top but we are following IE4's lead and making left
    // be the upper left and right be the upper right. IE4 introduced center which is not part 
    // of the spec.
    case NS_STYLE_TEXT_ALIGN_CENTER:
    case NS_STYLE_VERTICAL_ALIGN_BOTTOM:
    case NS_STYLE_VERTICAL_ALIGN_TOP:
    default:
      legendLeftOffset = (aDesiredSize.width - legendSize.width) / 2;
      break;
    }

    // reduce by legend margins
    legendRect = nsRect(legendLeftOffset + legendMargin.left, legendTopOffset + legendMargin.top, 
                        legendSize.width, legendSize.height);
    mLegendFrame->SetRect(legendRect);

    // cache values so the border will be painted around the vertical center of the legend
    mTopBorderOffset = legendSize.height / 2;
    mTopBorderGap = nsRect(legendLeftOffset, legendTopOffset, legendSize.width + legendMargin.left +
                           legendMargin.right, legendSize.height + legendMargin.top +
                           legendMargin.bottom);
  }

  // Place the content area frame
  nsRect contentRect(borderPadding.left, contentTopOffset, contentSize.width, contentSize.height);
  mContentFrame->SetRect(contentRect);
    
  // Return our size and our result
  aDesiredSize.height  = contentTopOffset + legendSize.height + contentSize.height + borderPadding.bottom;

  // if we are constrained and the child is smaller, use the constrained values
  //if ((NS_AUTOHEIGHT != aReflowState.computedHeight) && (aReflowState.computedHeight > aDesiredSize.height)) {
  //  aDesiredSize.height = aReflowState.computedHeight;
  //}
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width  = maxElementSize.width;
    aDesiredSize.maxElementSize->height = maxElementSize.height;
    aDesiredSize.AddBorderPaddingToMaxElementSize(borderPadding);
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

PRIntn
nsFieldSetFrame::GetSkipSides() const
{
  return 0;
}

