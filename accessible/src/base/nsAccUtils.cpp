/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsCoreUtils.h"
#include "nsAccUtils.h"

#include "States.h"
#include "nsIAccessibleTypes.h"

#include "nsAccessibilityService.h"
#include "nsARIAMap.h"
#include "nsDocAccessible.h"
#include "nsHyperTextAccessible.h"
#include "nsHTMLTableAccessible.h"
#include "nsTextAccessible.h"
#include "nsXULTreeGridAccessible.h"

#include "nsIDOMXULContainerElement.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsWhitespaceTokenizer.h"
#include "nsComponentManagerUtils.h"

namespace dom = mozilla::dom;
using namespace mozilla::a11y;

void
nsAccUtils::GetAccAttr(nsIPersistentProperties *aAttributes,
                       nsIAtom *aAttrName, nsAString& aAttrValue)
{
  aAttrValue.Truncate();

  aAttributes->GetStringProperty(nsAtomCString(aAttrName), aAttrValue);
}

void
nsAccUtils::SetAccAttr(nsIPersistentProperties *aAttributes,
                       nsIAtom *aAttrName, const nsAString& aAttrValue)
{
  nsAutoString oldValue;
  nsCAutoString attrName;

  aAttributes->SetStringProperty(nsAtomCString(aAttrName), aAttrValue, oldValue);
}

void
nsAccUtils::SetAccGroupAttrs(nsIPersistentProperties *aAttributes,
                             PRInt32 aLevel, PRInt32 aSetSize,
                             PRInt32 aPosInSet)
{
  nsAutoString value;

  if (aLevel) {
    value.AppendInt(aLevel);
    SetAccAttr(aAttributes, nsGkAtoms::level, value);
  }

  if (aSetSize && aPosInSet) {
    value.Truncate();
    value.AppendInt(aPosInSet);
    SetAccAttr(aAttributes, nsGkAtoms::posinset, value);

    value.Truncate();
    value.AppendInt(aSetSize);
    SetAccAttr(aAttributes, nsGkAtoms::setsize, value);
  }
}

PRInt32
nsAccUtils::GetDefaultLevel(nsAccessible *aAccessible)
{
  PRUint32 role = aAccessible->Role();

  if (role == nsIAccessibleRole::ROLE_OUTLINEITEM)
    return 1;

  if (role == nsIAccessibleRole::ROLE_ROW) {
    nsAccessible* parent = aAccessible->Parent();
    if (parent && parent->Role() == nsIAccessibleRole::ROLE_TREE_TABLE) {
      // It is a row inside flatten treegrid. Group level is always 1 until it
      // is overriden by aria-level attribute.
      return 1;
    }
  }

  return 0;
}

PRInt32
nsAccUtils::GetARIAOrDefaultLevel(nsAccessible *aAccessible)
{
  PRInt32 level = 0;
  nsCoreUtils::GetUIntAttr(aAccessible->GetContent(),
                           nsGkAtoms::aria_level, &level);

  if (level != 0)
    return level;

  return GetDefaultLevel(aAccessible);
}

void
nsAccUtils::GetPositionAndSizeForXULSelectControlItem(nsIContent *aContent,
                                                      PRInt32 *aPosInSet,
                                                      PRInt32 *aSetSize)
{
  nsCOMPtr<nsIDOMXULSelectControlItemElement> item(do_QueryInterface(aContent));
  if (!item)
    return;

  nsCOMPtr<nsIDOMXULSelectControlElement> control;
  item->GetControl(getter_AddRefs(control));
  if (!control)
    return;

  PRUint32 itemsCount = 0;
  control->GetItemCount(&itemsCount);

  PRInt32 indexOf = 0;
  control->GetIndexOfItem(item, &indexOf);

  *aSetSize = itemsCount;
  *aPosInSet = indexOf;

  for (PRUint32 index = 0; index < itemsCount; index++) {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> currItem;
    control->GetItemAtIndex(index, getter_AddRefs(currItem));
    nsCOMPtr<nsINode> currNode(do_QueryInterface(currItem));

    nsAccessible* itemAcc = currNode ?
      GetAccService()->GetAccessible(currNode) : nsnull;

    if (!itemAcc || itemAcc->State() & states::INVISIBLE) {
      (*aSetSize)--;
      if (index < static_cast<PRUint32>(indexOf))
        (*aPosInSet)--;
    }
  }

  (*aPosInSet)++; // group position is 1-index based.
}

