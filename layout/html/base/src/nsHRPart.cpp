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

#undef DEBUG_HR_REFCNT

static NS_DEFINE_IID(kStyleMoleculeSID, NS_STYLEMOLECULE_SID);
static NS_DEFINE_IID(kStyleFontSID, NS_STYLEFONT_SID);
static NS_DEFINE_IID(kStyleColorSID, NS_STYLECOLOR_SID);

// Default alignment value (so we can tell an unset value from a set value)
#define ALIGN_UNSET PRUint8(-1)

class HRulePart : public nsHTMLTagContent {
public:
  HRulePart(nsIAtom* aTag);

#ifdef DEBUG_HR_REFCNT
  virtual nsrefcnt AddRef(void);
  virtual nsrefcnt Release(void);
#endif
  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);
  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aResult) const;
  virtual void UnsetAttribute(nsIAtom* aAttribute);

  PRInt32 mThickness;           // in pixels
  PRPackedBool mNoShade;
  PRUint8 mAlign;

protected:
  virtual ~HRulePart();
  virtual nsContentAttr AttributeToString(nsIAtom* aAttribute,
                                          nsHTMLValue& aValue,
                                          nsString& aResult) const;
};

class HRuleFrame : public nsLeafFrame {
public:
  HRuleFrame(nsIContent* aContent,
             PRInt32 aIndexInParent,
             nsIFrame* aParentFrame);

  NS_METHOD Paint(nsIPresContext& aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect& aDirtyRect);

protected:
  virtual ~HRuleFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              nsReflowMetrics& aDesiredSize,
                              const nsSize& aMaxSize);

  // Weird color computing code stolen from winfe which was stolen
  // from the xfe which was written originally by Eric Bina. So there.
	static void Get3DColors(nscolor aResult[2], nscolor aColor);
  static const int RED_LUMINOSITY;
  static const int GREEN_LUMINOSITY;
  static const int BLUE_LUMINOSITY;
  static const int INTENSITY_FACTOR;
  static const int LIGHT_FACTOR;
  static const int LUMINOSITY_FACTOR;
  static const int MAX_COLOR;
  static const int COLOR_DARK_THRESHOLD;
  static const int COLOR_LIGHT_THRESHOLD;
};

HRuleFrame::HRuleFrame(nsIContent* aContent,
                       PRInt32 aIndexInParent,
                       nsIFrame* aParentFrame)
  : nsLeafFrame(aContent, aIndexInParent, aParentFrame)
{
}

HRuleFrame::~HRuleFrame()
{
}

NS_METHOD HRuleFrame::Paint(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect)
{
  float p2t = aPresContext.GetPixelsToTwips();
  nscoord thickness = nscoord(p2t * ((HRulePart*)mContent)->mThickness);

  // Get style data
  nsStyleMolecule* mol =
    (nsStyleMolecule*)mStyleContext->GetData(kStyleMoleculeSID);
  nsStyleColor* color =
    (nsStyleColor*)mStyleContext->GetData(kStyleColorSID);
  nscoord x0 = mol->borderPadding.left;
  nscoord y0 = mol->borderPadding.top;
  nscoord width = mRect.width -
    (mol->borderPadding.left + mol->borderPadding.right);
  nscoord height = mRect.height -
    (mol->borderPadding.top + mol->borderPadding.bottom);

  // Center hrule vertically within the available space
  y0 += (height - thickness) / 2;
  height = thickness;

  // To shade or not to shade, that is the question. Begin by collecting the
  // three decision criteria: rendering to the printer or the display, is the
  // "Beveled Lines" checkbox set in the page setup dialog, and does the tag
  // have the NOSHADE attribute set.
  PRBool printing = nsGlobalVariables::Instance()->GetPrinting(&aPresContext);

  PRBool bevel = nsGlobalVariables::Instance()->GetBeveledLines();

	PRBool noShadeAttribute = PRBool(((HRulePart*)mContent)->mNoShade);

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
    Get3DColors(colors, color->mBackgroundColor);
  }

  // Draw a "shadowed" box around the rule area
  if (!noShadeAttribute && ((printing && bevel) || !printing)) {
    // Lines render inclusively on the both the starting and ending
    // coordinate, so reduce the end coordinates by one pixel.
    nscoord x1 = nscoord(x0 + width - p2t);
    nscoord y1 = nscoord(y0 + height - p2t);

    // Draw top and left lines
    aRenderingContext.SetColor (colors[0]);
    aRenderingContext.DrawLine (x0, y1, x0, y0);
    aRenderingContext.DrawLine (x0, y0, x1, y0);

    // Draw bottom and right lines
    aRenderingContext.SetColor (colors[1]);
    aRenderingContext.DrawLine (x1, y0, x1, y1);
    aRenderingContext.DrawLine (x1, y1, x0, y1);
  } else {
    // When a rule is not shaded, then we use a uniform color and
    // draw half-circles on the end points.
    aRenderingContext.SetColor (colors[0]);
    float t2p = 1.0f / p2t;
    nscoord diameter = height;
    if ((diameter > width) || (diameter < nscoord(p2t * 3))) {
      // The half-circles on the ends of the rule aren't going to
      // look right so don't bother drawing them.
      aRenderingContext.FillRect(x0, y0, width, height);
    } else {
      nscoord pix = NS_POINTS_TO_TWIPS_INT(1);
      aRenderingContext.FillArc(x0, y0, diameter, diameter, 90.0f, 180.0f);
      aRenderingContext.FillArc(x0 + width - diameter, y0,
                                diameter, diameter, 270.0f, 180.0f);
      aRenderingContext.FillRect(x0 + diameter/2, y0,
                                 width - diameter, height);
     }
  }
  return NS_OK;
}

