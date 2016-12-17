/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ArithmeticArgChecker.h"
#include "CustomMatchers.h"

void ArithmeticArgChecker::registerMatcher(MatchFinder& AstMatcher) {
  AstMatcher.addMatcher(
      callExpr(allOf(hasDeclaration(noArithmeticExprInArgs()),
                     anyOf(hasDescendant(
                               binaryOperator(
                                   allOf(binaryArithmeticOperator(),
                                         hasLHS(hasDescendant(declRefExpr())),
                                         hasRHS(hasDescendant(declRefExpr()))))
                                   .bind("node")),
                           hasDescendant(
                               unaryOperator(
                                   allOf(unaryArithmeticOperator(),
                                         hasUnaryOperand(allOf(
                                             hasType(builtinType()),
                                             anyOf(hasDescendant(declRefExpr()),
                                                   declRefExpr())))))
                                   .bind("node")))))
          .bind("call"),
      this);
  AstMatcher.addMatcher(
      cxxConstructExpr(
          allOf(hasDeclaration(noArithmeticExprInArgs()),
                anyOf(hasDescendant(
                          binaryOperator(
                              allOf(binaryArithmeticOperator(),
                                    hasLHS(hasDescendant(declRefExpr())),
                                    hasRHS(hasDescendant(declRefExpr()))))
                              .bind("node")),
                      hasDescendant(
                          unaryOperator(
                              allOf(unaryArithmeticOperator(),
                                    hasUnaryOperand(allOf(
                                        hasType(builtinType()),
                                        anyOf(hasDescendant(declRefExpr()),
                                              declRefExpr())))))
                              .bind("node")))))
          .bind("call"),
      this);
}

void ArithmeticArgChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "cannot pass an arithmetic expression of built-in types to %0");
  const Expr *Expression = Result.Nodes.getNodeAs<Expr>("node");
  if (const CallExpr *Call = Result.Nodes.getNodeAs<CallExpr>("call")) {
    Diag.Report(Expression->getLocStart(), ErrorID) << Call->getDirectCallee();
  } else if (const CXXConstructExpr *Ctr =
                 Result.Nodes.getNodeAs<CXXConstructExpr>("call")) {
    Diag.Report(Expression->getLocStart(), ErrorID) << Ctr->getConstructor();
  }
}
