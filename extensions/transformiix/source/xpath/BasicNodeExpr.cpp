/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
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
 */

#include "Expr.h"

//- Constructors -/

/**
 * Creates a new BasicNodeExpr of the given type
**/
BasicNodeExpr::BasicNodeExpr(NodeExpr::NodeExprType nodeExprType) {
    this->type = nodeExprType;
    nodeNameSet = MB_FALSE;
} //-- BasicNodeExpr

  //------------------/
 //- Public Methods -/
//------------------/

void BasicNodeExpr::setNodeName(const String& name) {
    this->nodeName = name;
    nodeNameSet = MB_TRUE;
}

  //-----------------------------/
 //- Methods from NodeExpr.cpp -/
//-----------------------------/

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* BasicNodeExpr::evaluate(Node* context, ContextState* cs) {
    NS_ASSERTION(0, "BasicNodeExpr::evaluate called");
    return 0;
} //-- evaluate

/**
 * Returns the default priority of this Expr based on the given Node,
 * context Node, and ContextState.
**/
double BasicNodeExpr::getDefaultPriority(Node* node, Node* context,
                                         ContextState* cs) {
    if (nodeNameSet)
        return 0;
    return -0.5;
} //-- getDefaultPriority

/**
 * Determines whether this NodeExpr matches the given node within
 * the given context
**/
MBool BasicNodeExpr::matches(Node* node, Node* context, ContextState* cs) {
    if (!node)
        return MB_FALSE;
    switch (type) {
        case NodeExpr::COMMENT_EXPR:
            return (MBool)(node->getNodeType() == Node::COMMENT_NODE);
        case NodeExpr::PI_EXPR :
            return (MBool)(node->getNodeType() == Node::PROCESSING_INSTRUCTION_NODE &&
                            !nodeNameSet || nodeName.isEqual(node->getNodeName()));
        default: //-- node()
            if (node->getNodeType() == Node::TEXT_NODE ||
                node->getNodeType() == Node::CDATA_SECTION_NODE)
                return !cs->isStripSpaceAllowed(node);
            return MB_TRUE;
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
    switch (type) {
        case NodeExpr::COMMENT_EXPR:
            dest.append("comment()");
            break;
        case NodeExpr::PI_EXPR :
            dest.append("processing-instruction(");
            if (nodeNameSet) {
                dest.append('\'');
                dest.append(nodeName);
                dest.append('\'');
            }
            dest.append(')');
            break;
        default: //-- node()
            dest.append("node()");
            break;
    }
} //-- toString