// Weird color computing code stolen from winfe which was stolen
// from the xfe which was written originally by Eric Bina. So there.

const int HRuleFrame::RED_LUMINOSITY = 30;
const int HRuleFrame::GREEN_LUMINOSITY = 59;
const int HRuleFrame::BLUE_LUMINOSITY = 11;
const int HRuleFrame::INTENSITY_FACTOR = 25;
const int HRuleFrame::LIGHT_FACTOR = 0;
const int HRuleFrame::LUMINOSITY_FACTOR = 75;
const int HRuleFrame::MAX_COLOR = 255;
const int HRuleFrame::COLOR_DARK_THRESHOLD = 51;
const int HRuleFrame::COLOR_LIGHT_THRESHOLD = 204;


void HRuleFrame::Get3DColors(nscolor aResult[2], nscolor aColor)
{
  int rb = NS_GET_R(aColor);
  int gb = NS_GET_G(aColor);
  int bb = NS_GET_B(aColor);
  int intensity = (rb + gb + bb) / 3;
  int luminosity =
    ((RED_LUMINOSITY * rb) / 100) +
    ((GREEN_LUMINOSITY * gb) / 100) +
    ((BLUE_LUMINOSITY * bb) / 100);
  int brightness = ((intensity * INTENSITY_FACTOR) +
                    (luminosity * LUMINOSITY_FACTOR)) / 100;
  int f0, f1;
  if (brightness < COLOR_DARK_THRESHOLD) {
    f0 = 30;
    f1 = 50;
  } else if (brightness > COLOR_LIGHT_THRESHOLD) {
    f0 = 45;
    f1 = 50;
  } else {
    f0 = 30 + (brightness * (45 - 30) / MAX_COLOR);
    f1 = f0;
  }
  int r = rb - (f0 * rb / 100);
  int g = gb - (f0 * gb / 100);
  int b = bb - (f0 * bb / 100);
  aResult[0] = NS_RGB(r, g, b);
  r = rb + (f1 * (MAX_COLOR - rb) / 100);
  if (r > 255) r = 255;
  g = gb + (f1 * (MAX_COLOR - gb) / 100);
  if (g > 255) g = 255;
  b = bb + (f1 * (MAX_COLOR - bb) / 100);
  if (b > 255) b = 255;
  aResult[1] = NS_RGB(r, g, b);
}

void HRuleFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                nsReflowMetrics& aDesiredSize,
                                const nsSize& aMaxSize)
{
  // Get style data
  nsStyleFont* font =
    (nsStyleFont*)mStyleContext->GetData(kStyleFontSID);

  if (aMaxSize.width == NS_UNCONSTRAINEDSIZE) {
    aDesiredSize.width = 1;
  } else {
    // XXX look at width property; use percentage, etc.
    // XXX apply centering using our align property
    aDesiredSize.width = aMaxSize.width;
  }

  // Get the thickness of the rule (this is not css's height property)
  float p2t = aPresContext->GetPixelsToTwips();
  nscoord thickness = nscoord(p2t * ((HRulePart*)mContent)->mThickness);

  // Compute height of "line" that hrule will layout within
  nscoord lineHeight = thickness + nscoord(p2t * 2);
  nsIFontMetrics* fm = aPresContext->GetMetricsFor(font->mFont);
  nscoord defaultLineHeight = fm->GetHeight();
  NS_RELEASE(fm);
  if (lineHeight < defaultLineHeight) {
    lineHeight = defaultLineHeight;
  }

  aDesiredSize.height = lineHeight;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
}

//----------------------------------------------------------------------

HRulePart::HRulePart(nsIAtom* aTag)
  : nsHTMLTagContent(aTag)
{
  mAlign = ALIGN_UNSET;
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

//----------------------------------------------------------------------
// Attributes

static nsHTMLTagContent::EnumTable kAlignTable[] = {
  { "left", NS_STYLE_TEXT_ALIGN_LEFT },
  { "right", NS_STYLE_TEXT_ALIGN_RIGHT },
  { "center", NS_STYLE_TEXT_ALIGN_CENTER },
  { 0 }
};

void HRulePart::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::width) {
    nsHTMLValue val;
    ParseValueOrPercent(aValue, val);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    return;
  }
  if (aAttribute == nsHTMLAtoms::size) {
    nsHTMLValue val;
    ParseValue(aValue, 1, 100, val);
    mThickness = val.GetIntValue();
    return;
  }
  if (aAttribute == nsHTMLAtoms::noshade) {
    mNoShade = PR_TRUE;
    return;
  }
  if (aAttribute == nsHTMLAtoms::align) {
    nsHTMLValue val;
    if (ParseEnumValue(aValue, kAlignTable, val)) {
      mAlign = val.GetIntValue();
    } else {
      mAlign = ALIGN_UNSET;
    }
    return;
  }

  // Use default attribute catching code
  nsHTMLTagContent::SetAttribute(aAttribute, aValue);
}

nsContentAttr HRulePart::GetAttribute(nsIAtom* aAttribute,
                                      nsHTMLValue& aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  if (aAttribute == nsHTMLAtoms::size) {
    aResult.Reset();
    if (0 != mThickness) {
      aResult.Set(mThickness, eHTMLUnit_Absolute);
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::noshade) {
    aResult.Reset();
    if (mNoShade) {
      ca = eContentAttr_HasValue;
    }
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    aResult.Reset();
    if (ALIGN_UNSET != mAlign) {
      aResult.Set(mAlign, eHTMLUnit_Enumerated);
      ca = eContentAttr_HasValue;
    }
  }
  else {
    ca = nsHTMLTagContent::GetAttribute(aAttribute, aResult);
  }
  return ca;
}

void HRulePart::UnsetAttribute(nsIAtom* aAttribute)
{
  if (aAttribute == nsHTMLAtoms::noshade) {
    mNoShade = PR_FALSE;
  }
  else if (aAttribute == nsHTMLAtoms::size) {
    mThickness = 0;
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    mAlign = ALIGN_UNSET;
  }
  else {
    // Use default attribute catching code
    nsHTMLTagContent::UnsetAttribute(aAttribute);
  }
}

nsContentAttr HRulePart::AttributeToString(nsIAtom* aAttribute,
                                           nsHTMLValue& aValue,
                                           nsString& aResult) const
{
  nsContentAttr ca = eContentAttr_NotThere;
  if (aAttribute == nsHTMLAtoms::align) {
    if ((eHTMLUnit_Enumerated == aValue.GetUnit()) &&
        (ALIGN_UNSET != aValue.GetIntValue())) {
      EnumValueToString(aValue, kAlignTable, aResult);
      ca = eContentAttr_HasValue;
    }
  }
  return ca;
}

//----------------------------------------------------------------------

nsIFrame* HRulePart::CreateFrame(nsIPresContext* aPresContext,
                                 PRInt32 aIndexInParent,
                                 nsIFrame* aParentFrame)
{
  nsIFrame* rv = new HRuleFrame(this, aIndexInParent, aParentFrame);
  return rv;
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
