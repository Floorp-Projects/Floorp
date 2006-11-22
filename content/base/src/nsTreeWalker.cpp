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
 * The Original Code is this file as it was released on May 1 2001.
 *
 * The Initial Developer of the Original Code is
 * Jonas Sicking.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com> (Original Author)
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
 * Implementation of DOM Traversal's nsIDOMTreeWalker
 */

#include "nsTreeWalker.h"

#include "nsIDOMNode.h"
#include "nsIDOMNodeFilter.h"
#include "nsDOMError.h"

#include "nsIContent.h"
#include "nsIDocument.h"

#include "nsContentUtils.h"
#include "nsMemory.h"
#include "nsCOMArray.h"
#include "nsGkAtoms.h"

/*
 * Factories, constructors and destructors
 */

nsresult
NS_NewTreeWalker(nsIDOMNode *aRoot,
                 PRUint32 aWhatToShow,
                 nsIDOMNodeFilter *aFilter,
                 PRBool aEntityReferenceExpansion,
                 nsIDOMTreeWalker **aInstancePtrResult)
{
    NS_ENSURE_ARG_POINTER(aInstancePtrResult);

    nsCOMPtr<nsINode> root = do_QueryInterface(aRoot);
    NS_ENSURE_TRUE(root, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

    nsTreeWalker* walker = new nsTreeWalker(root,
                                            aWhatToShow,
                                            aFilter,
                                            aEntityReferenceExpansion);
    NS_ENSURE_TRUE(walker, NS_ERROR_OUT_OF_MEMORY);

    return CallQueryInterface(walker, aInstancePtrResult);
}

nsTreeWalker::nsTreeWalker(nsINode *aRoot,
                           PRUint32 aWhatToShow,
                           nsIDOMNodeFilter *aFilter,
                           PRBool aExpandEntityReferences) :
    mRoot(aRoot),
    mWhatToShow(aWhatToShow),
    mExpandEntityReferences(aExpandEntityReferences),
    mCurrentNode(aRoot),
    mPossibleIndexesPos(-1)
{
    mFilter.Set(aFilter, this);

    NS_ASSERTION(aRoot, "invalid root in call to nsTreeWalker constructor");
}

nsTreeWalker::~nsTreeWalker()
{
    /* destructor code */
}

/*
 * nsISupports stuff
 */

// QueryInterface implementation for nsTreeWalker
NS_INTERFACE_MAP_BEGIN(nsTreeWalker)
    NS_INTERFACE_MAP_ENTRY(nsIDOMTreeWalker)
    NS_INTERFACE_MAP_ENTRY(nsIDOMGCParticipant)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMTreeWalker)
    NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(TreeWalker)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsTreeWalker)
NS_IMPL_RELEASE(nsTreeWalker)

/*
 * nsIDOMTreeWalker Getters/Setters
 */

/* readonly attribute nsIDOMNode root; */
NS_IMETHODIMP nsTreeWalker::GetRoot(nsIDOMNode * *aRoot)
{
    if (mRoot) {
        return CallQueryInterface(mRoot, aRoot);
    }

    *aRoot = nsnull;

    return NS_OK;
}

/* readonly attribute unsigned long whatToShow; */
NS_IMETHODIMP nsTreeWalker::GetWhatToShow(PRUint32 *aWhatToShow)
{
    *aWhatToShow = mWhatToShow;
    return NS_OK;
}

/* readonly attribute nsIDOMNodeFilter filter; */
NS_IMETHODIMP nsTreeWalker::GetFilter(nsIDOMNodeFilter * *aFilter)
{
    NS_ENSURE_ARG_POINTER(aFilter);

    nsCOMPtr<nsIDOMNodeFilter> filter = mFilter.Get();
    filter.swap((*aFilter = nsnull));

    return NS_OK;
}

/* readonly attribute boolean expandEntityReferences; */
NS_IMETHODIMP
nsTreeWalker::GetExpandEntityReferences(PRBool *aExpandEntityReferences)
{
    *aExpandEntityReferences = mExpandEntityReferences;
    return NS_OK;
}

