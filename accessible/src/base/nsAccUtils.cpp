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

#include "nsIAccessibleStates.h"
#include "nsIAccessibleTypes.h"
#include "nsPIAccessible.h"

#include "nsAccessibleEventData.h"
#include "nsHyperTextAccessible.h"
#include "nsAccessibilityAtoms.h"
#include "nsAccessibleTreeWalker.h"
#include "nsAccessible.h"
#include "nsARIAMap.h"

#include "nsIDOMXULContainerElement.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsWhitespaceTokenizer.h"

void
nsAccUtils::GetAccAttr(nsIPersistentProperties *aAttributes,
                       nsIAtom *aAttrName, nsAString& aAttrValue)
{
  aAttrValue.Truncate();

  nsCAutoString attrName;
  aAttrName->ToUTF8String(attrName);
  aAttributes->GetStringProperty(attrName, aAttrValue);
}

void
nsAccUtils::SetAccAttr(nsIPersistentProperties *aAttributes,
                       nsIAtom *aAttrName, const nsAString& aAttrValue)
{
  nsAutoString oldValue;
  nsCAutoString attrName;

  aAttrName->ToUTF8String(attrName);
  aAttributes->SetStringProperty(attrName, aAttrValue, oldValue);
}

void
nsAccUtils::GetAccGroupAttrs(nsIPersistentProperties *aAttributes,
                             PRInt32 *aLevel, PRInt32 *aPosInSet,
                             PRInt32 *aSetSize)
{
  *aLevel = 0;
  *aPosInSet = 0;
  *aSetSize = 0;

  nsAutoString value;
  PRInt32 error = NS_OK;

  GetAccAttr(aAttributes, nsAccessibilityAtoms::level, value);
  if (!value.IsEmpty()) {
    PRInt32 level = value.ToInteger(&error);
    if (NS_SUCCEEDED(error))
      *aLevel = level;
  }

  GetAccAttr(aAttributes, nsAccessibilityAtoms::posinset, value);
  if (!value.IsEmpty()) {
    PRInt32 posInSet = value.ToInteger(&error);
    if (NS_SUCCEEDED(error))
      *aPosInSet = posInSet;
  }

  GetAccAttr(aAttributes, nsAccessibilityAtoms::setsize, value);
  if (!value.IsEmpty()) {
    PRInt32 sizeSet = value.ToInteger(&error);
    if (NS_SUCCEEDED(error))
      *aSetSize = sizeSet;
  }
}

PRBool
nsAccUtils::HasAccGroupAttrs(nsIPersistentProperties *aAttributes)
{
  nsAutoString value;

  GetAccAttr(aAttributes, nsAccessibilityAtoms::setsize, value);
  if (!value.IsEmpty()) {
    GetAccAttr(aAttributes, nsAccessibilityAtoms::posinset, value);
    return !value.IsEmpty();
  }

  return PR_FALSE;
}

void
nsAccUtils::SetAccGroupAttrs(nsIPersistentProperties *aAttributes,
                             PRInt32 aLevel, PRInt32 aPosInSet,
                             PRInt32 aSetSize)
{
  nsAutoString value;

  if (aLevel) {
    value.AppendInt(aLevel);
    SetAccAttr(aAttributes, nsAccessibilityAtoms::level, value);
  }

  if (aSetSize && aPosInSet) {
    value.Truncate();
    value.AppendInt(aPosInSet);
    SetAccAttr(aAttributes, nsAccessibilityAtoms::posinset, value);

    value.Truncate();
    value.AppendInt(aSetSize);
    SetAccAttr(aAttributes, nsAccessibilityAtoms::setsize, value);
  }
}

