/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcom_xpcAccessibletableCell_h_
#define mozilla_a11y_xpcom_xpcAccessibletableCell_h_

#include "nscore.h"

class nsIAccessibleTable;
class nsIArray;

namespace mozilla {
namespace a11y {

class TableAccessible;
class TableCellAccessible;

/**
 * This class provides an implementation of the nsIAccessibleTableCell
 * interface's methods.
 */
class xpcAccessibleTableCell
{
public:
  explicit xpcAccessibleTableCell(mozilla::a11y::TableCellAccessible* aTableCell) :
    mTableCell(aTableCell) { }

  nsresult GetTable(nsIAccessibleTable** aTable);
  nsresult GetColumnIndex(int32_t* aColIdx);
  nsresult GetRowIndex(int32_t* aRowIdx);
  nsresult GetColumnExtent(int32_t* aExtent);
  nsresult GetRowExtent(int32_t* aExtent);
  nsresult GetColumnHeaderCells(nsIArray** aHeaderCells);
  nsresult GetRowHeaderCells(nsIArray** aHeaderCells);
  nsresult IsSelected(bool* aSelected);

protected:
  mozilla::a11y::TableCellAccessible* mTableCell;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_xpcom_xpcAccessibletableCell_h_
