/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CACHED_TABLE_ACCESSIBLE_H
#define CACHED_TABLE_ACCESSIBLE_H

#include "mozilla/a11y/TableAccessible.h"
#include "mozilla/a11y/TableCellAccessible.h"
#include "mozilla/UniquePtr.h"
#include "nsTHashMap.h"

namespace mozilla::a11y {

const uint32_t kNoCellIdx = UINT32_MAX;

class AccIterable;

class CachedTableAccessible;

class CachedTableCellAccessible final : public TableCellAccessible {
 public:
  static CachedTableCellAccessible* GetFrom(Accessible* aAcc);

  virtual TableAccessible* Table() const override;

  virtual uint32_t ColIdx() const override {
    return static_cast<int32_t>(mColIdx);
  }

  virtual uint32_t RowIdx() const override {
    return static_cast<int32_t>(mRowIdx);
  }

  virtual uint32_t ColExtent() const override;

  virtual uint32_t RowExtent() const override;

  virtual void ColHeaderCells(nsTArray<Accessible*>* aCells) override;

  virtual void RowHeaderCells(nsTArray<Accessible*>* aCells) override;

  virtual bool Selected() override;

 private:
  CachedTableCellAccessible(uint64_t aAccID, Accessible* aAcc, uint32_t aRowIdx,
                            uint32_t aColIdx, uint32_t aPrevColHeaderCellIdx)
      : mAccID(aAccID),
        mAcc(aAcc),
        mRowIdx(aRowIdx),
        mColIdx(aColIdx),
        mPrevColHeaderCellIdx(aPrevColHeaderCellIdx) {}

  // Get the Accessible for this table cell given its ancestor table Accessible,
  // verifying that the Accessible is valid.
  Accessible* Acc(Accessible* aTableAcc) const;

  UniquePtr<AccIterable> GetExplicitHeadersIterator();

  uint64_t mAccID;
  // CachedTableAccessible methods which fetch a cell should retrieve the
  // Accessible using Acc() rather than using mAcc. We need mAcc for some
  // methods because we can't fetch a document by id. It's okay to use mAcc in
  // these methods because the caller has to hold the Accessible in order to
  // call them.
  Accessible* mAcc;
  uint32_t mRowIdx;
  uint32_t mColIdx;
  // The cell index of the previous implicit column header.
  uint32_t mPrevColHeaderCellIdx;
  friend class CachedTableAccessible;
};

/**
 * TableAccessible implementation which builds and queries a cache.
 */
class CachedTableAccessible final : public TableAccessible {
 public:
  static CachedTableAccessible* GetFrom(Accessible* aAcc);

  /**
   * This must be called whenever a table is destroyed or the structure of a
   * table changes; e.g. cells wer added or removed. It can be called with
   * either a table or a cell.
   */
  static void Invalidate(Accessible* aAcc);

  virtual Accessible* Caption() const override;
  virtual void Summary(nsString& aSummary) override;

  virtual uint32_t ColCount() const override { return mColCount; }

  virtual uint32_t RowCount() override { return mRowColToCellIdx.Length(); }

  virtual int32_t ColIndexAt(uint32_t aCellIdx) override {
    if (aCellIdx < mCells.Length()) {
      return static_cast<int32_t>(mCells[aCellIdx].mColIdx);
    }
    return -1;
  }

  virtual int32_t RowIndexAt(uint32_t aCellIdx) override {
    if (aCellIdx < mCells.Length()) {
      return static_cast<int32_t>(mCells[aCellIdx].mRowIdx);
    }
    return -1;
  }

  virtual void RowAndColIndicesAt(uint32_t aCellIdx, int32_t* aRowIdx,
                                  int32_t* aColIdx) override {
    if (aCellIdx < mCells.Length()) {
      CachedTableCellAccessible& cell = mCells[aCellIdx];
      *aRowIdx = static_cast<int32_t>(cell.mRowIdx);
      *aColIdx = static_cast<int32_t>(cell.mColIdx);
      return;
    }
    *aRowIdx = -1;
    *aColIdx = -1;
  }

  virtual uint32_t ColExtentAt(uint32_t aRowIdx, uint32_t aColIdx) override {
    int32_t cellIdx = CellIndexAt(aRowIdx, aColIdx);
    if (cellIdx == -1) {
      return 0;
    }
    // Verify that the cell's Accessible is valid.
    mCells[cellIdx].Acc(mAcc);
    return mCells[cellIdx].ColExtent();
  }

  virtual uint32_t RowExtentAt(uint32_t aRowIdx, uint32_t aColIdx) override {
    int32_t cellIdx = CellIndexAt(aRowIdx, aColIdx);
    if (cellIdx == -1) {
      return 0;
    }
    // Verify that the cell's Accessible is valid.
    mCells[cellIdx].Acc(mAcc);
    return mCells[cellIdx].RowExtent();
  }

