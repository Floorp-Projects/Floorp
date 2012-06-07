/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLTableAccessibleWrap.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLTableAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(HTMLTableAccessibleWrap,
                             HTMLTableAccessible)

IMPL_IUNKNOWN_INHERITED1(HTMLTableAccessibleWrap,
                         AccessibleWrap,
                         CAccessibleTable)


////////////////////////////////////////////////////////////////////////////////
// HTMLTableCellAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(HTMLTableCellAccessibleWrap,
                             HTMLTableCellAccessible)

IMPL_IUNKNOWN_INHERITED1(HTMLTableCellAccessibleWrap,
                         HyperTextAccessibleWrap,
                         CAccessibleTableCell)


////////////////////////////////////////////////////////////////////////////////
// HTMLTableCellAccessibleWrap
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(HTMLTableHeaderCellAccessibleWrap,
                             HTMLTableHeaderCellAccessible)

IMPL_IUNKNOWN_INHERITED1(HTMLTableHeaderCellAccessibleWrap,
                         HyperTextAccessibleWrap,
                         CAccessibleTableCell)
