/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * rendering object for the point that anchors out-of-flow rendering
 * objects such as floats and absolutely positioned elements
 */

#include "nsPlaceholderFrame.h"

#include "gfxContext.h"
#include "gfxUtils.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/PresShell.h"
#include "mozilla/PresShellInlines.h"
#include "mozilla/ServoStyleSetInlines.h"
#include "nsCSSFrameConstructor.h"
#include "nsDisplayList.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsIFrameInlines.h"
#include "nsIContentInlines.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::gfx;

nsPlaceholderFrame* NS_NewPlaceholderFrame(PresShell* aPresShell,
                                           ComputedStyle* aStyle,
                                           nsFrameState aTypeBits) {
  return new (aPresShell)
      nsPlaceholderFrame(aStyle, aPresShell->GetPresContext(), aTypeBits);
}

NS_IMPL_FRAMEARENA_HELPERS(nsPlaceholderFrame)

#ifdef DEBUG
NS_QUERYFRAME_HEAD(nsPlaceholderFrame)
  NS_QUERYFRAME_ENTRY(nsPlaceholderFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsIFrame)
#endif

/* virtual */
nsSize nsPlaceholderFrame::GetXULMinSize(nsBoxLayoutState& aBoxLayoutState) {
  nsSize size(0, 0);
  DISPLAY_MIN_SIZE(this, size);
  return size;
}

/* virtual */
nsSize nsPlaceholderFrame::GetXULPrefSize(nsBoxLayoutState& aBoxLayoutState) {
  nsSize size(0, 0);
  DISPLAY_PREF_SIZE(this, size);
  return size;
}

