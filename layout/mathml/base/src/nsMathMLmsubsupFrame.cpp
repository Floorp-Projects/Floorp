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
nsMathMLmsubsupFrame::Place(nsIPresContext*      aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            PRBool               aPlaceOrigin,
                            nsHTMLReflowMetrics& aDesiredSize)
{
  nsresult rv;
  aDesiredSize.width = aDesiredSize.height = aDesiredSize.ascent = aDesiredSize.descent = 0;
  nscoord ascent, descent;   
  nscoord count = 0;
  nsRect rect[3];
  nsIFrame* child[3];
  nsIFrame* childFrame = mFrames.FirstChild(); 
  while (childFrame) {
    if (!IsOnlyWhitespace(childFrame) && 3 > count) {
      child[count] = childFrame;
      childFrame->GetRect(rect[count]);
      count++;
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }
  descent = rect[0].x;
  ascent = rect[0].y;

  // Get the subscript and superscript offsets
  nscoord subscriptOffset, superscriptOffset, leading;
  nsCOMPtr<nsIFontMetrics> fm;
  const nsStyleFont* aFont =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
  aPresContext->GetMetricsFor(aFont->mFont, getter_AddRefs(fm));
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

  if (aPlaceOrigin) { 
    // child[0]->SetRect(aPresContext, rect[0]);
    // child[1]->SetRect(aPresContext, rect[1]);
    // child[2]->SetRect(aPresContext, rect[2]);
    nsHTMLReflowMetrics childSize(nsnull);
    for (PRInt32 i=0; i<count; i++) {
      childSize.width = rect[i].width;
      childSize.height = rect[i].height;
      FinishReflowChild(child[i], aPresContext, childSize, rect[i].x, rect[i].y, 0);
    }
  }
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  } 
  return NS_OK;
}
