/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

/**
 * XMLDOMUtils
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

#include "XMLDOMUtils.h"

void XMLDOMUtils::getNodeValue(Node* node, DOMString* target) {

    if (!node) {
        return;
    }

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
    } //-- switch

} //-- getNodeValue

