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
nsMathMLmsubsupFrame::Init(nsIPresContext*  aPresContext,
			   nsIContent*      aContent,
			   nsIFrame*        aParent,
			   nsIStyleContext* aContext,
			   nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init
    (aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mSubScriptShiftFactor = 0.0f;
  mSupScriptShiftFactor = 0.0f;
  mScriptSpace = 0;

  // check for subscriptshift and superscriptshift attribute in ex units
  nsAutoString value;
  mSubUserSetFlag = mSupUserSetFlag = PR_FALSE;
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute
      (kNameSpaceID_None, nsMathMLAtoms::subscriptshift_, value)) {
    PRInt32 aErrorCode;
    float aUserValue = value.ToFloat(&aErrorCode);
    if (NS_SUCCEEDED(aErrorCode)) {
      mSubUserSetFlag = PR_TRUE;
      mSubScriptShiftFactor = aUserValue;
    }
  }
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute
      (kNameSpaceID_None, nsMathMLAtoms::superscriptshift_, value)) {
    PRInt32 aErrorCode;
    float aUserValue = value.ToFloat(&aErrorCode);
    if (NS_SUCCEEDED(aErrorCode)) {
      mSupUserSetFlag = PR_TRUE;
      mSupScriptShiftFactor = aUserValue;
    }
  }

  return rv;
}

