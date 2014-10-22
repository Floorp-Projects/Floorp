/* -*- Mode: C++ MOZ_FINAL; tab-width: 2 MOZ_FINAL; indent-tabs-mode: nil MOZ_FINAL; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcAccessibleTable_h_
#define mozilla_a11y_xpcAccessibleTable_h_

#include "nsIAccessibleTable.h"
#include "xpcAccessibleGeneric.h"

namespace mozilla {
namespace a11y {

/**
 * XPCOM wrapper around TableAccessible class.
 */
class xpcAccessibleTable : public xpcAccessibleGeneric,
                           public nsIAccessibleTable
{
public:
  xpcAccessibleTable(Accessible* aIntl) : xpcAccessibleGeneric(aIntl) { }

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTable
  NS_IMETHOD GetCaption(nsIAccessible** aCaption) MOZ_FINAL;
  NS_IMETHOD GetSummary(nsAString& aSummary) MOZ_FINAL;
  NS_IMETHOD GetColumnCount(int32_t* aColumnCount) MOZ_FINAL;
  NS_IMETHOD GetRowCount(int32_t* aRowCount) MOZ_FINAL;
  NS_IMETHOD GetCellAt(int32_t aRowIndex, int32_t aColumnIndex,
                       nsIAccessible** aCell) MOZ_FINAL;
  NS_IMETHOD GetCellIndexAt(int32_t aRowIndex, int32_t aColumnIndex,
                            int32_t* aCellIndex) MOZ_FINAL;
  NS_IMETHOD GetColumnIndexAt(int32_t aCellIndex, int32_t* aColumnIndex) MOZ_FINAL;
  NS_IMETHOD GetRowIndexAt(int32_t aCellIndex, int32_t* aRowIndex) MOZ_FINAL;
  NS_IMETHOD GetRowAndColumnIndicesAt(int32_t aCellIndex, int32_t* aRowIndex,
                                      int32_t* aColumnIndex) MOZ_FINAL;
  NS_IMETHOD GetColumnExtentAt(int32_t row, int32_t column,
                               int32_t* aColumnExtent) MOZ_FINAL;
  NS_IMETHOD GetRowExtentAt(int32_t row, int32_t column,
                            int32_t* aRowExtent) MOZ_FINAL;
  NS_IMETHOD GetColumnDescription(int32_t aColIdx, nsAString& aDescription) MOZ_FINAL;
  NS_IMETHOD GetRowDescription(int32_t aRowIdx, nsAString& aDescription) MOZ_FINAL;
  NS_IMETHOD IsColumnSelected(int32_t aColIdx, bool* _retval) MOZ_FINAL;
  NS_IMETHOD IsRowSelected(int32_t aRowIdx, bool* _retval) MOZ_FINAL;
  NS_IMETHOD IsCellSelected(int32_t aRowIdx, int32_t aColIdx, bool* _retval) MOZ_FINAL;
  NS_IMETHOD GetSelectedCellCount(uint32_t* aSelectedCellCount) MOZ_FINAL;
  NS_IMETHOD GetSelectedColumnCount(uint32_t* aSelectedColumnCount) MOZ_FINAL;
  NS_IMETHOD GetSelectedRowCount(uint32_t* aSelectedRowCount) MOZ_FINAL;
  NS_IMETHOD GetSelectedCells(nsIArray** aSelectedCell) MOZ_FINAL;
  NS_IMETHOD GetSelectedCellIndices(uint32_t* aCellsArraySize,
                                    int32_t** aCellsArray) MOZ_FINAL;
  NS_IMETHOD GetSelectedColumnIndices(uint32_t* aColsArraySize,
                                      int32_t** aColsArray) MOZ_FINAL;
  NS_IMETHOD GetSelectedRowIndices(uint32_t* aRowsArraySize,
                                   int32_t** aRowsArray) MOZ_FINAL;
  NS_IMETHOD SelectColumn(int32_t aColIdx) MOZ_FINAL;
  NS_IMETHOD SelectRow(int32_t aRowIdx) MOZ_FINAL;
  NS_IMETHOD UnselectColumn(int32_t aColIdx) MOZ_FINAL;
  NS_IMETHOD UnselectRow(int32_t aRowIdx) MOZ_FINAL;
  NS_IMETHOD IsProbablyForLayout(bool* aIsForLayout) MOZ_FINAL;

protected:
  virtual ~xpcAccessibleTable() {}

private:
  TableAccessible* Intl() { return mIntl->AsTable(); }

  xpcAccessibleTable(const xpcAccessibleTable&) MOZ_DELETE;
  xpcAccessibleTable& operator =(const xpcAccessibleTable&) MOZ_DELETE;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_xpcAccessibleTable_h_
