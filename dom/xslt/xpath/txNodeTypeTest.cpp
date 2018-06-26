/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "nsAtom.h"
#include "txIXPathContext.h"
#include "txXPathTreeWalker.h"

nsresult
txNodeTypeTest::matches(const txXPathNode& aNode, txIMatchContext* aContext,
                        bool& aMatched)
{
    switch (mNodeType) {
        case COMMENT_TYPE:
        {
            aMatched = txXPathNodeUtils::isComment(aNode);
            return NS_OK;
        }
        case TEXT_TYPE:
        {
            aMatched = txXPathNodeUtils::isText(aNode);
            if (aMatched) {
                bool allowed;
                nsresult rv = aContext->isStripSpaceAllowed(aNode, allowed);
                NS_ENSURE_SUCCESS(rv, rv);

                aMatched = !allowed;
            }
            return NS_OK;
        }
        case PI_TYPE:
        {
            aMatched = txXPathNodeUtils::isProcessingInstruction(aNode) &&
                       (!mNodeName ||
                        txXPathNodeUtils::localNameEquals(aNode, mNodeName));
            return NS_OK;
        }
        case NODE_TYPE:
        {
            if (txXPathNodeUtils::isText(aNode)) {
                bool allowed;
                nsresult rv = aContext->isStripSpaceAllowed(aNode, allowed);
                NS_ENSURE_SUCCESS(rv, rv);

                aMatched = !allowed;
            } else {
                aMatched = true;
            }
            return NS_OK;
        }
    }

    MOZ_ASSERT_UNREACHABLE("Didn't deal with all values of the NodeType enum!");

    aMatched = false;
    return NS_OK;
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
            aDest.AppendLiteral("comment()");
            break;
        case TEXT_TYPE:
            aDest.AppendLiteral("text()");
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
            aDest.AppendLiteral("node()");
            break;
    }
}
#endif
