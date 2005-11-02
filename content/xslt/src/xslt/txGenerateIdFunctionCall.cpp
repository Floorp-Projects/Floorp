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
 * The Original Code is XSL:P XSLT processor.
 *
 * The Initial Developer of the Original Code is Keith Visco.
 * Portions created by Keith Visco (C) 1999, 2000 Keith Visco.
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Keith Visco, kvisco@ziplink.net
 *    -- original author.
 *
 * $Id: txGenerateIdFunctionCall.cpp,v 1.3 2005/11/02 07:34:00 nisheeth%netscape.com Exp $
 */

#include "XSLTFunctions.h"

/*
  Implementation of XSLT 1.0 extension function: generate-id
*/

/**
 * Creates a new generate-id function call
**/
GenerateIdFunctionCall::GenerateIdFunctionCall(DOMHelper* domHelper) : FunctionCall(GENERATE_ID_FN)
{
    this->domHelper = domHelper;
} //-- GenerateIdFunctionCall

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
 * @see FunctionCall.h
**/
ExprResult* GenerateIdFunctionCall::evaluate(Node* context, ContextState* cs) {

    Node* node = context;

    int argc = params.getLength();

    StringResult* stringResult = 0;

    if (argc > 0) {
        ListIterator* iter = params.iterator();
        Expr* param = (Expr*) iter->next();
        delete iter;
        ExprResult* exprResult = param->evaluate(context, cs);
        if (!exprResult) return new StringResult("");
        if (exprResult->getResultType() == ExprResult::NODESET) {
            NodeSet* nodes = (NodeSet*) exprResult;
            if (nodes->size() == 0)
                stringResult = new StringResult("");
            else
                node = nodes->get(0);
        }
        else {
            String err("Invalid argument passed to generate-id(), expecting NodeSet");
            stringResult = new StringResult(err);
        }
        delete exprResult;
    }

    if (stringResult) return stringResult;

    //-- generate id for selected node
    String id;
    domHelper->generateId(node, id);
    return new StringResult(id);

} //-- evaluate

