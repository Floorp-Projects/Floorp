/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsXULTreeGridAccessibleWrap_h__
#define __nsXULTreeGridAccessibleWrap_h__

#include "nsXULTreeGridAccessible.h"

#include "CAccessibleTable.h"
#include "CAccessibleTableCell.h"

/**
 * IA2 wrapper class for nsXULTreeGridAccessible class implementing
 * IAccessibleTable and IAccessibleTable2 interfaces.
 */
class nsXULTreeGridAccessibleWrap : public nsXULTreeGridAccessible,
                                    public CAccessibleTable
{
public:
  nsXULTreeGridAccessibleWrap(nsIContent* aContent, nsDocAccessible* aDoc);

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};

/**
 * IA2 wrapper class for nsXULTreeGridCellAccessible class, implements
 * IAccessibleTableCell interface.
 */
class nsXULTreeGridCellAccessibleWrap : public nsXULTreeGridCellAccessible,
                                        public CAccessibleTableCell
{
public:
  nsXULTreeGridCellAccessibleWrap(nsIContent* aContent,
                                  nsDocAccessible* aDoc,
                                  nsXULTreeGridRowAccessible* aRowAcc,
                                  nsITreeBoxObject* aTree,
                                  nsITreeView* aTreeView,
                                  PRInt32 aRow, nsITreeColumn* aColumn);

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};

#endif
