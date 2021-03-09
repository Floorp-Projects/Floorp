/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleTable_h_
#define mozilla_a11y_xpcAccessibleTable_h_

#include "nsIAccessibleTable.h"
#include "xpcAccessibleHyperText.h"

namespace mozilla {
namespace a11y {

/**
 * XPCOM wrapper around TableAccessible class.
 */
class xpcAccessibleTable : public xpcAccessibleHyperText,
                           public nsIAccessibleTable {
 public:
  explicit xpcAccessibleTable(Accessible* aIntl)
      : xpcAccessibleHyperText(aIntl) {}

  xpcAccessibleTable(RemoteAccessible* aProxy, uint32_t aInterfaces)
      : xpcAccessibleHyperText(aProxy, aInterfaces) {}

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTable
  NS_IMETHOD GetCaption(nsIAccessible** aCaption) final;
  NS_IMETHOD GetSummary(nsAString& aSummary) final;
  NS_IMETHOD GetColumnCount(int32_t* aColumnCount) final;
  NS_IMETHOD GetRowCount(int32_t* aRowCount) final;
  NS_IMETHOD GetCellAt(int32_t aRowIndex, int32_t aColumnIndex,
                       nsIAccessible** aCell) final;
  NS_IMETHOD GetCellIndexAt(int32_t aRowIndex, int32_t aColumnIndex,
                            int32_t* aCellIndex) final;
  NS_IMETHOD GetColumnIndexAt(int32_t aCellIndex, int32_t* aColumnIndex) final;
  NS_IMETHOD GetRowIndexAt(int32_t aCellIndex, int32_t* aRowIndex) final;
  NS_IMETHOD GetRowAndColumnIndicesAt(int32_t aCellIndex, int32_t* aRowIndex,
                                      int32_t* aColumnIndex) final;
  NS_IMETHOD GetColumnExtentAt(int32_t row, int32_t column,
                               int32_t* aColumnExtent) final;
  NS_IMETHOD GetRowExtentAt(int32_t row, int32_t column,
                            int32_t* aRowExtent) final;
  NS_IMETHOD GetColumnDescription(int32_t aColIdx,
                                  nsAString& aDescription) final;
  NS_IMETHOD GetRowDescription(int32_t aRowIdx, nsAString& aDescription) final;
  NS_IMETHOD IsColumnSelected(int32_t aColIdx, bool* _retval) final;
  NS_IMETHOD IsRowSelected(int32_t aRowIdx, bool* _retval) final;
  NS_IMETHOD IsCellSelected(int32_t aRowIdx, int32_t aColIdx,
                            bool* _retval) final;
  NS_IMETHOD GetSelectedCellCount(uint32_t* aSelectedCellCount) final;
  NS_IMETHOD GetSelectedColumnCount(uint32_t* aSelectedColumnCount) final;
  NS_IMETHOD GetSelectedRowCount(uint32_t* aSelectedRowCount) final;
  NS_IMETHOD GetSelectedCells(nsIArray** aSelectedCell) final;
  NS_IMETHOD GetSelectedCellIndices(nsTArray<uint32_t>& aCellsArray) final;
  NS_IMETHOD GetSelectedColumnIndices(nsTArray<uint32_t>& aColsArray) final;
  NS_IMETHOD GetSelectedRowIndices(nsTArray<uint32_t>& aRowsArray) final;
  NS_IMETHOD SelectColumn(int32_t aColIdx) final;
  NS_IMETHOD SelectRow(int32_t aRowIdx) final;
  NS_IMETHOD UnselectColumn(int32_t aColIdx) final;
  NS_IMETHOD UnselectRow(int32_t aRowIdx) final;
  NS_IMETHOD IsProbablyForLayout(bool* aIsForLayout) final;

 protected:
  virtual ~xpcAccessibleTable() {}

 private:
  TableAccessible* Intl() {
    return mIntl->IsLocal() ? mIntl->AsLocal()->AsTable() : nullptr;
  }

  xpcAccessibleTable(const xpcAccessibleTable&) = delete;
  xpcAccessibleTable& operator=(const xpcAccessibleTable&) = delete;
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_xpcAccessibleTable_h_
