/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsProgressFrame.h"

#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLProgressElement.h"
#include "nsIContent.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsFontMetrics.h"
#include <algorithm>

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame* NS_NewProgressFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsProgressFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsProgressFrame)

nsProgressFrame::nsProgressFrame(ComputedStyle* aStyle,
                                 nsPresContext* aPresContext)
    : nsContainerFrame(aStyle, aPresContext, kClassID), mBarDiv(nullptr) {}

nsProgressFrame::~nsProgressFrame() = default;

void nsProgressFrame::Destroy(DestroyContext& aContext) {
  NS_ASSERTION(!GetPrevContinuation(),
               "nsProgressFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");
  aContext.AddAnonymousContent(mBarDiv.forget());
  nsContainerFrame::Destroy(aContext);
}

nsresult nsProgressFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  // Create the progress bar div.
  nsCOMPtr<Document> doc = mContent->GetComposedDoc();
  mBarDiv = doc->CreateHTMLElement(nsGkAtoms::div);

  // Associate ::-moz-progress-bar pseudo-element to the anonymous child.
  if (StaticPrefs::layout_css_modern_range_pseudos_enabled()) {
    // TODO(emilio): Create also a slider-track pseudo-element.
    mBarDiv->SetPseudoElementType(PseudoStyleType::sliderFill);
  } else {
    mBarDiv->SetPseudoElementType(PseudoStyleType::mozProgressBar);
  }

  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier, or change the return type to void.
  aElements.AppendElement(mBarDiv);

  return NS_OK;
}

void nsProgressFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                               uint32_t aFilter) {
  if (mBarDiv) {
    aElements.AppendElement(mBarDiv);
  }
}

NS_QUERYFRAME_HEAD(nsProgressFrame)
  NS_QUERYFRAME_ENTRY(nsProgressFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void nsProgressFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                       const nsDisplayListSet& aLists) {
  BuildDisplayListForInline(aBuilder, aLists);
}

void nsProgressFrame::Reflow(nsPresContext* aPresContext,
                             ReflowOutput& aDesiredSize,
                             const ReflowInput& aReflowInput,
                             nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsProgressFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  NS_ASSERTION(mBarDiv, "Progress bar div must exist!");
  NS_ASSERTION(
      PrincipalChildList().GetLength() == 1 &&
          PrincipalChildList().FirstChild() == mBarDiv->GetPrimaryFrame(),
      "unexpected child frames");
  NS_ASSERTION(!GetPrevContinuation(),
               "nsProgressFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");

  const auto wm = aReflowInput.GetWritingMode();
  const auto contentBoxSize = aReflowInput.ComputedSizeWithBSizeFallback([&] {
    nscoord em = OneEmInAppUnits();
    return ResolvedOrientationIsVertical() == wm.IsVertical() ? em : 10 * em;
  });
  aDesiredSize.SetSize(
      wm,
      contentBoxSize + aReflowInput.ComputedLogicalBorderPadding(wm).Size(wm));
  aDesiredSize.SetOverflowAreasToDesiredBounds();

  for (nsIFrame* childFrame : PrincipalChildList()) {
    ReflowChildFrame(childFrame, aPresContext, aReflowInput, contentBoxSize,
                     aStatus);
    ConsiderChildOverflow(aDesiredSize.mOverflowAreas, childFrame);
  }

  FinishAndStoreOverflow(&aDesiredSize);

  aStatus.Reset();  // This type of frame can't be split.
}

