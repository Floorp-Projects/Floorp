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

#include "nsIAccessibleTypes.h"

#include "nsAccessNode.h"

#include "nsIDocument.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOM3Node.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMDocumentXBL.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMRange.h"
#include "nsIDOMViewCSS.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMXULElement.h"
#include "nsIDocShell.h"
#include "nsIDocumentViewer.h"
#include "nsIContentViewer.h"
#include "nsIEventListenerManager.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIScrollableFrame.h"
#include "nsIEventStateManager.h"
#include "nsISelection2.h"
#include "nsISelectionController.h"
#include "nsPIDOMWindow.h"
#include "nsGUIEvent.h"

#include "nsContentCID.h"
#include "nsComponentManagerUtils.h"
#include "nsIInterfaceRequestorUtils.h"

static NS_DEFINE_IID(kRangeCID, NS_RANGE_CID);

PRBool
nsCoreUtils::HasListener(nsIContent *aContent, const nsAString& aEventType)
{
  NS_ENSURE_TRUE(aContent, PR_FALSE);
  nsIEventListenerManager* listenerManager =
    aContent->GetListenerManager(PR_FALSE);

  return listenerManager && listenerManager->HasListenersFor(aEventType);  
}

void
nsCoreUtils::DispatchClickEvent(nsITreeBoxObject *aTreeBoxObj,
                                PRInt32 aRowIndex, nsITreeColumn *aColumn,
                                const nsCString& aPseudoElt)
{
  nsCOMPtr<nsIDOMElement> tcElm;
  aTreeBoxObj->GetTreeBody(getter_AddRefs(tcElm));
  if (!tcElm)
    return;

  nsCOMPtr<nsIContent> tcContent(do_QueryInterface(tcElm));
  nsIDocument *document = tcContent->GetCurrentDoc();
  if (!document)
    return;

  nsIPresShell *presShell = nsnull;
  presShell = document->GetPrimaryShell();
  if (!presShell)
    return;

  // Ensure row is visible.
  aTreeBoxObj->EnsureRowIsVisible(aRowIndex);

  // Calculate x and y coordinates.
  PRInt32 x = 0, y = 0, width = 0, height = 0;
  nsresult rv = aTreeBoxObj->GetCoordsForCellItem(aRowIndex, aColumn,
                                                  aPseudoElt,
                                                  &x, &y, &width, &height);
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIDOMXULElement> tcXULElm(do_QueryInterface(tcElm));
  nsCOMPtr<nsIBoxObject> tcBoxObj;
  tcXULElm->GetBoxObject(getter_AddRefs(tcBoxObj));

  PRInt32 tcX = 0;
  tcBoxObj->GetX(&tcX);

  PRInt32 tcY = 0;
  tcBoxObj->GetY(&tcY);

  // Dispatch mouse events.
  nsIFrame* tcFrame = presShell->GetPrimaryFrameFor(tcContent);  
  nsIFrame* rootFrame = presShell->GetRootFrame();

  nsPoint offset;
  nsIWidget *rootWidget =
    rootFrame->GetViewExternal()->GetNearestWidget(&offset);

  nsPresContext* presContext = presShell->GetPresContext();

  PRInt32 cnvdX = presContext->CSSPixelsToDevPixels(tcX + x + 1) +
    presContext->AppUnitsToDevPixels(offset.x);
  PRInt32 cnvdY = presContext->CSSPixelsToDevPixels(tcY + y + 1) +
    presContext->AppUnitsToDevPixels(offset.y);

  DispatchMouseEvent(NS_MOUSE_BUTTON_DOWN, cnvdX, cnvdY,
                     tcContent, tcFrame, presShell, rootWidget);

  DispatchMouseEvent(NS_MOUSE_BUTTON_UP, cnvdX, cnvdY,
                     tcContent, tcFrame, presShell, rootWidget);
}

