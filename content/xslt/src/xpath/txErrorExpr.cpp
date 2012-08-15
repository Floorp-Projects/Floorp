/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsError.h"
#include "txExpr.h"
#include "nsString.h"
#include "txIXPathContext.h"

nsresult
txErrorExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    *aResult = nullptr;

    nsAutoString err(NS_LITERAL_STRING("Invalid expression evaluated"));
#ifdef TX_TO_STRING
    err.AppendLiteral(": ");
    toString(err);
#endif
    aContext->receiveError(err,
                           NS_ERROR_XPATH_INVALID_EXPRESSION_EVALUATED);

    return NS_ERROR_XPATH_INVALID_EXPRESSION_EVALUATED;
}

TX_IMPL_EXPR_STUBS_0(txErrorExpr, ANY_RESULT)

bool
txErrorExpr::isSensitiveTo(ContextSensitivity aContext)
{
    // It doesn't really matter what we return here, but it might
    // be a good idea to try to keep this as unoptimizable as possible
    return true;
}

#ifdef TX_TO_STRING
void
txErrorExpr::toString(nsAString& aStr)
{
    aStr.Append(mStr);
}
#endif
