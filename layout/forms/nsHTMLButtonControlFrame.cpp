/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLButtonControlFrame.h"

#include "mozilla/PresShell.h"
#include "nsContainerFrame.h"
#include "nsIFormControlFrame.h"
#include "nsIFrameInlines.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsButtonFrameRenderer.h"
#include "nsCSSAnonBoxes.h"
#include "nsNameSpaceManager.h"
#include "nsDisplayList.h"
#include <algorithm>

using namespace mozilla;

nsContainerFrame* NS_NewHTMLButtonControlFrame(PresShell* aPresShell,
                                               ComputedStyle* aStyle) {
  return new (aPresShell)
      nsHTMLButtonControlFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsHTMLButtonControlFrame)

nsHTMLButtonControlFrame::nsHTMLButtonControlFrame(ComputedStyle* aStyle,
                                                   nsPresContext* aPresContext,
                                                   nsIFrame::ClassID aID)
    : nsContainerFrame(aStyle, aPresContext, aID) {}

nsHTMLButtonControlFrame::~nsHTMLButtonControlFrame() = default;

void nsHTMLButtonControlFrame::Init(nsIContent* aContent,
                                    nsContainerFrame* aParent,
                                    nsIFrame* aPrevInFlow) {
  nsContainerFrame::Init(aContent, aParent, aPrevInFlow);
  mRenderer.SetFrame(this, PresContext());
}

NS_QUERYFRAME_HEAD(nsHTMLButtonControlFrame)
  NS_QUERYFRAME_ENTRY(nsIFormControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

#ifdef ACCESSIBILITY
a11y::AccType nsHTMLButtonControlFrame::AccessibleType() {
  return a11y::eHTMLButtonType;
}
#endif

void nsHTMLButtonControlFrame::SetFocus(bool aOn, bool aRepaint) {}

nsresult nsHTMLButtonControlFrame::HandleEvent(nsPresContext* aPresContext,
                                               WidgetGUIEvent* aEvent,
                                               nsEventStatus* aEventStatus) {
  // if disabled do nothing
  if (mRenderer.isDisabled()) {
    return NS_OK;
  }

  // mouse clicks are handled by content
  // we don't want our children to get any events. So just pass it to frame.
  return nsIFrame::HandleEvent(aPresContext, aEvent, aEventStatus);
}

bool nsHTMLButtonControlFrame::ShouldClipPaintingToBorderBox() {
  return IsInput() || StyleDisplay()->mOverflowX != StyleOverflow::Visible;
}

void nsHTMLButtonControlFrame::BuildDisplayList(
    nsDisplayListBuilder* aBuilder, const nsDisplayListSet& aLists) {
  nsDisplayList onTop(aBuilder);
  if (IsVisibleForPainting()) {
    // Clip the button itself to its border area for event hit testing.
    Maybe<DisplayListClipState::AutoSaveRestore> eventClipState;
    if (aBuilder->IsForEventDelivery()) {
      eventClipState.emplace(aBuilder);
      nsRect rect(aBuilder->ToReferenceFrame(this), GetSize());
      nscoord radii[8];
      bool hasRadii = GetBorderRadii(radii);
      eventClipState->ClipContainingBlockDescendants(
          rect, hasRadii ? radii : nullptr);
    }

    mRenderer.DisplayButton(aBuilder, aLists.BorderBackground(), &onTop);
  }

  nsDisplayListCollection set(aBuilder);

  {
    DisplayListClipState::AutoSaveRestore clipState(aBuilder);

    if (ShouldClipPaintingToBorderBox()) {
      nsMargin border = StyleBorder()->GetComputedBorder();
      nsRect rect(aBuilder->ToReferenceFrame(this), GetSize());
      rect.Deflate(border);
      nscoord radii[8];
      bool hasRadii = GetPaddingBoxBorderRadii(radii);
      clipState.ClipContainingBlockDescendants(rect,
                                               hasRadii ? radii : nullptr);
    }

    BuildDisplayListForChild(aBuilder, mFrames.FirstChild(), set,
                             DisplayChildFlag::ForcePseudoStackingContext);
  }

  // Put the foreground outline and focus rects on top of the children
  set.Content()->AppendToTop(&onTop);
  set.MoveTo(aLists);

  DisplayOutline(aBuilder, aLists);

  // to draw border when selected in editor
  DisplaySelectionOverlay(aBuilder, aLists.Content());
}

nscoord nsHTMLButtonControlFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);
  if (Maybe<nscoord> containISize = ContainIntrinsicISize()) {
    result = *containISize;
  } else {
    nsIFrame* kid = mFrames.FirstChild();
    result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext, kid,
                                                  IntrinsicISizeType::MinISize);
  }
  return result;
}

