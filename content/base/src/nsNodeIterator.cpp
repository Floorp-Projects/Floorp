/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is this file as it was released on July 19 2008.
 *
 * The Initial Developer of the Original Code is
 * Craig Topper.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Craig Topper <craig.topper@gmail.com> (Original Author)
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

/*
 * Implementation of DOM Traversal's nsIDOMNodeIterator
 */

#include "nsNodeIterator.h"

#include "nsIDOMNode.h"
#include "nsIDOMNodeFilter.h"
#include "nsDOMError.h"

#include "nsIContent.h"
#include "nsIDocument.h"

#include "nsContentUtils.h"
#include "nsCOMPtr.h"

/*
 * NodePointer implementation
 */
nsNodeIterator::NodePointer::NodePointer(nsINode *aNode,
                                         PRBool aBeforeNode) :
    mNode(aNode),
    mBeforeNode(aBeforeNode)
{
}

PRBool nsNodeIterator::NodePointer::MoveToNext(nsINode *aRoot)
{
    if (!mNode)
      return PR_FALSE;

    if (mBeforeNode) {
        mBeforeNode = PR_FALSE;
        return PR_TRUE;
    }

    nsINode* child = mNode->GetFirstChild();
    if (child) {
        mNode = child;
        return PR_TRUE;
    }

    return MoveForward(aRoot, mNode);
}

PRBool nsNodeIterator::NodePointer::MoveToPrevious(nsINode *aRoot)
{
    if (!mNode)
      return PR_FALSE;

    if (!mBeforeNode) {
        mBeforeNode = PR_TRUE;
        return PR_TRUE;
    }

    if (mNode == aRoot)
        return PR_FALSE;

    MoveBackward(mNode->GetNodeParent(), mNode->GetPreviousSibling());

    return PR_TRUE;
}

void nsNodeIterator::NodePointer::AdjustAfterRemoval(nsINode *aRoot,
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
        mBeforeNode = PR_FALSE;
    }

    MoveBackward(aContainer, aPreviousSibling);
}

PRBool nsNodeIterator::NodePointer::MoveForward(nsINode *aRoot, nsINode *aNode)
{
    while (1) {
        if (aNode == aRoot)
            break;

        nsINode *sibling = aNode->GetNextSibling();
        if (sibling) {
            mNode = sibling;
            return PR_TRUE;
        }
        aNode = aNode->GetNodeParent();
    }

    return PR_FALSE;
}

void nsNodeIterator::NodePointer::MoveBackward(nsINode *aParent, nsINode *aNode)
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

nsNodeIterator::nsNodeIterator(nsINode *aRoot,
                               PRUint32 aWhatToShow,
                               nsIDOMNodeFilter *aFilter,
                               PRBool aExpandEntityReferences) :
    nsTraversal(aRoot, aWhatToShow, aFilter, aExpandEntityReferences),
    mDetached(PR_FALSE),
    mPointer(mRoot, PR_TRUE)
{
    aRoot->AddMutationObserver(this);
}

nsNodeIterator::~nsNodeIterator()
{
    /* destructor code */
    if (!mDetached && mRoot)
        mRoot->RemoveMutationObserver(this);
}

/*
 * nsISupports and cycle collection stuff
 */

NS_IMPL_CYCLE_COLLECTION_CLASS(nsNodeIterator)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsNodeIterator)
    if (!tmp->mDetached && tmp->mRoot)
        tmp->mRoot->RemoveMutationObserver(tmp);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRoot)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mFilter)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsNodeIterator)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mRoot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mFilter)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(NodeIterator, nsNodeIterator)

// QueryInterface implementation for nsNodeIterator
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsNodeIterator)
    NS_INTERFACE_MAP_ENTRY(nsIDOMNodeIterator)
    NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
    NS_INTERFACE_MAP_ENTRY(nsIMutationObserver2)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNodeIterator)
    NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(NodeIterator)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsNodeIterator)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsNodeIterator)

/* readonly attribute nsIDOMNode root; */
NS_IMETHODIMP nsNodeIterator::GetRoot(nsIDOMNode * *aRoot)
{
    if (mRoot)
        return CallQueryInterface(mRoot, aRoot);

    *aRoot = nsnull;

    return NS_OK;
}

