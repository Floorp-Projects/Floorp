/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KnownLiveChecker.h"
#include "CustomMatchers.h"

void KnownLiveChecker::registerMatchers(MatchFinder *AstMatcher) {
  // Note that this cannot catch mutations after pass-by-reference, and thus no
  // error for cycle collection macros.

  auto KnownLiveLHS = hasLHS(memberExpr(hasKnownLiveAnnotation()).bind("lhs"));
  auto ForGeneralFunctions = forFunction(
      functionDecl(unless(anyOf(cxxConstructorDecl(), cxxDestructorDecl())))
          .bind("func"));

  auto Matcher =
      allOf(isAssignmentOperator(), KnownLiveLHS, ForGeneralFunctions);

  AstMatcher->addMatcher(binaryOperator(Matcher), this);
  AstMatcher->addMatcher(cxxOperatorCallExpr(Matcher), this);
}

void KnownLiveChecker::check(const MatchFinder::MatchResult &Result) {
  const char *Error = "MOZ_KNOWN_LIVE members can only be modified by "
                      "constructors and destructors";

  if (const MemberExpr *Expr = Result.Nodes.getNodeAs<MemberExpr>("lhs")) {
    diag(Expr->getBeginLoc(), Error, DiagnosticIDs::Error);
  }
  if (const CXXOperatorCallExpr *Expr =
          Result.Nodes.getNodeAs<CXXOperatorCallExpr>("lhs")) {
    diag(Expr->getBeginLoc(), Error, DiagnosticIDs::Error);
  }
}
