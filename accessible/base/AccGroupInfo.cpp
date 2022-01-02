/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccGroupInfo.h"
#include "nsAccUtils.h"
#include "TableAccessible.h"

#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

AccGroupInfo::AccGroupInfo(const LocalAccessible* aItem, role aRole)
    : mPosInSet(0), mSetSize(0), mParent(nullptr), mItem(aItem), mRole(aRole) {
  MOZ_COUNT_CTOR(AccGroupInfo);
  Update();
}

void AccGroupInfo::Update() {
  mParent = nullptr;

  LocalAccessible* parent = mItem->LocalParent();
  if (!parent) return;

  int32_t indexInParent = mItem->IndexInParent();
  uint32_t siblingCount = parent->ChildCount();
  if (indexInParent == -1 ||
      indexInParent >= static_cast<int32_t>(siblingCount)) {
    NS_ERROR("Wrong index in parent! Tree invalidation problem.");
    return;
  }

  int32_t level = nsAccUtils::GetARIAOrDefaultLevel(mItem);

  // Compute position in set.
  mPosInSet = 1;
  for (int32_t idx = indexInParent - 1; idx >= 0; idx--) {
    LocalAccessible* sibling = parent->LocalChildAt(idx);
    roles::Role siblingRole = sibling->Role();

    // If the sibling is separator then the group is ended.
    if (siblingRole == roles::SEPARATOR) break;

    if (BaseRole(siblingRole) != mRole) {
      continue;
    }
    bool siblingHasGroupInfo =
        sibling->mBits.groupInfo && !sibling->HasDirtyGroupInfo();
    // Skip invisible siblings.
    // If the sibling has calculated group info, that means it's visible.
    if (!siblingHasGroupInfo && sibling->State() & states::INVISIBLE) {
      continue;
    }

    // Check if it's hierarchical flatten structure, i.e. if the sibling
    // level is lesser than this one then group is ended, if the sibling level
    // is greater than this one then the group is split by some child elements
    // (group will be continued).
    int32_t siblingLevel = nsAccUtils::GetARIAOrDefaultLevel(sibling);
    if (siblingLevel < level) {
      mParent = sibling;
      break;
    }

    // Skip subset.
    if (siblingLevel > level) continue;

    // If the previous item in the group has calculated group information then
    // build group information for this item based on found one.
    if (siblingHasGroupInfo) {
      mPosInSet += sibling->mBits.groupInfo->mPosInSet;
      mParent = sibling->mBits.groupInfo->mParent;
      mSetSize = sibling->mBits.groupInfo->mSetSize;
      return;
    }

    mPosInSet++;
  }

  // Compute set size.
  mSetSize = mPosInSet;

  for (uint32_t idx = indexInParent + 1; idx < siblingCount; idx++) {
    LocalAccessible* sibling = parent->LocalChildAt(idx);

    roles::Role siblingRole = sibling->Role();

    // If the sibling is separator then the group is ended.
    if (siblingRole == roles::SEPARATOR) break;

    if (BaseRole(siblingRole) != mRole) {
      continue;
    }
    bool siblingHasGroupInfo =
        sibling->mBits.groupInfo && !sibling->HasDirtyGroupInfo();
    // Skip invisible siblings.
    // If the sibling has calculated group info, that means it's visible.
    if (!siblingHasGroupInfo && sibling->State() & states::INVISIBLE) {
      continue;
    }

    // and check if it's hierarchical flatten structure.
    int32_t siblingLevel = nsAccUtils::GetARIAOrDefaultLevel(sibling);
    if (siblingLevel < level) break;

    // Skip subset.
    if (siblingLevel > level) continue;

    // If the next item in the group has calculated group information then
    // build group information for this item based on found one.
    if (siblingHasGroupInfo) {
      mParent = sibling->mBits.groupInfo->mParent;
      mSetSize = sibling->mBits.groupInfo->mSetSize;
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
    LocalAccessible* parentPrevSibling = parent->LocalPrevSibling();
    if (parentPrevSibling && parentPrevSibling->Role() == mRole) {
      mParent = parentPrevSibling;
      return;
    }
  }

  // Way #2 for ARIA list and tree: group is a child of an item. In other words
  // the parent of the item will be a group and containing item of the group is
  // a conceptual parent of the item.
  if (mRole == roles::LISTITEM || mRole == roles::OUTLINEITEM) {
    LocalAccessible* grandParent = parent->LocalParent();
    if (grandParent && grandParent->Role() == mRole) mParent = grandParent;
  }
}

LocalAccessible* AccGroupInfo::FirstItemOf(const LocalAccessible* aContainer) {
  // ARIA tree can be arranged by ARIA groups case #1 (previous sibling of a
  // group is a parent) or by aria-level.
  a11y::role containerRole = aContainer->Role();
  LocalAccessible* item = aContainer->LocalNextSibling();
  if (item) {
    if (containerRole == roles::OUTLINEITEM &&
        item->Role() == roles::GROUPING) {
      item = item->LocalFirstChild();
    }

    if (item) {
      AccGroupInfo* itemGroupInfo = item->GetGroupInfo();
      if (itemGroupInfo && itemGroupInfo->ConceptualParent() == aContainer) {
        return item;
      }
    }
  }

  // ARIA list and tree can be arranged by ARIA groups case #2 (group is
  // a child of an item).
  item = aContainer->LocalLastChild();
  if (!item) return nullptr;

  if (item->Role() == roles::GROUPING &&
      (containerRole == roles::LISTITEM ||
       containerRole == roles::OUTLINEITEM)) {
    item = item->LocalFirstChild();
    if (item) {
      AccGroupInfo* itemGroupInfo = item->GetGroupInfo();
      if (itemGroupInfo && itemGroupInfo->ConceptualParent() == aContainer) {
        return item;
      }
    }
  }

  // Otherwise, it can be a direct child if the container is a list or tree.
  item = aContainer->LocalFirstChild();
  if (ShouldReportRelations(item->Role(), containerRole)) return item;

  return nullptr;
}

uint32_t AccGroupInfo::TotalItemCount(LocalAccessible* aContainer,
                                      bool* aIsHierarchical) {
  uint32_t itemCount = 0;
  switch (aContainer->Role()) {
    case roles::TABLE:
      if (nsCoreUtils::GetUIntAttr(aContainer->GetContent(),
                                   nsGkAtoms::aria_rowcount,
                                   (int32_t*)&itemCount)) {
        break;
      }

      if (TableAccessible* tableAcc = aContainer->AsTable()) {
        return tableAcc->RowCount();
      }

      break;
    case roles::ROW:
      if (LocalAccessible* table = nsAccUtils::TableFor(aContainer)) {
        if (nsCoreUtils::GetUIntAttr(table->GetContent(),
                                     nsGkAtoms::aria_colcount,
                                     (int32_t*)&itemCount)) {
          break;
        }

        if (TableAccessible* tableAcc = table->AsTable()) {
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
      LocalAccessible* childItem = AccGroupInfo::FirstItemOf(aContainer);
      if (!childItem) {
        childItem = aContainer->LocalFirstChild();
        if (childItem && childItem->IsTextLeaf()) {
          // First child can be a text leaf, check its sibling for an item.
          childItem = childItem->LocalNextSibling();
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

LocalAccessible* AccGroupInfo::NextItemTo(LocalAccessible* aItem) {
  AccGroupInfo* groupInfo = aItem->GetGroupInfo();
  if (!groupInfo) return nullptr;

  // If the item in middle of the group then search next item in siblings.
  if (groupInfo->PosInSet() >= groupInfo->SetSize()) return nullptr;

  LocalAccessible* parent = aItem->LocalParent();
  uint32_t childCount = parent->ChildCount();
  for (uint32_t idx = aItem->IndexInParent() + 1; idx < childCount; idx++) {
    LocalAccessible* nextItem = parent->LocalChildAt(idx);
    AccGroupInfo* nextGroupInfo = nextItem->GetGroupInfo();
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
