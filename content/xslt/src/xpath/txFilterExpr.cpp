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
 * $Id: txFilterExpr.cpp,v 1.3 2005/11/02 07:33:52 sicking%bigfoot.com Exp $
 */

#include "Expr.h"


/**
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.3 $ $Date: 2005/11/02 07:33:52 $
**/
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
ExprResult* FilterExpr::evaluate(Node* context, ContextState* cs) {

    if (!context || !expr)
        return new NodeSet;

    ExprResult* exprResult = expr->evaluate(context, cs);
    if (!exprResult)
        return 0;
    
    if (exprResult->getResultType() == ExprResult::NODESET) {
        // Result is a nodeset, filter it.
        cs->sortByDocumentOrder((NodeSet*)exprResult);
        evaluatePredicates((NodeSet*)exprResult, cs);
    }
    else if(!isEmpty()) {
        // We can't filter a non-nodeset
        String err("Expecting nodeset as result of: ");
        expr->toString(err);
        cs->recieveError(err);
        delete exprResult;
        return new NodeSet;
    }

    return exprResult;
} //-- evaluate

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
 * If this pattern does not match the given Node under the current context Node and
 * ContextState then Negative Infinity is returned.
**/
double FilterExpr::getDefaultPriority(Node* node, Node* context, ContextState* cs) {
    //-- this method will never be called, it's only here since
    //-- I made it manditory for PatternExprs I will remove it soon
    return Double::NEGATIVE_INFINITY;
} //-- getDefaultPriority

/**
 * Determines whether this Expr matches the given node within
 * the given context
**/
MBool FilterExpr::matches(Node* node, Node* context, ContextState* cs) {

    if (!expr)
        return MB_FALSE;
        
    ExprResult* exprResult = evaluate(node, cs);
    if (!exprResult)
        return MB_FALSE;

    MBool result = MB_FALSE;
    if(exprResult->getResultType() == ExprResult::NODESET)
        result = ((NodeSet*)exprResult)->contains(node);

    delete exprResult;
    return result;

} //-- matches

/**
 * Creates a String representation of this Expr
 * @param str the destination String to append to
 * @see Expr
**/
void FilterExpr::toString(String& str) {
    if ( expr ) expr->toString(str);
    else str.append("null");
    PredicateList::toString(str);
} //-- toString

