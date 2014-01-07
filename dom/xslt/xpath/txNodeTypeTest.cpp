/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "nsIAtom.h"
#include "txIXPathContext.h"
#include "txXPathTreeWalker.h"

bool txNodeTypeTest::matches(const txXPathNode& aNode,
                               txIMatchContext* aContext)
{
    switch (mNodeType) {
        case COMMENT_TYPE:
        {
            return txXPathNodeUtils::isComment(aNode);
        }
        case TEXT_TYPE:
        {
            return txXPathNodeUtils::isText(aNode) &&
                   !aContext->isStripSpaceAllowed(aNode);
        }
        case PI_TYPE:
        {
            return txXPathNodeUtils::isProcessingInstruction(aNode) &&
                   (!mNodeName ||
                    txXPathNodeUtils::localNameEquals(aNode, mNodeName));
        }
        case NODE_TYPE:
        {
            return !txXPathNodeUtils::isText(aNode) ||
                   !aContext->isStripSpaceAllowed(aNode);
        }
    }
    return true;
}

txNodeTest::NodeTestType
txNodeTypeTest::getType()
{
    return NODETYPE_TEST;
}

/*
 * Returns the default priority of this txNodeTest
 */
double txNodeTypeTest::getDefaultPriority()
{
    return mNodeName ? 0 : -0.5;
}

bool
txNodeTypeTest::isSensitiveTo(Expr::ContextSensitivity aContext)
{
    return !!(aContext & Expr::NODE_CONTEXT);
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
                aDest.Append(char16_t('\''));
                aDest.Append(str);
                aDest.Append(char16_t('\''));
            }
            aDest.Append(char16_t(')'));
            break;
        case NODE_TYPE:
            aDest.Append(NS_LITERAL_STRING("node()"));
            break;
    }
}
#endif
