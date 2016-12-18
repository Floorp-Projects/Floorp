/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NoAutoTypeChecker.h"
#include "CustomMatchers.h"

void NoAutoTypeChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(varDecl(hasType(autoNonAutoableType())).bind("node"),
                         this);
}

void NoAutoTypeChecker::check(
    const MatchFinder::MatchResult &Result) {
  const VarDecl *D = Result.Nodes.getNodeAs<VarDecl>("node");

  diag(D->getLocation(),
       "Cannot use auto to declare a variable of type %0",
       DiagnosticIDs::Error) << D->getType();
  diag(D->getLocation(),
       "Please write out this type explicitly",
       DiagnosticIDs::Note);
}
