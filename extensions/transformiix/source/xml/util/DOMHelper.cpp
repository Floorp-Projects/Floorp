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
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco (C) 1999 Keith Visco.
 * All Rights Reserved..
 *
 * Contributor(s): 
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 */

/**
 * A class used to overcome DOM 1.0 deficiencies
**/

#include "baseutils.h"
#include "DOMHelper.h"
#include "primitives.h"

/**
 * Creates a new DOMHelper
**/
DOMHelper::DOMHelper() {
    orders.setOwnership(Map::eOwnsItems);
};

/**
 * Delets this DOMHelper
**/
DOMHelper::~DOMHelper() {}


/**
 * Returns the node which appears first in the document
 * If this method is called with nodes from a different
 * document, node1 will be returned.
 * @return the node which appears first in document order
**/
Node* DOMHelper::appearsFirst(Node* node1, Node* node2) {

    if (!node2) return node1;
    if (!node1) return node2;

    if (node1->getOwnerDocument() != node2->getOwnerDocument()) 
        return node1;

    OrderInfo* orderInfo1 = getDocumentOrder(node1);
    OrderInfo* orderInfo2 = getDocumentOrder(node2);
 
    int compare = orderInfo1->compareTo(orderInfo2);
    if (compare > 0) return node2;
    return node1;
    
} //-- compareDocumentOrders

//-------------------/
//- Private Methods -/
//-------------------/

/**
 * Returns the child number of the given node. Numbering
 * starts at 1 for all nodes except the Document node and 
 * attribute nodes which has child numbers of 0. The child
 * number is calculated by counting the number of times
 * Node#getPreviousSibling can be called.
 * @param node a pointer to the node in which to return the
 * child number of.
 * @return the child number for the given node
**/
int DOMHelper::getChildNumber(Node* node) {

    if (!node) return -1;

    int c = 0;
    Node* tmp = node;
    switch (node->getNodeType()) {
        case Node::DOCUMENT_NODE:
        case Node::ATTRIBUTE_NODE:
            break;
        default:
            while ((tmp = tmp->getPreviousSibling()))
                ++c;     
            break;
    }
    

  return c;
} //-- getChildNumber

/**
 * Returns the DocumentOrder for the given Node
 * @param node a pointer to the Node in which to return the 
 * DocumentOrder of
 * @return the DocumentOrder for the given Node
**/
OrderInfo* DOMHelper::getDocumentOrder(Node* node) {

    if (!node) return 0;

    OrderInfo* orderInfo = (OrderInfo*)orders.get(node);

    if (!orderInfo) {
        if (node->getNodeType() == Node::DOCUMENT_NODE) {
            orderInfo = new OrderInfo();
            orderInfo->size = 1;
            orderInfo->order = new int[1];
            orderInfo->order[0] = 0;
        }
        else {
            Node* parent = node->getXPathParent();
            OrderInfo* parentOrder = getDocumentOrder(parent);
            orderInfo = new OrderInfo();

            if (parentOrder) {
                orderInfo->size  = parentOrder->size+1;
                orderInfo->order = new int[orderInfo->size];
                int c = 0;
                for ( ; c < parentOrder->size; c++)
                    orderInfo->order[c] = parentOrder->order[c];
                orderInfo->order[c] = getChildNumber(node);
            }
            else {
                orderInfo->size = 1;
                orderInfo->order = new int[1];
                orderInfo->order[0] = 0;            
            }
        }
        orders.put(node, orderInfo);
    }

    return orderInfo;
  
} //-- getDocumentOrder

//-------------------------------/
//- Implementation of OrderInfo -/
//-------------------------------/

/**
 * Creates a new OrderInfo
**/
OrderInfo::OrderInfo() : TxObject() {
   order = 0;
   size  = 0;
} //-- OrderInfo

/**
 * Deletes this OrderInfo
**/
OrderInfo::~OrderInfo() {
    if (order) delete [] order;
} //-- ~OrderInfo


/**
 * Compares this OrderInfo with the given OrderInfo
 * @return -1 if this OrderInfo is less than the given OrderInfo;
 * 0 if they are equal; 1 if this OrderInfo is greater.
**/
int OrderInfo::compareTo(OrderInfo* orderInfo) {

    if (!orderInfo) return -1;

    int c = 0;
    while ( (c < size) && (c < orderInfo->size)) {
        if (order[c] < orderInfo->order[c]) return -1;
        else if (order[c] > orderInfo->order[c]) return 1;
        ++c;
    }
    if (c < size) return 1;
    else if (c < orderInfo->size) return -1;
    return 0;

} //-- compareTo
