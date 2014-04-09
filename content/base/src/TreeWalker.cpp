/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementation of DOM Traversal's nsIDOMTreeWalker
 */

#include "mozilla/dom/TreeWalker.h"

#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsError.h"
#include "nsINode.h"
#include "nsContentUtils.h"
#include "mozilla/dom/TreeWalkerBinding.h"

namespace mozilla {
namespace dom {

/*
 * Factories, constructors and destructors
 */

TreeWalker::TreeWalker(nsINode *aRoot,
                       uint32_t aWhatToShow,
                       const NodeFilterHolder &aFilter) :
    nsTraversal(aRoot, aWhatToShow, aFilter),
    mCurrentNode(aRoot)
{
}

TreeWalker::~TreeWalker()
{
    /* destructor code */
}

/*
 * nsISupports and cycle collection stuff
 */

NS_IMPL_CYCLE_COLLECTION_3(TreeWalker, mFilter, mCurrentNode, mRoot)

// QueryInterface implementation for TreeWalker
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TreeWalker)
    NS_INTERFACE_MAP_ENTRY(nsIDOMTreeWalker)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMTreeWalker)
NS_INTERFACE_MAP_END

// Have to pass in dom::TreeWalker because a11y has an a11y::TreeWalker that
// passes TreeWalker so refcount logging would get confused on the name
// collision.
NS_IMPL_CYCLE_COLLECTING_ADDREF(dom::TreeWalker)
NS_IMPL_CYCLE_COLLECTING_RELEASE(dom::TreeWalker)



/*
 * nsIDOMTreeWalker Getters/Setters
 */

/* readonly attribute nsIDOMNode root; */
NS_IMETHODIMP TreeWalker::GetRoot(nsIDOMNode * *aRoot)
{
    NS_ADDREF(*aRoot = Root()->AsDOMNode());
    return NS_OK;
}

/* readonly attribute unsigned long whatToShow; */
NS_IMETHODIMP TreeWalker::GetWhatToShow(uint32_t *aWhatToShow)
{
    *aWhatToShow = WhatToShow();
    return NS_OK;
}

/* readonly attribute nsIDOMNodeFilter filter; */
NS_IMETHODIMP TreeWalker::GetFilter(nsIDOMNodeFilter * *aFilter)
{
    NS_ENSURE_ARG_POINTER(aFilter);

    *aFilter = mFilter.ToXPCOMCallback().take();

    return NS_OK;
}

/* attribute nsIDOMNode currentNode; */
NS_IMETHODIMP TreeWalker::GetCurrentNode(nsIDOMNode * *aCurrentNode)
{
    if (mCurrentNode) {
        return CallQueryInterface(mCurrentNode, aCurrentNode);
    }

    *aCurrentNode = nullptr;

    return NS_OK;
}
NS_IMETHODIMP TreeWalker::SetCurrentNode(nsIDOMNode * aCurrentNode)
{
    NS_ENSURE_TRUE(aCurrentNode, NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    NS_ENSURE_TRUE(mRoot, NS_ERROR_UNEXPECTED);

    nsCOMPtr<nsINode> node = do_QueryInterface(aCurrentNode);
    NS_ENSURE_TRUE(node, NS_ERROR_UNEXPECTED);

    ErrorResult rv;
    SetCurrentNode(*node, rv);
    return rv.ErrorCode();
}

void
TreeWalker::SetCurrentNode(nsINode& aNode, ErrorResult& aResult)
{
    aResult = nsContentUtils::CheckSameOrigin(mRoot, &aNode);
    if (aResult.Failed()) {
        return;
    }

    mCurrentNode = &aNode;
}

/*
 * nsIDOMTreeWalker functions
 */

/* nsIDOMNode parentNode (); */
NS_IMETHODIMP TreeWalker::ParentNode(nsIDOMNode **_retval)
{
    return ImplNodeGetter(&TreeWalker::ParentNode, _retval);
}

already_AddRefed<nsINode>
TreeWalker::ParentNode(ErrorResult& aResult)
{
    nsCOMPtr<nsINode> node = mCurrentNode;

    while (node && node != mRoot) {
        node = node->GetParentNode();

        if (node) {
            int16_t filtered = TestNode(node, aResult);
            if (aResult.Failed()) {
                return nullptr;
            }
            if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT) {
                mCurrentNode = node;
                return node.forget();
            }
        }
    }

    return nullptr;
}

