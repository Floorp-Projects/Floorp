/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible.h"
#include "AccGroupInfo.h"
#include "ARIAMap.h"
#include "States.h"
#include "mozilla/a11y/HyperTextAccessibleBase.h"

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

uint32_t Accessible::StartOffset() {
  MOZ_ASSERT(IsLink(), "StartOffset is called not on hyper link!");
  Accessible* parent = Parent();
  HyperTextAccessibleBase* hyperText =
      parent ? parent->AsHyperTextBase() : nullptr;
  return hyperText ? hyperText->GetChildOffset(this) : 0;
}

GroupPos Accessible::GroupPosition() {
  GroupPos groupPos;

  // Get group position from ARIA attributes.
  ARIAGroupPosition(&groupPos.level, &groupPos.setSize, &groupPos.posInSet);

  // If ARIA is missed and the accessible is visible then calculate group
  // position from hierarchy.
  if (State() & states::INVISIBLE) return groupPos;

  // Calculate group level if ARIA is missed.
  if (groupPos.level == 0) {
    groupPos.level = GetLevel(false);
  }

  // Calculate position in group and group size if ARIA is missed.
  if (groupPos.posInSet == 0 || groupPos.setSize == 0) {
    int32_t posInSet = 0, setSize = 0;
    GetPositionAndSetSize(&posInSet, &setSize);
    if (posInSet != 0 && setSize != 0) {
      if (groupPos.posInSet == 0) groupPos.posInSet = posInSet;

      if (groupPos.setSize == 0) groupPos.setSize = setSize;
    }
  }

  return groupPos;
}

int32_t Accessible::GetLevel(bool aFast) const {
  int32_t level = 0;
  if (!Parent()) return level;

  roles::Role role = Role();
  if (role == roles::OUTLINEITEM) {
    // Always expose 'level' attribute for 'outlineitem' accessible. The number
    // of nested 'grouping' accessibles containing 'outlineitem' accessible is
    // its level.
    level = 1;

    if (!aFast) {
      const Accessible* parent = this;
      while ((parent = parent->Parent())) {
        roles::Role parentRole = parent->Role();

        if (parentRole == roles::OUTLINE) break;
        if (parentRole == roles::GROUPING) ++level;
      }
    }
  } else if (role == roles::LISTITEM && !aFast) {
    // Expose 'level' attribute on nested lists. We support two hierarchies:
    // a) list -> listitem -> list -> listitem (nested list is a last child
    //   of listitem of the parent list);
    // b) list -> listitem -> group -> listitem (nested listitems are contained
    //   by group that is a last child of the parent listitem).

    // Calculate 'level' attribute based on number of parent listitems.
    level = 0;
    const Accessible* parent = this;
    while ((parent = parent->Parent())) {
      roles::Role parentRole = parent->Role();

      if (parentRole == roles::LISTITEM) {
        ++level;
      } else if (parentRole != roles::LIST && parentRole != roles::GROUPING) {
        break;
      }
    }

    if (level == 0) {
      // If this listitem is on top of nested lists then expose 'level'
      // attribute.
      parent = Parent();
      uint32_t siblingCount = parent->ChildCount();
      for (uint32_t siblingIdx = 0; siblingIdx < siblingCount; siblingIdx++) {
        Accessible* sibling = parent->ChildAt(siblingIdx);

        Accessible* siblingChild = sibling->LastChild();
        if (siblingChild) {
          roles::Role lastChildRole = siblingChild->Role();
          if (lastChildRole == roles::LIST ||
              lastChildRole == roles::GROUPING) {
            return 1;
          }
        }
      }
    } else {
      ++level;  // level is 1-index based
    }
  } else if (role == roles::OPTION || role == roles::COMBOBOX_OPTION) {
    if (const Accessible* parent = Parent()) {
      if (parent->IsHTMLOptGroup()) {
        return 2;
      }

      if (parent->IsListControl() && !parent->ARIARoleMap()) {
        // This is for HTML selects only.
        if (aFast) {
          return 1;
        }

        for (Accessible* child = parent->FirstChild(); child;
             child = child->NextSibling()) {
          if (child->IsHTMLOptGroup()) {
            return 1;
          }
        }
      }
    }
  } else if (role == roles::HEADING) {
    nsAtom* tagName = TagName();
    if (tagName == nsGkAtoms::h1) {
      return 1;
    }
    if (tagName == nsGkAtoms::h2) {
      return 2;
    }
    if (tagName == nsGkAtoms::h3) {
      return 3;
    }
    if (tagName == nsGkAtoms::h4) {
      return 4;
    }
    if (tagName == nsGkAtoms::h5) {
      return 5;
    }
    if (tagName == nsGkAtoms::h6) {
      return 6;
    }

    const nsRoleMapEntry* ariaRole = this->ARIARoleMap();
    if (ariaRole && ariaRole->Is(nsGkAtoms::heading)) {
      // An aria heading with no aria level has a default level of 2.
      return 2;
    }
  } else if (role == roles::COMMENT) {
    // For comments, count the ancestor elements with the same role to get the
    // level.
    level = 1;

    if (!aFast) {
      const Accessible* parent = this;
      while ((parent = parent->Parent())) {
        roles::Role parentRole = parent->Role();
        if (parentRole == roles::COMMENT) {
          ++level;
        }
      }
    }
  } else if (role == roles::ROW) {
    // It is a row inside flatten treegrid. Group level is always 1 until it
    // is overriden by aria-level attribute.
    const Accessible* parent = Parent();
    if (parent->Role() == roles::TREE_TABLE) {
      return 1;
    }
  }

  return level;
}

void Accessible::GetPositionAndSetSize(int32_t* aPosInSet, int32_t* aSetSize) {
  auto groupInfo = GetOrCreateGroupInfo();
  if (groupInfo) {
    *aPosInSet = groupInfo->PosInSet();
    *aSetSize = groupInfo->SetSize();
  }
}
