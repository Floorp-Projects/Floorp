/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NoAddRefReleaseOnReturnChecker.h"
#include "CustomMatchers.h"

void NoAddRefReleaseOnReturnChecker::registerMatcher(MatchFinder& AstMatcher) {
  // First, look for direct parents of the MemberExpr.
  AstMatcher.addMatcher(
      callExpr(
          callee(functionDecl(hasNoAddRefReleaseOnReturnAttr()).bind("func")),
          hasParent(memberExpr(isAddRefOrRelease(), hasParent(callExpr()))
                        .bind("member")))
          .bind("node"),
      this);
  // Then, look for MemberExpr that need to be casted to the right type using
  // an intermediary CastExpr before we get to the CallExpr.
  AstMatcher.addMatcher(
      callExpr(
          callee(functionDecl(hasNoAddRefReleaseOnReturnAttr()).bind("func")),
          hasParent(castExpr(
              hasParent(memberExpr(isAddRefOrRelease(), hasParent(callExpr()))
                            .bind("member")))))
          .bind("node"),
      this);
}

void NoAddRefReleaseOnReturnChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "%1 cannot be called on the return value of %0");
  const Stmt *Node = Result.Nodes.getNodeAs<Stmt>("node");
  const FunctionDecl *Func = Result.Nodes.getNodeAs<FunctionDecl>("func");
  const MemberExpr *Member = Result.Nodes.getNodeAs<MemberExpr>("member");
  const CXXMethodDecl *Method =
      dyn_cast<CXXMethodDecl>(Member->getMemberDecl());

  Diag.Report(Node->getLocStart(), ErrorID) << Func << Method;
}
