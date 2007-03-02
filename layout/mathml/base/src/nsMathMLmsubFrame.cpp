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

#include "nsMathMLmsubFrame.h"

//
// <msub> -- attach a subscript to a base - implementation
//

nsIFrame*
NS_NewMathMLmsubFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmsubFrame(aContext);
}

nsMathMLmsubFrame::~nsMathMLmsubFrame()
{
}

NS_IMETHODIMP
nsMathMLmsubFrame::TransmitAutomaticData()
{
  // if our base is an embellished operator, let its state bubble to us
  mPresentationData.baseFrame = mFrames.FirstChild();
  GetEmbellishDataFrom(mPresentationData.baseFrame, mEmbellishData);

  // 1. The REC says:
  // The <msub> element increments scriptlevel by 1, and sets displaystyle to
  // "false", within subscript, but leaves both attributes unchanged within base.
  // 2. The TeXbook (Ch 17. p.141) says the subscript is compressed
  UpdatePresentationDataFromChildAt(1, -1, 1,
    ~NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED,
     NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED);

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmsubFrame::Place (nsIRenderingContext& aRenderingContext,
                          PRBool               aPlaceOrigin,
                          nsHTMLReflowMetrics& aDesiredSize)
{
  // extra spacing after sup/subscript
  nscoord scriptSpace = GetPresContext()->PointsToAppUnits(0.5f); // 0.5pt as in plain TeX

  // check if the subscriptshift attribute is there
  nscoord subScriptShift = 0;
  nsAutoString value;
  GetAttribute(mContent, mPresentationData.mstyle,
               nsGkAtoms::subscriptshift_, value);
  if (!value.IsEmpty()) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      subScriptShift = CalcLength(GetPresContext(), mStyleContext, cssValue);
    }
  }

  return nsMathMLmsubFrame::PlaceSubScript(GetPresContext(), 
                                           aRenderingContext,
                                           aPlaceOrigin,
                                           aDesiredSize,
                                           this,
                                           subScriptShift,
                                           scriptSpace);
}

// exported routine that both munder and msub share.
// munder uses this when movablelimits is set.
nsresult
nsMathMLmsubFrame::PlaceSubScript (nsPresContext*      aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   PRBool               aPlaceOrigin,
                                   nsHTMLReflowMetrics& aDesiredSize,
                                   nsMathMLContainerFrame* aFrame,
                                   nscoord              aUserSubScriptShift,
                                   nscoord              aScriptSpace)
{
  // force the scriptSpace to be atleast 1 pixel 
  aScriptSpace = PR_MAX(nsPresContext::CSSPixelsToAppUnits(1), aScriptSpace);

  ////////////////////////////////////
  // Get the children's desired sizes

  nsBoundingMetrics bmBase, bmSubScript;
  nsHTMLReflowMetrics baseSize;
  nsHTMLReflowMetrics subScriptSize;
  nsIFrame* baseFrame = aFrame->GetFirstChild(nsnull);
  nsIFrame* subScriptFrame = nsnull;
  if (baseFrame)
    subScriptFrame = baseFrame->GetNextSibling();
  if (!baseFrame || !subScriptFrame || subScriptFrame->GetNextSibling()) {
    // report an error, encourage people to get their markups in order
    NS_WARNING("invalid markup");
    return NS_STATIC_CAST(nsMathMLContainerFrame*, 
                          aFrame)->ReflowError(aRenderingContext, 
                                               aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  GetReflowAndBoundingMetricsFor(subScriptFrame, subScriptSize, bmSubScript);

  // get the subdrop from the subscript font
  nscoord subDrop;
  GetSubDropFromChild(subScriptFrame, subDrop);
  // parameter v, Rule 18a, App. G, TeXbook
  nscoord minSubScriptShift = bmBase.descent + subDrop;

  //////////////////
  // Place Children
  
  // get min subscript shift limit from x-height
  // = h(x) - 4/5 * sigma_5, Rule 18b, App. G, TeXbook
  nscoord xHeight = 0;
  nsCOMPtr<nsIFontMetrics> fm =
    aPresContext->GetMetricsFor(baseFrame->GetStyleFont()->mFont);

  fm->GetXHeight (xHeight);
  nscoord minShiftFromXHeight = (nscoord) 
    (bmSubScript.ascent - (4.0f/5.0f) * xHeight);

  // subScriptShift
  // = minimum amount to shift the subscript down set by user or from the font
  // = sub1 in TeX
  // = subscriptshift attribute * x-height
  nscoord subScriptShift, dummy;
  // get subScriptShift default from font
  GetSubScriptShifts (fm, subScriptShift, dummy);

  subScriptShift = 
    PR_MAX(subScriptShift, aUserSubScriptShift);

  // get actual subscriptshift to be used
  // Rule 18b, App. G, TeXbook
  nscoord actualSubScriptShift = 
    PR_MAX(minSubScriptShift,PR_MAX(subScriptShift,minShiftFromXHeight));
  // get bounding box for base + subscript
  nsBoundingMetrics boundingMetrics;
  boundingMetrics.ascent = 
    PR_MAX(bmBase.ascent, bmSubScript.ascent - actualSubScriptShift);
  boundingMetrics.descent = 
    PR_MAX(bmBase.descent, bmSubScript.descent + actualSubScriptShift);

  // add aScriptSpace to the subscript's width
  boundingMetrics.width = bmBase.width + bmSubScript.width + aScriptSpace;
  boundingMetrics.leftBearing = bmBase.leftBearing;
  boundingMetrics.rightBearing = PR_MAX(bmBase.rightBearing, bmBase.width +
    PR_MAX(bmSubScript.width + aScriptSpace, bmSubScript.rightBearing));
  aFrame->SetBoundingMetrics (boundingMetrics);

  // reflow metrics
  aDesiredSize.ascent = 
    PR_MAX(baseSize.ascent, subScriptSize.ascent - actualSubScriptShift);
  aDesiredSize.height = aDesiredSize.ascent +
    PR_MAX(baseSize.height - baseSize.ascent,
           subScriptSize.height - subScriptSize.ascent + actualSubScriptShift);
  aDesiredSize.width = boundingMetrics.width;
  aDesiredSize.mBoundingMetrics = boundingMetrics;

  aFrame->SetReference(nsPoint(0, aDesiredSize.ascent));

  if (aPlaceOrigin) {
    nscoord dx, dy;
    // now place the base ...
    dx = 0; dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, aPresContext, nsnull, baseSize, dx, dy, 0);
    // ... and subscript
    dx = bmBase.width; 
    dy = aDesiredSize.ascent - (subScriptSize.ascent - actualSubScriptShift);
    FinishReflowChild (subScriptFrame, aPresContext, nsnull, subScriptSize, dx, dy, 0);
  }

  return NS_OK;
}
