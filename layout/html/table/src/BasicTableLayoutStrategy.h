/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * buster@netscape.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef BasicTableLayoutStrategy_h__
#define BasicTableLayoutStrategy_h__

#include "nscore.h"
#include "nsITableLayoutStrategy.h"
#include "nsCoord.h"

class nsVoidArray;
class nsTableFrame;
struct nsStylePosition;
struct nsStyleTable;

static const PRInt32 kUninitialized=-1;

/* ---------- BasicTableLayoutStrategy ---------- */

/** Implementation of Nav4 compatible HTML browser table layout.
  * The input to this class is the results from pass1 table layout.
  * The output from this class is to set the column widths in
  * mTableFrame.
  */
class BasicTableLayoutStrategy : public nsITableLayoutStrategy
{
public:

  /** Public constructor.
    * @paran aFrame           - the table frame for which this delegate will do layout
    * @param aIsNavQuirksMode - honor NN4x table quirks
    */
  BasicTableLayoutStrategy(nsTableFrame *aFrame,
                           PRBool        aIsNavQuirksMode = PR_TRUE);

  /** destructor */
  virtual ~BasicTableLayoutStrategy();

  /** call every time any table thing changes that might effect the width of any column
    * in the table (content, structure, or style) 
    * @param aPresContext - the presentation context
	   * @param aReflowState - the reflow state for mTableFrame
    */
  virtual PRBool Initialize(nsIPresContext*          aPresContext,
                            const nsHTMLReflowState& aReflowState);

  /** Called during resize reflow to determine the new column widths
    * @param aPresContext - the presentation context
	   * @param aReflowState - the reflow state for mTableFrame
	   */
  virtual PRBool BalanceColumnWidths(nsIPresContext*          aPresContext,
                                     const nsHTMLReflowState& aReflowState);
 
  /**
    * Calculate the basis for percent width calculations of the table elements
    * @param aReflowState   - the reflow state of the table
    * @param aAvailWidth    - the available width for the table
    * @param aPixelToTwips  - the number of twips in a pixel.
    * @return               - the basis for percent calculations
    */
  virtual nscoord CalcPctAdjTableWidth(const nsHTMLReflowState& aReflowState,
                                       nscoord                  aAvailWidth,
                                       float                    aPixelToTwips);
  void Dump(PRInt32 aIndent);

protected:

  /** assign widths for each column.
    * if the column has a fixed coord width, use it.
    * if the column includes col spanning cells, 
    * then distribute the fixed space between cells proportionately.
    * Computes the minimum and maximum table widths. 
    * Set column width information in each column frame and in the table frame.
    *
    * @param aMaxWidth - the computed width of the table or 
    *  UNCONSTRAINED_SIZE if an auto width table
    * @return PR_TRUE has  a pct cell or col, PR_FALSE otherwise
    */
  virtual PRBool AssignNonPctColumnWidths(nsIPresContext*          aPresContext,
                                          nscoord                  aComputedWidth,
                                          const nsHTMLReflowState& aReflowState,
                                          float                    aPixelToTwips);

  /** 
    * Calculate the adjusted widths (min, desired, fixed, or pct) for a cell
    * spanning multiple columns. 
    * @param aReflowState - the reflow state of the table
    * @param aConsiderPct - if true, consider columns that have pct widths and are spanned by the cell
    * @param aPixelToTwips- the number of twips in a pixel.
    * @param aHasPctCol   - if not null, then set *aHasPctCol to true if there is a pct cell or col
    */
  void ComputeNonPctColspanWidths(const nsHTMLReflowState& aReflowState,
                                  PRBool                   aConsiderPct,
                                  float                    aPixelToTwips,
                                  PRBool*                  aHasPctCol);

  /** 
    * main helper for above. For min width calculations, it can get called up to
    * 3 times, 1st to let constrained pct cols reach their limit, 2nd 
    * to let fix cols reach their limit and 3rd to spread any remainder among 
    * auto cols. If there are no auto cols then only constrained cols are considered.
    * @param aWidthIndex  - the width to calculate (see nsTableColFrame.h for enums)
    * @param aCellFrame   - the frame of the cell with a colspan
    * @param aCellWidth   - the width of the cell with a colspan
    * @param aColIndex    - the column index of the cell in the table
    * @param aColSpan     - the colspan of the cell
    * @param aLimitType   - the type of limit (ie. pct, fix, none).
    * @param aPixelToTwips- the number of twips in a pixel.
    * @return             - true if all of aCellWidth was allocated, false otherwise
    */
  PRBool ComputeNonPctColspanWidths(PRInt32           aWidthIndex,
                                    nsTableCellFrame* aCellFrame,
                                    PRInt32           aCellWidth,
                                    PRInt32           aColIndex,
                                    PRInt32           aColSpan,
                                    PRInt32&          aLimitType,
                                    float             aPixelToTwips);