/* nsIDOMNode firstChild (); */
NS_IMETHODIMP TreeWalker::FirstChild(nsIDOMNode **_retval)
{
    return ImplNodeGetter(&TreeWalker::FirstChild, _retval);
}

already_AddRefed<nsINode>
TreeWalker::FirstChild(ErrorResult& aResult)
{
    return FirstChildInternal(false, aResult);
}

/* nsIDOMNode lastChild (); */
NS_IMETHODIMP TreeWalker::LastChild(nsIDOMNode **_retval)
{
    return ImplNodeGetter(&TreeWalker::LastChild, _retval);
}

already_AddRefed<nsINode>
TreeWalker::LastChild(ErrorResult& aResult)
{
    return FirstChildInternal(true, aResult);
}

/* nsIDOMNode previousSibling (); */
NS_IMETHODIMP TreeWalker::PreviousSibling(nsIDOMNode **_retval)
{
    return ImplNodeGetter(&TreeWalker::PreviousSibling, _retval);
}

already_AddRefed<nsINode>
TreeWalker::PreviousSibling(ErrorResult& aResult)
{
    return NextSiblingInternal(true, aResult);
}

/* nsIDOMNode nextSibling (); */
NS_IMETHODIMP TreeWalker::NextSibling(nsIDOMNode **_retval)
{
    return ImplNodeGetter(&TreeWalker::NextSibling, _retval);
}

already_AddRefed<nsINode>
TreeWalker::NextSibling(ErrorResult& aResult)
{
    return NextSiblingInternal(false, aResult);
}

/* nsIDOMNode previousNode (); */
NS_IMETHODIMP TreeWalker::PreviousNode(nsIDOMNode **_retval)
{
    return ImplNodeGetter(&TreeWalker::PreviousNode, _retval);
}

already_AddRefed<nsINode>
TreeWalker::PreviousNode(ErrorResult& aResult)
{
    nsCOMPtr<nsINode> node = mCurrentNode;

    while (node != mRoot) {
        while (nsINode *previousSibling = node->GetPreviousSibling()) {
            node = previousSibling;

            int16_t filtered = TestNode(node, aResult);
            if (aResult.Failed()) {
                return nullptr;
            }

            nsINode *lastChild;
            while (filtered != nsIDOMNodeFilter::FILTER_REJECT &&
                   (lastChild = node->GetLastChild())) {
                node = lastChild;
                filtered = TestNode(node, aResult);
                if (aResult.Failed()) {
                    return nullptr;
                }
            }

            if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT) {
                mCurrentNode = node;
                return node.forget();
            }
        }

        if (node == mRoot) {
            break;
        }

        node = node->GetParentNode();
        if (!node) {
            break;
        }

        int16_t filtered = TestNode(node, aResult);
        if (aResult.Failed()) {
            return nullptr;
        }

        if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT) {
            mCurrentNode = node;
            return node.forget();
        }
    }

    return nullptr;
}

/* nsIDOMNode nextNode (); */
NS_IMETHODIMP TreeWalker::NextNode(nsIDOMNode **_retval)
{
    return ImplNodeGetter(&TreeWalker::NextNode, _retval);
}

