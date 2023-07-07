/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Filters.h"

#include "LocalAccessible-inl.h"
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