void
nsAccUtils::SetAccAttrsForXULSelectControlItem(nsIDOMNode *aNode,
                                               nsIPersistentProperties *aAttributes)
{
  nsCOMPtr<nsIDOMXULSelectControlItemElement> item(do_QueryInterface(aNode));
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

  PRUint32 setSize = itemsCount, posInSet = indexOf;
  for (PRUint32 index = 0; index < itemsCount; index++) {
    nsCOMPtr<nsIDOMXULSelectControlItemElement> currItem;
    control->GetItemAtIndex(index, getter_AddRefs(currItem));
    nsCOMPtr<nsIDOMNode> currNode(do_QueryInterface(currItem));

    nsCOMPtr<nsIAccessible> itemAcc;
    nsAccessNode::GetAccService()->GetAccessibleFor(currNode,
                                                    getter_AddRefs(itemAcc));
    if (!itemAcc ||
        State(itemAcc) & nsIAccessibleStates::STATE_INVISIBLE) {
      setSize--;
      if (index < static_cast<PRUint32>(indexOf))
        posInSet--;
    }
  }

  SetAccGroupAttrs(aAttributes, 0, posInSet + 1, setSize);
}

void
nsAccUtils::SetAccAttrsForXULContainerItem(nsIDOMNode *aNode,
                                           nsIPersistentProperties *aAttributes)
{
  nsCOMPtr<nsIDOMXULContainerItemElement> item(do_QueryInterface(aNode));
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
  PRUint32 setSize = 0, posInSet = 0;
  for (PRInt32 index = indexOf; index >= 0; index--) {
    nsCOMPtr<nsIDOMXULElement> item;
    container->GetItemAtIndex(index, getter_AddRefs(item));

    nsCOMPtr<nsIAccessible> itemAcc;
    nsAccessNode::GetAccService()->GetAccessibleFor(item,
                                                    getter_AddRefs(itemAcc));

    if (itemAcc) {
      PRUint32 itemRole = Role(itemAcc);
      if (itemRole == nsIAccessibleRole::ROLE_SEPARATOR)
        break; // We reached the beginning of our group.

      PRUint32 itemState = State(itemAcc);
      if (!(itemState & nsIAccessibleStates::STATE_INVISIBLE)) {
        setSize++;
        posInSet++;
      }
    }
  }

  for (PRInt32 index = indexOf + 1; index < static_cast<PRInt32>(itemsCount);
       index++) {
    nsCOMPtr<nsIDOMXULElement> item;
    container->GetItemAtIndex(index, getter_AddRefs(item));
    
    nsCOMPtr<nsIAccessible> itemAcc;
    nsAccessNode::GetAccService()->GetAccessibleFor(item,
                                                    getter_AddRefs(itemAcc));

    if (itemAcc) {
      PRUint32 itemRole = Role(itemAcc);
      if (itemRole == nsIAccessibleRole::ROLE_SEPARATOR)
        break; // We reached the end of our group.

      PRUint32 itemState = State(itemAcc);
      if (!(itemState & nsIAccessibleStates::STATE_INVISIBLE))
        setSize++;
    }
  }

  // Get level of the item.
  PRInt32 level = -1;
  while (container) {
    level++;

    nsCOMPtr<nsIDOMXULContainerElement> parentContainer;
    container->GetParentContainer(getter_AddRefs(parentContainer));
    parentContainer.swap(container);
  }
  
  SetAccGroupAttrs(aAttributes, level, posInSet, setSize);
}

