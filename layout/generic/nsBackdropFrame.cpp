/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rendering object for CSS "::backdrop" */

#include "nsBackdropFrame.h"

#include "mozilla/PresShell.h"
#include "nsDisplayList.h"

using namespace mozilla;

NS_IMPL_FRAMEARENA_HELPERS(nsBackdropFrame)

#ifdef DEBUG_FRAME_DUMP
nsresult nsBackdropFrame::GetFrameName(nsAString& aResult) const {
  return MakeFrameName(u"Backdrop"_ns, aResult);
}
#endif

/* virtual */
ComputedStyle* nsBackdropFrame::GetParentComputedStyle(
    nsIFrame** aProviderFrame) const {
  // Style context of backdrop pseudo-element does not inherit from
  // any element, per the Fullscreen API spec.
  *aProviderFrame = nullptr;
  return nullptr;
}

/* virtual */
void nsBackdropFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                       const nsDisplayListSet& aLists) {
  DO_GLOBAL_REFLOW_COUNT_DSP("nsBackdropFrame");
  // We want this frame to always be there even if its display value is
  // none or contents so that we can respond to style change on it. To
  // support those values, we skip painting ourselves in those cases.
  auto display = StyleDisplay()->mDisplay;
  if (display == mozilla::StyleDisplay::None ||
      display == mozilla::StyleDisplay::Contents) {
    return;
  }

  DisplayBorderBackgroundOutline(aBuilder, aLists);
}

/* virtual */
LogicalSize nsBackdropFrame::ComputeAutoSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  // Note that this frame is a child of the viewport frame.
  LogicalSize result(aWM, 0xdeadbeef, NS_UNCONSTRAINEDSIZE);
  if (aFlags.contains(ComputeSizeFlag::ShrinkWrap)) {
    result.ISize(aWM) = 0;
  } else {
    result.ISize(aWM) =
        aAvailableISize - aMargin.ISize(aWM) - aBorderPadding.ISize(aWM);
  }
  return result;
}

/* virtual */
void nsBackdropFrame::Reflow(nsPresContext* aPresContext,
                             ReflowOutput& aDesiredSize,
                             const ReflowInput& aReflowInput,
                             nsReflowStatus& aStatus) {
  MarkInReflow();
  DO_GLOBAL_REFLOW_COUNT("nsBackdropFrame");
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");

  // Note that this frame is a child of the viewport frame.
  WritingMode wm = aReflowInput.GetWritingMode();
  aDesiredSize.SetSize(wm, aReflowInput.ComputedSizeWithBorderPadding(wm));
}
