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
 * The Initial Developer of the The Original Code is The University Of 
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
#include "nsStyleChangeList.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsIDOMText.h"
#include "nsITextContent.h"

#include "nsMathMLAtoms.h"
#include "nsMathMLParts.h"
#include "nsMathMLmiFrame.h"

//
// <mi> -- identifier - implementation
//

nsresult
NS_NewMathMLmiFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmiFrame* it = new nsMathMLmiFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;  
  return NS_OK;
}

nsMathMLmiFrame::nsMathMLmiFrame()
{
}

nsMathMLmiFrame::~nsMathMLmiFrame()
{
}

NS_IMETHODIMP
nsMathMLmiFrame::Init(nsIPresContext&  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow)
{
  nsresult rv = NS_OK;
  rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  return rv;
}
  
// if our content is not a single character, we turn the font style to normal.
//  XXX TrimWhitespace / CompressWhitespace?
NS_IMETHODIMP
nsMathMLmiFrame::ReResolveStyleContext(nsIPresContext*    aPresContext,
                                       nsIStyleContext*   aParentContext,
                                       PRInt32            aParentChange,
                                       nsStyleChangeList* aChangeList,
                                       PRInt32*           aLocalChange)
{
  // our content can include comment-nodes, attribute-nodes, text-nodes...
  // we use to DOM to make sure that we are only looking at text-nodes...
  PRInt32 aLength = 0;
  PRInt32 numKids;
  mContent->ChildCount(numKids);
  // nsAutoString aData;
  for (PRInt32 kid=0; kid<numKids; kid++) {
    nsCOMPtr<nsIContent> kidContent;
    mContent->ChildAt(kid, *getter_AddRefs(kidContent));
    if (kidContent.get()) {      	
      nsCOMPtr<nsIDOMText> kidText(do_QueryInterface(kidContent));
      if (kidText.get()) {
      	PRUint32 kidLength;
        kidText->GetLength(&kidLength);
        aLength += kidLength;        
      	// nsAutoString kidData;
        // kidText->GetData(kidData);
        // aData += kidData;
      }
    }
  }

  // re calculate our own style context
  PRInt32 ourChange = aParentChange;
  nsresult rv = nsFrame::ReResolveStyleContext(aPresContext, aParentContext,
                                               ourChange, aChangeList, &ourChange);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (aLocalChange) {
    *aLocalChange = ourChange;
  }

  // get a pseudo style context for the normal style font
  nsCOMPtr<nsIAtom> fontAtom(getter_AddRefs(NS_NewAtom(":-moz-math-font-style-normal")));
  nsIStyleContext* newStyleContext;
  aPresContext->ResolvePseudoStyleContextFor(mContent, fontAtom, mStyleContext,
                                             PR_FALSE, &newStyleContext);
  // re-resolve children
  PRInt32 childChange;
  nsIFrame* child = mFrames.FirstChild();
  while (nsnull != child && NS_SUCCEEDED(rv)) {
    if (!IsOnlyWhitespace(child)) {
      rv = child->ReResolveStyleContext(aPresContext, 
                                        (1 == aLength)? mStyleContext : newStyleContext,
                                        ourChange, aChangeList, &childChange);
    }
    child->GetNextSibling(&child);
  }
  return rv;
}

