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
 *   Olivier Gerardin <ogerardin@vo.lu> (Original Author)
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

/*
 * DocumentFunctionCall
 * A representation of the XSLT additional function: document()
 */

#include "nsGkAtoms.h"
#include "txIXPathContext.h"
#include "txXSLTFunctions.h"
#include "txExecutionState.h"
#include "txURIUtils.h"

/*
 * Creates a new DocumentFunctionCall.
 */
DocumentFunctionCall::DocumentFunctionCall(const nsAString& aBaseURI)
    : mBaseURI(aBaseURI)
{
}

static void
retrieveNode(txExecutionState* aExecutionState, const nsAString& aUri,
             const nsAString& aBaseUri, txNodeSet* aNodeSet)
{
    nsAutoString absUrl;
    URIUtils::resolveHref(aUri, aBaseUri, absUrl);

    PRInt32 hash = absUrl.RFindChar(PRUnichar('#'));
    PRUint32 urlEnd, fragStart, fragEnd;
    if (hash == kNotFound) {
        urlEnd = absUrl.Length();
        fragStart = 0;
        fragEnd = 0;
    }
    else {
        urlEnd = hash;
        fragStart = hash + 1;
        fragEnd = absUrl.Length();
    }

    nsDependentSubstring docUrl(absUrl, 0, urlEnd);
    nsDependentSubstring frag(absUrl, fragStart, fragEnd);

    const txXPathNode* loadNode = aExecutionState->retrieveDocument(docUrl);
    if (loadNode) {
        if (frag.IsEmpty()) {
            aNodeSet->add(*loadNode);
        }
        else {
            txXPathTreeWalker walker(*loadNode);
            if (walker.moveToElementById(frag)) {
                aNodeSet->add(walker.getCurrentPosition());
            }
        }
    }
}

/*
 * Evaluates this Expr based on the given context node and processor state
 * NOTE: the implementation is incomplete since it does not make use of the
 * second argument (base URI)
 * @param context the context node for evaluation of this Expr
 * @return the result of the evaluation
 */
nsresult
DocumentFunctionCall::evaluate(txIEvalContext* aContext,
                               txAExprResult** aResult)
{
    *aResult = nsnull;
    txExecutionState* es =
        static_cast<txExecutionState*>(aContext->getPrivateContext());

    nsRefPtr<txNodeSet> nodeSet;
    nsresult rv = aContext->recycler()->getNodeSet(getter_AddRefs(nodeSet));
    NS_ENSURE_SUCCESS(rv, rv);

    // document(object, node-set?)
    if (!requireParams(1, 2, aContext)) {
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }

    nsRefPtr<txAExprResult> exprResult1;
    rv = mParams[0]->evaluate(aContext, getter_AddRefs(exprResult1));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString baseURI;
    MBool baseURISet = MB_FALSE;

    if (mParams.Length() == 2) {
        // We have 2 arguments, get baseURI from the first node
        // in the resulting nodeset
        nsRefPtr<txNodeSet> nodeSet2;
        rv = evaluateToNodeSet(mParams[1],
                               aContext, getter_AddRefs(nodeSet2));
        NS_ENSURE_SUCCESS(rv, rv);

        // Make this true, even if nodeSet2 is empty. For relative URLs,
        // we'll fail to load the document with an empty base URI, and for
        // absolute URLs, the base URI doesn't matter
        baseURISet = MB_TRUE;

        if (!nodeSet2->isEmpty()) {
            txXPathNodeUtils::getBaseURI(nodeSet2->get(0), baseURI);
        }
    }

    if (exprResult1->getResultType() == txAExprResult::NODESET) {
        // The first argument is a NodeSet, iterate on its nodes
        txNodeSet* nodeSet1 = static_cast<txNodeSet*>
                                         (static_cast<txAExprResult*>
                                                     (exprResult1));
        PRInt32 i;
        for (i = 0; i < nodeSet1->size(); ++i) {
            const txXPathNode& node = nodeSet1->get(i);
            nsAutoString uriStr;
            txXPathNodeUtils::appendNodeValue(node, uriStr);
            if (!baseURISet) {
                // if the second argument wasn't specified, use
                // the baseUri of node itself
                txXPathNodeUtils::getBaseURI(node, baseURI);
            }
            retrieveNode(es, uriStr, baseURI, nodeSet);
        }
        
        NS_ADDREF(*aResult = nodeSet);
        
        return NS_OK;
    }

    // The first argument is not a NodeSet
    nsAutoString uriStr;
    exprResult1->stringValue(uriStr);
    const nsAString* base = baseURISet ? &baseURI : &mBaseURI;
    retrieveNode(es, uriStr, *base, nodeSet);

    NS_ADDREF(*aResult = nodeSet);

    return NS_OK;
}

Expr::ResultType
DocumentFunctionCall::getReturnType()
{
    return NODESET_RESULT;
}

PRBool
DocumentFunctionCall::isSensitiveTo(ContextSensitivity aContext)
{
    return (aContext & PRIVATE_CONTEXT) || argsSensitiveTo(aContext);
}

#ifdef TX_TO_STRING
nsresult
DocumentFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = nsGkAtoms::document;
    NS_ADDREF(*aAtom);
    return NS_OK;
}
#endif
