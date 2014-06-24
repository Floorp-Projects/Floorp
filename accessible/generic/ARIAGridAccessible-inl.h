/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_ARIAGridAccessible_inl_h__
#define mozilla_a11y_ARIAGridAccessible_inl_h__

#include "ARIAGridAccessible.h"

#include "AccIterator.h"

namespace mozilla {
namespace a11y {

inline Accessible*
ARIAGridCellAccessible::TableFor(Accessible* aRow) const
{
  if (aRow) {
    Accessible* table = aRow->Parent();
    if (table) {
      roles::Role tableRole = table->Role();
      if (tableRole == roles::GROUPING) { // if there's a rowgroup.
        table = table->Parent();
        if (table)
          tableRole = table->Role();
      }

      return tableRole == roles::TABLE || tableRole == roles::TREE_TABLE ?
        table : nullptr;
    }
  }

  return nullptr;
}

inline int32_t
ARIAGridCellAccessible::RowIndexFor(Accessible* aRow) const
{
  Accessible* table = TableFor(aRow);
  if (table) {
    int32_t rowIdx = 0;
    Accessible* row = nullptr;
    AccIterator rowIter(table, filters::GetRow);
    while ((row = rowIter.Next()) && row != aRow)
      rowIdx++;

    if (row)
      return rowIdx;
  }

  return -1;
}

} // namespace a11y
} // namespace mozilla

#endif
