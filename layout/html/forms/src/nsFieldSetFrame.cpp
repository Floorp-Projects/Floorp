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
#include "nsCOMPtr.h"

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

  nsIFrame* mLegendFrame;
  nsIFrame* mContentFrame;
  nsRect    mLegendRect;
  nscoord   mLegendSpace;
};

nsresult
NS_NewFieldSetFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsFieldSetFrame* it = new nsFieldSetFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsFieldSetFrame::nsFieldSetFrame()
  : nsHTMLContainerFrame()
{
  mContentFrame    = nsnull;
  mLegendFrame     = nsnull;
  mLegendSpace   = 0;
}

NS_IMETHODIMP
nsFieldSetFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aChildList)
{
  // cache our display type
  const nsStyleDisplay* styleDisplay;
  GetStyleData(eStyleStruct_Display, (const nsStyleStruct*&) styleDisplay);
  
  PRUint8 flags = (NS_STYLE_DISPLAY_BLOCK != styleDisplay->mDisplay) ? NS_BLOCK_SHRINK_WRAP : 0;
  NS_NewAreaFrame(&mContentFrame, flags);
  mFrames.SetFrames(mContentFrame);

  // Resolve style and initialize the frame
  nsIStyleContext* styleContext;
  aPresContext.ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::fieldsetContentPseudo,
                                            mStyleContext, PR_FALSE, &styleContext);
  mFrames.FirstChild()->Init(aPresContext, mContent, this, styleContext, nsnull);
  mFrames.FirstChild()->SetNextSibling(nsnull);
  NS_RELEASE(styleContext);                                           

  nsIFrame* newChildList = aChildList;

  // Set the geometric and content parent for each of the child frames 
  // that will go into the area frame's child list.
  // The legend frame does not go into the list
  nsIFrame* lastNewFrame = nsnull;
  for (nsIFrame* frame = aChildList; nsnull != frame;) {
    nsIFrame* legendFrame = nsnull;
    nsresult result = frame->QueryInterface(kLegendFrameCID, (void**)&legendFrame);
    if (NS_SUCCEEDED(result) && legendFrame) {
      if (mLegendFrame) { // we already have a legend, destroy it
        frame->GetNextSibling(&frame);
        if (lastNewFrame) {
          lastNewFrame->SetNextSibling(frame);
        } 
        else {
          aChildList = frame;
        }
        legendFrame->Destroy(aPresContext);
      } 
      else {
        nsIFrame* nextFrame;
        frame->GetNextSibling(&nextFrame);
        if (lastNewFrame) {
          lastNewFrame->SetNextSibling(nextFrame);
        } else {
          newChildList = nextFrame;
        }
        frame->SetParent(this);
        mFrames.FirstChild()->SetNextSibling(frame);
        mLegendFrame = frame;
        mLegendFrame->SetNextSibling(nsnull);
        frame = nextFrame;
      }
    } else {
      frame->SetParent(mFrames.FirstChild());
      lastNewFrame = frame;
      frame->GetNextSibling(&frame);
    }
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
      
        float p2t;
        aPresContext.GetScaledPixelsToTwips(&p2t);
        nscoord onePixel = NSIntPixelsToTwips(1, p2t);
 
        nsMargin border;
        if (!spacing->GetBorder(border)) {
          NS_NOTYETIMPLEMENTED("percentage border");
        }

        nscoord yoff = 0;
        
        // if the border is smaller than the legend. Move the border down
        // to be centered on the legend. 
        if (border.top < mLegendRect.height)
            yoff = (mLegendRect.height - border.top)/2;
       
        nsRect rect(0, yoff, mRect.width, mRect.height - yoff);

        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *spacing, 0, 0);


        if (mLegendFrame) {

          // we should probably use PaintBorderEdges to do this but for now just use clipping
          // to achieve the same effect.
          PRBool clipState;

          // draw left side
          nsRect clipRect(rect);
          clipRect.width = mLegendRect.x - rect.x;
          clipRect.height = border.top;

          aRenderingContext.PushState();
          aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *spacing, mStyleContext, skipSides);
  
          aRenderingContext.PopState(clipState);


          // draw right side
          clipRect = rect;
          clipRect.x = mLegendRect.x + mLegendRect.width;
          clipRect.width -= (mLegendRect.x + mLegendRect.width);
          clipRect.height = border.top;

          aRenderingContext.PushState();
          aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *spacing, mStyleContext, skipSides);
  
          aRenderingContext.PopState(clipState);

        
          // draw bottom

          clipRect = rect;
          clipRect.y += border.top;
          clipRect.height = mRect.height - (yoff + border.top);
        
          aRenderingContext.PushState();
          aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *spacing, mStyleContext, skipSides);
  
          aRenderingContext.PopState(clipState);
        } else {

          
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, nsRect(0,0,mRect.width, mRect.height), *spacing, mStyleContext, skipSides);
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


