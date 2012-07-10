/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object for the point that anchors out-of-flow rendering
 * objects such as floats and absolutely positioned elements
 */

#include "nsLayoutUtils.h"
#include "nsPlaceholderFrame.h"
#include "nsLineLayout.h"
#include "nsIContent.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsGkAtoms.h"
#include "nsFrameManager.h"
#include "nsDisplayList.h"

nsIFrame*
NS_NewPlaceholderFrame(nsIPresShell* aPresShell, nsStyleContext* aContext,
                       nsFrameState aTypeBit)
{
  return new (aPresShell) nsPlaceholderFrame(aContext, aTypeBit);
}

NS_IMPL_FRAMEARENA_HELPERS(nsPlaceholderFrame)

nsPlaceholderFrame::~nsPlaceholderFrame()
{
}

/* virtual */ nscoord
nsPlaceholderFrame::GetMinWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_MIN_WIDTH(this, result);
  return result;
}

/* virtual */ nscoord
nsPlaceholderFrame::GetPrefWidth(nsRenderingContext *aRenderingContext)
{
  nscoord result = 0;
  DISPLAY_PREF_WIDTH(this, result);
  return result;
}

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
nsPlaceholderFrame::AddInlineMinWidth(nsRenderingContext *aRenderingContext,
                                      nsIFrame::InlineMinWidthData *aData)
{
  // Override AddInlineMinWith so that *nothing* happens.  In
  // particular, we don't want to zero out |aData->trailingWhitespace|,
  // since nsLineLayout skips placeholders when trimming trailing
  // whitespace, and we don't want to set aData->skipWhitespace to
  // false.

  // ...but push floats onto the list
  if (mOutOfFlowFrame->GetStyleDisplay()->IsFloating())
    aData->floats.AppendElement(mOutOfFlowFrame);
}

/* virtual */ void
nsPlaceholderFrame::AddInlinePrefWidth(nsRenderingContext *aRenderingContext,
                                       nsIFrame::InlinePrefWidthData *aData)
{
  // Override AddInlinePrefWith so that *nothing* happens.  In
  // particular, we don't want to zero out |aData->trailingWhitespace|,
  // since nsLineLayout skips placeholders when trimming trailing
  // whitespace, and we don't want to set aData->skipWhitespace to
  // false.

  // ...but push floats onto the list
  if (mOutOfFlowFrame->GetStyleDisplay()->IsFloating())
    aData->floats.AppendElement(mOutOfFlowFrame);
}

NS_IMETHODIMP
nsPlaceholderFrame::Reflow(nsPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
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
  nsIPresShell* shell = PresContext()->GetPresShell();
  nsIFrame* oof = mOutOfFlowFrame;
  if (oof) {
    oof->InvalidateFrameSubtree();
    // Unregister out-of-flow frame
    shell->FrameManager()->UnregisterPlaceholderFrame(this);
    mOutOfFlowFrame = nsnull;
    // If aDestructRoot is not an ancestor of the out-of-flow frame,
    // then call RemoveFrame on it here.
    // Also destroy it here if it's a popup frame. (Bug 96291)
    if (shell->FrameManager() &&
        ((GetStateBits() & PLACEHOLDER_FOR_POPUP) ||
         !nsLayoutUtils::IsProperAncestorFrame(aDestructRoot, oof))) {
      ChildListID listId = nsLayoutUtils::GetChildListNameFor(oof);
      shell->FrameManager()->RemoveFrame(listId, oof, false);
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
                 nsPresContext::CSSPixelsToAppUnits(13), nsPresContext::CSSPixelsToAppUnits(3));
  nscoord y = nsPresContext::CSSPixelsToAppUnits(-10);
  aCtx->FillRect(aPt.x, aPt.y + y,
                 nsPresContext::CSSPixelsToAppUnits(3), nsPresContext::CSSPixelsToAppUnits(10));
}
#endif // DEBUG

#if defined(DEBUG) || (defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF))

NS_IMETHODIMP
nsPlaceholderFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  DO_GLOBAL_REFLOW_COUNT_DSP("nsPlaceholderFrame");
  
#ifdef DEBUG
  if (!GetShowFrameBorders())
    return NS_OK;
  
  return aLists.Outlines()->AppendNewToTop(new (aBuilder)
      nsDisplayGeneric(aBuilder, this, PaintDebugPlaceholder, "DebugPlaceholder",
                       nsDisplayItem::TYPE_DEBUG_PLACEHOLDER));
#else // DEBUG
  return NS_OK;
#endif // DEBUG
}
#endif // DEBUG || (MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF)

#ifdef DEBUG
NS_IMETHODIMP
nsPlaceholderFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Placeholder"), aResult);
}

NS_IMETHODIMP
nsPlaceholderFrame::List(FILE* out, PRInt32 aIndent) const
{
  IndentBy(out, aIndent);
  ListTag(out);
#ifdef DEBUG_waterson
  fprintf(out, " [parent=%p]", static_cast<void*>(mParent));
#endif
  if (HasView()) {
    fprintf(out, " [view=%p]", (void*)GetView());
  }
  fprintf(out, " {%d,%d,%d,%d}", mRect.x, mRect.y, mRect.width, mRect.height);
  if (0 != mState) {
    fprintf(out, " [state=%016llx]", (unsigned long long)mState);
  }
  nsIFrame* prevInFlow = GetPrevInFlow();
  nsIFrame* nextInFlow = GetNextInFlow();
  if (nsnull != prevInFlow) {
    fprintf(out, " prev-in-flow=%p", static_cast<void*>(prevInFlow));
  }
  if (nsnull != nextInFlow) {
    fprintf(out, " next-in-flow=%p", static_cast<void*>(nextInFlow));
  }
  if (nsnull != mContent) {
    fprintf(out, " [content=%p]", static_cast<void*>(mContent));
  }
  if (nsnull != mStyleContext) {
    fprintf(out, " [sc=%p]", static_cast<void*>(mStyleContext));
  }
  if (mOutOfFlowFrame) {
    fprintf(out, " outOfFlowFrame=");
    nsFrame::ListTag(out, mOutOfFlowFrame);
  }
  fputs("\n", out);
  return NS_OK;
}
#endif