PRBool
nsCoreUtils::DispatchMouseEvent(PRUint32 aEventType,
                                nsIPresShell *aPresShell,
                                nsIContent *aContent)
{
  nsIFrame *frame = aPresShell->GetPrimaryFrameFor(aContent);
  if (!frame)
    return PR_FALSE;

  nsIFrame* rootFrame = aPresShell->GetRootFrame();
  if (!rootFrame)
    return PR_FALSE;

  nsCOMPtr<nsIWidget> rootWidget = rootFrame->GetWindow();
  if (!rootWidget)
    return PR_FALSE;

  // Compute x and y coordinates.
  nsPoint point = frame->GetOffsetToExternal(rootFrame);
  nsSize size = frame->GetSize();

  nsPresContext* presContext = aPresShell->GetPresContext();

  PRInt32 x = presContext->AppUnitsToDevPixels(point.x + size.width / 2);
  PRInt32 y = presContext->AppUnitsToDevPixels(point.y + size.height / 2);

  // Fire mouse event.
  DispatchMouseEvent(aEventType, x, y, aContent, frame, aPresShell, rootWidget);
  return PR_TRUE;
}

void
nsCoreUtils::DispatchMouseEvent(PRUint32 aEventType, PRInt32 aX, PRInt32 aY,
                                nsIContent *aContent, nsIFrame *aFrame,
                                nsIPresShell *aPresShell, nsIWidget *aRootWidget)
{
  nsMouseEvent event(PR_TRUE, aEventType, aRootWidget,
                     nsMouseEvent::eReal, nsMouseEvent::eNormal);

  event.refPoint = nsIntPoint(aX, aY);

  event.clickCount = 1;
  event.button = nsMouseEvent::eLeftButton;
  event.time = PR_IntervalNow();

  nsEventStatus status = nsEventStatus_eIgnore;
  aPresShell->HandleEventWithTarget(&event, aFrame, aContent, &status);
}

PRUint32
nsCoreUtils::GetAccessKeyFor(nsIContent *aContent)
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

already_AddRefed<nsIDOMElement>
nsCoreUtils::GetDOMElementFor(nsIDOMNode *aNode)
{
  nsCOMPtr<nsINode> node(do_QueryInterface(aNode));
  nsIDOMElement *element = nsnull;

  if (node->IsNodeOfType(nsINode::eELEMENT))
    CallQueryInterface(node, &element);

  else if (node->IsNodeOfType(nsINode::eTEXT)) {
    nsCOMPtr<nsINode> nodeParent = node->GetNodeParent();
    NS_ASSERTION(nodeParent, "Text node has no parent!");
    if (nodeParent)
      CallQueryInterface(nodeParent, &element);
  }

  else if (node->IsNodeOfType(nsINode::eDOCUMENT)) {
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(node));
    if (htmlDoc) {
      nsCOMPtr<nsIDOMHTMLElement> bodyElement;
      htmlDoc->GetBody(getter_AddRefs(bodyElement));
      if (bodyElement) {
        CallQueryInterface(bodyElement, &element);
        return element;
      }
    }

    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(node));
    domDoc->GetDocumentElement(&element);
  }

  return element;
}

already_AddRefed<nsIDOMNode>
nsCoreUtils::GetDOMNodeFromDOMPoint(nsIDOMNode *aNode, PRUint32 aOffset)
{
  nsIDOMNode *resultNode = nsnull;

  nsCOMPtr<nsIContent> content(do_QueryInterface(aNode));
  if (content && content->IsNodeOfType(nsINode::eELEMENT)) {

    PRUint32 childCount = content->GetChildCount();
    NS_ASSERTION(aOffset >= 0 && aOffset <= childCount,
                 "Wrong offset of the DOM point!");

    // The offset can be after last child of container node that means DOM point
    // is placed immediately after the last child. In this case use the DOM node
    // from the given DOM point is used as result node.
    if (aOffset != childCount) {
      CallQueryInterface(content->GetChildAt(aOffset), &resultNode);
      return resultNode;
    }
  }

  NS_IF_ADDREF(resultNode = aNode);
  return resultNode;
}

nsIContent*
nsCoreUtils::GetRoleContent(nsIDOMNode *aDOMNode)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(aDOMNode));
  if (!content) {
    nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(aDOMNode));
    if (domDoc) {
      nsCOMPtr<nsIDOMHTMLDocument> htmlDoc(do_QueryInterface(aDOMNode));
      if (htmlDoc) {
        nsCOMPtr<nsIDOMHTMLElement> bodyElement;
        htmlDoc->GetBody(getter_AddRefs(bodyElement));
        content = do_QueryInterface(bodyElement);
      }
      else {
        nsCOMPtr<nsIDOMElement> docElement;
        domDoc->GetDocumentElement(getter_AddRefs(docElement));
        content = do_QueryInterface(docElement);
      }
    }
  }

  return content;
}

