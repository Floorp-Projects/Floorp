/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_XULTreeGridAccessibleWrap_h__
#define mozilla_a11y_XULTreeGridAccessibleWrap_h__

#include "XULTreeGridAccessible.h"

#include "ia2AccessibleTable.h"
#include "ia2AccessibleTableCell.h"

namespace mozilla {
namespace a11y {

/**
 * IA2 wrapper class for XULTreeGridAccessible class implementing
 * IAccessibleTable and IAccessibleTable2 interfaces.
 */
class XULTreeGridAccessibleWrap : public XULTreeGridAccessible,
                                  public ia2AccessibleTable
{
  ~XULTreeGridAccessibleWrap() {}

public:
  XULTreeGridAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc,
                            nsTreeBodyFrame* aTree) :
    XULTreeGridAccessible(aContent, aDoc, aTree), ia2AccessibleTable(this) {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual void Shutdown() override;
};

/**
 * IA2 wrapper class for XULTreeGridCellAccessible class, implements
 * IAccessibleTableCell interface.
 */
class XULTreeGridCellAccessibleWrap : public XULTreeGridCellAccessible,
                                      public ia2AccessibleTableCell
{
  ~XULTreeGridCellAccessibleWrap() {}

public:
  XULTreeGridCellAccessibleWrap(nsIContent* aContent,
                                DocAccessible* aDoc,
                                XULTreeGridRowAccessible* aRowAcc,
                                nsITreeBoxObject* aTree,
                                nsITreeView* aTreeView,
                                int32_t aRow, nsTreeColumn* aColumn) :
    XULTreeGridCellAccessible(aContent, aDoc, aRowAcc, aTree, aTreeView, aRow,
                              aColumn), ia2AccessibleTableCell(this) {}

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  virtual void Shutdown() override;
};

} // namespace a11y
} // namespace mozilla

#endif
