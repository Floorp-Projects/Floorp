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
#include "nsUnitConversion.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsMathMLmsubsupFrame.h"

//
// <msubsup> -- attach a subscript-superscript pair to a base - implementation
//

nsresult
NS_NewMathMLmsubsupFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmsubsupFrame* it = new (aPresShell) nsMathMLmsubsupFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmsubsupFrame::nsMathMLmsubsupFrame()
{
}

nsMathMLmsubsupFrame::~nsMathMLmsubsupFrame()
{
}

NS_IMETHODIMP
nsMathMLmsubsupFrame::TransmitAutomaticData(nsPresContext* aPresContext)
{
  // if our base is an embellished operator, let its state bubble to us
  nsIFrame* baseFrame = mFrames.FirstChild();
  GetEmbellishDataFrom(baseFrame, mEmbellishData);
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags))
    mEmbellishData.nextFrame = baseFrame;

  // 1. The REC says:
  //    The <msubsup> element increments scriptlevel by 1, and sets displaystyle to
  //    "false", within subscript and superscript, but leaves both attributes
  //    unchanged within base.
  // 2. The TeXbook (Ch 17. p.141) says the superscript inherits the compression
  //    while the subscript is compressed
  UpdatePresentationDataFromChildAt(aPresContext, 1, -1, 1,
    ~NS_MATHML_DISPLAYSTYLE,
     NS_MATHML_DISPLAYSTYLE);
  UpdatePresentationDataFromChildAt(aPresContext, 1,  1, 0,
     NS_MATHML_COMPRESSED,
     NS_MATHML_COMPRESSED);

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmsubsupFrame::Place(nsPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            PRBool               aPlaceOrigin,
                            nsHTMLReflowMetrics& aDesiredSize)
{
  // extra spacing between base and sup/subscript
  nscoord scriptSpace = 0;

  // check if the subscriptshift attribute is there
  nsAutoString value;
  nscoord subScriptShift = 0;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::subscriptshift_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      subScriptShift = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }
  // check if the superscriptshift attribute is there
  nscoord supScriptShift = 0;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::superscriptshift_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      supScriptShift = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }

  return nsMathMLmsubsupFrame::PlaceSubSupScript(aPresContext,
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
                                        nsIRenderingContext& aRenderingContext,
                                        PRBool               aPlaceOrigin,
                                        nsHTMLReflowMetrics& aDesiredSize,
                                        nsIFrame*            aFrame,
                                        nscoord              aUserSubScriptShift,
                                        nscoord              aUserSupScriptShift,
                                        nscoord              aScriptSpace)
{
  // the caller better be a mathml frame
  nsIMathMLFrame* mathMLFrame;
  aFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
  if (!mathMLFrame) return NS_ERROR_INVALID_ARG;

  // force the scriptSpace to be atleast 1 pixel
  nscoord onePixel = aPresContext->IntScaledPixelsToTwips(1);
  aScriptSpace = PR_MAX(onePixel, aScriptSpace);

  ////////////////////////////////////
  // Get the children's desired sizes

  nsHTMLReflowMetrics baseSize (nsnull);
  nsHTMLReflowMetrics subScriptSize (nsnull);
  nsHTMLReflowMetrics supScriptSize (nsnull);
  nsBoundingMetrics bmBase, bmSubScript, bmSupScript;
  nsIFrame* subScriptFrame = nsnull;
  nsIFrame* supScriptFrame = nsnull;
  nsIFrame* baseFrame = aFrame->GetFirstChild(nsnull);
  if (baseFrame)
    subScriptFrame = baseFrame->GetNextSibling();
  if (subScriptFrame)
    supScriptFrame = subScriptFrame->GetNextSibling();
  if (!baseFrame || !subScriptFrame || !supScriptFrame ||
      supScriptFrame->GetNextSibling()) {
    // report an error, encourage people to get their markups in order
    NS_WARNING("invalid markup");
    return NS_STATIC_CAST(nsMathMLContainerFrame*,
                          aFrame)->ReflowError(aPresContext,
                                               aRenderingContext,
                                               aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  GetReflowAndBoundingMetricsFor(subScriptFrame, subScriptSize, bmSubScript);
  GetReflowAndBoundingMetricsFor(supScriptFrame, supScriptSize, bmSupScript);

  // get the subdrop from the subscript font
  nscoord subDrop;
  GetSubDropFromChild(aPresContext, subScriptFrame, subDrop);
  // parameter v, Rule 18a, App. G, TeXbook
  nscoord minSubScriptShift = bmBase.descent + subDrop;

  // get the supdrop from the supscript font
  nscoord supDrop;
  GetSupDropFromChild(aPresContext, supScriptFrame, supDrop);
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

  aRenderingContext.SetFont(baseFrame->GetStyleFont()->mFont, nsnull);
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));

  // get x-height (an ex)
  nscoord xHeight;
  fm->GetXHeight (xHeight);

  nscoord ruleSize;
  GetRuleThickness (aRenderingContext, fm, ruleSize);

  // Get subScriptShift{1,2} default from font
  GetSubScriptShifts (fm, subScriptShift1, subScriptShift2);

  if (0 < aUserSubScriptShift) {
    // the user has set the subscriptshift attribute
    float scaler = ((float) subScriptShift2) / subScriptShift1;
    subScriptShift1 = PR_MAX(subScriptShift1, aUserSubScriptShift);
    subScriptShift2 = NSToCoordRound(scaler * subScriptShift1);
  }

  // get a tentative value for subscriptshift
  // Rule 18d, App. G, TeXbook
  nscoord subScriptShift =
    PR_MAX(minSubScriptShift,PR_MAX(subScriptShift1,subScriptShift2));

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
    supScriptShift1 = PR_MAX(supScriptShift1, aUserSupScriptShift);
    supScriptShift2 = NSToCoordRound(scaler2 * supScriptShift1);
    supScriptShift3 = NSToCoordRound(scaler3 * supScriptShift1);
  }

  // get sup script shift depending on current script level and display style
  // Rule 18c, App. G, TeXbook
  nscoord supScriptShift;
  nsPresentationData presentationData;
  mathMLFrame->GetPresentationData(presentationData);
  if ( presentationData.scriptLevel == 0 &&
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
    PR_MAX(minSupScriptShift,PR_MAX(supScriptShift,minShiftFromXHeight));

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
    PR_MAX(bmBase.ascent, (bmSupScript.ascent + supScriptShift));
  boundingMetrics.descent =
   PR_MAX(bmBase.descent, (bmSubScript.descent + subScriptShift));

  // add aScriptSpace to both super/subscript
  // add italicCorrection only to superscript
  nscoord italicCorrection;
  GetItalicCorrection(bmBase, italicCorrection);
  boundingMetrics.width = bmBase.width + aScriptSpace +
    PR_MAX((italicCorrection + bmSupScript.width), bmSubScript.width);
  boundingMetrics.leftBearing = bmBase.leftBearing;
  boundingMetrics.rightBearing = bmBase.width + aScriptSpace +
    PR_MAX((italicCorrection + bmSupScript.rightBearing), bmSubScript.rightBearing);
  mathMLFrame->SetBoundingMetrics(boundingMetrics);

  // reflow metrics
  aDesiredSize.ascent =
    PR_MAX(baseSize.ascent, 
       PR_MAX(subScriptSize.ascent - subScriptShift,
              supScriptSize.ascent + supScriptShift));
  aDesiredSize.descent =
    PR_MAX(baseSize.descent,
       PR_MAX(subScriptSize.descent + subScriptShift, 
              supScriptSize.descent - supScriptShift));
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = bmBase.width + aScriptSpace +
    PR_MAX((italicCorrection + supScriptSize.width), subScriptSize.width);
  aDesiredSize.mBoundingMetrics = boundingMetrics;

  mathMLFrame->SetReference(nsPoint(0, aDesiredSize.ascent));

  if (aPlaceOrigin) {
    nscoord dx, dy;
    // now place the base ...
    dx = 0; dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild(baseFrame, aPresContext, nsnull, baseSize, dx, dy, 0);
    // ... and subscript
    dx = bmBase.width + aScriptSpace;
    dy = aDesiredSize.ascent - (subScriptSize.ascent - subScriptShift);
    FinishReflowChild(subScriptFrame, aPresContext, nsnull, subScriptSize, dx, dy, 0);
    // ... and the superscript
    dx = bmBase.width + aScriptSpace + italicCorrection;
    dy = aDesiredSize.ascent - (supScriptSize.ascent + supScriptShift);
    FinishReflowChild(supScriptFrame, aPresContext, nsnull, supScriptSize, dx, dy, 0);
  }

  return NS_OK;
}