/* virtual */
nsSize nsPlaceholderFrame::GetXULMaxSize(nsBoxLayoutState& aBoxLayoutState) {
  nsSize size(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  DISPLAY_MAX_SIZE(this, size);
  return size;
}

/* virtual */
void nsPlaceholderFrame::AddInlineMinISize(
    gfxContext* aRenderingContext, nsIFrame::InlineMinISizeData* aData) {
  // Override AddInlineMinWith so that *nothing* happens.  In
  // particular, we don't want to zero out |aData->mTrailingWhitespace|,
  // since nsLineLayout skips placeholders when trimming trailing
  // whitespace, and we don't want to set aData->mSkipWhitespace to
  // false.

  // ...but push floats onto the list
  if (mOutOfFlowFrame->IsFloating()) {
    nscoord floatWidth = nsLayoutUtils::IntrinsicForContainer(
        aRenderingContext, mOutOfFlowFrame, IntrinsicISizeType::MinISize);
    aData->mFloats.AppendElement(
        InlineIntrinsicISizeData::FloatInfo(mOutOfFlowFrame, floatWidth));
  }
}

/* virtual */
void nsPlaceholderFrame::AddInlinePrefISize(
    gfxContext* aRenderingContext, nsIFrame::InlinePrefISizeData* aData) {
  // Override AddInlinePrefWith so that *nothing* happens.  In
  // particular, we don't want to zero out |aData->mTrailingWhitespace|,
  // since nsLineLayout skips placeholders when trimming trailing
  // whitespace, and we don't want to set aData->mSkipWhitespace to
  // false.

  // ...but push floats onto the list
  if (mOutOfFlowFrame->IsFloating()) {
    nscoord floatWidth = nsLayoutUtils::IntrinsicForContainer(
        aRenderingContext, mOutOfFlowFrame, IntrinsicISizeType::PrefISize);
    aData->mFloats.AppendElement(
        InlineIntrinsicISizeData::FloatInfo(mOutOfFlowFrame, floatWidth));
  }
}

void nsPlaceholderFrame::Reflow(nsPresContext* aPresContext,
                                ReflowOutput& aDesiredSize,
                                const ReflowInput& aReflowInput,
                                nsReflowStatus& aStatus) {
  // NOTE that the ReflowInput passed to this method is not fully initialized,
  // on the grounds that reflowing a placeholder is a rather trivial operation.
  // (See bug 1367711.)

#ifdef DEBUG
  // We should be getting reflowed before our out-of-flow.
  // If this is our first reflow, and our out-of-flow has already received its
  // first reflow (before us), complain.
  if (HasAnyStateBits(NS_FRAME_FIRST_REFLOW) &&
      !mOutOfFlowFrame->HasAnyStateBits(NS_FRAME_FIRST_REFLOW)) {
    // Unfortunately, this can currently happen when the placeholder is in a
    // later continuation or later IB-split sibling than its out-of-flow (as
    // is the case in some of our existing unit tests). So for now, in that
    // case, we'll warn instead of asserting.
    bool isInContinuationOrIBSplit = false;
    nsIFrame* ancestor = this;
    while ((ancestor = ancestor->GetParent())) {
      if (nsLayoutUtils::GetPrevContinuationOrIBSplitSibling(ancestor)) {
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
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  aDesiredSize.ClearSize();

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

static nsIFrame::ChildListID ChildListIDForOutOfFlow(
    nsFrameState aPlaceholderState, const nsIFrame* aChild) {
  if (aPlaceholderState & PLACEHOLDER_FOR_FLOAT) {
    return nsIFrame::kFloatList;
  }
  if (aPlaceholderState & PLACEHOLDER_FOR_POPUP) {
    return nsIFrame::kPopupList;
  }
  if (aPlaceholderState & PLACEHOLDER_FOR_FIXEDPOS) {
    return nsLayoutUtils::MayBeReallyFixedPos(aChild) ? nsIFrame::kFixedList
                                                      : nsIFrame::kAbsoluteList;
  }
  if (aPlaceholderState & PLACEHOLDER_FOR_ABSPOS) {
    return nsIFrame::kAbsoluteList;
  }
  MOZ_DIAGNOSTIC_ASSERT(false, "unknown list");
  return nsIFrame::kFloatList;
}

void nsPlaceholderFrame::DestroyFrom(nsIFrame* aDestructRoot,
                                     PostDestroyData& aPostDestroyData) {
  nsIFrame* oof = mOutOfFlowFrame;
  if (oof) {
    mOutOfFlowFrame = nullptr;
    oof->RemoveProperty(nsIFrame::PlaceholderFrameProperty());

    // If aDestructRoot is not an ancestor of the out-of-flow frame,
    // then call RemoveFrame on it here.
    // Also destroy it here if it's a popup frame. (Bug 96291)
    if (HasAnyStateBits(PLACEHOLDER_FOR_POPUP) ||
        !nsLayoutUtils::IsProperAncestorFrame(aDestructRoot, oof)) {
      ChildListID listId = ChildListIDForOutOfFlow(GetStateBits(), oof);
      nsFrameManager* fm = PresContext()->FrameConstructor();
      fm->RemoveFrame(listId, oof);
    }
    // else oof will be destroyed by its parent
  }

  nsIFrame::DestroyFrom(aDestructRoot, aPostDestroyData);
}

/* virtual */
bool nsPlaceholderFrame::CanContinueTextRun() const {
  if (!mOutOfFlowFrame) {
    return false;
  }
  // first-letter frames can continue text runs, and placeholders for floated
  // first-letter frames can too
  return mOutOfFlowFrame->CanContinueTextRun();
}

ComputedStyle* nsPlaceholderFrame::GetParentComputedStyleForOutOfFlow(
    nsIFrame** aProviderFrame) const {
  MOZ_ASSERT(GetParent(), "How can we not have a parent here?");

  Element* parentElement =
      mContent ? mContent->GetFlattenedTreeParentElement() : nullptr;
  // See the similar code in nsIFrame::DoGetParentComputedStyle.
  if (parentElement && MOZ_LIKELY(parentElement->HasServoData()) &&
      Servo_Element_IsDisplayContents(parentElement)) {
    RefPtr<ComputedStyle> style =
        ServoStyleSet::ResolveServoStyle(*parentElement);
    *aProviderFrame = nullptr;
    // See the comment in GetParentComputedStyle to see why returning this as a
    // weak ref is fine.
    return style;
  }

  return GetLayoutParentStyleForOutOfFlow(aProviderFrame);
}

ComputedStyle* nsPlaceholderFrame::GetLayoutParentStyleForOutOfFlow(
    nsIFrame** aProviderFrame) const {
  // Lie about our pseudo so we can step out of all anon boxes and
  // pseudo-elements.  The other option would be to reimplement the
  // {ib} split gunk here.
  //
  // See the hack in CorrectStyleParentFrame for why we pass `MAX`.
  *aProviderFrame = CorrectStyleParentFrame(GetParent(), PseudoStyleType::MAX);
  return *aProviderFrame ? (*aProviderFrame)->Style() : nullptr;
}

#ifdef DEBUG
static void PaintDebugPlaceholder(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                                  const nsRect& aDirtyRect, nsPoint aPt) {
  ColorPattern cyan(ToDeviceColor(sRGBColor(0.f, 1.f, 1.f, 1.f)));
  int32_t appUnitsPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();

  nscoord x = nsPresContext::CSSPixelsToAppUnits(-5);
  nsRect r(aPt.x + x, aPt.y, nsPresContext::CSSPixelsToAppUnits(13),
           nsPresContext::CSSPixelsToAppUnits(3));
  aDrawTarget->FillRect(NSRectToRect(r, appUnitsPerDevPixel), cyan);

  nscoord y = nsPresContext::CSSPixelsToAppUnits(-10);
  r = nsRect(aPt.x, aPt.y + y, nsPresContext::CSSPixelsToAppUnits(3),
             nsPresContext::CSSPixelsToAppUnits(10));
  aDrawTarget->FillRect(NSRectToRect(r, appUnitsPerDevPixel), cyan);
}
#endif  // DEBUG

#if defined(DEBUG) || (defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF))

void nsPlaceholderFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                          const nsDisplayListSet& aLists) {
  DO_GLOBAL_REFLOW_COUNT_DSP("nsPlaceholderFrame");

#  ifdef DEBUG
  if (GetShowFrameBorders()) {
    aLists.Outlines()->AppendNewToTop<nsDisplayGeneric>(
        aBuilder, this, PaintDebugPlaceholder, "DebugPlaceholder",
        DisplayItemType::TYPE_DEBUG_PLACEHOLDER);
  }
#  endif
}
#endif  // DEBUG || (MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF)

#ifdef DEBUG_FRAME_DUMP
nsresult nsPlaceholderFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"Placeholder"_ns, aResult);
}

void nsPlaceholderFrame::List(FILE* out, const char* aPrefix,
                              ListFlags aFlags) const {
  nsCString str;
  ListGeneric(str, aPrefix, aFlags);

  if (mOutOfFlowFrame) {
    str += " outOfFlowFrame=";
    str += mOutOfFlowFrame->ListTag();
  }
  fprintf_stderr(out, "%s\n", str.get());
}
#endif
