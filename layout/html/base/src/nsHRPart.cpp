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
#include "nsHTMLParts.h"
#include "nsHTMLTagContent.h"
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

#undef DEBUG_HR_REFCNT

// default hr thickness in pixels
#define DEFAULT_THICKNESS 3

class HRulePart : public nsHTMLTagContent {
public:
  HRulePart(nsIAtom* aTag);

#ifdef DEBUG_HR_REFCNT
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);
#endif

  virtual nsresult CreateFrame(nsIPresContext* aPresContext,
                               nsIFrame* aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*& aResult);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual void MapAttributesInto(nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext);

  PRInt32 GetThickness();

  PRBool GetNoShade();

protected:
  virtual ~HRulePart();

  virtual nsContentAttr AttributeToString(nsIAtom* aAttribute,
                                          nsHTMLValue& aValue,
                                          nsString& aResult) const;
};

class HRuleFrame : public nsLeafFrame {
public:
  HRuleFrame(nsIContent* aContent,
             nsIFrame* aParentFrame);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

protected:
  virtual ~HRuleFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredSize);
};

HRuleFrame::HRuleFrame(nsIContent* aContent,
                       nsIFrame* aParentFrame)
  : nsLeafFrame(aContent, aParentFrame)
{
}

HRuleFrame::~HRuleFrame()
{
}

