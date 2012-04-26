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
  NS_SCRIPTABLE NS_IMETHOD GetCellAt(PRInt32 rowIndex, PRInt32 columnIndex, nsIAccessible * *_retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GetCellIndexAt(PRInt32 rowIndex, PRInt32 columnIndex, PRInt32 *_retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GetColumnIndexAt(PRInt32 cellIndex, PRInt32 *_retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GetRowIndexAt(PRInt32 cellIndex, PRInt32 *_retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GetRowAndColumnIndicesAt(PRInt32 cellIndex, PRInt32 *rowIndex NS_OUTPARAM, PRInt32 *columnIndex NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GetColumnExtentAt(PRInt32 row, PRInt32 column, PRInt32 *_retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GetRowExtentAt(PRInt32 row, PRInt32 column, PRInt32 *_retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GetColumnDescription(PRInt32 columnIndex, nsAString & _retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GetRowDescription(PRInt32 rowIndex, nsAString & _retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD IsColumnSelected(PRInt32 columnIndex, bool *_retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD IsRowSelected(PRInt32 rowIndex, bool *_retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD IsCellSelected(PRInt32 rowIndex, PRInt32 columnIndex, bool *_retval NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedCellCount(PRUint32 *aSelectedCellCount); \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedColumnCount(PRUint32 *aSelectedColumnCount); \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedRowCount(PRUint32 *aSelectedRowCount); \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedCells(nsIArray * *aSelectedCells); \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedCellIndices(PRUint32 *cellsArraySize NS_OUTPARAM, PRInt32 **cellsArray NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedColumnIndices(PRUint32 *rowsArraySize NS_OUTPARAM, PRInt32 **rowsArray NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD GetSelectedRowIndices(PRUint32 *rowsArraySize NS_OUTPARAM, PRInt32 **rowsArray NS_OUTPARAM); \
  NS_SCRIPTABLE NS_IMETHOD SelectRow(PRInt32 rowIndex); \
  NS_SCRIPTABLE NS_IMETHOD SelectColumn(PRInt32 columnIndex); \
  NS_SCRIPTABLE NS_IMETHOD UnselectColumn(PRInt32 aColIdx) \
    { return xpcAccessibleTable::UnselectColumn(aColIdx); } \
  NS_IMETHOD UnselectRow(PRInt32 aRowIdx) \
    { return xpcAccessibleTable::UnselectRow(aRowIdx); } \
  NS_IMETHOD IsProbablyForLayout(bool* aResult) \
  { return xpcAccessibleTable::IsProbablyForLayout(aResult); } \

#endif // MOZILLA_A11Y_XPCOM_XPACCESSIBLETABLE_H_
