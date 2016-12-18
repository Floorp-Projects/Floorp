/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ArithmeticArgChecker.h"
#include "CustomMatchers.h"

void ArithmeticArgChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(
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
  AstMatcher->addMatcher(
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

void ArithmeticArgChecker::check(
    const MatchFinder::MatchResult &Result) {
  const char* Error = "cannot pass an arithmetic expression of built-in types to %0";
  const Expr *Expression = Result.Nodes.getNodeAs<Expr>("node");
  if (const CallExpr *Call = Result.Nodes.getNodeAs<CallExpr>("call")) {
    diag(Expression->getLocStart(), Error, DiagnosticIDs::Error) << Call->getDirectCallee();
  } else if (const CXXConstructExpr *Ctr =
                 Result.Nodes.getNodeAs<CXXConstructExpr>("call")) {
    diag(Expression->getLocStart(), Error, DiagnosticIDs::Error) << Ctr->getConstructor();
  }
}
