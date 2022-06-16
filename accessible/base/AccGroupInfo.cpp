/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccGroupInfo.h"
#include "mozilla/a11y/Accessible.h"
#include "mozilla/a11y/TableAccessibleBase.h"

#include "nsAccUtils.h"

#include "States.h"

using namespace mozilla::a11y;

AccGroupInfo::AccGroupInfo(const Accessible* aItem, role aRole)
    : mPosInSet(0), mSetSize(0), mParent(nullptr), mItem(aItem), mRole(aRole) {
  MOZ_COUNT_CTOR(AccGroupInfo);
  Update();
}

void AccGroupInfo::Update() {
  mParent = nullptr;

  Accessible* parent = mItem->Parent();
  if (!parent) return;

  int32_t indexInParent = mItem->IndexInParent();
  uint32_t siblingCount = parent->ChildCount();
  if (indexInParent == -1 ||
      indexInParent >= static_cast<int32_t>(siblingCount)) {
    NS_ERROR("Wrong index in parent! Tree invalidation problem.");
    return;
  }

  int32_t level = GetARIAOrDefaultLevel(mItem);

  // Compute position in set.
  mPosInSet = 1;
  for (int32_t idx = indexInParent - 1; idx >= 0; idx--) {
    Accessible* sibling = parent->ChildAt(idx);
    roles::Role siblingRole = sibling->Role();

    // If the sibling is separator then the group is ended.
    if (siblingRole == roles::SEPARATOR) break;

    if (BaseRole(siblingRole) != mRole) {
      continue;
    }

    AccGroupInfo* siblingGroupInfo = sibling->GetGroupInfo();
    // Skip invisible siblings.
    // If the sibling has calculated group info, that means it's visible.
    if (!siblingGroupInfo && sibling->State() & states::INVISIBLE) {
      continue;
    }

    // Check if it's hierarchical flatten structure, i.e. if the sibling
    // level is lesser than this one then group is ended, if the sibling level
    // is greater than this one then the group is split by some child elements
    // (group will be continued).
    int32_t siblingLevel = GetARIAOrDefaultLevel(sibling);
    if (siblingLevel < level) {
      mParent = sibling;
      break;
    }

    // Skip subset.
    if (siblingLevel > level) continue;

    // If the previous item in the group has calculated group information then
    // build group information for this item based on found one.
    if (siblingGroupInfo) {
      mPosInSet += siblingGroupInfo->mPosInSet;
      mParent = siblingGroupInfo->mParent;
      mSetSize = siblingGroupInfo->mSetSize;
      return;
    }

    mPosInSet++;
  }

  // Compute set size.
  mSetSize = mPosInSet;

  for (uint32_t idx = indexInParent + 1; idx < siblingCount; idx++) {
    Accessible* sibling = parent->ChildAt(idx);

    roles::Role siblingRole = sibling->Role();

    // If the sibling is separator then the group is ended.
    if (siblingRole == roles::SEPARATOR) break;

    if (BaseRole(siblingRole) != mRole) {
      continue;
    }
    AccGroupInfo* siblingGroupInfo = sibling->GetGroupInfo();
    // Skip invisible siblings.
    // If the sibling has calculated group info, that means it's visible.
    if (!siblingGroupInfo && sibling->State() & states::INVISIBLE) {
      continue;
    }

    // and check if it's hierarchical flatten structure.
    int32_t siblingLevel = GetARIAOrDefaultLevel(sibling);
    if (siblingLevel < level) break;

    // Skip subset.
    if (siblingLevel > level) continue;

    // If the next item in the group has calculated group information then
    // build group information for this item based on found one.
    if (siblingGroupInfo) {
      mParent = siblingGroupInfo->mParent;
      mSetSize = siblingGroupInfo->mSetSize;
      return;
    }

    mSetSize++;
  }

  if (mParent) return;

  roles::Role parentRole = parent->Role();
  if (ShouldReportRelations(mRole, parentRole)) mParent = parent;

  // ARIA tree and list can be arranged by using ARIA groups to organize levels.
  if (parentRole != roles::GROUPING) return;

  // Way #1 for ARIA tree (not ARIA treegrid): previous sibling of a group is a
  // parent. In other words the parent of the tree item will be a group and
  // the previous tree item of the group is a conceptual parent of the tree
  // item.
  if (mRole == roles::OUTLINEITEM) {
    Accessible* parentPrevSibling = parent->PrevSibling();
    if (parentPrevSibling && parentPrevSibling->Role() == mRole) {
      mParent = parentPrevSibling;
      return;
    }
  }

  // Way #2 for ARIA list and tree: group is a child of an item. In other words
  // the parent of the item will be a group and containing item of the group is
  // a conceptual parent of the item.
  if (mRole == roles::LISTITEM || mRole == roles::OUTLINEITEM) {
    Accessible* grandParent = parent->Parent();
    if (grandParent && grandParent->Role() == mRole) mParent = grandParent;
  }
}

AccGroupInfo* AccGroupInfo::CreateGroupInfo(const Accessible* aAccessible) {
  mozilla::a11y::role role = aAccessible->Role();
  if (role != mozilla::a11y::roles::ROW &&
      role != mozilla::a11y::roles::OUTLINEITEM &&
      role != mozilla::a11y::roles::OPTION &&
      role != mozilla::a11y::roles::LISTITEM &&
      role != mozilla::a11y::roles::MENUITEM &&
      role != mozilla::a11y::roles::COMBOBOX_OPTION &&
      role != mozilla::a11y::roles::RICH_OPTION &&
      role != mozilla::a11y::roles::CHECK_RICH_OPTION &&
      role != mozilla::a11y::roles::PARENT_MENUITEM &&
      role != mozilla::a11y::roles::CHECK_MENU_ITEM &&
      role != mozilla::a11y::roles::RADIO_MENU_ITEM &&
      role != mozilla::a11y::roles::RADIOBUTTON &&
      role != mozilla::a11y::roles::PAGETAB &&
      role != mozilla::a11y::roles::COMMENT) {
    return nullptr;
  }

  AccGroupInfo* info = new AccGroupInfo(aAccessible, BaseRole(role));
  return info;
}

