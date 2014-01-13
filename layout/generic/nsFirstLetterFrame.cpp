/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS :first-letter pseudo-element */

#include "nsFirstLetterFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsIContent.h"
#include "nsLineLayout.h"
#include "nsGkAtoms.h"
#include "nsAutoPtr.h"
#include "nsStyleSet.h"
#include "nsFrameManager.h"
#include "RestyleManager.h"
#include "nsPlaceholderFrame.h"
#include "nsCSSFrameConstructor.h"

using namespace mozilla;
using namespace mozilla::layout;

nsIFrame*
NS_NewFirstLetterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFirstLetterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsFirstLetterFrame)

NS_QUERYFRAME_HEAD(nsFirstLetterFrame)
  NS_QUERYFRAME_ENTRY(nsFirstLetterFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

#ifdef DEBUG_FRAME_DUMP
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

void
nsFirstLetterFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  BuildDisplayListForInline(aBuilder, aDirtyRect, aLists);
}

void
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
      newSC = PresContext()->StyleSet()->
        ResolveStyleForNonElement(parentStyleContext);
      SetStyleContextWithoutNotification(newSC);
    }
  }

  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);
}

NS_IMETHODIMP
nsFirstLetterFrame::SetInitialChildList(ChildListID  aListID,
                                        nsFrameList& aChildList)
{
  RestyleManager* restyleManager = PresContext()->RestyleManager();

  for (nsFrameList::Enumerator e(aChildList); !e.AtEnd(); e.Next()) {
    NS_ASSERTION(e.get()->GetParent() == this, "Unexpected parent");
    restyleManager->ReparentStyleContext(e.get());
  }

  mFrames.SetFrames(aChildList);
  return NS_OK;
}

NS_IMETHODIMP
nsFirstLetterFrame::GetChildFrameContainingOffset(int32_t inContentOffset,
                                                  bool inHint,
                                                  int32_t* outFrameContentOffset,
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
                                uint32_t aFlags)
{
  if (GetPrevInFlow()) {
    // We're wrapping the text *after* the first letter, so behave like an
    // inline frame.
    return nsSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }
  return nsContainerFrame::ComputeSize(aRenderingContext,
      aCBSize, aAvailableWidth, aMargin, aBorder, aPadding, aFlags);
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
  nsSize availSize(aReflowState.AvailableWidth(), aReflowState.AvailableHeight());
  const nsMargin& bp = aReflowState.ComputedPhysicalBorderPadding();
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
    nsLineLayout ll(aPresContext, nullptr, &aReflowState, nullptr);

    // For unicode-bidi: plaintext, we need to get the direction of the line
    // from the resolved paragraph level of the child, not the block frame,
    // because the block frame could be split by hard line breaks into
    // multiple paragraphs with different base direction
    uint8_t direction;
    nsIFrame* containerFrame = ll.LineContainerFrame();
    if (containerFrame->StyleTextReset()->mUnicodeBidi &
        NS_STYLE_UNICODE_BIDI_PLAINTEXT) {
      FramePropertyTable *propTable = aPresContext->PropertyTable();
      direction = NS_PTR_TO_INT32(propTable->Get(kid, BaseLevelProperty())) & 1;
    } else {
      direction = containerFrame->StyleVisibility()->mDirection;
    }
    ll.BeginLineReflow(bp.left, bp.top, availSize.width, NS_UNCONSTRAINEDSIZE,
                       false, true, direction);
    rs.mLineLayout = &ll;
    ll.SetInFirstLetter(true);
    ll.SetFirstLetterStyleOK(true);

    kid->WillReflow(aPresContext);
    kid->Reflow(aPresContext, aMetrics, rs, aReflowStatus);

    ll.EndLineReflow();
    ll.SetInFirstLetter(false);

    // In the floating first-letter case, we need to set this ourselves;
    // nsLineLayout::BeginSpan will set it in the other case
    mBaseline = aMetrics.TopAscent();
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
    ll->SetInFirstLetter(false);
  }

  // Place and size the child and update the output metrics
  kid->SetRect(nsRect(bp.left, bp.top, aMetrics.Width(), aMetrics.Height()));
  kid->FinishAndStoreOverflow(&aMetrics);
  kid->DidReflow(aPresContext, nullptr, nsDidReflowStatus::FINISHED);

  aMetrics.Width() += lr;
  aMetrics.Height() += tb;
  aMetrics.SetTopAscent(aMetrics.TopAscent() + bp.top);

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
        aReflowState.mLineLayout->SetFirstLetterStyleOK(false);
      }
      nsIFrame* kidNextInFlow = kid->GetNextInFlow();
      if (kidNextInFlow) {
        // Remove all of the childs next-in-flows
        static_cast<nsContainerFrame*>(kidNextInFlow->GetParent())
          ->DeleteNextInFlowChild(aPresContext, kidNextInFlow, true);
      }
    }
    else {
      // Create a continuation for the child frame if it doesn't already
      // have one.
      if (!IsFloating()) {
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
                                                 &continuation, true);
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
  return true;
}

