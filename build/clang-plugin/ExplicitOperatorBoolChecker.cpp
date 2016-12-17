/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExplicitOperatorBoolChecker.h"
#include "CustomMatchers.h"

void ExplicitOperatorBoolChecker::registerMatcher(MatchFinder& AstMatcher) {
  // Older clang versions such as the ones used on the infra recognize these
  // conversions as 'operator _Bool', but newer clang versions recognize these
  // as 'operator bool'.
  AstMatcher.addMatcher(
      cxxMethodDecl(anyOf(hasName("operator bool"), hasName("operator _Bool")))
          .bind("node"),
      this);
}

void ExplicitOperatorBoolChecker::run(
    const MatchFinder::MatchResult &Result) {
  DiagnosticsEngine &Diag = Result.Context->getDiagnostics();
  unsigned ErrorID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Error, "bad implicit conversion operator for %0");
  unsigned NoteID = Diag.getDiagnosticIDs()->getCustomDiagID(
      DiagnosticIDs::Note, "consider adding the explicit keyword to %0");
  const CXXConversionDecl *Method =
      Result.Nodes.getNodeAs<CXXConversionDecl>("node");
  const CXXRecordDecl *Clazz = Method->getParent();

  if (!Method->isExplicitSpecified() &&
      !MozChecker::hasCustomAnnotation(Method, "moz_implicit") &&
      !ASTIsInSystemHeader(Method->getASTContext(), *Method) &&
      isInterestingDeclForImplicitConversion(Method)) {
    Diag.Report(Method->getLocStart(), ErrorID) << Clazz;
    Diag.Report(Method->getLocStart(), NoteID) << "'operator bool'";
  }
}
