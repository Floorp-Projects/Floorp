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
#include "mozilla/StyleSetHandle.h"
#include "mozilla/StyleSetHandleInlines.h"
#include "nsFrameManager.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/RestyleManagerInlines.h"
#include "nsPlaceholderFrame.h"
#include "nsCSSFrameConstructor.h"

using namespace mozilla;
using namespace mozilla::layout;

nsFirstLetterFrame*
NS_NewFirstLetterFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsFirstLetterFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsFirstLetterFrame)

NS_QUERYFRAME_HEAD(nsFirstLetterFrame)
  NS_QUERYFRAME_ENTRY(nsFirstLetterFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult
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
nsFirstLetterFrame::Init(nsIContent*       aContent,
                         nsContainerFrame* aParent,
                         nsIFrame*         aPrevInFlow)
{
  RefPtr<nsStyleContext> newSC;
  if (aPrevInFlow) {
    // Get proper style context for ourselves.  We're creating the frame
    // that represents everything *except* the first letter, so just create
    // a style context like we would for a text node.
    nsStyleContext* parentStyleContext = mStyleContext->GetParentAllowServo();
    if (parentStyleContext) {
      newSC = PresContext()->StyleSet()->
        ResolveStyleForFirstLetterContinuation(parentStyleContext);
      SetStyleContextWithoutNotification(newSC);
    }
  }

  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);
}

void
nsFirstLetterFrame::SetInitialChildList(ChildListID  aListID,
                                        nsFrameList& aChildList)
{
  MOZ_ASSERT(aListID == kPrincipalList, "Principal child list is the only "
             "list that nsFirstLetterFrame should set via this function");
  RestyleManager* restyleManager = PresContext()->RestyleManager();

  for (nsFrameList::Enumerator e(aChildList); !e.AtEnd(); e.Next()) {
    NS_ASSERTION(e.get()->GetParent() == this, "Unexpected parent");
    restyleManager->ReparentStyleContext(e.get());
    nsLayoutUtils::MarkDescendantsDirty(e.get());
  }

  mFrames.SetFrames(aChildList);
}

nsresult
nsFirstLetterFrame::GetChildFrameContainingOffset(int32_t inContentOffset,
                                                  bool inHint,
                                                  int32_t* outFrameContentOffset,
                                                  nsIFrame **outChildFrame)
{
  nsIFrame *kid = mFrames.FirstChild();
  if (kid) {
    return kid->GetChildFrameContainingOffset(inContentOffset, inHint, outFrameContentOffset, outChildFrame);
  } else {
    return nsFrame::GetChildFrameContainingOffset(inContentOffset, inHint, outFrameContentOffset, outChildFrame);
  }
}

// Needed for non-floating first-letter frames and for the continuations
// following the first-letter that we also use nsFirstLetterFrame for.
/* virtual */ void
nsFirstLetterFrame::AddInlineMinISize(nsRenderingContext *aRenderingContext,
                                      nsIFrame::InlineMinISizeData *aData)
{
  DoInlineIntrinsicISize(aRenderingContext, aData, nsLayoutUtils::MIN_ISIZE);
}

// Needed for non-floating first-letter frames and for the continuations
// following the first-letter that we also use nsFirstLetterFrame for.
/* virtual */ void
nsFirstLetterFrame::AddInlinePrefISize(nsRenderingContext *aRenderingContext,
                                       nsIFrame::InlinePrefISizeData *aData)
{
  DoInlineIntrinsicISize(aRenderingContext, aData, nsLayoutUtils::PREF_ISIZE);
  aData->mLineIsEmpty = false;
}

// Needed for floating first-letter frames.
/* virtual */ nscoord
nsFirstLetterFrame::GetMinISize(nsRenderingContext *aRenderingContext)
{
  return nsLayoutUtils::MinISizeFromInline(this, aRenderingContext);
}

// Needed for floating first-letter frames.
/* virtual */ nscoord
nsFirstLetterFrame::GetPrefISize(nsRenderingContext *aRenderingContext)
{
  return nsLayoutUtils::PrefISizeFromInline(this, aRenderingContext);
}

