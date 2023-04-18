/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This frame type is used for input type=date, time, month, week, and
 * datetime-local.
 */

#include "nsDateTimeControlFrame.h"

#include "mozilla/PresShell.h"
#include "nsLayoutUtils.h"
#include "nsTextControlFrame.h"

using namespace mozilla;
using namespace mozilla::dom;

nsIFrame* NS_NewDateTimeControlFrame(PresShell* aPresShell,
                                     ComputedStyle* aStyle) {
  return new (aPresShell)
      nsDateTimeControlFrame(aStyle, aPresShell->GetPresContext());
}

NS_IMPL_FRAMEARENA_HELPERS(nsDateTimeControlFrame)

NS_QUERYFRAME_HEAD(nsDateTimeControlFrame)
  NS_QUERYFRAME_ENTRY(nsDateTimeControlFrame)
NS_QUERYFRAME_TAIL_INHERITING(nsTextControlFrame)

nsDateTimeControlFrame::nsDateTimeControlFrame(ComputedStyle* aStyle,
                                               nsPresContext* aPresContext)
    : nsTextControlFrame(aStyle, aPresContext, kClassID) {}

nscoord nsDateTimeControlFrame::GetMinISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_MIN_INLINE_SIZE(this, result);

  nsIFrame* kid = mFrames.FirstChild();
  if (kid) {  // display:none?
    result = nsLayoutUtils::IntrinsicForContainer(aRenderingContext, kid,
                                                  IntrinsicISizeType::MinISize);
  } else {
    result = 0;
  }

  return result;
}

nscoord nsDateTimeControlFrame::GetPrefISize(gfxContext* aRenderingContext) {
  nscoord result;
  DISPLAY_PREF_INLINE_SIZE(this, result);

  nsIFrame* kid = mFrames.FirstChild();
  if (kid) {  // display:none?
    result = nsLayoutUtils::IntrinsicForContainer(
        aRenderingContext, kid, IntrinsicISizeType::PrefISize);
  } else {
    result = 0;
  }

  return result;
}
