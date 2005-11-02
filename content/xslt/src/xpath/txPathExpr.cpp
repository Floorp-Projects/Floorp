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
 * Bob Miller, kbob@oblix.com
 *    -- plugged core leak.
 *
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *    -- fixed bug in PathExpr::matches
 *       - foo//bar would not match properly if there was more than
 *         one node in the NodeSet (nodes) on the final iteration
 *
 * $Id: txPathExpr.cpp,v 1.2 2005/11/02 07:33:38 kvisco%ziplink.net Exp $
 */

#include "Expr.h"

  //------------/
 //- PathExpr -/
//------------/


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
#if 0
            NodeSet* xNodes = (NodeSet*) pxi->pExpr->evaluate(node, cs);
#else
           ExprResult *res = pxi->pExpr->evaluate(node, cs);
           if (!res || res->getResultType() != ExprResult::NODESET)
               continue;
            NodeSet* xNodes = (NodeSet *) res;
#endif
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

    if ( (!node)  || (expressions.getLength() == 0))
       return MB_FALSE;

    NodeSet nodes(3);
    NodeSet tmpNodes(3);

    nodes.add(node);


    ListIterator* iter = expressions.iterator();
    iter->reverse();

    while ( iter->hasNext() ) {

        PathExprItem* pxi = (PathExprItem*)iter->next();

        for (int i = 0; i < nodes.size(); i++) {

            Node* tnode = nodes.get(i);

                //-- select node's parent or ancestors
            switch (pxi->ancestryOp) {
                case ANCESTOR_OP:
                {
                    Node* parent = tnode;
                    while (parent = cs->getParentNode(parent))  {
                        if (pxi->pExpr->matches(tnode, parent, cs))
                            tmpNodes.add(parent);
                    }
                    break;
                }
                case PARENT_OP:
                {
                    Node* parent = cs->getParentNode(tnode);
                    if (parent) {
                        if (pxi->pExpr->matches(tnode, parent, cs))
                            tmpNodes.add(parent);
                    }
                    break;
                }
                default:
                    if ( !iter->hasNext() ) {

                        /*
                          // PREVIOUS // result = pxi->pExpr->matches(tnode, context, cs);
                          // result was being overwritten if there was more than one
                          // node in nodes during the final iteration  (Marina)

                          result = result || pxi->pExpr->matches(tnode, context, cs)
                        */

                        //-- Just return true if we match here
                        if (pxi->pExpr->matches(tnode, context, cs)) {
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
        return (MBool) nodes.contains(doc);
    }

    return (MBool) (nodes.size() > 0);

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

