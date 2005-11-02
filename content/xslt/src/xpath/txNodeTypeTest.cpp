/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
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
 *   -- original author.
 *
 */

#include "Expr.h"
#include "txAtom.h"
#include "txIXPathContext.h"

/*
 * Creates a new txNodeTypeTest of the given type
 */
txNodeTypeTest::txNodeTypeTest(NodeType aNodeType)
    : mNodeType(aNodeType), mNodeName(0)
{
}

txNodeTypeTest::~txNodeTypeTest()
{
    TX_IF_RELEASE_ATOM(mNodeName);
}

void txNodeTypeTest::setNodeName(const String& aName)
{
    mNodeName = TX_GET_ATOM(aName);
}

/*
 * Determines whether this txNodeTest matches the given node
 */
MBool txNodeTypeTest::matches(Node* aNode, txIMatchContext* aContext)
{
    if (!aNode)
        return MB_FALSE;

    Node::NodeType type = (Node::NodeType)aNode->getNodeType();

    switch (mNodeType) {
        case COMMENT_TYPE:
            return type == Node::COMMENT_NODE;
        case TEXT_TYPE:
            return (type == Node::TEXT_NODE ||
                    type == Node::CDATA_SECTION_NODE) &&
                   !aContext->isStripSpaceAllowed(aNode);
        case PI_TYPE:
            if (type == Node::PROCESSING_INSTRUCTION_NODE) {
                txAtom* localName = 0;
                MBool result;
                result = !mNodeName ||
                         (aNode->getLocalName(&localName) &&
                          localName == mNodeName);
                TX_IF_RELEASE_ATOM(localName);
                return result;
            }
            return MB_FALSE;
        case NODE_TYPE:
            return ((type != Node::TEXT_NODE &&
                     type != Node::CDATA_SECTION_NODE) ||
                    !aContext->isStripSpaceAllowed(aNode));
    }
    return MB_TRUE;
}

/*
 * Returns the default priority of this txNodeTest
 */
double txNodeTypeTest::getDefaultPriority()
{
    return mNodeName ? 0 : -0.5;
}

/*
 * Returns the String representation of this txNodeTest.
 * @param aDest the String to use when creating the string representation.
 *              The string representation will be appended to the string.
 */
void txNodeTypeTest::toString(String& aDest)
{
    switch (mNodeType) {
        case COMMENT_TYPE:
            aDest.Append(NS_LITERAL_STRING("comment()"));
            break;
        case TEXT_TYPE:
            aDest.Append(NS_LITERAL_STRING("text()"));
            break;
        case PI_TYPE:
            aDest.Append(NS_LITERAL_STRING("processing-instruction("));
            if (mNodeName) {
                String str;
                TX_GET_ATOM_STRING(mNodeName, str);
                aDest.Append(PRUnichar('\''));
                aDest.Append(str);
                aDest.Append(PRUnichar('\''));
            }
            aDest.Append(PRUnichar(')'));
            break;
        case NODE_TYPE:
            aDest.Append(NS_LITERAL_STRING("node()"));
            break;
    }
}
