/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 *   Shyjan Mahamud <mahamud@cs.cmu.edu> (added TeX rendering rules)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
                            PRBool               aPlaceOrigin,
                            nsHTMLReflowMetrics& aDesiredSize)
{
  // extra spacing between base and sup/subscript
  nscoord scriptSpace = 0;

  // check if the subscriptshift attribute is there
  nsAutoString value;
  nscoord subScriptShift = 0;
  GetAttribute(mContent, mPresentationData.mstyle,
               nsGkAtoms::subscriptshift_, value);
  if (!value.IsEmpty()) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      subScriptShift = CalcLength(PresContext(), mStyleContext, cssValue);
    }
  }
  // check if the superscriptshift attribute is there
  nscoord supScriptShift = 0;
  GetAttribute(mContent, mPresentationData.mstyle,
               nsGkAtoms::superscriptshift_, value);
  if (!value.IsEmpty()) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      supScriptShift = CalcLength(PresContext(), mStyleContext, cssValue);
    }
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
                                        PRBool               aPlaceOrigin,
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
  nsIFrame* subScriptFrame = nsnull;
  nsIFrame* supScriptFrame = nsnull;
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
    dx = 0; dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild(baseFrame, aPresContext, nsnull,
                      baseSize, dx, dy, 0);
    // ... and subscript
    dx = bmBase.width;
    dy = aDesiredSize.ascent - (subScriptSize.ascent - subScriptShift);
    FinishReflowChild(subScriptFrame, aPresContext, nsnull,
                      subScriptSize, dx, dy, 0);
    // ... and the superscript
    dx = bmBase.width + italicCorrection;
    dy = aDesiredSize.ascent - (supScriptSize.ascent + supScriptShift);
    FinishReflowChild(supScriptFrame, aPresContext, nsnull,
                      supScriptSize, dx, dy, 0);
  }

  return NS_OK;
}
