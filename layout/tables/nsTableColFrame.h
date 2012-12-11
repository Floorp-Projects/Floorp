/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsTableColFrame_h__
#define nsTableColFrame_h__

#include "mozilla/Attributes.h"
#include "nscore.h"
#include "nsContainerFrame.h"
#include "nsTablePainter.h"
#include "nsTArray.h"

class nsTableCellFrame;

enum nsTableColType {
  eColContent            = 0, // there is real col content associated   
  eColAnonymousCol       = 1, // the result of a span on a col
  eColAnonymousColGroup  = 2, // the result of a span on a col group
  eColAnonymousCell      = 3  // the result of a cell alone
};

class nsTableColFrame : public nsSplittableFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  enum {eWIDTH_SOURCE_NONE          =0,   // no cell has contributed to the width style
        eWIDTH_SOURCE_CELL          =1,   // a cell specified a width
        eWIDTH_SOURCE_CELL_WITH_SPAN=2    // a cell implicitly specified a width via colspan
  };

  nsTableColType GetColType() const;
  void SetColType(nsTableColType aType);

  /** instantiate a new instance of nsTableRowFrame.
    * @param aPresShell the pres shell for this frame
    *
    * @return           the frame that was created
    */
  friend nsTableColFrame* NS_NewTableColFrame(nsIPresShell* aPresShell,
                                              nsStyleContext*  aContext);
  /** @see nsIFrame::DidSetStyleContext */
  virtual void DidSetStyleContext(nsStyleContext* aOldStyleContext) MOZ_OVERRIDE;
  
  int32_t GetColIndex() const;
  
  void SetColIndex (int32_t aColIndex);

  nsTableColFrame* GetNextCol() const;

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  /**
   * Table columns never paint anything, nor receive events.
   */
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                              const nsRect&           aDirtyRect,
                              const nsDisplayListSet& aLists) { return NS_OK; }

  /**
   * Get the "type" of the frame
   *
   * @see nsGkAtoms::tableColFrame
   */
  virtual nsIAtom* GetType() const MOZ_OVERRIDE;
  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const MOZ_OVERRIDE;
#endif

  virtual nsSplittableType GetSplittableType() const MOZ_OVERRIDE;

  /** return the number of the columns the col represents.  always >= 1 */
  int32_t GetSpan();

  /** convenience method, calls into cellmap */
  int32_t Count() const;

  nscoord GetLeftBorderWidth();
  void    SetLeftBorderWidth(BCPixelSize aWidth);
  nscoord GetRightBorderWidth();
  void    SetRightBorderWidth(BCPixelSize aWidth);

  /**
   * Gets inner border widths before collapsing with cell borders
   * Caller must get left border from previous column or from table
   * GetContinuousBCBorderWidth will not overwrite aBorder.left
   * see nsTablePainter about continuous borders
   *
   * @return outer right border width (left inner for next column)
   */
  nscoord GetContinuousBCBorderWidth(nsMargin& aBorder);
  /**
   * Set full border widths before collapsing with cell borders
   * @param aForSide - side to set; only valid for top, right, and bottom
   */
  void SetContinuousBCBorderWidth(uint8_t     aForSide,
                                  BCPixelSize aPixelValue);
#ifdef DEBUG
  void Dump(int32_t aIndent);
