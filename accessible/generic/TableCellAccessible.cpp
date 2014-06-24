/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TableCellAccessible.h"

#include "Accessible-inl.h"
#include "TableAccessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

void
TableCellAccessible::RowHeaderCells(nsTArray<Accessible*>* aCells)
{
  uint32_t rowIdx = RowIdx(), colIdx = ColIdx();
  TableAccessible* table = Table();
  if (!table)
    return;

  // Move to the left to find row header cells
  for (uint32_t curColIdx = colIdx - 1; curColIdx < colIdx; curColIdx--) {
    Accessible* cell = table->CellAt(rowIdx, curColIdx);
    if (!cell)
      continue;

    // CellAt should always return a TableCellAccessible (XXX Bug 587529)
    TableCellAccessible* tableCell = cell->AsTableCell();
    NS_ASSERTION(tableCell, "cell should be a table cell!");
    if (!tableCell)
      continue;

    // Avoid addding cells multiple times, if this cell spans more columns
    // we'll get it later.
    if (tableCell->ColIdx() == curColIdx && cell->Role() == roles::ROWHEADER)
      aCells->AppendElement(cell);
  }
}

void
TableCellAccessible::ColHeaderCells(nsTArray<Accessible*>* aCells)
{
  uint32_t rowIdx = RowIdx(), colIdx = ColIdx();
  TableAccessible* table = Table();
  if (!table)
    return;

  // Move up to find column header cells
  for (uint32_t curRowIdx = rowIdx - 1; curRowIdx < rowIdx; curRowIdx--) {
    Accessible* cell = table->CellAt(curRowIdx, colIdx);
    if (!cell)
      continue;

    // CellAt should always return a TableCellAccessible (XXX Bug 587529)
    TableCellAccessible* tableCell = cell->AsTableCell();
    NS_ASSERTION(tableCell, "cell should be a table cell!");
    if (!tableCell)
      continue;

    // Avoid addding cells multiple times, if this cell spans more rows
    // we'll get it later.
    if (tableCell->RowIdx() == curRowIdx && cell->Role() == roles::COLUMNHEADER)
      aCells->AppendElement(cell);
  }
}
