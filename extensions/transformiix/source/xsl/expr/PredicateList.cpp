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
 * Represents an ordered list of Predicates,
 * for use with Step and Filter Expressions
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/
//-- PredicateList Implementation --/

PredicateList::PredicateList() {
} //-- PredicateList

/**
 * Destructor, will delete all Expressions in the list, so remove
 * any you may need
**/
PredicateList::~PredicateList() {
    //cout << "-PredicateList() - start"<<endl;
    ListIterator* iter = predicates.iterator();
    while ( iter->hasNext() ) {
        iter->next();
        Expr* expr = (Expr*) iter->remove();
        delete expr;
    }
    delete iter;
    //cout << "~PredicateList() - end"<<endl;
} //-- ~PredicateList

/**
 * Adds the given Expr to the list
 * @param expr the Expr to add to the list
**/
void PredicateList::add(Expr* expr) {
    predicates.add(expr);
} //-- add

void PredicateList::evaluatePredicates(NodeSet* nodes, ContextState* cs) {
    if ( !nodes ) return;

    ListIterator* iter = predicates.iterator();
    NodeSet remNodes(nodes->size());

    Stack* nsStack = cs->getNodeSetStack();
    nsStack->push(nodes);
    while ( iter->hasNext() ) {
        int nIdx = 0;

        Expr* expr = (Expr*) iter->next();
        //-- filter each node currently in the NodeSet
        for (nIdx = 0; nIdx < nodes->size(); nIdx++) {

            Node* node = nodes->get(nIdx);

            //-- if expr evaluates to true using the node as it's context,
            //-- then we can keep it, otherwise add to remove list
            ExprResult* exprResult = expr->evaluate(node,cs);
            if ( !exprResult ) {
                cout << "ExprResult == null" << endl;
            }
            else {
                switch(exprResult->getResultType()) {
                    case ExprResult::NUMBER :
                    {
                        //-- handle default position()
                        int position = nodes->indexOf(node)+1;
                        if (( position <= 0 ) ||
                            ( ((double)position) != exprResult->numberValue()))
                            remNodes.add(node);

                        break;
                    }
                    default:
                        if (! exprResult->booleanValue() ) remNodes.add(node);
                        break;
                }
            }
        }
        //-- remove unmatched nodes
        for (nIdx = 0; nIdx < remNodes.size(); nIdx++)
            nodes->remove(remNodes.get(nIdx));
        //-- clear remove list
        remNodes.clear();
    }
    nsStack->pop();
    delete iter;
} //-- evaluatePredicates

/**
 * returns true if this predicate list is empty
**/
MBool PredicateList::isEmpty() {
    return (MBool)(predicates.getLength()>0);
} //-- isEmpty

/**
 * Removes the given Expr from the list
 * @param expr the Expr to remove from the list
**/
Expr* PredicateList::remove(Expr* expr) {
    return (Expr*)predicates.remove(expr);
} //-- remove

void PredicateList::toString(String& dest) {

    ListIterator* iter = predicates.iterator();

    while ( iter->hasNext() ) {
        Expr* expr = (Expr*) iter->next();
        dest.append("[");
        expr->toString(dest);
        dest.append("]");
    }
    delete iter;

} //-- toString

