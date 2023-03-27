
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SimpleXULLeafFrame.h"
#include "mozilla/PresShell.h"

nsIFrame* NS_NewSimpleXULLeafFrame(mozilla::PresShell* aPresShell,
                                   mozilla::ComputedStyle* aStyle) {
  return new (aPresShell)
      mozilla::SimpleXULLeafFrame(aStyle, aPresShell->GetPresContext());
}

namespace mozilla {

NS_IMPL_FRAMEARENA_HELPERS(SimpleXULLeafFrame)

void SimpleXULLeafFrame::Reflow(nsPresContext* aPresContext,
                                ReflowOutput& aDesiredSize,
                                const ReflowInput& aReflowInput,
                                nsReflowStatus& aStatus) {
  MarkInReflow();
  MOZ_ASSERT(aStatus.IsEmpty(), "Caller should pass a fresh reflow status!");
  const auto wm = GetWritingMode();
  const auto& bp = aReflowInput.ComputedLogicalBorderPadding(wm);
  aDesiredSize.ISize(wm) = bp.IStartEnd(wm) + aReflowInput.ComputedISize();
  aDesiredSize.BSize(wm) =
      bp.BStartEnd(wm) + (aReflowInput.ComputedBSize() == NS_UNCONSTRAINEDSIZE
                              ? aReflowInput.ComputedMinBSize()
                              : aReflowInput.ComputedBSize());
  aDesiredSize.SetOverflowAreasToDesiredBounds();
}

}  // namespace mozilla
