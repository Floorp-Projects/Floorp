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

  void SetMaxElementSize(nsSize& maxSize, nsSize* aSize);
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

  PRUint8 flags = (mInline) ? NS_BODY_SHRINK_WRAP : 0;
  NS_NewBodyFrame(mFirstChild, flags);
  mContentFrame = mFirstChild;

  // Resolve style and initialize the frame
  nsIStyleContext* styleContext = aPresContext.ResolvePseudoStyleContextFor(mContent, 
                                                                            nsHTMLAtoms::fieldsetContentPseudo,
                                                                            mStyleContext);
  mFirstChild->Init(aPresContext, mContent, this, styleContext);
  NS_RELEASE(styleContext);                                           

  nsIFrame* newChildList = aChildList;

  mFirstChild->SetNextSibling(nsnull);
  // Set the geometric and content parent for each of the child frames. 
  // The legend is handled differently and removed from aChildList
  nsIFrame* lastFrame = nsnull;
  for (nsIFrame* frame = aChildList; nsnull != frame;) {
    nsIFrame* legendFrame = nsnull;
    nsresult result = frame->QueryInterface(kLegendFrameCID, (void**)&legendFrame);
    if ((NS_OK == result) && legendFrame) {
      nsIFrame* nextFrame;
      frame->GetNextSibling(nextFrame);
      if (lastFrame) {
        lastFrame->SetNextSibling(nextFrame);
      } else {
        newChildList = nextFrame;
      }
      frame->SetGeometricParent(this);
      frame->SetContentParent(this);
      mFirstChild->SetNextSibling(frame);
      mLegendFrame = frame;
      mLegendFrame->SetNextSibling(nsnull);
      frame = nextFrame;
     } else {
      frame->SetGeometricParent(mFirstChild);
      frame->SetContentParent(mFirstChild);
      frame->GetNextSibling(frame);
    }
    lastFrame = frame;
  }

  // Queue up the frames for the content frame
  return mFirstChild->SetInitialChildList(aPresContext, nsnull, newChildList);
}

