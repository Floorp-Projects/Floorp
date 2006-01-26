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

#include "txXPathOptimizer.h"
#include "txExprResult.h"
#include "nsIAtom.h"
#include "txAtoms.h"
#include "txXPathNode.h"
#include "txExpr.h"

nsresult
txXPathOptimizer::optimize(Expr* aInExpr, Expr** aOutExpr)
{
    *aOutExpr = nsnull;
    nsresult rv = NS_OK;

    // First optimize sub expressions
    PRUint32 i = 0;
    Expr* subExpr;
    while ((subExpr = aInExpr->getSubExprAt(i))) {
        Expr* newExpr = nsnull;
        rv = optimize(subExpr, &newExpr);
        NS_ENSURE_SUCCESS(rv, rv);
        if (newExpr) {
            delete subExpr;
            aInExpr->setSubExprAt(i, newExpr);
        }

        ++i;
    }

    // Then see if current expression can be optimized
    switch (aInExpr->getType()) {
        case Expr::LOCATIONSTEP_ATTRIBUTE_EXPR:
            return optimizeAttributeStep(aInExpr, aOutExpr);

        case Expr::LOCATIONSTEP_OTHER_EXPR:
            return optimizeStep(aInExpr, aOutExpr);

        default:
            break;
    }

    return NS_OK;
}

nsresult
txXPathOptimizer::optimizeAttributeStep(Expr* aInExpr, Expr** aOutExpr)
{
    LocationStep* step = NS_STATIC_CAST(LocationStep*, aInExpr);

    // Test for @foo type steps.
    txNameTest* nameTest = nsnull;
    if (!step->getSubExprAt(0) &&
        step->getNodeTest()->getType() == txNameTest::NAME_TEST &&
        (nameTest = NS_STATIC_CAST(txNameTest*, step->getNodeTest()))->
            mLocalName != txXPathAtoms::_asterix) {

        *aOutExpr = new txNamedAttributeStep(nameTest->mNamespace,
                                             nameTest->mPrefix,
                                             nameTest->mLocalName);
        NS_ENSURE_TRUE(*aOutExpr, NS_ERROR_OUT_OF_MEMORY);

        return NS_OK; // return since we no longer have a step-object.
    }

    // Do general step optimizations

    return optimizeStep(aInExpr, aOutExpr);
}


nsresult
txXPathOptimizer::optimizeStep(Expr* aInExpr, Expr** aOutExpr)
{
    LocationStep* step = NS_STATIC_CAST(LocationStep*, aInExpr);

    // Test for predicates that can be combined into the nodetest
    Expr* pred;
    while ((pred = step->getSubExprAt(0)) &&
           !pred->canReturnType(Expr::NUMBER_RESULT) &&
           !pred->isSensitiveTo(Expr::NODESET_CONTEXT)) {
        txNodeTest* predTest = new txPredicatedNodeTest(step->getNodeTest(), pred);
        NS_ENSURE_TRUE(predTest, NS_ERROR_OUT_OF_MEMORY);

        step->dropFirst();
        step->setNodeTest(predTest);
    }

    return NS_OK;
}
