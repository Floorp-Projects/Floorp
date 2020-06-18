/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A namespace class for static muti-column utilities. */

#include "mozilla/ColumnUtils.h"

#include <algorithm>

#include "nsContainerFrame.h"
#include "nsLayoutUtils.h"

namespace mozilla {

/* static */
nscoord ColumnUtils::GetColumnGap(const nsContainerFrame* aFrame,
                                  nscoord aPercentageBasis) {
  const auto& columnGap = aFrame->StylePosition()->mColumnGap;
  if (columnGap.IsNormal()) {
    return aFrame->StyleFont()->mFont.size;
  }
  return nsLayoutUtils::ResolveGapToLength(columnGap, aPercentageBasis);
}

/* static */
nscoord ColumnUtils::ClampUsedColumnWidth(const Length& aColumnWidth) {
  // Per spec, used values will be clamped to a minimum of 1px.
  return std::max(CSSPixel::ToAppUnits(1), aColumnWidth.ToAppUnits());
}

/* static */
nscoord ColumnUtils::IntrinsicISize(uint32_t aColCount, nscoord aColGap,
                                    nscoord aColISize) {
  MOZ_ASSERT(aColCount > 0, "Cannot compute with zero columns!");

  // Column box's inline-size times number of columns (n), plus n-1 column gaps.
  nscoord iSize = aColISize * aColCount + aColGap * (aColCount - 1);

  // The multiplication above can make 'iSize' negative (integer overflow),
  // so use std::max to protect against that.
  return std::max(iSize, aColISize);
}

}  // namespace mozilla
