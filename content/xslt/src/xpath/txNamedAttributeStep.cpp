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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
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

#include "nsIAtom.h"
#include "txIXPathContext.h"
#include "txNodeSet.h"
#include "txExpr.h"
#include "txXPathTreeWalker.h"

txNamedAttributeStep::txNamedAttributeStep(PRInt32 aNsID,
                                           nsIAtom* aPrefix,
                                           nsIAtom* aLocalName)
    : mNamespace(aNsID),
      mPrefix(aPrefix),
      mLocalName(aLocalName)
{
}

nsresult
txNamedAttributeStep::evaluate(txIEvalContext* aContext,
                               txAExprResult** aResult)
{
    *aResult = nsnull;

    nsRefPtr<txNodeSet> nodes;
    nsresult rv = aContext->recycler()->getNodeSet(getter_AddRefs(nodes));
    NS_ENSURE_SUCCESS(rv, rv);

    txXPathTreeWalker walker(aContext->getContextNode());
    if (walker.moveToNamedAttribute(mLocalName, mNamespace)) {
        rv = nodes->append(walker.getCurrentPosition());
        NS_ENSURE_SUCCESS(rv, rv);
    }
    NS_ADDREF(*aResult = nodes);

    return NS_OK;
}

TX_IMPL_EXPR_STUBS_0(txNamedAttributeStep, NODESET_RESULT)

bool
txNamedAttributeStep::isSensitiveTo(ContextSensitivity aContext)
{
    return !!(aContext & NODE_CONTEXT);
}

#ifdef TX_TO_STRING
void
txNamedAttributeStep::toString(nsAString& aDest)
{
    aDest.Append(PRUnichar('@'));
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
