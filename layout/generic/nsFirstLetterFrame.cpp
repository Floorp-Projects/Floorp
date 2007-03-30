/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* rendering object for CSS :first-letter pseudo-element */

#include "nsCOMPtr.h"
#include "nsHTMLContainerFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsIContent.h"
#include "nsLineLayout.h"
#include "nsGkAtoms.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsFrameManager.h"

#define nsFirstLetterFrameSuper nsHTMLContainerFrame

class nsFirstLetterFrame : public nsFirstLetterFrameSuper {
public:
  nsFirstLetterFrame(nsStyleContext* aContext) : nsHTMLContainerFrame(aContext) {}

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD SetInitialChildList(nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
#ifdef NS_DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif
  virtual nsIAtom* GetType() const;

  virtual PRBool IsFrameOfType(PRUint32 aFlags) const
  {
    return nsFirstLetterFrameSuper::IsFrameOfType(aFlags &
      ~(nsIFrame::eBidiInlineContainer));
  }

  virtual nscoord GetMinWidth(nsIRenderingContext *aRenderingContext);
  virtual nscoord GetPrefWidth(nsIRenderingContext *aRenderingContext);
  virtual void AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                                 InlineMinWidthData *aData);
  virtual void AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                                  InlinePrefWidthData *aData);
  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual PRBool CanContinueTextRun() const;

  NS_IMETHOD SetSelected(nsPresContext* aPresContext, nsIDOMRange *aRange,PRBool aSelected, nsSpread aSpread);

//override of nsFrame method
  NS_IMETHOD GetChildFrameContainingOffset(PRInt32 inContentOffset,
                                           PRBool inHint,
                                           PRInt32* outFrameContentOffset,
                                           nsIFrame **outChildFrame);

protected:
  virtual PRIntn GetSkipSides() const;

  void DrainOverflowFrames(nsPresContext* aPresContext);
};

nsIFrame*
NS_NewFirstLetterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFirstLetterFrame(aContext);
}

#ifdef NS_DEBUG
NS_IMETHODIMP
nsFirstLetterFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Letter"), aResult);
}
#endif

nsIAtom*
nsFirstLetterFrame::GetType() const
{
  return nsGkAtoms::letterFrame;
}

PRIntn
nsFirstLetterFrame::GetSkipSides() const
{
  return 0;
}

NS_IMETHODIMP
nsFirstLetterFrame::Init(nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIFrame*        aPrevInFlow)
{
  nsRefPtr<nsStyleContext> newSC;
  if (aPrevInFlow) {
    // Get proper style context for ourselves.  We're creating the frame
    // that represents everything *except* the first letter, so just create
    // a style context like we would for a text node.
    nsStyleContext* parentStyleContext = mStyleContext->GetParent();
    if (parentStyleContext) {
      newSC = mStyleContext->GetRuleNode()->GetPresContext()->StyleSet()->
        ResolveStyleForNonElement(parentStyleContext);
      if (newSC)
        SetStyleContextWithoutNotification(newSC);
    }
  }

  return nsFirstLetterFrameSuper::Init(aContent, aParent, aPrevInFlow);
}

