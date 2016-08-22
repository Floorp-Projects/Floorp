/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object for the point that anchors out-of-flow rendering
 * objects such as floats and absolutely positioned elements
 */

#include "nsPlaceholderFrame.h"

#include "gfxUtils.h"
#include "mozilla/gfx/2D.h"
#include "nsDisplayList.h"
#include "nsFrameManager.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsRenderingContext.h"
#include "nsIFrameInlines.h"
#include "nsIContentInlines.h"

using namespace mozilla;
using namespace mozilla::gfx;

nsIFrame*
NS_NewPlaceholderFrame(nsIPresShell* aPresShell, nsStyleContext* aContext,
                       nsFrameState aTypeBit)
{
  return new (aPresShell) nsPlaceholderFrame(aContext, aTypeBit);
}

NS_IMPL_FRAMEARENA_HELPERS(nsPlaceholderFrame)

#ifdef DEBUG
NS_QUERYFRAME_HEAD(nsPlaceholderFrame)
  NS_QUERYFRAME_ENTRY(nsPlaceholderFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsFrame)
#endif

/* virtual */ nsSize
nsPlaceholderFrame::GetXULMinSize(nsBoxLayoutState& aBoxLayoutState)
{
  nsSize size(0, 0);
  DISPLAY_MIN_SIZE(this, size);
  return size;
}

/* virtual */ nsSize
nsPlaceholderFrame::GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState)
{
  nsSize size(0, 0);
  DISPLAY_PREF_SIZE(this, size);
  return size;
}

/* virtual */ nsSize
nsPlaceholderFrame::GetXULMaxSize(nsBoxLayoutState& aBoxLayoutState)
{
  nsSize size(NS_INTRINSICSIZE, NS_INTRINSICSIZE);
  DISPLAY_MAX_SIZE(this, size);
  return size;
}

/* virtual */ void
nsPlaceholderFrame::AddInlineMinISize(nsRenderingContext* aRenderingContext,
                                      nsIFrame::InlineMinISizeData* aData)
{
  // Override AddInlineMinWith so that *nothing* happens.  In
  // particular, we don't want to zero out |aData->mTrailingWhitespace|,
  // since nsLineLayout skips placeholders when trimming trailing
  // whitespace, and we don't want to set aData->mSkipWhitespace to
  // false.

  // ...but push floats onto the list
  if (mOutOfFlowFrame->IsFloating()) {
    nscoord floatWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                           mOutOfFlowFrame,
                                           nsLayoutUtils::MIN_ISIZE);
    aData->mFloats.AppendElement(
      InlineIntrinsicISizeData::FloatInfo(mOutOfFlowFrame, floatWidth));
  }
}

/* virtual */ void
nsPlaceholderFrame::AddInlinePrefISize(nsRenderingContext* aRenderingContext,
                                       nsIFrame::InlinePrefISizeData* aData)
{
  // Override AddInlinePrefWith so that *nothing* happens.  In
  // particular, we don't want to zero out |aData->mTrailingWhitespace|,
  // since nsLineLayout skips placeholders when trimming trailing
  // whitespace, and we don't want to set aData->mSkipWhitespace to
  // false.

  // ...but push floats onto the list
  if (mOutOfFlowFrame->IsFloating()) {
    nscoord floatWidth =
      nsLayoutUtils::IntrinsicForContainer(aRenderingContext,
                                           mOutOfFlowFrame,
                                           nsLayoutUtils::PREF_ISIZE);
    aData->mFloats.AppendElement(
      InlineIntrinsicISizeData::FloatInfo(mOutOfFlowFrame, floatWidth));
  }
}

void
nsPlaceholderFrame::Reflow(nsPresContext*           aPresContext,
                           ReflowOutput&     aDesiredSize,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus&          aStatus)
{
#ifdef DEBUG
  // We should be getting reflowed before our out-of-flow.
  // If this is our first reflow, and our out-of-flow has already received its
  // first reflow (before us), complain.
  // XXXdholbert This "look for a previous continuation or IB-split sibling"
  // code could use nsLayoutUtils::GetPrevContinuationOrIBSplitSibling(), if
  // we ever add a function like that. (We currently have a "Next" version.)
  if ((GetStateBits() & NS_FRAME_FIRST_REFLOW) &&
      !(mOutOfFlowFrame->GetStateBits() & NS_FRAME_FIRST_REFLOW)) {

    // Unfortunately, this can currently happen when the placeholder is in a
    // later continuation or later IB-split sibling than its out-of-flow (as
    // is the case in some of our existing unit tests). So for now, in that
    // case, we'll warn instead of asserting.
    bool isInContinuationOrIBSplit = false;
    nsIFrame* ancestor = this;
    while ((ancestor = ancestor->GetParent())) {
      if (ancestor->GetPrevContinuation() ||
          ancestor->Properties().Get(IBSplitPrevSibling())) {
        isInContinuationOrIBSplit = true;
        break;
      }
    }

    if (isInContinuationOrIBSplit) {
      NS_WARNING("Out-of-flow frame got reflowed before its placeholder");
    } else {
      NS_ERROR("Out-of-flow frame got reflowed before its placeholder");
    }
  }
#endif

  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsPlaceholderFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  aDesiredSize.ClearSize();

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
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

nsStyleContext*
nsPlaceholderFrame::GetParentStyleContext(nsIFrame** aProviderFrame) const
{
  NS_PRECONDITION(GetParent(), "How can we not have a parent here?");

  nsIContent* parentContent = mContent ? mContent->GetFlattenedTreeParent() : nullptr;
  if (parentContent) {
    nsStyleContext* sc =
      PresContext()->FrameManager()->GetDisplayContentsStyleFor(parentContent);
    if (sc) {
      *aProviderFrame = nullptr;
      return sc;
    }
  }

  // Lie about our pseudo so we can step out of all anon boxes and
  // pseudo-elements.  The other option would be to reimplement the
  // {ib} split gunk here.
  *aProviderFrame = CorrectStyleParentFrame(GetParent(), nsGkAtoms::placeholderFrame);
  return *aProviderFrame ? (*aProviderFrame)->StyleContext() : nullptr;
}


#ifdef DEBUG
static void
PaintDebugPlaceholder(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                      const nsRect& aDirtyRect, nsPoint aPt)
{
  ColorPattern cyan(ToDeviceColor(Color(0.f, 1.f, 1.f, 1.f)));
  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();

  nscoord x = nsPresContext::CSSPixelsToAppUnits(-5);
  nsRect r(aPt.x + x, aPt.y,
           nsPresContext::CSSPixelsToAppUnits(13),
           nsPresContext::CSSPixelsToAppUnits(3));
  aDrawTarget->FillRect(NSRectToRect(r, appUnitsPerDevPixel), cyan);

  nscoord y = nsPresContext::CSSPixelsToAppUnits(-10);
  r = nsRect(aPt.x, aPt.y + y,
             nsPresContext::CSSPixelsToAppUnits(3),
             nsPresContext::CSSPixelsToAppUnits(10));
  aDrawTarget->FillRect(NSRectToRect(r, appUnitsPerDevPixel), cyan);
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

#ifdef DEBUG_FRAME_DUMP
nsresult
nsPlaceholderFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("Placeholder"), aResult);
}

void
nsPlaceholderFrame::List(FILE* out, const char* aPrefix, uint32_t aFlags) const
{
  nsCString str;
  ListGeneric(str, aPrefix, aFlags);

  if (mOutOfFlowFrame) {
    str += " outOfFlowFrame=";
    nsFrame::ListTag(str, mOutOfFlowFrame);
  }
  fprintf_stderr(out, "%s\n", str.get());
}
#endif
