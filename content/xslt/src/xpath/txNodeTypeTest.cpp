/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Keith Visco <kvisco@ziplink.net> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "Expr.h"
#include "nsIAtom.h"
#include "txIXPathContext.h"
#include "txXPathTreeWalker.h"

/*
 * Creates a new txNodeTypeTest of the given type
 */
txNodeTypeTest::txNodeTypeTest(NodeType aNodeType)
    : mNodeType(aNodeType)
{
}

txNodeTypeTest::~txNodeTypeTest()
{
}

void txNodeTypeTest::setNodeName(const nsAString& aName)
{
    mNodeName = do_GetAtom(aName);
}

PRBool txNodeTypeTest::matches(const txXPathNode& aNode,
                               txIMatchContext* aContext)
{
    PRUint16 type = txXPathNodeUtils::getNodeType(aNode);

    switch (mNodeType) {
        case COMMENT_TYPE:
            return type == txXPathNodeType::COMMENT_NODE;
        case TEXT_TYPE:
            return (type == txXPathNodeType::TEXT_NODE ||
                    type == txXPathNodeType::CDATA_SECTION_NODE) &&
                   !aContext->isStripSpaceAllowed(aNode);
        case PI_TYPE:
            if (type == txXPathNodeType::PROCESSING_INSTRUCTION_NODE) {
                nsCOMPtr<nsIAtom> localName;
                return !mNodeName ||
                        ((localName = txXPathNodeUtils::getLocalName(aNode)) &&
                         localName == mNodeName);
            }
            return MB_FALSE;
        case NODE_TYPE:
            return ((type != txXPathNodeType::TEXT_NODE &&
                     type != txXPathNodeType::CDATA_SECTION_NODE) ||
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

#ifdef TX_TO_STRING
void
txNodeTypeTest::toString(nsAString& aDest)
{
    switch (mNodeType) {
        case COMMENT_TYPE:
            aDest.Append(NS_LITERAL_STRING("comment()"));
            break;
        case TEXT_TYPE:
            aDest.Append(NS_LITERAL_STRING("text()"));
            break;
        case PI_TYPE:
            aDest.AppendLiteral("processing-instruction(");
            if (mNodeName) {
                nsAutoString str;
                mNodeName->ToString(str);
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
#endif
