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
 
#ifndef nsTreeWalker_h___
#define nsTreeWalker_h___

/*
 * nsTreeWalker.h: interface of the nsTreeWalker object.
 */

#include "nsIDOMNode.h"
#include "nsIDOMTreeWalker.h"
#include "nsIDOMNodeFilter.h"
#include "nsCOMPtr.h"
#include "nsVoidArray.h"

class nsTreeWalker : public nsIDOMTreeWalker
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIDOMTREEWALKER

    nsTreeWalker(nsIDOMNode *aRoot,
                 PRUint32 aWhatToShow,
                 nsIDOMNodeFilter *aFilter,
                 PRBool aExpandEntityReferences);
    virtual ~nsTreeWalker();
    /* additional members */
private:
    nsCOMPtr<nsIDOMNode> mRoot;
    PRUint32 mWhatToShow;
    nsCOMPtr<nsIDOMNodeFilter> mFilter;
    PRBool mExpandEntityReferences;
    nsCOMPtr<nsIDOMNode> mCurrentNode;
    
    /*
     * Array with all child indexes up the tree. This should only be
     * considered a hint and the value could be wrong.
     * The array contains casted PRInt32's
     */
    nsAutoVoidArray mPossibleIndexes;
    
    /*
     * Position of mCurrentNode in mPossibleIndexes
     */
    PRInt32 mPossibleIndexesPos;
    
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
    nsresult FirstChildOf(nsIDOMNode* aNode,
                          PRBool aReversed,
                          PRInt32 aIndexPos,
                          nsIDOMNode** _retval);

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
    nsresult NextSiblingOf(nsIDOMNode* aNode,
                           PRBool aReversed,
                           PRInt32 aIndexPos,
                           nsIDOMNode** _retval);
                           
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
    nsresult NextInDocumentOrderOf(nsIDOMNode* aNode,
                                   PRBool aReversed,
                                   PRInt32 aIndexPos,
                                   nsIDOMNode** _retval);

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
    nsresult ChildOf(nsIDOMNode* aNode,
                     PRInt32 childNum,
                     PRBool aReversed,
                     PRInt32 aIndexPos,
                     nsIDOMNode** _retval);

    /*
     * Tests if and how a node should be filtered. Uses mWhatToShow and
     * mFilter to test the node.
     * @param aNode     Node to test
     * @param _filtered Returned filtervalue. See nsIDOMNodeFilter.idl
     * @returns         Errorcode
     */
    nsresult TestNode(nsIDOMNode* aNode, PRInt16* _filtered);
    
    /*
     * Gets the child index of a node within it's parent. Gets a possible index
     * from mPossibleIndexes to gain speed. If the value in mPossibleIndexes
     * isn't correct it'll get the index the usual way.
     * @param aParent   in which to get the index
     * @param aChild    node to get the index of
     * @param aIndexPos position in mPossibleIndexes that contains the possible.
     *                  index
     * @param _childNum returned index
     * @returns         Errorcode
     */
    nsresult IndexOf(nsIDOMNode* aParent,
                     nsIDOMNode* aChild,
                     PRInt32 aIndexPos,
                     PRInt32* _childNum);

    /*
     * Sets the child index at the specified level. It doesn't matter if this
     * fails since mPossibleIndexes should only be considered a hint
     * @param aIndexPos   position in mPossibleIndexes to set
     * @param aChildIndex child index at specified position
     */
    void SetChildIndex(PRInt32 aIndexPos, PRInt32 aChildIndex);

};

// Make a new nsIDOMTreeWalker object
nsresult NS_NewTreeWalker(nsIDOMNode *aRoot,
                          PRUint32 aWhatToShow,
                          nsIDOMNodeFilter *aFilter,
                          PRBool aEntityReferenceExpansion,
                          nsIDOMTreeWalker **aInstancePtrResult);

#endif

