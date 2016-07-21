/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Algorithms that determine column and table inline sizes used for
 * CSS2's 'table-layout: fixed'.
 */

#include "FixedTableLayoutStrategy.h"
#include "nsTableFrame.h"
#include "nsTableColFrame.h"
#include "nsTableCellFrame.h"
#include <algorithm>

FixedTableLayoutStrategy::FixedTableLayoutStrategy(nsTableFrame *aTableFrame)
  : nsITableLayoutStrategy(nsITableLayoutStrategy::Fixed)
  , mTableFrame(aTableFrame)
{
  MarkIntrinsicISizesDirty();
}

/* virtual */
FixedTableLayoutStrategy::~FixedTableLayoutStrategy()
{
}

/* virtual */ nscoord
FixedTableLayoutStrategy::GetMinISize(nsRenderingContext* aRenderingContext)
{
  DISPLAY_MIN_WIDTH(mTableFrame, mMinISize);
  if (mMinISize != NS_INTRINSIC_WIDTH_UNKNOWN) {
    return mMinISize;
  }

  // It's theoretically possible to do something much better here that
  // depends only on the columns and the first row (where we look at
  // intrinsic inline sizes inside the first row and then reverse the
  // algorithm to find the narrowest inline size that would hold all of
  // those intrinsic inline sizes), but it wouldn't be compatible with
  // other browsers, or with the use of GetMinISize by
  // nsTableFrame::ComputeSize to determine the inline size of a fixed
  // layout table, since CSS2.1 says:
  //   The width of the table is then the greater of the value of the
  //   'width' property for the table element and the sum of the column
  //   widths (plus cell spacing or borders).

  // XXX Should we really ignore 'min-width' and 'max-width'?
  // XXX Should we really ignore widths on column groups?

  nsTableCellMap *cellMap = mTableFrame->GetCellMap();
  int32_t colCount = cellMap->GetColCount();

  nscoord result = 0;

  if (colCount > 0) {
    result += mTableFrame->GetColSpacing(-1, colCount);
  }

  WritingMode wm = mTableFrame->GetWritingMode();
  for (int32_t col = 0; col < colCount; ++col) {
    nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
    if (!colFrame) {
      NS_ERROR("column frames out of sync with cell map");
      continue;
    }
    nscoord spacing = mTableFrame->GetColSpacing(col);
    const nsStyleCoord *styleISize = &colFrame->StylePosition()->ISize(wm);
    if (styleISize->ConvertsToLength()) {
      result += nsLayoutUtils::ComputeISizeValue(aRenderingContext,
                                                 colFrame, 0, 0, 0,
                                                 *styleISize);
    } else if (styleISize->GetUnit() == eStyleUnit_Percent) {
      // do nothing
    } else {
      NS_ASSERTION(styleISize->GetUnit() == eStyleUnit_Auto ||
                   styleISize->GetUnit() == eStyleUnit_Enumerated ||
                   (styleISize->IsCalcUnit() && styleISize->CalcHasPercent()),
                   "bad inline size");

      // The 'table-layout: fixed' algorithm considers only cells in the
      // first row.
      bool originates;
      int32_t colSpan;
      nsTableCellFrame *cellFrame = cellMap->GetCellInfoAt(0, col, &originates,
                                                           &colSpan);
      if (cellFrame) {
        styleISize = &cellFrame->StylePosition()->ISize(wm);
        if (styleISize->ConvertsToLength() ||
            (styleISize->GetUnit() == eStyleUnit_Enumerated &&
             (styleISize->GetIntValue() == NS_STYLE_WIDTH_MAX_CONTENT ||
              styleISize->GetIntValue() == NS_STYLE_WIDTH_MIN_CONTENT))) {
          nscoord cellISize =
            nsLayoutUtils::IntrinsicForContainer(aRenderingContext, cellFrame,
                                                 nsLayoutUtils::MIN_ISIZE);
          if (colSpan > 1) {
            // If a column-spanning cell is in the first row, split up
            // the space evenly.  (XXX This isn't quite right if some of
            // the columns it's in have specified inline sizes.  Should
            // we care?)
            cellISize = ((cellISize + spacing) / colSpan) - spacing;
          }
          result += cellISize;
        } else if (styleISize->GetUnit() == eStyleUnit_Percent) {
          if (colSpan > 1) {
            // XXX Can this force columns to negative inline sizes?
            result -= spacing * (colSpan - 1);
          }
        }
        // else, for 'auto', '-moz-available', '-moz-fit-content',
        // and 'calc()' with percentages, do nothing
      }
    }
  }

  return (mMinISize = result);
}

