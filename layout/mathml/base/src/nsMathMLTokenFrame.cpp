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
 */

#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsIPresContext.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsIDOMText.h"
#include "nsITextContent.h"
#include "nsIFrameManager.h"
#include "nsLayoutAtoms.h"
#include "nsStyleChangeList.h"
#include "nsINameSpaceManager.h"

#include "nsMathMLTokenFrame.h"

nsresult
NS_NewMathMLTokenFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLTokenFrame* it = new (aPresShell) nsMathMLTokenFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;  
  return NS_OK;
}

nsMathMLTokenFrame::nsMathMLTokenFrame()
{
}

nsMathMLTokenFrame::~nsMathMLTokenFrame()
{
}

NS_IMETHODIMP
nsMathMLTokenFrame::GetFrameType(nsIAtom** aType) const
{
  *aType = nsMathMLAtoms::ordinaryMathMLFrame;
  NS_ADDREF(*aType);
  return NS_OK;
}

static void
CompressWhitespace(nsIContent* aContent)
{
  PRInt32 numKids;
  aContent->ChildCount(numKids);
  for (PRInt32 kid = 0; kid < numKids; kid++) {
    nsCOMPtr<nsIContent> kidContent;
    aContent->ChildAt(kid, *getter_AddRefs(kidContent));
    if (kidContent) {       
      nsCOMPtr<nsIDOMText> kidText(do_QueryInterface(kidContent));
      if (kidText) {
        nsCOMPtr<nsITextContent> tc(do_QueryInterface(kidContent));
        if (tc) {
          nsAutoString text;
          tc->CopyText(text);
          text.CompressWhitespace();
          tc->SetText(text, PR_FALSE); // not meant to be used if notify is needed
        }
      }
    }
  }
}

NS_IMETHODIMP
nsMathMLTokenFrame::Init(nsIPresContext*  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
  // leading and trailing whitespace doesn't count -- bug 15402
  // brute force removal for people who do <mi> a </mi> instead of <mi>a</mi>
  // XXX the best fix is to skip these in nsTextFrame
  CompressWhitespace(aContent);

  // let the base class do its Init()
  return nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
}

NS_IMETHODIMP
nsMathMLTokenFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                        nsIAtom*        aListName,
                                        nsIFrame*       aChildList)
{
  // First, let the base class do its work
  nsresult rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
  if (NS_FAILED(rv))
    return rv;

  // Safety measure to cater for math fonts with metrics that sometimes
  // cause glyphs in the text frames to protrude outside. Without this,
  // such glyphs may be clipped at the painting stage
  mState |= NS_FRAME_OUTSIDE_CHILDREN;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsFrameState state;
    childFrame->GetFrameState(&state);
    childFrame->SetFrameState(state | NS_FRAME_OUTSIDE_CHILDREN);
    childFrame->GetNextSibling(&childFrame);
  }

  SetQuotes(aPresContext);
  ProcessTextData(aPresContext);
  return rv;
}

nsresult
nsMathMLTokenFrame::Reflow(nsIPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;

  // See if this is an incremental reflow
  if (aReflowState.reason == eReflowReason_Incremental) {
#ifdef MATHML_NOISY_INCREMENTAL_REFLOW
printf("nsMathMLContainerFrame::ReflowTokenFor:IncrementalReflow received by: ");
nsFrame::ListTag(stdout, aFrame);
printf("\n");
#endif
  }

  // initializations needed for empty markup like <mtag></mtag>
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;
  aDesiredSize.mBoundingMetrics.Clear();

  // ask our children to compute their bounding metrics
  nsHTMLReflowMetrics childDesiredSize(aDesiredSize.mComputeMEW,
                      aDesiredSize.mFlags | NS_REFLOW_CALC_BOUNDING_METRICS);
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);
  PRInt32 count = 0;
  nsIFrame* childFrame;
  FirstChild(aPresContext, nsnull, &childFrame);
  while (childFrame) {
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    rv = ReflowChild(childFrame, aPresContext, childDesiredSize,
                     childReflowState, aStatus);
    //NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
    if (NS_FAILED(rv)) return rv;

    // origins are used as placeholders to store the child's ascent and descent.
    childFrame->SetRect(aPresContext,
                        nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                               childDesiredSize.width, childDesiredSize.height));
    // compute and cache the bounding metrics
    if (0 == count)
      aDesiredSize.mBoundingMetrics  = childDesiredSize.mBoundingMetrics;
    else
      aDesiredSize.mBoundingMetrics += childDesiredSize.mBoundingMetrics;

    count++;
    childFrame->GetNextSibling(&childFrame);
  }

  if (aDesiredSize.mComputeMEW) {
    aDesiredSize.mMaxElementWidth = childDesiredSize.mMaxElementWidth;
  }

  // cache the frame's mBoundingMetrics
  mBoundingMetrics = aDesiredSize.mBoundingMetrics;

  // place and size children
  FinalizeReflow(aPresContext, *aReflowState.rendContext, aDesiredSize);
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

