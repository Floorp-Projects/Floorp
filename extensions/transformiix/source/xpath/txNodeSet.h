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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * Larry Fitzpatrick, OpenText, lef@opentext.com
 *   -- moved initialization of DEFAULT_SIZE to NodeSet.cpp
 *
 */

/*
 * Implementation of an XPath NodeSet
 */

#ifndef TRANSFRMX_NODESET_H
#define TRANSFRMX_NODESET_H

#include "ExprResult.h"
#include "txError.h"

class Node;

class txNodeSet : public txAExprResult
{

public:

    txNodeSet(); // Not to be implemented

    /*
     * Creates a new empty NodeSet
     */
    txNodeSet(txResultRecycler* aRecycler);

    /*
     * Creates a new NodeSet containing the supplied node
     */
    txNodeSet(Node* aNode, txResultRecycler* aRecycler);

    /*
     * Creates a new NodeSet, copying the Node references from the source
     * NodeSet
     */
    txNodeSet(const txNodeSet& aSource, txResultRecycler* aRecycler);

    /*
     * Destructor for NodeSet, will not delete referenced Nodes
     */
    virtual ~txNodeSet()
    {
        delete [] mElements;
    }

    /*
     * Adds the specified Node to this NodeSet if it is not already in this
     * NodeSet. The node is inserted according to document order.
     * @param  aNode the Node to add to the NodeSet
     * @return errorcode.
     */
    nsresult add(Node* aNode);

    /*
     * Adds the nodes in specified NodeSet to this NodeSet. The resulting
     * NodeSet is sorted in document order and does not contain any duplicate
     * nodes.
     * @param  aNodes the NodeSet to add, must be in document order.
     * @return errorcode.
     */
    nsresult add(const txNodeSet* aNodes);

    /*
     * Append API
     * These functions should be used with care.
     * They are intended to be used when the caller assures that the resulting
     * NodeSet remains in document order.
     * Abuse will break document order, and cause errors in the result.
     * These functions are significantly faster than the add API, as no
     * Node::OrderInfo structs will be generated.
     */

    /*
     * Appends the specified Node to the end of this NodeSet
     * @param  aNode the Node to append to the NodeSet
     * @return errorcode.
     */
    nsresult append(Node* aNode);

    /*
     * Appends the nodes in the specified NodeSet to the end of this NodeSet
     * @param  aNodes the NodeSet to append to the NodeSet
     * @return errorcode.
     */
    nsresult append(const txNodeSet* aNodes);

    /*
     * Reverse the order of the nodes.
     */
    void reverse();

    /*
     * Removes all nodes from this nodeset
     */
    void clear()
    {
        mElementCount = 0;
    }

    /*
     * Returns the index of the specified Node,
     * or -1 if the Node is not contained in the NodeSet
     * @param  aNode the Node to get the index for
     * @return index of specified node or -1 if the node does not exist
     */
    int indexOf(Node* aNode) const;

    /*
     * Returns true if the specified Node is contained in the set.
     * @param  aNode the Node to search for
     * @return true if specified Node is contained in the NodeSet
     */
    MBool contains(Node* aNode) const
    {
        return indexOf(aNode) >= 0;
    }

    /*
     * Returns the Node at the specified position in this NodeSet.
     * @param  aIndex the position of the Node to return
     * @return Node at specified position
     */
    Node* get(int aIndex) const;

    /*
     * Returns true if there are no Nodes in the NodeSet.
     * @return true if there are no Nodes in the NodeSet.
     */
    MBool isEmpty() const
    {
        return mElementCount == 0;
    }

    /*
     * Returns the number of elements in the NodeSet
     * @return the number of elements in the NodeSet
     */
    int size() const
    {
        return mElementCount;
    }

    /*
     * Virtual methods from ExprResult
     */
    TX_DECL_EXPRRESULT

private:

    /*
     * Makes sure that the mElements buffer contains at least aSize elements.
     * If a new allocation is required the elements are copied over to the new
     * buffer
     * @param  aSize requested number of elements
     * @return true if allocation succeded, false on out of memory
     */
    MBool ensureSize(int aSize);

    /*
     * Finds position in the mElements buffer where a node should be inserted
     * to keep the nodeset in document order. Searches the positions
     * aFirst-aLast, including both aFirst and aLast.
     * @param  aNode  Node to find insert position for
     * @param  aFirst First index to search, this index will be searched
     * @param  aLast  Last index to search, this index will be searched
     * @param  aPos   out-param. Will be set to the index where to insert the
     *                node. The node should be inserted before the node at
     *                this index. This value is always >= aFirst and
     *                <= aLast + 1. This value is always set, even if aNode
     *                already exists in the NodeSet.
     * @return true if the node should be inserted, false if it already exists
     *         in the NodeSet
     */
    MBool findPosition(Node* aNode, int aFirst, int aLast, int& aPos) const;

    Node** mElements;
    int mBufferSize;
    int mElementCount;

};

#endif
