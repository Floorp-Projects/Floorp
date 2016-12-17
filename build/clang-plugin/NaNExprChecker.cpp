/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NaNExprChecker.h"
#include "CustomMatchers.h"

void NaNExprChecker::registerMatcher(MatchFinder& AstMatcher) {
  AstMatcher.addMatcher(
      binaryOperator(
          allOf(binaryEqualityOperator(),
                hasLHS(hasIgnoringParenImpCasts(
                    declRefExpr(hasType(qualType((isFloat())))).bind("lhs"))),
                hasRHS(hasIgnoringParenImpCasts(
                    declRefExpr(hasType(qualType((isFloat())))).bind("rhs"))),
                unless(anyOf(isInSystemHeader(), isInWhitelistForNaNExpr()))))
          .bind("node"),
      this);
}

void NaNExprChecker::run(
    const MatchFinder::MatchResult &Result) {
  if (!Result.Context->getLangOpts().CPlusPlus) {
    // mozilla::IsNaN is not usable in C, so there is no point in issuing these
    // warnings.
    return;
  }

  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "comparing a floating point value to itself for "
                            "NaN checking can lead to incorrect results");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "consider using mozilla::IsNaN instead");
  const BinaryOperator *Expression = Result.Nodes.getNodeAs<BinaryOperator>(
    "node");
  const DeclRefExpr *LHS = Result.Nodes.getNodeAs<DeclRefExpr>("lhs");
  const DeclRefExpr *RHS = Result.Nodes.getNodeAs<DeclRefExpr>("rhs");
  const ImplicitCastExpr *LHSExpr = dyn_cast<ImplicitCastExpr>(
    Expression->getLHS());
  const ImplicitCastExpr *RHSExpr = dyn_cast<ImplicitCastExpr>(
    Expression->getRHS());
  // The AST subtree that we are looking for will look like this:
  // -BinaryOperator ==/!=
  //  |-ImplicitCastExpr LValueToRValue
  //  | |-DeclRefExpr
  //  |-ImplicitCastExpr LValueToRValue
  //    |-DeclRefExpr
  // The check below ensures that we are dealing with the correct AST subtree
  // shape, and
  // also that both of the found DeclRefExpr's point to the same declaration.
  if (LHS->getFoundDecl() == RHS->getFoundDecl() && LHSExpr && RHSExpr &&
      std::distance(LHSExpr->child_begin(), LHSExpr->child_end()) == 1 &&
      std::distance(RHSExpr->child_begin(), RHSExpr->child_end()) == 1 &&
      *LHSExpr->child_begin() == LHS && *RHSExpr->child_begin() == RHS) {
    Diag.Report(Expression->getLocStart(), ErrorID);
    Diag.Report(Expression->getLocStart(), NoteID);
  }
}
