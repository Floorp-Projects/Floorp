/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

    int32_t hash = absUrl.RFindChar(char16_t('#'));
    uint32_t urlEnd, fragStart, fragEnd;
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
    *aResult = nullptr;
    txExecutionState* es =
        static_cast<txExecutionState*>(aContext->getPrivateContext());

    RefPtr<txNodeSet> nodeSet;
    nsresult rv = aContext->recycler()->getNodeSet(getter_AddRefs(nodeSet));
    NS_ENSURE_SUCCESS(rv, rv);

    // document(object, node-set?)
    if (!requireParams(1, 2, aContext)) {
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }

    RefPtr<txAExprResult> exprResult1;
    rv = mParams[0]->evaluate(aContext, getter_AddRefs(exprResult1));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString baseURI;
    bool baseURISet = false;

    if (mParams.Length() == 2) {
        // We have 2 arguments, get baseURI from the first node
        // in the resulting nodeset
        RefPtr<txNodeSet> nodeSet2;
        rv = evaluateToNodeSet(mParams[1],
                               aContext, getter_AddRefs(nodeSet2));
        NS_ENSURE_SUCCESS(rv, rv);

        // Make this true, even if nodeSet2 is empty. For relative URLs,
        // we'll fail to load the document with an empty base URI, and for
        // absolute URLs, the base URI doesn't matter
        baseURISet = true;

        if (!nodeSet2->isEmpty()) {
            rv = txXPathNodeUtils::getBaseURI(nodeSet2->get(0), baseURI);
            NS_ENSURE_SUCCESS(rv, rv);
        }
    }

    if (exprResult1->getResultType() == txAExprResult::NODESET) {
        // The first argument is a NodeSet, iterate on its nodes
        txNodeSet* nodeSet1 = static_cast<txNodeSet*>
                                         (static_cast<txAExprResult*>
                                                     (exprResult1));
        int32_t i;
        for (i = 0; i < nodeSet1->size(); ++i) {
            const txXPathNode& node = nodeSet1->get(i);
            nsAutoString uriStr;
            txXPathNodeUtils::appendNodeValue(node, uriStr);
            if (!baseURISet) {
                // if the second argument wasn't specified, use
                // the baseUri of node itself
                rv = txXPathNodeUtils::getBaseURI(node, baseURI);
                NS_ENSURE_SUCCESS(rv, rv);
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

bool
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
