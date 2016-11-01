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
   * Flags to customize the behavior of AlignJustifySelf:
   */
  enum class AlignJustifyFlags {
    eNoFlags           = 0,
    // Indicates that we have <overflow-position> = safe.
    eOverflowSafe      = 1 << 0,
    // Indicates that the container's start side in aAxis is the same
    // as the child's start side in the child's parallel axis.
    eSameSide          = 1 << 1,
    // Indicates that AlignJustifySelf() shouldn't expand "auto" margins.
    // (By default, AlignJustifySelf() *will* expand such margins, to fill the
    // available space before any alignment is done.)
    eIgnoreAutoMargins = 1 << 2,
  };

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
   * @param aAxis The container's axis in which we're doing alignment.
   * @param aBaselineAdjust The amount to offset baseline-aligned children.
   * @param aCBSize The size of the alignment container, in its aAxis.
   * @param aRI A ReflowInput for the child.
   * @param aChildSize The child's LogicalSize (in its own writing mode).
   */
  static nscoord AlignJustifySelf(uint8_t aAlignment, LogicalAxis aAxis,
                                  AlignJustifyFlags aFlags,
                                  nscoord aBaselineAdjust, nscoord aCBSize,
                                  const ReflowInput& aRI,
                                  const LogicalSize& aChildSize);
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(CSSAlignUtils::AlignJustifyFlags)

} // namespace mozilla

#endif // mozilla_CSSAlignUtils_h
