/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TransforMiiX XSLT processor code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@propagandism.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "txIXPathContext.h"
#include "ExprResult.h"
#include "txAtoms.h"
#include "txError.h"
#include "XMLUtils.h"
#include "XSLTFunctions.h"
#include "txNamespaceMap.h"

/*
  Implementation of XSLT 1.0 extension function: function-available
*/

/**
 * Creates a new function-available function call
**/
FunctionAvailableFunctionCall::FunctionAvailableFunctionCall(txNamespaceMap* aMappings)
    : mMappings(aMappings)
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
nsresult
FunctionAvailableFunctionCall::evaluate(txIEvalContext* aContext,
                                        txAExprResult** aResult)
{
    *aResult = nsnull;
    if (!requireParams(1, 1, aContext)) {
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }

    txListIterator iter(&params);
    Expr* param = (Expr*)iter.next();
    nsRefPtr<txAExprResult> exprResult;
    nsresult rv = param->evaluate(aContext, getter_AddRefs(exprResult));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString property;
    exprResult->stringValue(property);
    txExpandedName qname;
    rv = qname.init(property, mMappings, MB_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool val = qname.mNamespaceID == kNameSpaceID_None &&
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
                  //qname.mLocalName == txXSLTAtoms::unparsedEntityUri ||
                  qname.mLocalName == txXSLTAtoms::systemProperty);

    aContext->recycler()->getBoolResult(val, aResult);
        
    return NS_OK;

}

#ifdef TX_TO_STRING
nsresult
FunctionAvailableFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = txXSLTAtoms::functionAvailable;
    NS_ADDREF(*aAtom);
    return NS_OK;
}
#endif
