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
 *    -- moved initialization of DEFAULT_SIZE from NodeSet.h to here
 *
 * Olivier Gerardin, ogerardin@vo.lu
 *    -- fixed numberValue()
 *
 */

#include "NodeSet.h"
#include "dom.h"
#include "XMLDOMUtils.h"
#include <string.h>

static const int kTxNodeSetMinSize = 4;
static const int kTxNodeSetGrowFactor = 2;

/*
 * Implementation of an XPath NodeSet
 */

/*
 * Creates a new empty NodeSet
 */
NodeSet::NodeSet() : mElements(0),
                     mBufferSize(0),
                     mElementCount(0)
{
}

/*
 * Creates a new NodeSet containing the supplied Node
 */
NodeSet::NodeSet(Node* aNode) : mBufferSize(1), 
                                mElementCount(1)
{
    NS_ASSERTION(aNode, "missing node to NodeSet::add");
    mElements = new Node*[1];
    if (!mElements) {
        NS_ASSERTION(0, "out of memory");
        mBufferSize = 0;
        mElementCount = 0;
    }
    else {
        mElements[0] = aNode;
    }
}

/*
 * Creates a new NodeSet, copying the Node references from the source
 * NodeSet
 */
NodeSet::NodeSet(const NodeSet& aSource) : mElements(0),
                                           mBufferSize(0),
                                           mElementCount(0)
{
    append(&aSource);
}

/*
 * Adds the specified Node to this NodeSet if it is not already in this
 * NodeSet. The node is inserted according to document order.
 * @param  aNode the Node to add to the NodeSet
 * @return errorcode.
 */
nsresult NodeSet::add(Node* aNode)
{
    NS_ASSERTION(aNode, "missing node to NodeSet::add");
    if (!aNode)
        return NS_ERROR_NULL_POINTER;

    MBool nonDup;
    int pos = findPosition(aNode, 0, mElementCount - 1, nonDup);
    if (nonDup) {
        if (!ensureSize(mElementCount + 1))
            return NS_ERROR_OUT_OF_MEMORY;
        memmove(mElements + pos + 1,
                mElements + pos,
                (mElementCount - pos) * sizeof(Node*));
        mElements[pos] = aNode;
        ++mElementCount;
    }
    return NS_OK;
}

/*
 * Adds the nodes in specified NodeSet to this NodeSet. The resulting NodeSet
 * is sorted in document order and does not contain any duplicate nodes.
 * @param  aNodes the NodeSet to add, must be in document order.
 * @return true on success. false on failure.
 */

/*
 * The code is optimized to make a minimum number of calls to
 * Node::compareDocumentPosition. The idea is this:
 * We have the two nodesets (number indicate "document position")
 * 
 * 1 3 7             <- source 1
 * 2 3 6 8 9         <- source 2
 * _ _ _ _ _ _ _ _   <- result
 * 
 * 
 * We select the last node in the smallest nodeset and find where in the other
 * nodeset it would be inserted. In this case we would take the 7 from the
 * first nodeset and find the position between the 6 and 8 in the second.
 * We then take the nodes after the insert-position and move it to the end of
 * the resulting nodeset, and then do the same for the node from the smaller
 * nodeset. Which in this case means that we'd first move the 8 and 9 nodes,
 * and then the 7 node, giving us the following:
 * 
 * 1 3               <- source 1
 * 2 3 6             <- source 2
 * _ _ _ _ _ 7 8 9   <- result
 * 
 * Repeat until one of the nodesets are empty. If we find a duplicate node
 * when searching for where insertposition we skip the step where we move the
 * node from the smaller nodeset to the resulting nodeset. So in this next
 * step in the example we would only move the 3 and 6 nodes from the second
 * nodeset and then just remove the 3 node from the first nodeset. Giving:
 * 
 * 1                 <- source 1
 * 2                 <- source 2
 * _ _ _ 3 6 7 8 9   <- result
 * 
 * We might therefor end up with some blanks in the bigining of the resulting
 * nodeset, which we simply fix by moving all the nodes one step down.
 */