#endif

  /**
   * Restore the default values of the intrinsic widths, so that we can
   * re-accumulate intrinsic widths from the cells in the column.
   */
  void ResetIntrinsics() {
    mMinCoord = 0;
    mPrefCoord = 0;
    mPrefPercent = 0.0f;
    mHasSpecifiedCoord = false;
  }

  /**
   * Restore the default value of the preferred percentage width (the
   * only intrinsic width used by FixedTableLayoutStrategy.
   */
  void ResetPrefPercent() {
    mPrefPercent = 0.0f;
  }

  /**
   * Restore the default values of the temporary buffer for
   * spanning-cell intrinsic widths (as we process spanning cells).
   */
  void ResetSpanIntrinsics() {
    mSpanMinCoord = 0;
    mSpanPrefCoord = 0;
    mSpanPrefPercent = 0.0f;
  }

  /**
   * Add the widths for a cell or column element, or the contribution of
   * the widths from a column-spanning cell:
   * @param aMinCoord The minimum intrinsic width
   * @param aPrefCoord The preferred intrinsic width or, if there is a
   *   specified non-percentage width, max(specified width, minimum intrinsic
   *   width).
   * @param aHasSpecifiedCoord Whether there is a specified
   *   non-percentage width.
   *
   * Note that the implementation of this functions is a bit tricky
   * since mPrefCoord means different things depending on
   * whether mHasSpecifiedCoord is true (and likewise for aPrefCoord and
   * aHasSpecifiedCoord).  If mHasSpecifiedCoord is false, then
   * all widths added had aHasSpecifiedCoord false and mPrefCoord is the
   * largest of the pref widths.  But if mHasSpecifiedCoord is true,
   * then mPrefCoord is the largest of (1) the pref widths for cells
   * with aHasSpecifiedCoord true and (2) the min widths for cells with
   * aHasSpecifiedCoord false.
   */
  void AddCoords(nscoord aMinCoord, nscoord aPrefCoord,
                 bool aHasSpecifiedCoord) {
    NS_ASSERTION(aMinCoord <= aPrefCoord, "intrinsic widths out of order");

    if (aHasSpecifiedCoord && !mHasSpecifiedCoord) {
      mPrefCoord = mMinCoord;
      mHasSpecifiedCoord = true;
    }
    if (!aHasSpecifiedCoord && mHasSpecifiedCoord) {
      aPrefCoord = aMinCoord; // NOTE: modifying argument
    }

    if (aMinCoord > mMinCoord)
      mMinCoord = aMinCoord;
    if (aPrefCoord > mPrefCoord)
      mPrefCoord = aPrefCoord;

    NS_ASSERTION(mMinCoord <= mPrefCoord, "min larger than pref");
  }

  /**
   * Add a percentage width specified on a cell or column element or the
   * contribution to this column of a percentage width specified on a
   * column-spanning cell.
   */
  void AddPrefPercent(float aPrefPercent) {
    if (aPrefPercent > mPrefPercent)
      mPrefPercent = aPrefPercent;
  }

  /**
   * Get the largest minimum intrinsic width for this column.
   */
  nscoord GetMinCoord() const { return mMinCoord; }
  /**
   * Get the largest preferred width for this column, or, if there were
   * any specified non-percentage widths (see GetHasSpecifiedCoord), the
   * largest minimum intrinsic width or specified width.
   */
  nscoord GetPrefCoord() const { return mPrefCoord; }
  /**
   * Get whether there were any specified widths contributing to this
   * column.
   */
  bool GetHasSpecifiedCoord() const { return mHasSpecifiedCoord; }

  /**
   * Get the largest specified percentage width contributing to this
   * column (returns 0 if there were none).
   */
  float GetPrefPercent() const { return mPrefPercent; }

  /**
   * Like AddCoords, but into a temporary buffer used for groups of
   * column-spanning cells.
   */
  void AddSpanCoords(nscoord aSpanMinCoord, nscoord aSpanPrefCoord,
                     bool aSpanHasSpecifiedCoord) {
    NS_ASSERTION(aSpanMinCoord <= aSpanPrefCoord,
                 "intrinsic widths out of order");

    if (!aSpanHasSpecifiedCoord && mHasSpecifiedCoord) {
      aSpanPrefCoord = aSpanMinCoord; // NOTE: modifying argument
    }

    if (aSpanMinCoord > mSpanMinCoord)
      mSpanMinCoord = aSpanMinCoord;
    if (aSpanPrefCoord > mSpanPrefCoord)
      mSpanPrefCoord = aSpanPrefCoord;

    NS_ASSERTION(mSpanMinCoord <= mSpanPrefCoord, "min larger than pref");
  }

  /*
   * Accumulate percentage widths on column spanning cells into
   * temporary variables.
   */
  void AddSpanPrefPercent(float aSpanPrefPercent) {
    if (aSpanPrefPercent > mSpanPrefPercent)
      mSpanPrefPercent = aSpanPrefPercent;
  }

  /*
   * Accumulate the temporary variables for column spanning cells into
   * the primary variables.
   */
  void AccumulateSpanIntrinsics() {
    AddCoords(mSpanMinCoord, mSpanPrefCoord, mHasSpecifiedCoord);
    AddPrefPercent(mSpanPrefPercent);
  }

  // Used to adjust a column's pref percent so that the table's total
  // never exceeeds 100% (by only allowing percentages to be used,
  // starting at the first column, until they reach 100%).
  void AdjustPrefPercent(float *aTableTotalPercent) {
    float allowed = 1.0f - *aTableTotalPercent;
    if (mPrefPercent > allowed)
      mPrefPercent = allowed;
    *aTableTotalPercent += mPrefPercent;
  }

  // The final width of the column.
  void ResetFinalWidth() {
    mFinalWidth = nscoord_MIN; // so we detect that it changed
  }
  void SetFinalWidth(nscoord aFinalWidth) {
    mFinalWidth = aFinalWidth;
  }
  nscoord GetFinalWidth() {
    return mFinalWidth;
  }
  
  virtual void InvalidateFrame(uint32_t aDisplayItemKey = 0) MOZ_OVERRIDE;
  virtual void InvalidateFrameWithRect(const nsRect& aRect, uint32_t aDisplayItemKey = 0) MOZ_OVERRIDE;
  virtual void InvalidateFrameForRemoval() MOZ_OVERRIDE { InvalidateFrameSubtree(); }

