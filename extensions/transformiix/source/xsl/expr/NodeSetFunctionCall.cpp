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

/**
 * NodeSetFunctionCall
 * A representation of the XPath NodeSet funtions
 * @author <a href="mailto:kvisco@mitre.org">Keith Visco</a>
**/

#include "FunctionLib.h"

/**
 * Creates a default NodeSetFunctionCall. The Position function
 * is the default
**/
NodeSetFunctionCall::NodeSetFunctionCall() : FunctionCall(POSITION_FN) {
    type = POSITION;
} //-- NodeSetFunctionCall

/**
 * Creates a NodeSetFunctionCall of the given type
**/
NodeSetFunctionCall::NodeSetFunctionCall(short type) : FunctionCall() {
    this->type = type;
    switch ( type ) {
        case COUNT :
            FunctionCall::setName(COUNT_FN);
            break;
        case LAST :
            FunctionCall::setName(LAST_FN);
            break;
        case LOCAL_PART:
            FunctionCall::setName(LOCAL_PART_FN);
            break;
        case NAME:
            FunctionCall::setName(NAME_FN);
            break;
        case NAMESPACE:
            FunctionCall::setName(NAMESPACE_FN);
            break;
        default:
            FunctionCall::setName(POSITION_FN);
            break;
    }
} //-- NodeSetFunctionCall

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
ExprResult* NodeSetFunctionCall::evaluate(Node* context, ContextState* cs) {
    NodeSet* nodeSet = (NodeSet*)cs->getNodeSetStack()->peek();
    ListIterator* iter = params.iterator();
    Int32 argc = params.getLength();
    ExprResult* result = 0;
    Expr* param = 0;
    switch ( type ) {
        case COUNT :
            if ( argc == 1 ) {
                double count = 0.0;
                param = (Expr*)iter->next();
                ExprResult* exprResult = param->evaluate(context, cs);
                if ( exprResult->getResultType() != ExprResult::NODESET ) {
                    String err("NodeSet expected as argument to count()");
                    cs->recieveError(err);
                }
                else count = (double) ((NodeSet*)exprResult)->size();
                delete exprResult;
                result = new NumberResult(count);
            }
            else {
                String err(INVALID_PARAM_COUNT);
                this->toString(err);
                cs->recieveError(err);
                result = new NumberResult(0.0);
            }
            break;
        case LAST :
            if ( nodeSet ) result = new NumberResult((double)nodeSet->size());
            else result = new NumberResult(0.0);
            break;
        case LOCAL_PART:
        case NAME:
        case NAMESPACE :
        {
            String name;
            Node* node = 0;
            if ( argc < 2 ) {

                //-- check for optional arg
                if ( argc == 1) {
                    param = (Expr*)iter->next();
                    ExprResult* exprResult = param->evaluate(context, cs);
                    if ( exprResult->getResultType() != ExprResult::NODESET ) {
                        String err("NodeSet expected as argument to ");
                        this->toString(err);
                        cs->recieveError(err);
                    }
                    else {
                        NodeSet* nodes = (NodeSet*)exprResult;
                        if (nodes->size() > 0) node = nodes->get(0);
                    }
                    delete exprResult;
                }
                if ( !node ) node = context;

                switch ( type ) {
                    case LOCAL_PART :
                        XMLUtils::getLocalPart(node->getNodeName(),name);
                        break;
                    case NAMESPACE :
                        XMLUtils::getNameSpace(node->getNodeName(),name);
                        break;
                    default:
                        name = node->getNodeName();
                        break;
                }
                result = new StringResult(name);
            }
            else {
                String err(INVALID_PARAM_COUNT);
                this->toString(err);
                cs->recieveError(err);
                result = new StringResult("");
            }
            break;
        }
        default : //-- position
            if ( nodeSet )
                result = new NumberResult((double)nodeSet->indexOf(context)+1);
            else
                result = new NumberResult(0.0);
            break;
    }
    delete iter;
    return result;
} //-- evaluate


