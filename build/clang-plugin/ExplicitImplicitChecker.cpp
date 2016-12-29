/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExplicitImplicitChecker.h"
#include "CustomMatchers.h"

void ExplicitImplicitChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(cxxConstructorDecl(isInterestingImplicitCtor(),
                                            ofClass(allOf(isConcreteClass(),
                                                          decl().bind("class"))),
                                            unless(isMarkedImplicit()))
                            .bind("ctor"),
                        this);
}

void ExplicitImplicitChecker::check(
    const MatchFinder::MatchResult &Result) {
  // We've already checked everything in the matcher, so we just have to report
  // the error.

  const CXXConstructorDecl *Ctor =
      Result.Nodes.getNodeAs<CXXConstructorDecl>("ctor");
  const CXXRecordDecl *Declaration =
      Result.Nodes.getNodeAs<CXXRecordDecl>("class");

  diag(Ctor->getLocation(), "bad implicit conversion constructor for %0",
       DiagnosticIDs::Error) << Declaration->getDeclName();
  diag(Ctor->getLocation(), "consider adding the explicit keyword to the constructor",
       DiagnosticIDs::Note);
}
