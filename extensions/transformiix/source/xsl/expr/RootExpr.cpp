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

MBool RootExpr::isAbsolute() {
    return MB_TRUE;
} //-- isAbsolute

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* RootExpr::evaluate(Node* context, ContextState* cs) {
    NodeSet* nodeSet = new NodeSet();
    if ( !context ) return nodeSet;
    nodeSet->add(context->getOwnerDocument());
    return nodeSet;
} //-- evaluate

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
 * If this pattern does not match the given Node under the current context Node and
 * ContextState then Negative Infinity is returned.
**/
double RootExpr::getDefaultPriority(Node* node, Node* context, ContextState* cs) {
    if ( matches(node, context, cs) ) {
        return 0.5;
    }
    else return Double::NEGATIVE_INFINITY;
} //-- getDefaultPriority

/**
 * Determines whether this NodeExpr matches the given node within
 * the given context
**/
MBool RootExpr::matches(Node* node, Node* context, ContextState* cs) {
    if ( node ) {
        return (MBool) (node->getNodeType() == Node::DOCUMENT_NODE);
    }
    return MB_FALSE;
} //-- matches

/**
 * Returns the String representation of this PatternExpr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this PatternExpr.
**/
void RootExpr::toString(String& dest) {
    dest.append('/');
} //-- toString

