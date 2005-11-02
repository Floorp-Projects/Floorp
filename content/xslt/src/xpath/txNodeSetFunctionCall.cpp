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
 * Marina Mechtcheriakova, mmarina@mindspring.com
 *   -- changed some behavoir to be more compliant with spec
 *    
 * $Id: txNodeSetFunctionCall.cpp,v 1.3 2005/11/02 07:33:46 axel%pike.org Exp $
 */

/**
 * NodeSetFunctionCall
 * A representation of the XPath NodeSet funtions
 * @author <A HREF="mailto:kvisco@ziplink.net">Keith Visco</a>
 * @version $Revision: 1.3 $ $Date: 2005/11/02 07:33:46 $
**/

#include "FunctionLib.h"

/**
 * Creates a default NodeSetFunctionCall. The Position function
 * is the default
**/
NodeSetFunctionCall::NodeSetFunctionCall() : FunctionCall(XPathNames::POSITION_FN) {
    type = POSITION;
} //-- NodeSetFunctionCall

/**
 * Creates a NodeSetFunctionCall of the given type
**/
NodeSetFunctionCall::NodeSetFunctionCall(short type) : FunctionCall() {
    this->type = type;
    switch ( type ) {
        case COUNT :
            FunctionCall::setName(XPathNames::COUNT_FN);
            break;
        case ID :
            FunctionCall::setName(XPathNames::ID_FN);
            break;
        case LAST :
            FunctionCall::setName(XPathNames::LAST_FN);
            break;
        case LOCAL_NAME:
            FunctionCall::setName(XPathNames::LOCAL_NAME_FN);
            break;
        case NAME:
            FunctionCall::setName(XPathNames::NAME_FN);
            break;
        case NAMESPACE_URI:
            FunctionCall::setName(XPathNames::NAMESPACE_URI_FN);
            break;
        default:
            FunctionCall::setName(XPathNames::POSITION_FN);
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
        case ID :
            if ( requireParams(1, 1, cs) ) {
                NodeSet* resultSet = new NodeSet();
                param = (Expr*)iter->next();
                ExprResult* exprResult = param->evaluate(context, cs);
                String lIDList;
                if ( exprResult->getResultType() == ExprResult::NODESET ) {
                    NodeSet *lNList = (NodeSet *)exprResult;
                    NodeSet tmp;
                    for (int i=0; i<lNList->size(); i++){
                        tmp.add(0,lNList->get(i));
                        tmp.stringValue(lIDList);
                        lIDList.append(' ');
                    };
                } else {
                    exprResult->stringValue(lIDList);
                };
                lIDList.trim();
                Int32 start=0;
                MBool hasSpace = MB_FALSE, isSpace;
                UNICODE_CHAR cc;
                String thisID;
                for (Int32 end=0; end<lIDList.length(); end++){
                    cc = lIDList.charAt(end);
                    isSpace = (cc==' ' || cc=='\n' || cc=='\t'|| cc=='\r');
                    if (isSpace && !hasSpace){
                        hasSpace = MB_TRUE;
                        lIDList.subString(start, end, thisID);
                        resultSet->add(context->getOwnerDocument()->getElementById(thisID));
                    } else if (!isSpace && hasSpace){
                        start = end;
                        hasSpace = MB_FALSE;
                    };
                };
                result = resultSet;
            };
            break;
        case LAST :
            if ( nodeSet ) result = new NumberResult((double)nodeSet->size());
            else result = new NumberResult(0.0);
            break;
        case LOCAL_NAME:
        case NAME:
        case NAMESPACE_URI :
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
                //if ( !node ) node = context;  ///Marina
                else node = context;

                //-- if no node was found just return an empty string (Marina)
                if ( !node ) {
                    result = new StringResult("");
                    break;
                }

                switch ( type ) {
                    case LOCAL_NAME :
                        XMLUtils::getLocalPart(node->getNodeName(),name);
                        break;
                    case NAMESPACE_URI :
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


