/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TABLE_ACCESSIBLE_H
#define TABLE_ACCESSIBLE_H

#include "nsString.h"
#include "nsTArray.h"
#include "prtypes.h"

class Accessible;

namespace mozilla {
namespace a11y {

/**
 * Accessible table interface.
 */
class TableAccessible
{
public:

  /**
   * Return the caption accessible if any for this table.
   */
  virtual Accessible* Caption() { return nsnull; }

  /**
   * Get the summary for this table.
   */
  virtual void Summary(nsString& aSummary) { aSummary.Truncate(); }

  /**
   * Return the number of columns in the table.
   */
  virtual PRUint32 ColCount() { return 0; }

  /**
   * Return the number of rows in the table.
   */
  virtual PRUint32 RowCount() { return 0; }

  /**
   * Return the accessible for the cell at the given row and column indices.
   */
  virtual Accessible* CellAt(PRUint32 aRowIdx, PRUint32 aColIdx) { return nsnull; }

  /**
   * Return the index of the cell at the given row and column.
   */
  virtual PRInt32 CellIndexAt(PRUint32 aRowIdx, PRUint32 aColIdx)
    { return ColCount() * aRowIdx + aColIdx; }

  /**
   * Return the column index of the cell with the given index.
   */
  virtual PRInt32 ColIndexAt(PRUint32 aCellIdx) { return -1; }

  /**
   * Return the row index of the cell with the given index.
   */
  virtual PRInt32 RowIndexAt(PRUint32 aCellIdx) { return -1; }

  /**
   * Get the row and column indices for the cell at the given index.
   */
  virtual void RowAndColIndicesAt(PRUint32 aCellIdx, PRInt32* aRowIdx,
                                  PRInt32* aColIdx) {}

  /**
   * Return the number of columns occupied by the cell at the given row and
   * column indices.
   */
  virtual PRUint32 ColExtentAt(PRUint32 aRowIdx, PRUint32 aColIdx) { return 1; }

  /**
   * Return the number of rows occupied by the cell at the given row and column
   * indices.
   */
  virtual PRUint32 RowExtentAt(PRUint32 aRowIdx, PRUint32 aColIdx) { return 1; }

  /**
   * Get the description of the given column.
   */
  virtual void ColDescription(PRUint32 aColIdx, nsString& aDescription)
    { aDescription.Truncate(); }

  /**
   * Get the description for the given row.
   */
  virtual void RowDescription(PRUint32 aRowIdx, nsString& aDescription)
    { aDescription.Truncate(); }

  /**
   * Return true if the given column is selected.
   */
  virtual bool IsColSelected(PRUint32 aColIdx) { return false; }

  /**
   * Return true if the given row is selected.
   */
  virtual bool IsRowSelected(PRUint32 aRowIdx) { return false; }

  /**
   * Return true if the given cell is selected.
   */
  virtual bool IsCellSelected(PRUint32 aRowIdx, PRUint32 aColIdx) { return false; }

  /**
   * Return the number of selected cells.
   */
  virtual PRUint32 SelectedCellCount() { return 0; }

  /**
   * Return the number of selected columns.
   */
  virtual PRUint32 SelectedColCount() { return 0; }

  /**
   * Return the number of selected rows.
   */
  virtual PRUint32 SelectedRowCount() { return 0; }

  /**
   * Get the set of selected cells.
   */
  virtual void SelectedCells(nsTArray<Accessible*>* aCells) {}

  /**
   * Get the set of selected column indices.
   */
  virtual void SelectedColIndices(nsTArray<PRUint32>* aCols) {}

  /**
   * Get the set of selected row indices.
   */
  virtual void SelectedRowIndices(nsTArray<PRUint32>* aRows) {}

  /**
   * Select the given column unselecting any other selected columns.
   */
  virtual void SelectCol(PRUint32 aColIdx) {}

  /**
   * Select the given row unselecting all other previously selected rows.
   */
  virtual void SelectRow(PRUint32 aRowIdx) {}

  /**
   * Unselect the given column leaving other selected columns selected.
   */
  virtual void UnselectCol(PRUint32 aColIdx) {}

  /**
   * Unselect the given row leaving other selected rows selected.
   */
  virtual void UnselectRow(PRUint32 aRowIdx) {}

  /**
   * Return true if the table is probably for layout.
   */
  virtual bool IsProbablyLayoutTable() { return false; }
};

} // namespace a11y
} // namespace mozilla

#endif
