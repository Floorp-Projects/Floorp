/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ARIAGridAccessibleWrap.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// ARIAGridAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(ARIAGridAccessibleWrap,
                             ARIAGridAccessible)

IMPL_IUNKNOWN_INHERITED1(ARIAGridAccessibleWrap,
                         AccessibleWrap,
                         ia2AccessibleTable)

void
ARIAGridAccessibleWrap::Shutdown()
{
  ia2AccessibleTable::mTable = nullptr;
  ARIAGridAccessible::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// ARIAGridCellAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(ARIAGridCellAccessibleWrap,
                             ARIAGridCellAccessible)

IMPL_IUNKNOWN_INHERITED1(ARIAGridCellAccessibleWrap,
                         HyperTextAccessibleWrap,
                         ia2AccessibleTableCell)

void
ARIAGridCellAccessibleWrap::Shutdown()
{
  ia2AccessibleTableCell::mTableCell = nullptr;
  ARIAGridCellAccessible::Shutdown();
}