/* virtual */
LogicalSize
nsFirstLetterFrame::ComputeSize(nsRenderingContext *aRenderingContext,
                                WritingMode aWM,
                                const LogicalSize& aCBSize,
                                nscoord aAvailableISize,
                                const LogicalSize& aMargin,
                                const LogicalSize& aBorder,
                                const LogicalSize& aPadding,
                                ComputeSizeFlags aFlags)
{
  if (GetPrevInFlow()) {
    // We're wrapping the text *after* the first letter, so behave like an
    // inline frame.
    return LogicalSize(aWM, NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  }
  return nsContainerFrame::ComputeSize(aRenderingContext, aWM,
      aCBSize, aAvailableISize, aMargin, aBorder, aPadding, aFlags);
}

void
nsFirstLetterFrame::Reflow(nsPresContext*          aPresContext,
                           ReflowOutput&     aMetrics,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus&          aReflowStatus)
{
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsFirstLetterFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aMetrics, aReflowStatus);

  // Grab overflow list
  DrainOverflowFrames(aPresContext);

  nsIFrame* kid = mFrames.FirstChild();

  // Setup reflow state for our child
  WritingMode wm = aReflowInput.GetWritingMode();
  LogicalSize availSize = aReflowInput.AvailableSize();
  const LogicalMargin& bp = aReflowInput.ComputedLogicalBorderPadding();
  NS_ASSERTION(availSize.ISize(wm) != NS_UNCONSTRAINEDSIZE,
               "should no longer use unconstrained inline size");
  availSize.ISize(wm) -= bp.IStartEnd(wm);
  if (NS_UNCONSTRAINEDSIZE != availSize.BSize(wm)) {
    availSize.BSize(wm) -= bp.BStartEnd(wm);
  }

  WritingMode lineWM = aMetrics.GetWritingMode();
  ReflowOutput kidMetrics(lineWM);

  // Reflow the child
  if (!aReflowInput.mLineLayout) {
    // When there is no lineLayout provided, we provide our own. The
    // only time that the first-letter-frame is not reflowing in a
    // line context is when its floating.
    WritingMode kidWritingMode = WritingModeForLine(wm, kid);
    LogicalSize kidAvailSize = availSize.ConvertTo(kidWritingMode, wm);
    ReflowInput rs(aPresContext, aReflowInput, kid, kidAvailSize);
    nsLineLayout ll(aPresContext, nullptr, &aReflowInput, nullptr, nullptr);

    ll.BeginLineReflow(bp.IStart(wm), bp.BStart(wm),
                       availSize.ISize(wm), NS_UNCONSTRAINEDSIZE,
                       false, true, kidWritingMode,
                       nsSize(aReflowInput.AvailableWidth(),
                              aReflowInput.AvailableHeight()));
    rs.mLineLayout = &ll;
    ll.SetInFirstLetter(true);
    ll.SetFirstLetterStyleOK(true);

    kid->Reflow(aPresContext, kidMetrics, rs, aReflowStatus);

    ll.EndLineReflow();
    ll.SetInFirstLetter(false);

    // In the floating first-letter case, we need to set this ourselves;
    // nsLineLayout::BeginSpan will set it in the other case
    mBaseline = kidMetrics.BlockStartAscent();

    // Place and size the child and update the output metrics
    LogicalSize convertedSize = kidMetrics.Size(lineWM).ConvertTo(wm, lineWM);
    kid->SetRect(nsRect(bp.IStart(wm), bp.BStart(wm),
                        convertedSize.ISize(wm), convertedSize.BSize(wm)));
    kid->FinishAndStoreOverflow(&kidMetrics);
    kid->DidReflow(aPresContext, nullptr, nsDidReflowStatus::FINISHED);

    convertedSize.ISize(wm) += bp.IStartEnd(wm);
    convertedSize.BSize(wm) += bp.BStartEnd(wm);
    aMetrics.SetSize(wm, convertedSize);
    aMetrics.SetBlockStartAscent(kidMetrics.BlockStartAscent() +
                                 bp.BStart(wm));

    // Ensure that the overflow rect contains the child textframe's
    // overflow rect.
    // Note that if this is floating, the overline/underline drawable
    // area is in the overflow rect of the child textframe.
    aMetrics.UnionOverflowAreasWithDesiredBounds();
    ConsiderChildOverflow(aMetrics.mOverflowAreas, kid);

    FinishAndStoreOverflow(&aMetrics);
  } else {
    // Pretend we are a span and reflow the child frame
    nsLineLayout* ll = aReflowInput.mLineLayout;
    bool          pushedFrame;

    ll->SetInFirstLetter(
      mStyleContext->GetPseudo() == nsCSSPseudoElements::firstLetter);
    ll->BeginSpan(this, &aReflowInput, bp.IStart(wm),
                  availSize.ISize(wm), &mBaseline);
    ll->ReflowFrame(kid, aReflowStatus, &kidMetrics, pushedFrame);
    NS_ASSERTION(lineWM.IsVertical() == wm.IsVertical(),
                 "we're assuming we can mix sizes between lineWM and wm "
                 "since we shouldn't have orthogonal writing modes within "
                 "a line.");
    aMetrics.ISize(lineWM) = ll->EndSpan(this) + bp.IStartEnd(wm);
    ll->SetInFirstLetter(false);

    if (mStyleContext->StyleTextReset()->mInitialLetterSize != 0.0f) {
      aMetrics.SetBlockStartAscent(kidMetrics.BlockStartAscent() +
                                   bp.BStart(wm));
      aMetrics.BSize(lineWM) = kidMetrics.BSize(lineWM) + bp.BStartEnd(wm);
    } else {
      nsLayoutUtils::SetBSizeFromFontMetrics(this, aMetrics, bp, lineWM, wm);
    }
  }

  if (!aReflowStatus.IsInlineBreakBefore()) {
    // Create a continuation or remove existing continuations based on
    // the reflow completion status.
    if (aReflowStatus.IsComplete()) {
      if (aReflowInput.mLineLayout) {
        aReflowInput.mLineLayout->SetFirstLetterStyleOK(false);
      }
      nsIFrame* kidNextInFlow = kid->GetNextInFlow();
      if (kidNextInFlow) {
        // Remove all of the childs next-in-flows
        kidNextInFlow->GetParent()->DeleteNextInFlowChild(kidNextInFlow, true);
      }
    } else {
      // Create a continuation for the child frame if it doesn't already
      // have one.
      if (!IsFloating()) {
        CreateNextInFlow(kid);
        // And then push it to our overflow list
        const nsFrameList& overflow = mFrames.RemoveFramesAfter(kid);
        if (overflow.NotEmpty()) {
          SetOverflowFrames(overflow);
        }
      } else if (!kid->GetNextInFlow()) {
        // For floating first letter frames (if a continuation wasn't already
        // created for us) we need to put the continuation with the rest of the
        // text that the first letter frame was made out of.
        nsIFrame* continuation;
        CreateContinuationForFloatingParent(aPresContext, kid,
                                            &continuation, true);
      }
    }
  }

  NS_FRAME_SET_TRUNCATION(aReflowStatus, aReflowInput, aMetrics);
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

  nsIPresShell* presShell = aPresContext->PresShell();
  nsPlaceholderFrame* placeholderFrame =
    presShell->FrameManager()->GetPlaceholderFrameFor(this);
  nsContainerFrame* parent = placeholderFrame->GetParent();

  nsIFrame* continuation = presShell->FrameConstructor()->
    CreateContinuingFrame(aPresContext, aChild, parent, aIsFluid);

  // The continuation will have gotten the first letter style from its
  // prev continuation, so we need to repair the style context so it
  // doesn't have the first letter styling.
  nsStyleContext* parentSC = this->StyleContext()->GetParentAllowServo();
  if (parentSC) {
    RefPtr<nsStyleContext> newSC;
    newSC = presShell->StyleSet()->ResolveStyleForFirstLetterContinuation(parentSC);
    continuation->SetStyleContext(newSC);
    nsLayoutUtils::MarkDescendantsDirty(continuation);
  }

  //XXX Bidi may not be involved but we have to use the list name
  // kNoReflowPrincipalList because this is just like creating a continuation
  // except we have to insert it in a different place and we don't want a
  // reflow command to try to be issued.
  nsFrameList temp(continuation, continuation);
  parent->InsertFrames(kNoReflowPrincipalList, placeholderFrame, temp);

  *aContinuation = continuation;
  return NS_OK;
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
      nsContainerFrame::ReparentFrameViewList(*overflowFrames, prevInFlow,
                                              this);
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
    RefPtr<nsStyleContext> sc;
    nsIContent* kidContent = kid->GetContent();
    if (kidContent) {
      NS_ASSERTION(kidContent->IsNodeOfType(nsINode::eTEXT),
                   "should contain only text nodes");
      nsStyleContext* parentSC = prevInFlow ? mStyleContext->GetParentAllowServo() :
                                              mStyleContext;
      sc = aPresContext->StyleSet()->ResolveStyleForText(kidContent, parentSC);
      kid->SetStyleContext(sc);
      nsLayoutUtils::MarkDescendantsDirty(kid);
    }
  }
}

nscoord
nsFirstLetterFrame::GetLogicalBaseline(WritingMode aWritingMode) const
{
  return mBaseline;
}

nsIFrame::LogicalSides
nsFirstLetterFrame::GetLogicalSkipSides(const ReflowInput* aReflowInput) const
{
  if (GetPrevContinuation()) {
    // We shouldn't get calls to GetSkipSides for later continuations since
    // they have separate style contexts with initial values for all the
    // properties that could trigger a call to GetSkipSides.  Then again,
    // it's not really an error to call GetSkipSides on any frame, so
    // that's why we handle it properly.
    return LogicalSides(eLogicalSideBitsAll);
  }
  return LogicalSides();  // first continuation displays all sides
}
