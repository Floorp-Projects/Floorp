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
#include "nsMathMLmsubsupFrame.h"

//
// <msubsup> -- attach a subscript-superscript pair to a base - implementation
//

nsresult
NS_NewMathMLmsubsupFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmsubsupFrame* it = new nsMathMLmsubsupFrame;
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
nsMathMLmsubsupFrame::Reflow(nsIPresContext&          aPresContext,
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

  nscoord ascent, descent;   
  nscoord count = 0;
  nsRect rect[3];
  nsIFrame* child[3];
  nsIFrame* childFrame = mFrames.FirstChild(); 
  while (nsnull != childFrame) 
  {
    //////////////
    // WHITESPACE: don't forget that whitespace doesn't count in MathML!
    if (IsOnlyWhitespace(childFrame)) {
      ReflowEmptyChild(childFrame);      
    }
    else if (3 > count) {
      nsHTMLReflowState childReflowState(aPresContext, aReflowState, 
                                         childFrame, availSize);  	
      rv = ReflowChild(childFrame, aPresContext, childDesiredSize, 
                       childReflowState, childStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
      if (NS_FAILED(rv)) {
        return rv;
      }                  

      child[count] = childFrame;
      rect[count].width = childDesiredSize.width;
      rect[count].height = childDesiredSize.height;
      if (0 == count) {
        ascent = childDesiredSize.ascent;
        descent = childDesiredSize.descent;
      }
      count++;
    }
//  else { invalid markup... }

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
  
//#if 0
// XXX temporary hack pending bug http://bugzilla.mozilla.org/show_bug.cgi?id=9640
nscoord fmAscent, xHeight;
fm->GetMaxAscent(fmAscent);
fm->GetXHeight(xHeight);
subscriptOffset = PR_MAX(subscriptOffset,fmAscent-(xHeight*4)/5);
//#endif

  subscriptOffset += leading;
  superscriptOffset += leading;

  // logic of superimposing a box with sub and the same box with sup

  rect[0].x = 0;
  rect[1].x = rect[0].width;
  rect[2].x = rect[0].width;

  nscoord subHeight = rect[0].height + rect[1].height - subscriptOffset;
  nscoord supHeight = rect[0].height + rect[2].height - superscriptOffset;

  aDesiredSize.descent = subHeight - ascent;
  aDesiredSize.ascent = supHeight - descent;
  aDesiredSize.height = aDesiredSize.descent + aDesiredSize.ascent;
  aDesiredSize.width = rect[0].width + PR_MAX(rect[1].width, rect[2].width);

  rect[2].y = 0;
  rect[1].y = aDesiredSize.height - rect[1].height;  
  rect[0].y = aDesiredSize.height - subHeight;  
 
  child[0]->SetRect(rect[0]);
  child[1]->SetRect(rect[1]);
  child[2]->SetRect(rect[2]);

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  } 
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}
