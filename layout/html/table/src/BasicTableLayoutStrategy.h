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
struct nsStyleTable;

/* ----------- SpanInfo ---------- */

/** SpanInfo is a transient data structure that holds info about 
  * cells that have col spans.  Used during column balancing.
  */
// TODO: collapse this data structure with ColSpanStruct.  They both (incompletely) describe 
//       the same situation with slightly different data.  
struct SpanInfo
{
  PRInt32 span;
  const PRInt32 initialColSpan;
  const PRInt32 initialColIndex;
  nscoord cellMinWidth;
  nscoord cellDesiredWidth;
  nscoord effectiveMinWidthOfSpannedCols;
  nscoord effectiveMaxWidthOfSpannedCols;

  SpanInfo(PRInt32 aColIndex, PRInt32 aSpan, nscoord aMinWidth, nscoord aDesiredWidth);
  ~SpanInfo() {};

};

static const PRInt32 kUninitialized=-1;

inline SpanInfo::SpanInfo(PRInt32 aColIndex, PRInt32 aSpan, 
                          nscoord aMinWidth, nscoord aDesiredWidth)
  : initialColIndex(aColIndex),
    initialColSpan(aSpan)
{
  span = aSpan;
  cellMinWidth = aMinWidth;
  cellDesiredWidth = aDesiredWidth;
  effectiveMinWidthOfSpannedCols=kUninitialized;
  effectiveMaxWidthOfSpannedCols=kUninitialized;
}



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
  BasicTableLayoutStrategy(nsTableFrame *aFrame);

  /** destructor */
  virtual ~BasicTableLayoutStrategy();

  /** call every time any table thing changes that might effect the width of any column
    * in the table (content, structure, or style) 
    * @param aMaxElementSize  [OUT] if not null, the max element size is computed and returned in this param
    * @param aNumCols         the total number of columns in the table
    */
  virtual PRBool Initialize(nsSize* aMaxElementSize, PRInt32 aNumCols);

  /** compute the max element size of the table.
    * assumes that Initialize has been called
    */
  virtual void SetMaxElementSize(nsSize* aMaxElementSize);

  /** Called during resize reflow to determine the new column widths
    * @param aTableStyle - the resolved style for mTableFrame
	  * @param aReflowState - the reflow state for mTableFrame
 	  * @param aMaxWidth - the computed max width for columns to fit into
	  */
  virtual PRBool BalanceColumnWidths(nsIStyleContext *    aTableStyle,
                                     const nsHTMLReflowState& aReflowState,
                                     nscoord              aMaxWidth);

  // these accessors are mostly for debugging purposes
  nscoord GetTableMinWidth() const;
  nscoord GetTableMaxWidth() const;
  nscoord GetTableFixedWidth() const;
  nscoord GetCOLSAttribute() const;
  nscoord GetNumCols() const;

