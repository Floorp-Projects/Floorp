/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TABLE_ACCESSIBLE_H
#define TABLE_ACCESSIBLE_H

#include "LocalAccessible.h"
#include "mozilla/a11y/TableAccessibleBase.h"
#include "mozilla/a11y/TableCellAccessibleBase.h"
#include "nsPointerHashKeys.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace a11y {

/**
 * Base class for LocalAccessible table implementations.
 */
class TableAccessible : public TableAccessibleBase {
 public:
  virtual LocalAccessible* Caption() const override { return nullptr; }

  virtual LocalAccessible* CellAt(uint32_t aRowIdx, uint32_t aColIdx) override {
    return nullptr;
  }

  virtual int32_t CellIndexAt(uint32_t aRowIdx, uint32_t aColIdx) override {
    return ColCount() * aRowIdx + aColIdx;
  }

  virtual int32_t ColIndexAt(uint32_t aCellIdx) override;
  virtual int32_t RowIndexAt(uint32_t aCellIdx) override;
  virtual void RowAndColIndicesAt(uint32_t aCellIdx, int32_t* aRowIdx,
                                  int32_t* aColIdx) override;
  virtual bool IsProbablyLayoutTable() override;
  virtual LocalAccessible* AsAccessible() override = 0;

  using HeaderCache =
      nsRefPtrHashtable<nsPtrHashKey<const TableCellAccessibleBase>,
                        LocalAccessible>;

  /**
   * Get the header cache, which maps a TableCellAccessible to its previous
   * header.
   * Although this data is only used in TableCellAccessible, it is stored on
   * TableAccessible so the cache can be easily invalidated when the table
   * is mutated.
   */
  HeaderCache& GetHeaderCache() { return mHeaderCache; }

 protected:
  /**
   * Return row accessible at the given row index.
   */
  LocalAccessible* RowAt(int32_t aRow);

  /**
   * Return cell accessible at the given column index in the row.
   */
  LocalAccessible* CellInRowAt(LocalAccessible* aRow, int32_t aColumn);

 private:
  HeaderCache mHeaderCache;
};

}  // namespace a11y
}  // namespace mozilla

#endif
