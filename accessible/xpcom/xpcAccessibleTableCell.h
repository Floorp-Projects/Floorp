/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_xpcom_xpcAccessibletableCell_h_
#define mozilla_a11y_xpcom_xpcAccessibletableCell_h_

#include "nsIAccessibleTable.h"

#include "xpcAccessibleHyperText.h"

namespace mozilla {
namespace a11y {

/**
 * XPCOM wrapper around TableAccessibleCell class.
 */
class xpcAccessibleTableCell : public xpcAccessibleHyperText,
                               public nsIAccessibleTableCell
{
public:
  xpcAccessibleTableCell(Accessible* aIntl) : xpcAccessibleHyperText(aIntl) { }

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTableCell
  NS_IMETHOD GetTable(nsIAccessibleTable** aTable) MOZ_FINAL;
  NS_IMETHOD GetColumnIndex(int32_t* aColIdx) MOZ_FINAL;
  NS_IMETHOD GetRowIndex(int32_t* aRowIdx) MOZ_FINAL;
  NS_IMETHOD GetColumnExtent(int32_t* aExtent) MOZ_FINAL;
  NS_IMETHOD GetRowExtent(int32_t* aExtent) MOZ_FINAL;
  NS_IMETHOD GetColumnHeaderCells(nsIArray** aHeaderCells) MOZ_FINAL;
  NS_IMETHOD GetRowHeaderCells(nsIArray** aHeaderCells) MOZ_FINAL;
  NS_IMETHOD IsSelected(bool* aSelected) MOZ_FINAL;

protected:
  virtual ~xpcAccessibleTableCell() {}

private:
  TableCellAccessible* Intl() { return mIntl->AsTableCell(); }

  xpcAccessibleTableCell(const xpcAccessibleTableCell&) MOZ_DELETE;
  xpcAccessibleTableCell& operator =(const xpcAccessibleTableCell&) MOZ_DELETE;
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_xpcom_xpcAccessibletableCell_h_
