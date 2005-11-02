/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "XSLTFunctions.h"
#include "XMLUtils.h"
#include "Names.h"
#include "txIXPathContext.h"

/*
  Implementation of XSLT 1.0 extension function: element-available
*/

/**
 * Creates a new element-available function call
 * aNode is the Element in the stylesheet containing the 
 * Expr and is used for namespaceID resolution
**/
ElementAvailableFunctionCall::ElementAvailableFunctionCall(Element* aNode) :
    FunctionCall(ELEMENT_AVAILABLE_FN), mStylesheetNode(aNode)
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
ExprResult* ElementAvailableFunctionCall::evaluate(txIEvalContext* aContext)
{
    ExprResult* result = NULL;

    if (requireParams(1, 1, aContext)) {
        txListIterator iter(&params);
        Expr* param = (Expr*) iter.next();
        ExprResult* exprResult = param->evaluate(aContext);
        if (exprResult &&
            exprResult->getResultType() == ExprResult::STRING) {
            String property;
            exprResult->stringValue(property);
            if (XMLUtils::isValidQName(property)) {
                String prefix;
                PRInt32 aNSID = kNameSpaceID_None;
                XMLUtils::getPrefix(property, prefix);
                if (!prefix.isEmpty()) {
                    txAtom* prefixAtom = TX_GET_ATOM(prefix);
                    aNSID = mStylesheetNode->lookupNamespaceID(prefixAtom);
                    TX_IF_RELEASE_ATOM(prefixAtom);
                }
                if (aNSID == kNameSpaceID_XSLT) {
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
                         localName.isEqual(DECIMAL_FORMAT) ||
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
            aContext->receiveError(err, NS_ERROR_XPATH_INVALID_ARG);
            result = new StringResult(err);
        }
        delete exprResult;
    }

    if (!result) {
        result = new BooleanResult(MB_FALSE);
    }

    return result;
}

