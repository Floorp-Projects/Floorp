/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "AccGroupInfo.h"

#include "Role.h"
#include "States.h"

using namespace mozilla::a11y;

AccGroupInfo::AccGroupInfo(nsAccessible* aItem, role aRole) :
  mPosInSet(0), mSetSize(0), mParent(nsnull)
{
  MOZ_COUNT_CTOR(AccGroupInfo);
  nsAccessible* parent = aItem->Parent();
  if (!parent)
    return;

  PRInt32 indexInParent = aItem->IndexInParent();
  PRInt32 siblingCount = parent->GetChildCount();
  if (siblingCount < indexInParent) {
    NS_ERROR("Wrong index in parent! Tree invalidation problem.");
    return;
  }

  PRInt32 level = nsAccUtils::GetARIAOrDefaultLevel(aItem);

  // Compute position in set.
  mPosInSet = 1;
  for (PRInt32 idx = indexInParent - 1; idx >=0 ; idx--) {
    nsAccessible* sibling = parent->GetChildAt(idx);
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

  for (PRInt32 idx = indexInParent + 1; idx < siblingCount; idx++) {
    nsAccessible* sibling = parent->GetChildAt(idx);

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

  nsAccessible* parentPrevSibling = parent->PrevSibling();
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