/* attribute nsIDOMNode currentNode; */
NS_IMETHODIMP nsTreeWalker::GetCurrentNode(nsIDOMNode * *aCurrentNode)
{
    if (mCurrentNode) {
        return CallQueryInterface(mCurrentNode, aCurrentNode);
    }

    *aCurrentNode = nsnull;

    return NS_OK;
}
NS_IMETHODIMP nsTreeWalker::SetCurrentNode(nsIDOMNode * aCurrentNode)
{
    NS_ENSURE_TRUE(aCurrentNode, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

    // This QI is dumb, but this shouldn't be a critical operation
    nsCOMPtr<nsIDOMNode> domRoot = do_QueryInterface(mRoot);
    nsresult rv = nsContentUtils::CheckSameOrigin(domRoot, aCurrentNode);
    NS_ENSURE_SUCCESS(rv, rv);

    mCurrentNode = do_QueryInterface(aCurrentNode);

    return NS_OK;
}

/*
 * nsIDOMTreeWalker functions
 */

/* nsIDOMNode parentNode (); */
NS_IMETHODIMP nsTreeWalker::ParentNode(nsIDOMNode **_retval)
{
    *_retval = nsnull;
    
    nsresult rv;

    PRInt32 indexPos = mPossibleIndexesPos;
    nsCOMPtr<nsINode> node = mCurrentNode;
    
    while (node && node != mRoot) {
        node = node->GetNodeParent();
        
        indexPos--;

        if (node) {
            PRInt16 filtered;
            rv = TestNode(node, &filtered);
            NS_ENSURE_SUCCESS(rv, rv);
            if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT) {
                mCurrentNode = node;
                mPossibleIndexesPos = indexPos >= 0 ? indexPos : -1;

                return CallQueryInterface(node, _retval);
            }
        }
    }

    return NS_OK;
}

/* nsIDOMNode firstChild (); */
NS_IMETHODIMP nsTreeWalker::FirstChild(nsIDOMNode **_retval)
{
    *_retval = nsnull;

    nsCOMPtr<nsINode> result;
    nsresult rv =  FirstChildOf(mCurrentNode,
                                PR_FALSE,
                                mPossibleIndexesPos + 1,
                                getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);

    return result ? CallQueryInterface(result, _retval) : NS_OK;
}

/* nsIDOMNode lastChild (); */
NS_IMETHODIMP nsTreeWalker::LastChild(nsIDOMNode **_retval)
{
    *_retval = nsnull;

    nsCOMPtr<nsINode> result;
    nsresult rv =  FirstChildOf(mCurrentNode,
                                PR_TRUE,
                                mPossibleIndexesPos + 1,
                                getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);

    return result ? CallQueryInterface(result, _retval) : NS_OK;
}

/* nsIDOMNode previousSibling (); */
NS_IMETHODIMP nsTreeWalker::PreviousSibling(nsIDOMNode **_retval)
{
    *_retval = nsnull;

    nsCOMPtr<nsINode> result;
    nsresult rv = NextSiblingOf(mCurrentNode,
                                PR_TRUE,
                                mPossibleIndexesPos,
                                getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);

    return result ? CallQueryInterface(result, _retval) : NS_OK;
}

/* nsIDOMNode nextSibling (); */
NS_IMETHODIMP nsTreeWalker::NextSibling(nsIDOMNode **_retval)
{
    *_retval = nsnull;

    nsCOMPtr<nsINode> result;
    nsresult rv = NextSiblingOf(mCurrentNode,
                                PR_FALSE,
                                mPossibleIndexesPos,
                                getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);

    return result ? CallQueryInterface(result, _retval) : NS_OK;
}

/* nsIDOMNode previousNode (); */
NS_IMETHODIMP nsTreeWalker::PreviousNode(nsIDOMNode **_retval)
{
    *_retval = nsnull;

    nsCOMPtr<nsINode> result;
    nsresult rv = NextInDocumentOrderOf(mCurrentNode,
                                        PR_TRUE,
                                        mPossibleIndexesPos,
                                        getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);

    return result ? CallQueryInterface(result, _retval) : NS_OK;
}