  virtual int32_t CellIndexAt(uint32_t aRowIdx, uint32_t aColIdx) override {
    if (aRowIdx < mRowColToCellIdx.Length()) {
      auto& row = mRowColToCellIdx[aRowIdx];
      if (aColIdx < row.Length()) {
        uint32_t cellIdx = row[aColIdx];
        if (cellIdx != kNoCellIdx) {
          return static_cast<int32_t>(cellIdx);
        }
      }
    }
    return -1;
  }

  virtual Accessible* CellAt(uint32_t aRowIdx, uint32_t aColIdx) override;

  virtual bool IsColSelected(uint32_t aColIdx) override {
    bool selected = false;
    for (uint32_t row = 0; row < RowCount(); ++row) {
      selected = IsCellSelected(row, aColIdx);
      if (!selected) {
        break;
      }
    }
    return selected;
  }

  virtual bool IsRowSelected(uint32_t aRowIdx) override {
    bool selected = false;
    for (uint32_t col = 0; col < mColCount; ++col) {
      selected = IsCellSelected(aRowIdx, col);
      if (!selected) {
        break;
      }
    }
    return selected;
  }

  virtual bool IsCellSelected(uint32_t aRowIdx, uint32_t aColIdx) override {
    int32_t cellIdx = CellIndexAt(aRowIdx, aColIdx);
    if (cellIdx == -1) {
      return false;
    }
    // Verify that the cell's Accessible is valid.
    mCells[cellIdx].Acc(mAcc);
    return mCells[cellIdx].Selected();
  }

  virtual uint32_t SelectedCellCount() override {
    uint32_t count = 0;
    for (auto& cell : mCells) {
      // Verify that the cell's Accessible is valid.
      cell.Acc(mAcc);
      if (cell.Selected()) {
        ++count;
      }
    }
    return count;
  }

  virtual uint32_t SelectedColCount() override {
    uint32_t count = 0;
    for (uint32_t col = 0; col < mColCount; ++col) {
      if (IsColSelected(col)) {
        ++count;
      }
    }
    return count;
  }

  virtual uint32_t SelectedRowCount() override {
    uint32_t count = 0;
    for (uint32_t row = 0; row < RowCount(); ++row) {
      if (IsRowSelected(row)) {
        ++count;
      }
    }
    return count;
  }

  virtual void SelectedCells(nsTArray<Accessible*>* aCells) override {
    for (auto& cell : mCells) {
      // Verify that the cell's Accessible is valid.
      Accessible* acc = cell.Acc(mAcc);
      if (cell.Selected()) {
        aCells->AppendElement(acc);
      }
    }
  }

  virtual void SelectedCellIndices(nsTArray<uint32_t>* aCells) override {
    for (uint32_t idx = 0; idx < mCells.Length(); ++idx) {
      CachedTableCellAccessible& cell = mCells[idx];
      // Verify that the cell's Accessible is valid.
      cell.Acc(mAcc);
      if (cell.Selected()) {
        aCells->AppendElement(idx);
      }
    }
  }

  virtual void SelectedColIndices(nsTArray<uint32_t>* aCols) override {
    for (uint32_t col = 0; col < mColCount; ++col) {
      if (IsColSelected(col)) {
        aCols->AppendElement(col);
      }
    }
  }

  virtual void SelectedRowIndices(nsTArray<uint32_t>* aRows) override {
    for (uint32_t row = 0; row < RowCount(); ++row) {
      if (IsRowSelected(row)) {
        aRows->AppendElement(row);
      }
    }
  }

  virtual Accessible* AsAccessible() override { return mAcc; }

  virtual bool IsProbablyLayoutTable() override;

 private:
  explicit CachedTableAccessible(Accessible* aAcc);

  // Ensure that the given row exists in our data structure, creating array
  // elements as needed.
  void EnsureRow(uint32_t aRowIdx);

  // Ensure that the given row and column coordinate exists in our data
  // structure, creating array elements as needed. A newly created coordinate
  // will be set to kNoCellIdx.
  void EnsureRowCol(uint32_t aRowIdx, uint32_t aColIdx);

  Accessible* mAcc;  // The table Accessible.
  // We track the column count because it might not be uniform across rows in
  // malformed tables.
  uint32_t mColCount = 0;
  // An array of cell instances. A cell index is an index into this array.
  nsTArray<CachedTableCellAccessible> mCells;
  // Maps row and column coordinates to cell indices.
  nsTArray<nsTArray<uint32_t>> mRowColToCellIdx;
  // Maps Accessibles to cell indexes to facilitate retrieval of a cell
  // instance from a cell Accessible. The Accessible* keys should only be used
  // for lookup. They should not be dereferenced.
  nsTHashMap<Accessible*, uint32_t> mAccToCellIdx;
  uint64_t mCaptionAccID = 0;

  friend class CachedTableCellAccessible;
};

}  // namespace mozilla::a11y

#endif
