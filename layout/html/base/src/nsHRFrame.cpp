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
#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsLeafFrame.h"
#include "nsIRenderingContext.h"
#include "nsGlobalVariables.h"
#include "nsIStyleContext.h"
#include "nsColor.h"
#include "nsIPresContext.h"
#include "nsIStyleContext.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIFontMetrics.h"
#include "nsIHTMLAttributes.h"
#include "nsStyleConsts.h"
#include "nsCSSRendering.h"
#include "nsIDOMHTMLHRElement.h"
#include "nsIDeviceContext.h"
#include "nsStyleUtil.h"

static NS_DEFINE_IID(kIDOMHTMLHRElementIID, NS_IDOMHTMLHRELEMENT_IID);

// default hr thickness in pixels
#define DEFAULT_THICKNESS 3

class HRuleFrame : public nsLeafFrame {
public:
  NS_IMETHOD Reflow(nsIPresContext& aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

protected:
  virtual ~HRuleFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);

  PRBool GetNoShade();
  PRInt32 GetThickness();

  nsMargin mBorderPadding;
  nscoord mComputedWidth;
};

nsresult
NS_NewHRFrame(nsIFrame*& aResult)
{
  nsIFrame* frame = new HRuleFrame;
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}

HRuleFrame::~HRuleFrame()
{
}

NS_METHOD
HRuleFrame::Paint(nsIPresContext&      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect&        aDirtyRect,
                  nsFramePaintLayer    aWhichLayer)
{
  if (eFramePaintLayer_Content != aWhichLayer) {
    return NS_OK;
  }

  const nsStyleDisplay* disp =
    (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
  if (!disp->mVisible) {
    return NS_OK;
  }

  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);
  nscoord thickness = NSIntPixelsToTwips(GetThickness(), p2t);

  // Get style data
  nscoord x0 = mBorderPadding.left;
  nscoord y0 = mBorderPadding.top;
  nscoord width = mRect.width -
    (mBorderPadding.left + mBorderPadding.right);
  nscoord height = mRect.height -
    (mBorderPadding.top + mBorderPadding.bottom);

  nscoord newWidth = mComputedWidth;
  if (newWidth < width) {
    // center or right align rule within the extra space
    const nsStyleText* text =
      (const nsStyleText*) mStyleContext->GetStyleData(eStyleStruct_Text);
    switch (text->mTextAlign) {
    case NS_STYLE_TEXT_ALIGN_RIGHT:
      x0 += width - newWidth;
      break;

    case NS_STYLE_TEXT_ALIGN_LEFT:
      break;

    default:
    case NS_STYLE_TEXT_ALIGN_CENTER:
      x0 += (width - newWidth) / 2;
      break;
    }
  }
  width = newWidth;

  // Center hrule vertically within the available space
  y0 += (height - thickness) / 2;
  height = thickness;

  // To shade or not to shade, that is the question. Begin by collecting the
  // three decision criteria: rendering to the printer or the display, is the
  // "Beveled Lines" checkbox set in the page setup dialog, and does the tag
  // have the NOSHADE attribute set.
  PRBool printing = nsGlobalVariables::Instance()->GetPrinting(&aPresContext);

  PRBool bevel = nsGlobalVariables::Instance()->GetBeveledLines();

	PRBool noShadeAttribute = GetNoShade();

  // Now that we have the data to make the shading criteria, we next
  // collect the decision criteria for rending in solid black:
  // printing (which we already have) and the "Black Lines" setting in
  // the page setup dialog

  const nsStyleColor* color;
  // Draw a "shadowed" box around the rule area
  if (!noShadeAttribute && ((printing && bevel) || !printing)) {
    nsRect rect(x0, y0, width, height);

    const nsStyleSpacing* spacing = 
      (nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
    color = (nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    
    nsCSSRendering::PaintBackground(aPresContext, aRenderingContext,
                                    this,aDirtyRect, rect, *color, 
                                    *spacing, 0, 0);
    nsCSSRendering::PaintBorder(aPresContext, aRenderingContext,
                                this,aDirtyRect, rect, *spacing,
                                mStyleContext, 0);
  } else {
    nscolor colors[2]; 
    PRBool blackLines = nsGlobalVariables::Instance()->GetBlackLines(); 
    if (printing && blackLines) { 
      colors[0] = NS_RGB(0,0,0); 
    } 
    else { 
      // Get the background color that applies to this HR 
      color = nsStyleUtil::FindNonTransparentBackground(mStyleContext); 
      NS_Get3DColors(colors, color->mBackgroundColor); 
    } 
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
                                diameter, diameter, 270.0f, 180.0f);
      aRenderingContext.FillRect(x0 + diameter/2, y0,
                                 width - diameter, height);
     }
  }
  return NS_OK;
}

