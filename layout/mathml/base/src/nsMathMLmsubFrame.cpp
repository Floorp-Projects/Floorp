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
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsMathMLmsubFrame.h"
#include "nslog.h"

NS_IMPL_LOG(nsMathMLmsubFrameLog)
#define PRINTF NS_LOG_PRINTF(nsMathMLmsubFrameLog)
#define FLUSH  NS_LOG_FLUSH(nsMathMLmsubFrameLog)

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
nsMathMLmsubFrame::Init(nsIPresContext*  aPresContext,
			nsIContent*      aContent,
			nsIFrame*        aParent,
			nsIStyleContext* aContext,
			nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init
    (aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mSubScriptShift = 0;
  mScriptSpace = NSFloatPointsToTwips(0.5f); // 0.5pt as in plain TeX

  // check if the subscriptshift attribute is there
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::subscriptshift_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      mSubScriptShift = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
//  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif
  return rv;
}

NS_IMETHODIMP
nsMathMLmsubFrame::Place(nsIPresContext*      aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         PRBool               aPlaceOrigin,
                         nsHTMLReflowMetrics& aDesiredSize)
{
  nsresult rv = NS_OK;

  ////////////////////////////////////
  // Get the children's desired sizes

  PRInt32 count = 0;
  nsHTMLReflowMetrics baseSize (nsnull);
  nsHTMLReflowMetrics subScriptSize (nsnull);
  nsIFrame* baseFrame = nsnull;
  nsIFrame* subScriptFrame = nsnull;
  // parameter v, Rule 18a, Appendix G of the TeXbook
  nscoord minSubScriptShift = 0; 

  nsBoundingMetrics bmBase, bmSubScript;

  nsIFrame* aChildFrame = mFrames.FirstChild();
  while (nsnull != aChildFrame)
  {
    if (!IsOnlyWhitespace(aChildFrame)) {
      if (0 == count) {
	// base 
	baseFrame = aChildFrame;
        GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
      }
      else if (1 == count) {
	// subscript
	subScriptFrame = aChildFrame;
        GetReflowAndBoundingMetricsFor(subScriptFrame, subScriptSize, bmSubScript);
	// get the subdrop from the subscript font
	nscoord aSubDrop;
	GetSubDropFromChild (aPresContext, subScriptFrame, aSubDrop);
	// parameter v, Rule 18a, App. G, TeXbook
	minSubScriptShift = bmBase.descent + aSubDrop;
      }
      count++;
    }
    rv = aChildFrame->GetNextSibling(&aChildFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }
#ifdef NS_DEBUG
  if (2 != count) PRINTF("msub: invalid markup");
#endif
  if ((2 != count) || !baseFrame || !subScriptFrame) {
    // report an error, encourage people to get their markups in order
    return ReflowError(aPresContext, aRenderingContext, aDesiredSize);
  }

  //////////////////
  // Place Children
  
  // get min subscript shift limit from x-height
  // = h(x) - 4/5 * sigma_5, Rule 18b, App. G, TeXbook
  nscoord xHeight = 0;
  nsCOMPtr<nsIFontMetrics> fm;

//  const nsStyleFont* aFont =
//    (const nsStyleFont*) mStyleContext->GetStyleData (eStyleStruct_Font);

  const nsStyleFont* aFont;
  baseFrame->GetStyleData(eStyleStruct_Font, (const nsStyleStruct *&)aFont);

  aPresContext->GetMetricsFor (aFont->mFont, getter_AddRefs(fm));
  fm->GetXHeight (xHeight);
  nscoord minShiftFromXHeight = (nscoord) 
    (bmSubScript.ascent - (4.0f/5.0f) * xHeight);

  // aSubScriptShift
  // = minimum amount to shift the subscript down set by user or from the font
  // = sub1 in TeX
  // = subscriptshift attribute * x-height
  nscoord aSubScriptShift, dummy;
  // get aSubScriptShift default from font
  GetSubScriptShifts (fm, aSubScriptShift, dummy);

  aSubScriptShift = 
    PR_MAX(aSubScriptShift, mSubScriptShift);

  // get actual subscriptshift to be used
  // Rule 18b, App. G, TeXbook
  nscoord actualSubScriptShift = 
    PR_MAX(minSubScriptShift,PR_MAX(aSubScriptShift,minShiftFromXHeight));
  // get bounding box for base + subscript
  mBoundingMetrics.ascent = 
    PR_MAX(bmBase.ascent, bmSubScript.ascent - actualSubScriptShift);
  mBoundingMetrics.descent = 
    PR_MAX(bmBase.descent, bmSubScript.descent + actualSubScriptShift);
  // add mScriptSpace between base and supscript
  mBoundingMetrics.width = bmBase.width + mScriptSpace + bmSubScript.width;
  mBoundingMetrics.leftBearing = bmBase.leftBearing;
  mBoundingMetrics.rightBearing = baseSize.width + mScriptSpace + bmSubScript.rightBearing;

  // reflow metrics
  aDesiredSize.ascent = 
    PR_MAX(baseSize.ascent, subScriptSize.ascent - actualSubScriptShift);
  aDesiredSize.descent = 
    PR_MAX(baseSize.descent, subScriptSize.descent + actualSubScriptShift);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = baseSize.width + mScriptSpace + subScriptSize.width;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aPlaceOrigin) {
    nscoord dx, dy;
    // now place the base ...
    dx = 0; dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, aPresContext, baseSize, dx, dy, 0);
    // ... and subscript
    dx = baseSize.width; 
    dy = aDesiredSize.ascent - (subScriptSize.ascent - actualSubScriptShift);
    FinishReflowChild (subScriptFrame, aPresContext, subScriptSize, dx, dy, 0);
  }

  return NS_OK;
}