NS_IMETHODIMP
nsFirstLetterFrame::SetInitialChildList(nsIAtom*  aListName,
                                        nsIFrame* aChildList)
{
  mFrames.SetFrames(aChildList);
  nsFrameManager *frameManager = PresContext()->FrameManager();

  for (nsIFrame* frame = aChildList; frame; frame = frame->GetNextSibling()) {
    NS_ASSERTION(frame->GetParent() == this, "Unexpected parent");
    frameManager->ReParentStyleContext(frame);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFirstLetterFrame::SetSelected(nsPresContext* aPresContext, nsIDOMRange *aRange,PRBool aSelected, nsSpread aSpread)
{
  if (aSelected && ParentDisablesSelection())
    return NS_OK;
  nsIFrame *child = GetFirstChild(nsnull);
  while (child)
  {
    child->SetSelected(aPresContext, aRange, aSelected, aSpread);
    // don't worry about result. there are more frames to come
    child = child->GetNextSibling();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFirstLetterFrame::GetChildFrameContainingOffset(PRInt32 inContentOffset,
                                                  PRBool inHint,
                                                  PRInt32* outFrameContentOffset,
                                                  nsIFrame **outChildFrame)
{
  nsIFrame *kid = mFrames.FirstChild();
  if (kid)
  {
    return kid->GetChildFrameContainingOffset(inContentOffset, inHint, outFrameContentOffset, outChildFrame);
  }
  else
    return nsFrame::GetChildFrameContainingOffset(inContentOffset, inHint, outFrameContentOffset, outChildFrame);
}

// Needed for non-floating first-letter frames and for the continuations
// following the first-letter that we also use nsFirstLetterFrame for.
/* virtual */ void
nsFirstLetterFrame::AddInlineMinWidth(nsIRenderingContext *aRenderingContext,
                                      nsIFrame::InlineMinWidthData *aData)
{
  DoInlineIntrinsicWidth(aRenderingContext, aData, nsLayoutUtils::MIN_WIDTH);
}

// Needed for non-floating first-letter frames and for the continuations
// following the first-letter that we also use nsFirstLetterFrame for.
/* virtual */ void
nsFirstLetterFrame::AddInlinePrefWidth(nsIRenderingContext *aRenderingContext,
                                       nsIFrame::InlinePrefWidthData *aData)
{
  DoInlineIntrinsicWidth(aRenderingContext, aData, nsLayoutUtils::PREF_WIDTH);
}

// Needed for floating first-letter frames.
/* virtual */ nscoord
nsFirstLetterFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  return nsLayoutUtils::MinWidthFromInline(this, aRenderingContext);
}

// Needed for floating first-letter frames.
/* virtual */ nscoord
nsFirstLetterFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  return nsLayoutUtils::PrefWidthFromInline(this, aRenderingContext);
}

NS_IMETHODIMP
nsFirstLetterFrame::Reflow(nsPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aMetrics,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aReflowStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsFirstLetterFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aReflowStatus);
  nsresult rv = NS_OK;

  // Grab overflow list
  DrainOverflowFrames(aPresContext);

  nsIFrame* kid = mFrames.FirstChild();

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
    // When there is no lineLayout provided, we provide our own. The
    // only time that the first-letter-frame is not reflowing in a
    // line context is when its floating.
    nsHTMLReflowState rs(aPresContext, aReflowState, kid, availSize);
    nsLineLayout ll(aPresContext, nsnull, &aReflowState, nsnull);
    ll.BeginLineReflow(0, 0, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE,
                       PR_FALSE, PR_TRUE);
    rs.mLineLayout = &ll;
    ll.SetFirstLetterStyleOK(PR_TRUE);

    kid->WillReflow(aPresContext);
    kid->Reflow(aPresContext, aMetrics, rs, aReflowStatus);

    ll.EndLineReflow();
  }
  else {
    // Pretend we are a span and reflow the child frame
    nsLineLayout* ll = aReflowState.mLineLayout;
    PRBool        pushedFrame;

    ll->BeginSpan(this, &aReflowState, bp.left, availSize.width);
    ll->ReflowFrame(kid, aReflowStatus, &aMetrics, pushedFrame);
    nsSize size;
    ll->EndSpan(this, size);
  }

  // Place and size the child and update the output metrics
  kid->SetRect(nsRect(bp.left, bp.top, aMetrics.width, aMetrics.height));
  kid->DidReflow(aPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);
  aMetrics.width += lr;
  aMetrics.height += tb;
  aMetrics.ascent += bp.top;

  // Create a continuation or remove existing continuations based on
  // the reflow completion status.
  if (NS_FRAME_IS_COMPLETE(aReflowStatus)) {
    nsIFrame* kidNextInFlow = kid->GetNextInFlow();
    if (kidNextInFlow) {
      // Remove all of the childs next-in-flows
      NS_STATIC_CAST(nsContainerFrame*, kidNextInFlow->GetParent())
        ->DeleteNextInFlowChild(aPresContext, kidNextInFlow);
    }
  }
  else {
    // Create a continuation for the child frame if it doesn't already
    // have one.
    nsIFrame* nextInFlow;
    rv = CreateNextInFlow(aPresContext, this, kid, nextInFlow);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // And then push it to our overflow list
    if (nextInFlow) {
      kid->SetNextSibling(nsnull);
      SetOverflowFrames(aPresContext, nextInFlow);
    }
    else {
      nsIFrame* nextSib = kid->GetNextSibling();
      if (nextSib) {
        kid->SetNextSibling(nsnull);
        SetOverflowFrames(aPresContext, nextSib);
      }
    }
  }

  NS_FRAME_SET_TRUNCATION(aReflowStatus, aReflowState, aMetrics);
  return rv;
}

/* virtual */ PRBool
nsFirstLetterFrame::CanContinueTextRun() const
{
  // We can continue a text run through a first-letter frame.
  return PR_TRUE;
}

void
nsFirstLetterFrame::DrainOverflowFrames(nsPresContext* aPresContext)
{
  nsIFrame* overflowFrames;

  // Check for an overflow list with our prev-in-flow
  nsFirstLetterFrame* prevInFlow = (nsFirstLetterFrame*)GetPrevInFlow();
  if (nsnull != prevInFlow) {
    overflowFrames = prevInFlow->GetOverflowFrames(aPresContext, PR_TRUE);
    if (overflowFrames) {
      NS_ASSERTION(mFrames.IsEmpty(), "bad overflow list");

      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      nsIFrame* f = overflowFrames;
      while (f) {
        nsHTMLContainerFrame::ReparentFrameView(aPresContext, f, prevInFlow, this);
        f = f->GetNextSibling();
      }
      mFrames.InsertFrames(this, nsnull, overflowFrames);
    }
  }

  // It's also possible that we have an overflow list for ourselves
  overflowFrames = GetOverflowFrames(aPresContext, PR_TRUE);
  if (overflowFrames) {
    NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");
    mFrames.AppendFrames(nsnull, overflowFrames);
  }

  // Now repair our first frames style context (since we only reflow
  // one frame there is no point in doing any other ones until they
  // are reflowed)
  nsIFrame* kid = mFrames.FirstChild();
  if (kid) {
    nsRefPtr<nsStyleContext> sc;
    nsIContent* kidContent = kid->GetContent();
    if (kidContent) {
      NS_ASSERTION(kidContent->IsNodeOfType(nsINode::eTEXT),
                   "should contain only text nodes");
      sc = aPresContext->StyleSet()->ResolveStyleForNonElement(mStyleContext);
      if (sc) {
        kid->SetStyleContext(sc);
      }
    }
  }
}
