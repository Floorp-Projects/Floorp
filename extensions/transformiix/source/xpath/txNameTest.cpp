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
#include "txAtoms.h"
#include "txIXPathContext.h"

txNameTest::txNameTest(nsIAtom* aPrefix, nsIAtom* aLocalName, PRInt32 aNSID,
                       Node::NodeType aNodeType)
    :mPrefix(aPrefix), mLocalName(aLocalName), mNamespace(aNSID),
     mNodeType(aNodeType)
{
    if (aPrefix == txXMLAtoms::_empty)
        mPrefix = 0;
    NS_ASSERTION(aLocalName, "txNameTest without a local name?");
    TX_IF_ADDREF_ATOM(mPrefix);
    TX_IF_ADDREF_ATOM(mLocalName);
}

txNameTest::~txNameTest()
{
    TX_IF_RELEASE_ATOM(mPrefix);
    TX_IF_RELEASE_ATOM(mLocalName);
}

/*
 * Determines whether this txNodeTest matches the given node
 */
MBool txNameTest::matches(Node* aNode, txIMatchContext* aContext)
{
    if (!aNode || aNode->getNodeType() != mNodeType)
        return MB_FALSE;

    // Totally wild?
    if (mLocalName == txXPathAtoms::_asterix && !mPrefix)
        return MB_TRUE;

    // Compare namespaces
    if (aNode->getNamespaceID() != mNamespace)
        return MB_FALSE;

    // Name wild?
    if (mLocalName == txXPathAtoms::_asterix)
        return MB_TRUE;

    // Compare local-names
    nsIAtom* localName;
    aNode->getLocalName(&localName);
    MBool result = localName == mLocalName;
    TX_IF_RELEASE_ATOM(localName);

    return result;
}

/*
 * Returns the default priority of this txNodeTest
 */
double txNameTest::getDefaultPriority()
{
    if (mLocalName == txXPathAtoms::_asterix) {
        if (!mPrefix)
            return -0.5;
        return -0.25;
    }
    return 0;
}

/*
 * Returns the String representation of this txNodeTest.
 * @param aDest the String to use when creating the string representation.
 *              The string representation will be appended to the string.
 */
void txNameTest::toString(nsAString& aDest)
{
    if (mPrefix) {
        const PRUnichar* prefix;
        mPrefix->GetUnicode(&prefix);
        aDest.Append(nsDependentString(prefix));
        aDest.Append(PRUnichar(':'));
    }
    const PRUnichar* localName;
    mLocalName->GetUnicode(&localName);
    aDest.Append(nsDependentString(localName));
}