nsresult
nsFirstLetterFrame::CreateContinuationForFloatingParent(nsPresContext* aPresContext,
                                                        nsIFrame* aChild,
                                                        nsIFrame** aContinuation,
                                                        bool aIsFluid)
{
  NS_ASSERTION(IsFloating(),
               "can only call this on floating first letter frames");
  NS_PRECONDITION(aContinuation, "bad args");

  *aContinuation = nullptr;
  nsresult rv = NS_OK;

  nsIPresShell* presShell = aPresContext->PresShell();
  nsPlaceholderFrame* placeholderFrame =
    presShell->FrameManager()->GetPlaceholderFrameFor(this);
  nsIFrame* parent = placeholderFrame->GetParent();

  nsIFrame* continuation = presShell->FrameConstructor()->
    CreateContinuingFrame(aPresContext, aChild, parent, aIsFluid);

  // The continuation will have gotten the first letter style from its
  // prev continuation, so we need to repair the style context so it
  // doesn't have the first letter styling.
  nsStyleContext* parentSC = this->StyleContext()->GetParent();
  if (parentSC) {
    nsRefPtr<nsStyleContext> newSC;
    newSC = presShell->StyleSet()->ResolveStyleForNonElement(parentSC);
    continuation->SetStyleContext(newSC);
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
  // Check for an overflow list with our prev-in-flow
  nsFirstLetterFrame* prevInFlow = (nsFirstLetterFrame*)GetPrevInFlow();
  if (prevInFlow) {
    AutoFrameListPtr overflowFrames(aPresContext,
                                    prevInFlow->StealOverflowFrames());
    if (overflowFrames) {
      NS_ASSERTION(mFrames.IsEmpty(), "bad overflow list");

      // When pushing and pulling frames we need to check for whether any
      // views need to be reparented.
      nsContainerFrame::ReparentFrameViewList(aPresContext, *overflowFrames,
                                              prevInFlow, this);
      mFrames.InsertFrames(this, nullptr, *overflowFrames);
    }
  }

  // It's also possible that we have an overflow list for ourselves
  AutoFrameListPtr overflowFrames(aPresContext, StealOverflowFrames());
  if (overflowFrames) {
    NS_ASSERTION(mFrames.NotEmpty(), "overflow list w/o frames");
    mFrames.AppendFrames(nullptr, *overflowFrames);
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
      nsStyleContext* parentSC = prevInFlow ? mStyleContext->GetParent() :
                                              mStyleContext;
      sc = aPresContext->StyleSet()->ResolveStyleForNonElement(parentSC);
      kid->SetStyleContext(sc);
    }
  }
}

nscoord
nsFirstLetterFrame::GetBaseline() const
{
  return mBaseline;
}

int
nsFirstLetterFrame::GetSkipSides(const nsHTMLReflowState* aReflowState) const
{
  if (GetPrevContinuation()) {
    // We shouldn't get calls to GetSkipSides for later continuations since
    // they have separate style contexts with initial values for all the
    // properties that could trigger a call to GetSkipSides.  Then again,
    // it's not really an error to call GetSkipSides on any frame, so
    // that's why we handle it properly.
    return 1 << NS_SIDE_LEFT |
           1 << NS_SIDE_RIGHT |
           1 << NS_SIDE_TOP |
           1 << NS_SIDE_BOTTOM;
  }
  return 0;  // first continuation displays all sides
}
