/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsTableColFrame_h__
#define nsTableColFrame_h__

#include "nscore.h"
#include "nsContainerFrame.h"
#include "nsTablePainter.h"

class nsVoidArray;
class nsTableCellFrame;

enum nsTableColType {
  eColContent            = 0, // there is real col content associated   
  eColAnonymousCol       = 1, // the result of a span on a col
  eColAnonymousColGroup  = 2, // the result of a span on a col group
  eColAnonymousCell      = 3  // the result of a cell alone
};

class nsTableColFrame : public nsFrame {
public:

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
  friend nsIFrame* NS_NewTableColFrame(nsIPresShell* aPresShell, nsStyleContext*  aContext);

  PRInt32 GetColIndex() const;
  
  void SetColIndex (PRInt32 aColIndex);

  nsTableColFrame* GetNextCol() const;

  NS_IMETHOD Init(nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIFrame*        aPrevInFlow);

  NS_IMETHOD Reflow(nsPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

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
  virtual nsIAtom* GetType() const;
  
#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  /** return the number of the columns the col represents.  always >= 0 */
  virtual PRInt32 GetSpan ();

  /** convenience method, calls into cellmap */
  nsVoidArray * GetCells();

  /** convenience method, calls into cellmap */
  PRInt32 Count() const;

  nscoord GetLeftBorderWidth(float* aPixelsToTwips = nsnull);
  void    SetLeftBorderWidth(BCPixelSize aWidth);
  nscoord GetRightBorderWidth(float* aPixelsToTwips = nsnull);
  void    SetRightBorderWidth(BCPixelSize aWidth);

  /**
   * Gets inner border widths before collapsing with cell borders
   * Caller must get left border from previous column or from table
   * GetContinuousBCBorderWidth will not overwrite aBorder.left
   * see nsTablePainter about continuous borders
   *
   * @return outer right border width (left inner for next column)
   */
  nscoord GetContinuousBCBorderWidth(float     aPixelsToTwips,
                                     nsMargin& aBorder);
  /**
   * Set full border widths before collapsing with cell borders
   * @param aForSide - side to set; only valid for top, right, and bottom
   */
  void SetContinuousBCBorderWidth(PRUint8     aForSide,
                                  BCPixelSize aPixelValue);
#ifdef DEBUG
  void Dump(PRInt32 aIndent);
#endif

  // The largest min-width of the cells.
  void ResetMinCoord() {
    mMinCoord = 0;
  }
  void AddMinCoord(nscoord aMinCoord) {
    if (aMinCoord > mMinCoord)
      mMinCoord = aMinCoord;
    // Needed in case mHasSpecifiedCoord is true.
    if (aMinCoord > mPrefCoord)
      mPrefCoord = aMinCoord;
  }
  nscoord GetMinCoord() {
    return mMinCoord;
  }

  // The largest pref-width of the cells
  void ResetPrefCoord() {
    mPrefCoord = 0;
    mHasSpecifiedCoord = PR_FALSE;
  }
  void AddPrefCoord(nscoord aPrefCoord, PRBool aHasSpecifiedCoord) {
    if (aHasSpecifiedCoord) {
      if (!mHasSpecifiedCoord) {
        mPrefCoord = mMinCoord;
      }
      mHasSpecifiedCoord = PR_TRUE;
    }
    if (aPrefCoord > mPrefCoord &&
        (aHasSpecifiedCoord || !mHasSpecifiedCoord)) {
      mPrefCoord = aPrefCoord;
    }
  }
  nscoord GetPrefCoord() {
    return mPrefCoord;
  }

  // Whether to expand greater than pref width more conservatively
  // because the width was specified.
  PRBool GetHasSpecifiedCoord() {
    return mHasSpecifiedCoord;
  }

  // The largest specified percentage width of the cells.
  void ResetPrefPercent() {
    mPrefPercent = 0.0f;
  }
  void AddPrefPercent(float aPrefPercent) {
    if (aPrefPercent > mPrefPercent)
      mPrefPercent = aPrefPercent;
  }
  float GetPrefPercent() {
    return mPrefPercent;
  }

  // The largest min-width of the cells (for column-spanning cells).
  void ResetSpanMinCoord() {
    mSpanMinCoord = 0;
  }
  void AddSpanMinCoord(nscoord aSpanMinCoord) {
    if (aSpanMinCoord > mSpanMinCoord)
      mSpanMinCoord = aSpanMinCoord;
  }
  nscoord GetSpanMinCoord() {
    return mSpanMinCoord;
  }

  // The largest pref-width of the column-spanning cells.
  void ResetSpanPrefCoord() {
    mSpanPrefCoord = 0;
  }
  void AddSpanPrefCoord(nscoord aSpanPrefCoord) {
    if (aSpanPrefCoord > mSpanPrefCoord)
      mSpanPrefCoord = aSpanPrefCoord;
  }
  nscoord GetSpanPrefCoord() {
    return mSpanPrefCoord;
  }

  // The largest specified percentage width of the column-spanning cells.
  void ResetSpanPrefPercent() {
    mSpanPrefPercent = 0.0f;
  }
  void AddSpanPrefPercent(float aSpanPrefPercent) {
    if (aSpanPrefPercent > mSpanPrefPercent)
      mSpanPrefPercent = aSpanPrefPercent;
  }
  float GetSpanPrefPercent() {
    return mSpanPrefPercent;
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

protected:

  nsTableColFrame(nsStyleContext* aContext);
  ~nsTableColFrame();

  // the index of the column with respect to the whole tabble (starting at 0) 
  // it should never be smaller then the start column index of the parent 
  // colgroup
  PRUint32 mColIndex:        16;
  
  // border width in pixels of the inner half of the border only
  BCPixelSize mLeftBorderWidth;
  BCPixelSize mRightBorderWidth;
  BCPixelSize mTopContBorderWidth;
  BCPixelSize mRightContBorderWidth;
  BCPixelSize mBottomContBorderWidth;

  PRPackedBool mHasSpecifiedCoord;
  nscoord mMinCoord;
  nscoord mPrefCoord;
  nscoord mSpanMinCoord; // XXX...
  nscoord mSpanPrefCoord; // XXX...
  float mPrefPercent;
  float mSpanPrefPercent; // XXX...
  // ...XXX the three members marked above could be allocated as part of
  // a separate array allocated only during
  // BasicTableLayoutStrategy::ComputeColumnIntrinsicWidths (and only
  // when colspans were present).
  nscoord mFinalWidth;
};

inline PRInt32 nsTableColFrame::GetColIndex() const
{
  return mColIndex; 
}

inline void nsTableColFrame::SetColIndex (PRInt32 aColIndex)
{ 
  mColIndex = aColIndex; 
}

inline nscoord nsTableColFrame::GetLeftBorderWidth(float*  aPixelsToTwips)
{
  nscoord width = (aPixelsToTwips) ? NSToCoordRound(*aPixelsToTwips * mLeftBorderWidth) : mLeftBorderWidth;
  return width;
}

inline void nsTableColFrame::SetLeftBorderWidth(BCPixelSize aWidth)
{
  mLeftBorderWidth = aWidth;
}

inline nscoord nsTableColFrame::GetRightBorderWidth(float*  aPixelsToTwips)
{
  nscoord width = (aPixelsToTwips) ? NSToCoordRound(*aPixelsToTwips * mRightBorderWidth) : mRightBorderWidth;
  return width;
}

inline void nsTableColFrame::SetRightBorderWidth(BCPixelSize aWidth)
{
  mRightBorderWidth = aWidth;
}

inline nscoord
nsTableColFrame::GetContinuousBCBorderWidth(float     aPixelsToTwips,
                                            nsMargin& aBorder)
{
  aBorder.top = BC_BORDER_BOTTOM_HALF_COORD(aPixelsToTwips,
                                            mTopContBorderWidth);
  aBorder.right = BC_BORDER_LEFT_HALF_COORD(aPixelsToTwips,
                                            mRightContBorderWidth);
  aBorder.bottom = BC_BORDER_TOP_HALF_COORD(aPixelsToTwips,
                                            mBottomContBorderWidth);
  return BC_BORDER_RIGHT_HALF_COORD(aPixelsToTwips, mRightContBorderWidth);
}

#endif