void
nsAccUtils::GetPositionAndSizeForXULContainerItem(nsIContent *aContent,
                                                  PRInt32 *aPosInSet,
                                                  PRInt32 *aSetSize)
{
  nsCOMPtr<nsIDOMXULContainerItemElement> item(do_QueryInterface(aContent));
  if (!item)
    return;

  nsCOMPtr<nsIDOMXULContainerElement> container;
  item->GetParentContainer(getter_AddRefs(container));
  if (!container)
    return;

  // Get item count.
  PRUint32 itemsCount = 0;
  container->GetItemCount(&itemsCount);

  // Get item index.
  PRInt32 indexOf = 0;
  container->GetIndexOfItem(item, &indexOf);

  // Calculate set size and position in the set.
  *aSetSize = 0, *aPosInSet = 0;
  for (PRInt32 index = indexOf; index >= 0; index--) {
    nsCOMPtr<nsIDOMXULElement> item;
    container->GetItemAtIndex(index, getter_AddRefs(item));
    nsCOMPtr<nsINode> itemNode(do_QueryInterface(item));

    nsAccessible* itemAcc = itemNode ?
      GetAccService()->GetAccessible(itemNode) : nsnull;

    if (itemAcc) {
      PRUint32 itemRole = Role(itemAcc);
      if (itemRole == nsIAccessibleRole::ROLE_SEPARATOR)
        break; // We reached the beginning of our group.

      if (!(itemAcc->State() & states::INVISIBLE)) {
        (*aSetSize)++;
        (*aPosInSet)++;
      }
    }
  }

  for (PRInt32 index = indexOf + 1; index < static_cast<PRInt32>(itemsCount);
       index++) {
    nsCOMPtr<nsIDOMXULElement> item;
    container->GetItemAtIndex(index, getter_AddRefs(item));
    nsCOMPtr<nsINode> itemNode(do_QueryInterface(item));

    nsAccessible* itemAcc =
      itemNode ? GetAccService()->GetAccessible(itemNode) : nsnull;

    if (itemAcc) {
      PRUint32 itemRole = Role(itemAcc);
      if (itemRole == nsIAccessibleRole::ROLE_SEPARATOR)
        break; // We reached the end of our group.

      if (!(itemAcc->State() & states::INVISIBLE))
        (*aSetSize)++;
    }
  }
}

PRInt32
nsAccUtils::GetLevelForXULContainerItem(nsIContent *aContent)
{
  nsCOMPtr<nsIDOMXULContainerItemElement> item(do_QueryInterface(aContent));
  if (!item)
    return 0;

  nsCOMPtr<nsIDOMXULContainerElement> container;
  item->GetParentContainer(getter_AddRefs(container));
  if (!container)
    return 0;

  // Get level of the item.
  PRInt32 level = -1;
  while (container) {
    level++;

    nsCOMPtr<nsIDOMXULContainerElement> parentContainer;
    container->GetParentContainer(getter_AddRefs(parentContainer));
    parentContainer.swap(container);
  }

  return level;
}

