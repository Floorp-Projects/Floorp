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
 //- PathExpr -/
//-----------/


/**
 * Creates a new PathExpr
**/
PathExpr::PathExpr() {
    //-- do nothing
}

/**
 * Destructor, will delete all Pattern Expressions
**/
PathExpr::~PathExpr() {
    ListIterator* iter = expressions.iterator();
    while ( iter->hasNext() ) {
         iter->next();
         PathExprItem* pxi = (PathExprItem*)iter->remove();
         delete pxi->pExpr;
         delete pxi;
    }
    delete iter;
} //-- ~PathExpr

/**
 * Adds the PatternExpr to this PathExpr
 * @param expr the Expr to add to this PathExpr
 * @param index the index at which to add the given Expr
**/
void PathExpr::addPatternExpr(int index, PatternExpr* expr, short ancestryOp) {
    if (expr) {
        PathExprItem* pxi = new PathExprItem;
        pxi->pExpr = expr;
        pxi->ancestryOp = ancestryOp;
        expressions.insert(index, pxi);
    }
} //-- addPattenExpr

/**
 * Adds the PatternExpr to this PathExpr
 * @param expr the Expr to add to this PathExpr
**/
void PathExpr::addPatternExpr(PatternExpr* expr, short ancestryOp) {
    if (expr) {
        PathExprItem* pxi = new PathExprItem;
        pxi->pExpr = expr;
        pxi->ancestryOp = ancestryOp;
        expressions.add(pxi);
    }
} //-- addPattenExpr

MBool PathExpr::isAbsolute() {
    if ( expressions.getLength() > 0 ) {
        ListIterator* iter = expressions.iterator();
        PathExprItem* pxi = (PathExprItem*)iter->next();
        delete iter;
        return (pxi->ancestryOp != RELATIVE_OP);
    }
    return MB_FALSE;
} //-- isAbsolute

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
ExprResult* PathExpr::evaluate(Node* context, ContextState* cs) {
    //-- add selectExpr functionality here

    if ( (!context)  || (expressions.getLength() == 0))
            return new NodeSet(0);

    NodeSet* nodes = new NodeSet();

    if ((isAbsolute()) && (context->getNodeType() != Node::DOCUMENT_NODE))
        nodes->add(context->getOwnerDocument());
    else
        nodes->add(context);


    ListIterator* iter = expressions.iterator();

    MBool ancestorMode = MB_FALSE;
    while ( iter->hasNext() ) {

        PathExprItem* pxi = (PathExprItem*)iter->next();
        ancestorMode = (ancestorMode || (pxi->ancestryOp == ANCESTOR_OP));
        NodeSet* tmpNodes = 0;
        cs->getNodeSetStack()->push(nodes);
        for (int i = 0; i < nodes->size(); i++) {
            Node* node = nodes->get(i);
            NodeSet* xNodes = (NodeSet*) pxi->pExpr->evaluate(node, cs);
            if ( tmpNodes ) {
                xNodes->copyInto(*tmpNodes);
            }
            else {
                tmpNodes = xNodes;
                xNodes = 0;
            }
            delete xNodes;
            //-- handle ancestorMode
            if ( ancestorMode ) fromDescendants(pxi->pExpr, node, cs, tmpNodes);
        }
        delete (NodeSet*) cs->getNodeSetStack()->pop();
        nodes = tmpNodes;
        if ( nodes->size() == 0 ) break;
    }
    delete iter;
    return nodes;
} //-- evaluate

/**
 * Selects from the descendants of the context node
 * all nodes that match the PatternExpr
 * -- this will be moving to a Utility class
**/
void PathExpr::fromDescendants
    (PatternExpr* pExpr, Node* context, ContextState* cs, NodeSet* nodes)
{

    if (( !context ) || (! pExpr )) return;

    NodeList* nl = context->getChildNodes();
    for (int i = 0; i < nl->getLength(); i++) {
        Node* child = nl->item(i);
        if (pExpr->matches(child, context, cs))
            nodes->add(child);
        //-- check childs descendants
        if (child->hasChildNodes())
            fromDescendants(pExpr, child, cs, nodes);
    }
} //-- fromDescendants

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
 * If this pattern does not match the given Node under the current context Node and
 * ContextState then Negative Infinity is returned.
**/
double PathExpr::getDefaultPriority(Node* node, Node* context, ContextState* cs) {

    if ( matches(node, context, cs) ) {
        int size = expressions.getLength();
        if ( size == 1) {
            ListIterator* iter = expressions.iterator();
            PathExprItem* pxi = (PathExprItem*)iter->next();
            delete iter;
            return pxi->pExpr->getDefaultPriority(node, context, cs);
        }
        else if ( size > 1 ) {
            return 0.5;
        }
    }
    return Double::NEGATIVE_INFINITY;
} //-- getDefaultPriority

/**
 * Determines whether this PatternExpr matches the given node within
 * the given context
**/
MBool PathExpr::matches(Node* node, Node* context, ContextState* cs) {
    NodeSet* nodeSet = (NodeSet*) evaluate(context, cs);
    MBool result = nodeSet->contains(node);
    delete nodeSet;
    return result;
} //-- matches


/**
 * Returns the String representation of this PatternExpr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 *  any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this PatternExpr.
**/
void PathExpr::toString(String& dest) {
    ListIterator* iter = expressions.iterator();
    while ( iter->hasNext() ) {
        //-- set operator
        PathExprItem* pxi = (PathExprItem*)iter->next();
        switch ( pxi->ancestryOp ) {
            case ANCESTOR_OP:
                dest.append("//");
                break;
            case PARENT_OP:
                dest.append('/');
                break;
            default:
                break;
        }
        pxi->pExpr->toString(dest);
    }
    delete iter;
} //-- toString

