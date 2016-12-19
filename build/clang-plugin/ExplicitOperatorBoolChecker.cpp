/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExplicitOperatorBoolChecker.h"
#include "CustomMatchers.h"

void ExplicitOperatorBoolChecker::registerMatchers(MatchFinder* AstMatcher) {
  // Older clang versions such as the ones used on the infra recognize these
  // conversions as 'operator _Bool', but newer clang versions recognize these
  // as 'operator bool'.
  AstMatcher->addMatcher(
      cxxMethodDecl(anyOf(hasName("operator bool"), hasName("operator _Bool")))
          .bind("node"),
      this);
}

void ExplicitOperatorBoolChecker::check(
    const MatchFinder::MatchResult &Result) {
  const CXXConversionDecl *Method =
      Result.Nodes.getNodeAs<CXXConversionDecl>("node");
  const CXXRecordDecl *Clazz = Method->getParent();

  if (!Method->isExplicitSpecified() &&
      !hasCustomAnnotation(Method, "moz_implicit") &&
      !ASTIsInSystemHeader(Method->getASTContext(), *Method) &&
      isInterestingDeclForImplicitConversion(Method)) {
    diag(Method->getLocStart(), "bad implicit conversion operator for %0",
         DiagnosticIDs::Error) << Clazz;
    diag(Method->getLocStart(), "consider adding the explicit keyword to %0",
         DiagnosticIDs::Note) << "'operator bool'";
  }
}