nscoord nsHTMLButtonControlFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);
  if (Maybe<nscoord> containISize = ContainIntrinsicISize()) {
    result = *containISize;
  } else {
    nsIFrame* kid = mFrames.FirstChild();
    result = nsLayoutUtils::IntrinsicForContainer(
        aRenderingContext, kid, IntrinsicISizeType::PrefISize);
  }
  return result;
}

void nsHTMLButtonControlFrame::Reflow(nsPresContext* aPresContext,
                                      ReflowOutput& aDesiredSize,
                                      const ReflowInput& aReflowInput,
                                      nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsHTMLButtonControlFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  // Reflow the child
  nsIFrame* firstKid = mFrames.FirstChild();

  MOZ_ASSERT(firstKid, "Button should have a child frame for its contents");
  MOZ_ASSERT(!firstKid->GetNextSibling(),
             "Button should have exactly one child frame");
  MOZ_ASSERT(
      firstKid->Style()->GetPseudoType() == PseudoStyleType::buttonContent,
      "Button's child frame has unexpected pseudo type!");

  // XXXbz Eventually we may want to check-and-bail if
  // !aReflowInput.ShouldReflowAllKids() &&
  // !firstKid->IsSubtreeDirty().
  // We'd need to cache our ascent for that, of course.

  // Reflow the contents of the button.
  // (This populates our aDesiredSize, too.)
  ReflowButtonContents(aPresContext, aDesiredSize, aReflowInput, firstKid);

  if (!ShouldClipPaintingToBorderBox()) {
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, firstKid);
  }
  // else, we ignore child overflow -- anything that overflows beyond our
  // own border-box will get clipped when painting.

  FinishReflowWithAbsoluteFrames(aPresContext, aDesiredSize, aReflowInput,
                                 aStatus);

  // We're always complete and we don't support overflow containers
  // so we shouldn't have a next-in-flow ever.
  aStatus.Reset();
  MOZ_ASSERT(!GetNextInFlow());
}

