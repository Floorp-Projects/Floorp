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
#include "txAtoms.h"
#include "txError.h"
#include "XMLUtils.h"
#include "XSLTFunctions.h"
#include "ExprResult.h"
#include "txNamespaceMap.h"

/*
  Implementation of XSLT 1.0 extension function: element-available
*/

/**
 * Creates a new element-available function call
 * aNode is the Element in the stylesheet containing the 
 * Expr and is used for namespaceID resolution
**/
ElementAvailableFunctionCall::ElementAvailableFunctionCall(txNamespaceMap* aMappings)
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
ElementAvailableFunctionCall::evaluate(txIEvalContext* aContext,
                                       txAExprResult** aResult)
{
    *aResult = nsnull;
    if (!requireParams(1, 1, aContext)) {
        return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;
    }

    txListIterator iter(&params);
    Expr* param = (Expr*) iter.next();

    nsRefPtr<txAExprResult> exprResult;
    nsresult rv = param->evaluate(aContext, getter_AddRefs(exprResult));
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString property;
    exprResult->stringValue(property);
    txExpandedName qname;
    rv = qname.init(property, mMappings, MB_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool val = qname.mNamespaceID == kNameSpaceID_XSLT &&
                 (qname.mLocalName == txXSLTAtoms::applyImports ||
                  qname.mLocalName == txXSLTAtoms::applyTemplates ||
                  qname.mLocalName == txXSLTAtoms::attribute ||
                  qname.mLocalName == txXSLTAtoms::attributeSet ||
                  qname.mLocalName == txXSLTAtoms::callTemplate ||
                  qname.mLocalName == txXSLTAtoms::choose ||
                  qname.mLocalName == txXSLTAtoms::comment ||
                  qname.mLocalName == txXSLTAtoms::copy ||
                  qname.mLocalName == txXSLTAtoms::copyOf ||
                  qname.mLocalName == txXSLTAtoms::decimalFormat ||
                  qname.mLocalName == txXSLTAtoms::element ||
                  qname.mLocalName == txXSLTAtoms::fallback ||
                  qname.mLocalName == txXSLTAtoms::forEach ||
                  qname.mLocalName == txXSLTAtoms::_if ||
                  qname.mLocalName == txXSLTAtoms::import ||
                  qname.mLocalName == txXSLTAtoms::include ||
                  qname.mLocalName == txXSLTAtoms::key ||
                  qname.mLocalName == txXSLTAtoms::message ||
                  //qname.mLocalName == txXSLTAtoms::namespaceAlias ||
                  qname.mLocalName == txXSLTAtoms::number ||
                  qname.mLocalName == txXSLTAtoms::otherwise ||
                  qname.mLocalName == txXSLTAtoms::output ||
                  qname.mLocalName == txXSLTAtoms::param ||
                  qname.mLocalName == txXSLTAtoms::preserveSpace ||
                  qname.mLocalName == txXSLTAtoms::processingInstruction ||
                  qname.mLocalName == txXSLTAtoms::sort ||
                  qname.mLocalName == txXSLTAtoms::stripSpace ||
                  qname.mLocalName == txXSLTAtoms::stylesheet ||
                  qname.mLocalName == txXSLTAtoms::_template ||
                  qname.mLocalName == txXSLTAtoms::text ||
                  qname.mLocalName == txXSLTAtoms::transform ||
                  qname.mLocalName == txXSLTAtoms::valueOf ||
                  qname.mLocalName == txXSLTAtoms::variable ||
                  qname.mLocalName == txXSLTAtoms::when ||
                  qname.mLocalName == txXSLTAtoms::withParam);

    aContext->recycler()->getBoolResult(val, aResult);

    return NS_OK;
}

#ifdef TX_TO_STRING
nsresult
ElementAvailableFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    *aAtom = txXSLTAtoms::elementAvailable;
    NS_ADDREF(*aAtom);
    return NS_OK;
}
#endif
