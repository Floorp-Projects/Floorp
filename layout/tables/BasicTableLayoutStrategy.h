/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

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
    * @paran aFrame           the table frame for which this delegate will do layout
    */
  BasicTableLayoutStrategy(nsTableFrame *aFrame,
                           PRBool        aIsNavQuirksMode = PR_TRUE);

  /** destructor */
  virtual ~BasicTableLayoutStrategy();

  /** call every time any table thing changes that might effect the width of any column
    * in the table (content, structure, or style) 
    * @param aMaxElementSize  [OUT] if not null, the max element size is computed and returned in this param
    */
  virtual PRBool Initialize(nsIPresContext*          aPresContext,
                            nsSize*                  aMaxElementSize,
                            nscoord                  aMaxSize,
                            const nsHTMLReflowState& aReflowState);

  /** compute the max element size of the table.
    * assumes that Initialize has been called
    */
  virtual void SetMaxElementSize(nsSize*         aMaxElementSize,
                                 const nsMargin& aPadding);

  void SetMinAndMaxTableContentWidths();

  /** Called during resize reflow to determine the new column widths
    * @param aTableStyle - the resolved style for mTableFrame
	  * @param aReflowState - the reflow state for mTableFrame
 	  * @param aMaxWidth - the computed max width for columns to fit into
	  */
  virtual PRBool BalanceColumnWidths(nsIPresContext*          aPresContext,
                                     nsIStyleContext*         aTableStyle,
                                     const nsHTMLReflowState& aReflowState,
                                     nscoord                  aMaxWidth);

  // these accessors are mostly for debugging purposes
  nscoord GetTableMinWidth() const;
  nscoord GetTableMaxWidth(const nsHTMLReflowState& aReflowState) const;
  nscoord GetCOLSAttribute() const;
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
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    *
    */
  virtual PRBool AssignNonPctColumnWidths(nsIPresContext*          aPresContext,
                                          nscoord                  aComputedWidth,
                                          const nsHTMLReflowState& aReflowState,
                                          float                    aPixelToTwips);

  /** 
    * Calculate the adjusted widths (min, desired, fixed, or pct) for a cell
    * spanning multiple columns. 
    * @param aWidthIndex  - the width to calculate (see nsTableColFrame.h for enums)
    * @param aCellFrame   - the frame of the cell with a colspan
    * @param aColIndex    - the column index of the cell in the table
    * @param aColSpan     - the colspan of the cell
    * @param aConsiderPct - if true, consider columns that have pct widths and are spanned by the cell
    */
  void ComputeNonPctColspanWidths(const nsHTMLReflowState& aReflowState,
                                  PRBool                   aConsiderPct,
                                  float                    aPixelToTwips);

  /** 
    * main helper for above. For min width calculations, it can get called up to
    * 3 times, 1st to let constrained (fix or pct) cols reach their limit, 2nd 
    * to let auto cols reach their limit and 3rd to spread any remainder among 
    * auto cols. If there are no auto cols then only constrained cols are considered.
    * @param aCellWidth   - the width of the cell
    * @param aLimitType   - value indicating which type of width is being targeted 
    *                       to reach a limit
    * @return             - true if the computation completed, false otherwise
    */
  PRBool ComputeNonPctColspanWidths(PRInt32           aWidthIndex,
                                    nsTableCellFrame* aCellFrame,
                                    nscoord           aCellWidth,
                                    PRInt32           aColIndex,
                                    PRInt32           aColSpan,
                                    PRInt32&          aLimitType,
                                    float             aPixelToTwips);

  nscoord AssignPctColumnWidths(const nsHTMLReflowState aReflowState,
                                nscoord                 aBasis,
                                PRBool                  aTableIsAutoWidth,
                                float                   aPixelToTwips);

  void ReduceOverSpecifiedPctCols(nscoord aExcess);

  void CalculateTotals(PRInt32& aCellSpacing,
                       PRInt32* aTotalCounts,
                       PRInt32* aTotalWidths,
                       PRInt32* aTotalAvailWidths,
                       PRInt32* aMinWidths,
                       PRInt32& a0ProportionalCount);

  void AllocateFully(nscoord& aTotalAllocated,
                     PRInt32* aAllocTypes,
                     PRInt32  aWidthType,
                     PRBool   aMarkAllocated = PR_TRUE);

  void AllocateConstrained(PRInt32  aAvailWidth,
                           PRInt32  aWidthType,
                           PRBool   aStartAtMin,        
                           PRInt32* aAllocTypes,
                           float    aPixelToTwips);

  void AllocateUnconstrained(PRInt32  aAllocAmount,
                             PRInt32* aAllocTypes,
                             PRBool   aSkip0Proportional,
                             float    aPixelToTwips);

  /** return true if the colIndex is in the list of colIndexes */
  virtual PRBool IsColumnInList(const PRInt32 colIndex, 
                                PRInt32 *colIndexes, 
                                PRInt32 aNumFixedColumns);

  /** returns true if the column is specified to have its min width */
  virtual PRBool ColIsSpecifiedAsMinimumWidth(PRInt32 aColIndex);

  /** eturns a list and count of all columns that behave like they have width=auto
    * this includes columns with no width specified (the normal definition of "auto"), 
    * columns explicitly set to auto width, 
    * and columns whose fixed width comes from a span (meaning the real width is indeterminate.)
    *
    * @param aOutNumColumns -- out param, the number of columns matching aType
    * @param aOutColumnIndexes -- out param, the indexes of the columns matching aType
    *                             
    * @return       aOutNumColumns set to the number of auto columns, may be 0
    *               allocates and fills aOutColumnIndexes
    *               caller must "delete [] aOutColumnIndexes" if it is not null
    */
  void GetColumnsThatActLikeAutoWidth(PRInt32&  aOutNumColumns,
                                      PRInt32*& aOutColumnIndexes);
  void ContinuingFrameCheck();

  // see nsTableFrame::ColumnsCanBeInvalidatedBy
  PRBool ColumnsCanBeInvalidatedBy(nsStyleCoord*           aPrevStyleWidth,
                                   const nsTableCellFrame& aCellFrame) const;

  // see nsTableFrame::ColumnsCanBeInvalidatedBy
  PRBool ColumnsCanBeInvalidatedBy(const nsTableCellFrame& aCellFrame,
                                   PRBool                  aConsiderMinWidth = PR_FALSE) const;

  // see nsTableFrame::ColumnsCanBeInvalidatedBy
  PRBool ColumnsAreValidFor(const nsTableCellFrame& aCellFrame,
                            nscoord                 aPrevCellMin,
                            nscoord                 aPrevCellDes) const;

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

inline nscoord BasicTableLayoutStrategy::GetCOLSAttribute() const
{ return mCols; };


#endif

