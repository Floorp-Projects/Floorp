/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "inDeepTreeWalker.h"
#include "inLayoutUtils.h"

#include "nsString.h"
#include "nsIDocument.h"
#include "nsServiceManagerUtils.h"
#include "nsIContent.h"
#include "ChildIterator.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/InspectorUtils.h"
#include "mozilla/dom/NodeFilterBinding.h"

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
    mWhatToShow(mozilla::dom::NodeFilter_Binding::SHOW_ALL)
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
inDeepTreeWalker::Init(nsINode* aRoot, uint32_t aWhatToShow)
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

NS_IMETHODIMP
inDeepTreeWalker::GetRoot(nsINode** aRoot)
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
inDeepTreeWalker::GetCurrentNode(nsINode** aCurrentNode)
{
  *aCurrentNode = mCurrentNode;
  NS_IF_ADDREF(*aCurrentNode);
  return NS_OK;
}

already_AddRefed<nsINode>
inDeepTreeWalker::GetParent()
{
  MOZ_ASSERT(mCurrentNode);

  if (mCurrentNode == mRoot) {
    return nullptr;
  }

  nsINode* parentNode =
    InspectorUtils::GetParentForNode(*mCurrentNode, mShowAnonymousContent);

  uint16_t nodeType = 0;
  if (parentNode) {
    nodeType = parentNode->NodeType();
  }
  // For compatibility reasons by default we skip the document nodes
  // from the walk.
  if (!mShowDocumentsAsNodes &&
      nodeType == nsINode::DOCUMENT_NODE &&
      parentNode != mRoot) {
    parentNode =
      InspectorUtils::GetParentForNode(*parentNode, mShowAnonymousContent);
  }

  return do_AddRef(parentNode);
}

static already_AddRefed<nsINodeList>
GetChildren(nsINode* aParent,
            bool aShowAnonymousContent,
            bool aShowSubDocuments)
{
  MOZ_ASSERT(aParent);

  nsCOMPtr<nsINodeList> ret;
  if (aShowSubDocuments) {
    nsIDocument* domdoc = inLayoutUtils::GetSubDocumentFor(aParent);
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
    ret = aParent->ChildNodes();
  }
  return ret.forget();
}

NS_IMETHODIMP
inDeepTreeWalker::SetCurrentNode(nsINode* aCurrentNode)
{
  // mCurrentNode can only be null if init either failed, or has not been
  // called yet.
  if (!mCurrentNode || !aCurrentNode) {
    return NS_ERROR_FAILURE;
  }

  // If Document nodes are skipped by the walk, we should not allow
  // one to set one as the current node either.
  if (!mShowDocumentsAsNodes) {
    if (aCurrentNode->NodeType() == nsINode::DOCUMENT_NODE) {
      return NS_ERROR_FAILURE;
    }
  }

  return SetCurrentNode(aCurrentNode, nullptr);
}


nsresult
inDeepTreeWalker::SetCurrentNode(nsINode* aCurrentNode,
                                 nsINodeList* aSiblings)
{
  MOZ_ASSERT(aCurrentNode);

  // We want to store the original state so in case of error
  // we can restore that.
  nsCOMPtr<nsINodeList> tmpSiblings = mSiblings;
  nsCOMPtr<nsINode> tmpCurrent = mCurrentNode;
  mSiblings = aSiblings;
  mCurrentNode = aCurrentNode;

  // If siblings were not passed in as argument we have to
  // get them from the parent node of aCurrentNode.
  // Note: in the mShowDoucmentsAsNodes case when a sub document
  // is set as the current, we don't want to get the children
  // from the iframe accidentally here, so let's just skip this
  // part for document nodes, they should never have siblings.
  if (!mSiblings) {
    if (aCurrentNode->NodeType() != nsINode::DOCUMENT_NODE) {
      nsCOMPtr<nsINode> parent = GetParent();
      if (parent) {
        mSiblings = GetChildren(parent,
                                mShowAnonymousContent,
                                mShowSubDocuments);
      }
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
inDeepTreeWalker::ParentNode(nsINode** _retval)
{
  *_retval = nullptr;
  if (!mCurrentNode || mCurrentNode == mRoot) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> parent = GetParent();

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
inDeepTreeWalker::EdgeChild(nsINode** _retval, bool aFront)
{
  if (!mCurrentNode) {
    return NS_ERROR_FAILURE;
  }

  *_retval = nullptr;

  nsCOMPtr<nsINode> echild;
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
      echild = children->Item(aFront ? 0 : children->Length() - 1);
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
inDeepTreeWalker::FirstChild(nsINode** _retval)
{
  return EdgeChild(_retval, /* aFront = */ true);
}

NS_IMETHODIMP
inDeepTreeWalker::LastChild(nsINode **_retval)
{
  return EdgeChild(_retval, /* aFront = */ false);
}

NS_IMETHODIMP
inDeepTreeWalker::PreviousSibling(nsINode **_retval)
{
  *_retval = nullptr;
  if (!mCurrentNode || !mSiblings || mCurrentIndex < 1) {
    return NS_OK;
  }

  nsIContent* prev = mSiblings->Item(--mCurrentIndex);
  mCurrentNode = prev;
  NS_ADDREF(*_retval = mCurrentNode);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::NextSibling(nsINode **_retval)
{
  *_retval = nullptr;
  if (!mCurrentNode || !mSiblings ||
      mCurrentIndex + 1 >= (int32_t) mSiblings->Length()) {
    return NS_OK;
  }

  nsIContent* next = mSiblings->Item(++mCurrentIndex);
  mCurrentNode = next;
  NS_ADDREF(*_retval = mCurrentNode);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::PreviousNode(nsINode **_retval)
{
  if (!mCurrentNode || mCurrentNode == mRoot) {
    // Nowhere to go from here
    *_retval = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsINode> node;
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
inDeepTreeWalker::NextNode(nsINode **_retval)
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
  nsINode* origCurrentNode = mCurrentNode;
#endif
  uint32_t lastChildCallsToMake = 0;
  while (1) {
    NextSibling(_retval);

    if (*_retval) {
      return NS_OK;
    }

    nsCOMPtr<nsINode> parent;
    ParentNode(getter_AddRefs(parent));
    if (!parent) {
      // Nowhere else to go; we're done.  Restore our state.
      while (lastChildCallsToMake--) {
        nsCOMPtr<nsINode> dummy;
        LastChild(getter_AddRefs(dummy));
      }
      NS_ASSERTION(mCurrentNode == origCurrentNode,
                   "Didn't go back to the right node?");
      *_retval = nullptr;
      return NS_OK;
    }
    ++lastChildCallsToMake;
  }

  MOZ_ASSERT_UNREACHABLE("how did we get here?");
  return NS_OK;
}