/* virtual */ nscoord
FixedTableLayoutStrategy::GetPrefISize(nsRenderingContext* aRenderingContext,
                                       bool aComputingSize)
{
  // It's theoretically possible to do something much better here that
  // depends only on the columns and the first row (where we look at
  // intrinsic inline sizes inside the first row and then reverse the
  // algorithm to find the narrowest inline size that would hold all of
  // those intrinsic inline sizes), but it wouldn't be compatible with
  // other browsers.
  nscoord result = nscoord_MAX;
  DISPLAY_PREF_WIDTH(mTableFrame, result);
  return result;
}

/* virtual */ void
FixedTableLayoutStrategy::MarkIntrinsicISizesDirty()
{
  mMinISize = NS_INTRINSIC_WIDTH_UNKNOWN;
  mLastCalcISize = nscoord_MIN;
}

static inline nscoord
AllocateUnassigned(nscoord aUnassignedSpace, float aShare)
{
  if (aShare == 1.0f) {
    // This happens when the numbers we're dividing to get aShare are
    // equal.  We want to return unassignedSpace exactly, even if it
    // can't be precisely round-tripped through float.
    return aUnassignedSpace;
  }
  return NSToCoordRound(float(aUnassignedSpace) * aShare);
}

/* virtual */ void
FixedTableLayoutStrategy::ComputeColumnISizes(const ReflowInput& aReflowInput)
{
  nscoord tableISize = aReflowInput.ComputedISize();

  if (mLastCalcISize == tableISize) {
    return;
  }
  mLastCalcISize = tableISize;

  nsTableCellMap *cellMap = mTableFrame->GetCellMap();
  int32_t colCount = cellMap->GetColCount();

  if (colCount == 0) {
    // No Columns - nothing to compute
    return;
  }

  // border-spacing isn't part of the basis for percentages.
  tableISize -= mTableFrame->GetColSpacing(-1, colCount);

  // store the old column inline sizes. We might call SetFinalISize
  // multiple times on the columns, due to this we can't compare at the
  // last call that the inline size has changed with respect to the last
  // call to ComputeColumnISizes. In order to overcome this we store the
  // old values in this array. A single call to SetFinalISize would make
  // it possible to call GetFinalISize before and to compare when
  // setting the final inline size.
  nsTArray<nscoord> oldColISizes;

  // XXX This ignores the 'min-width' and 'max-width' properties
  // throughout.  Then again, that's what the CSS spec says to do.

  // XXX Should we really ignore widths on column groups?

  uint32_t unassignedCount = 0;
  nscoord unassignedSpace = tableISize;
  const nscoord unassignedMarker = nscoord_MIN;

  // We use the PrefPercent on the columns to store the percentages
  // used to compute column inline sizes in case we need to shrink or
  // expand the columns.
  float pctTotal = 0.0f;

  // Accumulate the total specified (non-percent) on the columns for
  // distributing excess inline size to the columns.
  nscoord specTotal = 0;

  WritingMode wm = mTableFrame->GetWritingMode();
  for (int32_t col = 0; col < colCount; ++col) {
    nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
    if (!colFrame) {
      oldColISizes.AppendElement(0);
      NS_ERROR("column frames out of sync with cell map");
      continue;
    }
    oldColISizes.AppendElement(colFrame->GetFinalISize());
    colFrame->ResetPrefPercent();
    const nsStyleCoord *styleISize = &colFrame->StylePosition()->ISize(wm);
    nscoord colISize;
    if (styleISize->ConvertsToLength()) {
      colISize = nsLayoutUtils::ComputeISizeValue(aReflowInput.mRenderingContext,
                                                  colFrame, 0, 0, 0,
                                                  *styleISize);
      specTotal += colISize;
    } else if (styleISize->GetUnit() == eStyleUnit_Percent) {
      float pct = styleISize->GetPercentValue();
      colISize = NSToCoordFloor(pct * float(tableISize));
      colFrame->AddPrefPercent(pct);
      pctTotal += pct;
    } else {
      NS_ASSERTION(styleISize->GetUnit() == eStyleUnit_Auto ||
                   styleISize->GetUnit() == eStyleUnit_Enumerated ||
                   (styleISize->IsCalcUnit() && styleISize->CalcHasPercent()),
                   "bad inline size");

      // The 'table-layout: fixed' algorithm considers only cells in the
      // first row.
      bool originates;
      int32_t colSpan;
      nsTableCellFrame *cellFrame = cellMap->GetCellInfoAt(0, col, &originates,
                                                           &colSpan);
      if (cellFrame) {
        const nsStylePosition* cellStylePos = cellFrame->StylePosition();
        styleISize = &cellStylePos->ISize(wm);
        if (styleISize->ConvertsToLength() ||
            (styleISize->GetUnit() == eStyleUnit_Enumerated &&
             (styleISize->GetIntValue() == NS_STYLE_WIDTH_MAX_CONTENT ||
              styleISize->GetIntValue() == NS_STYLE_WIDTH_MIN_CONTENT))) {
          // XXX This should use real percentage padding
          // Note that the difference between MIN_ISIZE and PREF_ISIZE
          // shouldn't matter for any of these values of styleISize; use
          // MIN_ISIZE for symmetry with GetMinISize above, just in case
          // there is a difference.
          colISize =
            nsLayoutUtils::IntrinsicForContainer(aReflowInput.mRenderingContext,
                                                 cellFrame,
                                                 nsLayoutUtils::MIN_ISIZE);
        } else if (styleISize->GetUnit() == eStyleUnit_Percent) {
          // XXX This should use real percentage padding
          float pct = styleISize->GetPercentValue();
          colISize = NSToCoordFloor(pct * float(tableISize));

          if (cellStylePos->mBoxSizing == StyleBoxSizing::Content) {
            nsIFrame::IntrinsicISizeOffsetData offsets =
              cellFrame->IntrinsicISizeOffsets();
            colISize += offsets.hPadding + offsets.hBorder;
          }

          pct /= float(colSpan);
          colFrame->AddPrefPercent(pct);
          pctTotal += pct;
        } else {
          // 'auto', '-moz-available', '-moz-fit-content', and 'calc()'
          // with percentages
          colISize = unassignedMarker;
        }
        if (colISize != unassignedMarker) {
          if (colSpan > 1) {
            // If a column-spanning cell is in the first row, split up
            // the space evenly.  (XXX This isn't quite right if some of
            // the columns it's in have specified iSizes.  Should we
            // care?)
            nscoord spacing = mTableFrame->GetColSpacing(col);
            colISize = ((colISize + spacing) / colSpan) - spacing;
            if (colISize < 0) {
              colISize = 0;
            }
          }
          if (styleISize->GetUnit() != eStyleUnit_Percent) {
            specTotal += colISize;
          }
        }
      } else {
        colISize = unassignedMarker;
      }
    }

    colFrame->SetFinalISize(colISize);

    if (colISize == unassignedMarker) {
      ++unassignedCount;
    } else {
      unassignedSpace -= colISize;
    }
  }

  if (unassignedSpace < 0) {
    if (pctTotal > 0) {
      // If the columns took up too much space, reduce those that had
      // percentage inline sizes.  The spec doesn't say to do this, but
      // we've always done it in the past, and so does WinIE6.
      nscoord pctUsed = NSToCoordFloor(pctTotal * float(tableISize));
      nscoord reduce = std::min(pctUsed, -unassignedSpace);
      float reduceRatio = float(reduce) / pctTotal;
      for (int32_t col = 0; col < colCount; ++col) {
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
        if (!colFrame) {
          NS_ERROR("column frames out of sync with cell map");
          continue;
        }
        nscoord colISize = colFrame->GetFinalISize();
        colISize -= NSToCoordFloor(colFrame->GetPrefPercent() * reduceRatio);
        if (colISize < 0) {
          colISize = 0;
        }
        colFrame->SetFinalISize(colISize);
      }
    }
    unassignedSpace = 0;
  }

  if (unassignedCount > 0) {
    // The spec says to distribute the remaining space evenly among
    // the columns.
    nscoord toAssign = unassignedSpace / unassignedCount;
    for (int32_t col = 0; col < colCount; ++col) {
      nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
      if (!colFrame) {
        NS_ERROR("column frames out of sync with cell map");
        continue;
      }
      if (colFrame->GetFinalISize() == unassignedMarker) {
        colFrame->SetFinalISize(toAssign);
      }
    }
  } else if (unassignedSpace > 0) {
    // The spec doesn't say how to distribute the unassigned space.
    if (specTotal > 0) {
      // Distribute proportionally to non-percentage columns.
      nscoord specUndist = specTotal;
      for (int32_t col = 0; col < colCount; ++col) {
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
        if (!colFrame) {
          NS_ERROR("column frames out of sync with cell map");
          continue;
        }
        if (colFrame->GetPrefPercent() == 0.0f) {
          NS_ASSERTION(colFrame->GetFinalISize() <= specUndist,
                       "inline sizes don't add up");
          nscoord toAdd = AllocateUnassigned(unassignedSpace,
                                             float(colFrame->GetFinalISize()) /
                                              float(specUndist));
          specUndist -= colFrame->GetFinalISize();
          colFrame->SetFinalISize(colFrame->GetFinalISize() + toAdd);
          unassignedSpace -= toAdd;
          if (specUndist <= 0) {
            NS_ASSERTION(specUndist == 0, "math should be exact");
            break;
          }
        }
      }
      NS_ASSERTION(unassignedSpace == 0, "failed to redistribute");
    } else if (pctTotal > 0) {
      // Distribute proportionally to percentage columns.
      float pctUndist = pctTotal;
      for (int32_t col = 0; col < colCount; ++col) {
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
        if (!colFrame) {
          NS_ERROR("column frames out of sync with cell map");
          continue;
        }
        if (pctUndist < colFrame->GetPrefPercent()) {
          // This can happen with floating-point math.
          NS_ASSERTION(colFrame->GetPrefPercent() - pctUndist < 0.0001,
                       "inline sizes don't add up");
          pctUndist = colFrame->GetPrefPercent();
        }
        nscoord toAdd = AllocateUnassigned(unassignedSpace,
                                           colFrame->GetPrefPercent() /
                                             pctUndist);
        colFrame->SetFinalISize(colFrame->GetFinalISize() + toAdd);
        unassignedSpace -= toAdd;
        pctUndist -= colFrame->GetPrefPercent();
        if (pctUndist <= 0.0f) {
          break;
        }
      }
      NS_ASSERTION(unassignedSpace == 0, "failed to redistribute");
    } else {
      // Distribute equally to the zero-iSize columns.
      int32_t colsRemaining = colCount;
      for (int32_t col = 0; col < colCount; ++col) {
        nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
        if (!colFrame) {
          NS_ERROR("column frames out of sync with cell map");
          continue;
        }
        NS_ASSERTION(colFrame->GetFinalISize() == 0, "yikes");
        nscoord toAdd = AllocateUnassigned(unassignedSpace,
                                           1.0f / float(colsRemaining));
        colFrame->SetFinalISize(toAdd);
        unassignedSpace -= toAdd;
        --colsRemaining;
      }
      NS_ASSERTION(unassignedSpace == 0, "failed to redistribute");
    }
  }
  for (int32_t col = 0; col < colCount; ++col) {
    nsTableColFrame *colFrame = mTableFrame->GetColFrame(col);
    if (!colFrame) {
      NS_ERROR("column frames out of sync with cell map");
      continue;
    }
    if (oldColISizes.ElementAt(col) != colFrame->GetFinalISize()) {
      mTableFrame->DidResizeColumns();
      break;
    }
  }
}
