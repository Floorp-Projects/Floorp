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

#include "nsMathMLAtoms.h"
#include "nsMathMLParts.h"
#include "nsMathMLmmultiscriptsFrame.h"

//
// <mmultiscripts> -- attach prescripts and tensor indices to a base - implementation
//

nsresult
NS_NewMathMLmmultiscriptsFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmmultiscriptsFrame* it = new nsMathMLmmultiscriptsFrame;
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
nsMathMLmmultiscriptsFrame::Reflow(nsIPresContext&          aPresContext,
                                   nsHTMLReflowMetrics&     aDesiredSize,
                                   const nsHTMLReflowState& aReflowState,
                                   nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  nsReflowStatus childStatus;
  nsHTMLReflowMetrics childDesiredSize(aDesiredSize.maxElementSize);
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);

  //////////////////
  // Reflow Children
  
  nsIFrame* mprescriptsFrame = nsnull; // frame of <mprescripts/>, if there.  
  PRBool isSubscript = PR_FALSE;
  nscoord ascent, descent, width, height;
  nscoord subsupWidth, subHeight, supHeight, subDescent, supAscent;
  nscoord count = 0;

  nsIFrame* baseFrame = nsnull;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (nsnull != childFrame) 
  {
    //////////////
    // WHITESPACE: don't forget that whitespace doesn't count in MathML!
    if (IsOnlyWhitespace(childFrame)) {
      ReflowEmptyChild(childFrame);
    }
    else {
      nsCOMPtr<nsIContent> childContent;
      nsCOMPtr<nsIAtom> childTag;
      childFrame->GetContent(getter_AddRefs(childContent));
      childContent->GetTag(*getter_AddRefs(childTag));

      if (childTag == nsMathMLAtoms::mprescripts) {
//        NS_ASSERTION(mprescriptsFrame == nsnull,"duplicate <mprescripts/>");
//printf("mprescripts Found ...\n");  // should ignore?
        mprescriptsFrame = childFrame;
      }
      else if (childTag == nsMathMLAtoms::none) {
        childDesiredSize.height = 0;
        childDesiredSize.width = 0;
        childDesiredSize.ascent = 0;
        childDesiredSize.descent = 0;
      }
      else {
//printf("child count: %d...\n", count);
        nsHTMLReflowState childReflowState(aPresContext, aReflowState, 
                                           childFrame, availSize);
        rv = ReflowChild(childFrame, aPresContext, childDesiredSize, 
                         childReflowState, childStatus);
        NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
        if (NS_FAILED(rv)) {
          return rv;
        }

        if (0 == count) {
          baseFrame = childFrame;
          ascent = childDesiredSize.ascent;
          descent = childDesiredSize.descent;
          height = childDesiredSize.height;
          subsupWidth = childDesiredSize.width;
          subHeight = supHeight = height;
          supAscent = ascent; 
          subDescent = descent;
        }
        else {
      	  if (isSubscript) {
      	    subDescent = PR_MAX(subDescent, childDesiredSize.descent);
      	    subHeight = PR_MAX(subHeight, childDesiredSize.height);
      	    width = childDesiredSize.width;
          }
      	  else {
      	    supAscent = PR_MAX(supAscent, childDesiredSize.ascent);
            supHeight = PR_MAX(supHeight, childDesiredSize.height);
            width = PR_MAX(width, childDesiredSize.width);
            subsupWidth += width;
          }
        }
      
        // At this stage, the origin points of the children have no use, so we will use the
        // origins to store the child's ascent and descent. At the next pass, we should 
        // set the origins so as to overwrite what we are storing there now
        childFrame->SetRect(nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                            childDesiredSize.width, childDesiredSize.height));                        
        isSubscript = !isSubscript;
        count++;
      }
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }

  //////////////////
  // Place Children 

  // Get the subscript and superscript offsets
  nscoord subscriptOffset, superscriptOffset, leading;
  nsCOMPtr<nsIFontMetrics> fm;
  const nsStyleFont* aFont =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
  aPresContext.GetMetricsFor(aFont->mFont, getter_AddRefs(fm));
  fm->GetSubscriptOffset(subscriptOffset);
  fm->GetSuperscriptOffset(superscriptOffset);
  fm->GetLeading(leading);

  subHeight = PR_MAX(height, subHeight) + subscriptOffset + leading;
  supHeight = PR_MAX(height, supHeight) + superscriptOffset;
 
  aDesiredSize.descent = subHeight - ascent;
  aDesiredSize.ascent = supHeight - descent;
  aDesiredSize.height = aDesiredSize.descent + aDesiredSize.ascent;
  aDesiredSize.width = subsupWidth;
 
  // Place prescripts, followed by base, and then postscripts.
  // The list of frames is in the order: {base} {postscripts} {prescripts}
  // We go over the list in a circular manner, starting at <prescripts/>

  nscoord offset = 0;
  nsIFrame* child[2];
  nsRect rect[2];

  count = 0;
  childFrame = mprescriptsFrame; 
  do {
    if (nsnull == childFrame) { // end of prescripts,
//printf("Placing the base ...\n");
      childFrame = baseFrame;   // place the base
      childFrame->GetRect(rect[0]);
      rect[0].x = offset;
      rect[0].y = aDesiredSize.height - subHeight;
      childFrame->SetRect(rect[0]);
      offset += rect[0].width;
    }
    else if (mprescriptsFrame != childFrame) { 
      //////////////
      // WHITESPACE: whitespace doesn't count. Iteration over non empty frames
      if (!IsOnlyWhitespace(childFrame)) {
        child[count++] = childFrame;
        if (2 == count) { // place a sub-sup pair
          count = 0;
//printf("Placing the sub-sup pair...\n");

          child[0]->GetRect(rect[0]);
          child[1]->GetRect(rect[1]);
          rect[0].y = aDesiredSize.height - (subDescent + rect[0].y);  
          rect[1].y = supAscent - rect[1].y;

          width = PR_MAX(rect[0].width, rect[1].width);
          rect[0].x = offset + (width - rect[0].width) / 2; // centering
          rect[1].x = offset + (width - rect[1].width) / 2; 

          child[0]->SetRect(rect[0]);
          child[1]->SetRect(rect[1]);          
          offset += width;
        }        
      }
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  } while (mprescriptsFrame != childFrame);

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }  
  aStatus = NS_FRAME_COMPLETE;
  return rv;
}
