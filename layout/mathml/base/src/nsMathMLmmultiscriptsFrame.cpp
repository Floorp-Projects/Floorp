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
nsMathMLmmultiscriptsFrame::Place(nsIPresContext*      aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  PRBool               aPlaceOrigin,
                                  nsHTMLReflowMetrics& aDesiredSize)
{
  nsresult rv;
  nsIFrame* mprescriptsFrame = nsnull; // frame of <mprescripts/>, if there.  
  PRBool isSubscript = PR_FALSE;
  nscoord ascent, descent, width, height;
  nscoord subsupWidth, subHeight, supHeight, subDescent, supAscent;
  nscoord count = 0;

  nsIFrame* baseFrame = nsnull;
  nsIFrame* childFrame = mFrames.FirstChild();
  nsRect childRect;
  while (childFrame)  {
    if (!IsOnlyWhitespace(childFrame)) {

      nsCOMPtr<nsIContent> childContent;
      nsCOMPtr<nsIAtom> childTag;
      childFrame->GetContent(getter_AddRefs(childContent));
      childContent->GetTag(*getter_AddRefs(childTag));

      if (childTag.get() == nsMathMLAtoms::mprescripts_) {
        mprescriptsFrame = childFrame;
      }
      else if (childTag.get() != nsMathMLAtoms::none_) {
        childFrame->GetRect(childRect);
        if (0 == count) {
          baseFrame = childFrame;
          descent = childRect.x;
          ascent = childRect.y;
          height = childRect.height;
          subsupWidth = childRect.width;
          subHeight = supHeight = height;
          supAscent = ascent; 
          subDescent = descent;
        }
        else {
      	  if (isSubscript) {
      	    subDescent = PR_MAX(subDescent, childRect.x);
      	    subHeight = PR_MAX(subHeight, childRect.height);
      	    width = childRect.width;
          }
      	  else {
      	    supAscent = PR_MAX(supAscent, childRect.y);
            supHeight = PR_MAX(supHeight, childRect.height);
            width = PR_MAX(width, childRect.width);
            subsupWidth += width;
          }
        }
      
        isSubscript = !isSubscript;
        count++;
      }
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }

  // Get the subscript and superscript offsets
  nscoord subscriptOffset, superscriptOffset, leading;
  nsCOMPtr<nsIFontMetrics> fm;
  const nsStyleFont* aFont =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
  aPresContext->GetMetricsFor(aFont->mFont, getter_AddRefs(fm));
  fm->GetSubscriptOffset(subscriptOffset);
  fm->GetSuperscriptOffset(superscriptOffset);
  fm->GetLeading(leading);

  subHeight = PR_MAX(height, subHeight) + subscriptOffset + leading;
  supHeight = PR_MAX(height, supHeight) + superscriptOffset;
 
  aDesiredSize.descent = subHeight - ascent;
  aDesiredSize.ascent = supHeight - descent;
  aDesiredSize.height = aDesiredSize.descent + aDesiredSize.ascent;
  aDesiredSize.width = subsupWidth;
 
  nscoord offset = 0;
  nsIFrame* child[2];
  nsRect rect[2];

  count = 0;
  nsHTMLReflowMetrics childSize(nsnull);
  childFrame = mprescriptsFrame; 
  do {
    if (nsnull == childFrame) { // end of prescripts,
//printf("Placing the base ...\n");
      childFrame = baseFrame;   // place the base
      childFrame->GetRect(rect[0]);
      rect[0].x = offset;
      rect[0].y = aDesiredSize.height - subHeight;
      if (aPlaceOrigin) {
        // childFrame->SetRect(aPresContext, rect[0]);
        childSize.width = rect[0].width;
        childSize.height = rect[0].height;
        FinishReflowChild(child[0], aPresContext, childSize, rect[0].x, rect[0].y, 0);
      }
      offset += rect[0].width;
    }
    else if (mprescriptsFrame != childFrame) { 
      //////////////
      // WHITESPACE: whitespace doesn't count. Iteration over non empty frames
      if (!IsOnlyWhitespace(childFrame)) {
        child[count++] = childFrame;
        if (2 == count) { // place a sub-sup pair
//printf("Placing the sub-sup pair...\n");

          child[0]->GetRect(rect[0]);
          child[1]->GetRect(rect[1]);
          rect[0].y = aDesiredSize.height - (subDescent + rect[0].y);  
          rect[1].y = supAscent - rect[1].y;

          width = PR_MAX(rect[0].width, rect[1].width);
          rect[0].x = offset + (width - rect[0].width) / 2; // centering
          rect[1].x = offset + (width - rect[1].width) / 2; 

          if (aPlaceOrigin) {
            // child[0]->SetRect(aPresContext, rect[0]);
            // child[1]->SetRect(aPresContext, rect[1]);
            nsHTMLReflowMetrics childSize(nsnull);
            for (PRInt32 i=0; i<count; i++) {
              childSize.width = rect[i].width;
              childSize.height = rect[i].height;
              FinishReflowChild(child[i], aPresContext, childSize, rect[i].x, rect[i].y, 0);
            }
          }    
          offset += width;
          count = 0;
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
  return rv;
}

