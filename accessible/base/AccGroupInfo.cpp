/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccGroupInfo.h"
#include "nsAccUtils.h"

#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

AccGroupInfo::AccGroupInfo(const Accessible* aItem, role aRole) :
  mPosInSet(0), mSetSize(0), mParent(nullptr), mItem(aItem), mRole(aRole)
{
  MOZ_COUNT_CTOR(AccGroupInfo);
  Update();
}

void
AccGroupInfo::Update()
{
  Accessible* parent = mItem->Parent();
  if (!parent)
    return;

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
  for (int32_t idx = indexInParent - 1; idx >= 0 ; idx--) {
    Accessible* sibling = parent->GetChildAt(idx);
    roles::Role siblingRole = sibling->Role();

    // If the sibling is separator then the group is ended.
    if (siblingRole == roles::SEPARATOR)
      break;

    // If sibling is not visible and hasn't the same base role.
    if (BaseRole(siblingRole) != mRole || sibling->State() & states::INVISIBLE)
      continue;

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
    if (siblingLevel > level)
      continue;

    // If the previous item in the group has calculated group information then
    // build group information for this item based on found one.
    if (sibling->mBits.groupInfo) {
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
    Accessible* sibling = parent->GetChildAt(idx);

    roles::Role siblingRole = sibling->Role();

    // If the sibling is separator then the group is ended.
    if (siblingRole == roles::SEPARATOR)
      break;

    // If sibling is visible and has the same base role
    if (BaseRole(siblingRole) != mRole || sibling->State() & states::INVISIBLE)
      continue;

    // and check if it's hierarchical flatten structure.
    int32_t siblingLevel = nsAccUtils::GetARIAOrDefaultLevel(sibling);
    if (siblingLevel < level)
      break;

    // Skip subset.
    if (siblingLevel > level)
      continue;

    // If the next item in the group has calculated group information then
    // build group information for this item based on found one.
    if (sibling->mBits.groupInfo) {
      mParent = sibling->mBits.groupInfo->mParent;
      mSetSize = sibling->mBits.groupInfo->mSetSize;
      return;
    }

    mSetSize++;
  }

  if (mParent)
    return;

  roles::Role parentRole = parent->Role();
  if (ShouldReportRelations(mRole, parentRole))
    mParent = parent;

  // ARIA tree and list can be arranged by using ARIA groups to organize levels.
  if (parentRole != roles::GROUPING)
    return;

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
    if (grandParent && grandParent->Role() == mRole)
      mParent = grandParent;
  }
}

Accessible*
AccGroupInfo::FirstItemOf(const Accessible* aContainer)
{
  // ARIA tree can be arranged by ARIA groups case #1 (previous sibling of a
  // group is a parent) or by aria-level.
  a11y::role containerRole = aContainer->Role();
  Accessible* item = aContainer->NextSibling();
  if (item) {
    if (containerRole == roles::OUTLINEITEM && item->Role() == roles::GROUPING)
      item = item->FirstChild();

    if (item) {
      AccGroupInfo* itemGroupInfo = item->GetGroupInfo();
      if (itemGroupInfo && itemGroupInfo->ConceptualParent() == aContainer)
        return item;
    }
  }

  // ARIA list and tree can be arranged by ARIA groups case #2 (group is
  // a child of an item).
  item = aContainer->LastChild();
  if (!item)
    return nullptr;

  if (item->Role() == roles::GROUPING &&
      (containerRole == roles::LISTITEM || containerRole == roles::OUTLINEITEM)) {
    item = item->FirstChild();
    if (item) {
      AccGroupInfo* itemGroupInfo = item->GetGroupInfo();
      if (itemGroupInfo && itemGroupInfo->ConceptualParent() == aContainer)
        return item;
    }
  }

  // Otherwise, it can be a direct child if the container is a list or tree.
  item = aContainer->FirstChild();
  if (ShouldReportRelations(item->Role(), containerRole))
    return item;

  return nullptr;
}

Accessible*
AccGroupInfo::NextItemTo(Accessible* aItem)
{
  AccGroupInfo* groupInfo = aItem->GetGroupInfo();
  if (!groupInfo)
    return nullptr;

  // If the item in middle of the group then search next item in siblings.
  if (groupInfo->PosInSet() >= groupInfo->SetSize())
    return nullptr;

  Accessible* parent = aItem->Parent();
  uint32_t childCount = parent->ChildCount();
  for (uint32_t idx = aItem->IndexInParent() + 1; idx < childCount; idx++) {
    Accessible* nextItem = parent->GetChildAt(idx);
    AccGroupInfo* nextGroupInfo = nextItem->GetGroupInfo();
    if (nextGroupInfo &&
        nextGroupInfo->ConceptualParent() == groupInfo->ConceptualParent()) {
      return nextItem;
    }
  }

  MOZ_ASSERT_UNREACHABLE("Item in the middle of the group but there's no next item!");
  return nullptr;
}

bool
AccGroupInfo::ShouldReportRelations(role aRole, role aParentRole)
{
  // We only want to report hierarchy-based node relations for items in tree or
  // list form.  ARIA level/owns relations are always reported.
  if (aParentRole == roles::OUTLINE && aRole == roles::OUTLINEITEM)
    return true;
  if (aParentRole == roles::TREE_TABLE && aRole == roles::ROW)
    return true;
  if (aParentRole == roles::LIST && aRole == roles::LISTITEM)
    return true;

  return false;
}
