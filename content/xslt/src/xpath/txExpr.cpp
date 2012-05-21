/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"

nsresult
Expr::evaluateToBool(txIEvalContext* aContext, bool& aResult)
{
    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    aResult = exprRes->booleanValue();

    return NS_OK;
}

nsresult
Expr::evaluateToString(txIEvalContext* aContext, nsString& aResult)
{
    nsRefPtr<txAExprResult> exprRes;
    nsresult rv = evaluate(aContext, getter_AddRefs(exprRes));
    NS_ENSURE_SUCCESS(rv, rv);

    exprRes->stringValue(aResult);

    return NS_OK;
}
