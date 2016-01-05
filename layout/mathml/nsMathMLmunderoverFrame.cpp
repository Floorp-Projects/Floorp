/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLmunderoverFrame.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsMathMLmmultiscriptsFrame.h"
#include <algorithm>

//
// <munderover> -- attach an underscript-overscript pair to a base - implementation
// <mover> -- attach an overscript to a base - implementation
// <munder> -- attach an underscript to a base - implementation
//

nsIFrame*
NS_NewMathMLmunderoverFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmunderoverFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmunderoverFrame)

nsMathMLmunderoverFrame::~nsMathMLmunderoverFrame()
{
}

nsresult
nsMathMLmunderoverFrame::AttributeChanged(int32_t         aNameSpaceID,
                                          nsIAtom*        aAttribute,
                                          int32_t         aModType)
{
  if (nsGkAtoms::accent_ == aAttribute ||
      nsGkAtoms::accentunder_ == aAttribute) {
    // When we have automatic data to update within ourselves, we ask our
    // parent to re-layout its children
    return ReLayoutChildren(GetParent());
  }

  return nsMathMLContainerFrame::
         AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::UpdatePresentationData(uint32_t        aFlagsValues,
                                                uint32_t        aFlagsToUpdate)
{
  nsMathMLContainerFrame::UpdatePresentationData(aFlagsValues, aFlagsToUpdate);
  // disable the stretch-all flag if we are going to act like a subscript-superscript pair
  if (NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
      StyleFont()->mMathDisplay == NS_MATHML_DISPLAYSTYLE_INLINE) {
    mPresentationData.flags &= ~NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;
  }
  else {
    mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;

  return NS_OK;
}

uint8_t
nsMathMLmunderoverFrame::ScriptIncrement(nsIFrame* aFrame)
{
  nsIFrame* child = mFrames.FirstChild();
  if (!aFrame || aFrame == child) {
    return 0;
  }
  child = child->GetNextSibling();
  if (aFrame == child) {
    if (mContent->IsMathMLElement(nsGkAtoms::mover_)) {
      return mIncrementOver ? 1 : 0;
    }
    return mIncrementUnder ? 1 : 0;
  }
  if (child && aFrame == child->GetNextSibling()) {
    // must be a over frame of munderover
    return mIncrementOver ? 1 : 0;
  }
  return 0;  // frame not found
}

NS_IMETHODIMP
nsMathMLmunderoverFrame::TransmitAutomaticData()
{
  // At this stage, all our children are in sync and we can fully
  // resolve our own mEmbellishData struct
  //---------------------------------------------------------------------

  /* 
  The REC says:

  As regards munder (respectively mover) :
  The default value of accentunder is false, unless underscript
  is an <mo> element or an embellished operator.  If underscript is 
  an <mo> element, the value of its accent attribute is used as the
  default value of accentunder. If underscript is an embellished
  operator, the accent attribute of the <mo> element at its
  core is used as the default value. As with all attributes, an
  explicitly given value overrides the default.

XXX The winner is the outermost setting in conflicting settings like these:
<munder accentunder='true'>
  <mi>...</mi>
  <mo accentunder='false'> ... </mo>
</munder>

  As regards munderover:
  The accent and accentunder attributes have the same effect as
  the attributes with the same names on <mover>  and <munder>, 
  respectively. Their default values are also computed in the 
  same manner as described for those elements, with the default
  value of accent depending on overscript and the default value
  of accentunder depending on underscript.
  */

  nsIFrame* overscriptFrame = nullptr;
  nsIFrame* underscriptFrame = nullptr;
  nsIFrame* baseFrame = mFrames.FirstChild();

  if (baseFrame) {
    if (mContent->IsAnyOfMathMLElements(nsGkAtoms::munder_,
                                        nsGkAtoms::munderover_)) {
      underscriptFrame = baseFrame->GetNextSibling();
    } else {
      NS_ASSERTION(mContent->IsMathMLElement(nsGkAtoms::mover_),
                   "mContent->NodeInfo()->NameAtom() not recognized");
      overscriptFrame = baseFrame->GetNextSibling();
    }
  }
  if (underscriptFrame &&
      mContent->IsMathMLElement(nsGkAtoms::munderover_)) {
    overscriptFrame = underscriptFrame->GetNextSibling();

  }

  // if our base is an embellished operator, let its state bubble to us (in particular,
  // this is where we get the flag for NS_MATHML_EMBELLISH_MOVABLELIMITS). Our flags
  // are reset to the default values of false if the base frame isn't embellished.
  mPresentationData.baseFrame = baseFrame;
  GetEmbellishDataFrom(baseFrame, mEmbellishData);

  // The default value of accentunder is false, unless the underscript is embellished
  // and its core <mo> is an accent
  nsEmbellishData embellishData;
  nsAutoString value;
  if (mContent->IsAnyOfMathMLElements(nsGkAtoms::munder_,
                                      nsGkAtoms::munderover_)) {
    GetEmbellishDataFrom(underscriptFrame, embellishData);
    if (NS_MATHML_EMBELLISH_IS_ACCENT(embellishData.flags)) {
      mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTUNDER;
    } else {
      mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTUNDER;
    }    

    // if we have an accentunder attribute, it overrides what the underscript said
    if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accentunder_, value)) {
      if (value.EqualsLiteral("true")) {
        mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTUNDER;
      } else if (value.EqualsLiteral("false")) {
        mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTUNDER;
      }
    }
  }

  // The default value of accent is false, unless the overscript is embellished
  // and its core <mo> is an accent
  if (mContent->IsAnyOfMathMLElements(nsGkAtoms::mover_,
                                      nsGkAtoms::munderover_)) {
    GetEmbellishDataFrom(overscriptFrame, embellishData);
    if (NS_MATHML_EMBELLISH_IS_ACCENT(embellishData.flags)) {
      mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTOVER;
    } else {
      mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTOVER;
    }

    // if we have an accent attribute, it overrides what the overscript said
    if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::accent_, value)) {
      if (value.EqualsLiteral("true")) {
        mEmbellishData.flags |= NS_MATHML_EMBELLISH_ACCENTOVER;
      } else if (value.EqualsLiteral("false")) {
        mEmbellishData.flags &= ~NS_MATHML_EMBELLISH_ACCENTOVER;
      }
    }
  }

  bool subsupDisplay =
    NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
    StyleFont()->mMathDisplay == NS_MATHML_DISPLAYSTYLE_INLINE;

  // disable the stretch-all flag if we are going to act like a superscript
  if (subsupDisplay) {
    mPresentationData.flags &= ~NS_MATHML_STRETCH_ALL_CHILDREN_HORIZONTALLY;
  }

  // Now transmit any change that we want to our children so that they
  // can update their mPresentationData structs
  //---------------------------------------------------------------------

  /* The REC says:
     Within underscript, <munderover> always sets displaystyle to "false",
     but increments scriptlevel by 1 only when accentunder is "false". 

     Within overscript, <munderover> always sets displaystyle to "false", 
     but increments scriptlevel by 1 only when accent is "false".
 
     Within subscript and superscript it increments scriptlevel by 1, and 
     sets displaystyle to "false", but leaves both attributes unchanged within 
     base.

     The TeXBook treats 'over' like a superscript, so p.141 or Rule 13a
     say it shouldn't be compressed. However, The TeXBook says
     that math accents and \overline change uncramped styles to their
     cramped counterparts.
  */
  if (mContent->IsAnyOfMathMLElements(nsGkAtoms::mover_,
                                      nsGkAtoms::munderover_)) {
    uint32_t compress = NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags)
      ? NS_MATHML_COMPRESSED : 0;
    mIncrementOver =
      !NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags) ||
      subsupDisplay;
    SetIncrementScriptLevel(mContent->IsMathMLElement(nsGkAtoms::mover_) ? 1 : 2, mIncrementOver);
    if (mIncrementOver) {
      PropagateFrameFlagFor(overscriptFrame,
                            NS_FRAME_MATHML_SCRIPT_DESCENDANT);
    }
    PropagatePresentationDataFor(overscriptFrame, compress, compress);
  }
  /*
     The TeXBook treats 'under' like a subscript, so p.141 or Rule 13a 
     say it should be compressed
  */
  if (mContent->IsAnyOfMathMLElements(nsGkAtoms::munder_,
                                      nsGkAtoms::munderover_)) {
    mIncrementUnder =
      !NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags) ||
      subsupDisplay;
    SetIncrementScriptLevel(1, mIncrementUnder);
    if (mIncrementUnder) {
      PropagateFrameFlagFor(underscriptFrame,
                            NS_FRAME_MATHML_SCRIPT_DESCENDANT);
    }
    PropagatePresentationDataFor(underscriptFrame,
                                 NS_MATHML_COMPRESSED,
                                 NS_MATHML_COMPRESSED);
  }

  /* Set flags for dtls font feature settings.

     dtls
     Dotless Forms
     This feature provides dotless forms for Math Alphanumeric
     characters, such as U+1D422 MATHEMATICAL BOLD SMALL I,
     U+1D423 MATHEMATICAL BOLD SMALL J, U+1D456
     U+MATHEMATICAL ITALIC SMALL I, U+1D457 MATHEMATICAL ITALIC
     SMALL J, and so on.
     The dotless forms are to be used as base forms for placing
     mathematical accents over them.

     To opt out of this change, add the following to the stylesheet:
     "font-feature-settings: 'dtls' 0"
   */
  if (overscriptFrame &&
      NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags) &&
      !NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags)) {
    PropagatePresentationDataFor(baseFrame, NS_MATHML_DTLS, NS_MATHML_DTLS);
  }

  return NS_OK;
}

