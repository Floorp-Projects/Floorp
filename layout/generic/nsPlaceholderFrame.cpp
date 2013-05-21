/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object for the point that anchors out-of-flow rendering
 * objects such as floats and absolutely positioned elements
 */

#include "nsPlaceholderFrame.h"

#include "nsDisplayList.h"
#include "nsFrameManager.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"

nsIFrame*
NS_NewPlaceholderFrame(nsIPresShell* aPresShell, nsStyleContext* aContext,
                       nsFrameState aTypeBit)
{
  return new (aPresShell) nsPlaceholderFrame(aContext, aTypeBit);
}

NS_IMPL_FRAMEARENA_HELPERS(nsPlaceholderFrame)

/* virtual */ nsSize
nsPlaceholderFrame::GetMinSize(nsBoxLayoutState& aBoxLayoutState)
{
  nsSize size(0, 0);
  DISPLAY_MIN_SIZE(this, size);
  return size;
}

/* virtual */ nsSize
nsPlaceholderFrame::GetPrefSize(nsBoxLayoutState& aBoxLayoutState)
{
  nsSize size(0, 0);
  DISPLAY_PREF_SIZE(this, size);
  return size;
}

/* virtual */ nsSize
nsPlaceholderFrame::GetMaxSize(nsBoxLayoutState& aBoxLayoutState)
{
  nsSize size(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  DISPLAY_MAX_SIZE(this, size);
  return size;
}

/* virtual */ void
nsPlaceholderFrame::AddInlineMinWidth(nsRenderingContext* aRenderingContext,
                                      nsIFrame::InlineMinWidthData* aData)
{
  // Override AddInlineMinWith so that *nothing* happens.  In
  // particular, we don't want to zero out |aData->trailingWhitespace|,
  // since nsLineLayout skips placeholders when trimming trailing
  // whitespace, and we don't want to set aData->skipWhitespace to
  // false.

  // ...but push floats onto the list
  if (mOutOfFlowFrame->IsFloating()) {
    nscoord floatWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                           mOutOfFlowFrame,
                                           nsLayoutUtils::MIN_WIDTH);
    aData->floats.AppendElement(
      InlineIntrinsicWidthData::FloatInfo(mOutOfFlowFrame, floatWidth));
  }
}

/* virtual */ void
nsPlaceholderFrame::AddInlinePrefWidth(nsRenderingContext* aRenderingContext,
                                       nsIFrame::InlinePrefWidthData* aData)
{
  // Override AddInlinePrefWith so that *nothing* happens.  In
  // particular, we don't want to zero out |aData->trailingWhitespace|,
  // since nsLineLayout skips placeholders when trimming trailing
  // whitespace, and we don't want to set aData->skipWhitespace to
  // false.

  // ...but push floats onto the list
  if (mOutOfFlowFrame->IsFloating()) {
    nscoord floatWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                           mOutOfFlowFrame,
                                           nsLayoutUtils::PREF_WIDTH);
    aData->floats.AppendElement(
      InlineIntrinsicWidthData::FloatInfo(mOutOfFlowFrame, floatWidth));
  }
}