// this is identical to nsHTMLContainerFrame::Paint except for the background and border. 
NS_IMETHODIMP
nsFieldSetFrame::Paint(nsIPresContext& aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       const nsRect& aDirtyRect,
                       nsFramePaintLayer aWhichLayer)
{
  if (eFramePaintLayer_Underlay == aWhichLayer) {
    // Paint our background and border
    const nsStyleDisplay* disp =
      (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);

    if (disp->mVisible && mRect.width && mRect.height) {
      PRIntn skipSides = GetSkipSides();
      const nsStyleColor* color =
        (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
      const nsStyleSpacing* spacing =
        (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);

      nsRect backgroundRect(0, 0, mRect.width, mRect.height);
      // XXX our parent doesn't account for top and bottom margins yet, if we are inline
      if (mInline) {
        nsMargin margin;
        spacing->CalcMarginFor(this, margin); 
        nsRect rect(0, mTopBorderOffset, mRect.width, mRect.height - margin.top - 
                    margin.bottom - mTopBorderOffset);
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, 0, 0);
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *spacing, skipSides, &mTopBorderGap);
      } else {
        nsRect rect(0, mTopBorderOffset, mRect.width, mRect.height - mTopBorderOffset);
        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, 0, 0);
        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                    aDirtyRect, rect, *spacing, skipSides, &mTopBorderGap);
      } 
    }
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

  if ((eFramePaintLayer_Overlay == aWhichLayer) && GetShowFrameBorders()) {
    nsIView* view;
    GetView(view);
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

void
nsFieldSetFrame::SetMaxElementSize(nsSize& maxSize, nsSize* aSize)
{
  if (aSize) {
    maxSize.width  = (maxSize.width  >= aSize->width) ? maxSize.width  : aSize->width;
    maxSize.height = (maxSize.height >= aSize->width) ? maxSize.height : aSize->height;
  }
}

// XXX currently only supports legend align=left,center,right
#define MIN_TOP_BORDER 10
NS_IMETHODIMP 
nsFieldSetFrame::Reflow(nsIPresContext& aPresContext,
                        nsHTMLReflowMetrics& aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus& aStatus)
{
  nsSize availSize(aReflowState.maxSize);
  float p2t;
  aPresContext.GetScaledPixelsToTwips(p2t);
  const PRInt32 minTopBorder = NSIntPixelsToTwips(MIN_TOP_BORDER, p2t);

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
    mLegendFrame->GetStyleContext(legendSC);
    if (legendSC) {
      const nsStyleSpacing* legendSpacing =
        (const nsStyleSpacing*)legendSC->GetStyleData(eStyleStruct_Spacing);
      legendSpacing->CalcMarginFor(mLegendFrame, legendMargin);
      NS_RELEASE(legendSC);
    }
  }

  // reduce available space by border, padding, etc. if we're in a constrained situation
  nscoord horTaken = borderPadding.left + borderPadding.right + (2 * minTopBorder) + 
                     legendMargin.left + legendMargin.right;
  nscoord verTaken = padding.top + borderPadding.bottom + legendMargin.top + legendMargin.bottom;
  if (aReflowState.HaveFixedContentWidth()) {
    availSize.width = aReflowState.minWidth;
  }
  else {
    if (NS_UNCONSTRAINEDSIZE != availSize.width)
      availSize.width -= horTaken;
  }
  if (NS_UNCONSTRAINEDSIZE != availSize.height) {
    // XXX this assumes that the legend is taller than the top border width
    availSize.height -= verTaken;
    if (mInline) { // XXX parents don't account for inline top, bottom margins yet
      availSize.height -= margin.top;
    }
  }
  if (availSize.height < 0) 
    availSize.height = 1;

  nsSize maxElementSize(0,0);
  nsIHTMLReflow* htmlReflow = nsnull;

  // Try to reflow the legend into the available space. It might not fit
  nsSize legendSize(0,0);
  if (mLegendFrame) {
    nsHTMLReflowState legendReflowState(aPresContext, mLegendFrame,
                                        aReflowState, availSize);
    ReflowChild(mLegendFrame, aPresContext, aDesiredSize, legendReflowState, aStatus);

    legendSize.width  = aDesiredSize.width;
    legendSize.height = aDesiredSize.height;

    // The legend didn't fit
    if ((NS_UNCONSTRAINEDSIZE != availSize.width) && 
        (availSize.width < aDesiredSize.width + legendMargin.left + legendMargin.right)) {
      availSize.width = aDesiredSize.width + horTaken;
    } 
    if ((NS_UNCONSTRAINEDSIZE != availSize.height) && (availSize.height < aDesiredSize.height)) {
      availSize.height = 0;
    } else if (NS_UNCONSTRAINEDSIZE != availSize.height) {
      availSize.height -= aDesiredSize.height;
    }
    SetMaxElementSize(maxElementSize, aDesiredSize.maxElementSize);
  }

  PRBool needAnotherLegendReflow = PR_FALSE;

  // Try to reflow the content frame into the available space. It might not fit
  nsSize contentSize(0,0);
  nsHTMLReflowState contentReflowState(aPresContext, mContentFrame,
                                       aReflowState, availSize);
  nscoord contentTopOffset = (legendSize.height > border.top) 
    ? legendSize.height + padding.top
    : border.top + padding.top;
  ReflowChild(mContentFrame, aPresContext, aDesiredSize, contentReflowState, aStatus);

  contentSize.width  = aDesiredSize.width;
  contentSize.height = aDesiredSize.height;

  // The content didn't fit
  if ((NS_UNCONSTRAINEDSIZE != availSize.width) && (availSize.width < aDesiredSize.width)) {
    needAnotherLegendReflow = PR_TRUE;
    availSize.width = contentSize.width + borderPadding.left + borderPadding.right;
  }
  if ((NS_UNCONSTRAINEDSIZE != availSize.height) && (availSize.height < aDesiredSize.height)) {
    needAnotherLegendReflow = PR_TRUE;
  }
  SetMaxElementSize(maxElementSize, aDesiredSize.maxElementSize);

  // need to reflow the legend a 2nd time
  if (needAnotherLegendReflow && mLegendFrame) {
    nsHTMLReflowState legendReflowState(aPresContext, mLegendFrame,
                                        aReflowState, availSize);
    ReflowChild(mLegendFrame, aPresContext, aDesiredSize, legendReflowState, aStatus);

    legendSize.width  = aDesiredSize.width;
    legendSize.height = aDesiredSize.height;
  }

  nscoord legendWidth = legendSize.width + border.left + border.right + (2 * minTopBorder);
  nscoord contentWidth = contentSize.width + borderPadding.left + borderPadding.right;

  aDesiredSize.width = (legendWidth > contentWidth) ? legendWidth : contentWidth;
  if (aReflowState.HaveFixedContentWidth() && (aReflowState.minWidth > aDesiredSize.width)) {
    aDesiredSize.width = aReflowState.minWidth;
  }

  // Place the legend 
  nsRect legendRect(0, 0, 0, 0);
  contentTopOffset = border.top + padding.top;
  if (mInline) // XXX parents don't handle inline top, bottom margins
    contentTopOffset += margin.top;
  if (mLegendFrame) {
    nscoord legendTopOffset;
    if (legendSize.height > border.top) {
      legendTopOffset = (mInline) ? margin.top : 0; // XXX parents don't .......
    } else {
      legendTopOffset = (border.top - legendSize.height) / 2;
      if (mInline)  // XXX parents don't ......
        legendTopOffset += margin.top;
    }
    contentTopOffset = legendTopOffset + legendSize.height + padding.top;

    nscoord legendLeftOffset = 0;
    PRInt32 align = ((nsLegendFrame*)mLegendFrame)->GetAlign();

    switch(align) {
    case NS_STYLE_TEXT_ALIGN_LEFT:
      legendLeftOffset = border.left + minTopBorder;
      break;
    case NS_STYLE_TEXT_ALIGN_RIGHT:
      legendLeftOffset = aDesiredSize.width - border.right - minTopBorder - legendSize.width;
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
    if (mInline)      // XXX parents don't yet .....
      mTopBorderOffset += margin.top;
    mTopBorderGap = nsRect(legendLeftOffset, legendTopOffset, legendSize.width + legendMargin.left +
                           legendMargin.right, legendSize.height + legendMargin.top +
                           legendMargin.bottom);
  }

  // Place the content frame
  nsRect contentRect(borderPadding.left, contentTopOffset, contentSize.width, contentSize.height);
  mContentFrame->SetRect(contentRect);
    
  // Return our size and our result
  aDesiredSize.height  = contentTopOffset + contentSize.height + borderPadding.bottom;
  if (mInline) // XXX parents don't yet ...... 
    aDesiredSize.height += margin.bottom;
  if (aReflowState.HaveFixedContentHeight() && (aReflowState.minHeight > aDesiredSize.height)) {
    aDesiredSize.height = aReflowState.minHeight;
  }
  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.AddBorderPaddingToMaxElementSize(borderPadding);
    aDesiredSize.maxElementSize->width  = maxElementSize.width;
    aDesiredSize.maxElementSize->height = maxElementSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

PRIntn
nsFieldSetFrame::GetSkipSides() const
{
  return 0;
}