void
nsAccUtils::SetLiveContainerAttributes(nsIPersistentProperties *aAttributes,
                                       nsIContent *aStartContent,
                                       nsIContent *aTopContent)
{
  nsAutoString atomic, live, relevant, busy;
  nsIContent *ancestor = aStartContent;
  while (ancestor) {

    // container-relevant attribute
    if (relevant.IsEmpty() &&
        nsAccUtils::HasDefinedARIAToken(ancestor, nsGkAtoms::aria_relevant) &&
        ancestor->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_relevant, relevant))
      SetAccAttr(aAttributes, nsGkAtoms::containerRelevant, relevant);

    // container-live, and container-live-role attributes
    if (live.IsEmpty()) {
      nsRoleMapEntry *role = GetRoleMapEntry(ancestor);
      if (nsAccUtils::HasDefinedARIAToken(ancestor,
                                          nsGkAtoms::aria_live)) {
        ancestor->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_live,
                          live);
      } else if (role) {
        GetLiveAttrValue(role->liveAttRule, live);
      }
      if (!live.IsEmpty()) {
        SetAccAttr(aAttributes, nsGkAtoms::containerLive, live);
        if (role) {
          nsAccUtils::SetAccAttr(aAttributes,
                                 nsGkAtoms::containerLiveRole,
                                 NS_ConvertASCIItoUTF16(role->roleString));
        }
      }
    }

    // container-atomic attribute
    if (atomic.IsEmpty() &&
        nsAccUtils::HasDefinedARIAToken(ancestor, nsGkAtoms::aria_atomic) &&
        ancestor->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_atomic, atomic))
      SetAccAttr(aAttributes, nsGkAtoms::containerAtomic, atomic);

    // container-busy attribute
    if (busy.IsEmpty() &&
        nsAccUtils::HasDefinedARIAToken(ancestor, nsGkAtoms::aria_busy) &&
        ancestor->GetAttr(kNameSpaceID_None, nsGkAtoms::aria_busy, busy))
      SetAccAttr(aAttributes, nsGkAtoms::containerBusy, busy);

    if (ancestor == aTopContent)
      break;

    ancestor = ancestor->GetParent();
    if (!ancestor)
      ancestor = aTopContent; // Use <body>/<frameset>
  }
}

PRBool
nsAccUtils::HasDefinedARIAToken(nsIContent *aContent, nsIAtom *aAtom)
{
  NS_ASSERTION(aContent, "aContent is null in call to HasDefinedARIAToken!");

  if (!aContent->HasAttr(kNameSpaceID_None, aAtom) ||
      aContent->AttrValueIs(kNameSpaceID_None, aAtom,
                            nsGkAtoms::_empty, eCaseMatters) ||
      aContent->AttrValueIs(kNameSpaceID_None, aAtom,
                            nsGkAtoms::_undefined, eCaseMatters)) {
        return PR_FALSE;
  }
  return PR_TRUE;
}

nsIAtom*
nsAccUtils::GetARIAToken(dom::Element* aElement, nsIAtom* aAttr)
{
  if (!nsAccUtils::HasDefinedARIAToken(aElement, aAttr))
    return nsGkAtoms::_empty;

  static nsIContent::AttrValuesArray tokens[] =
    { &nsGkAtoms::_false, &nsGkAtoms::_true,
      &nsGkAtoms::mixed, nsnull};

  PRInt32 idx = aElement->FindAttrValueIn(kNameSpaceID_None,
                                          aAttr, tokens, eCaseMatters);
  if (idx >= 0)
    return *(tokens[idx]);

  return nsnull;
}

nsAccessible *
nsAccUtils::GetAncestorWithRole(nsAccessible *aDescendant, PRUint32 aRole)
{
  nsAccessible *document = aDescendant->GetDocAccessible();
  nsAccessible *parent = aDescendant;
  while ((parent = parent->Parent())) {
    PRUint32 testRole = parent->Role();
    if (testRole == aRole)
      return parent;

    if (parent == document)
      break;
  }
  return nsnull;
}

nsAccessible*
nsAccUtils::GetSelectableContainer(nsAccessible* aAccessible, PRUint64 aState)
{
  if (!aAccessible)
    return nsnull;

  if (!(aState & states::SELECTABLE))
    return nsnull;

  nsAccessible* parent = aAccessible;
  while ((parent = parent->Parent()) && !parent->IsSelect()) {
    if (Role(parent) == nsIAccessibleRole::ROLE_PANE)
      return nsnull;
  }
  return parent;
}

