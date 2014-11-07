/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULListboxAccessibleWrap.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULListboxAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(XULListboxAccessibleWrap,
                             XULListboxAccessible)

IMPL_IUNKNOWN_QUERY_HEAD(XULListboxAccessibleWrap)
IMPL_IUNKNOWN_QUERY_CLASS_COND(ia2AccessibleTable,
                               !IsDefunct() && IsMulticolumn());
IMPL_IUNKNOWN_QUERY_CLASS(AccessibleWrap)
IMPL_IUNKNOWN_QUERY_TAIL

void
XULListboxAccessibleWrap::Shutdown()
{
  ia2AccessibleTable::mTable = nullptr;
  XULListboxAccessible::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// XULListCellAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(XULListCellAccessibleWrap,
                             XULListCellAccessible)

IMPL_IUNKNOWN_INHERITED1(XULListCellAccessibleWrap,
                         HyperTextAccessibleWrap,
                         ia2AccessibleTableCell)

void
XULListCellAccessibleWrap::Shutdown()
{
  ia2AccessibleTableCell::mTableCell = nullptr;
  XULListCellAccessible::Shutdown();
}