NS_METHOD
HRuleFrame::Paint(nsIPresContext&      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect&        aDirtyRect)
{
  nsStyleDisplay* disp =
    (nsStyleDisplay*)mStyleContext->GetData(eStyleStruct_Display);
  if (PR_FALSE == disp->mVisible) {
    return NS_OK;
  }

  float p2t = aPresContext.GetPixelsToTwips();
  nscoord thickness = nscoord(p2t * ((HRulePart*)mContent)->GetThickness());

  // Get style data
  nsStyleSpacing* spacing = (nsStyleSpacing*)
    mStyleContext->GetData(eStyleStruct_Spacing);
  nsStyleColor* color = (nsStyleColor*)
    mStyleContext->GetData(eStyleStruct_Color);
  nsStylePosition* position = (nsStylePosition*)
    mStyleContext->GetData(eStyleStruct_Position);
  nsMargin borderPadding;
  spacing->CalcBorderPaddingFor(this, borderPadding);
  nscoord x0 = borderPadding.left;
  nscoord y0 = borderPadding.top;
  nscoord width = mRect.width -
    (borderPadding.left + borderPadding.right);
  nscoord height = mRect.height -
    (borderPadding.top + borderPadding.bottom);

  nscoord newWidth = width;
  if (position->mWidth.GetUnit() == eStyleUnit_Coord) {
    newWidth = position->mWidth.GetIntValue();
  }
  else if (position->mWidth.GetUnit() == eStyleUnit_Percent) {
    // Width is (mostly) interpreted at rendering time
    float pct = position->mWidth.GetPercentValue();
    newWidth = nscoord(pct * mRect.width);
  }
  if (newWidth < width) {
    // center or right align rule within the extra space
    nsStyleText* text =
      (nsStyleText*) mStyleContext->GetData(eStyleStruct_Text);
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

	PRBool noShadeAttribute = PRBool(((HRulePart*)mContent)->GetNoShade());

  // Now that we have the data to make the shading criteria, we next
  // collect the decision criteria for rending in solid black:
  // printing (which we already have) and the "Black Lines" setting in
  // the page setup dialog

  PRBool blackLines = nsGlobalVariables::Instance()->GetBlackLines();
  nscolor colors[2];
  // Get the background color that applies to this HR
  if (printing && blackLines)
  {
    colors[0] = NS_RGB(0,0,0);
    colors[1] = colors[0];
  }
  else
  {
    // XXX Get correct color by finding the first parent that actually
    // specifies a color.
    nsCSSRendering::Get3DColors(colors, color->mBackgroundColor);
  }

  // Draw a "shadowed" box around the rule area
  if (!noShadeAttribute && ((printing && bevel) || !printing)) {
    // Lines render inclusively on the both the starting and ending
    // coordinate, so reduce the end coordinates by one pixel.
    nscoord x1 = nscoord(x0 + width - p2t);
    nscoord y1 = nscoord(y0 + height - p2t);

    // Draw bottom and right lines
    aRenderingContext.SetColor (colors[1]);
    aRenderingContext.DrawLine (x1, y0, x1, y1);
    aRenderingContext.DrawLine (x1, y1, x0, y1);

    // Draw top and left lines
    aRenderingContext.SetColor (colors[0]);
    aRenderingContext.DrawLine (x0, y1, x0, y0);
    aRenderingContext.DrawLine (x0, y0, x1, y0);
  } else {
    // When a rule is not shaded, then we use a uniform color and
    // draw half-circles on the end points.
    aRenderingContext.SetColor (colors[0]);
    nscoord diameter = height;
    if ((diameter > width) || (diameter < nscoord(p2t * 3))) {
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

void
HRuleFrame::GetDesiredSize(nsIPresContext* aPresContext,
                           const nsReflowState& aReflowState,
                           nsReflowMetrics& aDesiredSize)
{
  nsStylePosition* position = (nsStylePosition*)
    mStyleContext->GetData(eStyleStruct_Position);
  if (position->mWidth.GetUnit() == eStyleUnit_Coord) {
    aDesiredSize.width = position->mWidth.GetCoordValue();
  }
  else if (position->mWidth.GetUnit() == eStyleUnit_Percent) {
    float pct = position->mWidth.GetPercentValue();
    aDesiredSize.width = nscoord(pct * aReflowState.maxSize.width);
  }
  else {
    if (aReflowState.maxSize.width == NS_UNCONSTRAINEDSIZE) {
      aDesiredSize.width = 1;
    } else {
      aDesiredSize.width = aReflowState.maxSize.width;
    }
  }
  if (aReflowState.maxSize.width != NS_UNCONSTRAINEDSIZE) {
    if (aDesiredSize.width  < aReflowState.maxSize.width) {
      aDesiredSize.width = aReflowState.maxSize.width;
    }
  }

  // XXX should we interpret css's height property as thickness? or as
  // line-height? In the meantime, ignore it...
  nscoord lineHeight;

  // Get the thickness of the rule (this is not css's height property)
  float p2t = aPresContext->GetPixelsToTwips();
  nscoord thickness = nscoord(p2t * ((HRulePart*)mContent)->GetThickness());

  // Compute height of "line" that hrule will layout within. Use the
  // default font to do this.
  lineHeight = thickness + nscoord(p2t * 2);
  const nsFont& defaultFont = aPresContext->GetDefaultFont();
  nsIFontMetrics* fm = aPresContext->GetMetricsFor(defaultFont);
  nscoord defaultLineHeight = fm->GetHeight();
  NS_RELEASE(fm);
  if (lineHeight < defaultLineHeight) {
    lineHeight = defaultLineHeight;
  }

  aDesiredSize.height = lineHeight;
  aDesiredSize.ascent = lineHeight;
  aDesiredSize.descent = 0;
}

//----------------------------------------------------------------------

HRulePart::HRulePart(nsIAtom* aTag)
  : nsHTMLTagContent(aTag)
{
}

HRulePart::~HRulePart()
{
}

#ifdef DEBUG_HR_REFCNT
nsrefcnt HRulePart::AddRef(void)
{
  return ++mRefCnt;
}

nsrefcnt HRulePart::Release(void)
{
  NS_PRECONDITION(mRefCnt != 0, "too many release's");
  if (--mRefCnt == 0) {
    if (mInHeap) {
      delete this;
    }
  }
  return mRefCnt;
}
#endif

PRInt32
HRulePart::GetThickness()
{
  PRInt32 result = DEFAULT_THICKNESS;
  nsHTMLValue value;
  if (eContentAttr_HasValue == GetAttribute(nsHTMLAtoms::size, value)) {
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
  return result;
}

PRBool
HRulePart::GetNoShade()
{
  PRBool result = PR_FALSE;
  nsHTMLValue value;
  if (eContentAttr_HasValue == GetAttribute(nsHTMLAtoms::noshade, value)) {
    if (value.GetUnit() == eHTMLUnit_Empty) {
      result = PR_TRUE;
    }
  }
  return result;
}

static nsHTMLTagContent::EnumTable kAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { 0 }
};

void
HRulePart::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  nsHTMLValue val;
  if (aAttribute == nsHTMLAtoms::width) {
    ParseValueOrPercent(aValue, val, eHTMLUnit_Pixel);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (aAttribute == nsHTMLAtoms::size) {
    ParseValue(aValue, 1, 100, val, eHTMLUnit_Pixel);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (aAttribute == nsHTMLAtoms::noshade) {
    val.SetEmptyValue();
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (ParseEnumValue(aValue, kAlignTable, val)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
  }
  else {
    // Use default attribute catching code
    nsHTMLTagContent::SetAttribute(aAttribute, aValue);
  }
}

void
HRulePart::MapAttributesInto(nsIStyleContext* aContext,
                             nsIPresContext* aPresContext)
{
  if (nsnull != mAttributes) {
    nsHTMLValue value;
    // align: enum
    GetAttribute(nsHTMLAtoms::align, value);
    if (value.GetUnit() == eHTMLUnit_Enumerated) {
      nsStyleText* text = (nsStyleText*)aContext->GetData(eStyleStruct_Text);
      text->mTextAlign = value.GetIntValue();
    }

    // width: pixel, percent
    float p2t = aPresContext->GetPixelsToTwips();
    nsStylePosition* pos = (nsStylePosition*)
      aContext->GetData(eStyleStruct_Position);
    GetAttribute(nsHTMLAtoms::width, value);
    if (value.GetUnit() == eHTMLUnit_Pixel) {
      nscoord twips = nscoord(p2t * value.GetPixelValue());
      pos->mWidth.SetCoordValue(twips);
    }
    else if (value.GetUnit() == eHTMLUnit_Percent) {
      pos->mWidth.SetPercentValue(value.GetPercentValue());
    }
  }
}

nsContentAttr
HRulePart::AttributeToString(nsIAtom*     aAttribute,
                             nsHTMLValue& aValue,
                             nsString&    aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  if (aAttribute == nsHTMLAtoms::align) {
    if (eHTMLUnit_Enumerated == aValue.GetUnit()) {
      EnumValueToString(aValue, kAlignTable, aResult);
      ca = eContentAttr_HasValue;
    }
  }
  return ca;
}

//----------------------------------------------------------------------

nsresult
HRulePart::CreateFrame(nsIPresContext*  aPresContext,
                       nsIFrame*        aParentFrame,
                       nsIStyleContext* aStyleContext,
                       nsIFrame*&       aResult)
{
  nsIFrame* frame = new HRuleFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

nsresult
NS_NewHRulePart(nsIHTMLContent** aInstancePtrResult,
                nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* body = new HRulePart(aTag);
  if (nsnull == body) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return body->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}
