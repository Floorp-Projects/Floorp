/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NonParamInsideFunctionDeclChecker.h"
#include "CustomMatchers.h"

void NonParamInsideFunctionDeclChecker::registerMatcher(MatchFinder& AstMatcher) {
  AstMatcher.addMatcher(
      functionDecl(anyOf(allOf(isDefinition(),
                               hasAncestor(classTemplateSpecializationDecl()
                                               .bind("spec"))),
                         isDefinition()))
          .bind("func"),
      this);
  AstMatcher.addMatcher(
      lambdaExpr().bind("lambda"),
      this);
}

void NonParamInsideFunctionDeclChecker::run(
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

  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "Type %0 must not be used as parameter");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "Please consider passing a const reference instead");
  unsigned SpecNoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "The bad argument was passed to %0 here");

  for (ParmVarDecl *p : func->parameters()) {
    QualType T = p->getType().withoutLocalFastQualifiers();
    if (NonParam.hasEffectiveAnnotation(T)) {
      Diag.Report(p->getLocation(), ErrorID) << T;
      Diag.Report(p->getLocation(), NoteID);

      if (Spec) {
        Diag.Report(Spec->getPointOfInstantiation(), SpecNoteID)
          << Spec->getSpecializedTemplate();
      }
    }
  }
}
