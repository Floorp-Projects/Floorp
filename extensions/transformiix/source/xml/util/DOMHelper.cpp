/*
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
DOMHelper::DOMHelper() {};

/**
 * Delets this DOMHelper
**/
DOMHelper::~DOMHelper() {
  ListIterator* iter = indexes.iterator();
  while (iter->hasNext()) {
    IndexState* idxState = (IndexState*)iter->next();
    delete idxState;
  }
  delete iter;
} //-- ~DOMHelper


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

/**
 * Generates a unique ID for the given node and places the result in
 * dest
**/
void DOMHelper::generateId(Node* node, String& dest) {

    if (!node) {
        dest.append("<null>");
        return;
    }

    dest.append("id");

    if (node->getNodeType() == Node::DOCUMENT_NODE) {
        Integer::toString(NS_PTR_TO_INT32(node),dest);
        return;
    }

    Integer::toString(NS_PTR_TO_INT32(node->getOwnerDocument()), dest);

    OrderInfo* orderInfo = getDocumentOrder(node);

    int i = 0;
    while (i < orderInfo->size) {
        dest.append('.');
        Integer::toString(orderInfo->order[i], dest);
        ++i;
    }

} //-- generateId

/**
 * Returns the parent of the given node. This method is available
 * mainly to compensate for the fact that Attr nodes in DOM 1.0
 * do not have parents. (Why??)
 * @param node the Node to return the parent of
**/
Node* DOMHelper::getParentNode(Node* node) {

    if (!node) return 0;
    //XXX Namespace: the parent of a namespace node is the element
    if (node->getNodeType() != Node::ATTRIBUTE_NODE)
        return node->getParentNode();

#ifndef TX_EXE
    // XXX temporary fix for 70979
    nsCOMPtr<nsIDOMAttr> attr(do_QueryInterface(node->getNSObj()));
    nsCOMPtr<nsIDOMElement> tmpParent;

    if (attr && NS_SUCCEEDED(attr->GetOwnerElement(getter_AddRefs(tmpParent))))
        return node->getOwnerDocument()->createWrapper(tmpParent);
    return NULL;
#else
    TxObjectWrapper* wrapper = 0;

    wrapper = (TxObjectWrapper*) parents.get(node);
    if (!wrapper) {
        continueIndexing(node);
        wrapper = (TxObjectWrapper*) parents.get(node);
    }

    if (wrapper) return (Node*)wrapper->object;
    return 0;    
#endif
} //-- getParentNode


//-------------------/
//- Private Methods -/
//-------------------/

/**
 * Adds the given child/parent mapping
**/
void DOMHelper::addParentReference(Node* child, Node* parent) {
    TxObjectWrapper* wrapper = (TxObjectWrapper*) parents.get(child);
    if (!wrapper) {
        wrapper = new TxObjectWrapper();
        parents.put(wrapper, child);
    } 
    wrapper->object = parent;

} //-- addParentReference

/**
 * Continues indexing until the given node has been indexed
 * @param node the target Node
**/
void DOMHelper::continueIndexing(Node* node) {
    if (!node) return;

    //-- get indexing information
    Document* doc = 0;
    if (node->getNodeType() == Node::DOCUMENT_NODE)
        doc = (Document*)node;
    else
        doc = node->getOwnerDocument();

    ListIterator* iter = indexes.iterator();
    IndexState* idxState = 0;
    while (iter->hasNext()) {
        idxState = (IndexState*)iter->next();
        if (idxState->document == doc) break;
            idxState = 0;
    }

    if (!idxState) {
        idxState = new IndexState();
        indexes.add(idxState);
        idxState->document = doc;
        idxState->next = doc->getDocumentElement();
        if (!idxState->next) idxState->done = MB_TRUE;
    }

    if (idxState->done) return;

    MBool found = MB_FALSE;
    MBool alreadyProcessed = MB_FALSE;

    while (!found) {

        if (!idxState->next) {
            idxState->done = MB_TRUE;
            break;
        }

        if (!alreadyProcessed) {
            //-- index attributes
            if (idxState->next->getNodeType() == Node::ELEMENT_NODE) {
                Element* element = (Element*)idxState->next;
                NamedNodeMap* atts = element->getAttributes();
                if (atts) {
                    for (PRUint32 i = 0; i < atts->getLength(); i++) {
                        Node* tmpNode = atts->item(i);
                        addParentReference(tmpNode, element);
                        if (node == tmpNode) found = MB_TRUE;
                    }
                }
            } 
        }
        
        //-- set next node to check
        if ((!alreadyProcessed) && (idxState->next->hasChildNodes())) {
            Node* child = idxState->next->getFirstChild();
            idxState->next = child;
        }
        else if (idxState->next->getNextSibling()) {
            idxState->next = idxState->next->getNextSibling();
            alreadyProcessed = MB_FALSE;
        }
        else {
            idxState->next = getParentNode(idxState->next);
            alreadyProcessed = MB_TRUE;
        }     
    }

} //-- continueIndexing


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
            Node* parent = getParentNode(node);
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
        orders.put(orderInfo, node);
    }

    return orderInfo;
  
} //-- getDocumentOrder

//--------------------------------/
//- implementation of IndexState -/
//--------------------------------/

/**
 * Creates a new IndexState
**/
IndexState::IndexState() {
    document = 0;
    done     = MB_FALSE;
    next     = 0;
} //-- IndexState

/**
 * Deletes this IndexState
**/
IndexState::~IndexState() {};

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
