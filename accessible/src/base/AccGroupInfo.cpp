/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccGroupInfo.h"

#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

AccGroupInfo::AccGroupInfo(Accessible* aItem, role aRole) :
  mPosInSet(0), mSetSize(0), mParent(nsnull)
{
  MOZ_COUNT_CTOR(AccGroupInfo);
  Accessible* parent = aItem->Parent();
  if (!parent)
    return;

  PRInt32 indexInParent = aItem->IndexInParent();
  PRUint32 siblingCount = parent->ChildCount();
  if (indexInParent == -1 ||
      indexInParent >= static_cast<PRInt32>(siblingCount)) {
    NS_ERROR("Wrong index in parent! Tree invalidation problem.");
    return;
  }

  PRInt32 level = nsAccUtils::GetARIAOrDefaultLevel(aItem);

  // Compute position in set.
  mPosInSet = 1;
  for (PRInt32 idx = indexInParent - 1; idx >= 0 ; idx--) {
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
    PRInt32 siblingLevel = nsAccUtils::GetARIAOrDefaultLevel(sibling);
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

  for (PRUint32 idx = indexInParent + 1; idx < siblingCount; idx++) {
    Accessible* sibling = parent->GetChildAt(idx);

    roles::Role siblingRole = sibling->Role();

    // If the sibling is separator then the group is ended.
    if (siblingRole == roles::SEPARATOR)
      break;

    // If sibling is visible and has the same base role
    if (BaseRole(siblingRole) != aRole || sibling->State() & states::INVISIBLE)
      continue;

    // and check if it's hierarchical flatten structure.
    PRInt32 siblingLevel = nsAccUtils::GetARIAOrDefaultLevel(sibling);
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
