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
 * The MITRE Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Keith Visco <kvisco@ziplink.net> (Original Author)
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

#include "ExprResult.h"
#include "FunctionLib.h"
#include "nsReadableUtils.h"
#include "txAtoms.h"
#include "txError.h"
#include "txIXPathContext.h"
#include "txStringUtils.h"
#include "txXPathTreeWalker.h"

/**
 * Creates a default BooleanFunctionCall, which always evaluates to False
**/
BooleanFunctionCall::BooleanFunctionCall(BooleanFunctions aType)
    : mType(aType)
{
}

/**
 * Evaluates this Expr based on the given context node and processor state
 * @param context the context node for evaluation of this Expr
 * @param ps the ContextState containing the stack information needed
 * for evaluation
 * @return the result of the evaluation
**/
nsresult
BooleanFunctionCall::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nsnull;

    txListIterator iter(&params);
    switch (mType) {
        case TX_BOOLEAN:
        {
            if (!requireParams(1, 1, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            aContext->recycler()->getBoolResult(
                evaluateToBoolean((Expr*)iter.next(), aContext), aResult);

            return NS_OK;
        }
        case TX_LANG:
        {
            if (!requireParams(1, 1, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            txXPathTreeWalker walker(aContext->getContextNode());

            nsAutoString lang;
            PRBool found;
            do {
                found = walker.getAttr(txXMLAtoms::lang, kNameSpaceID_XML,
                                       lang);
            } while (!found && walker.moveToParent());

            if (!found) {
                aContext->recycler()->getBoolResult(PR_FALSE, aResult);

                return NS_OK;
            }

            nsAutoString arg;
            evaluateToString((Expr*)iter.next(), aContext, arg);
            PRBool result = arg.Equals(Substring(lang, 0, arg.Length()),
                                       txCaseInsensitiveStringComparator()) &&
                            (lang.Length() == arg.Length() ||
                             lang.CharAt(arg.Length()) == '-');

            aContext->recycler()->getBoolResult(result, aResult);

            return NS_OK;
        }
        case TX_NOT:
        {
            if (!requireParams(1, 1, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            aContext->recycler()->getBoolResult(
                  !evaluateToBoolean((Expr*)iter.next(), aContext), aResult);

            return NS_OK;
        }
        case TX_TRUE:
        {
            if (!requireParams(0, 0, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            aContext->recycler()->getBoolResult(PR_TRUE, aResult);

            return NS_OK;
        }
        case TX_FALSE:
        {
            if (!requireParams(0, 0, aContext))
                return NS_ERROR_XPATH_BAD_ARGUMENT_COUNT;

            aContext->recycler()->getBoolResult(PR_FALSE, aResult);

            return NS_OK;
        }
    }

    aContext->receiveError(NS_LITERAL_STRING("Internal error"),
                           NS_ERROR_UNEXPECTED);
    return NS_ERROR_UNEXPECTED;
}

#ifdef TX_TO_STRING
nsresult
BooleanFunctionCall::getNameAtom(nsIAtom** aAtom)
{
    switch (mType) {
        case TX_BOOLEAN:
        {
            *aAtom = txXPathAtoms::boolean;
            break;
        }
        case TX_LANG:
        {
            *aAtom = txXPathAtoms::lang;
            break;
        }
        case TX_NOT:
        {
            *aAtom = txXPathAtoms::_not;
            break;
        }
        case TX_TRUE:
        {
            *aAtom = txXPathAtoms::_true;
            break;
        }
        case TX_FALSE:
        {
            *aAtom = txXPathAtoms::_false;
            break;
        }
        default:
        {
            *aAtom = 0;
            return NS_ERROR_FAILURE;
        }
    }
    NS_ADDREF(*aAtom);
    return NS_OK;
}
#endif
