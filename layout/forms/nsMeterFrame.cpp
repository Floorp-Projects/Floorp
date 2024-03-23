/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMeterFrame.h"

#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/HTMLMeterElement.h"
#include "nsIContent.h"
#include "nsLayoutUtils.h"
#include "nsPresContext.h"
#include "nsGkAtoms.h"
#include "nsNodeInfoManager.h"
#include "nsContentCreatorFunctions.h"
#include "nsFontMetrics.h"
#include <algorithm>

using namespace mozilla;
using mozilla::dom::Document;
using mozilla::dom::Element;
using mozilla::dom::HTMLMeterElement;

nsIFrame* NS_NewMeterFrame(PresShell* aPresShell, ComputedStyle* aStyle) {
  return new (aPresShell) nsMeterFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsMeterFrame)

nsMeterFrame::nsMeterFrame(ComputedStyle* aStyle, nsPresContext* aPresContext)
    : nsContainerFrame(aStyle, aPresContext, kClassID), mBarDiv(nullptr) {}

nsMeterFrame::~nsMeterFrame() = default;

void nsMeterFrame::Destroy(DestroyContext& aContext) {
  NS_ASSERTION(!GetPrevContinuation(),
               "nsMeterFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");
  aContext.AddAnonymousContent(mBarDiv.forget());
  nsContainerFrame::Destroy(aContext);
}

nsresult nsMeterFrame::CreateAnonymousContent(
    nsTArray<ContentInfo>& aElements) {
  // Get the NodeInfoManager and tag necessary to create the meter bar div.
  nsCOMPtr<Document> doc = mContent->GetComposedDoc();

  // Create the div.
  mBarDiv = doc->CreateHTMLElement(nsGkAtoms::div);

  // Associate the right pseudo-element to the anonymous child.
  if (StaticPrefs::layout_css_modern_range_pseudos_enabled()) {
    // TODO(emilio): Create also a slider-track pseudo-element.
    mBarDiv->SetPseudoElementType(PseudoStyleType::sliderFill);
  } else {
    mBarDiv->SetPseudoElementType(PseudoStyleType::mozMeterBar);
  }

  aElements.AppendElement(mBarDiv);

  return NS_OK;
}

void nsMeterFrame::AppendAnonymousContentTo(nsTArray<nsIContent*>& aElements,
                                            uint32_t aFilter) {
  if (mBarDiv) {
    aElements.AppendElement(mBarDiv);
  }
}

NS_QUERYFRAME_HEAD(nsMeterFrame)
  NS_QUERYFRAME_ENTRY(nsMeterFrame)
  NS_QUERYFRAME_ENTRY(nsIAnonymousContentCreator)
NS_QUERYFRAME_TAIL_INHERITING(nsContainerFrame)

void nsMeterFrame::Reflow(nsPresContext* aPresContext,
                          ReflowOutput& aDesiredSize,
                          const ReflowInput& aReflowInput,
                          nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsMeterFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowInput, aDesiredSize, aStatus);
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  NS_ASSERTION(mBarDiv, "Meter bar div must exist!");
  NS_ASSERTION(!GetPrevContinuation(),
               "nsMeterFrame should not have continuations; if it does we "
               "need to call RegUnregAccessKey only for the first.");

  nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();
  NS_ASSERTION(barFrame, "The meter frame should have a child with a frame!");

  const auto wm = aReflowInput.GetWritingMode();
  const auto contentBoxSize = aReflowInput.ComputedSizeWithBSizeFallback([&] {
    nscoord em = OneEmInAppUnits();
    return ResolvedOrientationIsVertical() == wm.IsVertical() ? em : 5 * em;
  });
  aDesiredSize.SetSize(
      wm,
      contentBoxSize + aReflowInput.ComputedLogicalBorderPadding(wm).Size(wm));
  aDesiredSize.SetOverflowAreasToDesiredBounds();

  ReflowBarFrame(barFrame, aPresContext, aReflowInput, contentBoxSize, aStatus);
  ConsiderChildOverflow(aDesiredSize.mOverflowAreas, barFrame);

  FinishAndStoreOverflow(&aDesiredSize);

  aStatus.Reset();  // This type of frame can't be split.
}

