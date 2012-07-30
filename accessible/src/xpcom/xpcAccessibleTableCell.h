/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_XPCOM_XPACESSIBLETABLECELL_H_
#define MOZILLA_A11Y_XPCOM_XPACESSIBLETABLECELL_H_

namespace mozilla {
namespace a11y {
class TableAccessible;
class TableCellAccessible;
}
}

class xpcAccessibleTableCell
{
public:
  xpcAccessibleTableCell(mozilla::a11y::TableCellAccessible* aTableCell) :
    mTableCell(aTableCell) { }

protected:
  mozilla::a11y::TableCellAccessible* mTableCell;
};

#define NS_DECL_OR_FORWARD_NSIACCESSIBLETABLECELL_WITH_XPCACCESSIBLETABLECELL \
  NS_IMETHOD GetTable(nsIAccessibleTable * *aTable); \
  NS_IMETHOD GetColumnIndex(PRInt32 *aColumnIndex); \
  NS_IMETHOD GetRowIndex(PRInt32 *aRowIndex); \
  NS_IMETHOD GetColumnExtent(PRInt32 *aColumnExtent); \
  NS_IMETHOD GetRowExtent(PRInt32 *aRowExtent); \
  NS_IMETHOD GetColumnHeaderCells(nsIArray * *aColumnHeaderCells); \
  NS_IMETHOD GetRowHeaderCells(nsIArray * *aRowHeaderCells); \
  NS_IMETHOD IsSelected(bool *_retval ); 

#endif // MOZILLA_A11Y_XPCOM_XPACESSIBLETABLECELL_H_
