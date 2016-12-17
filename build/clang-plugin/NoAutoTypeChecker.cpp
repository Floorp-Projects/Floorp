/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NoAutoTypeChecker.h"
#include "CustomMatchers.h"

void NoAutoTypeChecker::registerMatcher(MatchFinder& AstMatcher) {
  AstMatcher.addMatcher(varDecl(hasType(autoNonAutoableType())).bind("node"),
                        this);
}

void NoAutoTypeChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "Cannot use auto to declare a variable of type %0");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "Please write out this type explicitly");

  const VarDecl *D = Result.Nodes.getNodeAs<VarDecl>("node");

  Diag.Report(D->getLocation(), ErrorID) << D->getType();
  Diag.Report(D->getLocation(), NoteID);
}
