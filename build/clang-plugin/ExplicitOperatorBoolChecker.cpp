/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExplicitOperatorBoolChecker.h"
#include "CustomMatchers.h"

void ExplicitOperatorBoolChecker::registerMatchers(MatchFinder *AstMatcher) {
  AstMatcher->addMatcher(
      cxxMethodDecl(allOf(isFirstParty(), hasName("operator bool")))
          .bind("node"),
      this);
}

void ExplicitOperatorBoolChecker::check(
    const MatchFinder::MatchResult &Result) {
  const CXXConversionDecl *Method =
      Result.Nodes.getNodeAs<CXXConversionDecl>("node");
  const CXXRecordDecl *Clazz = Method->getParent();

  if (!Method->isExplicit() && !hasCustomAttribute<moz_implicit>(Method) &&
      !ASTIsInSystemHeader(Method->getASTContext(), *Method) &&
      isInterestingDeclForImplicitConversion(Method)) {
    diag(Method->getBeginLoc(), "bad implicit conversion operator for %0",
         DiagnosticIDs::Error)
        << Clazz;
    diag(Method->getBeginLoc(), "consider adding the explicit keyword to %0",
         DiagnosticIDs::Note)
        << "'operator bool'";
  }
}
