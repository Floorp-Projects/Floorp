/* -*- Mode: C++ final; tab-width: 2 final; indent-tabs-mode: nil final; c-basic-offset: 2 -*- */
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
  explicit xpcAccessibleTable(Accessible* aIntl) :
    xpcAccessibleGeneric(aIntl) { }

  xpcAccessibleTable(ProxyAccessible* aProxy, uint32_t aInterfaces) :
    xpcAccessibleGeneric(aProxy, aInterfaces) {}

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTable
  NS_IMETHOD GetCaption(nsIAccessible** aCaption) final override;
  NS_IMETHOD GetSummary(nsAString& aSummary) final override;
  NS_IMETHOD GetColumnCount(int32_t* aColumnCount) final override;
  NS_IMETHOD GetRowCount(int32_t* aRowCount) final override;
  NS_IMETHOD GetCellAt(int32_t aRowIndex, int32_t aColumnIndex,
                       nsIAccessible** aCell) final override;
  NS_IMETHOD GetCellIndexAt(int32_t aRowIndex, int32_t aColumnIndex,
                            int32_t* aCellIndex) final override;
  NS_IMETHOD GetColumnIndexAt(int32_t aCellIndex, int32_t* aColumnIndex)
    final override;
  NS_IMETHOD GetRowIndexAt(int32_t aCellIndex, int32_t* aRowIndex)
    final override;
  NS_IMETHOD GetRowAndColumnIndicesAt(int32_t aCellIndex, int32_t* aRowIndex,
                                      int32_t* aColumnIndex)
    final override;
  NS_IMETHOD GetColumnExtentAt(int32_t row, int32_t column,
                               int32_t* aColumnExtent) final override;
  NS_IMETHOD GetRowExtentAt(int32_t row, int32_t column,
                            int32_t* aRowExtent) final override;
  NS_IMETHOD GetColumnDescription(int32_t aColIdx, nsAString& aDescription)
    final override;
  NS_IMETHOD GetRowDescription(int32_t aRowIdx, nsAString& aDescription)
    final override;
  NS_IMETHOD IsColumnSelected(int32_t aColIdx, bool* _retval)
    final override;
  NS_IMETHOD IsRowSelected(int32_t aRowIdx, bool* _retval)
    final override;
  NS_IMETHOD IsCellSelected(int32_t aRowIdx, int32_t aColIdx, bool* _retval)
    final override;
  NS_IMETHOD GetSelectedCellCount(uint32_t* aSelectedCellCount)
    final override;
  NS_IMETHOD GetSelectedColumnCount(uint32_t* aSelectedColumnCount)
    final override;
  NS_IMETHOD GetSelectedRowCount(uint32_t* aSelectedRowCount)
    final override;
  NS_IMETHOD GetSelectedCells(nsIArray** aSelectedCell) final override;
  NS_IMETHOD GetSelectedCellIndices(uint32_t* aCellsArraySize,
                                    int32_t** aCellsArray)
    final override;
  NS_IMETHOD GetSelectedColumnIndices(uint32_t* aColsArraySize,
                                      int32_t** aColsArray)
    final override;
  NS_IMETHOD GetSelectedRowIndices(uint32_t* aRowsArraySize,
                                   int32_t** aRowsArray) final override;
  NS_IMETHOD SelectColumn(int32_t aColIdx) final override;
  NS_IMETHOD SelectRow(int32_t aRowIdx) final override;
  NS_IMETHOD UnselectColumn(int32_t aColIdx) final override;
  NS_IMETHOD UnselectRow(int32_t aRowIdx) final override;
  NS_IMETHOD IsProbablyForLayout(bool* aIsForLayout) final override;

protected:
  virtual ~xpcAccessibleTable() {}

private:
  TableAccessible* Intl()
  { return mIntl.IsAccessible() ? mIntl.AsAccessible()->AsTable() : nullptr; }

  xpcAccessibleTable(const xpcAccessibleTable&) = delete;
  xpcAccessibleTable& operator =(const xpcAccessibleTable&) = delete;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_xpcAccessibleTable_h_
