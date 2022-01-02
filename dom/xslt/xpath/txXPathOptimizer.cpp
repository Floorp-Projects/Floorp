/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "txXPathOptimizer.h"
#include "txExprResult.h"
#include "nsAtom.h"
#include "nsGkAtoms.h"
#include "txXPathNode.h"
#include "txExpr.h"
#include "txIXPathContext.h"

using mozilla::UniquePtr;
using mozilla::Unused;

class txEarlyEvalContext : public txIEvalContext {
 public:
  explicit txEarlyEvalContext(txResultRecycler* aRecycler)
      : mRecycler(aRecycler) {}

  // txIEvalContext
  nsresult getVariable(int32_t aNamespace, nsAtom* aLName,
                       txAExprResult*& aResult) override {
    MOZ_CRASH("shouldn't depend on this context");
  }
  nsresult isStripSpaceAllowed(const txXPathNode& aNode,
                               bool& aAllowed) override {
    MOZ_CRASH("shouldn't depend on this context");
  }
  void* getPrivateContext() override {
    MOZ_CRASH("shouldn't depend on this context");
  }
  txResultRecycler* recycler() override { return mRecycler; }
  void receiveError(const nsAString& aMsg, nsresult aRes) override {}
  const txXPathNode& getContextNode() override {
    MOZ_CRASH("shouldn't depend on this context");
  }
  uint32_t size() override { MOZ_CRASH("shouldn't depend on this context"); }
  uint32_t position() override {
    MOZ_CRASH("shouldn't depend on this context");
  }

 private:
  txResultRecycler* mRecycler;
};

void txXPathOptimizer::optimize(Expr* aInExpr, Expr** aOutExpr) {
  *aOutExpr = nullptr;

  // First check if the expression will produce the same result
  // under any context.
  Expr::ExprType exprType = aInExpr->getType();
  if (exprType != Expr::LITERAL_EXPR &&
      !aInExpr->isSensitiveTo(Expr::ANY_CONTEXT)) {
    RefPtr<txResultRecycler> recycler = new txResultRecycler;
    txEarlyEvalContext context(recycler);
    RefPtr<txAExprResult> exprRes;

    // Don't throw if this fails since it could be that the expression
    // is or contains an error-expression.
    nsresult rv = aInExpr->evaluate(&context, getter_AddRefs(exprRes));
    if (NS_SUCCEEDED(rv)) {
      *aOutExpr = new txLiteralExpr(exprRes);
    }

    return;
  }

  // Then optimize sub expressions
  uint32_t i = 0;
  Expr* subExpr;
  while ((subExpr = aInExpr->getSubExprAt(i))) {
    Expr* newExpr = nullptr;
    optimize(subExpr, &newExpr);
    if (newExpr) {
      delete subExpr;
      aInExpr->setSubExprAt(i, newExpr);
    }

    ++i;
  }

  // Finally see if current expression can be optimized
  switch (exprType) {
    case Expr::LOCATIONSTEP_EXPR:
      optimizeStep(aInExpr, aOutExpr);
      return;

    case Expr::PATH_EXPR:
      optimizePath(aInExpr, aOutExpr);
      return;

    case Expr::UNION_EXPR:
      optimizeUnion(aInExpr, aOutExpr);
      return;

    default:
      return;
  }
}

void txXPathOptimizer::optimizeStep(Expr* aInExpr, Expr** aOutExpr) {
  LocationStep* step = static_cast<LocationStep*>(aInExpr);

  if (step->getAxisIdentifier() == LocationStep::ATTRIBUTE_AXIS) {
    // Test for @foo type steps.
    txNameTest* nameTest = nullptr;
    if (!step->getSubExprAt(0) &&
        step->getNodeTest()->getType() == txNameTest::NAME_TEST &&
        (nameTest = static_cast<txNameTest*>(step->getNodeTest()))
                ->mLocalName != nsGkAtoms::_asterisk) {
      *aOutExpr = new txNamedAttributeStep(
          nameTest->mNamespace, nameTest->mPrefix, nameTest->mLocalName);
      return;  // return since we no longer have a step-object.
    }
  }

  // Test for predicates that can be combined into the nodetest
  Expr* pred;
  while ((pred = step->getSubExprAt(0)) &&
         !pred->canReturnType(Expr::NUMBER_RESULT) &&
         !pred->isSensitiveTo(Expr::NODESET_CONTEXT)) {
    txNodeTest* predTest = new txPredicatedNodeTest(step->getNodeTest(), pred);
    step->dropFirst();
    step->setNodeTest(predTest);
  }
}