void
nsAccUtils::SetLiveContainerAttributes(nsIPersistentProperties *aAttributes,
                                       nsIContent *aStartContent,
                                       nsIContent *aTopContent)
{
  nsAutoString atomic, live, relevant, busy;
  nsIContent *ancestor = aStartContent;
  while (ancestor) {
    if (relevant.IsEmpty() &&
        nsAccUtils::HasDefinedARIAToken(ancestor, nsAccessibilityAtoms::aria_relevant) &&
        ancestor->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_relevant, relevant))
      SetAccAttr(aAttributes, nsAccessibilityAtoms::containerRelevant, relevant);

    if (live.IsEmpty() &&
        nsAccUtils::HasDefinedARIAToken(ancestor, nsAccessibilityAtoms::aria_live) &&
        ancestor->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_live, live))
      SetAccAttr(aAttributes, nsAccessibilityAtoms::containerLive, live);

    if (atomic.IsEmpty() &&
        nsAccUtils::HasDefinedARIAToken(ancestor, nsAccessibilityAtoms::aria_atomic) &&
        ancestor->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_atomic, atomic))
      SetAccAttr(aAttributes, nsAccessibilityAtoms::containerAtomic, atomic);

    if (busy.IsEmpty() &&
        nsAccUtils::HasDefinedARIAToken(ancestor, nsAccessibilityAtoms::aria_busy) &&
        ancestor->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_busy, busy))
      SetAccAttr(aAttributes, nsAccessibilityAtoms::containerBusy, busy);

    if (ancestor == aTopContent)
      break;

    ancestor = ancestor->GetParent();
    if (!ancestor)
      ancestor = aTopContent; // Use <body>/<frameset>
  }
}

PRBool
nsAccUtils::IsARIAPropForObjectAttr(nsIAtom *aAtom)
{
  return aAtom != nsAccessibilityAtoms::aria_activedescendant &&
    aAtom != nsAccessibilityAtoms::aria_checked &&
    aAtom != nsAccessibilityAtoms::aria_controls &&
    aAtom != nsAccessibilityAtoms::aria_describedby &&
    aAtom != nsAccessibilityAtoms::aria_disabled &&
    aAtom != nsAccessibilityAtoms::aria_expanded &&
    aAtom != nsAccessibilityAtoms::aria_flowto &&
    aAtom != nsAccessibilityAtoms::aria_invalid &&
    aAtom != nsAccessibilityAtoms::aria_haspopup &&
    aAtom != nsAccessibilityAtoms::aria_labelledby &&
    aAtom != nsAccessibilityAtoms::aria_multiline &&
    aAtom != nsAccessibilityAtoms::aria_multiselectable &&
    aAtom != nsAccessibilityAtoms::aria_owns &&
    aAtom != nsAccessibilityAtoms::aria_pressed &&
    aAtom != nsAccessibilityAtoms::aria_readonly &&
    aAtom != nsAccessibilityAtoms::aria_relevant &&
    aAtom != nsAccessibilityAtoms::aria_required &&
    aAtom != nsAccessibilityAtoms::aria_selected &&
    aAtom != nsAccessibilityAtoms::aria_valuemax &&
    aAtom != nsAccessibilityAtoms::aria_valuemin &&
    aAtom != nsAccessibilityAtoms::aria_valuenow &&
    aAtom != nsAccessibilityAtoms::aria_valuetext;
}

PRBool
nsAccUtils::HasDefinedARIAToken(nsIContent *aContent, nsIAtom *aAtom)
{
  if (!aContent->HasAttr(kNameSpaceID_None, aAtom) ||
      aContent->AttrValueIs(kNameSpaceID_None, aAtom,
                            nsAccessibilityAtoms::_empty, eCaseMatters) ||
      aContent->AttrValueIs(kNameSpaceID_None, aAtom,
                            nsAccessibilityAtoms::_undefined, eCaseMatters)) {
        return PR_FALSE;
  }
  return PR_TRUE;
}

nsresult
nsAccUtils::FireAccEvent(PRUint32 aEventType, nsIAccessible *aAccessible,
                         PRBool aIsAsynch)
{
  NS_ENSURE_ARG(aAccessible);

  nsCOMPtr<nsPIAccessible> pAccessible(do_QueryInterface(aAccessible));
  NS_ASSERTION(pAccessible, "Accessible doesn't implement nsPIAccessible");

  nsCOMPtr<nsIAccessibleEvent> event =
    new nsAccEvent(aEventType, aAccessible, aIsAsynch);
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  return pAccessible->FireAccessibleEvent(event);
}

