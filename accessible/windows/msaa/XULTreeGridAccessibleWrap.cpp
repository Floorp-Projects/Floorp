/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULTreeGridAccessibleWrap.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(XULTreeGridAccessibleWrap,
                             XULTreeGridAccessible)

IMPL_IUNKNOWN_INHERITED1(XULTreeGridAccessibleWrap,
                         AccessibleWrap,
                         ia2AccessibleTable)

void
XULTreeGridAccessibleWrap::Shutdown()
{
  ia2AccessibleTable::mTable = nullptr;
  XULTreeGridAccessible::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// XULTreeGridCellAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(XULTreeGridCellAccessibleWrap,
                             XULTreeGridCellAccessible)

IMPL_IUNKNOWN_INHERITED1(XULTreeGridCellAccessibleWrap,
                         AccessibleWrap,
                         ia2AccessibleTableCell)

void
XULTreeGridCellAccessibleWrap::Shutdown()
{
  ia2AccessibleTableCell::mTableCell = nullptr;
  XULTreeGridCellAccessible::Shutdown();
}
