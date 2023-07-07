/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleTableCell.h"

#include "mozilla/a11y/TableAccessible.h"
#include "mozilla/a11y/TableCellAccessible.h"
#include "nsIAccessibleTable.h"

#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"
#include "xpcAccessibleDocument.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS_INHERITED(xpcAccessibleTableCell, xpcAccessibleHyperText,
                            nsIAccessibleTableCell)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibleTableCell

NS_IMETHODIMP
xpcAccessibleTableCell::GetTable(nsIAccessibleTable** aTable) {
  NS_ENSURE_ARG_POINTER(aTable);
  *aTable = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  TableAccessible* table = Intl()->Table();
  if (!table) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessibleTable> xpcTable = do_QueryInterface(
      static_cast<nsIAccessible*>(ToXPC(table->AsAccessible())));
  xpcTable.forget(aTable);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTableCell::GetColumnIndex(int32_t* aColIdx) {
  NS_ENSURE_ARG_POINTER(aColIdx);
  *aColIdx = -1;

  if (!Intl()) return NS_ERROR_FAILURE;

  *aColIdx = Intl()->ColIdx();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTableCell::GetRowIndex(int32_t* aRowIdx) {
  NS_ENSURE_ARG_POINTER(aRowIdx);
  *aRowIdx = -1;

  if (!Intl()) return NS_ERROR_FAILURE;

  *aRowIdx = Intl()->RowIdx();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTableCell::GetColumnExtent(int32_t* aExtent) {
  NS_ENSURE_ARG_POINTER(aExtent);
  *aExtent = -1;

  if (!Intl()) return NS_ERROR_FAILURE;

  *aExtent = Intl()->ColExtent();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTableCell::GetRowExtent(int32_t* aExtent) {
  NS_ENSURE_ARG_POINTER(aExtent);
  *aExtent = -1;

  if (!Intl()) return NS_ERROR_FAILURE;

  *aExtent = Intl()->RowExtent();
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTableCell::GetColumnHeaderCells(nsIArray** aHeaderCells) {
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  AutoTArray<Accessible*, 10> headerCells;
  Intl()->ColHeaderCells(&headerCells);

  nsCOMPtr<nsIMutableArray> cells = do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(cells, NS_ERROR_FAILURE);

  for (uint32_t idx = 0; idx < headerCells.Length(); idx++) {
    cells->AppendElement(static_cast<nsIAccessible*>(ToXPC(headerCells[idx])));
  }

  NS_ADDREF(*aHeaderCells = cells);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTableCell::GetRowHeaderCells(nsIArray** aHeaderCells) {
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nullptr;

  if (!Intl()) return NS_ERROR_FAILURE;

  AutoTArray<Accessible*, 10> headerCells;
  Intl()->RowHeaderCells(&headerCells);

  nsCOMPtr<nsIMutableArray> cells = do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(cells, NS_ERROR_FAILURE);

  for (uint32_t idx = 0; idx < headerCells.Length(); idx++) {
    cells->AppendElement(static_cast<nsIAccessible*>(ToXPC(headerCells[idx])));
  }

  NS_ADDREF(*aHeaderCells = cells);
  return NS_OK;
}

NS_IMETHODIMP
xpcAccessibleTableCell::IsSelected(bool* aSelected) {
  NS_ENSURE_ARG_POINTER(aSelected);
  *aSelected = false;

  if (!Intl()) return NS_ERROR_FAILURE;

  *aSelected = Intl()->Selected();
  return NS_OK;
}
