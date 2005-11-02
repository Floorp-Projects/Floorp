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
 * $Id: txPredicateList.cpp,v 1.2 2005/11/02 07:33:51 peterv%netscape.com Exp $
 */

#include "Expr.h"

/**
 * Represents an ordered list of Predicates,
 * for use with Step and Filter Expressions
 * @author <A HREF="mailto:kvisco@ziplink.net">Keith Visco</A>
 * @version $Revision: 1.2 $ $Date: 2005/11/02 07:33:51 $
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
                delete exprResult;
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
    return (MBool)(predicates.getLength() == 0);
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

