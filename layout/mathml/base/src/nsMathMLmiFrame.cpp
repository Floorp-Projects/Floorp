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
#include "nsIFrameManager.h"
#include "nsStyleChangeList.h"

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

static PRBool
IsStyleInvariant(PRUnichar aChar)
{
  return nsMathMLOperators::LookupInvariantChar(aChar);
}

// if our content is not a single character, we turn the font to normal
void
nsMathMLmiFrame::ProcessTextData(nsIPresContext* aPresContext)
{
  if (!mFrames.FirstChild())
    return;

  // Get the text content that we enclose and its length
  // our content can include comment-nodes, attribute-nodes, text-nodes...
  // we use the DOM to make sure that we are only looking at text-nodes...
  nsAutoString data;
  PRInt32 numKids;
  mContent->ChildCount(numKids);
  for (PRInt32 kid = 0; kid < numKids; kid++) {
    nsCOMPtr<nsIContent> kidContent;
    mContent->ChildAt(kid, *getter_AddRefs(kidContent));
    if (kidContent.get()) {
      nsCOMPtr<nsIDOMText> kidText(do_QueryInterface(kidContent));
      if (kidText.get()) {
        nsAutoString kidData;
        kidText->GetData(kidData);
        data += kidData;
      }
    }
  }

  // attributes may override the default behavior
  PRInt32 length = data.Length();
  nsAutoString fontstyle;
  PRBool restyle = PR_TRUE;
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::fontstyle_, fontstyle))
    restyle = PR_FALSE;
  if (1 == length) {
    // our textual content consists of a single character
    if (IsStyleInvariant(data[0])) {
      // bug 65951 - we always enforce the style to normal for a non-stylable char
      // XXX also disable bold type? (makes sense to let the set IR be bold, no?)
      fontstyle.Assign(NS_LITERAL_STRING("normal"));
      restyle = PR_TRUE;
    }
    else {
      fontstyle.Assign(NS_LITERAL_STRING("italic"));
    }
  }
  else {
    // our textual content consists of multiple characters
    fontstyle.Assign(NS_LITERAL_STRING("normal"));
  }

  // set the -moz-math-font-style attribute without notifying that we want a reflow
  if (restyle)
    mContent->SetAttr(kNameSpaceID_None, nsMathMLAtoms::fontstyle, fontstyle, PR_FALSE);

  // then, re-resolve the style contexts in our subtree
  nsCOMPtr<nsIPresShell> presShell;
  aPresContext->GetShell(getter_AddRefs(presShell));
  if (presShell) {
    nsCOMPtr<nsIFrameManager> fm;
    presShell->GetFrameManager(getter_AddRefs(fm));
    if (fm) {
      PRInt32 maxChange, minChange = NS_STYLE_HINT_NONE;
      nsStyleChangeList changeList;
      fm->ComputeStyleChangeFor(aPresContext, this,
                                kNameSpaceID_None, nsMathMLAtoms::fontstyle,
                                changeList, minChange, maxChange);
    }
  }
}

NS_IMETHODIMP
nsMathMLmiFrame::TransmitAutomaticData(nsIPresContext* aPresContext)
{
  // re-style our content depending on what it is
  // (this will query attributes from the mstyle hierarchy)
  ProcessTextData(aPresContext);
  return NS_OK;
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

NS_IMETHODIMP
nsMathMLmiFrame::ReflowDirtyChild(nsIPresShell* aPresShell,
                                  nsIFrame*     aChild)
{
  // if we get this, it means it was called by the nsTextFrame beneath us, and
  // this means something changed in the text content. So re-process our text

  nsCOMPtr<nsIPresContext> presContext;
  aPresShell->GetPresContext(getter_AddRefs(presContext));
  ProcessTextData(presContext);

  mState |= NS_FRAME_IS_DIRTY | NS_FRAME_HAS_DIRTY_CHILDREN;
  return mParent->ReflowDirtyChild(aPresShell, this);
}