already_AddRefed<nsINode>
TreeWalker::NextNode(ErrorResult& aResult)
{
    int16_t filtered = nsIDOMNodeFilter::FILTER_ACCEPT; // pre-init for inner loop

    nsCOMPtr<nsINode> node = mCurrentNode;

    while (1) {

        nsINode *firstChild;
        while (filtered != nsIDOMNodeFilter::FILTER_REJECT &&
               (firstChild = node->GetFirstChild())) {
            node = firstChild;

            filtered = TestNode(node, aResult);
            if (aResult.Failed()) {
                return nullptr;
            }

            if (filtered ==  nsIDOMNodeFilter::FILTER_ACCEPT) {
                // Node found
                mCurrentNode = node;
                return node.forget();
            }
        }

        nsINode *sibling = nullptr;
        nsINode *temp = node;
        do {
            if (temp == mRoot)
                break;

            sibling = temp->GetNextSibling();
            if (sibling)
                break;

            temp = temp->GetParentNode();
        } while (temp);

        if (!sibling)
            break;

        node = sibling;

        // Found a sibling. Either ours or ancestor's
        filtered = TestNode(node, aResult);
        if (aResult.Failed()) {
            return nullptr;
        }

        if (filtered ==  nsIDOMNodeFilter::FILTER_ACCEPT) {
            // Node found
            mCurrentNode = node;
            return node.forget();
        }
    }

    return nullptr;
}

/*
 * TreeWalker helper functions
 */

/*
 * Implements FirstChild and LastChild which only vary in which direction
 * they search.
 * @param aReversed Controls whether we search forwards or backwards
 * @param aResult   Whether we threw or not.
 * @returns         The desired node. Null if no child is found
 */
already_AddRefed<nsINode>
TreeWalker::FirstChildInternal(bool aReversed, ErrorResult& aResult)
{
    nsCOMPtr<nsINode> node = aReversed ? mCurrentNode->GetLastChild()
                                       : mCurrentNode->GetFirstChild();

    while (node) {
        int16_t filtered = TestNode(node, aResult);
        if (aResult.Failed()) {
            return nullptr;
        }

        switch (filtered) {
            case nsIDOMNodeFilter::FILTER_ACCEPT:
                // Node found
                mCurrentNode = node;
                return node.forget();
            case nsIDOMNodeFilter::FILTER_SKIP: {
                    nsINode *child = aReversed ? node->GetLastChild()
                                               : node->GetFirstChild();
                    if (child) {
                        node = child;
                        continue;
                    }
                    break;
                }
            case nsIDOMNodeFilter::FILTER_REJECT:
                // Keep searching
                break;
        }

        do {
            nsINode *sibling = aReversed ? node->GetPreviousSibling()
                                         : node->GetNextSibling();
            if (sibling) {
                node = sibling;
                break;
            }

            nsINode *parent = node->GetParentNode();

            if (!parent || parent == mRoot || parent == mCurrentNode) {
                return nullptr;
            }

            node = parent;

        } while (node);
    }

    return nullptr;
}

/*
 * Implements NextSibling and PreviousSibling which only vary in which
 * direction they search.
 * @param aReversed Controls whether we search forwards or backwards
 * @param aResult   Whether we threw or not.
 * @returns         The desired node. Null if no child is found
 */
already_AddRefed<nsINode>
TreeWalker::NextSiblingInternal(bool aReversed, ErrorResult& aResult)
{
    nsCOMPtr<nsINode> node = mCurrentNode;

    if (node == mRoot) {
        return nullptr;
    }

    while (1) {
        nsINode* sibling = aReversed ? node->GetPreviousSibling()
                                     : node->GetNextSibling();

        while (sibling) {
            node = sibling;

            int16_t filtered = TestNode(node, aResult);
            if (aResult.Failed()) {
                return nullptr;
            }

            if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT) {
                // Node found
                mCurrentNode = node;
                return node.forget();
            }

            // If rejected or no children, try a sibling
            if (filtered == nsIDOMNodeFilter::FILTER_REJECT ||
                !(sibling = aReversed ? node->GetLastChild()
                                      : node->GetFirstChild())) {
                sibling = aReversed ? node->GetPreviousSibling()
                                    : node->GetNextSibling();
            }
        }

        node = node->GetParentNode();

        if (!node || node == mRoot) {
            return nullptr;
        }

        // Is parent transparent in filtered view?
        int16_t filtered = TestNode(node, aResult);
        if (aResult.Failed()) {
            return nullptr;
        }
        if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT) {
            return nullptr;
        }
    }
}

JSObject*
TreeWalker::WrapObject(JSContext *cx)
{
    return TreeWalkerBinding::Wrap(cx, this);
}

} // namespace dom
} // namespace mozilla
