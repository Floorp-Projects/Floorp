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

  //-------------------/
 //- VariableRefExpr -/
//-------------------/

/**
 * Creates a VariableRefExpr with the given variable name
**/
VariableRefExpr::VariableRefExpr(const String& name) {
    this->name = name;
} //-- VariableRefExpr

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* VariableRefExpr::evaluate(Node* context, ContextState* cs) {

    ExprResult* exprResult = cs->getVariable(name);
    //-- make copy to prevent deletetion
    //-- I know, I should add a #copy method to ExprResult, I will
    ExprResult* copyOfResult = 0;

    if ( exprResult ) {
        switch ( exprResult->getResultType() ) {
            //-- BooleanResult
            case ExprResult::BOOLEAN :
                copyOfResult = new BooleanResult(exprResult->booleanValue());
                break;
            //-- NodeSet
            case ExprResult::NODESET :
            {
                NodeSet* src = (NodeSet*)exprResult;
                NodeSet* dest = new NodeSet(src->size());
                for ( int i = 0; i < src->size(); i++)
                    dest->add(src->get(i));
                copyOfResult = dest;
                break;
            }
            //-- NumberResult
            case ExprResult::NUMBER :
                copyOfResult = new NumberResult(exprResult->numberValue());
                break;
            //-- StringResult
            default:
                String tmp;
                exprResult->stringValue(tmp);
                StringResult* strResult = new StringResult(tmp);
                copyOfResult = strResult;
                break;
        }
    }
    else copyOfResult = new StringResult();

    return copyOfResult;
} //-- evaluate

/**
 * Returns the String representation of this Expr.
 * @param dest the String to use when creating the String
 * representation. The String representation will be appended to
 * any data in the destination String, to allow cascading calls to
 * other #toString() methods for Expressions.
 * @return the String representation of this Expr.
**/
void VariableRefExpr::toString(String& str) {
    str.append('$');
    str.append(name);
} //-- toString

