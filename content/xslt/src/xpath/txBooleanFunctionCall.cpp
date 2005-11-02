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
 * $Id: txBooleanFunctionCall.cpp,v 1.1 2005/11/02 07:33:34 kvisco%ziplink.net Exp $
 */

#include "FunctionLib.h"

/**
 * Creates a default BooleanFunctionCall, which always evaluates to False
 * @author <A HREF="mailto:kvisco@ziplink.net">Keith Visco</A>
 * @version $Revision: 1.1 $ $Date: 2005/11/02 07:33:34 $
**/
BooleanFunctionCall::BooleanFunctionCall() : FunctionCall(XPathNames::FALSE_FN) {
    this->type = FALSE;
} //-- BooleanFunctionCall

/**
 * Creates a BooleanFunctionCall of the given type
**/
BooleanFunctionCall::BooleanFunctionCall(short type) : FunctionCall()
{
    switch ( type ) {
        case BOOLEAN :
            FunctionCall::setName(XPathNames::BOOLEAN_FN);
            break;
        case NOT :
            FunctionCall::setName(XPathNames::NOT_FN);
            break;
        case TRUE :
            FunctionCall::setName(XPathNames::TRUE_FN);
            break;
        default:
            FunctionCall::setName(XPathNames::FALSE_FN);
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

    BooleanResult* result = new BooleanResult();
    ListIterator* iter = params.iterator();
    int argc = params.getLength();
    Expr* param = 0;
    String err;


    switch ( type ) {
        case BOOLEAN :
            if ( requireParams(1,1,cs) ) {
                param = (Expr*)iter->next();
                ExprResult* exprResult = param->evaluate(context, cs);
                result->setValue(exprResult->booleanValue());
                delete exprResult;
            }
            break;
        case NOT :
            if ( requireParams(1,1,cs) ) {
                param = (Expr*)iter->next();
                ExprResult* exprResult = param->evaluate(context, cs);
                result->setValue(!exprResult->booleanValue());
                delete exprResult;
            }
            break;
        case TRUE :
            result->setValue(MB_TRUE);
            break;
        default:
            result->setValue(MB_FALSE);
            break;
    }
    delete iter;
    return result;
} //-- evaluate

