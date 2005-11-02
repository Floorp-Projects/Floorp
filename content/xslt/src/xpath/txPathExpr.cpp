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
 * Bob Miller, kbob@oblix.com
 *    -- plugged core leak.
 *
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *    -- fixed bug in PathExpr::matches
 *       - foo//bar would not match properly if there was more than
 *         one node in the NodeSet (nodes) on the final iteration
 *
 */

#include "Expr.h"
#include "XMLUtils.h"

  //------------/
 //- PathExpr -/
//------------/


/**
 * Creates a new PathExpr
**/
PathExpr::PathExpr()
{
    //-- do nothing
}

/**
 * Destructor, will delete all Expressions
**/
PathExpr::~PathExpr()
{
    ListIterator* iter = expressions.iterator();
    while (iter->hasNext()) {
         iter->next();
         PathExprItem* pxi = (PathExprItem*)iter->remove();
         delete pxi->expr;
         delete pxi;
    }
    delete iter;
} //-- ~PathExpr

/**
 * Adds the Expr to this PathExpr
 * @param expr the Expr to add to this PathExpr
**/
void PathExpr::addExpr(Expr* expr, short ancestryOp)
{
    if (expr) {
        PathExprItem* pxi = new PathExprItem;
        pxi->expr = expr;
        pxi->ancestryOp = ancestryOp;
        expressions.add(pxi);
    }
} //-- addPattenExpr

MBool PathExpr::isAbsolute()
{
    if (expressions.getLength() > 0) {
        ListIterator* iter = expressions.iterator();
        PathExprItem* pxi = (PathExprItem*)iter->next();
        delete iter;
        return (pxi->ancestryOp != RELATIVE_OP);
    }
    return MB_FALSE;
} //-- isAbsolute

    //-----------------------------/
  //- Virtual methods from Expr -/
//-----------------------------/

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* PathExpr::evaluate(Node* context, ContextState* cs)
{
    //-- add selectExpr functionality here

    if (!context || (expressions.getLength() == 0))
            return new NodeSet(0);

    NodeSet* nodes = new NodeSet();

    if (isAbsolute() && (context->getNodeType() != Node::DOCUMENT_NODE))
        nodes->add(context->getOwnerDocument());
    else
        nodes->add(context);

    ListIterator* iter = expressions.iterator();

    while (iter->hasNext()) {
        PathExprItem* pxi = (PathExprItem*)iter->next();
        NodeSet* tmpNodes = 0;
        cs->getNodeSetStack()->push(nodes);
        for (int i = 0; i < nodes->size(); i++) {
            Node* node = nodes->get(i);
            
            NodeSet* resNodes;
            if (pxi->ancestryOp == ANCESTOR_OP) {
                resNodes = new NodeSet;
                evalDescendants(pxi->expr, node, cs, resNodes);
            }
            else {
                ExprResult *res = pxi->expr->evaluate(node, cs);
                if (!res || (res->getResultType() != ExprResult::NODESET)) {
                    //XXX ErrorReport: report nonnodeset error
                    delete res;
                    res = new NodeSet;
                }
                resNodes = (NodeSet*)res;
            }

            if (tmpNodes) {
                resNodes->copyInto(*tmpNodes);
                delete resNodes;
            }
            else
                tmpNodes = resNodes;

        }
        delete (NodeSet*) cs->getNodeSetStack()->pop();
        nodes = tmpNodes;
        if (!nodes || (nodes->size() == 0)) break;
    }
    delete iter;

    return nodes;
} //-- evaluate

/**
 * Selects from the descendants of the context node
 * all nodes that match the Expr
 * -- this will be moving to a Utility class
**/
void PathExpr::evalDescendants (Expr* expr, Node* context,
                                ContextState* cs, NodeSet* resNodes)
{
    ExprResult *res = expr->evaluate(context, cs);
    if (!res || (res->getResultType() != ExprResult::NODESET)) {
        //XXX ErrorReport: report nonnodeset error
    }
    else
        ((NodeSet*)res)->copyInto(*resNodes);
    delete res;

    MBool filterWS = cs->isStripSpaceAllowed(context);
    
    Node* child = context->getFirstChild();
    while (child) {
        if (!(filterWS && (child->getNodeType() == Node::TEXT_NODE) &&
             XMLUtils::shouldStripTextnode(child->getNodeValue())))
            evalDescendants(expr, child, cs, resNodes);
        child = child->getNextSibling();
    }
} //-- evalDescendants

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
 * If this pattern does not match the given Node under the current context Node and
 * ContextState then Negative Infinity is returned.
