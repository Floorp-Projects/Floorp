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

#include "nsMathMLmsupFrame.h"
#include "nslog.h"

NS_IMPL_LOG(nsMathMLmsupFrameLog)
#define PRINTF NS_LOG_PRINTF(nsMathMLmsupFrameLog)
#define FLUSH  NS_LOG_FLUSH(nsMathMLmsupFrameLog)

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
nsMathMLmsupFrame::Init(nsIPresContext*  aPresContext,
			nsIContent*      aContent,
			nsIFrame*        aParent,
			nsIStyleContext* aContext,
			nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init
    (aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mSupScriptShift = 0;
  mScriptSpace = NSFloatPointsToTwips(0.5f); // 0.5pt as in plain TeX

  // check if the superscriptshift attribute is there
  nsAutoString value;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::superscriptshift_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      mSupScriptShift = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
//  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif
  return rv;
}

NS_IMETHODIMP
nsMathMLmsupFrame::Place(nsIPresContext*      aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         PRBool               aPlaceOrigin,
                         nsHTMLReflowMetrics& aDesiredSize)
{
  nsresult rv = NS_OK;

  ////////////////////////////////////
  // Get the children's desired sizes

  PRInt32 count = 0;
  nsRect aRect;
  nsHTMLReflowMetrics baseSize (nsnull);
  nsHTMLReflowMetrics supScriptSize (nsnull);
  nsIFrame* baseFrame = nsnull;
  nsIFrame* supScriptFrame = nsnull;
  // parameter u in Rule 18a, Appendix G of the TeXbook
  nscoord minSupScriptShift = 0;   

  nsBoundingMetrics bmBase, bmSupScript;

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
	// superscript
	supScriptFrame = aChildFrame;
        GetReflowAndBoundingMetricsFor(supScriptFrame, supScriptSize, bmSupScript);
	// get the supdrop from the supscript font
	nscoord aSupDrop;
	GetSupDropFromChild (aPresContext, supScriptFrame, aSupDrop);
	// parameter u, Rule 18a, App. G, TeXbook
	minSupScriptShift = bmBase.ascent - aSupDrop;
      }
      count++;
    }
    rv = aChildFrame->GetNextSibling(&aChildFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }
#ifdef NS_DEBUG
  if (2 != count) PRINTF("msup: invalid markup");
#endif
  if ((2 != count) || !baseFrame || !supScriptFrame) {
    // report an error, encourage people to get their markups in order
    return ReflowError(aPresContext, aRenderingContext, aDesiredSize);
  }

  //////////////////
  // Place Children 
  
  // get min supscript shift limit from x-height
  // = d(x) + 1/4 * sigma_5, Rule 18c, App. G, TeXbook
  nscoord xHeight = 0;
  nsCOMPtr<nsIFontMetrics> fm;

//  const nsStyleFont* aFont =
//    (const nsStyleFont*) mStyleContext->GetStyleData (eStyleStruct_Font);

  const nsStyleFont *aFont;
  baseFrame->GetStyleData(eStyleStruct_Font, (const nsStyleStruct *&)aFont);

  aPresContext->GetMetricsFor (aFont->mFont, getter_AddRefs(fm));
  fm->GetXHeight (xHeight);
  nscoord minShiftFromXHeight = (nscoord) 
    (bmSupScript.descent + (1.0f/4.0f) * xHeight);

  // aSupScriptShift{1,2,3}
  // = minimum amount to shift the supscript up
  // = sup{1,2,3} in TeX
  // aSupScriptShift1 = superscriptshift attribute * x-height
  // Note that there are THREE values for supscript shifts depending
  // on the current style
  nscoord aSupScriptShift1, aSupScriptShift2, aSupScriptShift3;
  // Set aSupScriptShift{1,2,3} default from font
  GetSupScriptShifts (fm, aSupScriptShift1, aSupScriptShift2, aSupScriptShift3);

  if (0 < mSupScriptShift) {
    // the user has set the superscriptshift attribute
    float aFactor2 = ((float) aSupScriptShift2) / aSupScriptShift1;
    float aFactor3 = ((float) aSupScriptShift3) / aSupScriptShift1;
    aSupScriptShift1 = 
      PR_MAX(aSupScriptShift1, mSupScriptShift);
    aSupScriptShift2 = NSToCoordRound(aFactor2 * aSupScriptShift1);
    aSupScriptShift3 = NSToCoordRound(aFactor3 * aSupScriptShift1);
  }

  // get sup script shift depending on current script level and display style
  // Rule 18c, App. G, TeXbook
  nscoord aSupScriptShift;
  if ( mPresentationData.scriptLevel == 0 && 
       NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags) &&
      !NS_MATHML_IS_COMPRESSED(mPresentationData.flags)) {
    // Style D in TeXbook
    aSupScriptShift = aSupScriptShift1;
  }
  else if (NS_MATHML_IS_COMPRESSED(mPresentationData.flags)) {
    // Style C' in TeXbook = D',T',S',SS'
    aSupScriptShift = aSupScriptShift3;
  }
  else {
    // everything else = T,S,SS
    aSupScriptShift = aSupScriptShift2;
  }

  // get actual supscriptshift to be used
  // Rule 18c, App. G, TeXbook
  nscoord actualSupScriptShift = 
    PR_MAX(minSupScriptShift,PR_MAX(aSupScriptShift,minShiftFromXHeight));

  // bounding box
  mBoundingMetrics.ascent =
    PR_MAX(bmBase.ascent, (bmSupScript.ascent + actualSupScriptShift));
  mBoundingMetrics.descent =
    PR_MAX(bmBase.descent, (bmSupScript.descent - actualSupScriptShift));
  // add mScriptSpace between base and supscript
  mBoundingMetrics.width = bmBase.width + mScriptSpace + bmSupScript.width;
  mBoundingMetrics.leftBearing = bmBase.leftBearing;
  mBoundingMetrics.rightBearing = bmBase.width + mScriptSpace + bmSupScript.rightBearing;

  // reflow metrics
  aDesiredSize.ascent =
    PR_MAX(baseSize.ascent, (supScriptSize.ascent + actualSupScriptShift));
  aDesiredSize.descent =
    PR_MAX(baseSize.descent, (supScriptSize.descent - actualSupScriptShift));
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = baseSize.width + mScriptSpace + supScriptSize.width;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  if (aPlaceOrigin) {
    nscoord dx, dy;
    // now place the base ...
    dx = 0; dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, aPresContext, baseSize, dx, dy, 0);
    // ... and supscript
    dx = baseSize.width + mScriptSpace;
    dy = aDesiredSize.ascent - (supScriptSize.ascent + actualSupScriptShift);
    FinishReflowChild (supScriptFrame, aPresContext, supScriptSize, dx, dy, 0);
  }

  return NS_OK;
}
