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

#include "XMLDOMUtils.h"
#include "dom.h"
#include "nsString.h"

void XMLDOMUtils::getNodeValue(Node* aNode, nsAString& aResult)
{
    if (!aNode)
        return;

    unsigned short nodeType = aNode->getNodeType();

    switch (nodeType) {
        case Node::ATTRIBUTE_NODE:
        case Node::CDATA_SECTION_NODE:
        case Node::COMMENT_NODE:
        case Node::PROCESSING_INSTRUCTION_NODE:
        case Node::TEXT_NODE:
        {
            nsAutoString nodeValue;
            aNode->getNodeValue(nodeValue);
            aResult.Append(nodeValue);
            break;
        }
        case Node::DOCUMENT_NODE:
        {
            getNodeValue(((Document*)aNode)->getDocumentElement(),
                         aResult);
            break;
        }
        case Node::DOCUMENT_FRAGMENT_NODE:
        case Node::ELEMENT_NODE:
        {
            Node* tmpNode = aNode->getFirstChild();
            while (tmpNode) {
                nodeType = tmpNode->getNodeType();
                if ((nodeType == Node::TEXT_NODE) ||
                    (nodeType == Node::CDATA_SECTION_NODE)) {
                    nsAutoString nodeValue;
                    tmpNode->getNodeValue(nodeValue);
                    aResult.Append(nodeValue);
                }
                else if (nodeType == Node::ELEMENT_NODE) {
                    getNodeValue(tmpNode, aResult);
                }
                tmpNode = tmpNode->getNextSibling();
            }
            break;
        }
    }
}
