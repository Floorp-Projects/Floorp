/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * Portions created by Olivier Gerardin are  Copyright (C) 2000,
 * Olivier Gerardin. All rights reserved.
 *
 * Portions created by Keith Visco are  Copyright (C) 2000, Keith Visco.
 * All rights reserved.
 *
 * Contributor(s):
 * Olivier Gerardin, ogerardin@vo.lu
 *   -- original author.
 *
 * Keith Visco, kvisco@ziplink.net
 *   -- patched compilation error on due to improper usage of the
 *      3rd argument (error message) for URIUtils::getInputStream,
 *      in method DocumentFunctionCall::retrieveDocument
 */

/*
 * DocumentFunctionCall
 * A representation of the XSLT additional function: document()
 */

#include "XSLTFunctions.h"
#include "ProcessorState.h"
#include "XMLDOMUtils.h"
#include "Names.h"
#include "txIXPathContext.h"

/*
 * Creates a new DocumentFunctionCall.
 */
DocumentFunctionCall::DocumentFunctionCall(ProcessorState* aPs,
                                           Node* aDefResolveNode)
                                           : FunctionCall(DOCUMENT_FN)
{
    mProcessorState = aPs;
    mDefResolveNode = aDefResolveNode;
}

/*
 * Evaluates this Expr based on the given context node and processor state
 * NOTE: the implementation is incomplete since it does not make use of the
 * second argument (base URI)
 * @param context the context node for evaluation of this Expr
 * @return the result of the evaluation
 */
ExprResult* DocumentFunctionCall::evaluate(txIEvalContext* aContext)
{
    NodeSet* nodeSet = new NodeSet();

    // document(object, node-set?)
    if (requireParams(1, 2, aContext)) {
        txListIterator iter(&params);
        Expr* param1 = (Expr*)iter.next();
        ExprResult* exprResult1 = param1->evaluate(aContext);
        String baseURI;
        MBool baseURISet = MB_FALSE;

        if (iter.hasNext()) {
            // We have 2 arguments, get baseURI from the first node
            // in the resulting nodeset
            Expr* param2 = (Expr*)iter.next();
            ExprResult* exprResult2 = param2->evaluate(aContext);
            if (exprResult2->getResultType() != ExprResult::NODESET) {
                String err("node-set expected as second argument to document(): ");
                toString(err);
                aContext->receiveError(err, NS_ERROR_XPATH_INVALID_ARG);
                delete exprResult1;
                delete exprResult2;
                return nodeSet;
            }

            // Make this true, even if nodeSet2 is empty. For relative URLs,
            // we'll fail to load the document with an empty base URI, and for
            // absolute URLs, the base URI doesn't matter
            baseURISet = MB_TRUE;

            NodeSet* nodeSet2 = (NodeSet*) exprResult2;
            if (!nodeSet2->isEmpty()) {
                baseURI = nodeSet2->get(0)->getBaseURI();
            }
            delete exprResult2;
        }

        if (exprResult1->getResultType() == ExprResult::NODESET) {
            // The first argument is a NodeSet, iterate on its nodes
            NodeSet* nodeSet1 = (NodeSet*) exprResult1;
            int i;
            for (i = 0; i < nodeSet1->size(); i++) {
                Node* node = nodeSet1->get(i);
                String uriStr;
                XMLDOMUtils::getNodeValue(node, uriStr);
                if (!baseURISet) {
                    // if the second argument wasn't specified, use
                    // the baseUri of node itself
                    nodeSet->add(mProcessorState->retrieveDocument(uriStr, node->getBaseURI()));
                }
                else {
                    nodeSet->add(mProcessorState->retrieveDocument(uriStr, baseURI));
                }
            }
        }
        else {
            // The first argument is not a NodeSet
            String uriStr;
            exprResult1->stringValue(uriStr);
            if (!baseURISet) {
                nodeSet->add(mProcessorState->retrieveDocument(uriStr,
                    mDefResolveNode->getBaseURI()));
            }
            else {
                nodeSet->add(mProcessorState->retrieveDocument(uriStr, baseURI));
            }
        }
        delete exprResult1;
    }

    return nodeSet;
}
