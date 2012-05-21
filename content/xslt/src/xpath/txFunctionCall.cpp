/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

        return false;
    }

    return true;
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
            return true;
        }
    }

    return false;
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
