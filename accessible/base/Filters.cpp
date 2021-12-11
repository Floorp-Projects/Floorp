/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Filters.h"

#include "LocalAccessible-inl.h"
#include "nsAccUtils.h"
#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;
using namespace mozilla::a11y::filters;

uint32_t filters::GetSelected(LocalAccessible* aAccessible) {
  if (aAccessible->State() & states::SELECTED) return eMatch | eSkipSubtree;

  return eSkip;
}

uint32_t filters::GetSelectable(LocalAccessible* aAccessible) {
  if (aAccessible->InteractiveState() & states::SELECTABLE) {
    return eMatch | eSkipSubtree;
  }

  return eSkip;
}

uint32_t filters::GetRow(LocalAccessible* aAccessible) {
  if (aAccessible->IsTableRow()) return eMatch | eSkipSubtree;

  // Look for rows inside rowgroup or wrapping text containers.
  a11y::role role = aAccessible->Role();
  const nsRoleMapEntry* roleMapEntry = aAccessible->ARIARoleMap();
  if (role == roles::GROUPING ||
      (aAccessible->IsGenericHyperText() && !roleMapEntry)) {
    return eSkip;
  }

  return eSkipSubtree;
}

uint32_t filters::GetCell(LocalAccessible* aAccessible) {
  return aAccessible->IsTableCell() ? eMatch : eSkipSubtree;
}
