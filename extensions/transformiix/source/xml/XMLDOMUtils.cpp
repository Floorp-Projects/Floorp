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
 * $Id: XMLDOMUtils.cpp,v 1.4 2000/08/23 06:11:45 kvisco%ziplink.net Exp $
 */

/**
 * XMLDOMUtils
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.4 $ $Date: 2000/08/23 06:11:45 $
**/

#include "XMLDOMUtils.h"

/**
 *  Copies the given Node, using the owner Document to create all
 *  necessary new Node(s)
**/
Node* XMLDOMUtils::copyNode(Node* node, Document* owner) {

    if ( !node ) return 0;

    short nodeType = node->getNodeType();

    //-- make sure owner exists if we are copying nodes other than
    //-- document nodes
    if ((nodeType != Node::DOCUMENT_NODE) && (!owner)) return 0;
    Node* newNode = 0;
    int i = 0;
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
            NodeList* nl = doc->getChildNodes();
            for (i = 0; i < nl->getLength(); i++) {
                newDoc->appendChild(copyNode(nl->item(i), newDoc));
            }
            newNode = newDoc;
            break;
        }
        case Node::DOCUMENT_FRAGMENT_NODE :
        {
            newNode = owner->createDocumentFragment();
            NodeList* nl = node->getChildNodes();
            for (i = 0; i < nl->getLength(); i++) {
                newNode->appendChild(copyNode(nl->item(i), owner));
            }
            break;
        }
        case Node::ELEMENT_NODE :
        {
            Element* element = (Element*)node;
            Element* newElement = owner->createElement(element->getNodeName());

            //-- copy atts
            NamedNodeMap* attList = element->getAttributes();
            if ( attList ) {
                for ( i = 0; i < attList->getLength(); i++ ) {
                    Attr* attr = (Attr*) attList->item(i);
                    newElement->setAttribute(attr->getName(), attr->getValue());
                }
            }
            //-- copy children
            NodeList* nl = element->getChildNodes();
            for (i = 0; i < nl->getLength(); i++) {
                newElement->appendChild(copyNode(nl->item(i), owner));
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
    Element* element = 0;
    NodeList* nl = 0;

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
            nl = node->getChildNodes();
            for ( int i = 0; i < nl->getLength(); i++) {
                getNodeValue(nl->item(i),target);
            }
            break;
        }
        case Node::TEXT_NODE :
            target->append ( ((Text*)node)->getData() );
            break;
        case Node::COMMENT_NODE :
        case Node::PROCESSING_INSTRUCTION_NODE :
            target->append ( node->getNodeValue() );
            break;

    } //-- switch

} //-- getNodeValue

