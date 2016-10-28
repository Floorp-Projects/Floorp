/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utility code for performing CSS Box Alignment */

#ifndef mozilla_CSSAlignUtils_h
#define mozilla_CSSAlignUtils_h

#include "mozilla/WritingModes.h"

namespace mozilla {

class CSSAlignUtils {
public:
  /**
   * This computes the aligned offset of a CSS-aligned child within its
   * alignment container. The returned offset is distance between the
   * logical "start" edge of the alignment container & the logical "start" edge
   * of the aligned child (in terms of the alignment container's writing mode).
   *
   * @param aAlignment An enumerated value representing a keyword for
   *                   "align-self" or "justify-self". The values
   *                   NS_STYLE_ALIGN_{AUTO,LEFT,RIGHT} must *not* be
   *                   passed here; this method expects the caller to have
   *                   already resolved those to 'start', 'end', or 'stretch'.
   * @param aOverflowSafe Indicates whether we have <overflow-position> = safe.
   * @param aAxis The container's axis in which we're doing alignment.
   * @param aSameSide Indicates whether the container's start side in aAxis is
   *                  the same as the child's start side, in the child's
   *                  parallel axis.
   * @param aBaselineAdjust The amount to offset baseline-aligned children.
   * @param aCBSize The size of the alignment container, in its aAxis.
   * @param aRI A ReflowInput for the child.
   * @param aChildSize The child's LogicalSize (in its own writing mode).
   */
  static nscoord AlignJustifySelf(uint8_t aAlignment, bool aOverflowSafe,
                                  LogicalAxis aAxis, bool aSameSide,
                                  nscoord aBaselineAdjust, nscoord aCBSize,
                                  const ReflowInput& aRI,
                                  const LogicalSize& aChildSize);
};

} // namespace mozilla

#endif // mozilla_CSSAlignUtils_h
