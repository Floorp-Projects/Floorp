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
#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsLeafFrame.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"
#include "nsColor.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIFontMetrics.h"
#include "nsStyleConsts.h"
#include "nsCSSRendering.h"
#include "nsIDOMHTMLHRElement.h"
#include "nsIDeviceContext.h"
#include "nsStyleUtil.h"
#include "nsLayoutAtoms.h"


// default hr thickness in pixels
#define DEFAULT_THICKNESS 3

class HRuleFrame : public nsFrame {
public:
  HRuleFrame();

  NS_IMETHOD Reflow(nsIPresContext* aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  NS_IMETHOD Paint(nsIPresContext*      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer,
                   PRUint32             aFlags);
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const;
#endif

protected:
  PRBool GetNoShade();

  nsMargin mBorderPadding;
  nscoord mThickness;
};

nsresult
NS_NewHRFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  HRuleFrame* it = new (aPresShell) HRuleFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

HRuleFrame::HRuleFrame()
{
}

NS_METHOD
HRuleFrame::Paint(nsIPresContext*      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect&        aDirtyRect,
                  nsFramePaintLayer    aWhichLayer,
                  PRUint32             aFlags)
{
  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer) {
    return NS_OK;
  }

  const nsStyleVisibility* vis = 
      (const nsStyleVisibility*)((nsIStyleContext*)mStyleContext)->GetStyleData(eStyleStruct_Visibility);
  if (!vis->IsVisible()) {
    return NS_OK;
  }

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord thickness = mThickness;

  // Get style data
  nscoord x0 = mBorderPadding.left;
  nscoord y0 = mBorderPadding.top;
  nscoord width = mRect.width -
    (mBorderPadding.left + mBorderPadding.right);
  nscoord height = mRect.height -
    (mBorderPadding.top + mBorderPadding.bottom);

  // Center hrule vertically within the available space
  y0 += (height - thickness) / 2;
  height = thickness;

  // To shade or not to shade, that is the question. Begin by collecting the
  // three decision criteria: rendering to the printer or the display, is the
  // "Beveled Lines" checkbox set in the page setup dialog, and does the tag
  // have the NOSHADE attribute set.
	PRBool noShadeAttribute = GetNoShade();

  // Now that we have the data to make the shading criteria, we next
  // collect the decision criteria for rending in solid black:
  // printing (which we already have) and the "Black Lines" setting in
  // the page setup dialog

  const nsStyleBackground* color;
  // Draw a "shadowed" box around the rule area
  if (!noShadeAttribute) {
    nsRect rect(x0, y0, width, height);

    const nsStyleBorder* border = (const nsStyleBorder*)
      mStyleContext->GetStyleData(eStyleStruct_Border);
    color = (nsStyleBackground*)mStyleContext->GetStyleData(eStyleStruct_Background);
    
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext,
                                    this,aDirtyRect, rect, *color, 
                                    *border, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext,
                                this,aDirtyRect, rect, *border,
                                mStyleContext, 0);
  } else {
    nscolor colors[2]; 
    // Get the background color that applies to this HR 
    color = nsStyleUtil::FindNonTransparentBackground(mStyleContext); 
    NS_Get3DColors(colors, color->mBackgroundColor); 

    // When a rule is not shaded, then we use a uniform color and
    // draw half-circles on the end points.
    aRenderingContext.SetColor (colors[0]);
    nscoord diameter = height;
    if ((diameter > width) || (diameter < NSIntPixelsToTwips(3, p2t))) {
      // The half-circles on the ends of the rule aren't going to
      // look right so don't bother drawing them.
      aRenderingContext.FillRect(x0, y0, width, height);
    } else {
      aRenderingContext.FillArc(x0, y0, diameter, diameter, 90.0f, 270.0f);
      aRenderingContext.FillArc(x0 + width - diameter, y0,
                                diameter, diameter, 270.0f, 90.0f);
      aRenderingContext.FillRect(x0 + diameter/2, y0,
                                 width - diameter, height);
     }
  }
  return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