Accessible* AccGroupInfo::FirstItemOf(const Accessible* aContainer) {
  // ARIA tree can be arranged by ARIA groups case #1 (previous sibling of a
  // group is a parent) or by aria-level.
  a11y::role containerRole = aContainer->Role();
  Accessible* item = aContainer->NextSibling();
  if (item) {
    if (containerRole == roles::OUTLINEITEM &&
        item->Role() == roles::GROUPING) {
      item = item->FirstChild();
    }

    if (item) {
      AccGroupInfo* itemGroupInfo = item->GetOrCreateGroupInfo();
      if (itemGroupInfo && itemGroupInfo->ConceptualParent() == aContainer) {
        return item;
      }
    }
  }

  // ARIA list and tree can be arranged by ARIA groups case #2 (group is
  // a child of an item).
  item = aContainer->LastChild();
  if (!item) return nullptr;

  if (item->Role() == roles::GROUPING &&
      (containerRole == roles::LISTITEM ||
       containerRole == roles::OUTLINEITEM)) {
    item = item->FirstChild();
    if (item) {
      AccGroupInfo* itemGroupInfo = item->GetOrCreateGroupInfo();
      if (itemGroupInfo && itemGroupInfo->ConceptualParent() == aContainer) {
        return item;
      }
    }
  }

  // Otherwise, it can be a direct child if the container is a list or tree.
  item = aContainer->FirstChild();
  if (ShouldReportRelations(item->Role(), containerRole)) return item;

  return nullptr;
}

uint32_t AccGroupInfo::TotalItemCount(Accessible* aContainer,
                                      bool* aIsHierarchical) {
  uint32_t itemCount = 0;
  switch (aContainer->Role()) {
    case roles::TABLE:
      if (auto val = aContainer->GetIntARIAAttr(nsGkAtoms::aria_rowcount)) {
        if (*val >= 0) {
          return *val;
        }
      }
      if (TableAccessibleBase* tableAcc = aContainer->AsTableBase()) {
        return tableAcc->RowCount();
      }
      break;
    case roles::ROW:
      if (Accessible* table = nsAccUtils::TableFor(aContainer)) {
        if (auto val = table->GetIntARIAAttr(nsGkAtoms::aria_colcount)) {
          if (*val >= 0) {
            return *val;
          }
        }
        if (TableAccessibleBase* tableAcc = table->AsTableBase()) {
          return tableAcc->ColCount();
        }
      }
      break;
    case roles::OUTLINE:
    case roles::LIST:
    case roles::MENUBAR:
    case roles::MENUPOPUP:
    case roles::COMBOBOX:
    case roles::GROUPING:
    case roles::TREE_TABLE:
    case roles::COMBOBOX_LIST:
    case roles::LISTBOX:
    case roles::DEFINITION_LIST:
    case roles::EDITCOMBOBOX:
    case roles::RADIO_GROUP:
    case roles::PAGETABLIST: {
      Accessible* childItem = AccGroupInfo::FirstItemOf(aContainer);
      if (!childItem) {
        childItem = aContainer->FirstChild();
        if (childItem && childItem->IsTextLeaf()) {
          // First child can be a text leaf, check its sibling for an item.
          childItem = childItem->NextSibling();
        }
      }

      if (childItem) {
        GroupPos groupPos = childItem->GroupPosition();
        itemCount = groupPos.setSize;
        if (groupPos.level && aIsHierarchical) {
          *aIsHierarchical = true;
        }
      }
      break;
    }
    default:
      break;
  }

  return itemCount;
}

Accessible* AccGroupInfo::NextItemTo(Accessible* aItem) {
  AccGroupInfo* groupInfo = aItem->GetOrCreateGroupInfo();
  if (!groupInfo) return nullptr;

  // If the item in middle of the group then search next item in siblings.
  if (groupInfo->PosInSet() >= groupInfo->SetSize()) return nullptr;

  Accessible* parent = aItem->Parent();
  uint32_t childCount = parent->ChildCount();
  for (uint32_t idx = aItem->IndexInParent() + 1; idx < childCount; idx++) {
    Accessible* nextItem = parent->ChildAt(idx);
    AccGroupInfo* nextGroupInfo = nextItem->GetOrCreateGroupInfo();
    if (nextGroupInfo &&
        nextGroupInfo->ConceptualParent() == groupInfo->ConceptualParent()) {
      return nextItem;
    }
  }

  MOZ_ASSERT_UNREACHABLE(
      "Item in the middle of the group but there's no next item!");
  return nullptr;
}

bool AccGroupInfo::ShouldReportRelations(role aRole, role aParentRole) {
  // We only want to report hierarchy-based node relations for items in tree or
  // list form.  ARIA level/owns relations are always reported.
  if (aParentRole == roles::OUTLINE && aRole == roles::OUTLINEITEM) return true;
  if (aParentRole == roles::TREE_TABLE && aRole == roles::ROW) return true;
  if (aParentRole == roles::LIST && aRole == roles::LISTITEM) return true;

  return false;
}

int32_t AccGroupInfo::GetARIAOrDefaultLevel(const Accessible* aAccessible) {
  int32_t level = 0;
  aAccessible->ARIAGroupPosition(&level, nullptr, nullptr);

  if (level != 0) return level;

  return aAccessible->GetLevel(true);
}
