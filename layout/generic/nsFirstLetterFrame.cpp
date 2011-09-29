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
#include "nsFirstLetterFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsIContent.h"
#include "nsLineLayout.h"
#include "nsGkAtoms.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsFrameManager.h"
#include "nsPlaceholderFrame.h"
#include "nsCSSFrameConstructor.h"

nsIFrame*
NS_NewFirstLetterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFirstLetterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsFirstLetterFrame)

NS_QUERYFRAME_HEAD(nsFirstLetterFrame)
  NS_QUERYFRAME_ENTRY(nsFirstLetterFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsFirstLetterFrameSuper)

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
nsFirstLetterFrame::SetInitialChildList(ChildListID  aListID,
                                        nsFrameList& aChildList)
{
  nsFrameManager *frameManager = PresContext()->FrameManager();

  for (nsFrameList::Enumerator e(aChildList); !e.AtEnd(); e.Next()) {
    NS_ASSERTION(e.get()->GetParent() == this, "Unexpected parent");
    frameManager->ReparentStyleContext(e.get());
  }

  mFrames.SetFrames(aChildList);
  return NS_OK;
}

NS_IMETHODIMP
nsFirstLetterFrame::GetChildFrameContainingOffset(PRInt32 inContentOffset,
                                                  bool inHint,
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
nsFirstLetterFrame::AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                                      nsIFrame::InlineMinWidthData *aData)
{
  DoInlineIntrinsicWidth(aRenderingContext, aData, nsLayoutUtils::MIN_WIDTH);
}

// Needed for non-floating first-letter frames and for the continuations
// following the first-letter that we also use nsFirstLetterFrame for.
/* virtual */ void
nsFirstLetterFrame::AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
                                       nsIFrame::InlinePrefWidthData *aData)
{
  DoInlineIntrinsicWidth(aRenderingContext, aData, nsLayoutUtils::PREF_WIDTH);
}

// Needed for floating first-letter frames.
/* virtual */ nscoord
nsFirstLetterFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  return nsLayoutUtils::MinWidthFromInline(this, aRenderingContext);
}

// Needed for floating first-letter frames.
/* virtual */ nscoord
nsFirstLetterFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  return nsLayoutUtils::PrefWidthFromInline(this, aRenderingContext);
}

/* virtual */ nsSize
nsFirstLetterFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                                nsSize aCBSize, nscoord aAvailableWidth,
                                nsSize aMargin, nsSize aBorder, nsSize aPadding,
                                bool aShrinkWrap)
{
  if (GetPrevInFlow()) {
    // We're wrapping the text *after* the first letter, so behave like an
    // inline frame.
    return nsSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }
  return nsFirstLetterFrameSuper::ComputeSize(aRenderingContext,
      aCBSize, aAvailableWidth, aMargin, aBorder, aPadding, aShrinkWrap);
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
  NS_ASSERTION(availSize.width != NS_UNCONSTRAINEDSIZE,
               "should no longer use unconstrained widths");
  availSize.width -= lr;
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
    ll.BeginLineReflow(bp.left, bp.top, availSize.width, NS_UNCONSTRAINEDSIZE,
                       PR_FALSE, PR_TRUE);
    rs.mLineLayout = &ll;
    ll.SetInFirstLetter(PR_TRUE);
    ll.SetFirstLetterStyleOK(PR_TRUE);

    kid->WillReflow(aPresContext);
    kid->Reflow(aPresContext, aMetrics, rs, aReflowStatus);

    ll.EndLineReflow();
    ll.SetInFirstLetter(PR_FALSE);

    // In the floating first-letter case, we need to set this ourselves;
    // nsLineLayout::BeginSpan will set it in the other case
    mBaseline = aMetrics.ascent;
  }
  else {
    // Pretend we are a span and reflow the child frame
    nsLineLayout* ll = aReflowState.mLineLayout;
    bool          pushedFrame;

    ll->SetInFirstLetter(
      mStyleContext->GetPseudo() == nsCSSPseudoElements::firstLetter);
    ll->BeginSpan(this, &aReflowState, bp.left, availSize.width, &mBaseline);
    ll->ReflowFrame(kid, aReflowStatus, &aMetrics, pushedFrame);
    ll->EndSpan(this);
    ll->SetInFirstLetter(PR_FALSE);
  }

  // Place and size the child and update the output metrics
  kid->SetRect(nsRect(bp.left, bp.top, aMetrics.width, aMetrics.height));
  kid->FinishAndStoreOverflow(&aMetrics);
  kid->DidReflow(aPresContext, nsnull, NS_FRAME_REFLOW_FINISHED);

  aMetrics.width += lr;
  aMetrics.height += tb;
  aMetrics.ascent += bp.top;

  // Ensure that the overflow rect contains the child textframe's overflow rect.
  // Note that if this is floating, the overline/underline drawable area is in
  // the overflow rect of the child textframe.
  aMetrics.UnionOverflowAreasWithDesiredBounds();
  ConsiderChildOverflow(aMetrics.mOverflowAreas, kid);

  if (!NS_INLINE_IS_BREAK_BEFORE(aReflowStatus)) {
    // Create a continuation or remove existing continuations based on
    // the reflow completion status.
    if (NS_FRAME_IS_COMPLETE(aReflowStatus)) {
      if (aReflowState.mLineLayout) {
        aReflowState.mLineLayout->SetFirstLetterStyleOK(PR_FALSE);
      }
      nsIFrame* kidNextInFlow = kid->GetNextInFlow();
      if (kidNextInFlow) {
        // Remove all of the childs next-in-flows
        static_cast<nsContainerFrame*>(kidNextInFlow->GetParent())
          ->DeleteNextInFlowChild(aPresContext, kidNextInFlow, PR_TRUE);
      }
    }
    else {
      // Create a continuation for the child frame if it doesn't already
      // have one.
      if (!GetStyleDisplay()->IsFloating()) {
        nsIFrame* nextInFlow;
        rv = CreateNextInFlow(aPresContext, kid, nextInFlow);
        if (NS_FAILED(rv)) {
          return rv;
        }
    
        // And then push it to our overflow list
        const nsFrameList& overflow = mFrames.RemoveFramesAfter(kid);
        if (overflow.NotEmpty()) {
          SetOverflowFrames(aPresContext, overflow);
        }
      } else if (!kid->GetNextInFlow()) {
        // For floating first letter frames (if a continuation wasn't already
        // created for us) we need to put the continuation with the rest of the
        // text that the first letter frame was made out of.
        nsIFrame* continuation;
        rv = CreateContinuationForFloatingParent(aPresContext, kid,
                                                 &continuation, PR_TRUE);
      }
    }
  }

  FinishAndStoreOverflow(&aMetrics);

  NS_FRAME_SET_TRUNCATION(aReflowStatus, aReflowState, aMetrics);
  return rv;
}

