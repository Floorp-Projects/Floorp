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
 *    
 * $Id: txLocationStep.cpp,v 1.4 2005/11/02 07:33:44 kvisco%ziplink.net Exp $
 */

/*
  Implementation of an XPath LocationStep
  @version $Revision: 1.4 $ $Date: 2005/11/02 07:33:44 $
*/

#include "Expr.h"

/**
 * Creates a new LocationStep using the default Axis Identifier and no
 * NodeExpr (which matches nothing)
**/
LocationStep::LocationStep() : PredicateList() {
    nodeExpr = 0;
    this->axisIdentifier = CHILD_AXIS;
} //-- LocationStep

/**
 * Creates a new LocationStep using the default Axis Identifier and
 * the given NodeExpr
 * @param nodeExpr the NodeExpr to use when matching Nodes
**/
LocationStep::LocationStep(NodeExpr* nodeExpr) : PredicateList() {
    this->nodeExpr = nodeExpr;
    this->axisIdentifier = CHILD_AXIS;
} //-- LocationStep

/**
 * Creates a new LocationStep using the given NodeExpr and Axis Identifier
 * @param nodeExpr the NodeExpr to use when matching Nodes
 * @param axisIdentifier the Axis Identifier in which to search for nodes
**/
LocationStep::LocationStep(NodeExpr* nodeExpr, short axisIdentifier) : PredicateList() {
    this->nodeExpr = nodeExpr;
    this->axisIdentifier = axisIdentifier;
} //-- LocationStep

/**
 * Destroys this LocationStep
 * All predicates will be deleted
 * The NodeExpr will be deleted
**/
LocationStep::~LocationStep() {
    delete nodeExpr;
} //-- ~LocationStep

/**
 * Sets the Axis Identifier for this LocationStep
 * @param axisIdentifier the Axis in which to search for nodes
**/
void LocationStep::setAxisIdentifier(short axisIdentifier) {
    this->axisIdentifier = axisIdentifier;
} //-- setAxisIdentifier


/**
 * Sets the NodeExpr of this LocationStep for use when matching nodes
 * @param nodeExpr the NodeExpr to use when matching nodes
**/
void LocationStep::setNodeExpr(NodeExpr* nodeExpr) {
    // delete current NodeExpr
    if (this->nodeExpr) delete this->nodeExpr;
    this->nodeExpr = nodeExpr;
} //-- setNodeExpr


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
ExprResult* LocationStep::evaluate(Node* context, ContextState* cs) {

   NodeSet* nodes = new NodeSet();
   if (( !context ) || (! nodeExpr )) return nodes;

    Node* node = context;
    switch (axisIdentifier) {
        case ANCESTOR_AXIS :
            if (node) node = context->getParentNode();
            //-- do not break here
        case ANCESTOR_OR_SELF_AXIS :
            while (node) {
                if (nodeExpr->matches(node, context, cs)) {
                    nodes->add(node);
                }
                node = node->getParentNode();
            }
            break;
        case ATTRIBUTE_AXIS :
        {
            NamedNodeMap* atts = context->getAttributes();
            if ( atts ) {
                for ( UInt32 i = 0; i < atts->getLength(); i++ ) {
                    Attr* attr = (Attr*)atts->item(i);
                    if ( nodeExpr->matches(attr, context, cs) ) nodes->add(attr);
                }
            }
            break;
        }
        case DESCENDANT_OR_SELF_AXIS :
            if ( nodeExpr->matches(context, context, cs))
                nodes->add(context);
            //-- do not break here
        case DESCENDANT_AXIS :
            fromDescendants(context, cs, nodes);
            break;
        case FOLLOWING_AXIS :
        {
            node = context->getNextSibling();
            while (node) {
                if (nodeExpr->matches(node, context, cs))
                    nodes->add(node);

                if (node->hasChildNodes())
                    fromDescendants(node, cs, nodes);

                Node* tmpNode = node->getNextSibling();
                if (!tmpNode) {
                    node = node->getParentNode();
                    if ((node) && (node->getNodeType() != Node::DOCUMENT_NODE))
                        node = node->getNextSibling();
                }
                else node = tmpNode;
            }
            break;
        }
        case FOLLOWING_SIBLING_AXIS :
            node = context->getNextSibling();
            while (node) {
                if (nodeExpr->matches(node, context, cs))
                    nodes->add(node);
                node = node->getNextSibling();
            }
            break;
        case NAMESPACE_AXIS : //-- not yet implemented
            cout << "namespace axis not yet implemented"<<endl;
            break;
        case PARENT_AXIS :
        {
            Node* parent = context->getParentNode();
            if ( nodeExpr->matches(parent, context, cs) )
                    nodes->add(parent);
            break;
        }
        case PRECEDING_AXIS :
            node = context->getPreviousSibling();
            if ( !node ) node = context->getParentNode();
            while (node) {
                if (nodeExpr->matches(node, context, cs))
                    nodes->add(node);

                Node* temp = node->getPreviousSibling();
                if (!temp) node = node->getParentNode();
                else node = temp;
            }
            break;
        case PRECEDING_SIBLING_AXIS:
            node = context->getPreviousSibling();
            while (node) {
                if (nodeExpr->matches(node, context, cs))
                    nodes->add(node);
                node = node->getPreviousSibling();
            }
            break;
        case SELF_AXIS :
            if ( nodeExpr->matches(context, context, cs) )
                    nodes->add(context);
            break;
        default: //-- Children Axis
        {
            NodeList* nl = context->getChildNodes();
            for ( UInt32 i = 0; i < nl->getLength(); i++  ) {
                if ( nodeExpr->matches(nl->item(i), context, cs) )
                    nodes->add(nl->item(i));
            }
            break;
        }
    } //-- switch

    //-- apply predicates
    evaluatePredicates(nodes, cs);

    return nodes;
} //-- evaluate

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
 * If this pattern does not match the given Node under the current context Node and
 * ContextState then Negative Infinity is returned.
