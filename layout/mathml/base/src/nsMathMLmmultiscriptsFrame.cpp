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

  mSubScriptShift = 0;
  mSupScriptShift = 0;
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
  // check if the superscriptshift attribute is there
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::superscriptshift_, value)) {
    nsCSSValue cssValue;
    if (ParseNumericValue(value, cssValue) && cssValue.IsLengthUnit()) {
      mSupScriptShift = CalcLength(aPresContext, mStyleContext, cssValue);
    }
  }

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif
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

  nscoord minShiftFromXHeight, aSubDrop, aSupDrop;

  ////////////////////////////////////////
  // Initialize super/sub shifts that 
  // depend only on the current font
  ////////////////////////////////////////

  // get x-height (an ex)
#if 0
  nscoord xHeight = 0;
  nsCOMPtr<nsIFontMetrics> fm;
  const nsStyleFont* aFont =
    (const nsStyleFont*) mStyleContext->GetStyleData (eStyleStruct_Font);
  aPresContext->GetMetricsFor (aFont->mFont, getter_AddRefs(fm));
  fm->GetXHeight (xHeight);
#endif

  nsStyleFont font;
  mStyleContext->GetStyle(eStyleStruct_Font, font);
  aRenderingContext.SetFont(font.mFont);
  nsCOMPtr<nsIFontMetrics> fm;
  aRenderingContext.GetFontMetrics(*getter_AddRefs(fm));

  nscoord xHeight;
  fm->GetXHeight (xHeight);

  nscoord aRuleSize;
  GetRuleThickness (aRenderingContext, fm, aRuleSize);

  /////////////////////////////////////
  // first the shift for the subscript

  // aSubScriptShift{1,2}
  // = minimum amount to shift the subscript down 
  // = sub{1,2} in TeXbook
  // aSubScriptShift1 = subscriptshift attribute * x-height
  nscoord aSubScriptShift1, aSubScriptShift2;

  // Get aSubScriptShift{1,2} default from font
  GetSubScriptShifts (fm, aSubScriptShift1, aSubScriptShift2);
  if (0 < mSubScriptShift) {
    // the user has set the subscriptshift attribute
    float aFactor = ((float) aSubScriptShift2) / aSubScriptShift1;
    aSubScriptShift1 = PR_MAX(aSubScriptShift1, mSubScriptShift);
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
  if (0 < mSupScriptShift) {
    // the user has set the superscriptshift attribute
    float aFactor2 = ((float) aSupScriptShift2) / aSupScriptShift1;
    float aFactor3 = ((float) aSupScriptShift3) / aSupScriptShift1;
    aSupScriptShift1 = PR_MAX(aSupScriptShift1, mSupScriptShift);
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

  ////////////////////////////////////
  // Get the children's sizes
  ////////////////////////////////////
  
  nscoord width = 0, prescriptsWidth = 0, rightBearing = 0;
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
  nsHTMLReflowMetrics baseSize (nsnull);
  nsHTMLReflowMetrics subScriptSize (nsnull);
  nsHTMLReflowMetrics supScriptSize (nsnull);
  nsIFrame* baseFrame = nsnull;
  nsIFrame* subScriptFrame = nsnull;
  nsIFrame* supScriptFrame = nsnull;

  PRBool firstPrescriptsPair = PR_FALSE;
  nsBoundingMetrics bmBase, bmSubScript, bmSupScript;
  nscoord italicCorrection = 0;

  // XXX is there an NSPR macro for int_max ???
  mBoundingMetrics.ascent = mBoundingMetrics.descent = -10000000;
  mBoundingMetrics.width = 0;

  aDesiredSize.ascent = aDesiredSize.descent = -10000000;
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
        firstPrescriptsPair = PR_TRUE;
      }
      else {

        if (childTag.get() == nsMathMLAtoms::none_) {
          // we need to record the presence of none tag explicitly
          // for correct negotiation between sup/sub shifts later
          if (isSubScript) {
            isSubScriptPresent = PR_FALSE;
            bmSubScript.Clear();
            bmSubScript.leftBearing = 10000000;
          }
          else {
            isSupScriptPresent = PR_FALSE;
            bmSupScript.Clear();
            bmSupScript.leftBearing = 10000000;
          }
        }

        if (0 == count) {
          // base 
          baseFrame = aChildFrame;
          GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
          GetItalicCorrection(bmBase, italicCorrection);

          // we update mBoundingMetrics.{ascent,descent} with that
          // of the baseFrame only after processing all the sup/sub pairs
          // XXX need italic correction only *if* there are postscripts ?
          mBoundingMetrics.width = bmBase.width + italicCorrection;
          mBoundingMetrics.rightBearing = bmBase.rightBearing;
          mBoundingMetrics.leftBearing = bmBase.leftBearing; // until overwritten
        }
        else {
          // super/subscript block
          if (isSubScript) {
            if (isSubScriptPresent) {
              // subscript
              subScriptFrame = aChildFrame;
              GetReflowAndBoundingMetricsFor(subScriptFrame, subScriptSize, bmSubScript);
              // get the subdrop from the subscript font
              GetSubDropFromChild (aPresContext, subScriptFrame, aSubDrop);
              // parameter v, Rule 18a, App. G, TeXbook
              minSubScriptShift = bmBase.descent + aSubDrop;
              trySubScriptShift = PR_MAX(minSubScriptShift,aSubScriptShift);
              mBoundingMetrics.descent = 
                PR_MAX(mBoundingMetrics.descent,bmSubScript.descent);
              aDesiredSize.descent = 
                PR_MAX(aDesiredSize.descent,subScriptSize.descent);
              width = bmSubScript.width + mScriptSpace;
              rightBearing = bmSubScript.rightBearing + mScriptSpace;
            }
          }
          else {
            if (isSupScriptPresent) {
              // supscript
              supScriptFrame = aChildFrame;
              GetReflowAndBoundingMetricsFor(supScriptFrame, supScriptSize, bmSupScript);
              // get the supdrop from the supscript font
              GetSupDropFromChild (aPresContext, supScriptFrame, aSupDrop);
              // parameter u, Rule 18a, App. G, TeXbook
              minSupScriptShift = bmBase.ascent - aSupDrop;
              // get min supscript shift limit from x-height
              // = d(x) + 1/4 * sigma_5, Rule 18c, App. G, TeXbook
              minShiftFromXHeight = NSToCoordRound 
                ((bmSupScript.descent + (1.0f/4.0f) * xHeight));
              trySupScriptShift = 
                PR_MAX(minSupScriptShift,PR_MAX(minShiftFromXHeight,aSupScriptShift));
              mBoundingMetrics.ascent = 
                PR_MAX(mBoundingMetrics.ascent,bmSupScript.ascent);
              aDesiredSize.ascent = 
                PR_MAX(aDesiredSize.ascent,supScriptSize.ascent);
              width = PR_MAX(width, bmSupScript.width + mScriptSpace);
              rightBearing = PR_MAX(rightBearing, bmSupScript.rightBearing + mScriptSpace);
            }

            if (!isSubScriptPresent && !isSupScriptPresent) {
              // report an error, encourage people to get their markups in order
              return ReflowError(aPresContext, aRenderingContext, aDesiredSize);
            }

            if (!mprescriptsFrame) { // we are still looping over base & postscripts
              mBoundingMetrics.rightBearing = mBoundingMetrics.width + rightBearing;
              mBoundingMetrics.width += width;
            }
            else {
              prescriptsWidth += width;
              if (firstPrescriptsPair) {
                firstPrescriptsPair = PR_FALSE;
                mBoundingMetrics.leftBearing = 
                  PR_MIN(bmSubScript.leftBearing, bmSupScript.leftBearing);
              }
            }
            width = rightBearing = 0;

            if (isSubScriptPresent && isSupScriptPresent) {
              // negotiate between the various shifts so that 
              // there is enough gap between the sup and subscripts
              // Rule 18e, App. G, TeXbook
              nscoord gap = 
                (trySupScriptShift - bmSupScript.descent) - 
                (bmSubScript.ascent - trySubScriptShift);
              if (gap < 4.0f * aRuleSize) {
                // adjust trySubScriptShift to get a gap of (4.0 * aRuleSize)
                trySubScriptShift += NSToCoordRound ((4.0f * aRuleSize) - gap);
              }
              
              // next we want to ensure that the bottom of the superscript
              // will be > (4/5) * x-height above baseline 
              gap = NSToCoordRound ((4.0f/5.0f) * xHeight - 
                      (trySupScriptShift - bmSupScript.descent));
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
    
    aChildFrame->GetNextSibling(&aChildFrame);
  }
  // note: width=0 if all sup-sub pairs match correctly
  if ((0 != width) || !baseFrame || !subScriptFrame || !supScriptFrame) {
    // report an error, encourage people to get their markups in order
    return ReflowError(aPresContext, aRenderingContext, aDesiredSize);
  }

  // we left out the width of prescripts, so ...
  mBoundingMetrics.rightBearing += prescriptsWidth;
  mBoundingMetrics.width += prescriptsWidth;

  // we left out the base during our bounding box updates, so ...
  mBoundingMetrics.ascent = 
    PR_MAX(mBoundingMetrics.ascent+maxSupScriptShift,bmBase.ascent);
  mBoundingMetrics.descent = 
    PR_MAX(mBoundingMetrics.descent+maxSubScriptShift,bmBase.descent);

  // get the reflow metrics ...
  aDesiredSize.ascent = 
    PR_MAX(aDesiredSize.ascent+maxSupScriptShift,baseSize.ascent);
  aDesiredSize.descent = 
    PR_MAX(aDesiredSize.descent+maxSubScriptShift,baseSize.descent);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  //////////////////
  // Place Children 

  // Place prescripts, followed by base, and then postscripts.
  // The list of frames is in the order: {base} {postscripts} {prescripts}
  // We go over the list in a circular manner, starting at <prescripts/>

  if (aPlaceOrigin) {
    nscoord dx = 0, dy = 0;
    nsRect aRect;

    count = 0;
    aChildFrame = mprescriptsFrame; 
    do {
      if (nsnull == aChildFrame) { // end of prescripts,
        // place the base ...
        aChildFrame = baseFrame; 
        dy = aDesiredSize.ascent - baseSize.ascent;
        FinishReflowChild (baseFrame, aPresContext, baseSize, dx, dy, 0);
        dx += bmBase.width + mScriptSpace + italicCorrection;
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
            
            // get the ascent/descent of sup/subscripts stored in their rects
            // rect.x = descent, rect.y = ascent
            GetReflowAndBoundingMetricsFor(subScriptFrame, subScriptSize, bmSubScript);
            GetReflowAndBoundingMetricsFor(supScriptFrame, supScriptSize, bmSupScript);

            // center w.r.t. largest width
            width = PR_MAX(subScriptSize.width, supScriptSize.width);

            dy = aDesiredSize.ascent - 
              subScriptSize.ascent + 
              maxSubScriptShift;
            FinishReflowChild (subScriptFrame, aPresContext, subScriptSize,
                               dx + (width-subScriptSize.width)/2, dy, 0);

            dy = aDesiredSize.ascent - 
              supScriptSize.ascent -
              maxSupScriptShift;
            FinishReflowChild (supScriptFrame, aPresContext, supScriptSize,
                               dx + (width-supScriptSize.width)/2, dy, 0);

            dx += mScriptSpace + width;
          }
        }
      }

      aChildFrame->GetNextSibling(&aChildFrame);
    } while (mprescriptsFrame != aChildFrame);
  }
  
  return rv;
}
