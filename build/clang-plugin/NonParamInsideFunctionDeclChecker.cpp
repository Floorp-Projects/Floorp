/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NonParamInsideFunctionDeclChecker.h"
#include "CustomMatchers.h"

void NonParamInsideFunctionDeclChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(
      functionDecl(anyOf(allOf(isDefinition(),
                               hasAncestor(classTemplateSpecializationDecl()
                                               .bind("spec"))),
                         isDefinition()))
          .bind("func"),
      this);
  AstMatcher->addMatcher(
      lambdaExpr().bind("lambda"),
      this);
}

void NonParamInsideFunctionDeclChecker::check(
    const MatchFinder::MatchResult &Result) {
  static DenseSet<const FunctionDecl*> CheckedFunctionDecls;

  const FunctionDecl *func = Result.Nodes.getNodeAs<FunctionDecl>("func");
  if (!func) {
    const LambdaExpr *lambda = Result.Nodes.getNodeAs<LambdaExpr>("lambda");
    if (lambda) {
      func = lambda->getCallOperator();
    }
  }

  if (!func) {
    return;
  }

  if (func->isDeleted()) {
    return;
  }

  // Don't report errors on the same declarations more than once.
  if (CheckedFunctionDecls.count(func)) {
    return;
  }
  CheckedFunctionDecls.insert(func);

  const ClassTemplateSpecializationDecl *Spec =
      Result.Nodes.getNodeAs<ClassTemplateSpecializationDecl>("spec");

  for (ParmVarDecl *p : func->parameters()) {
    QualType T = p->getType().withoutLocalFastQualifiers();
    if (NonParam.hasEffectiveAnnotation(T)) {
      diag(p->getLocation(), "Type %0 must not be used as parameter",
           DiagnosticIDs::Error) << T;
      diag(p->getLocation(), "Please consider passing a const reference instead",
           DiagnosticIDs::Note);

      if (Spec) {
        diag(Spec->getPointOfInstantiation(),
             "The bad argument was passed to %0 here",
             DiagnosticIDs::Note)
          << Spec->getSpecializedTemplate();
      }
    }
  }
}
