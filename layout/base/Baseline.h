/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_BASE_BASELINE_H_
#define LAYOUT_BASE_BASELINE_H_

#include "nsCoord.h"
#include "mozilla/WritingModes.h"

class nsIFrame;

namespace mozilla {

// https://drafts.csswg.org/css-align-3/#baseline-sharing-group
enum class BaselineSharingGroup : uint8_t {
  // NOTE Used as an array index so must be 0 and 1.
  First = 0,
  Last = 1,
};

// Layout context under which the baseline is being exported to.
enum class BaselineExportContext : uint8_t {
  LineLayout = 0,
  Other = 1,
};

class Baseline {
 public:
  /**
   * Synthesize a first(last) inline-axis baseline in aWM based on aFrame's
   * margin-box.
   *
   * An alphabetical baseline is at the end edge of aFrame's margin-box with
   * respect to aWM's block-axis, and a central baseline is halfway between the
   * start and end edges. (aWM tells which baseline to use.)
   * https://drafts.csswg.org/css-align-3/#synthesize-baseline
   *
   * @note This works only when aFrame's writing-mode is parallel to aWM.
   * @param aWM the writing-mode of the alignment context.
   * @return an offset from aFrame's border-box start(end) edge in aWM's
   *         block-axis for a first(last) baseline, respectively.
   */
  static nscoord SynthesizeBOffsetFromMarginBox(const nsIFrame* aFrame,
                                                WritingMode aWM,
                                                BaselineSharingGroup);

  /**
   * Synthesize a first(last) inline-axis baseline in aWM based on aFrame's
   * border-box.
   *
   * An alphabetical baseline is at the end edge of aFrame's border-box with
   * respect to aWM's block-axis, and a central baseline is halfway between the
   * start and end edges. (aWM tells which baseline to use.)
   * https://drafts.csswg.org/css-align-3/#synthesize-baseline
   *
   * @param aWM the writing-mode of the alignment context.
   * @return an offset from aFrame's border-box start(end) edge in aWM's
   *         block-axis for a first(last) baseline, respectively.
   */
  static nscoord SynthesizeBOffsetFromBorderBox(const nsIFrame* aFrame,
                                                WritingMode aWM,
                                                BaselineSharingGroup);
  /**
   * As above, but using the content box.
   */
  static nscoord SynthesizeBOffsetFromContentBox(const nsIFrame*, WritingMode,
                                                 BaselineSharingGroup);
  /**
   * As above, but using the padding box.
   */
  static nscoord SynthesizeBOffsetFromPaddingBox(const nsIFrame*, WritingMode,
                                                 BaselineSharingGroup);
};

}  // namespace mozilla

#endif  // LAYOUT_BASE_BASELINE_H_