/* virtual */ bool
nsFirstLetterFrame::CanContinueTextRun() const
{
  // We can continue a text run through a first-letter frame.
  return PR_TRUE;
}

nsresult
nsFirstLetterFrame::CreateContinuationForFloatingParent(nsPresContext* aPresContext,
                                                        nsIFrame* aChild,
                                                        nsIFrame** aContinuation,
                                                        bool aIsFluid)
{
  NS_ASSERTION(GetStyleDisplay()->IsFloating(),
               "can only call this on floating first letter frames");
  NS_PRECONDITION(aContinuation, "bad args");

  *aContinuation = nsnull;
  nsresult rv = NS_OK;

  nsIPresShell* presShell = aPresContext->PresShell();
  nsPlaceholderFrame* placeholderFrame =
    presShell->FrameManager()->GetPlaceholderFrameFor(this);
  nsIFrame* parent = placeholderFrame->GetParent();

  nsIFrame* continuation;
  rv = presShell->FrameConstructor()->
    CreateContinuingFrame(aPresContext, aChild, parent, &continuation, aIsFluid);
  if (NS_FAILED(rv) || !continuation) {
    return rv;
  }

  // The continuation will have gotten the first letter style from it's
  // prev continuation, so we need to repair the style context so it
  // doesn't have the first letter styling.
  nsStyleContext* parentSC = this->GetStyleContext()->GetParent();
  if (parentSC) {
    nsRefPtr<nsStyleContext> newSC;
    newSC = presShell->StyleSet()->ResolveStyleForNonElement(parentSC);
    if (newSC) {
      continuation->SetStyleContext(newSC);
    }
  }

  //XXX Bidi may not be involved but we have to use the list name
  // kNoReflowPrincipalList because this is just like creating a continuation
  // except we have to insert it in a different place and we don't want a
  // reflow command to try to be issued.
  nsFrameList temp(continuation, continuation);
  rv = parent->InsertFrames(kNoReflowPrincipalList, placeholderFrame, temp);

  *aContinuation = continuation;
  return rv;
}

void
nsFirstLetterFrame::DrainOverflowFrames(nsPresContext* aPresContext)
{
  nsAutoPtr<nsFrameList> overflowFrames;

  // Check for an overflow list with our prev-in-flow
  nsFirstLetterFrame* prevInFlow = (nsFirstLetterFrame*)GetPrevInFlow();
  if (nsnull != prevInFlow) {
    overflowFrames = prevInFlow->StealOverflowFrames();
    if (overflowFrames) {
      NS_ASSERTION(mFrames.IsEmpty(), "bad overflow list");

      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      nsContainerFrame::ReparentFrameViewList(aPresContext, *overflowFrames,
                                              prevInFlow, this);
      mFrames.InsertFrames(this, nsnull, *overflowFrames);
    }
  }

  // It's also possible that we have an overflow list for ourselves
  overflowFrames = StealOverflowFrames();
  if (overflowFrames) {
    NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");
    mFrames.AppendFrames(nsnull, *overflowFrames);
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

nscoord
nsFirstLetterFrame::GetBaseline() const
{
  return mBaseline;
}
