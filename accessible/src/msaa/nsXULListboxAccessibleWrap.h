/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsXULListboxAccessibleWrap_h__
#define __nsXULListboxAccessibleWrap_h__

#include "nsXULListboxAccessible.h"

#include "CAccessibleTable.h"
#include "CAccessibleTableCell.h"

/**
 * IA2 wrapper class for nsXULListboxAccessible class implementing
 * IAccessibleTable and IAccessibleTable2 interfaces.
 */
class nsXULListboxAccessibleWrap : public nsXULListboxAccessible,
                                   public CAccessibleTable
{
public:
  nsXULListboxAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};

/**
 * IA2 wrapper class for nsXULListCellAccessible class, implements
 * IAccessibleTableCell interface.
 */
class nsXULListCellAccessibleWrap : public nsXULListCellAccessible,
                                    public CAccessibleTableCell
{
public:
  nsXULListCellAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);

  // IUnknown
  DECL_IUNKNOWN_INHERITED

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED
};

#endif
