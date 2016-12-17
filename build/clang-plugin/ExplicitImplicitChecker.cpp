/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExplicitImplicitChecker.h"
#include "CustomMatchers.h"

void ExplicitImplicitChecker::registerMatcher(MatchFinder& AstMatcher) {
  AstMatcher.addMatcher(cxxConstructorDecl(isInterestingImplicitCtor(),
                                           ofClass(allOf(isConcreteClass(),
                                                         decl().bind("class"))),
                                           unless(isMarkedImplicit()))
                            .bind("ctor"),
                        this);
}

void ExplicitImplicitChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "bad implicit conversion constructor for %0");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note,
      "consider adding the explicit keyword to the constructor");

  // We've already checked everything in the matcher, so we just have to report
  // the error.

  const CXXConstructorDecl *Ctor =
      Result.Nodes.getNodeAs<CXXConstructorDecl>("ctor");
  const CXXRecordDecl *Declaration =
      Result.Nodes.getNodeAs<CXXRecordDecl>("class");

  Diag.Report(Ctor->getLocation(), ErrorID) << Declaration->getDeclName();
  Diag.Report(Ctor->getLocation(), NoteID);
}