PRBool
nsCoreUtils::IsAncestorOf(nsIDOMNode *aPossibleAncestorNode,
                          nsIDOMNode *aPossibleDescendantNode)
{
  NS_ENSURE_TRUE(aPossibleAncestorNode && aPossibleDescendantNode, PR_FALSE);

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

PRBool
nsCoreUtils::AreSiblings(nsIDOMNode *aDOMNode1,
                        nsIDOMNode *aDOMNode2)
{
  NS_ENSURE_TRUE(aDOMNode1 && aDOMNode2, PR_FALSE);

  nsCOMPtr<nsIDOMNode> parentNode1, parentNode2;
  if (NS_SUCCEEDED(aDOMNode1->GetParentNode(getter_AddRefs(parentNode1))) &&
      NS_SUCCEEDED(aDOMNode2->GetParentNode(getter_AddRefs(parentNode2))) &&
      parentNode1 == parentNode2) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

nsresult
nsCoreUtils::ScrollSubstringTo(nsIFrame *aFrame,
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
nsCoreUtils::ScrollSubstringTo(nsIFrame *aFrame,
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
nsCoreUtils::ScrollFrameToPoint(nsIFrame *aScrollableFrame,
                                nsIFrame *aFrame,
                                const nsIntPoint& aPoint)
{
  nsIScrollableFrame *scrollableFrame = do_QueryFrame(aScrollableFrame);
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
nsCoreUtils::ConvertScrollTypeToPercents(PRUint32 aScrollType,
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

nsIntPoint
nsCoreUtils::GetScreenCoordsForWindow(nsIDOMNode *aNode)
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

already_AddRefed<nsIDocShellTreeItem>
nsCoreUtils::GetDocShellTreeItemFor(nsIDOMNode *aNode)
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

nsIFrame*
nsCoreUtils::GetFrameFor(nsIDOMElement *aElm)
{
  nsCOMPtr<nsIPresShell> shell = GetPresShellFor(aElm);
  if (!shell)
    return nsnull;
  
  nsCOMPtr<nsIContent> content(do_QueryInterface(aElm));
  if (!content)
    return nsnull;
  
  return shell->GetPrimaryFrameFor(content);
}

PRBool
nsCoreUtils::IsCorrectFrameType(nsIFrame *aFrame, nsIAtom *aAtom)
{
  NS_ASSERTION(aFrame != nsnull,
               "aFrame is null in call to IsCorrectFrameType!");
  NS_ASSERTION(aAtom != nsnull,
               "aAtom is null in call to IsCorrectFrameType!");
  
  return aFrame->GetType() == aAtom;
}

already_AddRefed<nsIPresShell>
nsCoreUtils::GetPresShellFor(nsIDOMNode *aNode)
{
  nsCOMPtr<nsIDOMDocument> domDocument;
  aNode->GetOwnerDocument(getter_AddRefs(domDocument));

  nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDocument));
  if (!doc) // This is necessary when the node is the document node
    doc = do_QueryInterface(aNode);

  nsIPresShell *presShell = nsnull;
  if (doc) {
    presShell = doc->GetPrimaryShell();
    NS_IF_ADDREF(presShell);
  }

  return presShell;
}

already_AddRefed<nsIDOMNode>
nsCoreUtils::GetDOMNodeForContainer(nsIDocShellTreeItem *aContainer)
{
  nsCOMPtr<nsIDocShell> shell = do_QueryInterface(aContainer);

  nsCOMPtr<nsIContentViewer> cv;
  shell->GetContentViewer(getter_AddRefs(cv));

  if (!cv)
    return nsnull;

  nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
  if (!docv)
    return nsnull;

  nsCOMPtr<nsIDocument> doc;
  docv->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return nsnull;

  nsIDOMNode* node = nsnull;
  CallQueryInterface(doc.get(), &node);
  return node;
}

PRBool
nsCoreUtils::GetID(nsIContent *aContent, nsAString& aID)
{
  nsIAtom *idAttribute = aContent->GetIDAttributeName();
  return idAttribute ? aContent->GetAttr(kNameSpaceID_None, idAttribute, aID) : PR_FALSE;
}

PRBool
nsCoreUtils::IsXLink(nsIContent *aContent)
{
  if (!aContent)
    return PR_FALSE;

  return aContent->AttrValueIs(kNameSpaceID_XLink, nsAccessibilityAtoms::type,
                               nsAccessibilityAtoms::simple, eCaseMatters) &&
         aContent->HasAttr(kNameSpaceID_XLink, nsAccessibilityAtoms::href);
}

nsIContent*
nsCoreUtils::FindNeighbourPointingToNode(nsIContent *aForNode, 
                                         nsIAtom *aRelationAttr,
                                         nsIAtom *aTagName,
                                         PRUint32 aAncestorLevelsToSearch)
{
  return FindNeighbourPointingToNode(aForNode, &aRelationAttr, 1, aTagName, aAncestorLevelsToSearch);
}

nsIContent*
nsCoreUtils::FindNeighbourPointingToNode(nsIContent *aForNode, 
                                         nsIAtom **aRelationAttrs,
                                         PRUint32 aAttrNum,
                                         nsIAtom *aTagName,
                                         PRUint32 aAncestorLevelsToSearch)
{
  nsAutoString controlID;
  if (!nsCoreUtils::GetID(aForNode, controlID)) {
    if (!aForNode->IsInAnonymousSubtree())
      return nsnull;

    aForNode->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::anonid, controlID);
    if (controlID.IsEmpty())
      return nsnull;
  }

  // Look for label in subtrees of nearby ancestors
  nsCOMPtr<nsIContent> binding(aForNode->GetBindingParent());
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
                                                    aRelationAttrs, aAttrNum,
                                                    nsnull, aTagName);
        }
      }
      break;
    }

    labelContent = FindDescendantPointingToID(&controlID, aForNode,
                                              aRelationAttrs, aAttrNum,
                                              prevSearched, aTagName);
    prevSearched = aForNode;
  }

  return labelContent;
}

