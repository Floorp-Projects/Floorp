/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// YY need to pass isMultiple before create called

#include "nsBoxFrame.h"
#include "nsCSSRendering.h"
#include "nsIStyleContext.h"

class nsGroupBoxFrame : public nsBoxFrame {
public:

  nsGroupBoxFrame(nsIPresShell* aShell);

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

  // make sure we our kids get our orient and align instead of us.
  // our child box has no content node so it will search for a parent with one.
  // that will be us.
  virtual void GetInitialOrientation(PRBool& aHorizontal) { aHorizontal = PR_FALSE; }
  virtual PRBool GetInitialHAlignment(Halignment& aHalign)  { aHalign = hAlign_Left; return PR_TRUE; } 
  virtual PRBool GetInitialVAlignment(Valignment& aValign)  { aValign = vAlign_Top; return PR_TRUE; } 
  virtual PRBool GetInitialAutoStretch(PRBool& aStretch)    { aStretch = PR_TRUE; return PR_TRUE; } 

  nsIBox* GetCaptionBox(nsIPresContext* aPresContext, nsRect& aCaptionRect);
};

/*
class nsGroupBoxInnerFrame : public nsBoxFrame {
public:

    nsGroupBoxInnerFrame(nsIPresShell* aShell):nsBoxFrame(aShell) {}


#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("GroupBoxFrameInner", aResult);
  }
#endif
  
  // we are always flexible
  virtual PRBool GetDefaultFlex(PRInt32& aFlex) { aFlex = 1; return PR_TRUE; }

};
*/

nsresult
NS_NewGroupBoxFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsGroupBoxFrame* it = new (aPresShell) nsGroupBoxFrame(aPresShell);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  *aNewFrame = it;
  return NS_OK;
}

nsGroupBoxFrame::nsGroupBoxFrame(nsIPresShell* aShell):nsBoxFrame(aShell)
{
}


// this is identical to nsHTMLContainerFrame::Paint except for the background and border. 
NS_IMETHODIMP
nsGroupBoxFrame::Paint(nsIPresContext*      aPresContext,
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

        nsRect groupRect;
        nsIBox* groupBox = GetCaptionBox(aPresContext, groupRect);

        if (groupBox) {        
            nsIFrame* groupFrame;
            groupBox->GetFrame(&groupFrame);

            // if the border is smaller than the legend. Move the border down
            // to be centered on the legend. 
            const nsStyleMargin* groupMarginData;
            groupFrame->GetStyleData(eStyleStruct_Margin,
                                     (const nsStyleStruct*&) groupMarginData);

            nsMargin groupMargin;
            groupMarginData->GetMargin(groupMargin);
            groupRect.Inflate(groupMargin);
         
            if (border.top < groupRect.height)
                yoff = (groupRect.height - border.top)/2 + groupRect.y;
        }

        nsRect rect(0, yoff, mRect.width, mRect.height - yoff);

        nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
                                        aDirtyRect, rect, *color, *borderStyleData, 0, 0);


        if (groupBox) {

          // we should probably use PaintBorderEdges to do this but for now just use clipping
          // to achieve the same effect.
          PRBool clipState;

          // draw left side
          nsRect clipRect(rect);
          clipRect.width = groupRect.x - rect.x;
          clipRect.height = border.top;

          aRenderingContext.PushState();
          aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect, clipState);
          nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
                                      aDirtyRect, rect, *borderStyleData, mStyleContext, skipSides);
  
          aRenderingContext.PopState(clipState);


          // draw right side
          clipRect = rect;
          clipRect.x = groupRect.x + groupRect.width;
          clipRect.width -= (groupRect.x + groupRect.width);
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
nsGroupBoxFrame::GetCaptionBox(nsIPresContext* aPresContext, nsRect& aCaptionRect)
{
    // first child is our grouped area
    nsIBox* box;
    GetChildBox(&box);

    // no area fail.
    if (!box)
      return nsnull;

    // get the first child in the grouped area, that is the caption
    box->GetChildBox(&box);

    // nothing in the area? fail
    if (!box)
      return nsnull;

    // now get the caption itself. It is in the caption frame.
    nsIBox* child = nsnull;
    box->GetChildBox(&child);

    if (child) {
       // convert to our coordinates.
       nsRect parentRect;
       box->GetBounds(parentRect);
       child->GetBounds(aCaptionRect);
       aCaptionRect.x += parentRect.x;
       aCaptionRect.y += parentRect.y;
    }

    return child;
}

NS_IMETHODIMP
nsGroupBoxFrame::GetBorderAndPadding(nsMargin& aBorderAndPadding)
{
  aBorderAndPadding.SizeTo(0,0,0,0);
  return NS_OK;
}

