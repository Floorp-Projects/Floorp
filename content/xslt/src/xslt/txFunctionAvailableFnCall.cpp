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

#include "txIXPathContext.h"
#include "txAtoms.h"
#include "XMLUtils.h"
#include "XSLTFunctions.h"

/*
  Implementation of XSLT 1.0 extension function: function-available
*/

/**
 * Creates a new function-available function call
**/
FunctionAvailableFunctionCall::FunctionAvailableFunctionCall(Node* aQNameResolveNode)
    : mQNameResolveNode(aQNameResolveNode)
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
ExprResult* FunctionAvailableFunctionCall::evaluate(txIEvalContext* aContext)
{
    ExprResult* result = NULL;

    if (requireParams(1, 1, aContext)) {
        txListIterator iter(&params);
        Expr* param = (Expr*)iter.next();
        ExprResult* exprResult = param->evaluate(aContext);
        if (exprResult &&
            exprResult->getResultType() == ExprResult::STRING) {
            String property;
            exprResult->stringValue(property);
            txExpandedName qname;
            nsresult rv = qname.init(property, mQNameResolveNode, MB_FALSE);
            if (NS_SUCCEEDED(rv) &&
                qname.mNamespaceID == kNameSpaceID_None &&
                (qname.mLocalName == txXPathAtoms::boolean ||
                 qname.mLocalName == txXPathAtoms::ceiling ||
                 qname.mLocalName == txXPathAtoms::concat ||
                 qname.mLocalName == txXPathAtoms::contains ||
                 qname.mLocalName == txXPathAtoms::count ||
                 qname.mLocalName == txXPathAtoms::_false ||
                 qname.mLocalName == txXPathAtoms::floor ||
                 qname.mLocalName == txXPathAtoms::id ||
                 qname.mLocalName == txXPathAtoms::lang ||
                 qname.mLocalName == txXPathAtoms::last ||
                 qname.mLocalName == txXPathAtoms::localName ||
                 qname.mLocalName == txXPathAtoms::name ||
                 qname.mLocalName == txXPathAtoms::namespaceUri ||
                 qname.mLocalName == txXPathAtoms::normalizeSpace ||
                 qname.mLocalName == txXPathAtoms::_not ||
                 qname.mLocalName == txXPathAtoms::number ||
                 qname.mLocalName == txXPathAtoms::position ||
                 qname.mLocalName == txXPathAtoms::round ||
                 qname.mLocalName == txXPathAtoms::startsWith ||
                 qname.mLocalName == txXPathAtoms::string ||
                 qname.mLocalName == txXPathAtoms::stringLength ||
                 qname.mLocalName == txXPathAtoms::substring ||
                 qname.mLocalName == txXPathAtoms::substringAfter ||
                 qname.mLocalName == txXPathAtoms::substringBefore ||
                 qname.mLocalName == txXPathAtoms::sum ||
                 qname.mLocalName == txXPathAtoms::translate ||
                 qname.mLocalName == txXPathAtoms::_true ||
                 qname.mLocalName == txXSLTAtoms::current ||
                 qname.mLocalName == txXSLTAtoms::document ||
                 qname.mLocalName == txXSLTAtoms::elementAvailable ||
                 qname.mLocalName == txXSLTAtoms::formatNumber ||
                 qname.mLocalName == txXSLTAtoms::functionAvailable ||
                 qname.mLocalName == txXSLTAtoms::generateId ||
                 qname.mLocalName == txXSLTAtoms::key ||
//                 qname.mLocalName == txXSLTAtoms::unparsedEntityUri ||
                 qname.mLocalName == txXSLTAtoms::systemProperty)) {
                result = new BooleanResult(MB_TRUE);
            }
        }
        else {
            String err(NS_LITERAL_STRING("Invalid argument passed to function-available, expecting String"));
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

nsresult FunctionAvailableFunctionCall::getNameAtom(txAtom** aAtom)
{
    *aAtom = txXSLTAtoms::functionAvailable;
    TX_ADDREF_ATOM(*aAtom);
    return NS_OK;
}