// Pass in aAriaProperty = null and aRelationAttr == nsnull if any <label> will do
nsIContent*
nsCoreUtils::FindDescendantPointingToID(const nsString *aId,
                                        nsIContent *aLookContent,
                                        nsIAtom **aRelationAttrs,
                                        PRUint32 aAttrNum,
                                        nsIContent *aExcludeContent,
                                        nsIAtom *aTagType)
{
  // Surround id with spaces for search
  nsCAutoString idWithSpaces(' ');
  LossyAppendUTF16toASCII(*aId, idWithSpaces);
  idWithSpaces += ' ';
  return FindDescendantPointingToIDImpl(idWithSpaces, aLookContent,
                                        aRelationAttrs, aAttrNum,
                                        aExcludeContent, aTagType);
}

nsIContent*
nsCoreUtils::FindDescendantPointingToID(const nsString *aId,
                                        nsIContent *aLookContent,
                                        nsIAtom *aRelationAttr,
                                        nsIContent *aExcludeContent,
                                        nsIAtom *aTagType)
{
  return FindDescendantPointingToID(aId, aLookContent, &aRelationAttr, 1, aExcludeContent, aTagType);
}

nsIContent*
nsCoreUtils::FindDescendantPointingToIDImpl(nsCString& aIdWithSpaces,
                                            nsIContent *aLookContent,
                                            nsIAtom **aRelationAttrs,
                                            PRUint32 aAttrNum,
                                            nsIContent *aExcludeContent,
                                            nsIAtom *aTagType)
{
  NS_ENSURE_TRUE(aLookContent, nsnull);
  NS_ENSURE_TRUE(aRelationAttrs && *aRelationAttrs, nsnull);

  if (!aTagType || aLookContent->Tag() == aTagType) {
    // Tag matches
    // Check for ID in the attributes aRelationAttrs, which can be a list
    for (PRUint32 i = 0; i < aAttrNum; i++) {
      nsAutoString idList;
      if (aLookContent->GetAttr(kNameSpaceID_None, aRelationAttrs[i], idList)) {
        idList.Insert(' ', 0);  // Surround idlist with spaces for search
        idList.Append(' ');
        // idList is now a set of id's with spaces around each,
        // and id also has spaces around it.
        // If id is a substring of idList then we have a match
        if (idList.Find(aIdWithSpaces) != -1) {
          return aLookContent;
        }
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
                                                    aRelationAttrs, aAttrNum,
                                                    aExcludeContent, aTagType);
      if (labelContent) {
        return labelContent;
      }
    }
  }
  return nsnull;
}