PRBool
nsAccUtils::HasAccessibleChildren(nsIDOMNode *aNode)
{
  if (!aNode)
    return PR_FALSE;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (!content)
    return PR_FALSE;

  nsCOMPtr<nsIPresShell> presShell = nsCoreUtils::GetPresShellFor(aNode);
  if (!presShell)
    return PR_FALSE;

  nsIFrame *frame = presShell->GetPrimaryFrameFor(content);
  if (!frame)
    return PR_FALSE;
  
  nsCOMPtr<nsIWeakReference> weakShell(do_GetWeakReference(presShell));
  nsAccessibleTreeWalker walker(weakShell, aNode, PR_FALSE);

  walker.mState.frame = frame;

  walker.GetFirstChild();
  return walker.mState.accessible ? PR_TRUE : PR_FALSE;
}

already_AddRefed<nsIAccessible>
nsAccUtils::GetAncestorWithRole(nsIAccessible *aDescendant, PRUint32 aRole)
{
  nsCOMPtr<nsIAccessible> parentAccessible = aDescendant, testRoleAccessible;
  while (NS_SUCCEEDED(parentAccessible->GetParent(getter_AddRefs(testRoleAccessible))) &&
         testRoleAccessible) {
    PRUint32 testRole;
    testRoleAccessible->GetFinalRole(&testRole);
    if (testRole == aRole) {
      nsIAccessible *returnAccessible = testRoleAccessible;
      NS_ADDREF(returnAccessible);
      return returnAccessible;
    }
    nsCOMPtr<nsIAccessibleDocument> docAccessible = do_QueryInterface(testRoleAccessible);
    if (docAccessible) {
      break;
    }
    parentAccessible.swap(testRoleAccessible);
  }
  return nsnull;
}

void
nsAccUtils::GetARIATreeItemParent(nsIAccessible *aStartTreeItem,
                                  nsIContent *aStartContent,
                                  nsIAccessible **aTreeItemParentResult)
{
  *aTreeItemParentResult = nsnull;
  nsAutoString levelStr;
  PRInt32 level = 0;
  if (nsAccUtils::HasDefinedARIAToken(aStartContent, nsAccessibilityAtoms::aria_level) &&
      aStartContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::aria_level, levelStr)) {
    // This is a tree that uses aria-level to define levels, so find the first previous
    // sibling accessible where level is defined to be less than the current level
    PRInt32 success;
    level = levelStr.ToInteger(&success);
    if (level > 1 && NS_SUCCEEDED(success)) {
      nsCOMPtr<nsIAccessible> currentAccessible = aStartTreeItem, prevAccessible;
      while (PR_TRUE) {
        currentAccessible->GetPreviousSibling(getter_AddRefs(prevAccessible));
        currentAccessible.swap(prevAccessible);
        nsCOMPtr<nsIAccessNode> accessNode = do_QueryInterface(currentAccessible);
        if (!accessNode) {
          break; // Reached top of tree, no higher level found
        }
        PRUint32 role;
        currentAccessible->GetFinalRole(&role);
        if (role != nsIAccessibleRole::ROLE_OUTLINEITEM)
          continue;
        nsCOMPtr<nsIDOMNode> treeItemNode;
        accessNode->GetDOMNode(getter_AddRefs(treeItemNode));
        nsCOMPtr<nsIContent> treeItemContent = do_QueryInterface(treeItemNode);
        if (treeItemContent &&
            nsAccUtils::HasDefinedARIAToken(treeItemContent,
                                     nsAccessibilityAtoms::aria_level) &&
            treeItemContent->GetAttr(kNameSpaceID_None,
                                     nsAccessibilityAtoms::aria_level, levelStr)) {
          if (levelStr.ToInteger(&success) < level && NS_SUCCEEDED(success)) {
            NS_ADDREF(*aTreeItemParentResult = currentAccessible);
            return;
          }
        }
      }
    }
  }

  // Possibly a tree arranged by using role="group" to organize levels
  // In this case the parent of the tree item will be a group and the
  // previous sibling of that should be the tree item parent.
  // Or, if the parent is something other than a tree we will return that.
  nsCOMPtr<nsIAccessible> parentAccessible;
  aStartTreeItem->GetParent(getter_AddRefs(parentAccessible));
  if (!parentAccessible)
    return;
  PRUint32 role;
  parentAccessible->GetFinalRole(&role);
  if (role != nsIAccessibleRole::ROLE_GROUPING) {
    NS_ADDREF(*aTreeItemParentResult = parentAccessible);
    return; // The container for the tree items
  }
  nsCOMPtr<nsIAccessible> prevAccessible;
  parentAccessible->GetPreviousSibling(getter_AddRefs(prevAccessible));
  if (!prevAccessible)
    return;
  prevAccessible->GetFinalRole(&role);
  if (role == nsIAccessibleRole::ROLE_TEXT_LEAF) {
    // XXX Sometimes an empty text accessible is in the hierarchy here,
    // although the text does not appear to be rendered, GetRenderedText() says that it is
    // so we need to skip past it to find the true previous sibling
    nsCOMPtr<nsIAccessible> tempAccessible = prevAccessible;
    tempAccessible->GetPreviousSibling(getter_AddRefs(prevAccessible));
    if (!prevAccessible)
      return;
    prevAccessible->GetFinalRole(&role);
  }
  if (role == nsIAccessibleRole::ROLE_OUTLINEITEM) {
    // Previous sibling of parent group is a tree item -- this is the conceptual tree item parent
    NS_ADDREF(*aTreeItemParentResult = prevAccessible);
  }
}

