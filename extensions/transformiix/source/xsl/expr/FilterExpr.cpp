/*
 * (C) Copyright The MITRE Corporation 1999  All rights reserved.
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The program provided "as is" without any warranty express or
 * implied, including the warranty of non-infringement and the implied
 * warranties of merchantibility and fitness for a particular purpose.
 * The Copyright owner will not be liable for any damages suffered by
 * you as a result of using the Program. In no event will the Copyright
 * owner be liable for any special, indirect or consequential damages or
 * lost profits even if the Copyright owner has been advised of the
 * possibility of their occurrence.
 *
 * Please see release.txt distributed with this file for more information.
 *
 */

#include "Expr.h"


/**
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
//-- Implementation of FilterExpr --/


FilterExpr::FilterExpr() : PredicateList() {
    expr = 0;
}

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

/**
 * Sets the Expr of this FilterExpr for use during evaluation
 * @param expr the Expr to use for evaluation
**/
void FilterExpr::setExpr(Expr* expr) {
    this->expr = expr;
} //-- setExpr


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

   NodeSet* nodes = new NodeSet();

   if (( !context ) || (! expr )) return nodes;


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

