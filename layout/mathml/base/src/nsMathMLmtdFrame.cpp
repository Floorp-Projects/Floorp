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
#include "nsAreaFrame.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsMathMLmtdFrame.h"

//
// <mtd> -- table or matrix - implementation
//

nsresult
NS_NewMathMLmtdFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
//  return NS_NewMathMLmrowFrame(aPresShell, aNewFrame);

  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmtdFrame* it = new (aPresShell) nsMathMLmtdFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
// XXX RBS -  what about mFlags ?
// it->SetFlags(NS_AREA_WRAP_SIZE); ?
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmtdFrame::nsMathMLmtdFrame()
{
}

nsMathMLmtdFrame::~nsMathMLmtdFrame()
{
}

#if 0
// wrap our children in an anonymous inferred mrow
NS_IMETHODIMP
nsMathMLmtdFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aChildList)
{
  // First, let the base class do its work
  nsresult rv = nsAreaFrame::SetInitialChildList(aPresContext, aListName, aChildList);

  nsCOMPtr<nsIPresShell> shell;
  aPresContext->GetShell(getter_AddRefs(shell));
 
  nsIFrame* newFrame = nsnull;
  NS_NewMathMLmrowFrame(shell, &newFrame);
  NS_ASSERTION(newFrame, "Failed to create new frame");
  if (newFrame) {
    nsCOMPtr<nsIStyleContext> newStyleContext;
    aPresContext->ResolvePseudoStyleContextFor(mContent, nsHTMLAtoms::mozAnonymousBlock, 
                                               mStyleContext, PR_FALSE, 
                                               getter_AddRefs(newStyleContext));
    rv = newFrame->Init(aPresContext, mContent, this, newStyleContext, nsnull);
    if (NS_FAILED(rv)) {
      newFrame->Destroy(aPresContext);
      return rv;
    }
    // our children become children of the new frame
    nsIFrame* firstChild =  mFrames.FirstChild();
    nsIFrame* childFrame = firstChild;
    while (childFrame) {
      childFrame->SetParent(newFrame);
      aPresContext->ReParentStyleContext(childFrame, newStyleContext);
      childFrame->GetNextSibling(&childFrame);
    }
    newFrame->SetInitialChildList(aPresContext, nsnull, firstChild);
    // the new frame becomes our sole child
    mFrames.SetFrames(newFrame);
  }

  return rv;
}
#endif


NS_IMETHODIMP
nsMathMLmtdFrame::Reflow(nsIPresContext*          aPresContext,
                         nsHTMLReflowMetrics&     aDesiredSize,
                         const nsHTMLReflowState& aReflowState,
                         nsReflowStatus&          aStatus)
{
  // Let the base class do the reflow
  nsresult rv = nsBlockFrame::Reflow(aPresContext, aDesiredSize, aReflowState, aStatus);

  printf("Ending cell body with aDesiredSize: asc:%d  desc:%d\n", 
         aDesiredSize.ascent, aDesiredSize.descent);

#if 0
  // make sure that we return the desired size of our inner mrow frame
  nsIFrame* childFrame = mFrames.FirstChild();
  nsIMathMLFrame* aMathMLFrame = nsnull;
  rv = childFrame->QueryInterface(nsIMathMLFrame::GetIID(), (void**)&aMathMLFrame);
  NS_ASSERTION(NS_SUCCEEDED(rv) && nsnull != aMathMLFrame, "Mystery");
  if (NS_SUCCEEDED(rv) && nsnull != aMathMLFrame) {
    // get our ascent and descent
    nsPoint ref;
    aMathMLFrame->GetReference(ref);
    nsBoundingMetrics bm;
    aMathMLFrame->GetBoundingMetrics(bm);

//    aDesiredSize.ascent = ref.y + bm.ascent;
//    aDesiredSize.descent = aDesiredSize.height - ref.y;

  nsRect rect;
  childFrame->GetRect(rect);

  nsFrame::ListTag(stdout, childFrame);
  printf("\nEnding mtd with: rect(%d,%d,%d,%d) ref(%d,%d) bm:asc=%d,desc=%d, asc:%d  desc:%d\n", 
     rect.x,rect.y,rect.width,rect.height, ref.x, ref.y, bm.ascent,bm.descent, aDesiredSize.ascent,aDesiredSize.descent);
  }
#endif

  // more about <maligngroup/> and <malignmark/> later
  // ...
  return rv;
}

