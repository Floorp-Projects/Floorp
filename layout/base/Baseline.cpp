/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Baseline.h"
#include "nsIFrame.h"

namespace mozilla {

nscoord Baseline::SynthesizeBOffsetFromMarginBox(const nsIFrame* aFrame,
                                                 WritingMode aWM,
                                                 BaselineSharingGroup aGroup) {
  MOZ_ASSERT(!aWM.IsOrthogonalTo(aFrame->GetWritingMode()));
  auto margin = aFrame->GetLogicalUsedMargin(aWM);
  if (aGroup == BaselineSharingGroup::First) {
    if (aWM.IsAlphabeticalBaseline()) {
      // First baseline for inverted-line content is the block-start margin
      // edge, as the frame is in effect "flipped" for alignment purposes.
      return MOZ_UNLIKELY(aWM.IsLineInverted())
                 ? -margin.BStart(aWM)
                 : aFrame->BSize(aWM) + margin.BEnd(aWM);
    }
    nscoord marginBoxCenter = (aFrame->BSize(aWM) + margin.BStartEnd(aWM)) / 2;
    return marginBoxCenter - margin.BStart(aWM);
  }
  MOZ_ASSERT(aGroup == BaselineSharingGroup::Last);
  if (aWM.IsAlphabeticalBaseline()) {
    // Last baseline for inverted-line content is the block-start margin edge,
    // as the frame is in effect "flipped" for alignment purposes.
    return MOZ_UNLIKELY(aWM.IsLineInverted())
               ? aFrame->BSize(aWM) + margin.BStart(aWM)
               : -margin.BEnd(aWM);
  }
  // Round up for central baseline offset, to be consistent with ::First.
  nscoord marginBoxSize = aFrame->BSize(aWM) + margin.BStartEnd(aWM);
  nscoord marginBoxCenter = (marginBoxSize / 2) + (marginBoxSize % 2);
  return marginBoxCenter - margin.BEnd(aWM);
}

nscoord Baseline::SynthesizeBOffsetFromBorderBox(const nsIFrame* aFrame,
                                                 WritingMode aWM,
                                                 BaselineSharingGroup aGroup) {
  nscoord borderBoxSize =
      MOZ_UNLIKELY(aWM.IsOrthogonalTo(aFrame->GetWritingMode()))
          ? aFrame->ISize(aWM)
          : aFrame->BSize(aWM);
  if (aGroup == BaselineSharingGroup::First) {
    if (MOZ_LIKELY(aWM.IsAlphabeticalBaseline())) {
      // First baseline for inverted-line content is the block-start border-box
      // edge, as the frame is in effect "flipped" for alignment purposes.
      return MOZ_UNLIKELY(aWM.IsLineInverted()) ? 0 : borderBoxSize;
    }
    return borderBoxSize / 2;
  }
  MOZ_ASSERT(aGroup == BaselineSharingGroup::Last);
  if (MOZ_LIKELY(aWM.IsAlphabeticalBaseline())) {
    // Last baseline for inverted-line content is the block-start border-box
    // edge, as the frame is in effect "flipped" for alignment purposes.
    return MOZ_UNLIKELY(aWM.IsLineInverted()) ? borderBoxSize : 0;
  }
  // Round up for central baseline offset, to be consistent with ::First.
  return (borderBoxSize / 2) + (borderBoxSize % 2);
}

nscoord Baseline::SynthesizeBOffsetFromContentBox(const nsIFrame* aFrame,
                                                  WritingMode aWM,
                                                  BaselineSharingGroup aGroup) {
  WritingMode wm = aFrame->GetWritingMode();
  MOZ_ASSERT(!aWM.IsOrthogonalTo(wm));
  const auto bp = aFrame->GetLogicalUsedBorderAndPadding(wm)
                      .ApplySkipSides(aFrame->GetLogicalSkipSides())
                      .ConvertTo(aWM, wm);

  if (MOZ_UNLIKELY(aWM.IsCentralBaseline())) {
    nscoord contentBoxBSize = aFrame->BSize(aWM) - bp.BStartEnd(aWM);
    if (aGroup == BaselineSharingGroup::First) {
      return contentBoxBSize / 2 + bp.BStart(aWM);
    }
    // Return the same center position as for ::First, but as offset from end:
    nscoord halfContentBoxBSize = (contentBoxBSize / 2) + (contentBoxBSize % 2);
    return halfContentBoxBSize + bp.BEnd(aWM);
  }
  if (aGroup == BaselineSharingGroup::First) {
    // First baseline for inverted-line content is the block-start content
    // edge, as the frame is in effect "flipped" for alignment purposes.
    return MOZ_UNLIKELY(aWM.IsLineInverted())
               ? bp.BStart(aWM)
               : aFrame->BSize(aWM) - bp.BEnd(aWM);
  }
  // Last baseline for inverted-line content is the block-start content edge,
  // as the frame is in effect "flipped" for alignment purposes.
  return MOZ_UNLIKELY(aWM.IsLineInverted())
             ? aFrame->BSize(aWM) - bp.BStart(aWM)
             : bp.BEnd(aWM);
}

}  // namespace mozilla
