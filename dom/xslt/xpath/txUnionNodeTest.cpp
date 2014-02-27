/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/FloatingPoint.h"

#include "txExpr.h"
#include "txExprResult.h"
#include "txSingleNodeContext.h"

bool
txUnionNodeTest::matches(const txXPathNode& aNode,
                         txIMatchContext* aContext)
{
    uint32_t i, len = mNodeTests.Length();
    for (i = 0; i < len; ++i) {
        if (mNodeTests[i]->matches(aNode, aContext)) {
            return true;
        }
    }

    return false;
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
    aDest.AppendLiteral("(");
    for (uint32_t i = 0; i < mNodeTests.Length(); ++i) {
        if (i != 0) {
            aDest.AppendLiteral(" | ");
        }
        mNodeTests[i]->toString(aDest);
    }
    aDest.AppendLiteral(")");
}
#endif