nsAccessible*
nsAccUtils::GetMultiSelectableContainer(nsINode* aNode)
{
  nsAccessible* accessible = GetAccService()->GetAccessible(aNode);
  if (accessible) {
    nsAccessible* container = GetSelectableContainer(accessible,
                                                     accessible->State());
    if (container && container->State() & states::MULTISELECTABLE)
      return container;
  }

  return nsnull;
}

PRBool
nsAccUtils::IsARIASelected(nsAccessible *aAccessible)
{
  return aAccessible->GetContent()->
    AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_selected,
                nsGkAtoms::_true, eCaseMatters);
}

nsHyperTextAccessible*
nsAccUtils::GetTextAccessibleFromSelection(nsISelection* aSelection)
{
  // Get accessible from selection's focus DOM point (the DOM point where
  // selection is ended).

  nsCOMPtr<nsIDOMNode> focusDOMNode;
  aSelection->GetFocusNode(getter_AddRefs(focusDOMNode));
  if (!focusDOMNode)
    return nsnull;

  PRInt32 focusOffset = 0;
  aSelection->GetFocusOffset(&focusOffset);

  nsCOMPtr<nsINode> focusNode(do_QueryInterface(focusDOMNode));
  nsCOMPtr<nsINode> resultNode =
    nsCoreUtils::GetDOMNodeFromDOMPoint(focusNode, focusOffset);
  nsCOMPtr<nsIWeakReference> weakShell(nsCoreUtils::GetWeakShellFor(resultNode));

  // Get text accessible containing the result node.
  nsAccessible* accessible =
    GetAccService()->GetAccessibleOrContainer(resultNode, weakShell);
  if (!accessible) {
    NS_NOTREACHED("No nsIAccessibleText for selection change event!");
    return nsnull;
  }

  do {
    nsHyperTextAccessible* textAcc = accessible->AsHyperText();
    if (textAcc)
      return textAcc;

  } while (accessible = accessible->Parent());

  NS_NOTREACHED("We must reach document accessible implementing nsIAccessibleText!");
  return nsnull;
}

