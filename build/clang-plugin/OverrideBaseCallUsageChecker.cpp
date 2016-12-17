/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OverrideBaseCallUsageChecker.h"
#include "CustomMatchers.h"

void OverrideBaseCallUsageChecker::registerMatcher(MatchFinder& AstMatcher) {
  AstMatcher.addMatcher(
      cxxMethodDecl(isNonVirtual(), isRequiredBaseMethod()).bind("method"),
      this);
}

void OverrideBaseCallUsageChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error,
      "MOZ_REQUIRED_BASE_METHOD can be used only on virtual methods");
  const CXXMethodDecl *Method = Result.Nodes.getNodeAs<CXXMethodDecl>("method");

  Diag.Report(Method->getLocation(), ErrorID);
}