void nsHTMLButtonControlFrame::ReflowButtonContents(
    nsPresContext* aPresContext, ReflowOutput& aButtonDesiredSize,
    const ReflowInput& aButtonReflowInput, nsIFrame* aFirstKid) {
  WritingMode wm = GetWritingMode();
  LogicalSize availSize = aButtonReflowInput.ComputedSize(wm);
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;

  // shorthand for a value we need to use in a bunch of places
  const LogicalMargin& clbp =
      aButtonReflowInput.ComputedLogicalBorderPadding(wm);

  LogicalPoint childPos(wm);
  childPos.I(wm) = clbp.IStart(wm);
  availSize.ISize(wm) = std::max(availSize.ISize(wm), 0);

  ReflowInput contentsReflowInput(aPresContext, aButtonReflowInput, aFirstKid,
                                  availSize);

  nsReflowStatus contentsReflowStatus;
  ReflowOutput contentsDesiredSize(aButtonReflowInput);
  childPos.B(wm) = 0;  // This will be set properly later, after reflowing the
                       // child to determine its size.

  const LayoutFrameType frameType = aFirstKid->Type();
  if (frameType == LayoutFrameType::FlexContainer ||
      frameType == LayoutFrameType::GridContainer) {
    contentsReflowInput.ComputedBSize() = aButtonReflowInput.ComputedBSize();
    contentsReflowInput.ComputedMinBSize() =
        aButtonReflowInput.ComputedMinBSize();
    contentsReflowInput.ComputedMaxBSize() =
        aButtonReflowInput.ComputedMaxBSize();
  }

  // We just pass a dummy containerSize here, as the child will be
  // repositioned later by FinishReflowChild.
  nsSize dummyContainerSize;
  ReflowChild(aFirstKid, aPresContext, contentsDesiredSize, contentsReflowInput,
              wm, childPos, dummyContainerSize, ReflowChildFlags::Default,
              contentsReflowStatus);
  MOZ_ASSERT(contentsReflowStatus.IsComplete(),
             "We gave button-contents frame unconstrained available height, "
             "so it should be complete");

  // Compute the button's content-box size:
  LogicalSize buttonContentBox(wm);
  if (aButtonReflowInput.ComputedBSize() != NS_UNCONSTRAINEDSIZE) {
    // Button has a fixed block-size -- that's its content-box bSize.
    buttonContentBox.BSize(wm) = aButtonReflowInput.ComputedBSize();
  } else {
    // Button is intrinsically sized -- it should shrinkwrap the
    // button-contents' bSize. But if it has size containment in block axis,
    // ignore the contents and use contain-intrinsic-block-size.
    nscoord bSize = aButtonReflowInput.mFrame->ContainIntrinsicBSize().valueOr(
        contentsDesiredSize.BSize(wm));

    // Make sure we obey min/max-bSize in the case when we're doing intrinsic
    // sizing (we get it for free when we have a non-intrinsic
    // aButtonReflowInput.ComputedBSize()).  Note that we do this before
    // adjusting for borderpadding, since mComputedMaxBSize and
    // mComputedMinBSize are content bSizes.
    buttonContentBox.BSize(wm) =
        NS_CSS_MINMAX(bSize, aButtonReflowInput.ComputedMinBSize(),
                      aButtonReflowInput.ComputedMaxBSize());
  }
  if (aButtonReflowInput.ComputedISize() != NS_UNCONSTRAINEDSIZE) {
    buttonContentBox.ISize(wm) = aButtonReflowInput.ComputedISize();
  } else {
    nscoord iSize = aButtonReflowInput.mFrame->ContainIntrinsicISize().valueOr(
        contentsDesiredSize.ISize(wm));
    buttonContentBox.ISize(wm) =
        NS_CSS_MINMAX(iSize, aButtonReflowInput.ComputedMinISize(),
                      aButtonReflowInput.ComputedMaxISize());
  }

  // Center child in the block-direction in the button
  // (technically, inside of the button's focus-padding area)
  nscoord extraSpace =
      buttonContentBox.BSize(wm) - contentsDesiredSize.BSize(wm);

  childPos.B(wm) = std::max(0, extraSpace / 2);

  // Adjust childPos.B() to be in terms of the button's frame-rect:
  childPos.B(wm) += clbp.BStart(wm);

  nsSize containerSize = (buttonContentBox + clbp.Size(wm)).GetPhysicalSize(wm);

  // Place the child
  FinishReflowChild(aFirstKid, aPresContext, contentsDesiredSize,
                    &contentsReflowInput, wm, childPos, containerSize,
                    ReflowChildFlags::Default);

  // Make sure we have a useful 'ascent' value for the child
  if (contentsDesiredSize.BlockStartAscent() ==
      ReflowOutput::ASK_FOR_BASELINE) {
    WritingMode wm = aButtonReflowInput.GetWritingMode();
    contentsDesiredSize.SetBlockStartAscent(aFirstKid->GetLogicalBaseline(wm));
  }

  // OK, we're done with the child frame.
  // Use what we learned to populate the button frame's reflow metrics.
  //  * Button's height & width are content-box size + border-box contribution:
  aButtonDesiredSize.SetSize(
      wm,
      LogicalSize(wm, aButtonReflowInput.ComputedISize() + clbp.IStartEnd(wm),
                  buttonContentBox.BSize(wm) + clbp.BStartEnd(wm)));

  //  * Button's ascent is its child's ascent, plus the child's block-offset
  // within our frame... unless it's orthogonal, in which case we'll use the
  // contents inline-size as an approximation for now.
  // XXX is there a better strategy? should we include border-padding?
  if (!aButtonReflowInput.mStyleDisplay->IsContainLayout()) {
    if (aButtonDesiredSize.GetWritingMode().IsOrthogonalTo(wm)) {
      aButtonDesiredSize.SetBlockStartAscent(contentsDesiredSize.ISize(wm));
    } else {
      aButtonDesiredSize.SetBlockStartAscent(
          contentsDesiredSize.BlockStartAscent() + childPos.B(wm));
    }
  }  // else: we're layout-contained, and so we have no baseline.

  aButtonDesiredSize.SetOverflowAreasToDesiredBounds();
}

