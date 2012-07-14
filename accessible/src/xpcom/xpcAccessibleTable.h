/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_XPCOM_XPACCESSIBLETABLE_H_
#define MOZILLA_A11Y_XPCOM_XPACCESSIBLETABLE_H_

#include "nsAString.h"
#include "nscore.h"


class nsIAccessible;
class nsIArray;
namespace mozilla {
namespace a11y {
class TableAccessible;
}
}

class xpcAccessibleTable
{
public:
  xpcAccessibleTable(mozilla::a11y::TableAccessible* aTable) : mTable(aTable) { }

  nsresult GetCaption(nsIAccessible** aCaption);
  nsresult GetSummary(nsAString& aSummary);
  nsresult GetColumnCount(PRInt32* aColumnCount);
  nsresult GetRowCount(PRInt32* aRowCount);
  nsresult GetCellAt(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                     nsIAccessible** aCell);
  nsresult GetCellIndexAt(PRInt32 aRowIndex, PRInt32 aColumnIndex,
                          PRInt32* aCellIndex);
  nsresult GetColumnIndexAt(PRInt32 aCellIndex, PRInt32* aColumnIndex);
  nsresult GetRowIndexAt(PRInt32 aCellIndex, PRInt32* aRowIndex);
  nsresult GetRowAndColumnIndicesAt(PRInt32 aCellIndex, PRInt32* aRowIndex,
                                    PRInt32* aColumnIndex);
  nsresult GetColumnExtentAt(PRInt32 row, PRInt32 column,
                             PRInt32* aColumnExtent);
  nsresult GetRowExtentAt(PRInt32 row, PRInt32 column,
                          PRInt32* aRowExtent);
  nsresult GetColumnDescription(PRInt32 aColIdx, nsAString& aDescription);
  nsresult GetRowDescription(PRInt32 aRowIdx, nsAString& aDescription);
  nsresult IsColumnSelected(PRInt32 aColIdx, bool* _retval);
  nsresult IsRowSelected(PRInt32 aRowIdx, bool* _retval);
  nsresult IsCellSelected(PRInt32 aRowIdx, PRInt32 aColIdx, bool* _retval);
  nsresult GetSelectedCellCount(PRUint32* aSelectedCellCount);
  nsresult GetSelectedColumnCount(PRUint32* aSelectedColumnCount);
  nsresult GetSelectedRowCount(PRUint32* aSelectedRowCount);
  nsresult GetSelectedCells(nsIArray** aSelectedCell);
  nsresult GetSelectedCellIndices(PRUint32* aCellsArraySize,
                                  PRInt32** aCellsArray);
  nsresult GetSelectedColumnIndices(PRUint32* aColsArraySize,
                                    PRInt32** aColsArray);
  nsresult GetSelectedRowIndices(PRUint32* aRowsArraySize,
                                 PRInt32** aRowsArray);
  nsresult SelectColumn(PRInt32 aColIdx);
  nsresult SelectRow(PRInt32 aRowIdx);
  nsresult UnselectColumn(PRInt32 aColIdx);
  nsresult UnselectRow(PRInt32 aRowIdx);
  nsresult IsProbablyForLayout(bool* aIsForLayout);

protected:
  mozilla::a11y::TableAccessible* mTable;
};

#endif // MOZILLA_A11Y_XPCOM_XPACCESSIBLETABLE_H_
