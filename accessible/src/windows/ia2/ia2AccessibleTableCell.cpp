/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleTableCell.h"

#include "Accessible2.h"
#include "AccessibleTable2_i.c"
#include "AccessibleTableCell_i.c"

#include "nsIAccessible.h"
#include "nsIAccessibleTable.h"
#include "nsIWinAccessNode.h"
#include "nsAccessNodeWrap.h"
#include "nsWinUtils.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#define TABLECELL_INTERFACE_UNSUPPORTED_MSG \
"Subclass of ia2AccessibleTableCell doesn't implement nsIAccessibleTableCell"\

// IUnknown

STDMETHODIMP
ia2AccessibleTableCell::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleTableCell == iid) {
    *ppv = static_cast<IAccessibleTableCell*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

////////////////////////////////////////////////////////////////////////////////
// IAccessibleTableCell

STDMETHODIMP
ia2AccessibleTableCell::get_table(IUnknown** aTable)
{
__try {
  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  nsCOMPtr<nsIAccessibleTable> geckoTable;
  nsresult rv = tableCell->GetTable(getter_AddRefs(geckoTable));
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  nsCOMPtr<nsIWinAccessNode> winAccessNode = do_QueryInterface(geckoTable);
  if (!winAccessNode)
    return E_FAIL;

  void *instancePtr = NULL;
  rv = winAccessNode->QueryNativeInterface(IID_IUnknown, &instancePtr);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aTable = static_cast<IUnknown*>(instancePtr);
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
ia2AccessibleTableCell::get_columnExtent(long* aNColumnsSpanned)
{
__try {
  *aNColumnsSpanned = 0;

  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  int32_t columnsSpanned = 0;
  nsresult rv = tableCell->GetColumnExtent(&columnsSpanned);
  if (NS_SUCCEEDED(rv)) {
    *aNColumnsSpanned = columnsSpanned;
    return S_OK;
  }

  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
ia2AccessibleTableCell::get_columnHeaderCells(IUnknown*** aCellAccessibles,
                                              long* aNColumnHeaderCells)
{
__try {
  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  nsCOMPtr<nsIArray> headerCells;
  nsresult rv = tableCell->GetColumnHeaderCells(getter_AddRefs(headerCells));  
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  return nsWinUtils::ConvertToIA2Array(headerCells, aCellAccessibles,
                                       aNColumnHeaderCells);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
ia2AccessibleTableCell::get_columnIndex(long* aColumnIndex)
{
__try {
  *aColumnIndex = -1;

  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  int32_t colIdx = -1;
  nsresult rv = tableCell->GetColumnIndex(&colIdx);
  if (NS_SUCCEEDED(rv)) {
    *aColumnIndex = colIdx;
    return S_OK;
  }

  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
ia2AccessibleTableCell::get_rowExtent(long* aNRowsSpanned)
{
__try {
  *aNRowsSpanned = 0;

  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  int32_t rowsSpanned = 0;
  nsresult rv = tableCell->GetRowExtent(&rowsSpanned);
  if (NS_SUCCEEDED(rv)) {
    *aNRowsSpanned = rowsSpanned;
    return S_OK;
  }

  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
ia2AccessibleTableCell::get_rowHeaderCells(IUnknown*** aCellAccessibles,
                                           long* aNRowHeaderCells)
{
__try {
  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  nsCOMPtr<nsIArray> headerCells;
  nsresult rv = tableCell->GetRowHeaderCells(getter_AddRefs(headerCells));  
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  return nsWinUtils::ConvertToIA2Array(headerCells, aCellAccessibles,
                                       aNRowHeaderCells);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
ia2AccessibleTableCell::get_rowIndex(long* aRowIndex)
{
__try {
  *aRowIndex = -1;

  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  int32_t rowIdx = -1;
  nsresult rv = tableCell->GetRowIndex(&rowIdx);
  if (NS_SUCCEEDED(rv)) {
    *aRowIndex = rowIdx;
    return S_OK;
  }

  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
ia2AccessibleTableCell::get_rowColumnExtents(long* aRow, long* aColumn,
                                             long* aRowExtents,
                                             long* aColumnExtents,
                                             boolean* aIsSelected)
{
__try {
  *aRow = 0;
  *aRow = 0;
  *aRow = 0;
  *aColumnExtents = 0;
  *aIsSelected = false;

  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  int32_t rowIdx = -1;
  nsresult rv = tableCell->GetRowIndex(&rowIdx);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  int32_t columnIdx = -1;
  rv = tableCell->GetColumnIndex(&columnIdx);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  int32_t spannedRows = 0;
  rv = tableCell->GetRowExtent(&spannedRows);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  int32_t spannedColumns = 0;
  rv = tableCell->GetColumnExtent(&spannedColumns);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  bool isSel = false;
  rv = tableCell->IsSelected(&isSel);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aRow = rowIdx;
  *aColumn = columnIdx;
  *aRowExtents = spannedRows;
  *aColumnExtents = spannedColumns;
  *aIsSelected = isSel;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_FAIL;
}

STDMETHODIMP
ia2AccessibleTableCell::get_isSelected(boolean* aIsSelected)
{
__try {
  *aIsSelected = false;

  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  bool isSel = false;
  nsresult rv = tableCell->IsSelected(&isSel);
  if (NS_SUCCEEDED(rv)) {
    *aIsSelected = isSel;
    return S_OK;
  }

  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}