already_AddRefed<nsIAccessibleText>
nsAccUtils::GetTextAccessibleFromSelection(nsISelection *aSelection,
                                           nsIDOMNode **aNode)
{
  // Get accessible from selection's focus DOM point (the DOM point where
  // selection is ended).

  nsCOMPtr<nsIDOMNode> focusNode;
  aSelection->GetFocusNode(getter_AddRefs(focusNode));
  if (!focusNode)
    return nsnull;

  PRInt32 focusOffset = 0;
  aSelection->GetFocusOffset(&focusOffset);

  nsCOMPtr<nsIDOMNode> resultNode =
    nsCoreUtils::GetDOMNodeFromDOMPoint(focusNode, focusOffset);

  nsIAccessibilityService *accService = nsAccessNode::GetAccService();

  // Get text accessible containing the result node.
  while (resultNode) {
    // Make sure to get the correct starting node for selection events inside
    // XBL content trees.
    nsCOMPtr<nsIDOMNode> relevantNode;
    nsresult rv = accService->
      GetRelevantContentNodeFor(resultNode, getter_AddRefs(relevantNode));
    if (NS_FAILED(rv))
      return nsnull;

    if (relevantNode)
      resultNode.swap(relevantNode);

    nsCOMPtr<nsIContent> content = do_QueryInterface(resultNode);
    if (!content || !content->IsNodeOfType(nsINode::eTEXT)) {
      nsCOMPtr<nsIAccessible> accessible;
      accService->GetAccessibleFor(resultNode, getter_AddRefs(accessible));
      if (accessible) {
        nsIAccessibleText *textAcc = nsnull;
        CallQueryInterface(accessible, &textAcc);
        if (textAcc) {
          if (aNode)
            NS_ADDREF(*aNode = resultNode);

          return textAcc;
        }
      }
    }

    nsCOMPtr<nsIDOMNode> parentNode;
    resultNode->GetParentNode(getter_AddRefs(parentNode));
    resultNode.swap(parentNode);
  }

  NS_NOTREACHED("No nsIAccessibleText for selection change event!");

  return nsnull;
}