  /**
    * Determine percentage col widths for each col frame
    * @param aReflowState      - the reflow state of the table
    * @param aBasis            - the basis for percent width as computed by CalcPctAdjTableWidth
    * @param aTableIsAutoWidth - true if no width specification for the table is available
    * @param aPixelToTwips     - the number of twips in a pixel.
    * @return                  - the adjusted basis including table border, padding and cell spacing
    */
  nscoord AssignPctColumnWidths(const nsHTMLReflowState& aReflowState,
                                nscoord                  aBasis,
                                PRBool                   aTableIsAutoWidth,
                                float                    aPixelToTwips);

  /**
    * Reduce the percent columns by the amount specified in aExcess as the percent width's
    * can accumulate to be over 100%
    * @param aExcess - reduction amount
    */
  void ReduceOverSpecifiedPctCols(nscoord aExcess);

  /** 
    * Sort rows by rising colspans, in order to treat the inner colspans first
    * the result will be returned in the aRowIndices array.
    * @param aRowIndices - array with indices of those rows which have colspans starting in the corresponding column
    * @param aColSpans   - array with the correspong colspan values
    * @param aIndex      - number of valid entries in the arrays
    */
  void RowSort(PRInt32* aRowIndices, 
               PRInt32* aColSpans,
               PRInt32 aIndex);

  /**
    * calculate totals by width type. The logic here is kept in synch with 
    * that in CanAllocate
    * @param aTotalCounts - array with counts for each width type that has determined the aTotalWidths sum
    * @param aTotalWidths - array with accumulated widths for each width type
    * @param aDupedWidths - (duplicatd) are widths that will be allocated in BalanceColumnWidths before aTotalsWidths
    * @param a0ProportionalCount -  number of columns with col="0*" constraint
    */   
  void CalculateTotals(PRInt32* aTotalCounts,
                       PRInt32* aTotalWidths,
                       PRInt32* aDupedWidths,
                       PRInt32& a0ProportionalCount);

  /**
    * Allocate aWidthType values to the corresponding columns
    * @param aTotalAllocated - width that has been allocated in this routine
    * @param aAllocTypes - width type that has determined col width
    * @param aWidthType - width type selecting the columns for full width allocation
    */
  void AllocateFully(nscoord& aTotalAllocated,
                     PRInt32* aAllocTypes,
                     PRInt32  aWidthType);

  /**
    * Allocate aWidthType values to the corresponding columns up to the aAvailWidth
    * @param aAvailWidth    - width that can distributed to the selected columns
    * @param aWidthType     - width type selecting the columns for width allocation
    * @param aStartAtMin    - allocation should start at min. content width
    * @param aAllocTypes    - width type that has determined col width
    * @param aPixelToTwips  - the number of twips in a pixel.
    */
  void AllocateConstrained(PRInt32  aAvailWidth,
                           PRInt32  aWidthType,
                           PRBool   aStartAtMin,        
                           PRInt32* aAllocTypes,
                           float    aPixelToTwips);

  /**
    * Give the remaining space and exclude the selected columns
    * @param aAllocAmount   - space that can be distributed
    * @param aAllocTypes    - width type that has determined col width
    * @param aExcludePct    - dont give space to percent columns
    * @param aExcludeFix    - dont give space to fixed width columns
    * @param aExcludePro    - dont give space to proportional columns
    * @param aExclude0Pro   - dont give space to proportional columns with 0*
    * @param aPixelToTwips  - the number of twips in a pixel.
    */
  void AllocateUnconstrained(PRInt32  aAllocAmount,
                             PRInt32* aAllocTypes,
                             PRBool   aExcludePct,
                             PRBool   aExcludeFix,
                             PRBool   aExcludePro,
                             PRBool   aExclude0Pro,
                             float    aPixelToTwips);

  /**
    * Check in debug mode whether the routine is called on a continuing frame
    */
  void ContinuingFrameCheck();

#ifdef DEBUG
  void  SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const {
    *aResult = sizeof(*this);
  }
#endif

protected:
  nsTableFrame * mTableFrame;
  PRInt32        mCols;
  // cached data
  nscoord        mCellSpacingTotal;       // all of the cellspacing for all of the cols
  float          mMinToDesProportionRatio;
  PRPackedBool   mIsNavQuirksMode;
};
#endif

