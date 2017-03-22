/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"

#include "txExpr.h"
#include "txExprResult.h"
#include "txSingleNodeContext.h"

nsresult
txUnionNodeTest::matches(const txXPathNode& aNode, txIMatchContext* aContext,
                         bool& aMatched)
{
    uint32_t i, len = mNodeTests.Length();
    for (i = 0; i < len; ++i) {
        nsresult rv = mNodeTests[i]->matches(aNode, aContext, aMatched);
        NS_ENSURE_SUCCESS(rv, rv);

        if (aMatched) {
            return NS_OK;
        }
    }

    aMatched = false;
    return NS_OK;
}

double
txUnionNodeTest::getDefaultPriority()
{
    NS_ERROR("Don't call getDefaultPriority on txUnionPattern");
    return mozilla::UnspecifiedNaN<double>();
}

bool
txUnionNodeTest::isSensitiveTo(Expr::ContextSensitivity aContext)
{
    uint32_t i, len = mNodeTests.Length();
    for (i = 0; i < len; ++i) {
        if (mNodeTests[i]->isSensitiveTo(aContext)) {
            return true;
        }
    }

    return false;
}

#ifdef TX_TO_STRING
void
txUnionNodeTest::toString(nsAString& aDest)
{
    aDest.Append('(');
    for (uint32_t i = 0; i < mNodeTests.Length(); ++i) {
        if (i != 0) {
            aDest.AppendLiteral(" | ");
        }
        mNodeTests[i]->toString(aDest);
    }
    aDest.Append(')');
}
#endif