nsresult
nsAccUtils::ConvertToScreenCoords(PRInt32 aX, PRInt32 aY,
                                  PRUint32 aCoordinateType,
                                  nsIAccessNode *aAccessNode,
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
                                  nsIAccessNode *aAccessNode)
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
nsAccUtils::GetScreenCoordsForWindow(nsIAccessNode *aAccessNode)
{
  nsCOMPtr<nsIDOMNode> DOMNode;
  aAccessNode->GetDOMNode(getter_AddRefs(DOMNode));
  if (DOMNode)
    return nsCoreUtils::GetScreenCoordsForWindow(DOMNode);

  return nsIntPoint(0, 0);
}

nsIntPoint
nsAccUtils::GetScreenCoordsForParent(nsIAccessNode *aAccessNode)
{
  nsRefPtr<nsAccessNode> parent;
  nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(aAccessNode));
  if (accessible) {
    nsCOMPtr<nsIAccessible> parentAccessible;
    accessible->GetParent(getter_AddRefs(parentAccessible));
    parent = nsAccUtils::QueryAccessNode(parentAccessible);
  } else {
    nsCOMPtr<nsIAccessNode> parentAccessNode;
    aAccessNode->GetParentNode(getter_AddRefs(parentAccessNode));
    parent = nsAccUtils::QueryAccessNode(parentAccessNode);
  }

  if (!parent)
    return nsIntPoint(0, 0);

  nsIFrame *parentFrame = parent->GetFrame();
  if (!parentFrame)
    return nsIntPoint(0, 0);

  nsIntRect parentRect = parentFrame->GetScreenRectExternal();
  return nsIntPoint(parentRect.x, parentRect.y);
}

nsRoleMapEntry*
nsAccUtils::GetRoleMapEntry(nsIDOMNode *aNode)
{
  nsIContent *content = nsCoreUtils::GetRoleContent(aNode);
  nsAutoString roleString;
  if (!content || !content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::role, roleString)) {
    return nsnull;
  }

  nsWhitespaceTokenizer tokenizer(roleString);
  while (tokenizer.hasMoreTokens()) {
    // Do a binary search through table for the next role in role list
    const char *role = NS_LossyConvertUTF16toASCII(tokenizer.nextToken()).get();
    PRInt32 low = 0;
    PRInt32 high = nsARIAMap::gWAIRoleMapLength;
    while (low <= high) {
      PRInt32 index = low + ((high - low) / 2);
      PRInt32 compare = PL_strcmp(role, nsARIAMap::gWAIRoleMap[index].roleString);
      if (compare == 0) {
        // The  role attribute maps to an entry in the role table
        return &nsARIAMap::gWAIRoleMap[index];
      }
      if (compare < 0) {
        high = index - 1;
      }
      else {
        low = index + 1;
      }
    }
  }

  // Always use some entry if there is a role string
  // To ensure an accessible object is created
  return &nsARIAMap::gLandmarkRoleMap;
}


#ifdef DEBUG_A11Y

PRBool
nsAccUtils::IsTextInterfaceSupportCorrect(nsIAccessible *aAccessible)
{
  PRBool foundText = PR_FALSE;
  
  nsCOMPtr<nsIAccessibleDocument> accDoc = do_QueryInterface(aAccessible);
  if (accDoc) {
    // Don't test for accessible docs, it makes us create accessibles too
    // early and fire mutation events before we need to
    return PR_TRUE;
  }

  nsCOMPtr<nsIAccessible> child, nextSibling;
  aAccessible->GetFirstChild(getter_AddRefs(child));
  while (child) {
    if (IsText(child)) {
      foundText = PR_TRUE;
      break;
    }
    child->GetNextSibling(getter_AddRefs(nextSibling));
    child.swap(nextSibling);
  }

  if (foundText) {
    // found text child node
    nsCOMPtr<nsIAccessibleText> text = do_QueryInterface(aAccessible);
    if (!text)
      return PR_FALSE;
  }

  return PR_TRUE; 
}
#endif

