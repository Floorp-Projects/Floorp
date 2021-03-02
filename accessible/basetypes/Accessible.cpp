/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible.h"
#include "ARIAMap.h"

using namespace mozilla;
using namespace mozilla::a11y;

Accessible::Accessible()
    : mType(static_cast<uint32_t>(0)),
      mGenericTypes(static_cast<uint32_t>(0)),
      mRoleMapEntryIndex(aria::NO_ROLE_MAP_ENTRY_INDEX) {}

Accessible::Accessible(AccType aType, AccGenericType aGenericTypes,
                       uint8_t aRoleMapEntryIndex)
    : mType(static_cast<uint32_t>(aType)),
      mGenericTypes(static_cast<uint32_t>(aGenericTypes)),
      mRoleMapEntryIndex(aRoleMapEntryIndex) {}

void Accessible::StaticAsserts() const {
  static_assert(eLastAccType <= (1 << kTypeBits) - 1,
                "Accessible::mType was oversized by eLastAccType!");
  static_assert(
      eLastAccGenericType <= (1 << kGenericTypesBits) - 1,
      "Accessible::mGenericType was oversized by eLastAccGenericType!");
}

const nsRoleMapEntry* Accessible::ARIARoleMap() const {
  return aria::GetRoleMapFromIndex(mRoleMapEntryIndex);
}

bool Accessible::HasARIARole() const {
  return mRoleMapEntryIndex != aria::NO_ROLE_MAP_ENTRY_INDEX;
}

bool Accessible::IsARIARole(nsAtom* aARIARole) const {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return roleMapEntry && roleMapEntry->Is(aARIARole);
}

bool Accessible::HasStrongARIARole() const {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return roleMapEntry && roleMapEntry->roleRule == kUseMapRole;
}

bool Accessible::HasGenericType(AccGenericType aType) const {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return (mGenericTypes & aType) ||
         (roleMapEntry && roleMapEntry->IsOfType(aType));
}

bool Accessible::IsTextRole() {
  if (!IsHyperText()) {
    return false;
  }

  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (roleMapEntry && (roleMapEntry->role == roles::GRAPHIC ||
                       roleMapEntry->role == roles::IMAGE_MAP ||
                       roleMapEntry->role == roles::SLIDER ||
                       roleMapEntry->role == roles::PROGRESSBAR ||
                       roleMapEntry->role == roles::SEPARATOR)) {
    return false;
  }

  return true;
}
