/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BaselineUtils_h___
#define BaselineUtils_h___

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

class Baseline {
 public:
  /**
   * Synthesize a first(last) inline-axis baseline based on our margin-box.
   */
  static nscoord SynthesizeBOffsetFromMarginBox(const nsIFrame*, WritingMode,
                                                BaselineSharingGroup);

  /**
   * Synthesize a first(last) inline-axis baseline based on our border-box.
   */
  static nscoord SynthesizeBOffsetFromBorderBox(const nsIFrame*, WritingMode,
                                                BaselineSharingGroup);

  /**
   * Synthesize a first(last) inline-axis baseline based on our content-box.
   */
  static nscoord SynthesizeBOffsetFromContentBox(const nsIFrame*, WritingMode,
                                                 BaselineSharingGroup);
};

}  // namespace mozilla
#endif  // BaselineUtils_h___
