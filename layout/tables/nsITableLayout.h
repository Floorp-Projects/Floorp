/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsITableLayout_h__
#define nsITableLayout_h__

#include "nsQueryFrame.h"
class nsIDOMElement;

/**
 * nsITableLayout
 * interface for layout objects that act like tables.
 * initially, we use this to get cell info 
 *
 * @author  sclark
 */
class nsITableLayout
{
public:

  NS_DECL_QUERYFRAME_TARGET(nsITableLayout)

  /** return all the relevant layout information about a cell.
   *  @param aRowIndex       a row which the cell intersects
   *  @param aColIndex       a col which the cell intersects
   *  @param aCell           [OUT] the content representing the cell at (aRowIndex, aColIndex)
   *  @param aStartRowIndex  [IN/OUT] the row in which aCell starts
   *  @param aStartColIndex  [IN/OUT] the col in which aCell starts
   *                         Initialize these with the "candidate" start indexes to use
   *                           for searching through the table when a cell isn't found 
   *                           because of "holes" in the cellmap
   *                           when ROWSPAN and/or COLSPAN > 1
   *  @param aRowSpan        [OUT] the value of the ROWSPAN attribute (may be 0 or actual number)
   *  @param aColSpan        [OUT] the value of the COLSPAN attribute (may be 0 or actual number)
   *  @param aActualRowSpan  [OUT] the actual number of rows aCell spans
   *  @param aActualColSpan  [OUT] the acutal number of cols aCell spans
   *  @param aIsSelected     [OUT] true if the frame that maps aCell is selected
   *                               in the presentation shell that owns this.
   */
  NS_IMETHOD GetCellDataAt(PRInt32 aRowIndex, PRInt32 aColIndex,
                           nsIDOMElement* &aCell,   //out params
                           PRInt32& aStartRowIndex, PRInt32& aStartColIndex, 
                           PRInt32& aRowSpan, PRInt32& aColSpan,
                           PRInt32& aActualRowSpan, PRInt32& aActualColSpan,
                           bool& aIsSelected)=0;

  /** Get the number of rows and column for a table from the frame's cellmap 
   *  Some rows may not have enough cells (the number returned is the maximum possible),
   *  which displays as a ragged-right edge table
   */
  NS_IMETHOD GetTableSize(PRInt32& aRowCount, PRInt32& aColCount)=0;

  /**
   * Retrieves the index of the cell at the given coordinates.
   *
   * @note  The index is the order number of the cell calculated from top left
   *        cell to the right bottom cell of the table.
   *
   * @param aRow     [in] the row the cell is in
   * @param aColumn  [in] the column the cell is in
   * @param aIndex   [out] the index to be returned
   */
  NS_IMETHOD GetIndexByRowAndColumn(PRInt32 aRow, PRInt32 aColumn,
                                    PRInt32 *aIndex) = 0;

  /**
   * Retrieves the coordinates of the cell at the given index.
   *
   * @see  nsITableLayout::GetIndexByRowAndColumn()
   *
   * @param aIndex  [in] the index for which the coordinates are to be retrieved
   * @param aRow    [out] the resulting row coordinate
   * @param aColumn [out] the resulting column coordinate
   */
  NS_IMETHOD GetRowAndColumnByIndex(PRInt32 aIndex,
                                    PRInt32 *aRow, PRInt32 *aColumn) = 0;
};

#endif