protected:

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

  /** compute the min and max width of this table.  Assumes AssignPreliminaryColumnWidths
    * has been called.  Sets mMinTableWidth and mMaxTableWidth as a side effect.
    */
  virtual void SetMinAndMaxTableWidths();

  /** assign widths for each column within a particular context.
    * The context is given by each of the parameters.
    * Sets mTableFrame->mColumnWidths as a side effect.
    *
    * @param aReflowState         the reflow state
    * @param aAvailWidth          the remaining amount of horizontal space available
    * @param aMaxWidth            the total amount of horizontal space available
    * @param aTableSpecifiedWidth the width of the table based on its attributes and its parent's width
    * @param aTableIsAutoWidth    PR_TRUE if the table is auto-width
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    *
    */
  virtual PRBool BalanceProportionalColumns(const nsHTMLReflowState& aReflowState,
                                            nscoord aAvailWidth,
                                            nscoord aMaxWidth,
                                            nscoord aTableSpecifiedWidth,
                                            PRBool  aTableIsAutoWidth);

  /** assign the minimum legal width for each column that has proportional width.
    * Typically called when the min table width doesn't fit in the available space.
    * Sets mTableFrame->mColumnWidths as a side effect.
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    */
  virtual PRBool BalanceColumnsTableDoesNotFit();

  /** assign the maximum allowed width for each column.
    * Typically called when the desired max table width is less than or equal to the available space.
    * Sets mTableFrame->mColumnWidths as a side effect.
    *
    * @param aReflowState         the reflow state
    * @param aAvailWidth          the remaining amount of horizontal space available
    * @param aMaxWidth            the total amount of horizontal space available
    * @param aTableSpecifiedWidth the specified width of the table.  If there is none,
    *                             this param is 0
    * @param aTableIsAutoWidth    PR_TRUE if the table is auto-width
    * 
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    */
  virtual PRBool BalanceColumnsTableFits(const nsHTMLReflowState& aReflowState,
                                         nscoord aAvailWidth,
                                         nscoord aMaxWidth,
                                         nscoord aTableSpecifiedWidth,
                                         PRBool  aTableIsAutoWidth);

  /** for each column, assign a width between the column's minimum and maximum width.
    * Typically called when the desired max table width is greater the available space.
    * Sets mTableFrame->mColumnWidths as a side effect.
    *
    * @param aReflowState       the reflow state
    * @param aAvailWidth        the remaining amount of horizontal space available
    * @param aMaxWidth          the total amount of horizontal space available
    * @param aTableIsAutoWidth  PR_TRUE if the table is auto-width
    *
    * @return PR_TRUE if all is well, PR_FALSE if there was an unrecoverable error
    *
    */
  virtual PRBool BalanceColumnsConstrained(const nsHTMLReflowState& aReflowState,
                                           nscoord aAvailWidth,
                                           nscoord aMaxWidth,
                                           PRBool  aTableIsAutoWidth);

  /** post-process to AssignFixedColumnWidths
    *
    * @param aColSpanList         a list of fixed-width columns that have colspans
    *
    * NOTE: does not yet properly handle overlapping col spans in all cases
    *
    * @return void
    */  
  virtual void DistributeFixedSpace(nsVoidArray *aColSpanList);

  /** starting with a partially balanced table, compute the amount
    * of space to pad each column by to completely balance the table.
    * Sets mTableFrame->mColumnWidths as a side effect.
    *
    * @param aAvailWidth          the space still to be allocated within the table
    * @param aTableWidth          the sum of all columns widths
    * @param aWidthOfFixedTableColumns the sum of the widths of fixed-width columns
    *
    * @return void
    */
  virtual void DistributeExcessSpace(nscoord  aAvailWidth,
                                     nscoord  aTableWidth, 
                                     nscoord  aWidthOfFixedTableColumns);

  /** starting with a partially balanced table, compute the amount
    * of space to allocate to each column to completely balance the table.
    * set the column widths in mTableFrame based on these computations.
    * assumes auto-width columns have been set to their minimum width.
    * must be called with a PRInt32 variable initialized to 0 for aRecursionControl.
    *
    * @param aTableFixedWidth     the specified width of the table.  If there is none,
    *                             this param is 0
    * @param aComputedTableWidth  IN: the width of the table before this step.
    *                             OUT:the width of the table after this step.
    * @param aTableIsAutoWidth    TRUE if the table style indicates it is autoWidth
    * @param aRecursionControl    IN: must be a PRInt32 set to 0
    *                             OUT: the number of iterations.  Not generally useful to the caller,
    *                                  but useful for limiting the number of iterations in bizarre cases.
    *
    * @return void
    */
  virtual void DistributeRemainingSpace(nscoord  aTableFixedWidth,
                                        nscoord &aComputedTableWidth, 
                                        PRBool   aTableIsAutoWidth,
                                        PRInt32 &aRecursionControl);

  virtual void AdjustTableThatIsTooWide(nscoord  aComputedWidth, 
                                        nscoord  aTableWidth, 
                                        PRBool   aShrinkFixedCols);

  virtual void AdjustTableThatIsTooNarrow(nscoord  aComputedWidth, 
                                          nscoord  aTableWidth);

  /** return true if the style indicates that the width is a specific width 
    * for the purposes of column width determination.
    * return false if the width changes based on content, parent size, etc.
    */
  static PRBool IsFixedWidth(const nsStylePosition* aStylePosition,
                              const nsStyleTable* aStyleTable);

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


protected:
  nsTableFrame * mTableFrame;
  PRInt32        mCols;
  PRInt32        mNumCols;
  // cached data
  nscoord        mMinTableWidth;          // the smallest size for the table
  nscoord        mMaxTableWidth;          // the "natural" size for the table, if unconstrained
  nscoord        mFixedTableWidth;        // the amount of space taken up by fixed-width columns

};

// these accessors are mostly for debugging purposes
inline nscoord BasicTableLayoutStrategy::GetTableMinWidth() const
{ return mMinTableWidth; };

inline nscoord BasicTableLayoutStrategy::GetTableMaxWidth() const
{ return mMaxTableWidth; };

inline nscoord BasicTableLayoutStrategy::GetTableFixedWidth() const
{ return mFixedTableWidth; };

inline nscoord BasicTableLayoutStrategy::GetCOLSAttribute() const
{ return mCols; };

inline nscoord BasicTableLayoutStrategy::GetNumCols() const
{ return mNumCols; };  


#endif

