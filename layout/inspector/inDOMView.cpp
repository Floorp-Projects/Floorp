/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "inDOMView.h"
#include "inIDOMUtils.h"

#include "inLayoutUtils.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMAttr.h"
#include "nsIDOMMozNamedAttrMap.h"
#include "nsIDOMMutationEvent.h"
#include "nsBindingManager.h"
#include "nsNameSpaceManager.h"
#include "nsIDocument.h"
#include "nsIServiceManager.h"
#include "nsITreeColumns.h"
#include "nsITreeBoxObject.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Services.h"

#ifdef ACCESSIBILITY
#include "nsIAccessible.h"
#include "nsIAccessibilityService.h"
#endif

using namespace mozilla;

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

  int32_t level;
  bool isOpen;
  bool isContainer;
  bool hasAnonymous;
  bool hasSubDocument;
};

inDOMViewNode::inDOMViewNode(nsIDOMNode* aNode) :
  node(aNode),
  parent(nullptr),
  next(nullptr),
  previous(nullptr),
  level(0),
  isOpen(false),
  isContainer(false),
  hasAnonymous(false),
  hasSubDocument(false)
{

}

inDOMViewNode::~inDOMViewNode()
{
}

////////////////////////////////////////////////////////////////////////

inDOMView::inDOMView() :
  mShowAnonymous(false),
  mShowSubDocuments(false),
  mShowWhitespaceNodes(true),
  mShowAccessibleNodes(false),
  mWhatToShow(nsIDOMNodeFilter::SHOW_ALL)
{
}

inDOMView::~inDOMView()
{
  SetRootNode(nullptr);
}