NS_IMETHODIMP
HRuleFrame::Reflow(nsIPresContext&          aPresContext,
                   nsHTMLReflowMetrics&     aDesiredSize,
                   const nsHTMLReflowState& aReflowState,
                   nsReflowStatus&          aStatus)
{
  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  // XXX add in code to check for width/height being set via css
  // and if set use them instead of calling GetDesiredSize.


  GetDesiredSize(&aPresContext, aReflowState, aDesiredSize);
  AddBordersAndPadding(&aPresContext, aReflowState, aDesiredSize,
                       mBorderPadding);

  // HR's do not impact the max-element-size, unless a width is specified
  // otherwise tables behave badly. This makes sense they are springy.
  if (nsnull != aDesiredSize.maxElementSize) {
    float p2t;
    aPresContext.GetScaledPixelsToTwips(&p2t);
    nscoord onePixel = NSIntPixelsToTwips(1, p2t);
    if (aReflowState.HaveFixedContentWidth()) {
      const nsStylePosition* pos;
      GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)pos);
      if (eStyleUnit_Percent == pos->mWidth.GetUnit()) {
        // When the HR is using a percentage width, make sure it
        // remains springy.
        aDesiredSize.maxElementSize->width = onePixel;
      }
      else {
        aDesiredSize.maxElementSize->width = aReflowState.computedWidth;
      }
      aDesiredSize.maxElementSize->height = onePixel;
    }
    else {
      aDesiredSize.maxElementSize->width = onePixel;
      aDesiredSize.maxElementSize->height = onePixel;
    }
  }

  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

void
HRuleFrame::GetDesiredSize(nsIPresContext* aPresContext,
                           const nsHTMLReflowState& aReflowState,
                           nsHTMLReflowMetrics& aDesiredSize)
{
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);

  if (aReflowState.HaveFixedContentWidth()) {
    aDesiredSize.width = aReflowState.computedWidth;
  }
  else {
    if (NS_UNCONSTRAINEDSIZE == aReflowState.availableWidth) {
      aDesiredSize.width =  nscoord(p2t);
    }
    else {
      aDesiredSize.width = aReflowState.availableWidth;
    }
  }
  mComputedWidth = aDesiredSize.width;
  if (aReflowState.availableWidth != NS_UNCONSTRAINEDSIZE) {
    if (aDesiredSize.width  < aReflowState.availableWidth) {
      aDesiredSize.width = aReflowState.availableWidth;
    }
  }

  // XXX should we interpret css's height property as thickness? or as
  // line-height? In the meantime, ignore it...
  nscoord lineHeight;

  // Get the thickness of the rule (this is not css's height property)
  nscoord thickness = NSIntPixelsToTwips(GetThickness(), p2t);

  // Compute height of "line" that hrule will layout within. Use the
  // default font to do this.
  lineHeight = thickness + NSIntPixelsToTwips(2, p2t);
  const nsFont& defaultFont = aPresContext->GetDefaultFontDeprecated();
  nsCOMPtr<nsIFontMetrics> fm;
  aPresContext->GetMetricsFor(defaultFont, getter_AddRefs(fm));
  nscoord defaultLineHeight;
  fm->GetHeight(defaultLineHeight);
  if (lineHeight < defaultLineHeight) {
    lineHeight = defaultLineHeight;
  }

  aDesiredSize.height = lineHeight;
  aDesiredSize.ascent = lineHeight;
  aDesiredSize.descent = 0;
}

PRInt32
HRuleFrame::GetThickness()
{
  PRInt32 result = DEFAULT_THICKNESS;

  // See if the underlying content is an HR that also implements the
  // nsIHTMLContent API.
  // XXX the dependency on nsIHTMLContent is done to avoid reparsing
  // the size value.
  nsIDOMHTMLHRElement* hr = nsnull;
  nsIHTMLContent* hc = nsnull;
  mContent->QueryInterface(kIDOMHTMLHRElementIID, (void**) &hr);
  mContent->QueryInterface(kIHTMLContentIID, (void**) &hc);
  if ((nsnull != hr) && (nsnull != hc)) {
    // Winner. Get the size attribute to determine how thick the HR
    // should be.
    nsHTMLValue value;
    if (NS_CONTENT_ATTR_HAS_VALUE ==
        hc->GetHTMLAttribute(nsHTMLAtoms::size, value)) {
      if (value.GetUnit() == eHTMLUnit_Pixel) {
        PRInt32 pixels = value.GetPixelValue();
        switch (pixels) {
        case 0:
        case 1:
          result = 1;
          break;
        default:
          result = pixels + 1;
          break;
        }
      }
    }
  }
  NS_IF_RELEASE(hc);
  NS_IF_RELEASE(hr);
  return result;
}

PRBool
HRuleFrame::GetNoShade()
{
  PRBool result = PR_FALSE;
  nsIDOMHTMLHRElement* hr = nsnull;
  mContent->QueryInterface(kIDOMHTMLHRElementIID, (void**) &hr);
  if (nsnull != hr) {
    hr->GetNoShade(&result);
    NS_RELEASE(hr);
  }
  return result;
}
