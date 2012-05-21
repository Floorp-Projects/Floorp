/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CAccessibleTableCell.h"

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
"Subclass of CAccessibleTableCell doesn't implement nsIAccessibleTableCell"\

// IUnknown

STDMETHODIMP
CAccessibleTableCell::QueryInterface(REFIID iid, void** ppv)
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
CAccessibleTableCell::get_table(IUnknown **table)
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

  *table = static_cast<IUnknown*>(instancePtr);
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
CAccessibleTableCell::get_columnExtent(long *nColumnsSpanned)
{
__try {
  *nColumnsSpanned = 0;

  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  PRInt32 columnsSpanned = 0;
  nsresult rv = tableCell->GetColumnExtent(&columnsSpanned);
  if (NS_SUCCEEDED(rv)) {
    *nColumnsSpanned = columnsSpanned;
    return S_OK;
  }

  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
CAccessibleTableCell::get_columnHeaderCells(IUnknown ***cellAccessibles,
                                            long *nColumnHeaderCells)
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

  return nsWinUtils::ConvertToIA2Array(headerCells, cellAccessibles,
                                       nColumnHeaderCells);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
CAccessibleTableCell::get_columnIndex(long *columnIndex)
{
__try {
  *columnIndex = -1;

  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  PRInt32 colIdx = -1;
  nsresult rv = tableCell->GetColumnIndex(&colIdx);
  if (NS_SUCCEEDED(rv)) {
    *columnIndex = colIdx;
    return S_OK;
  }

  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
CAccessibleTableCell::get_rowExtent(long *nRowsSpanned)
{
__try {
  *nRowsSpanned = 0;

  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  PRInt32 rowsSpanned = 0;
  nsresult rv = tableCell->GetRowExtent(&rowsSpanned);
  if (NS_SUCCEEDED(rv)) {
    *nRowsSpanned = rowsSpanned;
    return S_OK;
  }

  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
CAccessibleTableCell::get_rowHeaderCells(IUnknown ***cellAccessibles,
                                         long *nRowHeaderCells)
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

  return nsWinUtils::ConvertToIA2Array(headerCells, cellAccessibles,
                                       nRowHeaderCells);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
CAccessibleTableCell::get_rowIndex(long *rowIndex)
{
__try {
  *rowIndex = -1;

  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  PRInt32 rowIdx = -1;
  nsresult rv = tableCell->GetRowIndex(&rowIdx);
  if (NS_SUCCEEDED(rv)) {
    *rowIndex = rowIdx;
    return S_OK;
  }

  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}

STDMETHODIMP
CAccessibleTableCell::get_rowColumnExtents(long *row, long *column,
                                           long *rowExtents,
                                           long *columnExtents,
                                           boolean *isSelected)
{
__try {
  *row = 0;
  *column = 0;
  *rowExtents = 0;
  *columnExtents = 0;
  *isSelected = false;

  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  PRInt32 rowIdx = -1;
  nsresult rv = tableCell->GetRowIndex(&rowIdx);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  PRInt32 columnIdx = -1;
  rv = tableCell->GetColumnIndex(&columnIdx);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  PRInt32 spannedRows = 0;
  rv = tableCell->GetRowExtent(&spannedRows);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  PRInt32 spannedColumns = 0;
  rv = tableCell->GetColumnExtent(&spannedColumns);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  bool isSel = false;
  rv = tableCell->IsSelected(&isSel);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *row = rowIdx;
  *column = columnIdx;
  *rowExtents = spannedRows;
  *columnExtents = spannedColumns;
  *isSelected = isSel;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}
  return E_FAIL;
}

STDMETHODIMP
CAccessibleTableCell::get_isSelected(boolean *isSelected)
{
__try {
  *isSelected = false;

  nsCOMPtr<nsIAccessibleTableCell> tableCell(do_QueryObject(this));
  NS_ASSERTION(tableCell, TABLECELL_INTERFACE_UNSUPPORTED_MSG);
  if (!tableCell)
    return E_FAIL;

  bool isSel = false;
  nsresult rv = tableCell->IsSelected(&isSel);
  if (NS_SUCCEEDED(rv)) {
    *isSelected = isSel;
    return S_OK;
  }

  return GetHRESULT(rv);

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(),
                                                  GetExceptionInformation())) {}

  return E_FAIL;
}