NS_IMETHODIMP 
nsFieldSetFrame::Reflow(nsIPresContext&          aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  // availSize could have unconstrained values, don't perform any addition on them
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);
  
  // get our border and padding
  const nsMargin &borderPadding = aReflowState.mComputedBorderPadding;
  const nsMargin &padding       = aReflowState.mComputedPadding;
  nsMargin border = borderPadding - padding;

  // Figure out how big the legend is if there is one. 
  nsMargin legendMargin(0,0,0,0);
  
  // if there is a legend frame flow it.
  if (mLegendFrame) {
    nsHTMLReflowState legendReflowState(aPresContext, aReflowState,
                                        mLegendFrame, nsSize(NS_INTRINSICSIZE,NS_INTRINSICSIZE));

    // always give the legend as much size as it needs
    legendReflowState.mComputedWidth = NS_INTRINSICSIZE;
    legendReflowState.mComputedHeight = NS_INTRINSICSIZE;

    ReflowChild(mLegendFrame, aPresContext, aDesiredSize, legendReflowState, aStatus);

    // get the legend's margin
    nsIStyleContext* legendSC = nsnull;
    mLegendFrame->GetStyleContext(&legendSC);
    if (legendSC) {
      const nsStyleSpacing* legendSpacing =
        (const nsStyleSpacing*)legendSC->GetStyleData(eStyleStruct_Spacing);
      legendSpacing->GetMargin(legendMargin);
      NS_RELEASE(legendSC);
    }

    // figure out the legend's rectangle
    mLegendRect.width  = aDesiredSize.width + legendMargin.left + legendMargin.right;
    mLegendRect.height = aDesiredSize.height + legendMargin.top + legendMargin.bottom;
    mLegendRect.x = borderPadding.left;
    mLegendRect.y = 0;
    mLegendSpace = 0;
    if (mLegendRect.height > border.top) {
        // center the border on the legend
        mLegendSpace = mLegendRect.height - borderPadding.top;
    } else {
        mLegendRect.y = (border.top - mLegendRect.height)/2;
    }

    // if we are contrained then remove the legend from our available height.
    if (NS_INTRINSICSIZE != availSize.height) {
       if (availSize.height >= mLegendSpace)
           availSize.height -= mLegendSpace;
    }
  
    // don't get any smaller than the legend
    if (NS_INTRINSICSIZE != availSize.width) {
       if (availSize.width < mLegendRect.width)
           availSize.width = mLegendRect.width;
    }
  
  }

    // Try to reflow the area frame into the available space.
  nsHTMLReflowState contentReflowState(aPresContext, aReflowState,
                                       mContentFrame, availSize);

  ReflowChild(mContentFrame, aPresContext, aDesiredSize, contentReflowState, aStatus);

  // set the rect. make sure we add the margin back in.
  nsRect contentRect(borderPadding.left,borderPadding.top + mLegendSpace,aDesiredSize.width ,aDesiredSize.height);

  // Place the content area frame.
  mContentFrame->SetRect(contentRect);

  if (mLegendFrame) 
  {
    PRInt32 align = ((nsLegendFrame*)mLegendFrame)->GetAlign();
 
    switch(align) {
      case NS_STYLE_TEXT_ALIGN_RIGHT:
        mLegendRect.x = contentRect.width - mLegendRect.width + borderPadding.left;
        break;
      case NS_STYLE_TEXT_ALIGN_CENTER:
        mLegendRect.x = contentRect.width/2 - mLegendRect.width/2 + borderPadding.left;
    }
  
    // place the legend
    nsRect actualLegendRect(mLegendRect);
    actualLegendRect.Deflate(legendMargin);
    mLegendFrame->SetRect(actualLegendRect);
  }
  
  // Return our size and our result
  if (aReflowState.mComputedHeight == NS_INTRINSICSIZE) {
     aDesiredSize.height = mLegendSpace + 
                           borderPadding.top +
                           aDesiredSize.height +
                           borderPadding.bottom;
  } else {
     nscoord min = borderPadding.top + borderPadding.bottom + mLegendRect.height;
     aDesiredSize.height = aReflowState.mComputedHeight + borderPadding.top + borderPadding.bottom;
     if (aDesiredSize.height < min)
         aDesiredSize.height = min;
  }

  if (aReflowState.mComputedWidth == NS_INTRINSICSIZE) {
      aDesiredSize.width = borderPadding.left + 
                           aDesiredSize.width +
                           borderPadding.right;
  } else {
     nscoord min = borderPadding.left + borderPadding.right + mLegendRect.width;
     aDesiredSize.width = aReflowState.mComputedWidth + borderPadding.left + borderPadding.right;
     if (aDesiredSize.width < min)
         aDesiredSize.width = min;
  }

  aDesiredSize.ascent  = aDesiredSize.height;
  aDesiredSize.descent = 0;

  if (nsnull != aDesiredSize.maxElementSize) {
      // if the legend it wider use it
      if (aDesiredSize.maxElementSize->width < mLegendRect.width)
          aDesiredSize.maxElementSize->width = mLegendRect.width;

      // add in padding.
      aDesiredSize.maxElementSize->width += borderPadding.left + borderPadding.right;

      // height is border + legend
      aDesiredSize.maxElementSize->height += borderPadding.top + borderPadding.bottom + mLegendRect.height;
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

PRIntn
nsFieldSetFrame::GetSkipSides() const
{
  return 0;
}

