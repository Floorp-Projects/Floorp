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

#include "nsAccessibilityUtils.h"

#include "nsIAccessibleStates.h"
#include "nsIAccessibleTypes.h"
#include "nsPIAccessible.h"
#include "nsPIAccessNode.h"
#include "nsAccessibleEventData.h"

#include "nsAccessible.h"
#include "nsARIAMap.h"
#include "nsIDocument.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOM3Node.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMWindowInternal.h"
#include "nsIEventListenerManager.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIScrollableFrame.h"
#include "nsIEventStateManager.h"
#include "nsISelection2.h"
#include "nsISelectionController.h"

#include "nsContentCID.h"
#include "nsComponentManagerUtils.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsWhitespaceTokenizer.h"

static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

void
nsAccUtils::GetAccAttr(nsIPersistentProperties *aAttributes, nsIAtom *aAttrName,
                       nsAString& aAttrValue)
{
  aAttrValue.Truncate();

  nsCAutoString attrName;
  aAttrName->ToUTF8String(attrName);
  aAttributes->GetStringProperty(attrName, aAttrValue);
}

void
nsAccUtils::SetAccAttr(nsIPersistentProperties *aAttributes, nsIAtom *aAttrName,
                       const nsAString& aAttrValue)
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
        nsAccessible::State(itemAcc) & nsIAccessibleStates::STATE_INVISIBLE) {
      setSize--;
      if (index < static_cast<PRUint32>(indexOf))
        posInSet--;
    }
  }

  SetAccGroupAttrs(aAttributes, 0, posInSet + 1, setSize);
}

PRBool
nsAccUtils::HasListener(nsIContent *aContent, const nsAString& aEventType)
{
  NS_ENSURE_ARG_POINTER(aContent);
  nsCOMPtr<nsIEventListenerManager> listenerManager;
  aContent->GetListenerManager(PR_FALSE, getter_AddRefs(listenerManager));

  return listenerManager && listenerManager->HasListenersFor(aEventType);  
}

PRUint32
nsAccUtils::GetAccessKeyFor(nsIContent *aContent)
{
  if (!aContent)
    return 0;

  // Accesskeys are registered by @accesskey attribute only. At first check
  // whether it is presented on the given element to avoid the slow
  // nsIEventStateManager::GetRegisteredAccessKey() method.
  if (!aContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::accesskey))
    return 0;

  nsCOMPtr<nsIDocument> doc = aContent->GetOwnerDoc();
  if (!doc)
    return 0;

  nsCOMPtr<nsIPresShell> presShell = doc->GetPrimaryShell();
  if (!presShell)
    return 0;

  nsPresContext *presContext = presShell->GetPresContext();
  if (!presContext)
    return 0;

  nsIEventStateManager *esm = presContext->EventStateManager();
  if (!esm)
    return 0;

  PRUint32 key = 0;
  esm->GetRegisteredAccessKey(aContent, &key);
  return key;
}

nsresult
nsAccUtils::FireAccEvent(PRUint32 aEventType, nsIAccessible *aAccessible,
                         PRBool aIsAsynch)
{
  NS_ENSURE_ARG(aAccessible);

  nsCOMPtr<nsPIAccessible> pAccessible(do_QueryInterface(aAccessible));
  NS_ASSERTION(pAccessible, "Accessible doesn't implement nsPIAccessible");

  nsCOMPtr<nsIAccessibleEvent> event =
    new nsAccEvent(aEventType, aAccessible, nsnull, aIsAsynch);
  NS_ENSURE_TRUE(event, NS_ERROR_OUT_OF_MEMORY);

  return pAccessible->FireAccessibleEvent(event);
}

