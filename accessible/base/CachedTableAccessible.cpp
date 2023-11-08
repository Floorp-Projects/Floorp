/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CachedTableAccessible.h"

#include "AccIterator.h"
#include "HTMLTableAccessible.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"
#include "nsAccUtils.h"
#include "nsIAccessiblePivot.h"
#include "Pivot.h"
#include "RemoteAccessible.h"

namespace mozilla::a11y {

// Used to search for table descendants relevant to table structure.
class TablePartRule : public PivotRule {
 public:
  virtual uint16_t Match(Accessible* aAcc) override {
    role accRole = aAcc->Role();
    if (accRole == roles::CAPTION || aAcc->IsTableCell()) {
      return nsIAccessibleTraversalRule::FILTER_MATCH |
             nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
    }
    if (aAcc->IsTableRow()) {
      return nsIAccessibleTraversalRule::FILTER_MATCH;
    }
    if (aAcc->IsTable() ||
        // Generic containers.
        accRole == roles::TEXT || accRole == roles::TEXT_CONTAINER ||
        accRole == roles::SECTION ||
        // Row groups.
        accRole == roles::GROUPING) {
      // Walk inside these, but don't match them.
      return nsIAccessibleTraversalRule::FILTER_IGNORE;
    }
    return nsIAccessibleTraversalRule::FILTER_IGNORE |
           nsIAccessibleTraversalRule::FILTER_IGNORE_SUBTREE;
  }
};

// The Accessible* keys should only be used for lookup. They should not be
// dereferenced.
using CachedTablesMap = nsTHashMap<Accessible*, CachedTableAccessible>;
// We use a global map rather than a map in each document for three reasons:
// 1. We don't have a common base class for local and remote documents.
// 2. It avoids wasting memory in a document that doesn't have any tables.
// 3. It allows the cache management to be encapsulated here in
// CachedTableAccessible.
static StaticAutoPtr<CachedTablesMap> sCachedTables;

/* static */
CachedTableAccessible* CachedTableAccessible::GetFrom(Accessible* aAcc) {
  MOZ_ASSERT(aAcc->IsTable());
  if (!sCachedTables) {
    sCachedTables = new CachedTablesMap();
    ClearOnShutdown(&sCachedTables);
  }
  return &sCachedTables->LookupOrInsertWith(
      aAcc, [&] { return CachedTableAccessible(aAcc); });
}

/* static */
void CachedTableAccessible::Invalidate(Accessible* aAcc) {
  if (!sCachedTables) {
    return;
  }

  if (Accessible* table = nsAccUtils::TableFor(aAcc)) {
    // Destroy the instance (if any). We'll create a new one the next time it
    // is requested.
    sCachedTables->Remove(table);
  }
}

CachedTableAccessible::CachedTableAccessible(Accessible* aAcc) : mAcc(aAcc) {
  MOZ_ASSERT(mAcc);
  // Build the cache. The cache can only be built once per instance. When it's
  // invalidated, we just throw away the instance and create a new one when
  // the cache is next needed.
  int32_t rowIdx = -1;
  uint32_t colIdx = 0;
  // Maps a column index to the cell index of its previous implicit column
  // header.
  nsTHashMap<uint32_t, uint32_t> prevColHeaders;
  Pivot pivot(mAcc);
  TablePartRule rule;
  for (Accessible* part = pivot.Next(mAcc, rule); part;
       part = pivot.Next(part, rule)) {
    role partRole = part->Role();
    if (partRole == roles::CAPTION) {
      // If there are multiple captions, use the first.
      if (!mCaptionAccID) {
        mCaptionAccID = part->ID();
      }
      continue;
    }
    if (part->IsTableRow()) {
      ++rowIdx;
      colIdx = 0;
      // This might be an empty row, so ensure a row here, as our row count is
      // based on the length of mRowColToCellIdx.
      EnsureRow(rowIdx);
      continue;
    }
    MOZ_ASSERT(part->IsTableCell());
    if (rowIdx == -1) {
      // We haven't created a row yet, so this cell must be outside a row.
      continue;
    }
    // Check for a cell spanning multiple rows which already occupies this
    // position. Keep incrementing until we find a vacant position.
    for (;;) {
      EnsureRowCol(rowIdx, colIdx);
      if (mRowColToCellIdx[rowIdx][colIdx] == kNoCellIdx) {
        // This position is not occupied.
        break;
      }
      // This position is occupied.
      ++colIdx;
    }
    // Create the cell.
    uint32_t cellIdx = mCells.Length();
    auto prevColHeader = prevColHeaders.MaybeGet(colIdx);
    auto cell = mCells.AppendElement(
        CachedTableCellAccessible(part->ID(), part, rowIdx, colIdx,
                                  prevColHeader ? *prevColHeader : kNoCellIdx));
    mAccToCellIdx.InsertOrUpdate(part, cellIdx);
    // Update our row/col map.
    // This cell might span multiple rows and/or columns. In that case, we need
    // to occupy multiple coordinates in the row/col map.
    uint32_t lastRowForCell =
        static_cast<uint32_t>(rowIdx) + cell->RowExtent() - 1;
    MOZ_ASSERT(lastRowForCell >= static_cast<uint32_t>(rowIdx));
    uint32_t lastColForCell = colIdx + cell->ColExtent() - 1;
    MOZ_ASSERT(lastColForCell >= colIdx);
    for (uint32_t spannedRow = static_cast<uint32_t>(rowIdx);
         spannedRow <= lastRowForCell; ++spannedRow) {
      for (uint32_t spannedCol = colIdx; spannedCol <= lastColForCell;
           ++spannedCol) {
        EnsureRowCol(spannedRow, spannedCol);
        auto& rowCol = mRowColToCellIdx[spannedRow][spannedCol];
        // If a cell already occupies this position, it overlaps with this one;
        // e.g. r1..2c2 and r2c1..2. In that case, we want to prefer the first
        // cell.
        if (rowCol == kNoCellIdx) {
          rowCol = cellIdx;
        }
      }
    }
    if (partRole == roles::COLUMNHEADER) {
      for (uint32_t spannedCol = colIdx; spannedCol <= lastColForCell;
           ++spannedCol) {
        prevColHeaders.InsertOrUpdate(spannedCol, cellIdx);
      }
    }
    // Increment for the next cell.
    colIdx = lastColForCell + 1;
  }
}

void CachedTableAccessible::EnsureRow(uint32_t aRowIdx) {
  if (mRowColToCellIdx.Length() <= aRowIdx) {
    mRowColToCellIdx.AppendElements(aRowIdx - mRowColToCellIdx.Length() + 1);
  }
  MOZ_ASSERT(mRowColToCellIdx.Length() > aRowIdx);
}

void CachedTableAccessible::EnsureRowCol(uint32_t aRowIdx, uint32_t aColIdx) {
  EnsureRow(aRowIdx);
  auto& row = mRowColToCellIdx[aRowIdx];
  if (mColCount <= aColIdx) {
    mColCount = aColIdx + 1;
  }
  row.SetCapacity(mColCount);
  for (uint32_t newCol = row.Length(); newCol <= aColIdx; ++newCol) {
    // An entry doesn't yet exist for this column in this row.
    row.AppendElement(kNoCellIdx);
  }
  MOZ_ASSERT(row.Length() > aColIdx);
}

Accessible* CachedTableAccessible::Caption() const {
  if (mCaptionAccID) {
    Accessible* caption = nsAccUtils::GetAccessibleByID(
        nsAccUtils::DocumentFor(mAcc), mCaptionAccID);
    MOZ_ASSERT(caption, "Dead caption Accessible!");
    MOZ_ASSERT(caption->Role() == roles::CAPTION, "Caption has wrong role");
    return caption;
  }
  return nullptr;
}

void CachedTableAccessible::Summary(nsString& aSummary) {
  if (Caption()) {
    // If there's a caption, we map caption to Name and summary to Description.
    mAcc->Description(aSummary);
  } else {
    // If there's no caption, we map summary to Name.
    mAcc->Name(aSummary);
  }
}

Accessible* CachedTableAccessible::CellAt(uint32_t aRowIdx, uint32_t aColIdx) {
  int32_t cellIdx = CellIndexAt(aRowIdx, aColIdx);
  if (cellIdx == -1) {
    return nullptr;
  }
  return mCells[cellIdx].Acc(mAcc);
}

bool CachedTableAccessible::IsProbablyLayoutTable() {
  if (RemoteAccessible* remoteAcc = mAcc->AsRemote()) {
    return remoteAcc->TableIsProbablyForLayout();
  }
  if (auto* localTable = HTMLTableAccessible::GetFrom(mAcc->AsLocal())) {
    return localTable->IsProbablyLayoutTable();
  }
  return false;
}

/* static */
CachedTableCellAccessible* CachedTableCellAccessible::GetFrom(
    Accessible* aAcc) {
  MOZ_ASSERT(aAcc->IsTableCell());
  for (Accessible* parent = aAcc; parent; parent = parent->Parent()) {
    if (parent->IsDoc()) {
      break;  // Never cross document boundaries.
    }
    TableAccessible* table = parent->AsTable();
    if (!table) {
      continue;
    }
    if (LocalAccessible* local = parent->AsLocal()) {
      nsIContent* content = local->GetContent();
      if (content && content->IsXULElement()) {
        // XUL tables don't use CachedTableAccessible.
        break;
      }
    }
    // Non-XUL tables only use CachedTableAccessible.
    auto* cachedTable = static_cast<CachedTableAccessible*>(table);
    if (auto cellIdx = cachedTable->mAccToCellIdx.Lookup(aAcc)) {
      return &cachedTable->mCells[*cellIdx];
    }
    // We found a table, but it doesn't know about this cell. This can happen
    // if a cell is outside of a row due to authoring error. We must not search
    // ancestor tables, since this cell's data is not valid there and vice
    // versa.
    break;
  }
  return nullptr;
}

Accessible* CachedTableCellAccessible::Acc(Accessible* aTableAcc) const {
  Accessible* acc =
      nsAccUtils::GetAccessibleByID(nsAccUtils::DocumentFor(aTableAcc), mAccID);
  MOZ_DIAGNOSTIC_ASSERT(acc == mAcc, "Cell's cached mAcc is dead!");
  return acc;
}

TableAccessible* CachedTableCellAccessible::Table() const {
  for (const Accessible* acc = mAcc; acc; acc = acc->Parent()) {
    // Since the caller has this cell, the table is already created, so it's
    // okay to ignore the const restriction here.
    if (TableAccessible* table = const_cast<Accessible*>(acc)->AsTable()) {
      return table;
    }
  }
  return nullptr;
}

uint32_t CachedTableCellAccessible::ColExtent() const {
  if (RemoteAccessible* remoteAcc = mAcc->AsRemote()) {
    if (remoteAcc->mCachedFields) {
      if (auto colSpan = remoteAcc->mCachedFields->GetAttribute<int32_t>(
              CacheKey::ColSpan)) {
        MOZ_ASSERT(*colSpan > 0);
        return *colSpan;
      }
    }
  } else if (auto* cell = HTMLTableCellAccessible::GetFrom(mAcc->AsLocal())) {
    // For HTML table cells, we must use the HTMLTableCellAccessible
    // GetColExtent method rather than using the DOM attributes directly.
    // This is because of things like rowspan="0" which depend on knowing
    // about thead, tbody, etc., which is info we don't have in the a11y tree.
    uint32_t colExtent = cell->ColExtent();
    MOZ_ASSERT(colExtent > 0);
    if (colExtent > 0) {
      return colExtent;
    }
  }
  return 1;
}

uint32_t CachedTableCellAccessible::RowExtent() const {
  if (RemoteAccessible* remoteAcc = mAcc->AsRemote()) {
    if (remoteAcc->mCachedFields) {
      if (auto rowSpan = remoteAcc->mCachedFields->GetAttribute<int32_t>(
              CacheKey::RowSpan)) {
        MOZ_ASSERT(*rowSpan > 0);
        return *rowSpan;
      }
    }
  } else if (auto* cell = HTMLTableCellAccessible::GetFrom(mAcc->AsLocal())) {
    // For HTML table cells, we must use the HTMLTableCellAccessible
    // GetRowExtent method rather than using the DOM attributes directly.
    // This is because of things like rowspan="0" which depend on knowing
    // about thead, tbody, etc., which is info we don't have in the a11y tree.
    uint32_t rowExtent = cell->RowExtent();
    MOZ_ASSERT(rowExtent > 0);
    if (rowExtent > 0) {
      return rowExtent;
    }
  }
  return 1;
}

UniquePtr<AccIterable> CachedTableCellAccessible::GetExplicitHeadersIterator() {
  if (RemoteAccessible* remoteAcc = mAcc->AsRemote()) {
    if (remoteAcc->mCachedFields) {
      if (auto headers =
              remoteAcc->mCachedFields->GetAttribute<nsTArray<uint64_t>>(
                  CacheKey::CellHeaders)) {
        return MakeUnique<RemoteAccIterator>(*headers, remoteAcc->Document());
      }
    }
  } else if (LocalAccessible* localAcc = mAcc->AsLocal()) {
    return MakeUnique<IDRefsIterator>(
        localAcc->Document(), localAcc->GetContent(), nsGkAtoms::headers);
  }
  return nullptr;
}

void CachedTableCellAccessible::ColHeaderCells(nsTArray<Accessible*>* aCells) {
  auto* table = static_cast<CachedTableAccessible*>(Table());
  if (!table) {
    return;
  }
  if (auto iter = GetExplicitHeadersIterator()) {
    while (Accessible* header = iter->Next()) {
      role headerRole = header->Role();
      if (headerRole == roles::COLUMNHEADER) {
        aCells->AppendElement(header);
      } else if (headerRole != roles::ROWHEADER) {
        // Treat this cell as a column header only if it's in the same column.
        if (auto cellIdx = table->mAccToCellIdx.Lookup(header)) {
          CachedTableCellAccessible& cell = table->mCells[*cellIdx];
          if (cell.ColIdx() == ColIdx()) {
            aCells->AppendElement(header);
          }
        }
      }
    }
    if (!aCells->IsEmpty()) {
      return;
    }
  }
  Accessible* doc = nsAccUtils::DocumentFor(table->AsAccessible());
  // Each cell stores its previous implicit column header, effectively forming a
  // linked list. We traverse that to get all the headers.
  CachedTableCellAccessible* cell = this;
  for (;;) {
    if (cell->mPrevColHeaderCellIdx == kNoCellIdx) {
      break;  // No more headers.
    }
    cell = &table->mCells[cell->mPrevColHeaderCellIdx];
    Accessible* cellAcc = nsAccUtils::GetAccessibleByID(doc, cell->mAccID);
    aCells->AppendElement(cellAcc);
  }
}

void CachedTableCellAccessible::RowHeaderCells(nsTArray<Accessible*>* aCells) {
  auto* table = static_cast<CachedTableAccessible*>(Table());
  if (!table) {
    return;
  }
  if (auto iter = GetExplicitHeadersIterator()) {
    while (Accessible* header = iter->Next()) {
      role headerRole = header->Role();
      if (headerRole == roles::ROWHEADER) {
        aCells->AppendElement(header);
      } else if (headerRole != roles::COLUMNHEADER) {
        // Treat this cell as a row header only if it's in the same row.
        if (auto cellIdx = table->mAccToCellIdx.Lookup(header)) {
          CachedTableCellAccessible& cell = table->mCells[*cellIdx];
          if (cell.RowIdx() == RowIdx()) {
            aCells->AppendElement(header);
          }
        }
      }
    }
    if (!aCells->IsEmpty()) {
      return;
    }
  }
  Accessible* doc = nsAccUtils::DocumentFor(table->AsAccessible());
  // We don't cache implicit row headers because there are usually not that many
  // cells per row. Get all the row headers on the row before this cell.
  uint32_t row = RowIdx();
  uint32_t thisCol = ColIdx();
  for (uint32_t col = thisCol - 1; col < thisCol; --col) {
    int32_t cellIdx = table->CellIndexAt(row, col);
    if (cellIdx == -1) {
      continue;
    }
    CachedTableCellAccessible& cell = table->mCells[cellIdx];
    Accessible* cellAcc = nsAccUtils::GetAccessibleByID(doc, cell.mAccID);
    MOZ_ASSERT(cellAcc);
    // cell might span multiple columns. We don't want to visit it multiple
    // times, so ensure col is set to cell's starting column.
    col = cell.ColIdx();
    if (cellAcc->Role() != roles::ROWHEADER) {
      continue;
    }
    aCells->AppendElement(cellAcc);
  }
}

bool CachedTableCellAccessible::Selected() {
  return mAcc->State() & states::SELECTED;
}

}  // namespace mozilla::a11y
