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
void PathExpr::addExpr(Expr* expr, PathOperator pathOp)
{
    NS_ASSERTION(expressions.getLength() > 0 || pathOp == RELATIVE_OP,
                 "First step has to be relative in PathExpr");
    if (expr) {
        PathExprItem* pxi = new PathExprItem;
        if (!pxi) {
            // XXX ErrorReport: out of memory
            NS_ASSERTION(0, "out of memory");
            return;
        }
        pxi->expr = expr;
        pxi->pathOp = pathOp;
        expressions.add(pxi);
    }
} //-- addPattenExpr

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
    if (!nodes) {
        // XXX ErrorReport: out of memory
        NS_ASSERTION(0, "out of memory");
        return 0;
    }
    nodes->add(context);

    ListIterator iter(&expressions);
    PathExprItem* pxi;

    while ((pxi = (PathExprItem*)iter.next())) {
        NodeSet* tmpNodes = 0;
        for (int i = 0; i < nodes->size(); i++) {
            Node* node = nodes->get(i);
            
            NodeSet* resNodes;
            if (pxi->pathOp == DESCENDANT_OP) {
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
        delete nodes;
        nodes = tmpNodes;
        if (!nodes || (nodes->size() == 0)) break;
    }
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
        if (!(filterWS &&
              (child->getNodeType() == Node::TEXT_NODE ||
               child->getNodeType() == Node::CDATA_SECTION_NODE) &&
              XMLUtils::shouldStripTextnode(child->getNodeValue())))
            evalDescendants(expr, child, cs, resNodes);
        child = child->getNextSibling();
    }
} //-- evalDescendants

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
**/
double PathExpr::getDefaultPriority(Node* node, Node* context,
                                    ContextState* cs)
{
    int size = expressions.getLength();
    if (size > 1)
        return 0.5;

    return ((PathExprItem*)expressions.get(0))->
        expr->getDefaultPriority(node, context, cs);
} //-- getDefaultPriority

/**
 * Determines whether this Expr matches the given node within
 * the given context
**/
MBool PathExpr::matches(Node* node, Node* context, ContextState* cs)
{
    /*
     * The idea is to split up a path into blocks separated by descendant
     * operators. For example "foo/bar//baz/bop//ying/yang" is split up into
     * three blocks. The "ying/yang" block is handled by the first while-loop
     * and the "foo/bar" and "baz/bop" blocks are handled by the second
     * while-loop.
     * A block is considered matched when we find a list of ancestors that
     * match the block. If there are more than one list of ancestors that
     * match a block we only need to find the one furthermost down in the
     * tree.
     */

    if (!node || (expressions.getLength() == 0))
       return MB_FALSE;

    ListIterator iter(&expressions);
    iter.resetToEnd();

    PathExprItem* pxi;
    PathOperator pathOp = RELATIVE_OP;
    
    while (pathOp == RELATIVE_OP) {

        pxi = (PathExprItem*)iter.previous();
        if (!pxi)
            return MB_TRUE; // We've stepped through the entire list

        if (!node || !pxi->expr->matches(node, 0, cs))
            return MB_FALSE;
        
        node = node->getXPathParent();
        pathOp = pxi->pathOp;
    }
    
    // We have at least one DESCENDANT_OP
    
    Node* blockStart = node;
    ListIterator blockIter(iter);

    while ((pxi = (PathExprItem*)iter.previous())) {

        if (!node)
            return MB_FALSE; // There are more steps in the current block 
                             // then ancestors of the tested node

        if (!pxi->expr->matches(node, 0, cs)) {
            // Didn't match. We restart at beginning of block using a new
            // start node
            iter = blockIter;
            blockStart = blockStart->getXPathParent();
            node = blockStart;
        }
        else {
            node = node->getXPathParent();
            if (pxi->pathOp == DESCENDANT_OP) {
                // We've matched an entire block. Set new start iter and start node
                blockIter = iter;
                blockStart = node;
            }
        }
    }
    
    return MB_TRUE;

} //-- matches


/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void PathExpr::toString(String& dest)
{
    ListIterator iter(&expressions);
    
    PathExprItem* pxi = (PathExprItem*)iter.next();
    if (pxi) {
        NS_ASSERTION(pxi->pathOp == RELATIVE_OP,
                     "First step should be relative");
        pxi->expr->toString(dest);
    }
    
    while ((pxi = (PathExprItem*)iter.next())) {
        switch (pxi->pathOp) {
            case DESCENDANT_OP:
                dest.append("//");
                break;
            case RELATIVE_OP:
                dest.append('/');
                break;
        }
        pxi->expr->toString(dest);
    }
} //-- toString
