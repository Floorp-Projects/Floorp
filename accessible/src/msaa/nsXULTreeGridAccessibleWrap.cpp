/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULTreeGridAccessibleWrap.h"

////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

nsXULTreeGridAccessibleWrap::
  nsXULTreeGridAccessibleWrap(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsXULTreeGridAccessible(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsXULTreeGridAccessibleWrap,
                             nsXULTreeGridAccessible)

IMPL_IUNKNOWN_INHERITED1(nsXULTreeGridAccessibleWrap,
                         nsAccessibleWrap,
                         CAccessibleTable)


////////////////////////////////////////////////////////////////////////////////
// nsXULTreeGridCellAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

nsXULTreeGridCellAccessibleWrap::
  nsXULTreeGridCellAccessibleWrap(nsIContent* aContent,
                                  nsDocAccessible* aDoc,
                                  nsXULTreeGridRowAccessible* aRowAcc,
                                  nsITreeBoxObject* aTree,
                                  nsITreeView* aTreeView,
                                  PRInt32 aRow, nsITreeColumn* aColumn) :
  nsXULTreeGridCellAccessible(aContent, aDoc, aRowAcc, aTree, aTreeView,
                              aRow, aColumn)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsXULTreeGridCellAccessibleWrap,
                             nsXULTreeGridCellAccessible)

IMPL_IUNKNOWN_INHERITED1(nsXULTreeGridCellAccessibleWrap,
                         nsAccessibleWrap,
                         CAccessibleTableCell)
