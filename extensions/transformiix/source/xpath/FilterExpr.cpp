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
 * Bob Miller, kbob@oblix.com
 *    -- plugged core leak.
 * 
 */

#include "Expr.h"
#include "NodeSet.h"
#include "txIXPathContext.h"

//-- Implementation of FilterExpr --/

/**
 * Creates a new FilterExpr using the given Expr
 * @param expr the Expr to use for evaluation
**/
FilterExpr::FilterExpr(Expr* expr) : PredicateList() {
    this->expr = expr;
} //-- FilterExpr

/**
 * Destroys this FilterExpr, all predicates and the expr will be deleted
**/
FilterExpr::~FilterExpr() {
    delete expr;
} //-- ~FilterExpr

  //-----------------------------/
 //- Virtual methods from Expr -/
//-----------------------------/

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ProcessorState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
 * @see Expr
**/
ExprResult* FilterExpr::evaluate(txIEvalContext* aContext)
{
    if (!aContext || !expr)
        return new NodeSet;

    ExprResult* exprResult = expr->evaluate(aContext);
    if (!exprResult)
        return 0;
    
    if (exprResult->getResultType() == ExprResult::NODESET) {
        // Result is a nodeset, filter it.
        evaluatePredicates((NodeSet*)exprResult, aContext);
    }
    else if(!isEmpty()) {
        // We can't filter a non-nodeset
        nsAutoString err(NS_LITERAL_STRING("Expecting nodeset as result of: "));
        expr->toString(err);
        aContext->receiveError(err, NS_ERROR_XSLT_NODESET_EXPECTED);
        delete exprResult;
        return new NodeSet;
    }

    return exprResult;
} //-- evaluate

/**
 * Creates a String representation of this Expr
 * @param str the destination String to append to
 * @see Expr
**/
void FilterExpr::toString(nsAString& str) {
    if ( expr ) expr->toString(str);
    else str.Append(NS_LITERAL_STRING("null"));
    PredicateList::toString(str);
} //-- toString

