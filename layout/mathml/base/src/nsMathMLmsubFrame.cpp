/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is The University Of
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 *   Shyjan Mahamud <mahamud@cs.cmu.edu> (added TeX rendering rules)
 */


#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsIPresContext.h"
#include "nsUnitConversion.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsMathMLmsubFrame.h"

//
// <msub> -- attach a subscript to a base - implementation
//

nsresult
NS_NewMathMLmsubFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmsubFrame* it = new (aPresShell) nsMathMLmsubFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmsubFrame::nsMathMLmsubFrame()
{
}

nsMathMLmsubFrame::~nsMathMLmsubFrame()
{
}

NS_IMETHODIMP
nsMathMLmsubFrame::TransmitAutomaticData(nsIPresContext* aPresContext)
{
  // if our base is an embellished operator, let its state bubble to us
  nsIFrame* baseFrame = mFrames.FirstChild();
  GetEmbellishDataFrom(baseFrame, mEmbellishData);
  if (NS_MATHML_IS_EMBELLISH_OPERATOR(mEmbellishData.flags))
    mEmbellishData.nextFrame = baseFrame;

  // 1. The REC says:
  // The <msub> element increments scriptlevel by 1, and sets displaystyle to
  // "false", within subscript, but leaves both attributes unchanged within base.
  // 2. The TeXbook (Ch 17. p.141) says the subscript is compressed
  UpdatePresentationDataFromChildAt(aPresContext, 1, -1, 1,
    ~NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED,
     NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED);

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmsubFrame::Place (nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          PRBool               aPlaceOrigin,
                          nsHTMLReflowMetrics& aDesiredSize)
{
  // extra spacing between base and sup/subscript
  nscoord scriptSpace = NSFloatPointsToTwips(0.5f); // 0.5pt as in plain TeX

  // check if the subscriptshift attribute is there
  nscoord subScriptShift = 0;
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::subscriptshift_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      subScriptShift = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }

  return nsMathMLmsubFrame::PlaceSubScript(aPresContext, 
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
nsMathMLmsubFrame::PlaceSubScript (nsIPresContext*      aPresContext,
                                   nsIRenderingContext& aRenderingContext,
                                   PRBool               aPlaceOrigin,
                                   nsHTMLReflowMetrics& aDesiredSize,
                                   nsIFrame*            aFrame,
                                   nscoord              aUserSubScriptShift,
                                   nscoord              aScriptSpace)
{
  // the caller better be a mathml frame
  nsIMathMLFrame* mathMLFrame;
  aFrame->QueryInterface(NS_GET_IID(nsIMathMLFrame), (void**)&mathMLFrame);
  if (!mathMLFrame) return NS_ERROR_INVALID_ARG;

  // force the scriptSpace to be atleast 1 pixel 
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  aScriptSpace = PR_MAX(NSIntPixelsToTwips(1, p2t), aScriptSpace);

  ////////////////////////////////////
  // Get the children's desired sizes

  nsBoundingMetrics bmBase, bmSubScript;
  nsHTMLReflowMetrics baseSize(nsnull);
  nsHTMLReflowMetrics subScriptSize(nsnull);
  nsIFrame* baseFrame = aFrame->GetFirstChild(nsnull);
  nsIFrame* subScriptFrame = nsnull;
  if (baseFrame)
    subScriptFrame = baseFrame->GetNextSibling();
  if (!baseFrame || !subScriptFrame || subScriptFrame->GetNextSibling()) {
    // report an error, encourage people to get their markups in order
    NS_WARNING("invalid markup");
    return NS_STATIC_CAST(nsMathMLContainerFrame*, 
                          aFrame)->ReflowError(aPresContext, 
                                               aRenderingContext, 
                                               aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  GetReflowAndBoundingMetricsFor(subScriptFrame, subScriptSize, bmSubScript);

  // get the subdrop from the subscript font
  nscoord subDrop;
  GetSubDropFromChild(aPresContext, subScriptFrame, subDrop);
  // parameter v, Rule 18a, App. G, TeXbook
  nscoord minSubScriptShift = bmBase.descent + subDrop;

  //////////////////
  // Place Children
  
  // get min subscript shift limit from x-height
  // = h(x) - 4/5 * sigma_5, Rule 18b, App. G, TeXbook
  nscoord xHeight = 0;
  nsCOMPtr<nsIFontMetrics> fm;

  aPresContext->GetMetricsFor (baseFrame->GetStyleFont()->mFont,
                               getter_AddRefs(fm));
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
  // add aScriptSpace between base and supscript
  boundingMetrics.width = 
    bmBase.width 
    + aScriptSpace 
    + bmSubScript.width;
  boundingMetrics.leftBearing = bmBase.leftBearing;
  boundingMetrics.rightBearing = 
    bmBase.width 
    + aScriptSpace 
    + bmSubScript.rightBearing;
  mathMLFrame->SetBoundingMetrics (boundingMetrics);

  // reflow metrics
  aDesiredSize.ascent = 
    PR_MAX(baseSize.ascent, subScriptSize.ascent - actualSubScriptShift);
  aDesiredSize.descent = 
    PR_MAX(baseSize.descent, subScriptSize.descent + actualSubScriptShift);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = bmBase.width + aScriptSpace + subScriptSize.width;
  aDesiredSize.mBoundingMetrics = boundingMetrics;

  mathMLFrame->SetReference(nsPoint(0, aDesiredSize.ascent));

  if (aPlaceOrigin) {
    nscoord dx, dy;
    // now place the base ...
    dx = 0; dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, aPresContext, nsnull, baseSize, dx, dy, 0);
    // ... and subscript
    // XXX adding mScriptSpace seems to add more space than i like
    // may want to remove later.
    dx = bmBase.width + aScriptSpace; 
    dy = aDesiredSize.ascent - (subScriptSize.ascent - actualSubScriptShift);
    FinishReflowChild (subScriptFrame, aPresContext, nsnull, subScriptSize, dx, dy, 0);
  }

  return NS_OK;
}
