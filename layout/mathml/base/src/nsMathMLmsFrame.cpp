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
#include "nsITextContent.h"
#include "nsLayoutAtoms.h"

#include "nsMathMLmsFrame.h"

//
// <ms> -- string literal - implementation
//

nsresult
NS_NewMathMLmsFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmsFrame* it = new (aPresShell) nsMathMLmsFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmsFrame::nsMathMLmsFrame()
{
}

nsMathMLmsFrame::~nsMathMLmsFrame()
{
}

static void
SetQuote(nsIPresContext* aPresContext, 
         nsIFrame*       aFrame, 
         nsString&       aValue)
{
  nsIFrame* textFrame;
  do {
    // walk down the hierarchy of first children because they could be wrapped
    aFrame->FirstChild(aPresContext, nsnull, &textFrame);
    if (textFrame) {
      nsCOMPtr<nsIAtom> frameType;
      textFrame->GetFrameType(getter_AddRefs(frameType));
      if (frameType && frameType.get() == nsLayoutAtoms::textFrame)
        break;
    }
    aFrame = textFrame;
  } while (textFrame);
  if (textFrame) {
    nsCOMPtr<nsIContent> quoteContent;
    textFrame->GetContent(getter_AddRefs(quoteContent));
    if (quoteContent.get()) {
      nsCOMPtr<nsIDOMText> domText(do_QueryInterface(quoteContent));
      if (domText.get()) {
        nsCOMPtr<nsITextContent> tc(do_QueryInterface(quoteContent));
        if (tc) {
          tc->SetText(aValue, PR_FALSE); // no notify since we don't want a reflow yet
        }
      }
    }
  }
}

NS_IMETHODIMP
nsMathMLmsFrame::TransmitAutomaticData(nsIPresContext* aPresContext)
{
  // It is assumed that the mathml.css file contains two rules:
  // ms:before { content: open-quote; }
  // ms:after { content: close-quote; }
  // With these two rules, the frame construction code will
  // create inline frames that contain text frames which themselves
  // contain the text content of the quotes.
  // So the main idea in this code is to see if there are lquote and 
  // rquote attributes. If these are there, we ovewrite the default
  // quotes in the text frames.

  // But what if the mathml.css file wasn't loaded? 
  // We also check that we are not relying on null pointers...

  nsIFrame* rightFrame = nsnull;
  nsIFrame* baseFrame = nsnull;
  nsIFrame* leftFrame = mFrames.FirstChild();
  if (leftFrame)
    leftFrame->GetNextSibling(&baseFrame);
  if (baseFrame)
    baseFrame->GetNextSibling(&rightFrame);
  if (!leftFrame || !baseFrame || !rightFrame)
    return NS_OK;

  nsAutoString value;
  // lquote
  if (NS_CONTENT_ATTR_NOT_THERE != GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::lquote_, value)) {
    SetQuote(aPresContext, leftFrame, value);
  }
  // rquote
  if (NS_CONTENT_ATTR_NOT_THERE != GetAttribute(mContent, mPresentationData.mstyle,
                   nsMathMLAtoms::rquote_, value)) {
    SetQuote(aPresContext, rightFrame, value);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmsFrame::AttributeChanged(nsIPresContext* aPresContext,
                                  nsIContent*     aContent,
                                  PRInt32         aNameSpaceID,
                                  nsIAtom*        aAttribute,
                                  PRInt32         aModType, 
                                  PRInt32         aHint)
{
  if (nsMathMLAtoms::lquote_ == aAttribute ||
      nsMathMLAtoms::rquote_ == aAttribute) {
    // When the automatic data to update are only within our
    // children, we just re-layout them
    return ReLayoutChildren(aPresContext, this);
  }

  return nsMathMLContainerFrame::
         AttributeChanged(aPresContext, aContent, aNameSpaceID,
                          aAttribute, aModType, aHint);
}
