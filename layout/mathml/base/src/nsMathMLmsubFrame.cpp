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

  mSubScriptShiftFactor = 0.0;
  mScriptSpace = NSFloatPointsToTwips(0.5f); // 0.5pt as in plain TeX

  // check for subscriptshift attribute in ex units
  nsAutoString value;
  mUserSetFlag = PR_FALSE;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute
      (kNameSpaceID_None, nsMathMLAtoms::subscriptshift_, value)) {
    PRInt32 aErrorCode;
    float aUserValue = value.ToFloat(&aErrorCode);
    if (NS_SUCCEEDED(aErrorCode)) {
      mUserSetFlag = PR_TRUE;
      mSubScriptShiftFactor = aUserValue;
    }
  }

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
  nsRect aRect;
  nsHTMLReflowMetrics baseSize (nsnull);
  nsHTMLReflowMetrics subScriptSize (nsnull);
  nsIFrame* baseFrame;
  nsIFrame* subScriptFrame;
  // parameter v, Rule 18a, Appendix G of the TeXbook
  nscoord minSubScriptShift = 0; 

  nsBoundingMetrics baseBounds, subScriptBounds;

  nsIFrame* aChildFrame = mFrames.FirstChild();
  while (nsnull != aChildFrame)
  {
    if (!IsOnlyWhitespace(aChildFrame)) {
      aChildFrame->GetRect(aRect);
      if (0 == count) {
	// base 
	baseFrame = aChildFrame;
	baseSize.descent = aRect.x; baseSize.ascent = aRect.y;
	baseSize.width = aRect.width; baseSize.height = aRect.height;
        if (NS_SUCCEEDED(GetBoundingMetricsFor(baseFrame, baseBounds))) {
          baseBounds.descent = -baseBounds.descent;
        } else {
          baseBounds.descent = baseSize.descent;
          baseBounds.ascent = baseSize.ascent;
          baseBounds.width = baseSize.width;
        }
      }
      else if (1 == count) {
	// subscript
	subScriptFrame = aChildFrame;
	subScriptSize.descent = aRect.x; subScriptSize.ascent = aRect.y;
	subScriptSize.width = aRect.width; subScriptSize.height = aRect.height;
        if (NS_SUCCEEDED(GetBoundingMetricsFor(subScriptFrame, subScriptBounds))) {
          subScriptBounds.descent = -subScriptBounds.descent;
        } else {
          subScriptBounds.descent = subScriptSize.descent;
          subScriptBounds.ascent = subScriptSize.ascent;
          subScriptBounds.width = subScriptSize.width;
        }
	// get the subdrop from the subscript font
	nscoord aSubDrop;
	GetSubDropFromChild (aPresContext, subScriptFrame, aSubDrop);
	// parameter v, Rule 18a, App. G, TeXbook
	minSubScriptShift = baseSize.descent + aSubDrop;
      }
      else {
	NS_ASSERTION((count < 2),"nsMathMLmsubFrame : invalid markup");
      }
      count++;
    }
    
    rv = aChildFrame->GetNextSibling(&aChildFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }

  //////////////////
  // Place Children
  
  // get min subscript shift limit from x-height
  // = h(x) - 4/5 * sigma_5, Rule 18b, App. G, TeXbook
  nscoord xHeight = 0;
  nsCOMPtr<nsIFontMetrics> fm;
  const nsStyleFont* aFont =
    (const nsStyleFont*) mStyleContext->GetStyleData (eStyleStruct_Font);
  aPresContext->GetMetricsFor (aFont->mFont, getter_AddRefs(fm));
  fm->GetXHeight (xHeight);
  nscoord minShiftFromXHeight = (nscoord) 
    (subScriptSize.ascent - (4.0f/5.0f) * xHeight);

  // aSubScriptShift
  // = minimum amount to shift the subscript down set by user or from the font
  // = sub1 in TeX
  // = subscriptshift attribute * x-height
  nscoord aSubScriptShift, dummy;
  // get aSubScriptShift default from font
  GetSubScriptShifts (fm, aSubScriptShift, dummy);
  if (mUserSetFlag == PR_TRUE) {
    // the user has set the subscriptshift attribute
    aSubScriptShift = NSToCoordRound(mSubScriptShiftFactor * xHeight);
  }

  // get actual subscriptshift to be used
  // Rule 18b, App. G, TeXbook
  nscoord actualSubScriptShift = 
    PR_MAX(minSubScriptShift,PR_MAX(aSubScriptShift,minShiftFromXHeight));
#if 0
  // get bounding box for base + subscript
  aDesiredSize.ascent = 
    PR_MAX(baseSize.ascent,(subScriptSize.ascent-actualSubScriptShift));
  aDesiredSize.descent = 
    PR_MAX(baseSize.descent,(actualSubScriptShift+subScriptSize.descent));
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  // add mScriptSpace between base and subscript
  aDesiredSize.width = baseSize.width + mScriptSpace + subScriptSize.width;
#endif

  mBoundingMetrics.ascent = 
    PR_MAX(baseBounds.ascent, subScriptBounds.ascent - actualSubScriptShift);
  mBoundingMetrics.descent = 
   PR_MAX(baseBounds.descent, subScriptBounds.descent + actualSubScriptShift);
  // add mScriptSpace between base and supscript
  mBoundingMetrics.width = baseBounds.width + mScriptSpace + subScriptBounds.width;

  aDesiredSize.ascent = 
     PR_MAX(baseSize.ascent, subScriptSize.ascent - actualSubScriptShift);
  aDesiredSize.descent = 
     PR_MAX(baseSize.descent, subScriptSize.descent + actualSubScriptShift);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

//XXX wrong  aDesiredSize.width = mBoundingMetrics.width;

  mBoundingMetrics.descent = -mBoundingMetrics.descent;

  if (aPlaceOrigin) {
    nscoord dx, dy;
    // now place the base ...
    dx = 0; dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, aPresContext, baseSize, dx, dy, 0);
    // ... and subscript
    dx = baseSize.width; 
    dy = aDesiredSize.ascent - subScriptSize.ascent + actualSubScriptShift;
    FinishReflowChild (subScriptFrame, aPresContext, subScriptSize, dx, dy, 0);
  }

  return NS_OK;
}
