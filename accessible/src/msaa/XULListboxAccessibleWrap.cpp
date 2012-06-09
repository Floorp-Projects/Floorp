/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULListboxAccessibleWrap.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

XULListboxAccessibleWrap::
  XULListboxAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
  XULListboxAccessible(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(XULListboxAccessibleWrap,
                             XULListboxAccessible)

IMPL_IUNKNOWN_QUERY_HEAD(XULListboxAccessibleWrap)
IMPL_IUNKNOWN_QUERY_ENTRY_COND(CAccessibleTable, IsMulticolumn());
IMPL_IUNKNOWN_QUERY_ENTRY(AccessibleWrap)
IMPL_IUNKNOWN_QUERY_TAIL


////////////////////////////////////////////////////////////////////////////////
// XULListCellAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

XULListCellAccessibleWrap::
  XULListCellAccessibleWrap(nsIContent* aContent, DocAccessible* aDoc) :
  XULListCellAccessible(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(XULListCellAccessibleWrap,
                             XULListCellAccessible)

IMPL_IUNKNOWN_INHERITED1(XULListCellAccessibleWrap,
                         HyperTextAccessibleWrap,
                         CAccessibleTableCell)