nsresult NodeSet::add(const NodeSet* aNodes)
{
    NS_ASSERTION(aNodes, "missing nodeset to NodeSet::add");
    if (!aNodes)
        return NS_ERROR_NULL_POINTER;

    if (aNodes->mElementCount == 0)
        return NS_OK;

    // This is probably a rather common case, so lets try to shortcut
    if (mElementCount == 0 ||
        mElements[mElementCount-1]->compareDocumentPosition(aNodes->mElements[0]) < 0)
        return append(aNodes);

    if (!ensureSize(mElementCount + aNodes->mElementCount))
        return NS_ERROR_OUT_OF_MEMORY;

    // Index of last node in this nodeset
    int thisPos = mElementCount - 1;
    // Index of last node in other nodeset
    int otherPos = aNodes->mElementCount - 1;
    // Index in result where last insert was done.
    int lastInsertPos = mElementCount + aNodes->mElementCount;
    
    while (thisPos >= 0 && otherPos >= 0) {
        if (thisPos > otherPos) {
            int pos;
            MBool nonDup;
            // Find where in the remaining nodes in this nodeset a node from
            // the other nodeset should be inserted
            pos = findPosition(aNodes->mElements[otherPos], 0, thisPos,
                               nonDup);

            // Move nodes in this nodeset
            lastInsertPos -= thisPos - pos + 1;
            memmove(mElements + lastInsertPos,
                    mElements + pos,
                    (thisPos - pos + 1) * sizeof(Node*));

            // Copy node from the other nodeset unless it's a dup
            if (nonDup)
                mElements[--lastInsertPos] = aNodes->mElements[otherPos];
            
            // Adjust positions in both nodesets
            thisPos = pos - 1;
            --otherPos;
        }
        else {
            int pos;
            MBool nonDup;
            // Find where in the remaining nodes in the other nodeset a node
            // from this nodeset should be inserted
            pos = aNodes->findPosition(mElements[thisPos], 0, otherPos,
                                       nonDup);

            // Copy nodes from other nodeset to this
            lastInsertPos -= otherPos - pos + 1;
            memcpy(mElements + lastInsertPos,
                   aNodes->mElements + pos,
                   (otherPos - pos + 1) * sizeof(Node*));

            // Move node in this nodeset unless it's a dup
            if (nonDup)
                mElements[--lastInsertPos] = mElements[thisPos];
            
            // Adjust positions in both nodesets
            otherPos = pos - 1;
            --thisPos;
        }
    }
    
    if (thisPos >= 0) {
        // There were some elements still left in this nodeset that need to
        // be moved
        lastInsertPos -= thisPos + 1;
        memmove(mElements + lastInsertPos,
                mElements,
                (thisPos + 1) * sizeof(Node*));
    }
    else if (otherPos >= 0) {
        // There were some elements still left in the other nodeset that need
        // to be copied
        lastInsertPos -= otherPos + 1;
        memcpy(mElements + lastInsertPos,
               aNodes->mElements,
               (otherPos + 1) * sizeof(Node*));
    }
    
    // if lastInsertPos != 0 then we have found some duplicates causing the
    // first element to not be placed at mElements[0]
    mElementCount += aNodes->mElementCount - lastInsertPos;
    if (lastInsertPos) {
        memmove(mElements,
                mElements + lastInsertPos,
                mElementCount * sizeof(Node*));
    }

    return NS_OK;
}

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
 * @return true on success. false on failure.
 */
nsresult NodeSet::append(Node* aNode)
{
    NS_ASSERTION(aNode, "missing node to NodeSet::append");
    if (!aNode)
        return NS_ERROR_NULL_POINTER;

    if (!ensureSize(mElementCount + 1))
        return NS_ERROR_OUT_OF_MEMORY;

    mElements[mElementCount++] = aNode;

    return NS_OK;
}

/*
 * Appends the nodes in the specified NodeSet to the end of this NodeSet
 * @param  aNodes the NodeSet to append to the NodeSet
 * @return true on success. false on failure.
 */
nsresult NodeSet::append(const NodeSet* aNodes)
{
    NS_ASSERTION(aNodes, "missing nodeset to NodeSet::append");
    if (!aNodes)
        return NS_ERROR_NULL_POINTER;

    if (!ensureSize(mElementCount + aNodes->mElementCount))
        return NS_ERROR_OUT_OF_MEMORY;

    memcpy(mElements + mElementCount,
           aNodes->mElements,
           aNodes->mElementCount * sizeof(Node*));
    mElementCount += aNodes->mElementCount;

    return NS_OK;
}

/*
 * Reverse the order of the nodes.
 */
void NodeSet::reverse()
{
    int i;
    for (i = 0; i < mElementCount / 2; ++i) {
        Node* tmp;
        tmp = mElements[i];
        mElements[i] = mElements[mElementCount - 1 - i];
        mElements[mElementCount - 1 - i] = tmp;
    }
}

/*
 * Returns the index of the specified Node,
 * or -1 if the Node is not contained in the NodeSet
 * @param  aNode the Node to get the index for
 * @return index of specified node or -1 if the node does not exist
 */
