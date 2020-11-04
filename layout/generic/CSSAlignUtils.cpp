/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utility code for performing CSS Box Alignment */

#include "CSSAlignUtils.h"
#include "ReflowInput.h"

namespace mozilla {

static nscoord SpaceToFill(WritingMode aWM, const LogicalSize& aSize,
                           nscoord aMargin, LogicalAxis aAxis,
                           nscoord aCBSize) {
  nscoord size = aSize.Size(aAxis, aWM);
  return aCBSize - (size + aMargin);
}

nscoord CSSAlignUtils::AlignJustifySelf(const StyleAlignFlags& aAlignment,
                                        LogicalAxis aAxis,
                                        AlignJustifyFlags aFlags,
                                        nscoord aBaselineAdjust,
                                        nscoord aCBSize, const ReflowInput& aRI,
                                        const LogicalSize& aChildSize) {
  MOZ_ASSERT(aAlignment != StyleAlignFlags::AUTO,
             "auto values should have resolved already");
  MOZ_ASSERT(aAlignment != StyleAlignFlags::LEFT &&
                 aAlignment != StyleAlignFlags::RIGHT,
             "caller should map that to the corresponding START/END");

  // Promote aFlags to convenience bools:
  const bool isOverflowSafe = !!(aFlags & AlignJustifyFlags::OverflowSafe);
  const bool isSameSide = !!(aFlags & AlignJustifyFlags::SameSide);

  StyleAlignFlags alignment = aAlignment;
  // Map some alignment values to 'start' / 'end'.
  if (alignment == StyleAlignFlags::SELF_START) {
    // align/justify-self: self-start
    alignment =
        MOZ_LIKELY(isSameSide) ? StyleAlignFlags::START : StyleAlignFlags::END;
  } else if (alignment == StyleAlignFlags::SELF_END) {
    alignment =
        MOZ_LIKELY(isSameSide) ? StyleAlignFlags::END : StyleAlignFlags::START;
    // flex-start/flex-end are the same as start/end, in most contexts.
    // (They have special behavior in flex containers, so flex containers
    // should map them to some other value before calling this method.)
  } else if (alignment == StyleAlignFlags::FLEX_START) {
    alignment = StyleAlignFlags::START;
  } else if (alignment == StyleAlignFlags::FLEX_END) {
    alignment = StyleAlignFlags::END;
  }

  // Get the item's margin corresponding to the container's start/end side.
  WritingMode wm = aRI.GetWritingMode();
  const LogicalMargin margin = aRI.ComputedLogicalMargin(wm);
  const auto startSide = MakeLogicalSide(
      aAxis, MOZ_LIKELY(isSameSide) ? eLogicalEdgeStart : eLogicalEdgeEnd);
  const nscoord marginStart = margin.Side(startSide, wm);
  const auto endSide = GetOppositeSide(startSide);
  const nscoord marginEnd = margin.Side(endSide, wm);

  const auto& styleMargin = aRI.mStyleMargin->mMargin;
  bool hasAutoMarginStart;
  bool hasAutoMarginEnd;
  if (aFlags & AlignJustifyFlags::IgnoreAutoMargins) {
    // (Note: ReflowInput will have treated "auto" margins as 0, so we
    // don't need to do anything special to avoid expanding them.)
    hasAutoMarginStart = hasAutoMarginEnd = false;
  } else if (aAxis == eLogicalAxisBlock) {
    hasAutoMarginStart = styleMargin.GetBStart(wm).IsAuto();
    hasAutoMarginEnd = styleMargin.GetBEnd(wm).IsAuto();
  } else { /* aAxis == eLogicalAxisInline */
    hasAutoMarginStart = styleMargin.GetIStart(wm).IsAuto();
    hasAutoMarginEnd = styleMargin.GetIEnd(wm).IsAuto();
  }

  // https://drafts.csswg.org/css-align-3/#overflow-values
  // This implements <overflow-position> = 'safe'.
  // And auto-margins: https://drafts.csswg.org/css-grid/#auto-margins
  if ((MOZ_UNLIKELY(isOverflowSafe) && alignment != StyleAlignFlags::START) ||
      hasAutoMarginStart || hasAutoMarginEnd) {
    nscoord space =
        SpaceToFill(wm, aChildSize, marginStart + marginEnd, aAxis, aCBSize);
    // XXX we might want to include == 0 here as an optimization -
    // I need to see what the baseline/last baseline code looks like first.
    if (space < 0) {
      // "Overflowing elements ignore their auto margins and overflow
      // in the end directions"
      alignment = StyleAlignFlags::START;
    } else if (hasAutoMarginEnd) {
      alignment = hasAutoMarginStart ? StyleAlignFlags::CENTER
                                     : (isSameSide ? StyleAlignFlags::START
                                                   : StyleAlignFlags::END);
    } else if (hasAutoMarginStart) {
      alignment = isSameSide ? StyleAlignFlags::END : StyleAlignFlags::START;
    }
  }

  // Determine the offset for the child frame (its border-box) which will
  // achieve the requested alignment.
  nscoord offset = 0;
  if (alignment == StyleAlignFlags::BASELINE ||
      alignment == StyleAlignFlags::LAST_BASELINE) {
    if (MOZ_LIKELY(isSameSide == (alignment == StyleAlignFlags::BASELINE))) {
      offset = marginStart + aBaselineAdjust;
    } else {
      nscoord size = aChildSize.Size(aAxis, wm);
      offset = aCBSize - (size + marginEnd) - aBaselineAdjust;
    }
  } else if (alignment == StyleAlignFlags::STRETCH ||
             alignment == StyleAlignFlags::START) {
    // ComputeSize() deals with stretch
    offset = marginStart;
  } else if (alignment == StyleAlignFlags::END) {
    nscoord size = aChildSize.Size(aAxis, wm);
    offset = aCBSize - (size + marginEnd);
  } else if (alignment == StyleAlignFlags::CENTER) {
    nscoord size = aChildSize.Size(aAxis, wm);
    offset = (aCBSize - size + marginStart - marginEnd) / 2;
  } else {
    MOZ_ASSERT_UNREACHABLE("unknown align-/justify-self value");
  }

  return offset;
}

}  // namespace mozilla
