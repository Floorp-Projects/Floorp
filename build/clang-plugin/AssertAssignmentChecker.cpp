/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AssertAssignmentChecker.h"
#include "CustomMatchers.h"

void AssertAssignmentChecker::registerMatcher(MatchFinder& AstMatcher) {
  AstMatcher.addMatcher(
      callExpr(isAssertAssignmentTestFunc()).bind("funcCall"),
      this);
}

void AssertAssignmentChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned AssignInsteadOfComp = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "Forbidden assignment in assert expression");
  const CallExpr *FuncCall = Result.Nodes.getNodeAs<CallExpr>("funcCall");

  if (FuncCall && hasSideEffectAssignment(FuncCall)) {
    Diag.Report(FuncCall->getLocStart(), AssignInsteadOfComp);
  }
}
