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

enum class BoxType { Border, Padding, Content };

template <BoxType aType>
static nscoord SynthesizeBOffsetFromInnerBox(const nsIFrame* aFrame,
                                             WritingMode aWM,
                                             BaselineSharingGroup aGroup) {
  WritingMode wm = aFrame->GetWritingMode();
  MOZ_ASSERT_IF(aType != BoxType::Border, !aWM.IsOrthogonalTo(wm));
  const nscoord borderBoxSize = MOZ_UNLIKELY(aWM.IsOrthogonalTo(wm))
                                    ? aFrame->ISize(aWM)
                                    : aFrame->BSize(aWM);
  const LogicalMargin bp = ([&] {
    switch (aType) {
      case BoxType::Border:
        return LogicalMargin(aWM);
      case BoxType::Padding:
        return aFrame->GetLogicalUsedBorder(wm)
            .ApplySkipSides(aFrame->GetLogicalSkipSides())
            .ConvertTo(aWM, wm);
      case BoxType::Content:
        return aFrame->GetLogicalUsedBorderAndPadding(wm)
            .ApplySkipSides(aFrame->GetLogicalSkipSides())
            .ConvertTo(aWM, wm);
    }
    MOZ_CRASH();
  })();
  if (MOZ_UNLIKELY(aWM.IsCentralBaseline())) {
    nscoord boxBSize = borderBoxSize - bp.BStartEnd(aWM);
    if (aGroup == BaselineSharingGroup::First) {
      return boxBSize / 2 + bp.BStart(aWM);
    }
    // Return the same center position as for ::First, but as offset from end:
    nscoord halfBoxBSize = (boxBSize / 2) + (boxBSize % 2);
    return halfBoxBSize + bp.BEnd(aWM);
  }
  if (aGroup == BaselineSharingGroup::First) {
    // First baseline for inverted-line content is the block-start content
    // edge, as the frame is in effect "flipped" for alignment purposes.
    return MOZ_UNLIKELY(aWM.IsLineInverted()) ? bp.BStart(aWM)
                                              : borderBoxSize - bp.BEnd(aWM);
  }
  // Last baseline for inverted-line content is the block-start content edge,
  // as the frame is in effect "flipped" for alignment purposes.
  return MOZ_UNLIKELY(aWM.IsLineInverted()) ? borderBoxSize - bp.BStart(aWM)
                                            : bp.BEnd(aWM);
}

nscoord Baseline::SynthesizeBOffsetFromContentBox(const nsIFrame* aFrame,
                                                  WritingMode aWM,
                                                  BaselineSharingGroup aGroup) {
  return SynthesizeBOffsetFromInnerBox<BoxType::Content>(aFrame, aWM, aGroup);
}

nscoord Baseline::SynthesizeBOffsetFromPaddingBox(const nsIFrame* aFrame,
                                                  WritingMode aWM,
                                                  BaselineSharingGroup aGroup) {
  return SynthesizeBOffsetFromInnerBox<BoxType::Padding>(aFrame, aWM, aGroup);
}

nscoord Baseline::SynthesizeBOffsetFromBorderBox(const nsIFrame* aFrame,
                                                 WritingMode aWM,
                                                 BaselineSharingGroup aGroup) {
  return SynthesizeBOffsetFromInnerBox<BoxType::Border>(aFrame, aWM, aGroup);
}

}  // namespace mozilla