// For token elements, mBoundingMetrics is computed at the ReflowToken
// pass, it is not computed here because our children may be text frames
// that do not implement the GetBoundingMetrics() interface.
nsresult
nsMathMLTokenFrame::Place(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          PRBool               aPlaceOrigin,
                          nsHTMLReflowMetrics& aDesiredSize)
{
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;

  const nsStyleFont* font;
  GetStyleData(eStyleStruct_Font, (const nsStyleStruct*&)font);
  nsCOMPtr<nsIFontMetrics> fm;
  aPresContext->GetMetricsFor(font->mFont, getter_AddRefs(fm));
  nscoord ascent, descent;
  fm->GetMaxAscent(ascent);
  fm->GetMaxDescent(descent);

  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.ascent = PR_MAX(mBoundingMetrics.ascent, ascent);
  aDesiredSize.descent = PR_MAX(mBoundingMetrics.descent, descent);
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  if (aPlaceOrigin) {
    nscoord dy, dx = 0;
    nsRect rect;
    nsIFrame* childFrame;
    FirstChild(aPresContext, nsnull, &childFrame);
    while (childFrame) {
      childFrame->GetRect(rect);
      nsHTMLReflowMetrics childSize(nsnull);
      childSize.width = rect.width;
      childSize.height = aDesiredSize.height; //rect.height;

      // place and size the child
      dy = aDesiredSize.ascent - rect.y;
      FinishReflowChild(childFrame, aPresContext, nsnull, childSize, dx, dy, 0);
      dx += rect.width;
      childFrame->GetNextSibling(&childFrame);
    }
  }

  SetReference(nsPoint(0, aDesiredSize.ascent));

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLTokenFrame::ReflowDirtyChild(nsIPresShell* aPresShell,
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

NS_IMETHODIMP
nsMathMLTokenFrame::AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent*     aContent,
                                     PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aModType, 
                                     PRInt32         aHint)
{
  if (nsMathMLAtoms::lquote_ == aAttribute ||
      nsMathMLAtoms::rquote_ == aAttribute) {
    SetQuotes(aPresContext);
  }

  return nsMathMLContainerFrame::
         AttributeChanged(aPresContext, aContent, aNameSpaceID,
                          aAttribute, aModType, aHint);
}

void
nsMathMLTokenFrame::ProcessTextData(nsIPresContext* aPresContext)
{
  SetTextStyle(aPresContext);
}

///////////////////////////////////////////////////////////////////////////
// For <mi>, if the content is not a single character, turn the font to
// normal (this function will also query attributes from the mstyle hierarchy)

void
nsMathMLTokenFrame::SetTextStyle(nsIPresContext* aPresContext)
{
  nsCOMPtr<nsIAtom> tag;
  mContent->GetTag(*getter_AddRefs(tag));
  if (tag != nsMathMLAtoms::mi_)
    return;

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
    if (kidContent) {
      nsCOMPtr<nsIDOMText> kidText(do_QueryInterface(kidContent));
      if (kidText) {
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
    PRBool isStyleInvariant = nsMathMLOperators::LookupInvariantChar(data[0]);
    if (isStyleInvariant) {
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
      nsChangeHint maxChange, minChange = NS_STYLE_HINT_NONE;
      nsStyleChangeList changeList;
      fm->ComputeStyleChangeFor(aPresContext, this,
                                kNameSpaceID_None, nsMathMLAtoms::fontstyle,
                                changeList, minChange, maxChange);
#ifdef DEBUG
      // Use the parent frame to make sure we catch in-flows and such
      nsIFrame* parentFrame;
      GetParent(&parentFrame);
      fm->DebugVerifyStyleTree(aPresContext,
                               parentFrame ? parentFrame : this);
#endif
    }
  }
}

///////////////////////////////////////////////////////////////////////////
// For <ms>, it is assumed that the mathml.css file contains two rules:
// ms:before { content: open-quote; }
// ms:after { content: close-quote; }
// With these two rules, the frame construction code will
// create inline frames that contain text frames which themselves
// contain the text content of the quotes.
// So the main idea in this code is to see if there are lquote and 
// rquote attributes. If these are there, we ovewrite the default
// quotes in the text frames.
//
// But what if the mathml.css file wasn't loaded? 
// We also check that we are not relying on null pointers...

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
      if (frameType == nsLayoutAtoms::textFrame)
        break;
    }
    aFrame = textFrame;
  } while (textFrame);
  if (textFrame) {
    nsCOMPtr<nsIContent> quoteContent;
    textFrame->GetContent(getter_AddRefs(quoteContent));
    if (quoteContent) {
      nsCOMPtr<nsIDOMText> domText(do_QueryInterface(quoteContent));
      if (domText) {
        nsCOMPtr<nsITextContent> tc(do_QueryInterface(quoteContent));
        if (tc) {
          tc->SetText(aValue, PR_FALSE); // no notify since we don't want a reflow yet
        }
      }
    }
  }
}

void
nsMathMLTokenFrame::SetQuotes(nsIPresContext* aPresContext)
{
  nsCOMPtr<nsIAtom> tag;
  mContent->GetTag(*getter_AddRefs(tag));
  if (tag != nsMathMLAtoms::ms_)
    return;

  nsIFrame* rightFrame = nsnull;
  nsIFrame* baseFrame = nsnull;
  nsIFrame* leftFrame = mFrames.FirstChild();
  if (leftFrame)
    leftFrame->GetNextSibling(&baseFrame);
  if (baseFrame)
    baseFrame->GetNextSibling(&rightFrame);
  if (!leftFrame || !baseFrame || !rightFrame)
    return;

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
}
