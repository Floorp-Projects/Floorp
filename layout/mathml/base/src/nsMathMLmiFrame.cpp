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

// if our content is a single character, we turn the font to italic
// if our content is not a single character, we turn the font to normal
// XXX TrimWhitespace / CompressWhitespace?

NS_IMETHODIMP
nsMathMLmiFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aChildList)
{
  nsresult rv;

  // First, let the base class do its work
  rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);


  // Get the length of the text content that we enclose  
  // our content can include comment-nodes, attribute-nodes, text-nodes...
  // we use the DOM to make sure that we are only looking at text-nodes...
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

  // Insert a new pseudo frame between our children and us, i.e., the new frame
  // becomes our sole child, and our children become children of the new frame.
  nsIFrame* firstChild = mFrames.FirstChild();
  if (firstChild) {
    // Get a pseudo style context for the appropriate style font
    // XXX how important is the PseudoStyleContext?
    nsAutoString fontStyle = (1 == aLength) 
                           ? ":-moz-math-font-style-italic" 
                           : ":-moz-math-font-style-normal";
    nsCOMPtr<nsIAtom> fontAtom(getter_AddRefs(NS_NewAtom(fontStyle)));
    nsCOMPtr<nsIStyleContext> newStyleContext;
    aPresContext.ResolvePseudoStyleContextFor(mContent, fontAtom, mStyleContext,
                                              PR_FALSE, getter_AddRefs(newStyleContext));          
    
    if (newStyleContext && newStyleContext.get() != mStyleContext) {
    
      nsIFrame* newFrame = nsnull;
      NS_NewMathMLWrapperFrame(&newFrame);
      NS_ASSERTION(newFrame, "Failed to create new frame");
    
      newFrame->Init(aPresContext, mContent, this, newStyleContext, nsnull);
    
      // our children become children of the new frame
      nsIFrame* childFrame = firstChild;
      while (childFrame) {
        childFrame->SetParent(newFrame);
        aPresContext.ReParentStyleContext(childFrame, newStyleContext);
        childFrame->GetNextSibling(&childFrame);
      }
      newFrame->SetInitialChildList(aPresContext, nsnull, firstChild);
    
      // the new frame becomes our sole child
      mFrames.SetFrames(newFrame);
    }
  }
  return rv;
}