void nsMeterFrame::ReflowBarFrame(nsIFrame* aBarFrame,
                                  nsPresContext* aPresContext,
                                  const ReflowInput& aReflowInput,
                                  const LogicalSize& aParentContentBoxSize,
                                  nsReflowStatus& aStatus) {
  bool vertical = ResolvedOrientationIsVertical();
  const WritingMode wm = aBarFrame->GetWritingMode();
  const LogicalSize parentSizeInChildWM =
      aParentContentBoxSize.ConvertTo(wm, aReflowInput.GetWritingMode());
  const nsSize parentPhysicalSize = parentSizeInChildWM.GetPhysicalSize(wm);
  LogicalSize availSize = parentSizeInChildWM;
  availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
  ReflowInput reflowInput(aPresContext, aReflowInput, aBarFrame, availSize,
                          Some(parentSizeInChildWM));
  nscoord size =
      vertical ? parentPhysicalSize.Height() : parentPhysicalSize.Width();
  nscoord xoffset = aReflowInput.ComputedPhysicalBorderPadding().left;
  nscoord yoffset = aReflowInput.ComputedPhysicalBorderPadding().top;

  auto* meterElement = static_cast<HTMLMeterElement*>(GetContent());
  size = NSToCoordRound(size * meterElement->Position());

  if (!vertical && wm.IsPhysicalRTL()) {
    xoffset += parentPhysicalSize.Width() - size;
  }

  // The bar position is *always* constrained.
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

  xoffset += reflowInput.ComputedPhysicalMargin().left;
  yoffset += reflowInput.ComputedPhysicalMargin().top;

  ReflowOutput barDesiredSize(reflowInput);
  ReflowChild(aBarFrame, aPresContext, barDesiredSize, reflowInput, xoffset,
              yoffset, ReflowChildFlags::Default, aStatus);
  FinishReflowChild(aBarFrame, aPresContext, barDesiredSize, &reflowInput,
                    xoffset, yoffset, ReflowChildFlags::Default);
}

nsresult nsMeterFrame::AttributeChanged(int32_t aNameSpaceID,
                                        nsAtom* aAttribute, int32_t aModType) {
  NS_ASSERTION(mBarDiv, "Meter bar div must exist!");

  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::value || aAttribute == nsGkAtoms::max ||
       aAttribute == nsGkAtoms::min)) {
    nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();
    NS_ASSERTION(barFrame, "The meter frame should have a child with a frame!");
    PresShell()->FrameNeedsReflow(barFrame, IntrinsicDirty::None,
                                  NS_FRAME_IS_DIRTY);
    InvalidateFrame();
  }

  return nsContainerFrame::AttributeChanged(aNameSpaceID, aAttribute, aModType);
}

nscoord nsMeterFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord minISize = OneEmInAppUnits();
  if (ResolvedOrientationIsVertical() == GetWritingMode().IsVertical()) {
    // The orientation is inline
    minISize *= 5;
  }
  return minISize;
}

nscoord nsMeterFrame::GetPrefISize(gfxContext* aRenderingContext) {
  return GetMinISize(aRenderingContext);
}

bool nsMeterFrame::ShouldUseNativeStyle() const {
  nsIFrame* barFrame = mBarDiv->GetPrimaryFrame();

  // Use the native style if these conditions are satisfied:
  // - both frames use the native appearance;
  // - neither frame has author specified rules setting the border or the
  //   background.
  return StyleDisplay()->EffectiveAppearance() == StyleAppearance::Meter &&
         !Style()->HasAuthorSpecifiedBorderOrBackground() && barFrame &&
         barFrame->StyleDisplay()->EffectiveAppearance() ==
             StyleAppearance::Meterchunk &&
         !barFrame->Style()->HasAuthorSpecifiedBorderOrBackground();
}