int NodeSet::indexOf(Node* aNode) const
{
    // XXX this doesn't fully work since attribute-nodes are broken
    // and can't be pointer-compared. However it's the best we can
    // do for now.
    int i;
    for (i = 0; i < mElementCount; ++i) {
        if (mElements[i] == aNode)
            return i;
    }
    return -1;
}

/*
 * Returns the Node at the specified position in this NodeSet.
 * @param  aIndex the position of the Node to return
 * @return Node at specified position
 */
Node* NodeSet::get(int aIndex) const
{
    NS_ASSERTION(aIndex >= 0 && aIndex < mElementCount,
                 "invalid index in NodeSet::get");
    if (aIndex < 0 || aIndex >= mElementCount)
        return 0;

    return mElements[aIndex];
}

/*
 * Clones this ExprResult
 * @return clone of this ExprResult
 */
ExprResult* NodeSet::clone()
{
    return new NodeSet(*this);
}

/*
 * Returns the type of ExprResult represented
 * @return the type of ExprResult represented
 */
short NodeSet::getResultType()
{
    return ExprResult::NODESET;
}

/*
 * Converts this ExprResult to a Boolean (MBool) value
 * @return the Boolean value
 */
MBool NodeSet::booleanValue()
{
    return mElementCount > 0;
}

/*
 * Converts this ExprResult to a Number (double) value
 * @return the Number value
 */
double NodeSet::numberValue()
{
    nsAutoString str;
    stringValue(str);
    return Double::toDouble(str);
}

/*
 * Creates a String representation of this ExprResult
 * @param aStr the destination string to append the String representation to.
 */
void NodeSet::stringValue(nsAString& aStr)
{
    if (mElementCount > 0)
        XMLDOMUtils::getNodeValue(get(0), aStr);
}

/*
 * Makes sure that the mElements buffer contains at least aSize elements.
 * If a new allocation is required the elements are copied over to the new
 * buffer
 * @param  aSize requested number of elements
 * @return true if allocation succeded, false on out of memory
 */
MBool NodeSet::ensureSize(int aSize)
{
    if (aSize <= mBufferSize)
        return MB_TRUE;

    // This isn't 100% safe. But until someone manages to make a 1gig nodeset
    // it should be ok.
    int newSize = mBufferSize ? mBufferSize : kTxNodeSetMinSize;
    while (newSize < aSize)
        newSize *= kTxNodeSetGrowFactor;

    Node** newArr = new Node*[newSize];
    if (!newArr)
        return MB_FALSE;
    
    if (mElementCount)
        memcpy(newArr, mElements, mElementCount * sizeof(Node*));

    delete [] mElements;
    mElements = newArr;
    mBufferSize = newSize;
    
    return MB_TRUE;
}

/*
 * Finds position in the mElements buffer where a node should be inserted
 * to keep the nodeset in document order. Searches the positions
 * aFirst-aLast, including both aFirst and aLast.
 * @param  aNode   Node to find insert position for
 * @param  aFirst  First index to search, this index will be searched
 * @param  aLast   Last index to search, this index will be searched
 * @param  aNonDup Out-param. Set to true if the node should be inserted,
 *                 false if it already exists in the NodeSet.
 * @return         The index where to insert the node. The node should be
 *                 inserted before the node at this index. This value is
 *                 always >= aFirst and <= aLast + 1. This value is always
 *                 set, even if aNode already exists in the NodeSet
 */
int NodeSet::findPosition(Node* aNode, int aFirst,
                          int aLast, MBool& aNonDup) const
{
    NS_ASSERTION(aNode, "missing node in NodeSet::findPosition");
    NS_ASSERTION(aFirst <= aLast+1 && aLast < mElementCount,
                 "bad position in NodeSet::findPosition");

    if (aLast - aFirst <= 1) {
        // If we search 2 nodes or less there is no point in further divides
        int pos;
        for (pos = aFirst; pos <= aLast; ++pos) {
            int cmp = aNode->compareDocumentPosition(mElements[pos]);
            if (cmp < 0) {
                aNonDup = MB_TRUE;
                return pos;
            }

            if (cmp == 0) {
                aNonDup = MB_FALSE;
                return pos;
            }
        }

        aNonDup = MB_TRUE;
        return pos;
    }
    
    int midpos = (aFirst + aLast) / 2;
    int cmp = aNode->compareDocumentPosition(mElements[midpos]);
    if (cmp == 0) {
        aNonDup = MB_FALSE;
        return midpos;
    }

    if (cmp > 0)
        return findPosition(aNode, midpos + 1, aLast, aNonDup);

    return findPosition(aNode, aFirst, midpos - 1, aNonDup);
}
