/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULListboxAccessibleWrap.h"

////////////////////////////////////////////////////////////////////////////////
// nsXULListboxAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

nsXULListboxAccessibleWrap::
  nsXULListboxAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
  nsXULListboxAccessible(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsXULListboxAccessibleWrap,
                             nsXULListboxAccessible)

IMPL_IUNKNOWN_QUERY_HEAD(nsXULListboxAccessibleWrap)
IMPL_IUNKNOWN_QUERY_ENTRY_COND(CAccessibleTable, IsMulticolumn());
IMPL_IUNKNOWN_QUERY_ENTRY(AccessibleWrap)
IMPL_IUNKNOWN_QUERY_TAIL


////////////////////////////////////////////////////////////////////////////////
// nsXULListCellAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

nsXULListCellAccessibleWrap::
  nsXULListCellAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
  nsXULListCellAccessible(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsXULListCellAccessibleWrap,
                             nsXULListCellAccessible)

IMPL_IUNKNOWN_INHERITED1(nsXULListCellAccessibleWrap,
                         HyperTextAccessibleWrap,
                         CAccessibleTableCell)
