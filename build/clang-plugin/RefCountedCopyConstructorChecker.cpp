/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RefCountedCopyConstructorChecker.h"
#include "CustomMatchers.h"

void RefCountedCopyConstructorChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(
      cxxConstructExpr(
          hasDeclaration(cxxConstructorDecl(isCompilerProvidedCopyConstructor(),
                                            ofClass(hasRefCntMember()))))
          .bind("node"),
      this);
}

void RefCountedCopyConstructorChecker::check(
    const MatchFinder::MatchResult &Result) {
  const char* Error =
      "Invalid use of compiler-provided copy constructor on refcounted type";
  const char* Note =
      "The default copy constructor also copies the "
      "default mRefCnt property, leading to reference "
      "count imbalance issues. Please provide your own "
      "copy constructor which only copies the fields which "
      "need to be copied";

  // Everything we needed to know was checked in the matcher - we just report
  // the error here
  const CXXConstructExpr *E = Result.Nodes.getNodeAs<CXXConstructExpr>("node");

  diag(E->getLocation(), Error, DiagnosticIDs::Error);
  diag(E->getLocation(), Note, DiagnosticIDs::Note);
}
