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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
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

#include "inDOMView.h"
#include "inIDOMUtils.h"

#include "inLayoutUtils.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsISupportsArray.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMAttr.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMMutationEvent.h"
#include "nsBindingManager.h"
#include "nsINameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIServiceManager.h"
#include "nsITreeColumns.h"
#include "mozilla/dom/Element.h"

#ifdef ACCESSIBILITY
#include "nsIAccessible.h"
#include "nsIAccessibilityService.h"
#endif

namespace dom = mozilla::dom;

////////////////////////////////////////////////////////////////////////
// inDOMViewNode

class inDOMViewNode
{
public:
  inDOMViewNode() {}
  inDOMViewNode(nsIDOMNode* aNode);
  ~inDOMViewNode();

  nsCOMPtr<nsIDOMNode> node;

  inDOMViewNode* parent;
  inDOMViewNode* next;
  inDOMViewNode* previous;

  PRInt32 level;
  bool isOpen;
  bool isContainer;
  bool hasAnonymous;
  bool hasSubDocument;
};

inDOMViewNode::inDOMViewNode(nsIDOMNode* aNode) :
  node(aNode),
  parent(nsnull),
  next(nsnull),
  previous(nsnull),
  level(0),
  isOpen(PR_FALSE),
  isContainer(PR_FALSE),
  hasAnonymous(PR_FALSE),
  hasSubDocument(PR_FALSE)
{

}

inDOMViewNode::~inDOMViewNode()
{
}

////////////////////////////////////////////////////////////////////////

inDOMView::inDOMView() :
  mShowAnonymous(PR_FALSE),
  mShowSubDocuments(PR_FALSE),
  mShowWhitespaceNodes(PR_TRUE),
  mShowAccessibleNodes(PR_FALSE),
  mWhatToShow(nsIDOMNodeFilter::SHOW_ALL)
{
}

inDOMView::~inDOMView()
{
  SetRootNode(nsnull);
}

#define DOMVIEW_ATOM(name_, value_) nsIAtom* inDOMView::name_ = nsnull;
#include "inDOMViewAtomList.h"
#undef DOMVIEW_ATOM

#define DOMVIEW_ATOM(name_, value_) NS_STATIC_ATOM_BUFFER(name_##_buffer, value_)
#include "inDOMViewAtomList.h"
#undef DOMVIEW_ATOM

/* static */ const nsStaticAtom inDOMView::Atoms_info[] = {
#define DOMVIEW_ATOM(name_, value_) NS_STATIC_ATOM(name_##_buffer, &inDOMView::name_),
#include "inDOMViewAtomList.h"
#undef DOMVIEW_ATOM
};

/* static */ void
inDOMView::InitAtoms()
{
  NS_RegisterStaticAtoms(Atoms_info, NS_ARRAY_LENGTH(Atoms_info));
}

////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS3(inDOMView,
                   inIDOMView,
                   nsITreeView,
                   nsIMutationObserver)

////////////////////////////////////////////////////////////////////////
// inIDOMView

