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
#include "URIUtils.h"
#include "Names.h"

/**
 * Creates a new DocumentFunctionCall.
**/
DocumentFunctionCall::DocumentFunctionCall(ProcessorState* ps, Document* xslDocument) : FunctionCall(DOCUMENT_FN)
{
    this->processorState = ps;
    this->xslDocument = xslDocument; //this is just untill we can get the actuall xsl tag
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
        String baseURI;
        MBool baseURISet = MB_FALSE;

        if (iter->hasNext()) {
            // we have 2 arguments, get baseURI from the first node
            // in the resulting nodeset
            Expr* param2 = (Expr*) iter->next();
            ExprResult* exprResult2 = param2->evaluate(context, cs);
            if ( exprResult2->getResultType() != ExprResult::NODESET ) {
                String err("node-set expected as second argument to document(): ");
                toString(err);
                cs->recieveError(err);
                delete exprResult2;
                return nodeSet;
            }

            // Make this true, even if nodeSet2 is empty. For relative URLs,
            // we'll fail to load the document with an empty base URI, and for
            // absolute URLs, the base URI doesn't matter.
            baseURISet = MB_TRUE;

            NodeSet* nodeSet2 = (NodeSet*) exprResult2;
            if (!nodeSet2->isEmpty()) {
                processorState->sortByDocumentOrder(nodeSet2);
                baseURI = nodeSet2->get(0)->getBaseURI();
            }
            delete exprResult2;
        }

        if ( exprResult1->getResultType() == ExprResult::NODESET ) {
            // The first argument is a NodeSet, iterate on its nodes
            NodeSet* nodeSet1 = (NodeSet*) exprResult1;
            for (int i=0; i<nodeSet1->size(); i++) {
                Node* node = nodeSet1->get(i);
                String uriStr;
                XMLDOMUtils::getNodeValue(node, &uriStr);
                if (!baseURISet) {
                    // if the second argument wasn't specified, use
                    // the baseUri of node itself
                    retrieveDocument(uriStr, node->getBaseURI(), *nodeSet, cs);
                }
                else
                    retrieveDocument(uriStr, baseURI, *nodeSet, cs);
            }
        }

        else {
            // The first argument is not a NodeSet
            String uriStr;
            evaluateToString(param1, context, cs, uriStr);
            if (!baseURISet) {
                // XXX TODO: find the current xsl tag and get its base URI
                // until then we use the xslDocument
                retrieveDocument(uriStr, xslDocument->getBaseURI(), *nodeSet, cs);
            }
            else
                retrieveDocument(uriStr, baseURI, *nodeSet, cs);
        }
        delete exprResult1;
        delete iter;
    }

    return nodeSet;
} //-- evaluate


/**
 * Retrieve the document designated by the URI uri, using baseUri as base URI if
 * necessary, parses it as an XML document, and append the resulting document node
 * to resultNodeSet.
 *
 * @param uri the URI of the document to retrieve
 * @param baseUri the base URI used to resolve the URI if uri is relative
 * @param resultNodeSet the NodeSet to append the document to
 * @param cs the ContextState, used for reporting errors
 */
void DocumentFunctionCall::retrieveDocument(const String& uri, const String& baseUri, NodeSet& resultNodeSet, ContextState* cs)
{
    String absUrl, frag;
    URIUtils::resolveHref(uri, baseUri, absUrl);
    URIUtils::getFragmentIdentifier(absUrl, frag);
    
    // try to get already loaded document
    Document* xmlDoc = processorState->getLoadedDocument(absUrl);
    
    if(!xmlDoc) {
        // open URI
        String errMsg;
        XMLParser xmlParser;
        xmlDoc = xmlParser.getDocumentFromURI(uri, baseUri, errMsg);
        if (!xmlDoc) {
            String err("error in document() function: ");
            err.append(errMsg);
            cs->recieveError(err);
            return;
        }
        // add to ProcessorState list of documents
        processorState->addLoadedDocument(xmlDoc, absUrl);
    }


    // append the to resultNodeSet
    if(frag.length()) {
        Node* node = xmlDoc->getElementById(frag);
        if(node)
            resultNodeSet.add(node);
    }
    else
        resultNodeSet.add(xmlDoc);
}


