/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLFrame.h"

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "nsLayoutUtils.h"
#include "nsNameSpaceManager.h"
#include "nsMathMLChar.h"
#include "nsCSSPseudoElements.h"
#include "nsMathMLElement.h"

// used to map attributes into CSS rules
#include "nsStyleSet.h"
#include "nsAutoPtr.h"
#include "nsDisplayList.h"
#include "nsRenderingContext.h"

using namespace mozilla;
using namespace mozilla::gfx;

eMathMLFrameType
nsMathMLFrame::GetMathMLFrameType()
{
  // see if it is an embellished operator (mapped to 'Op' in TeX)
  if (mEmbellishData.coreFrame)
    return GetMathMLFrameTypeFor(mEmbellishData.coreFrame);

  // if it has a prescribed base, fetch the type from there
  if (mPresentationData.baseFrame)
    return GetMathMLFrameTypeFor(mPresentationData.baseFrame);

  // everything else is treated as ordinary (mapped to 'Ord' in TeX)
  return eMathMLFrameType_Ordinary;  
}

NS_IMETHODIMP
nsMathMLFrame::InheritAutomaticData(nsIFrame* aParent) 
{
  mEmbellishData.flags = 0;
  mEmbellishData.coreFrame = nullptr;
  mEmbellishData.direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
  mEmbellishData.leadingSpace = 0;
  mEmbellishData.trailingSpace = 0;

  mPresentationData.flags = 0;
  mPresentationData.baseFrame = nullptr;

  // by default, just inherit the display of our parent
  nsPresentationData parentData;
  GetPresentationDataFrom(aParent, parentData);

#if defined(DEBUG) && defined(SHOW_BOUNDING_BOX)
  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLFrame::UpdatePresentationData(uint32_t        aFlagsValues,
                                      uint32_t        aWhichFlags)
{
  NS_ASSERTION(NS_MATHML_IS_COMPRESSED(aWhichFlags) ||
               NS_MATHML_IS_DTLS_SET(aWhichFlags),
               "aWhichFlags should only be compression or dtls flag");

  if (NS_MATHML_IS_COMPRESSED(aWhichFlags)) {
    // updating the compression flag is allowed
    if (NS_MATHML_IS_COMPRESSED(aFlagsValues)) {
      // 'compressed' means 'prime' style in App. G, TeXbook
      mPresentationData.flags |= NS_MATHML_COMPRESSED;
    }
    // no else. the flag is sticky. it retains its value once it is set
  }
  // These flags determine whether the dtls font feature settings should
  // be applied.
  if (NS_MATHML_IS_DTLS_SET(aWhichFlags)) {
    if (NS_MATHML_IS_DTLS_SET(aFlagsValues)) {
      mPresentationData.flags |= NS_MATHML_DTLS;
    } else {
      mPresentationData.flags &= ~NS_MATHML_DTLS;
    }
  }
  return NS_OK;
}

// Helper to give a style context suitable for doing the stretching of
// a MathMLChar. Frame classes that use this should ensure that the 
// extra leaf style contexts given to the MathMLChars are accessible to
// the Style System via the Get/Set AdditionalStyleContext() APIs.
/* static */ void
nsMathMLFrame::ResolveMathMLCharStyle(nsPresContext*  aPresContext,
                                      nsIContent*      aContent,
                                      nsStyleContext*  aParentStyleContext,
                                      nsMathMLChar*    aMathMLChar)
{
  nsCSSPseudoElements::Type pseudoType =
    nsCSSPseudoElements::ePseudo_mozMathAnonymous; // savings
  nsRefPtr<nsStyleContext> newStyleContext;
  newStyleContext = aPresContext->StyleSet()->
    ResolvePseudoElementStyle(aContent->AsElement(), pseudoType,
                              aParentStyleContext, nullptr);

  aMathMLChar->SetStyleContext(newStyleContext);
}

/* static */ void
nsMathMLFrame::GetEmbellishDataFrom(nsIFrame*        aFrame,
                                    nsEmbellishData& aEmbellishData)
{
  // initialize OUT params
  aEmbellishData.flags = 0;
  aEmbellishData.coreFrame = nullptr;
  aEmbellishData.direction = NS_STRETCH_DIRECTION_UNSUPPORTED;
  aEmbellishData.leadingSpace = 0;
  aEmbellishData.trailingSpace = 0;

  if (aFrame && aFrame->IsFrameOfType(nsIFrame::eMathML)) {
    nsIMathMLFrame* mathMLFrame = do_QueryFrame(aFrame);
    if (mathMLFrame) {
      mathMLFrame->GetEmbellishData(aEmbellishData);
    }
  }
}

// helper to get the presentation data of a frame, by possibly walking up
// the frame hierarchy if we happen to be surrounded by non-MathML frames.
/* static */ void
nsMathMLFrame::GetPresentationDataFrom(nsIFrame*           aFrame,
                                       nsPresentationData& aPresentationData,
                                       bool                aClimbTree)
{
  // initialize OUT params
  aPresentationData.flags = 0;
  aPresentationData.baseFrame = nullptr;

  nsIFrame* frame = aFrame;
  while (frame) {
    if (frame->IsFrameOfType(nsIFrame::eMathML)) {
      nsIMathMLFrame* mathMLFrame = do_QueryFrame(frame);
      if (mathMLFrame) {
        mathMLFrame->GetPresentationData(aPresentationData);
        break;
      }
    }
    // stop if the caller doesn't want to lookup beyond the frame
    if (!aClimbTree) {
      break;
    }
    // stop if we reach the root <math> tag
    nsIContent* content = frame->GetContent();
    NS_ASSERTION(content || !frame->GetParent(), // no assert for the root
                 "dangling frame without a content node"); 
    if (!content)
      break;

    if (content->Tag() == nsGkAtoms::math) {
      break;
    }
    frame = frame->GetParent();
  }
  NS_WARN_IF_FALSE(frame && frame->GetContent(),
                   "bad MathML markup - could not find the top <math> element");
}

/* static */ void
nsMathMLFrame::GetRuleThickness(nsRenderingContext& aRenderingContext,
                                nsFontMetrics*      aFontMetrics,
                                nscoord&             aRuleThickness)
{
  // get the bounding metrics of the overbar char, the rendering context
  // is assumed to have been set with the font of the current style context
  NS_ASSERTION(aRenderingContext.FontMetrics()->Font().
               Equals(aFontMetrics->Font()),
               "unexpected state");

  nscoord xHeight = aFontMetrics->XHeight();
  char16_t overBar = 0x00AF;
  nsBoundingMetrics bm = aRenderingContext.GetBoundingMetrics(&overBar, 1);
  aRuleThickness = bm.ascent + bm.descent;
  if (aRuleThickness <= 0 || aRuleThickness >= xHeight) {
    // fall-back to the other version
    GetRuleThickness(aFontMetrics, aRuleThickness);
  }
}

/* static */ void
nsMathMLFrame::GetAxisHeight(nsRenderingContext& aRenderingContext,
                             nsFontMetrics*      aFontMetrics,
                             nscoord&             aAxisHeight)
{
  gfxFont* mathFont = aFontMetrics->GetThebesFontGroup()->GetFirstMathFont();
  if (mathFont) {
    aAxisHeight =
      mathFont->GetMathConstant(gfxFontEntry::AxisHeight,
                                aFontMetrics->AppUnitsPerDevPixel());
    return;
  }

  // get the bounding metrics of the minus sign, the rendering context
  // is assumed to have been set with the font of the current style context
  NS_ASSERTION(aRenderingContext.FontMetrics()->Font().
               Equals(aFontMetrics->Font()),
               "unexpected state");

  nscoord xHeight = aFontMetrics->XHeight();
  char16_t minus = 0x2212; // not '-', but official Unicode minus sign
  nsBoundingMetrics bm = aRenderingContext.GetBoundingMetrics(&minus, 1);
  aAxisHeight = bm.ascent - (bm.ascent + bm.descent)/2;
  if (aAxisHeight <= 0 || aAxisHeight >= xHeight) {
    // fall-back to the other version
    GetAxisHeight(aFontMetrics, aAxisHeight);
  }
}

/* static */ nscoord
nsMathMLFrame::CalcLength(nsPresContext*   aPresContext,
                          nsStyleContext*   aStyleContext,
                          const nsCSSValue& aCSSValue)
{
  NS_ASSERTION(aCSSValue.IsLengthUnit(), "not a length unit");

  if (aCSSValue.IsFixedLengthUnit()) {
    return aCSSValue.GetFixedLength(aPresContext);
  }
  if (aCSSValue.IsPixelLengthUnit()) {
    return aCSSValue.GetPixelLength();
  }

  nsCSSUnit unit = aCSSValue.GetUnit();

  if (eCSSUnit_EM == unit) {
    const nsStyleFont* font = aStyleContext->StyleFont();
    return NSToCoordRound(aCSSValue.GetFloatValue() * (float)font->mFont.size);
  }
  else if (eCSSUnit_XHeight == unit) {
    nsRefPtr<nsFontMetrics> fm;
    nsLayoutUtils::GetFontMetricsForStyleContext(aStyleContext,
                                                 getter_AddRefs(fm));
    nscoord xHeight = fm->XHeight();
    return NSToCoordRound(aCSSValue.GetFloatValue() * (float)xHeight);
  }

  // MathML doesn't specify other CSS units such as rem or ch
  NS_ERROR("Unsupported unit");
  return 0;
}

/* static */ void
nsMathMLFrame::ParseNumericValue(const nsString&   aString,
                                 nscoord*          aLengthValue,
                                 uint32_t          aFlags,
                                 nsPresContext*    aPresContext,
                                 nsStyleContext*   aStyleContext)
{
  nsCSSValue cssValue;

  if (!nsMathMLElement::ParseNumericValue(aString, cssValue, aFlags,
                                          aPresContext->Document())) {
    // Invalid attribute value. aLengthValue remains unchanged, so the default
    // length value is used.
    return;
  }

  nsCSSUnit unit = cssValue.GetUnit();

  if (unit == eCSSUnit_Percent || unit == eCSSUnit_Number) {
    // Relative units. A multiple of the default length value is used.
    *aLengthValue = NSToCoordRound(*aLengthValue * (unit == eCSSUnit_Percent ?
                                                    cssValue.GetPercentValue() :
                                                    cssValue.GetFloatValue()));
    return;
  }
  
  // Absolute units.
  *aLengthValue = CalcLength(aPresContext, aStyleContext, cssValue);
}

// ================
// Utils to map attributes into CSS rules (work-around to bug 69409 which
// is not scheduled to be fixed anytime soon)
//

struct
nsCSSMapping {
  int32_t        compatibility;
  const nsIAtom* attrAtom;
  const char*    cssProperty;
};

#if defined(DEBUG) && defined(SHOW_BOUNDING_BOX)
class nsDisplayMathMLBoundingMetrics : public nsDisplayItem {
public:
  nsDisplayMathMLBoundingMetrics(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame, const nsRect& aRect)
    : nsDisplayItem(aBuilder, aFrame), mRect(aRect) {
    MOZ_COUNT_CTOR(nsDisplayMathMLBoundingMetrics);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayMathMLBoundingMetrics() {
    MOZ_COUNT_DTOR(nsDisplayMathMLBoundingMetrics);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) MOZ_OVERRIDE;
  NS_DISPLAY_DECL_NAME("MathMLBoundingMetrics", TYPE_MATHML_BOUNDING_METRICS)
private:
  nsRect    mRect;
};

void nsDisplayMathMLBoundingMetrics::Paint(nsDisplayListBuilder* aBuilder,
                                           nsRenderingContext* aCtx)
{
  DrawTarget* drawTarget = aRenderingContext->GetDrawTarget();
  Rect r = NSRectToRect(mRect + ToReferenceFrame(),
                        mFrame->PresContext()->AppUnitsPerDevPixel());
  ColorPattern blue(ToDeviceColor(Color(0.f, 0.f, 1.f, 1.f)));
  drawTarget->StrokeRect(r, blue);
}

nsresult
nsMathMLFrame::DisplayBoundingMetrics(nsDisplayListBuilder* aBuilder,
                                      nsIFrame* aFrame, const nsPoint& aPt,
                                      const nsBoundingMetrics& aMetrics,
                                      const nsDisplayListSet& aLists) {
  if (!NS_MATHML_PAINT_BOUNDING_METRICS(mPresentationData.flags))
    return NS_OK;
    
  nscoord x = aPt.x + aMetrics.leftBearing;
  nscoord y = aPt.y - aMetrics.ascent;
  nscoord w = aMetrics.rightBearing - aMetrics.leftBearing;
  nscoord h = aMetrics.ascent + aMetrics.descent;

  return aLists.Content()->AppendNewToTop(new (aBuilder)
      nsDisplayMathMLBoundingMetrics(aBuilder, this, nsRect(x,y,w,h)));
}
#endif

class nsDisplayMathMLBar : public nsDisplayItem {
public:
  nsDisplayMathMLBar(nsDisplayListBuilder* aBuilder,
                     nsIFrame* aFrame, const nsRect& aRect)
    : nsDisplayItem(aBuilder, aFrame), mRect(aRect) {
    MOZ_COUNT_CTOR(nsDisplayMathMLBar);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayMathMLBar() {
    MOZ_COUNT_DTOR(nsDisplayMathMLBar);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) MOZ_OVERRIDE;
  NS_DISPLAY_DECL_NAME("MathMLBar", TYPE_MATHML_BAR)
private:
  nsRect    mRect;
};

void nsDisplayMathMLBar::Paint(nsDisplayListBuilder* aBuilder,
                               nsRenderingContext* aCtx)
{
  // paint the bar with the current text color
  DrawTarget* drawTarget = aCtx->GetDrawTarget();
  Rect rect = NSRectToRect(mRect + ToReferenceFrame(),
                           mFrame->PresContext()->AppUnitsPerDevPixel(),
                           *drawTarget);
  ColorPattern color(ToDeviceColor(
                       mFrame->GetVisitedDependentColor(eCSSProperty_color)));
  drawTarget->FillRect(rect, color);
}

void
nsMathMLFrame::DisplayBar(nsDisplayListBuilder* aBuilder,
                          nsIFrame* aFrame, const nsRect& aRect,
                          const nsDisplayListSet& aLists) {
  if (!aFrame->StyleVisibility()->IsVisible() || aRect.IsEmpty())
    return;

  aLists.Content()->AppendNewToTop(new (aBuilder)
    nsDisplayMathMLBar(aBuilder, aFrame, aRect));
}

void
nsMathMLFrame::GetRadicalParameters(nsFontMetrics* aFontMetrics,
                                    bool aDisplayStyle,
                                    nscoord& aRadicalRuleThickness,
                                    nscoord& aRadicalExtraAscender,
                                    nscoord& aRadicalVerticalGap)
{
  nscoord oneDevPixel = aFontMetrics->AppUnitsPerDevPixel();
  gfxFont* mathFont = aFontMetrics->GetThebesFontGroup()->GetFirstMathFont();

  // get the radical rulethickness
  if (mathFont) {
    aRadicalRuleThickness =
      mathFont->GetMathConstant(gfxFontEntry::RadicalRuleThickness,
                                oneDevPixel);
  } else {
    GetRuleThickness(aFontMetrics, aRadicalRuleThickness);
  }

  // get the leading to be left at the top of the resulting frame
  if (mathFont) {
    aRadicalExtraAscender =
      mathFont->GetMathConstant(gfxFontEntry::RadicalExtraAscender,
                                oneDevPixel);
  } else {
    // This seems more reliable than using aFontMetrics->GetLeading() on
    // suspicious fonts.
    nscoord em;
    GetEmHeight(aFontMetrics, em);
    aRadicalExtraAscender = nscoord(0.2f * em);
  }

  // get the clearance between rule and content
  if (mathFont) {
    aRadicalVerticalGap =
      mathFont->GetMathConstant(aDisplayStyle ?
                                gfxFontEntry::RadicalDisplayStyleVerticalGap :
                                gfxFontEntry::RadicalVerticalGap,
                                oneDevPixel);
  } else {
    // Rule 11, App. G, TeXbook
    aRadicalVerticalGap = aRadicalRuleThickness +
      (aDisplayStyle ? aFontMetrics->XHeight() : aRadicalRuleThickness) / 4;
  }
}
