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
#include "nsIPresContext.h"
#include "nsUnitConversion.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsMathMLmsupFrame.h"

//
// <msup> -- attach a superscript to a base - implementation
//

nsresult
NS_NewMathMLmsupFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmsupFrame* it = new (aPresShell) nsMathMLmsupFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmsupFrame::nsMathMLmsupFrame()
{
}

nsMathMLmsupFrame::~nsMathMLmsupFrame()
{
}

NS_IMETHODIMP
nsMathMLmsupFrame::TransmitAutomaticData(nsIPresContext* aPresContext)
{
  // if our base is an embellished operator, its flags bubble to us
  nsIFrame* baseFrame = mFrames.FirstChild();
  GetEmbellishDataFrom(baseFrame, mEmbellishData);
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags))
    mEmbellishData.nextFrame = baseFrame;

  // 1. The REC says:
  // The <msup> element increments scriptlevel by 1, and sets displaystyle to
  // "false", within superscript, but leaves both attributes unchanged within base.
  // 2. The TeXbook (Ch 17. p.141) says the superscript *inherits* the compression,
  // so we don't set the compression flag. Our parent will propagate its own.
  UpdatePresentationDataFromChildAt(aPresContext, 1, -1, 1,
    ~NS_MATHML_DISPLAYSTYLE,
     NS_MATHML_DISPLAYSTYLE);

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmsupFrame::Place(nsIPresContext*      aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         PRBool               aPlaceOrigin,
                         nsHTMLReflowMetrics& aDesiredSize)
{
  // extra spacing between base and sup/subscript
  nscoord scriptSpace = NSFloatPointsToTwips(0.5f); // 0.5pt as in plain TeX

  // check if the superscriptshift attribute is there
  nsAutoString value;
  nscoord supScriptShift = 0;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::superscriptshift_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      supScriptShift = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }

  return nsMathMLmsupFrame::PlaceSuperScript(aPresContext, 
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
nsMathMLmsupFrame::PlaceSuperScript(nsIPresContext*      aPresContext,
                                    nsIRenderingContext& aRenderingContext,
                                    PRBool               aPlaceOrigin,
                                    nsHTMLReflowMetrics& aDesiredSize,
                                    nsIFrame*            aFrame,
                                    nscoord              aUserSupScriptShift,
                                    nscoord              aScriptSpace)
{
  // the caller better be a mathml frame
  nsIMathMLFrame* mathMLFrame;
  aFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
  if (!mathMLFrame) return NS_ERROR_INVALID_ARG;

  // force the scriptSpace to be at least 1 pixel 
  nscoord onePixel = aPresContext->IntScaledPixelsToTwips(1);
  aScriptSpace = PR_MAX(onePixel, aScriptSpace);

  ////////////////////////////////////
  // Get the children's desired sizes

  nsHTMLReflowMetrics baseSize (nsnull);
  nsHTMLReflowMetrics supScriptSize (nsnull);
  nsBoundingMetrics bmBase, bmSupScript;
  nsIFrame* supScriptFrame = nsnull;
  nsIFrame* baseFrame = aFrame->GetFirstChild(nsnull);
  if (baseFrame)
    supScriptFrame = baseFrame->GetNextSibling();
  if (!baseFrame || !supScriptFrame || supScriptFrame->GetNextSibling()) {
    // report an error, encourage people to get their markups in order
    NS_WARNING("invalid markup");
    return NS_STATIC_CAST(nsMathMLContainerFrame*, 
                          aFrame)->ReflowError(aPresContext, 
                                               aRenderingContext, 
                                               aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  GetReflowAndBoundingMetricsFor(supScriptFrame, supScriptSize, bmSupScript);

  // get the supdrop from the supscript font
  nscoord supDrop;
  GetSupDropFromChild(aPresContext, supScriptFrame, supDrop);
  // parameter u, Rule 18a, App. G, TeXbook
  nscoord minSupScriptShift = bmBase.ascent - supDrop;

  //////////////////
  // Place Children 
  
  // get min supscript shift limit from x-height
  // = d(x) + 1/4 * sigma_5, Rule 18c, App. G, TeXbook
  nscoord xHeight = 0;
  nsCOMPtr<nsIFontMetrics> fm =
    aPresContext->GetMetricsFor(baseFrame->GetStyleFont()->mFont);

  fm->GetXHeight (xHeight);
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
      PR_MAX(supScriptShift1, aUserSupScriptShift);
    supScriptShift2 = NSToCoordRound(scaler2 * supScriptShift1);
    supScriptShift3 = NSToCoordRound(scaler3 * supScriptShift1);
  }

  // get sup script shift depending on current script level and display style
  // Rule 18c, App. G, TeXbook
  nscoord supScriptShift;
  nsPresentationData presentationData;
  mathMLFrame->GetPresentationData (presentationData);
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

  // get actual supscriptshift to be used
  // Rule 18c, App. G, TeXbook
  nscoord actualSupScriptShift = 
    PR_MAX(minSupScriptShift,PR_MAX(supScriptShift,minShiftFromXHeight));

  // bounding box
  nsBoundingMetrics boundingMetrics;
  boundingMetrics.ascent =
    PR_MAX(bmBase.ascent, (bmSupScript.ascent + actualSupScriptShift));
  boundingMetrics.descent =
    PR_MAX(bmBase.descent, (bmSupScript.descent - actualSupScriptShift));
  // add scriptSpace between base and supscript
  boundingMetrics.width = bmBase.width + aScriptSpace 
                        + italicCorrection + bmSupScript.width;
  boundingMetrics.leftBearing = bmBase.leftBearing;
  boundingMetrics.rightBearing = bmBase.width + aScriptSpace
                               + italicCorrection + bmSupScript.rightBearing;
  mathMLFrame->SetBoundingMetrics(boundingMetrics);

  // reflow metrics
  aDesiredSize.ascent =
    PR_MAX(baseSize.ascent, (supScriptSize.ascent + actualSupScriptShift));
  aDesiredSize.descent =
    PR_MAX(baseSize.descent, (supScriptSize.descent - actualSupScriptShift));
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = bmBase.width + aScriptSpace
                     + italicCorrection + supScriptSize.width;
  aDesiredSize.mBoundingMetrics = boundingMetrics;

  mathMLFrame->SetReference(nsPoint(0, aDesiredSize.ascent));

  if (aPlaceOrigin) {
    nscoord dx, dy;
    // now place the base ...
    dx = 0; dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, aPresContext, nsnull, baseSize, dx, dy, 0);
    // ... and supscript
    dx = bmBase.width + aScriptSpace + italicCorrection;
    dy = aDesiredSize.ascent - (supScriptSize.ascent + actualSupScriptShift);
    FinishReflowChild (supScriptFrame, aPresContext, nsnull, supScriptSize, dx, dy, 0);
  }

  return NS_OK;
}