NS_IMETHODIMP
nsPlaceholderFrame::Reflow(nsPresContext*           aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
#ifdef DEBUG
  // We should be getting reflowed before our out-of-flow.
  // If this is our first reflow, and our out-of-flow has already received its
  // first reflow (before us), complain.
  if ((GetStateBits() & NS_FRAME_FIRST_REFLOW) &&
      !(mOutOfFlowFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {

    // Unfortunately, this can currently happen when the placeholder is in a
    // later continuation than its out-of-flow (as is the case in some unit
    // tests). So for now, in that case, we'll warn instead of asserting.
    bool isInContinuation = false;
    nsIFrame* ancestor = this;
    while ((ancestor = ancestor->GetParent())) {
      if (ancestor->GetPrevContinuation()) {
        isInContinuation = true;
        break;
      }
    }

    if (isInContinuation) {
      NS_WARNING("Out-of-flow frame got reflowed before its placeholder");
    } else {
      NS_ERROR("Out-of-flow frame got reflowed before its placeholder");
    }
  }
#endif

  DO_GLOBAL_REFLOW_COUNT("nsPlaceholderFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  aDesiredSize.width = 0;
  aDesiredSize.height = 0;

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

void
nsPlaceholderFrame::DestroyFrom(nsIFrame* aDestructRoot)
{
  nsIFrame* oof = mOutOfFlowFrame;
  if (oof) {
    // Unregister out-of-flow frame
    nsFrameManager* fm = PresContext()->GetPresShell()->FrameManager();
    fm->UnregisterPlaceholderFrame(this);
    mOutOfFlowFrame = nullptr;
    // If aDestructRoot is not an ancestor of the out-of-flow frame,
    // then call RemoveFrame on it here.
    // Also destroy it here if it's a popup frame. (Bug 96291)
    if ((GetStateBits() & PLACEHOLDER_FOR_POPUP) ||
        !nsLayoutUtils::IsProperAncestorFrame(aDestructRoot, oof)) {
      ChildListID listId = nsLayoutUtils::GetChildListNameFor(oof);
      fm->RemoveFrame(listId, oof);
    }
    // else oof will be destroyed by its parent
  }

  nsFrame::DestroyFrom(aDestructRoot);
}

nsIAtom*
nsPlaceholderFrame::GetType() const
{
  return nsGkAtoms::placeholderFrame;
}

/* virtual */ bool
nsPlaceholderFrame::CanContinueTextRun() const
{
  if (!mOutOfFlowFrame) {
    return false;
  }
  // first-letter frames can continue text runs, and placeholders for floated
  // first-letter frames can too
  return mOutOfFlowFrame->CanContinueTextRun();
}

nsIFrame*
nsPlaceholderFrame::GetParentStyleContextFrame() const
{
  NS_PRECONDITION(GetParent(), "How can we not have a parent here?");

  // Lie about our pseudo so we can step out of all anon boxes and
  // pseudo-elements.  The other option would be to reimplement the
  // {ib} split gunk here.
  return CorrectStyleParentFrame(GetParent(), nsGkAtoms::placeholderFrame);
}


#ifdef DEBUG
static void
PaintDebugPlaceholder(nsIFrame* aFrame, nsRenderingContext* aCtx,
                      const nsRect& aDirtyRect, nsPoint aPt)
{
  aCtx->SetColor(NS_RGB(0, 255, 255));
  nscoord x = nsPresContext::CSSPixelsToAppUnits(-5);
  aCtx->FillRect(aPt.x + x, aPt.y,
                 nsPresContext::CSSPixelsToAppUnits(13),
                 nsPresContext::CSSPixelsToAppUnits(3));
  nscoord y = nsPresContext::CSSPixelsToAppUnits(-10);
  aCtx->FillRect(aPt.x, aPt.y + y,
                 nsPresContext::CSSPixelsToAppUnits(3),
                 nsPresContext::CSSPixelsToAppUnits(10));
}
#endif // DEBUG

#if defined(DEBUG) || (defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF))

void
nsPlaceholderFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  DO_GLOBAL_REFLOW_COUNT_DSP("nsPlaceholderFrame");
  
#ifdef DEBUG
  if (GetShowFrameBorders()) {
    aLists.Outlines()->AppendNewToTop(
      new (aBuilder) nsDisplayGeneric(aBuilder, this, PaintDebugPlaceholder,
                                      "DebugPlaceholder",
                                      nsDisplayItem::TYPE_DEBUG_PLACEHOLDER));
  }
#endif
}
#endif // DEBUG || (MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF)

#ifdef DEBUG
NS_IMETHODIMP
nsPlaceholderFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Placeholder"), aResult);
}

void
nsPlaceholderFrame::List(FILE* out, int32_t aIndent, uint32_t aFlags) const
{
  ListGeneric(out, aIndent, aFlags);

  if (mOutOfFlowFrame) {
    fprintf(out, " outOfFlowFrame=");
    nsFrame::ListTag(out, mOutOfFlowFrame);
  }
  fputs("\n", out);
}
#endif // DEBUG
