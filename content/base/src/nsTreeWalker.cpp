/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is this file as it was released on
 * May 1 2001.
 *
 * The Initial Developer of the Original Code is Jonas Sicking.
 * Portions created by Jonas Sicking are Copyright (C) 2001
 * Jonas Sicking.  All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 */

/*
 * nsTreeWalker.cpp: Implementation of the nsIDOMTreeWalker object.
 */

#include "nsTreeWalker.h"

#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsDOMError.h"

#include "nsContentUtils.h"
#include "nsMemory.h"

/*
 * Factories, constructors and destructors
 */

nsresult
NS_NewTreeWalker(nsIDOMNode *root,
                 PRUint32 whatToShow,
                 nsIDOMNodeFilter *filter,
                 PRBool expandEntityReferences,
                 nsIDOMTreeWalker **aInstancePtrResult) {
    NS_ENSURE_ARG_POINTER(aInstancePtrResult);

    NS_ENSURE_TRUE(root, NS_ERROR_DOM_NOT_SUPPORTED_ERR);

    nsTreeWalker* walker = new nsTreeWalker(root,
                                            whatToShow,
                                            filter,
                                            expandEntityReferences);
    NS_ENSURE_TRUE(walker, NS_ERROR_OUT_OF_MEMORY);
    
    return walker->QueryInterface(NS_GET_IID(nsIDOMTreeWalker),
                                  (void**) aInstancePtrResult);
}

nsTreeWalker::nsTreeWalker(nsIDOMNode *root,
                           PRUint32 whatToShow,
                           nsIDOMNodeFilter *filter,
                           PRBool expandEntityReferences) :
    mRoot(root),
    mWhatToShow(whatToShow),
    mFilter(filter),
    mExpandEntityReferences(expandEntityReferences),
    mCurrentNode(root)
{
    NS_INIT_ISUPPORTS();
    /* member initializers and constructor code */

    NS_ASSERTION(root, "invalid root in call to treeWalker constructor");
}

nsTreeWalker::~nsTreeWalker()
{
    /* destructor code */
}

/*
 * nsISupports stuff
 */

// QueryInterface implementation for nsDOMDocumentType
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
    NS_IF_ADDREF(*aRoot);
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
NS_IMETHODIMP nsTreeWalker::GetExpandEntityReferences(PRBool *aExpandEntityReferences)
{
    *aExpandEntityReferences = mExpandEntityReferences;
    return NS_OK;
}