**/
double LocationStep::getDefaultPriority(Node* node, Node* context, ContextState* cs) {

    if ( !nodeExpr ) {
        return Double::NEGATIVE_INFINITY;
    }
    if (!this->isEmpty()) {
        return 0.5;
    }
    return nodeExpr->getDefaultPriority(node, context, cs);

} //-- getDefaultPriority


void LocationStep::fromDescendants(Node* context, ContextState* cs, NodeSet* nodes) {

    if (( !context ) || (! nodeExpr )) return;

    NodeList* nl = context->getChildNodes();
    for (UInt32 i = 0; i < nl->getLength(); i++) {
        Node* child = nl->item(i);
        if (nodeExpr->matches(child, context, cs))
            nodes->add(child);
        //-- check childs descendants
        if (child->hasChildNodes()) fromDescendants(child, cs, nodes);
    }

} //-- fromDescendants

/**
 * Determines whether this PatternExpr matches the given node within
 * the given context
**/
MBool LocationStep::matches(Node* node, Node* context, ContextState* cs) {

    if ( !nodeExpr ) return MB_FALSE;

    if ( !nodeExpr->matches(node, context, cs) ) return MB_FALSE;

    NodeSet nodes;
    nodes.add(node);
    evaluatePredicates(&nodes, cs);

    return (MBool)(nodes.size() > 0);

} //-- matches

/**
 * Creates a String representation of this Expr
 * @param str the destination String to append to
 * @see Expr
**/
void LocationStep::toString(String& str) {
    switch (axisIdentifier) {
        case ANCESTOR_AXIS :
            str.append("ancestor::");
            break;
        case ANCESTOR_OR_SELF_AXIS :
            str.append("ancestor-or-self::");
            break;
        case ATTRIBUTE_AXIS:
            str.append("attribute::");
            break;
        case DESCENDANT_AXIS:
            str.append("descendant::");
            break;
        case DESCENDANT_OR_SELF_AXIS:
            str.append("descendant-or-self::");
            break;
        case FOLLOWING_AXIS :
            str.append("following::");
            break;
        case FOLLOWING_SIBLING_AXIS:
            str.append("following-sibling::");
            break;
        case NAMESPACE_AXIS:
            str.append("namespace::");
            break;
        case PARENT_AXIS :
            str.append("parent::");
            break;
        case PRECEDING_AXIS :
            str.append("preceding::");
            break;
        case PRECEDING_SIBLING_AXIS :
            str.append("preceding-sibling::");
            break;
        case SELF_AXIS :
            str.append("self::");
            break;
        default:
            break;
    }
    if ( nodeExpr ) nodeExpr->toString(str);
    else str.append("null");
    PredicateList::toString(str);
} //-- toString