/* nsIDOMNode nextNode (); */
NS_IMETHODIMP nsTreeWalker::NextNode(nsIDOMNode **_retval)
{
    *_retval = nsnull;

    nsCOMPtr<nsINode> result;
    nsresult rv = NextInDocumentOrderOf(mCurrentNode,
                                        PR_FALSE,
                                        mPossibleIndexesPos,
                                        getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);

    return result ? CallQueryInterface(result, _retval) : NS_OK;
}

/*
 * nsIDOMGCParticipant functions
 */
/* virtual */ nsIDOMGCParticipant*
nsTreeWalker::GetSCCIndex()
{
    return this;
}

/* virtual */ void
nsTreeWalker::AppendReachableList(nsCOMArray<nsIDOMGCParticipant>& aArray)
{
    nsCOMPtr<nsIDOMGCParticipant> gcp;
    
    gcp = do_QueryInterface(mRoot);
    if (gcp)
        aArray.AppendObject(gcp);

    gcp = do_QueryInterface(mCurrentNode);
    if (gcp)
        aArray.AppendObject(gcp);
}

/*
 * nsTreeWalker helper functions
 */

/*
 * Finds the first child of aNode and returns it. If a child is
 * found, mCurrentNode is set to that child.
 * @param aNode     Node to search for children.
 * @param aReversed Reverses search to find the last child instead
 *                  of first.
 * @param aIndexPos Position of aNode in mPossibleIndexes.
 * @param _retval   Returned node. Null if no child is found
 * @returns         Errorcode
 */
nsresult
nsTreeWalker::FirstChildOf(nsINode* aNode,
                           PRBool aReversed,
                           PRInt32 aIndexPos,
                           nsINode** _retval)
{
    *_retval = nsnull;
    PRInt32 start = aReversed ? (PRInt32)aNode->GetChildCount() : -1;

    return ChildOf(aNode, start, aReversed, aIndexPos, _retval);
}

/*
 * Finds the following sibling of aNode and returns it. If a sibling
 * is found, mCurrentNode is set to that node.
 * @param aNode     Node to start search at.
 * @param aReversed Reverses search to find the previous sibling
 *                  instead of next.
 * @param aIndexPos Position of aNode in mPossibleIndexes.
 * @param _retval   Returned node. Null if no sibling is found
 * @returns         Errorcode
 */
nsresult
nsTreeWalker::NextSiblingOf(nsINode* aNode,
                            PRBool aReversed,
                            PRInt32 aIndexPos,
                            nsINode** _retval)
{
    nsresult rv;
    nsCOMPtr<nsINode> node = aNode;
    PRInt16 filtered;
    PRInt32 childNum;

    if (node == mRoot) {
        *_retval = nsnull;
        return NS_OK;
    }

    while (1) {
        nsCOMPtr<nsINode> parent = node->GetNodeParent();

        if (!parent)
            break;

        childNum = IndexOf(parent, node, aIndexPos);
        NS_ENSURE_TRUE(childNum >= 0, NS_ERROR_UNEXPECTED);

        // Search siblings
        rv = ChildOf(parent, childNum, aReversed, aIndexPos, _retval);
        NS_ENSURE_SUCCESS(rv, rv);

        if (*_retval)
            return NS_OK;

        // Is parent the root?
        if (parent == mRoot)
            break;

        // Is parent transparent in filtered view?
        rv = TestNode(parent, &filtered);
        NS_ENSURE_SUCCESS(rv, rv);
        if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT)
            break;

        node = parent;
        aIndexPos = aIndexPos < 0 ? -1 : aIndexPos-1;
    }

    *_retval = nsnull;
    return NS_OK;
}

