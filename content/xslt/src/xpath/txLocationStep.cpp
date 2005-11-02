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

/*
  Implementation of an XPath LocationStep
*/

#include "Expr.h"
#include "NodeSet.h"
#include "txIXPathContext.h"

/**
 * Creates a new LocationStep using the given NodeExpr and Axis Identifier
 * @param nodeExpr the NodeExpr to use when matching Nodes
 * @param axisIdentifier the Axis Identifier in which to search for nodes
**/
LocationStep::LocationStep(txNodeTest* aNodeTest,
                           LocationStepType aAxisIdentifier)
    : mNodeTest(aNodeTest), mAxisIdentifier(aAxisIdentifier)
{
} //-- LocationStep

/**
 * Destroys this LocationStep
 * All predicates will be deleted
 * The NodeExpr will be deleted
**/
LocationStep::~LocationStep() {
    delete mNodeTest;
} //-- ~LocationStep

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
ExprResult* LocationStep::evaluate(txIEvalContext* aContext)
{
    NS_ASSERTION(aContext, "internal error");

    NodeSet* nodes = new NodeSet();
    if (!nodes)
        return 0;

    MBool reverse = MB_FALSE;

    Node* node = aContext->getContextNode();
    switch (mAxisIdentifier) {
        case ANCESTOR_AXIS :
            node = node->getXPathParent();
            //-- do not break here
        case ANCESTOR_OR_SELF_AXIS :
            reverse = MB_TRUE;
            while (node) {
                if (mNodeTest->matches(node, aContext)) {
                    nodes->append(node);
                }
                node = node->getXPathParent();
            }
            break;
        case ATTRIBUTE_AXIS :
        {
            NamedNodeMap* atts = node->getAttributes();
            if (atts) {
                for (PRUint32 i = 0; i < atts->getLength(); i++) {
                    Node* attr = atts->item(i);
                    if (attr->getNamespaceID() != kNameSpaceID_XMLNS &&
                        mNodeTest->matches(attr, aContext))
                        nodes->append(attr);
                }
            }
            break;
        }
        case DESCENDANT_OR_SELF_AXIS :
            if (mNodeTest->matches(node, aContext))
                nodes->append(node);
            //-- do not break here
        case DESCENDANT_AXIS :
            fromDescendants(node, aContext, nodes);
            break;
        case FOLLOWING_AXIS :
        {
            if ( node->getNodeType() == Node::ATTRIBUTE_NODE) {
                node = node->getXPathParent();
                fromDescendants(node, aContext, nodes);
            }
            while (node && !node->getNextSibling()) {
                node = node->getXPathParent();
            }
            while (node) {
                node = node->getNextSibling();

                if (mNodeTest->matches(node, aContext))
                    nodes->append(node);

                if (node->hasChildNodes())
                    fromDescendants(node, aContext, nodes);

                while (node && !node->getNextSibling()) {
                    node = node->getParentNode();
                }
            }
            break;
        }
        case FOLLOWING_SIBLING_AXIS :
            node = node->getNextSibling();
            while (node) {
                if (mNodeTest->matches(node, aContext))
                    nodes->append(node);
                node = node->getNextSibling();
            }
            break;
        case NAMESPACE_AXIS : //-- not yet implemented
#if 0
            // XXX DEBUG OUTPUT
            cout << "namespace axis not yet implemented"<<endl;
#endif
            break;
        case PARENT_AXIS :
        {
            Node* parent = node->getXPathParent();
            if (mNodeTest->matches(parent, aContext))
                    nodes->append(parent);
            break;
        }
        case PRECEDING_AXIS :
            reverse = MB_TRUE;
            while (node && !node->getPreviousSibling()) {
                node = node->getXPathParent();
            }
            while (node) {
                node = node->getPreviousSibling();

                if (node->hasChildNodes())
                    fromDescendantsRev(node, aContext, nodes);

                if (mNodeTest->matches(node, aContext))
                    nodes->append(node);

                while (node && !node->getPreviousSibling()) {
                    node = node->getParentNode();
                }
            }
            break;
        case PRECEDING_SIBLING_AXIS:
            reverse = MB_TRUE;
            node = node->getPreviousSibling();
            while (node) {
                if (mNodeTest->matches(node, aContext))
                    nodes->append(node);
                node = node->getPreviousSibling();
            }
            break;
        case SELF_AXIS :
            if (mNodeTest->matches(node, aContext))
                nodes->append(node);
            break;
        default: //-- Children Axis
        {
            node = node->getFirstChild();
            while (node) {
                if (mNodeTest->matches(node, aContext))
                    nodes->append(node);
                node = node->getNextSibling();
            }
            break;
        }
    } //-- switch

    //-- apply predicates
    if (!isEmpty())
        evaluatePredicates(nodes, aContext);

    if (reverse)
        nodes->reverse();

    return nodes;
}

void LocationStep::fromDescendants(Node* node, txIMatchContext* cs,
                                   NodeSet* nodes)
{
    if (!node)
        return;

    Node* child = node->getFirstChild();
    while (child) {
        if (mNodeTest->matches(child, cs))
            nodes->append(child);
        //-- check childs descendants
        if (child->hasChildNodes())
            fromDescendants(child, cs, nodes);

        child = child->getNextSibling();
    }

} //-- fromDescendants

void LocationStep::fromDescendantsRev(Node* node, txIMatchContext* cs,
                                      NodeSet* nodes)
{
    if (!node)
        return;

    Node* child = node->getLastChild();
    while (child) {
        //-- check childs descendants
        if (child->hasChildNodes())
            fromDescendantsRev(child, cs, nodes);

        if (mNodeTest->matches(child, cs))
            nodes->append(child);

        child = child->getPreviousSibling();
    }

} //-- fromDescendantsRev

/**
 * Creates a String representation of this Expr
 * @param str the destination String to append to
 * @see Expr
**/
void LocationStep::toString(String& str) {
    switch (mAxisIdentifier) {
        case ANCESTOR_AXIS :
            str.append("ancestor::");
            break;
        case ANCESTOR_OR_SELF_AXIS :
            str.append("ancestor-or-self::");
            break;
        case ATTRIBUTE_AXIS:
            str.append("@");
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
    NS_ASSERTION(mNodeTest, "mNodeTest is null, that's verboten");
    mNodeTest->toString(str);

    PredicateList::toString(str);
} // toString

