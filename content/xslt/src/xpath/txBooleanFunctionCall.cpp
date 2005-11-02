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
 * Portions created by Keith Visco as a Non MITRE employee,
 * (C) 1999 Keith Visco. All Rights Reserved.
 *
 * Contributor(s):
 * Keith Visco, kvisco@ziplink.net
 *   -- original author.
 *
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *   -- added lang() implementation
 *
 */

#include "FunctionLib.h"
#include "Names.h"

/**
 * Creates a default BooleanFunctionCall, which always evaluates to False
**/
BooleanFunctionCall::BooleanFunctionCall(short type) : FunctionCall()
{
    switch ( type ) {
        case TX_BOOLEAN :
            name = XPathNames::BOOLEAN_FN;
            break;
        case TX_LANG:
            name = XPathNames::LANG_FN;
            break;
        case TX_NOT :
            name = XPathNames::NOT_FN;
            break;
        case TX_TRUE :
            name = XPathNames::TRUE_FN;
            break;
        default:
            name = XPathNames::FALSE_FN;
            break;
    }
    this->type = type;
} //-- BooleanFunctionCall

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* BooleanFunctionCall::evaluate(Node* context, ContextState* cs) {

    MBool result = MB_FALSE;
    ListIterator* iter = params.iterator();
    Expr* param = 0;
    String err;


    switch ( type ) {
        case TX_BOOLEAN :
            if ( requireParams(1,1,cs) ) {
                param = (Expr*)iter->next();
                ExprResult* exprResult = param->evaluate(context, cs);
                result = exprResult->booleanValue();
                delete exprResult;
            }
            break;
        case TX_LANG:
            if ( requireParams(1,1,cs) ) {
                String arg1, lang;
                evaluateToString((Expr*)iter->next(),context, cs, arg1);
                lang = ((Element*)context)->getAttribute(XML_LANG_ATTR);
                arg1.toUpperCase(); // case-insensitive comparison
                lang.toUpperCase();
                result = (MBool)(lang.indexOf(arg1) == 0);
            }
            break;
        case TX_NOT :
            if ( requireParams(1,1,cs) ) {
                param = (Expr*)iter->next();
                ExprResult* exprResult = param->evaluate(context, cs);
                result = (MBool)(!exprResult->booleanValue());
                delete exprResult;
            }
            break;
        case TX_TRUE :
            result = MB_TRUE;
            break;
        default:
            break;
    }
    delete iter;
    return new BooleanResult(result);
} //-- evaluate