PRInt32
nsAccUtils::TextLength(nsIAccessible *aAccessible)
{
  if (!IsText(aAccessible))
    return 1;
  
  nsRefPtr<nsAccessNode> accNode = nsAccUtils::QueryAccessNode(aAccessible);
  
  nsIFrame *frame = accNode->GetFrame();
  if (frame && frame->GetType() == nsAccessibilityAtoms::textFrame) {
    // Ensure that correct text length is calculated (with non-rendered
    // whitespace chars not counted).
    nsIContent *content = frame->GetContent();
    if (content) {
      PRUint32 length;
      nsresult rv = nsHyperTextAccessible::
        ContentToRenderedOffset(frame, content->TextLength(), &length);
      return NS_SUCCEEDED(rv) ? static_cast<PRInt32>(length) : -1;
    }
  }
  
  // For list bullets (or anything other accessible which would compute its own
  // text. They don't have their own frame.
  // XXX In the future, list bullets may have frame and anon content, so 
  // we should be able to remove this at that point
  nsCOMPtr<nsPIAccessible> pAcc(do_QueryInterface(aAccessible));
  
  nsAutoString text;
  pAcc->AppendTextTo(text, 0, PR_UINT32_MAX); // Get all the text
  return text.Length();
}

PRBool
nsAccUtils::MustPrune(nsIAccessible *aAccessible)
{ 
  PRUint32 role = nsAccUtils::Role(aAccessible);

  return role == nsIAccessibleRole::ROLE_MENUITEM || 
    role == nsIAccessibleRole::ROLE_COMBOBOX_OPTION ||
    role == nsIAccessibleRole::ROLE_OPTION ||
    role == nsIAccessibleRole::ROLE_ENTRY ||
    role == nsIAccessibleRole::ROLE_FLAT_EQUATION ||
    role == nsIAccessibleRole::ROLE_PASSWORD_TEXT ||
    role == nsIAccessibleRole::ROLE_PUSHBUTTON ||
    role == nsIAccessibleRole::ROLE_TOGGLE_BUTTON ||
    role == nsIAccessibleRole::ROLE_GRAPHIC ||
    role == nsIAccessibleRole::ROLE_SLIDER ||
    role == nsIAccessibleRole::ROLE_PROGRESSBAR ||
    role == nsIAccessibleRole::ROLE_SEPARATOR;
}

PRBool
nsAccUtils::IsNodeRelevant(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMNode> relevantNode;
  nsAccessNode::GetAccService()->GetRelevantContentNodeFor(aNode,
                                                           getter_AddRefs(relevantNode));
  return aNode == relevantNode;
}

already_AddRefed<nsIAccessible>
nsAccUtils::GetMultiSelectFor(nsIDOMNode *aNode)
{
  if (!aNode)
    return nsnull;

  nsCOMPtr<nsIAccessible> accessible;
  nsAccessNode::GetAccService()->GetAccessibleFor(aNode,
                                                  getter_AddRefs(accessible));
  if (!accessible)
    return nsnull;

  PRUint32 state = State(accessible);
  if (0 == (state & nsIAccessibleStates::STATE_SELECTABLE))
    return nsnull;

  PRUint32 containerRole;
  while (0 == (state & nsIAccessibleStates::STATE_MULTISELECTABLE)) {
    nsIAccessible *current = accessible;
    current->GetParent(getter_AddRefs(accessible));
    if (!accessible || (NS_SUCCEEDED(accessible->GetFinalRole(&containerRole)) &&
                        containerRole == nsIAccessibleRole::ROLE_PANE)) {
      return nsnull;
    }
    state = State(accessible);
  }

  nsIAccessible *returnAccessible = nsnull;
  accessible.swap(returnAccessible);
  return returnAccessible;
}
