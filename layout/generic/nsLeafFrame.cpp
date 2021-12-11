/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for rendering objects that do not have child lists */

#include "nsLeafFrame.h"

#include "mozilla/PresShell.h"
#include "nsPresContext.h"

using namespace mozilla;

nsLeafFrame::~nsLeafFrame() = default;

/* virtual */
void nsLeafFrame::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                   const nsDisplayListSet& aLists) {
  DO_GLOBAL_REFLOW_COUNT_DSP("nsLeafFrame");
  DisplayBorderBackgroundOutline(aBuilder, aLists);
}

/* virtual */
nscoord nsLeafFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);
  result = GetIntrinsicISize();
  return result;
}

/* virtual */
nscoord nsLeafFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);
  result = GetIntrinsicISize();
  return result;
}

/* virtual */
LogicalSize nsLeafFrame::ComputeAutoSize(
    gfxContext* aRenderingContext, WritingMode aWM, const LogicalSize& aCBSize,
    nscoord aAvailableISize, const LogicalSize& aMargin,
    const LogicalSize& aBorderPadding, const StyleSizeOverrides& aSizeOverrides,
    ComputeSizeFlags aFlags) {
  const WritingMode wm = GetWritingMode();
  LogicalSize result(wm, GetIntrinsicISize(), GetIntrinsicBSize());
  return result.ConvertTo(aWM, wm);
}

nscoord nsLeafFrame::GetIntrinsicBSize() {
  MOZ_ASSERT_UNREACHABLE("Someone didn't override Reflow or ComputeAutoSize");
  return 0;
}

void nsLeafFrame::SizeToAvailSize(const ReflowInput& aReflowInput,
                                  ReflowOutput& aDesiredSize) {
  aDesiredSize.SetSize(aReflowInput.GetWritingMode(),
                       aReflowInput.AvailableSize());
  aDesiredSize.SetOverflowAreasToDesiredBounds();
  FinishAndStoreOverflow(&aDesiredSize, aReflowInput.mStyleDisplay);
}