NS_IMETHODIMP
HRuleFrame::Reflow(nsIPresContext*          aPresContext,
                   nsHTMLReflowMetrics&     aDesiredSize,
                   const nsHTMLReflowState& aReflowState,
                   nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("HRuleFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  // bug 18754: In compat mode, we treat HR's as inline elements
  //            with break-before and break-after semantics
  //            achieved in quirks.css using :before and :after
  //            In standard mode, HR is still just a block, no special handling
  nsCompatibility mode;
  aPresContext->GetCompatibilityMode(&mode);

  // Compute the width
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);  // get the rounding right
  nscoord twoPixels= NSIntPixelsToTwips(2, p2t);  // get the rounding right
  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedWidth) {
    aDesiredSize.width = aReflowState.mComputedWidth;
  }
  else {
    if (NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) {
      aDesiredSize.width = onePixel;
    }
    else {
      aDesiredSize.width = aReflowState.availableWidth;
    }
  }


  // Get the thickness of the rule. Note that this specifies the
  // height of the rule, not the height of the frame.
  nscoord thickness;
  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) 
  {
    thickness = aReflowState.mComputedHeight;
    // fix up thickness based on noshade and mode.  see bug 53568 and 54980
    if (eCompatibility_NavQuirks == mode)
    {
      nscoord adjustment =  aReflowState.mComputedBorderPadding.top + 
                            aReflowState.mComputedBorderPadding.bottom; 
      thickness += adjustment;  // adjust for -moz-bg-inset 
      PRBool noShadeAttribute = GetNoShade();
      if (thickness != onePixel)
      {
        if (!noShadeAttribute) { // this makes us compatible with Nav4, and one pixel taller than IE5
          thickness += onePixel;
        }
      }
    }
  }
  else {
    thickness = NSIntPixelsToTwips(DEFAULT_THICKNESS, p2t);
  }

  // remember the computed thickness
  mThickness = thickness;
  NS_ASSERTION(mThickness>=0, "negative height calculated for HR");

  // Compute height of "line" that hrule will layout within. Use the
  // font-size to do this.
  nscoord minLineHeight = thickness + twoPixels;
  const nsStyleFont* font;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&) font);
  const nsFont& f = font->mFont;
  nsCOMPtr<nsIFontMetrics> fm;
  aPresContext->GetMetricsFor(f, getter_AddRefs(fm));
  nscoord fontHeight;
  fm->GetHeight(fontHeight);

  aDesiredSize.height =
    (minLineHeight < fontHeight ? fontHeight : minLineHeight) +
    aReflowState.mComputedBorderPadding.top +
    aReflowState.mComputedBorderPadding.bottom;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  // HR's do not impact the max-element-size, unless a width is
  // specified otherwise tables behave badly. This makes sense -- they
  // are springy.
  if (nsnull != aDesiredSize.maxElementSize) {
    if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedWidth) {
      nsStyleUnit widthUnit = aReflowState.mStylePosition->mWidth.GetUnit();
      if ((eStyleUnit_Percent == widthUnit) || 
          (eStyleUnit_Auto == widthUnit)) {
        // When the HR is using an 'auto' or percentage width, make sure it
        // remains springy.
        aDesiredSize.maxElementSize->width = onePixel;
      }
      else {
        aDesiredSize.maxElementSize->width = aReflowState.mComputedWidth;
      }
    }
    else {
      aDesiredSize.maxElementSize->width = onePixel;
    }
    NS_ASSERTION(aDesiredSize.maxElementSize->width <= aDesiredSize.width, "bad max-element-size width");
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

PRBool
HRuleFrame::GetNoShade()
{
  PRBool result = PR_FALSE;
  nsIDOMHTMLHRElement* hr = nsnull;
  mContent->QueryInterface(NS_GET_IID(nsIDOMHTMLHRElement), (void**) &hr);
  if (nsnull != hr) {
    hr->GetNoShade(&result);
    NS_RELEASE(hr);
  }
  return result;
}

NS_IMETHODIMP
HRuleFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::hrFrame;
  NS_ADDREF(*aType);
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
HRuleFrame::SizeOf(nsISizeOfHandler* aHandler, PRUint32* aResult) const
{
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = sizeof(*this);
  return NS_OK;
}
#endif
