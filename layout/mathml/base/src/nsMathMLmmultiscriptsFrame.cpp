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

#include "nsMathMLmmultiscriptsFrame.h"

//
// <mmultiscripts> -- attach prescripts and tensor indices to a base - implementation
//

nsresult
NS_NewMathMLmmultiscriptsFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmmultiscriptsFrame* it = new (aPresShell) nsMathMLmmultiscriptsFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmmultiscriptsFrame::nsMathMLmmultiscriptsFrame()
{
}

nsMathMLmmultiscriptsFrame::~nsMathMLmmultiscriptsFrame()
{
}

NS_IMETHODIMP
nsMathMLmmultiscriptsFrame::Init(nsIPresContext*  aPresContext,
				 nsIContent*      aContent,
				 nsIFrame*        aParent,
				 nsIStyleContext* aContext,
				 nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init
    (aPresContext, aContent, aParent, aContext, aPrevInFlow);

  mSubScriptShiftFactor = 0.0f;
  mSupScriptShiftFactor = 0.0f;
  mScriptSpace = NSFloatPointsToTwips(0.5f); // 0.5pt as in plain TeX

  // check for subscriptshift and superscriptshift attribute in ex units
  nsAutoString value;
  mSubUserSetFlag = mSupUserSetFlag = PR_FALSE;
  mSubScriptShiftFactor = mSubScriptShiftFactor = 0.0f;
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
nsMathMLmmultiscriptsFrame::Place(nsIPresContext*      aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  PRBool               aPlaceOrigin,
                                  nsHTMLReflowMetrics& aDesiredSize)
{
  nsresult rv = NS_OK;

  ////////////////////////////////////
  // Get the children's desired sizes

  nscoord italicCorrection = 0;
  nscoord minShiftFromXHeight, aSubDrop, aSupDrop;

  ////////////////////////////////////////
  // Initialize super/sub shifts that 
  // depend only on the current font
  ////////////////////////////////////////

  // get x-height (an ex)
  nscoord xHeight = 0;
  nsCOMPtr<nsIFontMetrics> fm;
  const nsStyleFont* aFont =
    (const nsStyleFont*) mStyleContext->GetStyleData (eStyleStruct_Font);
  aPresContext->GetMetricsFor (aFont->mFont, getter_AddRefs(fm));
  fm->GetXHeight (xHeight);

  nscoord aRuleSize, dummy;
  // XXX need to do update this ...
  fm->GetStrikeout (dummy, aRuleSize);

  /////////////////////////////////////
  // first the shift for the subscript

  // aSubScriptShift{1,2}
  // = minimum amount to shift the subscript down 
  // = sub{1,2} in TeXbook
  // aSubScriptShift1 = subscriptshift attribute * x-height
  nscoord aSubScriptShift1, aSubScriptShift2;

  // Get aSubScriptShift{1,2} default from font
  GetSubScriptShifts (fm, aSubScriptShift1, aSubScriptShift2);
  if (mSubUserSetFlag) {
    // the user has set the subscriptshift attribute
    float aFactor = ((float) aSubScriptShift2) / aSubScriptShift1;
    aSubScriptShift1 = NSToCoordRound(mSubScriptShiftFactor * xHeight);
    aSubScriptShift2 = NSToCoordRound(aFactor * aSubScriptShift1);
  }
  // the font dependent shift
  nscoord aSubScriptShift = PR_MAX(aSubScriptShift1,aSubScriptShift2);

  /////////////////////////////////////
  // next the shift for the superscript

  // aSupScriptShift{1,2,3}
  // = minimum amount to shift the supscript up
  // = sup{1,2,3} in TeX
  // aSupScriptShift1 = superscriptshift attribute * x-height
  // Note that there are THREE values for supscript shifts depending
  // on the current style
  nscoord aSupScriptShift1, aSupScriptShift2, aSupScriptShift3;
  // Set aSupScriptShift{1,2,3} default from font
  GetSupScriptShifts (fm, aSupScriptShift1, aSupScriptShift2, aSupScriptShift3);
  if (mSupUserSetFlag) {
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
  if (mScriptLevel == 0 && mDisplayStyle && !mCompressed) {
    // Style D in TeXbook
    aSupScriptShift = aSupScriptShift1;
  }
  else if (mCompressed) {
    // Style C' in TeXbook = D',T',S',SS'
    aSupScriptShift = aSupScriptShift3;
  }
  else {
    // everything else = T,S,SS
    aSupScriptShift = aSupScriptShift2;
  }

  ////////////////////////////////////
  // Get the children's sizes
  ////////////////////////////////////
  
  nscoord width = 0;
  nsIFrame* mprescriptsFrame = nsnull; // frame of <mprescripts/>, if there.  
  PRBool isSubScript = PR_FALSE;
  PRBool isSubScriptPresent = PR_TRUE;
  PRBool isSupScriptPresent = PR_TRUE;
  nscoord minSubScriptShift = 0, minSupScriptShift = 0; 
  nscoord trySubScriptShift = aSubScriptShift;
  nscoord trySupScriptShift = aSupScriptShift; 
  nscoord maxSubScriptShift = aSubScriptShift;
  nscoord maxSupScriptShift = aSupScriptShift; 
  PRInt32 count = 0;
  nsRect aRect;
  nsHTMLReflowMetrics baseSize (nsnull);
  nsHTMLReflowMetrics subScriptSize (nsnull);
  nsHTMLReflowMetrics supScriptSize (nsnull);
  nsIFrame* baseFrame;
  nsIFrame* subScriptFrame;
  nsIFrame* supScriptFrame;

  // XXX is there an NSPR macro for int_max ???
  aDesiredSize.ascent = aDesiredSize.descent = -100000;
  aDesiredSize.width = aDesiredSize.height = 0;

  nsIFrame* aChildFrame = mFrames.FirstChild();
  while (nsnull != aChildFrame) 
  {
    if (!IsOnlyWhitespace (aChildFrame)) {
      nsCOMPtr<nsIContent> childContent;
      nsCOMPtr<nsIAtom> childTag;
      aChildFrame->GetContent(getter_AddRefs(childContent));
      childContent->GetTag(*getter_AddRefs(childTag));

      if (childTag.get() == nsMathMLAtoms::mprescripts_) {
        mprescriptsFrame = aChildFrame;
      }
      else {
	if (childTag.get() == nsMathMLAtoms::none_) {
	  // we need to record the presence of none tag explicitly
	  // for correct negotiation between sup/sub shifts later
	  if (isSubScript) {
	    isSubScriptPresent = PR_FALSE;
	  }
	  else {
	    isSupScriptPresent = PR_FALSE;
	  }
	  aRect = nsRect (0, 0, 0, 0);
	}
	else {
	  aChildFrame->GetRect(aRect);
	}
        if (0 == count) {
	  // base 
	  baseFrame = aChildFrame;
	  baseSize.descent = aRect.x; baseSize.ascent = aRect.y;
	  baseSize.width = aRect.width;
	  baseSize.height = aRect.height;
	  // we update aDesiredSize.{ascent,descent} with that
	  // of the baseFrame only after processing all the sup/sub pairs
	  // XXX need italic correction here
	  aDesiredSize.width = aRect.width + italicCorrection;
	}
        else {
	  // super/subscript block
      	  if (isSubScript) {
	    if (isSubScriptPresent) {
	      // subscript
	      subScriptFrame = aChildFrame;
	      subScriptSize.descent = aRect.x; 
	      subScriptSize.ascent = aRect.y;
	      subScriptSize.width = aRect.width; 
	      subScriptSize.height = aRect.height;
	      // get the subdrop from the subscript font
	      GetSubDropFromChild (aPresContext, subScriptFrame, aSubDrop);
	      // parameter v, Rule 18a, App. G, TeXbook
	      minSubScriptShift = baseSize.descent + aSubDrop;
	      trySubScriptShift = PR_MAX(minSubScriptShift,aSubScriptShift);
	      aDesiredSize.descent = 
		PR_MAX(aDesiredSize.descent,subScriptSize.descent);
	      width = subScriptSize.width + mScriptSpace;
	    }
	  }
	  else {
	    if (isSupScriptPresent) {
	      // supscript
	      supScriptFrame = aChildFrame;
	      supScriptSize.descent = aRect.x; 
	      supScriptSize.ascent = aRect.y;
	      supScriptSize.width = aRect.width; 
	      supScriptSize.height = aRect.height;
	      // get the supdrop from the supscript font
	      GetSupDropFromChild (aPresContext, supScriptFrame, aSupDrop);
	      // parameter u, Rule 18a, App. G, TeXbook
	      minSupScriptShift = baseSize.ascent - aSupDrop;
	      // get min supscript shift limit from x-height
	      // = d(x) + 1/4 * sigma_5, Rule 18c, App. G, TeXbook
	      minShiftFromXHeight = NSToCoordRound 
		((supScriptSize.descent + (1.0f/4.0f) * xHeight));
	      trySupScriptShift = 
		PR_MAX(minSupScriptShift,PR_MAX(minShiftFromXHeight,aSupScriptShift));
	      aDesiredSize.ascent = 
		PR_MAX(aDesiredSize.ascent,supScriptSize.ascent);
	      width = PR_MAX(width, supScriptSize.width + mScriptSpace);
	    }

	    NS_ASSERTION((isSubScriptPresent || isSupScriptPresent),"mmultiscripts : both sup/subscripts are absent");
	    aDesiredSize.width += width;
	    width = 0;
	    
	    if (isSubScriptPresent && isSupScriptPresent) {
	      // negotiate between the various shifts so that 
	      // there is enough gap between the sup and subscripts
              // Rule 18e, App. G, TeXbook
              nscoord gap = 
                (trySupScriptShift - supScriptSize.descent) - 
                (subScriptSize.ascent - trySubScriptShift);
              if (gap < 4.0f * aRuleSize) {
                // adjust trySubScriptShift to get a gap of (4.0 * aRuleSize)
                trySubScriptShift += NSToCoordRound ((4.0f * aRuleSize) - gap);
              }
              
              // next we want to ensure that the bottom of the superscript
              // will be > (4/5) * x-height above baseline 
              gap = NSToCoordRound ((4.0f/5.0f) * xHeight - 
                      (trySupScriptShift - supScriptSize.descent));
              if (gap > 0.0f) {
                trySupScriptShift += gap;
                trySubScriptShift -= gap;
              }
	    }
	    maxSubScriptShift = PR_MAX(maxSubScriptShift, trySubScriptShift);
	    maxSupScriptShift = PR_MAX(maxSupScriptShift, trySupScriptShift);

	    trySubScriptShift = aSubScriptShift;
	    trySupScriptShift = aSupScriptShift; 
	    isSubScriptPresent = PR_TRUE;
	    isSupScriptPresent = PR_TRUE;
          }
        }
      
        isSubScript = !isSubScript;
        count++;
      }
    }
    
    rv = aChildFrame->GetNextSibling(&aChildFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }
  // we left out base during our bounding box updates, so ...
  aDesiredSize.ascent = 
    PR_MAX(aDesiredSize.ascent+maxSupScriptShift,baseSize.ascent);
  aDesiredSize.descent = 
    PR_MAX(aDesiredSize.descent+maxSubScriptShift,baseSize.descent);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  
  //////////////////
  // Place Children 

  // Place prescripts, followed by base, and then postscripts.
  // The list of frames is in the order: {base} {postscripts} {prescripts}
  // We go over the list in a circular manner, starting at <prescripts/>

  if (aPlaceOrigin) {
    nscoord dx = 0, dy = 0;

    count = 0;
    aChildFrame = mprescriptsFrame; 
    do {
      if (nsnull == aChildFrame) { // end of prescripts,
	// place the base ...
	aChildFrame = baseFrame; 
	dy = aDesiredSize.ascent - baseSize.ascent;
	FinishReflowChild (baseFrame, aPresContext, baseSize, dx, dy, 0);
	dx += baseSize.width + italicCorrection;
      }
      else if (mprescriptsFrame != aChildFrame) { 
	// process each sup/sub pair
	if (!IsOnlyWhitespace (aChildFrame)) {
	  if (0 == count) {
	    subScriptFrame = aChildFrame;
	    count++;
	  }
	  else if (1 == count) {
	    supScriptFrame = aChildFrame;
	    count = 0;
	    
	    // get the bounding boxes of sup/subscripts stored in their rects
	    // rect.x = descent, rect.y = ascent
	    subScriptFrame->GetRect (aRect);
	    subScriptSize.ascent = aRect.y;
	    subScriptSize.width = aRect.width;
	    subScriptSize.height = aRect.height;

	    supScriptFrame->GetRect (aRect);
	    supScriptSize.ascent = aRect.y;
	    supScriptSize.width = aRect.width;
	    supScriptSize.height = aRect.height;

	    // XXX should we really center the boxes
	    // XXX i'm leaving it as left-justified 
	    // XXX which is consistent with what's done for <msubsup>
	    dy = aDesiredSize.ascent - 
	      subScriptSize.ascent + 
	      maxSubScriptShift;
	    FinishReflowChild (subScriptFrame, aPresContext, 
			       subScriptSize, dx, dy, 0);

	    dy = aDesiredSize.ascent - 
	      supScriptSize.ascent -
	      maxSupScriptShift;
	    FinishReflowChild (supScriptFrame, aPresContext, 
			       supScriptSize, dx, dy, 0);

	    width = mScriptSpace + 
	      PR_MAX(subScriptSize.width, supScriptSize.width);
	    dx += width;
	  }        
	}
      }
      
      rv = aChildFrame->GetNextSibling(&aChildFrame);
      NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
    } while (mprescriptsFrame != aChildFrame);
  }
  
  return rv;
}