nsresult
nsAccUtils::ConvertToScreenCoords(PRInt32 aX, PRInt32 aY,
                                  PRUint32 aCoordinateType,
                                  nsAccessNode *aAccessNode,
                                  nsIntPoint *aCoords)
{
  NS_ENSURE_ARG_POINTER(aCoords);

  aCoords->MoveTo(aX, aY);

  switch (aCoordinateType) {
    case nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE:
      break;

    case nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE:
    {
      NS_ENSURE_ARG(aAccessNode);
      *aCoords += GetScreenCoordsForWindow(aAccessNode);
      break;
    }

    case nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE:
    {
      NS_ENSURE_ARG(aAccessNode);
      *aCoords += GetScreenCoordsForParent(aAccessNode);
      break;
    }

    default:
      return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

nsresult
nsAccUtils::ConvertScreenCoordsTo(PRInt32 *aX, PRInt32 *aY,
                                  PRUint32 aCoordinateType,
                                  nsAccessNode *aAccessNode)
{
  switch (aCoordinateType) {
    case nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE:
      break;

    case nsIAccessibleCoordinateType::COORDTYPE_WINDOW_RELATIVE:
    {
      NS_ENSURE_ARG(aAccessNode);
      nsIntPoint coords = nsAccUtils::GetScreenCoordsForWindow(aAccessNode);
      *aX -= coords.x;
      *aY -= coords.y;
      break;
    }

    case nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE:
    {
      NS_ENSURE_ARG(aAccessNode);
      nsIntPoint coords = nsAccUtils::GetScreenCoordsForParent(aAccessNode);
      *aX -= coords.x;
      *aY -= coords.y;
      break;
    }

    default:
      return NS_ERROR_INVALID_ARG;
  }

  return NS_OK;
}

nsIntPoint
nsAccUtils::GetScreenCoordsForWindow(nsAccessNode *aAccessNode)
{
  return nsCoreUtils::GetScreenCoordsForWindow(aAccessNode->GetNode());
}

nsIntPoint
nsAccUtils::GetScreenCoordsForParent(nsAccessNode *aAccessNode)
{
  nsAccessible *parent =
    GetAccService()->GetContainerAccessible(aAccessNode->GetNode(),
                                            aAccessNode->GetWeakShell());
  if (!parent)
    return nsIntPoint(0, 0);

  nsIFrame *parentFrame = parent->GetFrame();
  if (!parentFrame)
    return nsIntPoint(0, 0);

  nsIntRect parentRect = parentFrame->GetScreenRectExternal();
  return nsIntPoint(parentRect.x, parentRect.y);
}

nsRoleMapEntry*
nsAccUtils::GetRoleMapEntry(nsINode *aNode)
{
  nsIContent *content = nsCoreUtils::GetRoleContent(aNode);
  nsAutoString roleString;
  if (!content ||
      !content->GetAttr(kNameSpaceID_None, nsGkAtoms::role, roleString) ||
      roleString.IsEmpty()) {
    // We treat role="" as if the role attribute is absent (per aria spec:8.1.1)
    return nsnull;
  }

  nsWhitespaceTokenizer tokenizer(roleString);
  while (tokenizer.hasMoreTokens()) {
    // Do a binary search through table for the next role in role list
    NS_LossyConvertUTF16toASCII role(tokenizer.nextToken());
    PRUint32 low = 0;
    PRUint32 high = nsARIAMap::gWAIRoleMapLength;
    while (low < high) {
      PRUint32 index = (low + high) / 2;
      PRInt32 compare = PL_strcmp(role.get(), nsARIAMap::gWAIRoleMap[index].roleString);
      if (compare == 0) {
        // The  role attribute maps to an entry in the role table
        return &nsARIAMap::gWAIRoleMap[index];
      }
      if (compare < 0) {
        high = index;
      }
      else {
        low = index + 1;
      }
    }
  }

  // Always use some entry if there is a non-empty role string
  // To ensure an accessible object is created
  return &nsARIAMap::gLandmarkRoleMap;
}

PRUint8
nsAccUtils::GetAttributeCharacteristics(nsIAtom* aAtom)
{
    for (PRUint32 i = 0; i < nsARIAMap::gWAIUnivAttrMapLength; i++)
      if (*nsARIAMap::gWAIUnivAttrMap[i].attributeName == aAtom)
        return nsARIAMap::gWAIUnivAttrMap[i].characteristics;

    return 0;
}

PRBool
nsAccUtils::GetLiveAttrValue(PRUint32 aRule, nsAString& aValue)
{
  switch (aRule) {
    case eOffLiveAttr:
      aValue = NS_LITERAL_STRING("off");
      return PR_TRUE;
    case ePoliteLiveAttr:
      aValue = NS_LITERAL_STRING("polite");
      return PR_TRUE;
  }

  return PR_FALSE;
}

#ifdef DEBUG_A11Y

PRBool
nsAccUtils::IsTextInterfaceSupportCorrect(nsAccessible *aAccessible)
{
  PRBool foundText = PR_FALSE;
  
  nsCOMPtr<nsIAccessibleDocument> accDoc = do_QueryObject(aAccessible);
  if (accDoc) {
    // Don't test for accessible docs, it makes us create accessibles too
    // early and fire mutation events before we need to
    return PR_TRUE;
  }

  PRInt32 childCount = aAccessible->GetChildCount();
  for (PRint32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsAccessible *child = GetChildAt(childIdx);
    if (IsText(child)) {
      foundText = PR_TRUE;
      break;
    }
  }

  if (foundText) {
    // found text child node
    nsCOMPtr<nsIAccessibleText> text = do_QueryObject(aAccessible);
    if (!text)
      return PR_FALSE;
  }

  return PR_TRUE; 
}
#endif

PRUint32
nsAccUtils::TextLength(nsAccessible *aAccessible)
{
  if (!IsText(aAccessible))
    return 1;

  nsTextAccessible* textLeaf = aAccessible->AsTextLeaf();
  if (textLeaf)
    return textLeaf->Text().Length();

  // For list bullets (or anything other accessible which would compute its own
  // text. They don't have their own frame.
  // XXX In the future, list bullets may have frame and anon content, so 
  // we should be able to remove this at that point
  nsAutoString text;
  aAccessible->AppendTextTo(text); // Get all the text
  return text.Length();
}

PRBool
nsAccUtils::MustPrune(nsIAccessible *aAccessible)
{ 
  PRUint32 role = nsAccUtils::Role(aAccessible);

  // We don't prune buttons any more however AT don't expect children inside of
  // button in general, we allow menu buttons to have children to make them
  // accessible.
  return role == nsIAccessibleRole::ROLE_MENUITEM || 
    role == nsIAccessibleRole::ROLE_COMBOBOX_OPTION ||
    role == nsIAccessibleRole::ROLE_OPTION ||
    role == nsIAccessibleRole::ROLE_ENTRY ||
    role == nsIAccessibleRole::ROLE_FLAT_EQUATION ||
    role == nsIAccessibleRole::ROLE_PASSWORD_TEXT ||
    role == nsIAccessibleRole::ROLE_TOGGLE_BUTTON ||
    role == nsIAccessibleRole::ROLE_GRAPHIC ||
    role == nsIAccessibleRole::ROLE_SLIDER ||
    role == nsIAccessibleRole::ROLE_PROGRESSBAR ||
    role == nsIAccessibleRole::ROLE_SEPARATOR;
}

nsresult
nsAccUtils::GetHeaderCellsFor(nsIAccessibleTable *aTable,
                              nsIAccessibleTableCell *aCell,
                              PRInt32 aRowOrColHeaderCells, nsIArray **aCells)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIMutableArray> cells = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowIdx = -1;
  rv = aCell->GetRowIndex(&rowIdx);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 colIdx = -1;
  rv = aCell->GetColumnIndex(&colIdx);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool moveToLeft = aRowOrColHeaderCells == eRowHeaderCells;

  // Move to the left or top to find row header cells or column header cells.
  PRInt32 index = (moveToLeft ? colIdx : rowIdx) - 1;
  for (; index >= 0; index--) {
    PRInt32 curRowIdx = moveToLeft ? rowIdx : index;
    PRInt32 curColIdx = moveToLeft ? index : colIdx;

    nsCOMPtr<nsIAccessible> cell;
    rv = aTable->GetCellAt(curRowIdx, curColIdx, getter_AddRefs(cell));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIAccessibleTableCell> tableCellAcc =
      do_QueryInterface(cell);

    // GetCellAt should always return an nsIAccessibleTableCell (XXX Bug 587529)
    NS_ENSURE_STATE(tableCellAcc);

    PRInt32 origIdx = 1;
    if (moveToLeft)
      rv = tableCellAcc->GetColumnIndex(&origIdx);
    else
      rv = tableCellAcc->GetRowIndex(&origIdx);
    NS_ENSURE_SUCCESS(rv, rv);

    if (origIdx == index) {
      // Append original header cells only.
      PRUint32 role = Role(cell);
      PRBool isHeader = moveToLeft ?
        role == nsIAccessibleRole::ROLE_ROWHEADER :
        role == nsIAccessibleRole::ROLE_COLUMNHEADER;

      if (isHeader)
        cells->AppendElement(cell, PR_FALSE);
    }
  }

  NS_ADDREF(*aCells = cells);
  return NS_OK;
}
