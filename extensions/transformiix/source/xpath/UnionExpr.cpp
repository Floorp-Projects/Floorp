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
    while (iter->hasNext()) {
         iter->next();
         delete  (Expr*)iter->remove();
    }
    delete iter;
} //-- ~UnionExpr

/**
 * Adds the Expr to this UnionExpr
 * @param expr the Expr to add to this UnionExpr
**/
void UnionExpr::addExpr(Expr* expr) {
    if (expr)
      expressions.add(expr);
} //-- addExpr

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
ExprResult* UnionExpr::evaluate(Node* context, ContextState* cs) {

    if (!context || (expressions.getLength() == 0))
            return new NodeSet(0);

    NodeSet* nodes = new NodeSet();

    ListIterator* iter = expressions.iterator();

    while (iter->hasNext()) {
        Expr* expr = (Expr*)iter->next();
        ExprResult* exprResult = expr->evaluate(context, cs);
        if (exprResult->getResultType() == ExprResult::NODESET) {
            ((NodeSet*)exprResult)->copyInto(*nodes);
        }
    }

    delete iter;
    return nodes;
} //-- evaluate

/**
 * Returns the default priority of this Pattern based on the given Node,
 * context Node, and ContextState.
**/
double UnionExpr::getDefaultPriority(Node* node, Node* context,
                                     ContextState* cs)
{
    //-- find highest priority
    double priority = Double::NEGATIVE_INFINITY;
    ListIterator iter(&expressions);
    while (iter.hasNext()) {
        Expr* expr = (Expr*)iter.next();
        double tmpPriority = expr->getDefaultPriority(node, context, cs);
        if (tmpPriority > priority && expr->matches(node, context, cs))
            priority = tmpPriority;
    }
    return priority;
} //-- getDefaultPriority

/**
 * Determines whether this UnionExpr matches the given node within
 * the given context
**/
MBool UnionExpr::matches(Node* node, Node* context, ContextState* cs) {

    ListIterator* iter = expressions.iterator();

    while (iter->hasNext()) {
        Expr* expr = (Expr*)iter->next();
        if (expr->matches(node, context, cs)) {
             delete iter;
             return MB_TRUE;
        }
    }
    delete iter;
    return MB_FALSE;
} //-- matches


/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 *  any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void UnionExpr::toString(String& dest) {
    ListIterator* iter = expressions.iterator();

    short count = 0;
    while (iter->hasNext()) {
        //-- set operator
        if (count > 0)
            dest.append(" | ");
        ((Expr*)iter->next())->toString(dest);
        ++count;
    }
    delete iter;
} //-- toString

