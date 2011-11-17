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

#include "States.h"

using namespace mozilla::a11y;

AccGroupInfo::AccGroupInfo(nsAccessible* aItem, PRUint32 aRole) :
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
    PRUint32 siblingRole = sibling->Role();

    // If the sibling is separator then the group is ended.
    if (siblingRole == nsIAccessibleRole::ROLE_SEPARATOR)
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

    PRUint32 siblingRole = sibling->Role();

    // If the sibling is separator then the group is ended.
    if (siblingRole == nsIAccessibleRole::ROLE_SEPARATOR)
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

  PRUint32 parentRole = parent->Role();
  if (IsConceptualParent(aRole, parentRole))
    mParent = parent;

  // In the case of ARIA tree (not ARIA treegrid) a tree can be arranged by
  // using ARIA groups to organize levels. In this case the parent of the tree
  // item will be a group and the previous treeitem of that should be the tree
  // item parent.
  if (parentRole != nsIAccessibleRole::ROLE_GROUPING ||
      aRole != nsIAccessibleRole::ROLE_OUTLINEITEM)
    return;

  nsAccessible* parentPrevSibling = parent->PrevSibling();
  if (!parentPrevSibling)
    return;

  PRUint32 parentPrevSiblingRole = parentPrevSibling->Role();
  if (parentPrevSiblingRole == nsIAccessibleRole::ROLE_TEXT_LEAF) {
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
  if (parentPrevSiblingRole == nsIAccessibleRole::ROLE_OUTLINEITEM)
    mParent = parentPrevSibling;
}

bool
AccGroupInfo::IsConceptualParent(PRUint32 aRole, PRUint32 aParentRole)
{
  if (aParentRole == nsIAccessibleRole::ROLE_OUTLINE &&
      aRole == nsIAccessibleRole::ROLE_OUTLINEITEM)
    return true;
  if ((aParentRole == nsIAccessibleRole::ROLE_TABLE ||
       aParentRole == nsIAccessibleRole::ROLE_TREE_TABLE) &&
      aRole == nsIAccessibleRole::ROLE_ROW)
    return true;
  if (aParentRole == nsIAccessibleRole::ROLE_ROW &&
      (aRole == nsIAccessibleRole::ROLE_CELL ||
       aRole == nsIAccessibleRole::ROLE_GRID_CELL))
    return true;
  if (aParentRole == nsIAccessibleRole::ROLE_LIST &&
      aRole == nsIAccessibleRole::ROLE_LISTITEM)
    return true;
  if (aParentRole == nsIAccessibleRole::ROLE_COMBOBOX_LIST &&
      aRole == nsIAccessibleRole::ROLE_COMBOBOX_OPTION)
    return true;
  if (aParentRole == nsIAccessibleRole::ROLE_LISTBOX &&
      aRole == nsIAccessibleRole::ROLE_OPTION)
    return true;
  if (aParentRole == nsIAccessibleRole::ROLE_PAGETABLIST &&
      aRole == nsIAccessibleRole::ROLE_PAGETAB)
    return true;
  if ((aParentRole == nsIAccessibleRole::ROLE_POPUP_MENU ||
       aParentRole == nsIAccessibleRole::ROLE_MENUPOPUP) &&
      aRole == nsIAccessibleRole::ROLE_MENUITEM)
    return true;

  return false;
}