/* readonly attribute unsigned long whatToShow; */
NS_IMETHODIMP nsNodeIterator::GetWhatToShow(PRUint32 *aWhatToShow)
{
    *aWhatToShow = mWhatToShow;
    return NS_OK;
}

/* readonly attribute nsIDOMNodeFilter filter; */
NS_IMETHODIMP nsNodeIterator::GetFilter(nsIDOMNodeFilter **aFilter)
{
    NS_ENSURE_ARG_POINTER(aFilter);

    NS_IF_ADDREF(*aFilter = mFilter);

    return NS_OK;
}

/* readonly attribute boolean expandEntityReferences; */
NS_IMETHODIMP nsNodeIterator::GetExpandEntityReferences(PRBool *aExpandEntityReferences)
{
    *aExpandEntityReferences = mExpandEntityReferences;
    return NS_OK;
}

/* nsIDOMNode nextNode ()  raises (DOMException); */
NS_IMETHODIMP nsNodeIterator::NextNode(nsIDOMNode **_retval)
{
    return NextOrPrevNode(&NodePointer::MoveToNext, _retval);
}

/* nsIDOMNode previousNode ()  raises (DOMException); */
NS_IMETHODIMP nsNodeIterator::PreviousNode(nsIDOMNode **_retval)
{
    return NextOrPrevNode(&NodePointer::MoveToPrevious, _retval);
}

nsresult
nsNodeIterator::NextOrPrevNode(NodePointer::MoveToMethodType aMove,
                               nsIDOMNode **_retval)
{
    nsresult rv;
    PRInt16 filtered;

    *_retval = nsnull;

    if (mDetached || mInAcceptNode)
        return NS_ERROR_DOM_INVALID_STATE_ERR;

    mWorkingPointer = mPointer;

    struct AutoClear {
        NodePointer* mPtr;
        AutoClear(NodePointer* ptr) : mPtr(ptr) {}
       ~AutoClear() { mPtr->Clear(); }
    } ac(&mWorkingPointer);

    while ((mWorkingPointer.*aMove)(mRoot)) {
        nsCOMPtr<nsINode> testNode = mWorkingPointer.mNode;
        rv = TestNode(testNode, &filtered);
        NS_ENSURE_SUCCESS(rv, rv);

        if (mDetached)
            return NS_ERROR_DOM_INVALID_STATE_ERR;

        if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT) {
            mPointer = mWorkingPointer;
            return CallQueryInterface(testNode, _retval);
        }
    }

    return NS_OK;
}

/* void detach (); */
NS_IMETHODIMP nsNodeIterator::Detach(void)
{
    if (!mDetached) {
        mRoot->RemoveMutationObserver(this);

        mPointer.Clear();

        mDetached = PR_TRUE;
    }

    return NS_OK;
}

/* readonly attribute nsIDOMNode referenceNode; */
NS_IMETHODIMP nsNodeIterator::GetReferenceNode(nsIDOMNode * *aRefNode)
{
    if (mPointer.mNode)
        return CallQueryInterface(mPointer.mNode, aRefNode);

    *aRefNode = nsnull;
    return NS_OK;
}

/* readonly attribute boolean pointerBeforeReferenceNode; */
NS_IMETHODIMP nsNodeIterator::GetPointerBeforeReferenceNode(PRBool *aBeforeNode)
{
    *aBeforeNode = mPointer.mBeforeNode;
    return NS_OK;
}

/*
 * nsIMutationObserver interface
 */

void nsNodeIterator::ContentRemoved(nsIDocument *aDocument,
                                    nsIContent *aContainer,
                                    nsIContent *aChild,
                                    PRInt32 aIndexInContainer,
                                    nsIContent *aPreviousSibling)
{
    nsINode *container = NODE_FROM(aContainer, aDocument);

    mPointer.AdjustAfterRemoval(mRoot, container, aChild, aPreviousSibling);
    mWorkingPointer.AdjustAfterRemoval(mRoot, container, aChild, aPreviousSibling);
}

void nsNodeIterator::AttributeChildRemoved(nsINode* aAttribute,
                                           nsIContent* aChild)
{
  mPointer.AdjustAfterRemoval(mRoot, aAttribute, aChild, 0);
  mWorkingPointer.AdjustAfterRemoval(mRoot, aAttribute, aChild, 0);
}