////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS(inDOMView,
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
      AppendNode(CreateNode(aNode, nullptr));
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
    mRootDocument = nullptr;
  }

  if (mTree)
    mTree->EndUpdateBatch();

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetNodeFromRowIndex(int32_t rowIndex, nsIDOMNode **_retval)
{
  inDOMViewNode* viewNode = nullptr;
  RowToNode(rowIndex, &viewNode);
  if (!viewNode) return NS_ERROR_FAILURE;
  *_retval = viewNode->node;
  NS_IF_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetRowIndexFromNode(nsIDOMNode *node, int32_t *_retval)
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
inDOMView::GetWhatToShow(uint32_t *aWhatToShow)
{
  *aWhatToShow = mWhatToShow;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetWhatToShow(uint32_t aWhatToShow)
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
inDOMView::GetRowCount(int32_t *aRowCount)
{
  *aRowCount = GetRowCount();
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetRowProperties(int32_t index, nsAString& aProps)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetCellProperties(int32_t row, nsITreeColumn* col,
                             nsAString& aProps)
{
  inDOMViewNode* node = nullptr;
  RowToNode(row, &node);
  if (!node) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content = do_QueryInterface(node->node);
  if (content && content->IsInAnonymousSubtree()) {
    aProps.AppendLiteral("anonymous ");
  }

  uint16_t nodeType;
  node->node->GetNodeType(&nodeType);
  switch (nodeType) {
    case nsIDOMNode::ELEMENT_NODE:
      aProps.AppendLiteral("ELEMENT_NODE");
      break;
    case nsIDOMNode::ATTRIBUTE_NODE:
      aProps.AppendLiteral("ATTRIBUTE_NODE");
      break;
    case nsIDOMNode::TEXT_NODE:
      aProps.AppendLiteral("TEXT_NODE");
      break;
    case nsIDOMNode::CDATA_SECTION_NODE:
      aProps.AppendLiteral("CDATA_SECTION_NODE");
      break;
    case nsIDOMNode::ENTITY_REFERENCE_NODE:
      aProps.AppendLiteral("ENTITY_REFERENCE_NODE");
      break;
    case nsIDOMNode::ENTITY_NODE:
      aProps.AppendLiteral("ENTITY_NODE");
      break;
    case nsIDOMNode::PROCESSING_INSTRUCTION_NODE:
      aProps.AppendLiteral("PROCESSING_INSTRUCTION_NODE");
      break;
    case nsIDOMNode::COMMENT_NODE:
      aProps.AppendLiteral("COMMENT_NODE");
      break;
    case nsIDOMNode::DOCUMENT_NODE:
      aProps.AppendLiteral("DOCUMENT_NODE");
      break;
    case nsIDOMNode::DOCUMENT_TYPE_NODE:
      aProps.AppendLiteral("DOCUMENT_TYPE_NODE");
      break;
    case nsIDOMNode::DOCUMENT_FRAGMENT_NODE:
      aProps.AppendLiteral("DOCUMENT_FRAGMENT_NODE");
      break;
    case nsIDOMNode::NOTATION_NODE:
      aProps.AppendLiteral("NOTATION_NODE");
      break;
  }

#ifdef ACCESSIBILITY
  if (mShowAccessibleNodes) {
	  nsCOMPtr<nsIAccessibilityService> accService =
        services::GetAccessibilityService();
    NS_ENSURE_TRUE(accService, NS_ERROR_FAILURE);

    nsCOMPtr<nsIAccessible> accessible;
    nsresult rv =
      accService->GetAccessibleFor(node->node, getter_AddRefs(accessible));
    if (NS_SUCCEEDED(rv) && accessible)
      aProps.AppendLiteral(" ACCESSIBLE_NODE");
  }
#endif

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetColumnProperties(nsITreeColumn* col, nsAString& aProps)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetImageSrc(int32_t row, nsITreeColumn* col, nsAString& _retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetProgressMode(int32_t row, nsITreeColumn* col, int32_t* _retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetCellValue(int32_t row, nsITreeColumn* col, nsAString& _retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetCellText(int32_t row, nsITreeColumn* col, nsAString& _retval)
{
  inDOMViewNode* node = nullptr;
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
    uint16_t nodeType;
    domNode->GetNodeType(&nodeType);
    nsAutoString temp;
    temp.AppendInt(int32_t(nodeType));
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
inDOMView::IsContainer(int32_t index, bool *_retval)
{
  inDOMViewNode* node = nullptr;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->isContainer;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsContainerOpen(int32_t index, bool *_retval)
{
  inDOMViewNode* node = nullptr;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->isOpen;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsContainerEmpty(int32_t index, bool *_retval)
{
  inDOMViewNode* node = nullptr;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->isContainer ? false : true;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetLevel(int32_t index, int32_t *_retval)
{
  inDOMViewNode* node = nullptr;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->level;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::GetParentIndex(int32_t rowIndex, int32_t *_retval)
{
  inDOMViewNode* node = nullptr;
  RowToNode(rowIndex, &node);
  if (!node) return NS_ERROR_FAILURE;

  // GetParentIndex returns -1 if there is no parent  
  *_retval = -1;
  
  inDOMViewNode* checkNode = nullptr;
  int32_t i = rowIndex - 1;
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
inDOMView::HasNextSibling(int32_t rowIndex, int32_t afterIndex, bool *_retval)
{
  inDOMViewNode* node = nullptr;
  RowToNode(rowIndex, &node);
  if (!node) return NS_ERROR_FAILURE;

  *_retval = node->next != nullptr;

  return NS_OK;
}

NS_IMETHODIMP
inDOMView::ToggleOpenState(int32_t index)
{
  inDOMViewNode* node = nullptr;
  RowToNode(index, &node);
  if (!node) return NS_ERROR_FAILURE;

  int32_t oldCount = GetRowCount();
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
inDOMView::SetCellValue(int32_t row, nsITreeColumn* col, const nsAString& value)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::SetCellText(int32_t row, nsITreeColumn* col, const nsAString& value)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::CycleHeader(nsITreeColumn* col)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::CycleCell(int32_t row, nsITreeColumn* col)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsEditable(int32_t row, nsITreeColumn* col, bool *_retval)
{
  return NS_OK;
}


NS_IMETHODIMP
inDOMView::IsSelectable(int32_t row, nsITreeColumn* col, bool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsSeparator(int32_t index, bool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::IsSorted(bool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::CanDrop(int32_t index, int32_t orientation,
                   nsIDOMDataTransfer* aDataTransfer, bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::Drop(int32_t row, int32_t orientation, nsIDOMDataTransfer* aDataTransfer)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::PerformAction(const char16_t *action)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::PerformActionOnRow(const char16_t *action, int32_t row)
{
  return NS_OK;
}

NS_IMETHODIMP
inDOMView::PerformActionOnCell(const char16_t* action, int32_t row, nsITreeColumn* col)
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
                            int32_t aNameSpaceID, nsIAtom* aAttribute,
                            int32_t aModType)
{
  if (!mTree) {
    return;
  }

  if (!(mWhatToShow & nsIDOMNodeFilter::SHOW_ATTRIBUTE)) {
    return;
  }

  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  
  // get the dom attribute node, if there is any
  nsCOMPtr<nsIDOMElement> el(do_QueryInterface(aElement));
  nsCOMPtr<nsIDOMAttr> domAttr;
  nsDependentAtomString attrStr(aAttribute);
  if (aNameSpaceID) {
    nsNameSpaceManager* nsm = nsNameSpaceManager::GetInstance();
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
    int32_t row = 0;
    NodeToRow(domAttr, &row);
    mTree->InvalidateRange(row, row);
  } else if (aModType == nsIDOMMutationEvent::ADDITION) {
    if (!domAttr) {
      return;
    }
    // get the number of attributes on this content node
    nsCOMPtr<nsIDOMMozNamedAttrMap> attrs;
    el->GetAttributes(getter_AddRefs(attrs));
    uint32_t attrCount;
    attrs->GetLength(&attrCount);

    inDOMViewNode* contentNode = nullptr;
    int32_t contentRow;
    int32_t attrRow;
    if (mRootNode == el &&
        !(mWhatToShow & nsIDOMNodeFilter::SHOW_ELEMENT)) {
      // if this view has a root node but is not displaying it,
      // it is ok to act as if the changed attribute is on the root.
      attrRow = attrCount - 1;
    } else {
      if (NS_FAILED(NodeToRow(el, &contentRow))) {
        return;
      }
      RowToNode(contentRow, &contentNode);
      if (!contentNode->isOpen) {
        return;
      }
      attrRow = contentRow + attrCount;
    }

    inDOMViewNode* newNode = CreateNode(domAttr, contentNode);
    inDOMViewNode* insertNode = nullptr;
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
    inDOMViewNode* contentNode = nullptr;
    int32_t contentRow;
    int32_t baseLevel;
    if (NS_SUCCEEDED(NodeToRow(el, &contentRow))) {
      RowToNode(contentRow, &contentNode);
      baseLevel = contentNode->level;
    } else {
      if (mRootNode == el) {
        contentRow = -1;
        baseLevel = -1;
      } else
        return;
    }

    // search for the attribute node that was removed
    inDOMViewNode* checkNode = nullptr;
    int32_t row = 0;
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
                           int32_t /* unused */)
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
                           nsIContent* aChild, int32_t /* unused */)
{
  if (!mTree)
    return;

  nsresult rv;
  nsCOMPtr<nsIDOMNode> childDOMNode(do_QueryInterface(aChild));
  nsCOMPtr<nsIDOMNode> parent;
  if (!mDOMUtils) {
    mDOMUtils = services::GetInDOMUtils();
    if (!mDOMUtils) {
      return;
    }
  }
  mDOMUtils->GetParentForNode(childDOMNode, mShowAnonymous,
                              getter_AddRefs(parent));

  // find the inDOMViewNode for the parent of the inserted content
  int32_t parentRow = 0;
  if (NS_FAILED(rv = NodeToRow(parent, &parentRow)))
    return;
  inDOMViewNode* parentNode = nullptr;
  if (NS_FAILED(rv = RowToNode(parentRow, &parentNode)))
    return;

  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  
  if (!parentNode->isOpen) {
    // Parent is not open, so don't bother creating tree rows for the
    // kids.  But do indicate that it's now a container, if needed.
    if (!parentNode->isContainer) {
      parentNode->isContainer = true;
      mTree->InvalidateRow(parentRow);
    }
    return;
  }

  // get the previous sibling of the inserted content
  nsCOMPtr<nsIDOMNode> previous;
  GetRealPreviousSibling(childDOMNode, parent, getter_AddRefs(previous));
  inDOMViewNode* previousNode = nullptr;

  int32_t row = 0;
  if (previous) {
    // find the inDOMViewNode for the previous sibling of the inserted content
    int32_t previousRow = 0;
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
    int32_t firstChildRow;
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
                          nsIContent* aChild, int32_t aIndexInContainer,
                          nsIContent* aPreviousSibling)
{
  if (!mTree)
    return;

  nsresult rv;

  // find the inDOMViewNode for the old child
  nsCOMPtr<nsIDOMNode> oldDOMNode(do_QueryInterface(aChild));
  int32_t row = 0;
  if (NS_FAILED(rv = NodeToRow(oldDOMNode, &row)))
    return;
  inDOMViewNode* oldNode;
  if (NS_FAILED(rv = RowToNode(row, &oldNode)))
    return;

  nsCOMPtr<nsIMutationObserver> kungFuDeathGrip(this);
  
  // The parent may no longer be a container.  Note that we don't want
  // to access oldNode after calling RemoveNode, so do this now.
  inDOMViewNode* parentNode = oldNode->parent;
  bool isOnlyChild = oldNode->previous == nullptr && oldNode->next == nullptr;
  
  // Keep track of how many rows we are removing.  It's at least one,
  // but if we're open it's more.
  int32_t oldCount = GetRowCount();
  
  if (oldNode->isOpen)
    CollapseNode(row);

  RemoveLink(oldNode);
  RemoveNode(row);

  if (isOnlyChild) {
    // Fix up the parent
    parentNode->isContainer = false;
    parentNode->isOpen = false;
    mTree->InvalidateRow(NodeToRow(parentNode));
  }
    
  mTree->RowCountChanged(row, GetRowCount() - oldCount);
}

///////////////////////////////////////////////////////////////////////
// inDOMView

//////// NODE MANAGEMENT

inDOMViewNode*
inDOMView::GetNodeAt(int32_t aRow)
{
  return mNodes.ElementAt(aRow);
}

int32_t
inDOMView::GetRowCount()
{
  return mNodes.Length();
}

int32_t
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
inDOMView::RowOutOfBounds(int32_t aRow, int32_t aCount)
{
  return aRow < 0 || aRow >= GetRowCount() || aCount+aRow > GetRowCount();
}

void
inDOMView::AppendNode(inDOMViewNode* aNode)
{
  mNodes.AppendElement(aNode);
}

void
inDOMView::InsertNode(inDOMViewNode* aNode, int32_t aRow)
{
  if (RowOutOfBounds(aRow, 1))
    AppendNode(aNode);
  else
    mNodes.InsertElementAt(aRow, aNode);
}

void
inDOMView::RemoveNode(int32_t aRow)
{
  if (RowOutOfBounds(aRow, 1))
    return;

  delete GetNodeAt(aRow);
  mNodes.RemoveElementAt(aRow);
}

void
inDOMView::ReplaceNode(inDOMViewNode* aNode, int32_t aRow)
{
  if (RowOutOfBounds(aRow, 1))
    return;

  delete GetNodeAt(aRow);
  mNodes.ElementAt(aRow) = aNode;
}

void
inDOMView::InsertNodes(nsTArray<inDOMViewNode*>& aNodes, int32_t aRow)
{
  if (aRow < 0 || aRow > GetRowCount())
    return;

  mNodes.InsertElementsAt(aRow, aNodes);
}

void
inDOMView::RemoveNodes(int32_t aRow, int32_t aCount)
{
  if (aRow < 0)
    return;

  int32_t rowCount = GetRowCount();
  for (int32_t i = aRow; i < aRow+aCount && i < rowCount; ++i) {
    delete GetNodeAt(i);
  }

  mNodes.RemoveElementsAt(aRow, aCount);
}

void
inDOMView::RemoveAllNodes()
{
  int32_t rowCount = GetRowCount();
  for (int32_t i = 0; i < rowCount; ++i) {
    delete GetNodeAt(i);
  }

  mNodes.Clear();
}

void
inDOMView::ExpandNode(int32_t aRow)
{
  inDOMViewNode* node = nullptr;
  RowToNode(aRow, &node);

  nsCOMArray<nsIDOMNode> kids;
  GetChildNodesFor(node ? node->node : mRootNode,
                   kids);
  int32_t kidCount = kids.Count();

  nsTArray<inDOMViewNode*> list(kidCount);

  inDOMViewNode* newNode = nullptr;
  inDOMViewNode* prevNode = nullptr;

  for (int32_t i = 0; i < kidCount; ++i) {
    newNode = CreateNode(kids[i], node);
    list.AppendElement(newNode);

    if (prevNode)
      prevNode->next = newNode;
    newNode->previous = prevNode;
    prevNode = newNode;
  }

  InsertNodes(list, aRow+1);

  if (node)
    node->isOpen = true;
}

void
inDOMView::CollapseNode(int32_t aRow)
{
  inDOMViewNode* node = nullptr;
  nsresult rv = RowToNode(aRow, &node);
  if (NS_FAILED(rv)) {
    return;
  }

  int32_t row = 0;
  GetLastDescendantOf(node, aRow, &row);

  RemoveNodes(aRow+1, row-aRow);

  node->isOpen = false;
}

//////// NODE AND ROW CONVERSION

nsresult
inDOMView::RowToNode(int32_t aRow, inDOMViewNode** aNode)
{
  if (aRow < 0 || aRow >= GetRowCount())
    return NS_ERROR_FAILURE;

  *aNode = GetNodeAt(aRow);
  return NS_OK;
}

nsresult
inDOMView::NodeToRow(nsIDOMNode* aNode, int32_t* aRow)
{
  int32_t rowCount = GetRowCount();
  for (int32_t i = 0; i < rowCount; ++i) {
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
inDOMView::GetFirstDescendantOf(inDOMViewNode* aNode, int32_t aRow, int32_t* aResult)
{
  // get the first node that is a descendant of the previous sibling
  int32_t row = 0;
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
inDOMView::GetLastDescendantOf(inDOMViewNode* aNode, int32_t aRow, int32_t* aResult)
{
  // get the last node that is a descendant of the previous sibling
  int32_t row = 0;
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
  // attribute nodes
  if (mWhatToShow & nsIDOMNodeFilter::SHOW_ATTRIBUTE) {
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(aNode);
    if (element) {
      nsCOMPtr<nsIDOMMozNamedAttrMap> attrs;
      element->GetAttributes(getter_AddRefs(attrs));
      if (attrs) {
        AppendAttrsToArray(attrs, aResult);
      }
    }
  }

  if (mWhatToShow & nsIDOMNodeFilter::SHOW_ELEMENT) {
    nsCOMPtr<nsIDOMNodeList> kids;
    if (!mDOMUtils) {
      mDOMUtils = services::GetInDOMUtils();
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
  uint32_t l = 0;
  aKids->GetLength(&l);
  nsCOMPtr<nsIDOMNode> kid;
  uint16_t nodeType = 0;

  // Try and get DOM Utils in case we don't have one yet.
  if (!mShowWhitespaceNodes && !mDOMUtils) {
    mDOMUtils = services::GetInDOMUtils();
  }

  for (uint32_t i = 0; i < l; ++i) {
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
    uint32_t filterForNodeType = 1 << (nodeType - 1);

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
inDOMView::AppendAttrsToArray(nsIDOMMozNamedAttrMap* aAttributes,
                              nsCOMArray<nsIDOMNode>& aArray)
{
  uint32_t l = 0;
  aAttributes->GetLength(&l);
  nsCOMPtr<nsIDOMAttr> attribute;
  for (uint32_t i = 0; i < l; ++i) {
    aAttributes->Item(i, getter_AddRefs(attribute));
    aArray.AppendObject(attribute);
  }
  return NS_OK;
}
