/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txExpr.h"

nsresult
txLiteralExpr::evaluate(txIEvalContext* aContext, txAExprResult** aResult)
{
    NS_ENSURE_TRUE(mValue, NS_ERROR_OUT_OF_MEMORY);

    *aResult = mValue;
    NS_ADDREF(*aResult);

    return NS_OK;
}

static Expr::ResultType resultTypes[] =
{
    Expr::NODESET_RESULT, // NODESET
    Expr::BOOLEAN_RESULT, // BOOLEAN
    Expr::NUMBER_RESULT,  // NUMBER
    Expr::STRING_RESULT,  // STRING
    Expr::RTF_RESULT      // RESULT_TREE_FRAGMENT
};

Expr::ResultType
txLiteralExpr::getReturnType()
{
    return resultTypes[mValue->getResultType()];
}

Expr*
txLiteralExpr::getSubExprAt(uint32_t aPos)
{
    return nullptr;
}
void
txLiteralExpr::setSubExprAt(uint32_t aPos, Expr* aExpr)
{
    NS_NOTREACHED("setting bad subexpression index");
}

bool
txLiteralExpr::isSensitiveTo(ContextSensitivity aContext)
{
    return false;
}

#ifdef TX_TO_STRING
void
txLiteralExpr::toString(nsAString& aStr)
{
    switch (mValue->getResultType()) {
        case txAExprResult::NODESET:
        {
            aStr.AppendLiteral(" { Nodeset literal } ");
            return;
        }
        case txAExprResult::BOOLEAN:
        {
            if (mValue->booleanValue()) {
              aStr.AppendLiteral("true()");
            }
            else {
              aStr.AppendLiteral("false()");
            }
            return;
        }
        case txAExprResult::NUMBER:
        {
            txDouble::toString(mValue->numberValue(), aStr);
            return;
        }
        case txAExprResult::STRING:
        {
            StringResult* strRes =
                static_cast<StringResult*>(static_cast<txAExprResult*>
                                       (mValue));
            char16_t ch = '\'';
            if (strRes->mValue.FindChar(ch) != kNotFound) {
                ch = '\"';
            }
            aStr.Append(ch);
            aStr.Append(strRes->mValue);
            aStr.Append(ch);
            return;
        }
        case txAExprResult::RESULT_TREE_FRAGMENT:
        {
            aStr.AppendLiteral(" { RTF literal } ");
            return;
        }
    }
}
#endif
