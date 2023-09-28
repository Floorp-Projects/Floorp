/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "inDeepTreeWalker.h"
#include "inLayoutUtils.h"

#include "mozilla/Try.h"
#include "mozilla/dom/CSSStyleRule.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/InspectorUtils.h"
#include "mozilla/dom/NodeFilterBinding.h"

#include "nsString.h"
#include "nsServiceManagerUtils.h"
#include "nsIContent.h"
#include "ChildIterator.h"

using mozilla::dom::InspectorUtils;

/*****************************************************************************
 * This implementation does not currently operaate according to the W3C spec.
 * In particular it does NOT handle DOM mutations during the walk.  It also
 * ignores whatToShow and the filter.
 *****************************************************************************/

////////////////////////////////////////////////////

inDeepTreeWalker::inDeepTreeWalker() = default;
inDeepTreeWalker::~inDeepTreeWalker() = default;

NS_IMPL_ISUPPORTS(inDeepTreeWalker, inIDeepTreeWalker)

////////////////////////////////////////////////////
// inIDeepTreeWalker

NS_IMETHODIMP
inDeepTreeWalker::GetShowAnonymousContent(bool* aShowAnonymousContent) {
  *aShowAnonymousContent = mShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::SetShowAnonymousContent(bool aShowAnonymousContent) {
  mShowAnonymousContent = aShowAnonymousContent;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::GetShowSubDocuments(bool* aShowSubDocuments) {
  *aShowSubDocuments = mShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::SetShowSubDocuments(bool aShowSubDocuments) {
  mShowSubDocuments = aShowSubDocuments;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::GetShowDocumentsAsNodes(bool* aShowDocumentsAsNodes) {
  *aShowDocumentsAsNodes = mShowDocumentsAsNodes;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::SetShowDocumentsAsNodes(bool aShowDocumentsAsNodes) {
  mShowDocumentsAsNodes = aShowDocumentsAsNodes;
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::Init(nsINode* aRoot) {
  if (!aRoot) {
    return NS_ERROR_INVALID_ARG;
  }

  mRoot = aRoot;
  mCurrentNode = aRoot;

  return NS_OK;
}

////////////////////////////////////////////////////

NS_IMETHODIMP
inDeepTreeWalker::GetRoot(nsINode** aRoot) {
  *aRoot = mRoot;
  NS_IF_ADDREF(*aRoot);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::GetCurrentNode(nsINode** aCurrentNode) {
  *aCurrentNode = mCurrentNode;
  NS_IF_ADDREF(*aCurrentNode);
  return NS_OK;
}

already_AddRefed<nsINode> inDeepTreeWalker::GetParent() {
  MOZ_ASSERT(mCurrentNode);

  if (mCurrentNode == mRoot) {
    return nullptr;
  }

  nsINode* parentNode =
      InspectorUtils::GetParentForNode(*mCurrentNode, mShowAnonymousContent);
  if (!parentNode) {
    return nullptr;
  }

  // For compatibility reasons by default we skip the document nodes
  // from the walk.
  if (!mShowDocumentsAsNodes && parentNode->IsDocument() &&
      parentNode != mRoot) {
    parentNode =
        InspectorUtils::GetParentForNode(*parentNode, mShowAnonymousContent);
  }

  return do_AddRef(parentNode);
}

void inDeepTreeWalker::GetChildren(nsINode& aParent, ChildList& aChildList) {
  aChildList.ClearAndRetainStorage();
  InspectorUtils::GetChildrenForNode(aParent, mShowAnonymousContent,
                                     /* aIncludeAssignedNodes = */ false,
                                     mShowSubDocuments, aChildList);
  if (aChildList.Length() == 1 && aChildList.ElementAt(0)->IsDocument() &&
      !mShowDocumentsAsNodes) {
    RefPtr parent = aChildList.ElementAt(0);
    aChildList.ClearAndRetainStorage();
    InspectorUtils::GetChildrenForNode(*parent, mShowAnonymousContent,
                                       /* aIncludeAssignedNodes = */ false,
                                       mShowSubDocuments, aChildList);
  }
}

NS_IMETHODIMP
inDeepTreeWalker::SetCurrentNode(nsINode* aCurrentNode) {
  // mCurrentNode can only be null if init either failed, or has not been called
  // yet.
  if (!mCurrentNode || !aCurrentNode) {
    return NS_ERROR_FAILURE;
  }

  // If Document nodes are skipped by the walk, we should not allow one to set
  // one as the current node either.
  if (!mShowDocumentsAsNodes) {
    if (aCurrentNode->IsDocument()) {
      return NS_ERROR_FAILURE;
    }
  }

  // We want to store the original state so in case of error
  // we can restore that.
  ChildList oldSiblings;
  mSiblings.SwapElements(oldSiblings);
  nsCOMPtr<nsINode> oldCurrent = std::move(mCurrentNode);

  mCurrentNode = aCurrentNode;
  if (RefPtr<nsINode> parent = GetParent()) {
    GetChildren(*parent, mSiblings);
    // We cached all the siblings (if there are any) of the current node, but we
    // still have to set the index too, to be able to iterate over them.
    int32_t index = mSiblings.IndexOf(mCurrentNode);
    if (index < 0) {
      // If someone tries to set current node to some value that is not
      // reachable otherwise, let's throw. (For example mShowAnonymousContent is
      // false and some NAC was passed in).
      // Restore state first.
      mCurrentNode = std::move(oldCurrent);
      oldSiblings.SwapElements(mSiblings);
      return NS_ERROR_INVALID_ARG;
    }
    mCurrentIndex = index;
  } else {
    mCurrentIndex = -1;
  }
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::ParentNode(nsINode** _retval) {
  *_retval = nullptr;
  if (!mCurrentNode || mCurrentNode == mRoot) {
    return NS_OK;
  }

  nsCOMPtr<nsINode> parent = GetParent();
  if (!parent) {
    return NS_OK;
  }

  MOZ_TRY(SetCurrentNode(parent));

  parent.forget(_retval);
  return NS_OK;
}

// FirstChild and LastChild are very similar methods, this is the generic
// version for internal use. With aReverse = true it returns the LastChild.
nsresult inDeepTreeWalker::EdgeChild(nsINode** _retval, bool aFront) {
  if (!mCurrentNode) {
    return NS_ERROR_FAILURE;
  }

  *_retval = nullptr;

  ChildList children;
  GetChildren(*mCurrentNode, children);
  if (children.IsEmpty()) {
    return NS_OK;
  }
  mSiblings = std::move(children);
  mCurrentIndex = aFront ? 0 : mSiblings.Length() - 1;
  mCurrentNode = mSiblings.ElementAt(mCurrentIndex);
  NS_ADDREF(*_retval = mCurrentNode);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::FirstChild(nsINode** _retval) {
  return EdgeChild(_retval, /* aFront = */ true);
}

NS_IMETHODIMP
inDeepTreeWalker::LastChild(nsINode** _retval) {
  return EdgeChild(_retval, /* aFront = */ false);
}

NS_IMETHODIMP
inDeepTreeWalker::PreviousSibling(nsINode** _retval) {
  *_retval = nullptr;
  if (!mCurrentNode || mCurrentIndex < 1) {
    return NS_OK;
  }

  nsINode* prev = mSiblings.ElementAt(--mCurrentIndex);
  mCurrentNode = prev;
  NS_ADDREF(*_retval = mCurrentNode);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::NextSibling(nsINode** _retval) {
  *_retval = nullptr;
  if (!mCurrentNode || mCurrentIndex + 1 >= (int32_t)mSiblings.Length()) {
    return NS_OK;
  }

  nsINode* next = mSiblings.ElementAt(++mCurrentIndex);
  mCurrentNode = next;
  NS_ADDREF(*_retval = mCurrentNode);
  return NS_OK;
}

NS_IMETHODIMP
inDeepTreeWalker::PreviousNode(nsINode** _retval) {
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
inDeepTreeWalker::NextNode(nsINode** _retval) {
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
