/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef BasicTableLayoutStrategy_h__
#define BasicTableLayoutStrategy_h__

#include "nscore.h"
#include "nsITableLayoutStrategy.h"
#include "nsCoord.h"

class nsVoidArray;
class nsTableFrame;
struct nsStylePosition;

/* ----------- SpanInfo ---------- */

struct SpanInfo
{
  PRInt32 span;
  const PRInt32 initialColSpan;
  nscoord cellMinWidth;
  nscoord cellDesiredWidth;

  SpanInfo(PRInt32 aSpan, nscoord aMinWidth, nscoord aDesiredWidth);
  ~SpanInfo() {};
};

inline SpanInfo::SpanInfo(PRInt32 aSpan, nscoord aMinWidth, nscoord aDesiredWidth)
  : initialColSpan(aSpan)
{
  span = aSpan;
  cellMinWidth = aMinWidth;
  cellDesiredWidth = aDesiredWidth;
}



/* ---------- BasicTableLayoutStrategy ---------- */

class BasicTableLayoutStrategy : public nsITableLayoutStrategy
{
public:

  /** Public constructor.
    * @paran aFrame           the table frame for which this delegate will do layout
    * @param aNumCols         the total number of columns in the table    
    */
  BasicTableLayoutStrategy(nsTableFrame *aFrame, PRInt32 aNumCols);

  ~BasicTableLayoutStrategy();

  /** call once every time any table thing changes (content, structure, or style) */
  virtual PRBool Initialize(nsSize* aMaxElementSize);

  virtual PRBool BalanceColumnWidths(nsIStyleContext *    aTableStyle,
                                     const nsReflowState& aReflowState,
                                     nscoord              aMaxWidth);

  /** assign widths for each column.
    * if the column has a fixed coord width, use it.
    * if the column includes col spanning cells, 
    * then distribute the fixed space between cells proportionately.
    * Computes the minimum and maximum table widths. 
    * Set column width information in each column frame and in the table frame.
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    *
    */
  virtual PRBool AssignPreliminaryColumnWidths();

  /** assign widths for each column that has proportional width inside a table that 
    * has auto width (width set by the content and available space.)
    * Sets mColumnWidths as a side effect.
    *
    * @param aTableStyle          the resolved style for the table
    * @param aAvailWidth          the remaining amount of horizontal space available
    * @param aMaxWidth            the total amount of horizontal space available
    * @param aTableSpecifiedWidth the width of the table based on its attributes and its parent's width
    * @param aTableIsAutoWidth    PR_TRUE if the table is auto-width
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    *
    */
  virtual PRBool BalanceProportionalColumns(const nsReflowState& aReflowState,
                                            nscoord aAvailWidth,
                                            nscoord aMaxWidth,
                                            nscoord aTableSpecifiedWidth,
                                            PRBool  aTableIsAutoWidth);

  /** assign the minimum allowed width for each column that has proportional width.
    * Typically called when the min table width doesn't fit in the available space.
    * Sets mColumnWidths as a side effect.
    *
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    */
  virtual PRBool SetColumnsToMinWidth();

  /** assign the maximum allowed width for each column that has proportional width.
    * Typically called when the desired max table width fits in the available space.
    * Sets mColumnWidths as a side effect.
    *
    * @param aAvailWidth          the remaining amount of horizontal space available
    * @param aMaxWidth            the total amount of horizontal space available
    * @param aTableSpecifiedWidth the specified width of the table.  If there is none,
    *                             this param is 0
    * @param aTableIsAutoWidth    PR_TRUE if the table is auto-width
    * 
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    */
  virtual PRBool BalanceColumnsTableFits(const nsReflowState& aReflowState,
                                         nscoord aAvailWidth,
                                         nscoord aMaxWidth,
                                         nscoord aTableSpecifiedWidth,
                                         PRBool  aTableIsAutoWidth);

  /** assign widths for each column that has proportional width inside a table that 
    * has auto width (width set by the content and available space) according to the
    * HTML 4 specification.
    * Sets mColumnWidths as a side effect.
    *
    * @param aTableStyle      the resolved style for the table
    * @param aAvailWidth      the remaining amount of horizontal space available
    * @param aMaxWidth        the total amount of horizontal space available
    * @param aMinTableWidth   the min possible table width
    * @param aMaxTableWidth   the max table width
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    *
    * TODO: rename this method to reflect that it is a Nav4 compatibility method
    */
  virtual PRBool BalanceColumnsConstrained(const nsReflowState& aReflowState,
                                           nscoord aAvailWidth,
                                           nscoord aMaxWidth);

  /** post-process to AssignFixedColumnWidths
    *
    * @param aColSpanList         a list of fixed-width columns that have colspans
    *
    * NOTE: does not yet properly handle overlapping col spans
    *
    * @return void
    */  
  virtual void DistributeFixedSpace(nsVoidArray *aColSpanList);

  /** starting with a partially balanced table, compute the amount
    * of space to pad each column by to completely balance the table.
    * set the column widths in mTableFrame based on these computations.
    *
    * @param aAvailWidth          the space still to be allocated within the table
    * @param aTableWidth          the sum of all columns widths
    * @param aWidthOfFixedTableColumns the sum of the widths of fixed-width columns
    * @param aColWidths           the effective column widths (ignoring col span cells)
    *
    * @return void
    */
  virtual void DistributeExcessSpace(nscoord  aAvailWidth,
                                     nscoord  aTableWidth, 
                                     nscoord  aWidthOfFixedTableColumns);

  /** starting with a partially balanced table, compute the amount
    * of space to remove from each column to completely balance the table.
    * set the column widths in mTableFrame based on these computations.
    *
    * @param aTableFixedWidth     the specified width of the table.  If there is none,
    *                             this param is 0
    * @param aComputedTableWidth  the width of the table before this final step.
    *
    * @return void
    */
  virtual void DistributeRemainingSpace(nscoord  aTableFixedWidth,
                                        nscoord  aComputedTableWidth);

  /** force all cells to be at least their minimum width, removing any excess space
    * created in the process from fat cells that can afford to lose a little tonnage.
    */
  virtual void EnsureCellMinWidths();

  virtual void AdjustTableThatIsTooWide(nscoord  aComputedWidth, 
                                        nscoord  aTableWidth);

  virtual void AdjustTableThatIsTooNarrow(nscoord  aComputedWidth, 
                                          nscoord  aTableWidth);

  /** return true if the style indicates that the width is a specific width 
    * for the purposes of column width determination.
    * return false if the width changes based on content, parent size, etc.
    */
  virtual PRBool IsFixedWidth(const nsStylePosition* aStylePosition);


protected:
  nsTableFrame * mTableFrame;
  PRInt32        mCols;
  PRInt32        mNumCols;
  // cached data
  nscoord        mMinTableWidth;    // the absolute smallest width for the table
  nscoord        mMaxTableWidth;    // the "natural" size for the table, if unconstrained
  nscoord        mFixedTableWidth;  // the amount of space taken up by fixed-width columns

};

#endif

