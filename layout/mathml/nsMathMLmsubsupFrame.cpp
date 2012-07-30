/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsRenderingContext.h"

#include "nsMathMLmsubsupFrame.h"

//
// <msubsup> -- attach a subscript-superscript pair to a base - implementation
//

nsIFrame*
NS_NewMathMLmsubsupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmsubsupFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmsubsupFrame)

nsMathMLmsubsupFrame::~nsMathMLmsubsupFrame()
{
}

NS_IMETHODIMP
nsMathMLmsubsupFrame::TransmitAutomaticData()
{
  // if our base is an embellished operator, let its state bubble to us
  mPresentationData.baseFrame = mFrames.FirstChild();
  GetEmbellishDataFrom(mPresentationData.baseFrame, mEmbellishData);

  // 1. The REC says:
  //    The <msubsup> element increments scriptlevel by 1, and sets displaystyle to
  //    "false", within subscript and superscript, but leaves both attributes
  //    unchanged within base.
  // 2. The TeXbook (Ch 17. p.141) says the superscript inherits the compression
  //    while the subscript is compressed
  UpdatePresentationDataFromChildAt(1, -1,
    ~NS_MATHML_DISPLAYSTYLE,
     NS_MATHML_DISPLAYSTYLE);
  UpdatePresentationDataFromChildAt(1,  1,
     NS_MATHML_COMPRESSED,
     NS_MATHML_COMPRESSED);

  return NS_OK;
}

/* virtual */ nsresult
nsMathMLmsubsupFrame::Place(nsRenderingContext& aRenderingContext,
                            bool                 aPlaceOrigin,
                            nsHTMLReflowMetrics& aDesiredSize)
{
  // extra spacing between base and sup/subscript
  nscoord scriptSpace = 0;

  // subscriptshift
  //
  // "Specifies the minimum amount to shift the baseline of subscript down; the
  // default is for the rendering agent to use its own positioning rules."
  //
  // values: length
  // default: automatic
  //
  // We use 0 as the default value so unitless values can be ignored.
  // XXXfredw Should we forbid negative values? (bug 411227)
  //
  nsAutoString value;
  nscoord subScriptShift = 0;
  GetAttribute(mContent, mPresentationData.mstyle,
               nsGkAtoms::subscriptshift_, value);
  if (!value.IsEmpty()) {
    ParseNumericValue(value, &subScriptShift,
                      nsMathMLElement::PARSE_ALLOW_NEGATIVE,
                      PresContext(), mStyleContext);
  }
  // superscriptshift
  //
  // "Specifies the minimum amount to shift the baseline of superscript up; the
  // default is for the rendering agent to use its own positioning rules."
  //
  // values: length
  // default: automatic
  //
  // We use 0 as the default value so unitless values can be ignored.
  // XXXfredw Should we forbid negative values? (bug 411227)
  //
  nscoord supScriptShift = 0;
  GetAttribute(mContent, mPresentationData.mstyle,
               nsGkAtoms::superscriptshift_, value);
  if (!value.IsEmpty()) {
    ParseNumericValue(value, &supScriptShift,
                      nsMathMLElement::PARSE_ALLOW_NEGATIVE,
                      PresContext(), mStyleContext);
  }

  return nsMathMLmsubsupFrame::PlaceSubSupScript(PresContext(),
                                                 aRenderingContext,
                                                 aPlaceOrigin,
                                                 aDesiredSize,
                                                 this,
                                                 subScriptShift,
                                                 supScriptShift,
                                                 scriptSpace);
}

