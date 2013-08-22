/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Traversal's nsIDOMNodeIterator
 */

#include "mozilla/dom/NodeIterator.h"

#include "nsIDOMNode.h"
#include "nsError.h"

#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsContentUtils.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/NodeIteratorBinding.h"

namespace mozilla {
namespace dom {

/*
 * NodePointer implementation
 */
NodeIterator::NodePointer::NodePointer(nsINode *aNode, bool aBeforeNode) :
    mNode(aNode),
    mBeforeNode(aBeforeNode)
{
}

bool NodeIterator::NodePointer::MoveToNext(nsINode *aRoot)
{
    if (!mNode)
      return false;

    if (mBeforeNode) {
        mBeforeNode = false;
        return true;
    }

    nsINode* child = mNode->GetFirstChild();
    if (child) {
        mNode = child;
        return true;
    }

    return MoveForward(aRoot, mNode);
}

bool NodeIterator::NodePointer::MoveToPrevious(nsINode *aRoot)
{
    if (!mNode)
      return false;

    if (!mBeforeNode) {
        mBeforeNode = true;
        return true;
    }

    if (mNode == aRoot)
        return false;

    MoveBackward(mNode->GetParentNode(), mNode->GetPreviousSibling());

    return true;
}

void NodeIterator::NodePointer::AdjustAfterRemoval(nsINode *aRoot,
                                                   nsINode *aContainer,
                                                   nsIContent *aChild,
                                                   nsIContent *aPreviousSibling)
{
    // If mNode is null or the root there is nothing to do.
    if (!mNode || mNode == aRoot)
        return;

    // check if ancestor was removed
    if (!nsContentUtils::ContentIsDescendantOf(mNode, aChild))
        return;

    if (mBeforeNode) {

        // Try the next sibling
        nsINode *nextSibling = aPreviousSibling ? aPreviousSibling->GetNextSibling()
                                                : aContainer->GetFirstChild();

        if (nextSibling) {
            mNode = nextSibling;
            return;
        }

        // Next try siblings of ancestors
        if (MoveForward(aRoot, aContainer))
            return;

        // No suitable node was found so try going backwards
        mBeforeNode = false;
    }

    MoveBackward(aContainer, aPreviousSibling);
}

bool NodeIterator::NodePointer::MoveForward(nsINode *aRoot, nsINode *aNode)
{
    while (1) {
        if (aNode == aRoot)
            break;

        nsINode *sibling = aNode->GetNextSibling();
        if (sibling) {
            mNode = sibling;
            return true;
        }
        aNode = aNode->GetParentNode();
    }

    return false;
}

void NodeIterator::NodePointer::MoveBackward(nsINode *aParent, nsINode *aNode)
{
    if (aNode) {
        do {
            mNode = aNode;
            aNode = aNode->GetLastChild();
        } while (aNode);
    } else {
        mNode = aParent;
    }
}

/*
 * Factories, constructors and destructors
 */

NodeIterator::NodeIterator(nsINode *aRoot,
                           uint32_t aWhatToShow,
                           const NodeFilterHolder &aFilter) :
    nsTraversal(aRoot, aWhatToShow, aFilter),
    mPointer(mRoot, true)
{
    aRoot->AddMutationObserver(this);
}

NodeIterator::~NodeIterator()
{
    /* destructor code */
    if (mRoot)
        mRoot->RemoveMutationObserver(this);
}

/*
 * nsISupports and cycle collection stuff
 */

NS_IMPL_CYCLE_COLLECTION_CLASS(NodeIterator)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(NodeIterator)
    if (tmp->mRoot)
        tmp->mRoot->RemoveMutationObserver(tmp);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRoot)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mFilter)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(NodeIterator)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRoot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFilter)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

// QueryInterface implementation for NodeIterator
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NodeIterator)
    NS_INTERFACE_MAP_ENTRY(nsIDOMNodeIterator)
    NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNodeIterator)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(NodeIterator)
