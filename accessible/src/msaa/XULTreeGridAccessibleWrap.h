/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULTreeGridAccessibleWrap_h__
#define mozilla_a11y_XULTreeGridAccessibleWrap_h__

#include "XULTreeGridAccessible.h"

#include "CAccessibleTable.h"
#include "CAccessibleTableCell.h"

namespace mozilla {
namespace a11y {

/**
 * IA2 wrapper class for XULTreeGridAccessible class implementing
 * IAccessibleTable and IAccessibleTable2 interfaces.
 */
class XULTreeGridAccessibleWrap : public XULTreeGridAccessible,
                                  public CAccessibleTable
{
public:
  XULTreeGridAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};

/**
 * IA2 wrapper class for XULTreeGridCellAccessible class, implements
 * IAccessibleTableCell interface.
 */
class XULTreeGridCellAccessibleWrap : public XULTreeGridCellAccessible,
                                      public CAccessibleTableCell
{
public:
  XULTreeGridCellAccessibleWrap(nsIContent* aContent,
                                DocAccessible* aDoc,
                                XULTreeGridRowAccessible* aRowAcc,
                                nsITreeBoxObject* aTree,
                                nsITreeView* aTreeView,
                                PRInt32 aRow, nsITreeColumn* aColumn);

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};

} // namespace a11y
} // namespace mozilla

#endif
