/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// YY need to pass isMultiple before create called

#include "nsCSSRendering.h"
#include "nsIStyleContext.h"
#include "nsBoxFrame.h"

class nsTitledBoxFrame : public nsBoxFrame {
public:

  nsTitledBoxFrame(nsIPresShell* aShell);

  NS_IMETHOD GetBorderAndPadding(nsMargin& aBorderAndPadding);

                               
  NS_METHOD Paint(nsIPresContext*      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect&        aDirtyRect,
                  nsFramePaintLayer    aWhichLayer,
                  PRUint32             aFlags);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("GroupBoxFrame", aResult);
  }
#endif

  // make sure we our kids get our orient, align, and autostretch instead of us.
  // our child box has no content node so it will search for a parent with one.
  // that will be us.
  virtual void GetInitialOrientation(PRBool& aHorizontal) { aHorizontal = PR_FALSE; }
  virtual PRBool GetInitialHAlignment(Halignment& aHalign)  { aHalign = hAlign_Left; return PR_TRUE; } 
  virtual PRBool GetInitialVAlignment(Valignment& aValign)  { aValign = vAlign_Top; return PR_TRUE; } 
  virtual PRBool GetInitialAutoStretch(PRBool& aStretch)    { aStretch = PR_TRUE; return PR_TRUE; } 

  nsIBox* GetTitleBox(nsIPresContext* aPresContext, nsRect& aRect);
};

/*
class nsTitledBoxInnerFrame : public nsBoxFrame {
public:

    nsTitledBoxInnerFrame(nsIPresShell* aShell):nsBoxFrame(aShell) {}


#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("TitledBoxFrameInner", aResult);
  }
#endif
  
  // we are always flexible
  virtual PRBool GetDefaultFlex(PRInt32& aFlex) { aFlex = 1; return PR_TRUE; }

};
*/

nsresult
NS_NewTitledBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTitledBoxFrame* it = new (aPresShell) nsTitledBoxFrame(aPresShell);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  *aNewFrame = it;
  return NS_OK;
}

nsTitledBoxFrame::nsTitledBoxFrame(nsIPresShell* aShell):nsBoxFrame(aShell)
{
}


// this is identical to nsHTMLContainerFrame::Paint except for the background and border. 
NS_IMETHODIMP
nsTitledBoxFrame::Paint(nsIPresContext*      aPresContext,
                        nsIRenderingContext& aRenderingContext,
                        const nsRect&        aDirtyRect,
                        nsFramePaintLayer    aWhichLayer,
                        PRUint32             aFlags)
{
  if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
    // Paint our background and border
    const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);

    if (vis->IsVisible() && mRect.width && mRect.height) {
      PRIntn skipSides = GetSkipSides();
      const nsStyleBackground* color =
        (const nsStyleBackground*)mStyleContext->GetStyleData(eStyleStruct_Background);
      const nsStyleBorder* borderStyleData =
        (const nsStyleBorder*)mStyleContext->GetStyleData(eStyleStruct_Border);
       
        nsMargin border;
        if (!borderStyleData->GetBorder(border)) {
          NS_NOTYETIMPLEMENTED("percentage border");
        }

        nscoord yoff = 0;

        nsRect titleRect;
        nsIBox* titleBox = GetTitleBox(aPresContext, titleRect);

        if (titleBox) {        
            nsIFrame* titleFrame;
            titleBox->GetFrame(&titleFrame);

            // if the border is smaller than the legend. Move the border down
            // to be centered on the legend. 
            const nsStyleMargin* titleMarginData;
            titleFrame->GetStyleData(eStyleStruct_Margin,
                                    (const nsStyleStruct*&) titleMarginData);

            nsMargin titleMargin;
            titleMarginData->GetMargin(titleMargin);
            titleRect.Inflate(titleMargin);
         
            if (border.top < titleRect.height)
                yoff = (titleRect.height - border.top)/2 + titleRect.y;
        }

        nsRect rect(0, yoff, mRect.width, mRect.height - yoff);

        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *borderStyleData, 0, 0);


        if (titleBox) {

          // we should probably use PaintBorderEdges to do this but for now just use clipping
          // to achieve the same effect.
          PRBool clipState;

          // draw left side
          nsRect clipRect(rect);
          clipRect.width = titleRect.x - rect.x;
          clipRect.height = border.top;

          aRenderingContext.PushState();
          aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *borderStyleData, mStyleContext, skipSides);
  
          aRenderingContext.PopState(clipState);


          // draw right side
          clipRect = rect;
          clipRect.x = titleRect.x + titleRect.width;
          clipRect.width -= (titleRect.x + titleRect.width);
          clipRect.height = border.top;

          aRenderingContext.PushState();
          aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *borderStyleData, mStyleContext, skipSides);
  
          aRenderingContext.PopState(clipState);

          
        
          // draw bottom

          clipRect = rect;
          clipRect.y += border.top;
          clipRect.height = mRect.height - (yoff + border.top);
        
          aRenderingContext.PushState();
          aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *borderStyleData, mStyleContext, skipSides);
  
          aRenderingContext.PopState(clipState);
          
        } else {

          
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, nsRect(0,0,mRect.width, mRect.height), *borderStyleData, mStyleContext, skipSides);
        }
    }
  }

  PaintChildren(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);

#ifdef DEBUG
  if ((NS_FRAME_PAINT_LAYER_DEBUG == aWhichLayer) && GetShowFrameBorders()) {
    nsIView* view;
    GetView(aPresContext, &view);
    if (nsnull != view) {
      aRenderingContext.SetColor(NS_RGB(0,0,255));
    }
    else {
      aRenderingContext.SetColor(NS_RGB(255,0,0));
    }
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
  }
#endif
  return NS_OK;
}

nsIBox*
nsTitledBoxFrame::GetTitleBox(nsIPresContext* aPresContext, nsRect& aTitleRect)
{
    // first child is out titled area
    nsIBox* box;
    GetChildBox(&box);

    // no area fail.
    if (!box)
      return nsnull;

    // get the first child in the titled area that is the title
    box->GetChildBox(&box);

    // nothing in the area? fail
    if (!box)
      return nsnull;

    // now get the title itself. It is in the title frame.
    nsIBox* child = nsnull;
    box->GetChildBox(&child);

    if (child) {
       // convert to our coordinates.
       nsRect parentRect;
       box->GetBounds(parentRect);
       child->GetBounds(aTitleRect);
       aTitleRect.x += parentRect.x;
       aTitleRect.y += parentRect.y;
    }

    return child;
}

NS_IMETHODIMP
nsTitledBoxFrame::GetBorderAndPadding(nsMargin& aBorderAndPadding)
{
  aBorderAndPadding.SizeTo(0,0,0,0);
  return NS_OK;
}