// exported routine that both munderover and msubsup share.
// munderover uses this when movablelimits is set.
nsresult
nsMathMLmsubsupFrame::PlaceSubSupScript(nsPresContext*      aPresContext,
                                        nsRenderingContext& aRenderingContext,
                                        bool                 aPlaceOrigin,
                                        nsHTMLReflowMetrics& aDesiredSize,
                                        nsMathMLContainerFrame* aFrame,
                                        nscoord              aUserSubScriptShift,
                                        nscoord              aUserSupScriptShift,
                                        nscoord              aScriptSpace)
{
  // force the scriptSpace to be atleast 1 pixel
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);
  aScriptSpace = NS_MAX(onePixel, aScriptSpace);

  ////////////////////////////////////
  // Get the children's desired sizes

  nsHTMLReflowMetrics baseSize;
  nsHTMLReflowMetrics subScriptSize;
  nsHTMLReflowMetrics supScriptSize;
  nsBoundingMetrics bmBase, bmSubScript, bmSupScript;
  nsIFrame* subScriptFrame = nullptr;
  nsIFrame* supScriptFrame = nullptr;
  nsIFrame* baseFrame = aFrame->GetFirstPrincipalChild();
  if (baseFrame)
    subScriptFrame = baseFrame->GetNextSibling();
  if (subScriptFrame)
    supScriptFrame = subScriptFrame->GetNextSibling();
  if (!baseFrame || !subScriptFrame || !supScriptFrame ||
      supScriptFrame->GetNextSibling()) {
    // report an error, encourage people to get their markups in order
    return aFrame->ReflowError(aRenderingContext, aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  GetReflowAndBoundingMetricsFor(subScriptFrame, subScriptSize, bmSubScript);
  GetReflowAndBoundingMetricsFor(supScriptFrame, supScriptSize, bmSupScript);

  // get the subdrop from the subscript font
  nscoord subDrop;
  GetSubDropFromChild(subScriptFrame, subDrop);
  // parameter v, Rule 18a, App. G, TeXbook
  nscoord minSubScriptShift = bmBase.descent + subDrop;

  // get the supdrop from the supscript font
  nscoord supDrop;
  GetSupDropFromChild(supScriptFrame, supDrop);
  // parameter u, Rule 18a, App. G, TeXbook
  nscoord minSupScriptShift = bmBase.ascent - supDrop;

  //////////////////
  // Place Children
  //////////////////

  //////////////////////////////////////////////////
  // Get subscript shift
  // slightly different from nsMathMLmsubFrame.cpp
  //////////////////////////////////////////////////

  // subScriptShift{1,2}
  // = minimum amount to shift the subscript down
  // = sub{1,2} in TeXbook
  // subScriptShift1 = subscriptshift attribute * x-height
  nscoord subScriptShift1, subScriptShift2;

  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(baseFrame, getter_AddRefs(fm));
  aRenderingContext.SetFont(fm);

  // get x-height (an ex)
  nscoord xHeight = fm->XHeight();

  nscoord ruleSize;
  GetRuleThickness (aRenderingContext, fm, ruleSize);

  // Get subScriptShift{1,2} default from font
  GetSubScriptShifts (fm, subScriptShift1, subScriptShift2);

  if (0 < aUserSubScriptShift) {
    // the user has set the subscriptshift attribute
    float scaler = ((float) subScriptShift2) / subScriptShift1;
    subScriptShift1 = NS_MAX(subScriptShift1, aUserSubScriptShift);
    subScriptShift2 = NSToCoordRound(scaler * subScriptShift1);
  }

  // get a tentative value for subscriptshift
  // Rule 18d, App. G, TeXbook
  nscoord subScriptShift =
    NS_MAX(minSubScriptShift,NS_MAX(subScriptShift1,subScriptShift2));

  //////////////////////////////////////////////////
  // Get supscript shift
  // same code from nsMathMLmsupFrame.cpp
  //////////////////////////////////////////////////

  // get min supscript shift limit from x-height
  // = d(x) + 1/4 * sigma_5, Rule 18c, App. G, TeXbook
  nscoord minShiftFromXHeight = (nscoord)
    (bmSupScript.descent + (1.0f/4.0f) * xHeight);

  // supScriptShift{1,2,3}
  // = minimum amount to shift the supscript up
  // = sup{1,2,3} in TeX
  // supScriptShift1 = superscriptshift attribute * x-height
  // Note that there are THREE values for supscript shifts depending
  // on the current style
  nscoord supScriptShift1, supScriptShift2, supScriptShift3;
  // Set supScriptShift{1,2,3} default from font
  GetSupScriptShifts (fm, supScriptShift1, supScriptShift2, supScriptShift3);
  if (0 < aUserSupScriptShift) {
    // the user has set the superscriptshift attribute
    float scaler2 = ((float) supScriptShift2) / supScriptShift1;
    float scaler3 = ((float) supScriptShift3) / supScriptShift1;
    supScriptShift1 = NS_MAX(supScriptShift1, aUserSupScriptShift);
    supScriptShift2 = NSToCoordRound(scaler2 * supScriptShift1);
    supScriptShift3 = NSToCoordRound(scaler3 * supScriptShift1);
  }

  // get sup script shift depending on current script level and display style
  // Rule 18c, App. G, TeXbook
  nscoord supScriptShift;
  nsPresentationData presentationData;
  aFrame->GetPresentationData(presentationData);
  if ( aFrame->GetStyleFont()->mScriptLevel == 0 &&
       NS_MATHML_IS_DISPLAYSTYLE(presentationData.flags) &&
      !NS_MATHML_IS_COMPRESSED(presentationData.flags)) {
    // Style D in TeXbook
    supScriptShift = supScriptShift1;
  }
  else if (NS_MATHML_IS_COMPRESSED(presentationData.flags)) {
    // Style C' in TeXbook = D',T',S',SS'
    supScriptShift = supScriptShift3;
  }
  else {
    // everything else = T,S,SS
    supScriptShift = supScriptShift2;
  }

  // get tentative value for superscriptshift
  // Rule 18c, App. G, TeXbook
  supScriptShift =
    NS_MAX(minSupScriptShift,NS_MAX(supScriptShift,minShiftFromXHeight));

  //////////////////////////////////////////////////
  // Negotiate between supScriptShift and subScriptShift
  // so that there will be enough gap between them
  // Rule 18e, App. G, TeXbook
  //////////////////////////////////////////////////

  nscoord gap =
    (supScriptShift - bmSupScript.descent) -
    (bmSubScript.ascent - subScriptShift);
  if (gap < 4.0f * ruleSize) {
    // adjust subScriptShift to get a gap of (4.0 * ruleSize)
    subScriptShift += NSToCoordRound ((4.0f * ruleSize) - gap);
  }

  // next we want to ensure that the bottom of the superscript
  // will be > (4/5) * x-height above baseline
  gap = NSToCoordRound ((4.0f/5.0f) * xHeight -
                        (supScriptShift - bmSupScript.descent));
  if (gap > 0) {
    supScriptShift += gap;
    subScriptShift -= gap;
  }

  //////////////////////////////////////////////////
  // Do the Placing
  //////////////////////////////////////////////////

  // get bounding box for base + subscript + superscript
  nsBoundingMetrics boundingMetrics;
  boundingMetrics.ascent =
    NS_MAX(bmBase.ascent, (bmSupScript.ascent + supScriptShift));
  boundingMetrics.descent =
   NS_MAX(bmBase.descent, (bmSubScript.descent + subScriptShift));

  // leave aScriptSpace after both super/subscript
  // add italicCorrection between base and superscript
  // add "a little to spare" as well (see TeXbook Ch.11, p.64), as we
  // estimate the italic creation ourselves and it isn't the same as TeX 
  nscoord italicCorrection;
  GetItalicCorrection(bmBase, italicCorrection);
  italicCorrection += onePixel;
  boundingMetrics.width = bmBase.width + aScriptSpace +
    NS_MAX((italicCorrection + bmSupScript.width), bmSubScript.width);
  boundingMetrics.leftBearing = bmBase.leftBearing;
  boundingMetrics.rightBearing = bmBase.width +
    NS_MAX((italicCorrection + bmSupScript.rightBearing), bmSubScript.rightBearing);
  aFrame->SetBoundingMetrics(boundingMetrics);

  // reflow metrics
  aDesiredSize.ascent =
    NS_MAX(baseSize.ascent, 
       NS_MAX(subScriptSize.ascent - subScriptShift,
              supScriptSize.ascent + supScriptShift));
  aDesiredSize.height = aDesiredSize.ascent +
    NS_MAX(baseSize.height - baseSize.ascent,
       NS_MAX(subScriptSize.height - subScriptSize.ascent + subScriptShift, 
              supScriptSize.height - subScriptSize.ascent - supScriptShift));
  aDesiredSize.width = boundingMetrics.width;
  aDesiredSize.mBoundingMetrics = boundingMetrics;

  aFrame->SetReference(nsPoint(0, aDesiredSize.ascent));

  if (aPlaceOrigin) {
    nscoord dx, dy;
    // now place the base ...
    dx = aFrame->MirrorIfRTL(aDesiredSize.width, baseSize.width, 0);
    dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild(baseFrame, aPresContext, nullptr,
                      baseSize, dx, dy, 0);
    // ... and subscript
    dx = aFrame->MirrorIfRTL(aDesiredSize.width, subScriptSize.width,
                             bmBase.width);
    dy = aDesiredSize.ascent - (subScriptSize.ascent - subScriptShift);
    FinishReflowChild(subScriptFrame, aPresContext, nullptr,
                      subScriptSize, dx, dy, 0);
    // ... and the superscript
    dx = aFrame->MirrorIfRTL(aDesiredSize.width, supScriptSize.width,
                             bmBase.width + italicCorrection);
    dy = aDesiredSize.ascent - (supScriptSize.ascent + supScriptShift);
    FinishReflowChild(supScriptFrame, aPresContext, nullptr,
                      supScriptSize, dx, dy, 0);
  }

  return NS_OK;
}
