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
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 * 
 * Contributor(s): 
 * Keith Visco 
 *    -- original author.
 *
 */

/**
 * XMLDOMUtils
**/

#include "XMLDOMUtils.h"

/**
 *  Copies the given Node, using the owner Document to create all
 *  necessary new Node(s)
**/
Node* XMLDOMUtils::copyNode(Node* node, Document* owner, NamespaceResolver* resolver) {

    if ( !node ) return 0;

    short nodeType = node->getNodeType();

    //-- make sure owner exists if we are copying nodes other than
    //-- document nodes
    if (nodeType != Node::DOCUMENT_NODE && !owner) return 0;
    Node* newNode = 0;
    PRUint32 i = 0;
    switch ( nodeType ) {

        case Node::ATTRIBUTE_NODE :
        {
            Attr* attr = (Attr*)node;
            Attr* newAttr = owner->createAttribute(attr->getName());
            newAttr->setValue(attr->getValue());
            newNode = newAttr;
            break;
        }
        case Node::DOCUMENT_NODE :
        {
            Document* doc = (Document*)node;
            Document* newDoc = new Document();
            if (!newDoc)
                break;
#ifndef TX_EXE
            owner->addWrapper(newDoc);
#endif
            NodeList* nl = doc->getChildNodes();
            for (i = 0; i < nl->getLength(); i++) {
                newDoc->appendChild(copyNode(nl->item(i), newDoc, resolver));
            }
            newNode = newDoc;
            break;
        }
        case Node::DOCUMENT_FRAGMENT_NODE :
        {
            newNode = owner->createDocumentFragment();
            Node* tmpNode = node->getFirstChild();
            while (tmpNode) {
                newNode->appendChild(copyNode(tmpNode, owner, resolver));
                tmpNode = tmpNode->getNextSibling();
            }
            break;
        }
        case Node::ELEMENT_NODE :
        {
            Element* element = (Element*)node;
#ifndef TX_EXE
            String name, nameSpaceURI;
            name = element->getNodeName();
            resolver->getResultNameSpaceURI(name, nameSpaceURI);
            Element* newElement = owner->createElementNS(nameSpaceURI, name);
#else
            Element* newElement = owner->createElement(element->getNodeName());
#endif

            //-- copy atts
            NamedNodeMap* attList = element->getAttributes();
            if ( attList ) {
                for ( i = 0; i < attList->getLength(); i++ ) {
                    Attr* attr = (Attr*) attList->item(i);
#ifndef TX_EXE
                    resolver->getResultNameSpaceURI(attr->getName(), nameSpaceURI);
                    newElement->setAttributeNS(nameSpaceURI, attr->getName(), attr->getValue());
#else
                    newElement->setAttribute(attr->getName(), attr->getValue());
#endif
                }
            }
            //-- copy children
            Node* tmpNode = element->getFirstChild();
            while (tmpNode) {
                newElement->appendChild(copyNode(tmpNode, owner, resolver));
                tmpNode = tmpNode->getNextSibling();
            }
            newNode = newElement;
            break;
        }
        case Node::TEXT_NODE :
            newNode = owner->createTextNode(((Text*)node)->getData());
            break;
        case Node::CDATA_SECTION_NODE :
            newNode = owner->createCDATASection(((CDATASection*)node)->getData());
            break;
        case Node::PROCESSING_INSTRUCTION_NODE:
        {
            ProcessingInstruction* pi = (ProcessingInstruction*)node;
            newNode = owner->createProcessingInstruction(pi->getTarget(), pi->getData());
            break;
        }
        case Node::COMMENT_NODE:
            newNode = owner->createComment(((Comment*)node)->getData());
            break;
        default:
            break;
    } //-- switch

    return newNode;
} //-- copyNode

void XMLDOMUtils::getNodeValue(Node* node, String* target) {

    if (!node) return;

    int nodeType = node->getNodeType();

    switch ( nodeType ) {
        case Node::ATTRIBUTE_NODE :
            target->append( ((Attr*)node)->getValue() );
            break;
        case Node::DOCUMENT_NODE :
            getNodeValue( ((Document*)node)->getDocumentElement(), target);
            break;
        case Node::DOCUMENT_FRAGMENT_NODE :
        case Node::ELEMENT_NODE :
        {
            Node* tmpNode = node->getFirstChild();
            while (tmpNode) {
                nodeType = tmpNode->getNodeType();
                if (nodeType == Node::TEXT_NODE ||
                    nodeType == Node::ELEMENT_NODE ||
                    nodeType == Node::CDATA_SECTION_NODE) {
                    getNodeValue(tmpNode,target);
                };
                tmpNode = tmpNode->getNextSibling();
            }
            break;
        }
        case Node::TEXT_NODE :
        case Node::CDATA_SECTION_NODE :
            target->append ( ((Text*)node)->getData() );
            break;
        case Node::COMMENT_NODE :
        case Node::PROCESSING_INSTRUCTION_NODE :
            target->append ( node->getNodeValue() );
            break;

    } //-- switch

} //-- getNodeValue

/**
 * Resolves the namespace for the given prefix. The namespace is put
 * into the dest argument.
 *
 * @return true if the namespace was found
**/
MBool XMLDOMUtils::getNameSpace
    (const String& prefix, Element* element, String& dest)
{
    String attName("xmlns");
    if (!prefix.isEmpty()) {
        attName.append(':');
        attName.append(prefix);
    }

    dest.clear();

    Element* elem = element;
    while( elem ) {
        NamedNodeMap* atts = elem->getAttributes();
        if ( atts ) {
            Node* attr = atts->getNamedItem(attName);
            if (attr) {
                dest.append(attr->getNodeValue());
                return MB_TRUE;
             }
        }
        Node* node = elem->getParentNode();
        if ((!node) || (node->getNodeType() != Node::ELEMENT_NODE)) break;
        elem = (Element*) node;
    }
    return MB_FALSE;

} //-- getNameSpace

