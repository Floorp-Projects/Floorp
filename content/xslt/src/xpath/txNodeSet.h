/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Implementation of an XPath NodeSet
 */

#ifndef txNodeSet_h__
#define txNodeSet_h__

#include "txExprResult.h"
#include "txError.h"
#include "txXPathNode.h"

class txNodeSet : public txAExprResult
{
public:
    /**
     * Creates a new empty NodeSet
     */
    txNodeSet(txResultRecycler* aRecycler);

    /**
     * Creates a new NodeSet with one node.
     */
    txNodeSet(const txXPathNode& aNode, txResultRecycler* aRecycler);

    /**
     * Creates a new txNodeSet, copying the node references from the source
     * NodeSet.
     */
    txNodeSet(const txNodeSet& aSource, txResultRecycler* aRecycler);

    /**
     * Destructor for txNodeSet, deletes the nodes.
     */
    virtual ~txNodeSet();

    /**
     * Adds the specified txXPathNode to this NodeSet if it is not already
     * in this NodeSet. The node is inserted according to document order.
     *
     * @param  aNode the txXPathNode to add to the NodeSet
     * @return errorcode.
     */
    nsresult add(const txXPathNode& aNode);

    /**
     * Adds the nodes in specified NodeSet to this NodeSet. The resulting
     * NodeSet is sorted in document order and does not contain any duplicate
     * nodes.
     *
     * @param  aNodes the NodeSet to add, must be in document order.
     * @return errorcode.
     */
    nsresult add(const txNodeSet& aNodes);
    nsresult addAndTransfer(txNodeSet* aNodes);

    /**
     * Append API
     * These functions should be used with care.
     * They are intended to be used when the caller assures that the resulting
     * NodeSet remains in document order.
     * Abuse will break document order, and cause errors in the result.
     * These functions are significantly faster than the add API, as no
     * order info operations will be performed.
     */

    /**
     * Appends the specified Node to the end of this NodeSet
     * @param  aNode the Node to append to the NodeSet
     * @return errorcode.
     */
    nsresult append(const txXPathNode& aNode);

    /**
     * Appends the nodes in the specified NodeSet to the end of this NodeSet
     * @param  aNodes the NodeSet to append to the NodeSet
     * @return errorcode.
     */
    nsresult append(const txNodeSet& aNodes);

    /**
     * API to implement reverse axes in LocationStep.
     *
     * Before adding nodes to the nodeset for a reversed axis, call
     * setReverse(). This will make the append(aNode) and get() methods treat
     * the nodeset as required. Do only call append(aNode), get(), mark()
     * and sweep() while the nodeset is reversed.
     * Afterwards, call unsetReverse(). The nodes are stored in document
     * order internally.
     */
    void setReverse()
    {
        mDirection = -1;
    }
    void unsetReverse()
    {
        mDirection = 1;
    }

    /**
     * API to implement predicates in PredicateExpr
     *
     * mark(aIndex) marks the specified member of the nodeset.
     * sweep() clears all members of the nodeset that haven't been
     * marked before and clear the mMarks array.
     */
    nsresult mark(PRInt32 aIndex);
    nsresult sweep();

    /**
     * Removes all nodes from this nodeset
     */
    void clear();

    /**
     * Returns the index of the specified Node,
     * or -1 if the Node is not contained in the NodeSet
     * @param  aNode the Node to get the index for
     * @param  aStart index to start searching at
     * @return index of specified node or -1 if the node does not exist
     */
    PRInt32 indexOf(const txXPathNode& aNode, PRUint32 aStart = 0) const;

    /**
     * Returns true if the specified Node is contained in the set.
     * @param  aNode the Node to search for
     * @return true if specified Node is contained in the NodeSet
     */
    bool contains(const txXPathNode& aNode) const
    {
        return indexOf(aNode) >= 0;
    }

    /**
     * Returns the Node at the specified node in this NodeSet.
     * @param  aIndex the node of the Node to return
     * @return Node at specified node
     */
    const txXPathNode& get(PRInt32 aIndex) const;

    /**
     * Returns true if there are no Nodes in the NodeSet.
     * @return true if there are no Nodes in the NodeSet.
     */
    bool isEmpty() const
    {
        return mStart ? mStart == mEnd : true;
    }

    /**
     * Returns the number of elements in the NodeSet
     * @return the number of elements in the NodeSet
     */
    PRInt32 size() const
    {
        return mStart ? mEnd - mStart : 0;
    }

    TX_DECL_EXPRRESULT

private:
    /**
     * Ensure that this nodeset can take another aSize nodes.
     *
     * Changes mStart and mEnd as well as mBufferStart and mBufferEnd.
     */
    bool ensureGrowSize(PRInt32 aSize);

    /**
     * Finds position in the buffer where a node should be inserted
     * to keep the nodeset in document order. Searches the positions
     * aFirst-aLast, including aFirst, but not aLast.
     * @param  aNode   Node to find insert position for.
     * @param  aFirst  First item of the search range, included.
     * @param  aLast   Last item of the search range, excluded.
     * @param  aDupe   out-param. Will be set to true if the node already
     *                 exists in the NodeSet, false if it should be
     *                 inserted.
     * @return pointer where to insert the node. The node should be inserted
     *         before the given node. This value is always set, even if aNode
     *         already exists in the NodeSet
     */
    txXPathNode* findPosition(const txXPathNode& aNode, 
                              txXPathNode* aFirst,
                              txXPathNode* aLast, bool& aDupe) const;

    static void copyElements(txXPathNode* aDest, const txXPathNode* aStart,
                             const txXPathNode* aEnd);
    static void transferElements(txXPathNode* aDest, const txXPathNode* aStart,
                                 const txXPathNode* aEnd);
    static void destroyElements(const txXPathNode* aStart,
                                const txXPathNode* aEnd)
    {
        while (aStart < aEnd) {
            aStart->~txXPathNode();
            ++aStart;
        }
    }

    typedef void (*transferOp) (txXPathNode* aDest, const txXPathNode* aStart,
                                const txXPathNode* aEnd);
    typedef void (*destroyOp) (const txXPathNode* aStart,
                               const txXPathNode* aEnd);
    nsresult add(const txNodeSet& aNodes, transferOp aTransfer,
                 destroyOp aDestroy);

    txXPathNode *mStart, *mEnd, *mStartBuffer, *mEndBuffer;
    PRInt32 mDirection;
    // used for mark() and sweep() in predicates
    bool* mMarks;
};

#endif