NS_IMPL_CYCLE_COLLECTING_RELEASE(NodeIterator)

/* readonly attribute nsIDOMNode root; */
NS_IMETHODIMP NodeIterator::GetRoot(nsIDOMNode * *aRoot)
{
    NS_ADDREF(*aRoot = Root()->AsDOMNode());
    return NS_OK;
}

/* readonly attribute unsigned long whatToShow; */
NS_IMETHODIMP NodeIterator::GetWhatToShow(uint32_t *aWhatToShow)
{
    *aWhatToShow = WhatToShow();
    return NS_OK;
}

/* readonly attribute nsIDOMNodeFilter filter; */
NS_IMETHODIMP NodeIterator::GetFilter(nsIDOMNodeFilter **aFilter)
{
    NS_ENSURE_ARG_POINTER(aFilter);

    *aFilter = mFilter.ToXPCOMCallback().get();

    return NS_OK;
}

/* nsIDOMNode nextNode ()  raises (DOMException); */
NS_IMETHODIMP NodeIterator::NextNode(nsIDOMNode **_retval)
{
    return ImplNodeGetter(&NodeIterator::NextNode, _retval);
}

/* nsIDOMNode previousNode ()  raises (DOMException); */
NS_IMETHODIMP NodeIterator::PreviousNode(nsIDOMNode **_retval)
{
    return ImplNodeGetter(&NodeIterator::PreviousNode, _retval);
}

already_AddRefed<nsINode>
NodeIterator::NextOrPrevNode(NodePointer::MoveToMethodType aMove,
                             ErrorResult& aResult)
{
    if (mInAcceptNode) {
        aResult.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
        return nullptr;
    }

    mWorkingPointer = mPointer;

    struct AutoClear {
        NodePointer* mPtr;
        AutoClear(NodePointer* ptr) : mPtr(ptr) {}
       ~AutoClear() { mPtr->Clear(); }
    } ac(&mWorkingPointer);

    while ((mWorkingPointer.*aMove)(mRoot)) {
        nsCOMPtr<nsINode> testNode = mWorkingPointer.mNode;
        int16_t filtered = TestNode(testNode, aResult);
        if (aResult.Failed()) {
            return nullptr;
        }

        if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT) {
            mPointer = mWorkingPointer;
            return testNode.forget();
        }
    }

    return nullptr;
}

/* void detach (); */
NS_IMETHODIMP NodeIterator::Detach(void)
{
    if (mRoot) {
        mRoot->OwnerDoc()->WarnOnceAbout(nsIDocument::eNodeIteratorDetach);
    }
    return NS_OK;
}

/* readonly attribute nsIDOMNode referenceNode; */
NS_IMETHODIMP NodeIterator::GetReferenceNode(nsIDOMNode * *aRefNode)
{
    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(GetReferenceNode()));
    node.forget(aRefNode);
    return NS_OK;
}

/* readonly attribute boolean pointerBeforeReferenceNode; */
NS_IMETHODIMP NodeIterator::GetPointerBeforeReferenceNode(bool *aBeforeNode)
{
    *aBeforeNode = PointerBeforeReferenceNode();
    return NS_OK;
}

/*
 * nsIMutationObserver interface
 */

void NodeIterator::ContentRemoved(nsIDocument *aDocument,
                                  nsIContent *aContainer,
                                  nsIContent *aChild,
                                  int32_t aIndexInContainer,
                                  nsIContent *aPreviousSibling)
{
    nsINode *container = NODE_FROM(aContainer, aDocument);

    mPointer.AdjustAfterRemoval(mRoot, container, aChild, aPreviousSibling);
    mWorkingPointer.AdjustAfterRemoval(mRoot, container, aChild, aPreviousSibling);
}

JSObject*
NodeIterator::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope)
{
    return NodeIteratorBinding::Wrap(cx, scope, this);
}

} // namespace dom
} // namespace mozilla
