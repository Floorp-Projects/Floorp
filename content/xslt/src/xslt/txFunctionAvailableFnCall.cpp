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
 * Peter Van der Beken, peterv@netscape.com
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
#include "FunctionLib.h"
#include "XMLUtils.h"
#include "Names.h"

/*
  Implementation of XSLT 1.0 extension function: function-available
*/

/**
 * Creates a new function-available function call
**/
FunctionAvailableFunctionCall::FunctionAvailableFunctionCall() :
        FunctionCall(FUNCTION_AVAILABLE_FN)
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
ExprResult* FunctionAvailableFunctionCall::evaluate(Node* context, ContextState* cs) {
    ExprResult* result = NULL;

    if ( requireParams(1,1,cs) ) {
        ListIterator iter(&params);
        Expr* param = (Expr*)iter.next();
        ExprResult* exprResult = param->evaluate(context, cs);
        if (exprResult &&
            exprResult->getResultType() == ExprResult::STRING) {
            String property;
            exprResult->stringValue(property);
            if (XMLUtils::isValidQName(property)) {
                String prefix;
                XMLUtils::getNameSpace(property, prefix);
                if (prefix.isEmpty() &&
                    (property.isEqual(XPathNames::BOOLEAN_FN) ||
                     property.isEqual(XPathNames::CONCAT_FN) ||
                     property.isEqual(XPathNames::CONTAINS_FN) ||
                     property.isEqual(XPathNames::COUNT_FN ) ||
                     property.isEqual(XPathNames::FALSE_FN) ||
                     property.isEqual(XPathNames::ID_FN) ||
                     property.isEqual(XPathNames::LANG_FN) ||
                     property.isEqual(XPathNames::LAST_FN) ||
                     property.isEqual(XPathNames::LOCAL_NAME_FN) ||
                     property.isEqual(XPathNames::NAME_FN) ||
                     property.isEqual(XPathNames::NAMESPACE_URI_FN) ||
                     property.isEqual(XPathNames::NORMALIZE_SPACE_FN) ||
                     property.isEqual(XPathNames::NOT_FN) ||
                     property.isEqual(XPathNames::POSITION_FN) ||
                     property.isEqual(XPathNames::STARTS_WITH_FN) ||
                     property.isEqual(XPathNames::STRING_FN) ||
                     property.isEqual(XPathNames::STRING_LENGTH_FN) ||
                     property.isEqual(XPathNames::SUBSTRING_FN) ||
                     property.isEqual(XPathNames::SUBSTRING_AFTER_FN) ||
                     property.isEqual(XPathNames::SUBSTRING_BEFORE_FN) ||
                     property.isEqual(XPathNames::SUM_FN) ||
                     property.isEqual(XPathNames::TRANSLATE_FN) ||
                     property.isEqual(XPathNames::TRUE_FN) ||
                     property.isEqual(XPathNames::NUMBER_FN) ||
                     property.isEqual(XPathNames::ROUND_FN) ||
                     property.isEqual(XPathNames::CEILING_FN) ||
                     property.isEqual(XPathNames::FLOOR_FN) ||
                     property.isEqual(DOCUMENT_FN) ||
                     property.isEqual(KEY_FN) ||
                     property.isEqual(FORMAT_NUMBER_FN) ||
                     property.isEqual(CURRENT_FN) ||
                     // property.isEqual(UNPARSED_ENTITY_URI_FN) ||
                     property.isEqual(GENERATE_ID_FN) ||
                     property.isEqual(SYSTEM_PROPERTY_FN) ||
                     property.isEqual(ELEMENT_AVAILABLE_FN) ||
                     property.isEqual(FUNCTION_AVAILABLE_FN))) {
                    result = new BooleanResult(MB_TRUE);
                }
            }
        }
        else {
            String err("Invalid argument passed to function-available, expecting String");
            delete result;
            result = new StringResult(err);
        }
        delete exprResult;
    }

    if (!result) {
        result = new BooleanResult(MB_FALSE);
    }

    return result;
}

