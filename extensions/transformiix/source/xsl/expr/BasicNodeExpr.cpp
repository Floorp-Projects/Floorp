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
 * This file was ported from XSL:P
**/

//- Constructors -/

/**
 * Creates a new BasicNodeExpr, which matchs any Node
**/
BasicNodeExpr::BasicNodeExpr() {
    this->type = NodeExpr::NODE_EXPR;
} //-- BasicNodeExpr

/**
 * Creates a new BasicNodeExpr of the given type
**/
BasicNodeExpr::BasicNodeExpr(NodeExpr::NodeExprType nodeExprType) {
    this->type = nodeExprType;
} //-- BasicNodeExpr

/**
 * Destroys this NodeExpr
**/
BasicNodeExpr::~BasicNodeExpr() {};

  //------------------/
 //- Public Methods -/
//------------------/

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* BasicNodeExpr::evaluate(Node* context, ContextState* cs) {
    NodeSet* nodeSet = new NodeSet();
    if ( !context ) return nodeSet;
    NodeList* nl = context->getChildNodes();
    for (int i = 0; i < nl->getLength(); i++ ) {
        Node* node = nl->item(i);
        if (matches(node, context, cs)) nodeSet->add(node);
    }
    return nodeSet;
} //-- evaluate

  //-----------------------------/
 //- Methods from NodeExpr.cpp -/
//-----------------------------/

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
 * If this pattern does not match the given Node under the current context Node and
 * ContextState then Negative Infinity is returned.
**/
double BasicNodeExpr::getDefaultPriority(Node* node, Node* context, ContextState* cs) {
    return -0.5;
} //-- getDefaultPriority

/**
 * Returns the type of this NodeExpr
 * @return the type of this NodeExpr
**/
short BasicNodeExpr::getType() {
    return type;
} //-- getType

/**
 * Determines whether this NodeExpr matches the given node within
 * the given context
**/
MBool BasicNodeExpr::matches(Node* node, Node* context, ContextState* cs) {
    if ( !node ) return MB_FALSE;
    switch ( type ) {
        case NodeExpr::COMMENT_EXPR:
            return (MBool) (node->getNodeType() == Node::COMMENT_NODE);
        case NodeExpr::PI_EXPR :
            return (MBool) (node->getNodeType() == Node::PROCESSING_INSTRUCTION_NODE);
        default: //-- node()
            break;
    }
    return MB_TRUE;

} //-- matches


/**
 * Returns the String representation of this NodeExpr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this NodeExpr.
**/
void BasicNodeExpr::toString(String& dest) {
    switch ( type ) {
        case NodeExpr::COMMENT_EXPR:
            dest.append("comment()");
            break;
        case NodeExpr::PI_EXPR :
            dest.append("processing-instruction()");
            break;
        default: //-- node()
            dest.append("node()");
            break;
    }
} //-- toString

