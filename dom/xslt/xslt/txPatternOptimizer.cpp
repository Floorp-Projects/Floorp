/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "txPatternOptimizer.h"
#include "txXSLTPatterns.h"

nsresult
txPatternOptimizer::optimize(txPattern* aInPattern, txPattern** aOutPattern)
{
    *aOutPattern = nullptr;
    nsresult rv = NS_OK;

    // First optimize sub expressions
    uint32_t i = 0;
    Expr* subExpr;
    while ((subExpr = aInPattern->getSubExprAt(i))) {
        Expr* newExpr = nullptr;
        rv = mXPathOptimizer.optimize(subExpr, &newExpr);
        NS_ENSURE_SUCCESS(rv, rv);
        if (newExpr) {
            delete subExpr;
            aInPattern->setSubExprAt(i, newExpr);
        }

        ++i;
    }

    // Then optimize sub patterns
    txPattern* subPattern;
    i = 0;
    while ((subPattern = aInPattern->getSubPatternAt(i))) {
        txPattern* newPattern = nullptr;
        rv = optimize(subPattern, &newPattern);
        NS_ENSURE_SUCCESS(rv, rv);
        if (newPattern) {
            delete subPattern;
            aInPattern->setSubPatternAt(i, newPattern);
        }

        ++i;
    }

    // Finally see if current pattern can be optimized
    switch (aInPattern->getType()) {
        case txPattern::STEP_PATTERN:
            return optimizeStep(aInPattern, aOutPattern);

        default:
            break;
    }

    return NS_OK;
}


nsresult
txPatternOptimizer::optimizeStep(txPattern* aInPattern,
                                 txPattern** aOutPattern)
{
    txStepPattern* step = static_cast<txStepPattern*>(aInPattern);

    // Test for predicates that can be combined into the nodetest
    Expr* pred;
    while ((pred = step->getSubExprAt(0)) &&
           !pred->canReturnType(Expr::NUMBER_RESULT) &&
           !pred->isSensitiveTo(Expr::NODESET_CONTEXT)) {
        txNodeTest* predTest = new txPredicatedNodeTest(step->getNodeTest(),
                                                        pred);
        step->dropFirst();
        step->setNodeTest(predTest);
    }

    return NS_OK;
}
