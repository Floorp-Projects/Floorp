/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TableCellAccessible.h"

#include "LocalAccessible-inl.h"
#include "TableAccessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

void TableCellAccessible::RowHeaderCells(nsTArray<Accessible*>* aCells) {
  uint32_t rowIdx = RowIdx(), colIdx = ColIdx();
  TableAccessible* table = Table();
  if (!table) return;

  // Move to the left to find row header cells
  for (uint32_t curColIdx = colIdx - 1; curColIdx < colIdx; curColIdx--) {
    LocalAccessible* cell = table->CellAt(rowIdx, curColIdx);
    if (!cell) continue;

    // CellAt should always return a TableCellAccessible (XXX Bug 587529)
    TableCellAccessible* tableCell = cell->AsTableCell();
    NS_ASSERTION(tableCell, "cell should be a table cell!");
    if (!tableCell) continue;

    // Avoid addding cells multiple times, if this cell spans more columns
    // we'll get it later.
    if (tableCell->ColIdx() == curColIdx && cell->Role() == roles::ROWHEADER) {
      aCells->AppendElement(cell);
    }
  }
}

LocalAccessible* TableCellAccessible::PrevColHeader() {
  TableAccessible* table = Table();
  if (!table) {
    return nullptr;
  }

  TableAccessible::HeaderCache& cache = table->GetHeaderCache();
  bool inCache = false;
  LocalAccessible* cachedHeader = cache.GetWeak(this, &inCache);
  if (inCache) {
    // Cached but null means we know there is no previous column header.
    // if defunct, the cell was removed, so behave as if there is no cached
    // value.
    if (!cachedHeader || !cachedHeader->IsDefunct()) {
      return cachedHeader;
    }
  }

  uint32_t rowIdx = RowIdx(), colIdx = ColIdx();
  for (uint32_t curRowIdx = rowIdx - 1; curRowIdx < rowIdx; curRowIdx--) {
    LocalAccessible* cell = table->CellAt(curRowIdx, colIdx);
    if (!cell) {
      continue;
    }
    // CellAt should always return a TableCellAccessible (XXX Bug 587529)
    TableCellAccessible* tableCell = cell->AsTableCell();
    MOZ_ASSERT(tableCell, "cell should be a table cell!");
    if (!tableCell) {
      continue;
    }

    // Check whether the previous table cell has a cached value.
    cachedHeader = cache.GetWeak(tableCell, &inCache);
    if (
        // We check the cache first because even though we might not use it,
        // it's faster than the other conditions.
        inCache &&
        // Only use the cached value if:
        // 1. cell is a table cell which is not a column header. In that case,
        // cell is the previous header and cachedHeader is the one before that.
        // We will return cell later.
        cell->Role() != roles::COLUMNHEADER &&
        // 2. cell starts in this column. If it starts in a previous column and
        // extends into this one, its header will be for the starting column,
        // which is wrong for this cell.
        // ColExtent is faster than ColIdx, so check that first.
        (tableCell->ColExtent() == 1 || tableCell->ColIdx() == colIdx)) {
      if (!cachedHeader || !cachedHeader->IsDefunct()) {
        // Cache it for this cell.
        cache.InsertOrUpdate(this, RefPtr<LocalAccessible>(cachedHeader));
        return cachedHeader;
      }
    }

    // Avoid addding cells multiple times, if this cell spans more rows
    // we'll get it later.
    if (cell->Role() != roles::COLUMNHEADER ||
        tableCell->RowIdx() != curRowIdx) {
      continue;
    }

    // Cache the header we found.
    cache.InsertOrUpdate(this, RefPtr<LocalAccessible>(cell));
    return cell;
  }

  // There's no header, so cache that fact.
  cache.InsertOrUpdate(this, RefPtr<LocalAccessible>(nullptr));
  return nullptr;
}

void TableCellAccessible::ColHeaderCells(nsTArray<Accessible*>* aCells) {
  for (LocalAccessible* cell = PrevColHeader(); cell;
       cell = cell->AsTableCell()->PrevColHeader()) {
    aCells->AppendElement(cell);
  }
}

a11y::role TableCellAccessible::GetHeaderCellRole(
    const LocalAccessible* aAcc) const {
  if (!aAcc || !aAcc->GetContent() || !aAcc->GetContent()->IsElement()) {
    return roles::NOTHING;
  }

  MOZ_ASSERT(aAcc->IsTableCell());

  // Check value of @scope attribute.
  static mozilla::dom::Element::AttrValuesArray scopeValues[] = {
      nsGkAtoms::col, nsGkAtoms::colgroup, nsGkAtoms::row, nsGkAtoms::rowgroup,
      nullptr};
  int32_t valueIdx = aAcc->GetContent()->AsElement()->FindAttrValueIn(
      kNameSpaceID_None, nsGkAtoms::scope, scopeValues, eCaseMatters);

  switch (valueIdx) {
    case 0:
    case 1:
      return roles::COLUMNHEADER;
    case 2:
    case 3:
      return roles::ROWHEADER;
  }

  TableAccessible* table = Table();
  if (!table) {
    return roles::NOTHING;
  }

  // If the cell next to this one is not a header cell then assume this cell is
  // a row header for it.
  uint32_t rowIdx = RowIdx(), colIdx = ColIdx();
  LocalAccessible* cell = table->CellAt(rowIdx, colIdx + ColExtent());
  if (cell && !nsCoreUtils::IsHTMLTableHeader(cell->GetContent())) {
    return roles::ROWHEADER;
  }

  // If the cell below this one is not a header cell then assume this cell is
  // a column header for it.
  uint32_t rowExtent = RowExtent();
  cell = table->CellAt(rowIdx + rowExtent, colIdx);
  if (cell && !nsCoreUtils::IsHTMLTableHeader(cell->GetContent())) {
    return roles::COLUMNHEADER;
  }

  // Otherwise if this cell is surrounded by header cells only then make a guess
  // based on its cell spanning. In other words if it is row spanned then assume
  // it's a row header, otherwise it's a column header.
  return rowExtent > 1 ? roles::ROWHEADER : roles::COLUMNHEADER;
}
