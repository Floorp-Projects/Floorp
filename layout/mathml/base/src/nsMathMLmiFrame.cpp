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
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsIDOMText.h"

#include "nsMathMLmiFrame.h"

//
// <mi> -- identifier - implementation
//

nsresult
NS_NewMathMLmiFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmiFrame* it = new (aPresShell) nsMathMLmiFrame;
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
nsMathMLmiFrame::Init(nsIPresContext*  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsIStyleContext* aContext,
                      nsIFrame*        aPrevInFlow)
{
  nsresult rv = NS_OK;
  rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif
  return rv;
}

static PRBool
IsStyleInvariant(PRUnichar aChar)
{
  return nsMathMLOperators::LookupInvariantChar(aChar);
}

// if our content is not a single character, we turn the font to normal
NS_IMETHODIMP
nsMathMLmiFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                     nsIAtom*        aListName,
                                     nsIFrame*       aChildList)
{
  nsresult rv;

  // First, let the base class do its work
  rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  // Get the text content that we enclose and its length
  // our content can include comment-nodes, attribute-nodes, text-nodes...
  // we use the DOM to make sure that we are only looking at text-nodes...
  nsAutoString data;
  PRInt32 length = 0;
  PRInt32 numKids;
  mContent->ChildCount(numKids);
  for (PRInt32 kid = 0; kid < numKids; kid++) {
    nsCOMPtr<nsIContent> kidContent;
    mContent->ChildAt(kid, *getter_AddRefs(kidContent));
    if (kidContent.get()) {      	
      nsCOMPtr<nsIDOMText> kidText(do_QueryInterface(kidContent));
      if (kidText.get()) {
      	PRUint32 kidLength;
        kidText->GetLength(&kidLength);
        length += kidLength;        
      	nsAutoString kidData;
        kidText->GetData(kidData);
        data += kidData;
      }
    }
  }

  PRBool isStyleInvariant = (1 == length) && IsStyleInvariant(data[0]);
  nsIFrame* firstChild = mFrames.FirstChild();
  if (firstChild && ((1 < length) || isStyleInvariant)) {
    // we are going to switch the font to normal ...

    // we always enforce the style to normal if this is a non-stylable char.
    // for other chars, we don't disable the italic style if we are in the
    // scope of a mstyle frame with an explicit fontstyle="italic" ...
    // XXX Need to also disable any bold style for non-stylable char.
    nsAutoString fontstyle;
    if (!isStyleInvariant) {
      if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                       nsMathMLAtoms::fontstyle_, fontstyle))
      {
        if (fontstyle.Equals(NS_LITERAL_STRING("italic")))
          return rv;
      }
    }

    // set the -moz-math-font-style attribute without notifying that we want a reflow
    fontstyle.Assign(NS_LITERAL_STRING("normal"));
    mContent->SetAttr(kNameSpaceID_None, nsMathMLAtoms::fontstyle,
                      fontstyle, PR_FALSE);
    // then, re-resolve the style contexts in our subtree
    nsCOMPtr<nsIStyleContext> parentStyleContext;
    parentStyleContext = getter_AddRefs(mStyleContext->GetParent());
    nsCOMPtr<nsIStyleContext> newStyleContext;
    aPresContext->ResolveStyleContextFor(mContent, parentStyleContext,
                                         PR_FALSE, getter_AddRefs(newStyleContext));
    if (newStyleContext && newStyleContext.get() != mStyleContext) {
      SetStyleContext(aPresContext, newStyleContext);
      nsIFrame* childFrame = mFrames.FirstChild();
      while (childFrame) {
        aPresContext->ReParentStyleContext(childFrame, newStyleContext);
        childFrame->GetNextSibling(&childFrame);
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsMathMLmiFrame::Reflow(nsIPresContext*          aPresContext,
                        nsHTMLReflowMetrics&     aDesiredSize,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aStatus)
{
  return ReflowTokenFor(this, aPresContext, aDesiredSize,
                        aReflowState, aStatus);
}

NS_IMETHODIMP
nsMathMLmiFrame::Place(nsIPresContext*      aPresContext,
                       nsIRenderingContext& aRenderingContext,
                       PRBool               aPlaceOrigin,
                       nsHTMLReflowMetrics& aDesiredSize)
{
  return PlaceTokenFor(this, aPresContext, aRenderingContext,
                       aPlaceOrigin, aDesiredSize);
}
