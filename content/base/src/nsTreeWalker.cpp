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
 * nsTreeWalker.cpp: Implementation of the nsIDOMTreeWalker object.
 */

#include "nsTreeWalker.h"

#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMEntityReference.h"
#include "nsDOMError.h"

#include "nsIContent.h"
#include "nsIDocument.h"

#include "nsContentUtils.h"
#include "nsMemory.h"

/*
 * Factories, constructors and destructors
 */

nsresult
NS_NewTreeWalker(nsIDOMNode *aRoot,
                 PRUint32 aWhatToShow,
                 nsIDOMNodeFilter *aFilter,
                 PRBool aEntityReferenceExpansion,
                 nsIDOMTreeWalker **aInstancePtrResult) {

    NS_ENSURE_ARG_POINTER(aInstancePtrResult);

    NS_ENSURE_TRUE(aRoot, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

    nsTreeWalker* walker = new nsTreeWalker(aRoot,
                                            aWhatToShow,
                                            aFilter,
                                            aEntityReferenceExpansion);
    NS_ENSURE_TRUE(walker, NS_ERROR_OUT_OF_MEMORY);

    return CallQueryInterface(walker, aInstancePtrResult);
}

nsTreeWalker::nsTreeWalker(nsIDOMNode *aRoot,
                           PRUint32 aWhatToShow,
                           nsIDOMNodeFilter *aFilter,
                           PRBool aExpandEntityReferences) :
    mRoot(aRoot),
    mWhatToShow(aWhatToShow),
    mFilter(aFilter),
    mExpandEntityReferences(aExpandEntityReferences),
    mCurrentNode(aRoot),
    mPossibleIndexesPos(-1)
{

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
    NS_INTERFACE_MAP_ENTRY(nsISupports)
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
    NS_ENSURE_ARG_POINTER(aRoot);
    *aRoot = mRoot;
    NS_ADDREF(*aRoot);
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
    *aFilter = mFilter;
    NS_IF_ADDREF(*aFilter);
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
    NS_ENSURE_ARG_POINTER(aCurrentNode);
    *aCurrentNode = mCurrentNode;
    NS_ADDREF(*aCurrentNode);
    return NS_OK;
}
NS_IMETHODIMP nsTreeWalker::SetCurrentNode(nsIDOMNode * aCurrentNode)
{
    NS_ENSURE_TRUE(aCurrentNode, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

    nsresult rv = nsContentUtils::CheckSameOrigin(mRoot, aCurrentNode);
    if(NS_FAILED(rv))
        return rv;

    mCurrentNode = aCurrentNode;
    return NS_OK;
}

/*
 * nsIDOMTreeWalker functions
 */

/* nsIDOMNode parentNode (); */
NS_IMETHODIMP nsTreeWalker::ParentNode(nsIDOMNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    
    nsCOMPtr<nsIDOMNode> node(mCurrentNode);
    nsresult rv;

    PRInt32 indexPos = mPossibleIndexesPos;
    
    while (node && node != mRoot) {
        nsCOMPtr<nsIDOMNode> tmp(node);
        rv = tmp->GetParentNode(getter_AddRefs(node));
        NS_ENSURE_SUCCESS(rv, rv);
        
        indexPos--;

        if (node) {
            PRInt16 filtered;
            rv = TestNode(node, &filtered);
            NS_ENSURE_SUCCESS(rv, rv);
            if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT) {
                mCurrentNode = node;
                mPossibleIndexesPos = indexPos >= 0 ? indexPos : -1;
                *_retval = node;
                NS_ADDREF(*_retval);

                return NS_OK;
            }
        }
    }

    *_retval = nsnull;
    return NS_OK;
}

/* nsIDOMNode firstChild (); */
NS_IMETHODIMP nsTreeWalker::FirstChild(nsIDOMNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    return FirstChildOf(mCurrentNode,
                        PR_FALSE,
                        mPossibleIndexesPos+1,
                        _retval);
}

/* nsIDOMNode lastChild (); */
NS_IMETHODIMP nsTreeWalker::LastChild(nsIDOMNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    return FirstChildOf(mCurrentNode,
                        PR_TRUE,
                        mPossibleIndexesPos+1,
                        _retval);
}

/* nsIDOMNode previousSibling (); */
NS_IMETHODIMP nsTreeWalker::PreviousSibling(nsIDOMNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    return NextSiblingOf(mCurrentNode,
                         PR_TRUE,
                         mPossibleIndexesPos,
                         _retval);
}

/* nsIDOMNode nextSibling (); */
NS_IMETHODIMP nsTreeWalker::NextSibling(nsIDOMNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    return NextSiblingOf(mCurrentNode,
                         PR_FALSE,
                         mPossibleIndexesPos,
                         _retval);
}

/* nsIDOMNode previousNode (); */
NS_IMETHODIMP nsTreeWalker::PreviousNode(nsIDOMNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    return NextInDocumentOrderOf(mCurrentNode,
                                 PR_TRUE,
                                 mPossibleIndexesPos,
                                 _retval);
}

/* nsIDOMNode nextNode (); */
NS_IMETHODIMP nsTreeWalker::NextNode(nsIDOMNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    return NextInDocumentOrderOf(mCurrentNode,
                                 PR_FALSE,
                                 mPossibleIndexesPos,
                                 _retval);
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
nsTreeWalker::FirstChildOf(nsIDOMNode* aNode,
                           PRBool aReversed,
                           PRInt32 aIndexPos,
                           nsIDOMNode** _retval)
{
    nsresult rv;

    // Don't step into entity references if expandEntityReferences = false
    if (!mExpandEntityReferences) {
        nsCOMPtr<nsIDOMEntityReference> ent(do_QueryInterface(aNode));

        if (ent) {
            *_retval = nsnull;
            return NS_OK;
        }
    }

    nsCOMPtr<nsIDOMNodeList> childNodes;

    PRInt32 start;
    if (!aReversed) {
        start = -1;
    }
    else {
        rv = aNode->GetChildNodes(getter_AddRefs(childNodes));
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(childNodes, NS_ERROR_UNEXPECTED);

        rv = childNodes->GetLength((PRUint32*)&start);
        NS_ENSURE_SUCCESS(rv, rv);
    }

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
nsTreeWalker::NextSiblingOf(nsIDOMNode* aNode,
                            PRBool aReversed,
                            PRInt32 aIndexPos,
                            nsIDOMNode** _retval)
{
    nsresult rv;
    nsCOMPtr<nsIDOMNode> node(aNode);
    PRInt16 filtered;
    PRInt32 childNum;

    if (node == mRoot) {
        *_retval = nsnull;
        return NS_OK;
    }

    while (1) {
        nsCOMPtr<nsIDOMNode> parent;

        // Get our index in the parent
        rv = node->GetParentNode(getter_AddRefs(parent));
        NS_ENSURE_SUCCESS(rv, rv);

        if (!parent)
            break;

        rv = IndexOf(parent, node, aIndexPos, &childNum);
        NS_ENSURE_SUCCESS(rv, rv);

        // Search siblings
        ChildOf(parent, childNum, aReversed, aIndexPos, _retval);
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
nsTreeWalker::NextInDocumentOrderOf(nsIDOMNode* aNode,
                                    PRBool aReversed,
                                    PRInt32 aIndexPos,
                                    nsIDOMNode** _retval)
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

    nsCOMPtr<nsIDOMNode> node(aNode);
    nsCOMPtr<nsIDOMNode> currentNodeBackup(mCurrentNode);
    PRInt16 filtered;
    PRInt32 childNum;

    while (1) {
        nsCOMPtr<nsIDOMNode> parent;

        // Get our index in the parent
        rv = node->GetParentNode(getter_AddRefs(parent));
        NS_ENSURE_SUCCESS(rv, rv);

        if (!parent)
            break;

        rv = IndexOf(parent, node, aIndexPos, &childNum);
        NS_ENSURE_SUCCESS(rv, rv);

        // Search siblings
        nsCOMPtr<nsIDOMNode> sibling;
        ChildOf(parent, childNum, aReversed, aIndexPos, getter_AddRefs(sibling));
        NS_ENSURE_SUCCESS(rv, rv);

        if (sibling) {
            if (aReversed) {
                // in reversed walking we first test if there are
                // any children. I don't like this piece of code :(
                nsCOMPtr<nsIDOMNode> child(sibling);
                while(child) {
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
nsTreeWalker::ChildOf(nsIDOMNode* aNode,
                      PRInt32 childNum,
                      PRBool aReversed,
                      PRInt32 aIndexPos,
                      nsIDOMNode** _retval)
{
    PRInt16 filtered;
    nsresult rv;
    nsCOMPtr<nsIDOMNodeList> childNodes;

    rv = aNode->GetChildNodes(getter_AddRefs(childNodes));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(childNodes, NS_ERROR_UNEXPECTED);

    PRInt32 dir, end;
    if (!aReversed) {
        dir = 1;
        rv = childNodes->GetLength((PRUint32*)&end);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
        dir = -1;
        end = -1;
    }

    // Step through all children
    PRInt32 i;
    for (i = childNum+dir; i != end; i += dir) {
        nsCOMPtr<nsIDOMNode> child;
        rv = childNodes->Item(i, getter_AddRefs(child));
        NS_ENSURE_SUCCESS(rv, rv);

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
nsresult nsTreeWalker::TestNode(nsIDOMNode* aNode, PRInt16* _filtered)
{
    nsresult rv;
    PRUint16 nodeType;
    PRUint32 mask = 1;

    rv = aNode->GetNodeType(&nodeType);
    NS_ENSURE_SUCCESS(rv, rv);

    if (nodeType <= 12 && !((mask << (nodeType-1)) & mWhatToShow)) {
        *_filtered = nsIDOMNodeFilter::FILTER_SKIP;

        return NS_OK;
    }

    if (mFilter)
        return mFilter->AcceptNode(aNode, _filtered);

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
 * @param _childNum returned index
 * @returns         Errorcode
 */
nsresult nsTreeWalker::IndexOf(nsIDOMNode* aParent,
                               nsIDOMNode* aChild,
                               PRInt32 aIndexPos,
                               PRInt32* _childNum)
{
    PRInt32 possibleIndex = -1;
    if (aIndexPos >= 0)
        possibleIndex = NS_PTR_TO_INT32(mPossibleIndexes[aIndexPos]);

    nsCOMPtr<nsIContent> contParent(do_QueryInterface(aParent));
    if (contParent) {
        nsCOMPtr<nsIContent> child(do_QueryInterface(aChild));

        if (possibleIndex >= 0) {
            if (child == contParent->GetChildAt(possibleIndex)) {
                *_childNum = possibleIndex;
                return NS_OK;
            }
        }

        *_childNum = contParent->IndexOf(child);

        return *_childNum >= 0 ? NS_OK : NS_ERROR_UNEXPECTED;
    }

    nsCOMPtr<nsIDocument> docParent(do_QueryInterface(aParent));
    if (docParent) {
        nsCOMPtr<nsIContent> child(do_QueryInterface(aChild));

        if (possibleIndex >= 0) {
            if (child == docParent->GetChildAt(possibleIndex)) {
                *_childNum = possibleIndex;
                return NS_OK;
            }
        }

        *_childNum = docParent->IndexOf(child);

        return *_childNum >= 0 ? NS_OK : NS_ERROR_UNEXPECTED;
    }

    nsresult rv;
    PRUint32 i, len;
    nsCOMPtr<nsIDOMNodeList> childNodes;

    rv = aParent->GetChildNodes(getter_AddRefs(childNodes));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ENSURE_TRUE(childNodes, NS_ERROR_UNEXPECTED);

    if (possibleIndex >= 0) {
        nsCOMPtr<nsIDOMNode> tmp;
        childNodes->Item(possibleIndex, getter_AddRefs(tmp));
        if (tmp == aChild) {
            *_childNum = possibleIndex;
            return NS_OK;
        }
    }

    rv = childNodes->GetLength(&len);
    NS_ENSURE_SUCCESS(rv, rv);

    for (i = 0; i < len; ++i) {
        nsCOMPtr<nsIDOMNode> node;
        rv = childNodes->Item(i, getter_AddRefs(node));
        NS_ENSURE_SUCCESS(rv, rv);

        if (node == aChild) {
            *_childNum = i;
            return NS_OK;
        }
    }

    return NS_ERROR_UNEXPECTED;
}

/*
 * Sets the child index at the specified level. It doesn't matter if this
 * fails since mPossibleIndexes should only be considered a hint
 * @param aIndexPos   position in mPossibleIndexes to set
 * @param aChildIndex child index at specified position
 */
void nsTreeWalker::SetChildIndex(PRInt32 aIndexPos, PRInt32 aChildIndex)
{
    mPossibleIndexes.ReplaceElementAt(NS_INT32_TO_PTR(aChildIndex), aIndexPos);
}