NS_IMETHODIMP
inDOMView::GetRootNode(nsIDOMNode** aNode)
{
  *aNode = mRootNode;
  NS_IF_ADDREF(*aNode);
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetRootNode(nsIDOMNode* aNode)
{
  if (mTree)
    mTree->BeginUpdateBatch();

  if (mRootDocument) {
    // remove previous document observer
    nsCOMPtr<nsINode> doc(do_QueryInterface(mRootDocument));
    if (doc)
      doc->RemoveMutationObserver(this);
  }

  RemoveAllNodes();

  mRootNode = aNode;

  if (aNode) {
    // If we are able to show element nodes, then start with the root node
    // as the first node in the buffer
    if (mWhatToShow & nsIDOMNodeFilter::SHOW_ELEMENT) {
      // allocate new node array
      AppendNode(CreateNode(aNode, nsnull));
    } else {
      // place only the children of the root node in the buffer
      ExpandNode(-1);
    }

    // store an owning reference to document so that it isn't
    // destroyed before we are
    mRootDocument = do_QueryInterface(aNode);
    if (!mRootDocument) {
      aNode->GetOwnerDocument(getter_AddRefs(mRootDocument));
    }

    // add document observer
    nsCOMPtr<nsINode> doc(do_QueryInterface(mRootDocument));
    if (doc)
      doc->AddMutationObserver(this);
  } else {
    mRootDocument = nsnull;
  }

  if (mTree)
    mTree->EndUpdateBatch();

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetNodeFromRowIndex(PRInt32 rowIndex, nsIDOMNode **_retval)
{
  inDOMViewNode* viewNode = nsnull;
  RowToNode(rowIndex, &viewNode);
  if (!viewNode) return NS_ERROR_FAILURE;
  *_retval = viewNode->node;
  NS_IF_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetRowIndexFromNode(nsIDOMNode *node, PRInt32 *_retval)
{
  NodeToRow(node, _retval);
  return NS_OK;
}


NS_IMETHODIMP
inDOMView::GetShowAnonymousContent(bool *aShowAnonymousContent)
{
  *aShowAnonymousContent = mShowAnonymous;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetShowAnonymousContent(bool aShowAnonymousContent)
{
  mShowAnonymous = aShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetShowSubDocuments(bool *aShowSubDocuments)
{
  *aShowSubDocuments = mShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetShowSubDocuments(bool aShowSubDocuments)
{
  mShowSubDocuments = aShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetShowWhitespaceNodes(bool *aShowWhitespaceNodes)
{
  *aShowWhitespaceNodes = mShowWhitespaceNodes;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetShowWhitespaceNodes(bool aShowWhitespaceNodes)
{
  mShowWhitespaceNodes = aShowWhitespaceNodes;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetShowAccessibleNodes(bool *aShowAccessibleNodes)
{
  *aShowAccessibleNodes = mShowAccessibleNodes;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetShowAccessibleNodes(bool aShowAccessibleNodes)
{
  mShowAccessibleNodes = aShowAccessibleNodes;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetWhatToShow(PRUint32 *aWhatToShow)
{
  *aWhatToShow = mWhatToShow;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetWhatToShow(PRUint32 aWhatToShow)
{
  mWhatToShow = aWhatToShow;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::Rebuild()
{
  nsCOMPtr<nsIDOMNode> root;
  GetRootNode(getter_AddRefs(root));
  SetRootNode(root);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsITreeView

NS_IMETHODIMP
inDOMView::GetRowCount(PRInt32 *aRowCount)
{
  *aRowCount = GetRowCount();
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetRowProperties(PRInt32 index, nsISupportsArray *properties)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetCellProperties(PRInt32 row, nsITreeColumn* col, nsISupportsArray *properties)
{
  inDOMViewNode* node = nsnull;
  RowToNode(row, &node);
  if (!node) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content = do_QueryInterface(node->node);
  if (content && content->IsInAnonymousSubtree()) {
    properties->AppendElement(kAnonymousAtom);
  }

  PRUint16 nodeType;
  node->node->GetNodeType(&nodeType);
  switch (nodeType) {
    case nsIDOMNode::ELEMENT_NODE:
      properties->AppendElement(kElementNodeAtom);
      break;
    case nsIDOMNode::ATTRIBUTE_NODE:
      properties->AppendElement(kAttributeNodeAtom);
      break;
    case nsIDOMNode::TEXT_NODE:
      properties->AppendElement(kTextNodeAtom);
      break;
    case nsIDOMNode::CDATA_SECTION_NODE:
      properties->AppendElement(kCDataSectionNodeAtom);
      break;
    case nsIDOMNode::ENTITY_REFERENCE_NODE:
      properties->AppendElement(kEntityReferenceNodeAtom);
      break;
    case nsIDOMNode::ENTITY_NODE:
      properties->AppendElement(kEntityNodeAtom);
      break;
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
      properties->AppendElement(kProcessingInstructionNodeAtom);
      break;
    case nsIDOMNode::COMMENT_NODE:
      properties->AppendElement(kCommentNodeAtom);
      break;
    case nsIDOMNode::DOCUMENT_NODE:
      properties->AppendElement(kDocumentNodeAtom);
      break;
    case nsIDOMNode::DOCUMENT_TYPE_NODE:
      properties->AppendElement(kDocumentTypeNodeAtom);
      break;
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
      properties->AppendElement(kDocumentFragmentNodeAtom);
      break;
    case nsIDOMNode::NOTATION_NODE:
      properties->AppendElement(kNotationNodeAtom);
      break;
  }

#ifdef ACCESSIBILITY
  if (mShowAccessibleNodes) {
    nsCOMPtr<nsIAccessibilityService> accService(
      do_GetService("@mozilla.org/accessibilityService;1"));
    NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

    nsCOMPtr<nsIAccessible> accessible;
    nsresult rv =
      accService->GetAccessibleFor(node->node, getter_AddRefs(accessible));
    if (NS_SUCCEEDED(rv) && accessible)
      properties->AppendElement(kAccessibleNodeAtom);
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetColumnProperties(nsITreeColumn* col, nsISupportsArray *properties)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetImageSrc(PRInt32 row, nsITreeColumn* col, nsAString& _retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetProgressMode(PRInt32 row, nsITreeColumn* col, PRInt32* _retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetCellValue(PRInt32 row, nsITreeColumn* col, nsAString& _retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetCellText(PRInt32 row, nsITreeColumn* col, nsAString& _retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(row, &node);
  if (!node) return NS_ERROR_FAILURE;

  nsIDOMNode* domNode = node->node;

  nsAutoString colID;
  col->GetId(colID);
  if (colID.EqualsLiteral("colNodeName"))
    domNode->GetNodeName(_retval);
  else if (colID.EqualsLiteral("colLocalName"))
    domNode->GetLocalName(_retval);
  else if (colID.EqualsLiteral("colPrefix"))
    domNode->GetPrefix(_retval);
  else if (colID.EqualsLiteral("colNamespaceURI"))
    domNode->GetNamespaceURI(_retval);
  else if (colID.EqualsLiteral("colNodeType")) {
    PRUint16 nodeType;
    domNode->GetNodeType(&nodeType);
    nsAutoString temp;
    temp.AppendInt(PRInt32(nodeType));
    _retval = temp;
  } else if (colID.EqualsLiteral("colNodeValue"))
    domNode->GetNodeValue(_retval);
  else {
    if (StringBeginsWith(colID, NS_LITERAL_STRING("col@"))) {
      nsCOMPtr<nsIDOMElement> el = do_QueryInterface(node->node);
      if (el) {
        nsAutoString attr;
        colID.Right(attr, colID.Length()-4); // have to use this because Substring is crashing on me!
        el->GetAttribute(attr, _retval);
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsContainer(PRInt32 index, bool *_retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->isContainer;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsContainerOpen(PRInt32 index, bool *_retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->isOpen;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsContainerEmpty(PRInt32 index, bool *_retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->isContainer ? PR_FALSE : PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetLevel(PRInt32 index, PRInt32 *_retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->level;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetParentIndex(PRInt32 rowIndex, PRInt32 *_retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(rowIndex, &node);
  if (!node) return NS_ERROR_FAILURE;

  // GetParentIndex returns -1 if there is no parent  
  *_retval = -1;
  
  inDOMViewNode* checkNode = nsnull;
  PRInt32 i = rowIndex - 1;
  do {
    nsresult rv = RowToNode(i, &checkNode);
    if (NS_FAILED(rv)) {
      // No parent. Just break out.
      break;
    }
    
    if (checkNode == node->parent) {
      *_retval = i;
      return NS_OK;
    }
    --i;
  } while (checkNode);

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::HasNextSibling(PRInt32 rowIndex, PRInt32 afterIndex, bool *_retval)
{
  inDOMViewNode* node = nsnull;
  RowToNode(rowIndex, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->next != nsnull;

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::ToggleOpenState(PRInt32 index)
{
  inDOMViewNode* node = nsnull;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  PRInt32 oldCount = GetRowCount();
  if (node->isOpen)
    CollapseNode(index);
  else
    ExpandNode(index);

  // Update the twisty.
  mTree->InvalidateRow(index);

  mTree->RowCountChanged(index+1, GetRowCount() - oldCount);

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetTree(nsITreeBoxObject *tree)
{
  mTree = tree;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetSelection(nsITreeSelection * *aSelection)
{
  *aSelection = mSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP inDOMView::SetSelection(nsITreeSelection * aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SelectionChanged()
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetCellValue(PRInt32 row, nsITreeColumn* col, const nsAString& value)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetCellText(PRInt32 row, nsITreeColumn* col, const nsAString& value)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::CycleHeader(nsITreeColumn* col)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::CycleCell(PRInt32 row, nsITreeColumn* col)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsEditable(PRInt32 row, nsITreeColumn* col, bool *_retval)
{
  return NS_OK;
}


NS_IMETHODIMP
inDOMView::IsSelectable(PRInt32 row, nsITreeColumn* col, bool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsSeparator(PRInt32 index, bool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsSorted(bool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::CanDrop(PRInt32 index, PRInt32 orientation,
                   nsIDOMDataTransfer* aDataTransfer, bool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::Drop(PRInt32 row, PRInt32 orientation, nsIDOMDataTransfer* aDataTransfer)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::PerformAction(const PRUnichar *action)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::PerformActionOnCell(const PRUnichar* action, PRInt32 row, nsITreeColumn* col)
{
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////
// nsIMutationObserver

void
inDOMView::NodeWillBeDestroyed(const nsINode* aNode)
{
  NS_NOTREACHED("Document destroyed while we're holding a strong ref to it");
}

void
inDOMView::AttributeChanged(nsIDocument* aDocument, dom::Element* aElement,
                            PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                            PRInt32 aModType)
{
  if (!mTree) {
    return;
  }

  if (!(mWhatToShow & nsIDOMNodeFilter::SHOW_ATTRIBUTE)) {
    return;
  }

  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  
  // get the dom attribute node, if there is any
  nsCOMPtr<nsIDOMNode> content(do_QueryInterface(aElement));
  nsCOMPtr<nsIDOMElement> el(do_QueryInterface(aElement));
  nsCOMPtr<nsIDOMAttr> domAttr;
  nsDependentAtomString attrStr(aAttribute);
  if (aNameSpaceID) {
    nsCOMPtr<nsINameSpaceManager> nsm =
      do_GetService(NS_NAMESPACEMANAGER_CONTRACTID);
    if (!nsm) {
      // we can't find out which attribute we want :(
      return;
    }
    nsString attrNS;
    nsresult rv = nsm->GetNameSpaceURI(aNameSpaceID, attrNS);
    if (NS_FAILED(rv)) {
      return;
    }
    (void)el->GetAttributeNodeNS(attrNS, attrStr, getter_AddRefs(domAttr));
  } else {
    (void)el->GetAttributeNode(attrStr, getter_AddRefs(domAttr));
  }

  if (aModType == nsIDOMMutationEvent::MODIFICATION) {
    // No fancy stuff here, just invalidate the changed row
    if (!domAttr) {
      return;
    }
    PRInt32 row = 0;
    NodeToRow(domAttr, &row);
    mTree->InvalidateRange(row, row);
  } else if (aModType == nsIDOMMutationEvent::ADDITION) {
    if (!domAttr) {
      return;
    }
    // get the number of attributes on this content node
    nsCOMPtr<nsIDOMNamedNodeMap> attrs;
    content->GetAttributes(getter_AddRefs(attrs));
    PRUint32 attrCount;
    attrs->GetLength(&attrCount);

    inDOMViewNode* contentNode = nsnull;
    PRInt32 contentRow;
    PRInt32 attrRow;
    if (mRootNode == content &&
        !(mWhatToShow & nsIDOMNodeFilter::SHOW_ELEMENT)) {
      // if this view has a root node but is not displaying it,
      // it is ok to act as if the changed attribute is on the root.
      attrRow = attrCount - 1;
    } else {
      if (NS_FAILED(NodeToRow(content, &contentRow))) {
        return;
      }
      RowToNode(contentRow, &contentNode);
      if (!contentNode->isOpen) {
        return;
      }
      attrRow = contentRow + attrCount;
    }

    inDOMViewNode* newNode = CreateNode(domAttr, contentNode);
    inDOMViewNode* insertNode = nsnull;
    RowToNode(attrRow, &insertNode);
    if (insertNode) {
      if (contentNode &&
          insertNode->level <= contentNode->level) {
        RowToNode(attrRow-1, &insertNode);
        InsertLinkAfter(newNode, insertNode);
      } else
        InsertLinkBefore(newNode, insertNode);
    }
    InsertNode(newNode, attrRow);
    mTree->RowCountChanged(attrRow, 1);
  } else if (aModType == nsIDOMMutationEvent::REMOVAL) {
    // At this point, the attribute is already gone from the DOM, but is still represented
    // in our mRows array.  Search through the content node's children for the corresponding
    // node and remove it.

    // get the row of the content node
    inDOMViewNode* contentNode = nsnull;
    PRInt32 contentRow;
    PRInt32 baseLevel;
    if (NS_SUCCEEDED(NodeToRow(content, &contentRow))) {
      RowToNode(contentRow, &contentNode);
      baseLevel = contentNode->level;
    } else {
      if (mRootNode == content) {
        contentRow = -1;
        baseLevel = -1;
      } else
        return;
    }

    // search for the attribute node that was removed
    inDOMViewNode* checkNode = nsnull;
    PRInt32 row = 0;
    for (row = contentRow+1; row < GetRowCount(); ++row) {
      checkNode = GetNodeAt(row);
      if (checkNode->level == baseLevel+1) {
        domAttr = do_QueryInterface(checkNode->node);
        if (domAttr) {
          nsAutoString attrName;
          domAttr->GetNodeName(attrName);
          if (attrName.Equals(attrStr)) {
            // we have found the row for the attribute that was removed
            RemoveLink(checkNode);
            RemoveNode(row);
            mTree->RowCountChanged(row, -1);
            break;
          }
        }
      }
      if (checkNode->level <= baseLevel)
        break;
    }

 }
}

void
inDOMView::ContentAppended(nsIDocument *aDocument,
                           nsIContent* aContainer,
                           nsIContent* aFirstNewContent,
                           PRInt32 /* unused */)
{
  if (!mTree) {
    return;
  }

  for (nsIContent* cur = aFirstNewContent; cur; cur = cur->GetNextSibling()) {
    // Our ContentInserted impl doesn't use the index
    ContentInserted(aDocument, aContainer, cur, 0);
  }
}

void
inDOMView::ContentInserted(nsIDocument *aDocument, nsIContent* aContainer,
                           nsIContent* aChild, PRInt32 /* unused */)
{
  if (!mTree)
    return;

  nsresult rv;
  nsCOMPtr<nsIDOMNode> childDOMNode(do_QueryInterface(aChild));
  nsCOMPtr<nsIDOMNode> parent;
  if (!mDOMUtils) {
    mDOMUtils = do_GetService("@mozilla.org/inspector/dom-utils;1");
    if (!mDOMUtils) {
      return;
    }
  }
  mDOMUtils->GetParentForNode(childDOMNode, mShowAnonymous,
                              getter_AddRefs(parent));

  // find the inDOMViewNode for the parent of the inserted content
  PRInt32 parentRow = 0;
  if (NS_FAILED(rv = NodeToRow(parent, &parentRow)))
    return;
  inDOMViewNode* parentNode = nsnull;
  if (NS_FAILED(rv = RowToNode(parentRow, &parentNode)))
    return;

  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  
  if (!parentNode->isOpen) {
    // Parent is not open, so don't bother creating tree rows for the
    // kids.  But do indicate that it's now a container, if needed.
    if (!parentNode->isContainer) {
      parentNode->isContainer = PR_TRUE;
      mTree->InvalidateRow(parentRow);
    }
    return;
  }

  // get the previous sibling of the inserted content
  nsCOMPtr<nsIDOMNode> previous;
  GetRealPreviousSibling(childDOMNode, parent, getter_AddRefs(previous));
  inDOMViewNode* previousNode = nsnull;

  PRInt32 row = 0;
  if (previous) {
    // find the inDOMViewNode for the previous sibling of the inserted content
    PRInt32 previousRow = 0;
    if (NS_FAILED(rv = NodeToRow(previous, &previousRow)))
      return;
    if (NS_FAILED(rv = RowToNode(previousRow, &previousNode)))
      return;

    // get the last descendant of the previous row, which is the row
    // after which to insert this new row
    GetLastDescendantOf(previousNode, previousRow, &row);
    ++row;
  } else {
    // there is no previous sibling, so the new row will be inserted after the parent
    row = parentRow+1;
  }

  inDOMViewNode* newNode = CreateNode(childDOMNode, parentNode);

  if (previous) {
    InsertLinkAfter(newNode, previousNode);
  } else {
    PRInt32 firstChildRow;
    if (NS_SUCCEEDED(GetFirstDescendantOf(parentNode, parentRow, &firstChildRow))) {
      inDOMViewNode* firstChild;
      RowToNode(firstChildRow, &firstChild);
      InsertLinkBefore(newNode, firstChild);
    }
  }

  // insert new node
  InsertNode(newNode, row);

  mTree->RowCountChanged(row, 1);
}

void
inDOMView::ContentRemoved(nsIDocument *aDocument, nsIContent* aContainer,
                          nsIContent* aChild, PRInt32 aIndexInContainer,
                          nsIContent* aPreviousSibling)
{
  if (!mTree)
    return;

  nsresult rv;

  // find the inDOMViewNode for the old child
  nsCOMPtr<nsIDOMNode> oldDOMNode(do_QueryInterface(aChild));
  PRInt32 row = 0;
  if (NS_FAILED(rv = NodeToRow(oldDOMNode, &row)))
    return;
  inDOMViewNode* oldNode;
  if (NS_FAILED(rv = RowToNode(row, &oldNode)))
    return;

  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  
  // The parent may no longer be a container.  Note that we don't want
  // to access oldNode after calling RemoveNode, so do this now.
  inDOMViewNode* parentNode = oldNode->parent;
  bool isOnlyChild = oldNode->previous == nsnull && oldNode->next == nsnull;
  
  // Keep track of how many rows we are removing.  It's at least one,
  // but if we're open it's more.
  PRInt32 oldCount = GetRowCount();
  
  if (oldNode->isOpen)
    CollapseNode(row);

  RemoveLink(oldNode);
  RemoveNode(row);

  if (isOnlyChild) {
    // Fix up the parent
    parentNode->isContainer = PR_FALSE;
    parentNode->isOpen = PR_FALSE;
    mTree->InvalidateRow(NodeToRow(parentNode));
  }
    
  mTree->RowCountChanged(row, GetRowCount() - oldCount);
}

///////////////////////////////////////////////////////////////////////
// inDOMView

//////// NODE MANAGEMENT

inDOMViewNode*
inDOMView::GetNodeAt(PRInt32 aRow)
{
  return mNodes.ElementAt(aRow);
}

PRInt32
inDOMView::GetRowCount()
{
  return mNodes.Length();
}

PRInt32
inDOMView::NodeToRow(inDOMViewNode* aNode)
{
  return mNodes.IndexOf(aNode);
}

inDOMViewNode*
inDOMView::CreateNode(nsIDOMNode* aNode, inDOMViewNode* aParent)
{
  inDOMViewNode* viewNode = new inDOMViewNode(aNode);
  viewNode->level = aParent ? aParent->level+1 : 0;
  viewNode->parent = aParent;

  nsCOMArray<nsIDOMNode> grandKids;
  GetChildNodesFor(aNode, grandKids);
  viewNode->isContainer = (grandKids.Count() > 0);
  return viewNode;
}

bool
inDOMView::RowOutOfBounds(PRInt32 aRow, PRInt32 aCount)
{
  return aRow < 0 || aRow >= GetRowCount() || aCount+aRow > GetRowCount();
}

void
inDOMView::AppendNode(inDOMViewNode* aNode)
{
  mNodes.AppendElement(aNode);
}

void
inDOMView::InsertNode(inDOMViewNode* aNode, PRInt32 aRow)
{
  if (RowOutOfBounds(aRow, 1))
    AppendNode(aNode);
  else
    mNodes.InsertElementAt(aRow, aNode);
}

void
inDOMView::RemoveNode(PRInt32 aRow)
{
  if (RowOutOfBounds(aRow, 1))
    return;

  delete GetNodeAt(aRow);
  mNodes.RemoveElementAt(aRow);
}

void
inDOMView::ReplaceNode(inDOMViewNode* aNode, PRInt32 aRow)
{
  if (RowOutOfBounds(aRow, 1))
    return;

  delete GetNodeAt(aRow);
  mNodes.ElementAt(aRow) = aNode;
}

void
inDOMView::InsertNodes(nsTArray<inDOMViewNode*>& aNodes, PRInt32 aRow)
{
  if (aRow < 0 || aRow > GetRowCount())
    return;

  mNodes.InsertElementsAt(aRow, aNodes);
}

void
inDOMView::RemoveNodes(PRInt32 aRow, PRInt32 aCount)
{
  if (aRow < 0)
    return;

  PRInt32 rowCount = GetRowCount();
  for (PRInt32 i = aRow; i < aRow+aCount && i < rowCount; ++i) {
    delete GetNodeAt(i);
  }

  mNodes.RemoveElementsAt(aRow, aCount);
}

void
inDOMView::RemoveAllNodes()
{
  PRInt32 rowCount = GetRowCount();
  for (PRInt32 i = 0; i < rowCount; ++i) {
    delete GetNodeAt(i);
  }

  mNodes.Clear();
}

void
inDOMView::ExpandNode(PRInt32 aRow)
{
  inDOMViewNode* node = nsnull;
  RowToNode(aRow, &node);

  nsCOMArray<nsIDOMNode> kids;
  GetChildNodesFor(node ? node->node : mRootNode,
                   kids);
  PRInt32 kidCount = kids.Count();

  nsTArray<inDOMViewNode*> list(kidCount);

  inDOMViewNode* newNode = nsnull;
  inDOMViewNode* prevNode = nsnull;

  for (PRInt32 i = 0; i < kidCount; ++i) {
    newNode = CreateNode(kids[i], node);
    list.AppendElement(newNode);

    if (prevNode)
      prevNode->next = newNode;
    newNode->previous = prevNode;
    prevNode = newNode;
  }

  InsertNodes(list, aRow+1);

  if (node)
    node->isOpen = PR_TRUE;
}

void
inDOMView::CollapseNode(PRInt32 aRow)
{
  inDOMViewNode* node = nsnull;
  nsresult rv = RowToNode(aRow, &node);
  if (NS_FAILED(rv)) {
    return;
  }

  PRInt32 row = 0;
  GetLastDescendantOf(node, aRow, &row);

  RemoveNodes(aRow+1, row-aRow);

  node->isOpen = PR_FALSE;
}

//////// NODE AND ROW CONVERSION

nsresult
inDOMView::RowToNode(PRInt32 aRow, inDOMViewNode** aNode)
{
  if (aRow < 0 || aRow >= GetRowCount())
    return NS_ERROR_FAILURE;

  *aNode = GetNodeAt(aRow);
  return NS_OK;
}

nsresult
inDOMView::NodeToRow(nsIDOMNode* aNode, PRInt32* aRow)
{
  PRInt32 rowCount = GetRowCount();
  for (PRInt32 i = 0; i < rowCount; ++i) {
    if (GetNodeAt(i)->node == aNode) {
      *aRow = i;
      return NS_OK;
    }
  }

  *aRow = -1;
  return NS_ERROR_FAILURE;
}

//////// NODE HIERARCHY MUTATION

void
inDOMView::InsertLinkAfter(inDOMViewNode* aNode, inDOMViewNode* aInsertAfter)
{
  if (aInsertAfter->next)
    aInsertAfter->next->previous = aNode;
  aNode->next = aInsertAfter->next;
  aInsertAfter->next = aNode;
  aNode->previous = aInsertAfter;
}

void
inDOMView::InsertLinkBefore(inDOMViewNode* aNode, inDOMViewNode* aInsertBefore)
{
  if (aInsertBefore->previous)
    aInsertBefore->previous->next = aNode;
  aNode->previous = aInsertBefore->previous;
  aInsertBefore->previous = aNode;
  aNode->next = aInsertBefore;
}

void
inDOMView::RemoveLink(inDOMViewNode* aNode)
{
  if (aNode->previous)
    aNode->previous->next = aNode->next;
  if (aNode->next)
    aNode->next->previous = aNode->previous;
}

void
inDOMView::ReplaceLink(inDOMViewNode* aNewNode, inDOMViewNode* aOldNode)
{
  if (aOldNode->previous)
    aOldNode->previous->next = aNewNode;
  if (aOldNode->next)
    aOldNode->next->previous = aNewNode;
  aNewNode->next = aOldNode->next;
  aNewNode->previous = aOldNode->previous;
}

//////// NODE HIERARCHY UTILITIES

nsresult
inDOMView::GetFirstDescendantOf(inDOMViewNode* aNode, PRInt32 aRow, PRInt32* aResult)
{
  // get the first node that is a descendant of the previous sibling
  PRInt32 row = 0;
  inDOMViewNode* node;
  for (row = aRow+1; row < GetRowCount(); ++row) {
    node = GetNodeAt(row);
    if (node->parent == aNode) {
      *aResult = row;
      return NS_OK;
    }
    if (node->level <= aNode->level)
      break;
  }
  return NS_ERROR_FAILURE;
}

nsresult
inDOMView::GetLastDescendantOf(inDOMViewNode* aNode, PRInt32 aRow, PRInt32* aResult)
{
  // get the last node that is a descendant of the previous sibling
  PRInt32 row = 0;
  for (row = aRow+1; row < GetRowCount(); ++row) {
    if (GetNodeAt(row)->level <= aNode->level)
      break;
  }
  *aResult = row-1;
  return NS_OK;
}

//////// DOM UTILITIES

nsresult
inDOMView::GetChildNodesFor(nsIDOMNode* aNode, nsCOMArray<nsIDOMNode>& aResult)
{
  NS_ENSURE_ARG(aNode);
  // Need to do this test to prevent unfortunate NYI assertion
  // on nsXULAttribute::GetChildNodes
  nsCOMPtr<nsIDOMAttr> attr = do_QueryInterface(aNode);
  if (!attr) {
    // attribute nodes
    if (mWhatToShow & nsIDOMNodeFilter::SHOW_ATTRIBUTE) {
      nsCOMPtr<nsIDOMNamedNodeMap> attrs;
      aNode->GetAttributes(getter_AddRefs(attrs));
      if (attrs) {
        AppendAttrsToArray(attrs, aResult);
      }
    }

    if (mWhatToShow & nsIDOMNodeFilter::SHOW_ELEMENT) {
      nsCOMPtr<nsIDOMNodeList> kids;
      if (!mDOMUtils) {
        mDOMUtils = do_GetService("@mozilla.org/inspector/dom-utils;1");
        if (!mDOMUtils) {
          return NS_ERROR_FAILURE;
        }
      }

      mDOMUtils->GetChildrenForNode(aNode, mShowAnonymous,
                                    getter_AddRefs(kids));

      if (kids) {
        AppendKidsToArray(kids, aResult);
      }
    }

    if (mShowSubDocuments) {
      nsCOMPtr<nsIDOMNode> domdoc =
        do_QueryInterface(inLayoutUtils::GetSubDocumentFor(aNode));
      if (domdoc) {
        aResult.AppendObject(domdoc);
      }
    }
  }

  return NS_OK;
}

nsresult
inDOMView::GetRealPreviousSibling(nsIDOMNode* aNode, nsIDOMNode* aRealParent, nsIDOMNode** aSibling)
{
  // XXXjrh: This won't work for some cases during some situations where XBL insertion points
  // are involved.  Fix me!
  aNode->GetPreviousSibling(aSibling);
  return NS_OK;
}

nsresult
inDOMView::AppendKidsToArray(nsIDOMNodeList* aKids,
                             nsCOMArray<nsIDOMNode>& aArray)
{
  PRUint32 l = 0;
  aKids->GetLength(&l);
  nsCOMPtr<nsIDOMNode> kid;
  PRUint16 nodeType = 0;

  // Try and get DOM Utils in case we don't have one yet.
  if (!mShowWhitespaceNodes && !mDOMUtils) {
    mDOMUtils = do_CreateInstance("@mozilla.org/inspector/dom-utils;1");
  }

  for (PRUint32 i = 0; i < l; ++i) {
    aKids->Item(i, getter_AddRefs(kid));
    kid->GetNodeType(&nodeType);

    NS_ASSERTION(nodeType && nodeType <= nsIDOMNode::NOTATION_NODE,
                 "Unknown node type. "
                 "Were new types added to the spec?");
    // As of DOM Level 2 Core and Traversal, each NodeFilter constant
    // is defined as the lower nth bit in the NodeFilter bitmask,
    // where n is the numeric constant of the nodeType it represents.
    // If this invariant ever changes, we will need to update the
    // following line.
    PRUint32 filterForNodeType = 1 << (nodeType - 1);

    if (mWhatToShow & filterForNodeType) {
      if ((nodeType == nsIDOMNode::TEXT_NODE ||
           nodeType == nsIDOMNode::COMMENT_NODE) &&
          !mShowWhitespaceNodes && mDOMUtils) {
        nsCOMPtr<nsIDOMCharacterData> data = do_QueryInterface(kid);
        NS_ASSERTION(data, "Does not implement nsIDOMCharacterData!");
        bool ignore;
        mDOMUtils->IsIgnorableWhitespace(data, &ignore);
        if (ignore) {
          continue;
        }
      }

      aArray.AppendObject(kid);
    }
  }

  return NS_OK;
}

nsresult
inDOMView::AppendAttrsToArray(nsIDOMNamedNodeMap* aKids,
                              nsCOMArray<nsIDOMNode>& aArray)
{
  PRUint32 l = 0;
  aKids->GetLength(&l);
  nsCOMPtr<nsIDOMNode> kid;
  for (PRUint32 i = 0; i < l; ++i) {
    aKids->Item(i, getter_AddRefs(kid));
    aArray.AppendObject(kid);
  }
  return NS_OK;
}
