/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* constants used throughout the Layout module */

#ifndef LayoutConstants_h___
#define LayoutConstants_h___

#include "mozilla/EnumSet.h"
#include "nsSize.h"  // for NS_MAXSIZE
#include "Units.h"

/**
 * Constant used to indicate an unconstrained size.
 *
 * NOTE: The constants defined in this file are semantically used as symbolic
 *       values, so user should not depend on the underlying numeric values. If
 *       new specific use cases arise, define a new constant here.
 */
#define NS_UNCONSTRAINEDSIZE NS_MAXSIZE

// NS_AUTOOFFSET is assumed to have the same value as NS_UNCONSTRAINEDSIZE.
#define NS_AUTOOFFSET NS_UNCONSTRAINEDSIZE

// +1 is to avoid clamped huge margin values being processed as auto margins
#define NS_AUTOMARGIN (NS_UNCONSTRAINEDSIZE + 1)

#define NS_INTRINSIC_ISIZE_UNKNOWN nscoord_MIN

namespace mozilla {

/**
 * Bit-flags to pass to various functions that compute sizes like
 * nsIFrame::ComputeSize().
 */
enum class ComputeSizeFlag : uint8_t {
  /**
   * Set if the frame is in a context where non-replaced blocks should
   * shrink-wrap (e.g., it's floating, absolutely positioned, or
   * inline-block).
   */
  ShrinkWrap,

  /**
   * Set if we'd like to compute our 'auto' bsize, regardless of our actual
   * corresponding computed value. (e.g. to get an intrinsic bsize for flex
   * items when resolving automatic minimum size in the main axis during flexbox
   * layout.)
   */
  UseAutoBSize,

  /**
   * Indicates that we should clamp the margin-box min-size to the given CB
   * size.  This is used for implementing the grid area clamping here:
   * https://drafts.csswg.org/css-grid/#min-size-auto
   */
  IClampMarginBoxMinSize,  // clamp in our inline axis
  BClampMarginBoxMinSize,  // clamp in our block axis

  /**
   * The frame is stretching (per CSS Box Alignment) and doesn't have an
   * Automatic Minimum Size in the indicated axis.
   * (may be used for both flex/grid items, but currently only used for Grid)
   * https://drafts.csswg.org/css-grid/#min-size-auto
   * https://drafts.csswg.org/css-align-3/#valdef-justify-self-stretch
   */
  IApplyAutoMinSize,  // only has an effect when eShrinkWrap is false
};
using ComputeSizeFlags = mozilla::EnumSet<ComputeSizeFlag>;

/**
 * The fallback size of width is 300px and the aspect-ratio is 2:1, based on
 * CSS2 section 10.3.2 and CSS Sizing Level 3 section 5.1:
 * https://drafts.csswg.org/css2/visudet.html#inline-replaced-width
 * https://drafts.csswg.org/css-sizing-3/#intrinsic-sizes
 */
inline constexpr CSSIntCoord kFallbackIntrinsicWidthInPixels(300);
inline constexpr CSSIntCoord kFallbackIntrinsicHeightInPixels(150);
inline constexpr CSSIntSize kFallbackIntrinsicSizeInPixels(
    kFallbackIntrinsicWidthInPixels, kFallbackIntrinsicHeightInPixels);

inline constexpr nscoord kFallbackIntrinsicWidth =
    kFallbackIntrinsicWidthInPixels * AppUnitsPerCSSPixel();
inline constexpr nscoord kFallbackIntrinsicHeight =
    kFallbackIntrinsicHeightInPixels * AppUnitsPerCSSPixel();
inline constexpr nsSize kFallbackIntrinsicSize(kFallbackIntrinsicWidth,
                                               kFallbackIntrinsicHeight);

/**
 * This is used in some nsLayoutUtils functions.
 * Declared here so that fewer files need to include nsLayoutUtils.h.
 */
enum class IntrinsicISizeType { MinISize, PrefISize };

}  // namespace mozilla

#endif  // LayoutConstants_h___
