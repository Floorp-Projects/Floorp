/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Peter Van der Beken, peterv@netscape.com
 *    -- original author.
 *
 */

#include "XSLTFunctions.h"
#include "XMLUtils.h"
#include "Names.h"

/*
  Implementation of XSLT 1.0 extension function: element-available
*/

/**
 * Creates a new element-available function call
**/
ElementAvailableFunctionCall::ElementAvailableFunctionCall() :
        FunctionCall(ELEMENT_AVAILABLE_FN)
{
}

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param cs the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
 * @see FunctionCall.h
**/
ExprResult* ElementAvailableFunctionCall::evaluate(Node* context, ContextState* cs) {
    ExprResult* result = NULL;

    if ( requireParams(1,1,cs) ) {
        ListIterator* iter = params.iterator();
        Expr* param = (Expr*) iter->next();
        delete iter;
        ExprResult* exprResult = param->evaluate(context, cs);
        if (exprResult->getResultType() == ExprResult::STRING) {
            String property;
            exprResult->stringValue(property);
            if (XMLUtils::isValidQName(property)) {
                String prefix, propertyNsURI;
                XMLUtils::getNameSpace(property, prefix);
                if (prefix.length() > 0) {
                    cs->getNameSpaceURIFromPrefix(property, propertyNsURI);
                }
                if (propertyNsURI.isEqual(XSLT_NS)) {
                    String localName;
                    XMLUtils::getLocalPart(property, localName);
                    if ( localName.isEqual(APPLY_IMPORTS) ||
                         localName.isEqual(APPLY_TEMPLATES) ||
                         localName.isEqual(ATTRIBUTE) ||
                         localName.isEqual(ATTRIBUTE_SET) ||
                         localName.isEqual(CALL_TEMPLATE) ||
                         localName.isEqual(CHOOSE) ||
                         localName.isEqual(COMMENT) ||
                         localName.isEqual(COPY) ||
                         localName.isEqual(COPY_OF) ||
                         localName.isEqual(ELEMENT) ||
                         localName.isEqual(FOR_EACH) ||
                         localName.isEqual(IF) ||
                         localName.isEqual(IMPORT) ||
                         localName.isEqual(INCLUDE) ||
                         localName.isEqual(KEY) ||
                         localName.isEqual(MESSAGE) ||
                         localName.isEqual(NUMBER) ||
                         localName.isEqual(OTHERWISE) ||
                         localName.isEqual(OUTPUT) ||
                         localName.isEqual(PARAM) ||
                         localName.isEqual(PROC_INST) ||
                         localName.isEqual(PRESERVE_SPACE) ||
                         localName.isEqual(SORT) ||
                         localName.isEqual(STRIP_SPACE) ||
                         localName.isEqual(TEMPLATE) ||
                         localName.isEqual(TEXT) ||
                         localName.isEqual(VALUE_OF) ||
                         localName.isEqual(VARIABLE) ||
                         localName.isEqual(WHEN) ||
                         localName.isEqual(WITH_PARAM) ) {
                        result = new BooleanResult(MB_TRUE);
                    }
                }
            }
        }
        else {
            String err("Invalid argument passed to element-available(), expecting String");
            delete result;
            result = new StringResult(err);
        }
    }

    if (!result) {
        result = new BooleanResult(MB_FALSE);
    }

    return result;
}

