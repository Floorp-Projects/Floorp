/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "inDeepTreeWalker.h"
#include "inLayoutUtils.h"

#include "nsString.h"
#include "nsIDOMDocument.h"
#include "nsIDOMNodeFilter.h"
#include "nsIDOMNodeList.h"
#include "nsServiceManagerUtils.h"
#include "nsIContent.h"
#include "ChildIterator.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/InspectorUtils.h"

/*****************************************************************************
 * This implementation does not currently operaate according to the W3C spec.
 * In particular it does NOT handle DOM mutations during the walk.  It also
 * ignores whatToShow and the filter.
 *****************************************************************************/

////////////////////////////////////////////////////

inDeepTreeWalker::inDeepTreeWalker()
  : mShowAnonymousContent(false),
    mShowSubDocuments(false),
    mShowDocumentsAsNodes(false),
    mWhatToShow(nsIDOMNodeFilter::SHOW_ALL)
{
}

inDeepTreeWalker::~inDeepTreeWalker()
{
}

NS_IMPL_ISUPPORTS(inDeepTreeWalker,
                  inIDeepTreeWalker)

////////////////////////////////////////////////////
// inIDeepTreeWalker

NS_IMETHODIMP
inDeepTreeWalker::GetShowAnonymousContent(bool *aShowAnonymousContent)
{
  *aShowAnonymousContent = mShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::SetShowAnonymousContent(bool aShowAnonymousContent)
{
  mShowAnonymousContent = aShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::GetShowSubDocuments(bool *aShowSubDocuments)
{
  *aShowSubDocuments = mShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::SetShowSubDocuments(bool aShowSubDocuments)
{
  mShowSubDocuments = aShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::GetShowDocumentsAsNodes(bool *aShowDocumentsAsNodes)
{
  *aShowDocumentsAsNodes = mShowDocumentsAsNodes;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::SetShowDocumentsAsNodes(bool aShowDocumentsAsNodes)
{
  mShowDocumentsAsNodes = aShowDocumentsAsNodes;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::Init(nsIDOMNode* aRoot, uint32_t aWhatToShow)
{
  if (!aRoot) {
    return NS_ERROR_INVALID_ARG;
  }

  mRoot = aRoot;
  mCurrentNode = aRoot;
  mWhatToShow = aWhatToShow;

  return NS_OK;
}

////////////////////////////////////////////////////
// nsIDOMTreeWalker

NS_IMETHODIMP
inDeepTreeWalker::GetRoot(nsIDOMNode** aRoot)
{
  *aRoot = mRoot;
  NS_IF_ADDREF(*aRoot);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::GetWhatToShow(uint32_t* aWhatToShow)
{
  *aWhatToShow = mWhatToShow;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::GetFilter(nsIDOMNodeFilter** aFilter)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
inDeepTreeWalker::GetCurrentNode(nsIDOMNode** aCurrentNode)
{
  *aCurrentNode = mCurrentNode;
  NS_IF_ADDREF(*aCurrentNode);
  return NS_OK;
}

already_AddRefed<nsIDOMNode>
inDeepTreeWalker::GetParent()
{
  MOZ_ASSERT(mCurrentNode);

  if (mCurrentNode == mRoot) {
    return nullptr;
  }

  nsCOMPtr<nsINode> currentNode = do_QueryInterface(mCurrentNode);
  nsCOMPtr<nsINode> root = do_QueryInterface(mRoot);

  nsCOMPtr<nsIDOMNode> parent;
  nsINode* parentNode =
    InspectorUtils::GetParentForNode(*currentNode, mShowAnonymousContent);

  uint16_t nodeType = 0;
  if (parentNode) {
    nodeType = parentNode->NodeType();
  }
  // For compatibility reasons by default we skip the document nodes
  // from the walk.
  if (!mShowDocumentsAsNodes &&
      nodeType == nsIDOMNode::DOCUMENT_NODE &&
      parentNode != root) {
    parentNode =
      InspectorUtils::GetParentForNode(*parentNode, mShowAnonymousContent);
  }

  parent = do_QueryInterface(parentNode);
  return parent.forget();
}

static already_AddRefed<nsINodeList>
GetChildren(nsIDOMNode* aParent,
            bool aShowAnonymousContent,
            bool aShowSubDocuments)
{
  MOZ_ASSERT(aParent);

  nsCOMPtr<nsINodeList> ret;
  if (aShowSubDocuments) {
    nsCOMPtr<nsIDOMDocument> domdoc = inLayoutUtils::GetSubDocumentFor(aParent);
    if (domdoc) {
      aParent = domdoc;
    }
  }

  nsCOMPtr<nsIContent> parentAsContent = do_QueryInterface(aParent);
  if (parentAsContent && aShowAnonymousContent) {
      ret = parentAsContent->GetChildren(nsIContent::eAllChildren);
  } else {
    // If it's not a content, then it's a document (or an attribute but we can ignore that
    // case here). If aShowAnonymousContent is false we also want to fall back to ChildNodes
    // so we can skip any native anon content that GetChildren would return.
    nsCOMPtr<nsINode> parentNode = do_QueryInterface(aParent);
    MOZ_ASSERT(parentNode);
    ret = parentNode->ChildNodes();
  }
  return ret.forget();
}

NS_IMETHODIMP
inDeepTreeWalker::SetCurrentNode(nsIDOMNode* aCurrentNode)
{
  // mCurrentNode can only be null if init either failed, or has not been
  // called yet.
  if (!mCurrentNode || !aCurrentNode) {
    return NS_ERROR_FAILURE;
  }

  // If Document nodes are skipped by the walk, we should not allow
  // one to set one as the current node either.
  uint16_t nodeType = 0;
  aCurrentNode->GetNodeType(&nodeType);
  if (!mShowDocumentsAsNodes && nodeType == nsIDOMNode::DOCUMENT_NODE) {
    return NS_ERROR_FAILURE;
  }

  return SetCurrentNode(aCurrentNode, nullptr);
}


nsresult
inDeepTreeWalker::SetCurrentNode(nsIDOMNode* aCurrentNode,
                                 nsINodeList* aSiblings)
{
  MOZ_ASSERT(aCurrentNode);

  // We want to store the original state so in case of error
  // we can restore that.
  nsCOMPtr<nsINodeList> tmpSiblings = mSiblings;
  nsCOMPtr<nsIDOMNode> tmpCurrent = mCurrentNode;
  mSiblings = aSiblings;
  mCurrentNode = aCurrentNode;

  // If siblings were not passed in as argument we have to
  // get them from the parent node of aCurrentNode.
  // Note: in the mShowDoucmentsAsNodes case when a sub document
  // is set as the current, we don't want to get the children
  // from the iframe accidentally here, so let's just skip this
  // part for document nodes, they should never have siblings.
  uint16_t nodeType = 0;
  aCurrentNode->GetNodeType(&nodeType);
  if (!mSiblings && nodeType != nsIDOMNode::DOCUMENT_NODE) {
    nsCOMPtr<nsIDOMNode> parent = GetParent();
    if (parent) {
      mSiblings = GetChildren(parent,
                              mShowAnonymousContent,
                              mShowSubDocuments);
    }
  }

  if (mSiblings && mSiblings->Length()) {
    // We cached all the siblings (if there are any) of the current node, but we
    // still have to set the index too, to be able to iterate over them.
    nsCOMPtr<nsIContent> currentAsContent = do_QueryInterface(mCurrentNode);
    MOZ_ASSERT(currentAsContent);
    int32_t index = mSiblings->IndexOf(currentAsContent);
    if (index < 0) {
      // If someone tries to set current node to some value that is not reachable
      // otherwise, let's throw. (For example mShowAnonymousContent is false and some
      // XBL anon content was passed in)

      // Restore state first.
      mCurrentNode = tmpCurrent;
      mSiblings = tmpSiblings;
      return NS_ERROR_INVALID_ARG;
    }
    mCurrentIndex = index;
  } else {
    mCurrentIndex = -1;
  }
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::ParentNode(nsIDOMNode** _retval)
{
  *_retval = nullptr;
  if (!mCurrentNode || mCurrentNode == mRoot) {
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> parent = GetParent();

  if (!parent) {
    return NS_OK;
  }

  nsresult rv = SetCurrentNode(parent);
  NS_ENSURE_SUCCESS(rv,rv);

  parent.forget(_retval);
  return NS_OK;
}

// FirstChild and LastChild are very similar methods, this is the generic
// version for internal use. With aReverse = true it returns the LastChild.
nsresult
inDeepTreeWalker::EdgeChild(nsIDOMNode** _retval, bool aFront)
{
  if (!mCurrentNode) {
    return NS_ERROR_FAILURE;
  }

  *_retval = nullptr;

  nsCOMPtr<nsIDOMNode> echild;
  if (mShowSubDocuments && mShowDocumentsAsNodes) {
    // GetChildren below, will skip the document node from
    // the walk. But if mShowDocumentsAsNodes is set to true
    // we want to include the (sub)document itself too.
    echild = inLayoutUtils::GetSubDocumentFor(mCurrentNode);
  }

  nsCOMPtr<nsINodeList> children;
  if (!echild) {
    children = GetChildren(mCurrentNode,
                           mShowAnonymousContent,
                           mShowSubDocuments);
    if (children && children->Length() > 0) {
      nsINode* childNode = children->Item(aFront ? 0 : children->Length() - 1);
      echild = childNode ? childNode->AsDOMNode() : nullptr;
    }
  }

  if (echild) {
    nsresult rv = SetCurrentNode(echild, children);
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ADDREF(*_retval = mCurrentNode);
  }

  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::FirstChild(nsIDOMNode** _retval)
{
  return EdgeChild(_retval, /* aFront = */ true);
}

NS_IMETHODIMP
inDeepTreeWalker::LastChild(nsIDOMNode **_retval)
{
  return EdgeChild(_retval, /* aFront = */ false);
}

NS_IMETHODIMP
inDeepTreeWalker::PreviousSibling(nsIDOMNode **_retval)
{
  *_retval = nullptr;
  if (!mCurrentNode || !mSiblings || mCurrentIndex < 1) {
    return NS_OK;
  }

  nsIContent* prev = mSiblings->Item(--mCurrentIndex);
  mCurrentNode = prev->AsDOMNode();
  NS_ADDREF(*_retval = mCurrentNode);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::NextSibling(nsIDOMNode **_retval)
{
  *_retval = nullptr;
  if (!mCurrentNode || !mSiblings ||
      mCurrentIndex + 1 >= (int32_t) mSiblings->Length()) {
    return NS_OK;
  }

  nsIContent* next = mSiblings->Item(++mCurrentIndex);
  mCurrentNode = next->AsDOMNode();
  NS_ADDREF(*_retval = mCurrentNode);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::PreviousNode(nsIDOMNode **_retval)
{
  if (!mCurrentNode || mCurrentNode == mRoot) {
    // Nowhere to go from here
    *_retval = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsIDOMNode> node;
  PreviousSibling(getter_AddRefs(node));

  if (!node) {
    return ParentNode(_retval);
  }

  // Now we're positioned at our previous sibling.  But since the DOM tree
  // traversal is depth-first, the previous node is its most deeply nested last
  // child.  Just loop until LastChild() returns null; since the LastChild()
  // call that returns null won't affect our position, we will then be
  // positioned at the correct node.
  while (node) {
    LastChild(getter_AddRefs(node));
  }

  NS_ADDREF(*_retval = mCurrentNode);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::NextNode(nsIDOMNode **_retval)
{
  if (!mCurrentNode) {
    return NS_OK;
  }

  // First try our kids
  FirstChild(_retval);

  if (*_retval) {
    return NS_OK;
  }

  // Now keep trying next siblings up the parent chain, but if we
  // discover there's nothing else restore our state.
#ifdef DEBUG
  nsIDOMNode* origCurrentNode = mCurrentNode;
#endif
  uint32_t lastChildCallsToMake = 0;
  while (1) {
    NextSibling(_retval);

    if (*_retval) {
      return NS_OK;
    }

    nsCOMPtr<nsIDOMNode> parent;
    ParentNode(getter_AddRefs(parent));
    if (!parent) {
      // Nowhere else to go; we're done.  Restore our state.
      while (lastChildCallsToMake--) {
        nsCOMPtr<nsIDOMNode> dummy;
        LastChild(getter_AddRefs(dummy));
      }
      NS_ASSERTION(mCurrentNode == origCurrentNode,
                   "Didn't go back to the right node?");
      *_retval = nullptr;
      return NS_OK;
    }
    ++lastChildCallsToMake;
  }

  NS_NOTREACHED("how did we get here?");
  return NS_OK;
}