/* attribute nsIDOMNode currentNode; */
NS_IMETHODIMP nsTreeWalker::GetCurrentNode(nsIDOMNode * *aCurrentNode)
{
    NS_ENSURE_ARG_POINTER(aCurrentNode);
    *aCurrentNode = mCurrentNode;
    NS_IF_ADDREF(*aCurrentNode);
    return NS_OK;
}
NS_IMETHODIMP nsTreeWalker::SetCurrentNode(nsIDOMNode * aCurrentNode)
{
    if (!aCurrentNode)
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
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
    
    while (node && node != mRoot) {
        nsCOMPtr<nsIDOMNode> tmp(node);
        rv = tmp->GetParentNode(getter_AddRefs(node));
        NS_ENSURE_SUCCESS(rv, rv);

        if (node) {
            PRInt16 filtered;
            rv = TestNode(node, &filtered);
            NS_ENSURE_SUCCESS(rv, rv);
            if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT) {
                mCurrentNode = node;
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
    return FirstChildOf(mCurrentNode, PR_FALSE, _retval);
}

/* nsIDOMNode lastChild (); */
NS_IMETHODIMP nsTreeWalker::LastChild(nsIDOMNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    return FirstChildOf(mCurrentNode, PR_TRUE, _retval);
}

/* nsIDOMNode previousSibling (); */
NS_IMETHODIMP nsTreeWalker::PreviousSibling(nsIDOMNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    return NextSiblingOf(mCurrentNode, PR_TRUE, _retval);
}

/* nsIDOMNode nextSibling (); */
NS_IMETHODIMP nsTreeWalker::NextSibling(nsIDOMNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    return NextSiblingOf(mCurrentNode, PR_FALSE, _retval);
}

/* nsIDOMNode previousNode (); */
NS_IMETHODIMP nsTreeWalker::PreviousNode(nsIDOMNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    return NextInDocumentOrderOf(mCurrentNode, PR_TRUE, _retval);
}

/* nsIDOMNode nextNode (); */
NS_IMETHODIMP nsTreeWalker::NextNode(nsIDOMNode **_retval)
{
    NS_ENSURE_ARG_POINTER(_retval);
    return NextInDocumentOrderOf(mCurrentNode, PR_FALSE, _retval);
}

/*
 * nsTreeWalker helper functions
 */

/*
 * Tests if and how a node should be filtered. Uses mWhatToShow and
 * mFilter to test the node.
 * @param aNode     Node to test
 * @param _filtered Returned filtervalue. See nsIDOMNodeFilter.idl
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
 * Finds the first child of aNode and returns it. If a child is
 * found, mCurrentNode is set to that child.
 * @param aNode     Node to search for children.
 * @param aReversed Reverses search to find the last child instead
 *                  of first.
 * @param _retval   Returned node. Null if no child is found
 * @returns         Errorcode
 */
nsresult
nsTreeWalker::FirstChildOf(nsIDOMNode* aNode,
                           PRBool aReversed,
                           nsIDOMNode** _retval)
{
    nsCOMPtr<nsIDOMNodeList> children;
    PRUint32 len;
    PRInt16 filtered;
    nsresult rv;
    
    // Don't step into entity references if expandEntityReferences = false
    if (!mExpandEntityReferences) {
        PRUint16 nodeType;
        rv = aNode->GetNodeType(&nodeType);
        NS_ENSURE_SUCCESS(rv, rv);
        
        if (nodeType == nsIDOMNode::ENTITY_REFERENCE_NODE) {
            *_retval = nsnull;

            return NS_OK;
        }
    }
    
    // Get childlist to step through
    rv = aNode->GetChildNodes(getter_AddRefs(children));
    NS_ENSURE_SUCCESS(rv, rv);
    NS_ASSERTION(children, "GetChildNodes returned nonexisting nodelist");

    rv = children->GetLength(&len);
    NS_ENSURE_SUCCESS(rv, rv);

    PRInt32 dir, start, end;
    if (!aReversed) {
        dir = 1;
        start = 0;
        end = len;
    }
    else {
        dir = -1;
        start = len-1;
        end = -1;
    }

    // Step through all children
    PRInt32 i;
    for (i = start; i != end; i += dir) {
        nsCOMPtr<nsIDOMNode> child;
        rv = children->Item((PRUint32)i, getter_AddRefs(child));
        NS_ENSURE_SUCCESS(rv, rv);
        
        rv = TestNode(child, &filtered);
        NS_ENSURE_SUCCESS(rv, rv);
        
        switch (filtered) {
            case nsIDOMNodeFilter::FILTER_ACCEPT:
                // Child found
                mCurrentNode = child;
                *_retval = child;
                NS_ADDREF(*_retval);

                return NS_OK;

            case nsIDOMNodeFilter::FILTER_SKIP:
                // Search children
                rv = FirstChildOf(child, aReversed, _retval);
                NS_ENSURE_SUCCESS(rv, rv);

                if (*_retval)
                    return NS_OK;
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
 * Finds the following sibling of aNode and returns it. If a sibling
 * is found, mCurrentNode is set to that node.
 * @param aNode     Node to start search at.
 * @param aReversed Reverses search to find the previous sibling
 *                  instead of next.
 * @param _retval   Returned node. Null if no sibling is found
 * @returns         Errorcode
 */
nsresult
nsTreeWalker::NextSiblingOf(nsIDOMNode* aNode,
                            PRBool aReversed,
                            nsIDOMNode** _retval)
{
    nsresult rv;
    nsCOMPtr<nsIDOMNode> node(aNode);
    PRInt16 filtered;

    while (1) {
        nsCOMPtr<nsIDOMNode> current(node);

        // Loop siblings
        if (!aReversed)
            rv = current->GetNextSibling(getter_AddRefs(node));
        else
            rv = current->GetPreviousSibling(getter_AddRefs(node));
        NS_ENSURE_SUCCESS(rv, rv);
        
        while (node) {
            rv = TestNode(node, &filtered);
            NS_ENSURE_SUCCESS(rv, rv);

            switch (filtered) {
                case nsIDOMNodeFilter::FILTER_ACCEPT:
                    // Sibling found
                    mCurrentNode = node;
                    *_retval = node;
                    NS_ADDREF(*_retval);

                    return NS_OK;

                case nsIDOMNodeFilter::FILTER_SKIP:
                    // Search children
                    rv = FirstChildOf(node, aReversed, _retval);
                    NS_ENSURE_SUCCESS(rv, rv);

                    if (*_retval)
                        return NS_OK;
                    break;

                case nsIDOMNodeFilter::FILTER_REJECT:
                    // Keep searching
                    break;

                default:
                    return NS_ERROR_UNEXPECTED;
            }
            
            current = node;
            if (!aReversed)
                rv = current->GetNextSibling(getter_AddRefs(node));
            else
                rv = current->GetPreviousSibling(getter_AddRefs(node));
            NS_ENSURE_SUCCESS(rv, rv);
        }

        // Try to get parent
        rv = current->GetParentNode(getter_AddRefs(node));
        NS_ENSURE_SUCCESS(rv, rv);
        if (!node || node == mRoot)
            break;

        // Is parent transparent in filtered view?
        rv = TestNode(node, &filtered);
        NS_ENSURE_SUCCESS(rv, rv);
        if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT)
            break;
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
 * @param _retval   Returned node. Null if no node is found
 * @returns         Errorcode
 */
nsresult
nsTreeWalker::NextInDocumentOrderOf(nsIDOMNode* aNode,
                                    PRBool aReversed,
                                    nsIDOMNode** _retval)
{
    nsresult rv;
    nsCOMPtr<nsIDOMNode> node(aNode);
    PRInt16 filtered;

    while (1) {
        nsCOMPtr<nsIDOMNode> current(node);

        // Loop siblings
        if (!aReversed)
            rv = current->GetNextSibling(getter_AddRefs(node));
        else
            rv = current->GetPreviousSibling(getter_AddRefs(node));
        NS_ENSURE_SUCCESS(rv, rv);
        
        while (node) {
            rv = TestNode(node, &filtered);
            NS_ENSURE_SUCCESS(rv, rv);

            switch (filtered) {
                case nsIDOMNodeFilter::FILTER_ACCEPT:
                    // Sibling found.
                    if (aReversed) {
                        // If reversed search we first
                        // check if it has a child...
                        rv = FirstChildOf(node, PR_TRUE, _retval);
                        NS_ENSURE_SUCCESS(rv, rv);

                        if (*_retval)
                            return NS_OK;
                    }

                    // ...else we return the sibling
                    mCurrentNode = node;
                    *_retval = node;
                    NS_ADDREF(*_retval);

                    return NS_OK;

                case nsIDOMNodeFilter::FILTER_SKIP:
                    // Search children
                    rv = FirstChildOf(node, aReversed, _retval);
                    NS_ENSURE_SUCCESS(rv, rv);

                    if (*_retval)
                        return NS_OK;
                    break;

                case nsIDOMNodeFilter::FILTER_REJECT:
                    // Keep searching
                    break;

                default:
                    return NS_ERROR_UNEXPECTED;
            }
            
            current = node;
            if (!aReversed)
                rv = current->GetNextSibling(getter_AddRefs(node));
            else
                rv = current->GetPreviousSibling(getter_AddRefs(node));
            NS_ENSURE_SUCCESS(rv, rv);
        }

        // Try to get parent
        rv = current->GetParentNode(getter_AddRefs(node));
        NS_ENSURE_SUCCESS(rv, rv);
        if (!node || node == mRoot)
            break;

        if (aReversed) {
            // Is parent transparent in filtered view?
            rv = TestNode(node, &filtered);
            NS_ENSURE_SUCCESS(rv, rv);
            if (filtered == nsIDOMNodeFilter::FILTER_ACCEPT) {
                mCurrentNode = node;
                *_retval = node;
                NS_ADDREF(*_retval);

                return NS_OK;
            }
        }
    }
    
    *_retval = nsnull;
    return NS_OK;
}
