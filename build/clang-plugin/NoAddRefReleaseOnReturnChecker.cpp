/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NoAddRefReleaseOnReturnChecker.h"
#include "CustomMatchers.h"

void NoAddRefReleaseOnReturnChecker::registerMatchers(MatchFinder* AstMatcher) {
  // Look for all of the calls to AddRef() or Release()
  AstMatcher->addMatcher(memberExpr(isAddRefOrRelease(), hasParent(callExpr())).bind("member"),
                         this);
}

void NoAddRefReleaseOnReturnChecker::check(
    const MatchFinder::MatchResult &Result) {
  const MemberExpr *Member = Result.Nodes.getNodeAs<MemberExpr>("member");
  const Expr *Base = IgnoreTrivials(Member->getBase());

  // Check if the call to AddRef() or Release() was made on the result of a call
  // to a MOZ_NO_ADDREF_RELEASE_ON_RETURN function or method.
  if (auto *Call = dyn_cast<CallExpr>(Base)) {
    if (auto *Callee = Call->getDirectCallee()) {
      if (hasCustomAnnotation(Callee, "moz_no_addref_release_on_return")) {
        diag(Call->getLocStart(),
             "%1 cannot be called on the return value of %0",
             DiagnosticIDs::Error)
          << Callee
          << dyn_cast<CXXMethodDecl>(Member->getMemberDecl());
      }
    }
  }
}
