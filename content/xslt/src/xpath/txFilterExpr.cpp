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
 * $Id: txFilterExpr.cpp,v 1.2 2005/11/02 07:33:51 sicking%bigfoot.com Exp $
 */

#include "Expr.h"


/**
 * @author <a href="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.2 $ $Date: 2005/11/02 07:33:51 $
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

  //------------------------------------/
 //- Virtual methods from PatternExpr -/
//------------------------------------/

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ProcessorState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
 * @see PatternExpr
**/
ExprResult* FilterExpr::evaluate(Node* context, ContextState* cs) {

   if (( !context ) || (! expr )) return new NodeSet;

    ExprResult* exprResult = expr->evaluate(context, cs);
    NodeSet* nodeSet = 0;

    switch (exprResult->getResultType()) {
        case ExprResult::NODESET:
            nodeSet = (NodeSet*)exprResult;
            break;
        /*
        case ExprResult.TREE_FRAGMENT:
            nodeSet = new NodeSet(1);
            nodeSet.add(((TreeFragmentResult)exprResult).getValue());
            break;
        */
        default:
            break;
            /*
            throw new InvalidExprException
                ("expecting NodeSet or TreeFragment as the result of the "+
                    "expression: " + primaryExpr);
            */
    }

    //-- filter nodes (apply predicates)
    evaluatePredicates(nodeSet, cs);

    return nodeSet;
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
 * Determines whether this PatternExpr matches the given node within
 * the given context
**/
MBool FilterExpr::matches(Node* node, Node* context, ContextState* cs) {

    if ( !expr ) return MB_FALSE;
    NodeSet* nodes = (NodeSet*)evaluate(node, cs);
    MBool result = (nodes->contains(node));
    delete nodes;
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