/*
The REC says:
*  If the base is an operator with movablelimits="true" (or an embellished
   operator whose <mo> element core has movablelimits="true"), and
   displaystyle="false", then underscript and overscript are drawn in
   a subscript and superscript position, respectively. In this case, 
   the accent and accentunder attributes are ignored. This is often
   used for limits on symbols such as &sum;.

i.e.,:
 if (NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishDataflags) &&
     StyleFont()->mMathDisplay == NS_MATHML_DISPLAYSTYLE_INLINE) {
  // place like subscript-superscript pair
 }
 else {
  // place like underscript-overscript pair
 }
*/

/* virtual */ nsresult
nsMathMLmunderoverFrame::Place(DrawTarget*          aDrawTarget,
                               bool                 aPlaceOrigin,
                               nsHTMLReflowMetrics& aDesiredSize)
{
  float fontSizeInflation = nsLayoutUtils::FontSizeInflationFor(this);
  if (NS_MATHML_EMBELLISH_IS_MOVABLELIMITS(mEmbellishData.flags) &&
      StyleFont()->mMathDisplay == NS_MATHML_DISPLAYSTYLE_INLINE) {
    //place like sub sup or subsup
    if (mContent->IsMathMLElement(nsGkAtoms::munderover_)) {
      return nsMathMLmmultiscriptsFrame::PlaceMultiScript(PresContext(),
                                                          aDrawTarget,
                                                          aPlaceOrigin,
                                                          aDesiredSize,
                                                          this, 0, 0,
                                                          fontSizeInflation);
    } else if (mContent->IsMathMLElement( nsGkAtoms::munder_)) {
      return nsMathMLmmultiscriptsFrame::PlaceMultiScript(PresContext(),
                                                          aDrawTarget,
                                                          aPlaceOrigin,
                                                          aDesiredSize,
                                                          this, 0, 0,
                                                          fontSizeInflation);
    } else {
      NS_ASSERTION(mContent->IsMathMLElement(nsGkAtoms::mover_),
                   "mContent->NodeInfo()->NameAtom() not recognized");
      return nsMathMLmmultiscriptsFrame::PlaceMultiScript(PresContext(),
                                                          aDrawTarget,
                                                          aPlaceOrigin,
                                                          aDesiredSize,
                                                          this, 0, 0,
                                                          fontSizeInflation);
    }
    
  }

  ////////////////////////////////////
  // Get the children's desired sizes

  nsBoundingMetrics bmBase, bmUnder, bmOver;
  nsHTMLReflowMetrics baseSize(aDesiredSize.GetWritingMode());
  nsHTMLReflowMetrics underSize(aDesiredSize.GetWritingMode());
  nsHTMLReflowMetrics overSize(aDesiredSize.GetWritingMode());
  nsIFrame* overFrame = nullptr;
  nsIFrame* underFrame = nullptr;
  nsIFrame* baseFrame = mFrames.FirstChild();
  underSize.SetBlockStartAscent(0);
  overSize.SetBlockStartAscent(0);
  bool haveError = false;
  if (baseFrame) {
    if (mContent->IsAnyOfMathMLElements(nsGkAtoms::munder_,
                                        nsGkAtoms::munderover_)) {
      underFrame = baseFrame->GetNextSibling();
    } else if (mContent->IsMathMLElement(nsGkAtoms::mover_)) {
      overFrame = baseFrame->GetNextSibling();
    }
  }
  if (underFrame && mContent->IsMathMLElement(nsGkAtoms::munderover_)) {
    overFrame = underFrame->GetNextSibling();
  }
  
  if (mContent->IsMathMLElement(nsGkAtoms::munder_)) {
    if (!baseFrame || !underFrame || underFrame->GetNextSibling()) {
      // report an error, encourage people to get their markups in order
      haveError = true;
    }
  }
  if (mContent->IsMathMLElement(nsGkAtoms::mover_)) {
    if (!baseFrame || !overFrame || overFrame->GetNextSibling()) {
      // report an error, encourage people to get their markups in order
      haveError = true;
    }
  }
  if (mContent->IsMathMLElement(nsGkAtoms::munderover_)) {
    if (!baseFrame || !underFrame || !overFrame || overFrame->GetNextSibling()) {
      // report an error, encourage people to get their markups in order
      haveError = true;
    }
  }
  if (haveError) {
    if (aPlaceOrigin) {
      ReportChildCountError();
    }
    return ReflowError(aDrawTarget, aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  if (underFrame) {
    GetReflowAndBoundingMetricsFor(underFrame, underSize, bmUnder);
  }
  if (overFrame) {
    GetReflowAndBoundingMetricsFor(overFrame, overSize, bmOver);
  }

  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);

  ////////////////////
  // Place Children

  RefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm),
                                        fontSizeInflation);

  nscoord xHeight = fm->XHeight();
  nscoord oneDevPixel = fm->AppUnitsPerDevPixel();
  gfxFont* mathFont = fm->GetThebesFontGroup()->GetFirstMathFont();

  nscoord ruleThickness;
  GetRuleThickness (aDrawTarget, fm, ruleThickness);

  nscoord correction = 0;
  GetItalicCorrection (bmBase, correction);

  // there are 2 different types of placement depending on 
  // whether we want an accented under or not

  nscoord underDelta1 = 0; // gap between base and underscript
  nscoord underDelta2 = 0; // extra space beneath underscript

  if (!NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags)) {
    // Rule 13a, App. G, TeXbook
    nscoord bigOpSpacing2, bigOpSpacing4, bigOpSpacing5, dummy; 
    GetBigOpSpacings (fm, 
                      dummy, bigOpSpacing2, 
                      dummy, bigOpSpacing4, 
                      bigOpSpacing5);
    if (mathFont) {
      // XXXfredw The Open Type MATH table has some StretchStack* parameters
      // that we may use when the base is a stretchy horizontal operator. See
      // bug 963131.
      bigOpSpacing2 =
        mathFont->GetMathConstant(gfxFontEntry::LowerLimitGapMin,
                                  oneDevPixel);
      bigOpSpacing4 =
        mathFont->GetMathConstant(gfxFontEntry::LowerLimitBaselineDropMin,
                                  oneDevPixel);
      bigOpSpacing5 = 0;
    }
    underDelta1 = std::max(bigOpSpacing2, (bigOpSpacing4 - bmUnder.ascent));
    underDelta2 = bigOpSpacing5;
  }
  else {
    // No corresponding rule in TeXbook - we are on our own here
    // XXX tune the gap delta between base and underscript 
    // XXX Should we use Rule 10 like \underline does?
    // XXXfredw Perhaps use the Underbar* parameters of the MATH table. See
    // bug 963125.
    underDelta1 = ruleThickness + onePixel/2;
    underDelta2 = ruleThickness;
  }
  // empty under?
  if (!(bmUnder.ascent + bmUnder.descent)) {
    underDelta1 = 0;
    underDelta2 = 0;
  }

  nscoord overDelta1 = 0; // gap between base and overscript
  nscoord overDelta2 = 0; // extra space above overscript

  if (!NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags)) {    
    // Rule 13a, App. G, TeXbook
    // XXXfredw The Open Type MATH table has some StretchStack* parameters
    // that we may use when the base is a stretchy horizontal operator. See
    // bug 963131.
    nscoord bigOpSpacing1, bigOpSpacing3, bigOpSpacing5, dummy; 
    GetBigOpSpacings (fm, 
                      bigOpSpacing1, dummy, 
                      bigOpSpacing3, dummy, 
                      bigOpSpacing5);
    if (mathFont) {
      // XXXfredw The Open Type MATH table has some StretchStack* parameters
      // that we may use when the base is a stretchy horizontal operator. See
      // bug 963131.
      bigOpSpacing1 =
        mathFont->GetMathConstant(gfxFontEntry::UpperLimitGapMin,
                                  oneDevPixel);
      bigOpSpacing3 =
        mathFont->GetMathConstant(gfxFontEntry::UpperLimitBaselineRiseMin,
                                  oneDevPixel);
      bigOpSpacing5 = 0;
    }
    overDelta1 = std::max(bigOpSpacing1, (bigOpSpacing3 - bmOver.descent));
    overDelta2 = bigOpSpacing5;

    // XXX This is not a TeX rule... 
    // delta1 (as computed abvove) can become really big when bmOver.descent is
    // negative,  e.g., if the content is &OverBar. In such case, we use the height
    if (bmOver.descent < 0)    
      overDelta1 = std::max(bigOpSpacing1, (bigOpSpacing3 - (bmOver.ascent + bmOver.descent)));
  }
  else {
    // Rule 12, App. G, TeXbook
    // We are going to modify this rule to make it more general.
    // The idea behind Rule 12 in the TeXBook is to keep the accent
    // as close to the base as possible, while ensuring that the
    // distance between the *baseline* of the accent char and 
    // the *baseline* of the base is atleast x-height. 
    // The idea is that for normal use, we would like all the accents
    // on a line to line up atleast x-height above the baseline 
    // if possible. 
    // When the ascent of the base is >= x-height, 
    // the baseline of the accent char is placed just above the base
    // (specifically, the baseline of the accent char is placed 
    // above the baseline of the base by the ascent of the base).
    // For ease of implementation, 
    // this assumes that the font-designer designs accents 
    // in such a way that the bottom of the accent is atleast x-height
    // above its baseline, otherwise there will be collisions
    // with the base. Also there should be proper padding between
    // the bottom of the accent char and its baseline.
    // The above rule may not be obvious from a first
    // reading of rule 12 in the TeXBook !!!
    // The mathml <mover> tag can use accent chars that
    // do not follow this convention. So we modify TeX's rule 
    // so that TeX's rule gets subsumed for accents that follow 
    // TeX's convention,
    // while also allowing accents that do not follow the convention :
    // we try to keep the *bottom* of the accent char atleast x-height 
    // from the baseline of the base char. we also slap on an extra
    // padding between the accent and base chars.
    overDelta1 = ruleThickness + onePixel/2;
    nscoord accentBaseHeight = xHeight;
    if (mathFont) {
      accentBaseHeight =
        mathFont->GetMathConstant(gfxFontEntry::AccentBaseHeight,
                                  oneDevPixel);
    }
    if (bmBase.ascent < accentBaseHeight) {
      // also ensure at least accentBaseHeight above the baseline of the base
      overDelta1 += accentBaseHeight - bmBase.ascent;
    }
    overDelta2 = ruleThickness;
  }
  // empty over?
  if (!(bmOver.ascent + bmOver.descent)) {
    overDelta1 = 0;
    overDelta2 = 0;
  }

  nscoord dxBase = 0, dxOver = 0, dxUnder = 0;
  nsAutoString valueAlign;
  enum {
    center,
    left,
    right
  } alignPosition = center;

  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::align, valueAlign)) {
    if (valueAlign.EqualsLiteral("left")) {
      alignPosition = left;
    } else if (valueAlign.EqualsLiteral("right")) {
      alignPosition = right;
    }
  }

  //////////
  // pass 1, do what <mover> does: attach the overscript on the base

  // Ad-hoc - This is to override fonts which have ready-made _accent_
  // glyphs with negative lbearing and rbearing. We want to position
  // the overscript ourselves
  nscoord overWidth = bmOver.width;
  if (!overWidth && (bmOver.rightBearing - bmOver.leftBearing > 0)) {
    overWidth = bmOver.rightBearing - bmOver.leftBearing;
    dxOver = -bmOver.leftBearing;
  }

  if (NS_MATHML_EMBELLISH_IS_ACCENTOVER(mEmbellishData.flags)) {
    mBoundingMetrics.width = bmBase.width; 
    if (alignPosition == center) {
      dxOver += correction;
    }
  }
  else {
    mBoundingMetrics.width = std::max(bmBase.width, overWidth);
    if (alignPosition == center) {
      dxOver += correction/2;
    }
  }
  
  if (alignPosition == center) {
    dxOver += (mBoundingMetrics.width - overWidth)/2;
    dxBase = (mBoundingMetrics.width - bmBase.width)/2;
  } else if (alignPosition == right) {
    dxOver += mBoundingMetrics.width - overWidth;
    dxBase = mBoundingMetrics.width - bmBase.width;
  }

  mBoundingMetrics.ascent = 
    bmBase.ascent + overDelta1 + bmOver.ascent + bmOver.descent;
  mBoundingMetrics.descent = bmBase.descent;
  mBoundingMetrics.leftBearing = 
    std::min(dxBase + bmBase.leftBearing, dxOver + bmOver.leftBearing);
  mBoundingMetrics.rightBearing = 
    std::max(dxBase + bmBase.rightBearing, dxOver + bmOver.rightBearing);

  //////////
  // pass 2, do what <munder> does: attach the underscript on the previous
  // result. We conceptually view the previous result as an "anynomous base" 
  // from where to attach the underscript. Hence if the underscript is empty,
  // we should end up like <mover>. If the overscript is empty, we should
  // end up like <munder>.

  nsBoundingMetrics bmAnonymousBase = mBoundingMetrics;
  nscoord ascentAnonymousBase =
    std::max(mBoundingMetrics.ascent + overDelta2,
             overSize.BlockStartAscent() + bmOver.descent +
             overDelta1 + bmBase.ascent);
  ascentAnonymousBase = std::max(ascentAnonymousBase,
                                 baseSize.BlockStartAscent());

  // Width of non-spacing marks is zero so use left and right bearing.
  nscoord underWidth = bmUnder.width;
  if (!underWidth) {
    underWidth = bmUnder.rightBearing - bmUnder.leftBearing;
    dxUnder = -bmUnder.leftBearing;
  }

  nscoord maxWidth = std::max(bmAnonymousBase.width, underWidth);
  if (alignPosition == center &&
      !NS_MATHML_EMBELLISH_IS_ACCENTUNDER(mEmbellishData.flags)) {
    GetItalicCorrection(bmAnonymousBase, correction);
    dxUnder += -correction/2;
  }
  nscoord dxAnonymousBase = 0;
  if (alignPosition == center) {
    dxUnder += (maxWidth - underWidth)/2;
    dxAnonymousBase = (maxWidth - bmAnonymousBase.width)/2;
  } else if (alignPosition == right) {
    dxUnder += maxWidth - underWidth;
    dxAnonymousBase = maxWidth - bmAnonymousBase.width;
  }

  // adjust the offsets of the real base and overscript since their
  // final offsets should be relative to us...
  dxOver += dxAnonymousBase;
  dxBase += dxAnonymousBase;

  mBoundingMetrics.width =
    std::max(dxAnonymousBase + bmAnonymousBase.width, dxUnder + bmUnder.width);
  // At this point, mBoundingMetrics.ascent = bmAnonymousBase.ascent 
  mBoundingMetrics.descent = 
    bmAnonymousBase.descent + underDelta1 + bmUnder.ascent + bmUnder.descent;
  mBoundingMetrics.leftBearing =
    std::min(dxAnonymousBase + bmAnonymousBase.leftBearing, dxUnder + bmUnder.leftBearing);
  mBoundingMetrics.rightBearing = 
    std::max(dxAnonymousBase + bmAnonymousBase.rightBearing, dxUnder + bmUnder.rightBearing);

  aDesiredSize.SetBlockStartAscent(ascentAnonymousBase);
  aDesiredSize.Height() = aDesiredSize.BlockStartAscent() +
    std::max(mBoundingMetrics.descent + underDelta2,
           bmAnonymousBase.descent + underDelta1 + bmUnder.ascent +
             underSize.Height() - underSize.BlockStartAscent());
  aDesiredSize.Height() = std::max(aDesiredSize.Height(),
                               aDesiredSize.BlockStartAscent() +
                               baseSize.Height() - baseSize.BlockStartAscent());
  aDesiredSize.Width() = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.BlockStartAscent();

  if (aPlaceOrigin) {
    nscoord dy;
    // place overscript
    if (overFrame) {
      dy = aDesiredSize.BlockStartAscent() -
           mBoundingMetrics.ascent + bmOver.ascent -
           overSize.BlockStartAscent();
      FinishReflowChild (overFrame, PresContext(), overSize, nullptr, dxOver, dy, 0);
    }
    // place base
    dy = aDesiredSize.BlockStartAscent() - baseSize.BlockStartAscent();
    FinishReflowChild (baseFrame, PresContext(), baseSize, nullptr, dxBase, dy, 0);
    // place underscript
    if (underFrame) {
      dy = aDesiredSize.BlockStartAscent() +
           mBoundingMetrics.descent - bmUnder.descent -
           underSize.BlockStartAscent();
      FinishReflowChild (underFrame, PresContext(), underSize, nullptr,
                         dxUnder, dy, 0);
    }
  }
  return NS_OK;
}