/*
 * Finds the next node in document order of aNode and returns it.
 * If a node is found, mCurrentNode is set to that node.
 * @param aNode     Node to start search at.
 * @param aReversed Reverses search to find the preceding node
 *                  instead of next.
 * @param aIndexPos Position of aNode in mPossibleIndexes.
 * @param _retval   Returned node. Null if no node is found
 * @returns         Errorcode
 */
nsresult
nsTreeWalker::NextInDocumentOrderOf(nsINode* aNode,
                                    PRBool aReversed,
                                    PRInt32 aIndexPos,
                                    nsINode** _retval)
{
    nsresult rv;

    if (!aReversed) {
        rv = FirstChildOf(aNode, aReversed, aIndexPos+1, _retval);
        NS_ENSURE_SUCCESS(rv, rv);

        if (*_retval)
            return NS_OK;
    }

    if (aNode == mRoot){
        *_retval = nsnull;
        return NS_OK;
    }

    nsCOMPtr<nsINode> node = aNode;
    nsCOMPtr<nsINode> currentNodeBackup = mCurrentNode;
    PRInt16 filtered;
    PRInt32 childNum;

    while (1) {
        // Get our index in the parent
        nsCOMPtr<nsINode> parent = node->GetNodeParent();
        if (!parent)
            break;

        childNum = IndexOf(parent, node, aIndexPos);
        NS_ENSURE_TRUE(childNum >= 0, NS_ERROR_UNEXPECTED);

        // Search siblings
        nsCOMPtr<nsINode> sibling;
        rv = ChildOf(parent, childNum, aReversed, aIndexPos,
                     getter_AddRefs(sibling));
        NS_ENSURE_SUCCESS(rv, rv);

        if (sibling) {
            if (aReversed) {
                // in reversed walking we first test if there are
                // any children. I don't like this piece of code :(
                nsCOMPtr<nsINode> child = sibling;
                while (child) {
                    sibling = child;
                    rv = FirstChildOf(sibling,
                                      PR_TRUE,
                                      aIndexPos,
                                      getter_AddRefs(child));
                    if (NS_FAILED(rv)) {
                        // ChildOf set mCurrentNode and then something
                        // failed. Restore the old value before returning
                        mCurrentNode = currentNodeBackup;
                        mPossibleIndexesPos = -1;
                        return rv;
                    }
                }
            }
            *_retval = sibling;
            NS_ADDREF(*_retval);
            return NS_OK;
        }

        aIndexPos = aIndexPos < 0 ? -1 : aIndexPos-1;

        if (aReversed) {
            // Is parent transparent in filtered view?
            rv = TestNode(parent, &filtered);
            NS_ENSURE_SUCCESS(rv, rv);
            if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT) {
                mCurrentNode = parent;
                mPossibleIndexesPos = aIndexPos;
                *_retval = parent;
                NS_ADDREF(*_retval);
                return NS_OK;
            }
        }

        // Is parent the root?
        if (parent == mRoot)
            break;

        node = parent;
    }

    *_retval = nsnull;
    return NS_OK;
}

/*
 * Finds the first child of aNode after child N and returns it. If a
 * child is found, mCurrentNode is set to that child
 * @param aNode     Node to search for children
 * @param childNum  Child number to start search from. The child with
 *                  this number is not searched
 * @param aReversed Reverses search to find the last child instead
 *                  of first
 * @param aIndexPos Position of aNode in mPossibleIndexes
 * @param _retval   Returned node. Null if no child is found
 * @returns         Errorcode
 */
