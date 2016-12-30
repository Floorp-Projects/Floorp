/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MustUseChecker.h"
#include "CustomMatchers.h"
#include "CustomTypeAnnotation.h"

CustomTypeAnnotation MustUse =
    CustomTypeAnnotation("moz_must_use_type", "must-use");

void MustUseChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(switchCase().bind("switchcase"), this);
  AstMatcher->addMatcher(compoundStmt().bind("compound"), this);
  AstMatcher->addMatcher(ifStmt().bind("if"), this);
  AstMatcher->addMatcher(whileStmt().bind("while"), this);
  AstMatcher->addMatcher(doStmt().bind("do"), this);
  AstMatcher->addMatcher(forStmt().bind("for"), this);
  AstMatcher->addMatcher(binaryOperator(binaryCommaOperator()).bind("bin"), this);
}

void MustUseChecker::check(
    const MatchFinder::MatchResult &Result) {
  if (auto SC = Result.Nodes.getNodeAs<SwitchCase>("switchcase")) {
    handleUnusedExprResult(SC->getSubStmt());
  }
  if (auto C = Result.Nodes.getNodeAs<CompoundStmt>("compound")) {
    for (const auto& S : C->body()) {
      handleUnusedExprResult(S);
    }
  }
  if (auto IF = Result.Nodes.getNodeAs<IfStmt>("if")) {
    handleUnusedExprResult(IF->getThen());
    handleUnusedExprResult(IF->getElse());
  }
  if (auto W = Result.Nodes.getNodeAs<WhileStmt>("while")) {
    handleUnusedExprResult(W->getBody());
  }
  if (auto D = Result.Nodes.getNodeAs<DoStmt>("do")) {
    handleUnusedExprResult(D->getBody());
  }
  if (auto F = Result.Nodes.getNodeAs<ForStmt>("for")) {
    handleUnusedExprResult(F->getBody());
    handleUnusedExprResult(F->getInit());
    handleUnusedExprResult(F->getInc());
  }
  if (auto C = Result.Nodes.getNodeAs<BinaryOperator>("bin")) {
    handleUnusedExprResult(C->getLHS());
  }
}

void MustUseChecker::handleUnusedExprResult(const Stmt *Statement) {
  const Expr *E = dyn_cast_or_null<Expr>(Statement);
  if (E) {
    E = E->IgnoreImplicit(); // Ignore ExprWithCleanup etc. implicit wrappers
    QualType T = E->getType();
    if (MustUse.hasEffectiveAnnotation(T) && !isIgnoredExprForMustUse(E)) {
      diag(E->getLocStart(), "Unused value of must-use type %0",
           DiagnosticIDs::Error) << T;
      MustUse.dumpAnnotationReason(*this, T, E->getLocStart());
    }
  }
}
