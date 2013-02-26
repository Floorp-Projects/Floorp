/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccGroupInfo.h"

#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

AccGroupInfo::AccGroupInfo(Accessible* aItem, role aRole) :
  mPosInSet(0), mSetSize(0), mParent(nullptr)
{
  MOZ_COUNT_CTOR(AccGroupInfo);
  Accessible* parent = aItem->Parent();
  if (!parent)
    return;

  int32_t indexInParent = aItem->IndexInParent();
  uint32_t siblingCount = parent->ChildCount();
  if (indexInParent == -1 ||
      indexInParent >= static_cast<int32_t>(siblingCount)) {
    NS_ERROR("Wrong index in parent! Tree invalidation problem.");
    return;
  }

  int32_t level = nsAccUtils::GetARIAOrDefaultLevel(aItem);

  // Compute position in set.
  mPosInSet = 1;
  for (int32_t idx = indexInParent - 1; idx >= 0 ; idx--) {
    Accessible* sibling = parent->GetChildAt(idx);
    roles::Role siblingRole = sibling->Role();

    // If the sibling is separator then the group is ended.
    if (siblingRole == roles::SEPARATOR)
      break;

    // If sibling is not visible and hasn't the same base role.
    if (BaseRole(siblingRole) != aRole || sibling->State() & states::INVISIBLE)
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
    if (sibling->mGroupInfo) {
      mPosInSet += sibling->mGroupInfo->mPosInSet;
      mParent = sibling->mGroupInfo->mParent;
      mSetSize = sibling->mGroupInfo->mSetSize;
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
    if (BaseRole(siblingRole) != aRole || sibling->State() & states::INVISIBLE)
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
    if (sibling->mGroupInfo) {
      mParent = sibling->mGroupInfo->mParent;
      mSetSize = sibling->mGroupInfo->mSetSize;
      return;
    }

    mSetSize++;
  }

  if (mParent)
    return;

  roles::Role parentRole = parent->Role();
  if (IsConceptualParent(aRole, parentRole))
    mParent = parent;

  // In the case of ARIA tree (not ARIA treegrid) a tree can be arranged by
  // using ARIA groups to organize levels. In this case the parent of the tree
  // item will be a group and the previous treeitem of that should be the tree
  // item parent.
  if (parentRole != roles::GROUPING || aRole != roles::OUTLINEITEM)
    return;

  Accessible* parentPrevSibling = parent->PrevSibling();
  if (!parentPrevSibling)
    return;

  roles::Role parentPrevSiblingRole = parentPrevSibling->Role();
  if (parentPrevSiblingRole == roles::TEXT_LEAF) {
    // XXX Sometimes an empty text accessible is in the hierarchy here,
    // although the text does not appear to be rendered, GetRenderedText()
    // says that it is so we need to skip past it to find the true
    // previous sibling.
    parentPrevSibling = parentPrevSibling->PrevSibling();
    if (parentPrevSibling)
      parentPrevSiblingRole = parentPrevSibling->Role();
  }

  // Previous sibling of parent group is a tree item, this is the
  // conceptual tree item parent.
  if (parentPrevSiblingRole == roles::OUTLINEITEM)
    mParent = parentPrevSibling;
}

Accessible*
AccGroupInfo::FirstItemOf(Accessible* aContainer)
{
  // ARIA trees can be arranged by ARIA groups, otherwise aria-level works.
  a11y::role containerRole = aContainer->Role();
  Accessible* item = aContainer->NextSibling();
  if (item) {
    if (containerRole == roles::OUTLINEITEM && item->Role() == roles::GROUPING)
      item = item->FirstChild();

    AccGroupInfo* itemGroupInfo = item->GetGroupInfo();
    if (itemGroupInfo && itemGroupInfo->ConceptualParent() == aContainer)
      return item;
  }

  // Otherwise it can be a direct child.
  item = aContainer->FirstChild();
  if (item && IsConceptualParent(BaseRole(item->Role()), containerRole))
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
  for (int32_t idx = aItem->IndexInParent() + 1; idx < childCount; idx++) {
    Accessible* nextItem = parent->GetChildAt(idx);
    AccGroupInfo* nextGroupInfo = nextItem->GetGroupInfo();
    if (nextGroupInfo &&
        nextGroupInfo->ConceptualParent() == groupInfo->ConceptualParent()) {
      return nextItem;
    }
  }

  NS_NOTREACHED("Item in the midle of the group but there's no next item!");
  return nullptr;
}

bool
AccGroupInfo::IsConceptualParent(role aRole, role aParentRole)
{
  if (aParentRole == roles::OUTLINE && aRole == roles::OUTLINEITEM)
    return true;
  if ((aParentRole == roles::TABLE || aParentRole == roles::TREE_TABLE) &&
      aRole == roles::ROW)
    return true;
  if (aParentRole == roles::ROW &&
      (aRole == roles::CELL || aRole == roles::GRID_CELL))
    return true;
  if (aParentRole == roles::LIST && aRole == roles::LISTITEM)
    return true;
  if (aParentRole == roles::COMBOBOX_LIST && aRole == roles::COMBOBOX_OPTION)
    return true;
  if (aParentRole == roles::LISTBOX && aRole == roles::OPTION)
    return true;
  if (aParentRole == roles::PAGETABLIST && aRole == roles::PAGETAB)
    return true;
  if ((aParentRole == roles::POPUP_MENU || aParentRole == roles::MENUPOPUP) &&
      aRole == roles::MENUITEM)
    return true;

  return false;
}
