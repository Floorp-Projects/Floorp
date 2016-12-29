/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AssertAssignmentChecker.h"
#include "CustomMatchers.h"

void AssertAssignmentChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(
      callExpr(isAssertAssignmentTestFunc()).bind("funcCall"),
      this);
}

void AssertAssignmentChecker::check(
    const MatchFinder::MatchResult &Result) {
  const CallExpr *FuncCall = Result.Nodes.getNodeAs<CallExpr>("funcCall");

  if (FuncCall && hasSideEffectAssignment(FuncCall)) {
    diag(FuncCall->getLocStart(),
         "Forbidden assignment in assert expression",
         DiagnosticIDs::Error);
  }
}
