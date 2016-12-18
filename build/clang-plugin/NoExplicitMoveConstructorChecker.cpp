/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NoExplicitMoveConstructorChecker.h"
#include "CustomMatchers.h"

void NoExplicitMoveConstructorChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(
      cxxConstructorDecl(isExplicitMoveConstructor()).bind("node"),
      this);
}

void NoExplicitMoveConstructorChecker::check(
    const MatchFinder::MatchResult &Result) {
  // Everything we needed to know was checked in the matcher - we just report
  // the error here
  const CXXConstructorDecl *D =
      Result.Nodes.getNodeAs<CXXConstructorDecl>("node");

  diag(D->getLocation(), "Move constructors may not be marked explicit",
       DiagnosticIDs::Error);
}
