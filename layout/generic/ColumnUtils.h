/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* A namespace class for static muti-column utilities. */

#ifndef mozilla_ColumnUtils_h
#define mozilla_ColumnUtils_h

#include "nsStyleCoord.h"

class nsContainerFrame;

namespace mozilla {

// ColumnUtils is a namespace class containing utility functions used by
// multi-column containers like ColumnSetWrapperFrame and nsColumnSetFrame.
//
class ColumnUtils final {
 public:
  // Compute used value of 'column-gap' for aFrame.
  static nscoord GetColumnGap(const nsContainerFrame* aFrame,
                              nscoord aPercentageBasis);

  // Clamp used column width to a minimum of 1px.
  static nscoord ClampUsedColumnWidth(const Length& aColumnWidth);

  // Compute the intrinsic inline-size of a column container, given a non-zero
  // column-count, column gap, and column box's inline-size.
  static nscoord IntrinsicISize(uint32_t aColCount, nscoord aColGap,
                                nscoord aColISize);
};

}  // namespace mozilla

#endif  // mozilla_ColumnUtils_h