NS_IMETHODIMP
nsMathMLmsubsupFrame::Place(nsIPresContext*      aPresContext,
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
  nsHTMLReflowMetrics supScriptSize (nsnull);
  nsIFrame* baseFrame;
  nsIFrame* subScriptFrame;
  nsIFrame* supScriptFrame;
  // parameter v, Rule 18a, Appendix G of the TeXbook
  nscoord minSubScriptShift = 0; 
  // parameter u in Rule 18a, Appendix G of the TeXbook
  nscoord minSupScriptShift = 0;   

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
      }
      else if (1 == count) {
	// subscript
	subScriptFrame = aChildFrame;
	subScriptSize.descent = aRect.x; subScriptSize.ascent = aRect.y;
	subScriptSize.width = aRect.width; subScriptSize.height = aRect.height;
	// get the subdrop from the subscript font
	nscoord aSubDrop;
	GetSubDropFromChild (aPresContext, subScriptFrame, aSubDrop);
	// parameter v, Rule 18a, App. G, TeXbook
	minSubScriptShift = baseSize.descent + aSubDrop;
      }
      else if (2 == count) {
	// superscript
	supScriptFrame = aChildFrame;
	supScriptSize.descent = aRect.x; supScriptSize.ascent = aRect.y;
	supScriptSize.width = aRect.width; supScriptSize.height = aRect.height;
	// get the supdrop from the supscript font
	nscoord aSupDrop;
	GetSupDropFromChild (aPresContext, supScriptFrame, aSupDrop);
	// parameter u, Rule 18a, App. G, TeXbook
	minSupScriptShift = baseSize.ascent - aSupDrop;
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
  //////////////////

  //////////////////////////////////////////////////
  // Get subscript shift
  // slightly different from nsMathMLmsubFrame.cpp
  //////////////////////////////////////////////////
  
  // aSubScriptShift{1,2}
  // = minimum amount to shift the subscript down 
  // = sub{1,2} in TeXbook
  // aSubScriptShift1 = subscriptshift attribute * x-height
  nscoord aSubScriptShift1, aSubScriptShift2;

  // get x-height (an ex)
  nscoord xHeight = 0;
  nsCOMPtr<nsIFontMetrics> fm;
  const nsStyleFont* aFont =
    (const nsStyleFont*) mStyleContext->GetStyleData (eStyleStruct_Font);
  aPresContext->GetMetricsFor (aFont->mFont, getter_AddRefs(fm));
  fm->GetXHeight (xHeight);
  
  // Get aSubScriptShift{1,2} default from font
  GetSubScriptShifts (fm, aSubScriptShift1, aSubScriptShift2);
  if (mSubUserSetFlag == PR_TRUE) {
    // the user has set the subscriptshift attribute
    float aFactor = ((float) aSubScriptShift2) / aSubScriptShift1;
    aSubScriptShift1 = NSToCoordRound(mSubScriptShiftFactor * xHeight);
    aSubScriptShift2 = NSToCoordRound(aFactor * aSubScriptShift1);
  }
 
  // get a tentative value for subscriptshift
  // Rule 18d, App. G, TeXbook
  nscoord aSubScriptShift = 
    PR_MAX(minSubScriptShift,PR_MAX(aSubScriptShift1,aSubScriptShift2));


  //////////////////////////////////////////////////
  // Get supscript shift
  // same code from nsMathMLmsupFrame.cpp
  //////////////////////////////////////////////////
  
  // get min supscript shift limit from x-height
  // = d(x) + 1/4 * sigma_5, Rule 18c, App. G, TeXbook
  nscoord minShiftFromXHeight = (nscoord) 
    (supScriptSize.descent + (1.0f/4.0f) * xHeight);

  // aSupScriptShift{1,2,3}
  // = minimum amount to shift the supscript up
  // = sup{1,2,3} in TeX
  // aSupScriptShift1 = superscriptshift attribute * x-height
  // Note that there are THREE values for supscript shifts depending
  // on the current style
  nscoord aSupScriptShift1, aSupScriptShift2, aSupScriptShift3;
  // Set aSupScriptShift{1,2,3} default from font
  GetSupScriptShifts (fm, aSupScriptShift1, aSupScriptShift2, aSupScriptShift3);
  if (mSupUserSetFlag == PR_TRUE) {
    // the user has set the superscriptshift attribute
    float aFactor2 = ((float) aSupScriptShift2) / aSupScriptShift1;
    float aFactor3 = ((float) aSupScriptShift3) / aSupScriptShift1;
    aSupScriptShift1 = NSToCoordRound(mSupScriptShiftFactor * xHeight);
    aSupScriptShift2 = NSToCoordRound(aFactor2 * aSupScriptShift1);
    aSupScriptShift3 = NSToCoordRound(aFactor3 * aSupScriptShift1);
  }

  // get sup script shift depending on current script level and display style
  // Rule 18c, App. G, TeXbook
  nscoord aSupScriptShift;
  if ((mScriptLevel == 0) && 
      (mDisplayStyle == PR_TRUE) && 
      (mCompressed == PR_FALSE)) {
    // Style D in TeXbook
    aSupScriptShift = aSupScriptShift1;
  }
  else if (mCompressed == PR_TRUE) {
    // Style C' in TeXbook = D',T',S',SS'
    aSupScriptShift = aSupScriptShift3;
  }
  else {
    // everything else = T,S,SS
    aSupScriptShift = aSupScriptShift2;
  }

  // get tentative value for superscriptshift 
  // Rule 18c, App. G, TeXbook
  aSupScriptShift = 
    PR_MAX(minSupScriptShift,PR_MAX(aSupScriptShift,minShiftFromXHeight));

  
  //////////////////////////////////////////////////
  // Negotiate between aSupScriptShift and aSubScriptShift
  // so that there will be enough gap between them
  // Rule 18e, App. G, TeXbook
  //////////////////////////////////////////////////

  nscoord aRuleSize, dummy;
  // XXX need to do update this ...
  fm->GetStrikeout (dummy, aRuleSize);
  nscoord gap = 
    (aSupScriptShift - supScriptSize.descent) - 
    (subScriptSize.ascent - aSubScriptShift);
  if (gap < 4.0f * aRuleSize) {
    // adjust aSubScriptShift to get a gap of (4.0 * aRuleSize)
    aSubScriptShift += NSToCoordRound ((4.0f * aRuleSize) - gap);
  }

  // next we want to ensure that the bottom of the superscript
  // will be > (4/5) * x-height above baseline 
  gap = NSToCoordRound ((4.0f/5.0f) * xHeight - 
			(aSupScriptShift - supScriptSize.descent));
  if (gap > 0.0f) {
    aSupScriptShift += gap;
    aSubScriptShift -= gap;
  }

  //////////////////////////////////////////////////
  // Do the Placing
  //////////////////////////////////////////////////
  
  // get bounding box for base + subscript + superscript
  aDesiredSize.ascent = 
    PR_MAX(baseSize.ascent,(supScriptSize.ascent+aSupScriptShift));
  aDesiredSize.descent = 
    PR_MAX(baseSize.descent,(supScriptSize.descent+aSubScriptShift));
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  // add mScriptSpace to both super/subscript
  // add italicCorrection only to superscript
  // XXX this will be handled properly later ...
  nscoord italicCorrection = 0;
  aDesiredSize.width = baseSize.width + mScriptSpace + 
    PR_MAX((supScriptSize.width + italicCorrection),subScriptSize.width);
  
  if (aPlaceOrigin) {
    nscoord dx, dy;
    // now place the base ...
    dx = 0; dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, aPresContext, baseSize, dx, dy, 0);
    // ... and subscript
    dx = baseSize.width; 
    dy = aDesiredSize.ascent - subScriptSize.ascent + aSubScriptShift;
    FinishReflowChild (subScriptFrame, aPresContext, subScriptSize, dx, dy, 0);
    // ... and the superscript
    dx = baseSize.width + italicCorrection;
    dy = aDesiredSize.ascent - supScriptSize.ascent - aSupScriptShift;
    FinishReflowChild (supScriptFrame, aPresContext, supScriptSize, dx, dy, 0);
  }

  return NS_OK;
}
