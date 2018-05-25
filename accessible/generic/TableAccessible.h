/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TABLE_ACCESSIBLE_H
#define TABLE_ACCESSIBLE_H

#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace a11y {

class Accessible;

/**
 * Accessible table interface.
 */
class TableAccessible
{
public:

  /**
   * Return the caption accessible if any for this table.
   */
  virtual Accessible* Caption() const { return nullptr; }

  /**
   * Get the summary for this table.
   */
  virtual void Summary(nsString& aSummary) { aSummary.Truncate(); }

  /**
   * Return the number of columns in the table.
   */
  virtual uint32_t ColCount() const { return 0; }

  /**
   * Return the number of rows in the table.
   */
  virtual uint32_t RowCount() { return 0; }

  /**
   * Return the accessible for the cell at the given row and column indices.
   */
  virtual Accessible* CellAt(uint32_t aRowIdx, uint32_t aColIdx) { return nullptr; }

  /**
   * Return the index of the cell at the given row and column.
   */
  virtual int32_t CellIndexAt(uint32_t aRowIdx, uint32_t aColIdx)
    { return ColCount() * aRowIdx + aColIdx; }

  /**
   * Return the column index of the cell with the given index.
   */
  virtual int32_t ColIndexAt(uint32_t aCellIdx)
    { return aCellIdx % ColCount(); }

  /**
   * Return the row index of the cell with the given index.
   */
  virtual int32_t RowIndexAt(uint32_t aCellIdx)
    { return aCellIdx / ColCount(); }

  /**
   * Get the row and column indices for the cell at the given index.
   */
  virtual void RowAndColIndicesAt(uint32_t aCellIdx, int32_t* aRowIdx,
                                  int32_t* aColIdx)
    {
      uint32_t colCount = ColCount();
      *aRowIdx = aCellIdx / colCount;
      *aColIdx = aCellIdx % colCount;
    }

  /**
   * Return the number of columns occupied by the cell at the given row and
   * column indices.
   */
  virtual uint32_t ColExtentAt(uint32_t aRowIdx, uint32_t aColIdx) { return 1; }

  /**
   * Return the number of rows occupied by the cell at the given row and column
   * indices.
   */
  virtual uint32_t RowExtentAt(uint32_t aRowIdx, uint32_t aColIdx) { return 1; }

  /**
   * Get the description of the given column.
   */
  virtual void ColDescription(uint32_t aColIdx, nsString& aDescription)
    { aDescription.Truncate(); }

  /**
   * Get the description for the given row.
   */
  virtual void RowDescription(uint32_t aRowIdx, nsString& aDescription)
    { aDescription.Truncate(); }

  /**
   * Return true if the given column is selected.
   */
  virtual bool IsColSelected(uint32_t aColIdx) { return false; }

  /**
   * Return true if the given row is selected.
   */
  virtual bool IsRowSelected(uint32_t aRowIdx) { return false; }

  /**
   * Return true if the given cell is selected.
   */
  virtual bool IsCellSelected(uint32_t aRowIdx, uint32_t aColIdx) { return false; }

  /**
   * Return the number of selected cells.
   */
  virtual uint32_t SelectedCellCount() { return 0; }

  /**
   * Return the number of selected columns.
   */
  virtual uint32_t SelectedColCount() { return 0; }

  /**
   * Return the number of selected rows.
   */
  virtual uint32_t SelectedRowCount() { return 0; }

  /**
   * Get the set of selected cells.
   */
  virtual void SelectedCells(nsTArray<Accessible*>* aCells) = 0;

  /**
   * Get the set of selected cell indices.
   */
  virtual void SelectedCellIndices(nsTArray<uint32_t>* aCells) = 0;

  /**
   * Get the set of selected column indices.
   */
  virtual void SelectedColIndices(nsTArray<uint32_t>* aCols) = 0;

  /**
   * Get the set of selected row indices.
   */
  virtual void SelectedRowIndices(nsTArray<uint32_t>* aRows) = 0;

  /**
   * Select the given column unselecting any other selected columns.
   */
  virtual void SelectCol(uint32_t aColIdx) {}

  /**
   * Select the given row unselecting all other previously selected rows.
   */
  virtual void SelectRow(uint32_t aRowIdx) {}

  /**
   * Unselect the given column leaving other selected columns selected.
   */
  virtual void UnselectCol(uint32_t aColIdx) {}

  /**
   * Unselect the given row leaving other selected rows selected.
   */
  virtual void UnselectRow(uint32_t aRowIdx) {}

  /**
   * Return true if the table is probably for layout.
   */
  virtual bool IsProbablyLayoutTable();

  /**
   * Convert the table to an Accessible*.
   */
  virtual Accessible* AsAccessible() = 0;
};

} // namespace a11y
} // namespace mozilla

#endif