nsIContent*
nsCoreUtils::GetLabelContent(nsIContent *aForNode)
{
  if (aForNode->IsXUL())
    return FindNeighbourPointingToNode(aForNode, nsAccessibilityAtoms::control,
                                       nsAccessibilityAtoms::label);

  return GetHTMLLabelContent(aForNode);
}

nsIContent*
nsCoreUtils::GetHTMLLabelContent(nsIContent *aForNode)
{
  // Get either <label for="[id]"> element which explictly points to aForNode,
  // or <label> ancestor which implicitly point to it.
  nsIContent *walkUpContent = aForNode;
  
  // Go up tree get name of ancestor label if there is one. Don't go up farther
  // than form element.
  while ((walkUpContent = walkUpContent->GetParent()) != nsnull) {
    nsIAtom *tag = walkUpContent->Tag();
    if (tag == nsAccessibilityAtoms::label)
      return walkUpContent;  // An ancestor <label> implicitly points to us

    if (tag == nsAccessibilityAtoms::form ||
        tag == nsAccessibilityAtoms::body) {
      // Reached top ancestor in form
      // There can be a label targeted at this control using the 
      // for="control_id" attribute. To save computing time, only 
      // look for those inside of a form element
      nsAutoString forId;
      if (!GetID(aForNode, forId))
        break;

      // Actually we'll be walking down the content this time, with a depth first search
      return FindDescendantPointingToID(&forId, walkUpContent,
                                        nsAccessibilityAtoms::_for);
    }
  }

  return nsnull;
}

void
nsCoreUtils::GetLanguageFor(nsIContent *aContent, nsIContent *aRootContent,
                            nsAString& aLanguage)
{
  aLanguage.Truncate();

  nsIContent *walkUp = aContent;
  while (walkUp && walkUp != aRootContent &&
         !walkUp->GetAttr(kNameSpaceID_None,
                          nsAccessibilityAtoms::lang, aLanguage))
    walkUp = walkUp->GetParent();
}

void
nsCoreUtils::GetElementsByIDRefsAttr(nsIContent *aContent, nsIAtom *aAttr,
                                     nsIArray **aRefElements)
{
  *aRefElements = nsnull;

  nsAutoString ids;
  if (!aContent->GetAttr(kNameSpaceID_None, aAttr, ids))
    return;

  ids.CompressWhitespace(PR_TRUE, PR_TRUE);

  nsCOMPtr<nsIDOMDocument> document = do_QueryInterface(aContent->GetOwnerDoc());
  NS_ASSERTION(document, "The given node is not in document!");
  if (!document)
    return;

  nsCOMPtr<nsIDOMDocumentXBL> xblDocument;
  if (aContent->IsInAnonymousSubtree())
    xblDocument = do_QueryInterface(document);

  nsCOMPtr<nsIMutableArray> refElms = do_CreateInstance(NS_ARRAY_CONTRACTID);

  while (!ids.IsEmpty()) {
    nsAutoString id;
    PRInt32 idLength = ids.FindChar(' ');
    NS_ASSERTION(idLength != 0,
                 "Should not be 0 because of CompressWhitespace() call above");

    if (idLength == kNotFound) {
      id = ids;
      ids.Truncate();
    } else {
      id = Substring(ids, 0, idLength);
      ids.Cut(0, idLength + 1);
    }

    // If content is anonymous subtree then use "anonid" attribute to get
    // elements, otherwise search elements in DOM by ID attribute.
    nsCOMPtr<nsIDOMElement> refElement;
    if (xblDocument) {
      nsCOMPtr<nsIDOMElement> elm =
        do_QueryInterface(aContent->GetBindingParent());
      xblDocument->GetAnonymousElementByAttribute(elm,
                                                  NS_LITERAL_STRING("anonid"),
                                                  id,
                                                  getter_AddRefs(refElement));
    } else {
      document->GetElementById(id, getter_AddRefs(refElement));
    }

    if (!refElement)
      continue;

    refElms->AppendElement(refElement, PR_FALSE);
  }

  NS_ADDREF(*aRefElements = refElms);
  return;
}