nsresult
nsTreeWalker::ChildOf(nsINode* aNode,
                      PRInt32 childNum,
                      PRBool aReversed,
                      PRInt32 aIndexPos,
                      nsINode** _retval)
{
    PRInt16 filtered;
    nsresult rv;

    PRInt32 dir = aReversed ? -1 : 1;

    // Step through all children
    PRInt32 i = childNum;
    while (1) {
        i += dir;
        nsCOMPtr<nsINode> child = aNode->GetChildAt(i);
        if (!child) {
            break;
        }

        rv = TestNode(child, &filtered);
        NS_ENSURE_SUCCESS(rv, rv);

        switch (filtered) {
            case nsIDOMNodeFilter::FILTER_ACCEPT:
                // Child found
                mCurrentNode = child;
                mPossibleIndexesPos = aIndexPos;
                *_retval = child;
                NS_ADDREF(*_retval);

                SetChildIndex(aIndexPos, i);

                return NS_OK;

            case nsIDOMNodeFilter::FILTER_SKIP:
                // Search children
                rv = FirstChildOf(child, aReversed, aIndexPos+1, _retval);
                NS_ENSURE_SUCCESS(rv, rv);

                if (*_retval) {
                    SetChildIndex(aIndexPos, i);
                    return NS_OK;
                }
                break;

            case nsIDOMNodeFilter::FILTER_REJECT:
                // Keep searching
                break;

            default:
                return NS_ERROR_UNEXPECTED;
        }
    }

    *_retval = nsnull;
    return NS_OK;
}

/*
 * Tests if and how a node should be filtered. Uses mWhatToShow and
 * mFilter to test the node.
 * @param aNode     Node to test
 * @param _filtered Returned filtervalue. See nsIDOMNodeFilter.idl
 * @returns         Errorcode
 */
nsresult nsTreeWalker::TestNode(nsINode* aNode, PRInt16* _filtered)
{
    nsresult rv;

    *_filtered = nsIDOMNodeFilter::FILTER_SKIP;

    PRUint16 nodeType = 0;
    // Check the most common cases
    if (aNode->IsNodeOfType(nsINode::eELEMENT)) {
        nodeType = nsIDOMNode::ELEMENT_NODE;
    }
    else if (aNode->IsNodeOfType(nsINode::eCONTENT)) {
        nsIAtom* tag = NS_STATIC_CAST(nsIContent*, aNode)->Tag();
        if (tag == nsGkAtoms::textTagName) {
            nodeType = nsIDOMNode::TEXT_NODE;
        }
        else if (tag == nsGkAtoms::cdataTagName) {
            nodeType = nsIDOMNode::CDATA_SECTION_NODE;
        }
        else if (tag == nsGkAtoms::commentTagName) {
            nodeType = nsIDOMNode::COMMENT_NODE;
        }
        else if (tag == nsGkAtoms::processingInstructionTagName) {
            nodeType = nsIDOMNode::PROCESSING_INSTRUCTION_NODE;
        }
    }

    nsCOMPtr<nsIDOMNode> domNode;
    if (!nodeType) {
        domNode = do_QueryInterface(aNode);
        rv = domNode->GetNodeType(&nodeType);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (nodeType <= 12 && !((1 << (nodeType-1)) & mWhatToShow)) {
        return NS_OK;
    }

    nsCOMPtr<nsIDOMNodeFilter> filter = mFilter.Get();
    if (filter) {
        if (!domNode) {
            domNode = do_QueryInterface(aNode);
        }

        return filter->AcceptNode(domNode, _filtered);
    }

    *_filtered = nsIDOMNodeFilter::FILTER_ACCEPT;
    return NS_OK;
}

/*
 * Gets the child index of a node within it's parent. Gets a possible index
 * from mPossibleIndexes to gain speed. If the value in mPossibleIndexes
 * isn't correct it'll get the index the usual way
 * @param aParent   in which to get the index
 * @param aChild    node to get the index of
 * @param aIndexPos position in mPossibleIndexes that contains the possible.
 *                  index
 * @returns         resulting index
 */
PRInt32 nsTreeWalker::IndexOf(nsINode* aParent,
                              nsINode* aChild,
                              PRInt32 aIndexPos)
{
    if (aIndexPos >= 0 && aIndexPos < mPossibleIndexes.Count()) {
        PRInt32 possibleIndex =
            NS_PTR_TO_INT32(mPossibleIndexes.FastElementAt(aIndexPos));
        if (aChild == aParent->GetChildAt(possibleIndex)) {
            return possibleIndex;
        }
    }

    return aParent->IndexOf(aChild);
}
