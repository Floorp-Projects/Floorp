/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAccUtils.h"

#include "Accessible-inl.h"
#include "nsAccessibilityService.h"
#include "nsARIAMap.h"
#include "nsCoreUtils.h"
#include "DocAccessible.h"
#include "HyperTextAccessible.h"
#include "nsIAccessibleTypes.h"
#include "Role.h"
#include "States.h"
#include "TextLeafAccessible.h"

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
nsAccUtils::GetDefaultLevel(Accessible* aAccessible)
{
  roles::Role role = aAccessible->Role();

  if (role == roles::OUTLINEITEM)
    return 1;

  if (role == roles::ROW) {
    Accessible* parent = aAccessible->Parent();
    // It is a row inside flatten treegrid. Group level is always 1 until it
    // is overriden by aria-level attribute.
    if (parent && parent->Role() == roles::TREE_TABLE) 
      return 1;
  }

  return 0;
}

PRInt32
nsAccUtils::GetARIAOrDefaultLevel(Accessible* aAccessible)
{
  PRInt32 level = 0;
  nsCoreUtils::GetUIntAttr(aAccessible->GetContent(),
                           nsGkAtoms::aria_level, &level);

  if (level != 0)
    return level;

  return GetDefaultLevel(aAccessible);
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
      nsRoleMapEntry* role = aria::GetRoleMap(ancestor);
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
          nsAccUtils::SetAccAttr(aAttributes, nsGkAtoms::containerLiveRole,
                                 role->ARIARoleString());
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

bool
nsAccUtils::HasDefinedARIAToken(nsIContent *aContent, nsIAtom *aAtom)
{
  NS_ASSERTION(aContent, "aContent is null in call to HasDefinedARIAToken!");

  if (!aContent->HasAttr(kNameSpaceID_None, aAtom) ||
      aContent->AttrValueIs(kNameSpaceID_None, aAtom,
                            nsGkAtoms::_empty, eCaseMatters) ||
      aContent->AttrValueIs(kNameSpaceID_None, aAtom,
                            nsGkAtoms::_undefined, eCaseMatters)) {
        return false;
  }
  return true;
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

Accessible*
nsAccUtils::GetAncestorWithRole(Accessible* aDescendant, PRUint32 aRole)
{
  Accessible* document = aDescendant->Document();
  Accessible* parent = aDescendant;
  while ((parent = parent->Parent())) {
    PRUint32 testRole = parent->Role();
    if (testRole == aRole)
      return parent;

    if (parent == document)
      break;
  }
  return nsnull;
}

Accessible*
nsAccUtils::GetSelectableContainer(Accessible* aAccessible, PRUint64 aState)
{
  if (!aAccessible)
    return nsnull;

  if (!(aState & states::SELECTABLE))
    return nsnull;

  Accessible* parent = aAccessible;
  while ((parent = parent->Parent()) && !parent->IsSelect()) {
    if (Role(parent) == nsIAccessibleRole::ROLE_PANE)
      return nsnull;
  }
  return parent;
}

bool
nsAccUtils::IsARIASelected(Accessible* aAccessible)
{
  return aAccessible->GetContent()->
    AttrValueIs(kNameSpaceID_None, nsGkAtoms::aria_selected,
                nsGkAtoms::_true, eCaseMatters);
}

HyperTextAccessible*
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

  // Get text accessible containing the result node.
  DocAccessible* doc = 
    GetAccService()->GetDocAccessible(resultNode->OwnerDoc());
  Accessible* accessible = doc ? 
    doc->GetAccessibleOrContainer(resultNode) : nsnull;
  if (!accessible) {
    NS_NOTREACHED("No nsIAccessibleText for selection change event!");
    return nsnull;
  }

  do {
    HyperTextAccessible* textAcc = accessible->AsHyperText();
    if (textAcc)
      return textAcc;

    accessible = accessible->Parent();
  } while (accessible);

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
  DocAccessible* document = aAccessNode->Document();
  Accessible* parent = document->GetContainerAccessible(aAccessNode->GetNode());
  if (!parent)
    return nsIntPoint(0, 0);

  nsIFrame *parentFrame = parent->GetFrame();
  if (!parentFrame)
    return nsIntPoint(0, 0);

  nsIntRect parentRect = parentFrame->GetScreenRectExternal();
  return nsIntPoint(parentRect.x, parentRect.y);
}

PRUint8
nsAccUtils::GetAttributeCharacteristics(nsIAtom* aAtom)
{
    for (PRUint32 i = 0; i < nsARIAMap::gWAIUnivAttrMapLength; i++)
      if (*nsARIAMap::gWAIUnivAttrMap[i].attributeName == aAtom)
        return nsARIAMap::gWAIUnivAttrMap[i].characteristics;

    return 0;
}

bool
nsAccUtils::GetLiveAttrValue(PRUint32 aRule, nsAString& aValue)
{
  switch (aRule) {
    case eOffLiveAttr:
      aValue = NS_LITERAL_STRING("off");
      return true;
    case ePoliteLiveAttr:
      aValue = NS_LITERAL_STRING("polite");
      return true;
  }

  return false;
}

#ifdef DEBUG

bool
nsAccUtils::IsTextInterfaceSupportCorrect(Accessible* aAccessible)
{
  // Don't test for accessible docs, it makes us create accessibles too
  // early and fire mutation events before we need to
  if (aAccessible->IsDoc())
    return true;

  bool foundText = false;
  PRUint32 childCount = aAccessible->ChildCount();
  for (PRUint32 childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = aAccessible->GetChildAt(childIdx);
    if (IsText(child)) {
      foundText = true;
      break;
    }
  }

  if (foundText) {
    // found text child node
    nsCOMPtr<nsIAccessibleText> text = do_QueryObject(aAccessible);
    if (!text)
      return false;
  }

  return true;
}
#endif

PRUint32
nsAccUtils::TextLength(Accessible* aAccessible)
{
  if (!IsText(aAccessible))
    return 1;

  TextLeafAccessible* textLeaf = aAccessible->AsTextLeaf();
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

bool
nsAccUtils::MustPrune(Accessible* aAccessible)
{ 
  roles::Role role = aAccessible->Role();

  // We don't prune buttons any more however AT don't expect children inside of
  // button in general, we allow menu buttons to have children to make them
  // accessible.
  return role == roles::MENUITEM || 
    role == roles::COMBOBOX_OPTION ||
    role == roles::OPTION ||
    role == roles::ENTRY ||
    role == roles::FLAT_EQUATION ||
    role == roles::PASSWORD_TEXT ||
    role == roles::TOGGLE_BUTTON ||
    role == roles::GRAPHIC ||
    role == roles::SLIDER ||
    role == roles::PROGRESSBAR ||
    role == roles::SEPARATOR;
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

  bool moveToLeft = aRowOrColHeaderCells == eRowHeaderCells;

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
      bool isHeader = moveToLeft ?
        role == nsIAccessibleRole::ROLE_ROWHEADER :
        role == nsIAccessibleRole::ROLE_COLUMNHEADER;

      if (isHeader)
        cells->AppendElement(cell, false);
    }
  }

  NS_ADDREF(*aCells = cells);
  return NS_OK;
}