void
nsCoreUtils::GetElementsHavingIDRefsAttr(nsIContent *aRootContent,
                                         nsIContent *aContent,
                                         nsIAtom *aIDRefsAttr,
                                         nsIArray **aElements)
{
  *aElements = nsnull;

  nsAutoString id;
  if (!GetID(aContent, id))
    return;

  nsCAutoString idWithSpaces(' ');
  LossyAppendUTF16toASCII(id, idWithSpaces);
  idWithSpaces += ' ';

  nsCOMPtr<nsIMutableArray> elms = do_CreateInstance(NS_ARRAY_CONTRACTID);
  if (!elms)
    return;

  GetElementsHavingIDRefsAttrImpl(aRootContent, idWithSpaces, aIDRefsAttr,
                                  elms);
  NS_ADDREF(*aElements = elms);
}

void
nsCoreUtils::GetElementsHavingIDRefsAttrImpl(nsIContent *aRootContent,
                                             nsCString& aIdWithSpaces,
                                             nsIAtom *aIDRefsAttr,
                                             nsIMutableArray *aElements)
{
  PRUint32 childCount = aRootContent->GetChildCount();
  for (PRUint32 index = 0; index < childCount; index++) {
    nsIContent* child = aRootContent->GetChildAt(index);
    nsAutoString idList;
    if (child->GetAttr(kNameSpaceID_None, aIDRefsAttr, idList)) {
      idList.Insert(' ', 0);  // Surround idlist with spaces for search
      idList.Append(' ');
      // idList is now a set of id's with spaces around each, and id also has
      // spaces around it. If id is a substring of idList then we have a match.
      if (idList.Find(aIdWithSpaces) != -1) {
        aElements->AppendElement(child, PR_FALSE);
        continue; // Do not search inside children.
      }
    }
    GetElementsHavingIDRefsAttrImpl(child, aIdWithSpaces,
                                    aIDRefsAttr, aElements);
  }
}

void
nsCoreUtils::GetComputedStyleDeclaration(const nsAString& aPseudoElt,
                                         nsIDOMNode *aNode,
                                         nsIDOMCSSStyleDeclaration **aCssDecl)
{
  *aCssDecl = nsnull;

  nsCOMPtr<nsIDOMElement> domElement = GetDOMElementFor(aNode);
  if (!domElement)
    return;

  // Returns number of items in style declaration
  nsCOMPtr<nsIContent> content = do_QueryInterface(domElement);
  nsCOMPtr<nsIDocument> doc = content->GetDocument();
  if (!doc)
    return;

  nsCOMPtr<nsIDOMViewCSS> viewCSS(do_QueryInterface(doc->GetWindow()));
  if (!viewCSS)
    return;

  viewCSS->GetComputedStyle(domElement, aPseudoElt, aCssDecl);
}

already_AddRefed<nsIBoxObject>
nsCoreUtils::GetTreeBodyBoxObject(nsITreeBoxObject *aTreeBoxObj)
{
  nsCOMPtr<nsIDOMElement> tcElm;
  aTreeBoxObj->GetTreeBody(getter_AddRefs(tcElm));
  nsCOMPtr<nsIDOMXULElement> tcXULElm(do_QueryInterface(tcElm));
  if (!tcXULElm)
    return nsnull;

  nsIBoxObject *boxObj = nsnull;
  tcXULElm->GetBoxObject(&boxObj);
  return boxObj;
}

void
nsCoreUtils::GetTreeBoxObject(nsIDOMNode *aDOMNode,
                              nsITreeBoxObject **aBoxObject)
{
  nsAutoString name;
  nsCOMPtr<nsIDOMNode> parentNode, currentNode;
  
  // Find DOMNode's parents recursively until reach the <tree> tag
  currentNode = aDOMNode;
  while (currentNode) {
    currentNode->GetLocalName(name);
    if (name.EqualsLiteral("tree")) {
      // We will get the nsITreeBoxObject from the tree node
      nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(currentNode));
      if (xulElement) {
        nsCOMPtr<nsIBoxObject> box;
        xulElement->GetBoxObject(getter_AddRefs(box));
        nsCOMPtr<nsITreeBoxObject> treeBox(do_QueryInterface(box));
        if (treeBox) {
          *aBoxObject = treeBox;
          NS_ADDREF(*aBoxObject);
          return;
        }
      }
    }
    currentNode->GetParentNode(getter_AddRefs(parentNode));
    currentNode = parentNode;
  }
  
  *aBoxObject = nsnull;
}

