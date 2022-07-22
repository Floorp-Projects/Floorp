/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NoPrincipalGetURI.h"
#include "CustomMatchers.h"

void NoPrincipalGetURI::registerMatchers(MatchFinder *AstMatcher) {

  AstMatcher->addMatcher(
      cxxMemberCallExpr(
          allOf(callee(cxxMethodDecl(hasName("GetURI"))),
                anyOf(on(hasType(hasCanonicalType(asString("class nsIPrincipal *")))),
                      on(hasType(hasCanonicalType(asString("class nsIPrincipal"))))),
                unless(isInWhiteListForPrincipalGetUri())),
          argumentCountIs(1))
          .bind("id"),
      this);
}

void NoPrincipalGetURI::check(const MatchFinder::MatchResult &Result) {
  const auto *MatchedDecl = Result.Nodes.getNodeAs<CXXMemberCallExpr>("id");
  diag(MatchedDecl->getExprLoc(),
       "Principal->GetURI is deprecated and will be removed soon. Please "
       "consider using the new helper functions of nsIPrincipal",
       DiagnosticIDs::Error);
}
