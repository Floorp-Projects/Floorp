/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TableAccessible.h"

#include "LocalAccessible-inl.h"
#include "AccIterator.h"

#include "nsTableCellFrame.h"
#include "nsTableWrapperFrame.h"
#include "TableCellAccessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

LocalAccessible* TableAccessible::RowAt(int32_t aRow) {
  int32_t rowIdx = aRow;

  AccIterator rowIter(this->AsAccessible(), filters::GetRow);

  LocalAccessible* row = rowIter.Next();
  while (rowIdx != 0 && (row = rowIter.Next())) {
    rowIdx--;
  }

  return row;
}

LocalAccessible* TableAccessible::CellInRowAt(LocalAccessible* aRow,
                                              int32_t aColumn) {
  int32_t colIdx = aColumn;

  AccIterator cellIter(aRow, filters::GetCell);
  LocalAccessible* cell = nullptr;

  while (colIdx >= 0 && (cell = cellIter.Next())) {
    MOZ_ASSERT(cell->IsTableCell(), "No table or grid cell!");
    colIdx -= cell->AsTableCell()->ColExtent();
  }

  return cell;
}

int32_t TableAccessible::ColIndexAt(uint32_t aCellIdx) {
  uint32_t colCount = ColCount();
  if (colCount < 1 || aCellIdx >= colCount * RowCount()) {
    return -1;  // Error: column count is 0 or index out of bounds.
  }

  return aCellIdx % colCount;
}

int32_t TableAccessible::RowIndexAt(uint32_t aCellIdx) {
  uint32_t colCount = ColCount();
  if (colCount < 1 || aCellIdx >= colCount * RowCount()) {
    return -1;  // Error: column count is 0 or index out of bounds.
  }

  return aCellIdx / colCount;
}

void TableAccessible::RowAndColIndicesAt(uint32_t aCellIdx, int32_t* aRowIdx,
                                         int32_t* aColIdx) {
  uint32_t colCount = ColCount();
  if (colCount < 1 || aCellIdx >= colCount * RowCount()) {
    *aRowIdx = -1;
    *aColIdx = -1;
    return;  // Error: column count is 0 or index out of bounds.
  }

  *aRowIdx = aCellIdx / colCount;
  *aColIdx = aCellIdx % colCount;
}
