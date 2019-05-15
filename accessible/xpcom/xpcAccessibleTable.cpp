/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleTable.h"

#include "Accessible.h"
#include "TableAccessible.h"
#include "xpcAccessibleDocument.h"

#include "nsIMutableArray.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla::a11y;

static const uint32_t XPC_TABLE_DEFAULT_SIZE = 40;

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED(xpcAccessibleTable, xpcAccessibleHyperText,
                            nsIAccessibleTable)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleTable

NS_IMETHODIMP
xpcAccessibleTable::GetCaption(nsIAccessible** aCaption) {
  NS_ENSURE_ARG_POINTER(aCaption);
  *aCaption = nullptr;
  if (!Intl()) return NS_ERROR_FAILURE;

  NS_IF_ADDREF(*aCaption = ToXPC(Intl()->Caption()));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetColumnCount(int32_t* aColumnCount) {
  NS_ENSURE_ARG_POINTER(aColumnCount);
  *aColumnCount = 0;

  if (!Intl()) return NS_ERROR_FAILURE;

  *aColumnCount = Intl()->ColCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetRowCount(int32_t* aRowCount) {
  NS_ENSURE_ARG_POINTER(aRowCount);
  *aRowCount = 0;

  if (!Intl()) return NS_ERROR_FAILURE;

  *aRowCount = Intl()->RowCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetCellAt(int32_t aRowIdx, int32_t aColIdx,
                              nsIAccessible** aCell) {
  NS_ENSURE_ARG_POINTER(aCell);
  *aCell = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= Intl()->RowCount() ||
      aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  NS_IF_ADDREF(*aCell = ToXPC(Intl()->CellAt(aRowIdx, aColIdx)));
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetCellIndexAt(int32_t aRowIdx, int32_t aColIdx,
                                   int32_t* aCellIdx) {
  NS_ENSURE_ARG_POINTER(aCellIdx);
  *aCellIdx = -1;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= Intl()->RowCount() ||
      aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  *aCellIdx = Intl()->CellIndexAt(aRowIdx, aColIdx);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetColumnExtentAt(int32_t aRowIdx, int32_t aColIdx,
                                      int32_t* aColumnExtent) {
  NS_ENSURE_ARG_POINTER(aColumnExtent);
  *aColumnExtent = -1;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= Intl()->RowCount() ||
      aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  *aColumnExtent = Intl()->ColExtentAt(aRowIdx, aColIdx);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetRowExtentAt(int32_t aRowIdx, int32_t aColIdx,
                                   int32_t* aRowExtent) {
  NS_ENSURE_ARG_POINTER(aRowExtent);
  *aRowExtent = -1;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= Intl()->RowCount() ||
      aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  *aRowExtent = Intl()->RowExtentAt(aRowIdx, aColIdx);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetColumnDescription(int32_t aColIdx,
                                         nsAString& aDescription) {
  if (!Intl()) return NS_ERROR_FAILURE;

  if (aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  nsAutoString description;
  Intl()->ColDescription(aColIdx, description);
  aDescription.Assign(description);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetRowDescription(int32_t aRowIdx,
                                      nsAString& aDescription) {
  if (!Intl()) return NS_ERROR_FAILURE;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  nsAutoString description;
  Intl()->RowDescription(aRowIdx, description);
  aDescription.Assign(description);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::IsColumnSelected(int32_t aColIdx, bool* aIsSelected) {
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  *aIsSelected = Intl()->IsColSelected(aColIdx);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::IsRowSelected(int32_t aRowIdx, bool* aIsSelected) {
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= Intl()->RowCount())
    return NS_ERROR_INVALID_ARG;

  *aIsSelected = Intl()->IsRowSelected(aRowIdx);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::IsCellSelected(int32_t aRowIdx, int32_t aColIdx,
                                   bool* aIsSelected) {
  NS_ENSURE_ARG_POINTER(aIsSelected);
  *aIsSelected = false;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= Intl()->RowCount() ||
      aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  *aIsSelected = Intl()->IsCellSelected(aRowIdx, aColIdx);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetSelectedCellCount(uint32_t* aSelectedCellCount) {
  NS_ENSURE_ARG_POINTER(aSelectedCellCount);
  *aSelectedCellCount = 0;

  if (!Intl()) return NS_ERROR_FAILURE;

  *aSelectedCellCount = Intl()->SelectedCellCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetSelectedColumnCount(uint32_t* aSelectedColumnCount) {
  NS_ENSURE_ARG_POINTER(aSelectedColumnCount);
  *aSelectedColumnCount = 0;

  if (!Intl()) return NS_ERROR_FAILURE;

  *aSelectedColumnCount = Intl()->SelectedColCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetSelectedRowCount(uint32_t* aSelectedRowCount) {
  NS_ENSURE_ARG_POINTER(aSelectedRowCount);
  *aSelectedRowCount = 0;

  if (!Intl()) return NS_ERROR_FAILURE;

  *aSelectedRowCount = Intl()->SelectedRowCount();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetSelectedCells(nsIArray** aSelectedCells) {
  NS_ENSURE_ARG_POINTER(aSelectedCells);
  *aSelectedCells = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> selCells =
      do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoTArray<Accessible*, XPC_TABLE_DEFAULT_SIZE> cellsArray;
  Intl()->SelectedCells(&cellsArray);

  uint32_t totalCount = cellsArray.Length();
  for (uint32_t idx = 0; idx < totalCount; idx++) {
    Accessible* cell = cellsArray.ElementAt(idx);
    selCells->AppendElement(static_cast<nsIAccessible*>(ToXPC(cell)));
  }

  NS_ADDREF(*aSelectedCells = selCells);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetSelectedCellIndices(nsTArray<uint32_t>& aCellsArray) {
  if (!Intl()) return NS_ERROR_FAILURE;

  Intl()->SelectedCellIndices(&aCellsArray);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetSelectedColumnIndices(nsTArray<uint32_t>& aColsArray) {
  if (!Intl()) return NS_ERROR_FAILURE;

  Intl()->SelectedColIndices(&aColsArray);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetSelectedRowIndices(nsTArray<uint32_t>& aRowsArray) {
  if (!Intl()) return NS_ERROR_FAILURE;

  Intl()->SelectedRowIndices(&aRowsArray);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetColumnIndexAt(int32_t aCellIdx, int32_t* aColIdx) {
  NS_ENSURE_ARG_POINTER(aColIdx);
  *aColIdx = -1;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (aCellIdx < 0 || static_cast<uint32_t>(aCellIdx) >=
                          Intl()->RowCount() * Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  *aColIdx = Intl()->ColIndexAt(aCellIdx);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetRowIndexAt(int32_t aCellIdx, int32_t* aRowIdx) {
  NS_ENSURE_ARG_POINTER(aRowIdx);
  *aRowIdx = -1;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (aCellIdx < 0 || static_cast<uint32_t>(aCellIdx) >=
                          Intl()->RowCount() * Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  *aRowIdx = Intl()->RowIndexAt(aCellIdx);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetRowAndColumnIndicesAt(int32_t aCellIdx, int32_t* aRowIdx,
                                             int32_t* aColIdx) {
  NS_ENSURE_ARG_POINTER(aRowIdx);
  *aRowIdx = -1;
  NS_ENSURE_ARG_POINTER(aColIdx);
  *aColIdx = -1;

  if (!Intl()) return NS_ERROR_FAILURE;

  if (aCellIdx < 0 || static_cast<uint32_t>(aCellIdx) >=
                          Intl()->RowCount() * Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  Intl()->RowAndColIndicesAt(aCellIdx, aRowIdx, aColIdx);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::GetSummary(nsAString& aSummary) {
  if (!Intl()) return NS_ERROR_FAILURE;

  nsAutoString summary;
  Intl()->Summary(summary);
  aSummary.Assign(summary);

  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::IsProbablyForLayout(bool* aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = false;
  if (!Intl()) return NS_ERROR_FAILURE;

  *aResult = Intl()->IsProbablyLayoutTable();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::SelectColumn(int32_t aColIdx) {
  if (!Intl()) return NS_ERROR_FAILURE;

  if (aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  Intl()->SelectCol(aColIdx);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::SelectRow(int32_t aRowIdx) {
  if (!Intl()) return NS_ERROR_FAILURE;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= Intl()->RowCount())
    return NS_ERROR_INVALID_ARG;

  Intl()->SelectRow(aRowIdx);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::UnselectColumn(int32_t aColIdx) {
  if (!Intl()) return NS_ERROR_FAILURE;

  if (aColIdx < 0 || static_cast<uint32_t>(aColIdx) >= Intl()->ColCount())
    return NS_ERROR_INVALID_ARG;

  Intl()->UnselectCol(aColIdx);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTable::UnselectRow(int32_t aRowIdx) {
  if (!Intl()) return NS_ERROR_FAILURE;

  if (aRowIdx < 0 || static_cast<uint32_t>(aRowIdx) >= Intl()->RowCount())
    return NS_ERROR_INVALID_ARG;

  Intl()->UnselectRow(aRowIdx);
  return NS_OK;
}