PRBool
nsAccUtils::IsAncestorOf(nsIDOMNode *aPossibleAncestorNode,
                         nsIDOMNode *aPossibleDescendantNode)
{
  NS_ENSURE_ARG_POINTER(aPossibleAncestorNode);
  NS_ENSURE_ARG_POINTER(aPossibleDescendantNode);

  nsCOMPtr<nsIDOMNode> loopNode = aPossibleDescendantNode;
  nsCOMPtr<nsIDOMNode> parentNode;
  while (NS_SUCCEEDED(loopNode->GetParentNode(getter_AddRefs(parentNode))) &&
         parentNode) {
    if (parentNode == aPossibleAncestorNode) {
      return PR_TRUE;
    }
    loopNode.swap(parentNode);
  }
  return PR_FALSE;
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

nsresult
nsAccUtils::ScrollSubstringTo(nsIFrame *aFrame,
                              nsIDOMNode *aStartNode, PRInt32 aStartIndex,
                              nsIDOMNode *aEndNode, PRInt32 aEndIndex,
                              PRUint32 aScrollType)
{
  PRInt16 vPercent, hPercent;
  ConvertScrollTypeToPercents(aScrollType, &vPercent, &hPercent);

  return ScrollSubstringTo(aFrame, aStartNode, aStartIndex, aEndNode, aEndIndex,
                           vPercent, hPercent);
}

nsresult
nsAccUtils::ScrollSubstringTo(nsIFrame *aFrame,
                              nsIDOMNode *aStartNode, PRInt32 aStartIndex,
                              nsIDOMNode *aEndNode, PRInt32 aEndIndex,
                              PRInt16 aVPercent, PRInt16 aHPercent)
{
  if (!aFrame || !aStartNode || !aEndNode)
    return NS_ERROR_FAILURE;

  nsPresContext *presContext = aFrame->PresContext();

  nsCOMPtr<nsIDOMRange> scrollToRange = do_CreateInstance(kRangeCID);
  NS_ENSURE_TRUE(scrollToRange, NS_ERROR_FAILURE);

  nsCOMPtr<nsISelectionController> selCon;
  aFrame->GetSelectionController(presContext, getter_AddRefs(selCon));
  NS_ENSURE_TRUE(selCon, NS_ERROR_FAILURE);

  scrollToRange->SetStart(aStartNode, aStartIndex);
  scrollToRange->SetEnd(aEndNode, aEndIndex);

  nsCOMPtr<nsISelection> selection1;
  selCon->GetSelection(nsISelectionController::SELECTION_ACCESSIBILITY,
                       getter_AddRefs(selection1));

  nsCOMPtr<nsISelection2> selection(do_QueryInterface(selection1));
  if (selection) {
    selection->RemoveAllRanges();
    selection->AddRange(scrollToRange);

    selection->ScrollIntoView(nsISelectionController::SELECTION_ANCHOR_REGION,
                              PR_TRUE, aVPercent, aHPercent);

    selection->CollapseToStart();
  }

  return NS_OK;
}

void
nsAccUtils::ScrollFrameToPoint(nsIFrame *aScrollableFrame,
                               nsIFrame *aFrame,
                               const nsIntPoint& aPoint)
{
  nsIScrollableFrame *scrollableFrame = nsnull;
  CallQueryInterface(aScrollableFrame, &scrollableFrame);
  if (!scrollableFrame)
    return;

  nsPresContext *presContext = aFrame->PresContext();

  nsIntRect frameRect = aFrame->GetScreenRectExternal();
  PRInt32 devDeltaX = aPoint.x - frameRect.x;
  PRInt32 devDeltaY = aPoint.y - frameRect.y;

  nsPoint deltaPoint;
  deltaPoint.x = presContext->DevPixelsToAppUnits(devDeltaX);
  deltaPoint.y = presContext->DevPixelsToAppUnits(devDeltaY);

  nsPoint scrollPoint = scrollableFrame->GetScrollPosition();
  scrollPoint -= deltaPoint;

  scrollableFrame->ScrollTo(scrollPoint);
}

void
nsAccUtils::ConvertScrollTypeToPercents(PRUint32 aScrollType,
                                        PRInt16 *aVPercent,
                                        PRInt16 *aHPercent)
{
  switch (aScrollType)
  {
    case nsIAccessibleScrollType::SCROLL_TYPE_TOP_LEFT:
      *aVPercent = NS_PRESSHELL_SCROLL_TOP;
      *aHPercent = NS_PRESSHELL_SCROLL_LEFT;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_BOTTOM_RIGHT:
      *aVPercent = NS_PRESSHELL_SCROLL_BOTTOM;
      *aHPercent = NS_PRESSHELL_SCROLL_RIGHT;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_TOP_EDGE:
      *aVPercent = NS_PRESSHELL_SCROLL_TOP;
      *aHPercent = NS_PRESSHELL_SCROLL_ANYWHERE;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_BOTTOM_EDGE:
      *aVPercent = NS_PRESSHELL_SCROLL_BOTTOM;
      *aHPercent = NS_PRESSHELL_SCROLL_ANYWHERE;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_LEFT_EDGE:
      *aVPercent = NS_PRESSHELL_SCROLL_ANYWHERE;
      *aHPercent = NS_PRESSHELL_SCROLL_LEFT;
      break;
    case nsIAccessibleScrollType::SCROLL_TYPE_RIGHT_EDGE:
      *aVPercent = NS_PRESSHELL_SCROLL_ANYWHERE;
      *aHPercent = NS_PRESSHELL_SCROLL_RIGHT;
      break;
    default:
      *aVPercent = NS_PRESSHELL_SCROLL_ANYWHERE;
      *aHPercent = NS_PRESSHELL_SCROLL_ANYWHERE;
  }
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
      nsIntPoint coords = GetScreenCoordsForWindow(aAccessNode);
      *aX -= coords.x;
      *aY -= coords.y;
      break;
    }

    case nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE:
    {
      NS_ENSURE_ARG(aAccessNode);
      nsIntPoint coords = GetScreenCoordsForParent(aAccessNode);
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
nsAccUtils::GetScreenCoordsForWindow(nsIDOMNode *aNode)
{
  nsIntPoint coords(0, 0);
  nsCOMPtr<nsIDocShellTreeItem> treeItem(GetDocShellTreeItemFor(aNode));
  if (!treeItem)
    return coords;

  nsCOMPtr<nsIDocShellTreeItem> rootTreeItem;
  treeItem->GetRootTreeItem(getter_AddRefs(rootTreeItem));
  nsCOMPtr<nsIDOMDocument> domDoc = do_GetInterface(rootTreeItem);
  nsCOMPtr<nsIDOMDocumentView> docView(do_QueryInterface(domDoc));
  if (!docView)
    return coords;

  nsCOMPtr<nsIDOMAbstractView> abstractView;
  docView->GetDefaultView(getter_AddRefs(abstractView));
  nsCOMPtr<nsIDOMWindowInternal> windowInter(do_QueryInterface(abstractView));
  if (!windowInter)
    return coords;

  windowInter->GetScreenX(&coords.x);
  windowInter->GetScreenY(&coords.y);
  return coords;
}

nsIntPoint
nsAccUtils::GetScreenCoordsForWindow(nsIAccessNode *aAccessNode)
{
  nsCOMPtr<nsIDOMNode> DOMNode;
  aAccessNode->GetDOMNode(getter_AddRefs(DOMNode));
  if (DOMNode)
    return GetScreenCoordsForWindow(DOMNode);

  return nsIntPoint(0, 0);
}

nsIntPoint
nsAccUtils::GetScreenCoordsForParent(nsIAccessNode *aAccessNode)
{
  nsCOMPtr<nsPIAccessNode> parent;
  nsCOMPtr<nsIAccessible> accessible(do_QueryInterface(aAccessNode));
  if (accessible) {
    nsCOMPtr<nsIAccessible> parentAccessible;
    accessible->GetParent(getter_AddRefs(parentAccessible));
    parent = do_QueryInterface(parentAccessible);
  } else {
    nsCOMPtr<nsIAccessNode> parentAccessNode;
    aAccessNode->GetParentNode(getter_AddRefs(parentAccessNode));
    parent = do_QueryInterface(parentAccessNode);
  }

  if (!parent)
    return nsIntPoint(0, 0);

  nsIFrame *parentFrame = parent->GetFrame();
  if (!parentFrame)
    return nsIntPoint(0, 0);

  nsIntRect parentRect = parentFrame->GetScreenRectExternal();
  return nsIntPoint(parentRect.x, parentRect.y);
}

already_AddRefed<nsIDocShellTreeItem>
nsAccUtils::GetDocShellTreeItemFor(nsIDOMNode *aNode)
{
  if (!aNode)
    return nsnull;

  nsCOMPtr<nsIDOMDocument> domDoc;
  aNode->GetOwnerDocument(getter_AddRefs(domDoc));
  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
  if (!doc)
    doc = do_QueryInterface(aNode);

  NS_ASSERTION(doc, "No document for node passed in");
  NS_ENSURE_TRUE(doc, nsnull);

  nsCOMPtr<nsISupports> container = doc->GetContainer();
  nsIDocShellTreeItem *docShellTreeItem = nsnull;
  if (container)
    CallQueryInterface(container, &docShellTreeItem);

  return docShellTreeItem;
}

PRBool
nsAccUtils::GetID(nsIContent *aContent, nsAString& aID)
{
  nsIAtom *idAttribute = aContent->GetIDAttributeName();
  return idAttribute ? aContent->GetAttr(kNameSpaceID_None, idAttribute, aID) : PR_FALSE;
}

nsIContent*
nsAccUtils::FindNeighbourPointingToNode(nsIContent *aForNode, 
                                        nsIAtom *aRelationAttr,
                                        nsIAtom *aTagName,
                                        PRUint32 aAncestorLevelsToSearch)
{
  nsCOMPtr<nsIContent> binding;
  nsAutoString controlID;
  if (!nsAccUtils::GetID(aForNode, controlID)) {
    binding = aForNode->GetBindingParent();
    if (binding == aForNode)
      return nsnull;

    aForNode->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::anonid, controlID);
    if (controlID.IsEmpty())
      return nsnull;
  }

  // Look for label in subtrees of nearby ancestors
  PRUint32 count = 0;
  nsIContent *labelContent = nsnull;
  nsIContent *prevSearched = nsnull;

  while (!labelContent && ++count <= aAncestorLevelsToSearch &&
         (aForNode = aForNode->GetParent()) != nsnull) {

    if (aForNode == binding) {
      // When we reach the binding parent, make sure to check
      // all of its anonymous child subtrees
      nsCOMPtr<nsIDocument> doc = aForNode->GetCurrentDoc();
      nsCOMPtr<nsIDOMDocumentXBL> xblDoc(do_QueryInterface(doc));
      if (!xblDoc)
        return nsnull;

      nsCOMPtr<nsIDOMNodeList> nodes;
      nsCOMPtr<nsIDOMElement> forElm(do_QueryInterface(aForNode));
      xblDoc->GetAnonymousNodes(forElm, getter_AddRefs(nodes));
      if (!nodes)
        return nsnull;

      PRUint32 length;
      nsresult rv = nodes->GetLength(&length);
      if (NS_FAILED(rv))
        return nsnull;

      for (PRUint32 index = 0; index < length && !labelContent; index++) {
        nsCOMPtr<nsIDOMNode> node;
        rv = nodes->Item(index, getter_AddRefs(node));
        if (NS_FAILED(rv))
          return nsnull;

        nsCOMPtr<nsIContent> content = do_QueryInterface(node);
        if (!content)
          return nsnull;

        if (content != prevSearched) {
          labelContent = FindDescendantPointingToID(&controlID, content,
                                                    aRelationAttr, nsnull, aTagName);
        }
      }
      break;
    }

    labelContent = FindDescendantPointingToID(&controlID, aForNode,
                                              aRelationAttr, prevSearched, aTagName);
    prevSearched = aForNode;
  }

  return labelContent;
}

// Pass in aAriaProperty = null and aRelationAttr == nsnull if any <label> will do
nsIContent*
nsAccUtils::FindDescendantPointingToID(const nsString *aId,
                                       nsIContent *aLookContent,
                                       nsIAtom *aRelationAttr,
                                       nsIContent *aExcludeContent,
                                       nsIAtom *aTagType)
{
  // Surround id with spaces for search
  nsCAutoString idWithSpaces(' ');
  LossyAppendUTF16toASCII(*aId, idWithSpaces);
  idWithSpaces += ' ';
  return FindDescendantPointingToIDImpl(idWithSpaces, aLookContent,
                                        aRelationAttr, aExcludeContent, aTagType);
}

nsIContent*
nsAccUtils::FindDescendantPointingToIDImpl(nsCString& aIdWithSpaces,
                                           nsIContent *aLookContent,
                                           nsIAtom *aRelationAttr,
                                           nsIContent *aExcludeContent,
                                           nsIAtom *aTagType)
{
  NS_ENSURE_TRUE(aLookContent, nsnull);
  NS_ENSURE_TRUE(aRelationAttr, nsnull);

  if (!aTagType || aLookContent->Tag() == aTagType) {
    // Tag matches
    // Check for ID in the attribute aRelationAttr, which can be a list
    nsAutoString idList;
    if (aLookContent->GetAttr(kNameSpaceID_None, aRelationAttr, idList)) {
      idList.Insert(' ', 0);  // Surround idlist with spaces for search
      idList.Append(' ');
      // idList is now a set of id's with spaces around each,
      // and id also has spaces around it.
      // If id is a substring of idList then we have a match
      if (idList.Find(aIdWithSpaces) != -1) {
        return aLookContent;
      }
    }
    if (aTagType) {
      // Don't bother to search descendants of an element with matching tag.
      // That would be like looking for a nested <label> or <description>
      return nsnull;
    }
  }

  // Recursively search descendants for match
  PRUint32 count  = 0;
  nsIContent *child;
  nsIContent *labelContent = nsnull;

  while ((child = aLookContent->GetChildAt(count++)) != nsnull) {
    if (child != aExcludeContent) {
      labelContent = FindDescendantPointingToIDImpl(aIdWithSpaces, child,
                                                    aRelationAttr, aExcludeContent, aTagType);
      if (labelContent) {
        return labelContent;
      }
    }
  }
  return nsnull;
}

nsRoleMapEntry*
nsAccUtils::GetRoleMapEntry(nsIDOMNode *aNode)
{
  nsIContent *content = nsAccessible::GetRoleContent(aNode);
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