**/
double PathExpr::getDefaultPriority(Node* node, Node* context,
                                    ContextState* cs)
{
    if (matches(node, context, cs)) {
        int size = expressions.getLength();
        if (size == 1) {
            ListIterator* iter = expressions.iterator();
            PathExprItem* pxi = (PathExprItem*)iter->next();
            delete iter;
            return pxi->expr->getDefaultPriority(node, context, cs);
        }
        else if (size > 1) 
            return 0.5;
    }
    return Double::NEGATIVE_INFINITY;
} //-- getDefaultPriority

/**
 * Determines whether this Expr matches the given node within
 * the given context
**/
MBool PathExpr::matches(Node* node, Node* context, ContextState* cs)
{
    if (!node || (expressions.getLength() == 0))
       return MB_FALSE;

    //-- for performance reasons, I've duplicated some code
    //-- here. If we only have one expression, there is no
    //-- reason to create NodeSets and go through the
    //-- while loop below. This resulted in a decent
    //-- performance gain in XSL:P, so I'm doing it here also,
    //-- even though I have no real code in place to test the
    //-- performance of transformiix.

    if (expressions.getLength() == 1) {
        PathExprItem* pxi = (PathExprItem*)expressions.get(0);
        switch(pxi->ancestryOp) {
            case ANCESTOR_OP:
            {
                Node* ancestor = node;
                while ((ancestor = cs->getParentNode(ancestor)))  {
                    if (pxi->expr->matches(node, ancestor, cs))
                        return MB_TRUE;
                }
                break;
            }
            case PARENT_OP:
            {
                Node* parent = cs->getParentNode(node);
                if (parent) {
                    //-- make sure node is Document node
                    if (parent->getNodeType() == Node::DOCUMENT_NODE)
                        return pxi->expr->matches(node, parent, cs);
                }
                break;
            }
            default:
                return pxi->expr->matches(node, context, cs);
        }

        return MB_FALSE;
    }

    //-- if we reach here we have subpaths...
    NodeSet nodes(3);
    NodeSet tmpNodes(3);

    nodes.add(node);

    ListIterator* iter = expressions.iterator();
    iter->resetToEnd();

    while (iter->hasPrevious()) {

        PathExprItem* pxi = (PathExprItem*)iter->previous();

        for (int i = 0; i < nodes.size(); i++) {
            Node* tnode = nodes.get(i);

            //-- select node's parent or ancestors
            switch (pxi->ancestryOp) {
                case ANCESTOR_OP:
                {
                    Node* parent = tnode;
                    while ((parent = cs->getParentNode(parent)))  {
                        if (pxi->expr->matches(tnode, parent, cs))
                            tmpNodes.add(parent);
                    }
                    break;
                }
                case PARENT_OP:
                {
                    Node* parent = cs->getParentNode(tnode);
                    if (parent) {
                        //-- make sure we have a document node if necessary
                        if (!iter->hasPrevious())
                            if (parent->getNodeType() != Node::DOCUMENT_NODE)
                                break;
                        if (pxi->expr->matches(tnode, parent, cs))
                            tmpNodes.add(parent);
                    }
                    break;
                }
                default:
                    if (!iter->hasPrevious()) {
                        /*
                          // PREVIOUS // result = pxi->expr->matches(tnode, context, cs);
                          // result was being overwritten if there was more than one
                          // node in nodes during the final iteration  (Marina)

                          result = result || pxi->expr->matches(tnode, context, cs)
                        */

                        //-- Just return true if we match here
                        if (pxi->expr->matches(tnode, context, cs)) {
                            delete iter;
                            return MB_TRUE;
                        }
                    }
                    else {
                        //-- error in expression, will we ever see this?
                        delete iter;
                        return MB_FALSE;
                    }
                    break;
            }
        } //-- for

        if (tmpNodes.size() == 0) {
            delete iter;
            return MB_FALSE;
        }

        nodes.clear();
        tmpNodes.copyInto(nodes);
        tmpNodes.clear();
    }

    delete iter;

    if ( this->isAbsolute()) {
        Node* doc = 0;
        if (node->getNodeType() == Node::DOCUMENT_NODE)
            doc = node;
        else
            doc = node->getOwnerDocument();
        return (MBool)nodes.contains(doc);
    }

    return (MBool)(nodes.size() > 0);

} //-- matches


/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 *  any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void PathExpr::toString(String& dest)
{
    ListIterator* iter = expressions.iterator();
    while (iter->hasNext()) {
        //-- set operator
        PathExprItem* pxi = (PathExprItem*)iter->next();
        switch (pxi->ancestryOp) {
            case ANCESTOR_OP:
                dest.append("//");
                break;
            case PARENT_OP:
                dest.append('/');
                break;
            default:
                break;
        }
        pxi->expr->toString(dest);
    }
    delete iter;
} //-- toString

