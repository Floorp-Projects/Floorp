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

#define NS_DECL_OR_FORWARD_NSIACCESSIBLETABLE_WITH_XPCACCESSIBLETABLE \
  NS_IMETHOD GetCaption(nsIAccessible** aCaption) \
    { return xpcAccessibleTable::GetCaption(aCaption); } \
  NS_SCRIPTABLE NS_IMETHOD GetSummary(nsAString & aSummary) \
    { return xpcAccessibleTable::GetSummary(aSummary); } \
  NS_SCRIPTABLE NS_IMETHOD GetColumnCount(PRInt32* aColumnCount) \
    { return xpcAccessibleTable::GetColumnCount(aColumnCount); } \
  NS_SCRIPTABLE NS_IMETHOD GetRowCount(PRInt32* aRowCount) \
    { return xpcAccessibleTable::GetRowCount(aRowCount); } \
  NS_SCRIPTABLE NS_IMETHOD GetCellAt(PRInt32 rowIndex, PRInt32 columnIndex, nsIAccessible** _retval NS_OUTPARAM) \
    { return xpcAccessibleTable::GetCellAt(rowIndex, columnIndex, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD GetCellIndexAt(PRInt32 rowIndex, PRInt32 columnIndex, PRInt32 *_retval NS_OUTPARAM) \
    { return xpcAccessibleTable::GetCellIndexAt(rowIndex, columnIndex, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD GetColumnIndexAt(PRInt32 cellIndex, PRInt32 *_retval NS_OUTPARAM) \
    { return xpcAccessibleTable::GetColumnIndexAt(cellIndex, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD GetRowIndexAt(PRInt32 cellIndex, PRInt32 *_retval NS_OUTPARAM) \
    { return xpcAccessibleTable::GetRowIndexAt(cellIndex, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD GetRowAndColumnIndicesAt(PRInt32 cellIndex, PRInt32 *rowIndex NS_OUTPARAM, PRInt32 *columnIndex NS_OUTPARAM) \
    { return xpcAccessibleTable::GetRowAndColumnIndicesAt(cellIndex, rowIndex, columnIndex); } \
  NS_SCRIPTABLE NS_IMETHOD GetColumnExtentAt(PRInt32 row, PRInt32 column, PRInt32* _retval NS_OUTPARAM) \
    { return xpcAccessibleTable::GetColumnExtentAt(row, column, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD GetRowExtentAt(PRInt32 row, PRInt32 column, PRInt32* _retval NS_OUTPARAM) \
    { return xpcAccessibleTable::GetRowExtentAt(row, column, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD GetColumnDescription(PRInt32 columnIndex, nsAString& _retval NS_OUTPARAM) \
    { return xpcAccessibleTable::GetColumnDescription(columnIndex, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD GetRowDescription(PRInt32 rowIndex, nsAString& _retval NS_OUTPARAM) \
    { return xpcAccessibleTable::GetRowDescription(rowIndex, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD IsColumnSelected(PRInt32 colIdx, bool* _retval NS_OUTPARAM) \
    { return xpcAccessibleTable::IsColumnSelected(colIdx, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD IsRowSelected(PRInt32 rowIdx, bool* _retval NS_OUTPARAM) \
    { return xpcAccessibleTable::IsRowSelected(rowIdx, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD IsCellSelected(PRInt32 rowIdx, PRInt32 colIdx, bool* _retval NS_OUTPARAM) \
    { return xpcAccessibleTable::IsCellSelected(rowIdx, colIdx, _retval); } \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedCellCount(PRUint32* aSelectedCellCount) \
    { return xpcAccessibleTable::GetSelectedCellCount(aSelectedCellCount); } \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedColumnCount(PRUint32* aSelectedColumnCount) \
    { return xpcAccessibleTable::GetSelectedColumnCount(aSelectedColumnCount); } \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedRowCount(PRUint32* aSelectedRowCount) \
    { return xpcAccessibleTable::GetSelectedRowCount(aSelectedRowCount); } \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedCells(nsIArray** aSelectedCells) \
    { return xpcAccessibleTable::GetSelectedCells(aSelectedCells); } \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedCellIndices(PRUint32* cellsArraySize NS_OUTPARAM, \
                                                  PRInt32** cellsArray NS_OUTPARAM) \
    { return xpcAccessibleTable::GetSelectedCellIndices(cellsArraySize, cellsArray); } \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedColumnIndices(PRUint32* colsArraySize NS_OUTPARAM, \
                                                    PRInt32** colsArray NS_OUTPARAM) \
    { return xpcAccessibleTable::GetSelectedColumnIndices(colsArraySize, colsArray); } \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedRowIndices(PRUint32* rowsArraySize NS_OUTPARAM, \
                                                 PRInt32** rowsArray NS_OUTPARAM) \
    { return xpcAccessibleTable::GetSelectedRowIndices(rowsArraySize, rowsArray); } \
  NS_SCRIPTABLE NS_IMETHOD SelectRow(PRInt32 aRowIdx) \
    { return xpcAccessibleTable::SelectRow(aRowIdx); } \
  NS_SCRIPTABLE NS_IMETHOD SelectColumn(PRInt32 aColIdx) \
    { return xpcAccessibleTable::SelectColumn(aColIdx); } \
  NS_SCRIPTABLE NS_IMETHOD UnselectColumn(PRInt32 aColIdx) \
    { return xpcAccessibleTable::UnselectColumn(aColIdx); } \
  NS_IMETHOD UnselectRow(PRInt32 aRowIdx) \
    { return xpcAccessibleTable::UnselectRow(aRowIdx); } \
  NS_IMETHOD IsProbablyForLayout(bool* aResult) \
  { return xpcAccessibleTable::IsProbablyForLayout(aResult); } \

#endif // MOZILLA_A11Y_XPCOM_XPACCESSIBLETABLE_H_
