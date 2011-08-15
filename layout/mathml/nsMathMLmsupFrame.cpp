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

#include "nsMathMLmsupFrame.h"

//
// <msup> -- attach a superscript to a base - implementation
//

nsIFrame*
NS_NewMathMLmsupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmsupFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmsupFrame)

nsMathMLmsupFrame::~nsMathMLmsupFrame()
{
}

NS_IMETHODIMP
nsMathMLmsupFrame::TransmitAutomaticData()
{
  // if our base is an embellished operator, its flags bubble to us
  mPresentationData.baseFrame = mFrames.FirstChild();
  GetEmbellishDataFrom(mPresentationData.baseFrame, mEmbellishData);

  // 1. The REC says:
  // The <msup> element increments scriptlevel by 1, and sets displaystyle to
  // "false", within superscript, but leaves both attributes unchanged within base.
  // 2. The TeXbook (Ch 17. p.141) says the superscript *inherits* the compression,
  // so we don't set the compression flag. Our parent will propagate its own.
  UpdatePresentationDataFromChildAt(1, -1,
    ~NS_MATHML_DISPLAYSTYLE,
     NS_MATHML_DISPLAYSTYLE);

  return NS_OK;
}

/* virtual */ nsresult
nsMathMLmsupFrame::Place(nsRenderingContext& aRenderingContext,
                         PRBool               aPlaceOrigin,
                         nsHTMLReflowMetrics& aDesiredSize)
{
  // extra spacing after sup/subscript
  nscoord scriptSpace = nsPresContext::CSSPointsToAppUnits(0.5f); // 0.5pt as in plain TeX

  // check if the superscriptshift attribute is there
  nsAutoString value;
  nscoord supScriptShift = 0;
  GetAttribute(mContent, mPresentationData.mstyle,
               nsGkAtoms::superscriptshift_, value);
  if (!value.IsEmpty()) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      supScriptShift = CalcLength(PresContext(), mStyleContext, cssValue);
    }
  }

  return nsMathMLmsupFrame::PlaceSuperScript(PresContext(), 
                                             aRenderingContext,
                                             aPlaceOrigin,
                                             aDesiredSize,
                                             this,
                                             supScriptShift,
                                             scriptSpace);
}

// exported routine that both mover and msup share.
// mover uses this when movablelimits is set.
nsresult
nsMathMLmsupFrame::PlaceSuperScript(nsPresContext*      aPresContext,
                                    nsRenderingContext& aRenderingContext,
                                    PRBool               aPlaceOrigin,
                                    nsHTMLReflowMetrics& aDesiredSize,
                                    nsMathMLContainerFrame* aFrame,
                                    nscoord              aUserSupScriptShift,
                                    nscoord              aScriptSpace)
{
  // force the scriptSpace to be at least 1 pixel 
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);
  aScriptSpace = NS_MAX(onePixel, aScriptSpace);

  ////////////////////////////////////
  // Get the children's desired sizes

  nsHTMLReflowMetrics baseSize;
  nsHTMLReflowMetrics supScriptSize;
  nsBoundingMetrics bmBase, bmSupScript;
  nsIFrame* supScriptFrame = nsnull;
  nsIFrame* baseFrame = aFrame->GetFirstChild(nsnull);
  if (baseFrame)
    supScriptFrame = baseFrame->GetNextSibling();
  if (!baseFrame || !supScriptFrame || supScriptFrame->GetNextSibling()) {
    // report an error, encourage people to get their markups in order
    return aFrame->ReflowError(aRenderingContext, aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  GetReflowAndBoundingMetricsFor(supScriptFrame, supScriptSize, bmSupScript);

  // get the supdrop from the supscript font
  nscoord supDrop;
  GetSupDropFromChild(supScriptFrame, supDrop);
  // parameter u, Rule 18a, App. G, TeXbook
  nscoord minSupScriptShift = bmBase.ascent - supDrop;

  //////////////////
  // Place Children 
  
  // get min supscript shift limit from x-height
  // = d(x) + 1/4 * sigma_5, Rule 18c, App. G, TeXbook
  nscoord xHeight = 0;
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(baseFrame, getter_AddRefs(fm));

  xHeight = fm->XHeight();
  nscoord minShiftFromXHeight = (nscoord) 
    (bmSupScript.descent + (1.0f/4.0f) * xHeight);
  nscoord italicCorrection;
  GetItalicCorrection(bmBase, italicCorrection);

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
    supScriptShift1 = 
      NS_MAX(supScriptShift1, aUserSupScriptShift);
    supScriptShift2 = NSToCoordRound(scaler2 * supScriptShift1);
    supScriptShift3 = NSToCoordRound(scaler3 * supScriptShift1);
  }

  // get sup script shift depending on current script level and display style
  // Rule 18c, App. G, TeXbook
  nscoord supScriptShift;
  nsPresentationData presentationData;
  aFrame->GetPresentationData (presentationData);
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

  // get actual supscriptshift to be used
  // Rule 18c, App. G, TeXbook
  nscoord actualSupScriptShift = 
    NS_MAX(minSupScriptShift,NS_MAX(supScriptShift,minShiftFromXHeight));

  // bounding box
  nsBoundingMetrics boundingMetrics;
  boundingMetrics.ascent =
    NS_MAX(bmBase.ascent, (bmSupScript.ascent + actualSupScriptShift));
  boundingMetrics.descent =
    NS_MAX(bmBase.descent, (bmSupScript.descent - actualSupScriptShift));

  // leave aScriptSpace after superscript
  // add italicCorrection between base and superscript
  // add "a little to spare" as well (see TeXbook Ch.11, p.64), as we
  // estimate the italic creation ourselves and it isn't the same as TeX 
  italicCorrection += onePixel;
  boundingMetrics.width = bmBase.width + italicCorrection +
                          bmSupScript.width + aScriptSpace;
  boundingMetrics.leftBearing = bmBase.leftBearing;
  boundingMetrics.rightBearing = bmBase.width + italicCorrection +
                                 bmSupScript.rightBearing;
  aFrame->SetBoundingMetrics(boundingMetrics);

  // reflow metrics
  aDesiredSize.ascent =
    NS_MAX(baseSize.ascent, (supScriptSize.ascent + actualSupScriptShift));
  aDesiredSize.height = aDesiredSize.ascent +
    NS_MAX(baseSize.height - baseSize.ascent,
           (supScriptSize.height - supScriptSize.ascent - actualSupScriptShift));
  aDesiredSize.width = boundingMetrics.width;
  aDesiredSize.mBoundingMetrics = boundingMetrics;

  aFrame->SetReference(nsPoint(0, aDesiredSize.ascent));

  if (aPlaceOrigin) {
    nscoord dx, dy;
    // now place the base ...
    dx = 0; dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, aPresContext, nsnull, baseSize, dx, dy, 0);
    // ... and supscript
    dx = bmBase.width + italicCorrection;
    dy = aDesiredSize.ascent - (supScriptSize.ascent + actualSupScriptShift);
    FinishReflowChild (supScriptFrame, aPresContext, nsnull, supScriptSize, dx, dy, 0);
  }

  return NS_OK;
}
