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
 * Axel Hecht.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Axel Hecht <axel@pike.org>
 *
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
#include "txAtoms.h"
#include "txError.h"
#include "XMLUtils.h"
#include "XSLTFunctions.h"
#include "ExprResult.h"
#include "txNamespaceMap.h"

/*
  Implementation of XSLT 1.0 extension function: system-property
*/

/**
 * Creates a new system-property function call
 * aNode is the Element in the stylesheet containing the 
 * Expr and is used for namespaceID resolution
**/
SystemPropertyFunctionCall::SystemPropertyFunctionCall(txNamespaceMap* aMappings)
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
SystemPropertyFunctionCall::evaluate(txIEvalContext* aContext,
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
    rv = qname.init(property, mMappings, MB_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    if (qname.mNamespaceID == kNameSpaceID_XSLT) {
        if (qname.mLocalName == txXSLTAtoms::version) {
            return aContext->recycler()->getNumberResult(1.0, aResult);
        }
        if (qname.mLocalName == txXSLTAtoms::vendor) {
            return aContext->recycler()->getStringResult(
                  NS_LITERAL_STRING("Transformiix"), aResult);
        }
        if (qname.mLocalName == txXSLTAtoms::vendorUrl) {
            return aContext->recycler()->getStringResult(
                  NS_LITERAL_STRING("http://www.mozilla.org/projects/xslt/"),
                  aResult);
        }
    }
    aContext->recycler()->getEmptyStringResult(aResult);

    return NS_OK;

}

#ifdef TX_TO_STRING
nsresult
SystemPropertyFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = txXSLTAtoms::systemProperty;
    NS_ADDREF(*aAtom);
    return NS_OK;
}
#endif
