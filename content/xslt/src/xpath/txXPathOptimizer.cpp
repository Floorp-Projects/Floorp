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
        case Expr::LOCATIONSTEP_EXPR:
            return optimizeStep(aInExpr, aOutExpr);

        case Expr::PATH_EXPR:
            return optimizePath(aInExpr, aOutExpr);

        case Expr::UNION_EXPR:
            return optimizeUnion(aInExpr, aOutExpr);

        default:
            break;
    }

    return NS_OK;
}

nsresult
txXPathOptimizer::optimizeStep(Expr* aInExpr, Expr** aOutExpr)
{
    LocationStep* step = NS_STATIC_CAST(LocationStep*, aInExpr);

    if (step->getAxisIdentifier() == LocationStep::ATTRIBUTE_AXIS) {
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
    }

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

nsresult
txXPathOptimizer::optimizePath(Expr* aInExpr, Expr** aOutExpr)
{
    PathExpr* path = NS_STATIC_CAST(PathExpr*, aInExpr);

    PRUint32 i;
    Expr* subExpr;
    // look for steps like "//foo" that can be turned into "/descendant::foo"
    // and "//." that can be turned into "/descendant-or-self::node()"
    for (i = 0; (subExpr = path->getSubExprAt(i)); ++i) {
        if (path->getPathOpAt(i) == PathExpr::DESCENDANT_OP &&
            subExpr->getType() == Expr::LOCATIONSTEP_EXPR &&
            !subExpr->getSubExprAt(0)) {
            LocationStep* step = NS_STATIC_CAST(LocationStep*, aInExpr);
            if (step->getAxisIdentifier() == LocationStep::CHILD_AXIS) {
                step->setAxisIdentifier(LocationStep::DESCENDANT_AXIS);
                path->setPathOpAt(i, PathExpr::RELATIVE_OP);
            }
            else if (step->getAxisIdentifier() == LocationStep::SELF_AXIS) {
                step->setAxisIdentifier(LocationStep::DESCENDANT_OR_SELF_AXIS);
                path->setPathOpAt(i, PathExpr::RELATIVE_OP);
            }
        }
    }

    // look for expressions that start with a "./"
    subExpr = path->getSubExprAt(0);
    LocationStep* step;
    if (subExpr->getType() == Expr::LOCATIONSTEP_EXPR &&
        path->getSubExprAt(1) &&
        path->getPathOpAt(1) != PathExpr::DESCENDANT_OP) {
        step = NS_STATIC_CAST(LocationStep*, subExpr);
        if (step->getAxisIdentifier() == LocationStep::SELF_AXIS &&
            !step->getSubExprAt(0)) {
            txNodeTest* test = step->getNodeTest();
            txNodeTypeTest* typeTest;
            if (test->getType() == txNodeTest::NODETYPE_TEST &&
                (typeTest = NS_STATIC_CAST(txNodeTypeTest*, test))->
                  getNodeTestType() == txNodeTypeTest::NODE_TYPE) {
                // We have a '.' as first step followed by a single '/'.

                // Check if there are only two steps. If so, return the second
                // as resulting expression.
                if (!path->getSubExprAt(2)) {
                    *aOutExpr = path->getSubExprAt(1);
                    path->setSubExprAt(1, nsnull);

                    return NS_OK;
                }

                // Just delete the '.' step and leave the rest of the PathExpr
                path->deleteExprAt(0);
            }
        }
    }

    return NS_OK;
}

nsresult
txXPathOptimizer::optimizeUnion(Expr* aInExpr, Expr** aOutExpr)
{
    UnionExpr* uni = NS_STATIC_CAST(UnionExpr*, aInExpr);

    // Check for expressions like "foo | bar" and
    // "descendant::foo | descendant::bar"

    nsresult rv;
    PRUint32 current;
    Expr* subExpr;
    for (current = 0; (subExpr = uni->getSubExprAt(current)); ++current) {
        if (subExpr->getType() != Expr::LOCATIONSTEP_EXPR ||
            subExpr->getSubExprAt(0)) {
            continue;
        }

        LocationStep* currentStep = NS_STATIC_CAST(LocationStep*, subExpr);
        LocationStep::LocationStepType axis = currentStep->getAxisIdentifier();

        txUnionNodeTest* unionTest = nsnull;

        // Check if there are any other steps with the same axis and merge
        // them with currentStep
        PRUint32 i;
        for (i = current + 1; (subExpr = uni->getSubExprAt(i)); ++i) {
            if (subExpr->getType() != Expr::LOCATIONSTEP_EXPR ||
                subExpr->getSubExprAt(0)) {
                continue;
            }

            LocationStep* step = NS_STATIC_CAST(LocationStep*, subExpr);
            if (step->getAxisIdentifier() != axis) {
                continue;
            }
            
            // Create a txUnionNodeTest if needed
            if (!unionTest) {
                unionTest = new txUnionNodeTest;
                NS_ENSURE_TRUE(unionTest, NS_ERROR_OUT_OF_MEMORY);
                
                rv = unionTest->addNodeTest(currentStep->getNodeTest());
                currentStep->setNodeTest(unionTest);
                NS_ENSURE_SUCCESS(rv, rv);
            }

            // Merge the nodetest into the union
            rv = unionTest->addNodeTest(step->getNodeTest());
            step->setNodeTest(nsnull);
            NS_ENSURE_SUCCESS(rv, rv);

            // Remove the step from the UnionExpr
            uni->deleteExprAt(i);
            --i;
        }

        // Check if all expressions were merged into a single step. If so,
        // return the step as the new expression.
        if (unionTest && current == 0 && !uni->getSubExprAt(1)) {
            // Make sure the step doesn't get deleted when the UnionExpr is
            uni->setSubExprAt(0, nsnull);
            *aOutExpr = currentStep;

            // Return right away since we no longer have a union            
            return NS_OK;
        }
    }

    return NS_OK;
}
