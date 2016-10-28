/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utility code for performing CSS Box Alignment */

#include "CSSAlignUtils.h"

namespace mozilla {

static nscoord
SpaceToFill(WritingMode aWM, const LogicalSize& aSize, nscoord aMargin,
            LogicalAxis aAxis, nscoord aCBSize)
{
  nscoord size = aAxis == eLogicalAxisBlock ? aSize.BSize(aWM)
                                            : aSize.ISize(aWM);
  return aCBSize - (size + aMargin);
}

nscoord
CSSAlignUtils::AlignJustifySelf(uint8_t aAlignment, bool aOverflowSafe,
                                LogicalAxis aAxis, bool aSameSide,
                                nscoord aBaselineAdjust, nscoord aCBSize,
                                const ReflowInput& aRI,
                                const LogicalSize& aChildSize)
{
  MOZ_ASSERT(aAlignment != NS_STYLE_ALIGN_AUTO,
             "auto values should have resolved already");
  MOZ_ASSERT(aAlignment != NS_STYLE_ALIGN_LEFT &&
             aAlignment != NS_STYLE_ALIGN_RIGHT,
             "caller should map that to the corresponding START/END");

  // Map some alignment values to 'start' / 'end'.
  switch (aAlignment) {
    case NS_STYLE_ALIGN_SELF_START: // align/justify-self: self-start
      aAlignment = MOZ_LIKELY(aSameSide) ? NS_STYLE_ALIGN_START
                                         : NS_STYLE_ALIGN_END;
      break;
    case NS_STYLE_ALIGN_SELF_END: // align/justify-self: self-end
      aAlignment = MOZ_LIKELY(aSameSide) ? NS_STYLE_ALIGN_END
                                         : NS_STYLE_ALIGN_START;
      break;
    // flex-start/flex-end are the same as start/end, in most contexts.
    // (They have special behavior in flex containers, so flex containers
    // should map them to some other value before calling this method.)
    case NS_STYLE_ALIGN_FLEX_START:
      aAlignment = NS_STYLE_ALIGN_START;
      break;
    case NS_STYLE_ALIGN_FLEX_END:
      aAlignment = NS_STYLE_ALIGN_END;
      break;
  }

  // XXX try to condense this code a bit by adding the necessary convenience
  // methods? (bug 1209710)

  // Get the item's margin corresponding to the container's start/end side.
  const LogicalMargin margin = aRI.ComputedLogicalMargin();
  WritingMode wm = aRI.GetWritingMode();
  nscoord marginStart, marginEnd;
  if (aAxis == eLogicalAxisBlock) {
    if (MOZ_LIKELY(aSameSide)) {
      marginStart = margin.BStart(wm);
      marginEnd = margin.BEnd(wm);
    } else {
      marginStart = margin.BEnd(wm);
      marginEnd = margin.BStart(wm);
    }
  } else {
    if (MOZ_LIKELY(aSameSide)) {
      marginStart = margin.IStart(wm);
      marginEnd = margin.IEnd(wm);
    } else {
      marginStart = margin.IEnd(wm);
      marginEnd = margin.IStart(wm);
    }
  }

  const auto& styleMargin = aRI.mStyleMargin->mMargin;
  bool hasAutoMarginStart;
  bool hasAutoMarginEnd;
  if (aAxis == eLogicalAxisBlock) {
    hasAutoMarginStart = styleMargin.GetBStartUnit(wm) == eStyleUnit_Auto;
    hasAutoMarginEnd = styleMargin.GetBEndUnit(wm) == eStyleUnit_Auto;
  } else {
    hasAutoMarginStart = styleMargin.GetIStartUnit(wm) == eStyleUnit_Auto;
    hasAutoMarginEnd = styleMargin.GetIEndUnit(wm) == eStyleUnit_Auto;
  }

  // https://drafts.csswg.org/css-align-3/#overflow-values
  // This implements <overflow-position> = 'safe'.
  // And auto-margins: https://drafts.csswg.org/css-grid/#auto-margins
  if ((MOZ_UNLIKELY(aOverflowSafe) && aAlignment != NS_STYLE_ALIGN_START) ||
      hasAutoMarginStart || hasAutoMarginEnd) {
    nscoord space = SpaceToFill(wm, aChildSize, marginStart + marginEnd,
                                aAxis, aCBSize);
    // XXX we might want to include == 0 here as an optimization -
    // I need to see what the baseline/last-baseline code looks like first.
    if (space < 0) {
      // "Overflowing elements ignore their auto margins and overflow
      // in the end directions"
      aAlignment = NS_STYLE_ALIGN_START;
    } else if (hasAutoMarginEnd) {
      aAlignment = hasAutoMarginStart ? NS_STYLE_ALIGN_CENTER
                                      : (aSameSide ? NS_STYLE_ALIGN_START
                                                   : NS_STYLE_ALIGN_END);
    } else if (hasAutoMarginStart) {
      aAlignment = aSameSide ? NS_STYLE_ALIGN_END : NS_STYLE_ALIGN_START;
    }
  }

  // Determine the offset for the child frame (its border-box) which will
  // achieve the requested alignment.
  nscoord offset = 0;
  switch (aAlignment) {
    case NS_STYLE_ALIGN_BASELINE:
    case NS_STYLE_ALIGN_LAST_BASELINE:
      if (MOZ_LIKELY(aSameSide == (aAlignment == NS_STYLE_ALIGN_BASELINE))) {
        offset = marginStart + aBaselineAdjust;
      } else {
        nscoord size = aAxis == eLogicalAxisBlock ? aChildSize.BSize(wm)
                                                  : aChildSize.ISize(wm);
        offset = aCBSize - (size + marginEnd) - aBaselineAdjust;
      }
      break;
    case NS_STYLE_ALIGN_STRETCH:
      MOZ_FALLTHROUGH; // ComputeSize() deals with it
    case NS_STYLE_ALIGN_START:
      offset = marginStart;
      break;
    case NS_STYLE_ALIGN_END: {
      nscoord size = aAxis == eLogicalAxisBlock ? aChildSize.BSize(wm)
                                                : aChildSize.ISize(wm);
      offset = aCBSize - (size + marginEnd);
      break;
    }
    case NS_STYLE_ALIGN_CENTER: {
      nscoord size = aAxis == eLogicalAxisBlock ? aChildSize.BSize(wm)
                                                : aChildSize.ISize(wm);
      offset = (aCBSize - size + marginStart - marginEnd) / 2;
      break;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("unknown align-/justify-self value");
  }

  return offset;
}

} // namespace mozilla
