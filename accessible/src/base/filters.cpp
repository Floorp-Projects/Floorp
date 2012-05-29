/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "filters.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

bool
filters::GetSelected(Accessible* aAccessible)
{
  return aAccessible->State() & states::SELECTED;
}

bool
filters::GetSelectable(Accessible* aAccessible)
{
  return aAccessible->State() & states::SELECTABLE;
}

bool
filters::GetRow(Accessible* aAccessible)
{
  return aAccessible->Role() == roles::ROW;
}

bool
filters::GetCell(Accessible* aAccessible)
{
  roles::Role role = aAccessible->Role();
  return role == roles::GRID_CELL || role == roles::ROWHEADER ||
      role == roles::COLUMNHEADER;
}

bool
filters::GetEmbeddedObject(Accessible* aAccessible)
{
  return nsAccUtils::IsEmbeddedObject(aAccessible);
}