protected:

  nsTableColFrame(nsStyleContext* aContext);
  ~nsTableColFrame();

  nscoord mMinCoord;
  nscoord mPrefCoord;
  nscoord mSpanMinCoord; // XXX...
  nscoord mSpanPrefCoord; // XXX...
  float mPrefPercent;
  float mSpanPrefPercent; // XXX...
  // ...XXX the four members marked above could be allocated as part of
  // a separate array allocated only during
  // BasicTableLayoutStrategy::ComputeColumnIntrinsicWidths (and only
  // when colspans were present).
  nscoord mFinalWidth;

  // the index of the column with respect to the whole table (starting at 0) 
  // it should never be smaller then the start column index of the parent 
  // colgroup
  uint32_t mColIndex;
  
  // border width in pixels of the inner half of the border only
  BCPixelSize mLeftBorderWidth;
  BCPixelSize mRightBorderWidth;
  BCPixelSize mTopContBorderWidth;
  BCPixelSize mRightContBorderWidth;
  BCPixelSize mBottomContBorderWidth;

  bool mHasSpecifiedCoord;
};

inline int32_t nsTableColFrame::GetColIndex() const
{
  return mColIndex; 
}

inline void nsTableColFrame::SetColIndex (int32_t aColIndex)
{ 
  mColIndex = aColIndex; 
}

inline nscoord nsTableColFrame::GetLeftBorderWidth()
{
  return mLeftBorderWidth;
}

inline void nsTableColFrame::SetLeftBorderWidth(BCPixelSize aWidth)
{
  mLeftBorderWidth = aWidth;
}

inline nscoord nsTableColFrame::GetRightBorderWidth()
{
  return mRightBorderWidth;
}

inline void nsTableColFrame::SetRightBorderWidth(BCPixelSize aWidth)
{
  mRightBorderWidth = aWidth;
}

inline nscoord
nsTableColFrame::GetContinuousBCBorderWidth(nsMargin& aBorder)
{
  int32_t aPixelsToTwips = nsPresContext::AppUnitsPerCSSPixel();
  aBorder.top = BC_BORDER_BOTTOM_HALF_COORD(aPixelsToTwips,
                                            mTopContBorderWidth);
  aBorder.right = BC_BORDER_LEFT_HALF_COORD(aPixelsToTwips,
                                            mRightContBorderWidth);
  aBorder.bottom = BC_BORDER_TOP_HALF_COORD(aPixelsToTwips,
                                            mBottomContBorderWidth);
  return BC_BORDER_RIGHT_HALF_COORD(aPixelsToTwips, mRightContBorderWidth);
}

#endif

