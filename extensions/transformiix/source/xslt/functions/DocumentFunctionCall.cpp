/*
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

/**
 * DocumentFunctionCall
 * A representation of the XSLT additional function: document()
 */


#include "XSLTFunctions.h"
#include "XMLParser.h"
#include "XMLDOMUtils.h"

/**
 * Creates a new DocumentFunctionCall.
**/
DocumentFunctionCall::DocumentFunctionCall(Document* xslDocument) : FunctionCall(DOCUMENT_FN)
{
    this->xslDocument = xslDocument;
} //-- DocumentFunctionCall


/**
 * Evaluates this Expr based on the given context node and processor state
 * NOTE: the implementation is incomplete since it does not make use of the
 * second argument (base URI)
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* DocumentFunctionCall::evaluate(Node* context, ContextState* cs) {
    NodeSet* nodeSet = new NodeSet();

    //-- document( object, node-set? )
    if ( requireParams(1, 2, cs) ) {
        ListIterator* iter = params.iterator();
        Expr* param1 = (Expr*) iter->next();
        ExprResult* exprResult1 = param1->evaluate(context, cs);
        NodeSet*    nodeSet2 = 0;

        if (iter->hasNext()) {
            // we have 2 arguments, make sure the second is a NodeSet
            Expr* param2 = (Expr*) iter->next();
            ExprResult* exprResult2 = param2->evaluate(context, cs);
            if ( exprResult2->getResultType() != ExprResult::NODESET ) {
                String err("node-set expected as second argument to document()");
                cs->recieveError(err);
                delete exprResult2;
            }
            else {
                nodeSet2 = (NodeSet*) exprResult2;
            }
        }

        if ( exprResult1->getResultType() == ExprResult::NODESET ) {
            // The first argument is a NodeSet, iterate on its nodes
            NodeSet* nodeSet1 = (NodeSet*) exprResult1;
            for (int i=0; i<nodeSet1->size(); i++) {
                Node* node = nodeSet1->get(i);
                String uriStr;
                XMLDOMUtils::getNodeValue(node, &uriStr);
                if (nodeSet2) {
                    // if the second argument was specified, use it
                    String baseUriStr;
                    // TODO: retrieve the base URI of the first node of nodeSet2 in document order
                    retrieveDocument(uriStr, baseUriStr, *nodeSet, cs);
                }
                else {
                    // otherwise, use the base URI of the node itself
                    String baseUriStr;
                    // TODO: retrieve the base URI of node
                    retrieveDocument(uriStr, baseUriStr, *nodeSet, cs);
                }
            }
        }

        else {
            // The first argument is not a NodeSet
            String uriStr;
            evaluateToString(param1, context, cs, uriStr);
            String baseUriStr;
            // TODO: retrieve the base URI of the first node of nodeSet2 in document order
            retrieveDocument(uriStr, baseUriStr, *nodeSet, cs);
        }
        delete exprResult1;
        delete nodeSet2;
        delete iter;
    }

    return nodeSet;
} //-- evaluate


/**
 * Retrieve the document designated by the URI uri, using baseUri as base URI if
 * necessary, parses it as an XML document, and append the resulting document node
 * to resultNodeSet.
 * NOTES: fragment identifiers in the URI are not supported
 *
 * @param uri the URI of the document to retrieve
 * @param baseUri the base URI used to resolve the URI if uri is relative
 * @param resultNodeSet the NodeSet to append the document to
 * @param cs the ContextState, used for reporting errors
 */
void DocumentFunctionCall::retrieveDocument(String& uri, String& baseUri, NodeSet& resultNodeSet, ContextState* cs)
{
    if (uri.length() == 0) {
        // if uri is the empty String, the document is the stylesheet itself
        resultNodeSet.add(xslDocument);
        return;
    }

    // open URI
    String errMsg("error: ");
    XMLParser xmlParser;
    // XXX (pvdb) Warning, where does this document get destroyed?
    Document* xmlDoc = xmlParser.getDocumentFromURI(uri, baseUri, errMsg);
    if (!xmlDoc) {
        String err("in document() function: ");
        err.append(errMsg);
        cs->recieveError(err);
        return;
    }

    // append the to resultNodeSet
    resultNodeSet.add(xmlDoc);
}


