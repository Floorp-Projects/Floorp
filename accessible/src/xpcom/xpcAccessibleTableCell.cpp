/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "xpcAccessibleTableCell.h"

#include "Accessible.h"
#include "TableAccessible.h"
#include "TableCellAccessible.h"

#include "nsIAccessibleTable.h"

#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"

using namespace mozilla;
using namespace mozilla::a11y;

nsresult
xpcAccessibleTableCell::GetTable(nsIAccessibleTable** aTable)
{
  NS_ENSURE_ARG_POINTER(aTable);
  *aTable = nullptr;

  if (!mTableCell)
    return NS_ERROR_FAILURE;

  TableAccessible* table = mTableCell->Table();
  if (!table)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAccessibleTable> xpcTable =
    do_QueryInterface(static_cast<nsIAccessible*>(table->AsAccessible()));
  xpcTable.forget(aTable);

  return NS_OK;
}

nsresult
xpcAccessibleTableCell::GetColumnIndex(int32_t* aColIdx)
{
  NS_ENSURE_ARG_POINTER(aColIdx);
  *aColIdx = -1;

  if (!mTableCell)
    return NS_ERROR_FAILURE;

  *aColIdx = mTableCell->ColIdx();
  return NS_OK;
}

nsresult
xpcAccessibleTableCell::GetRowIndex(int32_t* aRowIdx)
{
  NS_ENSURE_ARG_POINTER(aRowIdx);
  *aRowIdx = -1;

  if (!mTableCell)
    return NS_ERROR_FAILURE;

  *aRowIdx = mTableCell->RowIdx();

  return NS_OK;
}

nsresult
xpcAccessibleTableCell::GetColumnExtent(int32_t* aExtent)
{
  NS_ENSURE_ARG_POINTER(aExtent);
  *aExtent = -1;

  if (!mTableCell)
    return NS_ERROR_FAILURE;

  *aExtent = mTableCell->ColExtent();

  return NS_OK;
}

nsresult
xpcAccessibleTableCell::GetRowExtent(int32_t* aExtent)
{
  NS_ENSURE_ARG_POINTER(aExtent);
  *aExtent = -1;

  if (!mTableCell)
    return NS_ERROR_FAILURE;

  *aExtent = mTableCell->RowExtent();

  return NS_OK;
}

nsresult
xpcAccessibleTableCell::GetColumnHeaderCells(nsIArray** aHeaderCells)
{
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nullptr;

  if (!mTableCell)
    return NS_ERROR_FAILURE;

  nsAutoTArray<Accessible*, 10> headerCells;
  mTableCell->ColHeaderCells(&headerCells);

  nsCOMPtr<nsIMutableArray> cells = do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(cells, NS_ERROR_FAILURE);

  for (uint32_t idx = 0; idx < headerCells.Length(); idx++) {
    cells->
      AppendElement(static_cast<nsIAccessible*>(headerCells.ElementAt(idx)),
                    false);
  }

  NS_ADDREF(*aHeaderCells = cells);
  return NS_OK;
}

nsresult
xpcAccessibleTableCell::GetRowHeaderCells(nsIArray** aHeaderCells)
{
  NS_ENSURE_ARG_POINTER(aHeaderCells);
  *aHeaderCells = nullptr;

  if (!mTableCell)
    return NS_ERROR_FAILURE;

  nsAutoTArray<Accessible*, 10> headerCells;
  mTableCell->RowHeaderCells(&headerCells);

  nsCOMPtr<nsIMutableArray> cells = do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(cells, NS_ERROR_FAILURE);

  for (uint32_t idx = 0; idx < headerCells.Length(); idx++) {
    cells->
      AppendElement(static_cast<nsIAccessible*>(headerCells.ElementAt(idx)),
                    false);
  }

  NS_ADDREF(*aHeaderCells = cells);
  return NS_OK;
}

nsresult
xpcAccessibleTableCell::IsSelected(bool* aSelected)
{
  NS_ENSURE_ARG_POINTER(aSelected);
  *aSelected = false;

  if (!mTableCell)
    return NS_ERROR_FAILURE;

  *aSelected = mTableCell->Selected();

  return NS_OK;
}
