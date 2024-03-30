/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AspectRatio.h"

#include "mozilla/WritingModes.h"

namespace mozilla {

nscoord AspectRatio::ComputeRatioDependentSize(
    LogicalAxis aRatioDependentAxis, const WritingMode& aWM,
    nscoord aRatioDeterminingSize,
    const LogicalSize& aContentBoxSizeToBoxSizingAdjust) const {
  MOZ_ASSERT(*this,
             "Infinite or zero ratio may have undefined behavior when "
             "computing the size");
  const LogicalSize& boxSizingAdjust = mUseBoxSizing == UseBoxSizing::No
                                           ? LogicalSize(aWM)
                                           : aContentBoxSizeToBoxSizingAdjust;
  return aRatioDependentAxis == LogicalAxis::Inline
             ? ConvertToWritingMode(aWM).ApplyTo(aRatioDeterminingSize +
                                                 boxSizingAdjust.BSize(aWM)) -
                   boxSizingAdjust.ISize(aWM)
             : ConvertToWritingMode(aWM).Inverted().ApplyTo(
                   aRatioDeterminingSize + boxSizingAdjust.ISize(aWM)) -
                   boxSizingAdjust.BSize(aWM);
}

}  // namespace mozilla
