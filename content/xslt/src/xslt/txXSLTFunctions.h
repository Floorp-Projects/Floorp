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
 * $Id: txXSLTFunctions.h,v 1.1 2005/11/02 07:33:54 kvisco%ziplink.net Exp $
 */

#include "dom.h"
#include "Expr.h"
#include "ExprResult.h"
#include "Names.h"
#include "DOMHelper.h"
#include "TxString.h"

#ifndef TRANSFRMX_XSLT_FUNCTIONS_H
#define TRANSFRMX_XSLT_FUNCTIONS_H

class GenerateIdFunctionCall : public FunctionCall {

public:

    /**
     * Creates a new generate-id function call
    **/
    GenerateIdFunctionCall(DOMHelper* domHelper);

    /**
     * Evaluates this Expr based on the given context node and processor state
     * @param context the context node for evaluation of this Expr
     * @param ps the ContextState containing the stack information needed
     * for evaluation
     * @return the result of the evaluation
     * @see FunctionCall.h
    **/
    virtual ExprResult* evaluate(Node* context, ContextState* cs);

private:
    DOMHelper* domHelper;
};

 #endif