void txXPathOptimizer::optimizePath(Expr* aInExpr, Expr** aOutExpr) {
  PathExpr* path = static_cast<PathExpr*>(aInExpr);

  uint32_t i;
  Expr* subExpr;
  // look for steps like "//foo" that can be turned into "/descendant::foo"
  // and "//." that can be turned into "/descendant-or-self::node()"
  for (i = 0; (subExpr = path->getSubExprAt(i)); ++i) {
    if (path->getPathOpAt(i) == PathExpr::DESCENDANT_OP &&
        subExpr->getType() == Expr::LOCATIONSTEP_EXPR &&
        !subExpr->getSubExprAt(0)) {
      LocationStep* step = static_cast<LocationStep*>(subExpr);
      if (step->getAxisIdentifier() == LocationStep::CHILD_AXIS) {
        step->setAxisIdentifier(LocationStep::DESCENDANT_AXIS);
        path->setPathOpAt(i, PathExpr::RELATIVE_OP);
      } else if (step->getAxisIdentifier() == LocationStep::SELF_AXIS) {
        step->setAxisIdentifier(LocationStep::DESCENDANT_OR_SELF_AXIS);
        path->setPathOpAt(i, PathExpr::RELATIVE_OP);
      }
    }
  }

  // look for expressions that start with a "./"
  subExpr = path->getSubExprAt(0);
  LocationStep* step;
  if (subExpr->getType() == Expr::LOCATIONSTEP_EXPR && path->getSubExprAt(1) &&
      path->getPathOpAt(1) != PathExpr::DESCENDANT_OP) {
    step = static_cast<LocationStep*>(subExpr);
    if (step->getAxisIdentifier() == LocationStep::SELF_AXIS &&
        !step->getSubExprAt(0)) {
      txNodeTest* test = step->getNodeTest();
      if (test->getType() == txNodeTest::NODETYPE_TEST &&
          (static_cast<txNodeTypeTest*>(test))->getNodeTestType() ==
              txNodeTypeTest::NODE_TYPE) {
        // We have a '.' as first step followed by a single '/'.

        // Check if there are only two steps. If so, return the second
        // as resulting expression.
        if (!path->getSubExprAt(2)) {
          *aOutExpr = path->getSubExprAt(1);
          path->setSubExprAt(1, nullptr);

          return;
        }

        // Just delete the '.' step and leave the rest of the PathExpr
        path->deleteExprAt(0);
      }
    }
  }
}

void txXPathOptimizer::optimizeUnion(Expr* aInExpr, Expr** aOutExpr) {
  UnionExpr* uni = static_cast<UnionExpr*>(aInExpr);

  // Check for expressions like "foo | bar" and
  // "descendant::foo | descendant::bar"

  uint32_t current;
  Expr* subExpr;
  for (current = 0; (subExpr = uni->getSubExprAt(current)); ++current) {
    if (subExpr->getType() != Expr::LOCATIONSTEP_EXPR ||
        subExpr->getSubExprAt(0)) {
      continue;
    }

    LocationStep* currentStep = static_cast<LocationStep*>(subExpr);
    LocationStep::LocationStepType axis = currentStep->getAxisIdentifier();

    txUnionNodeTest* unionTest = nullptr;

    // Check if there are any other steps with the same axis and merge
    // them with currentStep
    uint32_t i;
    for (i = current + 1; (subExpr = uni->getSubExprAt(i)); ++i) {
      if (subExpr->getType() != Expr::LOCATIONSTEP_EXPR ||
          subExpr->getSubExprAt(0)) {
        continue;
      }

      LocationStep* step = static_cast<LocationStep*>(subExpr);
      if (step->getAxisIdentifier() != axis) {
        continue;
      }

      // Create a txUnionNodeTest if needed
      if (!unionTest) {
        UniquePtr<txNodeTest> owner(unionTest = new txUnionNodeTest);
        unionTest->addNodeTest(currentStep->getNodeTest());

        currentStep->setNodeTest(unionTest);
        Unused << owner.release();
      }

      // Merge the nodetest into the union
      unionTest->addNodeTest(step->getNodeTest());

      step->setNodeTest(nullptr);

      // Remove the step from the UnionExpr
      uni->deleteExprAt(i);
      --i;
    }

    // Check if all expressions were merged into a single step. If so,
    // return the step as the new expression.
    if (unionTest && current == 0 && !uni->getSubExprAt(1)) {
      // Make sure the step doesn't get deleted when the UnionExpr is
      uni->setSubExprAt(0, nullptr);
      *aOutExpr = currentStep;

      // Return right away since we no longer have a union
      return;
    }
  }
}
