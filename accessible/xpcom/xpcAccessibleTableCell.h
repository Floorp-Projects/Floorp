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
                               public nsIAccessibleTableCell {
 public:
  explicit xpcAccessibleTableCell(Accessible* aIntl)
      : xpcAccessibleHyperText(aIntl) {}

  xpcAccessibleTableCell(RemoteAccessible* aProxy, uint32_t aInterfaces)
      : xpcAccessibleHyperText(aProxy, aInterfaces) {}

  NS_DECL_ISUPPORTS_INHERITED

  // nsIAccessibleTableCell
  NS_IMETHOD GetTable(nsIAccessibleTable** aTable) final;
  NS_IMETHOD GetColumnIndex(int32_t* aColIdx) final;
  NS_IMETHOD GetRowIndex(int32_t* aRowIdx) final;
  NS_IMETHOD GetColumnExtent(int32_t* aExtent) final;
  NS_IMETHOD GetRowExtent(int32_t* aExtent) final;
  NS_IMETHOD GetColumnHeaderCells(nsIArray** aHeaderCells) final;
  NS_IMETHOD GetRowHeaderCells(nsIArray** aHeaderCells) final;
  NS_IMETHOD IsSelected(bool* aSelected) final;

 protected:
  virtual ~xpcAccessibleTableCell() {}

 private:
  TableCellAccessible* Intl() {
    if (LocalAccessible* acc = mIntl->AsLocal()) {
      return acc->AsTableCell();
    }

    return nullptr;
  }

  xpcAccessibleTableCell(const xpcAccessibleTableCell&) = delete;
  xpcAccessibleTableCell& operator=(const xpcAccessibleTableCell&) = delete;
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_xpcom_xpcAccessibletableCell_h_
