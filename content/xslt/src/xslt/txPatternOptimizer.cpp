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
 * The Original Code is TransforMiiX XSLT processor.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   IBM Corporation
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

#include "txPatternOptimizer.h"
#include "txXSLTPatterns.h"

nsresult
txPatternOptimizer::optimize(txPattern* aInPattern, txPattern** aOutPattern)
{
    *aOutPattern = nsnull;
    nsresult rv = NS_OK;

    // First optimize sub expressions
    PRUint32 i = 0;
    Expr* subExpr;
    while ((subExpr = aInPattern->getSubExprAt(i))) {
        Expr* newExpr = nsnull;
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
        txPattern* newPattern = nsnull;
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