bool nsHTMLButtonControlFrame::GetVerticalAlignBaseline(
    mozilla::WritingMode aWM, nscoord* aBaseline) const {
  nsIFrame* inner = mFrames.FirstChild();
  if (MOZ_UNLIKELY(inner->GetWritingMode().IsOrthogonalTo(aWM))) {
    return false;
  }
  if (!inner->GetVerticalAlignBaseline(aWM, aBaseline)) {
    // <input type=color> has an empty block frame as inner frame
    *aBaseline = inner->SynthesizeBaselineBOffsetFromBorderBox(
        aWM, BaselineSharingGroup::First);
  }
  nscoord innerBStart = inner->BStart(aWM, GetSize());
  *aBaseline += innerBStart;
  return true;
}

bool nsHTMLButtonControlFrame::GetNaturalBaselineBOffset(
    mozilla::WritingMode aWM, BaselineSharingGroup aBaselineGroup,
    nscoord* aBaseline) const {
  if (StyleDisplay()->IsContainLayout()) {
    return false;
  }

  nsIFrame* inner = mFrames.FirstChild();
  if (MOZ_UNLIKELY(inner->GetWritingMode().IsOrthogonalTo(aWM))) {
    return false;
  }
  if (!inner->GetNaturalBaselineBOffset(aWM, aBaselineGroup, aBaseline)) {
    // <input type=color> has an empty block frame as inner frame
    *aBaseline =
        inner->SynthesizeBaselineBOffsetFromBorderBox(aWM, aBaselineGroup);
  }
  nscoord innerBStart = inner->BStart(aWM, GetSize());
  if (aBaselineGroup == BaselineSharingGroup::First) {
    *aBaseline += innerBStart;
  } else {
    *aBaseline += BSize(aWM) - (innerBStart + inner->BSize(aWM));
  }
  return true;
}

nsresult nsHTMLButtonControlFrame::SetFormProperty(nsAtom* aName,
                                                   const nsAString& aValue) {
  if (nsGkAtoms::value == aName) {
    return mContent->AsElement()->SetAttr(kNameSpaceID_None, nsGkAtoms::value,
                                          aValue, true);
  }
  return NS_OK;
}

ComputedStyle* nsHTMLButtonControlFrame::GetAdditionalComputedStyle(
    int32_t aIndex) const {
  return mRenderer.GetComputedStyle(aIndex);
}

void nsHTMLButtonControlFrame::SetAdditionalComputedStyle(
    int32_t aIndex, ComputedStyle* aComputedStyle) {
  mRenderer.SetComputedStyle(aIndex, aComputedStyle);
}

void nsHTMLButtonControlFrame::AppendDirectlyOwnedAnonBoxes(
    nsTArray<OwnedAnonBox>& aResult) {
  MOZ_ASSERT(mFrames.FirstChild(), "Must have our button-content anon box");
  MOZ_ASSERT(!mFrames.FirstChild()->GetNextSibling(),
             "Must only have our button-content anon box");
  aResult.AppendElement(OwnedAnonBox(mFrames.FirstChild()));
}

#ifdef DEBUG
void nsHTMLButtonControlFrame::AppendFrames(ChildListID aListID,
                                            nsFrameList& aFrameList) {
  MOZ_CRASH("unsupported operation");
}

void nsHTMLButtonControlFrame::InsertFrames(
    ChildListID aListID, nsIFrame* aPrevFrame,
    const nsLineList::iterator* aPrevFrameLine, nsFrameList& aFrameList) {
  MOZ_CRASH("unsupported operation");
}

void nsHTMLButtonControlFrame::RemoveFrame(ChildListID aListID,
                                           nsIFrame* aOldFrame) {
  MOZ_CRASH("unsupported operation");
}
#endif
