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

  //-------------/
 //- UnionExpr -/
//-------------/


/**
 * Creates a new UnionExpr
**/
UnionExpr::UnionExpr() {
    //-- do nothing
}

/**
 * Destructor, will delete all Path Expressions
**/
UnionExpr::~UnionExpr() {
    ListIterator* iter = expressions.iterator();
    while ( iter->hasNext() ) {
         iter->next();
         delete  (PathExpr*)iter->remove();
    }
    delete iter;
} //-- ~UnionExpr

/**
 * Adds the PathExpr to this UnionExpr
 * @param pathExpr the PathExpr to add to this UnionExpr
**/
void UnionExpr::addPathExpr(PathExpr* pathExpr) {
    if (pathExpr) expressions.add(pathExpr);
} //-- addPathExpr

/**
 * Adds the PathExpr to this UnionExpr
 * @param pathExpr the PathExpr to add to this UnionExpr
**/
void UnionExpr::addPathExpr(int index, PathExpr* pathExpr) {
    if (pathExpr) expressions.insert(index, pathExpr);
} //-- addPathExpr

    //------------------------------------/
  //- Virtual methods from PatternExpr -/
//------------------------------------/

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* UnionExpr::evaluate(Node* context, ContextState* cs) {

    if ( (!context)  || (expressions.getLength() == 0))
            return new NodeSet(0);

    NodeSet* nodes = new NodeSet();

    ListIterator* iter = expressions.iterator();

    while ( iter->hasNext() ) {

        PathExpr* pExpr = (PathExpr*)iter->next();
        NodeSet* tmpNodes = (NodeSet*)pExpr->evaluate(context, cs);
        for (int j = 0; j < tmpNodes->size(); j++) {
            nodes->add(tmpNodes->get(j));
        }
        delete tmpNodes;
    }

    delete iter;
    return nodes;
} //-- evaluate

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
 * If this pattern does not match the given Node under the current context Node and
 * ContextState then Negative Infinity is returned.
**/
double UnionExpr::getDefaultPriority(Node* node, Node* context, ContextState* cs) {

    //-- find highest priority
    double priority = Double::NEGATIVE_INFINITY;
    ListIterator* iter = expressions.iterator();
    while ( iter->hasNext() ) {
        PathExpr* pExpr = (PathExpr*)iter->next();
        if ( pExpr->matches(node, context, cs) ) {
            double tmpPriority = pExpr->getDefaultPriority(node, context, cs);
            priority = (tmpPriority > priority) ? tmpPriority : priority;
        }
    }
    delete iter;
    return priority;

} //-- getDefaultPriority

/**
 * Determines whether this UnionExpr matches the given node within
 * the given context
**/
MBool UnionExpr::matches(Node* node, Node* context, ContextState* cs) {

    ListIterator* iter = expressions.iterator();

    while ( iter->hasNext() ) {
        PathExpr* pExpr = (PathExpr*)iter->next();
        if ( pExpr->matches(node, context, cs) ) {
             delete iter;
             return MB_TRUE;
        }
    }
    delete iter;
    return MB_FALSE;
} //-- matches


/**
 * Returns the String representation of this PatternExpr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 *  any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this PatternExpr.
**/
void UnionExpr::toString(String& dest) {
    ListIterator* iter = expressions.iterator();

    short count = 0;
    while ( iter->hasNext() ) {
        //-- set operator
        if (count > 0) dest.append(" | ");
        ((PathExpr*)iter->next())->toString(dest);
        ++count;
    }
    delete iter;
} //-- toString

