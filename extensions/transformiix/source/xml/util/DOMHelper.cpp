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
 * $Id: DOMHelper.cpp,v 1.4 2000/08/26 04:20:14 Peter.VanderBeken%pandora.be Exp $
 */

/**
 * A class used to overcome DOM 1.0 deficiencies
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.4 $ $Date: 2000/08/26 04:20:14 $
**/

#include "DOMHelper.h"


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
        Integer::toString((int)node,dest);
        return;
    }

    Integer::toString((int)node->getOwnerDocument(), dest);

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
    if (node->getNodeType() != Node::ATTRIBUTE_NODE)
        return node->getParentNode();

#ifdef MOZ_XSL
    void* key = node->getKey();
#else
    Int32 key = (Int32)node;
#endif
    MITREObjectWrapper* wrapper = 0;

    wrapper = (MITREObjectWrapper*) parents.retrieve(key);
    if (!wrapper) {
        continueIndexing(node);
        wrapper = (MITREObjectWrapper*) parents.retrieve(key);
    }

    if (wrapper) return (Node*)wrapper->object;
    return 0;    

} //-- getParentNode


//-------------------/
//- Private Methods -/
//-------------------/

/**
 * Adds the given child/parent mapping
**/
void DOMHelper::addParentReference(Node* child, Node* parent) {

#ifdef MOZ_XSL
    void* key = child->getKey();
#else
    Int32 key = (Int32)child;
#endif
    MITREObjectWrapper* wrapper = (MITREObjectWrapper*) parents.retrieve(key);
    if (!wrapper) {
        wrapper = new MITREObjectWrapper();
        parents.add(wrapper, key);
    } 
    wrapper->object = parent;

} //-- addParentReference

/**
 * Continues indexing until the given node has been indexed
 * @param node the target Node
**/
void DOMHelper::continueIndexing(Node* node) {
    if (!node) return;

    MITREObjectWrapper* wrapper = 0;

    //-- get indexing information
    Document* doc = node->getOwnerDocument();
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
                    for (int i = 0; i < atts->getLength(); i++) {
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
            while (tmp = tmp->getPreviousSibling()) ++c;     
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

#ifdef MOZ_XSL
    void* key = node->getKey();
#else
    Int32 key = (Int32)node;
#endif
    OrderInfo* orderInfo = (OrderInfo*)orders.retrieve(key);

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
        orders.add(orderInfo, key);
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
OrderInfo::OrderInfo() : MITREObject() {
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