already_AddRefed<nsITreeColumn>
nsCoreUtils::GetFirstSensibleColumn(nsITreeBoxObject *aTree)
{
  nsCOMPtr<nsITreeColumns> cols;
  aTree->GetColumns(getter_AddRefs(cols));
  if (!cols)
    return nsnull;

  nsCOMPtr<nsITreeColumn> column;
  cols->GetFirstColumn(getter_AddRefs(column));
  if (column && IsColumnHidden(column))
    return GetNextSensibleColumn(column);

  return column.forget();
}

already_AddRefed<nsITreeColumn>
nsCoreUtils::GetLastSensibleColumn(nsITreeBoxObject *aTree)
{
  nsCOMPtr<nsITreeColumns> cols;
  aTree->GetColumns(getter_AddRefs(cols));
  if (!cols)
    return nsnull;

  nsCOMPtr<nsITreeColumn> column;
  cols->GetLastColumn(getter_AddRefs(column));
  if (column && IsColumnHidden(column))
    return GetPreviousSensibleColumn(column);

  return column.forget();
}

PRUint32
nsCoreUtils::GetSensiblecolumnCount(nsITreeBoxObject *aTree)
{
  PRUint32 count = 0;

  nsCOMPtr<nsITreeColumns> cols;
  aTree->GetColumns(getter_AddRefs(cols));
  if (!cols)
    return count;

  nsCOMPtr<nsITreeColumn> column;
  cols->GetFirstColumn(getter_AddRefs(column));

  while (column) {
    if (!IsColumnHidden(column))
      count++;

    nsCOMPtr<nsITreeColumn> nextColumn;
    column->GetNext(getter_AddRefs(nextColumn));
    column.swap(nextColumn);
  }

  return count;
}

already_AddRefed<nsITreeColumn>
nsCoreUtils::GetSensibleColumnAt(nsITreeBoxObject *aTree, PRUint32 aIndex)
{
  PRUint32 idx = aIndex;

  nsCOMPtr<nsITreeColumn> column = GetFirstSensibleColumn(aTree);
  while (column) {
    if (idx == 0)
      return column.forget();

    idx--;
    column = GetNextSensibleColumn(column);
  }

  return nsnull;
}

already_AddRefed<nsITreeColumn>
nsCoreUtils::GetNextSensibleColumn(nsITreeColumn *aColumn)
{
  nsCOMPtr<nsITreeColumn> nextColumn;
  aColumn->GetNext(getter_AddRefs(nextColumn));

  while (nextColumn && IsColumnHidden(nextColumn)) {
    nsCOMPtr<nsITreeColumn> tempColumn;
    nextColumn->GetNext(getter_AddRefs(tempColumn));
    nextColumn.swap(tempColumn);
  }

  return nextColumn.forget();
}

already_AddRefed<nsITreeColumn>
nsCoreUtils::GetPreviousSensibleColumn(nsITreeColumn *aColumn)
{
  nsCOMPtr<nsITreeColumn> prevColumn;
  aColumn->GetPrevious(getter_AddRefs(prevColumn));

  while (prevColumn && IsColumnHidden(prevColumn)) {
    nsCOMPtr<nsITreeColumn> tempColumn;
    prevColumn->GetPrevious(getter_AddRefs(tempColumn));
    prevColumn.swap(tempColumn);
  }

  return prevColumn.forget();
}

PRBool
nsCoreUtils::IsColumnHidden(nsITreeColumn *aColumn)
{
  nsCOMPtr<nsIDOMElement> element;
  aColumn->GetElement(getter_AddRefs(element));
  nsCOMPtr<nsIContent> content = do_QueryInterface(element);
  return content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::hidden,
                              nsAccessibilityAtoms::_true, eCaseMatters);
}
