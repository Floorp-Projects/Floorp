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
#include "txAtoms.h"
#include "txXPathTreeWalker.h"
#include "txIXPathContext.h"

txNameTest::txNameTest(nsIAtom* aPrefix, nsIAtom* aLocalName, PRInt32 aNSID,
                       PRUint16 aNodeType)
    :mPrefix(aPrefix), mLocalName(aLocalName), mNamespace(aNSID),
     mNodeType(aNodeType)
{
    if (aPrefix == txXMLAtoms::_empty)
        mPrefix = 0;
    NS_ASSERTION(aLocalName, "txNameTest without a local name?");
}

txNameTest::~txNameTest()
{
}

PRBool txNameTest::matches(const txXPathNode& aNode, txIMatchContext* aContext)
{
    if (txXPathNodeUtils::getNodeType(aNode) != mNodeType)
        return MB_FALSE;

    // Totally wild?
    if (mLocalName == txXPathAtoms::_asterix && !mPrefix)
        return MB_TRUE;

    // Compare namespaces
    if (txXPathNodeUtils::getNamespaceID(aNode) != mNamespace)
        return MB_FALSE;

    // Name wild?
    if (mLocalName == txXPathAtoms::_asterix)
        return MB_TRUE;

    // Compare local-names
    nsCOMPtr<nsIAtom> localName = txXPathNodeUtils::getLocalName(aNode);
    return localName == mLocalName;
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

#ifdef TX_TO_STRING
void
txNameTest::toString(nsAString& aDest)
{
    if (mPrefix) {
        nsAutoString prefix;
        mPrefix->ToString(prefix);
        aDest.Append(prefix);
        aDest.Append(PRUnichar(':'));
    }
    nsAutoString localName;
    mLocalName->ToString(localName);
    aDest.Append(localName);
}
#endif