void nsProgressFrame::ReflowChildFrame(nsIFrame* aChild,
                                       nsPresContext* aPresContext,
                                       const ReflowInput& aReflowInput,
                                       const LogicalSize& aParentContentBoxSize,
                                       nsReflowStatus& aStatus) {
  bool vertical = ResolvedOrientationIsVertical();
  const WritingMode wm = aChild->GetWritingMode();
  const LogicalSize parentSizeInChildWM =
      aParentContentBoxSize.ConvertTo(wm, aReflowInput.GetWritingMode());
  const nsSize parentPhysicalSize = parentSizeInChildWM.GetPhysicalSize(wm);
  LogicalSize availSize = parentSizeInChildWM;
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
  ReflowInput reflowInput(aPresContext, aReflowInput, aChild, availSize,
                          Some(parentSizeInChildWM));
  nscoord size =
      vertical ? parentPhysicalSize.Height() : parentPhysicalSize.Width();
  nscoord xoffset = aReflowInput.ComputedPhysicalBorderPadding().left;
  nscoord yoffset = aReflowInput.ComputedPhysicalBorderPadding().top;

  double position = static_cast<HTMLProgressElement*>(GetContent())->Position();

  // Force the bar's size to match the current progress.
  // When indeterminate, the progress' size will be 100%.
  if (position >= 0.0) {
    size *= position;
  }

  if (!vertical && wm.IsPhysicalRTL()) {
    xoffset += parentPhysicalSize.Width() - size;
  }

  // The bar size is fixed in these cases:
  // - the progress position is determined: the bar size is fixed according
  //   to it's value.
  // - the progress position is indeterminate and the bar appearance should be
  //   shown as native: the bar size is forced to 100%.
  // Otherwise (when the progress is indeterminate and the bar appearance isn't
  // native), the bar size isn't fixed and can be set by the author.
  if (position != -1 || ShouldUseNativeStyle()) {
    if (vertical) {
      // We want the bar to begin at the bottom.
      yoffset += parentPhysicalSize.Height() - size;
      size -= reflowInput.ComputedPhysicalMargin().TopBottom() +
              reflowInput.ComputedPhysicalBorderPadding().TopBottom();
      size = std::max(size, 0);
      reflowInput.SetComputedHeight(size);
    } else {
      size -= reflowInput.ComputedPhysicalMargin().LeftRight() +
              reflowInput.ComputedPhysicalBorderPadding().LeftRight();
      size = std::max(size, 0);
      reflowInput.SetComputedWidth(size);
    }
  } else if (vertical) {
    // For vertical progress bars, we need to position the bar specifically when
    // the width isn't constrained (position == -1 and !ShouldUseNativeStyle())
    // because parentPhysiscalSize.Height() - size == 0.
    // FIXME(emilio): This assumes that the bar's height is constrained, which
    // seems like a wrong assumption?
    yoffset += parentPhysicalSize.Height() - reflowInput.ComputedHeight();
  }

  xoffset += reflowInput.ComputedPhysicalMargin().left;
  yoffset += reflowInput.ComputedPhysicalMargin().top;

  ReflowOutput barDesiredSize(aReflowInput);
  ReflowChild(aChild, aPresContext, barDesiredSize, reflowInput, xoffset,
              yoffset, ReflowChildFlags::Default, aStatus);
  FinishReflowChild(aChild, aPresContext, barDesiredSize, &reflowInput, xoffset,
                    yoffset, ReflowChildFlags::Default);
}

nsresult nsProgressFrame::AttributeChanged(int32_t aNameSpaceID,
                                           nsAtom* aAttribute,
                                           int32_t aModType) {
  NS_ASSERTION(mBarDiv, "Progress bar div must exist!");

  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::max)) {
    auto presShell = PresShell();
    for (auto childFrame : PrincipalChildList()) {
      presShell->FrameNeedsReflow(childFrame, IntrinsicDirty::None,
                                  NS_FRAME_IS_DIRTY);
    }
    InvalidateFrame();
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

nscoord nsProgressFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord minISize = OneEmInAppUnits();
  if (ResolvedOrientationIsVertical() == GetWritingMode().IsVertical()) {
    minISize *= 10;
  }
  return minISize;
}

nscoord nsProgressFrame::GetPrefISize(gfxContext* aRenderingContext) {
  return GetMinISize(aRenderingContext);
}

bool nsProgressFrame::ShouldUseNativeStyle() const {
  nsIFrame* barFrame = PrincipalChildList().FirstChild();

  // Use the native style if these conditions are satisfied:
  // - both frames use the native appearance;
  // - neither frame has author specified rules setting the border or the
  //   background.
  return StyleDisplay()->EffectiveAppearance() ==
             StyleAppearance::ProgressBar &&
         !Style()->HasAuthorSpecifiedBorderOrBackground() && barFrame &&
         barFrame->StyleDisplay()->EffectiveAppearance() ==
             StyleAppearance::Progresschunk &&
         !barFrame->Style()->HasAuthorSpecifiedBorderOrBackground();
}
