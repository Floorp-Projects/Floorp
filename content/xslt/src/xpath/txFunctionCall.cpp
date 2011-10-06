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

#include "txExpr.h"
#include "nsIAtom.h"
#include "txIXPathContext.h"
#include "txNodeSet.h"

/**
 * This class represents a FunctionCall as defined by the XSL Working Draft
**/

  //------------------/
 //- Public Methods -/
//------------------/

/*
 * Evaluates the given Expression and converts its result to a number.
 */
// static
nsresult
FunctionCall::evaluateToNumber(Expr* aExpr, txIEvalContext* aContext,
                               double* aResult)
{
    NS_ASSERTION(aExpr, "missing expression");
    nsRefPtr<txAExprResult> exprResult;
    nsresult rv = aExpr->evaluate(aContext, getter_AddRefs(exprResult));
    NS_ENSURE_SUCCESS(rv, rv);

    *aResult = exprResult->numberValue();

    return NS_OK;
}

/*
 * Evaluates the given Expression and converts its result to a NodeSet.
 * If the result is not a NodeSet NULL is returned.
 */
nsresult
FunctionCall::evaluateToNodeSet(Expr* aExpr, txIEvalContext* aContext,
                                txNodeSet** aResult)
{
    NS_ASSERTION(aExpr, "Missing expression to evaluate");
    *aResult = nsnull;

    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = aExpr->evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    if (exprRes->getResultType() != txAExprResult::NODESET) {
        aContext->receiveError(NS_LITERAL_STRING("NodeSet expected as argument"), NS_ERROR_XSLT_NODESET_EXPECTED);
        return NS_ERROR_XSLT_NODESET_EXPECTED;
    }

    *aResult =
        static_cast<txNodeSet*>(static_cast<txAExprResult*>(exprRes));
    NS_ADDREF(*aResult);

    return NS_OK;
}

bool FunctionCall::requireParams(PRInt32 aParamCountMin,
                                   PRInt32 aParamCountMax,
                                   txIEvalContext* aContext)
{
    PRInt32 argc = mParams.Length();
    if (argc < aParamCountMin ||
        (aParamCountMax > -1 && argc > aParamCountMax)) {
        nsAutoString err(NS_LITERAL_STRING("invalid number of parameters for function"));
#ifdef TX_TO_STRING
        err.AppendLiteral(": ");
        toString(err);
#endif
        aContext->receiveError(err, NS_ERROR_XPATH_INVALID_ARG);

        return PR_FALSE;
    }

    return PR_TRUE;
}

Expr*
FunctionCall::getSubExprAt(PRUint32 aPos)
{
    return mParams.SafeElementAt(aPos);
}

void
FunctionCall::setSubExprAt(PRUint32 aPos, Expr* aExpr)
{
    NS_ASSERTION(aPos < mParams.Length(),
                 "setting bad subexpression index");
    mParams[aPos] = aExpr;
}

bool
FunctionCall::argsSensitiveTo(ContextSensitivity aContext)
{
    PRUint32 i, len = mParams.Length();
    for (i = 0; i < len; ++i) {
        if (mParams[i]->isSensitiveTo(aContext)) {
            return PR_TRUE;
        }
    }

    return PR_FALSE;
}

#ifdef TX_TO_STRING
void
FunctionCall::toString(nsAString& aDest)
{
    nsCOMPtr<nsIAtom> functionNameAtom;
    if (NS_FAILED(getNameAtom(getter_AddRefs(functionNameAtom)))) {
        NS_ERROR("Can't get function name.");
        return;
    }



    aDest.Append(nsDependentAtomString(functionNameAtom) +
                 NS_LITERAL_STRING("("));
    for (PRUint32 i = 0; i < mParams.Length(); ++i) {
        if (i != 0) {
            aDest.Append(PRUnichar(','));
        }
        mParams[i]->toString(aDest);
    }
    aDest.Append(PRUnichar(')'));
}
#endif
