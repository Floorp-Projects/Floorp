/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"
#include "nsIAtom.h"
#include "nsGkAtoms.h"
#include "txXPathTreeWalker.h"
#include "txIXPathContext.h"

txNameTest::txNameTest(nsIAtom* aPrefix, nsIAtom* aLocalName, int32_t aNSID,
                       uint16_t aNodeType)
    :mPrefix(aPrefix), mLocalName(aLocalName), mNamespace(aNSID),
     mNodeType(aNodeType)
{
    if (aPrefix == nsGkAtoms::_empty)
        mPrefix = nullptr;
    NS_ASSERTION(aLocalName, "txNameTest without a local name?");
    NS_ASSERTION(aNodeType == txXPathNodeType::DOCUMENT_NODE ||
                 aNodeType == txXPathNodeType::ELEMENT_NODE ||
                 aNodeType == txXPathNodeType::ATTRIBUTE_NODE,
                 "Go fix txNameTest::matches");
}

nsresult
txNameTest::matches(const txXPathNode& aNode, txIMatchContext* aContext,
                    bool& aMatched)
{
    if ((mNodeType == txXPathNodeType::ELEMENT_NODE &&
         !txXPathNodeUtils::isElement(aNode)) ||
        (mNodeType == txXPathNodeType::ATTRIBUTE_NODE &&
         !txXPathNodeUtils::isAttribute(aNode)) ||
        (mNodeType == txXPathNodeType::DOCUMENT_NODE &&
         !txXPathNodeUtils::isRoot(aNode))) {
        aMatched = false;
        return NS_OK;
    }

    // Totally wild?
    if (mLocalName == nsGkAtoms::_asterisk && !mPrefix) {
        aMatched = true;
        return NS_OK;
    }

    // Compare namespaces
    if (mNamespace != txXPathNodeUtils::getNamespaceID(aNode)
        && !(mNamespace == kNameSpaceID_None &&
             txXPathNodeUtils::isHTMLElementInHTMLDocument(aNode))
       ) {
        aMatched = false;
        return NS_OK;
    }

    // Name wild?
    if (mLocalName == nsGkAtoms::_asterisk) {
        aMatched = true;
        return NS_OK;
    }

    // Compare local-names
    aMatched = txXPathNodeUtils::localNameEquals(aNode, mLocalName);
    return NS_OK;
}

/*
 * Returns the default priority of this txNodeTest
 */
double txNameTest::getDefaultPriority()
{
    if (mLocalName == nsGkAtoms::_asterisk) {
        if (!mPrefix)
            return -0.5;
        return -0.25;
    }
    return 0;
}

txNodeTest::NodeTestType
txNameTest::getType()
{
    return NAME_TEST;
}

bool
txNameTest::isSensitiveTo(Expr::ContextSensitivity aContext)
{
    return !!(aContext & Expr::NODE_CONTEXT);
}

#ifdef TX_TO_STRING
void
txNameTest::toString(nsAString& aDest)
{
    if (mPrefix) {
        nsAutoString prefix;
        mPrefix->ToString(prefix);
        aDest.Append(prefix);
        aDest.Append(char16_t(':'));
    }
    nsAutoString localName;
    mLocalName->ToString(localName);
    aDest.Append(localName);
}
#endif