NS_IMETHODIMP
nsMathMLmtdFrame::DidReflow(nsIPresContext*   aPresContext,
                            nsDidReflowStatus aStatus)
{
  nsRect rect;
  GetRect(rect);
  nsFrame::ListTag(stdout, this);
  printf("\nend mtd with: rect(%d,%d,%d,%d)\n", 
          rect.x,rect.y,rect.width,rect.height);
 
 #if 0
  nsIFrame* childFrame = mFrames.FirstChild();
  childFrame->GetRect(rect);
  nsFrame::ListTag(stdout, this);
  printf("\nend child mtd with: rect(%d,%d,%d,%d)\n", 
          rect.x,rect.y,rect.width,rect.height);
#endif

//  NS_ASSERTION(0, "Break in mtd::DidReflow");

  return nsHTMLContainerFrame::DidReflow(aPresContext, aStatus);
}

#if 0
NS_IMETHODIMP
nsMathMLmtdFrame:Reflow(nsIPresContext*          aPresContext,
                        nsHTMLReflowMetrics&     aMetrics,
                        const nsHTMLReflowState& aReflowState,
                        nsReflowStatus&          aReflowStatus)
{
  nsresult rv = NS_OK;

  nsIFrame* kid = mFrames.FirstChild();
  nsIFrame* nextRCFrame = nsnull;
  if (aReflowState.reason == eReflowReason_Incremental) {
    nsIFrame* target;
    aReflowState.reflowCommand->GetTarget(target);
    if (this != target) {
      aReflowState.reflowCommand->GetNext(nextRCFrame);
    }
  }

  // Setup reflow state for our child
  nsSize availSize(aReflowState.availableWidth, aReflowState.availableHeight);
  const nsMargin& bp = aReflowState.mComputedBorderPadding;
  nscoord lr = bp.left + bp.right;
  nscoord tb = bp.top + bp.bottom;
  if (NS_UNCONSTRAINEDSIZE != availSize.width) {
    availSize.width -= lr;
  }
  if (NS_UNCONSTRAINEDSIZE != availSize.height) {
    availSize.height -= tb;
  }

  // Reflow the child
  if (!aReflowState.mLineLayout) {
printf("No line layout for mtd ....\n");
    // When there is no lineLayout provided, we provide our own. The
    // only time that the first-letter-frame is not reflowing in a
    // line context is when its floating.
    nsHTMLReflowState rs(aPresContext, aReflowState, kid, availSize);
    nsLineLayout ll(aPresContext, nsnull, &aReflowState,
                    nsnull != aMetrics.maxElementSize);
//    ll.BeginLineReflow(0, 0, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE,
//                       PR_FALSE, PR_TRUE);

    ll.BeginLineReflow(0, 0, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE,
                       PR_FALSE, PR_FALSE);

//  aLineLayout.BeginLineReflow(x, aState.mY,
//                              availWidth, availHeight,
//                              impactedByFloaters,
//                              PR_FALSE /*XXX isTopOfPage*/);


    rs.mLineLayout = &ll;
//    ll.SetFirstLetterStyleOK(PR_TRUE);

    kid->WillReflow(aPresContext);
    kid->Reflow(aPresContext, aMetrics, rs, aReflowStatus);

    ll.EndLineReflow();
  }
  else {
printf("Line layout for mtd is %p....\n", aReflowState.mLineLayout);

    // Pretend we are a span and reflow the child frame
    nsLineLayout* ll = aReflowState.mLineLayout;
    PRBool        pushedFrame;

    ll->BeginSpan(this, &aReflowState, bp.left, availSize.width);
    ll->ReflowFrame(kid, &nextRCFrame, aReflowStatus, &aMetrics, pushedFrame);
    nsSize size;
    ll->EndSpan(this, size, aMetrics.maxElementSize);
  }

  // Place and size the child and update the output metrics
  kid->MoveTo(aPresContext, bp.left, bp.top);
  kid->SizeTo(aPresContext, aMetrics.width, aMetrics.height);
  kid->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);
  aMetrics.width += lr;
  aMetrics.height += tb;
  aMetrics.ascent += bp.top;
  aMetrics.descent += bp.bottom;
  if (aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width += lr;
    aMetrics.maxElementSize->height += tb;
  }

  return rv;
}
#endif
